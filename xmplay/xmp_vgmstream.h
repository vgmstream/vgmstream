#ifndef _XMP_VGMSTREAM_
#define _XMP_VGMSTREAM_

#include "xmpin.h"
#include "xmpfunc.h"
#include "../src/libvgmstream.h"


libstreamfile_t* open_xmplay_streamfile_by_xmpfile(XMPFILE infile, XMPFUNC_FILE* xmpf_file, const char* path, bool internal);

char* get_tags_from_filepath_info(libvgmstream_t* infostream, XMPFUNC_MISC* xmpf_misc, const char* filepath);

void build_extension_list(char* extension_list, int list_size, DWORD version);

#endif
