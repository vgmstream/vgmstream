#include "meta.h"
#include "../util/chunks.h"
#include "../coding/coding.h"

static int get_loop_points(STREAMFILE* sf, uint32_t cue_offset, uint32_t cue_size, uint32_t list_offset, uint32_t list_size, int* p_loop_start, int* p_loop_end);

/* Jade RIFF - from Ubisoft Jade engine games [Beyond Good & Evil (multi), Rayman Raving Rabbids 1/2 (multi)] */
VGMSTREAM* init_vgmstream_ubi_jade(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t fmt_offset = 0, fmt_size = 0, data_offset = 0, data_size = 0;
    uint32_t cue_offset = 0, cue_size = 0, list_offset = 0, list_size = 0;
    int loop_flag = 0, channels = 0, sample_rate = 0, codec = 0, block_size = 0;
    int loop_start = 0, loop_end = 0;
    int is_jade_v2 = 0;


    /* checks */
    if (!is_id32be(0x00,sf, "RIFF"))
        goto fail;
    if (read_u32le(0x04,sf) + 0x04 + 0x04 != get_streamfile_size(sf))
        goto fail;
    if (!is_id32be(0x08,sf, "WAVE"))
        goto fail;

    /* .waa: ambiances / .wam: music / .wac: sfx / .wad: dialogs (usually)
     * .wav: Beyond Good & Evil HD (PS3) */
    if (!check_extensions(sf,"waa,wac,wad,wam,wav,lwav"))
        goto fail;

    /* a slightly twisted RIFF with custom codecs */

    /* parse chunks (reads once linearly) */
    {
        chunk_t rc = {0};

        rc.current = 0x0c;
        while (next_chunk(&rc, sf)) {

            switch(rc.type) {
                case 0x666d7420: /* "fmt " */
                    fmt_offset = rc.offset;
                    fmt_size = rc.size;

                    if (fmt_size < 0x10) /* min 0x10: MSF, 0x12: common, 0x32: MSADPCM */
                        goto fail;
                    codec       = read_u16le(fmt_offset+0x00,sf);
                    channels    = read_u16le(fmt_offset+0x02,sf);
                    sample_rate = read_s32le(fmt_offset+0x04,sf);
                    block_size  = read_u16le(fmt_offset+0x0c,sf);
                    /* 0x08: average bytes, 0x0e: bps, etc */
                    break;

                case 0x64617461: /* "data" */
                    data_offset = rc.offset;
                    data_size = rc.size;
                    break;

                case 0x63756520: /* "cue ": catches PC Rabbids (hopefully) */
                    is_jade_v2 = 1;
                    cue_offset = rc.offset;
                    cue_size = rc.size;
                    break;

                case 0x66616374: /* "fact" */
                    /* ignore LyN RIFF (needed as codec 0xFFFE is reused, and Jade doesn't set "fact") */
                    //if (rc.size == 0x10 && !is_id32be(rc.offset + 0x04, sf, "LyN "))
                    //    goto fail; /* parsed elsewhere */
                    goto fail;

                case 0x4C495354: /* "LIST": labels (rare) */
                    list_offset = rc.offset;
                    list_size = rc.size;
                    break;

                default:
                    /* unknown chunk: must be another RIFF */
                    goto fail;
            }
        }
    }

    if (!fmt_offset || !fmt_size || !data_offset || !data_size)
        goto fail;

    /* autodetect Jade "v2", uses a different interleave [Rayman Raving Rabbids (PS2/Wii)] */
    switch(codec) {
        case 0xFFFF: { /* PS2 */
            int i;

            /* half interleave check as there is no flag (ends with the PS-ADPCM stop frame) */
            for (i = 0; i < channels; i++) {
                uint32_t end_frame = data_offset + (data_size / channels) * (i+1) - 0x10;
                if (read_u32be(end_frame+0x00,sf) != 0x07007777 ||
                    read_u32be(end_frame+0x04,sf) != 0x77777777 ||
                    read_u32be(end_frame+0x08,sf) != 0x77777777 ||
                    read_u32be(end_frame+0x0c,sf) != 0x77777777) {
                    is_jade_v2 = 1;
                    break;
                }
            }
            break;
        }

        case 0xFFFE: /* GC/Wii */
            is_jade_v2 = (read_u16le(fmt_offset+0x10,sf) == 0); /* extra data size (0x2e*channels) */
            break;

        default:
            break;
    }

    if (is_jade_v2) {
        loop_flag = get_loop_points(sf, cue_offset, cue_size, list_offset, list_size, &loop_start, &loop_end); /* loops in "LIST" */
    }
    else {
        /* BG&E files don't contain looping information, so the looping is done by extension.
         * wam and waa contain ambient sounds and music, so often they contain looped music.
         * Later, if the file is too short looping will be disabled. */
        loop_flag = check_extensions(sf,"waa,wam");
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_JADE;
    vgmstream->sample_rate = sample_rate;
    if (is_jade_v2) {
        vgmstream->loop_start_sample = loop_start;
        vgmstream->loop_end_sample = loop_end;
    }

    switch(codec) {

        case 0x0069: /* Xbox */
            /* Peter Jackson's King Kong uses 0x14 (other versions don't) */
            if (fmt_size != 0x12 && fmt_size != 0x14) goto fail;
            if (block_size != 0x24*channels) goto fail;

            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size, channels);
            if (!is_jade_v2) {
                vgmstream->loop_start_sample = 0;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }

            break;

        case 0xFFFF: /* PS2 */
            if (fmt_size != 0x12) goto fail;
            if (block_size != 0x10) goto fail;

            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;

            if (is_jade_v2) {
                vgmstream->interleave_block_size = 0x6400;
                if (vgmstream->interleave_block_size)
                    vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size*vgmstream->channels)) / vgmstream->channels;
            }
            else {
                vgmstream->interleave_block_size = data_size / channels;
            }

            vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
            if (!is_jade_v2) {
                vgmstream->loop_start_sample = 0;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }

            break;

        case 0xFFFE: /* GC/Wii */
            if (fmt_size != 0x12) goto fail;
            if (block_size != 0x08) goto fail;

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;

            vgmstream->num_samples = dsp_bytes_to_samples(data_size, channels);
            if (!is_jade_v2) {
                vgmstream->loop_start_sample = 0;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }

            /* coefs / interleave */
            if (is_jade_v2) {
                vgmstream->interleave_block_size = 0x6400;
                if (vgmstream->interleave_block_size)
                    vgmstream->interleave_last_block_size = ((data_size % (vgmstream->interleave_block_size*vgmstream->channels))/2+7)/8*8;

                {
                    static const int16_t coef[16] = { /* default Ubisoft coefs, from ELF */
                            0x04ab,0xfced,0x0789,0xfedf,0x09a2,0xfae5,0x0c90,0xfac1,
                            0x084d,0xfaa4,0x0982,0xfdf7,0x0af6,0xfafa,0x0be6,0xfbf5
                    };
                    int i, ch;

                    for (ch = 0; ch < channels; ch++) {
                        for (i = 0; i < 16; i++) {
                            vgmstream->ch[ch].adpcm_coef[i] = coef[i];
                        }
                    }
                }
            }
            else {
                /* has extra 0x2e coefs before each channel, not counted in data_size */
                vgmstream->interleave_block_size = (data_size + 0x2e*channels) / channels;

                dsp_read_coefs_be(vgmstream, sf, data_offset+0x00, vgmstream->interleave_block_size);
                dsp_read_hist_be (vgmstream, sf, data_offset+0x20, vgmstream->interleave_block_size);
                data_offset += 0x2e;
            }
            break;

        case 0x0002: /* PC */
            if (fmt_size != 0x12 && fmt_size != 0x32) goto fail;
            if (block_size != 0x24*channels) goto fail;

            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = block_size;

            /* King Kong: Gamers Edition (PC) */
            if (fmt_size == 0x32) {
                /* standard WAVEFORMATEX must write extra size here, Jade sets 0 */
                if (read_u16le(fmt_offset + 0x10, sf) != 0)
                    goto fail;
                /* 0x12: block samples */
                if (!msadpcm_check_coefs(sf, fmt_offset + 0x14))
                    goto fail;
            }

            vgmstream->num_samples = msadpcm_bytes_to_samples(data_size, vgmstream->frame_size, channels);
            if (!is_jade_v2) {
                vgmstream->loop_start_sample = 0;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }

            break;

        case 0x0001: { /* PS3 */
            VGMSTREAM* temp_vgmstream = NULL;
            STREAMFILE* temp_sf = NULL;

            if (fmt_size != 0x10) goto fail;
            if (block_size != 0x02 * channels) goto fail;

            /* a MSF (usually ATRAC3) masquerading as PCM */
            if (!is_id32be(data_offset, sf, "MSFC"))
                goto fail;

            temp_sf = setup_subfile_streamfile(sf, data_offset, data_size, "msf");
            if (!temp_sf) goto fail;

            temp_vgmstream = init_vgmstream_msf(temp_sf);
            close_streamfile(temp_sf);
            if (!temp_vgmstream) goto fail;

            temp_vgmstream->meta_type = vgmstream->meta_type;
            close_vgmstream(vgmstream);
            return temp_vgmstream;
        }

        default: /* X360 uses .XMA */
            goto fail;
    }

    /* V1 loops by extension, try to detect incorrectly looped jingles (too short) */
    if (!is_jade_v2) {
        if(loop_flag
                && vgmstream->num_samples < 15*sample_rate) { /* in seconds */
            vgmstream->loop_flag = 0;
        }
    }


    if (!vgmstream_open_stream(vgmstream, sf, data_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* extract loops from "cue /LIST", returns if loops (info from Droolie) */
static int get_loop_points(STREAMFILE* sf, uint32_t cue_offset, uint32_t cue_size, uint32_t list_offset, uint32_t list_size, int* p_loop_start, int* p_loop_end) {
    //off_t offset;
    int i, cue_count, loop_id = 0, loop_start = 0, loop_end = 0;
    chunk_t rc = {0};

    /* unlooped files may contain LIST, but also may not */
    if (!cue_offset || !cue_size || !list_offset || !list_size)
        goto fail;

    rc.current = list_offset + 0x04; /* skip "adtl" */
    rc.max = list_offset + list_size;
    while (next_chunk(&rc, sf)) {
        switch(rc.type) {
            case 0x6C61626C: /* "labl" */
                if (is_id32be(rc.offset + 0x04, sf, "loop")) /* actually a C-string tho */
                    loop_id = read_u32le(rc.offset + 0x00, sf);

                if (rc.size % 2) { /* string is even-padded after size */
                    rc.size++;
                    rc.current++;
                }
                break;

            case 0x6C747874: /* "ltxt" */
                if (loop_id == read_u32le(rc.offset + 0x00, sf))
                    loop_end = read_u32le(rc.offset + 0x04, sf);
                break;

            default:
                VGM_LOG("UBI JADE: unknown LIST chunk\n");
                goto fail;
        }
    }

    if (!loop_end)
        return 0;

    cue_count = read_u32le(cue_offset+0x00, sf);
    for (i = 0; i < cue_count; i++) {
        if (loop_id == read_u32le(cue_offset+0x04 + i*0x18 + 0x00, sf)) {
            loop_start = read_u32le(cue_offset+0x04 + i*0x18 + 0x04, sf);
            loop_end += loop_start;
            break;
        }
    }

    *p_loop_start = loop_start;
    *p_loop_end = loop_end;
    return 1;

fail:
    return 0;
}


/* Jade RIFF in containers */
VGMSTREAM* init_vgmstream_ubi_jade_container(STREAMFILE* sf) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_sf = NULL;
    off_t subfile_offset;
    size_t subfile_size;

    /* Jade packs files in bigfiles, and once extracted the sound files have extra engine data before
     * the RIFF + padding after. Most extractors don't remove the padding correctly, so here we add support. */

    /* checks */
    if (is_id32be(0x04,sf, "RIFF") &&
            read_u32le(0x00,sf)+0x04 == get_streamfile_size(sf)) {
        /* data size + RIFF + padding */
        subfile_offset = 0x04;
    }
    else if (is_id32be(0x00,sf, "RIFF") && 
            read_u32le(0x04,sf) + 0x04 + 0x04 < get_streamfile_size(sf) &&
            (get_streamfile_size(sf) + 0x04) % 0x800 == 0) {
        /* RIFF + padding with data size removed (bad extraction) */
        subfile_offset = 0x00;
    }
    else if (is_id32be(0x04,sf, "RIFF") &&
            read_u32le(0x00,sf) == get_streamfile_size(sf)) {
        /* data_size + RIFF + padding - 0x04 (bad extraction) */
        subfile_offset = 0x04;
    }
    else {
        goto fail;
    }

    /* standard Jade exts + .xma for padded XMA used in Beyond Good & Evil HD (X360) */
    if (!check_extensions(sf,"waa,wac,wad,wam,wav,lwav,xma"))
        goto fail;

    subfile_size = read_u32le(subfile_offset + 0x04,sf) + 0x04 + 0x04;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, NULL);
    if (!temp_sf) goto fail;

    if (read_u16le(0x14, sf) == 0x166) {
        vgmstream = init_vgmstream_xma(temp_sf);
    }
    else {
        vgmstream = init_vgmstream_ubi_jade(temp_sf);
    }

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
