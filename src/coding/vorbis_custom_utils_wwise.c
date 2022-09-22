#include "vorbis_custom_decoder.h"

#ifdef VGM_USE_VORBIS
#include "vorbis_bitreader.h"
#include <vorbis/codec.h>

#define WWISE_VORBIS_USE_PRECOMPILED_WVC 1 /* if enabled vgmstream weights ~150kb more but doesn't need external .wvc packets */
#if WWISE_VORBIS_USE_PRECOMPILED_WVC
#include "vorbis_custom_data_wwise.h"
#endif


/* **************************************************************************** */
/* DEFS                                                                         */
/* **************************************************************************** */

typedef struct {
    size_t header_size;
    size_t packet_size;
    int granulepos;

    int has_next;
    uint8_t inxt[0x01];
} wpacket_t;

static size_t build_header_identification(uint8_t* buf, size_t bufsize, vorbis_custom_config* cfg);
static size_t build_header_comment(uint8_t* buf, size_t bufsize);

static int read_packet(wpacket_t* wp, uint8_t* ibuf, size_t ibufsize, STREAMFILE* sf, off_t offset, vorbis_custom_codec_data* data, int is_setup);
static size_t rebuild_packet(uint8_t* obuf, size_t obufsize, wpacket_t* wp, STREAMFILE* sf, off_t offset, vorbis_custom_codec_data* data);
static size_t rebuild_setup(uint8_t* obuf, size_t obufsize, wpacket_t* wp, STREAMFILE* sf, off_t offset, vorbis_custom_codec_data* data);

static int ww2ogg_generate_vorbis_packet(bitstream_t* ow, bitstream_t* iw, wpacket_t* wp, vorbis_custom_codec_data* data);
static int ww2ogg_generate_vorbis_setup(bitstream_t* ow, bitstream_t* iw, vorbis_custom_codec_data* data, size_t packet_size, STREAMFILE* sf);

static int load_wvc(uint8_t* ibuf, size_t ibufsize, uint32_t codebook_id, wwise_setup_t setup_type, STREAMFILE* sf);
#if !(WWISE_VORBIS_USE_PRECOMPILED_WVC)
static int load_wvc_file(uint8_t* buf, size_t bufsize, uint32_t codebook_id, STREAMFILE* sf);
#endif
static int load_wvc_array(uint8_t* buf, size_t bufsize, uint32_t codebook_id, wwise_setup_t setup_type);


/* **************************************************************************** */
/* EXTERNAL API                                                                 */
/* **************************************************************************** */

/**
 * Wwise stores a reduced setup, and packets have mini headers with the size, and data packets
 * may reduced as well. The format evolved over time so there are many variations.
 * The Wwise implementation uses Tremor (fixed-point Vorbis) but shouldn't matter.
 *
 * Format reverse-engineered by hcs in ww2ogg (https://github.com/hcs64/ww2ogg).
 */
int vorbis_custom_setup_init_wwise(STREAMFILE* sf, off_t start_offset, vorbis_custom_codec_data* data) {
    wpacket_t wp = {0};
    int ok;


    if (data->config.setup_type == WWV_HEADER_TRIAD) {
        /* read 3 Wwise packets with triad (id/comment/setup), each with a Wwise header */
        off_t offset = start_offset;

        /* normal identificacion packet */
        ok = read_packet(&wp, data->buffer, data->buffer_size, sf, offset, data, 1);
        if (!ok) goto fail;
        data->op.bytes = wp.packet_size;
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail;
        offset += wp.header_size + wp.packet_size;

        /* normal comment packet */
        ok = read_packet(&wp, data->buffer, data->buffer_size, sf, offset, data, 1);
        if (!ok) goto fail;
        data->op.bytes = wp.packet_size;
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail;
        offset += wp.header_size + wp.packet_size;

        /* normal setup packet */
        ok = read_packet(&wp, data->buffer, data->buffer_size, sf, offset, data, 1);
        if (!ok) goto fail;
        data->op.bytes = wp.packet_size;
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail;
        offset += wp.header_size + wp.packet_size;
    }
    else {
        /* rebuild headers */

        /* new identificacion packet */
        data->op.bytes = build_header_identification(data->buffer, data->buffer_size, &data->config);
        if (!data->op.bytes) goto fail;
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse identification header */

        /* new comment packet */
        data->op.bytes = build_header_comment(data->buffer, data->buffer_size);
        if (!data->op.bytes) goto fail;
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) !=0 ) goto fail; /* parse comment header */

        /* rebuild setup packet */
        data->op.bytes = rebuild_setup(data->buffer, data->buffer_size, &wp, sf, start_offset, data);
        if (!data->op.bytes) goto fail;
        if (vorbis_synthesis_headerin(&data->vi, &data->vc, &data->op) != 0) goto fail; /* parse setup header */
    }

    return 1;

fail:
    return 0;
}


int vorbis_custom_parse_packet_wwise(VGMSTREAMCHANNEL* stream, vorbis_custom_codec_data* data) {
    wpacket_t wp = {0};

    /* reconstruct a Wwise packet, if needed; final bytes may be bigger than packet_size */
    data->op.bytes = rebuild_packet(data->buffer, data->buffer_size, &wp, stream->streamfile, stream->offset, data);
    stream->offset += wp.header_size + wp.packet_size;
    if (!data->op.bytes || data->op.bytes >= 0xFFFF) goto fail;

    data->op.granulepos = wp.granulepos;

    return 1;

fail:
    return 0;
}

/* **************************************************************************** */
/* INTERNAL HELPERS                                                             */
/* **************************************************************************** */

