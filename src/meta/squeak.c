#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/endianness.h"
#include "../util/layout_utils.h"

#define SQUEAK_MAX_CHANNELS  6  /* seen 3 in some voices */
typedef enum { PCM16LE, PCM16BE, PCM8, DSP, PSX, MSIMA, IMA, XMA2, VORBIS, SPEEX } squeak_type_t;

typedef struct {
    squeak_type_t type;
    int version;

    int channels;
    int codec;
    int sample_rate;
    uint32_t interleave;

    uint32_t extb_offset;
    uint32_t name_offset;

    int32_t num_samples;
    int32_t loop_start;
    int32_t loop_end;

    uint32_t data_offset;
    uint32_t coef_offset;
    uint32_t coef_spacing;

    uint32_t data_offsets[SQUEAK_MAX_CHANNELS];
    uint32_t coef_offsets[SQUEAK_MAX_CHANNELS];
    uint32_t data_size;

    bool big_endian;
    bool external_info;
    bool external_data;
    bool stream;
} squeak_header_t;

static VGMSTREAM* init_vgmstream_squeak_common(STREAMFILE* sf, squeak_header_t* h);


/* SqueakStream - from Torus games (name/engine as identified in .hnk subdirs) */
VGMSTREAM* init_vgmstream_squeakstream(STREAMFILE* sf) {
    squeak_header_t h = {0};
    bool is_old = false;


    /* checks */
    if (is_id32be(0x00,sf, "RAWI") || is_id32be(0x00,sf, "VORB") || is_id32be(0x00,sf, "SPEX")) {
        h.big_endian = false; /* VORB/SPEX only use vorbis/speex but no apparent diffs */
    }
    else if (is_id32be(0x00,sf, "IWAR")) {
        h.big_endian = true; /* Wii/PS3/X360 */
    }
    else {
        /* no header id in early version so test codec in dumb endian */
        if ((read_u32le(0x00,sf) & 0x00FFFFFF) > 9 || (read_u32be(0x00,sf) & 0x00FFFFFF) > 9)
            return NULL;
        is_old = true;
        h.big_endian = guess_endian32(0x04, sf);
    }

    if (get_streamfile_size(sf) > 0x1000) /* arbitrary max */
        return NULL;

    /* (extensionless): no known extension */
    if (!check_extensions(sf,""))
        return NULL;

    read_s32_t read_s32 = h.big_endian ? read_s32be : read_s32le;
    read_u32_t read_u32 = h.big_endian ? read_u32be : read_u32le;

    /* base header (with extra checks for old version since format is a bit simple) */
    if (!is_old) {
        h.version       =  read_u8(0x04,sf);
        if (h.version != 0x01) return NULL;
        h.codec         =  read_u8(0x05,sf);
        h.channels      =  read_u8(0x06,sf);
        /* 07: null */
        h.num_samples   = read_s32(0x08, sf);
        h.sample_rate   = read_s32(0x0c, sf);
        h.loop_start    = read_s32(0x10, sf);
        h.loop_end      = read_s32(0x14, sf);
        h.extb_offset = read_u32le(0x18, sf); /* LE! */
        h.name_offset = read_u32le(0x1c, sf);
        /* 20: null, unknown values (sometimes floats) */
        h.interleave    = read_u32(0x38, sf);
        
        h.data_offset = 0; /* implicit... */

        /* XX: extra values (may depend on codec/channels) */
        /* XX: DSP coefs / fmt headers (optional) */
        /* XX: extra table with offset to fmt headers / DSP coefs /etc (per channel) */
        /* XX: asset name */
    }
    else {
        h.codec         = read_s32(0x00,sf);
        if (h.codec > 0x09) return NULL;
        h.channels      = read_s32(0x04,sf);
        if (h.channels > SQUEAK_MAX_CHANNELS) return NULL;
        h.interleave    = read_u32(0x08, sf);
        if (h.interleave > 0xFFFFFF) return NULL;
        h.loop_start    = read_s32(0x0c, sf);
        h.loop_end      = read_s32(0x10, sf);
        h.num_samples   = read_s32(0x14, sf);
        if (h.loop_start > h.loop_end || h.loop_end > h.num_samples) return NULL;
        /* 18: float/value */
        /* 1c: float/value */
        /* 20: cue table entries (optional) */
        /* 22: unknown */
        /* 24: cues offset */
        /* 26: cues flags */
        h.extb_offset = read_u32le(0x28, sf); /* LE! */
        h.name_offset = read_u32le(0x2c, sf);
        h.data_offset   = read_u32(0x30, sf); /* PS2 uses a few big .raw rather than separate per header */

        /* XX: DSP coefs / fmt headers (optional) */
        /* XX: cue table (00=null + 04=sample start per entry) */
        /* XX: extra table (00=null + 00=sample rate, 04=samples, per channel) */
        /* XX: asset name */

        //sample_rate = ...; // read later after opening external info

        /* not ideal but... */
        if (h.data_offset && h.codec == 0x03) {
            h.data_size = (h.num_samples / 28) * 0x10 * h.channels;
        }
    }


    /* Wii streams uses a separate info file, check external flags */
    /* (possibly every section may be separate or not but only seen all at once) */
    h.stream = true;
    h.external_info = (h.name_offset & 0xF0000000);
    h.external_data = true;
    h.name_offset = h.name_offset & 0x0FFFFFFF;
    h.extb_offset = h.extb_offset & 0x0FFFFFFF;
    if (h.extb_offset > h.name_offset) return NULL;

    switch(h.codec) {
        case 0x00: h.type = DSP; break;         /* Turbo Super Stunt Squad (Wii/3DS), Penguins of Madagascar (Wii/U/3DS) */
        case 0x01: h.type = PCM16LE; break;     /* Falling Skies The Game (PC) */
        case 0x02: h.type = h.big_endian ? PCM16BE : PCM16LE; break;  /* Falling Skies The Game (X360)-be, Scooby Doo and the Spooky Swamp (PC)-le */
        case 0x03: h.type = PSX; break;         /* How to Train Your Dragon 2 (PS3), Falling Skies The Game (PS3) */
        case 0x04: h.type = MSIMA; break;       /* Barbie Dreamhouse Party (DS) */
        case 0x05: h.type = PCM8; break;        /* Scooby Doo and the Spooky Swamp (DS), Scooby Doo! First Frights (DS) */
        case 0x07: h.type = SPEEX; break;       /* Scooby Doo and the Spooky Swamp (PC) */
        case 0x08: h.type = VORBIS; break;      /* Scooby Doo and the Spooky Swamp (PC) */
        case 0x09: h.type = MSIMA; break;       /* Turbo Super Stunt Squad (DS) */
        default:
            VGM_LOG("SqueakStream: unknown codec %x\n", h.codec);
            return NULL;
    }

    return init_vgmstream_squeak_common(sf, &h);
}


