/* Handles Ubi-MPEG, a modified VBR MP2 (format seems to be called simply 'MPEG').
 *
 * Sync is slightly different and MPEG config is fixed (info removed from the bitstream). Frames
 * are also not byte-aligned, meaning a new frame starts right after prev ends in the bitstream
 * (in practice frames are aligned to 4-bits though so recognizable in plain sight).
 * It also has a mode where 1 stereo frame + 1 mono frame are used make surround(?) output.
 *
 * TODO: this doesn't handle surround modes at the moment, so it isn't fully accurate.
 *
 * Partially reverse engineered from DLLs.
 * - MPGMXBVR.DLL: regular version, seems to (mostly) not use simd (ex. sub_10001400)
 * - MPGMXSVR.DLL: xmm version
 *
 * Info+crosschecking from spec and libmpg123/various:
 * - http://www.mp3-tech.org/programmer/frame_header.html
 * - https://github.com/dreamerc/mpg123/blob/master/src/libmpg123/layer2.c
 */

//TODO: test if files have encoding delay, but some files seem to have very few spare samples vs declared block samples

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ubi_mpeg_helpers.h"
#include "../../util/log.h"

// forced size to simplify (explained later): (144 * bitrate * 1000 / sample_rate) + (1 optional padding byte)
//#define UBIMPEG_STEREO_FRAME_SIZE 0x20B // 128~160kkbps
//#define UBIMPEG_MONO_FRAME_SIZE 0x106 // 64~80kbps
#define UBIMPEG_FIXED_FRAME_SIZE 0x300 // ~256kbps at 48000hz

// standard MPEG modes
#define CH_MODE_MONO 3
#define CH_MODE_JOINT 1
#define CH_MODE_STEREO 0
#define MAX_BANDS 32  //30?
#define MAX_GRANULES  12

// band <> used allocation bits
// table 0 for 27 subbands (out of ~4); Ubi-MPEG only has this table.
// (spec's Table 3-B.2a, though we separate it into multiple tables for clarity)
static const uint8_t BITALLOC_TABLE_0[32] = {
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 0, 0, 0, 0, 0,
};

// index of "number of steps" to the "bits per codeword" (qbit) table, [band][index]
static const uint8_t QINDEX_TABLE_0[32][16] = {
    { 0, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, },
    { 0, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, },
    { 0, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, },

    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16 },

    { 0, 1, 2, 3, 4, 5, 16, },
    { 0, 1, 2, 3, 4, 5, 16, },
    { 0, 1, 2, 3, 4, 5, 16, },
    { 0, 1, 2, 3, 4, 5, 16, },
    { 0, 1, 2, 3, 4, 5, 16, },
    { 0, 1, 2, 3, 4, 5, 16, },
    { 0, 1, 2, 3, 4, 5, 16, },
    { 0, 1, 2, 3, 4, 5, 16, },
    { 0, 1, 2, 3, 4, 5, 16, },
    { 0, 1, 2, 3, 4, 5, 16, },
    { 0, 1, 2, 3, 4, 5, 16, },
    { 0, 1, 2, 3, 4, 5, 16, },

    { 0, 1, 16, },
    { 0, 1, 16, },
    { 0, 1, 16, },
    { 0, 1, 16, },
    { 0, 1, 16, },
    { 0, 1, 16, },
    { 0, 1, 16, },
};

// "bits per codeword" (qbit) table, number of bits used to quantize a value
// usually negative has a special meaning of "3 grouped qs"
static const int8_t QBITS_TABLE_0[17] = {
    -5, -7, 3, -10, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
};

