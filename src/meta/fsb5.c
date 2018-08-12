#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "fsb5_interleave_streamfile.h"


typedef struct {
    int total_subsongs;
    int version;
    int codec;
    int flags;

    int channels;
    int sample_rate;
    int32_t num_samples;
    int32_t loop_start;
    int32_t loop_end;
    int loop_flag;

    off_t sample_header_offset;
    size_t sample_header_size;
    size_t name_table_size;
    size_t sample_data_size;
    size_t base_header_size;

    off_t extradata_offset;
    size_t extradata_size;

    off_t stream_offset;
    size_t stream_size;
    off_t name_offset;
} fsb5_header;

/* ********************************************************************************** */

#ifdef VGM_USE_CELT
static layered_layout_data* build_layered_fsb5_celt(STREAMFILE *streamFile, fsb5_header* fsb5, celt_lib_t version);
#endif
static layered_layout_data* build_layered_fsb5_atrac9(STREAMFILE *streamFile, fsb5_header* fsb5, off_t configs_offset, size_t configs_size);

/* FSB5 - FMOD Studio multiplatform format */
VGMSTREAM * init_vgmstream_fsb5(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    fsb5_header fsb5 = {0};
    int target_subsong = streamFile->stream_index;
    int i;


    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"fsb"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x46534235) /* "FSB5" */
        goto fail;

    /* 0x00 is rare (seen in Tales from Space Vita) */
    fsb5.version = read_32bitLE(0x04,streamFile);
    if (fsb5.version != 0x00 && fsb5.version != 0x01) goto fail;

    fsb5.total_subsongs     = read_32bitLE(0x08,streamFile);
    fsb5.sample_header_size = read_32bitLE(0x0C,streamFile);
    fsb5.name_table_size    = read_32bitLE(0x10,streamFile);
    fsb5.sample_data_size   = read_32bitLE(0x14,streamFile);
    fsb5.codec              = read_32bitLE(0x18,streamFile);
    /* version 0x01 - 0x1c(4): zero,  0x24(16): hash,  0x34(8): unk
     * version 0x00 has an extra field (always 0?) at 0x1c */
    if (fsb5.version == 0x01) {
        /* found by tests and assumed to be flags, no games known */
        fsb5.flags = read_32bitLE(0x20,streamFile);
    }
    fsb5.base_header_size   = (fsb5.version==0x00) ? 0x40 : 0x3C;

    if ((fsb5.sample_header_size + fsb5.name_table_size + fsb5.sample_data_size + fsb5.base_header_size) != get_streamfile_size(streamFile)) {
        VGM_LOG("FSB5: bad size (%x + %x + %x + %x != %x)\n", fsb5.sample_header_size, fsb5.name_table_size, fsb5.sample_data_size, fsb5.base_header_size, get_streamfile_size(streamFile));
        goto fail;
    }

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong > fsb5.total_subsongs || fsb5.total_subsongs <= 0) goto fail;

    fsb5.sample_header_offset = fsb5.base_header_size;

    /* find target stream header and data offset, and read all needed values for later use
     *  (reads one by one as the size of a single stream header is variable) */
    for (i = 1; i <= fsb5.total_subsongs; i++) {
        size_t stream_header_size = 0;
        off_t data_offset = 0;
        uint32_t sample_mode1, sample_mode2; /* maybe one uint64? */

        sample_mode1 = (uint32_t)read_32bitLE(fsb5.sample_header_offset+0x00,streamFile);
        sample_mode2 = (uint32_t)read_32bitLE(fsb5.sample_header_offset+0x04,streamFile);
        stream_header_size += 0x08;

        /* get samples */
        fsb5.num_samples  = ((sample_mode2 >> 2) & 0x3FFFFFFF); /* bits2: 31..2 (30) */

        /* get offset inside data section */
        /* up to 0x07FFFFFF * 0x20 = full 32b offset 0xFFFFFFE0 */
        data_offset   = ((sample_mode2 & 0x03) << 25) | ((sample_mode1 >> 7) & 0x1FFFFFF) << 5; /* bits2: 1..0 (2) | bits1: 31..8 (25) */

        /* get channels */
        switch ((sample_mode1 >> 5) & 0x03) { /* bits1: 7..6 (2) */
            case 0:  fsb5.channels = 1; break;
            case 1:  fsb5.channels = 2; break;
            case 2:  fsb5.channels = 6; break; /* some Dark Souls 2 MPEG; some IMA ADPCM */
            case 3:  fsb5.channels = 8; break; /* some IMA ADPCM */
            /* other channels (ex. 4/10/12ch) use 0 here + set extra flags */
            default: /* not possible */
                goto fail;
        }

        /* get sample rate  */
        switch ((sample_mode1 >> 1) & 0x0f) { /* bits1: 5..1 (4) */
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
        if (sample_mode1 & 0x01) { /* bits1: 0 (1) */
            off_t extraflag_offset = fsb5.sample_header_offset+0x08;
            uint32_t extraflag, extraflag_type, extraflag_size, extraflag_end;

            do {
                extraflag = read_32bitLE(extraflag_offset,streamFile);
                extraflag_type = (extraflag >> 25) & 0x7F; /* bits 32..26 (7) */
                extraflag_size = (extraflag >> 1) & 0xFFFFFF; /* bits 25..1 (24)*/
                extraflag_end  = (extraflag & 0x01); /* bit 0 (1) */

                switch(extraflag_type) {
                    case 0x01:  /* channels */
                        fsb5.channels = read_8bit(extraflag_offset+0x04,streamFile);
                        break;
                    case 0x02:  /* sample rate */
                        fsb5.sample_rate = read_32bitLE(extraflag_offset+0x04,streamFile);
                        break;
                    case 0x03:  /* loop info */
                        fsb5.loop_start = read_32bitLE(extraflag_offset+0x04,streamFile);
                        if (extraflag_size > 0x04) /* probably not needed */
                            fsb5.loop_end = read_32bitLE(extraflag_offset+0x08,streamFile);

                        /* when start is 0 seems the song repeats with no real looping (ex. Sonic Boom Fire & Ice jingles) */
                        fsb5.loop_flag = (fsb5.loop_start != 0x00);
                        break;
                    case 0x04:  /* free comment, or maybe SFX info */
                        break;
                  //case 0x05:  /* Unknown (32b) */ //todo multistream marker?
                  //    /* found in Tearaway Vita, value 0, first stream only */
                  //    break;
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
                         * (xN entries)
                         */
                        break;
                  //case 0x0d:  /* Unknown (32b) */
                  //    /* found in some XMA2/Vorbis/FADPCM */
                  //    break;
                    default:
                        VGM_LOG("FSB5: unknown extraflag 0x%x at %lx + 0x04 (size 0x%x)\n", extraflag_type, extraflag_offset, extraflag_size);
                        break;
                }

                extraflag_offset += 0x04 + extraflag_size;
                stream_header_size += 0x04 + extraflag_size;
            } while (extraflag_end != 0x00);
        }

        /* stream found */
        if (i == target_subsong) {
            fsb5.stream_offset = fsb5.base_header_size + fsb5.sample_header_size + fsb5.name_table_size + data_offset;

            /* get stream size from next stream offset or full size if there is only one */
            if (i == fsb5.total_subsongs) {
                fsb5.stream_size = fsb5.sample_data_size - data_offset;
            }
            else {
                off_t next_data_offset;
                uint32_t next_sample_mode1, next_sample_mode2;
                next_sample_mode1 = (uint32_t)read_32bitLE(fsb5.sample_header_offset+stream_header_size+0x00,streamFile);
                next_sample_mode2 = (uint32_t)read_32bitLE(fsb5.sample_header_offset+stream_header_size+0x04,streamFile);
                next_data_offset = ((next_sample_mode2 & 0x03) << 25) | ((next_sample_mode1 >> 7) & 0x1FFFFFF) << 5;

                fsb5.stream_size = next_data_offset - data_offset;
            }

            break;
        }

        /* continue searching */
        fsb5.sample_header_offset += stream_header_size;
    }
    /* target stream not found*/
    if (!fsb5.stream_offset || !fsb5.stream_size) goto fail;

    /* get stream name */
    if (fsb5.name_table_size) {
        off_t name_suboffset = fsb5.base_header_size + fsb5.sample_header_size + 0x04*(target_subsong-1);
        fsb5.name_offset = fsb5.base_header_size + fsb5.sample_header_size + read_32bitLE(name_suboffset,streamFile);
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(fsb5.channels,fsb5.loop_flag);
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
        read_string(vgmstream->stream_name,STREAM_NAME_SIZE, fsb5.name_offset,streamFile);

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
            VGM_LOG("FSB5: FMOD_SOUND_FORMAT_PCM24 found\n");
            goto fail;

        case 0x04:  /* FMOD_SOUND_FORMAT_PCM32 */
            VGM_LOG("FSB5: FMOD_SOUND_FORMAT_PCM32 found\n");
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
	        dsp_read_coefs_be(vgmstream,streamFile,fsb5.extradata_offset,0x2E);
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
        case 0x0A: {/* FMOD_SOUND_FORMAT_XMA  [Dark Souls 2 (X360)] */
            uint8_t buf[0x100];
            int bytes, block_size, block_count;

            block_size = 0x8000; /* FSB default */
            block_count = fsb5.stream_size / block_size + (fsb5.stream_size % block_size ? 1 : 0);

            bytes = ffmpeg_make_riff_xma2(buf, 0x100, vgmstream->num_samples, fsb5.stream_size, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, fsb5.stream_offset,fsb5.stream_size);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x0B: {/* FMOD_SOUND_FORMAT_MPEG  [Final Fantasy X HD (PS3), Shantae Risky's Revenge (PC)] */
            mpeg_custom_config cfg = {0};

            cfg.fsb_padding = (vgmstream->channels > 2 ? 16 : 4); /* observed default */

            vgmstream->codec_data = init_mpeg_custom(streamFile, fsb5.stream_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_FSB, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

#ifdef VGM_USE_CELT
        case 0x0C: {  /* FMOD_SOUND_FORMAT_CELT  [BIT.TRIP Presents Runner2 (PC), Full Bore (PC)] */
            int is_multistream = fsb5.channels > 2;

            if (is_multistream) {
                vgmstream->layout_data = build_layered_fsb5_celt(streamFile, &fsb5, CELT_0_11_0);
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
            int is_multistream;
            off_t configs_offset = fsb5.extradata_offset;
            size_t configs_size = fsb5.extradata_size;


            /* skip frame size in newer FSBs [Day of the Tentacle Remastered (Vita), Tearaway Unfolded (PS4)] */
            if (configs_size >= 0x08 && (uint8_t)read_8bit(configs_offset, streamFile) != 0xFE) { /* ATRAC9 sync */
                configs_offset += 0x04;
                configs_size -= 0x04;
            }

            is_multistream = (configs_size / 0x04) > 1;

            if (is_multistream) {
                /* multichannel made of various streams [Little Big Planet (Vita)] */
                vgmstream->layout_data = build_layered_fsb5_atrac9(streamFile, &fsb5, configs_offset, configs_size);
                if (!vgmstream->layout_data) goto fail;
                vgmstream->coding_type = coding_ATRAC9;
                vgmstream->layout_type = layout_layered;
            }
            else {
                /* standard ATRAC9, can be multichannel [Tearaway Unfolded (PS4)] */
                atrac9_config cfg = {0};

                cfg.channels = vgmstream->channels;
                cfg.config_data = read_32bitBE(configs_offset,streamFile);
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
            uint8_t buf[0x100];
            int bytes, format, average_bps, block_align;

            format = read_16bitBE(fsb5.extradata_offset+0x00,streamFile);
            block_align = (uint16_t)read_16bitBE(fsb5.extradata_offset+0x02,streamFile);
            average_bps = (uint32_t)read_32bitBE(fsb5.extradata_offset+0x04,streamFile);
            /* rest: seek entries + mini seek table? */
            /* XWMA encoder only does up to 6ch (doesn't use FSB multistreams for more) */

            bytes = ffmpeg_make_riff_xwma(buf,0x100, format, fsb5.stream_size, vgmstream->channels, vgmstream->sample_rate, average_bps, block_align);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, fsb5.stream_offset,fsb5.stream_size);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

#ifdef VGM_USE_VORBIS
        case 0x0F: {/* FMOD_SOUND_FORMAT_VORBIS  [Shantae Half Genie Hero (PC), Pokemon Go (iOS)] */
            vorbis_custom_config cfg = {0};

            cfg.channels = vgmstream->channels;
            cfg.sample_rate = vgmstream->sample_rate;
            cfg.setup_id = read_32bitLE(fsb5.extradata_offset,streamFile);

            vgmstream->layout_type = layout_none;
            vgmstream->coding_type = coding_VORBIS_custom;
            vgmstream->codec_data = init_vorbis_custom(streamFile, fsb5.stream_offset, VORBIS_FSB, &cfg);
            if (!vgmstream->codec_data) goto fail;

            break;
        }
#endif

        case 0x10:  /* FMOD_SOUND_FORMAT_FADPCM  [Dead Rising 4 (PC), Sine Mora Ex (Switch)] */
            vgmstream->coding_type = coding_FADPCM;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x8c;
            break;

        default:
            VGM_LOG("FSB5: unknown codec %x found\n", fsb5.codec);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,fsb5.stream_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

#ifdef VGM_USE_CELT
static layered_layout_data* build_layered_fsb5_celt(STREAMFILE *streamFile, fsb5_header* fsb5, celt_lib_t version) {
    layered_layout_data* data = NULL;
    STREAMFILE* temp_streamFile = NULL;
    int i, layers = (fsb5->channels+1) / 2;
    size_t interleave;

    if (read_32bitBE(fsb5->stream_offset+0x00,streamFile) != 0x17C30DF3) /* FSB CELT frame ID */
        goto fail;
    interleave = 0x04+0x04+read_32bitLE(fsb5->stream_offset+0x04,streamFile); /* frame size */

    //todo unknown interleave for max quality odd channel streams (found in test files)
    /* FSB5 odd channels use 2ch+2ch...+1ch streams, and the last only goes up to 0x17a, and other
     * streams only use that max (doesn't happen for smaller frames, even channels, or FSB4)
     * however streams other than the last seem to be padded with 0s somehow and wont work */
    if (interleave > 0x17a && (fsb5->channels % 2 == 1))
        interleave = 0x17a;


    /* init layout */
    data = init_layout_layered(layers);
    if (!data) goto fail;

    /* open each layer subfile (1/2ch CELT streams: 2ch+2ch..+1ch or 2ch+2ch..+2ch) */
    for (i = 0; i < layers; i++) {
        int layer_channels = (i+1 == layers && fsb5->channels % 2 == 1)
                ? 1 : 2; /* last layer can be 1/2ch */

        /* build the layer VGMSTREAM */
        data->layers[i] = allocate_vgmstream(layer_channels, fsb5->loop_flag);
        if (!data->layers[i]) goto fail;

        data->layers[i]->sample_rate = fsb5->sample_rate;
        data->layers[i]->num_samples = fsb5->num_samples;
        data->layers[i]->loop_start_sample = fsb5->loop_start;
        data->layers[i]->loop_end_sample = fsb5->loop_end;

        data->layers[i]->codec_data = init_celt_fsb(layer_channels, version);
        if (!data->layers[i]->codec_data) goto fail;
        data->layers[i]->coding_type = coding_CELT_FSB;
        data->layers[i]->layout_type = layout_none;

        temp_streamFile = setup_fsb5_interleave_streamfile(streamFile, fsb5->stream_offset, fsb5->stream_size, layers, i, FSB5_INT_CELT, interleave);
        if (!temp_streamFile) goto fail;

        if (!vgmstream_open_stream(data->layers[i], temp_streamFile, 0x00))
            goto fail;
    }

    /* setup layered VGMSTREAMs */
    if (!setup_layout_layered(data))
        goto fail;
    close_streamfile(temp_streamFile);
    return data;

fail:
    close_streamfile(temp_streamFile);
    free_layout_layered(data);
    return NULL;
}
#endif

static layered_layout_data* build_layered_fsb5_atrac9(STREAMFILE *streamFile, fsb5_header* fsb5, off_t configs_offset, size_t configs_size) {
    layered_layout_data* data = NULL;
    STREAMFILE* temp_streamFile = NULL;
    int i, layers = (configs_size / 0x04);
    size_t interleave = 0;


    /* init layout */
    data = init_layout_layered(layers);
    if (!data) goto fail;

    /* open each layer subfile (2ch+2ch..+1/2ch) */
    for (i = 0; i < layers; i++) {
        uint32_t config = read_32bitBE(configs_offset + 0x04*i, streamFile);
        int channel_index, layer_channels;
        size_t frame_size;


        /* parse ATRAC9 config (see VGAudio docs) */
        channel_index = ((config >> 17) & 0x7);
        frame_size = (((config >> 5) & 0x7FF) + 1) * (1 << ((config >> 3) & 0x2)); /* frame size * superframe index */
        if (channel_index > 2)
            goto fail; /* only 1/2ch expected */

        layer_channels = (channel_index==0) ? 1 : 2;
        if (interleave == 0)
            interleave = frame_size;
        //todo in test files with 2ch+..+1ch interleave is off (uses some strange padding)


        /* build the layer VGMSTREAM */
        data->layers[i] = allocate_vgmstream(layer_channels, fsb5->loop_flag);
        if (!data->layers[i]) goto fail;

        data->layers[i]->sample_rate = fsb5->sample_rate;
        data->layers[i]->num_samples = fsb5->num_samples;
        data->layers[i]->loop_start_sample = fsb5->loop_start;
        data->layers[i]->loop_end_sample = fsb5->loop_end;

#ifdef VGM_USE_ATRAC9
        {
            atrac9_config cfg = {0};

            cfg.channels = layer_channels;
            cfg.config_data = config;
            //cfg.encoder_delay = 0x100; //todo not used? num_samples seems to count all data

            data->layers[i]->codec_data = init_atrac9(&cfg);
            if (!data->layers[i]->codec_data) goto fail;
            data->layers[i]->coding_type = coding_ATRAC9;
            data->layers[i]->layout_type = layout_none;
        }
#else
        goto fail;
#endif

        temp_streamFile = setup_fsb5_interleave_streamfile(streamFile, fsb5->stream_offset, fsb5->stream_size, layers, i, FSB5_INT_ATRAC9, interleave);
        if (!temp_streamFile) goto fail;

        if (!vgmstream_open_stream(data->layers[i], temp_streamFile, 0x00))
            goto fail;
    }

    /* setup layered VGMSTREAMs */
    if (!setup_layout_layered(data))
        goto fail;
    close_streamfile(temp_streamFile);
    return data;

fail:
    close_streamfile(temp_streamFile);
    free_layout_layered(data);
    return NULL;
}
