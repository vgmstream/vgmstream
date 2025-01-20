#include "layout.h"
#include "../vgmstream.h"
#include "../base/decode.h"
#include "../base/sbuf.h"
#include "../coding/coding.h"


/* Decodes samples for blocked streams.
 * Data is divided into headered blocks with a bunch of data. The layout calls external helper functions
 * when a block is decoded, and those must parse the new block and move offsets accordingly. */
void render_vgmstream_blocked(sample_t* outbuf, int32_t sample_count, VGMSTREAM* vgmstream) {

    int frame_size = decode_get_frame_size(vgmstream);
    int samples_per_frame = decode_get_samples_per_frame(vgmstream);
    int samples_this_block = 0;

    if (vgmstream->current_block_samples) {
        samples_this_block = vgmstream->current_block_samples;
    }
    else if (frame_size == 0) {
        //TO-DO: this case doesn't seem possible, codecs that return frame_size 0 (should) set current_block_samples
        samples_this_block = vgmstream->current_block_size * 2 * samples_per_frame;
    }
    else {
        samples_this_block = vgmstream->current_block_size / frame_size * samples_per_frame;
    }

    int samples_filled = 0;
    while (samples_filled < sample_count) {
        int samples_to_do; 

        if (vgmstream->loop_flag && decode_do_loop(vgmstream)) {
            /* handle looping, readjust back to loop start values */
            if (vgmstream->current_block_samples) {
                samples_this_block = vgmstream->current_block_samples;
            } else if (frame_size == 0) { /* assume 4 bit */
                samples_this_block = vgmstream->current_block_size * 2 * samples_per_frame;
            } else {
                samples_this_block = vgmstream->current_block_size / frame_size * samples_per_frame;
            }
            continue;
        }

        if (samples_this_block < 0) {
            /* probably block bug or EOF, next calcs would give wrong values/segfaults/infinite loop */
            VGM_LOG("BLOCKED: wrong block samples\n");
            goto decode_fail;
        }

        if (vgmstream->current_block_offset < 0 || vgmstream->current_block_offset == 0xFFFFFFFF) {
            /* probably block bug or EOF, block functions won't be able to read anything useful/infinite loop */
            VGM_LOG("BLOCKED: wrong block offset found\n");
            goto decode_fail;
        }

        samples_to_do = decode_get_samples_to_do(samples_this_block, samples_per_frame, vgmstream);
        if (samples_to_do > sample_count - samples_filled)
            samples_to_do = sample_count - samples_filled;

        if (samples_to_do > 0) {
            /* samples_this_block = 0 is allowed (empty block, do nothing then move to next block) */
            decode_vgmstream(vgmstream, samples_filled, samples_to_do, outbuf);
        }

        samples_filled += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block += samples_to_do;


        /* move to next block when all samples are consumed */
        if (vgmstream->samples_into_block == samples_this_block
                /*&& vgmstream->current_sample < vgmstream->num_samples*/) { /* don't go past last block */ //todo
            block_update(vgmstream->next_block_offset, vgmstream);

            /* update since these may change each block */
            frame_size = decode_get_frame_size(vgmstream);
            samples_per_frame = decode_get_samples_per_frame(vgmstream);
            if (vgmstream->current_block_samples) {
                samples_this_block = vgmstream->current_block_samples;
            }
            else if (frame_size == 0) {
                //TO-DO: this case doesn't seem possible, codecs that return frame_size 0 (should) set current_block_samples
                samples_this_block = vgmstream->current_block_size * 2 * samples_per_frame;
            }
            else {
                samples_this_block = vgmstream->current_block_size / frame_size * samples_per_frame;
            }

            vgmstream->samples_into_block = 0;
        }
    }

    return;
decode_fail:
    sbuf_silence_s16(outbuf, sample_count, vgmstream->channels, samples_filled);
}

