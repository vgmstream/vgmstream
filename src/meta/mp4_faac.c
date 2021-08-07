#include "meta.h"
#include "../coding/coding.h"

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
// VGM_USE_MP4V2
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

// VGM_USE_FDKAAC
VGMSTREAM * init_vgmstream_mp4_aac_offset(STREAMFILE *sf, uint64_t start, uint64_t size);

VGMSTREAM * init_vgmstream_mp4_aac(STREAMFILE *sf) {
	return init_vgmstream_mp4_aac_offset( sf, 0, sf->get_size(sf) );
}

VGMSTREAM * init_vgmstream_mp4_aac_offset(STREAMFILE *sf, uint64_t start, uint64_t size) {
	VGMSTREAM * vgmstream = NULL;

	char filename[PATH_LIMIT];

	mp4_aac_codec_data * aac_file = ( mp4_aac_codec_data * ) calloc(1, sizeof(mp4_aac_codec_data));

	CStreamInfo * stream_info;

	uint8_t * buffer = NULL;
	uint32_t buffer_size;
	UINT ubuffer_size, bytes_valid;

	if ( !aac_file ) goto fail;

	aac_file->if_file.streamfile = sf;
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

	sf->get_name( sf, filename, sizeof(filename) );

	aac_file->if_file.streamfile = sf->open(sf, filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
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
