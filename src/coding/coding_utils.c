#include "coding.h"
#include <math.h>
#include "../vgmstream.h"


/**
 * Various utils for formats that aren't handled their own decoder or meta
 */

/* ******************************************** */
/* XMA PARSING                                  */
/* ******************************************** */

/* read num_bits (up to 25) from a bit offset.
 * 25 since we read a 32 bit int, and need to adjust up to 7 bits from the byte-rounded fseek (32-7=25) */
static uint32_t read_bitsBE_b(int64_t bit_offset, int num_bits, STREAMFILE* sf) {
    uint32_t num, mask;
    if (num_bits > 25) return -1; //???

    num = read_32bitBE(bit_offset / 8, sf); /* fseek rounded to 8 */
    num = num << (bit_offset % 8); /* offset adjust (up to 7) */
    num = num >> (32 - num_bits);
    mask = 0xffffffff >> (32 - num_bits);

    return num & mask;
}


static void ms_audio_parse_header(STREAMFILE* sf, int xma_version, int64_t offset_b, int bits_frame_size, size_t *first_frame_b, size_t *packet_skip_count, size_t *header_size_b) {

    if (xma_version == 1) { /* XMA1 */
        //packet_sequence  = read_bitsBE_b(offset_b+0,  4,  sf); /* numbered from 0 to N */
        //unknown          = read_bitsBE_b(offset_b+4,  2,  sf); /* packet_metadata? (always 2) */
        *first_frame_b     = read_bitsBE_b(offset_b+6,  bits_frame_size, sf); /* offset in bits inside the packet */
        *packet_skip_count = read_bitsBE_b(offset_b+21, 11, sf); /* packets to skip for next packet of this stream */
        *header_size_b     = 32;
    } else if (xma_version == 2) { /* XMA2 */
        //frame_count      = read_bitsBE_b(offset_b+0,  6,  sf); /* frames that begin in this packet */
        *first_frame_b     = read_bitsBE_b(offset_b+6,  bits_frame_size, sf); /* offset in bits inside this packet */
        //packet_metadata = read_bitsBE_b(offset_b+21, 3,  sf); /* packet_metadata (always 1) */
        *packet_skip_count = read_bitsBE_b(offset_b+24, 8,  sf); /* packets to skip for next packet of this stream */
        *header_size_b     = 32;
    } else { /* WMAPRO(v3) */
        //packet_sequence  = read_bitsBE_b(offset_b+0,  4,  sf); /* numbered from 0 to N */
        //unknown          = read_bitsBE_b(offset_b+4,  2,  sf); /* packet_metadata? (always 2) */
        *first_frame_b     = read_bitsBE_b(offset_b+6,  bits_frame_size, sf);  /* offset in bits inside the packet */
        *packet_skip_count = 0; /* xwma has no need to skip packets since it uses real multichannel audio */
        *header_size_b     = 4+2+bits_frame_size; /* variable-sized header */
    }


    /* XMA2 packets with XMA1 RIFF (transmogrified), remove the packet metadata flag */
    if (xma_version == 1 && (*packet_skip_count & 0x700) == 0x100) {
        //VGM_LOG("MS_SAMPLES: XMA1 transmogrified packet header at 0x%lx\n", (off_t)offset_b/8);
        *packet_skip_count &= ~0x100;
    }

    /* full packet skip, no new frames start in this packet (prev frames can end here)
     * standardized to some value */
    if (*packet_skip_count == 0x7FF) { /* XMA1, 11b */
        VGM_LOG("MS_SAMPLES: XMA1 full packet_skip\n");// at %"PRIx64"\n", offset_b/8);
        *packet_skip_count = 0x800;
    }
    else if (*packet_skip_count == 0xFF) { /* XMA2, 8b*/
        VGM_LOG("MS_SAMPLES: XMA2 full packet_skip\n");// at %"PRIx64"\n", offset_b/8);
        *packet_skip_count = 0x800;
    }

    /* unusual but not impossible, as the encoder can interleave packets in any way */
    VGM_ASSERT((*packet_skip_count > 10 && *packet_skip_count < 0x800),
            "MS_SAMPLES: found big packet skip %i at 0x%x\n", *packet_skip_count, (uint32_t)offset_b/8);
}

