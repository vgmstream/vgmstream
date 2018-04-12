#include "layout.h"
#include "../vgmstream.h"

static void block_update(VGMSTREAM * vgmstream);

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
        if (samples_this_block < 0) {
            VGM_LOG("layout_blocked: wrong block at 0x%lx\n", vgmstream->current_block_offset);
            memset(buffer + samples_written*vgmstream->channels, 0, (sample_count - samples_written) * vgmstream->channels * sizeof(sample));
            break; /* probable infinite loop otherwise */
        }

        /* samples_this_block = 0 is allowed (empty block), will do nothing then move to next block */

        samples_to_do = vgmstream_samples_to_do(samples_this_block, samples_per_frame, vgmstream);
        if (samples_written + samples_to_do > sample_count)
            samples_to_do = sample_count - samples_written;

        if (vgmstream->current_block_offset >= 0) {
            decode_vgmstream(vgmstream, samples_written, samples_to_do, buffer);
        }
        else {
            /* block end signal (used in halpst): partially 0-set buffer */
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
            block_update(vgmstream);

            /* for VBR these may change */
            frame_size = get_vgmstream_frame_size(vgmstream);
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


static void block_update(VGMSTREAM * vgmstream) {
    switch (vgmstream->layout_type) {
        case layout_blocked_ast:
            block_update_ast(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_mxch:
            block_update_mxch(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_halpst:
            if (vgmstream->next_block_offset>=0)
                block_update_halpst(vgmstream->next_block_offset,vgmstream);
            else
                vgmstream->current_block_offset = -1;
            break;
        case layout_blocked_xa:
            block_update_xa(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_ea_schl:
            block_update_ea_schl(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_ea_1snh:
            block_update_ea_1snh(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_caf:
            block_update_caf(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_wsi:
            block_update_wsi(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_str_snds:
            block_update_str_snds(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_ws_aud:
            block_update_ws_aud(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_matx:
            block_update_matx(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_dec:
            block_update_dec(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_emff_ps2:
            block_update_emff_ps2(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_emff_ngc:
            block_update_emff_ngc(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_gsb:
            block_update_gsb(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_vs:
            block_update_vs(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_xvas:
            block_update_xvas(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_thp:
            block_update_thp(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_filp:
            block_update_filp(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_ivaud:
            block_update_ivaud(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_ea_swvr:
            block_update_ea_swvr(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_adm:
            block_update_adm(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_bdsp:
            block_update_bdsp(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_tra:
            block_update_tra(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_ps2_iab:
            block_update_ps2_iab(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_ps2_strlr:
            block_update_ps2_strlr(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_rws:
            block_update_rws(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_hwas:
            block_update_hwas(vgmstream->next_block_offset,vgmstream);
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
        case layout_blocked_xvag_subsong:
            block_update_xvag_subsong(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_ea_wve_au00:
            block_update_ea_wve_au00(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_ea_wve_ad10:
            block_update_ea_wve_ad10(vgmstream->next_block_offset,vgmstream);
            break;
        case layout_blocked_sthd:
            block_update_sthd(vgmstream->next_block_offset,vgmstream);
            break;
        default:
            break;
    }
}