/* SqueakSample - from Torus games (name/engine as identified in .hnk subdirs) */
VGMSTREAM* init_vgmstream_squeaksample(STREAMFILE* sf) {
    squeak_header_t h = {0};


    /* checks */
    if (read_u32le(0x00,sf) != 0x20 && read_u32le(0x00,sf) != 0x1c) /* even on BE */
        return NULL;
    //if (get_streamfile_size(sf) > 0x1000) /* not correct for non-external files */
    //    return NULL;

    /* (extensionless): no known extension */
    if (!check_extensions(sf,""))
        return NULL;

    h.big_endian = guess_endian32(0x04, sf);
    read_s32_t read_s32 = h.big_endian ? read_s32be : read_s32le;

    /* base header (with extra checks since format is a bit simple) */
    uint32_t offset = read_u32le(0x00, sf); /* old versions use 0x1c, new 0x20, but otherwise don't look different */

    h.channels      = read_s32(0x04,sf);
    if (h.channels > SQUEAK_MAX_CHANNELS) return NULL;
    /* 04: float/value */
    /* 0c: float/value */
    /* 14: value? */
    /* 18: value? (new) / 1 (old) */ 
    /* 1c: 1? (new) / none (old) */

    /* sample header per channel (separate fields but assumes all are repeated except offsets) */
    h.num_samples   = read_s32(offset + 0x00,sf);
    h.data_offset = read_u32le(offset + 0x04,sf);
    h.loop_start    = read_s32(offset + 0x08,sf);
    h.loop_end      = read_s32(offset + 0x0c,sf);
    if (h.loop_start > h.loop_end || h.loop_end > h.num_samples) return NULL;
    h.codec         = read_s32(offset + 0x10,sf);
    h.sample_rate   = read_s32(offset + 0x14,sf);
    if (h.sample_rate > 48000 || h.sample_rate < 0) return NULL;

    /* PCM has extended fields (0x68)*/
    if (h.codec != 0xFFFE0001) {
        /* 18: loop start offset? (not always) */
        /* 1c: loop end offset? */
        /* 20: data size? */
        /* 24: data size? (new) / count? (old) */
        h.coef_offset = read_u32le(offset + 0x28,sf);
    }

    /* DSP and old versions use a external .raw file (assumed extension) */
    h.stream = false;
    h.external_info = false;
    h.external_data = (h.data_offset & 0xF0000000);
    h.data_offset = h.data_offset & 0x0FFFFFFF;

    /* each channel has its own info but mostly repeats (data may have padding, but files end with no padding) */
    {
        int separation = 0;
        switch(h.codec) {
            case 0xFFFE0001:
            case 0x0001FFFE:
            case 0x01660001: separation = 0x68; break;
            default:         separation = 0x2c; break;
        }

        for (int i = 0; i < h.channels; i++) {
            h.data_offsets[i] = read_u32le(offset + 0x04 + i * separation, sf) & 0x0FFFFFFF;
            h.coef_offsets[i] = read_u32le(offset + 0x28 + i * separation, sf);
        }

        if (h.channels > 1) {
            h.interleave   = h.data_offsets[1] - h.data_offsets[0];
            h.coef_spacing = h.coef_offsets[1] - h.coef_offsets[0];
        }
    }

    switch(h.codec) {
        case 0x00: h.type = DSP; break;        /* (same as below for unlooped audio) */
        case 0x01: h.type = DSP; break;        /* Turbo Super Stunt Squad (Wii/3DS) */
        case 0x06:                              /* (same as below for unlooped audio) */
        case 0x07: h.type = PSX; break;        /* How to Train Your Dragon 2 (PS3), Falling Skies The Game (PS3) */
        case 0x08:                             /* (same as below for unlooped audio) */
        case 0x09: h.type = IMA; break;        /* Scooby-Doo! First Frights (DS), Turbo Super Stunt Squad (DS) */
        case 0xFFFE0001: h.type = PCM16BE; break; /* Falling Skies The Game (X360) */
        case 0x0001FFFE: h.type = PCM16LE; break; /* Scooby Doo and the Spooky Swamp (PC), Monster High: New Ghoul in School (PC) */
        case 0x01660001: h.type = XMA2; break; /* Rise of the Guardians (X360) */
        default:
            VGM_LOG("SqueakSample: unknown codec %x\n", h.codec);
            return NULL;
    }

    return init_vgmstream_squeak_common(sf, &h);
}


