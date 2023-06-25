#include "meta.h"

/* .pos - loop points for .wav [Ys I-II Complete (PC)] */
VGMSTREAM* init_vgmstream_pos(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_data = NULL;
    int32_t loop_start, loop_end;


    /* checks */
    if (get_streamfile_size(sf) != 0x08)
        return NULL;
    if (!check_extensions(sf, "pos"))
        return NULL;

    sf_data = open_streamfile_by_ext(sf, "wav");
    if (sf_data) {
        vgmstream = init_vgmstream_riff(sf_data);
        if (!vgmstream) goto fail;
        vgmstream->meta_type = meta_RIFF_WAVE_POS;
    }
    else {
        goto fail;
    }

    close_streamfile(sf_data);
    sf_data = NULL;

    /* install loops (wrong values are validated later) */
    loop_start = read_s32le(0x00, sf);
    loop_end = read_s32le(0x04, sf);
    if (loop_end <= 0 || (loop_end > vgmstream->num_samples)) {
        loop_end = vgmstream->num_samples;
    }
    vgmstream_force_loop(vgmstream, 1, loop_start, loop_end);

    return vgmstream;

fail:
    close_streamfile(sf_data);
    close_vgmstream(vgmstream);
    return NULL;
}