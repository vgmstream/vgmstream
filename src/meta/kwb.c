#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"

typedef enum { PCM16, MSADPCM, DSP_HEAD, DSP_BODY, AT9, MSF_APEX, XMA2, WBND_SDBK } kwb_codec;

typedef struct {
    int big_endian;
    int total_subsongs;
    int target_subsong;
    bool found;
    kwb_codec codec;

    int channels;
    int sample_rate;
    int32_t num_samples;
    int32_t loop_start;
    int32_t loop_end;
    int loop_flag;
    int block_size;

    uint32_t stream_offset;
    uint32_t stream_size;
    uint32_t companion_offset;
    uint32_t companion_size;
    int subfile_subsong;

    off_t dsp_offset;
    //off_t name_offset;
} kwb_header_t;

static bool parse_wbh_wbd(kwb_header_t* kwb, STREAMFILE* sf_h, uint32_t wbh_offset, STREAMFILE* sf_b, uint32_t wbd_offset);
static bool parse_xws(kwb_header_t* kwb, STREAMFILE* sf);
static VGMSTREAM* init_vgmstream_koei_wavebank(kwb_header_t* kwb, STREAMFILE* sf_h, STREAMFILE* sf_b);


/* KWB - WaveBank from Koei games */
VGMSTREAM* init_vgmstream_kwb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE *sf_h = NULL, *sf_b = NULL;
    kwb_header_t kwb = {0};
    int target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00, sf, "WBD_") &&
        !is_id32le(0x00, sf, "WBD_") &&
        !is_id32be(0x00, sf, "WHD1"))
        return NULL;

    /* .wbd+wbh: common [Bladestorm Nightmare (PC)]
     * .wbd+whd: uncommon [Nights of Azure 2 (PS4)]
     * .wb2+wh2: newer [Nights of Azure 2 (PC)]
     * .sed: mixed header+data [Dissidia NT (PC)] */
    if (!check_extensions(sf, "wbd,wb2,sed"))
        return NULL;


    /* open companion header */
    if (is_id32be(0x00, sf, "WHD1")) { /* .sed */
        sf_h = sf;
        sf_b = sf;
    }
    else if (check_extensions(sf, "wbd")) {
        sf_h = open_streamfile_by_ext(sf, "wbh");
        if (!sf_h)
            sf_h = open_streamfile_by_ext(sf, "whd");
        sf_b = sf;
    }
    else if (check_extensions(sf, "wb2")) {
        sf_h = open_streamfile_by_ext(sf, "wh2");
        sf_b = sf;
    }
    else {
        goto fail;
    }

    if (sf_h == NULL || sf_b == NULL)
        goto fail;

    if (target_subsong == 0) target_subsong = 1;
    kwb.target_subsong = target_subsong;

    if (!parse_wbh_wbd(&kwb, sf_h, 0x00, sf_b, 0x00))
        goto fail;

    vgmstream = init_vgmstream_koei_wavebank(&kwb, sf_h, sf_b);
    if (!vgmstream) goto fail;

    if (sf_h != sf) close_streamfile(sf_h);
    return vgmstream;

fail:
    if (sf_h != sf) close_streamfile(sf_h);
    close_vgmstream(vgmstream);
    return NULL;
}

/* XWS - WaveStream? from Koei games */
VGMSTREAM* init_vgmstream_xws(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    kwb_header_t kwb = {0};
    int target_subsong = sf->stream_index;


    /* checks */
    if (!(is_id32be(0x00, sf, "XWSF") || is_id32be(0x00, sf, "tdpa")))
        return NULL;

    if (!check_extensions(sf, "xws"))
        return NULL;

    if (target_subsong == 0) target_subsong = 1;
    kwb.target_subsong = target_subsong;

    if (!parse_xws(&kwb, sf))
        return NULL;

    vgmstream = init_vgmstream_koei_wavebank(&kwb, sf, sf);
    if (!vgmstream) goto fail;

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* SND - Sound? from Koei games [Ninja Gaiden Sigma -Master Collection- (PC)] */
VGMSTREAM* init_vgmstream_snd_koei(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    kwb_header_t kwb = {0};
    int target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00, sf, "SND\0"))
        return NULL;

    /* .snd: header id (used by extractors/DoaTool) */
    if (!check_extensions(sf, "snd"))
        return NULL;

    if (target_subsong == 0) target_subsong = 1;
    kwb.target_subsong = target_subsong;

    if (!parse_xws(&kwb, sf))
        return NULL;

    vgmstream = init_vgmstream_koei_wavebank(&kwb, sf, sf);
    if (!vgmstream) goto fail;

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}

