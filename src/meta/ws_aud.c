#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* Westwood Studios .aud (WS-AUD) */

VGMSTREAM * init_vgmstream_ws_aud(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];

    coding_t coding_type = -1;

    int channel_count;
    int new_type = 0;   /* if 0 is old type */

    int bytes_per_sample = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("aud",filename_extension(filename))) goto fail;

    /* check for 0x0000DEAF chunk marker for first chunk */
    if (read_32bitLE(0x10,streamFile)==0x0000DEAF) {    /* new */
        new_type = 1;
    } else if (read_32bitLE(0x08,streamFile)==0x0000DEAF) { /* old (?) */
        new_type = 0;
    } else goto fail;

    if (!new_type) goto fail; /* TODO: not yet supported */

    /* get channel count */
    if (read_8bit(0xa,streamFile) & 1)
        channel_count = 2;
    else
        channel_count = 1;

    if (channel_count == 2) goto fail; /* TODO: not yet supported */

    /* get output format */
    if (read_8bit(0xa,streamFile) & 2)
        bytes_per_sample = 2;
    else
        bytes_per_sample = 1;

    if (bytes_per_sample == 1) goto fail; /* TODO: not yet supported */

    /* check codec type */
    switch (read_8bit(0xb,streamFile)) {
        case 1:     /* Westwood custom */
            goto fail;  /* TODO: not yet supported */
            break;
        case 99:    /* IMA ADPCM */
            coding_type = coding_IMA;
            break;
        default:
            goto fail;
            break;
    }

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(channel_count,0);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitLE(0x06,streamFile)/bytes_per_sample/channel_count;
    vgmstream->sample_rate = (uint16_t)read_16bitLE(0x00,streamFile);

    vgmstream->coding_type = coding_type;
    if (new_type) {
        vgmstream->meta_type = meta_WS_AUD;
    } else {
        vgmstream->meta_type = meta_WS_AUD_old;
    }

    vgmstream->layout_type = layout_ws_aud_blocked;

    /* open the file for reading by each channel */
    {
        int i;
        STREAMFILE * file;

        file = streamFile->open(streamFile,filename,
                STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;

        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;
        }
    }

    /* start me up */
    if (new_type) {
        ws_aud_block_update(0xc,vgmstream);
    } else {
        ws_aud_block_update(0x8,vgmstream);
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
