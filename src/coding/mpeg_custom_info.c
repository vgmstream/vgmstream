#include "coding.h"

/**
 * Gets info from a MPEG frame header at offset. Normally you would use mpg123_info but somehow
 * is/was wrong at times (maybe only in older versions?) so here we do our thing.
 */
bool mpeg_get_frame_info_h(uint32_t header, mpeg_frame_info* info) {
    /* index tables */
    static const int versions[4] = { /* MPEG 2.5 */ 3, /* reserved */ -1,  /* MPEG 2 */ 2, /* MPEG 1 */ 1 };
    static const int layers[4] = { -1,3,2,1 };
    static const int bit_rates[5][16] = { /* [version index ][bit rate index] (0=free, -1=bad) */
            { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, -1 }, /* MPEG1 Layer I */
            { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, -1 }, /* MPEG1 Layer II */
            { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, -1 }, /* MPEG1 Layer III */
            { 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, -1 }, /* MPEG2/2.5 Layer I */
            { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, -1 }, /* MPEG2/2.5 Layer II/III */
    };
    static const int sample_rates[4][4] = { /* [version][sample rate index] */
            { 44100, 48000, 32000, -1}, /* MPEG1 */
            { 22050, 24000, 16000, -1}, /* MPEG2 */
            { 11025, 12000,  8000, -1}, /* MPEG2.5 */
    };
    static const int channels[4] = { 2,2,2, 1 }; /* [channel] */
    static const int frame_samples[3][3] = { /* [version][layer] */
            { 384, 1152, 1152 }, /* MPEG1 */
            { 384, 1152, 576  }, /* MPEG2 */
            { 384, 1152, 576  }  /* MPEG2.5 */
    };

    int idx, padding;


    memset(info, 0, sizeof(*info));

    if ((header >> 21) != 0x7FF) /* 31-21: sync */
        goto fail;

    info->version = versions[(header >> 19) & 0x3]; /* 20,19: version */
    if (info->version <= 0) goto fail;

    info->layer = layers[(header >> 17) & 0x3]; /* 18,17: layer */
    if (info->layer <= 0 || info->layer > 3) goto fail;

    //crc       = (header >> 16) & 0x1; /* 16: protected by crc? */

    idx = (info->version==1 ? info->layer-1 : (3 + (info->layer==1 ? 0 : 1)));
    info->bit_rate = bit_rates[idx][(header >> 12) & 0xf]; /* 15-12: bit rate */
    if (info->bit_rate <= 0) goto fail;

    info->sample_rate = sample_rates[info->version-1][(header >> 10) & 0x3]; /* 11-10: sampling rate */
    if (info->sample_rate <= 0) goto fail;

    padding     = (header >>  9) & 0x1; /* 9: padding? */
    //private   = (header >>  8) & 0x1; /* 8: private bit */

    info->channels = channels[(header >>  6) & 0x3]; /* 7,6: channel mode */

    //js_mode   = (header >>  4) & 0x3; /* 5,4: mode extension for joint stereo */
    //copyright = (header >>  3) & 0x1; /* 3: copyrighted */
    //original  = (header >>  2) & 0x1; /* 2: original */
    //emphasis  = (header >>  0) & 0x3; /* 1,0: emphasis */

    info->frame_samples = frame_samples[info->version-1][info->layer-1];

    /* calculate frame length (from hcs's fsb_mpeg) */
    switch (info->frame_samples) {
        case 384:  info->frame_size = (12l  * info->bit_rate * 1000l / info->sample_rate + padding) * 4; break; /* 384/32 = 12 */
        case 576:  info->frame_size = (72l  * info->bit_rate * 1000l / info->sample_rate + padding); break; /* 576/8 = 72 */
        case 1152: info->frame_size = (144l * info->bit_rate * 1000l / info->sample_rate + padding); break; /* 1152/8 = 144 */
        default: goto fail;
    }

    return true;

fail:
    return false;
}

bool mpeg_get_frame_info(STREAMFILE* sf, off_t offset, mpeg_frame_info* info) {
    uint32_t header = read_u32be(offset, sf);
    return mpeg_get_frame_info_h(header, info);
}


uint32_t mpeg_get_tag_size(STREAMFILE* sf, uint32_t offset, uint32_t header) {
    if (!header)
        header = read_u32be(offset+0x00, sf);

    /* skip ID3v2 */
    if ((header & 0xFFFFFF00) == get_id32be("ID3\0")) {
        size_t frame_size = 0;
        uint8_t flags = read_u8(offset+0x05, sf);
        /* this is how it's officially read :/ */
        frame_size += read_u8(offset+0x06, sf) << 21;
        frame_size += read_u8(offset+0x07, sf) << 14;
        frame_size += read_u8(offset+0x08, sf) << 7;
        frame_size += read_u8(offset+0x09, sf) << 0;
        frame_size += 0x0a;
        if (flags & 0x10) /* footer? */
            frame_size += 0x0a;

        return frame_size;
    }

    /* skip ID3v1 */
    if ((header & 0xFFFFFF00) == get_id32be("TAG\0")) {
        ;VGM_LOG("MPEG: ID3v1 at %x\n", offset);
        return 0x80;
    }

    return 0;
}

