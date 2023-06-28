#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util/endianness.h"


static off_t get_rws_string_size(off_t offset, STREAMFILE* sf);


typedef struct {
    int big_endian;

    uint32_t codec;
    int channels;
    int sample_rate;
    int interleave;
    int frame_size;

    off_t file_name_offset;

    int total_segments;
    int target_segment;
    off_t segment_offset;
    size_t segment_layers_size;
    off_t segment_name_offset;

    int total_layers;
    int target_layer;
    off_t layer_start;
    //size_t layer_size;
    off_t layer_name_offset;

    size_t file_size;
    size_t header_size;
    size_t data_size;
    off_t data_offset;

    size_t usable_size;
    size_t block_size;
    size_t block_layers_size;

    off_t coefs_offset;

    char readable_name[STREAM_NAME_SIZE];
} rws_header;


/* RWS - RenderWare Stream (from games using RenderWare Audio middleware) */
VGMSTREAM* init_vgmstream_rws(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, offset;
    size_t stream_size;
    int loop_flag;
    int i;
    int total_subsongs, target_subsong = sf->stream_index;
    rws_header rws = {0};
    read_u32_t read_u32;
    read_u16_t read_u16;


    if (read_u32le(0x00,sf) != 0x0000080d) /* audio file id */
        goto fail;
    rws.file_size = read_u32le(0x04, sf); /* audio file size */
    if (rws.file_size + 0x0c != get_streamfile_size(sf))
        goto fail;

    /* checks */
    if (!check_extensions(sf,"rws"))
        goto fail;

    /* Audio .RWS is made of file + header + data chunks (non-audio .RWS with other chunks exist).
     * Chunk format (LE): id, size, RW version, then data (version is repeated but same for all chunks).
     * Version is 16b main + 16b build (possibly shifted), no known differences between versions,
     * and can vary between files of a game. ex: 0c02, 1003, 1400 = 3.5, 1803 = 3.6, 1C02 = 3.7. */

    /* parse audio chunks */
    if (read_u32le(0x0c,sf) != 0x0000080e) /* header id */
        goto fail;
    rws.header_size = read_u32le(0x10, sf); /* header size */

    rws.data_offset = 0x0c + 0x0c + rws.header_size; /* usually 0x800 but not always */
    if (read_u32le(rws.data_offset + 0x00, sf) != 0x0000080f) /* data chunk id */
        goto fail;
    rws.data_size = read_u32le(rws.data_offset + 0x04, sf); /* data chunk size */
    if (rws.data_size+0x0c + rws.data_offset != get_streamfile_size(sf))
        goto fail;

    /* inside header chunk (many unknown fields are probably IDs/config/garbage,
     * as two files of the same size vary a lot) */
    offset = 0x0c + 0x0c;

    rws.big_endian = guess_endian32(offset + 0x00, sf); /* GC/Wii/X360 */
    read_u32 = rws.big_endian ? read_u32be : read_u32le;
    read_u16 = rws.big_endian ? read_u16be : read_u16le;

    /* base header */
    {
        /* 0x00: actual header size (less than chunk size) */
        /* 0x04/08/10: sizes of various sections? */
        /* 0x14/18: config? */
        /* 0x1c: null? */
        rws.total_segments = read_u32(offset + 0x20, sf);
        /* 0x24: config? */
        rws.total_layers   = read_u32(offset + 0x28, sf);
        /* 0x2c: config? */
        /* 0x30: 0x800? */
        /* 0x34: block_layers_size? */
        /* 0x38: data offset */
        /* 0x3c: 0? */
        /* 0x40-50: file uuid */
        offset += 0x50;
    }

    /* audio file name */
    {
        rws.file_name_offset = offset;
        offset += get_rws_string_size(offset, sf);
    }

    /* RWS data can be divided in two ways:
     * - "substreams" (layers): interleaved blocks, for fake multichannel L/R+C/S+LS/RS [Burnout 2 (GC/Xbox)]
     *   or song variations [Neighbours From Hell (Xbox/GC)]. Last layer may have padding to keep chunks aligned:
     *   ex.- 0x1700 data of substream_0 2ch, 0x1700 data + 0x200 pad of substream1 2ch, repeat until end
     * - "segments": cues/divisions within data, like intro+main/loop [[Max Payne 2 (PS2), Nana (PS2)]
     *   or voice1+2+..N, song1+2+..N [Madagascar (PS2), The Legend of Spyro: Dawn of the Dragon (X360)]
     *
     * As each layer is given sample rate/channel/codec/etc they are treated as full subsongs, though usually
     * all layers are the same. Segments are just divisions and can be played one after another, but are useful
     * to play as subsongs. Since both can exist at the same time (rarely) we make layers*segments=subsongs.
     * Ex.- subsong1=layer1 blocks in segment1, subsong2=layer2 blocks in segment1, subsong3=layer1 blocks in segment2, ...
     *
     * Segment1                                     Segment2
     * +-------------------------------------------+-----------------
     * |Layer1|Layer2|(pad)|...|Layer1|Layer2|(pad)|Layer1|Layer2|...
     * --------------------------------------------------------------
     */

    if (target_subsong == 0) target_subsong = 1;

    rws.target_layer = ((target_subsong-1) % rws.total_layers) + 1;
    rws.target_segment = ((target_subsong-1) / rws.total_layers) + 1;
    total_subsongs = rws.total_layers * rws.total_segments;

    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;


    /* segment info */
    for (i = 0; i < rws.total_segments; i++) {
        if (i+1 == rws.target_segment) {
            /* 0x00/04/0c: config? */
            /* others: ? */
            rws.segment_layers_size = read_u32(offset + 0x18, sf); /* sum of all including padding */
            rws.segment_offset = read_u32(offset + 0x1c, sf);
        }
        offset += 0x20;
    }

    /* usable layer sizes per segment */
    for (i = 0; i < (rws.total_segments * rws.total_layers); i++) {
        size_t usable_size = read_u32(offset, sf); /* without padding */
        /* size order: segment1 layer1 size, ..., segment1 layerN size, segment2 layer1 size, etc */
        if (i+1 == target_subsong) { /* order matches our subsong order */
            rws.usable_size = usable_size;
        }
        offset += 0x04;
    }

    /* segment uuids */
    {
        offset += 0x10 * rws.total_segments;
    }

    /* segment names */
    for (i = 0; i < rws.total_segments; i++) {
        if (i+1 == rws.target_segment) {
            rws.segment_name_offset = offset;
        }
        offset += get_rws_string_size(offset, sf);
    }

    /* layer info */
    for (i = 0; i < rws.total_layers; i++) { /* get block_sizes */
        if (i+1 == rws.target_layer) {
            /* 0x00/04: config? */
            /* 0x08: null? */
            /* 0x0c: related to samples per frame? (XBOX-IMA=07, PSX=1C, DSP=0E, PCM=01) */
          //block_size_pad  = read_u32(offset + 0x10, sf); /* with padding, can be different per layer */
            /* 0x14: config? */
            rws.interleave  = read_u16(offset + 0x18, sf); /* wrong values in Burnout 2 Xbox, otherwise correct */
            rws.frame_size  = read_u16(offset + 0x1a, sf); /* same */
            /* 0x1c: codec related? */
            rws.block_size  = read_u32(offset + 0x20, sf); /* without padding */
            rws.layer_start = read_u32(offset + 0x24, sf); /* skip data */
        }
        rws.block_layers_size += read_u32(offset + 0x10, sf); /* needed to skip during decode */
        offset += 0x28;
    }

    /* layer config */
    for (i = 0; i < rws.total_layers; i++) {
        uint32_t layer_codec = 0;
        if (i+1 == rws.target_layer) {
            rws.sample_rate     = read_u32(offset + 0x00, sf);
            /* 0x04: config? */
          //rws.layer_size  = read_u32(offset + 0x08, sf); /* same or close to usable size */
            /* 0x0c: bits per sample */
            rws.channels    =  read_u8(offset + 0x0d, sf);
            /* 0x0e: null or some value ? */
            /* 0x10/0x14: null? */
            /* 0x18: null or some size? */
            rws.codec       = read_u32(offset + 0x1c, sf); /* 128b uuid (32b-16b-16b-8b*8) but first 32b is enough */
        }
        layer_codec   = read_u32(offset + 0x1c, sf);
        offset += 0x2c;

        /* DSP has an extra field per layer */
        if (layer_codec == 0xF86215B0) {
            /* 0x00: approx num samples? */
            /* 0x04: approx size/loop related? (can be 0) */
            if (i+1 == rws.target_layer) {
                rws.coefs_offset = offset + 0x1c;
            }
            offset += 0x60;
        }
        offset += 0x04; /* padding/garbage */
    }

    /* layer uuids */
    {
        offset += 0x10 * rws.total_layers;
    }

    /* layer names */
    for (i = 0; i < rws.total_layers; i++) {
        if (i+1 == rws.target_layer) {
            rws.layer_name_offset = offset;
        }
        offset += get_rws_string_size(offset, sf);
    }

    /* rest is padding/garbage until chunk end (may contain strings and uninitialized memory) */
    // ...

    start_offset = rws.data_offset + 0x0c + (rws.segment_offset + rws.layer_start);
    stream_size = rws.usable_size;

    /* sometimes segment/layers go over file size in XBOX-IMA for no apparent reason, with usable_size bigger
     * than segment_layers_size yet data_size being correct (bug in RWS header? maybe stops decoding on file end) */
    {
        size_t expected_size = (rws.segment_layers_size / rws.block_layers_size) * (rws.block_size * rws.total_layers) / rws.total_layers;
        if (stream_size > expected_size) {
            VGM_LOG("RWS: readjusting wrong stream size %x vs expected %x\n", stream_size, expected_size);
            stream_size = expected_size;
        }
    }

    /* build readable name */
    {
        char base_name[STREAM_NAME_SIZE], file_name[STREAM_NAME_SIZE], segment_name[STREAM_NAME_SIZE], layer_name[STREAM_NAME_SIZE];

        get_streamfile_basename(sf, base_name, sizeof(base_name));
        /* null terminated */
        read_string(file_name,STREAM_NAME_SIZE, rws.file_name_offset, sf);
        read_string(segment_name,STREAM_NAME_SIZE, rws.segment_name_offset, sf);
        read_string(layer_name,STREAM_NAME_SIZE, rws.layer_name_offset, sf);

        /* some internal names aren't very interesting and are stuff like "SubStream" */
        if (strcmp(base_name, file_name) == 0) {
            if (rws.total_layers > 1)
                snprintf(rws.readable_name,STREAM_NAME_SIZE, "%s/%s", segment_name, layer_name);
            else
                snprintf(rws.readable_name,STREAM_NAME_SIZE, "%s", segment_name);
        }
        else {
            if (rws.total_layers > 1)
                snprintf(rws.readable_name,STREAM_NAME_SIZE, "%s/%s/%s", file_name, segment_name, layer_name);
            else
                snprintf(rws.readable_name,STREAM_NAME_SIZE, "%s/%s", file_name, segment_name);
        }
    }

    /* seemingly no actual looping supported (devs may fake it with segments) */
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(rws.channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RWS;
    vgmstream->sample_rate = rws.sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    strcpy(vgmstream->stream_name, rws.readable_name);

    vgmstream->layout_type = layout_blocked_rws;
    vgmstream->current_block_size = rws.block_size / vgmstream->channels;
    vgmstream->full_block_size = rws.block_layers_size;

    switch(rws.codec) {
        case 0xD01BD217: /* {D01BD217,3587,4EED,B9,D9,B8,E8,6E,A9,B9,95} PCM PC/X360/PS2 */
            /* D.i.R.T.: Origin of the Species (PC), The Legend of Spyro (X360), kill.switch (PS2) */
            if (rws.interleave == 0x02) { /* PC, X360 */
                vgmstream->coding_type = coding_PCM16_int;
                vgmstream->codec_endian = (rws.big_endian);
                vgmstream->interleave_block_size = rws.interleave; /* only to setup channels */
            }
            else { /* PS2 */
                vgmstream->coding_type = rws.big_endian ? coding_PCM16BE : coding_PCM16LE;
                vgmstream->interleave_block_size = rws.interleave; /* only to setup channels */
            }

            vgmstream->num_samples = pcm16_bytes_to_samples(stream_size, rws.channels);
            break;

        case 0xD9EA9798: /* {D9EA9798,BBBC,447B,96,B2,65,47,59,10,2E,16} PS-ADPCM PS2 */
            /* Silent Hill Origins (PS2), Ghost Rider (PS2), Max Payne 2 (PS2), Nana (PS2) */
            vgmstream->coding_type = coding_PSX;
            vgmstream->interleave_block_size = rws.block_size / 2;

            vgmstream->num_samples = ps_bytes_to_samples(stream_size, rws.channels);
            break;

        case 0xF86215B0: /* {F86215B0,31D5,4C29,BD,37,CD,BF,9B,D1,0C,53} DSP GC/Wii */
            /* Burnout 2 (GC), Alice in Wonderland (Wii) */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->interleave_block_size = rws.block_size / 2;

            /* get coefs (all channels share them; also seem fixed for all RWS) */
            dsp_read_coefs_be(vgmstream, sf, rws.coefs_offset, 0);

            vgmstream->num_samples = dsp_bytes_to_samples(stream_size, rws.channels);
            break;

        case 0xEF386593: /* {EF386593,B611,432D,95,7F,A7,1A,DE,44,22,7A} XBOX-IMA PC */
        case 0x632FA22B: /* {632FA22B,11DD,458F,AA,27,A5,C3,46,E9,79,0E} XBOX-IMA Xbox */
            /* Broken Sword 3 (PC), Jacked (PC/Xbox), Burnout 2 (Xbox) */
            vgmstream->coding_type = coding_XBOX_IMA; /* same data though different uuid */
            vgmstream->interleave_block_size = 0;

            vgmstream->num_samples = xbox_ima_bytes_to_samples(stream_size, rws.channels);
            break;

        default:
            VGM_LOG("RWS: unknown codec 0x%08x\n", rws.codec);
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* rws-strings are null-terminated then padded to 0x10 (weirdly enough the padding contains garbage) */
static off_t get_rws_string_size(off_t offset, STREAMFILE* sf) {
    int i;
    for (i = 0; i < 255; i++) { /* arbitrary max */
        if (read_u8(offset+i, sf) == 0) { /* null terminator */
            return i + (0x10 - (i % 0x10)); /* size is padded */
        }
    }

    return 0;
}
