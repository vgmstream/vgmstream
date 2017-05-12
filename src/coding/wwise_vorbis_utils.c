#include "../vgmstream.h"
#ifdef VGM_USE_VORBIS
#include "wwise_vorbis_utils.h"

#define WWISE_VORBIS_USE_PRECOMPILED_WVC 1 /* if enabled vgmstream weights ~150kb more but doesn't need external .wvc packets */

#if WWISE_VORBIS_USE_PRECOMPILED_WVC
#include "wwise_vorbis_data.h"
#endif


/* **************************************************************************** */
/* DEFS                                                                         */
/* **************************************************************************** */

/* A internal struct to pass around and simulate a bitstream.
 * Mainly to keep code cleaner and somewhat closer to ww2ogg */
typedef struct {
    uint8_t * buf;      /* buffer to read/write*/
    size_t bufsize;     /* max size of the buffer */
    off_t b_off;        /* current offset in bits inside the buffer */
} ww_stream;

static int generate_vorbis_packet(ww_stream * ow, ww_stream * iw, STREAMFILE *streamFile, off_t offset, vorbis_codec_data * data, int big_endian);
static int generate_vorbis_setup(ww_stream * ow, ww_stream * iw, vorbis_codec_data * data, int channels, size_t packet_size, STREAMFILE *streamFile);

static int codebook_library_copy(ww_stream * ow, ww_stream * iw);
static int codebook_library_rebuild(ww_stream * ow, ww_stream * iw, size_t cb_size, STREAMFILE *streamFile);
static int codebook_library_rebuild_by_id(ww_stream * ow, uint32_t codebook_id, wwise_setup_type setup_type, STREAMFILE *streamFile);
static int tremor_ilog(unsigned int v);
static unsigned int tremor_book_maptype1_quantvals(unsigned int entries, unsigned int dimensions);

static int load_wvc(uint8_t * ibuf, size_t ibufsize, uint32_t codebook_id, wwise_setup_type setup_type, STREAMFILE *streamFile);
static int load_wvc_file(uint8_t * buf, size_t bufsize, uint32_t codebook_id, STREAMFILE *streamFile);
static int load_wvc_array(uint8_t * buf, size_t bufsize, uint32_t codebook_id, wwise_setup_type setup_type);

static int r_bits(ww_stream * iw, int num_bits, uint32_t * value);
static int w_bits(ww_stream * ow, int num_bits, uint32_t value);


/* **************************************************************************** */
/* EXTERNAL API                                                                 */
/* **************************************************************************** */

/* loads info from a wwise packet header */
int wwise_vorbis_get_header(STREAMFILE *streamFile, off_t offset, wwise_header_type header_type, int * granulepos, size_t * packet_size, int big_endian) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = big_endian ? read_32bitBE : read_32bitLE;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = big_endian ? read_16bitBE : read_16bitLE;

    /* packet size doesn't include header size */
    switch(header_type) {
        case TYPE_8: /* size 4+4 */
            *packet_size = (uint32_t)read_32bit(offset, streamFile);
            *granulepos = read_32bit(offset+4, streamFile);
            return 8;

        case TYPE_6: /* size 4+2 */
            *packet_size = (uint16_t)read_16bit(offset, streamFile);
            *granulepos = read_32bit(offset+2, streamFile);
            return 6;

        case TYPE_2: /* size 2 */
            *packet_size = (uint16_t)read_16bit(offset, streamFile);
            *granulepos = 0; /* granule is an arbitrary unit so we could use offset instead; libvorbis has no actually need it actually */
            return 2;
            break;
        default: /* ? */
            return 0;
    }
}

/* Transforms a Wwise data packet into a real Vorbis one (depending on config) */
int wwise_vorbis_rebuild_packet(uint8_t * obuf, size_t obufsize, STREAMFILE *streamFile, off_t offset, vorbis_codec_data * data, int big_endian) {
    ww_stream ow, iw;
    int rc, granulepos;
    size_t header_size, packet_size;

    size_t ibufsize = 0x8000; /* arbitrary max size of a setup packet */
    uint8_t ibuf[0x8000]; /* Wwise setup packet buffer */
    if (obufsize < ibufsize) goto fail; /* arbitrary expected min */

    header_size = wwise_vorbis_get_header(streamFile, offset, data->header_type, &granulepos, &packet_size, big_endian);
    if (!header_size || packet_size > obufsize) goto fail;

    /* load Wwise data into internal buffer */
    if (read_streamfile(ibuf,offset+header_size,packet_size, streamFile)!=packet_size)
        goto fail;

    /* prepare helper structs */
    ow.buf = obuf;
    ow.bufsize = obufsize;
    ow.b_off = 0 ;

    iw.buf = ibuf;
    iw.bufsize = ibufsize;
    iw.b_off = 0;

    rc = generate_vorbis_packet(&ow,&iw, streamFile,offset, data, big_endian);
    if (!rc) goto fail;

    if (ow.b_off % 8 != 0) {
        VGM_LOG("Wwise Vorbis: didn't write exactly audio packet: 0x%lx + %li bits\n", ow.b_off / 8, ow.b_off % 8);
        goto fail;
    }


    return ow.b_off / 8;
fail:
    return 0;
}


