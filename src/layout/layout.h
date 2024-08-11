#ifndef _LAYOUT_H
#define _LAYOUT_H

#include "../streamtypes.h"
#include "../vgmstream.h"
#include "../util/reader_sf.h"
#include "../util/log.h"

/* basic layouts */
void render_vgmstream_flat(sample_t* buffer, int32_t sample_count, VGMSTREAM* vgmstream);

void render_vgmstream_interleave(sample_t* buffer, int32_t sample_count, VGMSTREAM* vgmstream);

/* segmented layout */
/* for files made of "continuous" segments, one per section of a song (using a complete sub-VGMSTREAM) */
typedef struct {
    int segment_count;
    VGMSTREAM** segments;
    int current_segment;
    sample_t* buffer;
    int input_channels;     /* internal buffer channels */
    int output_channels;    /* resulting channels (after mixing, if applied) */
    int mixed_channels;     /* segments have different number of channels */
} segmented_layout_data;

void render_vgmstream_segmented(sample_t* buffer, int32_t sample_count, VGMSTREAM* vgmstream);
segmented_layout_data* init_layout_segmented(int segment_count);
int setup_layout_segmented(segmented_layout_data* data);
void free_layout_segmented(segmented_layout_data* data);
void reset_layout_segmented(segmented_layout_data* data);
void seek_layout_segmented(VGMSTREAM* vgmstream, int32_t seek_sample);
void loop_layout_segmented(VGMSTREAM* vgmstream, int32_t loop_sample);

/* layered layout */
/* for files made of "parallel" layers, one per group of channels (using a complete sub-VGMSTREAM) */
typedef struct {
    int layer_count;
    VGMSTREAM** layers;
    sample_t* buffer;
    int input_channels;     /* internal buffer channels */
    int output_channels;    /* resulting channels (after mixing, if applied) */
    int external_looping;   /* don't loop using per-layer loops, but layout's own looping */
    int curr_layer;         /* helper */
} layered_layout_data;

void render_vgmstream_layered(sample_t* buffer, int32_t sample_count, VGMSTREAM* vgmstream);
layered_layout_data* init_layout_layered(int layer_count);
int setup_layout_layered(layered_layout_data* data);
void free_layout_layered(layered_layout_data* data);
void reset_layout_layered(layered_layout_data* data);
void seek_layout_layered(VGMSTREAM* vgmstream, int32_t seek_sample);
void loop_layout_layered(VGMSTREAM* vgmstream, int32_t loop_sample);

/* blocked layouts */
void render_vgmstream_blocked(sample_t* buffer, int32_t sample_count, VGMSTREAM* vgmstream);
void block_update(off_t block_offset, VGMSTREAM* vgmstream);
void blocked_count_samples(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t offset);

void block_update_ast(off_t block_ofset, VGMSTREAM* vgmstream);
void block_update_mxch(off_t block_ofset, VGMSTREAM* vgmstream);
void block_update_halpst(off_t block_ofset, VGMSTREAM* vgmstream);
void block_update_xa(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_ea_schl(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_ea_1snh(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_caf(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_wsi(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_str_snds(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_ws_aud(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_dec(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_vs(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_mul(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_gsb(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_xvas(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_thp(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_filp(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_rage_aud(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_ea_swvr(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_adm(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_ps2_iab(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_vs_str(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_rws(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_hwas(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_ea_sns(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_awc(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_vgs(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_xwav(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_xvag_subsong(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_ea_wve_au00(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_ea_wve_ad10(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_sthd(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_h4m(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_xa_aiff(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_vs_square(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_vid1(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_ubi_sce(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_tt_ad(off_t block_offset, VGMSTREAM* vgmstream);
void block_update_vas(off_t block_offset, VGMSTREAM* vgmstream);

#endif
