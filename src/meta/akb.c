#include "meta.h"
#include "../coding/coding.h"

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
/* AKB (AAC only) - found in SQEX iOS games */
VGMSTREAM * init_vgmstream_akb(STREAMFILE *streamFile) {
	VGMSTREAM * vgmstream = NULL;

	size_t filesize;
	uint32_t loop_start, loop_end;

	if ((uint32_t)read_32bitBE(0, streamFile) != 0x414b4220) goto fail;

	loop_start = read_32bitLE(0x14, streamFile);
	loop_end = read_32bitLE(0x18, streamFile);

	filesize = get_streamfile_size( streamFile );

	vgmstream = init_vgmstream_mp4_aac_offset( streamFile, 0x20, filesize - 0x20 );
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
VGMSTREAM * init_vgmstream_akb_multi(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, extra_data_offset = 0;
    size_t file_size, header_size, extra_header_size = 0, extra_data_size = 0;
    int loop_flag = 0, channel_count, codec;

    /* check extensions */
    /* .akb.bytes is the usual extension in later games */
    if ( !check_extensions(streamFile, "akb") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x414B4220) /* "AKB " */
        goto fail;

    channel_count = read_8bit(0x0d,streamFile);
    loop_flag = read_32bitLE(0x18,streamFile) > 0; /* loop end */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* 0x04 (2): version (iPad/IPhone?) */
    header_size = read_16bitLE(0x06,streamFile);
    file_size = read_32bitLE(0x08,streamFile);
    codec = read_8bit(0x0c,streamFile);
    vgmstream->sample_rate = (uint16_t)read_16bitLE(0x0e,streamFile);
    vgmstream->num_samples = read_32bitLE(0x10,streamFile);
    vgmstream->loop_start_sample = read_32bitLE(0x14,streamFile);
    vgmstream->loop_end_sample   = read_32bitLE(0x18,streamFile);
    vgmstream->meta_type = meta_AKB;

    /* should be ok but not exact (probably more complex, see AKB2) */
    if ( header_size >= 0x44) { /* v2 */
        /* 0x10+: config? (pan, volume), 0x24: file_id? */
        extra_data_size   = read_16bitLE(0x1c,streamFile);
        extra_header_size = read_16bitLE(0x28,streamFile);
        extra_data_offset = header_size + extra_header_size;
        start_offset = extra_data_offset + extra_data_size;
        /* ~num_samples at extra_data_offset + 0x04/0x0c? */
    }
    else { /* v0 */
        start_offset = header_size;
    }


    switch (codec) {
        case 0x02: { /* MSAPDCM [various SFX] */
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = read_16bitLE(extra_data_offset + 0x02,streamFile);

            /* adjusted samples; bigger or smaller than base samples, but seems more accurate
             * (base samples may have more than possible and read over file size otherwise, very strange)
             * loop_end seems to exist even with loop disabled */
            vgmstream->num_samples       = read_32bitLE(extra_data_offset + 0x04, streamFile);
            vgmstream->loop_start_sample = read_32bitLE(extra_data_offset + 0x08, streamFile);
            vgmstream->loop_end_sample   = read_32bitLE(extra_data_offset + 0x0c, streamFile);
            break;
        }

#ifdef VGM_USE_FFMPEG
        case 0x05: { /* Ogg Vorbis [Final Fantasy VI, Dragon Quest II-VI] */
            /* Starting from an offset in the current libvorbis code is a bit hard so just use FFmpeg.
             * Decoding seems to produce the same output with (inaudible) +-1 lower byte differences due to rounding. */
            ffmpeg_codec_data *ffmpeg_data;

            ffmpeg_data = init_ffmpeg_offset(streamFile, start_offset,file_size-start_offset);
            if ( !ffmpeg_data ) goto fail;

            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            /* These oggs have loop info in the comments, too */

            break;
        }

        case 0x06: { /* aac [The World Ends with You (iPad)] */
            /* init_vgmstream_akb above has priority, but this works fine too */
            ffmpeg_codec_data *ffmpeg_data;

            ffmpeg_data = init_ffmpeg_offset(streamFile, start_offset,file_size-start_offset);
            if ( !ffmpeg_data ) goto fail;

            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* remove encoder delay from the "global" sample values */
            vgmstream->num_samples -= ffmpeg_data->skipSamples;
            vgmstream->loop_start_sample -= ffmpeg_data->skipSamples;
            vgmstream->loop_end_sample -= ffmpeg_data->skipSamples;

            break;
        }
#endif

        default:
            goto fail;
    }

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* AKB2 - found in later SQEX iOS games */
VGMSTREAM * init_vgmstream_akb2_multi(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t datasize;
    int loop_flag = 0, channel_count, codec;
    int akb_header_size, sound_index = 0, sound_offset_data, sound, sound_header_size, material_offset_data, material_index = 0, material, extradata, encryption_flag;

    /* check extensions */
    /* .akb.bytes is the usual extension in later games */
    if ( !check_extensions(streamFile, "akb") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x414B4232) /* "AKB2" */
        goto fail;

    akb_header_size = read_16bitLE(0x06, streamFile);
    sound_offset_data = akb_header_size + sound_index * 0x10;
    sound = read_32bitLE(sound_offset_data + 0x04, streamFile);
    sound_header_size = read_16bitLE(sound + 0x02, streamFile);
    material_offset_data = sound + sound_header_size + material_index * 0x10;
    material = sound + read_32bitLE(material_offset_data + 0x04, streamFile);
    encryption_flag = read_8bit(material + 0x03, streamFile) & 0x08;
    extradata = material + read_16bitLE(material + 0x04, streamFile);

    start_offset = material + read_16bitLE(material + 0x04, streamFile) + read_32bitLE(material + 0x18, streamFile);
	header_offset = material;

    channel_count = read_8bit(header_offset+0x02,streamFile);
    loop_flag = read_32bitLE(header_offset+0x14,streamFile) > 0; /* loop end */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* 0x04: version?  0x08: filesize,  0x28: file_id?,  others: no idea */
    codec = read_8bit(header_offset+0x01,streamFile);
    datasize = read_32bitLE(header_offset+0x08,streamFile);
    vgmstream->sample_rate = (uint16_t)read_16bitLE(header_offset+0x06,streamFile);
    /* When loop_flag num_samples may be much larger than real num_samples (it's fine when looping is off)
     * Actual num_samples would be loop_end_sample+1, but more testing is needed */
    vgmstream->num_samples = read_32bitLE(header_offset+0x0c,streamFile);
    vgmstream->loop_start_sample = read_32bitLE(header_offset+0x10,streamFile);
    vgmstream->loop_end_sample   = read_32bitLE(header_offset+0x14,streamFile);

    vgmstream->meta_type = meta_AKB;

    switch (codec) {
        case 0x02: { /* MSAPDCM [The Irregular at Magic High School Lost Zero (Android)] */
            if (encryption_flag) goto fail;
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = read_16bitLE(extradata + 0x02, streamFile);

            /* adjusted samples; bigger or smaller than base samples, but seems more accurate
             * (base samples may have more than possible and read over file size otherwise, very strange)
             * loop_end seems to exist even with loop disabled */
            vgmstream->num_samples       = read_32bitLE(extradata + 0x04, streamFile);
            vgmstream->loop_start_sample = read_32bitLE(extradata + 0x08, streamFile);
            vgmstream->loop_end_sample   = read_32bitLE(extradata + 0x0c, streamFile);
            break;
        }

#ifdef VGM_USE_FFMPEG
        case 0x05: { /* ogg vorbis [The World Ends with You (iPhone / latest update)] */
            ffmpeg_codec_data *ffmpeg_data;

            ffmpeg_data = init_ffmpeg_offset(streamFile, start_offset,datasize);
            if ( !ffmpeg_data ) goto fail;

            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            break;
        }
#endif

        default:
            goto fail;
    }

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
