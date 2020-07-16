#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* VAWX - found in feelplus games [No More Heroes: Heroes Paradise (PS3/X360), Moon Diver (PS3/X360)] */
VGMSTREAM* init_vgmstream_vawx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, data_size;
    int loop_flag = 0, channel_count, codec;


    /* checks */
    /* .xwv: actual extension [Moon Diver (PS3/X360)]
     * .vawx: header id */
    if ( !check_extensions(sf, "xwv,vawx") )
        goto fail;
    if (read_32bitBE(0x00,sf) != 0x56415758) /* "VAWX" */
        goto fail;

    loop_flag = read_8bit(0x37,sf);
    channel_count = read_8bit(0x39,sf);
    start_offset = 0x800; /* ? read_32bitLE(0x0c,sf); */
    codec = read_8bit(0x36,sf); /* could be at 0x38 too */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* 0x04: filesize */
    /* 0x16: file id */
    vgmstream->num_samples = read_32bitBE(0x3c,sf);
    vgmstream->sample_rate = read_32bitBE(0x40,sf);

    vgmstream->meta_type = meta_VAWX;

    switch(codec) {
        case 2: /* PS-ADPCM */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = channel_count == 6 ? layout_blocked_vawx : layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->loop_start_sample = read_32bitBE(0x44,sf);
            vgmstream->loop_end_sample = read_32bitBE(0x48,sf);

            break;

#ifdef VGM_USE_FFMPEG
        case 1: { /* XMA2 */
            uint8_t buf[0x100];
            int32_t bytes, block_size, block_count;

            data_size = get_streamfile_size(sf)-start_offset;
            block_size = 0x10000; /* VAWX default */
            block_count = (uint16_t)read_16bitBE(0x3A, sf); /* also at 0x56 */

            bytes = ffmpeg_make_riff_xma2(buf,0x100, vgmstream->num_samples, data_size, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
            vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf,bytes, start_offset,data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->loop_start_sample = read_32bitBE(0x44,sf);
            vgmstream->loop_end_sample = read_32bitBE(0x48,sf);

            //todo fix loops/samples vs ATRAC3
            /* may be only applying end_skip to num_samples? */
            xma_fix_raw_samples(vgmstream, sf, start_offset,data_size, 0, 0,0);
            break;
        }

        case 7: { /* ATRAC3 */
            int block_align, encoder_delay;

            data_size = read_32bitBE(0x54,sf);
            block_align = 0x98 * vgmstream->channels;
            encoder_delay = 1024 + 69*2; /* observed default, matches XMA (needed as many files start with garbage) */
            vgmstream->num_samples = atrac3_bytes_to_samples(data_size, block_align) - encoder_delay; /* original samples break looping in some files otherwise */

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(sf, start_offset,data_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* set offset samples (offset 0 jumps to sample 0 > pre-applied delay, and offset end loops after sample end > adjusted delay) */
            vgmstream->loop_start_sample = atrac3_bytes_to_samples(read_32bitBE(0x44,sf), block_align); //- encoder_delay
            vgmstream->loop_end_sample   = atrac3_bytes_to_samples(read_32bitBE(0x48,sf), block_align) - encoder_delay;
            break;
        }
#endif
        default:
            goto fail;

    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
