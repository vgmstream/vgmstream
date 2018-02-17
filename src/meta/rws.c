#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"


static off_t get_rws_string_size(off_t off, STREAMFILE *streamFile);


/* RWS - RenderWare Stream (from games using RenderWare Audio middleware) */
VGMSTREAM * init_vgmstream_rws(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, off, coefs_offset = 0, stream_offset = 0, name_offset = 0;
    int loop_flag = 0, channel_count = 0, codec = 0, sample_rate = 0;
    size_t file_size, header_size, data_size, stream_size_full = 0, stream_size = 0, stream_size_expected = 0;
    size_t block_size = 0, block_size_total = 0;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int i, total_segments;
    int total_subsongs, target_subsong = streamFile->stream_index;


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

    /* get base header */
    /* 0x00: actual header size (less than chunk size),  0x04/08/10: sizes of various sections?,  0x14/18/24/2C: commands?
     * 0x1c: null?  0x30: 0x800?,  0x34: block_size_total?, 0x38: data offset,  0x3c: 0?, 0x40-50: file uuid */
    read_32bit = (read_32bitLE(off+0x00,streamFile) > header_size) ? read_32bitBE : read_32bitLE; /* GC/Wii/X360 = BE */
    total_segments = read_32bit(off+0x20,streamFile);
    total_subsongs = read_32bit(off+0x28,streamFile);

    /* skip audio file name */
    off += 0x50 + get_rws_string_size(off+0x50, streamFile);


    /* Data is divided into "segments" (cues/divisions within data, ex. intro+main, voice1+2+..N) and "streams"
     * of interleaved blocks (for multichannel?). last stream (only?) has padding. Segments divide all streams.
     * ex.- 0x1800 data + 0 pad of stream_0 2ch, 0x1800 data + 0x200 pad of stream1 2ch (xN). */
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    /* get segment info, for all streams */
    /* 0x00/04/0c: command?,  0x18: full segment size (including all streams),  0x1c: offset, others: ?) */
    for (i = 0; i < total_segments; i++) {
        stream_size_full += read_32bit(off + 0x20*i + 0x18,streamFile);
    }
    off += 0x20 * total_segments;

    /* get usable segment sizes (usually ok but sometimes > stream_size), per stream */
    for (i = 0; i < total_segments; i++) { /* sum usable segment sizes (no padding) */
        stream_size += read_32bit(off + 0x04*i + 0x04*total_segments*(target_subsong-1),streamFile);
    }
    off += 0x04 * (total_segments * total_subsongs);

    /* skip segment uuids */
    off += 0x10 * total_segments;

    /* skip segment names */
    for (i = 0; i < total_segments; i++) {
        off += get_rws_string_size(off, streamFile);
    }

    /* get stream layout */
    /* 0x00/04/14: command?, 0x08: null?  0x0c: spf related? (XADPCM=07, VAG=1C, DSP=0E, PCM=01)
     * 0x24: offset within data chunk, 0x1c: codec related?,  others: ?) */
    for (i = 0; i < total_subsongs; i++) { /* get block_sizes */
        block_size_total += read_32bit(off + 0x10 + 0x28*i, streamFile); /* for all streeams, to skip during decode */
        if (i+1 == target_subsong) {
            //block_size_full = read_32bit(off + 0x10 + 0x28*i, streamFile); /* with padding, can be different per stream */
            block_size = read_32bit(off + 0x20 + 0x28*i, streamFile); /* without padding */
            stream_offset = read_32bit(off + 0x24 + 0x28*i, streamFile); /* within data */
        }
    }
    off += 0x28 * total_subsongs;

    /* get stream config */
    /* 0x04: command?,  0x0c(1): bits per sample,  others: null? */
    for (i = 0; i < total_subsongs; i++) { /* size depends on codec so we must parse it */
        int prev_codec = 0;
        if (i+1 == target_subsong) {
            sample_rate   = read_32bit(off+0x00, streamFile);
            //unk_size    = read_32bit(off+0x08, streamFile); /* segment size again? loop-related? */
            channel_count =  read_8bit(off+0x0d, streamFile);
            codec         = read_32bitBE(off+0x1c, streamFile); /* uuid of 128b but first 32b is enough */
        }
        prev_codec = read_32bitBE(off+0x1c, streamFile);
        off += 0x2c;

        if (prev_codec == 0xF86215B0) { /* if codec is DSP there is an extra field per stream */
            /* 0x00: approx num samples?  0x04: approx size/loop related? (can be 0) */
            if (i+1 == target_subsong) {
                coefs_offset = off + 0x1c;
            }
            off += 0x60;
        }

        off += 0x04; /* padding/garbage */
    }

    /* skip stream uuids */
    off += 0x10 * total_subsongs;

    /* get stream name */
    for (i = 0; i < total_subsongs; i++) {
        if (i+1 == target_subsong) {
            name_offset = off;
        }
        off += get_rws_string_size(off, streamFile);
    }

    /* rest is padding/garbage until chunk end (may contain strings and weird stuff) */
    // ...

    /* usually 0x800 but not always */
    start_offset = 0x0c + 0x0c + header_size + 0x0c + stream_offset;

    /* sometimes it's wrong for no apparent reason (probably a bug in RWS) */
    stream_size_expected = (stream_size_full / block_size_total) * (block_size * total_subsongs) / total_subsongs;
    if (stream_size > stream_size_expected) {
        VGM_LOG("RWS: readjusting wrong stream size %x vs expected %x\n", stream_size, stream_size_expected);
        stream_size = stream_size_expected;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_RWS;
    if (name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,streamFile);

    vgmstream->layout_type = layout_rws_blocked;
    vgmstream->current_block_size = block_size / vgmstream->channels;
    vgmstream->full_block_size = block_size_total;

    switch(codec) {
        case 0x17D21BD0:    /* PCM PC (17D21BD0 8735ED4E B9D9B8E8 6EA9B995) */
        case 0xD01BD217:    /* PCM X360 (D01BD217 35874EED B9D9B8E8 6EA9B995) */
            /* ex. D.i.R.T. - Origin of the Species (PC), The Legend of Spyro (X360) */
            vgmstream->coding_type = coding_PCM16_int;
            vgmstream->codec_endian = (codec == 0xD01BD217);
            vgmstream->interleave_block_size = 0x02; /* only used to setup channels */

            vgmstream->num_samples = pcm_bytes_to_samples(stream_size, channel_count, 16);
            break;

        case 0x9897EAD9:    /* PS-ADPCM PS2 (9897EAD9 BCBB7B44 96B26547 59102E16) */
            /* ex. Silent Hill Origins (PS2), Ghost Rider (PS2), Max Payne 2 (PS2), Nana (PS2) */
            vgmstream->coding_type = coding_PSX;
            vgmstream->interleave_block_size = block_size / 2;

            vgmstream->num_samples = ps_bytes_to_samples(stream_size, channel_count);
            break;

        case 0xF86215B0:    /* DSP GC/Wii (F86215B0 31D54C29 BD37CDBF 9BD10C53) */
            /* ex. Burnout 2 (GC), Alice in Wonderland (Wii) */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->interleave_block_size = block_size / 2;

            /* get coefs (all channels share them so 0 spacing; also seem fixed for all RWS) */
            dsp_read_coefs_be(vgmstream,streamFile,coefs_offset, 0);

            vgmstream->num_samples = dsp_bytes_to_samples(stream_size, channel_count);
            break;

        case 0x936538EF:    /* XBOX-IMA PC   (936538EF 11B62D43 957FA71A DE44227A) */
        case 0x2BA22F63:    /* XBOX-IMA Xbox (2BA22F63 DD118F45 AA27A5C3 46E9790E) */
            /* ex. Broken Sword 3 (PC), Jacked (PC/Xbox), Burnout 2 (Xbox) */
            vgmstream->coding_type = coding_XBOX_IMA; /* PC and Xbox share the same data */
            vgmstream->interleave_block_size = 0;

            vgmstream->num_samples = xbox_ima_bytes_to_samples(stream_size, channel_count);
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
