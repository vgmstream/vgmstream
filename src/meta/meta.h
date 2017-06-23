#ifndef _META_H
#define _META_H

#include "../vgmstream.h"

VGMSTREAM * init_vgmstream_3ds_idsp(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_adx(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_afc(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_agsc(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ast(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_brstm(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_Cstr(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_gcsw(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_halpst(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_nds_strm(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_adpdtk(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_dsp_std(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_mdsp_std(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_dsp_stm(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_mpdsp(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_dsp_std_int(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_dsp_csmp(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_ads(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_npsf(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_rs03(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_rsf(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_rwsd(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_cdxa(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_rxws(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_ps2_rxw(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_int(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_exst(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_svag(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_mib(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_mic(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_raw(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_vag(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_psx_gms(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_str(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_ild(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_pnb(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_xbox_wavm(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_xbox_xwav(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_str(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ea(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_caf(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_vpk(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_genh(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_amts(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_xbox_stma(STREAMFILE *streamFile);

#ifdef VGM_USE_VORBIS
VGMSTREAM * init_vgmstream_ogg_vorbis(STREAMFILE *streamFile);

typedef struct {
    int loop_flag;
    int32_t loop_start;
    int loop_length_found;
    int32_t loop_length;
    int loop_end_found;
    int32_t loop_end;
    meta_t meta_type;
    layout_t layout_type;

    /* XOR setup (SCD) */
    int decryption_enabled;
    void (*decryption_callback)(void *ptr, size_t size, size_t nmemb, void *datasource, int bytes_read);
    uint8_t scd_xor;
    off_t scd_xor_length;

} vgm_vorbis_info_t;

VGMSTREAM * init_vgmstream_ogg_vorbis_callbacks(STREAMFILE *streamFile, const char * filename, ov_callbacks *callbacks, off_t other_header_bytes, const vgm_vorbis_info_t *vgm_inf);

VGMSTREAM * init_vgmstream_sli_ogg(STREAMFILE * streamFile);
#endif

VGMSTREAM * init_vgmstream_hca(STREAMFILE *streamFile);

#ifdef VGM_USE_FFMPEG
VGMSTREAM * init_vgmstream_ffmpeg(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_ffmpeg_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size);

VGMSTREAM * init_vgmstream_mp4_aac_ffmpeg(STREAMFILE * streamFile);
#endif

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
VGMSTREAM * init_vgmstream_mp4_aac(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_mp4_aac_offset(STREAMFILE *streamFile, uint64_t start, uint64_t size);

VGMSTREAM * init_vgmstream_akb(STREAMFILE *streamFile);
#endif

VGMSTREAM * init_vgmstream_sfl(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_sadb(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_bmdx(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_wsi(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_aifc(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_str_snds(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ws_aud(STREAMFILE * streamFile);

#ifdef VGM_USE_MPEG
VGMSTREAM * init_vgmstream_ahx(STREAMFILE * streamFile);
#endif

VGMSTREAM * init_vgmstream_ivb(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_svs(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_riff(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_rifx(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_xnbm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_pos(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_nwa(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_eacs(STREAMFILE * streamFile);

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

VGMSTREAM * init_vgmstream_xa30(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_musc(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_musx_v004(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_musx_v005(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_musx_v006(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_musx_v010(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_musx_v201(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_leg(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_filp(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ikm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_sfs(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_dvi(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_bg00(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_kcey(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_rstm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_acm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_kces(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_dxh(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_psh(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_mus_acm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_pcm_scd(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_pcm_ps2(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_rkv(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_psw(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_vas(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_tec(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_enth(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_sdt(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_aix(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ngc_tydsp(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ngc_swd(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_capdsp(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_xbox_wvs(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_wvs(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_dc_str(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_dc_str_v2(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_xbox_matx(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_de2(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_vs(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_xbox_xmu(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_xbox_xvas(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_bh2pcm(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_sat_sap(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_dc_idvi(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_rnd(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_wii_idsp(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_kraw(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_omu(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_xa2(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ss_stream(STREAMFILE * streamFile);

//VGMSTREAM * init_vgmstream_idsp(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_idsp2(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_idsp3(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_idsp4(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ngc_ymf(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_sadl(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_ccc(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_psx_fag(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_mihb(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ngc_pdt(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_wii_mus(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_rsd2vag(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd2pcmb(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd2xadp(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd3pcm(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd3pcmb(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd3gadp(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd3vag(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd4pcmb(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd4pcm(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd4radp(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd4vag(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd6vag(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd6wadp(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd6xadp(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd6radp(STREAMFILE * streamFile);
VGMSTREAM * init_vgmstream_rsd6oogv(STREAMFILE* streamFile);
VGMSTREAM * init_vgmstream_rsd6xma(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_dc_asd(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_naomi_spsd(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_bgw(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_spw(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_ass(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_waa_wac_wad_wam(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_seg(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_nds_strm_ffta2(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_str_asr(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_zwdsp(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_gca(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_spt_spd(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ish_isd(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ydsp(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_gsp_gsb(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_msvp(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ngc_ssm(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ps2_joe(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_vgs(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_dc_dcsw_dcs(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_wii_smp(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_emff_ps2(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_emff_ngc(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_thp(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_wii_sts(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_p2bt(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_gbts(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_wii_sng(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_aax(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_utf_dsp(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_ffcc_str(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_sat_baka(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_nds_swav(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_vsf(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_nds_rrds(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_tk5(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_vsf_tta(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ads(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_wii_str(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_mcg(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_zsd(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps2_vgs(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_RedSpark(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ivaud(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_wii_wsd(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_wii_ndp(STREAMFILE *streamFile);

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

VGMSTREAM * init_vgmstream_ngc_gcub(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_maxis_xa(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_ngc_sck_dsp(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_apple_caff(STREAMFILE * streamFile);

VGMSTREAM * init_vgmstream_pc_mxst(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_sab(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_exakt_sc(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_wii_bns(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_wii_was(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_pona_3do(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_pona_psx(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_xbox_hlwav(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_stx(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_stm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_myspd(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_his(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_ast(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_dmsg(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ngc_dsp_aaap(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ngc_dsp_konami(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_ster(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_bnsf(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_wb(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_s14_sss(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_gcm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_smpl(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_msa(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_voi(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_khv(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_pc_smp(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ngc_bo2(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_dsp_ddsp(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_p3d(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_tk1(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_adsc(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ngc_dsp_mpds(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_dsp_str_ig(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_psx_mgav(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ngc_dsp_sth_str1(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ngc_dsp_sth_str2(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ngc_dsp_sth_str3(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_b1s(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_wad(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_dsp_xiii(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_dsp_cabelas(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_adm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_lpcm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_dsp_bdsp(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_vms(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_xau(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_gh3_bar(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ffw(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_dsp_dspw(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_jstm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps3_xvag(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps3_cps(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_sqex_scd(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ngc_nst_dsp(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_baf(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps3_msf(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ngc_dsp_iadp(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_nub_vag(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps3_past(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_sgxd(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ngca(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_wii_ras(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_spm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_x360_tra(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_iab(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_strlr(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_lsf_n1nj4n(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_vawx(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_pc_snds(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_wmus(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_hyperscan_kvag(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ios_psnd(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_pc_adp_bos(STREAMFILE* streamFile);
VGMSTREAM * init_vgmstream_pc_adp_otns(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_eb_sfx(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_eb_sf0(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps3_klbs(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_mtaf(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_tun(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_wpd(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_mn_str(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_mss(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_hsf(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps3_ivag(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_2pfs(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ubi_ckd(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_vbk(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_otm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_bcstm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_bfstm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_bfwav(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_kt_g1l(STREAMFILE* streamFile);
VGMSTREAM * init_vgmstream_kt_wiibgm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_mca(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_btsnd(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_ps2_svag_snk(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_xma(STREAMFILE* streamFile);

#ifdef VGM_USE_FFMPEG
VGMSTREAM * init_vgmstream_bik(STREAMFILE* streamFile);
#endif

VGMSTREAM * init_vgmstream_ps2_vds_vdm(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_x360_cxs(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_dsp_adx(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_akb_multi(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_akb2_multi(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_x360_ast(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_wwise(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ubi_raki(STREAMFILE* streamFile);

VGMSTREAM * init_vgmstream_x360_nub(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_x360_pasx(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_sxd(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ogl(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_mc3(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_gtd(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ta_aac_x360(STREAMFILE *streamFile);
VGMSTREAM * init_vgmstream_ta_aac_ps3(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ps3_mta2(STREAMFILE *streamFile);

VGMSTREAM * init_vgmstream_ngc_ulw(STREAMFILE * streamFile);

#endif /*_META_H*/