/**
 * Find total and loop samples of Microsoft audio formats (WMAPRO/XMA1/XMA2) by reading frame headers.
 *
 * The stream is made of packets, each containing N small frames of X samples. Frames are further divided into subframes.
 * XMA1/XMA2 can divided into streams for multichannel (1/2ch ... 1/2ch). From the file start, packet 1..N is owned by
 * stream 1..N. Then must follow "packet_skip" value to find the stream next packet, as they are arbitrarily interleaved.
 * We only need to follow the first stream, as all must contain the same number of samples.
 *
 * XMA1/XMA2/WMAPRO data only differs in the packet headers.
 */
static void ms_audio_get_samples(ms_sample_data* msd, STREAMFILE* sf, int channels_per_packet, int bytes_per_packet, int samples_per_frame, int samples_per_subframe, int bits_frame_size) {
    int frames = 0, samples = 0, loop_start_frame = 0, loop_end_frame = 0;

    size_t first_frame_b, packet_skip_count, header_size_b, frame_size_b;
    int64_t offset_b, packet_offset_b, frame_offset_b;

    size_t packet_size = bytes_per_packet;
    size_t packet_size_b = packet_size * 8;
    int64_t offset = msd->data_offset;
    int64_t max_offset = msd->data_offset + msd->data_size;
    int64_t stream_offset_b = msd->data_offset * 8;

    /* read packets */
    while (offset < max_offset) {
        offset_b = offset * 8; /* global offset in bits */
        offset += packet_size; /* global offset in bytes */

        /* packet header */
        ms_audio_parse_header(sf, msd->xma_version, offset_b, bits_frame_size, &first_frame_b, &packet_skip_count, &header_size_b);
        if (packet_skip_count > 0x7FF) {
            continue; /* full skip */
        }

        packet_offset_b = header_size_b + first_frame_b;
        /* skip packets not owned by the first stream for next time */
        offset += packet_size * (packet_skip_count);


        /* read packet frames */
        while (packet_offset_b < packet_size_b) {
            frame_offset_b = offset_b + packet_offset_b; /* in bits for aligment stuff */

            /* frame loops, later adjusted with subframes (seems correct vs tests) */
            if (msd->loop_flag && (offset_b + packet_offset_b) - stream_offset_b == msd->loop_start_b)
                loop_start_frame = frames;
            if (msd->loop_flag && (offset_b + packet_offset_b) - stream_offset_b == msd->loop_end_b)
                loop_end_frame = frames;

            /* frame header */
            frame_size_b = read_bitsBE_b(frame_offset_b, bits_frame_size, sf);
            frame_offset_b += bits_frame_size;

            /* stop when packet padding starts (0x00 for XMA1 or 0xFF in XMA2) */
            if (frame_size_b == 0 || frame_size_b == (0xffffffff >> (32 - bits_frame_size)))  {
                break;
            }
            packet_offset_b += frame_size_b; /* including header */


            samples += samples_per_frame;
            frames++;

            /* last bit in frame = more frames flag, end packet to avoid reading garbage in some cases
             * (last frame spilling to other packets also has this flag, though it's ignored here) */
            if (packet_offset_b < packet_size_b && !read_bitsBE_b(offset_b + packet_offset_b - 1, 1, sf)) {
                break;
            }
        }
    }

    /* result */
    msd->num_samples = samples;
    if (msd->loop_flag && loop_end_frame > loop_start_frame) {
        msd->loop_start_sample = loop_start_frame * samples_per_frame + msd->loop_start_subframe * samples_per_subframe;
        msd->loop_end_sample = loop_end_frame * samples_per_frame + (msd->loop_end_subframe) * samples_per_subframe;
    }

    /* the above can't properly read skips for WMAPro ATM, but should fixed to 1 frame anyway */
    if (msd->xma_version == 0) {
        msd->num_samples -= samples_per_frame; /* FFmpeg does skip this */
#if 0
        msd->num_samples += (samples_per_frame / 2); /* but doesn't add extra samples */
#endif
    }
}

