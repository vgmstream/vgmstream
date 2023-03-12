#ifndef _META_H
#define _META_H

#include "../vgmstream.h"

typedef VGMSTREAM* (*init_vgmstream_t)(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_silence(int channels, int sample_rate, int32_t num_samples);
VGMSTREAM* init_vgmstream_silence_container(int total_subsongs);
VGMSTREAM* init_vgmstream_silence_base(VGMSTREAM* vgmstream);


VGMSTREAM* init_vgmstream_adx(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_adx_subkey(STREAMFILE* sf, uint16_t subkey);

VGMSTREAM * init_vgmstream_afc(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_agsc(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ast(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_brstm(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_cstr(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_gcsw(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_halpst(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_nds_strm(STREAMFILE *streamFile);

VGMSTREAM* init_vgmstream_dtk(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_ngc_dsp_std(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_ngc_mdsp_std(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_ngc_dsp_stm(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_ngc_mpdsp(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_ngc_dsp_std_int(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_idsp_namco(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_sadb(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_sadf(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_idsp_tt(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_idsp_nl(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_wii_wsd(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_ddsp(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_wii_was(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_str_ig(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_xiii(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_cabelas(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_ndp(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_ngc_dsp_aaap(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_dspw(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_ngc_dsp_iadp(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_mcadpcm(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_switch_audio(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_sps_n1(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_itl_ch(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_adpy(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_adpx(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_ds2(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_itl(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_sqex(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_wiivoice(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_wiiadpcm(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_cwac(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_idsp_tose(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_kwa(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_dsp_apex(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_csmp(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_rfrm(STREAMFILE *streamFile);

VGMSTREAM* init_vgmstream_ads(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_ads_container(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_nps(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_rs03(STREAMFILE *streamFile);

VGMSTREAM* init_vgmstream_rsf(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_rwsd(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_xa(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_rxws(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_raw_int(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_exst(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_svag_kcet(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps_headerless(STREAMFILE *streamFile);

VGMSTREAM* init_vgmstream_mib_mih(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_mic_koei(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_raw_pcm(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_vag(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_vag_aaap(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_seb(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ild(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_pnb(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_raw_wavm(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_str(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_caf(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_vpk(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_genh(STREAMFILE *streamFile);

VGMSTREAM* init_vgmstream_ogg_vorbis(STREAMFILE* sf);

typedef struct {
    int loop_flag;
    int32_t loop_start;
    int loop_length_found;
    int32_t loop_length;
    int loop_end_found;
    int32_t loop_end;
    meta_t meta_type;

    off_t stream_size;
    int total_subsongs;
    int disable_reordering;

    /* decryption setup */
    void (*decryption_callback)(void *ptr, size_t size, size_t nmemb, void *datasource);
    uint8_t scd_xor;
    off_t scd_xor_length;
    uint32_t xor_value;

    //ov_callbacks *callbacks

} ogg_vorbis_meta_info_t;

VGMSTREAM* init_vgmstream_ogg_vorbis_config(STREAMFILE* sf, off_t start, const ogg_vorbis_meta_info_t* ovmi);

VGMSTREAM* init_vgmstream_hca(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_hca_subkey(STREAMFILE* sf, uint16_t subkey);

#ifdef VGM_USE_FFMPEG
VGMSTREAM* init_vgmstream_ffmpeg(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_mp4_aac_ffmpeg(STREAMFILE* sf);
#endif

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
VGMSTREAM * init_vgmstream_mp4_aac(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_mp4_aac_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size);
#endif

VGMSTREAM * init_vgmstream_sli_ogg(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_sfl_ogg(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_bmdx(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_wsi(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_aifc(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_str_snds(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ws_aud(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ahx(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ivb(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_svs(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_riff(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rifx(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_xnb(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_pos(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_nwa(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ea_1snh(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ea_eacs(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_xss(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_sl3(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_hgc1(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_aus(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_rws(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_fsb(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_fsb4_wav(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_fsb5(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_rwx(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_xwb(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_xa30(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_musc(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_musx(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_leg(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_filp(STREAMFILE * streamFile);

VGMSTREAM* init_vgmstream_ikm(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_ster(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_sat_dvi(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_bg00(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_dc_kcey(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_rstm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_acm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_kces(STREAMFILE * streamFile);

VGMSTREAM* init_vgmstream_hxd(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_vsv(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_mus_acm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_scd_pcm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_pcm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_rkv(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_vas(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ps2_vas_container(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_enth(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_sdt(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_aix(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ngc_tydsp(STREAMFILE * streamFile);

VGMSTREAM* init_vgmstream_wvs_xbox(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_wvs_ngc(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_dc_str(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_dc_str_v2(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_xbox_matx(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_dec(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_vs(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_xmu(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_xvas(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_bh2pcm(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_sat_sap(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_dc_idvi(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_rnd(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_kraw(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_omu(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_xa2(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_idsp_ie(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ngc_ymf(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_sadl(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_fag(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_mihb(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ngc_pdt_split(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ngc_pdt(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_wii_mus(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_rsd(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_dc_asd(STREAMFILE * streamFile);

VGMSTREAM* init_vgmstream_spsd(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_bgw(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_spw(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_ass(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ubi_jade(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ubi_jade_container(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_seg(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_nds_strm_ffta2(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_knon(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_zwdsp(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_gca(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_spt_spd(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ish_isd(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ydsp(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_gsp_gsb(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ngc_ssm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_joe(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_vgs(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_dcs_wav(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_mul(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_thp(STREAMFILE *streamFile);

VGMSTREAM* init_vgmstream_sts(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_ps2_p2bt(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_gbts(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_wii_sng(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_aax(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_ffcc_str(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_sat_baka(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_swav(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_vsf(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_nds_rrds(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_tk5(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_ps2_tk1(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_vsf_tta(STREAMFILE *streamFile);

VGMSTREAM* init_vgmstream_ads_midway(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_ps2_mcg(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_zsd(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_vgs_ps(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_redspark(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ivaud(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_sps(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_xa2_rrp(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_nds_hwas(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_lps(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_snd(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_naomi_adpcm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_sd9(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_2dx9(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_dsp_ygo(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_vgv(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_gcub(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_maxis_xa(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ngc_sck_dsp(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_apple_caff(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_pc_mxst(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_sab(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_exakt_sc(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_wii_bns(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_pona_3do(STREAMFILE* streamFile);
VGMSTREAM * init_vgmstream_pona_psx(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_xbox_hlwav(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_myspd(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_his(STREAMFILE* streamFile);

VGMSTREAM* init_vgmstream_ast_mv(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_ast_mmv(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_dmsg(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ngc_dsp_konami(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_bnsf(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_wb(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_s14_sss(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_gcm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_smpl(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_msa(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_voi(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ngc_rkv(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_p3d(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ngc_dsp_mpds(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ea_swvr(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_b1s(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_wad(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_adm(STREAMFILE* streamFile);

VGMSTREAM* init_vgmstream_lpcm_shade(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_dsp_bdsp(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_vms(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_xau(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_bar(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ffw(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_jstm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_xvag(STREAMFILE* streamFile);

VGMSTREAM* init_vgmstream_cps(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_sqex_scd(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ngc_nst_dsp(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_baf(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_msf(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps3_past(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_sgxd(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_wii_ras(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_spm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_iab(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_vs_str(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_lsf_n1nj4n(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_xwav_new(STREAMFILE* sf);
VGMSTREAM * init_vgmstream_xwav_old(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_raw_snds(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_wmus(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_hyperscan_kvag(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ios_psnd(STREAMFILE* streamFile);

VGMSTREAM* init_vgmstream_adp_bos(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_adp_qd(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_eb_sfx(STREAMFILE* streamFile);
VGMSTREAM * init_vgmstream_eb_sf0(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_mtaf(STREAMFILE* streamFile);

VGMSTREAM* init_vgmstream_alp(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_wpd(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_mn_str(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_mss(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_hsf(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ivag(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_2pfs(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ubi_ckd(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_vbk(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_otm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_bcstm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_bfstm(STREAMFILE* streamFile);

VGMSTREAM* init_vgmstream_brwav(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_bfwav(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_bcwav(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_brwar(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_kt_g1l(STREAMFILE* streamFile);
VGMSTREAM * init_vgmstream_kt_wiibgm(STREAMFILE* streamFile);
VGMSTREAM * init_vgmstream_ktss(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_mca(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_btsnd(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_svag_snk(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_xma(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_bik(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_vds_vdm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_cxs(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_dsp_adx(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_akb(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_akb2(STREAMFILE *streamFile);

VGMSTREAM* init_vgmstream_astb(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_wwise(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_wwise_bnk(STREAMFILE* sf, int* p_prefetch);

VGMSTREAM * init_vgmstream_ubi_raki(STREAMFILE* streamFile);

VGMSTREAM* init_vgmstream_pasx(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_sxd(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ogl(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_mc3(STREAMFILE *streamFile);

VGMSTREAM* init_vgmstream_ghs(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_s_p_sth(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_aac_triace(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_va3(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_mta2(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_mta2_container(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_ulw(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_xa_xa30(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_xa_04sw(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_txth(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ea_schl(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_ea_schl_video(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_ea_bnk(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ea_abk(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ea_hdr_dat(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ea_hdr_dat_v2(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ea_map_mus(STREAMFILE * steeamFile);
VGMSTREAM * init_vgmstream_ea_mpf_mus(STREAMFILE * steeamFile);

VGMSTREAM * init_vgmstream_ea_schl_fixed(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_sk_aud(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_stm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_awc(STREAMFILE * streamFile);

VGMSTREAM* init_vgmstream_opus_std(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_opus_n1(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_opus_capcom(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_opus_nop(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_opus_shinen(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_opus_nus3(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_opus_sps_n1(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_opus_nxa(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_opus_opusx(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_opus_prototype(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_opus_opusnx(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_opus_nsopus(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_opus_sqex(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_opus_rsnd(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_raw_al(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_pc_ast(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_naac(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ubi_sb(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ubi_sm(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ubi_dat(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ubi_bnm(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ubi_bnm_ps2(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ubi_blk(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ezw(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_vxn(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ea_snu(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ea_snr_sns(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ea_sps(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ea_abk_eaac(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ea_hdr_sth_dat(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ea_mpf_mus_eaac(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ea_tmx(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ea_sbr(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ea_sbr_harmony(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ngc_vid1(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_flx(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_mogg(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_kma9(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_fsb_encrypted(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_xwc(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_atsl(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_sps_n1(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_sps_n1_segmented(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_atx(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_sqex_sead(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_waf(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_wave(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_wave_segmented(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_smv(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_nxap(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ea_wve_au00(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ea_wve_ad10(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_sthd(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ngc_dsp_std_le(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_pcm_sre(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ubi_lyn(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_ubi_lyn_container(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_msb_msh(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_txtp(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_smc_smh(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ppst(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ubi_bao_pk(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_ubi_bao_atomic(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_h4m(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_asf(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_xmd(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_cks(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_ckb(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_wv6(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_str_wav(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_wavebatch(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_hd3_bd3(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_bnk_sony(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_nus3bank(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_nus3bank_encrypted(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_sscf(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_a2m(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ahv(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_msv(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_sdf(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_svg(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_vis(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_vai(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_aif_asobo(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ao(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_apc(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_wv2(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_xau_konami(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_derf(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_utk(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_adpcm_capcom(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ue4opus(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_xwma(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_xopus(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_vs_square(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_msf_banpresto_wmsf(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_msf_banpresto_2msf(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_nwav(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_xpcm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_msf_tamasoft(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_xps_dat(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_xps(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_zsnd(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ogg_opus(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_nus3audio(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_imc(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_imc_container(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_smp(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_gin(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_dsf(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_208(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ffdl(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_mus_vc(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_strm_abylight(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_sfh(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_msf_konami(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_xwma_konami(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_9tav(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_fsb5_fev_bank(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_bwav(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_awb(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_awb_memory(STREAMFILE * streamFile, STREAMFILE *acbFile);

VGMSTREAM * init_vgmstream_acb(STREAMFILE * streamFile);
void load_acb_wave_name(STREAMFILE *acbFile, VGMSTREAM* vgmstream, int waveid, int port, int is_memory);

VGMSTREAM * init_vgmstream_rad(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_smk(STREAMFILE * streamFile);

VGMSTREAM* init_vgmstream_mzrt_v0(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_mzrt_v1(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_bsnf(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_xavs(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_psf_single(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_psf_segmented(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_sch(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ima(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_nub(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_nub_wav(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_nub_vag(STREAMFILE* streamFile);
VGMSTREAM * init_vgmstream_nub_at3(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_nub_xma(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_nub_dsp(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_nub_idsp(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_nub_is14(STREAMFILE * streamFile);

VGMSTREAM* init_vgmstream_xwv_valve(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_ubi_hx(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_bmp_konami(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_isb(STREAMFILE * streamFile);

VGMSTREAM* init_vgmstream_xssb(STREAMFILE *sf);

VGMSTREAM* init_vgmstream_xma_ue3(STREAMFILE *sf);

VGMSTREAM* init_vgmstream_csb(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_utf_dsp(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_utf_ahx(STREAMFILE* sf);

VGMSTREAM *init_vgmstream_fwse(STREAMFILE *streamFile);

VGMSTREAM* init_vgmstream_fda(STREAMFILE *sf);

VGMSTREAM * init_vgmstream_tgc(STREAMFILE *streamFile);

VGMSTREAM* init_vgmstream_kwb(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_xws(STREAMFILE* sf);

VGMSTREAM * init_vgmstream_lrmd(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_bkhd(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_bkhd_fx(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_encrypted(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_diva(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_imuse(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_ktsr(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_mups(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_kat(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_pcm_success(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_ktsc(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_adp_konami(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_zwv(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_dsb(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_bsf(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_sdrh_new(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_sdrh_old(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_wady(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_cpk(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_cpk_memory(STREAMFILE* sf, STREAMFILE* sf_acb);

VGMSTREAM *init_vgmstream_sbk(STREAMFILE *sf);

VGMSTREAM* init_vgmstream_ifs(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_acx(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_compresswave(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_ktac(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_mjb_mjh(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_tac(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_ogv_3rdeye(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_sspr(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_piff_tpcm(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_wxd_wxh(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_bnk_relic(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_xsh_xsd_xss(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_psb(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_lopu_fb(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_lpcm_fb(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_wbk(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_wbk_nslb(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_ubi_ckd_cwav(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_mpeg(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_sspf(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_s3v(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_esf(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_adm3(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_tt_ad(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_bw_mp3_riff(STREAMFILE* sf);
VGMSTREAM* init_vgmstream_bw_riff_mp3(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_sndz(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_vab(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_bigrp(STREAMFILE* sf);

VGMSTREAM* init_vgmstream_sscf_encrypted(STREAMFILE* sf);

#endif /*_META_H*/
