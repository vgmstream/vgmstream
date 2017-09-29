#include "coding.h"
#include "ffmpeg_decoder_utils.h"

#ifdef VGM_USE_FFMPEG

#define EAXMA_XMA_MAX_PACKETS_PER_SNS_BLOCK 3 /* only seen up to 3 (Dante's Inferno) */
#define EAXMA_XMA_MAX_STREAMS_PER_SNS_BLOCK 4 /* XMA2 max is 8ch = 4 * 2ch */
#define EAXMA_XMA_PACKET_SIZE 0x800
#define EAXMA_XMA_BUFFER_SIZE (EAXMA_XMA_MAX_PACKETS_PER_SNS_BLOCK * EAXMA_XMA_MAX_STREAMS_PER_SNS_BLOCK * EAXMA_XMA_PACKET_SIZE)

/**
 * EA-XMA is XMA2 with padding removed (so a real 0x450 block would be padded to a virtual 0x800 block).
 * Each EA-XMA SNS block contains 1~3 packets per stream, and multistream uses fully separate streams
 * (no packet_skip set). We'll pad and reinterleave packets so it resembles standard XMA2.
 *
 * XMA2 data layout (XMA1 is the same but doesn't use blocks, they are only for seeking):
 * - frames (containing 1..4 subframes): decode into 128*4 samples
 * - packets: size 0x800, containing N frames (last frame can spill into next packet), must be padded
 * - blocks: fixed size, containing N packets (last packet's frames won't spill into next block)
 * - stream: N interleaved packets (1/2ch) for multichannel (Nch) audio. Interleave is not fixed:
 *   at file start/new block has one packet per stream, then must follow the "packet_skip" value
 *   in the XMA packet header to find its next packet (skiping packets from other streams).
 *   ex.: s1_p1 skip1, s2_p1 skip2, s1_p2 skip0 s1_p3 skip1, s2_p2 skip1, s1_p4...
 */

static int get_block_max_packets(int num_streams, off_t packets_offset, STREAMFILE * streamfile);


int ffmpeg_custom_read_eaxma(ffmpeg_codec_data *data, uint8_t *buf, int buf_size) {
    uint8_t v_buf[EAXMA_XMA_BUFFER_SIZE]; /* intermediate buffer, could be simplified */
    int buf_done = 0;
    uint64_t real_offset = data->real_offset;
    uint64_t virtual_offset = data->virtual_offset - data->header_size;
    uint64_t virtual_base = data->virtual_base;
    /* EA-XMA always uses late XMA2 streams (2ch + ... + 1/2ch) */
    int num_streams = (data->config.channels / 2) + (data->config.channels % 2 ? 1 : 0);


    /* read and transform SNS/EA-XMA blocks into XMA packets */
    while (buf_done < buf_size) {
        int s, p, bytes_to_copy, max_packets;
        size_t data_size = 0, gap_size = 0;
        size_t block_size = read_32bitBE(real_offset, data->streamfile);
        /* 0x04(4): decoded samples */
        off_t packets_offset = real_offset + 0x08;

        max_packets = get_block_max_packets(num_streams, packets_offset, data->streamfile);
        if (max_packets == 0) goto fail;

        if (max_packets * num_streams * EAXMA_XMA_PACKET_SIZE > EAXMA_XMA_BUFFER_SIZE) {
            VGM_LOG("EA XMA: block too big at %lx\n", (off_t)real_offset);
            goto fail;
        }

        /* data is divided into a sub-block per stream (N packets), can be smaller than block_size (= has padding)
         * copy XMA data re-interleaving for multichannel. To simplify some calcs fills the same number of packets
         * per stream and adjusts packet headers (see above for XMA2 multichannel layout). */
        //to-do this doesn't make correct blocks sizes (but blocks are not needed to decode)
        for (s = 0; s < num_streams; s++) {
            size_t packets_size;
            size_t packets_size4 = read_32bitBE(packets_offset, data->streamfile); /* size * 4, no idea */

            packets_size = (packets_size4 / 4) - 0x04;

            /* Re-interleave all packets in order, one per stream. If one stream has more packets than
             * others we add empty packets to keep the same number for all, avoiding packet_skip calcs */
            for (p = 0; p < max_packets; p++) {
                off_t packet_offset = packets_offset + 0x04 + p * EAXMA_XMA_PACKET_SIZE; /* can be off but will copy 0 */
                off_t v_buf_offset  = p * EAXMA_XMA_PACKET_SIZE * num_streams + s * EAXMA_XMA_PACKET_SIZE;
                size_t packet_to_do = packets_size - p * EAXMA_XMA_PACKET_SIZE;
                size_t extra_size = 0;
                uint32_t header;

                if (packets_size < p * EAXMA_XMA_PACKET_SIZE)
                    packet_to_do = 0; /* empty packet */
                else if (packet_to_do > EAXMA_XMA_PACKET_SIZE)
                    packet_to_do = EAXMA_XMA_PACKET_SIZE;

                /* padding will be full size if packet_to_do is 0 */
                if (packet_to_do < EAXMA_XMA_PACKET_SIZE)
                    extra_size = EAXMA_XMA_PACKET_SIZE - (packet_to_do % EAXMA_XMA_PACKET_SIZE);

                /* copy data (or fully pad if empty packet) */
                read_streamfile(v_buf + v_buf_offset, packet_offset, packet_to_do, data->streamfile);
                memset(v_buf + v_buf_offset + packet_to_do, 0xFF, extra_size); /* add padding, typically 0xFF */

                /* rewrite packet header to add packet skips for multichannel (EA XMA streams are fully separate and have none)
                 * header bits: 6=num_frames, 15=first_frame_bits_offset, 3=metadata, 8=packet_skip */
                if (packet_to_do == 0)
                    header = 0x3FFF800; /* new empty packet header (0 num_frames, first_frame_bits_offset set to max) */
                else
                    header = (uint32_t)read_32bitBE(packet_offset, data->streamfile);

                /* get base header + change packet_skip since we know interleave is always 1 packet per stream */
                header = (header & 0xFFFFFF00) | ((header & 0x000000FF) + num_streams - 1);
                put_32bitBE(v_buf + v_buf_offset, header);
            }

            packets_offset += (packets_size4 / 4);
        }

        if (buf_done == 0) /* first read */
            gap_size = virtual_offset - virtual_base; /* might start a few bytes into the XMA */

        data_size = max_packets * num_streams * EAXMA_XMA_PACKET_SIZE;

        bytes_to_copy = data_size - gap_size;
        if (bytes_to_copy > buf_size - buf_done)
            bytes_to_copy = buf_size - buf_done;

        /* pad + copy */
        memcpy(buf + buf_done, v_buf + gap_size, bytes_to_copy);
        buf_done += bytes_to_copy;

        /* move when block is fully done */
        if (data_size == bytes_to_copy + gap_size) {
            real_offset += (block_size & 0x00FFFFFF);
            virtual_base += data_size;
        }

        /* exit on last block just in case, though should reach file size */
        if (block_size & 0x80000000)
            break;
    }


    data->real_offset = real_offset;
    data->virtual_base = virtual_base;
    return buf_size;

fail:
    return 0;
}