static int read_packet(wpacket_t* wp, uint8_t* ibuf, size_t ibufsize, STREAMFILE* sf, off_t offset, vorbis_custom_codec_data* data, int is_setup) {
    uint32_t (*get_u32)(const uint8_t*) = data->config.big_endian ? get_u32be : get_u32le;
    uint16_t (*get_u16)(const uint8_t*) = data->config.big_endian ? get_u16be : get_u16le;
    int32_t  (*get_s32)(const uint8_t*) = data->config.big_endian ? get_s32be : get_s32le;

    /* read header info (packet size doesn't include header size) */
    switch(data->config.header_type) {
        case WWV_TYPE_8:
            wp->header_size = 0x08;
            read_streamfile(ibuf, offset, wp->header_size, sf);
            wp->packet_size = get_u32(ibuf + 0x00);
            wp->granulepos  = get_s32(ibuf + 0x04);
            break;

        case WWV_TYPE_6:
            wp->header_size = 0x06;
            read_streamfile(ibuf, offset, wp->header_size, sf);
            wp->packet_size = get_u16(ibuf + 0x00);
            wp->granulepos  = get_s32(ibuf + 0x02);
            break;

        case WWV_TYPE_2:
            wp->header_size = 0x02;
            read_streamfile(ibuf, offset, wp->header_size, sf);
            wp->packet_size = get_u16(ibuf + 0x00);
            wp->granulepos  = 0; /* granule is an arbitrary unit so we could use offset instead; libvorbis has no need for it */
            break;

        default: /* ? */
            wp->header_size = 0;
            wp->packet_size = 0;
            wp->granulepos  = 0;
            break;
    }

    if (wp->header_size == 0 || wp->packet_size == 0)
        goto fail;

    /* read packet data */
    {
        size_t read_size = wp->packet_size;
        size_t read;

        /* mod packets need next packet's first byte (6 bits) except at EOF, so read now too */
        if (!is_setup && data->config.packet_type == WWV_MODIFIED) {
            read_size += wp->header_size + 0x01;
        }

        if (!wp->header_size || read_size > ibufsize)
            goto fail;

        read = read_streamfile(ibuf, offset + wp->header_size, read_size, sf);
        if (read < wp->packet_size) {
            VGM_LOG("Wwise Vorbis: truncated packet\n");
            goto fail;
        }

        if (!is_setup && data->config.packet_type == WWV_MODIFIED && read == read_size) {
            wp->has_next = 1;
            wp->inxt[0] = ibuf[wp->packet_size + wp->header_size];
        }
        else {
            wp->has_next = 0;
        }
    }

    return 1;
fail:
    return 0;
}


/* Transforms a Wwise data packet into a real Vorbis one (depending on config) */
static size_t rebuild_packet(uint8_t* obuf, size_t obufsize, wpacket_t* wp, STREAMFILE* sf, off_t offset, vorbis_custom_codec_data* data) {
    bitstream_t ow, iw;
    int ok;
    uint8_t ibuf[0x8000]; /* arbitrary max */
    size_t ibufsize = sizeof(ibuf);

    if (obufsize < ibufsize) /* arbitrary min */
        goto fail;

    ok = read_packet(wp, ibuf, ibufsize, sf, offset, data, 0);
    if (!ok) goto fail;

    init_bitstream(&ow, obuf, obufsize);
    init_bitstream(&iw, ibuf, ibufsize);

    ok = ww2ogg_generate_vorbis_packet(&ow, &iw, wp, data);
    if (!ok) goto fail;

    if (ow.b_off % 8 != 0) {
        //VGM_LOG("Wwise Vorbis: didn't write exactly audio packet: 0x%lx + %li bits\n", ow.b_off / 8, ow.b_off % 8);
        goto fail;
    }


    return ow.b_off / 8;
fail:
    return 0;
}

/* Transforms a Wwise setup packet into a real Vorbis one (depending on config). */
static size_t rebuild_setup(uint8_t* obuf, size_t obufsize, wpacket_t* wp, STREAMFILE* sf, off_t offset, vorbis_custom_codec_data* data) {
    bitstream_t ow, iw;
    int ok;
    uint8_t ibuf[0x8000]; /* arbitrary max */
    size_t ibufsize = sizeof(ibuf);

    if (obufsize < ibufsize) /* arbitrary min */
        goto fail;

    ok = read_packet(wp, ibuf, ibufsize, sf, offset, data, 1);
    if (!ok) goto fail;

    init_bitstream(&ow, obuf, obufsize);
    init_bitstream(&iw, ibuf, ibufsize);

    ok = ww2ogg_generate_vorbis_setup(&ow,&iw, data, wp->packet_size, sf);
    if (!ok) goto fail;

    if (ow.b_off % 8 != 0) {
        //VGM_LOG("Wwise Vorbis: didn't write exactly setup packet: 0x%lx + %li bits\n", ow.b_off / 8, ow.b_off % 8);
        goto fail;
    }


    return ow.b_off / 8;
fail:
    return 0;
}

static size_t build_header_identification(uint8_t* buf, size_t bufsize, vorbis_custom_config* cfg) {
    size_t bytes = 0x1e;
    uint8_t blocksizes;

    if (bytes > bufsize) return 0;

    blocksizes = (cfg->blocksize_0_exp << 4) | (cfg->blocksize_1_exp);

    put_8bit   (buf+0x00, 0x01);            /* packet_type (id) */
    memcpy     (buf+0x01, "vorbis", 6);     /* id */
    put_32bitLE(buf+0x07, 0x00);            /* vorbis_version (fixed) */
    put_8bit   (buf+0x0b, cfg->channels);   /* audio_channels */
    put_32bitLE(buf+0x0c, cfg->sample_rate);/* audio_sample_rate */
    put_32bitLE(buf+0x10, 0x00);            /* bitrate_maximum (optional hint) */
    put_32bitLE(buf+0x14, 0x00);            /* bitrate_nominal (optional hint) */
    put_32bitLE(buf+0x18, 0x00);            /* bitrate_minimum (optional hint) */
    put_8bit   (buf+0x1c, blocksizes);      /* blocksize_0 + blocksize_1 nibbles */
    put_8bit   (buf+0x1d, 0x01);            /* framing_flag (fixed) */

    return bytes;
}

