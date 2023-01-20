#include "meta.h"
#include "../coding/coding.h"
#include "sqex_streamfile.h"


/* AKB - found in SQEX 'sdlib' iOS/Android games */
VGMSTREAM* init_vgmstream_akb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, extradata_offset = 0;
    size_t stream_size, header_size, subheader_size = 0, extradata_size = 0;
    int loop_flag = 0, channels, codec, sample_rate, version, flags = 0;
    int num_samples, loop_start, loop_end;


    /* checks */
    if (!is_id32be(0x00,sf, "AKB "))
        goto fail;

    if (!check_extensions(sf, "akb"))
        goto fail;

    version = read_u8(0x04,sf); /* 00=TWEWY, 02=DQs, 03=FFAgito */
    /* 0x05(1); unused? */
    header_size = read_u16le(0x06,sf);
    if (read_u32le(0x08,sf) != get_streamfile_size(sf))
        goto fail;

    /* material info, though can only hold 1 */
    codec       =    read_u8(0x0c,sf);
    channels    =    read_u8(0x0d,sf);
    sample_rate = read_u16le(0x0e,sf);
    num_samples = read_s32le(0x10,sf);
    loop_start  = read_s32le(0x14,sf);
    loop_end    = read_s32le(0x18,sf);

    /* possibly more complex, see AKB2 */
    if (header_size >= 0x44) { /* v2+ */
        extradata_size = read_u16le(0x1c,sf);
        /* 0x20+: config? (pan, volume) */
        subheader_size = read_u16le(0x28,sf);
        /* 0x24: file_id? */
        /* 0x28: */
        /* 0x29: */
        /* 0x2a: */

        /* flags:
         * 1: (v2+) enable random volume
         * 2: (v2+) enable random pitch
         * 4: (v2+) enable random pan
         * 8: (v3+) encryption (for MS-ADPCM / Ogg) [Final Fantasy Agito (Android)-ogg bgm only, other sounds don't use AKB] */
        flags = read_u8(0x2B,sf);
        /* 0x2c: max random volume */
        /* 0x30: min random volume */
        /* 0x34: max random pitch */
        /* 0x38: min random pitch */
        /* 0x3c: max random pan */
        /* 0x40: min random pan */

        extradata_offset = header_size + subheader_size;
        start_offset = extradata_offset + extradata_size;
    }
    else { /* v0 */
        start_offset = header_size;
    }

    stream_size = get_streamfile_size(sf) - start_offset;
    loop_flag = (loop_end > loop_start);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_AKB;
    vgmstream->sample_rate = sample_rate;

    switch (codec) {
        case 0x02: { /* MSADPCM [Dragon Quest II (iOS) sfx] */
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = read_u16le(extradata_offset + 0x02,sf);

            /* encryption, untested but should be the same as Ogg */
            if (version >= 3 && (flags & 8))
                goto fail;

            /* adjusted samples; bigger or smaller than base samples, akb lib uses these fields instead
             * (base samples may have more than possible and read over file size otherwise, very strange)
             * loop_end seems to exist even with loop disabled */
            vgmstream->num_samples       = read_s32le(extradata_offset + 0x04, sf);
            vgmstream->loop_start_sample = read_s32le(extradata_offset + 0x08, sf);
            vgmstream->loop_end_sample   = read_s32le(extradata_offset + 0x0c, sf);
            break;
        }

#ifdef VGM_USE_VORBIS
        case 0x05: { /* Ogg Vorbis [Final Fantasy VI (iOS), Dragon Quest II-VI (iOS)] */
            STREAMFILE* temp_sf;
            VGMSTREAM *ogg_vgmstream = NULL;
            ogg_vorbis_meta_info_t ovmi = {0};

            ovmi.meta_type = vgmstream->meta_type;
            ovmi.stream_size = stream_size;
            /* extradata + 0x04: Ogg loop start offset */
            /* oggs have loop info in the comments */

            /* enable encryption */
            if (version >= 3 && (flags & 8)) {
                temp_sf = setup_sqex_streamfile(sf, start_offset, stream_size, 1, 0x00, 0x00, "ogg");
                if (!temp_sf) goto fail;

                ogg_vgmstream = init_vgmstream_ogg_vorbis_config(temp_sf, 0x00, &ovmi);
                close_streamfile(temp_sf);
            }
            else {
                ogg_vgmstream = init_vgmstream_ogg_vorbis_config(sf, start_offset, &ovmi);
            }

            if (ogg_vgmstream) {
                close_vgmstream(vgmstream);
                return ogg_vgmstream;
            }
            else {
                goto fail;
            }

            break;
        }

#elif defined(VGM_USE_FFMPEG)
        /* Alt decoding without libvorbis (minor number of beginning samples difference).
         * Otherwise same output with (inaudible) +-1 lower byte differences due to rounding. */
        case 0x05: { /* Ogg Vorbis [Final Fantasy VI (iOS), Dragon Quest II-VI (iOS)] */
            vgmstream->codec_data = init_ffmpeg_offset(sf, start_offset,stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            /* These oggs have loop info in the comments, too */

            vgmstream->num_samples = num_samples;
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;

            break;
        }
#endif

#ifdef VGM_USE_FFMPEG
        case 0x06: { /* M4A with AAC [The World Ends with You (iPad)] */
            /* init_vgmstream_akb_mp4 above has priority, but this works fine too */
            vgmstream->codec_data = init_ffmpeg_offset(sf, start_offset,stream_size-start_offset);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = num_samples;
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;
            /* bad total samples (some kind of duration? probably should be loop_end though) */
            if (loop_flag)
                vgmstream->num_samples = loop_end+1;

            /* loops are pre-adjusted with 2112 encoder delay (ex. TWEWY B04's loop_start=45) */
            break;
        }
#endif

        case 0x01: /* PCM16LE (from debugging, not seen) */
        default:
            goto fail;
    }

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* AKB2 - found in later SQEX 'sdlib' iOS/Android games */
VGMSTREAM* init_vgmstream_akb2(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, material_offset, extradata_offset;
    size_t material_size, extradata_size, stream_size;
    int loop_flag = 0, channel_count, flags, codec, sample_rate, num_samples, loop_start, loop_end;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00,sf, "AKB2"))
        goto fail;

    if (!check_extensions(sf, "akb"))
        goto fail;

    /* 0x04: version */
    if (read_u32le(0x08,sf) != get_streamfile_size(sf))
        goto fail;

    /* parse tables */
    {
        off_t table_offset;
        size_t table_size, entry_size;
        off_t akb_header_size = read_u16le(0x06, sf);
        int table_count = read_u8(0x0c, sf);

        /* probably each table has its type somewhere, but only seen last table = sound table */
        if (table_count > 2) /* 2 only seen in some Mobius FF sound banks */
            goto fail;
        entry_size = 0x10; /* technically every entry/table has its own size but to simplify... */

        table_offset = read_u32le(akb_header_size + (table_count-1)*entry_size + 0x04, sf);
        table_size = read_u16le(table_offset + 0x02, sf);

        total_subsongs = read_u8(table_offset + 0x0f, sf); /* can contain 0 entries too */
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        material_offset = table_offset + read_u32le(table_offset + table_size + (target_subsong-1)*entry_size + 0x04, sf);
    }

    /** stream header (material) **/
    /* 0x00: 0? */
    codec           =    read_u8(material_offset+0x01,sf);
    channel_count   =    read_u8(material_offset+0x02,sf);
    flags           =    read_u8(material_offset+0x03,sf);
    material_size   = read_u16le(material_offset+0x04,sf);
    sample_rate     = read_u16le(material_offset+0x06,sf);
    stream_size     = read_u32le(material_offset+0x08,sf);
    num_samples     = read_s32le(material_offset+0x0c,sf);

    loop_start      = read_s32le(material_offset+0x10,sf);
    loop_end        = read_s32le(material_offset+0x14,sf);
    extradata_size  = read_u32le(material_offset+0x18,sf);
    /* rest: ? (empty, floats or 0x3f80) */

    loop_flag = (loop_end > loop_start);
    extradata_offset = material_offset + material_size;
    start_offset = material_offset +  material_size + extradata_size;

    /* encrypted, not seen (see AKB flags) */
    if (flags & 0x08)
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    vgmstream->meta_type = meta_AKB;

    switch (codec) {
        case 0x01: /* PCM16LE [Mobius: Final Fantasy (Android)] */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples       = num_samples;
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample   = loop_end;
            break;


        case 0x02: { /* MSADPCM [The Irregular at Magic High School Lost Zero (Android)] */
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = read_u16le(extradata_offset + 0x02, sf);

            /* adjusted samples; bigger or smaller than base samples, akb lib uses these fields instead
             * (base samples may have more than possible and read over file size otherwise, very strange)
             * loop_end seems to exist even with loop disabled */
            vgmstream->num_samples       = read_s32le(extradata_offset + 0x04, sf);
            vgmstream->loop_start_sample = read_s32le(extradata_offset + 0x08, sf);
            vgmstream->loop_end_sample   = read_s32le(extradata_offset + 0x0c, sf);
            break;
        }

#ifdef VGM_USE_VORBIS
        case 0x05: { /* Ogg Vorbis [The World Ends with You (iOS / latest update)] */
            VGMSTREAM *ogg_vgmstream = NULL;
            ogg_vorbis_meta_info_t ovmi = {0};

            ovmi.meta_type = vgmstream->meta_type;
            ovmi.total_subsongs = total_subsongs;
            ovmi.stream_size = stream_size;

            ogg_vgmstream = init_vgmstream_ogg_vorbis_config(sf, start_offset, &ovmi);
            if (ogg_vgmstream) {
                ogg_vgmstream->num_streams = vgmstream->num_streams;
                ogg_vgmstream->stream_size = vgmstream->stream_size;

                close_vgmstream(vgmstream);
                return ogg_vgmstream;
            }
            else {
                goto fail;
            }

            break;
        }

#elif defined(VGM_USE_FFMPEG)
        /* Alt decoding without libvorbis (minor number of beginning samples difference).
         * Otherwise same output with (inaudible) +-1 lower byte differences due to rounding. */
        case 0x05: { /* Ogg Vorbis [The World Ends with You (iOS / latest update)] */
            ffmpeg_codec_data *ffmpeg_data;

            ffmpeg_data = init_ffmpeg_offset(sf, start_offset,stream_size);
            if ( !ffmpeg_data ) goto fail;

            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* When loop_flag num_samples may be much larger than real num_samples (it's fine when looping is off)
             * Actual num_samples would be loop_end_sample+1, but more testing is needed */
            vgmstream->num_samples       = read_s32le(material_offset+0x0c,sf);//num_samples;
            vgmstream->loop_start_sample = read_s32le(material_offset+0x10,sf);//loop_start;
            vgmstream->loop_end_sample   = loop_end;
            break;
        }
#endif

        default:
            goto fail;
    }

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, sf, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