static VGMSTREAM* init_vgmstream_koei_wavebank(kwb_header_t* kwb, STREAMFILE* sf_h, STREAMFILE* sf_b) {
    VGMSTREAM* vgmstream = NULL;
    read_u32_t read_u32 = kwb->big_endian ? read_u32be : read_u32le;
    read_s32_t read_s32 = kwb->big_endian ? read_s32be : read_s32le;


    /* container */
    if (kwb->codec == MSF_APEX) {
        if (kwb->stream_offset == 0) {
            vgmstream = init_vgmstream_silence(0,0,0); // dummy, whatevs
            if (!vgmstream) goto fail;
        }
        else {
            STREAMFILE* temp_sf = NULL;
            init_vgmstream_t init_vgmstream = NULL;
            const char* fake_ext;


            uint32_t id = read_u32be(kwb->stream_offset, sf_h);
            if ((id & 0xFFFFFF00) == get_id32be("MSF\0")) { /* PS3 */
                kwb->stream_size = read_u32(kwb->stream_offset + 0x0c, sf_h) + 0x40;
                fake_ext = "msf";
                init_vgmstream = init_vgmstream_msf;
            }
            else if (id == get_id32be("APEX")) { /* WiiU */
                kwb->stream_size = read_u32(kwb->stream_offset + 0x04, sf_h); /* not padded */
                fake_ext = "dsp";
                init_vgmstream = init_vgmstream_dsp_apex;
            }
            else {
                vgm_logi("KWB: unknown type id=%x at offset=%x\n", id, kwb->stream_offset);
                goto fail;
            }

            temp_sf = setup_subfile_streamfile(sf_h, kwb->stream_offset, kwb->stream_size, fake_ext);
            if (!temp_sf) goto fail;

            vgmstream = init_vgmstream(temp_sf);
            close_streamfile(temp_sf);
            if (!vgmstream) goto fail;
        }

        vgmstream->num_streams = kwb->total_subsongs;
        return vgmstream;
    }

    if (kwb->codec == WBND_SDBK) {
        STREAMFILE* temp_sf_xwb = NULL;
        STREAMFILE* temp_sf_xsb = NULL;

        temp_sf_xwb = setup_subfile_streamfile(sf_h, kwb->stream_offset, kwb->stream_size, "xwb");
        if (!temp_sf_xwb) goto fail;

        if (kwb->companion_offset && kwb->companion_size) {
            temp_sf_xsb = setup_subfile_streamfile(sf_h, kwb->companion_offset, kwb->companion_size, "xsb");
            if (!temp_sf_xsb) goto fail;
        }

        temp_sf_xwb->stream_index = kwb->subfile_subsong;

        vgmstream = init_vgmstream_wbnd_sdbk(temp_sf_xwb, temp_sf_xsb);
        close_streamfile(temp_sf_xwb);
        close_streamfile(temp_sf_xsb);
        if (!vgmstream) goto fail;

        vgmstream->loop_flag = false; //those .xwb seem to set full loops, but KWB never loops
        vgmstream->num_streams = kwb->total_subsongs;
        //vgmstream->stream_index = kwb->target_subsong;
        return vgmstream;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(kwb->channels, kwb->loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_KWB;
    vgmstream->sample_rate = kwb->sample_rate;
    vgmstream->num_samples = kwb->num_samples;
    vgmstream->stream_size = kwb->stream_size;
    vgmstream->num_streams = kwb->total_subsongs;

    switch(kwb->codec) {
        case PCM16: /* PCM */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case MSADPCM:
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = kwb->block_size;
            break;

        case DSP_HEAD:
        case DSP_BODY:
            if (kwb->channels > 1) goto fail;
            vgmstream->coding_type = coding_NGC_DSP; /* subinterleave? */
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x08;
            if (kwb->codec == DSP_HEAD) {
                dsp_read_coefs(vgmstream, sf_h, kwb->dsp_offset + 0x1c, 0x60, kwb->big_endian);
                dsp_read_hist (vgmstream, sf_h, kwb->dsp_offset + 0x40, 0x60, kwb->big_endian);
            }
            else {
                /* typical DSP header + data */
                vgmstream->num_samples = read_s32(kwb->stream_offset + 0x00, sf_b);
                dsp_read_coefs(vgmstream, sf_b, kwb->stream_offset + 0x1c, 0x60, kwb->big_endian);
                dsp_read_hist (vgmstream, sf_b, kwb->stream_offset + 0x40, 0x60, kwb->big_endian);
                kwb->stream_offset += 0x60;
            }

            break;

#ifdef VGM_USE_FFMPEG
        case XMA2: {
            int block_size = 0x800; /* ? */

            if (kwb->channels > 1) goto fail;

            vgmstream->codec_data = init_ffmpeg_xma2_raw(sf_b, kwb->stream_offset, kwb->stream_size, vgmstream->num_samples, kwb->channels, kwb->sample_rate, block_size, 0);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sf_b, kwb->stream_offset, kwb->stream_size, 0, 0,0); /* assumed */
            break;
        }
#endif

#ifdef VGM_USE_ATRAC9
        case AT9: {
            atrac9_config cfg = {0};

            {
                uint32_t extra_size = read_u32le(kwb->stream_offset + 0x00, sf_b);
                uint32_t config_data = read_u32be(kwb->stream_offset + 0x04, sf_b);
                // 0x0c: encoder delay?
                // 0x0e: encoder padding?
                // 0x10: samples per frame
                // 0x12: frame size

                cfg.channels = vgmstream->channels;
                cfg.config_data = config_data;

                kwb->stream_offset += extra_size;
                kwb->stream_size -= extra_size;
            }

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            //TODO: check encoder delay
            vgmstream->num_samples = atrac9_bytes_to_samples_cfg(kwb->stream_size, cfg.config_data);
            break;
        }
#endif
        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf_b, kwb->stream_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ************************************************************************* */

/* Koei banks may have multiple containers, so target/total subsongs is derived.
 * Returns relative subsong within current range, or -1 if not part of it.
 */
static int kwb_add_entries(kwb_header_t* kwb, int entries) {

    int current_subsongs = kwb->total_subsongs;
    kwb->total_subsongs += entries;
    if (kwb->target_subsong - 1 < current_subsongs || kwb->target_subsong > kwb->total_subsongs)
        return -1; //valid entry, but not target
    kwb->found = true;

    int relative_subsong = kwb->target_subsong - current_subsongs;

    return relative_subsong;
}

static int parse_type_kwb2(kwb_header_t* kwb, off_t offset, off_t body_offset, STREAMFILE* sf_h) {
    // 00: KWB2/KWBN id
    // 04: always 0x3200?
    int sounds = read_u16le(offset + 0x06, sf_h);
    // 08: ?
    // 0c: 1.0?
    // 10: null or 1
    // 14: offset to HDDB table (from type), can be null

    //;VGM_LOG("KWB2: sounds %i, o=%lx\n", sounds, offset);

    /* offset table to entries */
    for (int i = 0; i < sounds; i++) {
        uint32_t sound_offset = read_u32le(offset + 0x18 + i*0x04, sf_h);
        //;VGM_LOG("KWB2: entry %i, o=%lx, so=%lx\n", i, offset + 0x18 + i*0x04, sound_offset);

        if (sound_offset == 0) /* common... */
            continue;
        sound_offset += offset;

        /* sound entry */
        uint16_t version = read_u16le(sound_offset + 0x00, sf_h);
        // 00: version?
        // 02: 0x2b or 0x32
        int subsounds = read_u8(sound_offset + 0x03, sf_h);
        // 03: subsounds?
        // others: unknown or null

        /* unsure but seems to work, maybe upper byte only */
        int subsound_start, subsound_size;
        if (version < 0xc000) {
            subsound_start = 0x2c;
            subsound_size  = 0x48;
        }
        else {
            subsound_start = read_u16le(sound_offset + 0x2c, sf_h);
            subsound_size  = read_u16le(sound_offset + 0x2e, sf_h);
        }
        subsound_start += sound_offset;

        for (int j = 0; j < subsounds; j++) {

            int relative_subsong = kwb_add_entries(kwb, 1);
            if (relative_subsong <= 0)
                continue;
            uint32_t subsound_offset = subsound_start + j * subsound_size;

            kwb->sample_rate    = read_u16le(subsound_offset + 0x00, sf_h);
            uint8_t codec       = read_u8   (subsound_offset + 0x02, sf_h);
            kwb->channels       = read_u8   (subsound_offset + 0x03, sf_h);
            kwb->block_size     = read_u16le(subsound_offset + 0x04, sf_h);
            /* 0x06: samples per frame in MSADPCM? */
            /* 0x08: some id? (not always) */
            kwb->num_samples    = read_u32le(subsound_offset + 0x0c, sf_h);
            kwb->stream_offset  = read_u32le(subsound_offset + 0x10, sf_h);
            kwb->stream_size    = read_u32le(subsound_offset + 0x14, sf_h);
            /* when size > 0x48 */
            /* 0x48: subsound entry size */
            /* rest: reserved per codec? (usually null) */

            kwb->stream_offset += body_offset;

            switch(codec) {
                case 0x00:
                    kwb->codec = PCM16;
                    break;
                case 0x10:
                    kwb->codec = MSADPCM;
                    break;
                case 0x90:
                    kwb->codec = DSP_HEAD;
                    kwb->dsp_offset = subsound_offset + 0x4c;
                    break;
                default:
                    VGM_LOG("KWB2: unknown codec\n");
                    return false;
            }
        }
    }

    //TODO: read names
    /* HDDB table (optional and not too common)
    00 HDDB id
    04 1?
    08: 20? start?
    0c: 14? start?
    10: size
    14: name table start
    20: name offsets?
    then some subtable
    then name table (null terminated and one after other)
    */

    return true;
}

/* a format mimicking PSVita's .phd+pbd format */
static bool parse_type_k4hd_pvhd(kwb_header_t* kwb, off_t offset, off_t body_offset, STREAMFILE* sf_h) {

    // 00: K4HD/PVHD id
    // 04: chunk size
    // 08: ?
    // 0c: ?
    // 10: PPPG offset ('program'? cues?)
    // 14: PPTN offset ('tone'? sounds?)
    // 18: PPVA offset ('VAG'? waves)
    uint32_t ppva_offset = read_u16le(offset + 0x18, sf_h);
    ppva_offset += offset;

    /* PPVA table: */
    if (!is_id32be(ppva_offset + 0x00, sf_h, "PPVA"))
        return false;

    uint32_t entry_size = read_u32le(ppva_offset + 0x08, sf_h);
    // 0x0c: -1?
    // 0x10: 0?
    int entries = read_u32le(ppva_offset + 0x14, sf_h) + 1;
    // 0x18: -1?
    // 0x1c: -1?

    if (entry_size != 0x1c) {
        VGM_LOG("K4HD: unknown entry size\n");
        return false;
    }

    int relative_subsong = kwb_add_entries(kwb, entries);
    if (relative_subsong <= 0)
        return true;
    uint32_t header_offset = ppva_offset + 0x20 + (relative_subsong-1) * entry_size;

    kwb->stream_offset  = read_u32le(header_offset + 0x00, sf_h);
    kwb->sample_rate    = read_u32le(header_offset + 0x04, sf_h);
    kwb->stream_size    = read_u32le(header_offset + 0x08, sf_h);
    // 0x0c: -1? loop?
    if (read_u32le(header_offset + 0x10, sf_h) != 2) { // codec? flags?
        VGM_LOG("K4HD: unknown codec\n");
        return false;
    }
    // 0x14: loop start?
    // 0x18: loop end?

    // TO-DO: in rare cases offsets+sizes are incorrect after some subsongs (possibly a bug on their part? NGS+ Vita's Prologue_2)

    kwb->stream_offset += body_offset;

    kwb->channels = 1; // always, devs use dual subsongs to fake stereo (like as hd3+bd3)
    kwb->codec = AT9;
    return true;
}

/* format somewhat similar to Sony VABs */
static bool parse_type_sdsd(kwb_header_t* kwb, off_t offset, off_t body_offset, STREAMFILE* sf_h) {
    // 00: SDsdVers
    // 08: chunk size
    // 0c: null
    // 10: SDsdHead
    // 18: chunk size
    // 1c: ? size
    // 20: null
    // 24: SDsdProg offset ('program'? cues?)
    // 28: SDsdSmpl offset ('samples'? waves?)
    // rest: ?
    uint32_t smpl_offset = read_u32le(offset + 0x28, sf_h);
    smpl_offset += offset;

    /* Smpl table: */
    if (!is_id64be(smpl_offset + 0x00, sf_h, "SDsdSmpl"))
        return false;

    // 0x08: ?
    int entries = read_u32le(smpl_offset + 0x0c, sf_h);
    uint32_t entry_size = 0x9c;

    int relative_subsong = kwb_add_entries(kwb, entries);
    if (relative_subsong <= 0)
        return true;
    uint32_t header_offset = smpl_offset + 0x10 + (relative_subsong-1) * entry_size;

    kwb->stream_offset  = read_u32le(header_offset + 0x00, sf_h);
    // 08: ? + channels?
    // 0c: bps?
    kwb->sample_rate    = read_u32le(header_offset + 0x0c, sf_h);
    kwb->num_samples    = read_u32le(header_offset + 0x10, sf_h) / sizeof(int16_t); // PCM
    // rest: ? (flags, etc)
    kwb->stream_size    = read_u32le(header_offset + 0x44, sf_h);

    kwb->stream_offset += body_offset;

    kwb->channels = 1;
    kwb->codec = XMA2;
    return true;
}

/* variation of SDsd */
static bool parse_type_sdwi(kwb_header_t* kwb, off_t offset, off_t body_offset, STREAMFILE* sf_h) {
    // 00: SDWiVers
    // 08: chunk size
    // 0c: null
    // 10: SDsdHead
    // 18: chunk size
    // 1c: WBH_ size
    // 20: WBD_ size
    // 24: SDsdProg offset ('program'? cues?)
    // 28: SDsdSmpl offset ('samples'? waves?)
    // rest: ?
    uint32_t smpl_offset = read_u32be(offset + 0x28, sf_h);
    smpl_offset += offset;

    /* Smpl table: */
    if (!is_id64be(smpl_offset + 0x00, sf_h, "SDsdSmpl"))
        return false;

    // 0x08: ?
    int entries = read_u32le(smpl_offset + 0x0c, sf_h); /* LE! */
    uint32_t entry_size = 0x40;

    int relative_subsong = kwb_add_entries(kwb, entries);
    if (relative_subsong <= 0)
        return true;
    uint32_t header_offset = smpl_offset + 0x10 + (relative_subsong-1) * entry_size;

    // 00: "SS" + ID (0..N)
    kwb->stream_offset  = read_u32be(header_offset + 0x04, sf_h);
    // 08: flag?
    // 0c: ? + channels?
    kwb->sample_rate    = read_u32be(header_offset + 0x10, sf_h);
    // 14: bitrate
    // 18: codec? + bps
    // 1c: null?
    // 20: null?
    kwb->stream_size    = read_u32be(header_offset + 0x24, sf_h);
    // 28: full stream size (with padding)
    // 2c: related to samples?
    // 30: ID
    // 34-38: null

    kwb->stream_offset += body_offset;

    kwb->channels = 1;
    kwb->codec = DSP_BODY;
    return true;
}


/* container of fused .wbh+wbd or separate files*/
static bool parse_wbh_wbd(kwb_header_t* kwb, STREAMFILE* sf_h, uint32_t wbh_offset, STREAMFILE* sf_b, uint32_t wbd_offset) {
    read_u32_t read_u32 = NULL;

    uint32_t head_offset, body_offset, start;
    if (is_id32be(wbh_offset + 0x00, sf_h, "WHD1")) {
        /* container of fused .wbh+wbd */
        // 0x04: fixed value?
        kwb->big_endian = read_u8(wbh_offset + 0x08, sf_h) == 0xFF;
        // 0x0a: version?

        read_u32 = kwb->big_endian ? read_u32be : read_u32le;

        start = read_u32(wbh_offset + 0x0c, sf_h);
        start += wbh_offset;
        // 0x10: file size
        // 0x14: subfiles?
        // 0x18: subfiles?
        // 0x1c: null
        // 0x20: some size?
        // 0x24: some size?

        head_offset = read_u32(start + 0x00, sf_h);
        body_offset = read_u32(start + 0x04, sf_h);
        // 0x10: head size
        // 0x14: body size
        head_offset += wbh_offset;
        body_offset += wbh_offset;
    }
    else {
        /* dual file */
        head_offset = wbh_offset;
        body_offset = wbd_offset;

        kwb->big_endian = guess_endian32(head_offset + 0x08, sf_h);

        read_u32 = kwb->big_endian ? read_u32be : read_u32le;
    }

    if (read_u32(head_offset + 0x00, sf_h) != get_id32be("WBH_") ||
        read_u32(head_offset + 0x04, sf_h) != get_id32be("0000"))
        return false;
    if (read_u32(body_offset + 0x00, sf_b) != get_id32be("WBD_") ||
        read_u32(body_offset + 0x04, sf_b) != get_id32be("0000"))
        return false;
    // 0x08: head/body size

    head_offset += 0x0c;
    body_offset += 0x0c;

    /* format has multiple bank subtypes that are quite different from each other */
    uint32_t type = read_u32be(head_offset + 0x00, sf_h);
    switch(type) {
        case 0x4B574232: /* "KWB2" [Bladestorm Nightmare (PC), Dissidia NT (PC)] */
        case 0x4B57424E: /* "KWBN" [Fire Emblem Warriors (Switch)] */
            if (!parse_type_kwb2(kwb, head_offset, body_offset, sf_h))
                return false;
            break;

        case 0x4B344844: /* "K4HD" [Dissidia NT (PS4), (Vita) */
        case 0x50564844: /* "PVHD" [Nights of Azure 2 (PS4)] */
            if (!parse_type_k4hd_pvhd(kwb, head_offset, body_offset, sf_h))
                return false;
            break;

        case 0x53447364: /* "SDsd" [Bladestorm Nightmare (PC)-X360 leftover files] */
            if (!parse_type_sdsd(kwb, head_offset, body_offset, sf_h))
                return false;
            break;

        case 0x53445769: /* "SDWi" [Fatal Frame 5 (WiiU)] */
            if (!parse_type_sdwi(kwb, head_offset, body_offset, sf_h))
                return false;
            break;

        default:
            vgm_logi("KWB: unknown type\n");
            return false;
    }

    if (!kwb->found)
        return false;

    return true;
}

/* just like XWSF, abridged */
static bool parse_type_msfbank(kwb_header_t* kwb, uint32_t offset, STREAMFILE* sf) {
    int entries = read_u32be(offset + 0x14, sf);

    int relative_subsong = kwb_add_entries(kwb, entries);
    if (relative_subsong <= 0)
        return true;
    uint32_t header_offset = offset + 0x30 + (relative_subsong - 1) * 0x04;

    /* just a dumb table pointing to MSF/APEX, entries can be dummy */
    kwb->stream_offset = read_u32be(header_offset, sf);
    kwb->stream_offset += offset;

    kwb->codec = MSF_APEX;
    return true;
}

/* regular .xwb+xsd */
static bool parse_type_wbnd_sdbk(kwb_header_t* kwb, uint32_t xwb_offset, uint32_t xwb_size, uint32_t xsd_offset, uint32_t xsd_size, STREAMFILE* sf) {
    read_u32_t read_u32 = kwb->big_endian ? read_u32be : read_u32le;
    read_s32_t read_s32 = kwb->big_endian ? read_s32be : read_s32le;

    // xwb header, abridged
    int entries = 0;
    uint32_t xwb_version = read_u32(xwb_offset + 0x04, sf);
    if (xwb_version <= 0x01) { //not used by Koei but just in case
        entries = read_s32(xwb_offset + 0x0c, sf);
    }
    else {
        uint32_t offset = xwb_version <= 41 ? 0x08 : 0x0c;
        uint32_t base_offset = read_s32(xwb_offset + offset+0x00, sf);
        entries = read_s32(xwb_offset + base_offset+0x04, sf);
    }

    int relative_subsong = kwb_add_entries(kwb, entries);
    if (relative_subsong <= 0)
        return true;

    kwb->stream_offset = xwb_offset;
    kwb->stream_size = xwb_size;
    kwb->companion_offset = xsd_offset;
    kwb->companion_size = xsd_size;
    kwb->subfile_subsong = relative_subsong;

    kwb->codec = WBND_SDBK;
    return true;
}

static bool parse_type_xwsfile_tdpack(kwb_header_t* kwb, uint32_t offset, STREAMFILE* sf) {

    uint64_t id = read_u64be(offset + 0x00, sf);
    if ( !(id == get_id64be("XWSFILE\0") || id == get_id64be("tdpack\0\0") || id == get_id64be("SND\0\0\0\0\0")) )
        return false;

    kwb->big_endian = read_u8(offset + 0x08, sf) == 0xFF;
    // 0x0a: version? (0100: NG2 X360, NG2/NG3 PS3, NGm PC, 0101: DoA LR PC, NG2/3 PC)

    read_u32_t read_u32 = kwb->big_endian ? read_u32be : read_u32le;

    // 0x0c: tables start
    // 0x10: file size
    int chunks2 = read_u32(offset + 0x14, sf);
    int chunks  = read_u32(offset + 0x18, sf);
    // 0x1c: null

    bool is_snd = id == get_id64be("SND\0\0\0\0\0");
    if (is_snd && chunks != 3 && chunks2 != 4) //seen in NGm (PC) SND
        return false;
    if (!is_snd && chunks != chunks2)
        return false;

    uint32_t table1_offset = read_u32(offset + 0x20, sf); // offsets
    uint32_t table2_offset = read_u32(offset + 0x24, sf); // sizes
    // 0x28: null
    // 0x2c: null


    int i = 0;
    while (i < chunks) {
        //;VGM_LOG("XWS: entry %i/%i\n", i, chunks);

        uint32_t head_offset = 0, head_size = 0;
        /* NG2/NG3 PS3/PC have table1+2, DoA LR PC doesn't (not very useful) */
        if (table2_offset) {
            head_offset = read_u32(offset + table1_offset + i * 0x04, sf);
            head_size   = read_u32(offset + table2_offset + i * 0x04, sf);
            if (!head_size)  { /* sometimes has file end offset as entry with no size (NG PS3)*/
                i += 1;
                continue;
            }
        }
        else {
            head_offset = read_u32(offset + table1_offset + i * 0x04, sf);
        }

        if (!head_offset) { /* just in case */
            i += 1;
            continue;
        }

        head_offset += offset;

        //;VGM_LOG("XWS: head=%x + %x\n", head_offset, head_size);

        uint32_t entry_type = read_u32be(head_offset + 0x00, sf);
        if (entry_type == get_id32be("XWSF")) { //+ "ILE\0"
            i += 1;
            if (!parse_type_xwsfile_tdpack(kwb, head_offset, sf))
                return false;;
        }
        else if (entry_type == get_id32be("tdpa")) { // + "ck\0\0"
            i += 1;
            if (!parse_type_xwsfile_tdpack(kwb, head_offset, sf))
                return false;;
            continue;
        }
        else if (entry_type == get_id32be("_HBW")) {
            // .wbh+wbd with KWB2 [Ninja Gaiden Sigma (PC)]
            i += 1;

            uint32_t body_offset = read_u32(offset + table1_offset + i * 0x04, sf);
            body_offset += offset;
            i += 1;

            if (!parse_wbh_wbd(kwb, sf, head_offset, sf, body_offset))
                return false;;
        }
        else if (entry_type == get_id32be("CUEB") || entry_type < 0x100) {
            // CUE-like info (may start with 0 or a low number instead)
            i += 1;
        }
        else if (entry_type == get_id32be("MSFB")) { //+ "ANK\0" 
            i += 1;
            if (!parse_type_msfbank(kwb, head_offset, sf))
                return false;;
        }
        else if (entry_type == get_id32be("KWB2")) {
            // NG2/3 PC, DoA LR PC goes head,body,...
            uint32_t body_offset = read_u32(offset + table1_offset + i * 0x04 + 0x04, sf);
            body_offset += offset;
            i += 2;

            if (!parse_type_kwb2(kwb, head_offset, body_offset, sf))
                return false;;
        }
        else if (entry_type == get_id32be("PVHD")) {
            // [Ninja Gaiden 2 Sigma Plus (Vita)]
            uint32_t body_offset = read_u32(offset + table1_offset + i * 0x04 + 0x04, sf);
            body_offset += offset;
            i += 2;

            if (!parse_type_k4hd_pvhd(kwb, head_offset, body_offset, sf))
                return false;

            if (!parse_type_kwb2(kwb, head_offset, body_offset, sf))
                return false;;
        }
        else if (entry_type == get_id32be("DNBW")) {
            // .xwb+.xsd [Ninja Gaiden 2 (X360)]
            uint32_t xwb_offset = head_offset;
            uint32_t xwb_size = head_size;
            i += 1;

            // comes with a .xsd pair in next chunk
            uint32_t kbds_offset = 0, kbds_size = 0;
            if (table2_offset) {
                kbds_offset = read_u32(offset + table1_offset + i * 0x04, sf);
                kbds_size   = read_u32(offset + table2_offset + i * 0x04, sf);
                kbds_offset += offset;
            }
            i += 1;

            if (!parse_type_wbnd_sdbk(kwb, xwb_offset, xwb_size, kbds_offset, kbds_size, sf))
                return false;;
        }
        else if (entry_type == get_id32be("0000")) {
            // dummy/padding (all '0') [Ninja Gaiden 2 (X360)]

            i += 1;
        }
        else {
            // SND has 2 config chunks after first (no header id)
            if (is_snd && i > 0) {
                i += 1;
                continue;
            }

            vgm_logi("XWS: unknown chunk %i (%x) with head=%x at %x\n", i, entry_type, head_offset, offset);
            return false;
        }
    }

    return true;
}


static bool parse_xws(kwb_header_t* kwb, STREAMFILE* sf) {

    /* Format is similar to WHD1 with some annoyances of its own. Variations:
     * - XWSFILE w/ N chunks: CUE offsets + 1 MSFBANK offset
     *   [Ninja Gaiden Sigma 2 (PS3), Ninja Gaiden 3 Razor's Edge (PS3)]
     * - XWSFILE w/ 2*N chunks: KWB2 offset + data offset * N (ex. 3 pairs = 6 chunks)
     *   [Dead or Alive 5 Last Round (PC)]
     * - tdpack: same but points to N XWSFILE / DNBW
     *   [Ninja Gaiden 3 Razor's Edge (PS3/X360)]
     * - SND: similar to XWSFILE w/ 2*N chunks, points to tdpack (which point to _HBW0000+KWB2)
     *   chunks after first seem to be cue/config only
     *   [Ninja Gaiden Sigma -Master Collection- (PC)]
     *
     * Needs to call sub-parts multiple times to fill total subsongs when parsing xwsfile.
     */
    if (!parse_type_xwsfile_tdpack(kwb, 0x00, sf))
        return false;

    if (!kwb->found)
        return false;

    return true;
}
