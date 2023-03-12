#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "vgmstream.h"
#include "meta/meta.h"
#include "layout/layout.h"
#include "coding/coding.h"
#include "decode.h"
#include "render.h"
#include "mixing.h"

static void try_dual_file_stereo(VGMSTREAM* opened_vgmstream, STREAMFILE* sf, VGMSTREAM* (*init_vgmstream_function)(STREAMFILE*));


/* list of metadata parser functions that will recognize files, used on init */
VGMSTREAM* (*init_vgmstream_functions[])(STREAMFILE* sf) = {
    init_vgmstream_adx,
    init_vgmstream_brstm,
    init_vgmstream_brwav,
    init_vgmstream_bfwav,
    init_vgmstream_bcwav,
    init_vgmstream_brwar,
    init_vgmstream_nds_strm,
    init_vgmstream_afc,
    init_vgmstream_ast,
    init_vgmstream_halpst,
    init_vgmstream_rs03,
    init_vgmstream_ngc_dsp_std,
    init_vgmstream_ngc_dsp_std_le,
    init_vgmstream_ngc_mdsp_std,
    init_vgmstream_csmp,
    init_vgmstream_rfrm,
    init_vgmstream_cstr,
    init_vgmstream_gcsw,
    init_vgmstream_ads,
    init_vgmstream_nps,
    init_vgmstream_xa,
    init_vgmstream_rxws,
    init_vgmstream_ngc_dsp_stm,
    init_vgmstream_exst,
    init_vgmstream_svag_kcet,
    init_vgmstream_ngc_mpdsp,
    init_vgmstream_ngc_dsp_std_int,
    init_vgmstream_vag,
    init_vgmstream_vag_aaap,
    init_vgmstream_ild,
    init_vgmstream_ngc_str,
    init_vgmstream_ea_schl,
    init_vgmstream_caf,
    init_vgmstream_vpk,
    init_vgmstream_genh,
    init_vgmstream_ogg_vorbis,
    init_vgmstream_sfl_ogg,
    init_vgmstream_sadb,
    init_vgmstream_ps2_bmdx,
    init_vgmstream_wsi,
    init_vgmstream_aifc,
    init_vgmstream_str_snds,
    init_vgmstream_ws_aud,
    init_vgmstream_ahx,
    init_vgmstream_ivb,
    init_vgmstream_svs,
    init_vgmstream_riff,
    init_vgmstream_rifx,
    init_vgmstream_pos,
    init_vgmstream_nwa,
    init_vgmstream_ea_1snh,
    init_vgmstream_ea_eacs,
    init_vgmstream_xss,
    init_vgmstream_sl3,
    init_vgmstream_hgc1,
    init_vgmstream_aus,
    init_vgmstream_rws,
    init_vgmstream_fsb,
    init_vgmstream_fsb4_wav,
    init_vgmstream_fsb5,
    init_vgmstream_rwx,
    init_vgmstream_xwb,
    init_vgmstream_ps2_xa30,
    init_vgmstream_musc,
    init_vgmstream_musx,
    init_vgmstream_leg,
    init_vgmstream_filp,
    init_vgmstream_ikm,
    init_vgmstream_ster,
    init_vgmstream_bg00,
    init_vgmstream_sat_dvi,
    init_vgmstream_dc_kcey,
    init_vgmstream_ps2_rstm,
    init_vgmstream_acm,
    init_vgmstream_mus_acm,
    init_vgmstream_ps2_kces,
    init_vgmstream_hxd,
    init_vgmstream_vsv,
    init_vgmstream_ps2_pcm,
    init_vgmstream_ps2_rkv,
    init_vgmstream_ps2_vas,
    init_vgmstream_ps2_vas_container,
    init_vgmstream_ps2_enth,
    init_vgmstream_sdt,
    init_vgmstream_aix,
    init_vgmstream_ngc_tydsp,
    init_vgmstream_wvs_xbox,
    init_vgmstream_wvs_ngc,
    init_vgmstream_dc_str,
    init_vgmstream_dc_str_v2,
    init_vgmstream_xbox_matx,
    init_vgmstream_dec,
    init_vgmstream_vs,
    init_vgmstream_dc_str,
    init_vgmstream_dc_str_v2,
    init_vgmstream_xmu,
    init_vgmstream_xvas,
    init_vgmstream_ngc_bh2pcm,
    init_vgmstream_sat_sap,
    init_vgmstream_dc_idvi,
    init_vgmstream_ps2_rnd,
    init_vgmstream_idsp_tt,
    init_vgmstream_kraw,
    init_vgmstream_ps2_omu,
    init_vgmstream_ps2_xa2,
    init_vgmstream_idsp_nl,
    init_vgmstream_idsp_ie,
    init_vgmstream_ngc_ymf,
    init_vgmstream_sadl,
    init_vgmstream_fag,
    init_vgmstream_ps2_mihb,
    init_vgmstream_ngc_pdt_split,
    init_vgmstream_ngc_pdt,
    init_vgmstream_wii_mus,
    init_vgmstream_dc_asd,
    init_vgmstream_spsd,
    init_vgmstream_rsd,
    init_vgmstream_bgw,
    init_vgmstream_spw,
    init_vgmstream_ps2_ass,
    init_vgmstream_ubi_jade,
    init_vgmstream_ubi_jade_container,
    init_vgmstream_seg,
    init_vgmstream_nds_strm_ffta2,
    init_vgmstream_knon,
    init_vgmstream_gca,
    init_vgmstream_spt_spd,
    init_vgmstream_ish_isd,
    init_vgmstream_gsp_gsb,
    init_vgmstream_ydsp,
    init_vgmstream_ngc_ssm,
    init_vgmstream_ps2_joe,
    init_vgmstream_vgs,
    init_vgmstream_dcs_wav,
    init_vgmstream_mul,
    init_vgmstream_thp,
    init_vgmstream_sts,
    init_vgmstream_ps2_p2bt,
    init_vgmstream_ps2_gbts,
    init_vgmstream_wii_sng,
    init_vgmstream_ngc_dsp_iadp,
    init_vgmstream_aax,
    init_vgmstream_utf_dsp,
    init_vgmstream_ngc_ffcc_str,
    init_vgmstream_sat_baka,
    init_vgmstream_swav,
    init_vgmstream_vsf,
    init_vgmstream_nds_rrds,
    init_vgmstream_ps2_tk5,
    init_vgmstream_ps2_vsf_tta,
    init_vgmstream_ads_midway,
    init_vgmstream_ps2_mcg,
    init_vgmstream_zsd,
    init_vgmstream_vgs_ps,
    init_vgmstream_redspark,
    init_vgmstream_ivaud,
    init_vgmstream_wii_wsd,
    init_vgmstream_dsp_ndp,
    init_vgmstream_ps2_sps,
    init_vgmstream_ps2_xa2_rrp,
    init_vgmstream_nds_hwas,
    init_vgmstream_ngc_lps,
    init_vgmstream_ps2_snd,
    init_vgmstream_naomi_adpcm,
    init_vgmstream_sd9,
    init_vgmstream_2dx9,
    init_vgmstream_dsp_ygo,
    init_vgmstream_ps2_vgv,
    init_vgmstream_gcub,
    init_vgmstream_maxis_xa,
    init_vgmstream_ngc_sck_dsp,
    init_vgmstream_apple_caff,
    init_vgmstream_pc_mxst,
    init_vgmstream_sab,
    init_vgmstream_wii_bns,
    init_vgmstream_wii_was,
    init_vgmstream_pona_3do,
    init_vgmstream_pona_psx,
    init_vgmstream_xbox_hlwav,
    init_vgmstream_myspd,
    init_vgmstream_his,
    init_vgmstream_ast_mmv,
    init_vgmstream_ast_mv,
    init_vgmstream_dmsg,
    init_vgmstream_ngc_dsp_aaap,
    init_vgmstream_ngc_dsp_konami,
    init_vgmstream_ps2_wb,
    init_vgmstream_bnsf,
    init_vgmstream_ps2_gcm,
    init_vgmstream_ps2_smpl,
    init_vgmstream_ps2_msa,
    init_vgmstream_ps2_voi,
    init_vgmstream_ngc_rkv,
    init_vgmstream_dsp_ddsp,
    init_vgmstream_p3d,
    init_vgmstream_ps2_tk1,
    init_vgmstream_ngc_dsp_mpds,
    init_vgmstream_dsp_str_ig,
    init_vgmstream_ea_swvr,
    init_vgmstream_ps2_b1s,
    init_vgmstream_ps2_wad,
    init_vgmstream_dsp_xiii,
    init_vgmstream_dsp_cabelas,
    init_vgmstream_lpcm_shade,
    init_vgmstream_dsp_bdsp,
    init_vgmstream_ps2_vms,
    init_vgmstream_xau,
    init_vgmstream_bar,
    init_vgmstream_ffw,
    init_vgmstream_dsp_dspw,
    init_vgmstream_jstm,
    init_vgmstream_xvag,
    init_vgmstream_cps,
    init_vgmstream_sqex_scd,
    init_vgmstream_ngc_nst_dsp,
    init_vgmstream_baf,
    init_vgmstream_msf,
    init_vgmstream_ps3_past,
    init_vgmstream_sgxd,
    init_vgmstream_wii_ras,
    init_vgmstream_spm,
    init_vgmstream_ps2_iab,
    init_vgmstream_vs_str,
    init_vgmstream_lsf_n1nj4n,
    init_vgmstream_xwav_new,
    init_vgmstream_xwav_old,
    init_vgmstream_hyperscan_kvag,
    init_vgmstream_ios_psnd,
    init_vgmstream_adp_bos,
    init_vgmstream_adp_qd,
    init_vgmstream_eb_sfx,
    init_vgmstream_eb_sf0,
    init_vgmstream_mtaf,
    init_vgmstream_alp,
    init_vgmstream_wpd,
    init_vgmstream_mn_str,
    init_vgmstream_mss,
    init_vgmstream_ps2_hsf,
    init_vgmstream_ivag,
    init_vgmstream_ps2_2pfs,
    init_vgmstream_xnb,
    init_vgmstream_ubi_ckd,
    init_vgmstream_ps2_vbk,
    init_vgmstream_otm,
    init_vgmstream_bcstm,
    init_vgmstream_idsp_namco,
    init_vgmstream_kt_g1l,
    init_vgmstream_kt_wiibgm,
    init_vgmstream_bfstm,
    init_vgmstream_mca,
#if 0
    init_vgmstream_mp4_aac,
#endif
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    init_vgmstream_akb_mp4,
#endif
    init_vgmstream_ktss,
    init_vgmstream_hca,
    init_vgmstream_svag_snk,
    init_vgmstream_ps2_vds_vdm,
    init_vgmstream_cxs,
    init_vgmstream_dsp_adx,
    init_vgmstream_akb,
    init_vgmstream_akb2,
#ifdef VGM_USE_FFMPEG
    init_vgmstream_mp4_aac_ffmpeg,
#endif
    init_vgmstream_bik,
    init_vgmstream_astb,
    init_vgmstream_wwise,
    init_vgmstream_ubi_raki,
    init_vgmstream_pasx,
    init_vgmstream_xma,
    init_vgmstream_sxd,
    init_vgmstream_ogl,
    init_vgmstream_mc3,
    init_vgmstream_ghs,
    init_vgmstream_aac_triace,
    init_vgmstream_va3,
    init_vgmstream_mta2,
    init_vgmstream_mta2_container,
    init_vgmstream_xa_xa30,
    init_vgmstream_xa_04sw,
    init_vgmstream_ea_bnk,
    init_vgmstream_ea_abk,
    init_vgmstream_ea_hdr_dat,
    init_vgmstream_ea_hdr_dat_v2,
    init_vgmstream_ea_map_mus,
    init_vgmstream_ea_mpf_mus,
    init_vgmstream_ea_schl_fixed,
    init_vgmstream_sk_aud,
    init_vgmstream_stm,
    init_vgmstream_ea_snu,
    init_vgmstream_awc,
    init_vgmstream_opus_std,
    init_vgmstream_opus_n1,
    init_vgmstream_opus_capcom,
    init_vgmstream_opus_nop,
    init_vgmstream_opus_shinen,
    init_vgmstream_opus_nus3,
    init_vgmstream_opus_sps_n1,
    init_vgmstream_opus_nxa,
    init_vgmstream_pc_ast,
    init_vgmstream_naac,
    init_vgmstream_ubi_sb,
    init_vgmstream_ubi_sm,
    init_vgmstream_ubi_bnm,
    init_vgmstream_ubi_bnm_ps2,
    init_vgmstream_ubi_dat,
    init_vgmstream_ubi_blk,
    init_vgmstream_ezw,
    init_vgmstream_vxn,
    init_vgmstream_ea_snr_sns,
    init_vgmstream_ea_sps,
    init_vgmstream_ea_abk_eaac,
    init_vgmstream_ea_hdr_sth_dat,
    init_vgmstream_ea_mpf_mus_eaac,
    init_vgmstream_ea_tmx,
    init_vgmstream_ea_sbr,
    init_vgmstream_ea_sbr_harmony,
    init_vgmstream_ngc_vid1,
    init_vgmstream_flx,
    init_vgmstream_mogg,
    init_vgmstream_kma9,
    init_vgmstream_fsb_encrypted,
    init_vgmstream_xwc,
    init_vgmstream_atsl,
    init_vgmstream_sps_n1,
    init_vgmstream_atx,
    init_vgmstream_sqex_sead,
    init_vgmstream_waf,
    init_vgmstream_wave,
    init_vgmstream_wave_segmented,
    init_vgmstream_smv,
    init_vgmstream_nxap,
    init_vgmstream_ea_wve_au00,
    init_vgmstream_ea_wve_ad10,
    init_vgmstream_sthd,
    init_vgmstream_pcm_sre,
    init_vgmstream_dsp_mcadpcm,
    init_vgmstream_ubi_lyn,
    init_vgmstream_ubi_lyn_container,
    init_vgmstream_msb_msh,
    init_vgmstream_txtp,
    init_vgmstream_smc_smh,
    init_vgmstream_ppst,
    init_vgmstream_sps_n1_segmented,
    init_vgmstream_ubi_bao_pk,
    init_vgmstream_ubi_bao_atomic,
    init_vgmstream_dsp_switch_audio,
    init_vgmstream_sadf,
    init_vgmstream_h4m,
    init_vgmstream_ads_container,
    init_vgmstream_asf,
    init_vgmstream_xmd,
    init_vgmstream_cks,
    init_vgmstream_ckb,
    init_vgmstream_wv6,
    init_vgmstream_str_wav,
    init_vgmstream_wavebatch,
    init_vgmstream_hd3_bd3,
    init_vgmstream_bnk_sony,
    init_vgmstream_nus3bank,
    init_vgmstream_nus3bank_encrypted,
    init_vgmstream_sscf,
    init_vgmstream_dsp_sps_n1,
    init_vgmstream_dsp_itl_ch,
    init_vgmstream_a2m,
    init_vgmstream_ahv,
    init_vgmstream_msv,
    init_vgmstream_sdf,
    init_vgmstream_svg,
    init_vgmstream_vis,
    init_vgmstream_vai,
    init_vgmstream_aif_asobo,
    init_vgmstream_ao,
    init_vgmstream_apc,
    init_vgmstream_wv2,
    init_vgmstream_xau_konami,
    init_vgmstream_derf,
    init_vgmstream_utk,
    init_vgmstream_adpcm_capcom,
    init_vgmstream_ue4opus,
    init_vgmstream_xwma,
    init_vgmstream_xopus,
    init_vgmstream_vs_square,
    init_vgmstream_msf_banpresto_wmsf,
    init_vgmstream_msf_banpresto_2msf,
    init_vgmstream_nwav,
    init_vgmstream_xpcm,
    init_vgmstream_msf_tamasoft,
    init_vgmstream_xps_dat,
    init_vgmstream_xps,
    init_vgmstream_zsnd,
    init_vgmstream_opus_opusx,
    init_vgmstream_dsp_adpy,
    init_vgmstream_dsp_adpx,
    init_vgmstream_ogg_opus,
    init_vgmstream_nus3audio,
    init_vgmstream_imc,
    init_vgmstream_imc_container,
    init_vgmstream_smp,
    init_vgmstream_gin,
    init_vgmstream_dsf,
    init_vgmstream_208,
    init_vgmstream_dsp_ds2,
    init_vgmstream_ffdl,
    init_vgmstream_mus_vc,
    init_vgmstream_strm_abylight,
    init_vgmstream_sfh,
    init_vgmstream_ea_schl_video,
    init_vgmstream_msf_konami,
    init_vgmstream_xwma_konami,
    init_vgmstream_9tav,
    init_vgmstream_fsb5_fev_bank,
    init_vgmstream_bwav,
    init_vgmstream_opus_prototype,
    init_vgmstream_awb,
    init_vgmstream_acb,
    init_vgmstream_rad,
    init_vgmstream_smk,
    init_vgmstream_mzrt_v0,
    init_vgmstream_xavs,
    init_vgmstream_psf_single,
    init_vgmstream_psf_segmented,
    init_vgmstream_dsp_itl,
    init_vgmstream_sch,
    init_vgmstream_ima,
    init_vgmstream_nub,
    init_vgmstream_nub_wav,
    init_vgmstream_nub_vag,
    init_vgmstream_nub_at3,
    init_vgmstream_nub_xma,
    init_vgmstream_nub_idsp,
    init_vgmstream_nub_is14,
    init_vgmstream_xwv_valve,
    init_vgmstream_ubi_hx,
    init_vgmstream_bmp_konami,
    init_vgmstream_opus_opusnx,
    init_vgmstream_opus_sqex,
    init_vgmstream_isb,
    init_vgmstream_xssb,
    init_vgmstream_xma_ue3,
    init_vgmstream_csb,
    init_vgmstream_fwse,
    init_vgmstream_fda,
    init_vgmstream_kwb,
    init_vgmstream_lrmd,
    init_vgmstream_bkhd,
    init_vgmstream_bkhd_fx,
    init_vgmstream_diva,
    init_vgmstream_imuse,
    init_vgmstream_ktsr,
    init_vgmstream_mups,
    init_vgmstream_kat,
    init_vgmstream_pcm_success,
    init_vgmstream_ktsc,
    init_vgmstream_adp_konami,
    init_vgmstream_zwv,
    init_vgmstream_dsb,
    init_vgmstream_bsf,
    init_vgmstream_sdrh_new,
    init_vgmstream_sdrh_old,
    init_vgmstream_wady,
    init_vgmstream_dsp_sqex,
    init_vgmstream_dsp_wiivoice,
    init_vgmstream_xws,
    init_vgmstream_cpk,
    init_vgmstream_opus_nsopus,
    init_vgmstream_sbk,
    init_vgmstream_dsp_wiiadpcm,
    init_vgmstream_dsp_cwac,
    init_vgmstream_ifs,
    init_vgmstream_acx,
    init_vgmstream_compresswave,
    init_vgmstream_ktac,
    init_vgmstream_mzrt_v1,
    init_vgmstream_bsnf,
    init_vgmstream_tac,
    init_vgmstream_idsp_tose,
    init_vgmstream_dsp_kwa,
    init_vgmstream_ogv_3rdeye,
    init_vgmstream_sspr,
    init_vgmstream_piff_tpcm,
    init_vgmstream_wxd_wxh,
    init_vgmstream_bnk_relic,
    init_vgmstream_xsh_xsd_xss,
    init_vgmstream_psb,
    init_vgmstream_lopu_fb,
    init_vgmstream_lpcm_fb,
    init_vgmstream_wbk,
    init_vgmstream_wbk_nslb,
    init_vgmstream_dsp_apex,
    init_vgmstream_ubi_ckd_cwav,
    init_vgmstream_sspf,
    init_vgmstream_opus_rsnd,
    init_vgmstream_s3v,
    init_vgmstream_esf,
    init_vgmstream_adm3,
    init_vgmstream_tt_ad,
    init_vgmstream_bw_mp3_riff,
    init_vgmstream_bw_riff_mp3,
    init_vgmstream_sndz,
    init_vgmstream_vab,
    init_vgmstream_bigrp,
    init_vgmstream_sscf_encrypted,
    init_vgmstream_s_p_sth,
    init_vgmstream_utf_ahx,

    /* lower priority metas (no clean header identity, somewhat ambiguous, or need extension/companion file to identify) */
    init_vgmstream_scd_pcm,
    init_vgmstream_agsc,
    init_vgmstream_rsf,
    init_vgmstream_ps2_wmus,
    init_vgmstream_mib_mih,
    init_vgmstream_mjb_mjh,
    init_vgmstream_mic_koei,
    init_vgmstream_seb,
    init_vgmstream_ps2_pnb,
    init_vgmstream_sli_ogg,
    init_vgmstream_tgc,

    /* lowest priority metas (should go after all metas, and TXTH should go before raw formats) */
    init_vgmstream_txth,            /* proper parsers should supersede TXTH, once added */
    init_vgmstream_dtk,             /* semi-raw GC streamed files */
    init_vgmstream_mpeg,            /* semi-raw MP3 */
    init_vgmstream_btsnd,           /* semi-headerless */
    init_vgmstream_encrypted,       /* encrypted stuff */
    init_vgmstream_raw_int,         /* .int raw PCM */
    init_vgmstream_ps_headerless,   /* tries to detect a bunch of PS-ADPCM formats */
    init_vgmstream_raw_snds,        /* .snds raw SNDS IMA */
    init_vgmstream_raw_wavm,        /* .wavm raw xbox */
    init_vgmstream_raw_pcm,         /* .raw raw PCM */
    init_vgmstream_s14_sss,         /* .s14/sss raw siren14 */
    init_vgmstream_raw_al,          /* .al/al2 raw A-LAW */
    init_vgmstream_ngc_ulw,         /* .ulw raw u-Law */
    init_vgmstream_exakt_sc,        /* .sc raw PCM */
    init_vgmstream_zwdsp,           /* fake format */
    init_vgmstream_ps2_adm,         /* weird non-constant PSX blocks */
    init_vgmstream_rwsd,            /* crap, to be removed */
#ifdef VGM_USE_FFMPEG
    init_vgmstream_ffmpeg,          /* may play anything incorrectly, since FFmpeg doesn't check extensions */
#endif
};


