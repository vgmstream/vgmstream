#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/chunks.h"
#include "ubi_lyn_streamfile.h"


/* LyN RIFF - from Ubisoft LyN engine games [Red Steel 2 (Wii), Adventures of Tintin (Multi), From Dust (Multi), Just Dance 3/4 (multi)] */
VGMSTREAM* init_vgmstream_ubi_lyn(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    uint32_t fmt_offset = 0, fmt_size = 0, data_offset = 0, data_size = 0, fact_offset = 0, fact_size = 0;
    int loop_flag = 0, channels = 0, sample_rate = 0, codec = 0;
    int32_t num_samples = 0;


    /* checks */
    if (!is_id32be(0x00,sf, "RIFF"))
        goto fail;
    if (read_u32le(0x04,sf) + 0x04 + 0x04 != get_streamfile_size(sf))
        goto fail;
    if (!is_id32be(0x08,sf, "WAVE"))
        goto fail;

    /* .sns: Red Steel 2
     * .wav: Tintin, Just Dance
     * .son: From Dust, ZombieU */
    if (!check_extensions(sf,"sns,wav,lwav,son"))
        goto fail;

    /* a slightly eccentric RIFF with custom codecs */

    /* parse chunks (reads once linearly) */
    {
        chunk_t rc = {0};

        rc.current = 0x0c;
        while (next_chunk(&rc, sf)) {

            switch(rc.type) {
                case 0x666d7420: /* "fmt " */
                    fmt_offset = rc.offset;
                    fmt_size = rc.size;

                    if (fmt_size < 0x12)
                        goto fail;
                    codec       = read_u16le(fmt_offset+0x00,sf);
                    channels    = read_u16le(fmt_offset+0x02,sf);
                    sample_rate = read_s32le(fmt_offset+0x04,sf);
                    /* 0x08: average bytes, 0x0c: block align, 0x0e: bps, etc */

                    /* fake WAVEFORMATEX, used with > 2ch */
                    if (codec == 0xFFFE) {
                        if (fmt_size < 0x28)
                            goto fail;
                        /* fake GUID with first value doubling as codec */
                        codec = read_u32le(fmt_offset+0x18,sf);
                        if (read_u32be(fmt_offset+0x1c,sf) != 0x00001000 &&
                            read_u32be(fmt_offset+0x20,sf) != 0x800000AA &&
                            read_u32be(fmt_offset+0x24,sf) != 0x00389B71) {
                            goto fail;
                        }
                    }
                    break;

                case 0x64617461: /* "data" */
                    data_offset = rc.offset;
                    data_size = rc.size;
                    break;

                case 0x66616374: /* "fact" */
                    /* always found, even with PCM (LyN subchunk seems to contain the engine version, ex. 0x0d/10) */
                    fact_offset = rc.offset;
                    fact_size = rc.size;

                    if (fact_size != 0x10 || !is_id32be(fact_offset+0x04, sf, "LyN "))
                        goto fail;
                    num_samples = read_s32le(fact_offset+0x00, sf);
                    break;

                case 0x4C795345: /* "LySE": optional, config? */
                case 0x63756520: /* "cue ": total size cue? (rare) */
                case 0x4C495354: /* "LIST": labels (rare) */
                    break;

                default:
                    /* unknown chunk: must be another RIFF */
                    goto fail;
            }
        }
    }

    if (!fmt_offset || !fmt_size || !data_offset || !data_size || !fact_offset || !fact_size)
        goto fail;

    /* most songs simply repeat; loop if it looks long enough,
     * but not too long (ex. Michael Jackson The Experience songs) */
    loop_flag = (num_samples > 20*sample_rate && num_samples < 60*3*sample_rate); /* in seconds */
    start_offset = data_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->meta_type = meta_UBI_LYN;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    switch(codec) {
        case 0x0001: /* PCM */
            vgmstream->coding_type = coding_PCM16LE; /* LE even in X360 */
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case 0x5050: /* DSP (Wii) */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x08;

            /* setup default Ubisoft coefs */
            {
                static const int16_t coef[16] = {
                        0x04ab,0xfced,0x0789,0xfedf,0x09a2,0xfae5,0x0c90,0xfac1,
                        0x084d,0xfaa4,0x0982,0xfdf7,0x0af6,0xfafa,0x0be6,0xfbf5
                };
                int i, ch;

                for (ch = 0; ch < channels; ch++) {
                    for (i = 0; i < 16; i++) {
                        vgmstream->ch[ch].adpcm_coef[i] = coef[i];
                    }
                }
            }

            break;

#ifdef VGM_USE_VORBIS
        case 0x3156:    /* Ogg (PC), interleaved 1ch (older version) [Rabbids Go Home (PC)] */
        case 0x3157: {  /* Ogg (PC), interleaved 1ch (newer version) [Adventures of Tintin (PC)] */
            size_t interleave_size;
            layered_layout_data* data = NULL;
            int i;

            if (codec == 0x3157) {
                if (read_u32le(start_offset+0x00,sf) != 1) /* id? */
                    goto fail;
                start_offset += 0x04;
            }

            interleave_size = read_u32le(start_offset+0x00,sf);
            /* interleave is adjusted so there is no smaller last block, it seems */

            vgmstream->coding_type = coding_OGG_VORBIS;
            vgmstream->layout_type = layout_layered;

            /* init layout */
            data = init_layout_layered(channels);
            if (!data) goto fail;
            vgmstream->layout_data = data;

            /* open each layer subfile */
            for (i = 0; i < channels; i++) {
                STREAMFILE* temp_sf = NULL;
                size_t logical_size = read_u32le(start_offset+0x04 + 0x04*i,sf);
                off_t layer_offset = start_offset + 0x04 + 0x04*channels;

                temp_sf = setup_ubi_lyn_streamfile(sf, layer_offset, interleave_size, i, channels, logical_size);
                if (!temp_sf) goto fail;

                data->layers[i] = init_vgmstream_ogg_vorbis(temp_sf);
                close_streamfile(temp_sf);
                if (!data->layers[i]) goto fail;

                /* could validate between layers, meh */
            }

            /* setup layered VGMSTREAMs */
            if (!setup_layout_layered(data))
                goto fail;

            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x5051: { /* MPEG (PS3/PC), interleaved 1ch */
            mpeg_custom_config cfg = {0};
            int i, frame_samples;

            /* 0x00: config? (related to chunk size? 2=Tintin, some JD4?, 3=Michael Jackson, JD4) */
            cfg.interleave = read_u32le(start_offset+0x04,sf);
            /* 0x08: CBR frame size (not counting MPEG padding) */
            /* 0x0c: bps (32) */
            frame_samples = read_s32le(start_offset+0x10,sf);

            /* skip seek tables and find actual start */
            /* table: entries per channel (1 per chunk, but not chunk-aligned?) */
            /* - 0x00: offset but same for both channels? */
            /* - 0x04: flag? (-1~-N) */
            /* - 0x06: flag? (-1~-N) */
            start_offset += 0x14;
            data_size -= 0x14;
            for (i = 0; i < channels; i++) {
                int entries = read_s32le(start_offset,sf);

                start_offset += 0x04 + entries*0x08;
                data_size -= 0x04 + entries*0x08;
            }

            cfg.data_size = data_size;

            /* last interleave is half remaining data */
            cfg.max_chunks = (cfg.data_size / (cfg.interleave * channels));
            /* somehow certain interleaves don't work:
             * - 0xB400: 0x2D00 ko, 0xE100 ok (max-1)
             * - 0x8000: 0x3ec7 ko, 0xbec7 ok (max-1)
             * - 0x8000: 0x5306 ok, 0xd306 ko (max-1) !!!
             * (doesn't seem related to anything like data size/frame samples/sample rate/etc) */
            cfg.max_chunks--;
            cfg.interleave_last = (cfg.data_size - cfg.max_chunks * cfg.interleave * channels) / channels;
            if (cfg.interleave <= 0x8000 && cfg.interleave_last > 0xD000) {
                cfg.max_chunks++;
                cfg.interleave_last = (cfg.data_size - cfg.max_chunks * cfg.interleave * channels) / channels;
            }

            /* for some reason num_samples may be +1 or +2 of max possible samples (no apparent meaning) */
            vgmstream->num_samples = vgmstream->num_samples / frame_samples * frame_samples;
            vgmstream->loop_end_sample = vgmstream->num_samples;

            vgmstream->codec_data = init_mpeg_custom(sf, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_LYN, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            break;
        }
#endif

#ifdef VGM_USE_FFMPEG
        case 0x5052: { /* MP4 AAC (WiiU), custom  */
            mp4_custom_t mp4 = {0};
            int entries;

            /* 0x00: null? */
            /* 0x04: fact samples again */
            entries = read_s32le(start_offset + 0x08, sf);

            /* has a seek/frame table then raw (non-header) AAC data */
            mp4.channels = channels;
            mp4.sample_rate = sample_rate;
            mp4.num_samples = num_samples;
            mp4.stream_offset = data_offset + (0x0c + entries * 0x04);
            mp4.stream_size = data_size - (0x0c + entries * 0x04);
            mp4.table_offset = data_offset + 0x0c;
            mp4.table_entries = entries;

            /* assumed (not in fmt's block size, fact, LyN, etc) */
            mp4.encoder_delay = 1024; /* observed, uses libaac */
            mp4.end_padding = 0;
            mp4.frame_samples = 1024;

            vgmstream->num_samples -= mp4.encoder_delay;
            vgmstream->loop_end_sample -= mp4.encoder_delay;

            vgmstream->codec_data = init_ffmpeg_mp4_custom_lyn(sf, &mp4);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case 0x0166: { /* XMA (X360), standard */
            off_t chunk_offset;
            size_t chunk_size, seek_size;

            /* skip standard XMA header + seek table */
            /* 0x00: version? no apparent differences (0x1=Just Dance 4, 0x3=others) */
            chunk_offset = start_offset + 0x04 + 0x04;
            chunk_size = read_u32le(start_offset + 0x04, sf);
            seek_size = read_u32le(chunk_offset+chunk_size, sf);
            start_offset += (0x04 + 0x04 + chunk_size + 0x04 + seek_size);
            data_size    -= (0x04 + 0x04 + chunk_size + 0x04 + seek_size);

            vgmstream->codec_data = init_ffmpeg_xma_chunk(sf, start_offset, data_size, chunk_offset, chunk_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            break;
        }
#endif

        default:
            goto fail;
    }


    if ( !vgmstream_open_stream(vgmstream,sf,start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* LyN RIFF in containers */
VGMSTREAM* init_vgmstream_ubi_lyn_container(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset;
    size_t subfile_size;

    /* LyN packs files in bigfiles, and once extracted the sound files have extra engine
     * data before the RIFF. Might as well support them in case the RIFF wasn't extracted. */

    /* checks */
    if (!check_extensions(sf,"sns,wav,lwav,son"))
        goto fail;

    /* find "RIFF" position */
    if (is_id32be(0x00,sf, "LySE") &&
        is_id32be(0x14,sf, "RIFF")) {
        subfile_offset = 0x14; /* Adventures of Tintin */
    }
    else if (read_u32le(0x00,sf) + 0x22 == get_streamfile_size(sf) &&
             is_id32be(0x20,sf, "LySE") &&
             is_id32be(0x34,sf, "RIFF")) {
        subfile_offset = 0x34; /* Michael Jackson The Experience (Wii) */
    }
    else if (read_u32le(0x00,sf)+0x20 == get_streamfile_size(sf) &&
             is_id32be(0x20,sf, "RIFF")) {
        subfile_offset = 0x20; /* Red Steel 2, From Dust, ZombieU (also has "SON\0" at 0x18) */
    }
    else {
        goto fail;
    }

    subfile_size = read_u32le(subfile_offset+0x04,sf) + 0x04 + 0x04;

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, NULL);
    if (!temp_sf) goto fail;

    vgmstream = init_vgmstream_ubi_lyn(temp_sf);

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
