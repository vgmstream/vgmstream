#ifndef _CODING_H
#define _CODING_H

#include "../vgmstream.h"

/* adx_decoder */
void decode_adx(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int32_t frame_bytes);
void decode_adx_exp(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int32_t frame_bytes);
void decode_adx_fixed(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int32_t frame_bytes);
void decode_adx_enc(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int32_t frame_bytes);
void adx_next_key(VGMSTREAMCHANNEL * stream);

/* g721_decoder */
void decode_g721(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void g72x_init_state(struct g72x_state *state_ptr);

/* ima_decoder */
void decode_standard_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo, int is_high_first);
void decode_3ds_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_snds_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_otns_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_wv6_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_alp_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_ffta2_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_blitz_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

void decode_ms_ima(VGMSTREAM * vgmstream,VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel);
void decode_ref_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel);

void decode_xbox_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo);
void decode_xbox_ima_mch(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_nds_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_dat4_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_rad_ima(VGMSTREAM * vgmstream,VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel);
void decode_rad_ima_mono(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_apple_ima4(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_fsb_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_wwise_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_awc_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_ubi_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_h4m_ima(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, uint16_t frame_format);
size_t ima_bytes_to_samples(size_t bytes, int channels);
size_t ms_ima_bytes_to_samples(size_t bytes, int block_align, int channels);
size_t xbox_ima_bytes_to_samples(size_t bytes, int channels);
size_t dat4_ima_bytes_to_samples(size_t bytes, int channels);
size_t apple_ima4_bytes_to_samples(size_t bytes, int channels);

/* ngc_dsp_decoder */
void decode_ngc_dsp(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_ngc_dsp_subint(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int interleave);
size_t dsp_bytes_to_samples(size_t bytes, int channels);
int32_t dsp_nibbles_to_samples(int32_t nibbles);
void dsp_read_coefs_be(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing);
void dsp_read_coefs_le(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing);
void dsp_read_coefs(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing, int be);
void dsp_read_hist_be(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing);
void dsp_read_hist_le(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing);
void dsp_read_hist(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing, int be);

/* ngc_dtk_decoder */
void decode_ngc_dtk(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);

/* ngc_afc_decoder */
void decode_ngc_afc(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* pcm_decoder */
void decode_pcm16le(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm16be(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm16_int(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int big_endian);
void decode_pcm8(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm8_int(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm8_unsigned(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm8_unsigned_int(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm8_sb(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm4(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_pcm4_unsigned(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_ulaw(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_ulaw_int(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_alaw(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcmfloat(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int big_endian);
size_t pcm_bytes_to_samples(size_t bytes, int channels, int bits_per_sample);

/* psx_decoder */
void decode_psx(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int is_badflags);
void decode_psx_configurable(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int frame_size);
void decode_psx_pivotal(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int frame_size);
int ps_find_loop_offsets(STREAMFILE *streamFile, off_t start_offset, size_t data_size, int channels, size_t interleave, int32_t * out_loop_start, int32_t * out_loop_end);
int ps_find_loop_offsets_full(STREAMFILE *streamFile, off_t start_offset, size_t data_size, int channels, size_t interleave, int32_t * out_loop_start, int32_t * out_loop_end);
size_t ps_find_padding(STREAMFILE *streamFile, off_t start_offset, size_t data_size, int channels, size_t interleave, int discard_empty);
size_t ps_bytes_to_samples(size_t bytes, int channels);
size_t ps_cfg_bytes_to_samples(size_t bytes, size_t frame_size, int channels);
int ps_check_format(STREAMFILE *streamFile, off_t offset, size_t max);

/* psv_decoder */
void decode_hevag(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* xa_decoder */
void decode_xa(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
size_t xa_bytes_to_samples(size_t bytes, int channels, int is_blocked);

/* ea_xa_decoder */
void decode_ea_xa(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_ea_xa_int(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_ea_xa_v2(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel);
void decode_maxis_xa(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);

/* ea_xas_decoder */
void decode_ea_xas_v0(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_ea_xas_v1(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);

/* sdx2_decoder */
void decode_sdx2(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_sdx2_int(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_cbd2(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_cbd2_int(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* ws_decoder */
void decode_ws(VGMSTREAM * vgmstream, int channel, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* acm_decoder */
acm_codec_data *init_acm(STREAMFILE *streamFile, int force_channel_number);
void decode_acm(acm_codec_data *data, sample * outbuf, int32_t samples_to_do, int channelspacing);
void reset_acm(acm_codec_data *data);
void free_acm(acm_codec_data *data);

/* nwa_decoder */
void decode_nwa(NWAData *nwa, sample *outbuf, int32_t samples_to_do);

/* msadpcm_decoder */
void decode_msadpcm_stereo(VGMSTREAM * vgmstream, sample_t * outbuf, int32_t first_sample, int32_t samples_to_do);
void decode_msadpcm_mono(VGMSTREAM * vgmstream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_msadpcm_ck(VGMSTREAM * vgmstream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
long msadpcm_bytes_to_samples(long bytes, int block_size, int channels);

/* yamaha_decoder */
void decode_yamaha(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int is_stereo);
void decode_aska(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_nxap(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
size_t yamaha_bytes_to_samples(size_t bytes, int channels);
size_t aska_bytes_to_samples(size_t bytes, int channels);

/* nds_procyon_decoder */
void decode_nds_procyon(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* l5_555_decoder */
void decode_l5_555(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* sassc_decoder */
void decode_sassc(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* lsf_decode */
void decode_lsf(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* mtaf_decoder */
void decode_mtaf(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);

/* mta2_decoder */
void decode_mta2(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);

/* mc3_decoder */
void decode_mc3(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);

/* fadpcm_decoder */
void decode_fadpcm(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* asf_decoder */
void decode_asf(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* dsa_decoder */
void decode_dsa(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* xmd_decoder */
void decode_xmd(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, size_t frame_size);

/* derf_decoder */
void decode_derf(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* circus_decoder */
void decode_circus_adpcm(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* oki_decoder */
void decode_pcfx(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int mode);
void decode_oki16(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
size_t oki_bytes_to_samples(size_t bytes, int channels);

/* ptadpcm_decoder */
void decode_ptadpcm(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, size_t frame_size);
size_t ptadpcm_bytes_to_samples(size_t bytes, int channels, size_t frame_size);

/* ubi_adpcm_decoder */
ubi_adpcm_codec_data *init_ubi_adpcm(STREAMFILE *streamFile, off_t offset, int channels);
void decode_ubi_adpcm(VGMSTREAM * vgmstream, sample_t * outbuf, int32_t samples_to_do);
void reset_ubi_adpcm(ubi_adpcm_codec_data *data);
void seek_ubi_adpcm(ubi_adpcm_codec_data *data, int32_t num_sample);
void free_ubi_adpcm(ubi_adpcm_codec_data *data);

/* ea_mt_decoder*/
ea_mt_codec_data *init_ea_mt(int channels, int type);
ea_mt_codec_data *init_ea_mt_loops(int channels, int pcm_blocks, int loop_sample, off_t *loop_offsets);
void decode_ea_mt(VGMSTREAM * vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel);
void reset_ea_mt(VGMSTREAM * vgmstream);
void flush_ea_mt(VGMSTREAM *vgmstream);
void seek_ea_mt(VGMSTREAM * vgmstream, int32_t num_sample);
void free_ea_mt(ea_mt_codec_data *data, int channels);

/* hca_decoder */
hca_codec_data *init_hca(STREAMFILE *streamFile);
void decode_hca(hca_codec_data * data, sample * outbuf, int32_t samples_to_do);
void reset_hca(hca_codec_data * data);
void loop_hca(hca_codec_data * data, int32_t num_sample);
void free_hca(hca_codec_data * data);
int test_hca_key(hca_codec_data * data, unsigned long long keycode);

#ifdef VGM_USE_VORBIS
/* ogg_vorbis_decoder */
void decode_ogg_vorbis(ogg_vorbis_codec_data * data, sample_t * outbuf, int32_t samples_to_do, int channels);
void reset_ogg_vorbis(VGMSTREAM *vgmstream);
void seek_ogg_vorbis(VGMSTREAM *vgmstream, int32_t num_sample);
void free_ogg_vorbis(ogg_vorbis_codec_data *data);

/* vorbis_custom_decoder */
vorbis_custom_codec_data *init_vorbis_custom(STREAMFILE *streamfile, off_t start_offset, vorbis_custom_t type, vorbis_custom_config * config);
void decode_vorbis_custom(VGMSTREAM * vgmstream, sample_t * outbuf, int32_t samples_to_do, int channels);
void reset_vorbis_custom(VGMSTREAM *vgmstream);
void seek_vorbis_custom(VGMSTREAM *vgmstream, int32_t num_sample);
void free_vorbis_custom(vorbis_custom_codec_data *data);
#endif

#ifdef VGM_USE_MPEG
/* mpeg_decoder */
mpeg_codec_data *init_mpeg(STREAMFILE *streamfile, off_t start_offset, coding_t *coding_type, int channels);
mpeg_codec_data *init_mpeg_custom(STREAMFILE *streamFile, off_t start_offset, coding_t *coding_type, int channels, mpeg_custom_t custom_type, mpeg_custom_config *config);
void decode_mpeg(VGMSTREAM * vgmstream, sample_t * outbuf, int32_t samples_to_do, int channels);
void reset_mpeg(VGMSTREAM *vgmstream);
void seek_mpeg(VGMSTREAM *vgmstream, int32_t num_sample);
void free_mpeg(mpeg_codec_data *data);
void flush_mpeg(mpeg_codec_data * data);

long mpeg_bytes_to_samples(long bytes, const mpeg_codec_data *data);
#endif

#ifdef VGM_USE_G7221
/* g7221_decoder */
g7221_codec_data *init_g7221(int channel_count, int frame_size);
void decode_g7221(VGMSTREAM *vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel);
void reset_g7221(VGMSTREAM *vgmstream);
void free_g7221(VGMSTREAM *vgmstream);
#endif

#ifdef VGM_USE_G719
/* g719_decoder */
g719_codec_data *init_g719(int channel_count, int frame_size);
void decode_g719(VGMSTREAM *vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel);
void reset_g719(g719_codec_data * data, int channels);
void free_g719(g719_codec_data * data, int channels);
#endif

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
/* mp4_aac_decoder */
void decode_mp4_aac(mp4_aac_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels);
void reset_mp4_aac(VGMSTREAM *vgmstream);
void seek_mp4_aac(VGMSTREAM *vgmstream, int32_t num_sample);
void free_mp4_aac(mp4_aac_codec_data * data);
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
/* at3plus_decoder */
maiatrac3plus_codec_data *init_at3plus();
void decode_at3plus(VGMSTREAM *vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel);
void reset_at3plus(VGMSTREAM *vgmstream);
void seek_at3plus(VGMSTREAM *vgmstream, int32_t num_sample);
void free_at3plus(maiatrac3plus_codec_data *data);
#endif

#ifdef VGM_USE_ATRAC9
/* atrac9_decoder */
atrac9_codec_data *init_atrac9(atrac9_config *cfg);
void decode_atrac9(VGMSTREAM *vgmstream, sample_t * outbuf, int32_t samples_to_do, int channels);
void reset_atrac9(VGMSTREAM *vgmstream);
void seek_atrac9(VGMSTREAM *vgmstream, int32_t num_sample);
void free_atrac9(atrac9_codec_data *data);
size_t atrac9_bytes_to_samples(size_t bytes, atrac9_codec_data *data);
size_t atrac9_bytes_to_samples_cfg(size_t bytes, uint32_t atrac9_config);
#endif

#ifdef VGM_USE_CELT
/* celt_fsb_decoder */
celt_codec_data *init_celt_fsb(int channels, celt_lib_t version);
void decode_celt_fsb(VGMSTREAM *vgmstream, sample * outbuf, int32_t samples_to_do, int channels);
void reset_celt_fsb(VGMSTREAM *vgmstream);
void seek_celt_fsb(VGMSTREAM *vgmstream, int32_t num_sample);
void free_celt_fsb(celt_codec_data *data);
#endif

#ifdef VGM_USE_FFMPEG
/* ffmpeg_decoder */
ffmpeg_codec_data *init_ffmpeg_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size);
ffmpeg_codec_data *init_ffmpeg_header_offset(STREAMFILE *streamFile, uint8_t * header, uint64_t header_size, uint64_t start, uint64_t size);
ffmpeg_codec_data *init_ffmpeg_header_offset_subsong(STREAMFILE *streamFile, uint8_t * header, uint64_t header_size, uint64_t start, uint64_t size, int target_subsong);

void decode_ffmpeg(VGMSTREAM *stream, sample_t * outbuf, int32_t samples_to_do, int channels);
void reset_ffmpeg(VGMSTREAM *vgmstream);
void seek_ffmpeg(VGMSTREAM *vgmstream, int32_t num_sample);
void free_ffmpeg(ffmpeg_codec_data *data);

void ffmpeg_set_skip_samples(ffmpeg_codec_data * data, int skip_samples);
uint32_t ffmpeg_get_channel_layout(ffmpeg_codec_data * data);
void ffmpeg_set_channel_remapping(ffmpeg_codec_data * data, int *channels_remap);


/* ffmpeg_decoder_utils.c (helper-things) */
ffmpeg_codec_data * init_ffmpeg_atrac3_raw(STREAMFILE *sf, off_t offset, size_t data_size, int sample_count, int channels, int sample_rate, int block_align, int encoder_delay);
ffmpeg_codec_data * init_ffmpeg_atrac3_riff(STREAMFILE *sf, off_t offset, int* out_samples);


/* ffmpeg_decoder_custom_opus.c (helper-things) */
typedef struct {
    int channels;
    int skip;
    int sample_rate;
    /* multichannel-only */
    int coupled_count;
    int stream_count;
    int channel_mapping[8];
} opus_config;

ffmpeg_codec_data * init_ffmpeg_switch_opus_config(STREAMFILE *streamFile, off_t start_offset, size_t data_size, opus_config* cfg);
ffmpeg_codec_data * init_ffmpeg_switch_opus(STREAMFILE *streamFile, off_t start_offset, size_t data_size, int channels, int skip, int sample_rate);
ffmpeg_codec_data * init_ffmpeg_ue4_opus(STREAMFILE *streamFile, off_t start_offset, size_t data_size, int channels, int skip, int sample_rate);
ffmpeg_codec_data * init_ffmpeg_ea_opus(STREAMFILE *streamFile, off_t start_offset, size_t data_size, int channels, int skip, int sample_rate);
ffmpeg_codec_data * init_ffmpeg_x_opus(STREAMFILE *streamFile, off_t start_offset, size_t data_size, int channels, int skip, int sample_rate);

size_t switch_opus_get_samples(off_t offset, size_t stream_size, STREAMFILE *streamFile);

size_t switch_opus_get_encoder_delay(off_t offset, STREAMFILE *streamFile);
size_t ue4_opus_get_encoder_delay(off_t offset, STREAMFILE *streamFile);
size_t ea_opus_get_encoder_delay(off_t offset, STREAMFILE *streamFile);
#endif


/* coding_utils */
int ffmpeg_fmt_chunk_swap_endian(uint8_t * chunk, size_t chunk_size, uint16_t codec);
int ffmpeg_make_riff_atrac3plus(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_align, int encoder_delay);
int ffmpeg_make_riff_xma1(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int stream_mode);
int ffmpeg_make_riff_xma2(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_count, int block_size);
int ffmpeg_make_riff_xma_from_fmt_chunk(uint8_t * buf, size_t buf_size, off_t fmt_offset, size_t fmt_size, size_t data_size, STREAMFILE *streamFile, int big_endian);
int ffmpeg_make_riff_xma2_from_xma2_chunk(uint8_t * buf, size_t buf_size, off_t xma2_offset, size_t xma2_size, size_t data_size, STREAMFILE *streamFile);
int ffmpeg_make_riff_xwma(uint8_t * buf, size_t buf_size, int codec, size_t data_size, int channels, int sample_rate, int avg_bps, int block_align);

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
void xma_get_samples(ms_sample_data * msd, STREAMFILE *streamFile);
void wmapro_get_samples(ms_sample_data * msd, STREAMFILE *streamFile, int block_align, int sample_rate, uint32_t decode_flags);
void wma_get_samples(ms_sample_data * msd, STREAMFILE *streamFile, int block_align, int sample_rate, uint32_t decode_flags);

void xma1_parse_fmt_chunk(STREAMFILE *streamFile, off_t chunk_offset, int * channels, int * sample_rate, int * loop_flag, int32_t * loop_start_b, int32_t * loop_end_b, int32_t * loop_subframe, int be);
void xma2_parse_fmt_chunk_extra(STREAMFILE *streamFile, off_t chunk_offset, int * loop_flag, int32_t * out_num_samples, int32_t * out_loop_start_sample, int32_t * out_loop_end_sample, int be);
void xma2_parse_xma2_chunk(STREAMFILE *streamFile, off_t chunk_offset, int * channels, int * sample_rate, int * loop_flag, int32_t * num_samples, int32_t * loop_start_sample, int32_t * loop_end_sample);

void xma_fix_raw_samples(VGMSTREAM *vgmstream, STREAMFILE*streamFile, off_t stream_offset, size_t stream_size, off_t chunk_offset, int fix_num_samples, int fix_loop_samples);
void xma_fix_raw_samples_hb(VGMSTREAM *vgmstream, STREAMFILE *headerFile, STREAMFILE *bodyFile, off_t stream_offset, size_t stream_size, off_t chunk_offset, int fix_num_samples, int fix_loop_samples);
void xma_fix_raw_samples_ch(VGMSTREAM *vgmstream, STREAMFILE*streamFile, off_t stream_offset, size_t stream_size, int channel_per_stream, int fix_num_samples, int fix_loop_samples);

size_t atrac3_bytes_to_samples(size_t bytes, int full_block_align);
size_t atrac3plus_bytes_to_samples(size_t bytes, int full_block_align);
size_t ac3_bytes_to_samples(size_t bytes, int full_block_align, int channels);
size_t aac_get_samples(STREAMFILE *streamFile, off_t start_offset, size_t bytes);
size_t mpeg_get_samples(STREAMFILE *streamFile, off_t start_offset, size_t bytes);


/* An internal struct to pass around and simulate a bitstream. */
typedef enum { BITSTREAM_MSF, BITSTREAM_VORBIS } vgm_bitstream_t;
typedef struct {
    uint8_t * buf;          /* buffer to read/write*/
    size_t bufsize;         /* max size of the buffer */
    off_t b_off;            /* current offset in bits inside the buffer */
    off_t info_offset;      /* for logging */
    vgm_bitstream_t mode;   /* read/write modes */
} vgm_bitstream;

int r_bits(vgm_bitstream * ib, int num_bits, uint32_t * value);
int w_bits(vgm_bitstream * ob, int num_bits, uint32_t value);


/* helper to pass a wrapped, clamped, fake extension-ed, SF to another meta */
STREAMFILE* setup_subfile_streamfile(STREAMFILE *streamFile, off_t subfile_offset, size_t subfile_size, const char* extension);

#endif /*_CODING_H*/
