#include "../vgmstream.h"
#include "meta.h"
#include "../util.h"

#ifdef VGM_USE_MP4V2
void* mp4_file_open( const char* name, MP4FileMode mode )
{
	char * endptr;
#ifdef _MSC_VER
	unsigned __int64 ptr = _strtoui64( name, &endptr, 16 );
#else
	unsigned long ptr = strtoul( name, &endptr, 16 );
#endif
	return (void*) ptr;
}

int mp4_file_seek( void* handle, int64_t pos )
{
	mp4_streamfile * file = ( mp4_streamfile * ) handle;
	if ( pos > file->size ) pos = file->size;
	pos += file->start;
	file->offset = pos;
	return 0;
}

int mp4_file_get_size( void* handle, int64_t* size )
{
	mp4_streamfile * file = ( mp4_streamfile * ) handle;
	*size = file->size;
	return 0;
}

int mp4_file_read( void* handle, void* buffer, int64_t size, int64_t* nin, int64_t maxChunkSize )
{
	mp4_streamfile * file = ( mp4_streamfile * ) handle;
	int64_t max_size = file->size - file->offset - file->start;
	if ( size > max_size ) size = max_size;
	if ( size > 0 )
	{
		*nin = read_streamfile( (uint8_t *) buffer, file->offset, size, file->streamfile );
		file->offset += *nin;
	}
	else
	{
		*nin = 0;
		return 1;
	}
	return 0;
}

int mp4_file_write( void* handle, const void* buffer, int64_t size, int64_t* nout, int64_t maxChunkSize )
{
	return 1;
}

int mp4_file_close( void* handle )
{
	return 0;
}

MP4FileProvider mp4_file_provider = { mp4_file_open, mp4_file_seek, mp4_file_read, mp4_file_write, mp4_file_close, mp4_file_get_size };

#ifdef VGM_USE_FDKAAC
VGMSTREAM * init_vgmstream_mp4_aac_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size);

VGMSTREAM * init_vgmstream_mp4_aac(STREAMFILE *streamFile) {
	return init_vgmstream_mp4_aac_offset( streamFile, 0, streamFile->get_size(streamFile) );
}

