#include "coding.h"
#include "ffmpeg_decoder_utils.h"
#include <string.h>

#ifdef VGM_USE_FFMPEG


/**
 * Xiph Opus without Ogg layer and custom packet headers. This creates valid Ogg pages with single Opus packets.
 * Wwise opus looks encoded roughly like "opusenc --hard-cbr --framesize 40". The packets are Opus-compliant.
 *
 * Info, CRC and stuff:
 *   https://www.opus-codec.org/docs/
 *   https://tools.ietf.org/html/rfc7845.html
 *   https://github.com/hcs64/ww2ogg
 */

static size_t make_oggs_page(uint8_t * buf, int buf_size, size_t data_size, int page_sequence, int granule);
static size_t make_opus_header(uint8_t * buf, int buf_size, int channels, int skip, int sample_rate);
static size_t make_opus_comment(uint8_t * buf, int buf_size);
static uint32_t get_opus_samples_per_frame(const uint8_t * data, int Fs);


size_t ffmpeg_make_opus_header(uint8_t * buf, int buf_size, int channels, int skip, int sample_rate) {
    int buf_done = 0;
    size_t bytes;

    if (buf_size < 0x100) /* approx */
        goto fail;

    /* make header */
    bytes = make_opus_header(buf+buf_done + 0x1c,buf_size, channels, skip, sample_rate);
    make_oggs_page(buf+buf_done + 0x00,buf_size, bytes, 0, 0);
    buf_done += 0x1c + bytes;

    /* make comment */
    bytes = make_opus_comment(buf+buf_done + 0x1c,buf_size);
    make_oggs_page(buf+buf_done + 0x00,buf_size, bytes, 1, 0);
    buf_done += 0x1c + bytes;

    return buf_done;
fail:
    return 0;
}


int ffmpeg_custom_read_wwise_opus(ffmpeg_codec_data *data, uint8_t *buf, int buf_size) {
    uint8_t v_buf[0x8000]; /* intermediate buffer, could be simplified */
    int buf_done = 0;
    uint64_t real_offset = data->real_offset;
    uint64_t virtual_offset = data->virtual_offset - data->header_size;
    uint64_t virtual_base = data->virtual_base;


    if (data->config.sequence == 0)
        data->config.sequence = 2;

    /* read and transform Wwise Opus block into Ogg Opus block by making Ogg pages */
    while (buf_done < buf_size) {
        int bytes_to_copy, samples_per_frame;
        size_t extra_size = 0, gap_size = 0;
        size_t data_size = read_32bitBE(real_offset, data->streamfile);
        /* 0x00: data size, 0x04: ?, 0x08+: data */

        /* setup */
        extra_size = 0x1b + (int)(data_size / 0xFF + 1); /* OggS page: base size + lacing values */
        if (buf_done == 0) /* first read */
            gap_size = virtual_offset - virtual_base; /* might start a few bytes into the block */

        if (data_size + extra_size > 0x8000) {
            VGM_LOG("WW OPUS: total size bigger than buffer at %lx\n", (off_t)real_offset);
            return 0;
        }

        bytes_to_copy = data_size + extra_size - gap_size;
        if (bytes_to_copy > buf_size - buf_done)
            bytes_to_copy = buf_size - buf_done;

        /* transform */
        read_streamfile(v_buf + extra_size, real_offset + 0x08, data_size, data->streamfile);
        samples_per_frame = get_opus_samples_per_frame(v_buf + extra_size, 48000); /* fixed? */
        make_oggs_page(v_buf,0x8000, data_size, data->config.sequence, data->config.samples_done + samples_per_frame);
        memcpy(buf + buf_done, v_buf + gap_size, bytes_to_copy);

        /* move when block is fully done */
        if (data_size + extra_size == bytes_to_copy + gap_size) {
            real_offset += 0x04 + 0x04 + data_size;
            virtual_base += data_size + extra_size;
            data->config.sequence++;
            data->config.samples_done += samples_per_frame;
        }

        buf_done += bytes_to_copy;
    }

    data->real_offset = real_offset;
    data->virtual_base = virtual_base;
    return buf_size;
}

