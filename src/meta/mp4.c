#include "meta.h"
#include "../coding/coding.h"

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
#endif


#ifdef VGM_USE_FFMPEG

typedef struct {
    int32_t num_samples;
    int loop_flag;
    int32_t loop_start;
    int32_t loop_end;
    int32_t encoder_delay;
} mp4_header;

static void parse_mp4(STREAMFILE* sf, mp4_header* mp4);


VGMSTREAM* init_vgmstream_mp4_aac_ffmpeg(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset = 0;
    mp4_header mp4 = {0};
    size_t file_size;
    ffmpeg_codec_data* ffmpeg_data = NULL;


    /* checks */
    /* .bin: Final Fantasy Dimensions (iOS), Final Fantasy V (iOS)
     * .msd: UNO (iOS) */
    if (!check_extensions(sf,"mp4,m4a,m4v,lmp4,bin,lbin,msd"))
        goto fail;

    if ((read_u32be(0x00,sf) & 0xFFFFFF00) != 0) /* first atom BE size (usually ~0x18) */
        goto fail;
    if (!is_id32be(0x04,sf, "ftyp"))
        goto fail;

    file_size = get_streamfile_size(sf);

    ffmpeg_data = init_ffmpeg_offset(sf, start_offset, file_size);
    if (!ffmpeg_data) goto fail;

    parse_mp4(sf, &mp4);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ffmpeg_data->channels, mp4.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MP4;
    vgmstream->sample_rate = ffmpeg_data->sampleRate;
    vgmstream->num_samples = mp4.num_samples;
    if (vgmstream->num_samples == 0)
        vgmstream->num_samples = ffmpeg_data->totalSamples;
    vgmstream->loop_start_sample = mp4.loop_start;
    vgmstream->loop_end_sample = mp4.loop_end;

    vgmstream->codec_data = ffmpeg_data;
    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->layout_type = layout_none;
    vgmstream->num_streams = ffmpeg_data->streamCount; /* may contain N tracks */

    vgmstream->channel_layout = ffmpeg_get_channel_layout(vgmstream->codec_data);
    if (mp4.encoder_delay)
        ffmpeg_set_skip_samples(vgmstream->codec_data, mp4.encoder_delay);

    return vgmstream;

fail:
    if (ffmpeg_data) {
        free_ffmpeg(ffmpeg_data);
        if (vgmstream) vgmstream->codec_data = NULL;
    }
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* read useful MP4 chunks */
static void parse_mp4(STREAMFILE* sf, mp4_header* mp4) {
    off_t offset, suboffset, max_offset, max_suboffset;


    /* MOV format chunks, called "atoms", size goes first because Apple */
    offset = 0x00;
    max_offset = get_streamfile_size(sf);
    while (offset < max_offset) {
        uint32_t size = read_u32be(offset + 0x00,sf);
        uint32_t type = read_u32be(offset + 0x04,sf);
        //offset += 0x08;

        /* just in case */
        if (size == 0)
            break;

        switch(type) {
            case 0x66726565: /* "free" */
                /* Tales of Hearts R (iOS) has loop info in the first "free" atom */
                if (read_u32be(offset + 0x08,sf) == 0x4F700002 && (size == 0x38 || size == 0x40)) {
                    /* 0x00: id / "Op" */
                    /* 0x02: channels */
                    /* 0x04/8: sample rate */
                    /* 0x0c: null? */
                    /* 0x10: num_samples (without padding, same as FFmpeg's) */
                    /* 0x14/18/1c/20: offsets to stream info (stts/stsc/stsz/stco) */
                    mp4->encoder_delay = read_u32be(offset + 0x08 + 0x24,sf); /* Apple's 2112 */
                    mp4->loop_flag = read_u32be(offset + 0x08 + 0x28,sf);
                    if (mp4->loop_flag) { /* atom ends if no loop flag */
                        mp4->loop_start = read_u32be(offset + 0x08 + 0x2c,sf);
                        mp4->loop_end = read_u32be(offset + 0x08 + 0x30,sf);
                    }
                    /* could stop reading since FFmpeg will too */
                }
                break;

            case 0x6D6F6F76: { /* "moov" (header) */
                suboffset = offset += 0x08;
                max_suboffset = offset + size;
                while (suboffset < max_suboffset) {
                    uint32_t subsize = read_u32be(suboffset + 0x00,sf);
                    uint32_t subtype = read_u32be(suboffset + 0x04,sf);

                    /* padded in ToRR */
                    if (subsize == 0)
                        break;

                    switch(subtype) {
                        case 0x75647461: /* "udta" */
                            /* CRI subchunk [Imperial SaGa Eclipse (Browser)]
                             * incidentally "moov" header comes after data ("mdat") in CRI's files */
                            if (subsize >= 0x28 && is_id32be(suboffset + 0x08 + 0x04,sf, "criw")) {
                                off_t criw_offset = suboffset + 0x08 + 0x08;

                                mp4->loop_start     = read_s32be(criw_offset + 0x00,sf);
                                mp4->loop_end       = read_s32be(criw_offset + 0x04,sf);
                                mp4->encoder_delay  = read_s32be(criw_offset + 0x08,sf); /* Apple's 2112 */
                                mp4->num_samples    = read_s32be(criw_offset + 0x0c,sf);
                                mp4->loop_flag = (mp4->loop_end > 0);
                                /* next 2 fields are null */
                            }
                            break;

                        default:
                            break;
                    }

                    suboffset += subsize;
                }

                break;
            }

            default:
                break;
        }

        offset += size; /* atoms don't seem to need to padding byte, unlike RIFF */
    }
}

/* CRI's encryption info (for lack of a better place) [Final Fantasy Digital Card Game (Browser)]
 * 
 * Like other CRI stuff their MP4 can be encrypted, from file's beginning (including headers).
 * This is more or less how data is decrypted (supposedly, from decompilations), for reference:
 */
#if 0
void criAacCodec_SetDecryptionKey(uint64_t keycode, uint16_t* key) {
    if (!keycode)
        return;
    uint16_t k0 = 4 * ((keycode >> 0)  & 0x0FFF) | 1;
    uint16_t k1 = 2 * ((keycode >> 12) & 0x1FFF) | 1;
    uint16_t k2 = 4 * ((keycode >> 25) & 0x1FFF) | 1;
    uint16_t k3 = 2 * ((keycode >> 38) & 0x3FFF) | 1;

    key[0] = k0 ^ k1;
    key[1] = k1 ^ k2;
    key[2] = k2 ^ k3;
    key[3] = ~k3;

    /* criatomexacb_generate_aac_decryption_key is slightly different, unsure which one is used: */
  //key[0] = k0 ^ k3;
  //key[1] = k2 ^ k3;
  //key[2] = k2 ^ k3;
  //key[3] = ~k3;
}

void criAacCodec_DecryptData(const uint16_t* key, uint8_t* data, uint32_t size) {
    if (data_size)
        return;
    uint16_t seed0 = ~key[3];
    uint16_t seed1 = seed0 ^ key[2];
    uint16_t seed2 = seed1 ^ key[1];
    uint16_t seed3 = seed2 ^ key[0];

    uint16_t xor = 2 * seed0 | 1;
    uint16_t add = 2 * seed0 | 1; /* not seed1 */
    uint16_t mul = 4 * seed2 | 1;

    for (int i = 0; i < data_size; i++) {

        if (!(uint16_t)i) { /* every 0x10000, without modulo */
            mul = (4 * seed2 + seed3 * (mul & 0xFFFC)) & 0xFFFD | 1;
            add = (2 * seed0 + seed1 * (add & 0xFFFE)) | 1;
        }
        xor = xor * mul + add;

        *data ^= (xor >> 8) & 0xFF;
        ++data;
    }
}
#endif

#endif
