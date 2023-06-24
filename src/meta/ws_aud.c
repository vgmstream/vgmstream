#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* Westwood Studios .aud (WS-AUD) */

VGMSTREAM* init_vgmstream_ws_aud(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    coding_t coding_type = -1;
    off_t format_offset;
    int channels;
    bool new_type = false;
    int bytes_per_sample = 0;


    /* checks **/
    if (!check_extensions(sf, "aud") )
        return NULL;

    /* check for 0x0000DEAF chunk marker for first chunk */
    if (read_u32le(0x10,sf) == 0x0000DEAF) {    /* new */
        new_type = true;
        format_offset = 0x0A;
    }
    else if (read_u32le(0x0C,sf) == 0x0000DEAF) { /* old */
        new_type = false;
        format_offset = 0x06;
    }
    else {
        return NULL;
    }

    /* blocked format with a mini-header */

    if (read_u8(format_offset + 0x00, sf) & 1)
        channels = 2;
    else
        channels = 1;

    if (channels == 2)
        goto fail; /* not seen */

    if (read_u8(format_offset + 0x01,sf) & 2)
        bytes_per_sample = 2;
    else
        bytes_per_sample = 1;

    /* check codec type */
    switch (read_u8(format_offset + 0x01,sf)) {
        case 1:     /* Westwood custom */
            coding_type = coding_WS;
            if (bytes_per_sample != 1) goto fail; /* shouldn't happen? */
            break;
        case 99:    /* IMA ADPCM */
            coding_type = coding_IMA_int;
            break;
        default:
            goto fail;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, 0);
    if (!vgmstream) goto fail;

    if (new_type) {
        vgmstream->num_samples = read_32bitLE(0x06,sf)/bytes_per_sample/channels;
    }
    else {
        /* Doh, no output size in old type files. We have to read through the
         * file looking at chunk headers! Crap! */
        int32_t out_size = 0;
        off_t current_offset = 0x8;
        off_t file_size = get_streamfile_size(sf);

        while (current_offset < file_size) {
            int16_t chunk_size;
            chunk_size = read_16bitLE(current_offset,sf);
            out_size += read_16bitLE(current_offset+2,sf);
            /* while we're here might as well check for valid chunks */
            if (read_32bitLE(current_offset+4,sf) != 0x0000DEAF) goto fail;
            current_offset+=8+chunk_size;
        }

        vgmstream->num_samples = out_size/bytes_per_sample/channels;
    }
    
    /* they tend to not actually have data for the last odd sample */
    if (vgmstream->num_samples & 1) vgmstream->num_samples--;
    vgmstream->sample_rate = (uint16_t)read_16bitLE(0x00,sf);

    vgmstream->coding_type = coding_type;
    if (new_type) {
        vgmstream->meta_type = meta_WS_AUD;
    }

    vgmstream->layout_type = layout_blocked_ws_aud;

    if (!vgmstream_open_stream(vgmstream, sf, 0x00) )
        goto fail;

    if (new_type) {
        block_update(0x0c, vgmstream);
    } else {
        block_update(0x08, vgmstream);
    }

    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
