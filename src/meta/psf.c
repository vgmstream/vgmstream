#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* PSF single - Pivotal games single segment (external in some PC/Xbox or inside bigfiles) [The Great Escape, Conflict series] */
VGMSTREAM* init_vgmstream_psf_single(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate, rate_value, interleave;
    uint32_t psf_config;
    uint8_t flags;
    size_t data_size;
    coding_t codec;


    /* checks */
    if ((read_u32be(0x00,sf) & 0xFFFFFF00) != get_id32be("PSF\0"))
        goto fail;
    /* .psf: actual extension
     * .swd: bigfile extension */
    if (!check_extensions(sf, "psf,swd"))
        goto fail;

    flags = read_8bit(0x03,sf);
    switch(flags) {
        case 0xC0: /* [The Great Escape (PS2), Conflict: Desert Storm (PS2)] */
        case 0x40: /* [The Great Escape (PS2)] */
        case 0xA1: /* [Conflict: Desert Storm 2 (PS2)] */
        case 0x21: /* [Conflict: Desert Storm 2 (PS2), Conflict: Global Storm (PS2)] */
      //case 0x22: /* [Conflict: Vietman (PS2)] */ //todo weird size value, stereo, only one found
            codec = coding_PSX;
            interleave = 0x10;

            channel_count = 2;
            if (flags == 0x21 || flags == 0x40)
                channel_count = 1;
            start_offset = 0x08;
            break;

        case 0x80: /* [The Great Escape (PC/Xbox), Conflict: Desert Storm (Xbox/GC), Conflict: Desert Storm 2 (Xbox)] */
        case 0x81: /* [Conflict: Desert Storm 2 (Xbox), Conflict: Vietnam (Xbox)] */
        case 0x01: /* [Conflict: Global Storm (Xbox)] */
            codec = coding_PSX_pivotal;
            interleave = 0x10;

            channel_count = 2;
            if (flags == 0x01)
                channel_count = 1;
            start_offset = 0x08;
            break;

        case 0xD1: /* [Conflict: Desert Storm 2 (GC)] */
            codec = coding_NGC_DSP;
            interleave = 0x08;

            channel_count = 2;
            start_offset = 0x08 + 0x60 * channel_count;
            break;

        default:
            goto fail;
    }

    loop_flag = 0;

    psf_config = read_u32le(0x04, sf);

    /* pitch/cents? */
    rate_value = (psf_config >> 20) & 0xFFF;
    switch(rate_value) {
        case 3763: sample_rate = 44100; break;
        case 1365: sample_rate = 16000; break;
        case 940:  sample_rate = 11050; break;
        case 460:  sample_rate = 5000;  break;
        default:
            VGM_LOG("PSF: unknown rate value %x\n", rate_value);
            sample_rate = rate_value * 11.72; /* not exact but works well enough */
            break;
    }

    data_size = (psf_config & 0xFFFFF) * (interleave * channel_count); /* in blocks */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PSF;
    vgmstream->sample_rate = sample_rate;

    switch(codec) {
        case coding_PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = ps_bytes_to_samples(data_size, channel_count);
            break;

        case coding_PSX_pivotal:
            vgmstream->coding_type = coding_PSX_pivotal;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = ps_cfg_bytes_to_samples(data_size, 0x10, channel_count);
            break;

        case coding_NGC_DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;
            /* has standard DSP headers at 0x08 */
            dsp_read_coefs_be(vgmstream,sf,0x08+0x1c,0x60);
            dsp_read_hist_be (vgmstream,sf,0x08+0x40,0x60);

            vgmstream->num_samples = read_32bitBE(0x08, sf);//dsp_bytes_to_samples(data_size, channel_count);
            break;

        default:
            goto fail;
    }

    vgmstream->stream_size = data_size;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}



