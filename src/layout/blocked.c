#include "layout.h"
#include "../vgmstream.h"

void render_vgmstream_blocked(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    int samples_written=0;

    int frame_size = get_vgmstream_frame_size(vgmstream);
    int samples_per_frame = get_vgmstream_samples_per_frame(vgmstream);
    int samples_this_block;

    /* get samples in the current block */
    if (vgmstream->current_block_samples) {
        samples_this_block = vgmstream->current_block_samples;
    } else if (frame_size == 0) { /* assume 4 bit */ //TODO: get_vgmstream_frame_size() really should return bits... */
        samples_this_block = vgmstream->current_block_size * 2 * samples_per_frame;
    } else {
        samples_this_block = vgmstream->current_block_size / frame_size * samples_per_frame;
    }

    /* decode all samples */
    while (samples_written < sample_count) {
        int samples_to_do; 

        if (vgmstream->loop_flag && vgmstream_do_loop(vgmstream)) {
            /* on loop those values are changed */
            if (vgmstream->current_block_samples) {
                samples_this_block = vgmstream->current_block_samples;
            } else if (frame_size == 0) { /* assume 4 bit */ //TODO: get_vgmstream_frame_size() really should return bits... */
                samples_this_block = vgmstream->current_block_size * 2 * samples_per_frame;
            } else {
                samples_this_block = vgmstream->current_block_size / frame_size * samples_per_frame;
            }
            continue;
        }

        /* probably block bug or EOF, next calcs would give wrong values and buffer segfaults */
        if (samples_this_block <= 0) {
            VGM_LOG("layout_blocked: empty/wrong block\n");
            memset(buffer + samples_written*vgmstream->channels, 0, (sample_count - samples_written) * vgmstream->channels * sizeof(sample));
            break;
        }


        samples_to_do = vgmstream_samples_to_do(samples_this_block, samples_per_frame, vgmstream);
        if (samples_written + samples_to_do > sample_count)
            samples_to_do = sample_count - samples_written;

        if (vgmstream->current_block_offset >= 0) {
            decode_vgmstream(vgmstream, samples_written, samples_to_do, buffer);
        }
        else {
            /* block end signal (used below): partially 0-set buffer */
            int i;
            for (i = samples_written*vgmstream->channels; i < (samples_written+samples_to_do)*vgmstream->channels; i++) {
                buffer[i]=0;
            }
        }

        samples_written += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block += samples_to_do;


        /* move to next block when all samples are consumed */
        if (vgmstream->samples_into_block==samples_this_block
                /*&& vgmstream->current_sample < vgmstream->num_samples*/) { /* don't go past last block */
            switch (vgmstream->layout_type) {
                case layout_ast_blocked:
                    ast_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_mxch_blocked:
                    mxch_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_halpst_blocked:
                    if (vgmstream->next_block_offset>=0)
                        halpst_block_update(vgmstream->next_block_offset,vgmstream);
                    else
                        vgmstream->current_block_offset = -1;
                    break;
                case layout_xa_blocked:
                    xa_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_blocked_ea_schl:
                    block_update_ea_schl(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_blocked_ea_1snh:
                    block_update_ea_1snh(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_caf_blocked:
                    caf_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_wsi_blocked:
                    wsi_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_str_snds_blocked:
                    str_snds_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_ws_aud_blocked:
                    ws_aud_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_matx_blocked:
                    matx_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_blocked_dec:
                    block_update_dec(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_emff_ps2_blocked:
                    emff_ps2_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_emff_ngc_blocked:
                    emff_ngc_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_gsb_blocked:
                    gsb_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_vs_blocked:
                    vs_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_xvas_blocked:
                    xvas_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_thp_blocked:
                    thp_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_filp_blocked:
                    filp_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_blocked_ivaud:
                    block_update_ivaud(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_psx_mgav_blocked:
                    psx_mgav_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_ps2_adm_blocked:
                    ps2_adm_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_dsp_bdsp_blocked:
                    dsp_bdsp_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_tra_blocked:
                    tra_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_ps2_iab_blocked:
                    ps2_iab_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_ps2_strlr_blocked:
                    ps2_strlr_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_rws_blocked:
                    rws_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_hwas_blocked:
                    hwas_block_update(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_blocked_ea_sns:
                    block_update_ea_sns(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_blocked_awc:
                    block_update_awc(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_blocked_vgs:
                    block_update_vgs(vgmstream->next_block_offset,vgmstream);
                    break;
                case layout_blocked_vawx:
                    block_update_vawx(vgmstream->next_block_offset,vgmstream);
                    break;
                default:
                    break;
            }

            /* for VBR these may change */
            frame_size = get_vgmstream_frame_size(vgmstream); /* for VBR these may change */
            samples_per_frame = get_vgmstream_samples_per_frame(vgmstream);

            /* get samples in the current block */
            if (vgmstream->current_block_samples) {
                samples_this_block = vgmstream->current_block_samples;
            } else if (frame_size == 0) { /* assume 4 bit */ //TODO: get_vgmstream_frame_size() really should return bits... */
                samples_this_block = vgmstream->current_block_size * 2 * samples_per_frame;
            } else {
                samples_this_block = vgmstream->current_block_size / frame_size * samples_per_frame;
            }

            vgmstream->samples_into_block = 0;
        }

    }
}