/* simlar to the above but only gets skips */
static void ms_audio_get_skips(STREAMFILE* sf, int xma_version, off_t data_offset, int channels_per_packet, int bytes_per_packet, int samples_per_frame, int bits_frame_size, int *out_start_skip, int *out_end_skip) {
    int start_skip = 0, end_skip = 0;

    size_t first_frame_b, packet_skip_count, header_size_b, frame_size_b;
    int64_t offset_b, packet_offset_b, frame_offset_b;

    size_t packet_size = bytes_per_packet;
    size_t packet_size_b = packet_size * 8;
    int64_t offset = data_offset;

    /* read packet */
    {
        offset_b = offset * 8; /* global offset in bits */
        offset += packet_size; /* global offset in bytes */

        /* packet header */
        ms_audio_parse_header(sf, 2, offset_b, bits_frame_size, &first_frame_b, &packet_skip_count, &header_size_b);
        if (packet_skip_count > 0x7FF) {
            return; /* full skip */
        }

        packet_offset_b = header_size_b + first_frame_b;

        /* read packet frames */
        while (packet_offset_b < packet_size_b) {
            frame_offset_b = offset_b + packet_offset_b; /* in bits for aligment stuff */

            /* frame header */
            frame_size_b = read_bitsBE_b(frame_offset_b, bits_frame_size, sf);
            frame_offset_b += bits_frame_size;

            /* stop when packet padding starts (0x00 for XMA1 or 0xFF in XMA2) */
            if (frame_size_b == 0 || frame_size_b == (0xffffffff >> (32 - bits_frame_size)))  {
                break;
            }
            packet_offset_b += frame_size_b; /* including header */

            /* find skips (info from FFmpeg) */
            if (channels_per_packet && (xma_version == 1 || xma_version == 2)) {
                int flag;
                int len_tilehdr_size = 15; //todo incorrect but usable for XMA, fix for WMAPro (complex, see ffmpeg decode_tilehdr)

                frame_offset_b += len_tilehdr_size;

                /* ignore "postproc transform" */
                if (channels_per_packet > 1) {
                    flag = read_bitsBE_b(frame_offset_b, 1, sf);
                    frame_offset_b += 1;
                    if (flag) {
                        flag = read_bitsBE_b(frame_offset_b, 1, sf);
                        frame_offset_b += 1;
                        if (flag) {
                            frame_offset_b += 1 + 4 * channels_per_packet*channels_per_packet; /* 4-something per double channel? */
                        }
                    }
                }

                /* get start/end skips to get the proper number of samples (both can be 0) */
                flag = read_bitsBE_b(frame_offset_b, 1, sf);
                frame_offset_b += 1;
                if (flag) {
                    /* get start skip */
                    flag = read_bitsBE_b(frame_offset_b, 1, sf);
                    frame_offset_b += 1;
                    if (flag) {
                        int new_skip = read_bitsBE_b(frame_offset_b, 10, sf);
                        //;VGM_LOG("MS_SAMPLES: start_skip %i at 0x%x (bit 0x%x)\n", new_skip, (uint32_t)frame_offset_b/8, (uint32_t)frame_offset_b);
                        frame_offset_b += 10;

                        if (new_skip > samples_per_frame) /* from xmaencode */
                            new_skip = samples_per_frame;

                        if (start_skip==0) /* only use first skip */
                            start_skip = new_skip;
                    }

                    /* get end skip */
                    flag = read_bitsBE_b(frame_offset_b, 1, sf);
                    frame_offset_b += 1;
                    if (flag) {
                        int new_skip = read_bitsBE_b(frame_offset_b, 10, sf);
                        //;VGM_LOG("MS_SAMPLES: end_skip %i at 0x%x (bit 0x%x)\n", new_skip, (uint32_t)frame_offset_b/8, (uint32_t)frame_offset_b);
                        frame_offset_b += 10;

                        if (new_skip > samples_per_frame) /* from xmaencode  */
                            new_skip = samples_per_frame;

                        end_skip = new_skip; /* always use last skip */
                    }
                }
            }
        }
    }

    /* output results */
    if (out_start_skip) *out_start_skip = start_skip;
    if (out_end_skip) *out_end_skip = end_skip;
}


static int wma_get_samples_per_frame(int version, int sample_rate, uint32_t decode_flags) {
    int frame_len_bits;

    if (sample_rate <= 16000)
        frame_len_bits = 9;
    else if (sample_rate <= 22050 || (sample_rate <= 32000 && version == 1))
        frame_len_bits = 10;
    else if (sample_rate <= 48000 || version < 3)
        frame_len_bits = 11;
    else if (sample_rate <= 96000)
        frame_len_bits = 12;
    else
        frame_len_bits = 13;

    if (version == 3) {
        int tmp = decode_flags & 0x6;
        if (tmp == 0x2)
            ++frame_len_bits;
        else if (tmp == 0x4)
            --frame_len_bits;
        else if (tmp == 0x6)
            frame_len_bits -= 2;
    }

    return 1 << frame_len_bits;
}

