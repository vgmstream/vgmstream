#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* RWSD is quite similar to BRSTM, but can contain several streams.
 * Still, some games use it for single streams. We only support the single stream form here */
//TODO this meta is a hack as WSD is just note info, and data offsets are elsewhere,
// while this assumes whatever data follows RWSD must belong to it; rework for Wii Sports
VGMSTREAM* init_vgmstream_rwsd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    size_t wave_length;
    int codec, channels, loop_flag;
    size_t stream_size;
    off_t start_offset, wave_offset = 0, labl_offset;


    if (!is_id32be(0x00, sf, "RWSD")) 
        goto fail;

    if (!check_extensions(sf, "brwsd,rwsd"))
        goto fail;

    /* check header */
    switch (read_u32be(0x04, sf)) {
        case 0xFEFF0102:
            /* ideally we would look through the chunk list for a WAVE chunk,
                * but it's always in the same order */

            /* get WAVE offset, check */
            wave_offset = read_32bitBE(0x18,sf);
            if (!is_id32be(wave_offset + 0x00, sf, "WAVE")) 
                goto fail;

            /* get WAVE size, check */
            wave_length = read_32bitBE(0x1c,sf);
            if (read_32bitBE(wave_offset + 0x04,sf) != wave_length)
                goto fail;

            /* check wave count */
            if (read_32bitBE(wave_offset + 0x08,sf) != 1)
                goto fail; /* only support 1 */

            break;

        case 0xFEFF0103: /* followed by RWAR, extract that or use .txth subfile */
            goto fail;
    }

    /* get type details */
    codec = read_u8(wave_offset+0x10,sf);
    loop_flag = read_u8(wave_offset+0x11,sf);
    channels = read_u8(wave_offset+0x12,sf);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = dsp_nibbles_to_samples(read_32bitBE(wave_offset+0x1c,sf));
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(read_32bitBE(wave_offset+0x18,sf));

    vgmstream->sample_rate = (uint16_t)read_16bitBE(wave_offset + 0x14,sf);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    switch (codec) {
        case 0:
            vgmstream->coding_type = coding_PCM8;
            break;
        case 1:
            vgmstream->coding_type = coding_PCM16BE;
            break;
        case 2:
            vgmstream->coding_type = coding_NGC_DSP;
            break;
        default:
            goto fail;
    }

    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_RWSD;

    {
        off_t codec_info_offset;
        int i, j;

        for (j = 0 ; j < vgmstream->channels; j++) {
            // dummy for RWSD, must be a proper way to work this out
            codec_info_offset = wave_offset + 0x6c + j*0x30;

            if (vgmstream->coding_type == coding_NGC_DSP) {
                for (i = 0; i < 16; i++) {
                    vgmstream->ch[j].adpcm_coef[i] = read_16bitBE(codec_info_offset + i*0x2, sf);
                }
            }
        }
    }

    
    /* this is just data size and following data may or may not be from this RWSD */
    start_offset = read_32bitBE(0x08, sf);
    if (is_id32be(start_offset, sf, "LABL")) {
        labl_offset = start_offset;
        start_offset += read_32bitBE(start_offset + 0x04, sf);
        read_string(vgmstream->stream_name, 0x28, labl_offset + 0x18, sf);
    }

    stream_size = read_32bitBE(wave_offset + 0x50,sf);

    /* this meta is a hack as WSD is just note info, and data offsets are elsewhere in the brsar,
     * while this assumes whatever data follows RWSD must belong to it (common but fails in Wii Sports),
     * detect data excess and reject (probably should use brawlbox's info offsets) */
    if (stream_size * channels + 0x10000 < get_streamfile_size(sf) - start_offset)
        goto fail;

    /* open the file for reading by each channel */
    {
        int i;
        char filename[PATH_LIMIT];

        sf->get_name(sf,filename,sizeof(filename));

        for (i = 0; i < channels; i++) {
            vgmstream->ch[i].streamfile = sf->open(sf,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset = vgmstream->ch[i].offset =
                start_offset + i*stream_size;
        }
    }

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