static size_t build_header_comment(uint8_t* buf, size_t bufsize) {
    size_t bytes = 0x19;

    if (bytes > bufsize) return 0;

    put_8bit   (buf+0x00, 0x03);            /* packet_type (comments) */
    memcpy     (buf+0x01, "vorbis", 6);     /* id */
    put_32bitLE(buf+0x07, 0x09);            /* vendor_length */
    memcpy     (buf+0x0b, "vgmstream", 9);  /* vendor_string */
    put_32bitLE(buf+0x14, 0x00);            /* user_comment_list_length */
    put_8bit   (buf+0x18, 0x01);            /* framing_flag (fixed) */

    return bytes;
}


/* copy packet bytes, where input/output bufs may not be byte-aligned (so no memcpy) */
static int copy_bytes(bitstream_t* ob, bitstream_t* ib, uint32_t bytes) {
    int i;

#if 0
    /* in theory this would be faster, but not clear results; maybe default is just optimized by compiler */
    for (i = 0; i < bytes / 4; i++) {
        uint32_t c = 0;

        rv_bits(ib, 32, &c);
        wv_bits(ob, 32,  c);
    }
    for (i = 0; i < bytes % 4; i++) {
        uint32_t c = 0;

        rv_bits(ib,  8, &c);
        wv_bits(ob,  8,  c);
    }
#endif

#if 0
    /* output bits are never(?) byte aligned but input always is, yet this doesn't seem any faster */
    if (ib->b_off % 8 == 0) {
        int iw_pos = ib->b_off / 8;

        for (i = 0; i < bytes; i++, iw_pos++) {
            uint32_t c = ib->buf[iw_pos];

          //rv_bits(ib,  8, &c);
            wv_bits(ob,  8,  c);
        }

        ib->b_off += bytes * 8;
        return 1;
    }
#endif

    for (i = 0; i < bytes; i++) {
        uint32_t c = 0;

        rv_bits(ib,  8, &c);
        wv_bits(ob,  8,  c);
    }

    return 1;
}

/* **************************************************************************** */
/* INTERNAL WW2OGG STUFF                                                        */
/* **************************************************************************** */
/* The following code was mostly and manually converted from hcs's ww2ogg (https://github.com/hcs64/ww2ogg).
 * Could be simplified but roughly tries to preserve the structure for comparison.
 *
 * Some validations are ommited (ex. read/write), as incorrect data should be rejected by libvorbis.
 * To avoid GCC complaining all values are init to 0, and some that do need it are init again, for clarity.
 * Reads/writes unsigned ints as most are bit values less than 32 and with no sign meaning.
 */

/* Copy packet as-is or rebuild to standard Vorbis packet if mod_packets is used.
 * (ref: https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-720004.3) */
static int ww2ogg_generate_vorbis_packet(bitstream_t* ow, bitstream_t* iw, wpacket_t* wp, vorbis_custom_codec_data* data) {

    /* this may happen in the first packet; maybe it's for the encoder delay but doesn't seem to affect libvorbis */
    //VGM_ASSERT(granule < 0, "Wwise Vorbis: negative granule %i @ 0x%lx\n", granule, offset);


    if (data->config.packet_type == WWV_MODIFIED) {
        /* rebuild first bits of packet type and window info (for the i-MDCT) */
        uint32_t packet_type = 0, mode_number = 0, remainder = 0;

        /* audio packet type */
        packet_type = 0;
        wv_bits(ow,  1, packet_type);

        /* collect this packet mode from the first byte */
        rv_bits(iw,  data->mode_bits,&mode_number); /* max 6b */
        wv_bits(ow,  data->mode_bits, mode_number);

        rv_bits(iw,  8-data->mode_bits,&remainder);

        /* adjust window info */
        if (data->mode_blockflag[mode_number]) {
            /* long window: peek at next frame to find flags */
            uint32_t next_blockflag = 0, prev_window_type = 0, next_window_type = 0;

            if (wp->has_next) {
                /* get next first byte to read next_mode_number */
                uint32_t next_mode_number;
                bitstream_t nw;

                init_bitstream(&nw, wp->inxt, sizeof(wp->inxt));

                rv_bits(&nw,  data->mode_bits,&next_mode_number); /* max 6b */

                next_blockflag = data->mode_blockflag[next_mode_number];
            }
            else {
                /* EOF (probably doesn't matter) */
                next_blockflag = 0;
            }

            prev_window_type = data->prev_blockflag;
            wv_bits(ow,  1, prev_window_type);

            next_window_type = next_blockflag;
            wv_bits(ow,  1, next_window_type);
        }

        data->prev_blockflag = data->mode_blockflag[mode_number]; /* save for next packet */

        wv_bits(ow,  8-data->mode_bits, remainder);

        /* rest of the packet (input/output bytes aren't byte aligned here, so no memcpy) */
        copy_bytes(ow, iw, wp->packet_size - 1);

        /* remove trailing garbage bits (probably unneeded) */
        if (ow->b_off % 8 != 0) {
            uint32_t padding = 0;
            int padding_bits = 8 - (ow->b_off % 8);

            wv_bits(ow,  padding_bits,  padding);
        }
    }
    else {
        /* normal packets */

        /* can directly copy (much, much faster), but least common case vs the above... */
        memcpy(ow->buf + ow->b_off / 8, iw->buf + iw->b_off / 8, wp->packet_size);
        ow->b_off += wp->packet_size * 8;
        iw->b_off += wp->packet_size * 8;
    }


    return 1;
//fail:
//    return 0;
}

