#include "layout.h"
#include "../vgmstream.h"
#include "../base/decode.h"
#include "../base/sbuf.h"
#include "../coding/coding.h"


/* Decodes samples for blocked streams.
 * Data is divided into headered blocks with a bunch of data. The layout calls external helper functions
 * when a block is decoded, and those must parse the new block and move offsets accordingly. */
void render_vgmstream_blocked(sbuf_t* sdst, VGMSTREAM* vgmstream) {

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

    while (sdst->filled < sdst->samples) {
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
        if (samples_to_do > sdst->samples - sdst->filled)
            samples_to_do = sdst->samples - sdst->filled;

        if (samples_to_do > 0) {
            /* samples_this_block = 0 is allowed (empty block, do nothing then move to next block) */
            decode_vgmstream(sdst, vgmstream, samples_to_do);
        }

        sdst->filled += samples_to_do;
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
    sbuf_silence_rest(sdst);
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
        case layout_blocked_gsnd:
            block_update_gsnd(block_offset,vgmstream);
            break;
        case layout_blocked_vs_mh:
            block_update_vs_mh(block_offset,vgmstream);
            break;
        case layout_blocked_vas_kceo:
            block_update_vas_kceo(block_offset,vgmstream);
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
        default: /* not a blocked layout */
            break;
    }
}