int64_t ffmpeg_custom_seek_eaxma(ffmpeg_codec_data *data, int64_t virtual_offset) {
    int64_t real_offset, virtual_base;
    int64_t current_virtual_offset = data->virtual_offset;

    /* Find SNS block start closest to offset. ie. virtual_offset 0x1A10 could mean SNS blocks
     * of 0x456+0x820 padded to 0x800+0x1000 (base) + 0x210 (extra for reads), thus real_offset = 0xC76 */

    if (virtual_offset > current_virtual_offset) { /* seek after current: start from current block */
        real_offset = data->real_offset;
        virtual_base = data->virtual_base;
    }
    else { /* seek before current: start from the beginning */
        real_offset = data->real_start;
        virtual_base = 0;
    }


    /* find target block */
    while (virtual_base < virtual_offset) {
        size_t data_size, extra_size = 0;
        size_t block_size = read_32bitBE(real_offset, data->streamfile);

        data_size = (block_size & 0x00FFFFFF) - 0x0c;
        if (data_size % EAXMA_XMA_PACKET_SIZE)
            extra_size = EAXMA_XMA_PACKET_SIZE - (data_size % EAXMA_XMA_PACKET_SIZE);

        /* stop if virtual_offset lands inside current block */
        if (data_size + extra_size > virtual_offset)
            break;

        real_offset += (block_size & 0x00FFFFFF);
        virtual_base += data_size + extra_size;
    }

    /* closest we can use for reads */
    data->real_offset = real_offset;
    data->virtual_base = virtual_base;

    return virtual_offset;
}

int64_t ffmpeg_custom_size_eaxma(ffmpeg_codec_data *data) {

    uint64_t virtual_size = data->config.virtual_size;
    if (!virtual_size)
        return 0;

    return virtual_size + data->header_size;
}

/* needed to know in meta for fake RIFF */
size_t ffmpeg_get_eaxma_virtual_size(int channels, off_t real_offset, size_t real_size, STREAMFILE *streamFile) {
    size_t virtual_size = 0;
    size_t real_end_offset = real_offset + real_size;
    /* EA-XMA always uses late XMA2 streams (2ch + ... + 1/2ch) */
    int num_streams = (channels / 2) + (channels % 2 ? 1 : 0);


    /* count all SNS/EAXMA blocks size + padding size */
    while (real_offset < real_end_offset) {
        int max_packets;
        size_t block_size = read_32bitBE(real_offset + 0x00, streamFile);
        /* 0x04(4): decoded samples */
        off_t packets_offset = real_offset + 0x08;

        if ((block_size & 0xFF000000) && !(block_size & 0x80000000)) {
            VGM_LOG("EA-XMA: unknown flag found at %lx\n", (off_t)real_offset);
            goto fail;
        }

        max_packets = get_block_max_packets(num_streams, packets_offset, streamFile);
        if (max_packets == 0) goto fail;

        /* fixed data_size per block for multichannel, see reads */
        virtual_size += max_packets * num_streams * EAXMA_XMA_PACKET_SIZE;

        real_offset += (block_size & 0x00FFFFFF);

        /* exit on last block just in case, though should reach real_size */
        if (block_size & 0x80000000)
            break;
    }

    return virtual_size;

fail:
    return 0;
}

/* a block can have N streams each with a varying number of packets, get max */
static int get_block_max_packets(int num_streams, off_t packets_offset, STREAMFILE * streamfile) {
    int s;
    int max_packets = 0;

    for (s = 0; s < num_streams; s++) {
        size_t packets_size;
        size_t packets_size4 = read_32bitBE(packets_offset, streamfile); /* size * 4, no idea */
        int num_packets;

        if (packets_size4 == 0) {
            VGM_LOG("EA XMA: null packets in stream %i at %lx\n", s, (off_t)packets_offset);
            goto fail;
        }
        packets_size = (packets_size4 / 4) - 0x04;

        num_packets = (int)(packets_size / EAXMA_XMA_PACKET_SIZE) + 1;
        if (num_packets > max_packets)
            max_packets = num_packets;
    }

    return max_packets;

fail:
    return 0;
}

#endif
