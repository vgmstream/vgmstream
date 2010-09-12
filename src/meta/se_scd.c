#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* Square-Enix SCD (FF XIII, XIV) */
VGMSTREAM * init_vgmstream_se_scd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset, meta_offset_offset, meta_offset, size_offset;
    int32_t loop_start, loop_end;

    int loop_flag = 0;
	int channel_count;
    int codec_id;

    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("scd",filename_extension(filename))) goto fail;

    /* SEDB */
    if (read_32bitBE(0,streamFile) != 0x53454442) goto fail;
    /* SSCF */
    if (read_32bitBE(4,streamFile) != 0x53534346) goto fail;
    if (read_32bitBE(8,streamFile) == 2 ||
        read_32bitBE(8,streamFile) == 3) {
        /* version 2 BE, as seen in FFXIII demo for PS3 */
        /* version 3 BE, as seen in FFXIII for PS3 */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        size_offset = 0x14;
        meta_offset_offset = 0x70;
    } else if (read_32bitLE(8,streamFile) == 3 ||
               read_32bitLE(8,streamFile) == 2) {
        /* version 2/3 LE, as seen in FFXIV for ?? */
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
        size_offset = 0x10;
        meta_offset_offset = 0x70;
    } else goto fail;

    /* check file size with header value */
    if (read_32bit(size_offset,streamFile) != get_streamfile_size(streamFile))
        goto fail;

    /* this is probably some kind of chunk offset (?) */
    meta_offset = read_32bit(0x70,streamFile);

    /* check that chunk size equals stream size (?) */
    loop_start = read_32bit(meta_offset+0x10,streamFile);
    loop_end = read_32bit(meta_offset+0x14,streamFile);
    loop_flag = (loop_end > 0);

    channel_count = read_32bit(meta_offset+4,streamFile);
    codec_id = read_32bit(meta_offset+0xc,streamFile);
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bit(meta_offset+8,streamFile);
    start_offset = meta_offset + 0x20 + read_32bit(meta_offset+0x18,streamFile);

    switch (codec_id) {
        case 0x1:
            /* PCM */
            vgmstream->coding_type = coding_PCM16LE_int;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = read_32bit(meta_offset+0,streamFile) / 2 / channel_count;

            if (loop_flag) {
                vgmstream->loop_start_sample = loop_start / 2 / channel_count;
                vgmstream->loop_end_sample = loop_end / 2 / channel_count;
            }
            break;
#ifdef VGM_USE_MPEG
        case 0x7:
            /* MPEG */
            {
                mpeg_codec_data *mpeg_data = NULL;
                struct mpg123_frameinfo mi;
                coding_t ct;

                mpeg_data = init_mpeg_codec_data(streamFile, start_offset, vgmstream->sample_rate, vgmstream->channels, &ct);
                if (!mpeg_data) goto fail;
                vgmstream->codec_data = mpeg_data;

                if (MPG123_OK != mpg123_info(mpeg_data->m, &mi)) goto fail;

                vgmstream->coding_type = ct;
                vgmstream->layout_type = layout_mpeg;
                if (mi.vbr != MPG123_CBR) goto fail;
                vgmstream->num_samples = mpeg_bytes_to_samples(read_32bit(meta_offset+0,streamFile), &mi);
                vgmstream->num_samples = vgmstream->num_samples/2*2;
                if (loop_flag) {
                    vgmstream->loop_start_sample = mpeg_bytes_to_samples(loop_start, &mi);
                    vgmstream->loop_start_sample = vgmstream->loop_start_sample/2*2;
                    vgmstream->loop_end_sample = mpeg_bytes_to_samples(loop_end, &mi);
                    vgmstream->loop_end_sample = vgmstream->loop_end_sample/2*2;
                }
                vgmstream->interleave_block_size = 0;
            }
            break;
#endif
        case 0xC:
            /* MS ADPCM */
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = read_16bit(meta_offset+0x2c,streamFile);
            vgmstream->num_samples = msadpcm_bytes_to_samples(read_32bit(meta_offset+0,streamFile), vgmstream->interleave_block_size, vgmstream->channels);

            if (loop_flag) {
                vgmstream->loop_start_sample = msadpcm_bytes_to_samples(loop_start, vgmstream->interleave_block_size, vgmstream->channels);
                vgmstream->loop_end_sample = msadpcm_bytes_to_samples(loop_end, vgmstream->interleave_block_size, vgmstream->channels);
            }
            break;
        default:
            goto fail;
    }

    vgmstream->meta_type = meta_SE_SCD;

    /* open the file for reading */
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=start_offset;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