int64_t ffmpeg_custom_seek_wwise_opus(ffmpeg_codec_data *data, int64_t virtual_offset) {
    int64_t real_offset, virtual_base;
    int64_t current_virtual_offset = data->virtual_offset;

    /* find Wwise block start closest to offset; a 0x1E8 block expands to 0x1D + 0x1E0 (oggs + data) */

    if (virtual_offset > current_virtual_offset) { /* seek after current: start from current block */
        real_offset = data->real_offset;
        virtual_base = data->virtual_base;
    }
    else { /* seek before current: start from the beginning */
        real_offset = data->real_start;
        virtual_base = 0;
        data->config.sequence = 0;
        data->config.samples_done = 0;
    }


    /* find target block */
    while (virtual_base < virtual_offset) {
        size_t extra_size;
        size_t data_size = read_32bitBE(real_offset, data->streamfile);

        extra_size = 0x1b + (int)(data_size / 0xFF + 1); /* OggS page: base size + lacing values */

        /* stop if virtual_offset lands inside current block */
        if (data_size + extra_size > virtual_offset)
            break;

        real_offset += 0x04 + 0x04 + data_size;
        virtual_base += data_size + extra_size;
    }

    /* closest we can use for reads */
    data->real_offset = real_offset;
    data->virtual_base = virtual_base;

    return virtual_offset;
}

int64_t ffmpeg_custom_size_wwise_opus(ffmpeg_codec_data *data) {
    uint64_t real_offset = data->real_start;
    uint64_t real_size = data->real_size;
    uint64_t virtual_size = data->header_size;

    /* count all Wwise Opus blocks size + OggS page size */
    while (real_offset < real_size) {
        size_t extra_size;
        size_t data_size = read_32bitBE(real_offset, data->streamfile);
        /* 0x00: data size, 0x04: ? (not a sequence or CRC), 0x08+: data */

        extra_size = 0x1b + (int)(data_size / 0xFF + 1); /* OggS page: base size + lacing values */

        real_offset += 0x04 + 0x04 + data_size;
        virtual_size += extra_size + data_size;
    }


    return virtual_size;
}



/* ************************************************** */