static int xma_get_channels_per_stream(STREAMFILE* sf, off_t chunk_offset, int channels) {
    int start_stream = 0;
    int channels_per_stream = 0;

    /* get from stream config (needed to find skips) */
    if (chunk_offset) {
        int format = read_16bitLE(chunk_offset,sf);
        if (format == 0x0165 || format == 0x6501) { /* XMA1 */
            channels_per_stream = read_8bit(chunk_offset + 0x0C + 0x14*start_stream + 0x11,sf);
        } else if (format == 0x0166 || format == 0x6601) { /* new XMA2 */
            channels_per_stream = channels > 1 ? 2 : 1;
        } else { /* old XMA2 */
            int version = read_8bit(chunk_offset,sf);
            channels_per_stream = read_8bit(chunk_offset + 0x20 + (version==3 ? 0x00 : 0x08) + 0x4*start_stream + 0x00,sf);
        }
    }
    else if (channels) {
        channels_per_stream = channels == 1 ? 1 : 2; /* default for XMA without RIFF chunks, most common */
    }

    if (channels_per_stream > 2)
        channels_per_stream = 0;

    return channels_per_stream;
}

void xma_get_samples(ms_sample_data* msd, STREAMFILE* sf) {
    const int bytes_per_packet = 2048;
    const int samples_per_frame = 512;
    const int samples_per_subframe = 128;
    const int bits_frame_size = 15;
    int channels_per_stream = xma_get_channels_per_stream(sf, msd->chunk_offset, msd->channels);

    ms_audio_get_samples(msd, sf, channels_per_stream, bytes_per_packet, samples_per_frame, samples_per_subframe, bits_frame_size);
}

void wmapro_get_samples(ms_sample_data* msd, STREAMFILE* sf, int block_align, int sample_rate, uint32_t decode_flags) {
    const int version = 3; /* WMAPRO = WMAv3 */
    int bytes_per_packet = block_align;
    int samples_per_frame = 0;
    int samples_per_subframe = 0;
    int bits_frame_size = 0;
    int channels_per_stream = msd->channels;

    if (!(decode_flags & 0x40)) {
        VGM_LOG("MS_SAMPLES: no frame length in WMAPro\n");
        msd->num_samples = 0;
        return;
    }
    samples_per_frame = wma_get_samples_per_frame(version, sample_rate, decode_flags);
    bits_frame_size = (int)floor(log(block_align) / log(2)) + 4; /* max bits needed to represent this block_align */
    samples_per_subframe = 0; /* not needed as WMAPro can't use loop subframes (complex subframe lengths) */
    msd->xma_version = 0; /* signal it's not XMA */

    ms_audio_get_samples(msd, sf, channels_per_stream, bytes_per_packet, samples_per_frame, samples_per_subframe, bits_frame_size);
}

void wma_get_samples(ms_sample_data* msd, STREAMFILE* sf, int block_align, int sample_rate, uint32_t decode_flags) {
    const int version = 2; /* WMAv1 rarely used */
    int use_bit_reservoir = 0; /* last packet frame can spill into the next packet */
    int samples_per_frame = 0;
    int num_frames = 0;

    samples_per_frame = wma_get_samples_per_frame(version, sample_rate, decode_flags);

    /* assumed (ASF has a flag for this but XWMA doesn't) */
    if (version == 2)
        use_bit_reservoir = 1;


    if (!use_bit_reservoir) {
        /* 1 frame per packet */
        num_frames = msd->data_size / block_align + (msd->data_size % block_align ? 1 : 0);
    }
    else {
        /* variable frames per packet (mini-header values) */
        off_t offset = msd->data_offset;
        off_t max_offset = msd->data_offset + msd->data_size;
        while (offset < max_offset) { /* read packets (superframes) */
            int packet_frames;
            uint8_t header = read_8bit(offset, sf); /* upper nibble: index;  lower nibble: frames */

            /* frames starting in this packet (ie. counts frames that spill due to bit_reservoir) */
            packet_frames = (header & 0xf);

            num_frames += packet_frames;
            offset += block_align;
        }
    }

    msd->num_samples = num_frames * samples_per_frame;

#if 0 //todo apply once FFmpeg decode is ok
    msd->num_samples += (samples_per_frame / 2); /* last IMDCT samples */
    msd->num_samples -= (samples_per_frame * 2); /* WMA default encoder delay */
#endif
}

int32_t xwma_get_samples(STREAMFILE* sf, uint32_t data_offset, uint32_t data_size, int format, int channels, int sample_rate, int block_size) {
    /* manually find total samples, why don't they put this in the header is beyond me */
    ms_sample_data msd = {0};

    msd.channels = channels;
    msd.data_offset = data_offset;
    msd.data_size = data_size;

    if (format == 0x0162)
        wmapro_get_samples(&msd, sf, block_size, sample_rate, 0x00E0);
    else
        wma_get_samples(&msd, sf, block_size, sample_rate, 0x001F);

    return msd.num_samples;
}

