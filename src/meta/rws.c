#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"


static off_t get_rws_string_size(off_t off, STREAMFILE *streamFile);


/* RWS - RenderWare Stream (from games using RenderWare Audio middleware) */
VGMSTREAM * init_vgmstream_rws(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, off, coefs_offset = 0, stream_offset = 0;
    int loop_flag = 0, channel_count, codec;
    size_t file_size, header_size, data_size, stream_size = 0, info_size;
    int block_size_max = 0, block_size = 0, sample_rate;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int i, total_segments, total_streams, target_stream = 0;


    if (!check_extensions(streamFile,"rws"))
        goto fail;

    /* check chunks (always LE) */
    /* Made of a file chunk with header and data chunks (other chunks exist for non-audio .RWS).
     * A chunk is: id, size, RW version (no real diffs), data of size (version is repeated but same for all chunks).
     * Version: 16b main + 16b build (can vary between files) ex: 0c02, 1003, 1400 = 3.5, 1803 = 3.6, 1C02 = 3.7. */
    if (read_32bitLE(0x00,streamFile) != 0x0000080d) /* audio file chunk id */
        goto fail;
    file_size = read_32bitLE(0x04,streamFile); /* audio file chunk size  */
    if (file_size + 0x0c != get_streamfile_size(streamFile)) goto fail;

    if (read_32bitLE(0x0c,streamFile) != 0x0000080e) /* header chunk id */
        goto fail;
    header_size = read_32bitLE(0x10,streamFile); /* header chunk size */

    off = 0x0c + 0x0c + header_size;
    if (read_32bitLE(off+0x00,streamFile) != 0x0000080f) /* data chunk id */
        goto fail;
    data_size = read_32bitLE(off+0x04,streamFile); /* data chunk size */
    if (data_size+0x0c + off != get_streamfile_size(streamFile))
        goto fail;

    /* inside header chunk (many unknown fields are probably IDs/config, as two same-sized files vary a lot) */
    off = 0x0c + 0x0c;

    /* 0x00: actual header size (less than chunk size), useful to check endianness (Wii/X360 = BE) */
    read_32bit = (read_32bitLE(off+0x00,streamFile) > header_size) ? read_32bitBE : read_32bitLE;

    /* 0x04-14: sizes of various sections?,  others: ? */
    total_segments = read_32bit(off+0x20,streamFile);
    total_streams = read_32bit(off+0x28,streamFile);
    /* 0x2c: unk,  0x30: 0x800?,  0x34: max block size?, 0x38: data offset,  0x3c: 0?, 0x40-50: file uuid?  */
    off += 0x50 + get_rws_string_size(off+0x50, streamFile); /* skip audio file name */

    /* check streams/segments */
    /* Data can be divided into segments (cues/divisions within data, ex. intro+main, voice1+2+..N) or
     * tracks/streams in interleaved blocks that can contain padding and don't need to match between tracks
     * (ex 0x1800 data + 0 pad of stream_0 2ch, 0x1800 data + 0x200 pad of stream1 2ch, etc). */
    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || target_stream > total_streams || total_streams < 1) goto fail;

    /* skip segment stuff and get stream size (from sizes for all segments, repeated per track) */
    off += 0x20 * total_segments; /* segment data (mostly unknown except @ 0x18: full data size, 0x1c: offset) */
    for (i = 0; i < total_segments; i++) { /* sum usable segment sizes (no padding) */
        stream_size += read_32bit(off + 0x04 * i + total_segments*(target_stream-1),streamFile);
    }
    off += 0x04 * (total_segments * total_streams);
    off += 0x10 * total_segments; /* segment uuids? */
    for (i = 0; i < total_segments; i++) { /* skip segments names */
        off += get_rws_string_size(off, streamFile);
    }

    /* get stream layout: 0xc: samples per frame (ex. 28 in VAG), 0x24: offset within data chunk,  others: ? */
    /* get block_size for our target stream and from all streams, to skip their blocks during decode */
    for (i = 0; i < total_streams; i++) { /* get block_sizes */
        block_size_max += read_32bit(off+0x10 + 0x28*i,streamFile); /* includes padding and can be different per stream */
        if (target_stream-1 == i) {
            block_size = read_32bit(off+0x20 + 0x28*i,streamFile); /* actual size */
            stream_offset = read_32bit(off+0x24 + 0x28*i,streamFile); /* within data */
        }
    }
    off += 0x28 * total_streams;

    /* get stream config: 0x0c(1): bits per sample,  others: ? */
    info_size = total_streams > 1 ? 0x30 : 0x2c; //todo this doesn't look right
    sample_rate   = read_32bit(off+0x00 + info_size*(target_stream-1),streamFile);
    //unk_size    = read_32bit(off+0x08 + info_size*(target_stream-1),streamFile); /* segment size? loop-related? */
    channel_count =  read_8bit(off+0x0d + info_size*(target_stream-1),streamFile);
    codec         = read_32bitBE(off+0x1c + info_size*(target_stream-1),streamFile); /* uuid of 128b but the first is enough */
    off += info_size * total_streams;


    /* if codec is DSP there is an extra field */
    if (codec == 0xF86215B0) {
        /* 0x00: approx num samples?  0x04: approx size? */
        coefs_offset = off + 0x1c;
    }

    /* next is 0x14 * streams = ?(4) + uuid? (header ends), rest is garbage/padding until chunk end (may contain strings and weird stuff) */

    start_offset = 0x0c + 0x0c + header_size + 0x0c + stream_offset; /* usually 0x800 but not always */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RWS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_streams;

    vgmstream->layout_type = layout_rws_blocked;
    vgmstream->current_block_size = block_size / vgmstream->channels;
    vgmstream->full_block_size = block_size_max;

    switch(codec) {
        case 0xD01BD217:    /* PCM X360 (D01BD217 35874EED B9D9B8E8 6EA9B995) */
            /* The Legend of Spyro (X360) */
            vgmstream->coding_type = coding_PCM16BE;
            //vgmstream->interleave_block_size = block_size / 2; //0x2; //todo 2ch PCM not working correctly (interleaved PCM not ok?)

            vgmstream->num_samples = pcm_bytes_to_samples(stream_size, channel_count, 16);
            break;

        case 0x9897EAD9:    /* PS-ADPCM PS2 (9897EAD9 BCBB7B44 96B26547 59102E16) */
            /* ex. Silent Hill Origins (PS2), Ghost Rider (PS2), Max Payne 2 (PS2), Nana (PS2) */
            vgmstream->coding_type = coding_PSX;
            vgmstream->interleave_block_size = block_size / 2;

            vgmstream->num_samples = ps_bytes_to_samples(stream_size, channel_count);
            break;

        case 0xF86215B0:    /* DSP Wii (F86215B0 31D54C29 BD37CDBF 9BD10C53) */
            /* ex. Alice in Wonderland (Wii) */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->interleave_block_size = block_size / 2;

            /* get coefs (all channels share them so 0 spacing; also seem fixed for all RWS) */
            dsp_read_coefs_be(vgmstream,streamFile,coefs_offset, 0);

            vgmstream->num_samples = dsp_bytes_to_samples(stream_size, channel_count);
            break;

        case 0x936538EF:    /* MS-IMA PC   (936538EF 11B62D43 957FA71A DE44227A) */
        case 0x2BA22F63:    /* MS-IMA Xbox (2BA22F63 DD118F45 AA27A5C3 46E9790E) */
            /* ex. Broken Sword 3 (PC), Jacked (PC/Xbox), Burnout 2 (Xbox) */
            vgmstream->coding_type = coding_XBOX;
            vgmstream->interleave_block_size = 0; /* uses regular XBOX/MS-IMA interleave */

            vgmstream->num_samples = ms_ima_bytes_to_samples(stream_size, 0x48, channel_count);
            break;

        default:
            VGM_LOG("RSW: unknown codec 0x%08x\n", codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    rws_block_update(start_offset, vgmstream); /* block init */

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* rws-strings are null-terminated then padded to 0x10 (weirdly the padding contains garbage) */
static off_t get_rws_string_size(off_t off, STREAMFILE *streamFile) {
    int i;
    for (i = 0; i < 0x800; i++) { /* 0x800=arbitrary max */
        if (read_8bit(off+i,streamFile) == 0) { /* null terminator */
            return i + (0x10 - (i % 0x10)); /* size is padded */
        }
    }

    return 0;
}
