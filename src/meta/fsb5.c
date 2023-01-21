#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "fsb5_streamfile.h"


typedef struct {
    int total_subsongs;
    int version;
    int codec;
    int flags;

    int channels;
    int layers;
    int sample_rate;
    int32_t num_samples;
    int32_t loop_start;
    int32_t loop_end;
    int loop_flag;

    uint32_t sample_header_size;
    uint32_t name_table_size;
    uint32_t sample_data_size;
    uint32_t base_header_size;

    uint32_t extradata_offset;
    uint32_t extradata_size;

    uint32_t stream_offset;
    uint32_t stream_size;
    uint32_t name_offset;
} fsb5_header;

/* ********************************************************************************** */

static layered_layout_data* build_layered_fsb5(STREAMFILE* sf, STREAMFILE* sb, fsb5_header* fsb5);

/* FSB5 - Firelight's FMOD Studio SoundBank format */
VGMSTREAM* init_vgmstream_fsb5(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sb = NULL;
    fsb5_header fsb5 = {0};
    uint32_t offset;
    int target_subsong = sf->stream_index;
    int i;


    /* checks */
    if (!is_id32be(0x00,sf, "FSB5"))
        goto fail;

    /* .fsb: standard
     * .snd: Alchemy engine (also Unity) */
    if (!check_extensions(sf,"fsb,snd"))
        goto fail;

    /* v0 is rare, seen in Tales from Space (Vita) */
    fsb5.version = read_u32le(0x04,sf);
    if (fsb5.version != 0x00 && fsb5.version != 0x01)
        goto fail;

    fsb5.total_subsongs     = read_u32le(0x08,sf);
    fsb5.sample_header_size = read_u32le(0x0C,sf);
    fsb5.name_table_size    = read_u32le(0x10,sf);
    fsb5.sample_data_size   = read_u32le(0x14,sf);
    fsb5.codec              = read_u32le(0x18,sf);
    /* 0x1c: zero */
    if (fsb5.version == 0x01) {
        fsb5.flags = read_u32le(0x20,sf); /* found by tests and assumed to be flags, no games known */
        /* 0x24: 128-bit hash */
        /* 0x34: unknown (64-bit sub-hash?) */
        fsb5.base_header_size = 0x3c;
    }
    else {
        /* 0x20: zero/flags? */
        /* 0x24: zero/flags? */
        /* 0x28: 128-bit hash */
        /* 0x38: unknown (64-bit sub-hash?) */
        fsb5.base_header_size = 0x40;
    }

    if ((fsb5.sample_header_size + fsb5.name_table_size + fsb5.sample_data_size + fsb5.base_header_size) != get_streamfile_size(sf)) {
        vgm_logi("FSB5: wrong size, expected %x + %x + %x + %x vs %x (re-rip)\n", fsb5.sample_header_size, fsb5.name_table_size, fsb5.sample_data_size, fsb5.base_header_size, (uint32_t)get_streamfile_size(sf));
        goto fail;
    }

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong > fsb5.total_subsongs || fsb5.total_subsongs <= 0) goto fail;

    /* find target stream header and data offset, and read all needed values for later use
     *  (reads one by one as the size of a single stream header is variable) */
    offset = fsb5.base_header_size;
    for (i = 0; i < fsb5.total_subsongs; i++) {
        uint32_t stream_header_size = 0;
        uint32_t data_offset = 0;
        uint64_t sample_mode;

        sample_mode = read_u64le(offset+0x00,sf);
        stream_header_size += 0x08;

        /* get samples */
        fsb5.num_samples  = ((sample_mode >> 34) & 0x3FFFFFFF); /* bits: 63..34 (30) */

        /* get offset inside data section (max 32b offset 0xFFFFFFE0) */
        data_offset   =  ((sample_mode >> 7) & 0x07FFFFFF) << 5; /* bits: 33..8 (25) */

        /* get channels */
        switch ((sample_mode >> 5) & 0x03) { /* bits: 7..6 (2) */
            case 0:  fsb5.channels = 1; break;
            case 1:  fsb5.channels = 2; break;
            case 2:  fsb5.channels = 6; break; /* some Dark Souls 2 MPEG; some IMA ADPCM */
            case 3:  fsb5.channels = 8; break; /* some IMA ADPCM */
            /* other channels (ex. 4/10/12ch) use 0 here + set extra flags */
            default: /* not possible */
                goto fail;
        }

        /* get sample rate  */
        switch ((sample_mode >> 1) & 0x0f) { /* bits: 5..1 (4) */
            case 0:  fsb5.sample_rate = 4000;  break;
            case 1:  fsb5.sample_rate = 8000;  break;
            case 2:  fsb5.sample_rate = 11000; break;
            case 3:  fsb5.sample_rate = 11025; break;
            case 4:  fsb5.sample_rate = 16000; break;
            case 5:  fsb5.sample_rate = 22050; break;
            case 6:  fsb5.sample_rate = 24000; break;
            case 7:  fsb5.sample_rate = 32000; break;
            case 8:  fsb5.sample_rate = 44100; break;
            case 9:  fsb5.sample_rate = 48000; break;
            case 10: fsb5.sample_rate = 96000; break;
            /* other sample rates (ex. 3000/64000/192000) use 0 here + set extra flags */
            default: /* 11-15: rejected (FMOD error) */
                goto fail;
        }

        /* get extra flags */
        if (sample_mode & 0x01) { /* bits: 0 (1) */
            uint32_t extraflag_offset = offset + 0x08;
            uint32_t extraflag, extraflag_type, extraflag_size, extraflag_end;

            do {
                extraflag = read_u32le(extraflag_offset,sf);
                extraflag_type = (extraflag >> 25) & 0x7F; /* bits 32..26 (7) */
                extraflag_size = (extraflag >> 1) & 0xFFFFFF; /* bits 25..1 (24)*/
                extraflag_end  = (extraflag & 0x01); /* bit 0 (1) */

                /* parse target only, as flags change between subsongs */
                if (i + 1 == target_subsong) {
                    switch(extraflag_type) {
                        case 0x01:  /* channels */
                            fsb5.channels = read_u8(extraflag_offset+0x04,sf);
                            break;
                        case 0x02:  /* sample rate */
                            fsb5.sample_rate = read_s32le(extraflag_offset+0x04,sf);
                            break;
                        case 0x03:  /* loop info */
                            fsb5.loop_start = read_s32le(extraflag_offset+0x04,sf);
                            if (extraflag_size > 0x04) { /* probably not needed */
                                fsb5.loop_end = read_s32le(extraflag_offset+0x08,sf);
                                fsb5.loop_end += 1; /* correct compared to FMOD's tools */
                            }
                            //;VGM_LOG("FSB5: stream %i loop start=%i, loop end=%i, samples=%i\n", i, fsb5.loop_start, fsb5.loop_end, fsb5.num_samples);

                            /* autodetect unwanted loops */
                            {
                                /* like FSB4 jingles/sfx/music do full loops for no reason, but happens a lot less.
                                 * Most songs loop normally now with proper values [ex. Shantae, FFX] */
                                int full_loop, ajurika_loops, is_small;

                                /* disable some jingles, it's even possible one jingle (StingerA Var1) to not have loops
                                 * and next one (StingerA Var2) do [Sonic Boom Fire & Ice (3DS)] */
                                full_loop = fsb5.loop_start == 0 && fsb5.loop_end + 1152 >= fsb5.num_samples; /* around ~15 samples less, ~1000 for MPEG */
                                /* a few longer Sonic songs shouldn't repeat */
                                is_small = 1; //fsb5.num_samples < 20 * fsb5.sample_rate;

                                /* wrong values in some files [Pac-Man CE2 Plus (Switch) pce2p_bgm_ajurika_*.fsb] */
                                ajurika_loops = fsb5.loop_start == 0x3c && fsb5.loop_end == (0x007F007F + 1) &&
                                        fsb5.num_samples > fsb5.loop_end + 10000; /* arbitrary test in case some game does have those */

                                fsb5.loop_flag = 1;
                                if ((full_loop && is_small) || ajurika_loops) {
                                    VGM_LOG("FSB5: stream %i disabled unwanted loop ls=%i, le=%i, ns=%i\n", i, fsb5.loop_start, fsb5.loop_end, fsb5.num_samples);
                                    fsb5.loop_flag = 0;
                                }
                            }
                            break;
                        case 0x04:  /* free comment, or maybe SFX info */
                            break;
                        case 0x05:  /* unknown 32b */
                            /* rare, found in Tearaway (Vita) with value 0 in first stream and
                             * Shantae and the Seven Sirens (Mobile) with value 0x0003bd72 BE in #44 (Arena Town) */
                            VGM_LOG("FSB5: stream %i flag %x with value %08x\n", i, extraflag_type, read_u32le(extraflag_offset+0x04,sf));
                            break;
                        case 0x06:  /* XMA seek table */
                            /* no need for it */
                            break;
                        case 0x07:  /* DSP coefs */
                            fsb5.extradata_offset = extraflag_offset + 0x04;
                            break;
                        case 0x09:  /* ATRAC9 config */
                            fsb5.extradata_offset = extraflag_offset + 0x04;
                            fsb5.extradata_size = extraflag_size;
                            break;
                        case 0x0a:  /* XWMA config */
                            fsb5.extradata_offset = extraflag_offset + 0x04;
                            break;
                        case 0x0b:  /* Vorbis setup ID and seek table */
                            fsb5.extradata_offset = extraflag_offset + 0x04;
                            /* seek table format:
                             * 0x08: table_size (total_entries = seek_table_size / (4+4)), not counting this value; can be 0
                             * 0x0C: sample number (only some samples are saved in the table)
                             * 0x10: offset within data, pointing to a FSB vorbis block (with the 16b block size header)
                             * (xN entries) */
                            break;
                        case 0x0d:  /* peak volume float (optional setting when making fsb) */
                            break;
                        case 0x0f:  /* OPUS data size not counting frames headers */
                            break;
                        case 0x0e:  /* Vorbis intra-layers (multichannel FMOD ~2021) [Invisible, Inc. (Switch), Just Cause 4 (PC)] */
                            fsb5.layers = read_u32le(extraflag_offset+0x04,sf);
                            /* info only as decoding is standard Vorbis that handles Nch multichannel (channels is 1 here) */
                            fsb5.channels = fsb5.channels * fsb5.layers;
                            break;
                        default:
                            vgm_logi("FSB5: stream %i unknown flag 0x%x at %x + 0x04 + 0x%x (report)\n", i, extraflag_type, extraflag_offset, extraflag_size);
                            break;
                    }
                }

                extraflag_offset += 0x04 + extraflag_size;
                stream_header_size += 0x04 + extraflag_size;
            }
            while (extraflag_end != 0x00);
        }

        /* target found */
        if (i + 1 == target_subsong) {
            fsb5.stream_offset = fsb5.base_header_size + fsb5.sample_header_size + fsb5.name_table_size + data_offset;

            /* catch bad rips (like incorrectly split +1.5GB .fsb with wrong header+data) */
            if (fsb5.stream_offset > get_streamfile_size(sf))
                goto fail;


            /* get stream size from next stream offset or full size if there is only one */
            if (i + 1 == fsb5.total_subsongs) {
                fsb5.stream_size = fsb5.sample_data_size - data_offset;
            }
            else {
                uint32_t next_data_offset;
                uint64_t next_sample_mode;
                next_sample_mode = read_u64le(offset+stream_header_size+0x00,sf);
                next_data_offset   =  ((next_sample_mode >> 7) & 0x07FFFFFF) << 5;

                fsb5.stream_size = next_data_offset - data_offset;
            }

            break;
        }

        /* continue searching target */
        offset += stream_header_size;
    }

    if (!fsb5.stream_offset || !fsb5.stream_size)
        goto fail;

    /* get stream name */
    if (fsb5.name_table_size) {
        off_t name_suboffset = fsb5.base_header_size + fsb5.sample_header_size + 0x04*(target_subsong-1);
        fsb5.name_offset = fsb5.base_header_size + fsb5.sample_header_size + read_u32le(name_suboffset,sf);
    }


    /* FSB5 can hit +2GB offsets, but since decoders aren't ready to handle that use a subfile to hide big offsets
     * (some FSB5 CLI versions make buggy offsets = bad output but this was fixed later) */
    if (fsb5.stream_offset > 0x7FFFFFFF) {
        sb = setup_subfile_streamfile(sf, fsb5.stream_offset, fsb5.stream_size, NULL);
        fsb5.stream_offset = 0x00;
    }
    else {
        sb = sf;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(fsb5.channels, fsb5.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = fsb5.sample_rate;
    vgmstream->num_samples = fsb5.num_samples;
    if (fsb5.loop_flag) {
        vgmstream->loop_start_sample = fsb5.loop_start;
        vgmstream->loop_end_sample = fsb5.loop_end;
    }
    vgmstream->num_streams = fsb5.total_subsongs;
    vgmstream->stream_size = fsb5.stream_size;
    vgmstream->meta_type = meta_FSB5;
    if (fsb5.name_offset)
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, fsb5.name_offset, sf);

    switch (fsb5.codec) {
        case 0x00:  /* FMOD_SOUND_FORMAT_NONE */
            goto fail;

        case 0x01:  /* FMOD_SOUND_FORMAT_PCM8  [Anima - Gate of Memories (PC)] */
            vgmstream->coding_type = coding_PCM8_U;
            vgmstream->layout_type = fsb5.channels == 1 ? layout_none : layout_interleave;
            vgmstream->interleave_block_size = 0x01;
            break;

        case 0x02:  /* FMOD_SOUND_FORMAT_PCM16  [Shantae Risky's Revenge (PC)] */
            vgmstream->coding_type = (fsb5.flags & 0x01) ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = fsb5.channels == 1 ? layout_none : layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case 0x03:  /* FMOD_SOUND_FORMAT_PCM24 */
            vgm_logi("FSB5: FMOD_SOUND_FORMAT_PCM24 found (report)\n");
            goto fail;

        case 0x04:  /* FMOD_SOUND_FORMAT_PCM32 */
            vgm_logi("FSB5: FMOD_SOUND_FORMAT_PCM32 found (report)\n");
            goto fail;

        case 0x05:  /* FMOD_SOUND_FORMAT_PCMFLOAT  [Anima: Gate of Memories (PC)] */
            vgmstream->coding_type = coding_PCMFLOAT;
            vgmstream->layout_type = (fsb5.channels == 1) ? layout_none : layout_interleave;
            vgmstream->interleave_block_size = 0x04;
            break;

        case 0x06:  /* FMOD_SOUND_FORMAT_GCADPCM  [Sonic Boom: Fire and Ice (3DS)] */
            if (fsb5.flags & 0x02) { /* non-interleaved mode */
                vgmstream->coding_type = coding_NGC_DSP;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = (fsb5.stream_size / fsb5.channels);
            }
            else {
                vgmstream->coding_type = coding_NGC_DSP_subint;
                vgmstream->layout_type = layout_none;
                vgmstream->interleave_block_size = 0x02;
            }
            dsp_read_coefs_be(vgmstream, sf, fsb5.extradata_offset, 0x2E);
            break;

        case 0x07:  /* FMOD_SOUND_FORMAT_IMAADPCM  [Skylanders] */
            vgmstream->coding_type = (vgmstream->channels > 2) ? coding_FSB_IMA : coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            break;

        case 0x08:  /* FMOD_SOUND_FORMAT_VAG  [from fsbankex tests, no known games] */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            if (fsb5.flags & 0x02) { /* non-interleaved mode */
                vgmstream->interleave_block_size = (fsb5.stream_size / fsb5.channels);
            }
            else {
                vgmstream->interleave_block_size = 0x10;
            }
            break;

        case 0x09:  /* FMOD_SOUND_FORMAT_HEVAG  [Guacamelee (Vita)] */
            vgmstream->coding_type = coding_HEVAG;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x0A: {/* FMOD_SOUND_FORMAT_XMA  [Minecraft Story Mode (X360)] */
            int block_size = 0x8000; /* FSB default */

            vgmstream->codec_data = init_ffmpeg_xma2_raw(sf, fsb5.stream_offset, fsb5.stream_size, fsb5.num_samples, fsb5.channels, fsb5.sample_rate, block_size, 0);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sb, fsb5.stream_offset, fsb5.stream_size, 0, 0,0); /* samples look ok */
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x0B: {/* FMOD_SOUND_FORMAT_MPEG  [Final Fantasy X HD (PS3), Shantae Risky's Revenge (PC)] */
            mpeg_custom_config cfg = {0};

            cfg.fsb_padding = (vgmstream->channels > 2 ? 16 : 4); /* observed default */

            vgmstream->codec_data = init_mpeg_custom(sb, fsb5.stream_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_FSB, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

#ifdef VGM_USE_CELT
        case 0x0C: {  /* FMOD_SOUND_FORMAT_CELT  [BIT.TRIP Presents Runner2 (PC), Full Bore (PC)] */
            fsb5.layers = (fsb5.channels <= 2) ? 1 : (fsb5.channels+1) / 2;

            if (fsb5.layers > 1) {
                vgmstream->layout_data = build_layered_fsb5(sf, sb, &fsb5);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->coding_type = coding_CELT_FSB;
                vgmstream->layout_type = layout_layered;
            }
            else {
                vgmstream->codec_data = init_celt_fsb(vgmstream->channels, CELT_0_11_0);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->coding_type = coding_CELT_FSB;
                vgmstream->layout_type = layout_none;
            }
            break;
        }
#endif

#ifdef VGM_USE_ATRAC9
        case 0x0D: {/* FMOD_SOUND_FORMAT_AT9 */
            /* skip frame size in newer FSBs [Day of the Tentacle Remastered (Vita), Tearaway Unfolded (PS4)] */
            if (fsb5.extradata_size >= 0x08
                    && read_u8(fsb5.extradata_offset, sf) != 0xFE) { /* not ATRAC9 sync */
                fsb5.extradata_offset += 0x04;
                fsb5.extradata_size -= 0x04;
            }

            fsb5.layers = (fsb5.extradata_size / 0x04);

            if (fsb5.layers > 1) {
                /* multichannel made of various layers [Little Big Planet (Vita)] */
                vgmstream->layout_data = build_layered_fsb5(sf, sb, &fsb5);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->coding_type = coding_ATRAC9;
                vgmstream->layout_type = layout_layered;
            }
            else {
                /* standard ATRAC9, can be multichannel [Tearaway Unfolded (PS4)] */
                atrac9_config cfg = {0};

                cfg.channels = vgmstream->channels;
                cfg.config_data = read_u32be(fsb5.extradata_offset,sf);
                //cfg.encoder_delay = 0x100; //todo not used? num_samples seems to count all data

                vgmstream->codec_data = init_atrac9(&cfg);
                if (!vgmstream->codec_data) goto fail;
                vgmstream->coding_type = coding_ATRAC9;
                vgmstream->layout_type = layout_none;
            }
            break;
        }
#endif

#ifdef VGM_USE_FFMPEG
        case 0x0E: { /* FMOD_SOUND_FORMAT_XWMA  [from fsbankex tests, no known games] */
            int format,avg_bitrate, block_size;

            format      = read_u16be(fsb5.extradata_offset+0x00,sf);
            block_size  = read_u16be(fsb5.extradata_offset+0x02,sf);
            avg_bitrate = read_u32be(fsb5.extradata_offset+0x04,sf);
            /* rest: seek entries + mini seek table? */
            /* XWMA encoder only does up to 6ch (doesn't use FSB multistreams for more) */

            vgmstream->codec_data = init_ffmpeg_xwma(sf, fsb5.stream_offset, fsb5.stream_size, format, fsb5.channels, fsb5.sample_rate, avg_bitrate, block_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

#ifdef VGM_USE_VORBIS
        case 0x0F: {/* FMOD_SOUND_FORMAT_VORBIS  [Shantae Half Genie Hero (PC), Pokemon Go (iOS)] */
            vorbis_custom_config cfg = {0};

            cfg.channels = fsb5.channels;
            cfg.sample_rate = fsb5.sample_rate;
            cfg.setup_id = read_u32le(fsb5.extradata_offset,sf);
            cfg.stream_end = fsb5.stream_offset + fsb5.stream_size;

            vgmstream->codec_data = init_vorbis_custom(sb, fsb5.stream_offset, VORBIS_FSB, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_VORBIS_custom;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        case 0x10:  /* FMOD_SOUND_FORMAT_FADPCM  [Dead Rising 4 (PC), Sine Mora Ex (Switch)] */
            vgmstream->coding_type = coding_FADPCM;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x8c;
            break;

#if 0 //disabled until some game is found, can be created in the GUI tool
#ifdef VGM_USE_FFMPEG
        case 0x11: { /* FMOD_SOUND_FORMAT_OPUS */
            int skip = 312; //fsb_opus_get_encoder_delay(fsb5.stream_offset, sb); /* returns 120 but this seems correct */
            //vgmstream->num_samples -= skip;

            vgmstream->codec_data = init_ffmpeg_fsb_opus(sb, fsb5.stream_offset, fsb5.stream_size, vgmstream->channels, skip, vgmstream->sample_rate);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
#endif
        default:
            vgm_logi("FSB5: unknown codec 0x%x (report)\n", fsb5.codec);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sb, fsb5.stream_offset))
        goto fail;

    if (sb != sf) close_streamfile(sb);
    return vgmstream;

fail:
    if (sb != sf) close_streamfile(sb);
    close_vgmstream(vgmstream);
    return NULL;
}


static layered_layout_data* build_layered_fsb5(STREAMFILE* sf, STREAMFILE* sb, fsb5_header* fsb5) {
    layered_layout_data* data = NULL;
    STREAMFILE* temp_sf = NULL;
    size_t interleave, config = 0;
    int i, layer_channels;


    /* init layout */
    data = init_layout_layered(fsb5->layers);
    if (!data) goto fail;

    for (i = 0; i < fsb5->layers; i++) {
        switch (fsb5->codec) {
            case 0x0C: { /* CELT */
                /* 2ch+2ch..+1ch or 2ch+2ch..+2ch = check last layer */
                layer_channels = (i+1 == fsb5->layers && fsb5->channels % 2 == 1) ? 1 : 2;

                if (read_u32be(fsb5->stream_offset+0x00,sb) != 0x17C30DF3) /* FSB CELT frame ID */
                    goto fail;
                interleave = 0x04+0x04+read_u32le(fsb5->stream_offset+0x04,sb); /* frame size */

                //todo unknown interleave for max quality odd channel streams (found in test files)
                /* FSB5 odd channels use 2ch+2ch...+1ch streams, and the last only goes up to 0x17a, and other
                 * streams only use that max (doesn't happen for smaller frames, even channels, or FSB4)
                 * however streams other than the last seem to be padded with 0s somehow and wont work */
                if (interleave > 0x17a && (fsb5->channels % 2 == 1))
                    interleave = 0x17a;
                break;
            }

            case 0x0D: { /* ATRAC9 */
                int channel_index;
                size_t frame_size;

                /* 2ch+2ch..+1/2ch */
                config = read_u32be(fsb5->extradata_offset + 0x04*i, sf); /* ATRAC9 config */

                channel_index = ((config >> 17) & 0x7);
                frame_size = (((config >> 5) & 0x7FF) + 1) * (1 << ((config >> 3) & 0x2)); /* frame size * superframe index */
                if (channel_index > 2)
                    goto fail; /* only 1/2ch expected */

                layer_channels = (channel_index==0) ? 1 : 2;
                interleave = frame_size;
                //todo in test files with 2ch+..+1ch interleave is off (uses some strange padding)
                break;
            }

            case 0x0F: { /* VORBIS */
                layer_channels = fsb5->channels;
                interleave = 0;
                break;
            }

            default:
                goto fail;
        }

        /* build the layer VGMSTREAM */
        data->layers[i] = allocate_vgmstream(layer_channels, fsb5->loop_flag);
        if (!data->layers[i]) goto fail;

        data->layers[i]->sample_rate = fsb5->sample_rate;
        data->layers[i]->num_samples = fsb5->num_samples;
        data->layers[i]->loop_start_sample = fsb5->loop_start;
        data->layers[i]->loop_end_sample = fsb5->loop_end;


        switch (fsb5->codec) {
#ifdef VGM_USE_CELT
            case 0x0C: { /* CELT */
                data->layers[i]->codec_data = init_celt_fsb(layer_channels, CELT_0_11_0);
                if (!data->layers[i]->codec_data) goto fail;
                data->layers[i]->coding_type = coding_CELT_FSB;
                data->layers[i]->layout_type = layout_none;
                break;
            }
#endif

#ifdef VGM_USE_ATRAC9
            case 0x0D: { /* ATRAC9 */
                atrac9_config cfg = {0};

                cfg.channels = layer_channels;
                cfg.config_data = config;
                //cfg.encoder_delay = 0x100; //todo not used? num_samples seems to count all data

                data->layers[i]->codec_data = init_atrac9(&cfg);
                if (!data->layers[i]->codec_data) goto fail;
                data->layers[i]->coding_type = coding_ATRAC9;
                data->layers[i]->layout_type = layout_none;
                break;
            }
#endif

            default:
                goto fail;
        }


        temp_sf = setup_fsb5_streamfile(sb, fsb5->stream_offset, fsb5->stream_size, fsb5->layers, i, interleave);
        if (!temp_sf) goto fail;

        if (!vgmstream_open_stream(data->layers[i], temp_sf, 0x00))
            goto fail;

        close_streamfile(temp_sf);
        temp_sf = NULL;
    }

    /* setup layered VGMSTREAMs */
    if (!setup_layout_layered(data))
        goto fail;
    return data;

fail:
    close_streamfile(temp_sf);
    free_layout_layered(data);
    return NULL;
}