/* helper functions to parse new block */
void block_update(off_t block_offset, VGMSTREAM* vgmstream) {
    switch (vgmstream->layout_type) {
        case layout_blocked_ast:
            block_update_ast(block_offset,vgmstream);
            break;
        case layout_blocked_mxch:
            block_update_mxch(block_offset,vgmstream);
            break;
        case layout_blocked_halpst:
            block_update_halpst(block_offset,vgmstream);
            break;
        case layout_blocked_xa:
            block_update_xa(block_offset,vgmstream);
            break;
        case layout_blocked_ea_schl:
            block_update_ea_schl(block_offset,vgmstream);
            break;
        case layout_blocked_ea_1snh:
            block_update_ea_1snh(block_offset,vgmstream);
            break;
        case layout_blocked_caf:
            block_update_caf(block_offset,vgmstream);
            break;
        case layout_blocked_wsi:
            block_update_wsi(block_offset,vgmstream);
            break;
        case layout_blocked_str_snds:
            block_update_str_snds(block_offset,vgmstream);
            break;
        case layout_blocked_ws_aud:
            block_update_ws_aud(block_offset,vgmstream);
            break;
        case layout_blocked_dec:
            block_update_dec(block_offset,vgmstream);
            break;
        case layout_blocked_mul:
            block_update_mul(block_offset,vgmstream);
            break;
        case layout_blocked_gsb:
            block_update_gsb(block_offset,vgmstream);
            break;
        case layout_blocked_vs_mh:
            block_update_vs_mh(block_offset,vgmstream);
            break;
        case layout_blocked_xvas:
            block_update_xvas(block_offset,vgmstream);
            break;
        case layout_blocked_thp:
            block_update_thp(block_offset,vgmstream);
            break;
        case layout_blocked_filp:
            block_update_filp(block_offset,vgmstream);
            break;
        case layout_blocked_rage_aud:
            block_update_rage_aud(block_offset,vgmstream);
            break;
        case layout_blocked_ea_swvr:
            block_update_ea_swvr(block_offset,vgmstream);
            break;
        case layout_blocked_adm:
            block_update_adm(block_offset,vgmstream);
            break;
        case layout_blocked_ps2_iab:
            block_update_ps2_iab(block_offset,vgmstream);
            break;
        case layout_blocked_vs_str:
            block_update_vs_str(block_offset,vgmstream);
            break;
        case layout_blocked_rws:
            block_update_rws(block_offset,vgmstream);
            break;
        case layout_blocked_hwas:
            block_update_hwas(block_offset,vgmstream);
            break;
        case layout_blocked_ea_sns:
            block_update_ea_sns(block_offset,vgmstream);
            break;
        case layout_blocked_awc:
            block_update_awc(block_offset,vgmstream);
            break;
        case layout_blocked_vgs:
            block_update_vgs(block_offset,vgmstream);
            break;
        case layout_blocked_xwav:
            block_update_xwav(block_offset,vgmstream);
            break;
        case layout_blocked_xvag_subsong:
            block_update_xvag_subsong(block_offset,vgmstream);
            break;
        case layout_blocked_ea_wve_au00:
            block_update_ea_wve_au00(block_offset,vgmstream);
            break;
        case layout_blocked_ea_wve_ad10:
            block_update_ea_wve_ad10(block_offset,vgmstream);
            break;
        case layout_blocked_sthd:
            block_update_sthd(block_offset,vgmstream);
            break;
        case layout_blocked_h4m:
            block_update_h4m(block_offset,vgmstream);
            break;
        case layout_blocked_xa_aiff:
            block_update_xa_aiff(block_offset,vgmstream);
            break;
        case layout_blocked_vs_square:
            block_update_vs_square(block_offset,vgmstream);
            break;
        case layout_blocked_vid1:
            block_update_vid1(block_offset,vgmstream);
            break;
        case layout_blocked_ubi_sce:
            block_update_ubi_sce(block_offset,vgmstream);
            break;
        case layout_blocked_tt_ad:
            block_update_tt_ad(block_offset,vgmstream);
            break;
        case layout_blocked_vas:
            block_update_vas(block_offset,vgmstream);
            break;
        case layout_blocked_snd_gcw_str:
            block_update_snd_gcw_str(block_offset, vgmstream);
            break;
        default: /* not a blocked layout */
            break;
    }
}