int32_t xwma_dpds_get_samples(STREAMFILE* sf, uint32_t dpds_offset, uint32_t dpds_size, int channels, int be) {
    int32_t (*read_s32)(off_t,STREAMFILE*) = be ? read_s32be : read_s32le;
    uint32_t offset;
    if (!dpds_offset || !dpds_size || !channels)
        return 0;

    offset = dpds_offset + (dpds_size - 0x04); /* last entry */
    /* XWMA's seek table ("dpds") contains max decoded bytes (after encoder delay), checked vs xWMAEncode.
     * WMAPRO usually encodes a few more tail samples though (see xwma_get_samples). */
    return read_s32(offset, sf) / channels / sizeof(int16_t); /* in PCM16 bytes */
}


/* XMA hell for precise looping and gapless support, fixes raw sample values from headers
 * that don't count XMA's final subframe/encoder delay/encoder padding, and FFmpeg stuff.
 * Configurable since different headers vary for maximum annoyance. */
void xma_fix_raw_samples_ch(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t stream_offset, size_t stream_size, int channels_per_stream, int fix_num_samples, int fix_loop_samples) {
    const int bytes_per_packet = 2048;
    const int samples_per_frame = 512;
    const int samples_per_subframe = 128;
    const int bits_frame_size = 15;

    int xma_version = 2; /* works ok even for XMA1 */
    off_t first_packet = stream_offset;
    off_t last_packet = stream_offset + stream_size - bytes_per_packet;
    int32_t start_skip = 0, end_skip = 0;

    if (stream_offset + stream_size > get_streamfile_size(sf)) {
        VGM_LOG("XMA SKIPS: ignoring bad stream offset+size vs real size\n");
        return;
    }

    /* find delay/padding values in the bitstream (should be safe even w/ multistreams
     * as every stream repeats them). Theoretically every packet could contain skips,
     * doesn't happen in practice though. */
    ms_audio_get_skips(sf, xma_version, first_packet, channels_per_stream, bytes_per_packet, samples_per_frame, bits_frame_size, &start_skip, NULL);
    ms_audio_get_skips(sf, xma_version,  last_packet, channels_per_stream, bytes_per_packet, samples_per_frame, bits_frame_size, NULL, &end_skip);

    //;VGM_LOG("XMA SKIPS: apply start=%i, end=%i\n", start_skip, end_skip);
    VGM_ASSERT(start_skip < samples_per_frame, "XMA SKIPS: small start skip\n");

    if (end_skip == 512) { /* most likely a read bug */
        VGM_LOG("XMA SKIPS: ignoring big end_skip\n");
        end_skip = 0;
    }


    /* apply XMA extra samples */
    if (fix_num_samples) {
        vgmstream->num_samples += samples_per_subframe; /* final extra IMDCT samples */
        vgmstream->num_samples -= start_skip; /* first samples skipped at the beginning */
        vgmstream->num_samples -= end_skip; /* last samples discarded at the end */
    }

    /* from xma2encode tests this is correct (probably encodes/decodes loops considering all skips), ex.-
     * full loop wav to xma makes start=384 (0 + ~512 delay - 128 padding), then xma to wav has "smpl" start=0 */
    if (fix_loop_samples && vgmstream->loop_flag) {
        vgmstream->loop_start_sample += samples_per_subframe;
        vgmstream->loop_start_sample -= start_skip;
        vgmstream->loop_end_sample += samples_per_subframe;
        vgmstream->loop_end_sample -= start_skip;
        /* since loops are adjusted this shouldn't happen (often loop_end == num_samples after applying all) */
        if (vgmstream->loop_end_sample > vgmstream->num_samples &&
                vgmstream->loop_end_sample - end_skip <= vgmstream->loop_end_sample) {
            VGM_LOG("XMA SAMPLES: adjusted loop end\n");
            vgmstream->loop_end_sample -= end_skip;
        }
    }


#if 0
    //TODO: ffmpeg now handles internal frame encoder delay, but not correctly in all cases
    // without this in most cases should be equivalent as before
//#ifdef VGM_USE_FFMPEG
    /* also fix FFmpeg, since we now know exact skips */
    {
        ffmpeg_codec_data* data = vgmstream->codec_data;

        /* FFmpeg doesn't XMA apply encoder delay ATM so here we fix it manually.
         * XMA delay is part if the bitstream and while theoretically it could be any
         *  value (and is honored by xmaencoder), basically it's always 512.
         *
         * Somehow also needs to skip 64 extra samples (looks like another FFmpeg bug
         * where XMA outputs half a subframe samples late, WMAPRO isn't affected),
         * which sometimes makes FFmpeg complain (=reads after end) but doesn't seem audible. */
        ffmpeg_set_skip_samples(data, start_skip+64);
    }
#endif
}