/*****************************************************************************/
/* INIT/META                                                                 */
/*****************************************************************************/
#define LOCAL_ARRAY_LENGTH(array) (sizeof(array) / sizeof(array[0]))

/* internal version with all parameters */
static VGMSTREAM* init_vgmstream_internal(STREAMFILE* sf) {
    int i, fcns_count;

    if (!sf)
        return NULL;

    fcns_count = LOCAL_ARRAY_LENGTH(init_vgmstream_functions);

    /* try a series of formats, see which works */
    for (i = 0; i < fcns_count; i++) {
        /* call init function and see if valid VGMSTREAM was returned */
        VGMSTREAM* vgmstream = (init_vgmstream_functions[i])(sf);
        if (!vgmstream)
            continue;

        /* fail if there is nothing/too much to play (<=0 generates empty files, >N writes GBs of garbage) */
        if (vgmstream->num_samples <= 0 || vgmstream->num_samples > VGMSTREAM_MAX_NUM_SAMPLES) {
            VGM_LOG("VGMSTREAM: wrong num_samples %i\n", vgmstream->num_samples);
            close_vgmstream(vgmstream);
            continue;
        }

        /* everything should have a reasonable sample rate */
        if (vgmstream->sample_rate < VGMSTREAM_MIN_SAMPLE_RATE || vgmstream->sample_rate > VGMSTREAM_MAX_SAMPLE_RATE) {
            VGM_LOG("VGMSTREAM: wrong sample_rate %i\n", vgmstream->sample_rate);
            close_vgmstream(vgmstream);
            continue;
        }

        /* sanify loops and remove bad metadata */
        if (vgmstream->loop_flag) {
            if (vgmstream->loop_end_sample <= vgmstream->loop_start_sample
                    || vgmstream->loop_end_sample > vgmstream->num_samples
                    || vgmstream->loop_start_sample < 0) {
                VGM_LOG("VGMSTREAM: wrong loops ignored (lss=%i, lse=%i, ns=%i)\n",
                        vgmstream->loop_start_sample, vgmstream->loop_end_sample, vgmstream->num_samples);
                vgmstream->loop_flag = 0;
                vgmstream->loop_start_sample = 0;
                vgmstream->loop_end_sample = 0;
            }
        }

        /* test if candidate for dual stereo */
        if (vgmstream->channels == 1 && vgmstream->allow_dual_stereo == 1) {
            try_dual_file_stereo(vgmstream, sf, init_vgmstream_functions[i]);
        }

        /* clean as loops are readable metadata but loop fields may contain garbage
         * (done *after* dual stereo as it needs loop fields to match) */
        if (!vgmstream->loop_flag) {
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = 0;
        }

#ifdef VGM_USE_FFMPEG
        /* check FFmpeg streams here, for lack of a better place */
        if (vgmstream->coding_type == coding_FFmpeg) {
            int ffmpeg_subsongs = ffmpeg_get_subsong_count(vgmstream->codec_data);
            if (ffmpeg_subsongs && !vgmstream->num_streams) {
                vgmstream->num_streams = ffmpeg_subsongs;
            }
        }
#endif

        /* some players are picky with incorrect channel layouts */
        if (vgmstream->channel_layout > 0) {
            int output_channels = vgmstream->channels;
            int ch, count = 0, max_ch = 32;
            for (ch = 0; ch < max_ch; ch++) {
                int bit = (vgmstream->channel_layout >> ch) & 1;
                if (ch > 17 && bit) {
                    VGM_LOG("VGMSTREAM: wrong bit %i in channel_layout %x\n", ch, vgmstream->channel_layout);
                    vgmstream->channel_layout = 0;
                    break;
                }
                count += bit;
            }

            if (count > output_channels) {
                VGM_LOG("VGMSTREAM: wrong totals %i in channel_layout %x\n", count, vgmstream->channel_layout);
                vgmstream->channel_layout = 0;
            }
        }

        /* files can have thousands subsongs, but let's put a limit */
        if (vgmstream->num_streams < 0 || vgmstream->num_streams > VGMSTREAM_MAX_SUBSONGS) {
            VGM_LOG("VGMSTREAM: wrong num_streams (ns=%i)\n", vgmstream->num_streams);
            close_vgmstream(vgmstream);
            continue;
        }

        /* save info */
        /* stream_index 0 may be used by plugins to signal "vgmstream default" (IOW don't force to 1) */
        if (vgmstream->stream_index == 0) {
            vgmstream->stream_index = sf->stream_index;
        }


        setup_vgmstream(vgmstream); /* final setup */

        return vgmstream;
    }

    /* not supported */
    return NULL;
}

