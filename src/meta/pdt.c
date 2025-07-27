#include "meta.h"
#include "../coding/coding.h"

/* .PDT - Hudson's stream container [Adventure Island (GC), Muscle Champion (GC), Mario Party series (GC)] */
VGMSTREAM* init_vgmstream_pdt(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int loop_flag, channel_count, sample_rate;
    size_t entries, nibble_size, loop_start;
    off_t entries_offset, coefs_offset, header_offset;
    off_t channel1_offset = 0, channel2_offset = 0, coef_offset = 0;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!check_extensions(sf, "pdt"))
        return NULL;

    if (read_16bitBE(0x00,sf) != 0x01)      /* version? */
        return NULL;
    if (read_32bitBE(0x04,sf) != 0x02 &&    /* Mario Party 4 (GC) */
        read_32bitBE(0x04,sf) != 0x04)      /* Cubic Lode Runner (GC) */
        return NULL;
    if (read_32bitBE(0x08,sf) != 0x7d00)    /* not-sample rate? */
        return NULL;
    if (read_32bitBE(0x0c,sf) != 0x02 &&    /* not-channels? */
        read_32bitBE(0x0c,sf) != 0x04)
        return NULL;

    entries = read_16bitBE(0x02,sf);
    entries_offset = read_32bitBE(0x10,sf);
    coefs_offset   = read_32bitBE(0x14,sf);
  //headers_offset = read_32bitBE(0x18,streamFile); /* we'll have pointers to those two */
  //streams_offset = read_32bitBE(0x1c,streamFile);

    /* find subsongs and target header, as entries can be empty/repeated */
    {
        /* tables to cache reads as it can be kinda slow with so many loops */
        uint32_t data_offsets[0x2000];
        uint32_t entry_offset, data_offset;

        if (entries > 0x2000)
            goto fail;

        total_subsongs = 0;
        if (target_subsong == 0) target_subsong = 1;

        header_offset = 0;
        for (int i = 0; i < entries; i++) {
            int is_unique = 1;

            entry_offset = read_32bitBE(entries_offset + i*0x04,sf);
            if (entry_offset == 0x00)
                continue;
            data_offset = read_32bitBE(entry_offset+0x10,sf);

            /* check if current entry header was repeated (same file offset, difference in flags only) */
            for (int j = 0; j < total_subsongs; j++)  {
                if (data_offsets[j] == data_offset) {
                    is_unique = 0;
                    break;
                }
            }
            if (!is_unique)
                continue;

            data_offsets[total_subsongs] = data_offset;
            total_subsongs++;

            /* target GET, but keep going to count subsongs */
            if (!header_offset && target_subsong == total_subsongs) {
                header_offset = entry_offset;
            }
        }
    }

    /* parse header */
    {
        uint8_t flags;
        size_t coef1_entry;
        off_t coef1_offset;

        flags       =    read_8bit(header_offset+0x00,sf);
        sample_rate = read_32bitBE(header_offset+0x04,sf);
        /* 0x01: unknown + 0x4000 */
        sample_rate = read_32bitBE(header_offset+0x04,sf);
        nibble_size = read_32bitBE(header_offset+0x08,sf);
        loop_start  = read_32bitBE(header_offset+0x0c,sf);

        channel1_offset = read_32bitBE(header_offset+0x10,sf);
        coef1_entry     = read_16bitBE(header_offset+0x14,sf);
        coef1_offset    = coefs_offset + coef1_entry*0x20;

        if (flags & 0x01) {
            //size_t coef2_entry;
            //off_t coef2_offset;

            channel2_offset = read_32bitBE(header_offset+0x18,sf);
            /* always after coef1 in practice */
            //coef2_entry     = read_16bitBE(header_offset+0x1c,streamFile);
            //coef2_offset    = coefs_offset + coef2_entry*0x20;
            //if (coef1_offset + 0x20 != coef2_offset)
            //    goto fail;
        }

        coef_offset = coef1_offset;
        loop_flag = (flags & 0x02);
        channel_count = (flags & 0x01) ? 2 : 1;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    //vgmstream->num_samples = dsp_bytes_to_samples(data_size, channel_count);//todo remove
    vgmstream->num_samples = dsp_nibbles_to_samples(nibble_size);
    //vgmstream->loop_start_sample = dsp_bytes_to_samples(loop_start, channel_count);//todo remove
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(loop_start);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_PDT;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    dsp_read_coefs_be(vgmstream, sf, coef_offset, 0x20);

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = nibble_size / 2 * channel_count;

    if (!vgmstream_open_stream(vgmstream,sf,channel1_offset))
        goto fail;

    /* channels may start at slightly separated offsets */
    if (channel_count == 2) {
        vgmstream->ch[1].channel_start_offset = vgmstream->ch[1].offset = channel2_offset;
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* PDT - custom fake header for split (PDTExt) .ptd [Mario Party (GC)] */
VGMSTREAM* init_vgmstream_pdt_split(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (!check_extensions(sf, "pdt"))
        return NULL;

    /* 0x10 fake header + chunks of the original header / data pasted together */
    if (read_32bitBE(0x00,sf) != 0x50445420 && /* "PDT " */
        read_32bitBE(0x04,sf) != 0x44535020 && /* "DSP " */
        read_32bitBE(0x08,sf) != 0x48454144 && /* "HEAD " */
        read_16bitBE(0x0C,sf) != 0x4552)       /* "ER " */
        goto fail;

    start_offset = 0x800;
    channels = (uint16_t)(read_16bitLE(0x0E,sf));
    loop_flag = (read_32bitBE(0x1C,sf) != 2);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x14,sf);

    if (channels == 1) {
        vgmstream->num_samples = read_32bitBE(0x18,sf)*14/8/channels/2;
        if (loop_flag) {
            vgmstream->loop_start_sample = read_32bitBE(0x1C,sf)*14/8/channels/2;
            vgmstream->loop_end_sample = read_32bitBE(0x18,sf)*14/8/channels/2;
        }
    }
    else if (channels == 2) {
        vgmstream->num_samples = read_32bitBE(0x18,sf)*14/8/channels;
        if (loop_flag) {
            vgmstream->loop_start_sample = read_32bitBE(0x1C,sf)*14/8/channels;
            vgmstream->loop_end_sample = read_32bitBE(0x18,sf)*14/8/channels;
        }
    }
    else {
        goto fail;
    }

    vgmstream->meta_type = meta_PDT;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    dsp_read_coefs_be(vgmstream, sf, 0x50, 0x20);

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;

    if (channels == 2) {
        vgmstream->ch[1].channel_start_offset =
                vgmstream->ch[1].offset = ((get_streamfile_size(sf)+start_offset) / channels);
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
