#include "../vgmstream.h"
#include "meta.h"
#include "../util.h"

static VGMSTREAM * init_vgmstream_hca_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size);

VGMSTREAM * init_vgmstream_hca(STREAMFILE *streamFile) {

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile, "hca")) return NULL;

	return init_vgmstream_hca_offset( streamFile, 0, streamFile->get_size(streamFile) );
}

static VGMSTREAM * init_vgmstream_hca_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size) {
	unsigned int ciphKey1;
	unsigned int ciphKey2;

	char filename[PATH_LIMIT];
    
	VGMSTREAM * vgmstream = NULL;

	hca_codec_data * hca_file = ( hca_codec_data * ) calloc(1, sizeof(hca_codec_data) + clHCA_sizeof());
	void * hca_data = NULL;
	clHCA * hca;

	uint8_t header[8];

	int header_size;

	if ( !hca_file ) goto fail;

	if ( size < 8 ) goto fail;

	hca_file->streamfile = streamFile;
	hca_file->start = start;
	hca_file->size = size;
    
	if ( read_streamfile( header, start, 8, streamFile) != 8 ) goto fail;

	header_size = clHCA_isOurFile0( header );
    
	if ( header_size < 0 ) goto fail;

	hca_data = malloc( header_size );

	if ( !hca_data ) goto fail;

	memcpy( hca_data, header, 8 );

	if ( read_streamfile( ((uint8_t*)hca_data) + 8, start + 8, header_size - 8, streamFile ) != header_size - 8 ) goto fail;

	if ( clHCA_isOurFile1( hca_data, header_size ) < 0 ) goto fail;

	hca = (clHCA *)(hca_file + 1);

    /* try to find key in external file */
    {
        uint8_t keybuf[8];

        if ( read_key_file(keybuf, 8, streamFile) ) {
            ciphKey2 = get_32bitBE(keybuf+0);
            ciphKey1 = get_32bitBE(keybuf+4);
        } else {
            /* PSO2 */
            ciphKey2=0xCC554639;
            ciphKey1=0x30DBE1AB;
        }
    }

	clHCA_clear(hca, ciphKey1, ciphKey2);
    
	if (clHCA_Decode(hca, hca_data, header_size, 0) < 0) goto fail;

	free( hca_data );
	hca_data = NULL;
    
	if (clHCA_getInfo(hca, &hca_file->info) < 0) goto fail;

	hca_file->sample_ptr = clHCA_samplesPerBlock;
	hca_file->samples_discard = 0;

	streamFile->get_name( streamFile, filename, sizeof(filename) );

	hca_file->streamfile = streamFile->open(streamFile, filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
	if (!hca_file->streamfile) goto fail;
    
	vgmstream = allocate_vgmstream( hca_file->info.channelCount, 1 );
	if (!vgmstream) goto fail;

	vgmstream->loop_flag = hca_file->info.loopEnabled;
	vgmstream->loop_start_sample = hca_file->info.loopStart * clHCA_samplesPerBlock;
	vgmstream->loop_end_sample = hca_file->info.loopEnd * clHCA_samplesPerBlock;

	vgmstream->codec_data = hca_file;

	vgmstream->channels = hca_file->info.channelCount;
	vgmstream->sample_rate = hca_file->info.samplingRate;

	vgmstream->num_samples = hca_file->info.blockCount * clHCA_samplesPerBlock;

	vgmstream->coding_type = coding_CRI_HCA;
	vgmstream->layout_type = layout_none;
	vgmstream->meta_type = meta_HCA;

	return vgmstream;

fail:
	if ( hca_data ) {
		free( hca_data );
	}
	if ( hca_file ) {
		free( hca_file );
	}
	return NULL;
}