/*******************************************************************************/

/* fixed-point ilog from Xiph's Tremor */
static int ww2ogg_tremor_ilog(unsigned int v) {
    int ret=0;
    while(v){
        ret++;
        v>>=1;
    }
    return(ret);
}

/* quantvals-something from Xiph's Tremor */
static unsigned int ww2ogg_tremor_book_maptype1_quantvals(unsigned int entries, unsigned int dimensions) {
    /* get us a starting hint, we'll polish it below */
    int bits=ww2ogg_tremor_ilog(entries);
    int vals=entries>>((bits-1)*(dimensions-1)/dimensions);

    while(1){
        unsigned long acc=1;
        unsigned long acc1=1;
        unsigned int i;
        for(i=0;i<dimensions;i++){
            acc*=vals;
            acc1*=vals+1;
        }
        if(acc<=entries && acc1>entries){
            return(vals);
        }else{
            if(acc>entries){
                vals--;
            }else{
                vals++;
            }
        }
    }
}


/* copies Vorbis codebooks (untouched, but size uncertain) */
static int ww2ogg_codebook_library_copy(bitstream_t* ow, bitstream_t* iw) {
    int i;
    uint32_t id = 0, dimensions = 0, entries = 0;
    uint32_t ordered = 0, lookup_type = 0;

    rv_bits(iw, 24,&id);
    wv_bits(ow, 24, id);
    rv_bits(iw, 16,&dimensions);
    wv_bits(ow, 16, dimensions);
    rv_bits(iw, 24,&entries);
    wv_bits(ow, 24, entries);

    if (0x564342 != id) { /* "VCB" */
        VGM_LOG("Wwise Vorbis: invalid codebook identifier\n");
        goto fail;
    }

    /* codeword lengths */
    rv_bits(iw,  1,&ordered);
    wv_bits(ow,  1, ordered);
    if (ordered) {
        uint32_t initial_length = 0, current_entry = 0;

        rv_bits(iw,  5,&initial_length);
        wv_bits(ow,  5, initial_length);

        current_entry = 0;
        while (current_entry < entries) {
            uint32_t number = 0;
            int numberv_bits = ww2ogg_tremor_ilog(entries-current_entry);

            rv_bits(iw, numberv_bits,&number);
            wv_bits(ow, numberv_bits, number);
            current_entry += number;
        }
        if (current_entry > entries) {
            VGM_LOG("Wwise Vorbis: current_entry out of range\n");
            goto fail;
        }
    }
    else {
        uint32_t sparse = 0;

        rv_bits(iw,  1,&sparse);
        wv_bits(ow,  1, sparse);

        for (i = 0; i < entries; i++) {
            uint32_t present_bool = 0;

            present_bool = 1;
            if (sparse) {
                uint32_t present = 0;

                rv_bits(iw,  1,&present);
                wv_bits(ow,  1, present);

                present_bool = (0 != present);
            }

            if (present_bool) {
                uint32_t codeword_length = 0;

                rv_bits(iw,  5,&codeword_length);
                wv_bits(ow,  5, codeword_length);
            }
        }
    }


    /* lookup table */
    rv_bits(iw,  4,&lookup_type);
    wv_bits(ow,  4, lookup_type);

    if (0 == lookup_type) {
        //VGM_LOG("Wwise Vorbis: no lookup table\n");
    }
    else if (1 == lookup_type) {
        //VGM_LOG("Wwise Vorbis: lookup type 1\n");
        uint32_t quantvals = 0, min = 0, max = 0;
        uint32_t value_length = 0, sequence_flag = 0;

        rv_bits(iw, 32,&min);
        wv_bits(ow, 32, min);
        rv_bits(iw, 32,&max);
        wv_bits(ow, 32, max);
        rv_bits(iw,  4,&value_length);
        wv_bits(ow,  4, value_length);
        rv_bits(iw,  1,&sequence_flag);
        wv_bits(ow,  1, sequence_flag);

        quantvals = ww2ogg_tremor_book_maptype1_quantvals(entries, dimensions);
        for (i = 0; i < quantvals; i++) {
            uint32_t val = 0, val_bits = 0;
            val_bits = value_length+1;

            rv_bits(iw, val_bits,&val);
            wv_bits(ow, val_bits, val);
        }
    }
    else if (2 == lookup_type) {
        VGM_LOG("Wwise Vorbis: didn't expect lookup type 2\n");
        goto fail;
    }
    else {
        VGM_LOG("Wwise Vorbis: invalid lookup type\n");
        goto fail;
    }

    return 1;
fail:
    return 0;
}