/* PSF segmented - Pivotal games multiple segments (external in some PC/Xbox or inside bigfiles) [The Great Escape, Conflict series] */
VGMSTREAM* init_vgmstream_psf_segmented(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    segmented_layout_data *data = NULL;
    int i,j, sequence_count = 0, loop_flag = 0, loop_start = 0, loop_end = 0;
    int sequence[512] = {0};
    off_t offsets[512] = {0};
    int total_subsongs = 0, target_subsong = sf->stream_index;
    char stream_name[STREAM_NAME_SIZE] = {0};
    size_t stream_size = 0;


    /* checks */
    if (!is_id32be(0x00,sf, "PSF\x60") &&  /* [The Great Escape (PC/Xbox/PS2), Conflict: Desert Storm (Xbox/GC)] */
        !is_id32be(0x00,sf, "PSF\x31"))    /* [Conflict: Desert Storm 2 (Xbox/GC/PS2), Conflict: Global Terror (Xbox)] */
        goto fail;

    /* .psf: actual extension
     * .swd: bigfile extension */
    if (!check_extensions(sf, "psf,swd"))
        goto fail;



    /* transition table info:
     * 0x00: offset
     * 0x04: 0x02*4 next segment points (one per track)
     * (xN segments)
     *
     * There are 4 possible tracks, like: normal, tension, action, high action. Segment 0 has tracks'
     * entry segments (where 1=first, right after segment 0), and each segment has a link point to next
     * (or starting) segment of any of other tracks. Thus, follow point 1/2/3/4 to playtrack 1/2/3/4
     * (points also loop back). It's designed to go smoothly between any tracks (1>3, 4>1, etc),
     * so sometimes "step" segments (that aren't normally played) are used.
     * If a track doesn't exist it may keep repeating silent segments, but still defines points.
     *
     * ex. sequence could go like this:
     * (read segment 0 track1 entry): 1
     * - track0: 1>2>3>4>5>6>7>8>1>2>3, then to track2 goes 3>15>9
     * - track1: 9>10>11>12>13>14>9>10, then to track4 goes 10>33
     * - track2: 33>34>35>36>30>31>32>33, then to track1 goes 33>3
     * - track3: 3>4>5>6... (etc)
     *
     * Well make a sequence based on target subsong:
     * - 1: tracks mixed with transitions, looping back to track1 (only first is used in .sch so we want all)
     * - 2~5: track1~4 looping back to themselves
     * - 6+: single segment (where 6=first segment) to allow free mixes
     */
    {
        int track[4][255] = {0};
        int count[4] = {0};
        int current_track, current_point, next_point, repeat_point;
        int transition_count = read_s32le(0x04, sf);

        total_subsongs = 1 + 4 + (transition_count - 1);
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;


        if (target_subsong == 1) {
            current_track = 0; /* start from first track, will move to others automatically */

            snprintf(stream_name,sizeof(stream_name), "full");
        }
        else if (target_subsong <= 1+4) {
            current_track = (target_subsong-1) - 1; /* where 0 = first track */

            snprintf(stream_name,sizeof(stream_name), "track%i", (current_track+1));
        }
        else {
            int segment = target_subsong - 1 - 4; /* where 1 = first segment */

            sequence[0] = segment;
            sequence_count = 1;
            current_track = -1;

            /* show transitions to help with ordering */
            track[0][0] = read_16bitLE(0x08 + 0x0c*segment + 0x04, sf);
            track[1][0] = read_16bitLE(0x08 + 0x0c*segment + 0x06, sf);
            track[2][0] = read_16bitLE(0x08 + 0x0c*segment + 0x08, sf);
            track[3][0] = read_16bitLE(0x08 + 0x0c*segment + 0x0a, sf);
            snprintf(stream_name,sizeof(stream_name), "segment%03i to %03i/%03i/%03i/%03i", segment,track[0][0],track[1][0],track[2][0],track[3][0]);
        }

        /* find target sequence */
        current_point = 0; /* segment 0 has track entry points */
        while (sequence_count < 512 && current_track >= 0) {

            next_point = read_16bitLE(0x08 + 0x0c*current_point + 0x04 + 0x02*current_track, sf);

            /* find if next point repeats in our current track */
            repeat_point = -1;
            for (i = 0; i < count[current_track]; i++) {

                if (track[current_track][i] == next_point) {
                    repeat_point = i;
                    break;
                }
            }

            /* set loops and end sequence */
            if (repeat_point >= 0) {
                if (target_subsong == 1) {
                    /* move to next track and change transition to next track too */
                    current_track++;

                    /* to loop properly we set loop end in track3 and loop start in track0
                     * when track3 ends and move to track0 could have a transition segment
                     * before actually looping track0, so we do this in 2 steps */

                    if (loop_flag) { /* 2nd time repeat is found = loop start in track0 */
                        loop_start = repeat_point;
                        break; /* sequence fully done */
                    }

                    if (current_track > 3) { /* 1st time repeat is found = loop end in track3 */
                        current_track = 0;
                        loop_flag = 1;
                    }

                    next_point = read_16bitLE(0x08 + 0x0c*current_point + 0x04 + 0x02*current_track, sf);

                    if (loop_flag) {
                        loop_end = sequence_count; /* this points to the next_point that will be added below */
                    }
                }
                else {
                    /* end track N */
                    loop_flag = 1;
                    loop_start = repeat_point;
                    loop_end = sequence_count - 1;
                    break;
                }
            }


            /* separate track info to find repeated points (since some transitions are common for all tracks) */
            track[current_track][count[current_track]] = next_point;
            count[current_track]++;

            sequence[sequence_count] = next_point;
            sequence_count++;

            current_point = next_point;
        }

        if (sequence_count >= 512 || count[current_track] >=  512)
            goto fail;
    }


    /* build segments */
    data = init_layout_segmented(sequence_count);
    if (!data) goto fail;

    for (i = 0; i < sequence_count; i++) {
        off_t psf_offset;
        size_t psf_size;
        int old_psf = -1;

        psf_offset = read_u32le(0x08 + sequence[i]*0x0c + 0x00, sf);
        psf_size = get_streamfile_size(sf) - psf_offset; /* not ok but meh */

        /* find repeated sections (sequences often repeat PSFs) */
        offsets[i] = psf_offset;
        for (j = 0; j < i; j++) {
            if (offsets[j] == psf_offset) {
                old_psf = j;
                break;
            }
        }

        /* reuse repeated VGMSTREAMs to improve memory and bitrate calcs a bit */
        if (old_psf >= 0) {
            data->segments[i] = data->segments[old_psf];
        }
        else {
            temp_sf = setup_subfile_streamfile(sf, psf_offset, psf_size, "psf");
            if (!temp_sf) goto fail;

            data->segments[i] = init_vgmstream_psf_single(temp_sf);
            if (!data->segments[i]) goto fail;

            stream_size += data->segments[i]->stream_size; /* only non-repeats */
        }
    }

    /* setup VGMSTREAMs */
    if (!setup_layout_segmented(data))
        goto fail;
    vgmstream = allocate_segmented_vgmstream(data,loop_flag, loop_start, loop_end);
    if (!vgmstream) goto fail;

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    strcpy(vgmstream->stream_name, stream_name);

    return vgmstream;
fail:
    if (!vgmstream) free_layout_segmented(data);
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

/* ***************************************************** */

static VGMSTREAM* init_vgmstream_psf_pfsm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate = 0,  rate_value = 0, interleave, big_endian;
    size_t data_size;
    coding_t codec;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;

    /* standard:
     * 0x00: -1/number (lang?)
     * 0x04: config/size?
     * 0x08: channel size? only ok for PSX-pivotal
     * 0x0c: sample rate or rate_value
     * 0x0e: 0x4=PSX-pivotal or 0xFF=PSX
     * 0x0f: name size (0xCC/FF=null)
     * 0x10: data
     *
     * GC is similar with 0x20-align between some fields
     */

    /* checks */
    if (!is_id32be(0x00,sf, "PFSM") &&
        !is_id32le(0x00,sf, "PFSM"))
        goto fail;
    //if (!check_extensions(sf, "psf"))
    //    goto fail;

    big_endian = is_id32le(0x00,sf, "PFSM");
    if (big_endian) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    }
    else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    loop_flag = 0;


    if (big_endian && read_32bit(0x50, sf) != 0) { /* GC */
        codec = coding_NGC_DSP;
        interleave = 0x08;
        channel_count = 1;
        rate_value = (uint16_t)read_16bit(0x48, sf);

        start_offset = 0x60 + 0x60 * channel_count;
    }
    else if (big_endian) { /* GC */
        codec = coding_PCM16BE;
        interleave = 0x02;
        channel_count = 1;
        rate_value = (uint16_t)read_16bit(0x48, sf);

        start_offset = 0x60;
    }
    else if ((uint8_t)read_8bit(0x16, sf) == 0xFF) { /* PS2 */
        codec = coding_PSX;
        interleave = 0x10;
        rate_value = (uint16_t)read_16bit(0x14, sf);
        channel_count = 1;

        start_offset = 0x18;
    }
    else { /* PC/Xbox, some PS2/GC */
        codec = coding_PSX_pivotal;
        interleave = 0x10;
        sample_rate = (uint16_t)read_16bit(0x14, sf);
        channel_count = 1;

        start_offset = 0x18;
    }

    data_size = get_streamfile_size(sf) - start_offset;

    /* pitch/cents? */
    if (sample_rate == 0) {
        /* pitch/cents? */
        switch(rate_value) {
            case 3763: sample_rate = 44100; break;
            case 1365: sample_rate = 16000; break;
            case 940:  sample_rate = 11050; break;
            case 460:  sample_rate = 5000;  break;
            default:
                VGM_LOG("PSF: unknown rate value %x\n", rate_value);
                sample_rate = rate_value * 11.72; /* not exact but works well enough */
                break;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PSF;
    vgmstream->sample_rate = sample_rate;

    switch(codec) {
        case coding_PCM16BE:
            vgmstream->coding_type = coding_PCM16BE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = pcm_bytes_to_samples(data_size, channel_count, 16);
            break;

        case coding_PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = ps_bytes_to_samples(data_size, channel_count);
            break;

        case coding_PSX_pivotal:
            vgmstream->coding_type = coding_PSX_pivotal;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = ps_cfg_bytes_to_samples(data_size, 0x10, channel_count);
            break;

        case coding_NGC_DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;
            /* has standard DSP headers at 0x08 */
            dsp_read_coefs_be(vgmstream,sf,0x60+0x1c,0x60);
            dsp_read_hist_be (vgmstream,sf,0x60+0x40,0x60);

            vgmstream->num_samples = read_32bitBE(0x60, sf);//dsp_bytes_to_samples(data_size, channel_count);
            break;

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}



typedef enum { UNKNOWN, IMUS, PFST, PFSM } sch_type;
#define SCH_STREAM "Stream.swd"


/* SCH - Pivotal games multi-audio container [The Great Escape, Conflict series] */
VGMSTREAM* init_vgmstream_sch(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* external_sf = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t skip = 0, chunk_offset, target_offset = 0, header_offset, subfile_offset = 0;
    size_t file_size, chunk_padding, target_size = 0, subfile_size = 0;
    int big_endian;
    int total_subsongs = 0, target_subsong = sf->stream_index;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    sch_type target_type = UNKNOWN;
    char stream_name[STREAM_NAME_SIZE] ={0};


    /* checks */
    if (is_id32be(0x00,sf, "HDRS")) /* "HDRSND" (found on later games) */
        skip = 0x0E;
    if (!is_id32be(skip + 0x00,sf, "SCH\0") &&
        !is_id32le(skip + 0x00,sf, "SCH\0"))    /* (BE consoles) */
        goto fail;

    if (!check_extensions(sf, "sch"))
        goto fail;


    /* chunked format (id+size, GC pads to 0x20 and uses BE/inverted ids):
     * no other info so total subsongs would be count of usable chunks
     * (offsets are probably in level .dat files) */
    big_endian = (is_id32le(skip + 0x00,sf, "SCH\0"));
    if (big_endian) {
        read_32bit = read_32bitBE;
        chunk_padding = 0x18;
    }
    else {
        read_32bit = read_32bitLE;
        chunk_padding = 0;
    }

    file_size = get_streamfile_size(sf);
    if (read_32bit(skip + 0x04,sf) + skip + 0x08 + chunk_padding < file_size) /* sometimes padded */
        goto fail;

    if (target_subsong == 0) target_subsong = 1;

    chunk_offset = skip + 0x08 + chunk_padding;

    /* get all files*/
    while (chunk_offset < file_size) {
        uint32_t chunk_id   = read_32bitBE(chunk_offset + 0x00,sf);
        uint32_t chunk_size = read_32bit(chunk_offset + 0x04,sf);
        sch_type current_type = UNKNOWN;

        switch(chunk_id) {
            case 0x494D5553: /* "IMUS" (TGE PC/Xbox only) */
                current_type = IMUS;
                break;

            case 0x54534650:
            case 0x50465354: /* "PFST" */
                current_type = PFST;
                break;

            case 0x4D534650:
            case 0x5046534D: /* "PFSM" */
                current_type = PFSM;
                break;

            case 0x4B4E4142:
            case 0x42414E4B: /* "BANK" */
                /* unknown format (variable size), maybe config for entry numbers */
                break;
            case 0x424C4F4B: /* "BLOK" [Conflict: Desert Storm (Xbox)] */
                /* some ids or something? */
                break;

            default:
                VGM_LOG("SCH: unknown chunk at %lx\n", chunk_offset);
                goto fail;
        }

        if (current_type != UNKNOWN)
            total_subsongs++;

        if (total_subsongs == target_subsong && target_type == UNKNOWN) {
            target_type = current_type;
            target_offset = chunk_offset;
            target_size = 0x08 + chunk_padding + chunk_size;
        }

        chunk_offset += 0x08 + chunk_padding + chunk_size;
    }

    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    if (target_size == 0) goto fail;

    header_offset = target_offset + 0x08 + chunk_padding;

    //;VGM_LOG("SCH: offset=%lx, size=%x\n",target_offset, target_size);

    switch(target_type) {
        case IMUS: { /* external segmented track */
            STREAMFILE *psf_sf;
            uint8_t name_size;
            char name[255];

            /* 0x00: config/size?
             * 0x04: name size
             * 0x05: segments
             * 0x06: ?
             * 0x08: relative path to .psf
             * 0xNN: segment table (same as .psf)
             */

            name_size = read_u8(header_offset + 0x04, sf);
            read_string(name,name_size, header_offset + 0x08, sf);

            /* later games have name but actually use bigfile [Conflict: Global Storm (Xbox)] */
            if (read_u8(header_offset + 0x07, sf) == 0xCC) {
                external_sf = open_streamfile_by_filename(sf, SCH_STREAM);
                if (!external_sf) {
                    vgm_logi("SCH: external file '%s' not found (put together)\n", SCH_STREAM);
                    goto fail;
                }

                subfile_offset = read_32bit(header_offset + 0x08 + name_size, sf);
                subfile_size = get_streamfile_size(external_sf) - subfile_offset; /* not ok but meh */

                temp_sf = setup_subfile_streamfile(external_sf, subfile_offset,subfile_size, "psf");
                if (!temp_sf) goto fail;

                psf_sf = temp_sf;
            }
            else {
                external_sf = open_streamfile_by_filename(sf, name);
                if (!external_sf) {
                    vgm_logi("SCH: external file '%s' not found (put together)\n", name);
                    goto fail;
                }

                psf_sf = external_sf;
            }

            vgmstream = init_vgmstream_psf_segmented(psf_sf);
            if (!vgmstream) {
                vgmstream = init_vgmstream_psf_single(psf_sf);
                if (!vgmstream) goto fail;
            }

            snprintf(stream_name,sizeof(stream_name), "%s-%s" , "IMUS", name);
            break;
        }

        case PFST: { /* external track */
            STREAMFILE *psf_sf;
            uint8_t name_size;
            char name[255];

            if (chunk_padding == 0 && target_size > 0x08 + 0x0c) { /* TGE PC/Xbox version */
                /* 0x00: -1/0
                 * 0x04: config/size?
                 * 0x08: channel size
                 * 0x0c: sample rate? (differs vs PSF)
                 * 0x0e: 4?
                 * 0x0f: name size
                 * 0x10: name
                 */

                /* later games have name but actually use bigfile [Conflict: Global Storm (Xbox)] */
                if ((read_32bitBE(header_offset + 0x14, sf) & 0x0000FFFF) == 0xCCCC) {
                    name_size = read_8bit(header_offset + 0x13, sf);
                    read_string(name,name_size, header_offset + 0x18, sf);

                    external_sf = open_streamfile_by_filename(sf, SCH_STREAM);
                    if (!external_sf) {
                        vgm_logi("SCH: external file '%s' not found (put together)\n", SCH_STREAM);
                        goto fail;
                    }

                    subfile_offset = read_32bit(header_offset + 0x0c, sf);
                    subfile_size = get_streamfile_size(external_sf) - subfile_offset; /* not ok but meh */

                    temp_sf = setup_subfile_streamfile(external_sf, subfile_offset,subfile_size, "psf");
                    if (!temp_sf) goto fail;

                    psf_sf = temp_sf;
                }
                else {
                    name_size = read_8bit(header_offset + 0x0f, sf);
                    read_string(name,name_size, header_offset + 0x10, sf);

                    external_sf = open_streamfile_by_filename(sf, name);
                    if (!external_sf) {
                        vgm_logi("SCH: external file '%s' not found (put together)\n", name);
                        goto fail;
                    }

                    psf_sf = external_sf;
                }
            }
            else if (chunk_padding) {
                strcpy(name, "STREAM.SWD"); /* fixed */

                /* 0x00: -1
                 * 0x04: config/size?
                 * 0x08: .swd offset
                 */
                external_sf = open_streamfile_by_filename(sf, name);
                if (!external_sf) goto fail;

                subfile_offset = read_32bit(header_offset + 0x24, sf);
                subfile_size = get_streamfile_size(external_sf) - subfile_offset; /* not ok but meh */

                temp_sf = setup_subfile_streamfile(external_sf, subfile_offset,subfile_size, "psf");
                if (!temp_sf) goto fail;

                psf_sf = temp_sf;
            }
            else { /* others */
                strcpy(name, "STREAM.SWD"); /* fixed */

                /* 0x00: -1
                 * 0x04: config/size?
                 * 0x08: .swd offset
                 */
                external_sf = open_streamfile_by_filename(sf, name);
                if (!external_sf) goto fail;

                subfile_offset = read_32bit(header_offset + 0x08, sf);
                subfile_size = get_streamfile_size(external_sf) - subfile_offset; /* not ok but meh */

                temp_sf = setup_subfile_streamfile(external_sf, subfile_offset,subfile_size, "psf");
                if (!temp_sf) goto fail;

                psf_sf = temp_sf;
            }

            vgmstream = init_vgmstream_psf_segmented(psf_sf);
            if (!vgmstream) {
                vgmstream = init_vgmstream_psf_single(psf_sf);
                if (!vgmstream) goto fail;
            }

            snprintf(stream_name,sizeof(stream_name), "%s-%s" , "PFST", name);
            break;
        }

        case PFSM:
            /* internal sound */

            temp_sf = setup_subfile_streamfile(sf, target_offset,target_size, NULL);
            if (!temp_sf) goto fail;

            vgmstream = init_vgmstream_psf_pfsm(temp_sf);
            if (!vgmstream) goto fail;

            snprintf(stream_name,sizeof(stream_name), "%s" , "PFSM");
            break;

        default: /* target not found */
            goto fail;
    }

    vgmstream->num_streams = total_subsongs;
    strcpy(vgmstream->stream_name, stream_name);

    close_streamfile(temp_sf);
    close_streamfile(external_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_streamfile(external_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
