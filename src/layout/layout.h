#ifndef _LAYOUT_H
#define _LAYOUT_H

#include "../streamtypes.h"
#include "../vgmstream.h"

/* blocked layouts */
void render_vgmstream_blocked(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);

void block_update_ast(off_t block_ofset, VGMSTREAM * vgmstream);
void block_update_mxch(off_t block_ofset, VGMSTREAM * vgmstream);
void block_update_halpst(off_t block_ofset, VGMSTREAM * vgmstream);
void block_update_xa(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_ea_schl(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_ea_1snh(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_caf(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_wsi(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_str_snds(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_ws_aud(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_matx(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_dec(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_vs(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_emff_ps2(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_emff_ngc(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_gsb(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_xvas(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_thp(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_filp(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_ivaud(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_ea_swvr(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_adm(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_bdsp(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_tra(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_ps2_iab(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_ps2_strlr(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_rws(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_hwas(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_ea_sns(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_awc(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_vgs(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_vawx(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_xvag_subsong(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_ea_wve_au00(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_ea_wve_ad10(off_t block_offset, VGMSTREAM * vgmstream);
void block_update_sthd(off_t block_offset, VGMSTREAM * vgmstream);

/* other layouts */
void render_vgmstream_interleave(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);

void render_vgmstream_nolayout(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);

void render_vgmstream_aix(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);

void render_vgmstream_segmented(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);
segmented_layout_data* init_layout_segmented(int segment_count);
int setup_layout_segmented(segmented_layout_data* data);
void free_layout_segmented(segmented_layout_data *data);
void reset_layout_segmented(segmented_layout_data *data);

void render_vgmstream_layered(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream);
layered_layout_data* init_layout_layered(int layer_count);
int setup_layout_layered(layered_layout_data* data);
void free_layout_layered(layered_layout_data *data);
void reset_layout_layered(layered_layout_data *data);

#endif
