/* SPDX-License-Identifier: 0BSD */
#ifndef MAAC_EXTRAS_INCLUDE_GUARD
#define MAAC_EXTRAS_INCLUDE_GUARD

#include "maac.h"

#ifdef __cplusplus
extern "C" {
#endif


/* these are functions to make runtime/ffi querying easier,
when an implementation doesn't have the full adts struct
definition available */

maac_const
MAAC_PUBLIC
size_t
maac_adts_size(void);

maac_const
MAAC_PUBLIC
size_t
maac_adts_alignof(void);

maac_pure
MAAC_PUBLIC
maac_adts*
maac_adts_align(void*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_tolerance(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_mpeg_version(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_layer(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_protection_absent(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_profile(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_frequency_index(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_sample_rate(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_channel_configuration(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_channels(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_original_copy(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_home(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_copyright_id_bit(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_copyright_id_start(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_frame_length(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_buffer_fullness(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_raw_data_blocks(const maac_adts* a);

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_raw_ele_id(const maac_adts* a);

/* get the underlying maac_raw object used to decode */
maac_pure
MAAC_PUBLIC
maac_raw*
maac_adts_raw(maac_adts* a);

/* sets a tolerance value for how many bytes to read during the first sync,
   so for example if you start reading from the middle of a file, the maac_adts
   object won't immediately error out - it will try reading bytes for a while
   until it sees a syncword.

   If unset, default is 0 - meaning the data must immediately start on an ADTS
   syncword */

MAAC_PUBLIC
void
maac_adts_set_tolerance(maac_adts* a, maac_u32 tolerance);

MAAC_PUBLIC
void
maac_adts_set_out_channels(maac_adts* a, maac_channel* ch);

MAAC_PUBLIC
void
maac_adts_set_num_out_channels(maac_adts* a, maac_u32 num);


maac_const
MAAC_PUBLIC
size_t
maac_bitreader_size(void);

maac_const
MAAC_PUBLIC
size_t
maac_bitreader_alignof(void);

MAAC_PUBLIC
maac_bitreader*
maac_bitreader_align(void*);

/* returns how many bytes are left to be read by the bitreader */
maac_pure
MAAC_PUBLIC
maac_u32
maac_bitreader_avail(const maac_bitreader* br);

/* setter functions */

MAAC_PUBLIC
void
maac_bitreader_set_data(maac_bitreader* br, const maac_u8* data);

MAAC_PUBLIC
void
maac_bitreader_set_pos(maac_bitreader* br, maac_u32 pos);

MAAC_PUBLIC
void
maac_bitreader_set_len(maac_bitreader* br, maac_u32 len);

/* getter functions */

maac_pure
MAAC_PUBLIC
const maac_u8*
maac_bitreader_data(const maac_bitreader* br);

maac_pure
MAAC_PUBLIC
maac_u32
maac_bitreader_pos(const maac_bitreader* br);

maac_pure
MAAC_PUBLIC
maac_u32
maac_bitreader_len(const maac_bitreader* br);


maac_const
MAAC_PUBLIC
size_t
maac_fil_size(void);

maac_const
MAAC_PUBLIC
size_t
maac_fil_alignof(void);

maac_pure
MAAC_PUBLIC
maac_fil*
maac_fil_align(void*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_fil_extension_type(const maac_fil* f);

maac_const
MAAC_PUBLIC
const char*
maac_fil_extension_type_name(const maac_u32 t);

maac_const
MAAC_PUBLIC
size_t
maac_fil_extension_type_name_len(const maac_u32 t);


maac_const
MAAC_PUBLIC
size_t
maac_ics_size(void);

maac_const
MAAC_PUBLIC
size_t
maac_ics_alignof(void);

maac_pure
MAAC_PUBLIC
maac_ics*
maac_ics_align(void*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_state(const maac_ics* ics);

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_global_gain(const maac_ics* ics);

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_pulse_data_present(const maac_ics* ics);

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_tns_data_present(const maac_ics* ics);

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_gain_control_data_present(const maac_ics* ics);

maac_const
MAAC_PUBLIC
const char*
maac_ics_state_name(const maac_u32 state);

maac_const
MAAC_PUBLIC
size_t
maac_ics_state_name_len(const maac_u32 state);



maac_const
MAAC_PUBLIC
const char*
maac_ics_info_state_name(const maac_u32 state);

maac_const
MAAC_PUBLIC
size_t
maac_ics_info_state_name_len(const maac_u32 state);

maac_const
MAAC_PUBLIC
size_t
maac_ics_info_size(void);

maac_const
MAAC_PUBLIC
size_t
maac_ics_info_alignof(void);

maac_pure
MAAC_PUBLIC
maac_ics_info*
maac_ics_info_align(void*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_state(const maac_ics_info* ics_info);

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_window_sequence(const maac_ics_info* ics_info);

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_window_shape(const maac_ics_info* ics_info);

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_max_sfb(const maac_ics_info* ics_info);

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_scale_factor_grouping(const maac_ics_info* ics_info);

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_num_window_groups(const maac_ics_info* ics_info);

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_predictor_data_present(const maac_ics_info* ics_info);

#if MAAC_ENABLE_MAINPROFILE
maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_predictor_reset(const maac_ics_info* ics_info);

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_predictor_reset_group_number(const maac_ics_info* ics_info);

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_prediction_used(const maac_ics_info* ics_info, maac_u32 sfb);
#endif


maac_const
MAAC_PUBLIC
const char*
maac_window_sequence_name(const maac_u32 seq);

maac_const
MAAC_PUBLIC
size_t
maac_window_sequence_name_len(const maac_u32 seq);


maac_const
MAAC_PUBLIC
const char*
maac_result_name(const maac_s32 result);

maac_const
MAAC_PUBLIC
size_t
maac_result_name_len(const maac_s32 result);


/* helper - returns the number of channels indicated by channel config,
only useful if you used maac_config with an AudioSpecificConfig */
maac_pure
MAAC_PUBLIC
maac_u32 maac_raw_channels(const maac_raw* r);

maac_const
MAAC_PUBLIC
size_t
maac_raw_size(void);

maac_const
MAAC_PUBLIC
size_t
maac_raw_alignof(void);

maac_pure
MAAC_PUBLIC
maac_raw*
maac_raw_align(void*);

maac_const
MAAC_PUBLIC
const char*
maac_raw_data_block_id_name(const maac_u32 id);

maac_const
MAAC_PUBLIC
size_t
maac_raw_data_block_id_name_len(const maac_u32 id);

maac_pure
MAAC_PUBLIC
maac_u32 maac_raw_state(const maac_raw* r);

maac_pure
MAAC_PUBLIC
maac_u32 maac_raw_sf_index(const maac_raw* r);

maac_pure
MAAC_PUBLIC
maac_u32 maac_raw_ele_id(const maac_raw* r);

maac_pure
MAAC_PUBLIC
maac_u32 maac_raw_sample_rate(const maac_raw* r);

maac_pure
MAAC_PUBLIC
const maac_channel* maac_raw_out_channels(const maac_raw* r);

maac_pure
MAAC_PUBLIC
maac_u32 maac_raw_num_out_channels(const maac_raw* r);

maac_pure
MAAC_PUBLIC
maac_u32 maac_raw_channel_configuration(const maac_raw* r);

MAAC_PUBLIC
maac_fil*
maac_raw_fil(maac_raw* r);

MAAC_PUBLIC
maac_sce*
maac_raw_sce(maac_raw* r);

MAAC_PUBLIC
maac_cpe*
maac_raw_cpe(maac_raw* r);

MAAC_PUBLIC
void
maac_raw_set_out_channels(maac_raw* r, maac_channel* ch);

MAAC_PUBLIC
void
maac_raw_set_num_out_channels(maac_raw* r, maac_u32 n);


maac_const
MAAC_PUBLIC
size_t
maac_sce_size(void);

maac_const
MAAC_PUBLIC
size_t
maac_sce_alignof(void);

maac_pure
MAAC_PUBLIC
maac_sce*
maac_sce_align(void*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_sce_state(const maac_sce*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_sce_element_instance_tag(const maac_sce*);

maac_const
MAAC_PUBLIC
const char*
maac_sce_state_name(maac_u32 state);

maac_const
MAAC_PUBLIC
size_t
maac_sce_state_name_len(maac_u32 state);


maac_const
MAAC_PUBLIC
size_t
maac_cpe_size(void);

maac_const
MAAC_PUBLIC
size_t
maac_cpe_alignof(void);

maac_pure
MAAC_PUBLIC
maac_cpe*
maac_cpe_align(void*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_cpe_state(const maac_cpe*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_cpe_element_instance_tag(const maac_cpe*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_cpe_common_window(const maac_cpe*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_cpe_ms_mask_present(const maac_cpe*);

maac_const
MAAC_PUBLIC
const char*
maac_cpe_state_name(maac_u32 state);

maac_const
MAAC_PUBLIC
size_t
maac_cpe_state_name_len(maac_u32 state);


maac_const
MAAC_PUBLIC
size_t
maac_pce_size(void);

maac_const
MAAC_PUBLIC
size_t
maac_pce_alignof(void);

maac_pure
MAAC_PUBLIC
maac_pce*
maac_pce_align(void*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_state(const maac_pce*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_element_instance_tag(const maac_pce*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_front_channels(const maac_pce*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_back_channels(const maac_pce*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_side_channels(const maac_pce*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_lfe_channels(const maac_pce*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_channels(const maac_pce*);

maac_const
MAAC_PUBLIC
const char*
maac_pce_state_name(maac_u32 state);

maac_const
MAAC_PUBLIC
size_t
maac_pce_state_name_len(maac_u32 state);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_element_instance_tag(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_profile(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_sampling_frequency_index(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_front_channel_elements(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_side_channel_elements(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_back_channel_elements(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_lfe_channel_elements(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_assoc_data_elements(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_valid_cc_elements(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_mono_mixdown_present(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_mono_mixdown_element_number(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_stereo_mixdown_present(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_stereo_mixdown_element_number(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_matrix_mixdown_idx_present(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_matrix_mixdown_idx(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_pseudo_surround_enable(const maac_pce* p);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_front_element_is_cpe(const maac_pce* p, maac_u32 idx);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_front_element_tag_select(const maac_pce* p, maac_u32 idx);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_side_element_is_cpe(const maac_pce* p, maac_u32 idx);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_side_element_tag_select(const maac_pce* p, maac_u32 idx);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_back_element_is_cpe(const maac_pce* p, maac_u32 idx);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_back_element_tag_select(const maac_pce* p, maac_u32 idx);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_lfe_element_tag_select(const maac_pce* p, maac_u32 idx);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_assoc_data_element_tag_select(const maac_pce* p, maac_u32 idx);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_cc_element_is_ind_sw(const maac_pce* p, maac_u32 idx);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_valid_cc_element_tag_select(const maac_pce* p, maac_u32 idx);

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_comment_field_bytes(const maac_pce* p);

maac_pure
MAAC_PUBLIC
const char*
maac_pce_comment_field_data(const maac_pce* p);


maac_const
MAAC_PUBLIC
size_t
maac_dse_size(void);

maac_const
MAAC_PUBLIC
size_t
maac_dse_alignof(void);

maac_pure
MAAC_PUBLIC
maac_dse*
maac_dse_align(void*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_dse_state(const maac_dse*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_dse_element_instance_tag(const maac_dse*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_dse_data_byte_align_flag(const maac_dse*);

maac_pure
MAAC_PUBLIC
maac_u32
maac_dse_count(const maac_dse*);

maac_pure
MAAC_PUBLIC
const unsigned char*
maac_dse_data_stream_byte(const maac_dse*);

maac_const
MAAC_PUBLIC
const char*
maac_dse_state_name(maac_u32 state);

maac_const
MAAC_PUBLIC
size_t
maac_dse_state_name_len(maac_u32 state);


maac_pure
MAAC_PUBLIC
void*
maac_align(void*,maac_u32);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_GUARD */

#ifdef MAAC_EXTRAS_IMPLEMENTATION
#ifndef MAAC_EXTRAS_IMPLEMENTATION_DEFINED
#define MAAC_EXTRAS_IMPLEMENTATION_DEFINED

static maac_inline
void* maac_align_inline(void* ptr, size_t align) {
    unsigned char* mem;
    size_t s;
    size_t o;

    if(align <= 1) return ptr;

    mem = (unsigned char *)ptr;
    s = (size_t)mem;

    o = ((s + align - 1) & (~(align-1))) - s;

    return (void*)&mem[o];
}

maac_const
MAAC_PUBLIC
size_t
maac_adts_size(void) {
    return sizeof(maac_adts);
}

struct maac_adts_aligner {
    char c;
    maac_adts a;
};

maac_const
MAAC_PUBLIC
size_t
maac_adts_alignof(void) {
    return offsetof(struct maac_adts_aligner, a);
}

maac_pure
MAAC_PUBLIC
maac_adts* maac_adts_align(void* p) {
    return (maac_adts*)maac_align_inline(p, offsetof(struct maac_adts_aligner, a));
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_tolerance(const maac_adts* a) {
    return a->tolerance;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_mpeg_version(const maac_adts* a) {
    return a->fixed_header.version;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_layer(const maac_adts* a) {
    return a->fixed_header.layer;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_protection_absent(const maac_adts* a) {
    return a->fixed_header.protection_absent;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_profile(const maac_adts* a) {
    return a->fixed_header.profile + 1;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_frequency_index(const maac_adts* a) {
    return a->fixed_header.sampling_frequency_index;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_sample_rate(const maac_adts* a) {
    return maac_sampling_frequency((maac_u32)a->fixed_header.sampling_frequency_index);
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_channel_configuration(const maac_adts* a) {
    return a->fixed_header.channel_configuration;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_channels(const maac_adts* a) {
    switch(a->fixed_header.channel_configuration) {
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        case 4: return 4;
        case 5: return 5;
        case 6: return 6;
        case 7: return 8;
        default: break;
    }
    return 0;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_original_copy(const maac_adts* a) {
    return a->fixed_header.original_copy;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_home(const maac_adts* a) {
    return a->fixed_header.home;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_copyright_id_bit(const maac_adts* a) {
    return a->variable_header.copyright_id_bit;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_copyright_id_start(const maac_adts* a) {
    return a->variable_header.copyright_id_start;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_frame_length(const maac_adts* a) {
    return a->variable_header.frame_length;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_buffer_fullness(const maac_adts* a) {
    return a->variable_header.buffer_fullness;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_raw_data_blocks(const maac_adts* a) {
    return a->variable_header.raw_data_blocks + 1;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_adts_raw_ele_id(const maac_adts* a) {
    return a->raw.ele_id;
}

MAAC_PUBLIC
void
maac_adts_set_tolerance(maac_adts* a, maac_u32 tolerance) {
    a->tolerance = tolerance;
}

MAAC_PUBLIC
void
maac_adts_set_num_out_channels(maac_adts* a, maac_u32 num) {
    a->raw.num_out_channels = num;
}

MAAC_PUBLIC
void
maac_adts_set_out_channels(maac_adts* a, maac_channel* ch) {
    a->raw.out_channels = ch;
}

maac_const
MAAC_PUBLIC
size_t
maac_bitreader_size(void) {
    return sizeof(maac_bitreader);
}

struct maac_bitreader_aligner {
    char c;
    maac_bitreader b;
};

maac_const
MAAC_PUBLIC
size_t
maac_bitreader_alignof(void) {
    return offsetof(struct maac_bitreader_aligner, b);
}

MAAC_PUBLIC
maac_bitreader*
maac_bitreader_align(void* ptr) {
    return (maac_bitreader*)maac_align_inline(ptr, offsetof(struct maac_bitreader_aligner, b));
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_bitreader_avail(const maac_bitreader* br) {
    return br->len - br->pos;
}

MAAC_PUBLIC
void
maac_bitreader_set_data(maac_bitreader* br, const maac_u8* data) {
    br->data = data;
}

MAAC_PUBLIC
void
maac_bitreader_set_pos(maac_bitreader* br, maac_u32 pos) {
    br->pos = pos;
}

MAAC_PUBLIC
void
maac_bitreader_set_len(maac_bitreader* br, maac_u32 len) {
    br->len = len;
}

/* getter functions */

maac_pure
MAAC_PUBLIC
const maac_u8*
maac_bitreader_data(const maac_bitreader* br) {
    return br->data;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_bitreader_pos(const maac_bitreader* br) {
    return br->pos;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_bitreader_len(const maac_bitreader* br) {
    return br->len;
}

maac_const
MAAC_PUBLIC
size_t
maac_channel_size(void) {
    return sizeof(maac_channel);
}

struct maac_channel_aligner {
    char c;
    maac_channel ch;
};

maac_const
MAAC_PUBLIC
size_t
maac_channel_alignof(void) {
    return offsetof(struct maac_channel_aligner, ch);
}

maac_pure
MAAC_PUBLIC
maac_channel* maac_channel_align(void* p) {
    return (maac_channel*)maac_align_inline(p, offsetof(struct maac_channel_aligner, ch));
}

maac_pure
MAAC_PUBLIC
maac_flt* maac_channel_samples(maac_channel* ch) {
    return &ch->samples[0];
}

maac_pure
MAAC_PUBLIC
maac_u32 maac_channel_n_samples(const maac_channel* ch) {
    return ch->n_samples - ch->_n;
}

static const maac_flt maac_channel_divisor = MAAC_FLT_C(1.0) / MAAC_FLT_C(32768.0);
static const maac_f32 maac_channel_max = MAAC_F32_C(32767.0) / MAAC_F32_C(32768.0);

MAAC_PUBLIC
void
maac_channel_samples_float(maac_channel* ch, float* s, maac_u32 lim) {
    maac_u32 i;
    maac_flt t;
    if(lim > (maac_u32)(ch->n_samples - ch->_n)) {
        lim = ch->n_samples - ch->_n;
    }

    for(i=0;i<lim;i++) {
        t = ch->samples[ch->_n++] * maac_channel_divisor;
        t = maac_clamp(t, MAAC_FLT_C(-1.0), maac_flt_cast(maac_channel_max));
        s[i] = (float)t;
    }
}

MAAC_PUBLIC
void
maac_channel_samples_s16(maac_channel* ch, maac_s16* s, maac_u32 lim) {
    maac_u32 i;
    maac_s32 t;
    if(lim > (maac_u32)(ch->n_samples - ch->_n)) {
        lim = ch->n_samples - ch->_n;
    }

    for(i=0;i<lim;i++) {
        t = (maac_s32)ch->samples[ch->_n++];
        t = maac_clamp(t, MAAC_S32_C(-32768), MAAC_S32_C(32767));
        s[i] = (maac_s16)t;
    }
}

maac_const
MAAC_PUBLIC
size_t
maac_cpe_size(void) {
    return sizeof(maac_cpe);
}

struct maac_cpe_aligner {
    char c;
    maac_cpe cpe;
};

maac_const
MAAC_PUBLIC
size_t
maac_cpe_alignof(void) {
    return offsetof(struct maac_cpe_aligner, cpe);
}

maac_pure
MAAC_PUBLIC
maac_cpe*
maac_cpe_align(void* p) {
    return (maac_cpe*)maac_align_inline(p, maac_cpe_alignof());
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_cpe_state(const maac_cpe* c) {
    return (maac_u32)c->state;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_cpe_element_instance_tag(const maac_cpe* c) {
    return (maac_u32)c->element_instance_tag;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_cpe_common_window(const maac_cpe* c) {
    return (maac_u32)c->common_window;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_cpe_ms_mask_present(const maac_cpe* c) {
    return (maac_u32)c->ms_mask_present;
}

static const char* MAAC_CPE_STATE_INVALID_STR = "INVALID";
static size_t MAAC_CPE_STATE_INVALID_LEN = sizeof("INVALID") - 1;

static const char* const maac_cpe_state_str_tbl[7] = {
    "TAG",
    "COMMON_WINDOW",
    "ICS_INFO",
    "MS_MASK_PRESENT",
    "MS_USED",
    "ICS_LEFT",
    "ICS_RIGHT"
};

static const size_t maac_cpe_state_len_tbl[7] = {
    sizeof("TAG") - 1,
    sizeof("COMMON_WINDOW") - 1,
    sizeof("ICS_INFO") - 1,
    sizeof("MS_MASK_PRESENT") - 1,
    sizeof("MS_USED") - 1,
    sizeof("ICS_LEFT") - 1,
    sizeof("ICS_RIGHT") - 1
};


maac_const
MAAC_PUBLIC
const char*
maac_cpe_state_name(const maac_u32 state) {
    return state > MAAC_CPE_STATE_ICS_RIGHT ?
      MAAC_CPE_STATE_INVALID_STR
      :
      maac_cpe_state_str_tbl[state];
}

maac_const
MAAC_PUBLIC
size_t
maac_cpe_state_name_len(const maac_u32 state) {
    return state > MAAC_CPE_STATE_ICS_RIGHT ?
      MAAC_CPE_STATE_INVALID_LEN
      :
      maac_cpe_state_len_tbl[state];
}


maac_const
MAAC_PUBLIC
size_t
maac_dse_size(void) {
    return sizeof(maac_dse);
}

struct maac_dse_aligner {
    char c;
    maac_dse d;
};

maac_const
MAAC_PUBLIC
size_t
maac_dse_alignof(void) {
    return offsetof(struct maac_dse_aligner, d);
}

maac_pure
MAAC_PUBLIC
maac_dse*
maac_dse_align(void* p) {
    return (maac_dse*)maac_align_inline(p, maac_dse_alignof());
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_dse_state(const maac_dse* d) {
    return (maac_u32)d->state;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_dse_element_instance_tag(const maac_dse* d) {
    return (maac_u32)d->element_instance_tag;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_dse_data_byte_align_flag(const maac_dse* d) {
    return (maac_u32)d->data_byte_align_flag;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_dse_count(const maac_dse* d) {
    return (maac_u32)d->count;
}

maac_pure
MAAC_PUBLIC
const unsigned char*
maac_dse_data_stream_byte(const maac_dse* d) {
    return (const unsigned char*)d->data_stream_byte;
}

static const char* MAAC_DSE_STATE_INVALID_STR = "INVALID";
static size_t MAAC_DSE_STATE_INVALID_LEN = sizeof("INVALID") - 1;

static const char* const maac_dse_state_str_tbl[5] = {
    "ELEMENT_INSTANCE_TAG",
    "DATA_BYTE_ALGN_FLAG",
    "COUNT",
    "ESC_COUNT",
    "DATA_STREAM_BYTE"
};

static const size_t maac_dse_state_len_tbl[5] = {
    sizeof("ELEMENT_INSTANCE_TAG") - 1,
    sizeof("DATA_BYTE_ALGN_FLAG") - 1,
    sizeof("COUNT") - 1,
    sizeof("ESC_COUNT") - 1,
    sizeof("DATA_STREAM_BYTE") - 1
};

maac_const
MAAC_PUBLIC
const char*
maac_dse_state_name(maac_u32 state) {
    return state > MAAC_DSE_STATE_DATA_STREAM_BYTE ?
      MAAC_DSE_STATE_INVALID_STR
      :
      maac_dse_state_str_tbl[state];
}

maac_const
MAAC_PUBLIC
size_t
maac_dse_state_name_len(maac_u32 state) {
    return state > MAAC_DSE_STATE_DATA_STREAM_BYTE ?
    MAAC_DSE_STATE_INVALID_LEN
    :
    maac_dse_state_len_tbl[state];
}

maac_const
MAAC_PUBLIC
size_t
maac_fil_size(void) {
    return sizeof(maac_fil);
}

struct maac_fil_aligner {
    char c;
    maac_fil f;
};

maac_const
MAAC_PUBLIC
size_t
maac_fil_alignof(void) {
    return offsetof(struct maac_fil_aligner, f);
}

maac_pure
MAAC_PUBLIC
maac_fil*
maac_fil_align(void* p) {
    return (maac_fil*)maac_align_inline(p, offsetof(struct maac_fil_aligner, f));
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_fil_extension_type(const maac_fil* f) {
    return f->extension_type;
}

static const char* MAAC_FIL_EXT_INVALID_STR = "INVALID";
static const size_t MAAC_FIL_EXT_INVALID_LEN = sizeof("INVALID") - 1;

static const char* const maac_fil_extension_type_str_tbl[16] = {
    /* 0x00 */ "FILL",
    /* 0x01 */ "FILL_DATA",
    /* 0x02 */ "INVALID",
    /* 0x03 */ "INVALID",
    /* 0x04 */ "INVALID",
    /* 0x05 */ "INVALID",
    /* 0x06 */ "INVALID",
    /* 0x07 */ "INVALID",
    /* 0x08 */ "INVALID",
    /* 0x09 */ "INVALID",
    /* 0x0a */ "INVALID",
    /* 0x0b */ "DYNAMIC_RANGE",
    /* 0x0c */ "INVALID",
    /* 0x0d */ "SBR_DATA",
    /* 0x0e */ "SBR_DATA_CRC",
    /* 0x00 */ "INVALID"
};

static const size_t maac_fil_extension_type_len_tbl[16] = {
    /* 0x00 */ sizeof("FILL") - 1,
    /* 0x01 */ sizeof("FILL_DATA") - 1,
    /* 0x02 */ sizeof("INVALID") - 1,
    /* 0x03 */ sizeof("INVALID") - 1,
    /* 0x04 */ sizeof("INVALID") - 1,
    /* 0x05 */ sizeof("INVALID") - 1,
    /* 0x06 */ sizeof("INVALID") - 1,
    /* 0x07 */ sizeof("INVALID") - 1,
    /* 0x08 */ sizeof("INVALID") - 1,
    /* 0x09 */ sizeof("INVALID") - 1,
    /* 0x0a */ sizeof("INVALID") - 1,
    /* 0x0b */ sizeof("DYNAMIC_RANGE") - 1,
    /* 0x0c */ sizeof("INVALID") - 1,
    /* 0x0d */ sizeof("SBR_DATA") - 1,
    /* 0x0e */ sizeof("SBR_DATA_CRC") - 1,
    /* 0x00 */ sizeof("INVALID") - 1
};

maac_const
MAAC_PUBLIC
const char*
maac_fil_extension_type_name(const maac_u32 id) {
    return id > 15 ? MAAC_FIL_EXT_INVALID_STR : 
        maac_fil_extension_type_str_tbl[id];
}

maac_const
MAAC_PUBLIC
size_t
maac_fil_extension_type_name_len(const maac_u32 id) {
    return id > 15 ? MAAC_FIL_EXT_INVALID_LEN : 
        maac_fil_extension_type_len_tbl[id];
}

maac_const
MAAC_PUBLIC
size_t
maac_ics_size(void) {
    return sizeof(maac_ics);
}

struct maac_ics_aligner {
    char c;
    maac_ics ics;
};

maac_const
MAAC_PUBLIC
size_t
maac_ics_alignof(void) {
    return offsetof(struct maac_ics_aligner, ics);
}


maac_pure
MAAC_PUBLIC
maac_ics*
maac_ics_align(void* p) {
    return (maac_ics*)maac_align_inline(p, offsetof(struct maac_ics_aligner, ics));
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_state(const maac_ics* ics) {
    return (maac_u32)ics->state;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_global_gain(const maac_ics* ics) {
    return (maac_u32)ics->global_gain;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_pulse_data_present(const maac_ics* ics) {
    return (maac_u32)ics->pulse_data_present;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_tns_data_present(const maac_ics* ics) {
    return (maac_u32)ics->tns_data_present;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_gain_control_data_present(const maac_ics* ics) {
    return (maac_u32)ics->gain_control_data_present;
}

static const char* MAAC_ICS_STATE_INVALID_STR = "INVALID";
static size_t MAAC_ICS_STATE_INVALID_LEN = sizeof("INVALID") - 1;

static const char* const maac_ics_state_str_tbl[18] = {
    "GLOBAL_GAIN",
    "ICS_INFO",
    "SECTION_CODEBOOK",
    "SECTION_CODEBOOK_LENGTH",
    "SCALE_FACTOR_DATA",
    "PULSE_DATA_PRESENT",
    "PULSE_DATA",
    "TNS_DATA_PRESENT",
    "TNS_DATA_N_FILT",
    "TNS_DATA_COEF_RES",
    "TNS_DATA_LENGTH",
    "TNS_DATA_ORDER",
    "TNS_DATA_DIRECTION",
    "TNS_DATA_COEF_COMPRESS",
    "TNS_DATA_COEF",
    "GAIN_CONTROL_DATA_PRESENT",
    "GAIN_CONTROL_DATA",
    "SPECTRAL_DATA"
};

static const size_t maac_ics_state_len_tbl[18] = {
    sizeof("GLOBAL_GAIN") - 1,
    sizeof("ICS_INFO") - 1,
    sizeof("SECTION_CODEBOOK") - 1,
    sizeof("SECTION_CODEBOOK_LENGTH") - 1,
    sizeof("SCALE_FACTOR_DATA") - 1,
    sizeof("PULSE_DATA_PRESENT") - 1,
    sizeof("PULSE_DATA") - 1,
    sizeof("TNS_DATA_PRESENT") - 1,
    sizeof("TNS_DATA_N_FILT") - 1,
    sizeof("TNS_DATA_COEF_RES") - 1,
    sizeof("TNS_DATA_LENGTH") - 1,
    sizeof("TNS_DATA_ORDER") - 1,
    sizeof("TNS_DATA_DIRECTION") - 1,
    sizeof("TNS_DATA_COEF_COMPRESS") - 1,
    sizeof("TNS_DATA_COEF") - 1,
    sizeof("GAIN_CONTROL_DATA_PRESENT") - 1,
    sizeof("GAIN_CONTROL_DATA") - 1,
    sizeof("SPECTRAL_DATA") - 1
};


maac_const
MAAC_PUBLIC
const char*
maac_ics_state_name(const maac_u32 state) {
    return state > MAAC_ICS_STATE_SPECTRAL_DATA ?
      MAAC_ICS_STATE_INVALID_STR
      :
      maac_ics_state_str_tbl[state];
}

maac_const
MAAC_PUBLIC
size_t
maac_ics_state_name_len(const maac_u32 state) {
    return state > MAAC_ICS_STATE_SPECTRAL_DATA ?
      MAAC_ICS_STATE_INVALID_LEN
      :
      maac_ics_state_len_tbl[state];
}

maac_const
MAAC_PUBLIC
size_t
maac_ics_info_size(void) {
    return sizeof(maac_ics_info);
}

struct maac_ics_info_aligner {
    char c;
    maac_ics_info ics_info;
};

maac_const
MAAC_PUBLIC
size_t
maac_ics_info_alignof(void) {
    return offsetof(struct maac_ics_info_aligner, ics_info);
}

maac_pure
MAAC_PUBLIC
maac_ics_info*
maac_ics_info_align(void* p) {
    return (maac_ics_info*)maac_align_inline(p, offsetof(struct maac_ics_info_aligner, ics_info));
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_state(const maac_ics_info* ics_info) {
    return (maac_u32)ics_info->state;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_max_sfb(const maac_ics_info* ics_info) {
    return (maac_u32)ics_info->max_sfb;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_predictor_data_present(const maac_ics_info* ics_info) {
    return (maac_u32)ics_info->predictor_data_present;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_scale_factor_grouping(const maac_ics_info* ics_info) {
    return (maac_u32)ics_info->scale_factor_grouping;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_num_window_groups(const maac_ics_info* ics_info) {
    maac_u32 c = 1;
    maac_u32 pc = 0;
    maac_u8 sfg = ics_info->scale_factor_grouping;

    if(ics_info->window_sequence == MAAC_WINDOW_SEQUENCE_EIGHT_SHORT) {

        while(sfg) {
            sfg &= sfg - 1;
            ++pc;
        }
        c += 7 - pc;
    }

    return c;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_window_sequence(const maac_ics_info* ics_info) {
    return (maac_u32)ics_info->window_sequence;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_window_shape(const maac_ics_info* ics_info) {
    return (maac_u32)ics_info->window_shape;
}

#if MAAC_ENABLE_MAINPROFILE
maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_predictor_reset(const maac_ics_info* ics_info) {
    return (maac_u32)ics_info->predictor_reset;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_predictor_reset_group_number(const maac_ics_info* ics_info) {
    return (maac_u32)ics_info->predictor_reset_group_number;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_ics_info_prediction_used(const maac_ics_info* info, maac_u32 sfb) {
    if(sfb > 63) {
        return 0;
    }
    return (info->prediction_used[sfb/32] >> (sfb % 32)) & 0x01;
}

#endif

static const char* MAAC_ICS_INFO_STATE_INVALID_STR = "INVALID";
static const size_t MAAC_ICS_INFO_STATE_INVALID_LEN = sizeof("INVALID") - 1;

static const char* const maac_ics_info_state_str_tbl[MAAC_ICS_INFO_STATE_LAST+1] = {
    "RESERVED_BIT",
    "WINDOW_SEQUENCE",
    "WINDOW_SHAPE",
    "MAX_SFB",
    "SCALE_FACTOR_GROUPING",
    "PREDICTOR_DATA_PRESENT"
#if MAAC_ENABLE_MAINPROFILE
    ,"PREDICTOR_RESET",
    "PREDICTOR_RESET_GROUP_NUMBER",
    "PREDICTION_USED"
#endif
};

static const size_t maac_ics_info_state_len_tbl[MAAC_ICS_INFO_STATE_LAST+1] = {
    sizeof("RESERVED_BIT") - 1,
    sizeof("WINDOW_SEQUENCE") - 1,
    sizeof("WINDOW_SHAPE") - 1,
    sizeof("MAX_SFB") - 1,
    sizeof("SCALE_FACTOR_GROUPING") - 1,
    sizeof("PREDICTOR_DATA_PRESENT") - 1
#if MAAC_ENABLE_MAINPROFILE
    ,sizeof("PREDICTOR_RESET") - 1,
    sizeof("PREDICTOR_RESET_GROUP_NUMBER") - 1,
    sizeof("PREDICTION_USED") - 1
#endif
};

maac_const
MAAC_PUBLIC
const char*
maac_ics_info_state_name(const maac_u32 state) {
    return state > MAAC_ICS_INFO_STATE_LAST ?
      MAAC_ICS_INFO_STATE_INVALID_STR
      :
      maac_ics_info_state_str_tbl[state];
}

maac_const
MAAC_PUBLIC
size_t
maac_ics_info_state_name_len(const maac_u32 state) {
    return state > MAAC_ICS_INFO_STATE_LAST ?
      MAAC_ICS_INFO_STATE_INVALID_LEN
      :
      maac_ics_info_state_len_tbl[state];
}

static const char* MAAC_WINDOW_SEQUENCE_INVALID = "INVALID";
size_t MAAC_WINDOW_SEQUENCE_INVALID_LEN = sizeof("INVALID") - 1;

static const char* const maac_window_sequence_str_tbl[4] = {
    "ONLY_LONG",
    "LONG_START",
    "EIGHT_SHORT",
    "LONG_STOP"
};

static const size_t maac_window_sequence_len_tbl[4] = {
    sizeof("ONLY_LONG") - 1,
    sizeof("LONG_START") - 1,
    sizeof("EIGHT_SHORT") - 1,
    sizeof("LONG_STOP") - 1
};

maac_const
MAAC_PUBLIC
const char*
maac_window_sequence_name(const maac_u32 seq) {
    return seq > MAAC_WINDOW_SEQUENCE_LONG_STOP ?
      MAAC_WINDOW_SEQUENCE_INVALID
      :
      maac_window_sequence_str_tbl[seq];
}

maac_const
MAAC_PUBLIC
size_t
maac_window_sequence_name_len(const maac_u32 seq) {
    return seq > MAAC_WINDOW_SEQUENCE_LONG_STOP ?
      MAAC_WINDOW_SEQUENCE_INVALID_LEN
      :
      maac_window_sequence_len_tbl[seq];
}

static const char* MAAC_RESULT_INVALID_STR = "INVALID";
static size_t MAAC_RESULT_INVALID_LEN = sizeof("INVALID") - 1;

static const char* const maac_result_str_tbl[17] = {
    "HUFFMAN_DECODE_ERROR",
    "ADTS_RDB_NOT_CALLED",
    "ADTS_SYNCWORD_NOT_FOUND",
    "PREDICTOR_DATA_NOT_IMPLEMENTED",
    "GAIN_CONTROL_DATA_NOT_IMPLEMENTED",
    "UNSUPPORTED_AOT",
    "PULSE_DATA_NOT_IMPLEMENTED",
    "PCE_NOT_IMPLEMENTED",
    "DSE_NOT_IMPLEMENTED",
    "LFE_NOT_IMPLEMENTED",
    "CCE_NOT_IMPLEMENTED",
    "SF_INDEX_NOT_SET",
    "OUT_OF_SEQUENCE",
    "NOT_IMPLEMENTED",
    "ERROR",
    "CONTINUE",
    "OK"
};

static const size_t maac_result_len_tbl[17] = {
    sizeof("HUFFMAN_DECODE_ERROR") - 1,
    sizeof("ADTS_RDB_NOT_CALLED") - 1,
    sizeof("ADTS_SYNCWORD_NOT_FOUND") - 1,
    sizeof("PREDICTOR_DATA_NOT_IMPLEMENTED") - 1,
    sizeof("GAIN_CONTROL_DATA_NOT_IMPLEMENTED") - 1,
    sizeof("UNSUPPORTED_AOT") - 1,
    sizeof("PULSE_DATA_NOT_IMPLEMENTED") - 1,
    sizeof("PCE_NOT_IMPLEMENTED") - 1,
    sizeof("DSE_NOT_IMPLEMENTED") - 1,
    sizeof("LFE_NOT_IMPLEMENTED") - 1,
    sizeof("CCE_NOT_IMPLEMENTED") - 1,
    sizeof("SF_INDEX_NOT_SET") - 1,
    sizeof("OUT_OF_SEQUENCE") - 1,
    sizeof("NOT_IMPLEMENTED") - 1,
    sizeof("ERROR") - 1,
    sizeof("CONTINUE") - 1,
    sizeof("OK") - 1
};


maac_const
MAAC_PUBLIC
const char*
maac_result_name(const maac_s32 result) {
    return result > MAAC_RESULT_MAX ?
      MAAC_RESULT_INVALID_STR
      :
        result < MAAC_RESULT_MIN ?
          MAAC_RESULT_INVALID_STR :
          maac_result_str_tbl[15 + result];
}

maac_const
MAAC_PUBLIC
size_t
maac_result_name_len(const maac_s32 result) {
    return result > MAAC_RESULT_MAX ?
      MAAC_RESULT_INVALID_LEN
      :
        result < MAAC_RESULT_MIN ?
          MAAC_RESULT_INVALID_LEN :
          maac_result_len_tbl[15 + result];
}

maac_const
MAAC_PUBLIC
size_t
maac_pce_size(void) {
    return sizeof(maac_pce);
}

struct maac_pce_aligner {
    char c;
    maac_pce p;
};

maac_const
MAAC_PUBLIC
size_t
maac_pce_alignof(void) {
    return offsetof(struct maac_pce_aligner, p);
}

maac_pure
MAAC_PUBLIC
maac_pce*
maac_pce_align(void* p) {
    return (maac_pce*)maac_align_inline(p, maac_pce_alignof());
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_state(const maac_pce* p) {
    return (maac_u32)p->state;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_front_channels(const maac_pce* p) {
    return p->num_front_channel_elements + maac_popcnt(p->front_element_is_cpe);
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_back_channels(const maac_pce* p) {
    return p->num_back_channel_elements + maac_popcnt(p->back_element_is_cpe);
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_side_channels(const maac_pce* p) {
    return p->num_side_channel_elements + maac_popcnt(p->side_element_is_cpe);
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_lfe_channels(const maac_pce* p) {
    return p->num_lfe_channel_elements;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_channels(const maac_pce* p) {
    return
      maac_pce_num_front_channels(p) +
      maac_pce_num_back_channels(p) +
      maac_pce_num_side_channels(p) +
      maac_pce_num_lfe_channels(p);
}

static const char* MAAC_PCE_STATE_INVALID_STR = "INVALID";
static size_t MAAC_PCE_STATE_INVALID_LEN = sizeof("INVALID") - 1;

static const char* const maac_pce_state_str_tbl[28] = {
    "ELEMENT_INSTANCE_TAG",
    "PROFILE",
    "SAMPLING_FREQUENCY_INDEX",
    "NUM_FRONT_CHANNEL_ELEMENTS",
    "NUM_SIDE_CHANNEL_ELEMENTS",
    "NUM_BACK_CHANNEL_ELEMENTS",
    "NUM_LFE_CHANNEL_ELEMENTS",
    "NUM_ASSOC_DATA_ELEMENTS",
    "NUM_VALID_CC_ELEMENTS",
    "MONO_MIXDOWN_PRESENT",
    "MONO_MIXDOWN_ELEMENT_NUMBER",
    "STEREO_MIXDOWN_PRESENT",
    "STEREO_MIXDOWN_ELEMENT_NUMBER",
    "MATRIX_MIXDOWN_IDX_PRESENT",
    "MATRIX_MIXDOWN_IDX",
    "PSEUDO_SURROUND_ENABLE",
    "FRONT_ELEMENT_IS_CPE",
    "FRONT_ELEMENT_TAG_SELECT",
    "SIDE_ELEMENT_IS_CPE",
    "SIDE_ELEMENT_TAG_SELECT",
    "BACK_ELEMENT_IS_CPE",
    "BACK_ELEMENT_TAG_SELECT",
    "LFE_ELEMENT_TAG_SELECT",
    "ASSOC_DATA_ELEMENT_TAG_SELECT",
    "CC_ELEMENT_IS_IND_SW",
    "VALID_CC_ELEMENT_TAG_SELECT",
    "COMMENT_FIELD_BYTES",
    "COMMENT_FIELD_DATA"
};

static const size_t maac_pce_state_len_tbl[28] = {
    sizeof("ELEMENT_INSTANCE_TAG") - 1,
    sizeof("PROFILE") - 1,
    sizeof("SAMPLING_FREQUENCY_INDEX") - 1,
    sizeof("NUM_FRONT_CHANNEL_ELEMENTS") - 1,
    sizeof("NUM_SIDE_CHANNEL_ELEMENTS") - 1,
    sizeof("NUM_BACK_CHANNEL_ELEMENTS") - 1,
    sizeof("NUM_LFE_CHANNEL_ELEMENTS") - 1,
    sizeof("NUM_ASSOC_DATA_ELEMENTS") - 1,
    sizeof("NUM_VALID_CC_ELEMENTS") - 1,
    sizeof("MONO_MIXDOWN_PRESENT") - 1,
    sizeof("MONO_MIXDOWN_ELEMENT_NUMBER") - 1,
    sizeof("STEREO_MIXDOWN_PRESENT") - 1,
    sizeof("STEREO_MIXDOWN_ELEMENT_NUMBER") - 1,
    sizeof("MATRIX_MIXDOWN_IDX_PRESENT") - 1,
    sizeof("MATRIX_MIXDOWN_IDX") - 1,
    sizeof("PSEUDO_SURROUND_ENABLE") - 1,
    sizeof("FRONT_ELEMENT_IS_CPE") - 1,
    sizeof("FRONT_ELEMENT_TAG_SELECT") - 1,
    sizeof("SIDE_ELEMENT_IS_CPE") - 1,
    sizeof("SIDE_ELEMENT_TAG_SELECT") - 1,
    sizeof("BACK_ELEMENT_IS_CPE") - 1,
    sizeof("BACK_ELEMENT_TAG_SELECT") - 1,
    sizeof("LFE_ELEMENT_TAG_SELECT") - 1,
    sizeof("ASSOC_DATA_ELEMENT_TAG_SELECT") - 1,
    sizeof("CC_ELEMENT_IS_IND_SW") - 1,
    sizeof("VALID_CC_ELEMENT_TAG_SELECT") - 1,
    sizeof("COMMENT_FIELD_BYTES") - 1,
    sizeof("COMMENT_FIELD_DATA") - 1
};


maac_const
MAAC_PUBLIC
const char*
maac_pce_state_name(const maac_u32 state) {
    return state > MAAC_PCE_STATE_COMMENT_FIELD_DATA ?
      MAAC_PCE_STATE_INVALID_STR
      :
      maac_pce_state_str_tbl[state];
}

maac_const
MAAC_PUBLIC
size_t
maac_pce_state_name_len(const maac_u32 state) {
    return state > MAAC_PCE_STATE_COMMENT_FIELD_DATA ?
      MAAC_PCE_STATE_INVALID_LEN
      :
      maac_pce_state_len_tbl[state];
}


maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_element_instance_tag(const maac_pce* p) {
  return (maac_u32)p->element_instance_tag;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_profile(const maac_pce* p) {
  return (maac_u32)p->profile;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_sampling_frequency_index(const maac_pce* p) {
  return (maac_u32)p->sampling_frequency_index;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_front_channel_elements(const maac_pce* p) {
  return (maac_u32)p->num_front_channel_elements;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_side_channel_elements(const maac_pce* p) {
  return (maac_u32)p->num_side_channel_elements;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_back_channel_elements(const maac_pce* p) {
  return (maac_u32)p->num_back_channel_elements;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_lfe_channel_elements(const maac_pce* p) {
  return (maac_u32)p->num_lfe_channel_elements;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_assoc_data_elements(const maac_pce* p) {
  return (maac_u32)p->num_assoc_data_elements;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_num_valid_cc_elements(const maac_pce* p) {
  return (maac_u32)p->num_valid_cc_elements;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_mono_mixdown_present(const maac_pce* p) {
  return (maac_u32)p->mono_mixdown_present;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_mono_mixdown_element_number(const maac_pce* p) {
  return (maac_u32)p->mono_mixdown_element_number;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_stereo_mixdown_present(const maac_pce* p) {
  return (maac_u32)p->stereo_mixdown_present;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_stereo_mixdown_element_number(const maac_pce* p) {
  return (maac_u32)p->stereo_mixdown_element_number;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_matrix_mixdown_idx_present(const maac_pce* p) {
  return (maac_u32)p->matrix_mixdown_idx_present;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_matrix_mixdown_idx(const maac_pce* p) {
  return (maac_u32)p->matrix_mixdown_idx;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_pseudo_surround_enable(const maac_pce* p) {
  return (maac_u32)p->pseudo_surround_enable;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_front_element_is_cpe(const maac_pce* p, maac_u32 idx) {
  return (maac_u32)((p->front_element_is_cpe >> idx) & 1);
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_front_element_tag_select(const maac_pce* p, maac_u32 idx) {
  return (maac_u32)( (p->front_element_tag_select[idx/2] >> (4 * (idx % 2))) & 0x0f);
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_side_element_is_cpe(const maac_pce* p, maac_u32 idx) {
  return (maac_u32)((p->side_element_is_cpe >> idx) & 1);
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_side_element_tag_select(const maac_pce* p, maac_u32 idx) {
  return (maac_u32)( (p->side_element_tag_select[idx/2] >> (4 * (idx % 2))) & 0x0f);
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_back_element_is_cpe(const maac_pce* p, maac_u32 idx) {
  return (maac_u32)((p->back_element_is_cpe >> idx) & 1);
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_back_element_tag_select(const maac_pce* p, maac_u32 idx) {
  return (maac_u32)( (p->back_element_tag_select[idx/2] >> (4 * (idx % 2))) & 0x0f);
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_lfe_element_tag_select(const maac_pce* p, maac_u32 idx) {
  return (maac_u32)( (p->lfe_element_tag_select[idx/2] >> (4 * (idx % 2))) & 0x0f);
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_assoc_data_element_tag_select(const maac_pce* p, maac_u32 idx) {
  return (maac_u32)( (p->assoc_data_element_tag_select[idx/2] >> (4 * (idx % 2))) & 0x0f);
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_cc_element_is_ind_sw(const maac_pce* p, maac_u32 idx) {
  return (maac_u32)((p->cc_element_is_ind_sw >> idx) & 1);
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_valid_cc_element_tag_select(const maac_pce* p, maac_u32 idx) {
  return (maac_u32)( (p->valid_cc_element_tag_select[idx/2] >> (4 * (idx % 2))) & 0x0f);
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_pce_comment_field_bytes(const maac_pce* p) {
  return (maac_u32)p->comment_field_bytes;
}

maac_pure
MAAC_PUBLIC
const char*
maac_pce_comment_field_data(const maac_pce* p) {
  return (const char*)p->comment_field_data;
}

maac_pure
MAAC_PUBLIC
maac_u32 maac_raw_channels(const maac_raw* r) {
    return maac_channel_config_channels(r->channel_configuration);
}

maac_const
MAAC_PUBLIC
size_t
maac_raw_size(void) {
    return sizeof(maac_raw);
}

struct maac_raw_aligner {
    char c;
    maac_raw r;
};

maac_const
MAAC_PUBLIC
size_t
maac_raw_alignof(void) {
    return offsetof(struct maac_raw_aligner, r);
}

maac_pure
MAAC_PUBLIC
maac_raw* maac_raw_align(void* p) {
    return (maac_raw*)maac_align_inline(p, offsetof(struct maac_raw_aligner, r));
}

maac_pure
MAAC_PUBLIC
maac_u32 maac_raw_state(const maac_raw* r) {
    return (maac_u32)r->state;
}

maac_pure
MAAC_PUBLIC
maac_u32 maac_raw_sf_index(const maac_raw* r) {
    return (maac_u32)r->sf_index;
}

maac_pure
MAAC_PUBLIC
maac_u32 maac_raw_ele_id(const maac_raw* r) {
    return (maac_u32)r->ele_id;
}

maac_pure
MAAC_PUBLIC
maac_u32 maac_raw_sample_rate(const maac_raw* r) {
    return (maac_u32)r->sample_rate;
}

maac_pure
MAAC_PUBLIC
maac_u32 maac_raw_num_out_channels(const maac_raw* r) {
    return (maac_u32)r->num_out_channels;
}

maac_pure
MAAC_PUBLIC
maac_u32 maac_raw_channel_configuration(const maac_raw* r) {
    return (maac_u32)r->channel_configuration;
}

maac_pure
MAAC_PUBLIC
const maac_channel* maac_raw_out_channels(const maac_raw* r) {
    return r->out_channels;
}

MAAC_PUBLIC
maac_fil*
maac_raw_fil(maac_raw* r) {
    return &r->ele.fil;
}

MAAC_PUBLIC
maac_sce*
maac_raw_sce(maac_raw* r) {
    return &r->ele.sce;
}

MAAC_PUBLIC
maac_cpe*
maac_raw_cpe(maac_raw* r) {
    return &r->ele.cpe;
}

static const char* MAAC_RAW_DATA_BLOCK_ID_INVALID_STR = "INVALID";
static size_t MAAC_RAW_DATA_BLOCK_ID_INVALID_LEN = sizeof("INVALID") - 1;

static const char* const maac_raw_data_block_id_str_tbl[8] = {
    "SCE",
    "CPE",
    "CCE",
    "LFE",
    "DSE",
    "PCE",
    "FIL",
    "END"
};

static const size_t maac_raw_data_block_id_len_tbl[8] = {
    sizeof("SCE") - 1,
    sizeof("CPE") - 1,
    sizeof("CCE") - 1,
    sizeof("LFE") - 1,
    sizeof("DSE") - 1,
    sizeof("PCE") - 1,
    sizeof("FIL") - 1,
    sizeof("END") - 1
};

maac_const
MAAC_PUBLIC
const char*
maac_raw_data_block_id_name(const maac_u32 id) {
    return id > MAAC_RAW_DATA_BLOCK_ID_END ?
      MAAC_RAW_DATA_BLOCK_ID_INVALID_STR
      :
      maac_raw_data_block_id_str_tbl[id];
}

maac_const
MAAC_PUBLIC
size_t
maac_raw_data_block_id_name_len(const maac_u32 id) {
    return id > MAAC_RAW_DATA_BLOCK_ID_END ?
      MAAC_RAW_DATA_BLOCK_ID_INVALID_LEN
      :
      maac_raw_data_block_id_len_tbl[id];
}

MAAC_PUBLIC
void
maac_raw_set_out_channels(maac_raw* r, maac_channel* ch) {
    r->out_channels = ch;
}

MAAC_PUBLIC
void
maac_raw_set_num_out_channels(maac_raw* r, maac_u32 n) {
    r->num_out_channels = n;
}

maac_const
MAAC_PUBLIC
size_t
maac_sce_size(void) {
    return sizeof(maac_sce);
}

struct maac_sce_aligner {
    char c;
    maac_sce s;
};

maac_const
MAAC_PUBLIC
size_t
maac_sce_alignof(void) {
    return offsetof(struct maac_sce_aligner, s);
}

maac_pure
MAAC_PUBLIC
maac_sce*
maac_sce_align(void* p) {
    return (maac_sce*)maac_align_inline(p, maac_sce_alignof());
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_sce_state(const maac_sce* s) {
    return (maac_u32)s->state;
}

maac_pure
MAAC_PUBLIC
maac_u32
maac_sce_element_instance_tag(const maac_sce* s) {
    return (maac_u32)s->element_instance_tag;
}

static const char* MAAC_SCE_STATE_INVALID_STR = "INVALID";
static size_t MAAC_SCE_STATE_INVALID_LEN = sizeof("INVALID") - 1;

static const char* const maac_sce_state_str_tbl[2] = {
    "TAG",
    "ICS"
};

static const size_t maac_sce_state_len_tbl[2] = {
    sizeof("TAG") - 1,
    sizeof("ICS") - 1
};


maac_const
MAAC_PUBLIC
const char*
maac_sce_state_name(const maac_u32 state) {
    return state > MAAC_SCE_STATE_TAG ?
      MAAC_SCE_STATE_INVALID_STR
      :
      maac_sce_state_str_tbl[state];
}

maac_const
MAAC_PUBLIC
size_t
maac_sce_state_name_len(const maac_u32 state) {
    return state > MAAC_SCE_STATE_TAG ?
      MAAC_SCE_STATE_INVALID_LEN
      :
      maac_sce_state_len_tbl[state];
}

maac_pure
MAAC_PUBLIC
void*
maac_align(void* ptr, maac_u32 align) {
    return maac_align_inline(ptr, (size_t)align);
}

#endif /* EXTRAS_IMPLEMENTATION_DEFINED */
#endif


/*

Copyright (c) 2026 John Regan

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

*/
