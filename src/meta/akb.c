#include "meta.h"
#include "../coding/coding.h"

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
/* AKB (AAC only) - found in SQEX iOS games */
VGMSTREAM * init_vgmstream_akb_mp4(STREAMFILE *sf) {
	VGMSTREAM * vgmstream = NULL;

	size_t filesize;
	uint32_t loop_start, loop_end;

	if ((uint32_t)read_32bitBE(0, sf) != 0x414b4220) goto fail;

	loop_start = read_32bitLE(0x14, sf);
	loop_end = read_32bitLE(0x18, sf);

	filesize = get_streamfile_size( sf );

	vgmstream = init_vgmstream_mp4_aac_offset( sf, 0x20, filesize - 0x20 );
	if ( !vgmstream ) goto fail;

	if ( loop_start || loop_end ) {
		vgmstream->loop_flag = 1;
		vgmstream->loop_start_sample = loop_start;
		vgmstream->loop_end_sample = loop_end;
	}

	return vgmstream;

fail:
	return NULL;
}
#endif


/* AKB - found in SQEX iOS games */
VGMSTREAM * init_vgmstream_akb(STREAMFILE *sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, extradata_offset = 0;
    size_t stream_size, header_size, subheader_size = 0, extradata_size = 0;
    int loop_flag = 0, channel_count, codec, sample_rate;
    int num_samples, loop_start, loop_end;


    /* checks */
    if ( !check_extensions(sf, "akb") )
        goto fail;
    if (read_32bitBE(0x00,sf) != 0x414B4220) /* "AKB " */
        goto fail;
    if (read_32bitLE(0x08,sf) != get_streamfile_size(sf))
        goto fail;

    /* 0x04(1): version */
    header_size = read_16bitLE(0x06,sf);

    codec         =  read_8bit(0x0c,sf);
    channel_count =  read_8bit(0x0d,sf);
    sample_rate = (uint16_t)read_16bitLE(0x0e,sf);
    num_samples = read_32bitLE(0x10,sf);
    loop_start  = read_32bitLE(0x14,sf);
    loop_end    = read_32bitLE(0x18,sf);

    /* possibly more complex, see AKB2 */
    if (header_size >= 0x44) { /* v2+ */
        extradata_size = read_16bitLE(0x1c,sf);
        /* 0x20+: config? (pan, volume) */
        subheader_size = read_16bitLE(0x28,sf);
        /* 0x24: file_id? */
        /* 0x2b: encryption bitflag if version > 2? */
        extradata_offset = header_size + subheader_size;
        start_offset = extradata_offset + extradata_size;
    }
    else { /* v0 */
        start_offset = header_size;
    }

    stream_size = get_streamfile_size(sf) - start_offset;
    loop_flag = (loop_end > loop_start);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->meta_type = meta_AKB;


    switch (codec) {
        case 0x02: { /* MSADPCM [Dragon Quest II (iOS) sfx] */
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = read_16bitLE(extradata_offset + 0x02,sf);

            /* adjusted samples; bigger or smaller than base samples, akb lib uses these fields instead
             * (base samples may have more than possible and read over file size otherwise, very strange)
             * loop_end seems to exist even with loop disabled */
            vgmstream->num_samples       = read_32bitLE(extradata_offset + 0x04, sf);
            vgmstream->loop_start_sample = read_32bitLE(extradata_offset + 0x08, sf);
            vgmstream->loop_end_sample   = read_32bitLE(extradata_offset + 0x0c, sf);
            break;
        }

#ifdef VGM_USE_VORBIS
        case 0x05: { /* Ogg Vorbis [Final Fantasy VI (iOS), Dragon Quest II-VI (iOS)] */
            VGMSTREAM *ogg_vgmstream = NULL;
            ogg_vorbis_meta_info_t ovmi = {0};

            ovmi.meta_type = vgmstream->meta_type;
            ovmi.stream_size = stream_size;
            /* extradata + 0x04: Ogg loop start offset */
            /* oggs have loop info in the comments */

            ogg_vgmstream = init_vgmstream_ogg_vorbis_config(sf, start_offset, &ovmi);
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

        case 0x01: /* PCM16LE */
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


/* AKB2 - found in later SQEX iOS games */
VGMSTREAM * init_vgmstream_akb2(STREAMFILE *sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, material_offset, extradata_offset;
    size_t material_size, extradata_size, stream_size;
    int loop_flag = 0, channel_count, encryption_flag, codec, sample_rate, num_samples, loop_start, loop_end;
    int total_subsongs, target_subsong = sf->stream_index;

    /* check extensions */
    if ( !check_extensions(sf, "akb") )
        goto fail;

    /* checks */
    if (read_32bitBE(0x00,sf) != 0x414B4232) /* "AKB2" */
        goto fail;
    if (read_32bitLE(0x08,sf) != get_streamfile_size(sf))
        goto fail;
    /* 0x04: version */

    /* parse tables */
    {
        off_t table_offset;
        size_t table_size, entry_size;
        off_t akb_header_size = read_16bitLE(0x06, sf);
        int table_count = read_8bit(0x0c, sf);

        /* probably each table has its type somewhere, but only seen last table = sound table */
        if (table_count > 2) /* 2 only seen in some Mobius FF sound banks */
            goto fail;
        entry_size = 0x10; /* technically every entry/table has its own size but to simplify... */

        table_offset = read_32bitLE(akb_header_size + (table_count-1)*entry_size + 0x04, sf);
        table_size = read_16bitLE(table_offset + 0x02, sf);

        total_subsongs = read_8bit(table_offset + 0x0f, sf); /* can contain 0 entries too */
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        material_offset = table_offset + read_32bitLE(table_offset + table_size + (target_subsong-1)*entry_size + 0x04, sf);
    }

    /** stream header (material) **/
    /* 0x00: 0? */
    codec           =    read_8bit(material_offset+0x01,sf);
    channel_count   =    read_8bit(material_offset+0x02,sf);
    encryption_flag =    read_8bit(material_offset+0x03,sf);
    material_size   = read_16bitLE(material_offset+0x04,sf);
    sample_rate     = (uint16_t)read_16bitLE(material_offset+0x06,sf);
    stream_size     = read_32bitLE(material_offset+0x08,sf);
    num_samples     = read_32bitLE(material_offset+0x0c,sf);

    loop_start      = read_32bitLE(material_offset+0x10,sf);
    loop_end        = read_32bitLE(material_offset+0x14,sf);
    extradata_size  = read_32bitLE(material_offset+0x18,sf);
    /* rest: ? (empty or 0x3f80) */

    loop_flag = (loop_end > loop_start);
    extradata_offset = material_offset + material_size;
    start_offset = material_offset +  material_size + extradata_size;

    if (encryption_flag & 0x08)
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
            vgmstream->frame_size = read_16bitLE(extradata_offset + 0x02, sf);

            /* adjusted samples; bigger or smaller than base samples, akb lib uses these fields instead
             * (base samples may have more than possible and read over file size otherwise, very strange)
             * loop_end seems to exist even with loop disabled */
            vgmstream->num_samples       = read_32bitLE(extradata_offset + 0x04, sf);
            vgmstream->loop_start_sample = read_32bitLE(extradata_offset + 0x08, sf);
            vgmstream->loop_end_sample   = read_32bitLE(extradata_offset + 0x0c, sf);
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
            vgmstream->num_samples       = read_32bitLE(material_offset+0x0c,sf);//num_samples;
            vgmstream->loop_start_sample = read_32bitLE(material_offset+0x10,sf);//loop_start;
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