/* rebuilds a Wwise codebook into a Vorbis codebook */
static int ww2ogg_codebook_library_rebuild(bitstream_t* ow, bitstream_t* iw, size_t cb_size, STREAMFILE* sf) {
    int i;
    uint32_t id = 0, dimensions = 0, entries = 0;
    uint32_t ordered = 0, lookup_type = 0;

    id = 0x564342; /* "VCB" */

    wv_bits(ow, 24, id);
    rv_bits(iw,  4,&dimensions);
    wv_bits(ow, 16, dimensions); /* 4 to 16 */
    rv_bits(iw, 14,&entries);
    wv_bits(ow, 24, entries); /* 14 to 24*/

    /* codeword lengths */
    rv_bits(iw,  1,&ordered);
    wv_bits(ow,  1, ordered);
    if (ordered) {
        uint32_t initial_length = 0, current_entry = 0;

        rv_bits(iw,  5,&initial_length);
        wv_bits(ow,  5, initial_length);

        current_entry = 0;
        while (current_entry < entries) {
            uint32_t number = 0;
            int numberv_bits = ww2ogg_tremor_ilog(entries-current_entry);

            rv_bits(iw, numberv_bits,&number);
            wv_bits(ow, numberv_bits, number);
            current_entry += number;
        }
        if (current_entry > entries) {
            VGM_LOG("Wwise Vorbis: current_entry out of range\n");
            goto fail;
        }
    }
    else {
        uint32_t codeword_length_length = 0, sparse = 0;

        rv_bits(iw,  3,&codeword_length_length);
        rv_bits(iw,  1,&sparse);
        wv_bits(ow,  1, sparse);

        if (0 == codeword_length_length || 5 < codeword_length_length) {
            VGM_LOG("Wwise Vorbis: nonsense codeword length\n");
            goto fail;
        }

        for (i = 0; i < entries; i++) {
            uint32_t present_bool = 0;

            present_bool = 1;
            if (sparse) {
                uint32_t present = 0;

                rv_bits(iw,  1,&present);
                wv_bits(ow,  1, present);

                present_bool = (0 != present);
            }

            if (present_bool) {
                uint32_t codeword_length = 0;

                rv_bits(iw,  codeword_length_length,&codeword_length);
                wv_bits(ow,  5, codeword_length); /* max 7 (3b) to 5 */
            }
        }
    }


    /* lookup table */
    rv_bits(iw,  1,&lookup_type);
    wv_bits(ow,  4, lookup_type); /* 1 to 4 */

    if (0 == lookup_type) {
        //VGM_LOG("Wwise Vorbis: no lookup table\n");
    }
    else if (1 == lookup_type) {
        //VGM_LOG("Wwise Vorbis: lookup type 1\n");
        uint32_t quantvals = 0, min = 0, max = 0;
        uint32_t value_length = 0, sequence_flag = 0;

        rv_bits(iw, 32,&min);
        wv_bits(ow, 32, min);
        rv_bits(iw, 32,&max);
        wv_bits(ow, 32, max);
        rv_bits(iw,  4,&value_length);
        wv_bits(ow,  4, value_length);
        rv_bits(iw,  1,&sequence_flag);
        wv_bits(ow,  1, sequence_flag);

        quantvals = ww2ogg_tremor_book_maptype1_quantvals(entries, dimensions);
        for (i = 0; i < quantvals; i++) {
            uint32_t val = 0, val_bits = 0;
            val_bits = value_length+1;

            rv_bits(iw, val_bits,&val);
            wv_bits(ow, val_bits, val);
        }
    }
    else if (2 == lookup_type) {
        VGM_LOG("Wwise Vorbis: didn't expect lookup type 2\n");
        goto fail;
    }
    else {
        VGM_LOG("Wwise Vorbis: invalid lookup type\n");
        goto fail;
    }


    /* check that we used exactly all bytes */
    /* note: if all bits are used in the last byte there will be one extra 0 byte */
    if ( 0 != cb_size && iw->b_off/8+1 != cb_size ) {
        //VGM_LOG("Wwise Vorbis: codebook size mistach (expected 0x%x, wrote 0x%lx)\n", cb_size, iw->b_off/8+1);
        goto fail;
    }

    return 1;
fail:
    return 0;
}

/* rebuilds an external Wwise codebook referenced by id to a Vorbis codebook */
static int ww2ogg_codebook_library_rebuild_by_id(bitstream_t* ow, uint32_t codebook_id, wwise_setup_t setup_type, STREAMFILE* sf) {
    size_t ibufsize = 0x8000; /* arbitrary max size of a codebook */
    uint8_t ibuf[0x8000]; /* Wwise codebook buffer */
    size_t cb_size;
    bitstream_t iw;

    cb_size = load_wvc(ibuf,ibufsize, codebook_id, setup_type, sf);
    if (cb_size == 0) goto fail;

    init_bitstream(&iw, ibuf, ibufsize);

    return ww2ogg_codebook_library_rebuild(ow, &iw, cb_size, sf);
fail:
    return 0;
}

/* Rebuild a Wwise setup (simplified with removed stuff), recreating all six setup parts.
 * (ref: https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-650004.2.4) */
