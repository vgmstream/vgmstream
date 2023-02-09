#ifdef VGM_USE_MPEG
#include "mpeg_decoder.h"
#include "../util/bitstream_msb.h"
#include "coding.h"

#define MPEG_AHX_EXPECTED_FRAME_SIZE 0x414

/* AHX is more or less VBR MP2 using a fixed header (0xFFF5E0C0) that sets frame size 0x414 (1ch, 160kbps, 22050Hz)
 * but are typically much shorter (ignores padding), output sample rate is also ignored.
 *
 * MPEG1 Layer II (MP2) bitstream format for reference:
 * - MPEG header, 32b
 * - 'bit allocation' indexes (MP2's config determines bands and table with bit size per band, in AHX's case 30 bands and total 107 bits)
 * - 16-bit CRC if set in header (never in AHX)
 * - scale factor selection info (SCFSI), 2b per band/channel (if band has bit alloc set)
 * - scale factors, bits depending on selection info (if band has bit alloc set)
 * - quantized samples, bits depending on bit alloc info
 * - padding (removed in AHX)
 */


#define AHX_BANDS  30
#define AHX_GRANULES  12
static const uint8_t AHX_BITALLOC_TABLE[32] = { 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 };
static const uint8_t AHX_OFFSET_TABLE[5][16] = {
    { 0 },
    { 0 },
    { 0, 1,  3, 4,                                         },
    { 0, 1,  3, 4, 5, 6,  7, 8,                            },
    { 0, 1,  2, 3, 4, 5,  6, 7,  8,  9, 10, 11, 12, 13, 14 }
};
static const int8_t AHX_QBITS_TABLE[17] = { -5, -7, 3, -10, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

/* Decrypts and tests a AHX frame with current by reading all bits, as wrong keys should go over size. Reverse engineered
 * from CRI libs. (MPEG1 Layer II code abridged for AHX, which is always mono and has fixed bands/tables, some info from ahx2wav.c) */
static int ahx_decrypt(uint8_t* buf, int curr_size, crikey_t* crikey) {
    uint32_t bit_alloc[AHX_BANDS] = {0};
    uint32_t scfsi[AHX_BANDS] = {0};
    bitstream_t ib = {0};
    bitstream_t ob = {0};

    bm_setup(&ib, buf, curr_size); /* frame */
    bm_setup(&ob, buf, curr_size); /* decrypted frame */

    /* MPEG header (fixed in AHX, otherwise layer/bitrate/channels sets bands+tables) */
    bm_skip(&ib, 32);
    bm_skip(&ob, 32);

    /* read bit allocs for later */
    for (int i = 0; i < AHX_BANDS; i++) {
        int ba_bits = AHX_BITALLOC_TABLE[i];

        bm_get (&ib, ba_bits, &bit_alloc[i]);
        bm_skip(&ob, ba_bits);
    }

    /* get first scalefactor info to decide key */
    if (bit_alloc[0]) {
        bm_get (&ib, 2, &scfsi[0]);
        bm_skip(&ob, 2);
    }

    uint16_t key;
    switch(scfsi[0]) {
        case 1: key = crikey->key1; break;
        case 2: key = crikey->key2; break;
        case 3: key = crikey->key3; break;
        default: key = 0; /* 0: no key (common in null frames) */
    }

    /* decrypt rest of scalefactors (only first ones are encrypted though) */
    for (int i = 1; i < AHX_BANDS; i++) {
        if (bit_alloc[i]) {
            bm_get (&ib, 2, &scfsi[i]);
            scfsi[i] ^= (key & 3);
            bm_put(&ob, 2,  scfsi[i]);
        }
        key >>= 2;
    }

    /* read scalefactors (past this point no need to decrypt/write frame) */
    for (int i = 0; i < AHX_BANDS; i++) {
        if (bit_alloc[i] == 0)
            continue;

        switch(scfsi[i]) {
            case 0: bm_skip(&ib, 6 * 3); break;
            case 1:
            case 3: bm_skip(&ib, 6 * 2); break;
            case 2: bm_skip(&ib, 6 * 1); break;
            default: break;
        }
    }

    /* read quants */
    for (int gr = 0; gr < AHX_GRANULES; gr++) {
        for (int i = 0; i < AHX_BANDS; i++) {
            int ba_value = bit_alloc[i];
            if (ba_value == 0)
                continue;

            int ba_bits = AHX_BITALLOC_TABLE[i];
            int qb_index = AHX_OFFSET_TABLE[ba_bits][ba_value - 1];
            int qbits = AHX_QBITS_TABLE[qb_index];

            if (qbits < 0)
                qbits = -qbits;
            else
                qbits = qbits * 3; /* 3 qs */

            int ok = bm_skip(&ib, qbits);
            if (!ok) goto fail;
        }
    }

    /* read padding */
    {
        int bpos = bm_pos(&ib);
        if (bpos % 8) {
            bm_skip(&ib, 8 - (bpos % 8));
        }
    }

    /* if file was properly read/decrypted this size should land in next frame header or near EOF */
    return bm_pos(&ib) / 8;
fail:
    return 0;
}


/* writes data to the buffer and moves offsets, transforming AHX frames as needed */
int mpeg_custom_parse_frame_ahx(VGMSTREAMCHANNEL* stream, mpeg_codec_data* data, int num_stream) {
    mpeg_custom_stream *ms = data->streams[num_stream];
    size_t curr_size = 0;
    size_t file_size = get_streamfile_size(stream->streamfile);


    /* Find actual frame size by looking for the next frame header. Not very elegant but simpler, works with encrypted AHX,
     * and possibly faster than reading frame size's bits with ahx_decrypt */
    {
        ms->bytes_in_buffer = read_streamfile(ms->buffer, stream->offset, MPEG_AHX_EXPECTED_FRAME_SIZE + 0x04, stream->streamfile);

        uint32_t curr_header = get_u32be(ms->buffer);
        int pos = 0x04;
        while (pos <= MPEG_AHX_EXPECTED_FRAME_SIZE) {

            /* next sync test */
            if (ms->buffer[pos] == 0xFF) {
                uint32_t next_header = get_u32be(ms->buffer + pos);
                if (curr_header == next_header) {
                    curr_size = pos;
                    break;
                }
            }

            /* AHX footer (0x8001000C 41485845 28632943 52490000 = 0x8001 tag + size + "AHXE(c)CRI\0\0") */
            if (stream->offset + pos + 0x10 >= file_size) {
                curr_size = pos;
                break;
            }

            pos++;
        }
    }

    if (curr_size == 0 || curr_size > ms->buffer_size || curr_size > MPEG_AHX_EXPECTED_FRAME_SIZE) {
        VGM_LOG("MPEG AHX: incorrect data_size 0x%x\n", curr_size);
        goto fail;
    }

    /* 0-fill up to expected size to keep mpg123 happy */
    memset(ms->buffer + curr_size, 0, MPEG_AHX_EXPECTED_FRAME_SIZE - curr_size);
    ms->bytes_in_buffer = MPEG_AHX_EXPECTED_FRAME_SIZE;

    /* decrypt if needed (only 0x08 is known but 0x09 is probably the same) */
    if (data->config.encryption == 0x08) {
        ahx_decrypt(ms->buffer, curr_size, &data->config.crikey);
    }

    /* update offsets */
    stream->offset += curr_size;
    if (stream->offset + 0x10 >= file_size)
        stream->offset = file_size; /* skip footer to reach EOF (shouldn't happen normally) */

    return 1;
fail:
    return 0;
}


#define AHX_KEY_BUFFER  0x1000 /* not too big since it's read per new key */
#define AHX_KEY_TEST_FRAMES  25 /* wrong keys may work ok in some frames (specially blank) */

/* check if current key ends properly in frame syncs */
int test_ahx_key(STREAMFILE* sf, off_t offset, crikey_t* crikey) {
    int bytes = 0;
    uint8_t buf[AHX_KEY_BUFFER];
    const int buf_size = sizeof(buf);
    int pos = 0;
    uint32_t base_sync, curr_sync;


    for (int i = 0; i < AHX_KEY_TEST_FRAMES; i++) {
        if (bytes < MPEG_AHX_EXPECTED_FRAME_SIZE)  {
            offset += pos;
            pos = 0;
            bytes = read_streamfile(buf, offset, buf_size, sf);
            //if (bytes != buf_size) goto fail; /* possible in small AHX */
            base_sync = get_u32be(buf + 0x00);
        }

        int frame_size = ahx_decrypt(buf + pos, bytes, crikey);
        if (frame_size <= 0 || frame_size >= bytes - 0x04)
            goto fail;

        bytes -= frame_size;
        pos += frame_size;

        curr_sync = get_u32be(buf + pos);
        if (curr_sync == 0x00800100) /* EOF tag */
            break;
        if (base_sync != curr_sync)
            goto fail;
    }

    return 1;
fail:
    return 0;
}

#endif