snd_gcw_str_blocked_layout_data* init_snd_gcw_str_blocked_layout(int32_t num_samples, int channels, size_t block_size)
{
    snd_gcw_str_blocked_layout_data* data = NULL;

    if (channels < 1 || channels > 2)
        goto fail;
    if (num_samples < 0)
        goto fail;
    if (block_size < 0x800 || block_size > 0x8000)
        goto fail;

    if ((channels == 2) && (block_size < 0x8000))
    {
        // if stereo sound has a block size of less than 0x8000 bytes; unlikely to happen.
        vgm_logi("SND+GCW: unknown block size (%d) for stereo stream (should be %d), abort\n", block_size, 0x8000);
        goto fail;
    }

    data = calloc(1, sizeof(snd_gcw_str_blocked_layout_data));
    if (!data) goto fail;

    data->info = calloc(channels, sizeof(snd_gcw_str_first_block_header_info*));
    if (!data->info) goto fail;

    if (!data->finished_all_calcs)
    {
        data->channels = channels;
        data->block_size = block_size;
        data->first_block_header_size = 0x40;

        // bigfile-adjacent dsp consist of the following "header":
        // 0x00-0x20 - dsp coef data (per channel)
        // 0x20-0x40 - blank space (per channel)
        // what follows is actual, raw sound codec data,
        // divided into 0x8000 blocks or less (for a multiple of 0x800)
        // regardless if said data is mono or stereo.

        // first step: use num_samples to set boundaries regarding how a sound begins, and how it ends.
        data->data_size_calc = num_samples / 14;
        // ^ assuming that vgmstream->num_samples wasn't tempered beforehand.
        data->data_size_modulus = 14 - (num_samples % 14);
        if (data->data_size_modulus != 14) data->data_size_calc++;
        data->data_size = (data->data_size_calc * 8) + data->first_block_header_size;

        data->first_sample_threshold = 0;
        data->last_sample_threshold = data->data_size_calc * 14;

        // second step: calculate how many physical data blocks are needed to process all that blocked sound data.
        // worst-case scenario: calculate the remainder of that and use it as a last block size.
        data->blocks = data->data_size / data->block_size;
        data->last_block_size = data->data_size - (data->blocks * data->block_size);
        if (data->last_block_size != 0)
            data->blocks++;

        // third step: calculate the boundaries of when a sample of a sound block begins, and when it ends.
        // of course, when muliple blocks are involved, makes sense to calculate only the size of start, overall, and end block.
        if (data->blocks != 0)
        {
            // example of an sound that needs two blocks: "CROWD_OH04", from "nhl2k3.snd" [NHL 2K3 (GC)]
            // when this happens, overall block size will have to be sacrificed.
            if (data->blocks == 1)
            {
                data->last_block_size -= data->first_block_header_size;
                data->first_block_size = data->last_block_size;
                data->current_block_size = 0;
                data->last_block_size = 0;
            }
            else {
                data->first_block_size = data->block_size - data->first_block_header_size;
                data->current_block_size = (data->blocks == 2) ? 0 : data->block_size;
            }
            // calc first block samples regardless of how many blocks there are.
            data->first_block_samples = data->first_block_size / 8;
            data->first_block_samples_mod = 8 - (data->first_block_size % 8);
            if (data->first_block_samples_mod != 8) data->first_block_samples++;
            data->first_block_samples *= 14;
            data->first_block_samples_threshold = data->first_block_samples;
            // calc samples of both current and last block.
            if (data->blocks == 1)
            {
                data->current_block_samples = 0;
                data->last_block_samples = 0;
                data->last_block_samples_threshold = data->first_block_samples_threshold + 0;
            }
            else {
                if (data->blocks > 2)
                {
                    data->current_block_samples = data->current_block_size / 8;
                    data->current_block_samples_mod = 8 - (data->current_block_samples % 8);
                    if (data->current_block_samples_mod != 8) data->current_block_samples++;
                    data->current_block_samples *= 14;
                }

                data->last_block_samples = data->last_block_size / 8;
                data->last_block_samples_mod = 8 - (data->last_block_size % 8);
                if (data->last_block_samples_mod != 8) data->last_block_samples++;
                data->last_block_samples *= 14;
                data->last_block_samples_threshold = (data->blocks == 2)
                    ? data->first_block_samples_threshold + 0
                    : data->first_block_samples_threshold + ((data->blocks - 2) * data->current_block_samples);
            }
        }

        data->finished_all_calcs = true;
    }

    return data;
fail:
    free_snd_gcw_str_blocked_layout(data);
    return NULL;
}

void free_snd_gcw_str_blocked_layout(snd_gcw_str_blocked_layout_data* data)
{
    if (!data)
        return;

    for (int i = 0; i < data->channels; i++)
        free(data->info[i]);

    free(data->info);
    free(data);
}
