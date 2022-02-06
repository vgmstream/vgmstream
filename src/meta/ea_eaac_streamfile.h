#ifndef _EA_EAAC_STREAMFILE_H_
#define _EA_EAAC_STREAMFILE_H_
#include "../streamfile.h"
#include "ea_eaac_opus_streamfile.h"

#define XMA_FRAME_SIZE 0x800

typedef struct {
    /* config */
    int version;
    int codec;
    int streamed;
    int stream_number;
    int stream_count;
    off_t stream_offset;
    uint32_t stream_size;

    /* state */
    off_t logical_offset;   /* offset that corresponds to physical_offset */
    off_t physical_offset;  /* actual file offset */

    uint32_t block_flag;    /* current block flags */
    size_t block_size;      /* current block size */
    size_t skip_size;       /* size to skip from a block start to reach data start */
    size_t data_size;       /* logical size of the block */
    size_t extra_size;      /* extra padding/etc size of the block */

    size_t logical_size;
} eaac_io_data;


/* Reads skipping EA's block headers, so the resulting data is smaller or larger than physical data.
 * physical/logical_offset will be at the start of a block and only advance when a block is done */
static size_t eaac_io_read(STREAMFILE* sf, uint8_t *dest, off_t offset, size_t length, eaac_io_data* data) {
    size_t total_read = 0;

    /* ignore bad reads */
    if (offset < 0 || offset > data->logical_size) {
        return total_read;
    }

    /* previous offset: re-start as we can't map logical<>physical offsets
     * (kinda slow as it trashes buffers, but shouldn't happen often) */
    if (offset < data->logical_offset) {
        ;VGM_LOG("EAAC IO: restart offset=%lx + %x, po=%lx, lo=%lx\n", offset, length, data->physical_offset, data->logical_offset);
        data->physical_offset = data->stream_offset;
        data->logical_offset = 0x00;
        data->data_size = 0;
        data->extra_size = 0;
    }

    /* read blocks, one at a time */
    while (length > 0) {

        /* ignore EOF (implicitly handles block end flags) */
        if (data->logical_offset >= data->logical_size) {
            break;
        }

        /* process new block */
        if (data->data_size == 0) {
            data->block_flag = (uint8_t)read_8bit(data->physical_offset+0x00,sf);
            data->block_size = read_32bitBE(data->physical_offset+0x00,sf) & 0x00FFFFFF;

            /* ignore header block */
            if (data->version == 1 && data->block_flag == 0x48) {
                data->physical_offset += data->block_size;
                continue;
            }

            switch(data->codec) {
                case 0x03: { /* EA-XMA */
                    /* block format: 0x04=num-samples, (size*4 + N XMA packets) per stream (with 1/2ch XMA headers) */
                    int i;

                    data->skip_size = 0x04 + 0x04;
                    for (i = 0; i < data->stream_number; i++) {
                        data->skip_size += read_32bitBE(data->physical_offset+data->skip_size, sf) / 4;
                    }
                    data->data_size = read_32bitBE(data->physical_offset+data->skip_size, sf) / 4; /* why size*4...? */
                    data->skip_size += 0x04; /* skip mini header */
                    data->data_size -= 0x04; /* remove mini header */
                    if (data->data_size % XMA_FRAME_SIZE)
                        data->extra_size = XMA_FRAME_SIZE - (data->data_size % XMA_FRAME_SIZE);
                    break;
                }

                case 0x05: /* EALayer3 v1 */
                case 0x06: /* EALayer3 v2 "PCM" */
                case 0x07: /* EALayer3 v2 "Spike" */
                case 0x09: /* EASpeex */
                case 0x0b: /* EAMP3 */
                case 0x0c: /* EAOpus */
                    data->skip_size = 0x08;
                    data->data_size = data->block_size - data->skip_size;
                    break;

                case 0x0a: /* EATrax */
                    data->skip_size = 0x08;
                    data->data_size = read_32bitBE(data->physical_offset+0x04,sf); /* also block_size - 0x08 */
                    break;

                default:
                    return total_read;
            }
        }

        /* move to next block */
        if (offset >= data->logical_offset + data->data_size + data->extra_size) {
            data->physical_offset += data->block_size;
            data->logical_offset += data->data_size + data->extra_size;
            data->data_size = 0;
            data->extra_size = 0;
            continue;
        }

        /* read data */
        {
            size_t bytes_consumed, bytes_done, to_read;

            bytes_consumed = offset - data->logical_offset;

            switch(data->codec) {
                case 0x03: { /* EA-XMA */
                    if (bytes_consumed < data->data_size) { /* offset falls within actual data */
                        to_read = data->data_size - bytes_consumed;
                        if (to_read > length)
                            to_read = length;
                        bytes_done = read_streamfile(dest, data->physical_offset + data->skip_size + bytes_consumed, to_read, sf);
                    }
                    else { /* offset falls within logical padded data */
                        to_read = data->data_size + data->extra_size - bytes_consumed;
                        if (to_read > length)
                            to_read = length;
                        memset(dest, 0xFF, to_read); /* no real need though, padding is ignored */
                        bytes_done = to_read;
                    }
                    break;
                }

                default:
                    to_read = data->data_size - bytes_consumed;
                    if (to_read > length)
                        to_read = length;
                    bytes_done = read_streamfile(dest, data->physical_offset + data->skip_size + bytes_consumed, to_read, sf);
                    break;
            }

            total_read += bytes_done;
            dest += bytes_done;
            offset += bytes_done;
            length -= bytes_done;

            if (bytes_done != to_read || bytes_done == 0) {
                break; /* error/EOF */
            }
        }
    }

    return total_read;
}


