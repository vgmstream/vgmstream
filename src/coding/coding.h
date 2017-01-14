#ifndef _CODING_H
#define _CODING_H

#include "../vgmstream.h"

/* adx_decoder */
void decode_adx(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_adx_enc(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void adx_next_key(VGMSTREAMCHANNEL * stream);

/* g721_decoder */
void decode_g721(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void g72x_init_state(struct g72x_state *state_ptr);

/* ima_decoder */
void decode_nds_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_dat4_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_xbox_ima(VGMSTREAM * vgmstream,VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel);
void decode_int_xbox_ima(VGMSTREAM * vgmstream,VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel);
void decode_dvi_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_eacs_ima(VGMSTREAM * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_snds_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_ima(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_rad_ima(VGMSTREAM * vgmstream,VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel);
void decode_rad_ima_mono(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_apple_ima4(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_ms_ima(VGMSTREAM * vgmstream,VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel);

/* ngc_dsp_decoder */
void decode_ngc_dsp(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_ngc_dsp_mem(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, uint8_t * mem);
int32_t dsp_nibbles_to_samples(int32_t nibbles);
void dsp_read_coefs_be(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing);

/* ngc_dtk_decoder */
void decode_ngc_dtk(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);

/* ngc_afc_decoder */
void decode_ngc_afc(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* pcm_decoder */
void decode_pcm16LE(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm16LE_int(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm16LE_XOR_int(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm16BE(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm8(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm8_int(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm8_sb_int(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm8_unsigned_int(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_pcm8_unsigned(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* psx_decoder */
void decode_psx(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_psx_badflags(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_psx_bmdx(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_psx_configurable(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int frame_size);
void decode_hevag(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* xa_decoder */
void decode_xa(VGMSTREAM * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void init_get_high_nibble(VGMSTREAM * vgmstream);

/*eaxa_decoder */
void decode_ea_xa(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do,int channel);
void decode_ea_adpcm(VGMSTREAM * vgmstream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_maxis_adpcm(VGMSTREAM * vgmstream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);

/* sdx2_decoder */
void decode_sdx2(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_sdx2_int(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
/* sdx2_decoder */
void decode_cbd2(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_cbd2_int(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* ws_decoder */
void decode_ws(VGMSTREAM * vgmstream, int channel, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* acm_decoder */
void decode_acm(ACMStream * acm, sample * outbuf, int32_t samples_to_do, int channelspacing);

/* nwa_decoder */
void decode_nwa(NWAData *nwa, sample *outbuf, int32_t samples_to_do);

/* msadpcm_decoder */
long msadpcm_bytes_to_samples(long bytes, int block_size, int channels);
void decode_msadpcm_stereo(VGMSTREAM * vgmstream, sample * outbuf, int32_t first_sample, int32_t samples_to_do);
void decode_msadpcm_mono(VGMSTREAM * vgmstream, sample * outbuf, int32_t first_sample, int32_t samples_to_do);

/* aica_decoder */
void decode_aica(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* nds_procyon_decoder */
void decode_nds_procyon(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* l5_555_decoder */
void decode_l5_555(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* SASSC_decoder */
void decode_SASSC(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* lsf_decode */
void decode_lsf(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* mtaf_decoder */
void decode_mtaf(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel, int channels);

/* hca_decoder */
void decode_hca(hca_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels);

/* ogg_vorbis_decoder */
#ifdef VGM_USE_VORBIS
void decode_ogg_vorbis(ogg_vorbis_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels);
#endif

/* mpeg_decoder */
#ifdef VGM_USE_MPEG
mpeg_codec_data *init_mpeg_codec_data(STREAMFILE *streamfile, off_t start_offset, long given_sample_rate, int given_channels, coding_t *coding_type, int * actual_sample_rate, int * actual_channels);
void decode_fake_mpeg2_l2(VGMSTREAMCHANNEL * stream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do);
void decode_mpeg(VGMSTREAMCHANNEL * stream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels);
long mpeg_bytes_to_samples(long bytes, const struct mpg123_frameinfo *mi);
void mpeg_set_error_logging(mpeg_codec_data * data, int enable);
#endif

/* g7221_decoder */
#ifdef VGM_USE_G7221
void decode_g7221(VGMSTREAM *vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel);
#endif

/* g719_decoder */
#ifdef VGM_USE_G719
void decode_g719(VGMSTREAM *vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel);
#endif

/* mp4_aac_decoder */
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
void decode_mp4_aac(mp4_aac_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels);
#endif

/* at3_decoder */
#ifdef VGM_USE_MAIATRAC3PLUS
void decode_at3plus(VGMSTREAM *vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel);
#endif

/* ffmpeg_decoder */
#ifdef VGM_USE_FFMPEG
void decode_ffmpeg(VGMSTREAM *stream, sample * outbuf, int32_t samples_to_do, int channels);
void reset_ffmpeg(VGMSTREAM *vgmstream);
void seek_ffmpeg(VGMSTREAM *vgmstream, int32_t num_sample);

int ffmpeg_make_riff_atrac3(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_align, int joint_stereo, int encoder_delay);
int ffmpeg_make_riff_xma2(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_count, int block_size);
int ffmpeg_make_riff_xma2_from_fmt(uint8_t * buf, size_t buf_size, off_t fmt_offset, size_t fmt_size, size_t data_size, STREAMFILE *streamFile, int big_endian);

#endif

#endif /*_CODING_H*/