static int ww2ogg_generate_vorbis_setup(bitstream_t* ow, bitstream_t* iw, vorbis_custom_codec_data* data, size_t packet_size, STREAMFILE* sf) {
    int i, j, k;
    int channels = data->config.channels;
    uint32_t codebook_count = 0, floor_count = 0, residue_count = 0;
    uint32_t codebook_count_less1 = 0;
    uint32_t time_count_less1 = 0, dummy_time_value = 0;


    /* packet header */
    put_8bit(ow->buf+0x00, 0x05);            /* packet_type (setup) */
    memcpy  (ow->buf+0x01, "vorbis", 6);     /* id */
    ow->b_off += (1+6) * 8; /* bit offset of output (Vorbis) setup, after fake type + id */


    /* Codebooks */
    rv_bits(iw,  8,&codebook_count_less1);
    wv_bits(ow,  8, codebook_count_less1);
    codebook_count = codebook_count_less1 + 1;

    if (data->config.setup_type == WWV_FULL_SETUP) {
        /* rebuild Wwise codebooks: untouched */
        for (i = 0; i < codebook_count; i++) {
            if (!ww2ogg_codebook_library_copy(ow, iw))
                goto fail;
        }
    }
    else if (data->config.setup_type == WWV_INLINE_CODEBOOKS) {
        /* rebuild Wwise codebooks: inline in simplified format */
        for (i = 0; i < codebook_count; i++) {
            if (!ww2ogg_codebook_library_rebuild(ow, iw, 0, sf))
                goto fail;
        }
    }
    else {
        /* rebuild Wwise codebooks: external (referenced by id) in simplified format */
        for (i = 0; i < codebook_count; i++) {
            int rc;
            uint32_t codebook_id = 0;

            rv_bits(iw, 10,&codebook_id);

            rc = ww2ogg_codebook_library_rebuild_by_id(ow, codebook_id, data->config.setup_type, sf);
            if (!rc) goto fail;
        }
    }


    /* Time domain transforms */
    time_count_less1 = 0;
    wv_bits(ow,  6, time_count_less1);
    dummy_time_value = 0;
    wv_bits(ow, 16, dummy_time_value);


    if (data->config.setup_type == WWV_FULL_SETUP) {
        /* rest of setup is untouched, copy bits */
        uint32_t bitly = 0;
        uint32_t total_bits_read = iw->b_off;
        uint32_t setup_packet_size_bits = packet_size * 8;

        while (total_bits_read < setup_packet_size_bits) {
            rv_bits(iw,  1,&bitly);
            wv_bits(ow,  1, bitly);
            total_bits_read = iw->b_off;
        }
    }
    else {
        /* rest of setup is altered, reconstruct */
        uint32_t floor_count_less1 = 0, floor1_multiplier_less1 = 0, rangebits = 0;
        uint32_t residue_count_less1 = 0;
        uint32_t mapping_count_less1 = 0, mapping_count = 0;
        uint32_t mode_count_less1 = 0, mode_count = 0;


        /* Floors */
        rv_bits(iw,  6,&floor_count_less1);
        wv_bits(ow,  6, floor_count_less1);
        floor_count = floor_count_less1 + 1;

        for (i = 0; i < floor_count; i++) {
            uint32_t floor_type = 0, floor1_partitions = 0;
            uint32_t maximum_class = 0;
            uint32_t floor1_partition_class_list[32]; /* max 5b */
            uint32_t floor1_class_dimensions_list[16+1]; /* max 4b+1 */

            // Always floor type 1
            floor_type = 1;
            wv_bits(ow, 16, floor_type);

            rv_bits(iw,  5,&floor1_partitions);
            wv_bits(ow,  5, floor1_partitions);

            memset(floor1_partition_class_list, 0, sizeof(uint32_t)*32);

            maximum_class = 0;
            for (j = 0; j < floor1_partitions; j++) {
                uint32_t floor1_partition_class = 0;

                rv_bits(iw,  4,&floor1_partition_class);
                wv_bits(ow,  4, floor1_partition_class);

                floor1_partition_class_list[j] = floor1_partition_class;

                if (floor1_partition_class > maximum_class)
                    maximum_class = floor1_partition_class;
            }

            memset(floor1_class_dimensions_list, 0, sizeof(uint32_t)*(16+1));

            for (j = 0; j <= maximum_class; j++) {
                uint32_t class_dimensions_less1 = 0, class_subclasses = 0;

                rv_bits(iw,  3,&class_dimensions_less1);
                wv_bits(ow,  3, class_dimensions_less1);

                floor1_class_dimensions_list[j] = class_dimensions_less1 + 1;

                rv_bits(iw,  2,&class_subclasses);
                wv_bits(ow,  2, class_subclasses);

                if (0 != class_subclasses) {
                    uint32_t masterbook = 0;

                    rv_bits(iw,  8,&masterbook);
                    wv_bits(ow,  8, masterbook);

                    if (masterbook >= codebook_count) {
                        VGM_LOG("Wwise Vorbis: invalid floor1 masterbook\n");
                        goto fail;
                    }
                }

                for (k = 0; k < (1U<<class_subclasses); k++) {
                    uint32_t subclass_book_plus1 = 0;
                    int subclass_book = 0; /* this MUST be int */

                    rv_bits(iw,  8,&subclass_book_plus1);
                    wv_bits(ow,  8, subclass_book_plus1);

                    subclass_book = subclass_book_plus1 - 1;
                    if (subclass_book >= 0 && subclass_book >= codebook_count) {
                        VGM_LOG("Wwise Vorbis: invalid floor1 subclass book\n");
                        goto fail;
                    }
                }
            }

            rv_bits(iw,  2,&floor1_multiplier_less1);
            wv_bits(ow,  2, floor1_multiplier_less1);

            rv_bits(iw,  4,&rangebits);
            wv_bits(ow,  4, rangebits);

            for (j = 0; j < floor1_partitions; j++) {
                uint32_t current_class_number = 0;

                current_class_number = floor1_partition_class_list[j];
                for (k = 0; k < floor1_class_dimensions_list[current_class_number]; k++) {
                    uint32_t X = 0; /* max 4b (15) */

                    rv_bits(iw,  rangebits,&X);
                    wv_bits(ow,  rangebits, X);
                }
            }
        }


        /* Residues */
        rv_bits(iw,  6,&residue_count_less1);
        wv_bits(ow,  6, residue_count_less1);
        residue_count = residue_count_less1 + 1;

        for (i = 0; i < residue_count; i++) {
            uint32_t residue_type = 0, residue_classifications = 0;
            uint32_t residue_begin = 0, residue_end = 0, residue_partition_size_less1 = 0, residue_classifications_less1 = 0, residue_classbook = 0;
            uint32_t residue_cascade[64+1]; /* 6b +1 */

            rv_bits(iw,  2,&residue_type);
            wv_bits(ow, 16, residue_type); /* 2b to 16b */

            if (residue_type > 2) {
                VGM_LOG("Wwise Vorbis: invalid residue type\n");
                goto fail;
            }

            rv_bits(iw, 24,&residue_begin);
            wv_bits(ow, 24, residue_begin);
            rv_bits(iw, 24,&residue_end);
            wv_bits(ow, 24, residue_end);
            rv_bits(iw, 24,&residue_partition_size_less1);
            wv_bits(ow, 24, residue_partition_size_less1);
            rv_bits(iw,  6,&residue_classifications_less1);
            wv_bits(ow,  6, residue_classifications_less1);
            rv_bits(iw,  8,&residue_classbook);
            wv_bits(ow,  8, residue_classbook);
            residue_classifications = residue_classifications_less1 + 1;

            if (residue_classbook >= codebook_count) {
                VGM_LOG("Wwise Vorbis: invalid residue classbook\n");
                goto fail;
            }

            memset(residue_cascade, 0, sizeof(uint32_t)*(64+1));

            for (j = 0; j < residue_classifications; j++) {
                uint32_t high_bits = 0, lowv_bits = 0, bitflag = 0;

                high_bits = 0;

                rv_bits(iw, 3,&lowv_bits);
                wv_bits(ow, 3, lowv_bits);

                rv_bits(iw, 1,&bitflag);
                wv_bits(ow, 1, bitflag);
                if (bitflag) {
                    rv_bits(iw, 5,&high_bits);
                    wv_bits(ow, 5, high_bits);
                }

                residue_cascade[j] = high_bits * 8 + lowv_bits;
            }

            for (j = 0; j < residue_classifications; j++) {
                for (k = 0; k < 8; k++) {
                    if (residue_cascade[j] & (1 << k)) {
                        uint32_t residue_book = 0;

                        rv_bits(iw, 8,&residue_book);
                        wv_bits(ow, 8, residue_book);

                        if (residue_book >= codebook_count) {
                            VGM_LOG("Wwise Vorbis: invalid residue book\n");
                            goto fail;
                        }
                    }
                }
            }
        }


        /* Mappings */
        rv_bits(iw,  6,&mapping_count_less1);
        wv_bits(ow,  6, mapping_count_less1);
        mapping_count = mapping_count_less1 + 1;

        for (i = 0; i < mapping_count; i++) {
            uint32_t mapping_type = 0, submaps_flag = 0, submaps = 0, square_polar_flag = 0;
            uint32_t mapping_reserved = 0;

            // always mapping type 0, the only one
            mapping_type = 0;
            wv_bits(ow, 16, mapping_type);

            rv_bits(iw,  1,&submaps_flag);
            wv_bits(ow,  1, submaps_flag);

            submaps = 1;
            if (submaps_flag) {
                uint32_t submaps_less1 = 0;

                rv_bits(iw,  4,&submaps_less1);
                wv_bits(ow,  4, submaps_less1);
                submaps = submaps_less1 + 1;
            }

            rv_bits(iw,  1,&square_polar_flag);
            wv_bits(ow,  1, square_polar_flag);

            if (square_polar_flag) {
                uint32_t coupling_steps_less1 = 0, coupling_steps = 0;

                rv_bits(iw,  8,&coupling_steps_less1);
                wv_bits(ow,  8, coupling_steps_less1);
                coupling_steps = coupling_steps_less1 + 1;

                for (j = 0; j < coupling_steps; j++) {
                    uint32_t magnitude = 0, angle = 0;
                    int magnitude_bits = ww2ogg_tremor_ilog(channels-1);
                    int angle_bits = ww2ogg_tremor_ilog(channels-1);

                    rv_bits(iw,  magnitude_bits,&magnitude);
                    wv_bits(ow,  magnitude_bits, magnitude);
                    rv_bits(iw,  angle_bits,&angle);
                    wv_bits(ow,  angle_bits, angle);

                    if (angle == magnitude || magnitude >= channels || angle >= channels) {
                        VGM_LOG("Wwise Vorbis: invalid coupling (angle=%i, mag=%i, ch=%i)\n", angle, magnitude,channels);
                        goto fail;
                    }
                }
            }

            // a rare reserved field not removed by Ak!
            rv_bits(iw,  2,&mapping_reserved);
            wv_bits(ow,  2, mapping_reserved);
            if (0 != mapping_reserved) {
                VGM_LOG("Wwise Vorbis: mapping reserved field nonzero\n");
                goto fail;
            }

            if (submaps > 1) {
                for (j = 0; j < channels; j++) {
                    uint32_t mapping_mux = 0;

                    rv_bits(iw,  4,&mapping_mux);
                    wv_bits(ow,  4, mapping_mux);
                    if (mapping_mux >= submaps) {
                        VGM_LOG("Wwise Vorbis: mapping_mux >= submaps\n");
                        goto fail;
                    }
                }
            }

            for (j = 0; j < submaps; j++) {
                uint32_t time_config = 0, floor_number = 0, residue_number = 0;

                // Another! Unused time domain transform configuration placeholder!
                rv_bits(iw,  8,&time_config);
                wv_bits(ow,  8, time_config);

                rv_bits(iw,  8,&floor_number);
                wv_bits(ow,  8, floor_number);
                if (floor_number >= floor_count) {
                    VGM_LOG("Wwise Vorbis: invalid floor mapping\n");
                    goto fail;
                }

                rv_bits(iw,  8,&residue_number);
                wv_bits(ow,  8, residue_number);
                if (residue_number >= residue_count) {
                    VGM_LOG("Wwise Vorbis: invalid residue mapping\n");
                    goto fail;
                }
            }
        }


        /* Modes */
        rv_bits(iw,  6,&mode_count_less1);
        wv_bits(ow,  6, mode_count_less1);
        mode_count = mode_count_less1 + 1;

        memset(data->mode_blockflag, 0, sizeof(uint8_t)*(64+1)); /* up to max mode_count */
        data->mode_bits = ww2ogg_tremor_ilog(mode_count-1); /* for mod_packets */

        for (i = 0; i < mode_count; i++) {
            uint32_t block_flag = 0, windowtype = 0, transformtype = 0, mapping = 0;

            rv_bits(iw,  1,&block_flag);
            wv_bits(ow,  1, block_flag);

            data->mode_blockflag[i] = (block_flag != 0); /* for mod_packets */

            windowtype = 0;
            transformtype = 0;
            wv_bits(ow, 16, windowtype);
            wv_bits(ow, 16, transformtype);

            rv_bits(iw,  8,&mapping);
            wv_bits(ow,  8, mapping);
            if (mapping >= mapping_count) {
                VGM_LOG("Wwise Vorbis: invalid mode mapping\n");
                goto fail;
            }
        }
    }


    /* end flag */
    {
        uint32_t framing = 0;

        framing = 1;
        wv_bits(ow,  1, framing);
    }

    /* remove trailing garbage bits */
    if (ow->b_off % 8 != 0) {
        uint32_t padding = 0;
        int padding_bits = 8 - (ow->b_off % 8);

        wv_bits(ow,  padding_bits,  padding);
    }


    return 1;
fail:
    return 0;
}


