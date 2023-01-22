#include "layout.h"
#include "../vgmstream.h"
#include "../decode.h"
#include "../coding/coding.h"


/* Decodes samples for blocked streams.
 * Data is divided into headered blocks with a bunch of data. The layout calls external helper functions
 * when a block is decoded, and those must parse the new block and move offsets accordingly. */
void render_vgmstream_blocked(sample_t* buffer, int32_t sample_count, VGMSTREAM* vgmstream) {
    int samples_written = 0;
    int frame_size, samples_per_frame, samples_this_block;

    frame_size = get_vgmstream_frame_size(vgmstream);
    samples_per_frame = get_vgmstream_samples_per_frame(vgmstream);
    samples_this_block = 0;

    if (vgmstream->current_block_samples) {
        samples_this_block = vgmstream->current_block_samples;
    } else if (frame_size == 0) { /* assume 4 bit */ //TODO: get_vgmstream_frame_size() really should return bits... */
        samples_this_block = vgmstream->current_block_size * 2 * samples_per_frame;
    } else {
        samples_this_block = vgmstream->current_block_size / frame_size * samples_per_frame;
    }


    while (samples_written < sample_count) {
        int samples_to_do; 


        if (vgmstream->loop_flag && vgmstream_do_loop(vgmstream)) {
            /* handle looping, readjust back to loop start values */
            if (vgmstream->current_block_samples) {
                samples_this_block = vgmstream->current_block_samples;
            } else if (frame_size == 0) { /* assume 4 bit */ //TODO: get_vgmstream_frame_size() really should return bits... */
                samples_this_block = vgmstream->current_block_size * 2 * samples_per_frame;
            } else {
                samples_this_block = vgmstream->current_block_size / frame_size * samples_per_frame;
            }
            continue;
        }

        if (samples_this_block < 0) {
            /* probably block bug or EOF, next calcs would give wrong values/segfaults/infinite loop */
            VGM_LOG("layout_blocked: wrong block samples at 0x%x\n", (uint32_t)vgmstream->current_block_offset);
            memset(buffer + samples_written*vgmstream->channels, 0, (sample_count - samples_written) * vgmstream->channels * sizeof(sample_t));
            break;
        }

        if (vgmstream->current_block_offset < 0 || vgmstream->current_block_offset == 0xFFFFFFFF) {
            /* probably block bug or EOF, block functions won't be able to read anything useful/infinite loop */
            VGM_LOG("layout_blocked: wrong block offset found\n");
            memset(buffer + samples_written*vgmstream->channels, 0, (sample_count - samples_written) * vgmstream->channels * sizeof(sample_t));
            break;
        }

        samples_to_do = get_vgmstream_samples_to_do(samples_this_block, samples_per_frame, vgmstream);
        if (samples_to_do > sample_count - samples_written)
            samples_to_do = sample_count - samples_written;

        if (samples_to_do > 0) {
            /* samples_this_block = 0 is allowed (empty block, do nothing then move to next block) */
            decode_vgmstream(vgmstream, samples_written, samples_to_do, buffer);
        }

        samples_written += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block += samples_to_do;


        /* move to next block when all samples are consumed */
        if (vgmstream->samples_into_block == samples_this_block
                /*&& vgmstream->current_sample < vgmstream->num_samples*/) { /* don't go past last block */ //todo
            block_update(vgmstream->next_block_offset,vgmstream);

            /* update since these may change each block */
            frame_size = get_vgmstream_frame_size(vgmstream);
            samples_per_frame = get_vgmstream_samples_per_frame(vgmstream);
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
        case layout_blocked_matx:
            block_update_matx(block_offset,vgmstream);
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
        case layout_blocked_vs:
            block_update_vs(block_offset,vgmstream);
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
        case layout_blocked_ivaud:
            block_update_ivaud(block_offset,vgmstream);
            break;
        case layout_blocked_ea_swvr:
            block_update_ea_swvr(block_offset,vgmstream);
            break;
        case layout_blocked_adm:
            block_update_adm(block_offset,vgmstream);
            break;
        case layout_blocked_bdsp:
            block_update_bdsp(block_offset,vgmstream);
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
        default: /* not a blocked layout */
            break;
    }
}

void blocked_count_samples(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t offset) {
    int block_samples;
    off_t max_offset = get_streamfile_size(sf);

    vgmstream->next_block_offset = offset;
    do {
        block_update(vgmstream->next_block_offset, vgmstream);

        if (vgmstream->current_block_samples < 0 || vgmstream->current_block_size == 0xFFFFFFFF)
            break;

        if (vgmstream->current_block_samples) {
            block_samples = vgmstream->current_block_samples;
        }
        else {
            switch(vgmstream->coding_type) {
                case coding_PCM16_int:  block_samples = pcm16_bytes_to_samples(vgmstream->current_block_size, 1); break;
                case coding_PCM8_int:
                case coding_PCM8_U_int: block_samples = pcm8_bytes_to_samples(vgmstream->current_block_size, 1); break;
                case coding_XBOX_IMA:   block_samples = xbox_ima_bytes_to_samples(vgmstream->current_block_size, 1); break;
                case coding_NGC_DSP:    block_samples = dsp_bytes_to_samples(vgmstream->current_block_size, 1); break;
                case coding_PSX:        block_samples = ps_bytes_to_samples(vgmstream->current_block_size,1); break;
                default:
                    VGM_LOG("BLOCKED: missing codec\n");
                    return;
            }
        }

        vgmstream->num_samples += block_samples;
    }
    while (vgmstream->next_block_offset < max_offset);

    block_update(offset, vgmstream); /* reset */
}
