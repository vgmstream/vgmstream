#include "meta.h"
#include "../coding/coding.h"


/* SXD - Sony/SCE's SNDX lib format (cousin of SGXD) [Gravity Rush, Freedom Wars, Soul Sacrifice PSV] */
VGMSTREAM * init_vgmstream_sxd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;
    off_t start_offset, chunk_offset, first_offset = 0x60, name_offset = 0;
    size_t chunk_size, stream_size = 0;

    int is_separate;
    int loop_flag, channels, codec;
    int sample_rate, num_samples, loop_start_sample, loop_end_sample;
    uint32_t at9_config_data = 0;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* check extension, case insensitive */
    /* .sxd: header+data (SXDF), .sxd1: header (SXDF) + .sxd2 = data (SXDS) */
    if (!check_extensions(streamFile,"sxd,sxd2")) goto fail;
    is_separate = !check_extensions(streamFile,"sxd");

    /* sxd1+sxd2: use sxd1 as header; otherwise use the current file as header */
    if (is_separate) {
        if (read_32bitBE(0x00,streamFile) != 0x53584453) /* "SXDS" */
            goto fail;
        streamHeader = open_stream_ext(streamFile, "sxd1");
        if (!streamHeader) goto fail;
    } else {
        streamHeader = streamFile;
    }
    if (read_32bitBE(0x00,streamHeader) != 0x53584446) /* "SXDF" */
        goto fail;


    /* typical chunks: NAME, WAVE and many control chunks (SXDs don't need to contain any sound data) */
    if (!find_chunk_le(streamHeader, 0x57415645,first_offset,0, &chunk_offset,&chunk_size)) goto fail; /* "WAVE" */

    /* check multi-streams (usually only in SFX containers) */
    total_subsongs = read_32bitLE(chunk_offset+0x04,streamHeader);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    /* read stream header */
    {
        off_t table_offset, header_offset, stream_offset;

        /* get target offset using table of relative offsets within WAVE */
        table_offset  = chunk_offset + 0x08 + 4*(target_subsong-1);
        header_offset = table_offset + read_32bitLE(table_offset,streamHeader);

        /* 0x00(4): type/location? (00/01=sxd/RAM?, 02/03=sxd2/stream?) */
        codec         = read_8bit   (header_offset+0x04,streamHeader);
        channels      = read_8bit   (header_offset+0x05,streamHeader);
        sample_rate   = read_32bitLE(header_offset+0x08,streamHeader);
        /* 0x0c(4): unknown size? (0x4000/0x3999/0x3333/etc, not related to extra data) */
        /* 0x10(4): ? + volume? + pan? (can be 0 for music) */
        num_samples       = read_32bitLE(header_offset+0x14,streamHeader);
        loop_start_sample = read_32bitLE(header_offset+0x18,streamHeader);
        loop_end_sample   = read_32bitLE(header_offset+0x1c,streamHeader);
        stream_size       = read_32bitLE(header_offset+0x20,streamHeader);
        stream_offset     = read_32bitLE(header_offset+0x24,streamHeader);

        /* Extra data, variable sized and uses some kind of TLVs (HEVAG's is optional and much smaller).
         * One tag seems to add a small part of the ATRAC9 data, for RAM preloding I guess. */
        if (codec == 0x42) {
            off_t extra_offset = header_offset+0x28;
            off_t max_offset = chunk_offset + chunk_size;

            /* manually try to find certain tag, no idea about the actual format
             * (most variable in Soul Sacrifice; extra size isn't found in the SXD AFAIK) */
            while (extra_offset < max_offset) {
                uint32_t tag = read_32bitBE(extra_offset, streamHeader);
                if (tag == 0x0A010000 || tag == 0x0A010600) {
                    at9_config_data = read_32bitLE(extra_offset+0x04,streamHeader); /* yes, LE */
                    break;
                }

                extra_offset += 0x04;
            }
            if (!at9_config_data)
                goto fail;
        }

        loop_flag = loop_start_sample != -1 && loop_end_sample != -1;

        /* from current offset in sxd, absolute in sxd2 */
        if (is_separate) {
            start_offset = stream_offset;
        } else {
            start_offset = header_offset+0x24 + stream_offset;
        }
    }

    /* get stream name (NAME is tied to REQD/cues, and SFX cues repeat WAVEs, but should work ok for streams) */
    if (is_separate && find_chunk_le(streamHeader, 0x4E414D45,first_offset,0, &chunk_offset,NULL)) { /* "NAME" */
        /* table: relative offset (32b) + hash? (32b) + cue index (32b) */
        int i;
        int num_entries = read_16bitLE(chunk_offset+0x04,streamHeader); /* can be bigger than streams */
        for (i = 0; i < num_entries; i++) {
            uint32_t index = (uint32_t)read_32bitLE(chunk_offset+0x08 + 0x08 + i*0x0c,streamHeader);
            if (index+1 == target_subsong) {
                name_offset = chunk_offset+0x08 + 0x00 + i*0x0c + read_32bitLE(chunk_offset+0x08 + 0x00 + i*0x0c,streamHeader);
                break;
            }
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_SXD;
    if (name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, name_offset,streamHeader);

    switch (codec) {
        case 0x20:      /* PS-ADPCM [Hot Shots Golf: World Invitational (Vita) sfx] */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;
            break;

        case 0x21:      /* HEVAG [Gravity Rush (Vita) sfx] */
            vgmstream->coding_type = coding_HEVAG;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;
            break;

#ifdef VGM_USE_ATRAC9
        case 0x42: {    /* ATRAC9 [Soul Sacrifice (Vita), Freedom Wars (Vita)] */
            atrac9_config cfg = {0};

            cfg.channels = vgmstream->channels;
            cfg.config_data = at9_config_data;

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
      //case 0x28:      /* dummy codec? (found with 0 samples) [Hot Shots Golf: World Invitational (Vita) sfx] */
        default:
            VGM_LOG("SXD: unknown codec 0x%x\n", codec);
            goto fail;
    }


    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    if (is_separate && streamHeader) close_streamfile(streamHeader);
    return vgmstream;

fail:
    if (is_separate && streamHeader) close_streamfile(streamHeader);
    close_vgmstream(vgmstream);
    return NULL;
}
