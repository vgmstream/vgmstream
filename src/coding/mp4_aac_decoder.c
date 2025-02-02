#include "../vgmstream.h"

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
#ifdef VGM_USE_MP4V2
#define MP4V2_NO_STDINT_DEFS
#include <mp4v2/mp4v2.h>
#endif

#ifdef VGM_USE_FDKAAC
#include <aacdecoder_lib.h>
#endif

typedef struct {
    STREAMFILE* streamfile;
    uint64_t start;
    uint64_t offset;
    uint64_t size;
} mp4_streamfile;

struct  mp4_aac_codec_data {
    mp4_streamfile if_file;
    MP4FileHandle h_mp4file;
    MP4TrackId track_id;
    unsigned long sampleId;
    unsigned long numSamples;
    UINT codec_init_data_size;
    HANDLE_AACDECODER h_aacdecoder;
    unsigned int sample_ptr;
    unsigned int samples_per_frame
    unsigned int samples_discard;
    INT_PCM sample_buffer[( (6) * (2048)*4 )];
};


// VGM_USE_MP4V2
static void* mp4_file_open( const char* name, MP4FileMode mode ) {
    char * endptr;
#ifdef _MSC_VER
    unsigned __int64 ptr = _strtoui64( name, &endptr, 16 );
#else
    unsigned long ptr = strtoul( name, &endptr, 16 );
#endif
    return (void*) ptr;
}

static int mp4_file_seek( void* handle, int64_t pos ) {
    mp4_streamfile * file = ( mp4_streamfile * ) handle;
    if ( pos > file->size ) pos = file->size;
    pos += file->start;
    file->offset = pos;
    return 0;
}

static int mp4_file_get_size( void* handle, int64_t* size ) {
    mp4_streamfile * file = ( mp4_streamfile * ) handle;
    *size = file->size;
    return 0;
}

