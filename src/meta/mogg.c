/*
2017-12-10: Preliminary MOGG Support. As long as the stream is unencrypted, this should be fine.
			This will also work on unconventional 5 channel Vorbis streams but some sound cards might not like it.
			TODO (Eventually): Add decryption for encrypted MOGG types (Rock Band, etc.)

			-bxaimc
*/

#include "meta.h"
#include "../coding/coding.h"

/* MOGG - Harmonix Music Systems (Guitar Hero)[Unencrypted Type] */
VGMSTREAM * init_vgmstream_mogg(STREAMFILE *streamFile) {
#ifdef VGM_USE_VORBIS
	char filename[PATH_LIMIT];
	off_t start_offset;

	/* check extension, case insensitive */
	streamFile->get_name(streamFile, filename, sizeof(filename));
	if (strcasecmp("mogg", filename_extension(filename))) goto fail;

	{
		vgm_vorbis_info_t inf;
		VGMSTREAM * result = NULL;

		memset(&inf, 0, sizeof(inf));
		inf.layout_type = layout_ogg_vorbis;
		inf.meta_type = meta_MOGG;

		start_offset = read_32bitLE(0x04, streamFile);
		result = init_vgmstream_ogg_vorbis_callbacks(streamFile, filename, NULL, start_offset, &inf);

		if (result != NULL) {
			return result;
		}
	}

fail:
	/* clean up anything we may have opened */
#endif
	return NULL;
}