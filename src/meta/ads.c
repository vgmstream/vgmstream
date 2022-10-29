#include "meta.h"
#include "../coding/coding.h"


/* .ADS - Sony's "Audio Stream" format [Edit Racing (PS2), Evergrace II (PS2), Pri-Saga! Portable (PSP)] */
VGMSTREAM* init_vgmstream_ads(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate, interleave, is_loop_samples = 0;
    size_t body_size, stream_size, file_size;
    uint32_t codec, loop_start_sample = 0, loop_end_sample = 0, loop_start_offset = 0, loop_end_offset = 0;
    coding_t coding_type;
    int ignore_silent_frame_cavia = 0, ignore_silent_frame_capcom = 0;


    /* checks */
    if (!is_id32be(0x00,sf,"SShd"))
        goto fail;

    /* .ads: actual extension
     * .ss2: demuxed videos (fake?)
     * .pcm: Taisho Mononoke Ibunroku (PS2)
     * .adx: Armored Core 3 (PS2)
     * (extensionless): MotoGP (PS2)
     * .800: Mobile Suit Gundam: The One Year War (PS2) */
    if (!check_extensions(sf, "ads,ss2,pcm,adx,,800"))
        goto fail;

    if (read_u32le(0x04,sf) != 0x18 &&  /* standard header size */
        read_u32le(0x04,sf) != 0x20 &&  /* True Fortune (PS2) */
        read_u32le(0x04,sf) != get_streamfile_size(sf) - 0x08) /* Katamari Damacy videos */
        goto fail;

    if (!is_id32be(0x20,sf,"SSbd"))
        goto fail;

    /* base values (a bit unorderly since devs hack ADS too much and detection is messy) */
    {
        codec = read_u32le(0x08,sf);
        sample_rate = read_s32le(0x0C,sf);
        channels = read_s32le(0x10,sf); /* up to 4 [Eve of Extinction (PS2)] */
        interleave = read_s32le(0x14,sf); /* set even when mono */


        switch(codec) {
            case 0x01: /* official definition */
            case 0x80000001: /* [Evergrace II (PS2), but not other From Soft games] */
                coding_type = coding_PCM16LE;

                /* Angel Studios/Rockstar San Diego videos codec hijack [Red Dead Revolver (PS2), Spy Hunter 2 (PS2)] */
                if (sample_rate == 12000 && interleave == 0x200) {
                    sample_rate = 48000;
                    interleave = 0x40;
                    coding_type = coding_DVI_IMA_int;
                    /* should try to detect IMA data but it's not so easy, this works ok since
                     * no known games use these settings, videos normally are 48000/24000hz */
                }
                break;

            case 0x10: /* official definition */
            case 0x02: /* Capcom games extension, stereo only [Megaman X7 (PS2), Breath of Fire V (PS2), Clock Tower 3 (PS2)] */
                coding_type = coding_PSX;
                break;

            case 0x00: /* PCM16BE from official docs, probably never used */
            default:
                vgm_logi("ADS: unknown codec\n");
                goto fail;
        }
    }


    /* sizes */
    {
        file_size = get_streamfile_size(sf);
        body_size = read_32bitLE(0x24,sf);

        /* bigger than file_size in rare cases, even if containing all data (ex. Megaman X7's SY04.ADS) */
        if (body_size + 0x28 > file_size) {
            body_size = file_size - 0x28;
        }

        /* True Fortune: weird stream size */
        if (body_size * 2 == file_size - 0x18) {
            body_size = (body_size * 2) - 0x10;
        }

        stream_size = body_size;
    }


    /* offset */
    {
        start_offset = 0x28;

        /* start padding (body size is ok, may have end padding) [Evergrace II (PS2), Armored Core 3 (PS2)] */
        /*  detection depends on files being properly ripped, so broken/cut files won't play ok */
        if (file_size - body_size >= 0x800) {
            start_offset = 0x800; /* aligned to sector */

            /* too much end padding, happens in Super Galdelic Hour's SEL.ADS, maybe in bad rips too */
            VGM_ASSERT(file_size - body_size > 0x8000, "ADS: big end padding %x\n", file_size - body_size);
        }

        /* "ADSC" container */
        if (coding_type == coding_PSX
                && read_32bitLE(0x28,sf) == 0x1000 /* real start */
                && read_32bitLE(0x2c,sf) == 0
                && read_32bitLE(0x1008,sf) != 0) {
            int i;
            int is_adsc = 1;

            /* should be empty up to data start */
            for (i = 0; i < 0xFDC/4; i++) {
                if (read_32bitLE(0x2c+(i*4),sf) != 0) {
                    is_adsc = 0;
                    break;
                }
            }

            if (is_adsc) {
                start_offset = 0x1000 - 0x08; /* remove "ADSC" alignment */
                /* stream_size doesn't count start offset padding */
            }
        }
    }


    /* loops */
    {
        uint32_t loop_start, loop_end;

        loop_start = read_u32le(0x18,sf);
        loop_end = read_u32le(0x1C,sf);

        loop_flag = 0;

        /* detect loops the best we can; docs say those are loop block addresses,
         * but each maker does whatever (no games seem to use PS-ADPCM loop flags though) */


        if (loop_start != 0xFFFFFFFF && loop_end == 0xFFFFFFFF) {

            if (codec == 0x02) { /* Capcom codec */
                /* Capcom games: loop_start is address * 0x10 [Mega Man X7, Breath of Fire V, Clock Tower 3] */
                loop_flag = ((loop_start * 0x10) + 0x200 < body_size); /* near the end (+0x20~80) means no loop */
                loop_start_offset = loop_start * 0x10;
                ignore_silent_frame_capcom = 1;
            }
            else if (is_id32be(0x28,sf, "PAD!")) { /* padding until 0x800 */
                /* Super Galdelic Hour: loop_start is PCM bytes */
                loop_flag = 1;
                loop_start_sample = loop_start / 2 / channels;
                is_loop_samples = 1;
            }
            else if ((loop_start % 0x800 == 0) && loop_start > 0) { /* sector-aligned, min/0 is 0x800 */
                /* cavia games: loop_start is offset [Drakengard 1/2, GITS: Stand Alone Complex] */
                /* offset is absolute from the "cavia stream format" container that adjusts ADS start */
                loop_flag = 1;
                loop_start_offset = loop_start - 0x800;
                ignore_silent_frame_cavia = 1;
            }
            else if (loop_start % 0x800 != 0 || loop_start == 0) { /* not sector aligned */
                /* Katakamuna: loop_start is address * 0x10 */
                loop_flag = 1;
                loop_start_offset = loop_start * 0x10;
            }
        }
        else if (loop_start != 0xFFFFFFFF && loop_end != 0xFFFFFFFF
                && loop_end > 0) { /* ignore Kamen Rider Blade and others */
#if 0
            //todo improve detection to avoid clashing with address*0x20
            if (loop_end == body_size / 0x10) { /* always body_size? but not all files should loop */
                /* Akane Iro ni Somaru Saka - Parallel: loops is address * 0x10 */
                loop_flag = 1;
                loop_start_offset = loop_start * 0x10;
                loop_end_offset = loop_end * 0x10;
            }
#endif
            if (loop_end <= body_size / 0x200 && coding_type == coding_PCM16LE) { /* close to body_size */
                /* Gofun-go no Sekai: loops is address * 0x200 */
                loop_flag = 1;
                loop_start_offset = loop_start * 0x200;
                loop_end_offset = loop_end * 0x200;
            }
            else if (loop_end <= body_size / 0x70 && coding_type == coding_PCM16LE) { /* close to body_size */
                /* Armored Core - Nexus: loops is address * 0x70 */
                loop_flag = 1;
                loop_start_offset = loop_start * 0x70;
                loop_end_offset = loop_end * 0x70;
            }
            else if (loop_end <= body_size / 0x20 && coding_type == coding_PCM16LE) { /* close to body_size */
                /* Armored Core - Nine Breaker: loops is address * 0x20 */
                loop_flag = 1;
                loop_start_offset = loop_start * 0x20;
                loop_end_offset = loop_end * 0x20;
            }
            else if (loop_end <= body_size / 0x20 && coding_type == coding_PSX) {
                /* various games: loops is address * 0x20 [Fire Pro Wrestling Returns, A.C.E. - Another Century's Episode] */
                loop_flag = 1;
                loop_start_offset = loop_start * 0x20;
                loop_end_offset = loop_end * 0x20;
            }
            else if (loop_end <= body_size / 0x10 && coding_type == coding_PSX
                    && (read_32bitBE(0x28 + loop_end*0x10 + 0x10 + 0x00, sf) == 0x00077777 ||
                        read_32bitBE(0x28 + loop_end*0x10 + 0x20 + 0x00, sf) == 0x00077777)) {
                /* not-quite-looping sfx, ending with a "non-looping PS-ADPCM end frame" [Kono Aozora ni Yakusoku, Chanter] */
                loop_flag = 0;
            }
            else if ((loop_end > body_size / 0x20 && coding_type == coding_PSX) ||
                     (loop_end > body_size / 0x70 && coding_type == coding_PCM16LE)) {
                /* various games: loops in samples [Eve of Extinction, Culdcept, WWE Smackdown! 3] */
                loop_flag = 1;
                loop_start_sample = loop_start;
                loop_end_sample = loop_end;
                is_loop_samples = 1;
            }
        }

        //todo Jet Ion Grand Prix seems to have some loop-like values at 0x28
        //todo Yoake mae yori Ruriiro na has loops in unknown format
    }


    /* most games have empty PS-ADPCM frames in the last interleave block that should be skipped for smooth looping */
    if (coding_type == coding_PSX) {
        off_t offset, min_offset;

        offset = start_offset + stream_size;
        min_offset = offset - interleave;

        do {
            offset -= 0x10;

            if (read_8bit(offset+0x01,sf) == 0x07) {
                stream_size -= 0x10*channels;/* ignore don't decode flag/padding frame (most common) [ex. Capcom games] */
            }
            else if (read_32bitBE(offset+0x00,sf) == 0x00000000 && read_32bitBE(offset+0x04,sf) == 0x00000000 &&
                     read_32bitBE(offset+0x08,sf) == 0x00000000 && read_32bitBE(offset+0x0c,sf) == 0x00000000) {
                stream_size -= 0x10*channels; /* ignore null frame [ex. A.C.E. Another Century Episode 1/2/3] */
            }
            else if (read_32bitBE(offset+0x00,sf) == 0x00007777 && read_32bitBE(offset+0x04,sf) == 0x77777777 &&
                     read_32bitBE(offset+0x08,sf) == 0x77777777 && read_32bitBE(offset+0x0c,sf) == 0x77777777) {
                stream_size -= 0x10*channels; /* ignore padding frame [ex. Akane Iro ni Somaru Saka - Parallel]  */
            }
            else if (read_32bitBE(offset+0x00,sf) == 0x0C020000 && read_32bitBE(offset+0x04,sf) == 0x00000000 &&
                     read_32bitBE(offset+0x08,sf) == 0x00000000 && read_32bitBE(offset+0x0c,sf) == 0x00000000 &&
                     ignore_silent_frame_cavia) {
                stream_size -= 0x10*channels; /* ignore silent frame [ex. cavia games]  */
            }
            else if (read_32bitBE(offset+0x00,sf) == 0x0C010000 && read_32bitBE(offset+0x04,sf) == 0x00000000 &&
                     read_32bitBE(offset+0x08,sf) == 0x00000000 && read_32bitBE(offset+0x0c,sf) == 0x00000000 &&
                     ignore_silent_frame_capcom) {
                stream_size -= 0x10*channels; /* ignore silent frame [ex. Capcom games]  */
            }
            else {
                break; /* standard frame */
            }
        }
        while(offset > min_offset);

        /* don't bother fixing loop_end_offset since will be adjusted to num_samples later, if needed */
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->coding_type = coding_type;
    vgmstream->interleave_block_size = interleave;
    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_ADS;

    switch(coding_type) {
        case coding_PCM16LE:
            vgmstream->num_samples = pcm16_bytes_to_samples(stream_size, channels);
            break;
        case coding_PSX:
            vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);
            break;
        case coding_DVI_IMA_int:
            vgmstream->num_samples = ima_bytes_to_samples(stream_size, channels);
            break;
        default:
            goto fail;
    }

    if (vgmstream->loop_flag) {
        if (is_loop_samples) {
            vgmstream->loop_start_sample = loop_start_sample;
            vgmstream->loop_end_sample = loop_end_sample;
        }
        else {
            switch(vgmstream->coding_type) {
                case coding_PCM16LE:
                    vgmstream->loop_start_sample = pcm16_bytes_to_samples(loop_start_offset, channels);
                    vgmstream->loop_end_sample = pcm16_bytes_to_samples(loop_end_offset, channels);
                    break;
                case coding_PSX:
                    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start_offset, channels);
                    vgmstream->loop_end_sample = ps_bytes_to_samples(loop_end_offset, channels);
                    break;
                default:
                    goto fail;
            }
        }

        /* when loop_end = 0xFFFFFFFF */
        if (vgmstream->loop_end_sample == 0)
            vgmstream->loop_end_sample = vgmstream->num_samples;

        /* happens even when loops are directly samples, loops sound fine (ex. Culdcept) */
        if (vgmstream->loop_end_sample > vgmstream->num_samples)
            vgmstream->loop_end_sample = vgmstream->num_samples;
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ****************************************************************************** */

/* ADS in containers */
VGMSTREAM* init_vgmstream_ads_container(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset;
    size_t subfile_size;

    /* checks */
    if (is_id32be(0x00,sf, "ADSC")) {
        /* Kenka Bancho 2, Kamen Rider Hibiki/Kabuto, Shinjuku no Okami */
        if (read_u32le(0x04,sf) != 0x01)
            goto fail;

        if (!check_extensions(sf, "ads"))
            goto fail;

        subfile_offset = 0x08;
    }
    else if (is_id32be(0x00,sf, "cavi") &&
             is_id32be(0x04,sf, "a st") &&
             is_id32be(0x08,sf, "ream")) {
        /* cavia games: Drakengard 1/2, Dragon Quest Yangus, GITS: Stand Alone Complex */
        subfile_offset = 0x7d8;

        /* .ads: assumed
         * .cads: probable extension (found in bigfiles as LE field) */
        if (!check_extensions(sf, "ads,cads"))
            goto fail;
    }
    else {
        goto fail;
    }

    subfile_size = get_streamfile_size(sf) - subfile_offset;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, "ads");
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_ads(temp_sf);
    close_streamfile(temp_sf);

    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