static int mp4_file_read( void* handle, void* buffer, int64_t size, int64_t* nin, int64_t maxChunkSize ) {
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

static int mp4_file_write( void* handle, const void* buffer, int64_t size, int64_t* nout, int64_t maxChunkSize ) {
    return 1;
}

static int mp4_file_close( void* handle ) {
    return 0;
}

MP4FileProvider mp4_file_provider = { mp4_file_open, mp4_file_seek, mp4_file_read, mp4_file_write, mp4_file_close, mp4_file_get_size };


mp4_aac_codec_data* init_mp4_aac(STREAMFILE* sf) {
    char filename[PATH_LIMIT];
    uint32_t start = 0;
    uint32_t size = get_streamfile_size(sf);

    CStreamInfo* stream_info = NULL;

    uint8_t* buffer = NULL;
    uint32_t buffer_size;
    UINT ubuffer_size, bytes_valid;

    mp4_aac_codec_data* data = calloc(1, sizeof(mp4_aac_codec_data));
    if (!data) goto fail;

    data->if_file.streamfile = sf;
    data->if_file.start = start;
    data->if_file.offset = start;
    data->if_file.size = size;

    /* Big ol' kludge! */
    sprintf( filename, "%p", &data->if_file );
    data->h_mp4file = MP4ReadProvider( filename, &mp4_file_provider );
    if ( !data->h_mp4file ) goto fail;

    if ( MP4GetNumberOfTracks(data->h_mp4file, MP4_AUDIO_TRACK_TYPE, '\000') != 1 ) goto fail;

    data->track_id = MP4FindTrackId( data->h_mp4file, 0, MP4_AUDIO_TRACK_TYPE, '\000' );

    data->h_aacdecoder = aacDecoder_Open( TT_MP4_RAW, 1 );
    if ( !data->h_aacdecoder ) goto fail;

    MP4GetTrackESConfiguration( data->h_mp4file, data->track_id, (uint8_t**)(&buffer), (uint32_t*)(&buffer_size));

    ubuffer_size = buffer_size;
    if ( aacDecoder_ConfigRaw( data->h_aacdecoder, &buffer, &ubuffer_size ) ) goto fail;

    free( buffer ); buffer = NULL;

    data->sampleId = 1;
    data->numSamples = MP4GetTrackNumberOfSamples( data->h_mp4file, data->track_id );

    if (!MP4ReadSample(data->h_mp4file, data->track_id, data->sampleId, (uint8_t**)(&buffer), (uint32_t*)(&buffer_size), 0, 0, 0, 0)) goto fail;

    ubuffer_size = buffer_size;
    bytes_valid = buffer_size;
    if ( aacDecoder_Fill( data->h_aacdecoder, &buffer, &ubuffer_size, &bytes_valid ) || bytes_valid ) goto fail;
    if ( aacDecoder_DecodeFrame( data->h_aacdecoder, data->sample_buffer, ( (6) * (2048)*4 ), 0 ) ) goto fail;

    free( buffer ); buffer = NULL;

    data->sample_ptr = 0;

    stream_info = aacDecoder_GetStreamInfo( data->h_aacdecoder );

    data->samples_per_frame = stream_info->frameSize;
    data->samples_discard = 0;

    sf->get_name( sf, filename, sizeof(filename) );

    data->if_file.streamfile = sf->open(sf, filename, 0);
    if (!data->if_file.streamfile) goto fail;

    return data;
fail:
    free( buffer ); buffer = NULL;
    free_mp4_aac(data);
}


static void convert_samples(INT_PCM * src, sample_t* dest, int32_t count) {
    int32_t i;
    for ( i = 0; i < count; i++ ) {
        INT_PCM sample = *src++;
        sample >>= SAMPLE_BITS - 16;
        if ( ( sample + 0x8000 ) & 0xFFFF0000 ) sample = 0x7FFF ^ ( sample >> 31 );
        *dest++ = sample;
    }
}

void decode_mp4_aac(mp4_aac_codec_data * data, sample_t* outbuf, int32_t samples_to_do, int channels) {
    int samples_done = 0;

    uint8_t * buffer = NULL;
    uint32_t buffer_size;
    UINT ubuffer_size, bytes_valid;

    CStreamInfo * stream_info = aacDecoder_GetStreamInfo( data->h_aacdecoder );

    int32_t samples_remain = data->samples_per_frame - data->sample_ptr;

    if ( data->samples_discard ) {
        if ( samples_remain <= data->samples_discard ) {
            data->samples_discard -= samples_remain;
            samples_remain = 0;
        }
        else {
            samples_remain -= data->samples_discard;
            data->sample_ptr += data->samples_discard;
            data->samples_discard = 0;
        }
    }

    if ( samples_remain > samples_to_do ) samples_remain = samples_to_do;

    convert_samples( data->sample_buffer + data->sample_ptr * stream_info->numChannels, outbuf, samples_remain * stream_info->numChannels );

    outbuf += samples_remain * stream_info->numChannels;

    data->sample_ptr += samples_remain;

    samples_done += samples_remain;

    while ( samples_done < samples_to_do ) {
        if (data->sampleId >= data->numSamples) {
            memset(outbuf, 0, (samples_to_do - samples_done) * stream_info->numChannels * sizeof(sample_t));
            break;
        }
        if (!MP4ReadSample( data->h_mp4file, data->track_id, ++data->sampleId, (uint8_t**)(&buffer), (uint32_t*)(&buffer_size), 0, 0, 0, 0)) return;
        ubuffer_size = buffer_size;
        bytes_valid = buffer_size;
        if ( aacDecoder_Fill( data->h_aacdecoder, &buffer, &ubuffer_size, &bytes_valid ) || bytes_valid ) {
            free( buffer );
            return;
        }
        if ( aacDecoder_DecodeFrame( data->h_aacdecoder, data->sample_buffer, ( (6) * (2048)*4 ), 0 ) ) {
            free( buffer );
            return;
        }
        free( buffer ); buffer = NULL;
        stream_info = aacDecoder_GetStreamInfo( data->h_aacdecoder );
        samples_remain = data->samples_per_frame = stream_info->frameSize;
        data->sample_ptr = 0;
        if ( data->samples_discard ) {
            if ( samples_remain <= data->samples_discard ) {
                data->samples_discard -= samples_remain;
                samples_remain = 0;
            }
            else {
                samples_remain -= data->samples_discard;
                data->sample_ptr = data->samples_discard;
                data->samples_discard = 0;
            }
        }
        if ( samples_remain > samples_to_do - samples_done ) samples_remain = samples_to_do - samples_done;
        convert_samples( data->sample_buffer + data->sample_ptr * stream_info->numChannels, outbuf, samples_remain * stream_info->numChannels );
        samples_done += samples_remain;
        outbuf += samples_remain * stream_info->numChannels;
        data->sample_ptr = samples_remain;
    }
}


void reset_mp4_aac(VGMSTREAM *vgmstream) {
    mp4_aac_codec_data *data = vgmstream->codec_data;
    if (!data) return;

    data->sampleId = 0;
    data->sample_ptr = data->samples_per_frame;
    data->samples_discard = 0;
}

void seek_mp4_aac(VGMSTREAM *vgmstream, int32_t num_sample) {
    mp4_aac_codec_data *data = (mp4_aac_codec_data *)(vgmstream->codec_data);
    if (!data) return;

    data->sampleId = 0;
    data->sample_ptr = data->samples_per_frame;
    data->samples_discard = num_sample;
}

void free_mp4_aac(mp4_aac_codec_data * data) {
    if (data) {
        if (data->h_aacdecoder) aacDecoder_Close(data->h_aacdecoder);
        if (data->h_mp4file) MP4Close(data->h_mp4file, 0);
        if (data->if_file.streamfile) close_streamfile(data->if_file.streamfile);
        free(data);
    }
}

void mp4_aac_get_streamfile(mp4_aac_codec_data* data) {
    if (!data)
        return NULL;
    return data->if_file.streamfile;
}

int32_t mp4_aac_get_samples(mp4_aac_codec_data* data) {
    if (!data)
        return 0;
    return (int32_t)(data->samples_per_frame * data->numSamples);
}

int32_t mp4_aac_get_samples_per_frame(mp4_aac_codec_data* data) {
    if (!data)
        return 0;
    return (int32_t)(data->samples_per_frame);
}

int mp4_aac_get_sample_rate(mp4_aac_codec_data* data) {
    if (!data)
        return 0;

    CStreamInfo* stream_info = aacDecoder_GetStreamInfo( data->h_aacdecoder );
    return stream_info->sample_rate;
}

int mp4_aac_get_channels(mp4_aac_codec_data* data) {
    if (!data)
        return 0;

    CStreamInfo* stream_info = aacDecoder_GetStreamInfo( data->h_aacdecoder );
    return stream_info->numChannels;
}

#endif