static STREAMFILE* load_assets(STREAMFILE* sf, squeak_header_t* h) {
    STREAMFILE* sb = NULL;
    STREAMFILE* sn = NULL;
    read_s32_t read_s32 = h->big_endian ? read_s32be : read_s32le;


    char asset_name[0x20]; /* "(8-byte crc).raw", "xx(6-byte crc).raw", "(regular name).raw" */
    if (h->external_info) {
        sn = open_streamfile_by_ext(sf, "asset"); /* unknown real extension if any, based on debug strings */
        if (!sn) {
            vgm_logi("Squeak: external name '.asset' not found (put together)\n");
            goto fail;
        }
    }

    if (h->stream) {
        if (h->version == 0) {
            h->sample_rate = read_s32(h->extb_offset + 0x04, sn ? sn : sf); /* per channel, use first */
        }

        read_string(asset_name, sizeof(asset_name), h->name_offset, sn ? sn : sf);

        /* extb_offset defines N coef offset per channel but in practice this seem fixed, simplify */
        h->coef_offset = 0x40;
        h->coef_spacing = 0x30;
    }

    /* try to open external data .raw in various ways, since this format is a bit hard to use */
    if (h->stream) {
        /* "(asset name)": plain as found  */
        if (!sb) {
            sb = open_streamfile_by_filename(sf, asset_name);
        }

        /* "sound/(asset name)": most common way to store files */
        char path_name[256];
        snprintf(path_name, sizeof(path_name), "sound/%s", asset_name);
        if (!sb) {
            sb = open_streamfile_by_filename(sf, path_name);
        }
    }

    /* "(header name).raw": for squeakstreams and renamed files */
    if (!sb) {
        sb = open_streamfile_by_ext(sf, "raw");
    }

    if (!sb) {
        char* info = h->stream ? asset_name : "(filename).raw";
        vgm_logi("Squeak: external file '%s' not found (put together)\n", info);
        goto fail;
    }

    close_streamfile(sn);
    return sb;
fail:
    close_streamfile(sn);
    return NULL;
}

