#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/endianness.h"
#include "../util/layout_utils.h"
#include "audiopkg_streamfile.h"

// streams are defined as "hot" (memory) + "warm" (memory + stream?) + "cold" (stream) sample 'temperatures'.
// BGM is typically set to either while SFX banks may include hot+cold types.
#define MAX_TEMPERATURES 3

#define MAX_CHANNELS 2
#define MAX_NAME 256

typedef enum { CODEC_NONE, CODEC_PSX, CODEC_XBOX, CODEC_XBOX_PC, CODEC_DSP, CODEC_PCM, CODEC_MP3 } audiopkg_codec_t;
typedef enum { PT_NONE, PT_PS2, PT_XBOX, PT_GC, PT_PC } audiopkg_platform_t;

typedef struct {
    audiopkg_platform_t platform;
    int version;
    bool big_endian;

    int total_subsongs;
    int target_subsong;

    char name[MAX_NAME];
    int name_count;

    // package info
    int descriptors; // total cues
    int identifiers; // total strings
    uint32_t descriptors_size;
    uint32_t strings_size;
    uint32_t lipsyncs_size;
    uint32_t musicdata_size;
    uint32_t breakpoints_size;
    int sample_headers[MAX_TEMPERATURES];   // actual stream headers
    int sample_indices[MAX_TEMPERATURES];   // pointers to stream headers
    int sample_sizes[MAX_TEMPERATURES];     // size per headers (in practice 0x28 or 0x56 for DSP)

    // derived package info
    uint32_t strings_offset;  // c-strings, referenced by identifiers 
    uint32_t lipsyncs_offset; // big table
    uint32_t breakpoints_offset; // strings + data, probably identifiers to be triggered; some strings start with an u8 ID
    uint32_t musicdata_offset; // seeks? has N mini tables of count + N time position(?) floats
    uint32_t identifiers_index_offset; // identifier N to descriptor index
    uint32_t descriptors_index_offset; // points to descriptor (cue)
    uint32_t descriptors_offset; // variable-sized cues

    uint32_t sample_indices_offset; // index to header
    uint32_t sample_headers_offset; // stream info
    int sample_indices_count; // total indices
    int sample_indices_extras; // extra indices for stereo handling
    int sample_headers_count; // total headers

    // calculated current stream info
    int target_index;
    int target_temperature;
    uint32_t head_offset;
    uint32_t head_size;

    // current stream header
    audiopkg_codec_t codec;
    int type;
    int channels;
    int sample_rate;
    int32_t num_samples;
    int32_t loop_start;
    int32_t loop_end;
    uint32_t stream_offset;
    uint32_t stream_size;
    uint32_t coefs_offset;
    uint32_t coefs_spacing;
    uint32_t stream_offset2;
    uint32_t stream_size2;

    bool is_interleaved;
    bool loop_flag;
} audiopkg_header_t;

static bool parse_header(audiopkg_header_t* h, STREAMFILE* sf);
static VGMSTREAM* init_vgmstream_audiopkg_main(STREAMFILE* sf, audiopkg_header_t* h);


/* .AUDIOPKG - from Inevitable / Midway Austin games [Area 51 (multi), The Hobbit (multi)] */
VGMSTREAM* init_vgmstream_audiopkg(STREAMFILE* sf) {

    /* checks */
    if (!(is_id32be(0x00,sf, "v1.5") || is_id32be(0x00,sf, "v1.6") || is_id32be(0x00,sf, "v1.7") || is_id32be(0x00,sf, "v1.8")))
        return NULL;

    if (!check_extensions(sf,"audiopkg"))
        return NULL;

    audiopkg_header_t h = {0};
    if (!parse_header(&h, sf))
        return NULL;

    return init_vgmstream_audiopkg_main(sf, &h);
}

