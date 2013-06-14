#include "../vgmstream.h"
#include "meta.h"

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
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