void xma_fix_raw_samples_hb(VGMSTREAM* vgmstream, STREAMFILE* sf_head, STREAMFILE* sf_body, off_t stream_offset, size_t stream_size, off_t chunk_offset, int fix_num_samples, int fix_loop_samples) {
    int channels_per_stream = xma_get_channels_per_stream(sf_head, chunk_offset, vgmstream->channels);
    xma_fix_raw_samples_ch(vgmstream, sf_body, stream_offset, stream_size, channels_per_stream, fix_num_samples, fix_loop_samples);
}

void xma_fix_raw_samples(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t stream_offset, size_t stream_size, off_t chunk_offset, int fix_num_samples, int fix_loop_samples) {
    int channels_per_stream = xma_get_channels_per_stream(sf, chunk_offset, vgmstream->channels);
    xma_fix_raw_samples_ch(vgmstream, sf, stream_offset, stream_size, channels_per_stream, fix_num_samples, fix_loop_samples);
}


/* ******************************************** */
/* HEADER PARSING                               */
/* ******************************************** */

/* Read values from a XMA1 RIFF "fmt" chunk (XMAWAVEFORMAT), starting from an offset *after* chunk type+size.
 * Useful as custom X360 headers commonly have it lurking inside. */
void xma1_parse_fmt_chunk(STREAMFILE* sf, off_t chunk_offset, int * channels, int* sample_rate, int* loop_flag, int32_t * loop_start_b, int32_t * loop_end_b, int32_t * loop_subframe, int be) {
    int16_t (*read_16bit)(off_t,STREAMFILE*) = be ? read_16bitBE : read_16bitLE;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = be ? read_32bitBE : read_32bitLE;
    int i, num_streams, total_channels = 0;

    if (read_16bit(chunk_offset+0x00,sf) != 0x165)
        return;

    num_streams = read_16bit(chunk_offset+0x08,sf);
    if(loop_flag)  *loop_flag = (uint8_t)read_8bit(chunk_offset+0xA,sf) > 0;

    /* sample rate and loop bit offsets are defined per stream, but the first is enough */
    if(sample_rate)   *sample_rate   = read_32bit(chunk_offset+0x10,sf);
    if(loop_start_b)  *loop_start_b  = read_32bit(chunk_offset+0x14,sf);
    if(loop_end_b)    *loop_end_b    = read_32bit(chunk_offset+0x18,sf);
    if(loop_subframe) *loop_subframe = (uint8_t)read_8bit(chunk_offset+0x1C,sf);

    /* channels is the sum of all streams */
    for (i = 0; i < num_streams; i++) {
        total_channels += read_8bit(chunk_offset+0x0C+0x14*i+0x11,sf);
    }
    if(channels)      *channels = total_channels;
}

/* Read values from a 'new' XMA2 RIFF "fmt" chunk (XMA2WAVEFORMATEX), starting from an offset *after* chunk type+size.
 * Useful as custom X360 headers commonly have it lurking inside. Only parses the extra data (before is a normal WAVEFORMATEX). */