static size_t eaac_io_size(STREAMFILE *streamfile, eaac_io_data* data) {
    off_t physical_offset, max_physical_offset;
    size_t logical_size = 0;

    if (data->logical_size)
        return data->logical_size;

    physical_offset = data->stream_offset;
    max_physical_offset = physical_offset + data->stream_size;

    /* get size of the logical stream */
    while (physical_offset < max_physical_offset) {
        uint32_t block_flag, block_size, data_size, skip_size;
        int i;

        block_flag = (uint8_t)read_8bit(physical_offset+0x00,streamfile);
        block_size = read_32bitBE(physical_offset+0x00,streamfile) & 0x00FFFFFF;

        if (block_size == 0)
            break;  /* bad data */

        if (data->version == 0 && block_flag != 0x00 && block_flag != 0x80)
            break; /* unknown block */

        if (data->version == 1 && block_flag == 0x48) {
            physical_offset += block_size;
            continue; /* skip header block */
        }
        if (data->version == 1 && block_flag == 0x45)
            break; /* stop on last block (always empty) */
        if (data->version == 1 && block_flag != 0x44)
            break; /* unknown block */

        switch(data->codec) {
            case 0x03: /* EA-XMA */
                skip_size = 0x04 + 0x04;
                for (i = 0; i < data->stream_number; i++) {
                    skip_size += read_32bitBE(physical_offset + skip_size, streamfile) / 4; /* why size*4...? */
                }
                data_size = read_32bitBE(physical_offset + skip_size, streamfile) / 4;
                skip_size += 0x04; /* skip mini header */
                data_size -= 0x04; /* remove mini header */
                if (data_size % XMA_FRAME_SIZE)
                    data_size += XMA_FRAME_SIZE - (data_size % XMA_FRAME_SIZE); /* extra padding */
                break;

            case 0x05: /* EALayer3 v1 */
            case 0x06: /* EALayer3 v2 "PCM" */
            case 0x07: /* EALayer3 v2 "Spike" */
            case 0x09: /* EASpeex */
            case 0x0b: /* EAMP3 */
            case 0x0c: /* EAOpus */
                data_size = block_size - 0x08;
                break;

            case 0x0a: /* EATrax */
                data_size = read_32bitBE(physical_offset+0x04,streamfile); /* also block_size - 0x08 */
                break;

            default:
                return 0;
        }

        physical_offset += block_size;
        logical_size += data_size;

        if (data->version == 0 && (!data->streamed || block_flag == 0x80))
            break; /* stop on last block */
    }

    /* logical size can be bigger in EA-XMA though */
    if (physical_offset > max_physical_offset) {
        VGM_LOG("EA EAAC: wrong size\n");
        return 0;
    }

    data->logical_size = logical_size;
    return data->logical_size;
}


/* Prepares custom IO for some blocked EAAudioCore formats, that need clean reads without block headers:
 * - EA-XMA: deflated XMA in multistreams (separate 1/2ch packets)
 * - EALayer3: MPEG granule 1 can go in the next block (in V2"P" mainly, others could use layout blocked_sns)
 * - EATrax: ATRAC9 frames can be split between blooks
 * - EAOpus: multiple Opus packets of frame size + Opus data per block
 */
static STREAMFILE* setup_eaac_audio_streamfile(STREAMFILE* sf, int version, int codec, int streamed, int stream_number, int stream_count, uint32_t stream_offset, uint32_t stream_size) {
    STREAMFILE *new_sf = NULL;
    eaac_io_data io_data = {0};

    if (!stream_size)
        stream_size = get_streamfile_size(sf) - stream_offset;

    io_data.version = version;
    io_data.codec = codec;
    io_data.streamed = streamed;
    io_data.stream_number = stream_number;
    io_data.stream_count = stream_count;
    io_data.stream_offset = stream_offset;
    io_data.physical_offset = stream_offset;
    io_data.stream_size = stream_size;
    io_data.logical_size = eaac_io_size(sf, &io_data); /* force init */

    /* setup subfile */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_io_streamfile_f(new_sf, &io_data, sizeof(eaac_io_data), eaac_io_read, eaac_io_size);
    new_sf = open_buffer_streamfile_f(new_sf, 0); /* EA-XMA and multichannel EALayer3 benefit from this */
    if (codec == 0x0c && stream_count > 1) /* multichannel opus */
        new_sf = open_io_eaac_opus_streamfile_f(new_sf, stream_number, stream_count);
    return new_sf;
}

#endif /* _EA_EAAC_STREAMFILE_H_ */