/* from ww2ogg - from Tremor (lowmem) */
static uint32_t crc_lookup[256]={
  0x00000000,0x04c11db7,0x09823b6e,0x0d4326d9,  0x130476dc,0x17c56b6b,0x1a864db2,0x1e475005,
  0x2608edb8,0x22c9f00f,0x2f8ad6d6,0x2b4bcb61,  0x350c9b64,0x31cd86d3,0x3c8ea00a,0x384fbdbd,
  0x4c11db70,0x48d0c6c7,0x4593e01e,0x4152fda9,  0x5f15adac,0x5bd4b01b,0x569796c2,0x52568b75,
  0x6a1936c8,0x6ed82b7f,0x639b0da6,0x675a1011,  0x791d4014,0x7ddc5da3,0x709f7b7a,0x745e66cd,
  0x9823b6e0,0x9ce2ab57,0x91a18d8e,0x95609039,  0x8b27c03c,0x8fe6dd8b,0x82a5fb52,0x8664e6e5,
  0xbe2b5b58,0xbaea46ef,0xb7a96036,0xb3687d81,  0xad2f2d84,0xa9ee3033,0xa4ad16ea,0xa06c0b5d,
  0xd4326d90,0xd0f37027,0xddb056fe,0xd9714b49,  0xc7361b4c,0xc3f706fb,0xceb42022,0xca753d95,
  0xf23a8028,0xf6fb9d9f,0xfbb8bb46,0xff79a6f1,  0xe13ef6f4,0xe5ffeb43,0xe8bccd9a,0xec7dd02d,
  0x34867077,0x30476dc0,0x3d044b19,0x39c556ae,  0x278206ab,0x23431b1c,0x2e003dc5,0x2ac12072,
  0x128e9dcf,0x164f8078,0x1b0ca6a1,0x1fcdbb16,  0x018aeb13,0x054bf6a4,0x0808d07d,0x0cc9cdca,
  0x7897ab07,0x7c56b6b0,0x71159069,0x75d48dde,  0x6b93dddb,0x6f52c06c,0x6211e6b5,0x66d0fb02,
  0x5e9f46bf,0x5a5e5b08,0x571d7dd1,0x53dc6066,  0x4d9b3063,0x495a2dd4,0x44190b0d,0x40d816ba,
  0xaca5c697,0xa864db20,0xa527fdf9,0xa1e6e04e,  0xbfa1b04b,0xbb60adfc,0xb6238b25,0xb2e29692,
  0x8aad2b2f,0x8e6c3698,0x832f1041,0x87ee0df6,  0x99a95df3,0x9d684044,0x902b669d,0x94ea7b2a,
  0xe0b41de7,0xe4750050,0xe9362689,0xedf73b3e,  0xf3b06b3b,0xf771768c,0xfa325055,0xfef34de2,
  0xc6bcf05f,0xc27dede8,0xcf3ecb31,0xcbffd686,  0xd5b88683,0xd1799b34,0xdc3abded,0xd8fba05a,
  0x690ce0ee,0x6dcdfd59,0x608edb80,0x644fc637,  0x7a089632,0x7ec98b85,0x738aad5c,0x774bb0eb,
  0x4f040d56,0x4bc510e1,0x46863638,0x42472b8f,  0x5c007b8a,0x58c1663d,0x558240e4,0x51435d53,
  0x251d3b9e,0x21dc2629,0x2c9f00f0,0x285e1d47,  0x36194d42,0x32d850f5,0x3f9b762c,0x3b5a6b9b,
  0x0315d626,0x07d4cb91,0x0a97ed48,0x0e56f0ff,  0x1011a0fa,0x14d0bd4d,0x19939b94,0x1d528623,
  0xf12f560e,0xf5ee4bb9,0xf8ad6d60,0xfc6c70d7,  0xe22b20d2,0xe6ea3d65,0xeba91bbc,0xef68060b,
  0xd727bbb6,0xd3e6a601,0xdea580d8,0xda649d6f,  0xc423cd6a,0xc0e2d0dd,0xcda1f604,0xc960ebb3,
  0xbd3e8d7e,0xb9ff90c9,0xb4bcb610,0xb07daba7,  0xae3afba2,0xaafbe615,0xa7b8c0cc,0xa379dd7b,
  0x9b3660c6,0x9ff77d71,0x92b45ba8,0x9675461f,  0x8832161a,0x8cf30bad,0x81b02d74,0x857130c3,
  0x5d8a9099,0x594b8d2e,0x5408abf7,0x50c9b640,  0x4e8ee645,0x4a4ffbf2,0x470cdd2b,0x43cdc09c,
  0x7b827d21,0x7f436096,0x7200464f,0x76c15bf8,  0x68860bfd,0x6c47164a,0x61043093,0x65c52d24,
  0x119b4be9,0x155a565e,0x18197087,0x1cd86d30,  0x029f3d35,0x065e2082,0x0b1d065b,0x0fdc1bec,
  0x3793a651,0x3352bbe6,0x3e119d3f,0x3ad08088,  0x2497d08d,0x2056cd3a,0x2d15ebe3,0x29d4f654,
  0xc5a92679,0xc1683bce,0xcc2b1d17,0xc8ea00a0,  0xd6ad50a5,0xd26c4d12,0xdf2f6bcb,0xdbee767c,
  0xe3a1cbc1,0xe760d676,0xea23f0af,0xeee2ed18,  0xf0a5bd1d,0xf464a0aa,0xf9278673,0xfde69bc4,
  0x89b8fd09,0x8d79e0be,0x803ac667,0x84fbdbd0,  0x9abc8bd5,0x9e7d9662,0x933eb0bb,0x97ffad0c,
  0xafb010b1,0xab710d06,0xa6322bdf,0xa2f33668,  0xbcb4666d,0xb8757bda,0xb5365d03,0xb1f740b4
};

/* from ww2ogg */
static uint32_t get_oggs_checksum(uint8_t * data, int bytes) {
  uint32_t crc_reg=0;
  int i;

  for(i=0;i<bytes;++i)
      crc_reg=(crc_reg<<8)^crc_lookup[((crc_reg >> 24)&0xff)^data[i]];

  return crc_reg;
}

/* from opus_decoder.c */
static uint32_t get_opus_samples_per_frame(const uint8_t * data, int Fs) {
    int audiosize;
    if (data[0]&0x80)
    {
       audiosize = ((data[0]>>3)&0x3);
       audiosize = (Fs<<audiosize)/400;
    } else if ((data[0]&0x60) == 0x60)
    {
       audiosize = (data[0]&0x08) ? Fs/50 : Fs/100;
    } else {
       audiosize = ((data[0]>>3)&0x3);
       if (audiosize == 3)
          audiosize = Fs*60/1000;
       else
          audiosize = (Fs<<audiosize)/100;
    }
    return audiosize;
}