/* Transforms a Wwise setup packet into a real Vorbis one (depending on config). */
int wwise_vorbis_rebuild_setup(uint8_t * obuf, size_t obufsize, STREAMFILE *streamFile, off_t offset, vorbis_codec_data * data, int big_endian, int channels) {
    ww_stream ow, iw;
    int rc, granulepos;
    size_t header_size, packet_size;

    size_t ibufsize = 0x8000; /* arbitrary max size of a setup packet */
    uint8_t ibuf[0x8000]; /* Wwise setup packet buffer */
    if (obufsize < ibufsize) goto fail; /* arbitrary expected min */

    /* read Wwise packet header */
    header_size = wwise_vorbis_get_header(streamFile, offset, data->header_type, &granulepos, &packet_size, big_endian);
    if (!header_size || packet_size > ibufsize) goto fail;

    /* load Wwise setup into internal buffer */
    if (read_streamfile(ibuf,offset+header_size,packet_size, streamFile)!=packet_size)
        goto fail;

    /* prepare helper structs */
    ow.buf = obuf;
    ow.bufsize = obufsize;
    ow.b_off = 0;

    iw.buf = ibuf;
    iw.bufsize = ibufsize;
    iw.b_off = 0;

    rc = generate_vorbis_setup(&ow,&iw, data, channels, packet_size, streamFile);
    if (!rc) goto fail;

    if (ow.b_off % 8 != 0) {
        VGM_LOG("Wwise Vorbis: didn't write exactly setup packet: 0x%lx + %li bits\n", ow.b_off / 8, ow.b_off % 8);
        goto fail;
    }


    return ow.b_off / 8;
fail:
    return 0;
}


/* **************************************************************************** */
/* INTERNAL WW2OGG STUFF                                                        */
/* **************************************************************************** */
/* The following code was mostly converted from hcs's ww2ogg.
 * Could be simplified but roughly tries to preserve the structure in case fixes have to be backported.
 *
 * Some validations are ommited (ex. read/write), as incorrect data should be rejected by libvorbis.
 * To avoid GCC complaining all values are init to 0, and some that do need it are init again, for clarity.
 * Reads/writes unsigned ints as most are bit values less than 32 and with no sign meaning.
 */

/* Copy packet as-is or rebuild first byte if mod_packets is used.
 * (ref: https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-720004.3) */
static int generate_vorbis_packet(ww_stream * ow, ww_stream * iw, STREAMFILE *streamFile, off_t offset, vorbis_codec_data * data, int big_endian) {
    int i,granule;
    size_t header_size, packet_size, data_size;

    header_size = wwise_vorbis_get_header(streamFile,offset, data->header_type, &granule, &packet_size, big_endian);
    if (!header_size || packet_size > iw->bufsize) goto fail;

    data_size = get_streamfile_size(streamFile);//todo get external data_size

    if (offset + header_size + packet_size > data_size) {
        VGM_LOG("Wwise Vorbis: page header truncated\n");
        goto fail;
    }

    /* this may happen in the first packet; maybe it's for the encoder delay but doesn't seem to affect libvorbis */
    //VGM_ASSERT(granule < 0, "Wwise Vorbis: negative granule %i @ 0x%lx\n", granule, offset);


    if (data->packet_type == MODIFIED) {
        /* rebuild first bits of packet type and window info (for the i-MDCT) */
        uint32_t packet_type = 0, mode_number = 0, remainder = 0;

        if (!data->mode_blockflag) { /* config error */
            VGM_LOG("Wwise Vorbis: didn't load mode_blockflag\n");
            goto fail;
        }

        /* audio packet type */
        packet_type = 0;
        w_bits(ow,  1, packet_type);

        /* collect this packet mode from the first byte */
        r_bits(iw,  data->mode_bits,&mode_number); /* max 6b */
        w_bits(ow,  data->mode_bits, mode_number);
        r_bits(iw,  8-data->mode_bits,&remainder);

        /* adjust window info */
        if (data->mode_blockflag[mode_number]) {
            /* long window: peek at next frame to find flags */
            off_t next_offset = offset + header_size + packet_size;
            uint32_t next_blockflag = 0, prev_window_type = 0, next_window_type = 0;

            next_blockflag = 0;
            /* check if more data / not eof */
            if (next_offset + header_size <= data_size) {
                size_t next_header_size, next_packet_size;
                int next_granule;

                next_header_size = wwise_vorbis_get_header(streamFile,next_offset, data->header_type, &next_granule, &next_packet_size, big_endian);
                if (!next_header_size) goto fail;

                if (next_packet_size > 0) {
                    /* get next first byte to read next_mode_number */
                    uint32_t next_mode_number;
                    uint8_t nbuf[1];
                    ww_stream nw;
                    nw.buf = nbuf;
                    nw.bufsize = 1;
                    nw.b_off = 0;

                    if (read_streamfile(nw.buf, next_offset + next_header_size, nw.bufsize, streamFile) != nw.bufsize)
                        goto fail;

                    r_bits(&nw,  data->mode_bits,&next_mode_number); /* max 6b */
                    
                    next_blockflag = data->mode_blockflag[next_mode_number];
                }
            }

            prev_window_type = data->prev_blockflag;
            w_bits(ow,  1, prev_window_type);

            next_window_type = next_blockflag;
            w_bits(ow,  1, next_window_type);
        }

        data->prev_blockflag = data->mode_blockflag[mode_number]; /* save for next packet */

        w_bits(ow,  8-data->mode_bits, remainder); /* this *isn't* byte aligned (ex. could be 10 bits written) */
    }
    else {
        /* normal packets: first byte unchanged */
        uint32_t c = 0;

        r_bits(iw,  8, &c);
        w_bits(ow,  8,  c);
    }


    /* remainder of packet (not byte-aligned when using mod_packets) */
    for (i = 1; i < packet_size; i++) {
        uint32_t c = 0;

        r_bits(iw,  8, &c);
        w_bits(ow,  8,  c);
    }

    /* remove trailing garbage bits */
    if (ow->b_off % 8 != 0) {
        uint32_t padding = 0;
        int padding_bits = 8 - (ow->b_off % 8);

        w_bits(ow,  padding_bits,  padding);
    }


    return 1;
fail:
    return 0;
}