size_t mpeg_get_samples(STREAMFILE* sf, off_t start_offset, size_t bytes) {
    off_t offset = start_offset;
    off_t max_offset = start_offset + bytes;
    int frames = 0, samples = 0, encoder_delay = 0, encoder_padding = 0;
    mpeg_frame_info info;

    if (!sf)
        return 0;

    if (max_offset > get_streamfile_size(sf))
        max_offset = get_streamfile_size(sf);

    /* MPEG may use VBR so must read all frames */
    while (offset < max_offset) {
        uint32_t header = read_u32be(offset+0x00, sf);
        size_t tag_size = mpeg_get_tag_size(sf, offset, header);
        if (tag_size) {
            offset += tag_size;
            continue;
        }

        /* regular frame (assumed) */
        if (!mpeg_get_frame_info_h(header, &info)) {
            VGM_LOG("MPEG: unknown frame at %lx\n", offset);
            break;
        }

        /* detect Xing header (disguised as a normal frame) */
        if (frames < 3) { /* should be first after tags */
            /* frame is empty so Xing goes after MPEG side info */
            off_t xing_offset;
            if (info.version == 1)
                xing_offset = (info.channels == 2 ? 0x20 : 0x11) + 0x04;
            else
                xing_offset = (info.channels == 2 ? 0x11 : 0x09) + 0x04;

            if (info.frame_size >= xing_offset + 0x78 &&
                read_u32be(offset + 0x04, sf) == 0 && /* empty frame */
                (read_u32be(offset + xing_offset, sf) == 0x58696E67 ||  /* "Xing" (mainly for VBR) */
                 read_u32be(offset + xing_offset, sf) == 0x496E666F)) { /* "Info" (mainly for CBR) */
                uint32_t flags = read_u32be(offset + xing_offset + 0x04, sf);

                if (flags & 1) {
                    uint32_t frame_count = read_u32be(offset + xing_offset + 0x08, sf);
                    samples = frame_count * info.frame_samples;
                }
                /* other flags indicate seek table and stuff */

                ;VGM_LOG("MPEG: found Xing header\n");

                /* vendor specific */
                if (info.frame_size > xing_offset + 0x78 + 0x24) {
                    uint32_t sub_id = read_u32be(offset + xing_offset + 0x78, sf);
                    if (sub_id == get_id32be("LAME") || /* LAME */
                        sub_id == get_id32be("Lavc")) { /* FFmpeg */
                        if (info.layer == 3) {
                            uint32_t delays = read_u32be(offset + xing_offset + 0x8C, sf);
                            encoder_delay   = ((delays >> 12) & 0xFFF);
                            encoder_padding =  ((delays >> 0) & 0xFFF);

                            encoder_delay += (528 + 1); /* implicit MDCT decoder delay (seen in LAME source) */
                            if (encoder_padding > 528 + 1)
                                encoder_padding -= (528 + 1);
                        }
                        else {
                            encoder_delay = 240 + 1;
                        }
                    }

                    /* replay gain and stuff */
                }

                /* there is also "iTunes" vendor with no apparent extra info, iTunes delays are in "iTunSMPB" ID3 tag */
                break; /* we got samples */
             }
        }

        //TODO: detect "VBRI" header (Fraunhofer encoder)
        // https://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header#VBRIHeader

        /* could detect VBR/CBR but read frames to remove ID3 end tags */

        frames++;
        offset += info.frame_size;

        // Rarely the last frame may be truncated and our MPEG decoder may reject it (even if it's just the padding byte).
        // Other decoders like foobar seem to return partial granules at EOF; could try to salvage it by feeding blank data
        VGM_ASSERT(offset > max_offset, "MPEG: truncated frame count (last samples will be blank)\n");
        samples += info.frame_samples;
    }

    ;VGM_LOG("MPEG: samples=%i, ed=%i, ep=%i, end=%i\n", samples,encoder_delay,encoder_padding, samples - encoder_delay - encoder_padding);

    //todo return encoder delay
    samples = samples - encoder_delay - encoder_padding;
    return samples;
}


/* variation of the above, for clean streams = no ID3/VBR headers
 * (maybe should be fused in a single thing with config, API is kinda messy too) */
int32_t mpeg_get_samples_clean(STREAMFILE* sf, off_t start, size_t size, uint32_t* p_loop_start, uint32_t* p_loop_end, int is_vbr) {
    mpeg_frame_info info;
    off_t offset = start;
    int32_t num_samples = 0, loop_start = 0, loop_end = 0;

    if (!is_vbr) {
        /* CBR = quick calcs */
        if (!mpeg_get_frame_info(sf, offset, &info))
            goto fail;

        num_samples = size / info.frame_size * info.frame_samples;
        if (p_loop_start)
            loop_start = *p_loop_start / info.frame_size * info.frame_samples;
        if (p_loop_end)
            loop_end = *p_loop_end / info.frame_size * info.frame_samples;
    }
    else {
        /* VBR (or unknown) = count frames */
        while (offset < start + size) {
            if (!mpeg_get_frame_info(sf, offset, &info))
                goto fail;

            if (p_loop_start && *p_loop_start + start == offset)
                loop_start = num_samples;

            num_samples += info.frame_samples;
            offset += info.frame_size;

            if (p_loop_end && *p_loop_end + start == offset)
                loop_end = num_samples;
        }
    }

    if (p_loop_start)
        *p_loop_start = loop_start;
    if (p_loop_end)
        *p_loop_end = loop_end;

    return num_samples;
fail:
    VGM_LOG("MPEG: sample reader failed at %lx\n", offset);
    return 0;
}
