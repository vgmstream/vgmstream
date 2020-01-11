#include "meta.h"
#include "../coding/coding.h"


/* .ISB - Creative ISACT (Interactive Spatial Audio Composition Tools) middleware [Psychonauts (PC)] */
VGMSTREAM * init_vgmstream_isb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset = 0, name_offset = 0;
    size_t stream_size = 0, name_size = 0;
    int loop_flag = 0, channel_count = 0, sample_rate = 0, codec = 0, pcm_bytes = 0, bps = 0;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "isb"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x52494646) /* "RIFF" */
        goto fail;
    if (read_32bitLE(0x04,streamFile) + 0x08 != get_streamfile_size(streamFile))
        goto fail;
    if (read_32bitBE(0x08,streamFile) != 0x69736266) /* "isbf" */
        goto fail;

    /* some files have a companion .icb, seems to be a cue file pointing here */

    /* format is RIFF with many custom chunks, apparently for their DAW-like editor with
     * complex functions, but most seem always included by default and unused, and games
     * Psychonauts seems to use the format as a simple audio bank. Mass Effect (X360)
     * apparently uses ISACT, while Psychonauts Xbox/PS2 don't. */

    {
        off_t offset, max_offset, header_offset = 0;
        size_t header_size = 0;

        total_subsongs = 0; /* not specified */
        if (target_subsong == 0) target_subsong = 1;

        /* parse base RIFF */
        offset = 0x0c;
        max_offset = get_streamfile_size(streamFile);
        while (offset < max_offset) {
            uint32_t chunk_type = read_u32be(offset + 0x00,streamFile);
            uint32_t chunk_size = read_s32le(offset + 0x04,streamFile);
            offset += 0x08;

            switch(chunk_type) {
                case 0x4C495354: /* "LIST" */
                    if (read_u32be(offset, streamFile) != 0x73616D70) /* "samp" */
                        break; /* there are "bfob" LIST without data */

                    total_subsongs++;
                    if (target_subsong == total_subsongs && header_offset == 0) {
                        header_offset = offset;
                        header_size = chunk_size;
                    }
                    break;

                default: /* most are common chunks at the start that seem to contain defaults */
                    break;
            }

            //if (offset + chunk_size+0x01 <= max_offset && chunk_size % 0x02)
            //    chunk_size += 0x01;
            offset += chunk_size;
        }

        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
        if (header_offset == 0) goto fail;

        /* parse header inside LIST */
        offset = header_offset + 0x04;
        max_offset = offset + header_size;
        while (offset < max_offset) {
            uint32_t chunk_type = read_u32be(offset + 0x00,streamFile);
            uint32_t chunk_size = read_s32le(offset + 0x04,streamFile);
            offset += 0x08;

            switch(chunk_type) {
                case 0x7469746C: /* "titl" */
                    name_offset = offset;
                    name_size = chunk_size;
                    break;

                case 0x63686E6B: /* "chnk" */
                    channel_count = read_u32le(offset + 0x00, streamFile);
                    break;

                case 0x73696E66: /* "sinf" */
                    /* 0x00: null? */
                    /* 0x04: some value? */
                    sample_rate = read_u32le(offset + 0x08, streamFile);
                    pcm_bytes = read_u32le(offset + 0x0c, streamFile);
                    bps = read_u16le(offset + 0x10, streamFile);
                    /* 0x12: some value? */
                    break;

                case 0x636D7069: /* "cmpi" */
                    codec = read_u32le(offset + 0x00, streamFile);
                    if (read_u32le(offset + 0x04, streamFile) != codec) {
                        VGM_LOG("ISB: unknown compression repeat\n");
                        goto fail;
                    }
                    /* 0x08: extra value for some codecs? */
                    /* 0x0c: block size when codec is XBOX-IMA */
                    /* 0x10: null? */
                    /* 0x14: flags? */
                    break;

                case 0x64617461: /* "data" */
                    start_offset = offset;
                    stream_size = chunk_size;
                    break;

                default: /* most of the same default chunks */
                    break;
            }

            //if (offset + chunk_size+0x01 <= max_offset && chunk_size % 0x02)
            //    chunk_size += 0x01;
            offset += chunk_size;
        }

        if (start_offset == 0)
            goto fail;
    }


    /* some files are marked */
    loop_flag  = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WAF; //todo
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    switch(codec) {
        case 0x00:
            if (bps == 8) {
                vgmstream->coding_type = coding_PCM8_U;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = 0x01;
            }
            else {
                vgmstream->coding_type = coding_PCM16LE;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = 0x02;
            }
            vgmstream->num_samples = pcm_bytes_to_samples(stream_size, channel_count, bps); /* unsure about pcm_bytes */
            break;

        case 0x01:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = xbox_ima_bytes_to_samples(stream_size, channel_count); /* pcm_bytes has excess data */
            break;

#ifdef VGM_USE_VORBIS
        case 0x02:
            vgmstream->codec_data = init_ogg_vorbis(streamFile, start_offset, stream_size, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_OGG_VORBIS;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = pcm_bytes / channel_count / (bps/8);
            break;
#endif

        default: /* according to press releases ISACT may support WMA and XMA */
            VGM_LOG("ISB: unknown codec %i\n", codec);
            goto fail;
    }

    if (name_offset) { /* UTF16 but only uses lower bytes */
        if (name_size > STREAM_NAME_SIZE)
            name_size = STREAM_NAME_SIZE;
        read_string_utf16le(vgmstream->stream_name,name_size, name_offset, streamFile);
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