static VGMSTREAM* init_vgmstream_audiopkg_main(STREAMFILE* sf, audiopkg_header_t* h) {
    VGMSTREAM* vgmstream = NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(h->channels, h->loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_AUDIOPKG;
    vgmstream->sample_rate = h->sample_rate;
    vgmstream->num_samples = h->num_samples;
    vgmstream->loop_start_sample = h->loop_start;
    vgmstream->loop_end_sample = h->loop_end;
    vgmstream->stream_size = h->stream_size;
    vgmstream->num_streams = h->total_subsongs;

    if (h->name[0] != '\0') {
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s", h->name);
        vgmstream->stream_name[STREAM_NAME_SIZE-1] = '\0';
    }

    switch(h->codec) {
        case CODEC_PCM: // Area 51 (PC)-sfx
            if (h->big_endian){
                VGM_LOG("AUDIOPKG: unsupported big endian found\n");
                goto fail;
            }
            vgmstream->coding_type = h->big_endian ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case CODEC_PSX: // The Hobbit (PS2), Area 51 (PS2)
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x8000;
            break;

        case CODEC_XBOX_PC: // The Hobbit (PC)
            vgmstream->coding_type = coding_XBOX_IMA_mono;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x9000;
            break;

        case CODEC_XBOX: // Area 51 (Xbox), The Hobbit (Xbox)
            vgmstream->coding_type = coding_XBOX_IMA_mono;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x8000;
            break;

        case CODEC_DSP: // The Hobbit (GC)-sfx
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x8000;

            dsp_read_coefs(vgmstream, sf, h->coefs_offset + 0x00, h->coefs_spacing, h->big_endian);
            dsp_read_hist (vgmstream, sf, h->coefs_offset + 0x24, h->coefs_spacing, h->big_endian);
            break;

#ifdef VGM_USE_MPEG
        case CODEC_MP3: { // Area 51 (PC)-music, The Hobbit (GC)-music/voices
            // not seen, interleaved uses regular stereo MP3
            if (!h->is_interleaved && h->channels > 1) {
                VGM_LOG("AUDIOPKG: found non-interleaved MPEG stereo\n");
                goto fail;
            }

            // some The Hobbit (GC) file have issues, probably skipped/synced
            // (engine seems to use Miles Sound System for MPEG)

            // garbage at frame start, seems consistent 
            if ((read_u16be(h->stream_offset, sf) & 0xFFFE0) != 0xFFE0 
                && (read_u16be(h->stream_offset + 0x61, sf) & 0xFFE0) == 0xFFE0) {
                h->stream_offset += 0x61;
                h->stream_size -= 0x61;
            }

            // single a blank frame
            if (read_u32be(h->stream_offset, sf) == 0) {
                h->stream_offset += 0x1B0;
                h->stream_size -= 0x1B0;
            }

            mpeg_custom_config cfg = {0};
            cfg.skip_samples = 0; //?
            cfg.data_size = h->stream_size;

            vgmstream->layout_type = layout_none;
            vgmstream->codec_data = init_mpeg_custom(sf, h->stream_offset, &vgmstream->coding_type, h->channels, MPEG_STANDARD, &cfg);
            if (!vgmstream->codec_data) goto fail;

            vgmstream->num_samples = h->num_samples;
            break;
        }
#endif

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, h->stream_offset))
        goto fail;


    // needed for memory 1ch XBOX_mono
    if (!h->is_interleaved) {
        vgmstream->layout_type = layout_none;
        vgmstream->interleave_block_size = 0x00;
    }

    if (!h->is_interleaved && h->channels > 1) {
        vgmstream->ch[0].channel_start_offset = h->stream_offset;
        vgmstream->ch[1].channel_start_offset = h->stream_offset2;
        vgmstream->ch[0].offset = h->stream_offset;
        vgmstream->ch[1].offset = h->stream_offset2;
    }

    // TODO: handle streamfiles in a cleaner way
    // streamed XBOX_mono 0x24 frames can't fit exactly into 0x8000 interleave, and (instead of using 0x9000 like PC) blocks
    // work like this: [B1-0x8000-ch1][B1-0x8000-ch2][B2-0x7FF0-ch1 + 0x10 padding][B2-0x7FF0-ch2 + 0x10 padding] xN
    // Last frame in B1 is partially cut (since it doesn't fix that block), meaning we need a custom streamfile to read.
    if (h->codec == CODEC_XBOX && h->target_temperature == 2) {
        vgmstream->layout_type = layout_none;
        vgmstream->interleave_block_size = 0x00;

        for (int ch = 0; ch < h->channels; ch++) {
            vgmstream->ch[ch].offset = vgmstream->ch[ch].channel_start_offset = 0;
            close_streamfile(vgmstream->ch[ch].streamfile);

            vgmstream->ch[ch].streamfile = setup_audiopkg_streamfile(sf, h->stream_offset, h->stream_size, ch, h->channels);
            if (!vgmstream->ch[ch].streamfile) goto fail;
        }
    }


    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


// read base header
static bool parse_package_identifier(audiopkg_header_t* h, STREAMFILE* sf) {
    uint32_t offset = 0x00;

    // 00 version string
    // 10 platform string
    // 20 build user string
    // 30 build date string

    // The Hobbit: v1.5 PS2/Xbox/GC, v1.6* PC (actually 1.5)
    // Area 51 (beta): v1.6 Xbox, v1.7 PS2
    // Area 51 (final): v1.7 PS2/Xbox/GC, v1.8* PC (actually 1.7)
    h->version = read_u8(offset + 0x03, sf) - 0x30;
    if (h->version < 5 || h->version > 8)
        return false;

    uint32_t platform = read_u32be(offset + 0x10, sf);
    if (platform == get_id32be("Wind")) { // Windows
        h->platform = PT_PC;
    }
    else if (platform == get_id32be("Xbox")) {
        h->platform = PT_XBOX;
    }
    else if (platform == get_id32be("Play")) { // PlayStation II
        h->platform = PT_PS2;
    }
    else if (platform == get_id32be("Game")) { // Gamecube
        h->platform = PT_GC;
    }
    else {
        return false;
    }

    // simplify
    if (h->platform == PT_PC && h->version == 6) {
        h->version = 5;
    }

    h->big_endian = (h->platform == PT_GC);

    return true;
}

// read main header
static bool parse_package_header(audiopkg_header_t* h, STREAMFILE* sf) {
    read_s32_t read_s32 = h->big_endian ? read_s32be : read_s32le;
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;

    uint32_t offset = 0x40;

    /* global bank config (volumes, pan, pitch, etc), changes a bit between versions */
    if (h->version == 5)
        offset += 0x60;
    else if (h->version == 6)
        offset += 0x70;
    else if (h->version == 7 || h->version == 8)
        offset += 0x80;
    else
        return false;

    //;VGM_LOG("AUDIOPKG: table at %x\n", offset);

    /* table values */
    h->descriptors          = read_s32(offset + 0x00, sf);
    h->identifiers          = read_s32(offset + 0x04, sf);
    h->descriptors_size     = read_u32(offset + 0x08, sf);
    h->strings_size         = read_u32(offset + 0x0c, sf);
    h->lipsyncs_size        = read_u32(offset + 0x10, sf);
    h->musicdata_size       = read_u32(offset + 0x14, sf);
    h->breakpoints_size     = read_u32(offset + 0x18, sf);
    h->sample_headers[0]    = read_s32(offset + 0x1c, sf);
    h->sample_headers[1]    = read_s32(offset + 0x20, sf);
    h->sample_headers[2]    = read_s32(offset + 0x24, sf);
    h->sample_indices[0]    = read_s32(offset + 0x28, sf);
    h->sample_indices[1]    = read_s32(offset + 0x2c, sf);
    h->sample_indices[2]    = read_s32(offset + 0x30, sf);
    // 34: compression types x3, useless since it's also defined per stream
    h->sample_sizes[0]      = read_s32(offset + 0x40, sf);
    h->sample_sizes[1]      = read_s32(offset + 0x44, sf);
    h->sample_sizes[2]      = read_s32(offset + 0x48, sf);

    offset += 0x4c;
    if (h->version >= 6) {
        // 4c: V1.6: size? v1.7: always 1?, v1.8: always 0?
        offset += 0x04;
    }

    if (h->sample_indices[1] != 0 || h->sample_headers[1] != 0) {
        vgm_logi("AUDIOPKG: 'warm' samples found\n");
        return false; //not seen (not implemented?)
    }

    if ((h->lipsyncs_size && h->musicdata_size) || (h->lipsyncs_size && h->breakpoints_size)) {
        vgm_logi("AUDIOPKG: lipsyncs and musicdata/breakpoints found\n");
        return false; //unknown order
    }


    /* tables */
    h->strings_offset = offset;
    offset += h->strings_size;

    h->lipsyncs_offset = offset;
    offset += h->lipsyncs_size;

    h->breakpoints_offset = offset;
    offset += h->breakpoints_size;

    h->musicdata_offset = offset;
    offset += h->musicdata_size;

    h->identifiers_index_offset = offset;
    offset += (h->identifiers * 0x08);

    h->descriptors_index_offset = offset;
    offset += (h->descriptors * 0x04);

    h->descriptors_offset = offset;
    offset += h->descriptors_size;

    // u16 index N in sample-header table, N per hot + warm + cold,
    // plus 1 index pointing to end for each defined (for stereo handling)
    h->sample_indices_count = h->sample_indices[0] + h->sample_indices[1] + h->sample_indices[2];
    h->sample_indices_extras = 0;
    for (int i = 0; i < 3; i++) {
        if (!h->sample_indices[i])
            continue;
        h->sample_indices_extras++;
    }
    h->sample_indices_offset = offset; 
    uint32_t sample_indices_size = (h->sample_indices_count + h->sample_indices_extras) * 0x02;
    offset += sample_indices_size;

    h->sample_headers_count = h->sample_headers[0] + h->sample_headers[1] + h->sample_headers[2];
    h->sample_headers_offset = offset;
    uint32_t sample_headers_size = 0;
    for (int i = 0; i < 3; i++) {
        sample_headers_size += h->sample_headers[i] * h->sample_sizes[i];
    }
    offset += sample_headers_size;

    // after xN sample headers is variable padding then data start

#if 0
    VGM_LOG("AUDIOPKG: data:\n");
    VGM_LOG("  descriptors=%i\n", h->descriptors);
    VGM_LOG("  identifiers=%i\n", h->identifiers);
    VGM_LOG("  strings_offset=%x\n", h->strings_offset);
    VGM_LOG("  lipsyncs_offset=%x\n", h->lipsyncs_offset);
    VGM_LOG("  breakpoints_offset=%x\n", h->breakpoints_offset);
    VGM_LOG("  musicdata_offset=%x\n", h->musicdata_offset);
    VGM_LOG("  identifiers_index_offset=%x\n", h->identifiers_index_offset);
    VGM_LOG("  descriptors_index_offset=%x\n", h->descriptors_index_offset);
    VGM_LOG("  descriptors_offset=%x + %x\n", h->descriptors_offset, h->descriptors_size);
    VGM_LOG("  sample_indices_offset=%x + %x (%i + %i + %i)\n", h->sample_indices_offset, sample_indices_size, h->sample_indices[0], h->sample_indices[1], h->sample_indices[2]);
    VGM_LOG("  sample_headers_offset=%x + %x (%i + %i + %i)\n", h->sample_headers_offset, sample_headers_size, h->sample_headers[0], h->sample_headers[1], h->sample_headers[2]);
#endif
    return true;
}

// calculate subsong info
static bool parse_sample_indices(audiopkg_header_t* h, STREAMFILE* sf) {
    read_u16_t read_u16 = h->big_endian ? read_u16be : read_u16le;

    if (h->sample_indices_count > h->sample_headers_count) {
        vgm_logi("AUDIOPKG: unexpected count\n");
        return false;
    }

    // indices point to  headers, but for stereo files there is 1 index but 2 headers,
    // so sample_indices_counts are total subsongs rather than sample_headers_count.
    h->total_subsongs = h->sample_indices_count;

    if (h->total_subsongs <= 0) {
        vgm_logi("AUDIOPKG: bank has no subsongs (ignore)\n");
        return false;
    }
    if (h->target_subsong < 1 || h->target_subsong > h->total_subsongs)
        return false;

    // map target subsong to hot/warm/cold index + offsets
    int entries_left = h->target_subsong - 1;
    uint32_t target_offset = h->sample_indices_offset;
    for (int i = 0; i < 3; i++) {
        if (!h->sample_indices[i])
            continue;

        if (entries_left >= h->sample_indices[i]) {
            // not in this section
            target_offset += (h->sample_indices[i] + 1) * 0x02;
            entries_left -= h->sample_indices[i];
            continue;
        }

        target_offset += entries_left * 0x02;

        h->target_temperature = i;
        h->target_index = entries_left;
        break;
    }

    // Stereo songs are detected by checking current index vs next index (really).
    // There is always +1 extra index to account for this calculation.
    int header_index0 = read_u16(target_offset + 0x00, sf);
    int header_index1 = read_u16(target_offset + 0x02, sf);

    h->channels = (header_index1 - header_index0);
    if (h->channels < 1 || h->channels > MAX_CHANNELS) {
        VGM_LOG("AUDIOPKG: wrong channels %i (target %x, temp=%i, index=%i)\n", h->channels, target_offset, h->target_temperature, h->target_index);
        return false;
    }

    h->head_size = h->sample_sizes[h->target_temperature];

    h->head_offset = h->sample_headers_offset;
    for (int i = 0; i < 3; i++) {
        if (!h->sample_headers[i])
            continue;

        if (i < h->target_temperature) {
            // not in this section
            h->head_offset += h->sample_headers[i] * h->sample_sizes[i];
            continue;
        }

        h->head_offset += header_index0 * h->sample_sizes[i];
        break;
    }

#if 0
    VGM_LOG("AUDIOPKG: subsong:\n");
    VGM_LOG("  subsongs=%i / %i\n", h->target_subsong, h->total_subsongs);
    VGM_LOG("  target offset=%x\n", target_offset);
    VGM_LOG("  header offset=%x + %x\n", h->head_offset, h->head_size);
    VGM_LOG("  channels=%i\n", h->channels);
#endif

    return true;
}

// read subsong header
static bool parse_sample_header(audiopkg_header_t* h, STREAMFILE* sf) {
    read_s32_t read_s32 = h->big_endian ? read_s32be : read_s32le;
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;

    h->type = -1;

    uint32_t offset = h->head_offset;
    // 00: memory offset? (null)
    h->stream_offset    = read_u32(offset + 0x04, sf); // absolute
    h->stream_size      = read_u32(offset + 0x08, sf);
    // 0c: lipsync offset? (-1 if not set)
    // 10: breakpoint offset (-1 if not set)
    h->type             = read_s32(offset + 0x14, sf);
    h->num_samples      = read_s32(offset + 0x18, sf);
    h->sample_rate      = read_s32(offset + 0x1c, sf);
    h->loop_start       = read_s32(offset + 0x20, sf);
    h->loop_end         = read_s32(offset + 0x24, sf);
    h->coefs_offset     = offset + 0x28; // DSP only
    h->coefs_spacing    = h->head_size;

    h->loop_flag = (h->loop_end > 0);

    // map codec
    switch(h->type) {
        case 0x00:
            if (h->platform == PT_PS2) {
                h->codec = CODEC_PSX;
            }
            if (h->platform == PT_GC) {
                h->codec = CODEC_DSP;
            }
            if (h->platform == PT_XBOX) {
                h->codec = CODEC_XBOX;
            }
            if (h->platform == PT_PC) {
                h->codec = CODEC_XBOX_PC;
            }
            break;

        case 0x01:
            h->codec = CODEC_PCM;
            break;

        case 0x02:
            h->codec = CODEC_MP3;
            break;

        default:
            VGM_LOG("AUDIOPKG: unknown codec\n");
            return false;
    }

    // stereo files have 2 sample headers; if both point to the same stream it's interleaved, otherwise dual mono
    if (h->channels == 2) {
        offset += h->head_size;
        h->stream_offset2   = read_u32(offset + 0x04, sf);
        h->stream_size2     = read_u32(offset + 0x08, sf);

        h->is_interleaved = h->stream_offset == h->stream_offset2;
    }

#if 0
    VGM_LOG("AUDIOPKG: header %x\n", h->head_offset);
    VGM_LOG("  stream=%x / %x\n", h->stream_offset, h->stream_size);
    VGM_LOG("  stream2=%x / %x\n", h->stream_offset2, h->stream_size2);
    VGM_LOG("  srate=%i\n", h->sample_rate);
    VGM_LOG("  samples=%i\n", h->num_samples);
    VGM_LOG("  type=%i / interleaved=%i\n", h->type, h->is_interleaved);
#endif

    return true;
}


// descriptor have a TLV-like header + flags, then optional size, then payload depending on type
// which is usually simple list pointing to sample indices
static bool parse_names_descriptor(audiopkg_header_t* h, STREAMFILE* sf, uint32_t offset, int depth) {
    read_u16_t read_u16 = h->big_endian ? read_u16be : read_u16le;
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;

    if (depth > 1) {
        VGM_LOG("AUDIOPKG: recursion found\n");
        return false;
    }

    uint16_t descriptor_header = read_u16(offset, sf);
    // 02: flags (volume, pan, etc)

    uint16_t descriptor_type    = (descriptor_header >> 14) & 0x03;
    uint16_t descriptor_params  = (descriptor_header >> 13) & 0x01;
    //uint16_t descriptor_value   = (descriptor_header >> 0) & 0x1FFF; //always 0?
    //VGM_LOG("AUDIOPKG: descriptor: type=%x, param=%x, value=%x at %x\n", descriptor_type, descriptor_params, descriptor_value, offset);

    offset += 0x04;
    if (descriptor_params) {
        uint16_t params_size = read_u16(offset, sf);
        offset += params_size;
    }

    int items = 0;
    switch(descriptor_type) {
        case 0x00: // simple cue
            items = 1;
            break;

        case 0x01: // complex cue
            items = read_u16(offset + 0x00, sf);
            offset += 0x02;
            break;

        case 0x02: // random list
            items = read_u16(offset + 0x00, sf);
            offset += 0x02;
            offset += 0x08; // up to 64 bits to make a random list
            break;

        case 0x03: // weighted list
            items = read_u16(offset + 0x00, sf);
            offset += 0x02;
            offset += 0x02 * items; // weights
            break;

        default:
            break;
    }

    //;VGM_LOG("AUDIOPKG:   parse %i items at %x (type=%i)\n", items, offset, descriptor_type);

    // parse index list
    for (int i = 0; i < items; i++) {
        if (descriptor_type == 0x01) {
            offset += 0x02; //time?
        }
        uint16_t index_header = read_u16(offset, sf);
        // 02: flags? (unused?)

        uint16_t index_type     = (index_header >> 14) & 0x03;
        uint16_t index_params   = (index_header >> 13) & 0x01;
        uint16_t index_value    = (index_header >> 0) & 0x1FFF;
        //;VGM_LOG("AUDIOPKG:    index: type=%x, param=%x, value=%x at %x\n", index_type, index_params, index_value, offset);

        // index points to another descriptor
        if (index_type == 0x03) {
            uint32_t descriptor_index_offset = h->descriptors_index_offset + 0x04 * index_value;
            uint32_t descriptor_offset = read_u32(descriptor_index_offset, sf);
            descriptor_offset += h->descriptors_offset;

            return parse_names_descriptor(h, sf, descriptor_offset, depth + 1);
        }

        // index points to sample (0=hot, 1=warm, 2=cold)
        int index_temperature = index_type;
        if (index_temperature == h->target_temperature && index_value == h->target_index) {
            return true;
        }

        // skip to next index
        offset += 0x04;
        if (index_params) { // not seen but happens in theory 
            uint16_t params_size = read_u16(offset + 0x00, sf);
            offset += params_size;
        }
    }

    return false;
}

//todo safeops, avoid recalc lens 
static void v_strcat(char* dst, int dst_max, const char* src) {
    int dst_len = strlen(dst);
    int src_len = strlen(dst);
    if (dst_len + src_len > dst_max - 1)
        return;
    strcat(dst, src);
}
#if 0
static void v_strcpy(char* dst, int dst_max, const char* src) {
    int src_len = strlen(dst);
    if (src_len > dst_max - 1)
        return;
    strcpy(dst, src);
}
#endif

// Assign names based on identifiers pointing to our stream.
// In rare cases some streams don't have assigned names, probably an user mistake or just unused
// (ex. The Hobbit's SFX_GUI.AUDIOPKG: LOCKTIMER_TICK + LOCKTIMER_TOCK both point to #20, but the latter should be #21)
static bool parse_names(audiopkg_header_t* h, STREAMFILE* sf) {
    read_u16_t read_u16 = h->big_endian ? read_u16be : read_u16le;
    read_u32_t read_u32 = h->big_endian ? read_u32be : read_u32le;

    uint32_t offset = h->identifiers_index_offset;
    char identifier[0x100];

    for (int i = 0; i < h->identifiers; i++) {
        // identifier index table
        uint16_t string_offset      = read_u16(offset + 0x00, sf) + h->strings_offset;
        uint16_t descriptor_index   = read_u16(offset + 0x02, sf);
        // 04: reserved (set to some value after load)

        // descriptor index table
        uint32_t descriptor_index_offset = h->descriptors_index_offset + 0x04 * descriptor_index;
        uint32_t descriptor_offset = read_u32(descriptor_index_offset, sf);

        uint16_t index_type = descriptor_offset >> 16; //unsure
        descriptor_offset += h->descriptors_offset;

        if (index_type != 0) {
            VGM_LOG("AUDIOPKG: bad index=%i in descriptor %i\n", index_type, descriptor_index);
            return false;
        }

        //read_string(identifier, sizeof(identifier), string_offset, sf);
        //;VGM_LOG("AUDIOPKG: name=%s > %i\n", identifier, descriptor_index);

        bool string_used = parse_names_descriptor(h, sf, descriptor_offset, 0);
        if (string_used) {
            if (h->name_count)
                v_strcat(h->name, sizeof(h->name), "; ");

            read_string(identifier, sizeof(identifier), string_offset, sf);
            v_strcat(h->name, sizeof(h->name), identifier);
            h->name_count++;
        }

        offset += 0x08;
    }

    return true;
}

// AUDIOPKG info:
// - defines a basic "package identifier" and  "package header" with common parameters + tables
// - tables define N "identifiers" (cue names)
// - identifiers point to 1 "descriptor" (cue)
// - each descriptor points to N "sample indices"
// - each sample index points to 1 or 2 "sample headers"
// - sample headers have actual stream info
// To play anything devs would call some identifier by name, event style.
// Streams are divided into memory ("hot sample") and stream ("cold sample") data, but both work the same.
static bool parse_header(audiopkg_header_t* h, STREAMFILE* sf) {
    h->target_subsong = sf->stream_index;
    if (h->target_subsong == 0)
        h->target_subsong = 1;

    if (!parse_package_identifier(h, sf))
        return false;
    if (!parse_package_header(h, sf))
        return false;
    if (!parse_sample_indices(h, sf))
        return false;
    if (!parse_sample_header(h, sf))
        return false;
    if (!parse_names(h, sf))
        return false;

    return true;
}
