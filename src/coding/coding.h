#ifndef _CODING_H
#define _CODING_H

#include "../vgmstream.h"
//todo remove
#include "hca_decoder_clhca.h"

/* adx_decoder */
void decode_adx(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int32_t frame_bytes, coding_t coding_type);
void adx_next_key(VGMSTREAMCHANNEL* stream);


/* g721_decoder */
void decode_g721(VGMSTREAMCHANNEL* stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void g72x_init_state(struct g72x_state* state_ptr);


/* ima_decoder */
void decode_standard_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo, int is_high_first);
void decode_nw_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_snds_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_otns_ima(VGMSTREAM* vgmstream, VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_wv6_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_hv_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_ffta2_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_blitz_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_mtf_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo);

void decode_ms_ima(VGMSTREAM* vgmstream, VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel);
void decode_ref_ima(VGMSTREAM* vgmstream, VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel);

void decode_xbox_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo);
void decode_xbox_ima_mch(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_nds_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_dat4_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_rad_ima(VGMSTREAM* vgmstream, VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel);
void decode_rad_ima_mono(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_apple_ima4(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_fsb_ima(VGMSTREAM* vgmstream, VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_wwise_ima(VGMSTREAM* vgmstream, VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_awc_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_ubi_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_ubi_sce_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_h4m_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, uint16_t frame_format);
void decode_cd_ima(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
size_t ima_bytes_to_samples(size_t bytes, int channels);
size_t ms_ima_bytes_to_samples(size_t bytes, int block_align, int channels);
size_t xbox_ima_bytes_to_samples(size_t bytes, int channels);
size_t dat4_ima_bytes_to_samples(size_t bytes, int channels);
size_t apple_ima4_bytes_to_samples(size_t bytes, int channels);
int xbox_check_format(STREAMFILE* sf, uint32_t offset, uint32_t max, int channels);

/* ngc_dsp_decoder */
void decode_ngc_dsp(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_ngc_dsp_subint(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int interleave);
size_t dsp_bytes_to_samples(size_t bytes, int channels);
int32_t dsp_nibbles_to_samples(int32_t nibbles);
void dsp_read_coefs_be(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t offset, off_t spacing);
void dsp_read_coefs_le(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t offset, off_t spacing);
void dsp_read_coefs(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t offset, off_t spacing, int be);
void dsp_read_hist_be(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t offset, off_t spacing);
void dsp_read_hist_le(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t offset, off_t spacing);
void dsp_read_hist(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t offset, off_t spacing, int be);


/* ngc_dtk_decoder */
void decode_ngc_dtk(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);


/* ngc_afc_decoder */
void decode_ngc_afc(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);


/* vadpcm_decoder */
void decode_vadpcm(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int order);
//int32_t vadpcm_bytes_to_samples(size_t bytes, int channels);
void vadpcm_read_coefs_be(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t offset, int order, int entries, int ch);


/* pcm_decoder */
void decode_pcm16le(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm16be(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm16_int(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int big_endian);
void decode_pcm8(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm8_int(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm8_unsigned(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm8_unsigned_int(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm8_sb(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm4(VGMSTREAM* vgmstream, VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_pcm4_unsigned(VGMSTREAM* vgmstream, VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_ulaw(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_ulaw_int(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_alaw(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcmfloat(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int big_endian);
void decode_pcm24le(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
int32_t pcm_bytes_to_samples(size_t bytes, int channels, int bits_per_sample);
int32_t pcm16_bytes_to_samples(size_t bytes, int channels);
int32_t pcm8_bytes_to_samples(size_t bytes, int channels);


/* psx_decoder */
void decode_psx(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int is_badflags, int config);
void decode_psx_configurable(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int frame_size, int config);
void decode_psx_pivotal(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int frame_size);
int ps_find_loop_offsets(STREAMFILE* sf, off_t start_offset, size_t data_size, int channels, size_t interleave, int32_t* out_loop_start, int32_t* out_loop_end);
int ps_find_loop_offsets_full(STREAMFILE* sf, off_t start_offset, size_t data_size, int channels, size_t interleave, int32_t* out_loop_start, int32_t* out_loop_end);
size_t ps_find_padding(STREAMFILE* sf, off_t start_offset, size_t data_size, int channels, size_t interleave, int discard_empty);
size_t ps_bytes_to_samples(size_t bytes, int channels);
size_t ps_cfg_bytes_to_samples(size_t bytes, size_t frame_size, int channels);
int ps_check_format(STREAMFILE* sf, off_t offset, size_t max);


/* psv_decoder */
void decode_hevag(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);


/* xa_decoder */
void decode_xa(VGMSTREAM* v, sample_t* outbuf, int32_t samples_to_do);
size_t xa_bytes_to_samples(size_t bytes, int channels, int is_blocked, int is_form2, int bps);


/* ea_xa_decoder */
void decode_ea_xa(VGMSTREAMCHANNEL* stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo);
void decode_ea_xa_v2(VGMSTREAMCHANNEL* stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_maxis_xa(VGMSTREAMCHANNEL* stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
int32_t ea_xa_bytes_to_samples(size_t bytes, int channels);


/* ea_xas_decoder */
void decode_ea_xas_v0(VGMSTREAMCHANNEL* stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_ea_xas_v1(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);


/* sdx2_decoder */
void decode_sdx2(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_sdx2_int(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_cbd2(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_cbd2_int(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);


/* ws_decoder */
void decode_ws(VGMSTREAM* vgmstream, int channel, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);


/* acm_decoder */
acm_codec_data* init_acm(STREAMFILE* sf, int force_channel_number);
void decode_acm(acm_codec_data* data, sample_t* outbuf, int32_t samples_to_do, int channelspacing);
void reset_acm(acm_codec_data* data);
void free_acm(acm_codec_data* data);
STREAMFILE* acm_get_streamfile(acm_codec_data* data);


/* nwa_decoder */
typedef struct nwa_codec_data nwa_codec_data;

nwa_codec_data* init_nwa(STREAMFILE* sf);
void decode_nwa(nwa_codec_data* data, sample_t* outbuf, int32_t samples_to_do);
void seek_nwa(nwa_codec_data *data, int32_t sample);
void reset_nwa(nwa_codec_data *data);
void free_nwa(nwa_codec_data* data);
STREAMFILE* nwa_get_streamfile(nwa_codec_data* data);

/* msadpcm_decoder */
#define MSADPCM_MAX_BLOCK_SIZE  0x800 /* known max and RIFF spec seems to concur, while MS's encoders may be lower (typical stereo: 0x8c, 0x2C, 0x48, 0x400) */

void decode_msadpcm_stereo(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t first_sample, int32_t samples_to_do);
void decode_msadpcm_mono(VGMSTREAM* vgmstream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int config);
void decode_msadpcm_ck(VGMSTREAM* vgmstream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
long msadpcm_bytes_to_samples(long bytes, int block_size, int channels);
int msadpcm_check_coefs(STREAMFILE* sf, uint32_t offset);


/* yamaha_decoder */
void decode_aica(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo, int is_high_first);
void decode_cp_ym(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo);
void decode_aska(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, size_t frame_size);
void decode_nxap(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
size_t yamaha_bytes_to_samples(size_t bytes, int channels);
size_t aska_bytes_to_samples(size_t bytes, size_t frame_size, int channels);


/* tgcadpcm_decoder */
void decode_tgc(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int32_t first_sample, int32_t samples_to_do);


/* nds_procyon_decoder */
void decode_nds_procyon(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);


/* l5_555_decoder */
void decode_l5_555(VGMSTREAMCHANNEL* stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);


/* sassc_decoder */
void decode_sassc(VGMSTREAMCHANNEL* stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);


/* lsf_decode */
void decode_lsf(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);


/* mtaf_decoder */
void decode_mtaf(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);


/* mta2_decoder */
void decode_mta2(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int config);


/* mc3_decoder */
void decode_mc3(VGMSTREAM* vgmstream, VGMSTREAMCHANNEL* stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);


/* fadpcm_decoder */
void decode_fadpcm(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);


/* asf_decoder */
void decode_asf(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
int32_t asf_bytes_to_samples(size_t bytes, int channels);


/* dsa_decoder */
void decode_dsa(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);


/* xmd_decoder */
void decode_xmd(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, size_t frame_size);


/* tantalus_decoder */
void decode_tantalus(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
int32_t tantalus_bytes_to_samples(size_t bytes, int channels);


/* derf_decoder */
void decode_derf(VGMSTREAMCHANNEL* stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* wady_decoder */
void decode_wady(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* circus_decoder */
typedef struct circus_codec_data circus_codec_data;

circus_codec_data* init_circus_vq(STREAMFILE* sf, off_t start, uint8_t codec, uint8_t flags);
void decode_circus_vq(circus_codec_data* data, sample_t* outbuf, int32_t samples_to_do, int channels);
void reset_circus_vq(circus_codec_data* data);
void seek_circus_vq(circus_codec_data* data, int32_t num_sample);
void free_circus_vq(circus_codec_data* data);
void decode_circus_adpcm(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);


/* oki_decoder */
void decode_pcfx(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int mode);
void decode_oki16(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_oki4s(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
size_t oki_bytes_to_samples(size_t bytes, int channels);


/* ptadpcm_decoder */
void decode_ptadpcm(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, size_t frame_size);
size_t ptadpcm_bytes_to_samples(size_t bytes, int channels, size_t frame_size);


/* ubi_adpcm_decoder */
typedef struct ubi_adpcm_codec_data ubi_adpcm_codec_data;

ubi_adpcm_codec_data* init_ubi_adpcm(STREAMFILE* sf, uint32_t offset, uint32_t size, int channels);
void decode_ubi_adpcm(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do);
void reset_ubi_adpcm(ubi_adpcm_codec_data* data);
void seek_ubi_adpcm(ubi_adpcm_codec_data* data, int32_t num_sample);
void free_ubi_adpcm(ubi_adpcm_codec_data* data);
int32_t ubi_adpcm_get_samples(ubi_adpcm_codec_data* data);


/* imuse_decoder */
typedef struct imuse_codec_data imuse_codec_data;

imuse_codec_data* init_imuse(STREAMFILE* sf, int channels);
void decode_imuse(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do);
void reset_imuse(imuse_codec_data* data);
void seek_imuse(imuse_codec_data* data, int32_t num_sample);
void free_imuse(imuse_codec_data* data);


/* compresswave_decoder */
typedef struct compresswave_codec_data compresswave_codec_data;

compresswave_codec_data* init_compresswave(STREAMFILE* sf);
void decode_compresswave(compresswave_codec_data* data, sample_t* outbuf, int32_t samples_to_do);
void reset_compresswave(compresswave_codec_data* data);
void seek_compresswave(compresswave_codec_data* data, int32_t num_sample);
void free_compresswave(compresswave_codec_data* data);
STREAMFILE* compresswave_get_streamfile(compresswave_codec_data* data);


/* ea_mt_decoder*/
typedef struct ea_mt_codec_data ea_mt_codec_data;

ea_mt_codec_data* init_ea_mt(int channels, int type);
ea_mt_codec_data* init_ea_mt_loops(int channels, int pcm_blocks, int loop_sample, off_t* loop_offsets);
void decode_ea_mt(VGMSTREAM* vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel);
void reset_ea_mt(VGMSTREAM* vgmstream);
void flush_ea_mt(VGMSTREAM* vgmstream);
void seek_ea_mt(VGMSTREAM* vgmstream, int32_t num_sample);
void free_ea_mt(ea_mt_codec_data* data, int channels);


/* relic_decoder */
typedef struct relic_codec_data relic_codec_data;

relic_codec_data* init_relic(int channels, int bitrate, int codec_rate);
void decode_relic(VGMSTREAMCHANNEL* stream, relic_codec_data* data, sample_t* outbuf, int32_t samples_to_do);
void reset_relic(relic_codec_data* data);
void seek_relic(relic_codec_data* data, int32_t num_sample);
void free_relic(relic_codec_data* data);
int32_t relic_bytes_to_samples(size_t bytes, int channels, int bitrate);


/* hca_decoder */
typedef struct hca_codec_data hca_codec_data;

hca_codec_data* init_hca(STREAMFILE* sf);
void decode_hca(hca_codec_data* data, sample_t* outbuf, int32_t samples_to_do);
void reset_hca(hca_codec_data* data);
void loop_hca(hca_codec_data* data, int32_t num_sample);
void free_hca(hca_codec_data* data);
clHCA_stInfo* hca_get_info(hca_codec_data* data);

typedef struct {
    /* config + output */
    uint64_t key;
    uint16_t subkey;
    uint64_t best_key;
    int best_score;
    /* internals */
    uint32_t start_offset;
} hca_keytest_t;

void test_hca_key(hca_codec_data* data, hca_keytest_t* hk);
void hca_set_encryption_key(hca_codec_data* data, uint64_t keycode, uint64_t subkey);

STREAMFILE* hca_get_streamfile(hca_codec_data* data);


/* tac_decoder */
typedef struct tac_codec_data tac_codec_data;

tac_codec_data* init_tac(STREAMFILE* sf);
void decode_tac(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do);
void reset_tac(tac_codec_data* data);
void seek_tac(tac_codec_data* data, int32_t num_sample);
void free_tac(tac_codec_data* data);


/* ice_decoder */
typedef struct ice_codec_data ice_codec_data;

ice_codec_data* init_ice(STREAMFILE* sf, int subsong);
void decode_ice(ice_codec_data* data, sample_t* outbuf, int32_t samples_to_do);
void reset_ice(ice_codec_data* data);
void seek_ice(ice_codec_data* data, int32_t num_sample);
void free_ice(ice_codec_data* data);


#ifdef VGM_USE_VORBIS
/* ogg_vorbis_decoder */
typedef struct ogg_vorbis_codec_data ogg_vorbis_codec_data;
typedef struct { //todo simplify
    STREAMFILE *streamfile;
    int64_t start; /* file offset where the Ogg starts */
    int64_t offset; /* virtual offset, from 0 to size */
    int64_t size; /* virtual size of the Ogg */

    /* decryption setup */
    void (*decryption_callback)(void* ptr, size_t size, size_t nmemb, void* datasource);
    uint8_t scd_xor;
    off_t scd_xor_length;
    uint32_t xor_value;
} ogg_vorbis_io;

ogg_vorbis_codec_data* init_ogg_vorbis(STREAMFILE* sf, off_t start, off_t size, ogg_vorbis_io* io);
void decode_ogg_vorbis(ogg_vorbis_codec_data* data, sample_t* outbuf, int32_t samples_to_do, int channels);
void reset_ogg_vorbis(ogg_vorbis_codec_data* data);
void seek_ogg_vorbis(ogg_vorbis_codec_data* data, int32_t num_sample);
void free_ogg_vorbis(ogg_vorbis_codec_data* data);

int ogg_vorbis_get_comment(ogg_vorbis_codec_data* data, const char** comment);
void ogg_vorbis_get_info(ogg_vorbis_codec_data* data, int* p_channels, int* p_sample_rate);
void ogg_vorbis_get_samples(ogg_vorbis_codec_data* data, int* p_samples);
void ogg_vorbis_set_disable_reordering(ogg_vorbis_codec_data* data, int set);
void ogg_vorbis_set_force_seek(ogg_vorbis_codec_data* data, int set);
STREAMFILE* ogg_vorbis_get_streamfile(ogg_vorbis_codec_data* data);


/* vorbis_custom_decoder */
typedef struct vorbis_custom_codec_data vorbis_custom_codec_data;

typedef enum {
    VORBIS_FSB,         /* FMOD FSB: simplified/external setup packets, custom packet headers */
    VORBIS_WWISE,       /* Wwise WEM: many variations (custom setup, headers and data) */
    VORBIS_OGL,         /* Shin'en OGL: custom packet headers */
    VORBIS_SK,          /* Silicon Knights AUD: "OggS" replaced by "SK" */
    VORBIS_VID1,        /* Neversoft VID1: custom packet blocks/headers */
    VORBIS_AWC,         /* Rockstar AWC: custom packet blocks/headers */
} vorbis_custom_t;

/* config for Wwise Vorbis (3 types for flexibility though not all combinations exist) */
typedef enum { WWV_HEADER_TRIAD, WWV_FULL_SETUP, WWV_INLINE_CODEBOOKS, WWV_EXTERNAL_CODEBOOKS, WWV_AOTUV603_CODEBOOKS } wwise_setup_t;
typedef enum { WWV_TYPE_8, WWV_TYPE_6, WWV_TYPE_2 } wwise_header_t;
typedef enum { WWV_STANDARD, WWV_MODIFIED } wwise_packet_t;

typedef struct {
    /* to reconstruct init packets */
    int channels;
    int sample_rate;
    int blocksize_0_exp;
    int blocksize_1_exp;

    uint32_t setup_id; /* external setup */
    int big_endian; /* flag */
    uint32_t stream_end; /* optional, to avoid overreading into next subsong or chunk */

    /* Wwise Vorbis config */
    wwise_setup_t setup_type;
    wwise_header_t header_type;
    wwise_packet_t packet_type;

    /* AWC config */
    off_t header_offset;

    /* output (kinda ugly here but to simplify) */
    off_t data_start_offset;

} vorbis_custom_config;

vorbis_custom_codec_data* init_vorbis_custom(STREAMFILE* sf, off_t start_offset, vorbis_custom_t type, vorbis_custom_config* config);
void decode_vorbis_custom(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do, int channels);
void reset_vorbis_custom(VGMSTREAM* vgmstream);
void seek_vorbis_custom(VGMSTREAM* vgmstream, int32_t num_sample);
void free_vorbis_custom(vorbis_custom_codec_data* data);
#endif

typedef struct {
    int version;
    int layer;
    int bit_rate;
    int sample_rate;
    int frame_samples;
    int frame_size; /* bytes */
    int channels;
} mpeg_frame_info;

#ifdef VGM_USE_MPEG
/* mpeg_decoder */
typedef struct mpeg_codec_data mpeg_codec_data;

/* Custom MPEG modes, mostly differing in the data layout */
typedef enum {
    MPEG_STANDARD,          /* 1 stream */
    MPEG_AHX,               /* 1 stream with false frame headers */
    MPEG_XVAG,              /* N streams of fixed interleave (frame-aligned, several data-frames of fixed size) */
    MPEG_FSB,               /* N streams of 1 data-frame+padding (=interleave) */
    MPEG_P3D,               /* N streams of fixed interleave (not frame-aligned) */
    MPEG_SCD,               /* N streams of fixed interleave (not frame-aligned) */
    MPEG_EA,                /* 1 stream (maybe N streams in absolute offsets?) */
    MPEG_EAL31,             /* EALayer3 v1 (SCHl), custom frames with v1 header */
    MPEG_EAL31b,            /* EALayer3 v1 (SNS), custom frames with v1 header + minor changes */
    MPEG_EAL32P,            /* EALayer3 v2 "PCM", custom frames with v2 header + bigger PCM blocks? */
    MPEG_EAL32S,            /* EALayer3 v2 "Spike", custom frames with v2 header + smaller PCM blocks? */
    MPEG_LYN,               /* N streams of fixed interleave */
    MPEG_AWC,               /* N streams in block layout (music) or absolute offsets (sfx) */
    MPEG_EAMP3              /* custom frame header + MPEG frame + PCM blocks */
} mpeg_custom_t;

/* config for the above modes */
typedef struct {
    int channels; /* max channels */
    int fsb_padding; /* fsb padding mode */
    int chunk_size; /* size of a data portion */
    int max_chunks;
    int data_size; /* playable size */
    int interleave; /* size of stream interleave */
    int interleave_last;
    int encryption; /* encryption mode */
    int big_endian;
    int skip_samples;
    /* for AHX */
    int cri_type;
    uint16_t cri_key1;
    uint16_t cri_key2;
    uint16_t cri_key3;
} mpeg_custom_config;

mpeg_codec_data* init_mpeg(STREAMFILE* sf, off_t start_offset, coding_t *coding_type, int channels);
mpeg_codec_data* init_mpeg_custom(STREAMFILE* sf, off_t start_offset, coding_t* coding_type, int channels, mpeg_custom_t custom_type, mpeg_custom_config* config);
void decode_mpeg(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do, int channels);
void reset_mpeg(mpeg_codec_data* data);
void seek_mpeg(VGMSTREAM* vgmstream, int32_t num_sample);
void free_mpeg(mpeg_codec_data* data);

int mpeg_get_sample_rate(mpeg_codec_data* data);
long mpeg_bytes_to_samples(long bytes, const mpeg_codec_data* data);

uint32_t mpeg_get_tag_size(STREAMFILE* sf, uint32_t offset, uint32_t header);
int mpeg_get_frame_info(STREAMFILE* sf, off_t offset, mpeg_frame_info* info);
#endif


#ifdef VGM_USE_G7221
/* g7221_decoder */
typedef struct g7221_codec_data g7221_codec_data;

g7221_codec_data* init_g7221(int channel_count, int frame_size);
void decode_g7221(VGMSTREAM* vgmstream, sample_t* outbuf, int channelspacing, int32_t samples_to_do, int channel);
void reset_g7221(g7221_codec_data* data);
void free_g7221(g7221_codec_data* data);
void set_key_g7221(g7221_codec_data* data, const uint8_t* key);
int test_key_g7221(g7221_codec_data* data, off_t start, STREAMFILE* sf);
#endif


#ifdef VGM_USE_G719
/* g719_decoder */
typedef struct g719_codec_data g719_codec_data;

g719_codec_data* init_g719(int channel_count, int frame_size);
void decode_g719(VGMSTREAM* vgmstream, sample_t* outbuf, int channelspacing, int32_t samples_to_do, int channel);
void reset_g719(g719_codec_data* data, int channels);
void free_g719(g719_codec_data* data, int channels);
#endif


#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
/* mp4_aac_decoder */
void decode_mp4_aac(mp4_aac_codec_data* data, sample * outbuf, int32_t samples_to_do, int channels);
void reset_mp4_aac(VGMSTREAM* vgmstream);
void seek_mp4_aac(VGMSTREAM* vgmstream, int32_t num_sample);
void free_mp4_aac(mp4_aac_codec_data* data);
#endif


#ifdef VGM_USE_MAIATRAC3PLUS
/* at3plus_decoder */
typedef struct maiatrac3plus_codec_data maiatrac3plus_codec_data;

maiatrac3plus_codec_data* init_at3plus();
void decode_at3plus(VGMSTREAM* vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel);
void reset_at3plus(VGMSTREAM* vgmstream);
void seek_at3plus(VGMSTREAM* vgmstream, int32_t num_sample);
void free_at3plus(maiatrac3plus_codec_data* data);
#endif


#ifdef VGM_USE_ATRAC9
/* atrac9_decoder */
typedef struct {
    int channels;           /* to detect weird multichannel */
    uint32_t config_data;   /* ATRAC9 config header */
    int encoder_delay;      /* initial samples to discard */
} atrac9_config;
typedef struct atrac9_codec_data atrac9_codec_data;

atrac9_codec_data* init_atrac9(atrac9_config* cfg);
void decode_atrac9(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do, int channels);
void reset_atrac9(atrac9_codec_data* data);
void seek_atrac9(VGMSTREAM* vgmstream, int32_t num_sample);
void free_atrac9(atrac9_codec_data* data);
size_t atrac9_bytes_to_samples(size_t bytes, atrac9_codec_data* data);
size_t atrac9_bytes_to_samples_cfg(size_t bytes, uint32_t atrac9_config);
#endif


#ifdef VGM_USE_CELT
/* celt_fsb_decoder */
typedef enum { CELT_0_06_1,CELT_0_11_0} celt_lib_t;
typedef struct celt_codec_data celt_codec_data;

celt_codec_data* init_celt_fsb(int channels, celt_lib_t version);
void decode_celt_fsb(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do, int channels);
void reset_celt_fsb(celt_codec_data* data);
void seek_celt_fsb(VGMSTREAM* vgmstream, int32_t num_sample);
void free_celt_fsb(celt_codec_data* data);
#endif


#ifdef VGM_USE_SPEEX
/* speex_decoder */
typedef struct speex_codec_data speex_codec_data;

speex_codec_data* init_speex_ea(int channels);
void decode_speex(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do);
void reset_speex(speex_codec_data* data);
void seek_speex(VGMSTREAM* vgmstream, int32_t num_sample);
void free_speex(speex_codec_data* data);
#endif


#ifdef VGM_USE_FFMPEG
/* ffmpeg_decoder */
typedef struct ffmpeg_codec_data ffmpeg_codec_data;

ffmpeg_codec_data* init_ffmpeg_offset(STREAMFILE* sf, uint64_t start, uint64_t size);
ffmpeg_codec_data* init_ffmpeg_header_offset(STREAMFILE* sf, uint8_t* header, uint64_t header_size, uint64_t start, uint64_t size);
ffmpeg_codec_data* init_ffmpeg_header_offset_subsong(STREAMFILE* sf, uint8_t* header, uint64_t header_size, uint64_t start, uint64_t size, int target_subsong);

void decode_ffmpeg(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do, int channels);
void reset_ffmpeg(ffmpeg_codec_data* data);
void seek_ffmpeg(ffmpeg_codec_data* data, int32_t num_sample);
void free_ffmpeg(ffmpeg_codec_data* data);

void ffmpeg_set_skip_samples(ffmpeg_codec_data* data, int skip_samples);
uint32_t ffmpeg_get_channel_layout(ffmpeg_codec_data* data);
void ffmpeg_set_channel_remapping(ffmpeg_codec_data* data, int* channels_remap);
const char* ffmpeg_get_codec_name(ffmpeg_codec_data* data);
void ffmpeg_set_force_seek(ffmpeg_codec_data* data);
void ffmpeg_set_invert_floats(ffmpeg_codec_data* data);
const char* ffmpeg_get_metadata_value(ffmpeg_codec_data* data, const char* key);

int32_t ffmpeg_get_samples(ffmpeg_codec_data* data);
int ffmpeg_get_sample_rate(ffmpeg_codec_data* data);
int ffmpeg_get_channels(ffmpeg_codec_data* data);
int ffmpeg_get_subsong_count(ffmpeg_codec_data* data);

STREAMFILE* ffmpeg_get_streamfile(ffmpeg_codec_data* data);

/* ffmpeg_decoder_utils.c (helper-things) */
ffmpeg_codec_data* init_ffmpeg_atrac3_raw(STREAMFILE* sf, off_t offset, size_t data_size, int sample_count, int channels, int sample_rate, int block_align, int encoder_delay);
ffmpeg_codec_data* init_ffmpeg_atrac3_riff(STREAMFILE* sf, off_t offset, int* out_samples);
ffmpeg_codec_data* init_ffmpeg_aac(STREAMFILE* sf, off_t offset, size_t size, int skip_samples);
ffmpeg_codec_data* init_ffmpeg_xwma(STREAMFILE* sf, uint32_t data_offset, uint32_t data_size, int format, int channels, int sample_rate, int avg_bitrate, int block_size);

/* ffmpeg_decoder_custom_opus.c (helper-things) */
typedef struct {
    int channels;
    int skip;
    int sample_rate;
    /* multichannel-only */
    int coupled_count;
    int stream_count;
    int channel_mapping[8];
    /* frame table */
    off_t table_offset;
    int table_count;
    /* fixed frames */
    uint16_t frame_size;
} opus_config;

ffmpeg_codec_data* init_ffmpeg_switch_opus_config(STREAMFILE* sf, off_t start_offset, size_t data_size, opus_config* cfg);
ffmpeg_codec_data* init_ffmpeg_switch_opus(STREAMFILE* sf, off_t start_offset, size_t data_size, int channels, int skip, int sample_rate);
ffmpeg_codec_data* init_ffmpeg_ue4_opus(STREAMFILE* sf, off_t start_offset, size_t data_size, int channels, int skip, int sample_rate);
ffmpeg_codec_data* init_ffmpeg_ea_opus(STREAMFILE* sf, off_t start_offset, size_t data_size, int channels, int skip, int sample_rate);
ffmpeg_codec_data* init_ffmpeg_ea_opusm(STREAMFILE* sf, off_t data_offset, size_t data_size, opus_config* cfg);
ffmpeg_codec_data* init_ffmpeg_x_opus(STREAMFILE* sf, off_t table_offset, int table_count, off_t data_offset, size_t data_size, int channels, int skip);
ffmpeg_codec_data* init_ffmpeg_fsb_opus(STREAMFILE* sf, off_t start_offset, size_t data_size, int channels, int skip, int sample_rate);
ffmpeg_codec_data* init_ffmpeg_wwise_opus(STREAMFILE* sf, off_t data_offset, size_t data_size, opus_config* cfg);
ffmpeg_codec_data* init_ffmpeg_fixed_opus(STREAMFILE* sf, off_t data_offset, size_t data_size, opus_config* cfg);

size_t switch_opus_get_samples(off_t offset, size_t stream_size, STREAMFILE* sf);

size_t switch_opus_get_encoder_delay(off_t offset, STREAMFILE* sf);
size_t ue4_opus_get_encoder_delay(off_t offset, STREAMFILE* sf);
size_t ea_opus_get_encoder_delay(off_t offset, STREAMFILE* sf);
size_t fsb_opus_get_encoder_delay(off_t offset, STREAMFILE* sf);

/* ffmpeg_decoder_custom_mp4.c*/
typedef struct {
    int channels;
    int sample_rate;
    int32_t num_samples;

    uint32_t stream_offset;
    uint32_t stream_size;
    uint32_t table_offset;
    uint32_t table_entries;

    int encoder_delay;
    int end_padding;
    int frame_samples;
} mp4_custom_t;

ffmpeg_codec_data* init_ffmpeg_mp4_custom_std(STREAMFILE* sf, mp4_custom_t* mp4);
ffmpeg_codec_data* init_ffmpeg_mp4_custom_lyn(STREAMFILE* sf, mp4_custom_t* mp4);

#endif


/* coding_utils */
int ffmpeg_fmt_chunk_swap_endian(uint8_t* chunk, size_t chunk_size, uint16_t codec);
int ffmpeg_make_riff_atrac3plus(uint8_t* buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_align, int encoder_delay);
int ffmpeg_make_riff_xma1(uint8_t* buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int stream_mode);
int ffmpeg_make_riff_xma2(uint8_t* buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_count, int block_size);
int ffmpeg_make_riff_xma_from_fmt_chunk(uint8_t* buf, size_t buf_size, off_t fmt_offset, size_t fmt_size, size_t data_size, STREAMFILE* sf, int big_endian);
int ffmpeg_make_riff_xma2_from_xma2_chunk(uint8_t* buf, size_t buf_size, off_t xma2_offset, size_t xma2_size, size_t data_size, STREAMFILE* sf);
int ffmpeg_make_riff_xwma(uint8_t* buf, size_t buf_size, int codec, size_t data_size, int channels, int sample_rate, int avg_bps, int block_align);

/* MS audio format's sample info (struct to avoid passing so much stuff, separate for reusing) */
typedef struct {
    /* input */
    int xma_version;
    off_t data_offset;
    size_t data_size;

    int channels; /* for skips */
    off_t chunk_offset; /* for multistream config */

    /* frame offsets */
    int loop_flag;
    uint32_t loop_start_b;
    uint32_t loop_end_b;
    uint32_t loop_start_subframe;
    uint32_t loop_end_subframe;

    /* output */
    int32_t num_samples;
    int32_t loop_start_sample;
    int32_t loop_end_sample;
} ms_sample_data;
void xma_get_samples(ms_sample_data* msd, STREAMFILE* sf);
void wmapro_get_samples(ms_sample_data* msd, STREAMFILE* sf, int block_align, int sample_rate, uint32_t decode_flags);
void wma_get_samples(ms_sample_data* msd, STREAMFILE* sf, int block_align, int sample_rate, uint32_t decode_flags);
int32_t xwma_get_samples(STREAMFILE* sf, uint32_t data_offset, uint32_t data_size, int format, int channels, int sample_rate, int block_size);
int32_t xwma_dpds_get_samples(STREAMFILE* sf, uint32_t dpds_offset, uint32_t dpds_size, int channels, int be);

void xma1_parse_fmt_chunk(STREAMFILE* sf, off_t chunk_offset, int* channels, int* sample_rate, int* loop_flag, int32_t* loop_start_b, int32_t* loop_end_b, int32_t* loop_subframe, int be);
void xma2_parse_fmt_chunk_extra(STREAMFILE* sf, off_t chunk_offset, int* loop_flag, int32_t* out_num_samples, int32_t* out_loop_start_sample, int32_t* out_loop_end_sample, int be);
void xma2_parse_xma2_chunk(STREAMFILE* sf, off_t chunk_offset, int* channels, int* sample_rate, int* loop_flag, int32_t* num_samples, int32_t* loop_start_sample, int32_t* loop_end_sample);

void xma_fix_raw_samples(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t stream_offset, size_t stream_size, off_t chunk_offset, int fix_num_samples, int fix_loop_samples);
void xma_fix_raw_samples_hb(VGMSTREAM* vgmstream, STREAMFILE* sf_head, STREAMFILE* sf_body, off_t stream_offset, size_t stream_size, off_t chunk_offset, int fix_num_samples, int fix_loop_samples);
void xma_fix_raw_samples_ch(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t stream_offset, size_t stream_size, int channel_per_stream, int fix_num_samples, int fix_loop_samples);

size_t atrac3_bytes_to_samples(size_t bytes, int full_block_align);
size_t atrac3plus_bytes_to_samples(size_t bytes, int full_block_align);
size_t ac3_bytes_to_samples(size_t bytes, int full_block_align, int channels);
size_t aac_get_samples(STREAMFILE* sf, off_t start_offset, size_t bytes);
size_t mpeg_get_samples(STREAMFILE* sf, off_t start_offset, size_t bytes);
int32_t mpeg_get_samples_clean(STREAMFILE* sf, off_t start, size_t size, uint32_t* p_loop_start, uint32_t* p_loop_end, int is_vbr);
int mpc_get_samples(STREAMFILE* sf, off_t offset, int32_t* p_samples, int32_t* p_delay);


/* helper to pass a wrapped, clamped, fake extension-ed, SF to another meta */
STREAMFILE* setup_subfile_streamfile(STREAMFILE* sf, offv_t subfile_offset, size_t subfile_size, const char* extension);

#endif /*_CODING_H*/
