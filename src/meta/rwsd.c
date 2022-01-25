#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* Wii RWAV */

typedef struct {
    // in
    off_t offset;
    STREAMFILE *sf;
    int32_t (*read_32bit)(off_t,STREAMFILE*);

    // out
    int version;
    off_t start_offset;
    off_t info_chunk;
    off_t wave_offset;
}  rwav_data_t;

static void read_rwav(rwav_data_t* rd) {
    off_t chunk_table_offset;
    off_t chunk_table_step;
    off_t info_chunk;
    off_t data_chunk;

    if (!is_id32be(rd->offset, rd->sf, "RWAV")) 
        return;

    /* big endian, version 2 */
    if (read_u32be(rd->offset+4,rd->sf) != 0xFEFF0102)
        return;

    chunk_table_offset = rd->offset + 0x10;
    chunk_table_step = 0x08;

    info_chunk = rd->offset + rd->read_32bit(chunk_table_offset, rd->sf);
    if (!is_id32be(info_chunk, rd->sf, "INFO")) 
        return;

    data_chunk = rd->offset + rd->read_32bit(chunk_table_offset + chunk_table_step, rd->sf);
    if (!is_id32be(data_chunk, rd->sf, "DATA")) 
        return;

    rd->start_offset = data_chunk + 0x08;
    rd->info_chunk = info_chunk + 0x08;
    rd->version = 2;
    rd->wave_offset = info_chunk - 0x08;   /* pretend to have a WAVE */

    return;
}

static void read_rwar(rwav_data_t* rd) {
    if (!is_id32be(rd->offset, rd->sf, "RWAR")) 
        return;

    if (read_u32be(rd->offset + 0x04, rd->sf) != 0xFEFF0100) /* version 0 */
        return;

    rd->offset += 0x60;
    read_rwav(rd);
    rd->version = 0;
    return;
}

/* RWSD is quite similar to BRSTM, but can contain several streams.
 * Still, some games use it for single streams. We only support the
 * single stream form here */
VGMSTREAM* init_vgmstream_rwsd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    char filename[PATH_LIMIT];

    size_t wave_length;
    int codec;
    int channels;
    int loop_flag;
    int rwar = 0;
    int rwav = 0;
    rwav_data_t rwav_data;

    size_t stream_size;

    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;

    rwav_data.version = -1;
    rwav_data.start_offset = 0;
    rwav_data.info_chunk = -1;
    rwav_data.wave_offset = -1;

    /* check extension, case insensitive */
    sf->get_name(sf,filename,sizeof(filename));

    if (check_extensions(sf, "rwsd")) {
        ;
    }
    else if (check_extensions(sf, "rwar")) {
        rwar = 1;
    }
    else if (check_extensions(sf, "rwav")) {
        rwav = 1;
    }
    else {
        goto fail;
    }

    read_16bit = read_16bitBE;
    read_32bit = read_32bitBE;


    /* check header */
    if (rwar || rwav) {
        rwav_data.offset = 0;
        rwav_data.sf = sf;
        rwav_data.read_32bit = read_32bit;

        if (rwar) read_rwar(&rwav_data);
        if (rwav) read_rwav(&rwav_data);
        if (rwav_data.wave_offset < 0) goto fail;
    }
    else {
        if (!is_id32be(0x00, sf, "RWSD")) 
            goto fail;

        switch (read_u32be(0x04, sf)) {
            case 0xFEFF0102:
                /* ideally we would look through the chunk list for a WAVE chunk,
                 * but it's always in the same order */

                /* get WAVE offset, check */
                rwav_data.wave_offset = read_32bit(0x18,sf);
                if (!is_id32be(rwav_data.wave_offset + 0x00, sf, "WAVE")) 
                    goto fail;

                /* get WAVE size, check */
                wave_length = read_32bit(0x1c,sf);
                if (read_32bit(rwav_data.wave_offset + 0x04,sf) != wave_length)
                    goto fail;

                /* check wave count */
                if (read_32bit(rwav_data.wave_offset + 0x08,sf) != 1)
                    goto fail; /* only support 1 */

                rwav_data.version = 2;
                break;

            case 0xFEFF0103:
                rwav_data.offset = 0xe0;
                rwav_data.sf = sf;
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
    codec = read_u8(rwav_data.wave_offset+0x10,sf);
    loop_flag = read_u8(rwav_data.wave_offset+0x11,sf);
    channels = read_u8(rwav_data.wave_offset+0x12,sf);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = dsp_nibbles_to_samples(read_32bit(rwav_data.wave_offset+0x1c,sf));
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(read_32bit(rwav_data.wave_offset+0x18,sf));

    vgmstream->sample_rate = (uint16_t)read_16bit(rwav_data.wave_offset + 0x14,sf);
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

    if (rwar) {
        vgmstream->meta_type = meta_RWAR;
    }
    else if (rwav) {
        vgmstream->meta_type = meta_RWAV;
    }
    else {
        vgmstream->meta_type = meta_RWSD;
    }

    {
        off_t data_start_offset;
        off_t codec_info_offset;
        int i, j;

        for (j = 0 ; j < vgmstream->channels; j++) {
            if (rwar || rwav) {
                /* This is pretty nasty, so an explaination is in order.
                * At 0x10 in the info_chunk is the offset of a table with
                * one entry per channel. Each entry in this table is itself
                * an offset to a set of information for the channel. The
                * first element in the set is the offset into DATA of the channel. 
                * The second element is the offset of the codec-specific setup for the channel. */

                off_t channel_info_offset = rwav_data.info_chunk +
                    read_32bit(rwav_data.info_chunk +
                        read_32bit(rwav_data.info_chunk + 0x10,sf) + j*0x04, sf);

                data_start_offset = rwav_data.start_offset +
                    read_32bit(channel_info_offset + 0x00, sf);
                codec_info_offset = rwav_data.info_chunk + 
                    read_32bit(channel_info_offset + 0x04, sf);

                vgmstream->ch[j].channel_start_offset =
                    vgmstream->ch[j].offset = data_start_offset;

            } else {
                // dummy for RWSD, must be a proper way to work this out
                codec_info_offset = rwav_data.wave_offset + 0x6c + j*0x30;
            }

            if (vgmstream->coding_type == coding_NGC_DSP) {
                for (i = 0; i < 16; i++) {
                    vgmstream->ch[j].adpcm_coef[i] = read_16bit(codec_info_offset + i*0x2, sf);
                }
            }
        }
    }

    if (rwar || rwav) {
        /* */
    }
    else {
        if (rwav_data.version == 2)
            rwav_data.start_offset = read_32bit(0x08, sf);
    }

    stream_size = read_32bit(rwav_data.wave_offset + 0x50,sf);

    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<channels;i++) {
            vgmstream->ch[i].streamfile = sf->open(sf,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

            if (!vgmstream->ch[i].streamfile) goto fail;

            if (!(rwar || rwav)) {
                vgmstream->ch[i].channel_start_offset=
                    vgmstream->ch[i].offset=
                    rwav_data.start_offset + i*stream_size;
            }
        }
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