void xma2_parse_fmt_chunk_extra(STREAMFILE* sf, off_t chunk_offset, int* out_loop_flag, int32_t* out_num_samples, int32_t* out_loop_start_sample, int32_t* out_loop_end_sample, int be) {
    int16_t (*read_16bit)(off_t,STREAMFILE*) = be ? read_16bitBE : read_16bitLE;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = be ? read_32bitBE : read_32bitLE;
    int num_samples, loop_start_sample, loop_end_sample, loop_flag;

    if (read_16bit(chunk_offset+0x00,sf) != 0x166)
        return;
    if (read_16bit(chunk_offset+0x10,sf) < 0x22)
        return; /* expected extra data size */

    num_samples       = read_32bit(chunk_offset+0x18,sf);
    loop_start_sample = read_32bit(chunk_offset+0x28,sf);
    loop_end_sample   = loop_start_sample + read_32bit(chunk_offset+0x2C,sf);
    loop_flag         = (uint8_t)read_8bit(chunk_offset+0x30,sf) != 0;
    /* may need loop end +1, though some header doesn't need it (ex.- Sonic and Sega All Stars Racing  .str) */

    /* flag rarely set, use loop_end as marker */
    if (!loop_flag) {
        loop_flag = loop_end_sample > 0;

        /* some XMA incorrectly do full loops for every song/jingle [Shadows of the Damned (X360)] */
        if ((loop_start_sample + 128 - 512) == 0 && (loop_end_sample + 128 - 512) + 256 >= (num_samples + 128 - 512)) {
            VGM_LOG("XMA2 PARSE: disabling full loop\n");
            loop_flag = 0;
        }
    }

    /* samples are "raw" values, must be fixed externally (see xma_fix_raw_samples) */
    if(out_num_samples)        *out_num_samples       = num_samples;
    if(out_loop_start_sample)  *out_loop_start_sample = loop_start_sample;
    if(out_loop_end_sample)    *out_loop_end_sample   = loop_end_sample;
    if(out_loop_flag)          *out_loop_flag         = loop_flag;

    /* play_begin+end = pcm_samples in original sample rate (not usable as file may be resampled) */
    /* int32_t play_begin_sample = read_32bit(xma->chunk_offset+0x20,sf); */
    /* int32_t play_end_sample = play_begin_sample + read_32bit(xma->chunk_offset+0x24,sf); */
}

/* Read values from an 'old' XMA2 RIFF "XMA2" chunk (XMA2WAVEFORMAT), starting from an offset *after* chunk type+size.
 * Useful as custom X360 headers commonly have it lurking inside. */
void xma2_parse_xma2_chunk(STREAMFILE* sf, off_t chunk_offset, int* out_channels, int* out_sample_rate, int* out_loop_flag, int32_t* out_num_samples, int32_t* out_loop_start_sample, int32_t* out_loop_end_sample) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = read_32bitBE; /* XMA2WAVEFORMAT is always big endian */
    int i, xma2_chunk_version, num_streams;
    int channels, sample_rate, loop_flag, num_samples, loop_start_sample, loop_end_sample;
    off_t offset;

    xma2_chunk_version = read_u8(chunk_offset+0x00,sf);
    num_streams        = read_u8(chunk_offset+0x01,sf);
    loop_start_sample = read_32bit(chunk_offset+0x04,sf);
    loop_end_sample   = read_32bit(chunk_offset+0x08,sf);
    loop_flag         = read_u8(chunk_offset+0x03,sf) > 0 || loop_end_sample; /* rarely not set, encoder default */
    sample_rate       = read_32bit(chunk_offset+0x0c,sf);
    /* may need loop end +1 */

    offset = xma2_chunk_version == 3 ? 0x14 : 0x1C;
    num_samples = read_32bit(chunk_offset+offset+0x00,sf);
    //pcm_samples = read_32bitBE(chunk_offset+offset+0x04,sf) /* in original sample rate (not usable as file may be resampled) */

    offset = xma2_chunk_version == 3 ? 0x20 : 0x28;
    channels = 0; /* channels is the sum of all streams */
    for (i = 0; i < num_streams; i++) {
        channels += read_8bit(chunk_offset+offset+i*0x04,sf);
    }

    /* samples are "raw" values, must be fixed externally (see xma_fix_raw_samples) */
    if(out_channels)           *out_channels          = channels;
    if(out_sample_rate)        *out_sample_rate       = sample_rate;
    if(out_num_samples)        *out_num_samples       = num_samples;
    if(out_loop_start_sample)  *out_loop_start_sample = loop_start_sample;
    if(out_loop_end_sample)    *out_loop_end_sample   = loop_end_sample;
    if(out_loop_flag)          *out_loop_flag         = loop_flag;
}

/* ******************************************** */
/* OTHER STUFF                                  */
/* ******************************************** */

size_t atrac3_bytes_to_samples(size_t bytes, int full_block_align) {
    if (full_block_align <= 0) return 0;
    /* ATRAC3 expects full block align since as is can mix joint stereo with mono blocks;
     * so (full_block_align / channels) DOESN'T give the size of a single channel (uncommon in ATRAC3 though) */
    return (bytes / full_block_align) * 1024;
}

size_t atrac3plus_bytes_to_samples(size_t bytes, int full_block_align) {
    if (full_block_align <= 0) return 0;
    /* ATRAC3plus expects full block align since as is can mix joint stereo with mono blocks;
     * so (full_block_align / channels) DOESN'T give the size of a single channel (common in ATRAC3plus) */
    return (bytes / full_block_align) * 2048;
}

size_t ac3_bytes_to_samples(size_t bytes, int full_block_align, int channels) {
    if (full_block_align <= 0) return 0;
    return (bytes / full_block_align) * 256 * channels;
}