/* **************************************************************************** */
/* INTERNAL UTILS                                                               */
/* **************************************************************************** */

/* loads an external Wwise Vorbis Codebooks file (wvc) referenced by ID and returns size */
static int load_wvc(uint8_t* ibuf, size_t ibufsize, uint32_t codebook_id, wwise_setup_t setup_type, STREAMFILE* sf) {
    size_t bytes;

    /* try to locate from the precompiled list */
    bytes = load_wvc_array(ibuf, ibufsize, codebook_id, setup_type);
    if (bytes)
        return bytes;

#if !(WWISE_VORBIS_USE_PRECOMPILED_WVC)
    /* try to load from external file (ignoring type, just use file if found) */
    bytes = load_wvc_file(ibuf, ibufsize, codebook_id, sf);
    if (bytes)
        return bytes;
#endif

    /* not found */
    VGM_LOG("Wwise Vorbis: codebook_id %04x not found\n", codebook_id);
    return 0;
}

#if !(WWISE_VORBIS_USE_PRECOMPILED_WVC)
static int load_wvc_file(uint8_t* buf, size_t bufsize, uint32_t codebook_id, STREAMFILE* sf) {
    STREAMFILE* sf_setup = NULL;
    size_t wvc_size = 0;

    /* get from artificial external file (used if compiled without codebooks) */
    {
        char setupname[0x20];

        snprintf(setupname, sizeof(setupname), ".wvc");
        sf_setup = open_streamfile_by_filename(sf, setupname);
        if (!sf_setup) goto fail;

        wvc_size = get_streamfile_size(sf_setup);
    }

    /* find codebook and copy to buffer */
    {
        off_t table_start, codebook_offset;
        size_t codebook_size;
        int codebook_count;

        /* at the end of the WVC is an offset table, and we need to find codebook id (number) offset */
        table_start = read_u32le(wvc_size - 4, sf_setup); /* last offset */
        codebook_count = ((wvc_size - table_start) / 4) - 1;
        if (table_start > wvc_size || codebook_id >= codebook_count) goto fail;

        codebook_offset = read_u32le(table_start + codebook_id*4, sf_setup);
        codebook_size   = read_u32le(table_start + codebook_id*4 + 4, sf_setup) - codebook_offset;
        if (codebook_size > bufsize) goto fail;

        if (read_streamfile(buf, codebook_offset, codebook_size, sf_setup) != codebook_size)
            goto fail;

        close_streamfile(sf_setup);
        return codebook_size;
    }


fail:
    close_streamfile(sf_setup);
    return 0;
}
#endif