void setup_vgmstream(VGMSTREAM* vgmstream) {

    /* save start things so we can restart when seeking */
    memcpy(vgmstream->start_ch, vgmstream->ch, sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
    memcpy(vgmstream->start_vgmstream, vgmstream, sizeof(VGMSTREAM));

    /* layout's sub-VGMSTREAM are expected to setup externally and maybe call this,
     * as they can be created using init_vgmstream or manually */
}


/* format detection and VGMSTREAM setup, uses default parameters */
VGMSTREAM* init_vgmstream(const char* const filename) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf = open_stdio_streamfile(filename);
    if (sf) {
        vgmstream = init_vgmstream_from_STREAMFILE(sf);
        close_streamfile(sf);
    }
    return vgmstream;
}

VGMSTREAM* init_vgmstream_from_STREAMFILE(STREAMFILE* sf) {
    return init_vgmstream_internal(sf);
}

/* Reset a VGMSTREAM to its state at the start of playback (when a plugin seeks back to zero). */
void reset_vgmstream(VGMSTREAM* vgmstream) {

    /* reset the VGMSTREAM and channels back to their original state */
    memcpy(vgmstream, vgmstream->start_vgmstream, sizeof(VGMSTREAM));
    memcpy(vgmstream->ch, vgmstream->start_ch, sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
    /* loop_ch is not reset here because there is a possibility of the
     * init_vgmstream_* function doing something tricky and precomputing it.
     * Otherwise hit_loop will be 0 and it will be copied over anyway when we
     * really hit the loop start. */

    reset_codec(vgmstream);

    reset_layout(vgmstream);

    /* note that this does not reset the constituent STREAMFILES
     * (vgmstream->ch[N].streamfiles' internal state, like internal offset, though shouldn't matter) */
}

/* Allocate memory and setup a VGMSTREAM */
VGMSTREAM* allocate_vgmstream(int channel_count, int loop_flag) {
    VGMSTREAM* vgmstream;

    /* up to ~16-24 aren't too rare for multilayered files, more is probably a bug */
    if (channel_count <= 0 || channel_count > VGMSTREAM_MAX_CHANNELS) {
        VGM_LOG("VGMSTREAM: error allocating %i channels\n", channel_count);
        return NULL;
    }

    /* VGMSTREAM's alloc'ed internals work as follows:
     * - vgmstream: main struct (config+state) modified by metas, layouts and codings as needed
     * - ch: config+state per channel, also modified by those
     * - start_vgmstream: vgmstream clone copied on init_vgmstream and restored on reset_vgmstream
     * - start_ch: ch clone copied on init_vgmstream and restored on reset_vgmstream
     * - loop_ch: ch clone copied on loop start and restored on loop end (vgmstream_do_loop)
     * - codec/layout_data: custom state for complex codecs or layouts, handled externally
     *
     * Here we only create the basic structs to be filled, and only after init_vgmstream it
     * can be considered ready. Clones are shallow copies, in that they share alloc'ed struts
     * (like, vgmstream->ch and start_vgmstream->ch will be the same after init_vgmstream, or
     * start_vgmstream->start_vgmstream will end up pointing to itself)
     *
     * This is all a bit too brittle, so code alloc'ing or changing anything sensitive should
     * take care clones are properly synced.
     */

    /* create vgmstream + main structs (other data is 0'ed) */
    vgmstream = calloc(1,sizeof(VGMSTREAM));
    if (!vgmstream) return NULL;

    vgmstream->start_vgmstream = calloc(1,sizeof(VGMSTREAM));
    if (!vgmstream->start_vgmstream) goto fail;

    vgmstream->ch = calloc(channel_count,sizeof(VGMSTREAMCHANNEL));
    if (!vgmstream->ch) goto fail;

    vgmstream->start_ch = calloc(channel_count,sizeof(VGMSTREAMCHANNEL));
    if (!vgmstream->start_ch) goto fail;

    if (loop_flag) {
        vgmstream->loop_ch = calloc(channel_count,sizeof(VGMSTREAMCHANNEL));
        if (!vgmstream->loop_ch) goto fail;
    }

    /* garbage buffer for decode discarding (local bufs may cause stack overflows with segments/layers)
     * in theory the bigger the better but in practice there isn't much difference */
    vgmstream->tmpbuf_size = 0x10000; /* for all channels */
    vgmstream->tmpbuf = malloc(sizeof(sample_t) * vgmstream->tmpbuf_size);


    vgmstream->channels = channel_count;
    vgmstream->loop_flag = loop_flag;

    mixing_init(vgmstream); /* pre-init */

    /* BEWARE: try_dual_file_stereo does some free'ing too */ 

    //vgmstream->stream_name_size = STREAM_NAME_SIZE;
    return vgmstream;
fail:
    if (vgmstream) {
        mixing_close(vgmstream);
        free(vgmstream->tmpbuf);
        free(vgmstream->ch);
        free(vgmstream->start_ch);
        free(vgmstream->loop_ch);
        free(vgmstream->start_vgmstream);
    }
    free(vgmstream);
    return NULL;
}

void close_vgmstream(VGMSTREAM* vgmstream) {
    if (!vgmstream)
        return;

    free_codec(vgmstream);
    vgmstream->codec_data = NULL;

    free_layout(vgmstream);
    vgmstream->layout_data = NULL;


    /* now that the special cases have had their chance, clean up the standard items */
    {
        int i,j;

        for (i = 0; i < vgmstream->channels; i++) {
            if (vgmstream->ch[i].streamfile) {
                close_streamfile(vgmstream->ch[i].streamfile);
                /* Multiple channels might have the same streamfile. Find the others
                 * that are the same as this and clear them so they won't be closed again. */
                for (j = 0; j < vgmstream->channels; j++) {
                    if (i != j && vgmstream->ch[j].streamfile == vgmstream->ch[i].streamfile) {
                        vgmstream->ch[j].streamfile = NULL;
                    }
                }
                vgmstream->ch[i].streamfile = NULL;
            }
        }
    }

    mixing_close(vgmstream);
    free(vgmstream->tmpbuf);
    free(vgmstream->ch);
    free(vgmstream->start_ch);
    free(vgmstream->loop_ch);
    free(vgmstream->start_vgmstream);
    free(vgmstream);
}

void vgmstream_force_loop(VGMSTREAM* vgmstream, int loop_flag, int loop_start_sample, int loop_end_sample) {
    if (!vgmstream) return;

    /* ignore bad values (may happen with layers + TXTP loop install) */
    if (loop_flag && (loop_start_sample < 0 ||
            loop_start_sample > loop_end_sample ||
            loop_end_sample > vgmstream->num_samples))
        return;

    /* this requires a bit more messing with the VGMSTREAM than I'm comfortable with... */
    if (loop_flag && !vgmstream->loop_flag && !vgmstream->loop_ch) {
        vgmstream->loop_ch = calloc(vgmstream->channels, sizeof(VGMSTREAMCHANNEL));
        if (!vgmstream->loop_ch) loop_flag = 0; /* ??? */
    }
#if 0
    /* allow in case loop_flag is re-enabled later  */
    else if (!loop_flag && vgmstream->loop_flag) {
        free(vgmstream->loop_ch);
        vgmstream->loop_ch = NULL;
    }
#endif

    vgmstream->loop_flag = loop_flag;

    if (loop_flag) {
        vgmstream->loop_start_sample = loop_start_sample;
        vgmstream->loop_end_sample = loop_end_sample;
    }
#if 0 /* keep metadata as it's may be shown (with 'loop disabled' info) */
    else {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = 0;
    }
#endif

    /* propagate changes to layouts that need them */
    if (vgmstream->layout_type == layout_layered) {
        int i;
        layered_layout_data* data = vgmstream->layout_data;

        /* layered loops using the internal VGMSTREAMs */
        for (i = 0; i < data->layer_count; i++) {
            if (!data->layers[i]->config_enabled) /* only in simple mode */
                vgmstream_force_loop(data->layers[i], loop_flag, loop_start_sample, loop_end_sample);
            /* layer's force_loop also calls setup_vgmstream, no need to do it here */
        }
    }

    /* segmented layout loops with standard loop start/end values and works ok */

    /* notify of new initial state */
    setup_vgmstream(vgmstream);
}

void vgmstream_set_loop_target(VGMSTREAM* vgmstream, int loop_target) {
    if (!vgmstream) return;
    if (!vgmstream->loop_flag) return;


    vgmstream->loop_target = loop_target; /* loop count must be rounded (int) as otherwise target is meaningless */

    /* propagate changes to layouts that need them */
    if (vgmstream->layout_type == layout_layered) {
        int i;
        layered_layout_data *data = vgmstream->layout_data;
        for (i = 0; i < data->layer_count; i++) {
            vgmstream_set_loop_target(data->layers[i], loop_target);
        }
    }

    /* notify of new initial state */
    setup_vgmstream(vgmstream);
}


/*******************************************************************************/
/* MISC                                                                        */
/*******************************************************************************/

/* See if there is a second file which may be the second channel, given an already opened mono vgmstream.
 * If a suitable file is found, open it and change opened_vgmstream to a stereo vgmstream. */
static void try_dual_file_stereo(VGMSTREAM* opened_vgmstream, STREAMFILE* sf, VGMSTREAM*(*init_vgmstream_function)(STREAMFILE*)) {
    /* filename search pairs for dual file stereo */
    static const char* const dfs_pairs[][2] = {
        {"L","R"}, /* most common in .dsp and .vag */
        {"l","r"}, /* same */
        {"left","right"}, /* Freaky Flyers (GC) .adp, Velocity (PSP) .vag, Hyper Fighters (Wii) .dsp */
        {"Left","Right"}, /* Geometry Wars: Galaxies (Wii) .dsp */
        {".V0",".V1"}, /* Homura (PS2) */
        {".L",".R"}, /* Crash Nitro Racing (PS2), Gradius V (PS2) */
        {"_0.dsp","_1.dsp"}, /* Wario World (GC) */
        {".adpcm","_NxEncoderOut_.adpcm"}, /* Kill la Kill: IF (Switch) */
        {".adpcm","_2.adpcm"}, /* Desire: Remaster Version (Switch) */
    };
    char new_filename[PATH_LIMIT];
    char* extension;
    int dfs_pair = -1; /* -1=no stereo, 0=opened_vgmstream is left, 1=opened_vgmstream is right */
    VGMSTREAM* new_vgmstream = NULL;
    STREAMFILE* dual_sf = NULL;
    int i,j, dfs_pair_count, extension_len, filename_len;

    if (opened_vgmstream->channels != 1)
        return;

    /* custom codec/layouts aren't designed for this (should never get here anyway) */
    if (opened_vgmstream->codec_data || opened_vgmstream->layout_data)
        return;

    //todo other layouts work but some stereo codecs do weird things
    //if (opened_vgmstream->layout != layout_none) return;

    get_streamfile_name(sf, new_filename, sizeof(new_filename));
    filename_len = strlen(new_filename);
    if (filename_len < 2)
        return;

    extension = (char *)filename_extension(new_filename);
    if (extension - new_filename >= 1 && extension[-1] == '.') /* [-1] is ok, yeah */
        extension--; /* must include "." */
    extension_len = strlen(extension);


    /* find pair from base name and modify new_filename with the opposite (tries L>R then R>L) */
    dfs_pair_count = (sizeof(dfs_pairs)/sizeof(dfs_pairs[0]));
    for (i = 0; dfs_pair == -1 && i < dfs_pair_count; i++) {
        for (j = 0; dfs_pair == -1 && j < 2; j++) {
            const char* this_suffix = dfs_pairs[i][j];
            const char* that_suffix = dfs_pairs[i][j^1];
            size_t this_suffix_len = strlen(dfs_pairs[i][j]);
            size_t that_suffix_len = strlen(dfs_pairs[i][j^1]);

            //;VGM_LOG("DFS: l=%s, r=%s\n", this_suffix,that_suffix);

            /* if suffix matches paste opposite suffix (+ terminator) to extension pointer, thus to new_filename */
            if (filename_len > this_suffix_len && strchr(this_suffix, '.') != NULL) { /* same suffix with extension */
                //;VGM_LOG("DFS: suf+ext %s vs %s len %i\n", new_filename, this_suffix, this_suffix_len);
                if (memcmp(new_filename + (filename_len - this_suffix_len), this_suffix, this_suffix_len) == 0) {
                    memcpy (new_filename + (filename_len - this_suffix_len), that_suffix,that_suffix_len+1);
                    dfs_pair = j;
                }
            }
            else if (filename_len - extension_len > this_suffix_len) { /* same suffix without extension */
                //;VGM_LOG("DFS: suf-ext %s vs %s len %i\n", extension - this_suffix_len, this_suffix, this_suffix_len);
                if (memcmp(extension - this_suffix_len, this_suffix,this_suffix_len) == 0) {
                    memmove(extension + that_suffix_len - this_suffix_len, extension,extension_len+1); /* move old extension to end */
                    memcpy (extension - this_suffix_len, that_suffix,that_suffix_len); /* overwrite with new suffix */
                    dfs_pair = j;
                }
            }

            if (dfs_pair != -1) {
                //VGM_LOG("DFS: try %i: %s\n", dfs_pair, new_filename);
                /* try to init other channel (new_filename now has the opposite name) */
                dual_sf = open_streamfile(sf, new_filename);
                if (!dual_sf) {
                    /* restore filename and keep trying (if found it'll break and init) */
                    dfs_pair = -1;
                    get_streamfile_name(sf, new_filename, sizeof(new_filename));
                }
            }
        }
    }

    /* filename didn't have a suitable L/R-pair name */
    if (dfs_pair == -1)
        goto fail;
    //;VGM_LOG("DFS: match %i filename=%s\n", dfs_pair, new_filename);

    new_vgmstream = init_vgmstream_function(dual_sf); /* use the init function that just worked */
    close_streamfile(dual_sf);

    /* see if we were able to open the file, and if everything matched nicely */
    if (!(new_vgmstream &&
            new_vgmstream->channels == 1 &&
            /* we have seen legitimate pairs where these are off by one...
             * but leaving it commented out until I can find those and recheck */
            /* abs(new_vgmstream->num_samples-opened_vgmstream->num_samples <= 1) && */
            new_vgmstream->num_samples == opened_vgmstream->num_samples &&
            new_vgmstream->sample_rate == opened_vgmstream->sample_rate &&
            new_vgmstream->meta_type   == opened_vgmstream->meta_type &&
            new_vgmstream->coding_type == opened_vgmstream->coding_type &&
            new_vgmstream->layout_type == opened_vgmstream->layout_type &&
            /* check even if the layout doesn't use them, because it is
             * difficult to determine when it does, and they should be zero otherwise, anyway */
            new_vgmstream->interleave_block_size == opened_vgmstream->interleave_block_size &&
            new_vgmstream->interleave_last_block_size == opened_vgmstream->interleave_last_block_size)) {
        goto fail;
    }

    /* check these even if there is no loop, because they should then be zero in both
     * (Homura PS2 right channel doesn't have loop points so this check is ignored) */
    if (new_vgmstream->meta_type != meta_PS2_SMPL &&
            !(new_vgmstream->loop_flag      == opened_vgmstream->loop_flag &&
            new_vgmstream->loop_start_sample== opened_vgmstream->loop_start_sample &&
            new_vgmstream->loop_end_sample  == opened_vgmstream->loop_end_sample)) {
        goto fail;
    }

    /* We seem to have a usable, matching file. Merge in the second channel. */
    {
        VGMSTREAMCHANNEL* new_chans;
        VGMSTREAMCHANNEL* new_loop_chans = NULL;
        VGMSTREAMCHANNEL* new_start_chans = NULL;

        /* build the channels */
        new_chans = calloc(2,sizeof(VGMSTREAMCHANNEL));
        if (!new_chans) goto fail;

        memcpy(&new_chans[dfs_pair],&opened_vgmstream->ch[0],sizeof(VGMSTREAMCHANNEL));
        memcpy(&new_chans[dfs_pair^1],&new_vgmstream->ch[0],sizeof(VGMSTREAMCHANNEL));

        /* loop and start will be initialized later, we just need to allocate them here */
        new_start_chans = calloc(2,sizeof(VGMSTREAMCHANNEL));
        if (!new_start_chans) {
            free(new_chans);
            goto fail;
        }

        if (opened_vgmstream->loop_ch) {
            new_loop_chans = calloc(2,sizeof(VGMSTREAMCHANNEL));
            if (!new_loop_chans) {
                free(new_chans);
                free(new_start_chans);
                goto fail;
            }
        }

        /* remove the existing structures */
        /* not using close_vgmstream as that would close the file */
        free(opened_vgmstream->ch);
        free(new_vgmstream->ch);

        free(opened_vgmstream->start_ch);
        free(new_vgmstream->start_ch);

        if (opened_vgmstream->loop_ch) {
            free(opened_vgmstream->loop_ch);
            free(new_vgmstream->loop_ch);
        }

        /* fill in the new structures */
        opened_vgmstream->ch = new_chans;
        opened_vgmstream->start_ch = new_start_chans;
        opened_vgmstream->loop_ch = new_loop_chans;

        /* stereo! */
        opened_vgmstream->channels = 2;
        if (opened_vgmstream->layout_type == layout_interleave)
            opened_vgmstream->layout_type = layout_none; /* fixes some odd cases */

        /* discard the second VGMSTREAM */
        mixing_close(new_vgmstream);
        free(new_vgmstream->tmpbuf);
        free(new_vgmstream->start_vgmstream);
        free(new_vgmstream);

        mixing_update_channel(opened_vgmstream); /* notify of new channel hacked-in */
    }

    return;
fail:
    close_vgmstream(new_vgmstream);
    return;
}


/**
 * Inits vgmstream, doing two things:
 * - sets the starting offset per channel (depending on the layout)
 * - opens its own streamfile from on a base one. One streamfile per channel may be open (to improve read/seeks).
 * Should be called in metas before returning the VGMSTREAM.
 */
int vgmstream_open_stream(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t start_offset) {
    return vgmstream_open_stream_bf(vgmstream, sf, start_offset, 0);
}
int vgmstream_open_stream_bf(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t start_offset, int force_multibuffer) {
    STREAMFILE* file = NULL;
    char filename[PATH_LIMIT];
    int ch;
    int use_streamfile_per_channel = 0;
    int use_same_offset_per_channel = 0;
    int is_stereo_codec = 0;


    if (vgmstream == NULL) {
        VGM_LOG("VGMSTREAM: buggy code (null vgmstream)\n");
        goto fail;
    }


    /* stream/offsets not needed, managed by layout */
    if (vgmstream->layout_type == layout_segmented ||
        vgmstream->layout_type == layout_layered)
        return 1;

    /* stream/offsets not needed, managed by decoder */
    if (vgmstream->coding_type == coding_NWA ||
        vgmstream->coding_type == coding_ACM ||
        vgmstream->coding_type == coding_CRI_HCA)
        return 1;

#ifdef VGM_USE_VORBIS
    /* stream/offsets not needed, managed by decoder */
    if (vgmstream->coding_type == coding_OGG_VORBIS)
        return 1;
#endif

#ifdef VGM_USE_FFMPEG
    /* stream/offsets not needed, managed by decoder */
    if (vgmstream->coding_type == coding_FFmpeg)
        return 1;
#endif

    if ((vgmstream->coding_type == coding_PSX_cfg ||
            vgmstream->coding_type == coding_PSX_pivotal) &&
            (vgmstream->interleave_block_size == 0 || vgmstream->interleave_block_size > 0x50)) {
        VGM_LOG("VGMSTREAM: PSX-cfg decoder with wrong frame size %x\n", vgmstream->interleave_block_size);
        goto fail;
    }

    if ((vgmstream->coding_type == coding_CRI_ADX ||
            vgmstream->coding_type == coding_CRI_ADX_enc_8 ||
            vgmstream->coding_type == coding_CRI_ADX_enc_9 ||
            vgmstream->coding_type == coding_CRI_ADX_exp ||
            vgmstream->coding_type == coding_CRI_ADX_fixed) &&
            (vgmstream->interleave_block_size == 0 || vgmstream->interleave_block_size > 0x12)) {
        VGM_LOG("VGMSTREAM: ADX decoder with wrong frame size %x\n", vgmstream->interleave_block_size);
        goto fail;
    }

    if ((vgmstream->coding_type == coding_MSADPCM || vgmstream->coding_type == coding_MSADPCM_ck ||
            vgmstream->coding_type == coding_MSADPCM_int ||
            vgmstream->coding_type == coding_MS_IMA || vgmstream->coding_type == coding_MS_IMA_mono
            ) &&
            vgmstream->frame_size == 0) {
        vgmstream->frame_size = vgmstream->interleave_block_size;
    }

    if ((vgmstream->coding_type == coding_MSADPCM ||
            vgmstream->coding_type == coding_MSADPCM_ck ||
            vgmstream->coding_type == coding_MSADPCM_int) &&
            (vgmstream->frame_size > MSADPCM_MAX_BLOCK_SIZE)) {
        VGM_LOG("VGMSTREAM: MSADPCM decoder with wrong frame size %x\n", vgmstream->frame_size);
        goto fail;
    }

    /* big interleaved values for non-interleaved data may result in incorrect behavior,
     * quick fix for now since layouts are finicky, with 'interleave' left for meta info
     * (certain layouts+codecs combos results in funny output too, should rework the whole thing) */
    if (vgmstream->layout_type == layout_interleave
            && vgmstream->channels == 1
            && vgmstream->interleave_block_size > 0) {
        /* main codecs that use arbitrary interleaves but could happen for others too */
        switch(vgmstream->coding_type) {
            case coding_NGC_DSP:
            case coding_NGC_DSP_subint:
            case coding_PSX:
            case coding_PSX_badflags:
                vgmstream->interleave_block_size = 0;
                break;
            default:
                break;
        }
    }

    /* if interleave is big enough keep a buffer per channel */
    if (vgmstream->interleave_block_size * vgmstream->channels >= STREAMFILE_DEFAULT_BUFFER_SIZE) {
        use_streamfile_per_channel = 1;
    }

    /* if blocked layout (implicit) use multiple streamfiles; using only one leads to
     * lots of buffer-trashing, with all the jumping around in the block layout
     * (this increases total of data read but still seems faster) */
    if (vgmstream->layout_type != layout_none && vgmstream->layout_type != layout_interleave) {
        use_streamfile_per_channel = 1;
    }

    /* for hard-to-detect fixed offsets or full interleave */
    if (force_multibuffer) {
        use_streamfile_per_channel = 1;
    }

    /* for mono or codecs like IMA (XBOX, MS IMA, MS ADPCM) where channels work with the same bytes */
    if (vgmstream->layout_type == layout_none) {
        use_same_offset_per_channel = 1;
    }

    /* stereo codecs interleave in 2ch pairs (interleave size should still be: full_block_size / channels) */
    if (vgmstream->layout_type == layout_interleave &&
            (vgmstream->coding_type == coding_XBOX_IMA || vgmstream->coding_type == coding_MTAF)) {
        is_stereo_codec = 1;
    }

    if (sf == NULL || start_offset < 0) {
        VGM_LOG("VGMSTREAM: buggy code (null streamfile / wrong start_offset)\n");
        goto fail;
    }

    get_streamfile_name(sf, filename, sizeof(filename));
    /* open the file for reading by each channel */
    {
        if (!use_streamfile_per_channel) {
            file = open_streamfile(sf, filename);
            if (!file) goto fail;
        }

        for (ch = 0; ch < vgmstream->channels; ch++) {
            off_t offset;
            if (use_same_offset_per_channel) {
                offset = start_offset;
            } else if (is_stereo_codec) {
                int ch_mod = (ch & 1) ? ch - 1 : ch; /* adjust odd channels (ch 0,1,2,3,4,5 > ch 0,0,2,2,4,4) */
                offset = start_offset + vgmstream->interleave_block_size * ch_mod;
            } else if (vgmstream->interleave_first_block_size) {
                /* start_offset assumes + vgmstream->interleave_first_block_size, maybe should do it here */
                offset = start_offset + (vgmstream->interleave_first_block_size +  vgmstream->interleave_first_skip) * ch;
            } else {
                offset = start_offset + vgmstream->interleave_block_size * ch;
            }

            /* open new one if needed, useful to avoid jumping around when each channel data is too apart
             * (don't use when data is close as it'd make buffers read the full file multiple times) */
            if (use_streamfile_per_channel) {
                file = open_streamfile(sf,filename);
                if (!file) goto fail;
            }

            vgmstream->ch[ch].streamfile = file;
            vgmstream->ch[ch].channel_start_offset = offset;
            vgmstream->ch[ch].offset = offset;
        }
    }

    /* init first block for blocked layout (if not blocked this will do nothing) */
    block_update(start_offset, vgmstream);

    /* EA-MT decoder is a bit finicky and needs this when channel offsets change */
    if (vgmstream->coding_type == coding_EA_MT) {
        flush_ea_mt(vgmstream);
    }

    return 1;

fail:
    /* open streams will be closed in close_vgmstream(), hopefully called by the meta */
    return 0;
}

int vgmstream_is_virtual_filename(const char* filename) {
    int len = strlen(filename);
    if (len < 6)
        return 0;

    /* vgmstream can play .txtp files that have size 0 but point to another file with config
     * based only in the filename (ex. "file.fsb #2.txtp" plays 2nd subsong of file.fsb).
     *
     * Also, .m3u playlist can include files that don't exist, and players often allow filenames
     * pointing to nothing (since could be some protocol/url).
     *
     * Plugins can use both quirks to allow "virtual files" (.txtp) in .m3u that don't need
     * to exist but allow config. Plugins with this function if the filename is virtual,
     * and their STREAMFILEs should be modified as to ignore null FILEs and report size 0. */
    return strcmp(&filename[len-5], ".txtp") == 0;
}