static size_t make_oggs_page(uint8_t * buf, int buf_size, size_t data_size, int page_sequence, int granule) {
    size_t page_done, lacing_done = 0;
    uint64_t absolute_granule = granule; /* seem wrong values matter for Opus (0, less than real samples, etc) */
    int header_type_flag = (page_sequence==0 ? 2 : 0);
    int stream_serial_number = 0x7667; /* 0 is legal, but should be specified */
    int checksum = 0;
    int segment_count;


    if (0x1b + (data_size/0xFF + 1) + data_size > buf_size) {
        VGM_LOG("WW OPUS: buffer can't hold OggS page\n");
        goto fail;
    }

    segment_count = (int)(data_size / 0xFF + 1);

    put_32bitBE(buf+0x00, 0x4F676753); /* capture pattern ("OggS") */
    put_8bit   (buf+0x04, 0); /* stream structure version, fixed */
    put_8bit   (buf+0x05, header_type_flag); /* bitflags (0: normal, continued = 1, first = 2, last = 4) */
    put_32bitLE(buf+0x06, (uint32_t)(absolute_granule >>  0 & 0xFFFFFFFF)); /* lower */
    put_32bitLE(buf+0x0A, (uint32_t)(absolute_granule >> 32 & 0xFFFFFFFF)); /* upper */
    put_32bitLE(buf+0x0E, stream_serial_number); /* for interleaved multi-streams */
    put_32bitLE(buf+0x12, page_sequence);
    put_32bitLE(buf+0x16, checksum); /* 0 for now, until all data is written */
    put_8bit   (buf+0x1A, segment_count); /* count of all lacing values */

    /* segment table: size N in "lacing values" (ex. 0x20E=0xFF+FF+10; 0xFF=0xFF+00) */
    page_done = 0x1B;
    while (lacing_done < data_size) {
        int bytes = data_size - lacing_done;
        if (bytes > 0xFF)
            bytes = 0xFF;

        put_8bit(buf+page_done, bytes);
        page_done++;
        lacing_done += bytes;

        if (lacing_done == data_size && bytes == 0xFF) {
            put_8bit(buf+page_done, 0x00);
            page_done++;
        }
    }

    /* data */
    //memcpy(buf+page_done, data_buf, data_size); /* data must be copied before this call */
    page_done += data_size;

    /* final checksum */
    checksum = get_oggs_checksum(buf, page_done);
    put_32bitLE(buf+0x16, checksum);

    return page_done;

fail:
    return 0;
}

static size_t make_opus_header(uint8_t * buf, int buf_size, int channels, int skip, int sample_rate) {
    size_t header_size = 0x13;
    int output_gain = 0;
    int channel_papping_family = 0;

    if (header_size > buf_size) {
        VGM_LOG("WW OPUS: buffer can't hold header\n");
        goto fail;
    }

    put_32bitBE(buf+0x00, 0x4F707573); /* header magic ("Opus") */
    put_32bitBE(buf+0x04, 0x48656164); /* header magic ("Head") */
    put_8bit   (buf+0x08, 1); /* version, fixed */
    put_8bit   (buf+0x09, channels);
    put_16bitLE(buf+0x0A, skip);
    put_32bitLE(buf+0x0c, sample_rate);
    put_16bitLE(buf+0x10, output_gain);
    put_8bit   (buf+0x12, channel_papping_family);


    return header_size;
fail:
    return 0;
}

static size_t make_opus_comment(uint8_t * buf, int buf_size) {
    size_t comment_size;
    int vendor_string_length, user_comment_0_length;
    char * vendor_string = "libopus 1.0.2";
    char * user_comment_0_string = "ENCODER=opusenc from opus-tools 0.1.6";
    vendor_string_length = strlen(vendor_string);
    user_comment_0_length = strlen(user_comment_0_string);

    comment_size = 0x14 + vendor_string_length + user_comment_0_length;

    if (comment_size > buf_size) {
        VGM_LOG("WW OPUS: buffer can't hold comment\n");
        goto fail;
    }

    put_32bitBE(buf+0x00, 0x4F707573); /* header magic ("Opus") */
    put_32bitBE(buf+0x04, 0x54616773); /* header magic ("Tags") */
    put_32bitLE(buf+0x08, vendor_string_length);
    memcpy(buf+0x0c, vendor_string, vendor_string_length);
    put_32bitLE(buf+0x0c + vendor_string_length+0x00, 1); /* user_comment_list_length */
    put_32bitLE(buf+0x0c + vendor_string_length+0x04, user_comment_0_length);
    memcpy(buf+0x0c + vendor_string_length+0x08, user_comment_0_string, user_comment_0_length);


    return comment_size;
fail:
    return 0;
}


#endif