/* Rebuild a Wwise setup (simplified with removed stuff), recreating all six setup parts.
 * (ref: https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-650004.2.4) */
static int generate_vorbis_setup(ww_stream * ow, ww_stream * iw, vorbis_codec_data * data, int channels, size_t packet_size, STREAMFILE *streamFile) {
    int i,j,k;
    uint32_t codebook_count = 0, floor_count = 0, residue_count = 0;
    uint32_t codebook_count_less1 = 0;
    uint32_t time_count_less1 = 0, dummy_time_value = 0;


    /* packet header */
    put_8bit(ow->buf+0x00, 0x05);            /* packet_type (setup) */
    memcpy  (ow->buf+0x01, "vorbis", 6);     /* id */
    ow->b_off += (1+6) * 8; /* bit offset of output (Vorbis) setup, after fake type + id */


    /* Codebooks */
    r_bits(iw,  8,&codebook_count_less1);
    w_bits(ow,  8, codebook_count_less1);
    codebook_count = codebook_count_less1 + 1;

    if (data->setup_type == FULL_SETUP) {
        /* rebuild Wwise codebooks: untouched */
        for (i = 0; i < codebook_count; i++) {
            if(!codebook_library_copy(ow, iw)) goto fail;
        }
    }
    else if (data->setup_type == INLINE_CODEBOOKS) {
        /* rebuild Wwise codebooks: inline in simplified format */
        for (i = 0; i < codebook_count; i++) {
            if(!codebook_library_rebuild(ow, iw, 0, streamFile)) goto fail;
        }
    }
    else {
        /* rebuild Wwise codebooks: external (referenced by id) in simplified format */
        for (i = 0; i < codebook_count; i++) {
            int rc;
            uint32_t codebook_id = 0;

            r_bits(iw, 10,&codebook_id);

            rc = codebook_library_rebuild_by_id(ow, codebook_id, data->setup_type, streamFile);
            if (!rc) goto fail;
        }
    }


    /* Time domain transforms */
    time_count_less1 = 0;
    w_bits(ow,  6, time_count_less1);
    dummy_time_value = 0;
    w_bits(ow, 16, dummy_time_value);


    if (data->setup_type == FULL_SETUP) {
        /* rest of setup is untouched, copy bits */
        uint32_t bitly = 0;
        uint32_t total_bits_read = iw->b_off;
        uint32_t setup_packet_size_bits = packet_size*8;

        while (total_bits_read < setup_packet_size_bits) {
            r_bits(iw,  1,&bitly);
            w_bits(ow,  1, bitly);
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
        r_bits(iw,  6,&floor_count_less1);
        w_bits(ow,  6, floor_count_less1);
        floor_count = floor_count_less1 + 1;

        for (i = 0; i < floor_count; i++) {
            uint32_t floor_type = 0, floor1_partitions = 0;
            uint32_t maximum_class = 0;
            uint32_t floor1_partition_class_list[32]; /* max 5b */
            uint32_t floor1_class_dimensions_list[16+1]; /* max 4b+1 */

            // Always floor type 1
            floor_type = 1;
            w_bits(ow, 16, floor_type);

            r_bits(iw,  5,&floor1_partitions);
            w_bits(ow,  5, floor1_partitions);

            memset(floor1_partition_class_list, 0, sizeof(uint32_t)*32);

            maximum_class = 0;
            for (j = 0; j < floor1_partitions; j++) {
                uint32_t floor1_partition_class = 0;

                r_bits(iw,  4,&floor1_partition_class);
                w_bits(ow,  4, floor1_partition_class);

                floor1_partition_class_list[j] = floor1_partition_class;

                if (floor1_partition_class > maximum_class)
                    maximum_class = floor1_partition_class;
            }

            memset(floor1_class_dimensions_list, 0, sizeof(uint32_t)*(16+1));

            for (j = 0; j <= maximum_class; j++) {
                uint32_t class_dimensions_less1 = 0, class_subclasses = 0;

                r_bits(iw,  3,&class_dimensions_less1);
                w_bits(ow,  3, class_dimensions_less1);

                floor1_class_dimensions_list[j] = class_dimensions_less1 + 1;

                r_bits(iw,  2,&class_subclasses);
                w_bits(ow,  2, class_subclasses);

                if (0 != class_subclasses) {
                    uint32_t masterbook = 0;

                    r_bits(iw,  8,&masterbook);
                    w_bits(ow,  8, masterbook);

                    if (masterbook >= codebook_count) {
                        VGM_LOG("Wwise Vorbis: invalid floor1 masterbook\n");
                        goto fail;
                    }
                }

                for (k = 0; k < (1U<<class_subclasses); k++) {
                    uint32_t subclass_book_plus1 = 0;
                    int subclass_book = 0; /* this MUST be int */

                    r_bits(iw,  8,&subclass_book_plus1);
                    w_bits(ow,  8, subclass_book_plus1);

                    subclass_book = subclass_book_plus1 - 1;
                    if (subclass_book >= 0 && subclass_book >= codebook_count) {
                        VGM_LOG("Wwise Vorbis: invalid floor1 subclass book\n");
                        goto fail;
                    }
                }
            }

            r_bits(iw,  2,&floor1_multiplier_less1);
            w_bits(ow,  2, floor1_multiplier_less1);

            r_bits(iw,  4,&rangebits);
            w_bits(ow,  4, rangebits);

            for (j = 0; j < floor1_partitions; j++) {
                uint32_t current_class_number = 0;

                current_class_number = floor1_partition_class_list[j];
                for (k = 0; k < floor1_class_dimensions_list[current_class_number]; k++) {
                    uint32_t X = 0; /* max 4b (15) */

                    r_bits(iw,  rangebits,&X);
                    w_bits(ow,  rangebits, X);
                }
            }
        }


        /* Residues */
        r_bits(iw,  6,&residue_count_less1);
        w_bits(ow,  6, residue_count_less1);
        residue_count = residue_count_less1 + 1;

        for (i = 0; i < residue_count; i++) {
            uint32_t residue_type = 0, residue_classifications = 0;
            uint32_t residue_begin = 0, residue_end = 0, residue_partition_size_less1 = 0, residue_classifications_less1 = 0, residue_classbook = 0;
            uint32_t residue_cascade[64+1]; /* 6b +1 */

            r_bits(iw,  2,&residue_type);
            w_bits(ow, 16, residue_type); /* 2b to 16b */

            if (residue_type > 2) {
                VGM_LOG("Wwise Vorbis: invalid residue type\n");
                goto fail;
            }

            r_bits(iw, 24,&residue_begin);
            w_bits(ow, 24, residue_begin);
            r_bits(iw, 24,&residue_end);
            w_bits(ow, 24, residue_end);
            r_bits(iw, 24,&residue_partition_size_less1);
            w_bits(ow, 24, residue_partition_size_less1);
            r_bits(iw,  6,&residue_classifications_less1);
            w_bits(ow,  6, residue_classifications_less1);
            r_bits(iw,  8,&residue_classbook);
            w_bits(ow,  8, residue_classbook);
            residue_classifications = residue_classifications_less1 + 1;

            if (residue_classbook >= codebook_count) {
                VGM_LOG("Wwise Vorbis: invalid residue classbook\n");
                goto fail;
            }

            memset(residue_cascade, 0, sizeof(uint32_t)*(64+1));

            for (j = 0; j < residue_classifications; j++) {
                uint32_t high_bits = 0, low_bits = 0, bitflag = 0;

                high_bits = 0;

                r_bits(iw, 3,&low_bits);
                w_bits(ow, 3, low_bits);

                r_bits(iw, 1,&bitflag);
                w_bits(ow, 1, bitflag);
                if (bitflag) {
                    r_bits(iw, 5,&high_bits);
                    w_bits(ow, 5, high_bits);
                }

                residue_cascade[j] = high_bits * 8 + low_bits;
            }

            for (j = 0; j < residue_classifications; j++) {
                for (k = 0; k < 8; k++) {
                    if (residue_cascade[j] & (1 << k)) {
                        uint32_t residue_book = 0;

                        r_bits(iw, 8,&residue_book);
                        w_bits(ow, 8, residue_book);

                        if (residue_book >= codebook_count) {
                            VGM_LOG("Wwise Vorbis: invalid residue book\n");
                            goto fail;
                        }
                    }
                }
            }
        }


        /* Mappings */
        r_bits(iw,  6,&mapping_count_less1);
        w_bits(ow,  6, mapping_count_less1);
        mapping_count = mapping_count_less1 + 1;

        for (i = 0; i < mapping_count; i++) {
            uint32_t mapping_type = 0, submaps_flag = 0, submaps = 0, square_polar_flag = 0;
            uint32_t mapping_reserved = 0;

            // always mapping type 0, the only one
            mapping_type = 0;
            w_bits(ow, 16, mapping_type);

            r_bits(iw,  1,&submaps_flag);
            w_bits(ow,  1, submaps_flag);

            submaps = 1;
            if (submaps_flag) {
                uint32_t submaps_less1 = 0;

                r_bits(iw,  4,&submaps_less1);
                w_bits(ow,  4, submaps_less1);
                submaps = submaps_less1 + 1;
            }

            r_bits(iw,  1,&square_polar_flag);
            w_bits(ow,  1, square_polar_flag);

            if (square_polar_flag) {
                uint32_t coupling_steps_less1 = 0, coupling_steps = 0;

                r_bits(iw,  8,&coupling_steps_less1);
                w_bits(ow,  8, coupling_steps_less1);
                coupling_steps = coupling_steps_less1 + 1;

                for (j = 0; j < coupling_steps; j++) {
                    uint32_t magnitude = 0, angle = 0;
                    int magnitude_bits = tremor_ilog(channels-1);
                    int angle_bits = tremor_ilog(channels-1);

                    r_bits(iw,  magnitude_bits,&magnitude);
                    w_bits(ow,  magnitude_bits, magnitude);
                    r_bits(iw,  angle_bits,&angle);
                    w_bits(ow,  angle_bits, angle);

                    if (angle == magnitude || magnitude >= channels || angle >= channels) {
                        VGM_LOG("Wwise Vorbis: invalid coupling (angle=%i, mag=%i, ch=%i)\n", angle, magnitude,channels);
                        goto fail;
                    }
                }
            }

            // a rare reserved field not removed by Ak!
            r_bits(iw,  2,&mapping_reserved);
            w_bits(ow,  2, mapping_reserved);
            if (0 != mapping_reserved) {
                VGM_LOG("Wwise Vorbis: mapping reserved field nonzero\n");
                goto fail;
            }

            if (submaps > 1) {
                for (j = 0; j < channels; j++) {
                    uint32_t mapping_mux = 0;

                    r_bits(iw,  4,&mapping_mux);
                    w_bits(ow,  4, mapping_mux);
                    if (mapping_mux >= submaps) {
                        VGM_LOG("Wwise Vorbis: mapping_mux >= submaps\n");
                        goto fail;
                    }
                }
            }

            for (j = 0; j < submaps; j++) {
                uint32_t time_config = 0, floor_number = 0, residue_number = 0;

                // Another! Unused time domain transform configuration placeholder!
                r_bits(iw,  8,&time_config);
                w_bits(ow,  8, time_config);

                r_bits(iw,  8,&floor_number);
                w_bits(ow,  8, floor_number);
                if (floor_number >= floor_count) {
                    VGM_LOG("Wwise Vorbis: invalid floor mapping\n");
                    goto fail;
                }

                r_bits(iw,  8,&residue_number);
                w_bits(ow,  8, residue_number);
                if (residue_number >= residue_count) {
                    VGM_LOG("Wwise Vorbis: invalid residue mapping\n");
                    goto fail;
                }
            }
        }


        /* Modes */
        r_bits(iw,  6,&mode_count_less1);
        w_bits(ow,  6, mode_count_less1);
        mode_count = mode_count_less1 + 1;

        memset(data->mode_blockflag, 0, sizeof(uint8_t)*(64+1)); /* up to max mode_count */
        data->mode_bits = tremor_ilog(mode_count-1); /* for mod_packets */

        for (i = 0; i < mode_count; i++) {
            uint32_t block_flag = 0, windowtype = 0, transformtype = 0, mapping = 0;

            r_bits(iw,  1,&block_flag);
            w_bits(ow,  1, block_flag);

            data->mode_blockflag[i] = (block_flag != 0); /* for mod_packets */

            windowtype = 0;
            transformtype = 0;
            w_bits(ow, 16, windowtype);
            w_bits(ow, 16, transformtype);

            r_bits(iw,  8,&mapping);
            w_bits(ow,  8, mapping);
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
        w_bits(ow,  1, framing);
    }

    /* remove trailing garbage bits */
    if (ow->b_off % 8 != 0) {
        uint32_t padding = 0;
        int padding_bits = 8 - (ow->b_off % 8);

        w_bits(ow,  padding_bits,  padding);
    }


    return 1;
fail:
    return 0;
}


/* copies Vorbis codebooks (untouched, but size uncertain) */
static int codebook_library_copy(ww_stream * ow, ww_stream * iw) {
    int i;
    uint32_t id = 0, dimensions = 0, entries = 0;
    uint32_t ordered = 0, lookup_type = 0;

    r_bits(iw, 24,&id);
    w_bits(ow, 24, id);
    r_bits(iw, 16,&dimensions);
    w_bits(ow, 16, dimensions);
    r_bits(iw, 24,&entries);
    w_bits(ow, 24, entries);

    if (0x564342 != id) { /* "VCB" */
        VGM_LOG("Wwise Vorbis: invalid codebook identifier\n");
        goto fail;
    }

    /* codeword lengths */
    r_bits(iw,  1,&ordered);
    w_bits(ow,  1, ordered);
    if (ordered) {
        uint32_t initial_length = 0, current_entry = 0;

        r_bits(iw,  5,&initial_length);
        w_bits(ow,  5, initial_length);

        current_entry = 0;
        while (current_entry < entries) {
            uint32_t number = 0;
            int number_bits = tremor_ilog(entries-current_entry);
            
            r_bits(iw, number_bits,&number);
            w_bits(ow, number_bits, number);
            current_entry += number;
        }
        if (current_entry > entries) {
            VGM_LOG("Wwise Vorbis: current_entry out of range\n");
            goto fail;
        }
    }
    else {
        uint32_t sparse = 0;

        r_bits(iw,  1,&sparse);
        w_bits(ow,  1, sparse);

        for (i = 0; i < entries; i++) {
            uint32_t present_bool = 0;

            present_bool = 1;
            if (sparse) {
                uint32_t present = 0;

                r_bits(iw,  1,&present);
                w_bits(ow,  1, present);

                present_bool = (0 != present);
            }

            if (present_bool) {
                uint32_t codeword_length = 0;

                r_bits(iw,  5,&codeword_length);
                w_bits(ow,  5, codeword_length);
            }
        }
    }


    /* lookup table */
    r_bits(iw,  4,&lookup_type);
    w_bits(ow,  4, lookup_type);

    if (0 == lookup_type) {
        //VGM_LOG("Wwise Vorbis: no lookup table\n");
    }
    else if (1 == lookup_type) {
        //VGM_LOG("Wwise Vorbis: lookup type 1\n");
        uint32_t quantvals = 0, min = 0, max = 0;
        uint32_t value_length = 0, sequence_flag = 0;

        r_bits(iw, 32,&min);
        w_bits(ow, 32, min);
        r_bits(iw, 32,&max);
        w_bits(ow, 32, max);
        r_bits(iw,  4,&value_length);
        w_bits(ow,  4, value_length);
        r_bits(iw,  1,&sequence_flag);
        w_bits(ow,  1, sequence_flag);

        quantvals = tremor_book_maptype1_quantvals(entries, dimensions);
        for (i = 0; i < quantvals; i++) {
            uint32_t val = 0, val_bits = 0;
            val_bits = value_length+1;

            r_bits(iw, val_bits,&val);
            w_bits(ow, val_bits, val);
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
static int codebook_library_rebuild(ww_stream * ow, ww_stream * iw, size_t cb_size, STREAMFILE *streamFile) {
    int i;
    uint32_t id = 0, dimensions = 0, entries = 0;
    uint32_t ordered = 0, lookup_type = 0;

    id = 0x564342; /* "VCB" */

    w_bits(ow, 24, id);
    r_bits(iw,  4,&dimensions);
    w_bits(ow, 16, dimensions); /* 4 to 16 */
    r_bits(iw, 14,&entries);
    w_bits(ow, 24, entries); /* 14 to 24*/

    /* codeword lengths */
    r_bits(iw,  1,&ordered);
    w_bits(ow,  1, ordered);
    if (ordered) {
        uint32_t initial_length = 0, current_entry = 0;

        r_bits(iw,  5,&initial_length);
        w_bits(ow,  5, initial_length);

        current_entry = 0;
        while (current_entry < entries) {
            uint32_t number = 0;
            int number_bits = tremor_ilog(entries-current_entry);
            
            r_bits(iw, number_bits,&number);
            w_bits(ow, number_bits, number);
            current_entry += number;
        }
        if (current_entry > entries) {
            VGM_LOG("Wwise Vorbis: current_entry out of range\n");
            goto fail;
        }
    }
    else {
        uint32_t codeword_length_length = 0, sparse = 0;

        r_bits(iw,  3,&codeword_length_length);
        r_bits(iw,  1,&sparse);
        w_bits(ow,  1, sparse);

        if (0 == codeword_length_length || 5 < codeword_length_length) {
            VGM_LOG("Wwise Vorbis: nonsense codeword length\n");
            goto fail;
        }

        for (i = 0; i < entries; i++) {
            uint32_t present_bool = 0;

            present_bool = 1;
            if (sparse) {
                uint32_t present = 0;

                r_bits(iw,  1,&present);
                w_bits(ow,  1, present);

                present_bool = (0 != present);
            }

            if (present_bool) {
                uint32_t codeword_length = 0;

                r_bits(iw,  codeword_length_length,&codeword_length);
                w_bits(ow,  5, codeword_length); /* max 7 (3b) to 5 */
            }
        }
    }


    /* lookup table */
    r_bits(iw,  1,&lookup_type);
    w_bits(ow,  4, lookup_type); /* 1 to 4 */

    if (0 == lookup_type) {
        //VGM_LOG("Wwise Vorbis: no lookup table\n");
    }
    else if (1 == lookup_type) {
        //VGM_LOG("Wwise Vorbis: lookup type 1\n");
        uint32_t quantvals = 0, min = 0, max = 0;
        uint32_t value_length = 0, sequence_flag = 0;

        r_bits(iw, 32,&min);
        w_bits(ow, 32, min);
        r_bits(iw, 32,&max);
        w_bits(ow, 32, max);
        r_bits(iw,  4,&value_length);
        w_bits(ow,  4, value_length);
        r_bits(iw,  1,&sequence_flag);
        w_bits(ow,  1, sequence_flag);

        quantvals = tremor_book_maptype1_quantvals(entries, dimensions);
        for (i = 0; i < quantvals; i++) {
            uint32_t val = 0, val_bits = 0;
            val_bits = value_length+1;

            r_bits(iw, val_bits,&val);
            w_bits(ow, val_bits, val);
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
        VGM_LOG("Wwise Vorbis: codebook size mistach (expected 0x%x, wrote 0x%lx)\n", cb_size, iw->b_off/8+1);
        goto fail;
    }

    return 1;
fail:
    return 0;
}

/* rebuilds an external Wwise codebook referenced by id to a Vorbis codebook */
static int codebook_library_rebuild_by_id(ww_stream * ow, uint32_t codebook_id, wwise_setup_type setup_type, STREAMFILE *streamFile) {
    size_t ibufsize = 0x8000; /* arbitrary max size of a codebook */
    uint8_t ibuf[0x8000]; /* Wwise codebook buffer */
    size_t cb_size;
    ww_stream iw;

    cb_size = load_wvc(ibuf,ibufsize, codebook_id, setup_type, streamFile);
    if (cb_size == 0) goto fail;

    iw.buf = ibuf;
    iw.bufsize = ibufsize;
    iw.b_off = 0;

    return codebook_library_rebuild(ow, &iw, cb_size, streamFile);
fail:
    return 0;
}


/* fixed-point ilog from Xiph's Tremor */
static int tremor_ilog(unsigned int v) {
    int ret=0;
    while(v){
        ret++;
        v>>=1;
    }
    return(ret);
}
/* quantvals-something from Xiph's Tremor */
static unsigned int tremor_book_maptype1_quantvals(unsigned int entries, unsigned int dimensions) {
    /* get us a starting hint, we'll polish it below */
    int bits=tremor_ilog(entries);
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


/* **************************************************************************** */
/* INTERNAL UTILS                                                               */
/* **************************************************************************** */

/* loads an external Wwise Vorbis Codebooks file (wvc) referenced by ID and returns size */
static int load_wvc(uint8_t * ibuf, size_t ibufsize, uint32_t codebook_id, wwise_setup_type setup_type, STREAMFILE *streamFile) {
    size_t bytes;

    /* try to load from external file (ignoring type, just use file if found) */
    bytes = load_wvc_file(ibuf, ibufsize, codebook_id, streamFile);
    if (bytes)
        return bytes;

    /* try to locate from the precompiled list */
    bytes = load_wvc_array(ibuf, ibufsize, codebook_id, setup_type);
    if (bytes)
        return bytes;

    /* not found */
    VGM_LOG("Wwise Vorbis: codebook_id %04x not found\n", codebook_id);
    return 0;
}

static int load_wvc_file(uint8_t * buf, size_t bufsize, uint32_t codebook_id, STREAMFILE *streamFile) {
    STREAMFILE * streamFileWvc = NULL;
    size_t wvc_size = 0;

    {
        char setupname[PATH_LIMIT];
        char pathname[PATH_LIMIT];
        char *path;

        /* read "(dir/).wvc" */
        streamFile->get_name(streamFile,pathname,sizeof(pathname));
        path = strrchr(pathname,DIR_SEPARATOR);
        if (path)
            *(path+1) = '\0';
        else
            pathname[0] = '\0';

        snprintf(setupname,PATH_LIMIT,"%s.wvc", pathname);
        streamFileWvc = streamFile->open(streamFile,setupname,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!streamFileWvc) goto fail;

        wvc_size = streamFileWvc->get_size(streamFileWvc);
    }

    /* find codebook and copy to buffer */
    {
        off_t table_start, codebook_offset;
        size_t codebook_size;
        int codebook_count;

        /* at the end of the WVC is an offset table, and we need to find codebook id (number) offset */
        table_start = read_32bitLE(wvc_size - 4, streamFileWvc); /* last offset */
        codebook_count = ((wvc_size - table_start) / 4) - 1;
        if (table_start > wvc_size || codebook_id >= codebook_count) goto fail;

        codebook_offset = read_32bitLE(table_start + codebook_id*4, streamFileWvc);
        codebook_size   = read_32bitLE(table_start + codebook_id*4 + 4, streamFileWvc) - codebook_offset;
        if (codebook_size > bufsize) goto fail;

        if (read_streamfile(buf, codebook_offset, codebook_size, streamFileWvc) != codebook_size)
            goto fail;
        streamFileWvc->close(streamFileWvc);

        return codebook_size;
    }


fail:
    if (streamFileWvc) streamFileWvc->close(streamFileWvc);
    return 0;
}

static int load_wvc_array(uint8_t * buf, size_t bufsize, uint32_t codebook_id, wwise_setup_type setup_type) {
#if WWISE_VORBIS_USE_PRECOMPILED_WVC

    /* get pointer to array */
    {
        int i, list_length;
        const wvc_info * wvc_list;

        switch (setup_type) {
            case EXTERNAL_CODEBOOKS:
                wvc_list = wvc_list_standard;
                list_length = sizeof(wvc_list_standard) / sizeof(wvc_info);
                break;
            case AOTUV603_CODEBOOKS:
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

    // this can be used if the lists contained a 1:1 dump of the codebook files
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

/* ********************************************* */

/* Read bits (max 32) from buf and update the bit offset. Vorbis packs values in LSB order and byte by byte.
 * (ex. from 2 bytes 00100111 00000001 we can could read 4b=0111 and 6b=010010, 6b=remainder (second value is split into the 2nd byte) */
static int r_bits(ww_stream * iw, int num_bits, uint32_t * value) {
    off_t off, pos;
    int i, bit_buf, bit_val;
    if (num_bits == 0) return 1;
    if (num_bits > 32 || num_bits < 0 || iw->b_off + num_bits > iw->bufsize*8) goto fail;

    *value = 0; /* set all bits to 0 */
    off = iw->b_off / 8; /* byte offset */
    pos = iw->b_off % 8; /* bit sub-offset */
    for (i = 0; i < num_bits; i++) {
        bit_buf = (1U << pos) & 0xFF;   /* bit check for buf */
        bit_val = (1U << i);            /* bit to set in value */

        if (iw->buf[off] & bit_buf)     /* is bit in buf set? */
            *value |= bit_val;          /* set bit */

        pos++;                          /* new byte starts */
        if (pos%8 == 0) {
            pos = 0;
            off++;
        }
    }

    iw->b_off += num_bits;
    return 1;
fail:
    return 0;
}

/* Write bits (max 32) to buf and update the bit offset. Vorbis packs values in LSB order and byte by byte.
 * (ex. writing 1101011010 from b_off 2 we get 01101011 00001101 (value split, and 11 in the first byte skipped)*/
static int w_bits(ww_stream * ow, int num_bits, uint32_t value) {
    off_t off, pos;
    int i, bit_val, bit_buf;
    if (num_bits == 0) return 1;
    if (num_bits > 32 || num_bits < 0 || ow->b_off + num_bits > ow->bufsize*8) goto fail;


    off = ow->b_off / 8; /* byte offset */
    pos = ow->b_off % 8; /* bit sub-offset */
    for (i = 0; i < num_bits; i++) {
        bit_val = (1U << i);            /* bit check for value */
        bit_buf = (1U << pos) & 0xFF;   /* bit to set in buf */

        if (value & bit_val)            /* is bit in val set? */
            ow->buf[off] |= bit_buf;    /* set bit */
        else
            ow->buf[off] &= ~bit_buf;   /* unset bit */

        pos++;                          /* new byte starts */
        if (pos%8 == 0) {
            pos = 0;
            off++;
        }
    }

    ow->b_off += num_bits;
    return 1;
fail:
    return 0;
}

#endif