static VGMSTREAM* init_vgmstream_squeak_common(STREAMFILE* sf, squeak_header_t* h) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sb = NULL;
    STREAMFILE* sf_body = NULL;

    /* common */
    int loop_flag = h->loop_end > 0;
  

    /* open external asset */
    if (h->external_data) {
        sb = load_assets(sf, h);
        if (!sb) goto fail;
    }

    sf_body = sb ? sb : sf;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(h->channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = h->stream ? meta_SQUEAKSTREAM : meta_SQUEAKSAMPLE;
    vgmstream->sample_rate = h->sample_rate;
    vgmstream->num_samples = h->num_samples;
    vgmstream->loop_start_sample = h->loop_start;
    vgmstream->loop_end_sample = h->loop_end + 1;
    vgmstream->stream_size = h->data_size;

    switch(h->type) {
        case DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = h->interleave;
            //vgmstream->interleave_last_block_size = ...; /* apparently padded */

            dsp_read_coefs(vgmstream, sf, h->coef_offset + 0x00, h->coef_spacing, h->big_endian);
            dsp_read_hist (vgmstream, sf, h->coef_offset + 0x24, h->coef_spacing, h->big_endian);
            break;

        case PCM16LE:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = h->interleave; /* not 0x02 */

            break;

        case PCM16BE:
            vgmstream->coding_type = coding_PCM16BE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = h->interleave; /* not 0x02 */

            break;

        case PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = h->interleave;
            break;

        case PCM8:
            vgmstream->coding_type = coding_PCM8;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = h->interleave;
            break;

        case MSIMA:
            vgmstream->coding_type = coding_MS_IMA;
            vgmstream->layout_type = layout_none;
            //vgmstream->interleave_block_size = h->interleave; /* unused? (mono) */
            vgmstream->frame_size = h->codec == 0x04 ? 0x400 : 0x20;
            break;

        case IMA:
            vgmstream->coding_type = coding_IMA;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = h->interleave;

            /* possibly considered MS-IMA in a single block (not valid though), first 2 values maybe are adpcm hist */
            h->data_offset += 0x04;
            break;

#ifdef VGM_USE_FFMPEG
        case XMA2: {
            /* uses separate mono streams */
            vgmstream->coding_type = coding_FFmpeg;
            for (int i = 0; i < h->channels; i++) {
                uint32_t offset = h->data_offsets[i];
                uint32_t next_offset = (i + 1 == h->channels) ? get_streamfile_size(sf_body) : h->data_offsets[i+1];
                uint32_t data_size = next_offset - offset;
                int layer_channels = 1;

                vgmstream->codec_data = init_ffmpeg_xma2_raw(sf_body, offset, data_size, h->num_samples, layer_channels, h->sample_rate, 0, 0);
                if (!layered_add_codec(vgmstream, 0, layer_channels))
                    goto fail;
            }
            if (!layered_add_done(vgmstream))
                goto fail;

            break;
        }
#endif
#ifdef VGM_USE_VORBIS
        case VORBIS:
            vgmstream->codec_data = init_ogg_vorbis(sf_body, h->data_offset, 0, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_OGG_VORBIS;
            vgmstream->layout_type = layout_none;

            break;
#endif
#ifdef VGM_USE_SPEEX
        case SPEEX: {
            vgmstream->codec_data = init_speex_torus(h->channels);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_SPEEX;
            vgmstream->layout_type = layout_none;

            break;
        }
#endif
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sb ? sb : sf, h->data_offset))
        goto fail;
    close_streamfile(sb);
    return vgmstream;
fail:
    close_streamfile(sb);
    close_vgmstream(vgmstream);
    return NULL;
}
