#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* .AUD - from Westwood Studios games [Command & Conquer (PC), ] */
VGMSTREAM* init_vgmstream_ws_aud(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t format_offset;
    bool new_type = false;


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
    int sample_rate = read_u16le(0x00,sf);
    uint8_t channel_flags = read_u8(format_offset + 0x00, sf);
    uint8_t format_flags = read_u8(format_offset + 0x01, sf);

    int channels = channel_flags & 1 ? 2 : 1;
    if (channels == 2)
        return NULL; /* not seen */
    int bytes_per_sample = (channel_flags & 2) ? 2 : 1;

    uint32_t data_size;
    if (new_type) {
        data_size = read_u32le(0x06,sf);
    }
    else {
        /* to read through the file looking at chunk headers */
        off_t offset = 0x08;
        off_t file_size = get_streamfile_size(sf);

        data_size = 0;
        while (offset < file_size) {
            uint16_t chunk_size = read_u16le(offset + 0x00,sf);
            data_size += read_u16le(offset + 0x02,sf);
            /* while we're here might as well check for valid chunks */
            if (read_u32le(offset + 0x04, sf) != 0x0000DEAF)
                goto fail;
            offset += 0x08 + chunk_size;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, 0);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WS_AUD;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = data_size / bytes_per_sample / channels;
    /* they tend to not actually have data for the last odd sample */
    if (vgmstream->num_samples & 1)
        vgmstream->num_samples--;

    switch (format_flags) {
        case 0x01:     /* Westwood ADPCM [The Legend of Kyrandia - Book 3 (PC)] */
            vgmstream->coding_type = coding_WS;
            if (bytes_per_sample != 1) /* shouldn't happen? */
                goto fail;
            break;

        case 0x63:    /* IMA ADPCM [Blade Runner (PC)] */
            vgmstream->coding_type = coding_IMA_mono;
            break;
        default:
            goto fail;
    }

    vgmstream->layout_type = layout_blocked_ws_aud;

    if (!vgmstream_open_stream(vgmstream, sf, 0x00) )
        goto fail;
    block_update(new_type ? 0x0c : 0x08, vgmstream);

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