VGMSTREAM * init_vgmstream_mp4_aac_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size) {
	VGMSTREAM * vgmstream = NULL;

	char filename[PATH_LIMIT];

	mp4_aac_codec_data * aac_file = ( mp4_aac_codec_data * ) calloc(1, sizeof(mp4_aac_codec_data));

	CStreamInfo * stream_info;

	uint8_t * buffer = NULL;
	uint32_t buffer_size;
	UINT ubuffer_size, bytes_valid;

	if ( !aac_file ) goto fail;

	aac_file->if_file.streamfile = streamFile;
	aac_file->if_file.start = start;
	aac_file->if_file.offset = start;
	aac_file->if_file.size = size;

	/* Big ol' kludge! */
	sprintf( filename, "%p", &aac_file->if_file );
	aac_file->h_mp4file = MP4ReadProvider( filename, &mp4_file_provider );
	if ( !aac_file->h_mp4file ) goto fail;

	if ( MP4GetNumberOfTracks(aac_file->h_mp4file, MP4_AUDIO_TRACK_TYPE, '\000') != 1 ) goto fail;

	aac_file->track_id = MP4FindTrackId( aac_file->h_mp4file, 0, MP4_AUDIO_TRACK_TYPE, '\000' );

	aac_file->h_aacdecoder = aacDecoder_Open( TT_MP4_RAW, 1 );
	if ( !aac_file->h_aacdecoder ) goto fail;

	aacDecoder_SetParam( aac_file->h_aacdecoder, AAC_PCM_OUTPUT_CHANNELS, 2 );

	MP4GetTrackESConfiguration( aac_file->h_mp4file, aac_file->track_id, (uint8_t**)(&buffer), (uint32_t*)(&buffer_size));

	ubuffer_size = buffer_size;
	if ( aacDecoder_ConfigRaw( aac_file->h_aacdecoder, &buffer, &ubuffer_size ) ) goto fail;

	free( buffer ); buffer = NULL;

	aac_file->sampleId = 1;
	aac_file->numSamples = MP4GetTrackNumberOfSamples( aac_file->h_mp4file, aac_file->track_id );

	if (!MP4ReadSample(aac_file->h_mp4file, aac_file->track_id, aac_file->sampleId, (uint8_t**)(&buffer), (uint32_t*)(&buffer_size), 0, 0, 0, 0)) goto fail;

	ubuffer_size = buffer_size;
	bytes_valid = buffer_size;
	if ( aacDecoder_Fill( aac_file->h_aacdecoder, &buffer, &ubuffer_size, &bytes_valid ) || bytes_valid ) goto fail;
	if ( aacDecoder_DecodeFrame( aac_file->h_aacdecoder, aac_file->sample_buffer, ( (6) * (2048)*4 ), 0 ) ) goto fail;

	free( buffer ); buffer = NULL;

	aac_file->sample_ptr = 0;

	stream_info = aacDecoder_GetStreamInfo( aac_file->h_aacdecoder );

	aac_file->samples_per_frame = stream_info->frameSize;
	aac_file->samples_discard = 0;

	streamFile->get_name( streamFile, filename, sizeof(filename) );

	aac_file->if_file.streamfile = streamFile->open(streamFile, filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
	if (!aac_file->if_file.streamfile) goto fail;

	vgmstream = allocate_vgmstream( stream_info->numChannels, 1 );
	if (!vgmstream) goto fail;

	vgmstream->loop_flag = 0;

	vgmstream->codec_data = aac_file;

	vgmstream->channels = stream_info->numChannels;
	vgmstream->sample_rate = stream_info->sampleRate;

	vgmstream->num_samples = stream_info->frameSize * aac_file->numSamples;

	vgmstream->coding_type = coding_MP4_AAC;
	vgmstream->layout_type = layout_none;
	vgmstream->meta_type = meta_MP4;

	return vgmstream;

fail:
	if ( buffer ) free( buffer );
	if ( aac_file ) {
		if ( aac_file->h_aacdecoder ) aacDecoder_Close( aac_file->h_aacdecoder );
		if ( aac_file->h_mp4file ) MP4Close( aac_file->h_mp4file, 0 );
		free( aac_file );
	}
	return NULL;
}
#endif
#endif


#ifdef VGM_USE_FFMPEG

VGMSTREAM * init_vgmstream_mp4_aac_ffmpeg(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset = 0;
    int loop_flag = 0;
    int32_t num_samples = 0, loop_start_sample = 0, loop_end_sample = 0;

    ffmpeg_codec_data *ffmpeg_data = NULL;


    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if ( strcasecmp("mp4",filename_extension(filename))
            && strcasecmp("m4a",filename_extension(filename))
            && strcasecmp("m4v",filename_extension(filename))
            && strcasecmp("bin",filename_extension(filename)) ) /* Final Fantasy Dimensions iOS */
        goto fail;


    /* check header for Final Fantasy Dimensions */
    if (read_32bitBE(0x00,streamFile) == 0x4646444C) { /* "FFDL" (any kind of FFD file) */
        if (read_32bitBE(0x04,streamFile) == 0x6D747873) { /* "mtxs" (bgm file) */
            num_samples = read_32bitLE(0x08,streamFile);
            loop_start_sample = read_32bitLE(0x0c,streamFile);
            loop_end_sample = read_32bitLE(0x10,streamFile);
            loop_flag = !(loop_start_sample==0 && loop_end_sample==num_samples);
            start_offset = 0x14;
        } else {
            start_offset = 0x4; /* some SEs */
        }
        /*  todo some FFDL have multi streams ("FFLD" + mtxsdata1 + mp4data1 + mtxsdata2 + mp4data2 + etc) */
    }


    /* check header */
    if ( read_32bitBE(start_offset+0x04,streamFile) != 0x66747970) /* size 0x00 + "ftyp" 0x04 */
        goto fail;

    ffmpeg_data = init_ffmpeg_offset(streamFile, start_offset, streamFile->get_size(streamFile));
    if ( !ffmpeg_data ) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ffmpeg_data->channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = ffmpeg_data->totalSamples; /* todo FFD num_samples is different from this */
    vgmstream->sample_rate = ffmpeg_data->sampleRate;
    vgmstream->channels = ffmpeg_data->channels;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_start_sample;
        vgmstream->loop_end_sample = loop_end_sample;
    }

    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_FFmpeg;
    vgmstream->codec_data = ffmpeg_data;


    return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (ffmpeg_data) {
        free_ffmpeg(ffmpeg_data);
        if (vgmstream) vgmstream->codec_data = NULL;
    }
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

#endif
