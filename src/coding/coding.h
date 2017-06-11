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
void decode_otns_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_fsb_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
void decode_wwise_ima(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);
size_t ms_ima_bytes_to_samples(size_t bytes, int block_align, int channels);

/* ngc_dsp_decoder */
void decode_ngc_dsp(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_ngc_dsp_mem(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, uint8_t * mem);
size_t dsp_bytes_to_samples(size_t bytes, int channels);
int32_t dsp_nibbles_to_samples(int32_t nibbles);
void dsp_read_coefs_be(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing);
void dsp_read_coefs_le(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing);
void dsp_read_coefs(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing, int be);

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
void decode_ulaw(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
size_t pcm_bytes_to_samples(size_t bytes, int channels, int bits_per_sample);

/* psx_decoder */
void decode_psx(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_psx_badflags(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_psx_bmdx(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_psx_configurable(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int frame_size);
void decode_hevag(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
size_t ps_bytes_to_samples(size_t bytes, int channels);

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
void decode_cbd2(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);
void decode_cbd2_int(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* ws_decoder */
void decode_ws(VGMSTREAM * vgmstream, int channel, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do);

/* acm_decoder */
void decode_acm(ACMStream * acm, sample * outbuf, int32_t samples_to_do, int channelspacing);

/* nwa_decoder */
void decode_nwa(NWAData *nwa, sample *outbuf, int32_t samples_to_do);

/* msadpcm_decoder */
void decode_msadpcm_stereo(VGMSTREAM * vgmstream, sample * outbuf, int32_t first_sample, int32_t samples_to_do);
void decode_msadpcm_mono(VGMSTREAM * vgmstream, sample * outbuf, int32_t first_sample, int32_t samples_to_do);
long msadpcm_bytes_to_samples(long bytes, int block_size, int channels);

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

/* mta2_decoder */
void decode_mta2(VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);

/* mc3_decoder */
void decode_mc3(VGMSTREAM * vgmstream, VGMSTREAMCHANNEL * stream, sample * outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do, int channel);

/* hca_decoder */
void decode_hca(hca_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels);
void reset_hca(VGMSTREAM *vgmstream);
void loop_hca(VGMSTREAM *vgmstream);
void free_hca(hca_codec_data * data);

#ifdef VGM_USE_VORBIS
/* ogg_vorbis_decoder */
void decode_ogg_vorbis(ogg_vorbis_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels);
void reset_ogg_vorbis(VGMSTREAM *vgmstream);
void seek_ogg_vorbis(VGMSTREAM *vgmstream, int32_t num_sample);
void free_ogg_vorbis(ogg_vorbis_codec_data *data);

/* fsb_vorbis_decoder */
vorbis_codec_data * init_fsb_vorbis_codec_data(STREAMFILE *streamfile, off_t start_offset, int channels, int sample_rate, uint32_t setup_id);
void decode_fsb_vorbis(VGMSTREAM * vgmstream, sample * outbuf, int32_t samples_to_do, int channels);
void reset_fsb_vorbis(VGMSTREAM *vgmstream);
void seek_fsb_vorbis(VGMSTREAM *vgmstream, int32_t num_sample);
void free_fsb_vorbis(vorbis_codec_data *data);

/* wwise_vorbis_decoder */
vorbis_codec_data * init_wwise_vorbis_codec_data(STREAMFILE *streamfile, off_t start_offset, int channels, int sample_rate, int blocksize_0_exp, int blocksize_1_exp,
        wwise_setup_type setup_type, wwise_header_type header_type, wwise_packet_type packet_type, int big_endian);
void decode_wwise_vorbis(VGMSTREAM * vgmstream, sample * outbuf, int32_t samples_to_do, int channels);
void reset_wwise_vorbis(VGMSTREAM *vgmstream);
void seek_wwise_vorbis(VGMSTREAM *vgmstream, int32_t num_sample);
void free_wwise_vorbis(vorbis_codec_data *data);

/* ogl_vorbis_decoder */
vorbis_codec_data * init_ogl_vorbis_codec_data(STREAMFILE *streamFile, off_t start_offset, off_t * data_start_offset);
void decode_ogl_vorbis(VGMSTREAM * vgmstream, sample * outbuf, int32_t samples_to_do, int channels);
void reset_ogl_vorbis(VGMSTREAM *vgmstream);
void seek_ogl_vorbis(VGMSTREAM *vgmstream, int32_t num_sample);
void free_ogl_vorbis(vorbis_codec_data *data);
#endif

#ifdef VGM_USE_MPEG
/* mpeg_decoder */
mpeg_codec_data *init_mpeg_codec_data(STREAMFILE *streamfile, off_t start_offset, coding_t *coding_type, int channels);
mpeg_codec_data *init_mpeg_codec_data_interleaved(STREAMFILE *streamfile, off_t start_offset, coding_t *coding_type, int channels, int fixed_frame_size, int fsb_padding);
mpeg_codec_data *init_mpeg_codec_data_ahx(STREAMFILE *streamFile, off_t start_offset, int channel_count);

void decode_mpeg(VGMSTREAM * vgmstream, sample * outbuf, int32_t samples_to_do, int channels);
void decode_fake_mpeg2_l2(VGMSTREAMCHANNEL * stream, mpeg_codec_data * data, sample * outbuf, int32_t samples_to_do);
void reset_mpeg(VGMSTREAM *vgmstream);
void seek_mpeg(VGMSTREAM *vgmstream, int32_t num_sample);
void free_mpeg(mpeg_codec_data *data);

long mpeg_bytes_to_samples(long bytes, const mpeg_codec_data *data);
void mpeg_set_error_logging(mpeg_codec_data * data, int enable);
#endif

#ifdef VGM_USE_G7221
/* g7221_decoder */
void decode_g7221(VGMSTREAM *vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel);
void reset_g7221(VGMSTREAM *vgmstream);
void free_g7221(VGMSTREAM *vgmstream);
#endif

#ifdef VGM_USE_G719
/* g719_decoder */
void decode_g719(VGMSTREAM *vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel);
void reset_g719(VGMSTREAM *vgmstream);
void free_g719(VGMSTREAM *vgmstream);
#endif

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
/* mp4_aac_decoder */
void decode_mp4_aac(mp4_aac_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels);
void reset_mp4_aac(VGMSTREAM *vgmstream);
void seek_mp4_aac(VGMSTREAM *vgmstream, int32_t num_sample);
void free_mp4_aac(mp4_aac_codec_data * data);
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
/* at3_decoder */
void decode_at3plus(VGMSTREAM *vgmstream, sample * outbuf, int channelspacing, int32_t samples_to_do, int channel);
void reset_at3plus(VGMSTREAM *vgmstream);
void seek_at3plus(VGMSTREAM *vgmstream, int32_t num_sample);
void free_at3plus(maiatrac3plus_codec_data *data);
#endif

#ifdef VGM_USE_FFMPEG
/* ffmpeg_decoder */
ffmpeg_codec_data * init_ffmpeg_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size);
ffmpeg_codec_data * init_ffmpeg_header_offset(STREAMFILE *streamFile, uint8_t * header, uint64_t header_size, uint64_t start, uint64_t size);

void decode_ffmpeg(VGMSTREAM *stream, sample * outbuf, int32_t samples_to_do, int channels);
void reset_ffmpeg(VGMSTREAM *vgmstream);
void seek_ffmpeg(VGMSTREAM *vgmstream, int32_t num_sample);
void free_ffmpeg(ffmpeg_codec_data *data);

void ffmpeg_set_skip_samples(ffmpeg_codec_data * data, int skip_samples);
#endif

/* coding_utils */
int ffmpeg_fmt_chunk_swap_endian(uint8_t * chunk, size_t chunk_size, uint16_t codec);
int ffmpeg_make_riff_atrac3(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_align, int joint_stereo, int encoder_delay);
int ffmpeg_make_riff_atrac3plus(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_align, int encoder_delay);
int ffmpeg_make_riff_xma1(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int stream_mode);
int ffmpeg_make_riff_xma2(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_count, int block_size);
int ffmpeg_make_riff_xma_from_fmt_chunk(uint8_t * buf, size_t buf_size, off_t fmt_offset, size_t fmt_size, size_t data_size, STREAMFILE *streamFile, int big_endian);
int ffmpeg_make_riff_xma2_from_xma2_chunk(uint8_t * buf, size_t buf_size, off_t xma2_offset, size_t xma2_size, size_t data_size, STREAMFILE *streamFile);
int ffmpeg_make_riff_xwma(uint8_t * buf, size_t buf_size, int codec, size_t data_size, int channels, int sample_rate, int avg_bps, int block_align);

/* MS audio format's sample info (struct to avoid passing so much stuff, separate for reusing) */
typedef struct {
    int xma_version;
    int channels;
    int stream_mode;
    off_t data_offset;
    size_t data_size;

    /* frame offsets */
    int loop_flag;
    uint32_t loop_start_b;
    uint32_t loop_end_b;
    uint32_t loop_start_subframe;
    uint32_t loop_end_subframe;

    /* output */
    int32_t num_samples;
    int32_t skip_samples;
    int32_t loop_start_sample;
    int32_t loop_end_sample;
} ms_sample_data;
void xma_get_samples(ms_sample_data * msd, STREAMFILE *streamFile);
void wmapro_get_samples(ms_sample_data * msd, STREAMFILE *streamFile, int block_align, int sample_rate, uint32_t decode_flags);
void wma_get_samples(ms_sample_data * msd, STREAMFILE *streamFile, int block_align, int sample_rate, uint32_t decode_flags);

void xma1_parse_fmt_chunk(STREAMFILE *streamFile, off_t chunk_offset, int * channels, int * sample_rate, int * loop_flag, int32_t * loop_start_b, int32_t * loop_end_b, int32_t * loop_subframe, int be);
void xma2_parse_fmt_chunk_extra(STREAMFILE *streamFile, off_t chunk_offset, int * loop_flag, int32_t * num_samples, int32_t * loop_start_sample, int32_t * loop_end_sample, int be);
void xma2_parse_xma2_chunk(STREAMFILE *streamFile, off_t chunk_offset, int * channels, int * sample_rate, int * loop_flag, int32_t * num_samples, int32_t * loop_start_sample, int32_t * loop_end_sample);

size_t atrac3_bytes_to_samples(size_t bytes, int full_block_align);
size_t atrac3plus_bytes_to_samples(size_t bytes, int full_block_align);


#endif /*_CODING_H*/
