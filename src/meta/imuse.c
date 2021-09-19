#include "meta.h"
#include "../coding/coding.h"


/* LucasArts iMUSE (Interactive Music Streaming Engine) formats */
VGMSTREAM* init_vgmstream_imuse(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, name_offset = 0;
    off_t head_offset, map_offset, offset;
    size_t map_size, data_bytes;
    int loop_flag, channels, sample_rate, num_samples;


    /* checks */

    /* base decoder block table */
    if (is_id32be(0x00, sf, "COMP")) { /* The Curse of Monkey Island (PC), The Dig (PC) */
        int entries = read_u32be(0x04,sf);
        head_offset = 0x10 + entries * 0x10 + 0x02; /* base header + table + header size */
    }
    else if (is_id32be(0x00, sf, "MCMP")) { /* Grim Fandango (multi), Star Wars: X-Wing Alliance (PC) */
        int entries = read_u16be(0x04,sf);
        head_offset = 0x06 + entries * 0x09; /* base header + table */
        head_offset += 0x02 + read_u16be(head_offset, sf); /* + mini text header */
    }
    else {
        goto fail;
    }

    /* .imx: The Curse of Monkey Island (PC) 
     * .imc: Grim Fandango (multi)
     * .wav: Grim Fandango (multi) RIFF sfx */
    if (!check_extensions(sf, "imx,imc,wav,lwav"))
        goto fail;


    /* "offsets" below seem to count decoded data. Data is divided into variable-sized blocks that usually
     * return 0x2000 bytes (starting from and including header). File starts with a block table to make
     * this manageable. Most offsets don't seem to match block or data boundaries so not really sure. */

    /* main header after table */
    if (is_id32be(head_offset, sf, "iMUS")) { /* COMP/MCMP */
        int header_found = 0;

        /* 0x04: decompressed size (header size + pcm bytes) */
        if (!is_id32be(head_offset + 0x08, sf, "MAP "))
            goto fail;
        map_size = read_u32be(head_offset + 0x0c, sf);
        map_offset = head_offset + 0x10;

        /* MAP table (commands for interactive use) */
        offset = map_offset;
        while (offset < map_offset + map_size) {
            uint32_t type = read_u32be(offset + 0x00, sf);
            uint32_t size = read_u32be(offset + 0x04, sf);
            offset += 0x08;

            switch(type) {
                case 0x46524D54: /* "FRMT" (header, always first) */
                    if (header_found)
                        goto fail;
                    header_found = 1;
                    /* 00: data offset */
                    /* 04: 0? */
                    /* 08: sample size (16b) */
                    sample_rate = read_u32be(offset + 0x0c,sf);
                    channels    = read_u32be(offset + 0x10,sf);
                    break;

                case 0x54455854: /* "TEXT" (info) */
                    /* optional info usually before some REGN: "****"=start, "loop"=loop,
                     * use first TEXT as name, usually filename for music */
                    /* 00: offset */
                    if (!name_offset) /*  */
                        name_offset = offset + 0x04; /* null terminated */
                    break;

                /* - SYNC: 'lip' sync info
                 *   00 offset
                 *   04 sync commands until end?
                 * - REGN: section config (at least one?)
                 *   00 offset
                 *   04 size
                 * - JUMP: usually defines a loop, sometimes after a REGN
                 *   00 offset (from iMUS)
                 *   04 size?
                 *   08 number?
                 *   0c size?
                 * - STOP: last command (always?)
                 *   00 offset
                 */
                default: /* maybe set REGN as subsongs? + imuse_set_region(vgmstream->data, offset, size) */
                    break;
            }
            offset += size;
        }

        if (!header_found)
            goto fail;

        if (!is_id32be(head_offset + 0x10 + map_size + 0x00, sf, "DATA"))
            goto fail;
        data_bytes = read_u32be(head_offset + 0x10 + map_size + 0x04, sf);
        num_samples = data_bytes / channels / sizeof(int16_t);
    }
    else if (is_id32be(head_offset, sf, "RIFF")) { /* MCMP voices */
        /* standard (LE), with fake codec 1 and sizes also in decoded bytes (see above),
         * has standard RIFF chunks (may include extra), start offset in MCSC */

        if (!find_chunk_le(sf, 0x666D7420, head_offset + 0x0c, 0, &offset, NULL)) /* "fmt " */
            goto fail;
        channels    = read_u16le(offset + 0x02,sf);
        sample_rate = read_u32le(offset + 0x04,sf);

        if (!find_chunk_le(sf, 0x64617461, head_offset + 0x0c, 0, NULL, &data_bytes)) /*"data"*/
            goto fail;
        num_samples = data_bytes / channels / sizeof(int16_t);
    }
    else {
        vgm_logi("IMUSE: unsupported format\n");
        goto fail; /* The Dig (PC) has no header, detect? (needs a bunch of sub-codecs) */
    }

    loop_flag = 0;
    start_offset = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_IMUSE;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    vgmstream->coding_type = coding_IMUSE;
    vgmstream->layout_type = layout_none;
    vgmstream->codec_data = init_imuse(sf, channels);
    if (!vgmstream->codec_data) goto fail;

    if (name_offset > 0)
        read_string(vgmstream->stream_name, STREAM_NAME_SIZE, name_offset, sf);


    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