size_t aac_get_samples(STREAMFILE* sf, off_t start_offset, size_t bytes) {
    const int samples_per_frame = 1024; /* theoretically 960 exists in .MP4 so may need a flag */
    int frames = 0;
    off_t offset = start_offset;
    off_t max_offset = start_offset + bytes;

    if (!sf)
        return 0;

    if (max_offset > get_streamfile_size(sf))
        max_offset = get_streamfile_size(sf);

    /* AAC sometimes comes with an "ADIF" header right before data but probably not in games,
     * while standard raw frame headers are called "ADTS" and are similar to MPEG's:
     * (see https://wiki.multimedia.cx/index.php/ADTS) */

    /* AAC uses VBR so must read all frames */
    while (offset < max_offset) {
        uint16_t frame_sync = read_u16be(offset+0x00, sf);
        uint32_t frame_size = read_u32be(offset+0x02, sf);

        frame_sync = (frame_sync >> 4) & 0x0FFF; /* 12b */
        frame_size = (frame_size >> 5) & 0x1FFF; /* 13b */

        if (frame_sync != 0xFFF)
            break;
        if (frame_size <= 0x08)
            break;

        //;VGM_LOG("AAC: %lx, %x\n", offset, frame_size);
        frames++;
        offset += frame_size;
    }

    return frames * samples_per_frame;
}


/* variable-sized var reader */
static int mpc_get_size(uint8_t* header, int header_size, int pos, int32_t* p_size) {
    uint8_t tmp;
    int32_t size = 0;

    do {
        if (pos >= header_size)
            return pos;

        tmp = header[pos];
        size = (size << 7) | (tmp & 0x7F);
        pos++;
    }
    while((tmp & 0x80));

    *p_size = size;
    return pos;
}

int mpc_get_samples(STREAMFILE* sf, off_t offset, int32_t* p_samples, int32_t* p_delay) {
    uint8_t header[0x20];
    int pos, size;
    int32_t samples = 0, delay = 0;
    
    if (read_streamfile(header, offset, sizeof(header), sf) != sizeof(header))
        goto fail;;

    if ((get_u32be(header) & 0xFFFFFF0F) == get_id32be("MP+\x07")) {
        samples = get_u32le(header + 0x04) * 1152; /* total frames */
        delay = 481; /* MPC_DECODER_SYNTH_DELAY */

        samples -= delay;
        /* in theory one header field can contain actual delay, not observed */
    }
    else if (get_u32be(header) == get_id32be("MPCK")) {
        /* V8 header is made if mini chunks (16b type, 8b size including type+size):
         * - SH: stream header
         * - RG: replay gain
         * - EI: encoder info
         * - SO: seek?
         * - ST: stream?
         * - AP: audio part start */
        if (get_u16be(header + 0x04) != 0x5348)
            goto fail;
        size = get_u8(header + 0x06);
        if (0x04 + size > sizeof(header))
            goto fail;
        if (get_u8(header + 0x0b) != 0x08)
            goto fail;
        /* SH chunk: */
        /* 0x00: CRC */
        /* 0x04: header version (8) */
        /* 0x05: samples (variable sized) */
        /* 0xNN: "beginning silence" (variable sized) */
        /* 0xNN: bitpacked channels/rate/etc */
        pos = mpc_get_size(header, sizeof(header), 0x0C, &samples);
        pos = mpc_get_size(header, sizeof(header), pos, &delay);

        samples -= delay; /* original delay, not SYNTH_DELAY */
        delay += 481; /* MPC_DECODER_SYNTH_DELAY */
        /* SYNTH_DELAY seems to be forced, but official code isn't very clear (known samples set delay to 0 but need SYNTH DELAY) */
    }
    else {
        goto fail;
    }

    if (p_samples) *p_samples = samples;
    if (p_delay) *p_delay = delay;

    return 1;
fail:
    return 0;
}


/* ******************************************** */
/* CUSTOM STREAMFILES                           */
/* ******************************************** */

STREAMFILE* setup_subfile_streamfile(STREAMFILE* sf, offv_t subfile_offset, size_t subfile_size, const char* extension) {
    STREAMFILE* new_sf = NULL;

    new_sf = open_wrap_streamfile(sf);
    new_sf = open_clamp_streamfile_f(new_sf, subfile_offset, subfile_size);
    if (extension) {
        new_sf = open_fakename_streamfile_f(new_sf, NULL, extension);
    }
    return new_sf;
}
