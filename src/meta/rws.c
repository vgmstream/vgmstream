#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"


static off_t get_rws_string_size(off_t offset, STREAMFILE *streamFile);


typedef struct {
    int channel_count;
    int codec;
    int sample_rate;
    int total_segments;
    int total_layers;

    size_t file_size;
    size_t header_size;
    size_t data_size;
    size_t stream_size_full;
    size_t stream_size;
    size_t stream_size_expected;

    size_t block_size;
    size_t block_size_total;

    off_t data_offset;

    off_t coefs_offset;
    off_t stream_offset;
    off_t name_offset;

} rws_header;


/* RWS - RenderWare Stream (from games using RenderWare Audio middleware) */
VGMSTREAM * init_vgmstream_rws(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, offset;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int loop_flag;
    int total_subsongs, target_subsong = streamFile->stream_index;
    int i;
    rws_header rws = {0};

    /* check */
    if (!check_extensions(streamFile,"rws"))
        goto fail;

    /* parse chunks (always LE) */
    /* RWS are made of a file chunk with header and data chunks (other chunks exist for non-audio .RWS).
     * A chunk is: id, size, RW version (no real diffs), data of size (version is repeated but same for all chunks).
     * Version: 16b main + 16b build (can vary between files) ex: 0c02, 1003, 1400 = 3.5, 1803 = 3.6, 1C02 = 3.7. */

    if (read_32bitLE(0x00,streamFile) != 0x0000080d) /* audio file chunk id */
        goto fail;
    rws.file_size = read_32bitLE(0x04,streamFile); /* audio file chunk size */
    if (rws.file_size + 0x0c != get_streamfile_size(streamFile))
        goto fail;

    if (read_32bitLE(0x0c,streamFile) != 0x0000080e) /* header chunk id */
        goto fail;
    rws.header_size = read_32bitLE(0x10,streamFile); /* header chunk size */

    rws.data_offset = 0x0c + 0x0c + rws.header_size; /* usually 0x800 but not always */
    if (read_32bitLE(rws.data_offset+0x00,streamFile) != 0x0000080f) /* data chunk id */
        goto fail;
    rws.data_size = read_32bitLE(rws.data_offset+0x04,streamFile); /* data chunk size */
    if (rws.data_size+0x0c + rws.data_offset != get_streamfile_size(streamFile))
        goto fail;

    /* inside header chunk (many unknown fields are probably IDs/config, as two same-sized files vary a lot) */
    offset = 0x0c + 0x0c;


    /* get base header */
    /* 0x00: actual header size (less than chunk size),  0x04/08/10: sizes of various sections?,  0x14/18/24/2C: commands?
     * 0x1c: null?  0x30: 0x800?,  0x34: block_size_total?, 0x38: data offset,  0x3c: 0?, 0x40-50: file uuid */
    read_32bit = (read_32bitLE(offset+0x00,streamFile) > rws.header_size) ? read_32bitBE : read_32bitLE; /* GC/Wii/X360 = BE */
    rws.total_segments = read_32bit(offset+0x20,streamFile);
    rws.total_layers   = read_32bit(offset+0x28,streamFile);

    /* skip audio file name */
    offset += 0x50 + get_rws_string_size(offset+0x50, streamFile);


    /* RWS data can be divided in two ways:
     * - "segments": cues/divisions within data, like intro+main/loop [[Max Payne 2 (PS2), Nana (PS2)]
     *   or voice1+2+..N, song1+2+..N [Madagascar (PS2), The Legend of Spyro: Dawn of the Dragon (X360)]
     * - "streams" (layers): interleaved blocks, like L/R+C/S+LS/RS [Burnout 2 (GC/Xbox)]. Last stream has padding:
     *   ex.- 1 block: 0x1800 data of stream_0 2ch, 0x1800 data + 0x200 pad of stream1 2ch.
     *
     * Layers seem only used to fake multichannel, but as they are given sample rate/channel/codec/coefs/etc
     * they are treated as subsongs. Similarly segments can be treated as subsongs in some cases.
     * They don't seem used at the same time, though could be possible. */

    total_subsongs = rws.total_layers;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;


    /* get segment info, for all streams */
    /* 0x00/04/0c: command?,  0x18: full segment size (including all streams),  0x1c: offset, others: ?) */
    for (i = 0; i < rws.total_segments; i++) {
        rws.stream_size_full += read_32bit(offset + 0x20*i + 0x18,streamFile);
    }
    offset += 0x20 * rws.total_segments;


    /* get usable segment sizes (usually ok but sometimes > stream_size), per stream */
    for (i = 0; i < rws.total_segments; i++) { /* sum usable segment sizes (no padding) */
        rws.stream_size += read_32bit(offset + 0x04*i + 0x04*rws.total_segments*(target_subsong-1),streamFile);
    }
    offset += 0x04 * (rws.total_segments * rws.total_layers);

    /* skip segment uuids */
    offset += 0x10 * rws.total_segments;

    /* skip segment names */
    for (i = 0; i < rws.total_segments; i++) {
        offset += get_rws_string_size(offset, streamFile);
    }


    /* get stream layout */
    /* 0x00/04/14: command?, 0x08: null?  0x0c: related to samples per frame? (XADPCM=07, VAG=1C, DSP=0E, PCM=01)
     * 0x24: offset within data chunk, 0x1c: codec related?,  others: ?) */
    for (i = 0; i < rws.total_layers; i++) { /* get block_sizes */
        rws.block_size_total += read_32bit(offset + 0x10 + 0x28*i, streamFile); /* for all streeams, to skip during decode */
        if (i+1 == target_subsong) {
            //block_size_full = read_32bit(off + 0x10 + 0x28*i, streamFile); /* with padding, can be different per stream */
            rws.block_size = read_32bit(offset + 0x20 + 0x28*i, streamFile); /* without padding */
            rws.stream_offset = read_32bit(offset + 0x24 + 0x28*i, streamFile); /* within data */
        }
    }
    offset += 0x28 * rws.total_layers;

    /* get stream config */
    /* 0x04: command?,  0x0c(1): bits per sample,  others: null? */
    for (i = 0; i < rws.total_layers; i++) { /* size depends on codec so we must parse it */
        int prev_codec = 0;
        if (i+1 == target_subsong) {
            rws.sample_rate   = read_32bit(offset+0x00, streamFile);
            //unk_size        = read_32bit(off+0x08, streamFile); /* segment size again? loop-related? */
            rws.channel_count =  read_8bit(offset+0x0d, streamFile);
            rws.codec         = read_32bitBE(offset+0x1c, streamFile); /* uuid of 128b but first 32b is enough */
        }
        prev_codec = read_32bitBE(offset+0x1c, streamFile);
        offset += 0x2c;

        if (prev_codec == 0xF86215B0) { /* if codec is DSP there is an extra field per stream */
            /* 0x00: approx num samples?  0x04: approx size/loop related? (can be 0) */
            if (i+1 == target_subsong) {
                rws.coefs_offset = offset + 0x1c;
            }
            offset += 0x60;
        }

        offset += 0x04; /* padding/garbage */
    }

    /* skip stream uuids */
    offset += 0x10 * rws.total_layers;

    /* get stream name */
    for (i = 0; i < rws.total_layers; i++) {
        if (i+1 == target_subsong) {
            rws.name_offset = offset;
        }
        offset += get_rws_string_size(offset, streamFile);
    }

    /* rest is padding/garbage until chunk end (may contain strings and weird stuff) */
    // ...

    start_offset = rws.data_offset + 0x0c + rws.stream_offset;

    /* sometimes it's wrong for no apparent reason (probably a bug in RWS) */
    rws.stream_size_expected = (rws.stream_size_full / rws.block_size_total) * (rws.block_size * rws.total_layers) / rws.total_layers;
    if (rws.stream_size > rws.stream_size_expected) {
        VGM_LOG("RWS: readjusting wrong stream size %x vs expected %x\n", rws.stream_size, rws.stream_size_expected);
        rws.stream_size = rws.stream_size_expected;
    }


    loop_flag = 0; /* RWX doesn't seem to include actual looping (so devs may fake it with segments) */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(rws.channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = rws.sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = rws.stream_size;
    vgmstream->meta_type = meta_RWS;
    if (rws.name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, rws.name_offset,streamFile);

    vgmstream->layout_type = layout_blocked_rws;
    vgmstream->current_block_size = rws.block_size / vgmstream->channels;
    vgmstream->full_block_size = rws.block_size_total;

    switch(rws.codec) {
        case 0x17D21BD0:    /* PCM PC   (17D21BD0 8735ED4E B9D9B8E8 6EA9B995) */
        case 0xD01BD217:    /* PCM X360 (D01BD217 35874EED B9D9B8E8 6EA9B995) */
            /* ex. D.i.R.T. - Origin of the Species (PC), The Legend of Spyro (X360) */
            vgmstream->coding_type = coding_PCM16_int;
            vgmstream->codec_endian = (rws.codec == 0xD01BD217); /* X360: BE */
            vgmstream->interleave_block_size = 0x02; /* only used to setup channels */

            vgmstream->num_samples = pcm_bytes_to_samples(rws.stream_size, rws.channel_count, 16);
            break;

        case 0x9897EAD9:    /* PS-ADPCM PS2 (9897EAD9 BCBB7B44 96B26547 59102E16) */
            /* ex. Silent Hill Origins (PS2), Ghost Rider (PS2), Max Payne 2 (PS2), Nana (PS2) */
            vgmstream->coding_type = coding_PSX;
            vgmstream->interleave_block_size = rws.block_size / 2;

            vgmstream->num_samples = ps_bytes_to_samples(rws.stream_size, rws.channel_count);
            break;

        case 0xF86215B0:    /* DSP GC/Wii (F86215B0 31D54C29 BD37CDBF 9BD10C53) */
            /* ex. Burnout 2 (GC), Alice in Wonderland (Wii) */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->interleave_block_size = rws.block_size / 2;

            /* get coefs (all channels share them so 0 spacing; also seem fixed for all RWS) */
            dsp_read_coefs_be(vgmstream,streamFile,rws.coefs_offset, 0);

            vgmstream->num_samples = dsp_bytes_to_samples(rws.stream_size, rws.channel_count);
            break;

        case 0x936538EF:    /* XBOX-IMA PC   (936538EF 11B62D43 957FA71A DE44227A) */
        case 0x2BA22F63:    /* XBOX-IMA Xbox (2BA22F63 DD118F45 AA27A5C3 46E9790E) */
            /* ex. Broken Sword 3 (PC), Jacked (PC/Xbox), Burnout 2 (Xbox) */
            vgmstream->coding_type = coding_XBOX_IMA; /* PC and Xbox share the same data */
            vgmstream->interleave_block_size = 0;

            vgmstream->num_samples = xbox_ima_bytes_to_samples(rws.stream_size, rws.channel_count);
            break;

        default:
            VGM_LOG("RWS: unknown codec 0x%08x\n", rws.codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    block_update_rws(start_offset, vgmstream);

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* rws-strings are null-terminated then padded to 0x10 (weirdly the padding contains garbage) */
static off_t get_rws_string_size(off_t offset, STREAMFILE *streamFile) {
    int i;
    for (i = 0; i < 0x800; i++) { /* 0x800=arbitrary max */
        if (read_8bit(offset+i,streamFile) == 0) { /* null terminator */
            return i + (0x10 - (i % 0x10)); /* size is padded */
        }
    }

    return 0;
}
