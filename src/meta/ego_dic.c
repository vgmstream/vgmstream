#include "meta.h"
#include "../coding/coding.h"
#include "../util/reader_text.h"


/* DIC1 - from Codemaster's Ego engine 'dictionaries' [DiRT (PC), F1 2011 (PC), Race Driver Grid (PC)] */
VGMSTREAM* init_vgmstream_ego_dic(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sb = NULL;
    uint32_t stream_offset = 0, stream_size = 0, codec = 0;
    int channels = 0, loop_flag = 0, sample_rate = 0;


    /* checks */
    if (!is_id32be(0x00, sf, "DIC1"))
        return NULL;
    if (!check_extensions(sf, "dic"))
        return NULL;

    /* 0x04: hash/info? */
    /* 0x08: version ("0.46": DiRT, "2.00": DiRT 2+ (no diffs?) */
    int containers = read_s32le(0x0c,sf);

    int total_subsongs = 0;
    int target_subsong = sf->stream_index;
    if (target_subsong == 0) target_subsong = 1;

    char container_name[0x10+1];
    char track_name[0x10+1];
    char ext[0x04+1];
    {
        /* N containers (external files) that may have N tracks, so we'll map all to subsongs */

        bool track_found = false;
        uint32_t offset = 0x10;
        for (int i = 0; i < containers; i++) {
            uint32_t header_offset = read_u32le(offset + 0x00,sf);
            int tracks = read_s32le(offset + 0x04,sf);

            /* target in container */
            if (!track_found && target_subsong >= total_subsongs + 1 && target_subsong < total_subsongs + 1 + tracks) {
                read_string(container_name, 0x10, offset + 0x08, sf);

                int track_pos = target_subsong - total_subsongs - 1;
                uint32_t track_offset = header_offset + track_pos * 0x18;
                uint32_t curr_offset, flags, next_offset;

                curr_offset = read_u32le(track_offset + 0x00, sf);
                sample_rate = read_u16le(track_offset + 0x04, sf);
                flags       = read_u16le(track_offset + 0x06, sf);
                read_string(track_name, 0x10, track_offset + 0x08, sf);
                next_offset = read_u32le(track_offset + 0x18, sf); /* always exists as even after last track */

                /* 8ch seen in Race Driver Grid music*.WIM, 2ch in other music tracks */
                channels = ((flags >> 5) & 0x7) + 1;
                loop_flag = flags & (1<<4);
                /* flag 0x8000: ? (common) */
                /* others: not seen */

                stream_offset = curr_offset;
                stream_size = next_offset - curr_offset;

                /* after all tracks comes container size + extension/codec */
                uint32_t ext_offset = header_offset + tracks * 0x18 + 0x04;
                codec = read_u32be(ext_offset + 0x00, sf);
                read_string(ext, 0x04, ext_offset + 0x00, sf);

                track_found = true;
            }

            total_subsongs += tracks;
            offset += 0x18;
        }

        if (target_subsong > total_subsongs || total_subsongs <= 0) goto fail;
        if (!track_found) goto fail;
    }

    {
        char resource_name[255];
        snprintf(resource_name, sizeof(resource_name), "%s.%s", container_name, ext);

        sb = open_streamfile_by_filename(sf, resource_name);
        if (sb == NULL) {
            vgm_logi("DIC1: external file '%s' not found (put together)\n", resource_name);
            /* allow missing as silence since some game use huge .dic that is a bit hard to get */
            codec = 0xFFFFFFFF; //goto fail;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_DIC1;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;


    switch(codec) {
        case 0xFFFFFFFF: //fake
            vgmstream->coding_type = coding_SILENCE;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = sample_rate;
            break;

        case 0x57495000: //WIP\0
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples = pcm16_bytes_to_samples(stream_size, channels);
            break;

        case 0x57494D00: //WIM\0
            vgmstream->coding_type = coding_ULAW;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x01;

            vgmstream->num_samples = pcm8_bytes_to_samples(stream_size, channels);
            break;

        case 0x57494100: //WIA\0
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            
            vgmstream->num_samples = xbox_ima_bytes_to_samples(stream_size, channels);
            stream_offset += 0x04; /* all streams start with 0xFFFFFFFF, may be some delimiter */
            break;

#ifdef VGM_USE_VORBIS
        case 0x57494F00: //WIO\0
            vgmstream->codec_data = init_ogg_vorbis(sb, stream_offset, stream_size, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_OGG_VORBIS;
            vgmstream->layout_type = layout_none;

            ogg_vorbis_get_samples(vgmstream->codec_data, &vgmstream->num_samples);
            break;
#endif

        default:
            VGM_LOG("DIC1: unknown codec\n");
            goto fail;
    }
    snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s.%s%s/%s", container_name, ext, sb ? "" : "[MISSING]", track_name);

    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    if (!vgmstream_open_stream(vgmstream, sb, stream_offset))
        goto fail;

    close_streamfile(sb);
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    close_streamfile(sb);
    return NULL;
}