#if 0
// often QBITS_TABLE uses negative values meaning "grouping" to avoid this table (-5, -7, 3, -10, ...)
static const int8_t GROUPING_TABLE[17] = {
    3, 3, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
#endif

// bands in the bitstream, fixed to index 0 in Ubi MPEG
static const int MAX_SUBBANDS[5] = { 27, 30, 8, 12, 30 };

// number of subbands that are regular stereo, depending on mode extension (rest are intensity stereo)
static int JOINT_BOUNDS[4] = { 4, 8, 12, 16 };


// most Ubi MPEG files have frames right after other, but sometimes they'll have some
// of padding. This doesn't seem a bitreading issue, their OG libs do seem to manually
// try to find sync until EOF on every new frame.
static int find_sync(bitstream_t* is) {

    // read until some limit rather than EOF
    int tests = 0;
    while (tests < 32) {
        //TODO: OG lib seems to advance bit-by-bit, but this seems to work
        uint16_t sync = bm_read(is, 12); // 12 bits unlike 11 in MPEG
        if (sync != 0)
            return sync;

        tests++;
    }

    return 0;
}

// reads a UBI-MPEG frame and transforms it into a regular MP2 frame
// Note that UBI-MPEG simply reads and decodes it by itself while handling surround modes,
// so this is just a quick hack.
int ubimpeg_transform_frame(bitstream_t* is, bitstream_t* os) {
    //uint32_t offset = bm_pos(is) / 8;

    // config per sample rae + bitrate + channels, fixed in Ubi MPEG
    const int table = 0;
    // bit info about how samples in subband are quantized
    uint8_t bit_alloc[MAX_BANDS][2] = {0};//todo invert ch, todo 'allocation'
    uint32_t scfsi[MAX_BANDS][2] = {0};

    /* read 16-bit Ubi-MPEG header */
    uint16_t sync = find_sync(is);
    if (sync != 0xFFF) {
        //VGM_LOG("UBI-MPEG: sync not found at %x (+ %i), value=%x\n", is->b_off / 8, is->b_off % 8, sync);
        return 0;
    }

    uint8_t mode = bm_read(is, 4);

    int extmode = (mode >> 0) & 0x03;
    int ch_mode = (mode >> 2) & 0x03;
    int channels = (ch_mode == CH_MODE_MONO) ? 1 : 2;
    int subbands = MAX_SUBBANDS[table];
    int joint_bound = (ch_mode != CH_MODE_JOINT) ? subbands : JOINT_BOUNDS[extmode]; // boundary of non-js bands

    // Ubi-MPEG uses VBR and settings close to 44100 + 128~160kbps (stereo) / 64~80kbps (mono). However some
    // frames spill into 1 more byte than the 0x20b allowed by 160kbps (even with padding bit). Sample rate + bitrate
    // combo must end up using certain tables in decoders, so use a setting that allows a consistenct bitrate.
    // Free bitrate (br_index 0) is usable too but decoders need several frames to detect sizes.
    int frame_size = UBIMPEG_FIXED_FRAME_SIZE;
    int br_index = 12; // 256kbps
    int sr_index = 1; // fixed to 48000 (Ubi MPEG is 44100 but shouldn't affect decoding)
    int padding = 0;
    
    // real-ish settings, but doesn't work for all frames
    //frame_size = (channels == 2) ? UBIMPEG_STEREO_FRAME_SIZE : UBIMPEG_MONO_FRAME_SIZE;
    //br_index = (channels == 2) ? 9 : 5; // 160kbps / 80kbps
    //sr_index = 0; // fixed to 44100
    //padding = 1;

    /* write 32-bit MPEG frame header */
    bm_put(os, 11, 0x7FF);      // sync
    bm_put(os,  2, 3);          // MPEG 1 index
    bm_put(os,  2, 2);          // layer II index
    bm_put(os,  1, 1);          // "no CRC" flag
    bm_put(os,  4, br_index);   // bitrate index
    bm_put(os,  2, sr_index);   // sample rate index
    bm_put(os,  1, padding);     // padding
    bm_put(os,  1, 0);          // private
    bm_put(os,  2, ch_mode);    // channel mode
    bm_put(os,  2, extmode);    // mode extension
    bm_put(os,  1, 1);          // copyrighted
    bm_put(os,  1, 1);          // original
    bm_put(os,  2, 0);          // emphasis


    // step I: read allocation bits 
    for (int i = 0; i < joint_bound; i++) {
        int ba_bits = BITALLOC_TABLE_0[i];
        for (int ch = 0; ch < channels; ch++) {
            bit_alloc[i][ch] = bm_read(is, ba_bits);
            bm_put(os, ba_bits, bit_alloc[i][ch]);
        }
    }

    // step I: read rest of joint stereo allocation bits (same for L and R)
    for (int i = joint_bound; i < subbands; i++) {
        int ba_bits = BITALLOC_TABLE_0[i];

        bit_alloc[i][0] = bm_read(is, ba_bits);
        bit_alloc[i][1] = bit_alloc[i][0];
        bm_put(os, ba_bits, bit_alloc[i][0]);
    }

    // step I: read scalefactor selector information (how many scalefactors are included)
    for (int i = 0; i < subbands; i++) {
        for (int ch = 0; ch < channels; ch++) {
            if (bit_alloc[i][ch] == 0)
                continue;

            scfsi[i][ch] = bm_read(is, 2);
            bm_put(os, 2, scfsi[i][ch]);
        }
    }

    // step I: read scalefactors from scfsi indexes
    for (int i = 0; i < subbands; i++) {
        for (int ch = 0; ch < channels; ch++) {
            if (bit_alloc[i][ch] == 0)
                continue;

            int scf;
            switch(scfsi[i][ch]) {
                case 0: // 3 scalefactors
                    scf = bm_read(is, 6);
                    bm_put(os, 6, scf);
                    scf = bm_read(is, 6);
                    bm_put(os, 6, scf);
                    scf = bm_read(is, 6);
                    bm_put(os, 6, scf);
                    break;
                case 1: // 2 scalefactors (reuses 1st for 2nd)
                case 3: // 2 scalefactors (reuses 2nd for 3rd)
                    scf = bm_read(is, 6);
                    bm_put(os, 6, scf);
                    scf = bm_read(is, 6);
                    bm_put(os, 6, scf);
                    break;
                case 2: // 1 scalefactor (reuses all 3)
                    scf = bm_read(is, 6);
                    bm_put(os, 6, scf);
                    break;
                default:
                    break;
            }
        }
    }

    // step II: read quantized DCT coefficients
    // (restored to 3 samples per granule and up to 32 subbands)
    for (int gr = 0; gr < MAX_GRANULES; gr++) {
        // regular quants
        for (int i = 0; i < joint_bound; i++) {
            for (int ch = 0; ch < channels; ch++) {
                int ba_index = bit_alloc[i][ch];
                if (ba_index == 0)
                    continue;

                int qb_index = QINDEX_TABLE_0[i][ba_index - 1];
                int qbits = QBITS_TABLE_0[qb_index];

                int qs;
                if (qbits < 0) {        // grouping used
                    qbits = -qbits;     // remove flag
                    qs = 1;             // same value for 3 codes
                }
                else {
                    qs = 3;             // 3 distinct codes
                }

                for (int q = 0; q < qs; q++) {
                    int qcode = bm_read(is, qbits);
                    bm_put(os, qbits, qcode);
                }
            }
        }

        // joint stereo quants (1 channel only)
        for (int i = joint_bound; i < subbands; i++) {
            {
                int ba_index = bit_alloc[i][0];
                if (ba_index == 0)
                    continue;

                int qb_index = QINDEX_TABLE_0[i][ba_index - 1];
                int qbits = QBITS_TABLE_0[qb_index];

                int qs;
                if (qbits < 0) {        // grouping used
                    qbits = -qbits;     // remove flag
                    qs = 1;             // same value for 3 codes
                }
                else {
                    qs = 3;             // 3 distinct codes
                }

                for (int q = 0; q < qs; q++) {
                    int qcode = bm_read(is, qbits);
                    bm_put(os, qbits, qcode);
                }
            }
        }
    }

    // real MPEG frames are byte padded, but not Ubi-MPEG's (usually)
    uint32_t bitpos = bm_pos(os);
    if (bitpos % 8) {
        bm_skip(os, 8 - (bitpos % 8));
    }

    //uint32_t end_offset = bm_pos(is) / 8;
    //VGM_LOG("frame: ch_mode=%x, extmode=%x = chs=%i, reg_bands=%i, o=%x + %x\n", ch_mode, extmode, channels, joint_bound, offset, end_offset - offset);

    // 0-pad as may confuse decoders otherwise
    uint32_t obuf_done = bm_pos(os) / 8;
    if (obuf_done > frame_size) {
        //VGM_LOG("error: buf done=%x (%i) vs %x\n", obuf_done, bm_pos(os), frame_size);
        return 0;
    }
    else {
        memset(os->buf + obuf_done, 0x00, frame_size - obuf_done);
    }

    return frame_size;
}



#if 0
static bool parse_file(uint8_t* buf, int buf_size, FILE* outfile_mp2a, FILE* outfile_mp2b, FILE* outfile0_wav, FILE* outfile1_wav) {

    printf("UBI-MPEG: samples=%i, 2rus=%i, 1rus=%i\n", samples, is_2rus, is_1rus);


    bitstream_t is;
    bm_setup(&is, buf, buf_size);
    bm_skip(&is, 0x04 * 8);
    if (is_1rus || is_2rus)
        bm_skip(&is, 0x04 * 8);

    uint8_t obuf[0x800];
    bitstream_t os;

    mp3dec_t mp3d0;
    mp3dec_init(&mp3d0);
    mp3dec_frame_info_t info0;
    short pcm0[MINIMP3_MAX_SAMPLES_PER_FRAME];

    mp3dec_t mp3d1;
    mp3dec_init(&mp3d1);
    mp3dec_frame_info_t info1;
    short pcm1[MINIMP3_MAX_SAMPLES_PER_FRAME];

    int frames = 0;
    int done = 0;
    int obuf_size;
    while (1) {

        // 1st frame
        {
            bm_setup(&os, obuf, sizeof(obuf));

            obuf_size = ubimpeg_transform_frame(&is, &os);
            if (!obuf_size) break;
            frames++;
            fwrite(obuf, sizeof(uint8_t), obuf_size, outfile_mp2a);

            int pcm0_samples = mp3dec_decode_frame(&mp3d0, obuf, obuf_size, pcm0, &info0);
            printf("mpeg0: pcm=%i, br=%i, ch=%i, by=%x\n", pcm0_samples, info0.bitrate_kbps, info0.channels, info0.frame_bytes);
            fwrite(pcm0, sizeof(short), pcm0_samples * info0.channels, outfile0_wav);
        }

        // 2nd frame
        if (is_1rus || is_2rus) {
            bm_setup(&os, obuf, sizeof(obuf));

            obuf_size = ubimpeg_transform_frame(&is, &os);
            if (!obuf_size) break;
            frames++;

            fwrite(obuf, sizeof(uint8_t), obuf_size, outfile_mp2b);

            int pcm1_samples = mp3dec_decode_frame(&mp3d1, obuf, obuf_size, pcm1, &info1);
            printf("mpeg1: pcm=%i, br=%i, ch=%i, by=%x\n", pcm1_samples, info1.bitrate_kbps, info1.channels, info1.frame_bytes);
            fwrite(pcm1, sizeof(short), pcm1_samples * info1.channels, outfile1_wav);
        }


        done += 1152;
        printf("samples: %i / %i, obuf=%x (%x)\n", done, samples, obuf_size);
        if (done >= samples)
            break;
    }
    printf("frames: done=%i (%x) / samples=%i (%x)\n", done, done, samples, samples);

    return true;
}
#endif