static int load_wvc_array(uint8_t* buf, size_t bufsize, uint32_t codebook_id, wwise_setup_t setup_type) {
#if WWISE_VORBIS_USE_PRECOMPILED_WVC

    /* get pointer to array */
    {
        int i, list_length;
        const wvc_info * wvc_list;

        switch (setup_type) {
            case WWV_EXTERNAL_CODEBOOKS:
                wvc_list = wvc_list_standard;
                list_length = sizeof(wvc_list_standard) / sizeof(wvc_info);
                break;
            case WWV_AOTUV603_CODEBOOKS:
                wvc_list = wvc_list_aotuv603;
                list_length = sizeof(wvc_list_standard) / sizeof(wvc_info);
                break;
            default:
                goto fail;
        }

        for (i=0; i < list_length; i++) {
            if (wvc_list[i].id == codebook_id) {
                if (wvc_list[i].size > bufsize) goto fail;
                /* found: copy data as-is */
                memcpy(buf,wvc_list[i].codebook, wvc_list[i].size);
                return wvc_list[i].size;
            }
        }
    }

    // this can be used with 1:1 dump of the codebook file
#if 0
    if (wvc == NULL) goto fail;
    /* find codebook and copy to buffer */
    {
        off_t table_start, codebook_offset;
        size_t codebook_size;
        int codebook_count;

        /* at the end of the WVC is an offset table, and we need to find codebook id (number) offset */
        table_start = get_32bitLE(wvc + wvc_size - 4); /* last offset */
        codebook_count = ((wvc_size - table_start) / 4) - 1;
        if (codebook_id >= codebook_count) goto fail;

        codebook_offset = get_32bitLE(wvc + table_start + codebook_id*4);
        codebook_size   = get_32bitLE(wvc + table_start + codebook_id*4 + 4) - codebook_offset;
        if (codebook_size > bufsize) goto fail;

        memcpy(buf, wvc+codebook_offset, codebook_size);

        return codebook_size;
    }
#endif

fail:
#endif
    return 0;
}

#endif
