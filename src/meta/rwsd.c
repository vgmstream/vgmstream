#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* Wii RWAV, 3DS CWAV */

struct rwav_data {
    // in
    off_t offset;
    STREAMFILE *streamFile;
    int big_endian;
    int32_t (*read_32bit)(off_t,STREAMFILE*);

    // out
    int version;
    off_t start_offset;
    off_t info_chunk;
    off_t wave_offset;
};

static void read_rwav(struct rwav_data * rd)
{
    off_t chunk_table_offset;
    off_t chunk_table_step;
    off_t info_chunk;
    off_t data_chunk;

    if (rd->big_endian)
    {
        /* "RWAV" */
        if ((uint32_t)read_32bitBE(rd->offset,rd->streamFile)!=0x52574156)
            return;

        /* big endian, version 2 */
        if ((uint32_t)read_32bitBE(rd->offset+4,rd->streamFile)!=0xFEFF0102)
            return;

        chunk_table_offset = rd->offset+0x10;
        chunk_table_step = 8;
    }
    else
    {
        /* "CWAV" */
        if ((uint32_t)read_32bitBE(rd->offset,rd->streamFile)!=0x43574156) 
            return;

        /* little endian, version 2 */
        if ((uint32_t)read_32bitBE(rd->offset+4,rd->streamFile)!=0xFFFE4000 ||
            (uint32_t)read_32bitBE(rd->offset+8,rd->streamFile)!=0x00000102)
            return;

        chunk_table_offset = rd->offset+0x18;
        chunk_table_step = 0xc;
    }

    info_chunk = rd->offset+rd->read_32bit(chunk_table_offset,rd->streamFile);
    /* "INFO" */
    if ((uint32_t)read_32bitBE(info_chunk,rd->streamFile)!=0x494e464f)
        return;

    data_chunk = rd->offset+rd->read_32bit(chunk_table_offset+chunk_table_step,rd->streamFile);
    /* "DATA" */
    if ((uint32_t)read_32bitBE(data_chunk,rd->streamFile)!=0x44415441)
        return;

    rd->start_offset = data_chunk + 8;
    rd->info_chunk = info_chunk + 8;
    rd->version = 2;
    rd->wave_offset = info_chunk - 8;   // pretend to have a WAVE

    return;
}

static void read_rwar(struct rwav_data * rd)
{
    if ((uint32_t)read_32bitBE(rd->offset,rd->streamFile)!=0x52574152) /* "RWAR" */
        return;
    if ((uint32_t)read_32bitBE(rd->offset+4,rd->streamFile)!=0xFEFF0100) /* version 0 */
        return;

    rd->offset += 0x60;
    read_rwav(rd);
    rd->version = 0;
    return;
}

/* RWSD is quite similar to BRSTM, but can contain several streams.
 * Still, some games use it for single streams. We only support the
 * single stream form here */
