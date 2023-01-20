#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

/* VGS  - from Guitar Hero Encore - Rocks the 80s, Guitar Hero II PS2 */
VGMSTREAM* init_vgmstream_vgs(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t channel_size = 0, stream_data_size, stream_frame_count;
    int channels = 0, loop_flag = 0, sample_rate = 0, stream_sample_rate;
    int i;


    /* checks */
    if (!is_id32be(0x00,sf, "VgS!"))
        goto fail;
    /* 0x04: version? */

    if (!check_extensions(sf,"vgs"))
        goto fail;

    /* contains N streams, which can have one less frame, or half frame and sample rate */
    for (i = 0; i < 8; i++) {
        stream_sample_rate = read_32bitLE(0x08 + 0x08*i + 0x00,sf);
        stream_frame_count = read_32bitLE(0x08 + 0x08*i + 0x04,sf);
        stream_data_size = stream_frame_count*0x10;

        if (stream_sample_rate == 0)
            break;

        if (!sample_rate || !channel_size) {
            sample_rate = stream_sample_rate;
            channel_size = stream_data_size;
        }

        /* some streams end 1 frame early */
        if (channel_size - 0x10 == stream_data_size) {
            channel_size -= 0x10;
        }

        /* Guitar Hero II sometimes uses half sample rate for last stream */
        if (sample_rate != stream_sample_rate) {
            VGM_LOG("VGS: ignoring stream %i\n", i);
            //total_streams++; // todo handle substreams
            break;
        }

        channels++;
    }

    start_offset = 0x80;

    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VGS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(channel_size * channels, channels);

    vgmstream->coding_type = coding_PSX_badflags; /* flag = stream/channel number */
    vgmstream->layout_type = layout_blocked_vgs;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
