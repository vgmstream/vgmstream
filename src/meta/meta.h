#ifndef _META_H
#define _META_H

#include "../vgmstream.h"

VGMSTREAM * init_vgmstream_adx(const char * const filename);

VGMSTREAM * init_vgmstream_afc(const char * const filename);

VGMSTREAM * init_vgmstream_agsc(const char * const filename);

VGMSTREAM * init_vgmstream_ast(const char * const filename);

VGMSTREAM * init_vgmstream_brstm(const char * const filename);

VGMSTREAM * init_vgmstream_Cstr(const char * const filename);

VGMSTREAM * init_vgmstream_gcsw(const char * const filename);

VGMSTREAM * init_vgmstream_halpst(const char * const filename);

VGMSTREAM * init_vgmstream_nds_strm(const char * const filename);

VGMSTREAM * init_vgmstream_ngc_adpdtk(const char * const filename);

VGMSTREAM * init_vgmstream_ngc_dsp_std(const char * const filename);

VGMSTREAM * init_vgmstream_ngc_dsp_stm(const char * const filename);

VGMSTREAM * init_vgmstream_ngc_mpdsp(const char * const filename);

VGMSTREAM * init_vgmstream_ps2_ads(const char * const filename);

VGMSTREAM * init_vgmstream_ps2_npsf(const char * const filename);

VGMSTREAM * init_vgmstream_rs03(const char * const filename);

VGMSTREAM * init_vgmstream_rsf(const char * const filename);

VGMSTREAM * init_vgmstream_rwsd(const char * const filename);

VGMSTREAM * init_vgmstream_cdxa(const char * const filename);

VGMSTREAM * init_vgmstream_ps2_rxw(const char * const filename);

VGMSTREAM * init_vgmstream_ps2_int(const char * const filename);

VGMSTREAM * init_vgmstream_ps2_exst(const char * const filename);

VGMSTREAM * init_vgmstream_ps2_svag(const char * const filename);

VGMSTREAM * init_vgmstream_ps2_mib(const char * const filename);

#endif