VGMSTREAM * init_vgmstream_rwsd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];

    coding_t coding_type;

    size_t wave_length;
    int codec_number;
    int channel_count;
    int loop_flag;
    int rwar = 0;
    int rwav = 0;
    struct rwav_data rwav_data;

    size_t stream_size;

    int big_endian = 1;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;

    const char *ext;

    rwav_data.version = -1;
    rwav_data.start_offset = 0;
    rwav_data.info_chunk = -1;
    rwav_data.wave_offset = -1;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));

    ext = filename_extension(filename);

    if (strcasecmp("rwsd",ext))
    {
        if (strcasecmp("rwar",ext))
        {
            if (strcasecmp("rwav",ext))
            {
                if (strcasecmp("bcwav",ext) && strcasecmp("bms",ext))
                {
                    goto fail;
                }
                else
                {
                    // cwav, similar to little endian rwav
                    rwav = 1;
                    big_endian = 0;
                }
            }
            else
            {
                // matched rwav
                rwav = 1;
            }
        }
        else
        {
            // matched rwar
            rwar = 1;
        }
    }
    else
    {
        // match rwsd
    }

    if (big_endian)
    {
        read_16bit = read_16bitBE;
        read_32bit = read_32bitBE;
    }
    else
    {
        read_16bit = read_16bitLE;
        read_32bit = read_32bitLE;
    }

    /* check header */
    if (rwar || rwav)
    {
        rwav_data.offset = 0;
        rwav_data.streamFile = streamFile;
        rwav_data.big_endian = big_endian;
        rwav_data.read_32bit = read_32bit;

        if (rwar) read_rwar(&rwav_data);
        if (rwav) read_rwav(&rwav_data);
        if (rwav_data.wave_offset < 0) goto fail;
    }
    else
    {
        if ((uint32_t)read_32bitBE(0,streamFile)!=0x52575344) /* "RWSD" */
            goto fail;

        switch (read_32bitBE(4,streamFile))
        {
            case 0xFEFF0102:
                /* ideally we would look through the chunk list for a WAVE chunk,
                 * but it's always in the same order */
                /* get WAVE offset, check */
                rwav_data.wave_offset = read_32bit(0x18,streamFile);
                if ((uint32_t)read_32bitBE(rwav_data.wave_offset,streamFile)!=0x57415645) /* "WAVE" */
                    goto fail;
                /* get WAVE size, check */
                wave_length = read_32bit(0x1c,streamFile);
                if (read_32bit(rwav_data.wave_offset+4,streamFile)!=wave_length)
                    goto fail;

                /* check wave count */
                if (read_32bit(rwav_data.wave_offset+8,streamFile) != 1)
                    goto fail; /* only support 1 */

                rwav_data.version = 2;

                break;
            case 0xFEFF0103:
                rwav_data.offset = 0xe0;
                rwav_data.streamFile = streamFile;
                rwav_data.big_endian = big_endian;
                rwav_data.read_32bit = read_32bit;

                read_rwar(&rwav_data);
                if (rwav_data.wave_offset < 0) goto fail;

                rwar = 1;
                break;
            default:
                goto fail;
        }

    }

    /* get type details */
    codec_number = read_8bit(rwav_data.wave_offset+0x10,streamFile);
    loop_flag = read_8bit(rwav_data.wave_offset+0x11,streamFile);
    if (big_endian)
        channel_count = read_8bit(rwav_data.wave_offset+0x12,streamFile);
    else
        channel_count = read_32bit(rwav_data.wave_offset+0x24,streamFile);

    switch (codec_number) {
        case 0:
            coding_type = coding_PCM8;
            break;
        case 1:
            if (big_endian)
                coding_type = coding_PCM16BE;
            else
                coding_type = coding_PCM16LE;
            break;
        case 2:
            coding_type = coding_NGC_DSP;
            break;
        case 3:
            coding_type = coding_IMA;
            break;
        default:
            goto fail;
    }

    if (channel_count < 1) goto fail;

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    if (big_endian)
    {
        vgmstream->num_samples = dsp_nibbles_to_samples(read_32bit(rwav_data.wave_offset+0x1c,streamFile));
        vgmstream->loop_start_sample = dsp_nibbles_to_samples(read_32bit(rwav_data.wave_offset+0x18,streamFile));
    }
    else
    {
        vgmstream->num_samples = read_32bit(rwav_data.wave_offset+0x1c,streamFile);
        vgmstream->loop_start_sample = read_32bit(rwav_data.wave_offset+0x18,streamFile);
    }

    vgmstream->sample_rate = (uint16_t)read_16bit(rwav_data.wave_offset+0x14,streamFile);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_type;
    vgmstream->layout_type = layout_none;

    if (rwar)
        vgmstream->meta_type = meta_RWAR;
    else if (rwav)
    {
        if (big_endian)
            vgmstream->meta_type = meta_RWAV;
        else
            vgmstream->meta_type = meta_CWAV;
    }
    else
        vgmstream->meta_type = meta_RWSD;

    {
        off_t data_start_offset;
        off_t codec_info_offset;
        int i,j;

        for (j=0;j<vgmstream->channels;j++) {
            if (rwar || rwav)
            {
                if (big_endian)
                {
                /* This is pretty nasty, so an explaination is in order.
                 * At 0x10 in the info_chunk is the offset of a table with
                 * one entry per channel. Each entry in this table is itself
                 * an offset to a set of information for the channel. The
                 * first element in the set is the offset into DATA of the
                 * channel. 
                 * The second element is the
                 * offset of the codec-specific setup for the channel. */

                off_t channel_info_offset;
                channel_info_offset = rwav_data.info_chunk +
                    read_32bit(rwav_data.info_chunk+
                    read_32bit(rwav_data.info_chunk+0x10,streamFile)+j*4,
                        streamFile);

                data_start_offset = rwav_data.start_offset +
                    read_32bit(channel_info_offset+0, streamFile);
                codec_info_offset = rwav_data.info_chunk + 
                    read_32bit(channel_info_offset+4, streamFile);
                }

                else
                {
                // CWAV uses some relative offsets
                off_t cur_pos = rwav_data.info_chunk + 0x14; // channel count
                cur_pos = cur_pos + read_32bit(cur_pos + 4 + j*8 + 4,streamFile);

                // size is at cur_pos + 4
                data_start_offset = rwav_data.start_offset + read_32bit(cur_pos + 4, streamFile);
                // codec-specific info is at cur_pos + 0xC
                codec_info_offset = cur_pos + read_32bit(cur_pos + 0xC,streamFile);
                }
                vgmstream->ch[j].channel_start_offset=
                    vgmstream->ch[j].offset=data_start_offset;
            } else {
                // dummy for RWSD, must be a proper way to work this out
                codec_info_offset=rwav_data.wave_offset+0x6c+j*0x30;
            }

            if (vgmstream->coding_type == coding_NGC_DSP) {
                for (i=0;i<16;i++) {
                    vgmstream->ch[j].adpcm_coef[i]=read_16bit(codec_info_offset+i*2,streamFile);
                }
            }

            if (vgmstream->coding_type == coding_IMA) {
                vgmstream->ch[j].adpcm_history1_16 = read_16bit(codec_info_offset,streamFile);
                vgmstream->ch[j].adpcm_step_index = read_16bit(codec_info_offset+2,streamFile);
            }
        }
    }

    if (rwar || rwav)
    {
        /* */
    }
    else
    {
        if (rwav_data.version == 2)
            rwav_data.start_offset = read_32bit(8,streamFile);
    }
    stream_size = read_32bit(rwav_data.wave_offset+0x50,streamFile);

    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,
                    0x1000);

            if (!vgmstream->ch[i].streamfile) goto fail;

            if (!(rwar || rwav))
            {
                vgmstream->ch[i].channel_start_offset=
                    vgmstream->ch[i].offset=
                    rwav_data.start_offset + i*stream_size;
            }
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
