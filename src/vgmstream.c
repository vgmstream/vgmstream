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
#include "mixing.h"

static void try_dual_file_stereo(VGMSTREAM * opened_vgmstream, STREAMFILE *streamFile, VGMSTREAM* (*init_vgmstream_function)(STREAMFILE*));


/* list of metadata parser functions that will recognize files, used on init */
VGMSTREAM * (*init_vgmstream_functions[])(STREAMFILE *streamFile) = {
    init_vgmstream_adx,
    init_vgmstream_brstm,
    init_vgmstream_bfwav,
    init_vgmstream_bfstm,
    init_vgmstream_mca,
    init_vgmstream_btsnd,
    init_vgmstream_nds_strm,
    init_vgmstream_agsc,
    init_vgmstream_ngc_adpdtk,
    init_vgmstream_rsf,
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
    init_vgmstream_ps2_ads,
    init_vgmstream_nps,
    init_vgmstream_rwsd,
    init_vgmstream_xa,
    init_vgmstream_ps2_rxws,
    init_vgmstream_ps2_rxw,
    init_vgmstream_ngc_dsp_stm,
    init_vgmstream_ps2_exst,
    init_vgmstream_ps2_svag,
    init_vgmstream_mib_mih,
    init_vgmstream_ngc_mpdsp,
    init_vgmstream_ps2_mic,
    init_vgmstream_ngc_dsp_std_int,
    init_vgmstream_vag,
    init_vgmstream_vag_aaap,
    init_vgmstream_seb,
    init_vgmstream_ps2_ild,
    init_vgmstream_ps2_pnb,
    init_vgmstream_ngc_str,
    init_vgmstream_ea_schl,
    init_vgmstream_caf,
    init_vgmstream_vpk,
    init_vgmstream_genh,
#ifdef VGM_USE_VORBIS
    init_vgmstream_ogg_vorbis,
#endif
    init_vgmstream_sli_ogg,
    init_vgmstream_sfl_ogg,
#if 0
    init_vgmstream_mp4_aac,
#endif
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    init_vgmstream_akb_mp4,
#endif
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
    init_vgmstream_ikm_ps2,
    init_vgmstream_ikm_pc,
    init_vgmstream_ikm_psp,
    init_vgmstream_sfs,
    init_vgmstream_bg00,
    init_vgmstream_sat_dvi,
    init_vgmstream_dc_kcey,
    init_vgmstream_ps2_rstm,
    init_vgmstream_acm,
    init_vgmstream_mus_acm,
    init_vgmstream_ps2_kces,
    init_vgmstream_ps2_dxh,
    init_vgmstream_vsv,
    init_vgmstream_scd_pcm,
    init_vgmstream_ps2_pcm,
    init_vgmstream_ps2_rkv,
    init_vgmstream_ps2_vas,
    init_vgmstream_ps2_vas_container,
    init_vgmstream_ps2_tec,
    init_vgmstream_ps2_enth,
    init_vgmstream_sdt,
    init_vgmstream_aix,
    init_vgmstream_ngc_tydsp,
    init_vgmstream_capdsp,
    init_vgmstream_xbox_wvs,
    init_vgmstream_ngc_wvs,
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
    init_vgmstream_ps2_ccc,
    init_vgmstream_fag,
    init_vgmstream_ps2_mihb,
    init_vgmstream_ngc_pdt_split,
    init_vgmstream_ngc_pdt,
    init_vgmstream_wii_mus,
    init_vgmstream_dc_asd,
    init_vgmstream_naomi_spsd,
    init_vgmstream_rsd,
    init_vgmstream_bgw,
    init_vgmstream_spw,
    init_vgmstream_ps2_ass,
    init_vgmstream_ubi_jade,
    init_vgmstream_ubi_jade_container,
    init_vgmstream_seg,
    init_vgmstream_nds_strm_ffta2,
    init_vgmstream_str_asr,
    init_vgmstream_zwdsp,
    init_vgmstream_gca,
    init_vgmstream_spt_spd,
    init_vgmstream_ish_isd,
    init_vgmstream_gsp_gsb,
    init_vgmstream_ydsp,
    init_vgmstream_msvp,
    init_vgmstream_ngc_ssm,
    init_vgmstream_ps2_joe,
    init_vgmstream_vgs,
    init_vgmstream_dcs_wav,
    init_vgmstream_mul,
    init_vgmstream_thp,
    init_vgmstream_wii_sts,
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
    init_vgmstream_ads,
    init_vgmstream_ps2_mcg,
    init_vgmstream_zsd,
    init_vgmstream_ps2_vgs,
    init_vgmstream_redspark,
    init_vgmstream_ivaud,
    init_vgmstream_wii_wsd,
    init_vgmstream_wii_ndp,
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
    init_vgmstream_ngc_gcub,
    init_vgmstream_maxis_xa,
    init_vgmstream_ngc_sck_dsp,
    init_vgmstream_apple_caff,
    init_vgmstream_pc_mxst,
    init_vgmstream_sab,
    init_vgmstream_exakt_sc,
    init_vgmstream_wii_bns,
    init_vgmstream_wii_was,
    init_vgmstream_pona_3do,
    init_vgmstream_pona_psx,
    init_vgmstream_xbox_hlwav,
    init_vgmstream_myspd,
    init_vgmstream_his,
    init_vgmstream_ps2_ast,
    init_vgmstream_dmsg,
    init_vgmstream_ngc_dsp_aaap,
    init_vgmstream_ngc_dsp_konami,
    init_vgmstream_ps2_ster,
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
    init_vgmstream_ps2_adm,
    init_vgmstream_ps2_lpcm,
    init_vgmstream_dsp_bdsp,
    init_vgmstream_ps2_vms,
    init_vgmstream_xau,
    init_vgmstream_bar,
    init_vgmstream_ffw,
    init_vgmstream_dsp_dspw,
    init_vgmstream_jstm,
    init_vgmstream_xvag,
    init_vgmstream_ps3_cps,
    init_vgmstream_sqex_scd,
    init_vgmstream_ngc_nst_dsp,
    init_vgmstream_baf,
    init_vgmstream_baf_badrip,
    init_vgmstream_msf,
    init_vgmstream_ps3_past,
    init_vgmstream_sgxd,
    init_vgmstream_ngca,
    init_vgmstream_wii_ras,
    init_vgmstream_ps2_spm,
    init_vgmstream_x360_tra,
    init_vgmstream_ps2_iab,
    init_vgmstream_vs_str,
    init_vgmstream_lsf_n1nj4n,
    init_vgmstream_vawx,
    init_vgmstream_ps2_wmus,
    init_vgmstream_hyperscan_kvag,
    init_vgmstream_ios_psnd,
    init_vgmstream_pc_adp_bos,
    init_vgmstream_pc_adp_otns,
    init_vgmstream_eb_sfx,
    init_vgmstream_eb_sf0,
    init_vgmstream_mtaf,
    init_vgmstream_tun,
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
    init_vgmstream_ktss,
    init_vgmstream_hca,
    init_vgmstream_ps2_svag_snk,
    init_vgmstream_ps2_vds_vdm,
    init_vgmstream_x360_cxs,
    init_vgmstream_dsp_adx,
    init_vgmstream_akb,
    init_vgmstream_akb2,
#ifdef VGM_USE_FFMPEG
    init_vgmstream_mp4_aac_ffmpeg,
#endif
    init_vgmstream_bik,
    init_vgmstream_x360_ast,
    init_vgmstream_wwise,
    init_vgmstream_ubi_raki,
    init_vgmstream_x360_pasx,
    init_vgmstream_xma,
    init_vgmstream_sxd,
    init_vgmstream_ogl,
    init_vgmstream_mc3,
    init_vgmstream_gtd,
    init_vgmstream_ta_aac_x360,
    init_vgmstream_ta_aac_ps3,
    init_vgmstream_ta_aac_mobile,
    init_vgmstream_ta_aac_mobile_vorbis,
    init_vgmstream_ta_aac_vita,
    init_vgmstream_va3,
    init_vgmstream_mta2,
    init_vgmstream_mta2_container,
    init_vgmstream_ngc_ulw,
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
    init_vgmstream_opus_sps_n1_segmented,
    init_vgmstream_ubi_bao_pk,
    init_vgmstream_ubi_bao_atomic,
    init_vgmstream_dsp_switch_audio,
    init_vgmstream_sadf,
    init_vgmstream_h4m,
    init_vgmstream_ps2_ads_container,
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
    init_vgmstream_scd_sscf,
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
    init_vgmstream_mzrt,
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
    init_vgmstream_xmv_valve,
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
    init_vgmstream_tgc,
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

    /* lowest priority metas (should go after all metas, and TXTH should go before raw formats) */
    init_vgmstream_txth,            /* proper parsers should supersede TXTH, once added */
    init_vgmstream_encrypted,       /* encrypted stuff */
    init_vgmstream_raw_int,         /* .int raw PCM */
    init_vgmstream_ps_headerless,   /* tries to detect a bunch of PS-ADPCM formats */
    init_vgmstream_raw_snds,        /* .snds raw SNDS IMA (*after* ps_headerless) */
    init_vgmstream_raw_wavm,        /* .wavm raw xbox */
    init_vgmstream_raw_pcm,         /* .raw raw PCM */
    init_vgmstream_s14_sss,         /* .s14/sss raw siren14 */
    init_vgmstream_raw_al,          /* .al/al2 raw A-LAW */
#ifdef VGM_USE_FFMPEG
    init_vgmstream_ffmpeg,          /* may play anything incorrectly, since FFmpeg doesn't check extensions */
#endif
};


/* internal version with all parameters */
static VGMSTREAM * init_vgmstream_internal(STREAMFILE *streamFile) {
    int i, fcns_size;
    
    if (!streamFile)
        return NULL;

    fcns_size = (sizeof(init_vgmstream_functions)/sizeof(init_vgmstream_functions[0]));
    /* try a series of formats, see which works */
    for (i = 0; i < fcns_size; i++) {
        /* call init function and see if valid VGMSTREAM was returned */
        VGMSTREAM * vgmstream = (init_vgmstream_functions[i])(streamFile);
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
            try_dual_file_stereo(vgmstream, streamFile, init_vgmstream_functions[i]);
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
            ffmpeg_codec_data *data = (ffmpeg_codec_data *) vgmstream->codec_data;
            if (data && data->streamCount && !vgmstream->num_streams) {
                vgmstream->num_streams = data->streamCount;
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
            vgmstream->stream_index = streamFile->stream_index;
        }


        setup_vgmstream(vgmstream); /* final setup */

        return vgmstream;
    }

    /* not supported */
    return NULL;
}

void setup_vgmstream(VGMSTREAM * vgmstream) {

    /* save start things so we can restart when seeking */
    memcpy(vgmstream->start_ch, vgmstream->ch, sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
    memcpy(vgmstream->start_vgmstream, vgmstream, sizeof(VGMSTREAM));

    /* layout's sub-VGMSTREAM are expected to setup externally and maybe call this,
     * as they can be created using init_vgmstream or manually */
}


/* format detection and VGMSTREAM setup, uses default parameters */
VGMSTREAM * init_vgmstream(const char * const filename) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *streamFile = open_stdio_streamfile(filename);
    if (streamFile) {
        vgmstream = init_vgmstream_from_STREAMFILE(streamFile);
        close_streamfile(streamFile);
    }
    return vgmstream;
}

VGMSTREAM * init_vgmstream_from_STREAMFILE(STREAMFILE *streamFile) {
    return init_vgmstream_internal(streamFile);
}

/* Reset a VGMSTREAM to its state at the start of playback (when a plugin seeks back to zero). */
void reset_vgmstream(VGMSTREAM * vgmstream) {

    /* reset the VGMSTREAM and channels back to their original state */
    memcpy(vgmstream, vgmstream->start_vgmstream, sizeof(VGMSTREAM));
    memcpy(vgmstream->ch, vgmstream->start_ch, sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
    /* loop_ch is not reset here because there is a possibility of the
     * init_vgmstream_* function doing something tricky and precomputing it.
     * Otherwise hit_loop will be 0 and it will be copied over anyway when we
     * really hit the loop start. */

    /* reset custom codec */
#ifdef VGM_USE_VORBIS
    if (vgmstream->coding_type == coding_OGG_VORBIS) {
        reset_ogg_vorbis(vgmstream);
    }

    if (vgmstream->coding_type == coding_VORBIS_custom) {
        reset_vorbis_custom(vgmstream);
    }
#endif

    if (vgmstream->coding_type == coding_CIRCUS_VQ) {
        reset_circus_vq(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_RELIC) {
        reset_relic(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_CRI_HCA) {
        reset_hca(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_UBI_ADPCM) {
        reset_ubi_adpcm(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_IMUSE) {
        reset_imuse(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_EA_MT) {
        reset_ea_mt(vgmstream);
    }

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type == coding_MP4_AAC) {
        reset_mp4_aac(vgmstream);
    }
#endif

#ifdef VGM_USE_MPEG
    if (vgmstream->coding_type == coding_MPEG_custom ||
        vgmstream->coding_type == coding_MPEG_ealayer3 ||
        vgmstream->coding_type == coding_MPEG_layer1 ||
        vgmstream->coding_type == coding_MPEG_layer2 ||
        vgmstream->coding_type == coding_MPEG_layer3) {
        reset_mpeg(vgmstream);
    }
#endif

#ifdef VGM_USE_G7221
    if (vgmstream->coding_type == coding_G7221C) {
        reset_g7221(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_G719
    if (vgmstream->coding_type == coding_G719) {
        reset_g719(vgmstream->codec_data, vgmstream->channels);
    }
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
    if (vgmstream->coding_type == coding_AT3plus) {
        reset_at3plus(vgmstream);
    }
#endif

#ifdef VGM_USE_ATRAC9
    if (vgmstream->coding_type == coding_ATRAC9) {
        reset_atrac9(vgmstream);
    }
#endif

#ifdef VGM_USE_CELT
    if (vgmstream->coding_type == coding_CELT_FSB) {
        reset_celt_fsb(vgmstream);
    }
#endif

#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type == coding_FFmpeg) {
        reset_ffmpeg(vgmstream);
    }
#endif

    if (vgmstream->coding_type == coding_ACM) {
        reset_acm(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_NWA) {
        nwa_codec_data *data = vgmstream->codec_data;
        if (data) reset_nwa(data->nwa);
    }

    /* reset custom layouts */
    if (vgmstream->layout_type == layout_segmented) {
        reset_layout_segmented(vgmstream->layout_data);
    }

    if (vgmstream->layout_type == layout_layered) {
        reset_layout_layered(vgmstream->layout_data);
    }

    /* note that this does not reset the constituent STREAMFILES
     * (vgmstream->ch[N].streamfiles' internal state, though shouldn't matter) */
}

/* Allocate memory and setup a VGMSTREAM */
VGMSTREAM * allocate_vgmstream(int channel_count, int loop_flag) {
    VGMSTREAM * vgmstream;

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

    vgmstream->channels = channel_count;
    vgmstream->loop_flag = loop_flag;

    mixing_init(vgmstream); /* pre-init */

    //vgmstream->stream_name_size = STREAM_NAME_SIZE;
    return vgmstream;
fail:
    if (vgmstream) {
        mixing_close(vgmstream);
        free(vgmstream->ch);
        free(vgmstream->start_ch);
        free(vgmstream->loop_ch);
        free(vgmstream->start_vgmstream);
    }
    free(vgmstream);
    return NULL;
}

void close_vgmstream(VGMSTREAM * vgmstream) {
    if (!vgmstream)
        return;

    /* free custom codecs */
#ifdef VGM_USE_VORBIS
    if (vgmstream->coding_type == coding_OGG_VORBIS) {
        free_ogg_vorbis(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }

    if (vgmstream->coding_type == coding_VORBIS_custom) {
        free_vorbis_custom(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }
#endif

    if (vgmstream->coding_type == coding_CIRCUS_VQ) {
        free_circus_vq(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }

    if (vgmstream->coding_type == coding_RELIC) {
        free_relic(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }

    if (vgmstream->coding_type == coding_CRI_HCA) {
        free_hca(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }

    if (vgmstream->coding_type == coding_UBI_ADPCM) {
        free_ubi_adpcm(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }

    if (vgmstream->coding_type == coding_IMUSE) {
        free_imuse(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }

    if (vgmstream->coding_type == coding_EA_MT) {
        free_ea_mt(vgmstream->codec_data, vgmstream->channels);
        vgmstream->codec_data = NULL;
    }

#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type == coding_FFmpeg) {
        free_ffmpeg(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }
#endif

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type == coding_MP4_AAC) {
        free_mp4_aac(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }
#endif

#ifdef VGM_USE_MPEG
    if (vgmstream->coding_type == coding_MPEG_custom ||
        vgmstream->coding_type == coding_MPEG_ealayer3 ||
        vgmstream->coding_type == coding_MPEG_layer1 ||
        vgmstream->coding_type == coding_MPEG_layer2 ||
        vgmstream->coding_type == coding_MPEG_layer3) {
        free_mpeg(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }
#endif

#ifdef VGM_USE_G7221
    if (vgmstream->coding_type == coding_G7221C) {
        free_g7221(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }
#endif

#ifdef VGM_USE_G719
    if (vgmstream->coding_type == coding_G719) {
        free_g719(vgmstream->codec_data, vgmstream->channels);
        vgmstream->codec_data = NULL;
    }
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
    if (vgmstream->coding_type == coding_AT3plus) {
        free_at3plus(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }
#endif

#ifdef VGM_USE_ATRAC9
    if (vgmstream->coding_type == coding_ATRAC9) {
        free_atrac9(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }
#endif

#ifdef VGM_USE_CELT
    if (vgmstream->coding_type == coding_CELT_FSB) {
        free_celt_fsb(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }
#endif

    if (vgmstream->coding_type == coding_ACM) {
        free_acm(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }

    if (vgmstream->coding_type == coding_NWA) {
        if (vgmstream->codec_data) {
            nwa_codec_data *data = (nwa_codec_data *) vgmstream->codec_data;
            if (data->nwa)
                close_nwa(data->nwa);
            free(data);
            vgmstream->codec_data = NULL;
        }
    }


    /* free custom layouts */
    if (vgmstream->layout_type == layout_segmented) {
        free_layout_segmented(vgmstream->layout_data);
        vgmstream->layout_data = NULL;
    }

    if (vgmstream->layout_type == layout_layered) {
        free_layout_layered(vgmstream->layout_data);
        vgmstream->layout_data = NULL;
    }


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
    free(vgmstream->ch);
    free(vgmstream->start_ch);
    free(vgmstream->loop_ch);
    free(vgmstream->start_vgmstream);
    free(vgmstream);
}

/* calculate samples based on player's config */
int32_t get_vgmstream_play_samples(double looptimes, double fadeseconds, double fadedelayseconds, VGMSTREAM * vgmstream) {
    if (vgmstream->loop_flag) {
        if (vgmstream->loop_target == (int)looptimes) { /* set externally, as this function is info-only */
            /* Continue playing the file normally after looping, instead of fading.
             * Most files cut abruply after the loop, but some do have proper endings.
             * With looptimes = 1 this option should give the same output vs loop disabled */
            int loop_count = (int)looptimes; /* no half loops allowed */
            return vgmstream->loop_start_sample
                + (vgmstream->loop_end_sample - vgmstream->loop_start_sample) * loop_count
                + (vgmstream->num_samples - vgmstream->loop_end_sample);
        }
        else {
            return vgmstream->loop_start_sample
                + (vgmstream->loop_end_sample - vgmstream->loop_start_sample) * looptimes
                + (fadedelayseconds + fadeseconds) * vgmstream->sample_rate;
        }
    }
    else {
        return vgmstream->num_samples;
    }
}

void vgmstream_force_loop(VGMSTREAM* vgmstream, int loop_flag, int loop_start_sample, int loop_end_sample) {
    if (!vgmstream) return;

    /* this requires a bit more messing with the VGMSTREAM than I'm comfortable with... */
    if (loop_flag && !vgmstream->loop_flag && !vgmstream->loop_ch) {
        vgmstream->loop_ch = calloc(vgmstream->channels,sizeof(VGMSTREAMCHANNEL));
        if (!vgmstream->loop_ch) loop_flag = 0; /* ??? */
    }
    else if (!loop_flag && vgmstream->loop_flag) {
        free(vgmstream->loop_ch); /* not important though */
        vgmstream->loop_ch = NULL;
    }

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
        layered_layout_data *data = vgmstream->layout_data;
        for (i = 0; i < data->layer_count; i++) {
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


/* Decode data into sample buffer */
void render_vgmstream(sample_t * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    switch (vgmstream->layout_type) {
        case layout_interleave:
            render_vgmstream_interleave(buffer,sample_count,vgmstream);
            break;
        case layout_none:
            render_vgmstream_flat(buffer,sample_count,vgmstream);
            break;
        case layout_blocked_mxch:
        case layout_blocked_ast:
        case layout_blocked_halpst:
        case layout_blocked_xa:
        case layout_blocked_ea_schl:
        case layout_blocked_ea_1snh:
        case layout_blocked_caf:
        case layout_blocked_wsi:
        case layout_blocked_str_snds:
        case layout_blocked_ws_aud:
        case layout_blocked_matx:
        case layout_blocked_dec:
        case layout_blocked_vs:
        case layout_blocked_mul:
        case layout_blocked_gsb:
        case layout_blocked_xvas:
        case layout_blocked_thp:
        case layout_blocked_filp:
        case layout_blocked_ivaud:
        case layout_blocked_ea_swvr:
        case layout_blocked_adm:
        case layout_blocked_bdsp:
        case layout_blocked_tra:
        case layout_blocked_ps2_iab:
        case layout_blocked_vs_str:
        case layout_blocked_rws:
        case layout_blocked_hwas:
        case layout_blocked_ea_sns:
        case layout_blocked_awc:
        case layout_blocked_vgs:
        case layout_blocked_vawx:
        case layout_blocked_xvag_subsong:
        case layout_blocked_ea_wve_au00:
        case layout_blocked_ea_wve_ad10:
        case layout_blocked_sthd:
        case layout_blocked_h4m:
        case layout_blocked_xa_aiff:
        case layout_blocked_vs_square:
        case layout_blocked_vid1:
        case layout_blocked_ubi_sce:
            render_vgmstream_blocked(buffer,sample_count,vgmstream);
            break;
        case layout_segmented:
            render_vgmstream_segmented(buffer,sample_count,vgmstream);
            break;
        case layout_layered:
            render_vgmstream_layered(buffer,sample_count,vgmstream);
            break;
        default:
            break;
    }

    mix_vgmstream(buffer, sample_count, vgmstream);
}

/* Get the number of samples of a single frame (smallest self-contained sample group, 1/N channels) */
int get_vgmstream_samples_per_frame(VGMSTREAM * vgmstream) {
    /* Value returned here is the max (or less) that vgmstream will ask a decoder per
     * "decode_x" call. Decoders with variable samples per frame or internal discard
     * may return 0 here and handle arbitrary samples_to_do values internally
     * (or some internal sample buffer max too). */

    switch (vgmstream->coding_type) {
        case coding_CRI_ADX:
        case coding_CRI_ADX_fixed:
        case coding_CRI_ADX_exp:
        case coding_CRI_ADX_enc_8:
        case coding_CRI_ADX_enc_9:
            return (vgmstream->interleave_block_size - 2) * 2;

        case coding_NGC_DSP:
        case coding_NGC_DSP_subint:
            return 14;
        case coding_NGC_AFC:
        case coding_VADPCM:
            return 16;
        case coding_NGC_DTK:
            return 28;
        case coding_G721:
            return 1;

        case coding_PCM16LE:
        case coding_PCM16BE:
        case coding_PCM16_int:
        case coding_PCM8:
        case coding_PCM8_int:
        case coding_PCM8_U:
        case coding_PCM8_U_int:
        case coding_PCM8_SB:
        case coding_ULAW:
        case coding_ULAW_int:
        case coding_ALAW:
        case coding_PCMFLOAT:
            return 1;
#ifdef VGM_USE_VORBIS
        case coding_OGG_VORBIS:
        case coding_VORBIS_custom:
#endif
#ifdef VGM_USE_MPEG
        case coding_MPEG_custom:
        case coding_MPEG_ealayer3:
        case coding_MPEG_layer1:
        case coding_MPEG_layer2:
        case coding_MPEG_layer3:
#endif
        case coding_SDX2:
        case coding_SDX2_int:
        case coding_CBD2:
        case coding_ACM:
        case coding_DERF:
        case coding_NWA:
        case coding_SASSC:
        case coding_CIRCUS_ADPCM:
            return 1;

        case coding_IMA:
        case coding_DVI_IMA:
        case coding_SNDS_IMA:
        case coding_OTNS_IMA:
        case coding_UBI_IMA:
        case coding_OKI16:
        case coding_OKI4S:
        case coding_MTF_IMA:
            return 1;
        case coding_PCM4:
        case coding_PCM4_U:
        case coding_IMA_int:
        case coding_DVI_IMA_int:
        case coding_3DS_IMA:
        case coding_WV6_IMA:
        case coding_ALP_IMA:
        case coding_FFTA2_IMA:
        case coding_BLITZ_IMA:
        case coding_PCFX:
            return 2;
        case coding_XBOX_IMA:
        case coding_XBOX_IMA_mch:
        case coding_XBOX_IMA_int:
        case coding_FSB_IMA:
        case coding_WWISE_IMA:
        case coding_CD_IMA:
            return 64;
        case coding_APPLE_IMA4:
            return 64;
        case coding_MS_IMA:
        case coding_REF_IMA:
            return ((vgmstream->interleave_block_size - 0x04*vgmstream->channels) * 2 / vgmstream->channels) + 1;
        case coding_RAD_IMA:
            return (vgmstream->interleave_block_size - 0x04*vgmstream->channels) * 2 / vgmstream->channels;
        case coding_NDS_IMA:
        case coding_DAT4_IMA:
            return (vgmstream->interleave_block_size - 0x04) * 2;
        case coding_AWC_IMA:
            return (0x800 - 0x04) * 2;
        case coding_RAD_IMA_mono:
            return 32;
        case coding_H4M_IMA:
            return 0; /* variable (block-controlled) */

        case coding_XA:
            return 28*8 / vgmstream->channels; /* 8 subframes per frame, mono/stereo */
        case coding_PSX:
        case coding_PSX_badflags:
        case coding_HEVAG:
            return 28;
        case coding_PSX_cfg:
        case coding_PSX_pivotal:
            return (vgmstream->interleave_block_size - 0x01) * 2; /* size 0x01 header */

        case coding_EA_XA:
        case coding_EA_XA_int:
        case coding_EA_XA_V2:
        case coding_MAXIS_XA:
            return 28;
        case coding_EA_XAS_V0:
            return 32;
        case coding_EA_XAS_V1:
            return 128;

        case coding_MSADPCM:
            return (vgmstream->frame_size - 0x07*vgmstream->channels)*2 / vgmstream->channels + 2;
        case coding_MSADPCM_int:
        case coding_MSADPCM_ck:
            return (vgmstream->frame_size - 0x07)*2 + 2;
        case coding_WS: /* only works if output sample size is 8 bit, which always is for WS ADPCM */
            return vgmstream->ws_output_size;
        case coding_AICA:
            return 1;
        case coding_AICA_int:
            return 2;
        case coding_ASKA:
            return (0x40-0x04*vgmstream->channels) * 2 / vgmstream->channels;
        case coding_NXAP:
            return (0x40-0x04) * 2;
        case coding_NDS_PROCYON:
            return 30;
        case coding_L5_555:
            return 32;
        case coding_LSF:
            return 54;

#ifdef VGM_USE_G7221
        case coding_G7221C:
            return 32000/50; /* Siren7: 16000/50 */
#endif
#ifdef VGM_USE_G719
        case coding_G719:
            return 48000/50;
#endif
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
            return 0;
#endif
        case coding_MTAF:
            return 128*2;
        case coding_MTA2:
            return 128*2;
        case coding_MC3:
            return 10;
        case coding_FADPCM:
            return 256; /* (0x8c - 0xc) * 2 */
        case coding_ASF:
            return 32;  /* (0x11 - 0x1) * 2 */
        case coding_DSA:
            return 14;  /* (0x08 - 0x1) * 2 */
        case coding_XMD:
            return (vgmstream->interleave_block_size - 0x06)*2 + 2;
        case coding_PTADPCM:
            return (vgmstream->interleave_block_size - 0x05)*2 + 2;
        case coding_UBI_ADPCM:
            return 0; /* varies per mode */
        case coding_IMUSE:
            return 0; /* varies per frame */
        case coding_EA_MT:
            return 0; /* 432, but variable in looped files */
        case coding_CIRCUS_VQ:
            return 0;
        case coding_RELIC:
            return 0; /* 512 */
        case coding_CRI_HCA:
            return 0; /* 1024 - delay/padding (which can be bigger than 1024) */
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
        case coding_MP4_AAC:
            return ((mp4_aac_codec_data*)vgmstream->codec_data)->samples_per_frame;
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
        case coding_AT3plus:
            return 2048 - ((maiatrac3plus_codec_data*)vgmstream->codec_data)->samples_discard;
#endif
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9:
            return 0; /* varies with config data, usually 256 or 1024 */
#endif
#ifdef VGM_USE_CELT
        case coding_CELT_FSB:
            return 0; /* 512? */
#endif
        default:
            return 0;
    }
}

/* Get the number of bytes of a single frame (smallest self-contained byte group, 1/N channels) */
int get_vgmstream_frame_size(VGMSTREAM * vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_CRI_ADX:
        case coding_CRI_ADX_fixed:
        case coding_CRI_ADX_exp:
        case coding_CRI_ADX_enc_8:
        case coding_CRI_ADX_enc_9:
            return vgmstream->interleave_block_size;

        case coding_NGC_DSP:
            return 0x08;
        case coding_NGC_DSP_subint:
            return 0x08 * vgmstream->channels;
        case coding_NGC_AFC:
        case coding_VADPCM:
            return 0x09;
        case coding_NGC_DTK:
            return 0x20;
        case coding_G721:
            return 0;

        case coding_PCM16LE:
        case coding_PCM16BE:
        case coding_PCM16_int:
            return 0x02;
        case coding_PCM8:
        case coding_PCM8_int:
        case coding_PCM8_U:
        case coding_PCM8_U_int:
        case coding_PCM8_SB:
        case coding_ULAW:
        case coding_ULAW_int:
        case coding_ALAW:
            return 0x01;
        case coding_PCMFLOAT:
            return 0x04;

        case coding_SDX2:
        case coding_SDX2_int:
        case coding_CBD2:
        case coding_DERF:
        case coding_NWA:
        case coding_SASSC:
        case coding_CIRCUS_ADPCM:
            return 0x01;

        case coding_PCM4:
        case coding_PCM4_U:
        case coding_IMA:
        case coding_IMA_int:
        case coding_DVI_IMA:
        case coding_DVI_IMA_int:
        case coding_3DS_IMA:
        case coding_WV6_IMA:
        case coding_ALP_IMA:
        case coding_FFTA2_IMA:
        case coding_BLITZ_IMA:
        case coding_PCFX:
        case coding_OKI16:
        case coding_OKI4S:
        case coding_MTF_IMA:
            return 0x01;
        case coding_MS_IMA:
        case coding_RAD_IMA:
        case coding_NDS_IMA:
        case coding_DAT4_IMA:
        case coding_REF_IMA:
            return vgmstream->interleave_block_size;
        case coding_AWC_IMA:
            return 0x800;
        case coding_RAD_IMA_mono:
            return 0x14;
        case coding_SNDS_IMA:
        case coding_OTNS_IMA:
            return 0; //todo: 0x01?
        case coding_UBI_IMA: /* variable (PCM then IMA) */
            return 0;
        case coding_XBOX_IMA:
            //todo should be  0x48 when stereo, but blocked/interleave layout don't understand stereo codecs
            return 0x24; //vgmstream->channels==1 ? 0x24 : 0x48;
        case coding_XBOX_IMA_int:
        case coding_WWISE_IMA:
        case coding_CD_IMA:
            return 0x24;
        case coding_XBOX_IMA_mch:
        case coding_FSB_IMA:
            return 0x24 * vgmstream->channels;
        case coding_APPLE_IMA4:
            return 0x22;
        case coding_H4M_IMA:
            return 0x00; /* variable (block-controlled) */

        case coding_XA:
            return 0x80;
        case coding_PSX:
        case coding_PSX_badflags:
        case coding_HEVAG:
            return 0x10;
        case coding_PSX_cfg:
        case coding_PSX_pivotal:
            return vgmstream->interleave_block_size;

        case coding_EA_XA:
            return 0x1E;
        case coding_EA_XA_int:
            return 0x0F;
        case coding_MAXIS_XA:
            return 0x0F*vgmstream->channels;
        case coding_EA_XA_V2:
            return 0; /* variable (ADPCM frames of 0x0f or PCM frames of 0x3d) */
        case coding_EA_XAS_V0:
            return 0xF+0x02+0x02;
        case coding_EA_XAS_V1:
            return 0x4c*vgmstream->channels;

        case coding_MSADPCM:
        case coding_MSADPCM_int:
        case coding_MSADPCM_ck:
            return vgmstream->frame_size;
        case coding_WS:
            return vgmstream->current_block_size;
        case coding_AICA:
        case coding_AICA_int:
            return 0x01;
        case coding_ASKA:
        case coding_NXAP:
            return 0x40;
        case coding_NDS_PROCYON:
            return 0x10;
        case coding_L5_555:
            return 0x12;
        case coding_LSF:
            return 0x1C;

#ifdef VGM_USE_G7221
        case coding_G7221C:
#endif
#ifdef VGM_USE_G719
        case coding_G719:
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
        case coding_AT3plus:
#endif
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
#endif
        case coding_MTAF:
            return vgmstream->interleave_block_size;
        case coding_MTA2:
            return 0x90;
        case coding_MC3:
            return 0x04;
        case coding_FADPCM:
            return 0x8c;
        case coding_ASF:
            return 0x11;
        case coding_DSA:
            return 0x08;
        case coding_XMD:
            return vgmstream->interleave_block_size;
        case coding_PTADPCM:
            return vgmstream->interleave_block_size;
        case coding_UBI_ADPCM:
            return 0; /* varies per mode? */
        case coding_IMUSE:
            return 0; /* varies per frame */
        case coding_EA_MT:
            return 0; /* variable (frames of bit counts or PCM frames) */
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9:
            return 0; /* varies with config data, usually 0x100-200 */
#endif
#ifdef VGM_USE_CELT
        case coding_CELT_FSB:
            return 0; /* varies, usually 0x80-100 */
#endif
        default: /* Vorbis, MPEG, ACM, etc */
            return 0;
    }
}

/* In NDS IMA the frame size is the block size, so the last one is short */
int get_vgmstream_samples_per_shortframe(VGMSTREAM * vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_NDS_IMA:
            return (vgmstream->interleave_last_block_size-4)*2;
        default:
            return get_vgmstream_samples_per_frame(vgmstream);
    }
}
int get_vgmstream_shortframe_size(VGMSTREAM * vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_NDS_IMA:
            return vgmstream->interleave_last_block_size;
        default:
            return get_vgmstream_frame_size(vgmstream);
    }
}

/* Decode samples into the buffer. Assume that we have written samples_written into the
 * buffer already, and we have samples_to_do consecutive samples ahead of us. */
void decode_vgmstream(VGMSTREAM * vgmstream, int samples_written, int samples_to_do, sample_t * buffer) {
    int ch;

    switch (vgmstream->coding_type) {
        case coding_CRI_ADX:
        case coding_CRI_ADX_exp:
        case coding_CRI_ADX_fixed:
        case coding_CRI_ADX_enc_8:
        case coding_CRI_ADX_enc_9:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_adx(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do,
                        vgmstream->interleave_block_size, vgmstream->coding_type);
            }
            break;
        case coding_NGC_DSP:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ngc_dsp(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_NGC_DSP_subint:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ngc_dsp_subint(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do,
                        ch, vgmstream->interleave_block_size);
            }
            break;

        case coding_PCM16LE:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm16le(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_PCM16BE:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm16be(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_PCM16_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm16_int(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do,
                        vgmstream->codec_endian);
            }
            break;
        case coding_PCM8:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_PCM8_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8_int(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_PCM8_U:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8_unsigned(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_PCM8_U_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8_unsigned_int(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_PCM8_SB:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8_sb(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_PCM4:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm4(vgmstream,&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do,ch);
            }
            break;
        case coding_PCM4_U:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm4_unsigned(vgmstream, &vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do,ch);
            }
            break;

        case coding_ULAW:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ulaw(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_ULAW_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ulaw_int(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_ALAW:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_alaw(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_PCMFLOAT:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcmfloat(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do,
                        vgmstream->codec_endian);
            }
            break;

        case coding_NDS_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_nds_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_DAT4_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_dat4_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_XBOX_IMA:
        case coding_XBOX_IMA_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                int is_stereo = (vgmstream->channels > 1 && vgmstream->coding_type == coding_XBOX_IMA);
                decode_xbox_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch,
                        is_stereo);
            }
            break;
        case coding_XBOX_IMA_mch:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_xbox_ima_mch(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_MS_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ms_ima(vgmstream,&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_RAD_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_rad_ima(vgmstream,&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_RAD_IMA_mono:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_rad_ima_mono(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_NGC_DTK:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ngc_dtk(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_G721:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_g721(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_NGC_AFC:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ngc_afc(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_VADPCM: {
            int order = vgmstream->codec_config;
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_vadpcm(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, order);
            }
            break;
        }
        case coding_PSX:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_psx(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, 0, vgmstream->codec_config);
            }
            break;
        case coding_PSX_badflags:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_psx(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, 1, vgmstream->codec_config);
            }
            break;
        case coding_PSX_cfg:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_psx_configurable(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, vgmstream->interleave_block_size, vgmstream->codec_config);
            }
            break;
        case coding_PSX_pivotal:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_psx_pivotal(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do,
                        vgmstream->interleave_block_size);
            }
            break;
        case coding_HEVAG:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_hevag(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_XA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_xa(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_EA_XA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_xa(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_EA_XA_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_xa_int(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_EA_XA_V2:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_xa_v2(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_MAXIS_XA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_maxis_xa(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_EA_XAS_V0:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_xas_v0(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_EA_XAS_V1:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_xas_v1(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
#ifdef VGM_USE_VORBIS
        case coding_OGG_VORBIS:
            decode_ogg_vorbis(vgmstream->codec_data, buffer+samples_written*vgmstream->channels,
                    samples_to_do,vgmstream->channels);
            break;

        case coding_VORBIS_custom:
            decode_vorbis_custom(vgmstream, buffer+samples_written*vgmstream->channels,
                    samples_to_do,vgmstream->channels);
            break;
#endif
        case coding_CIRCUS_VQ:
            decode_circus_vq(vgmstream->codec_data, buffer+samples_written*vgmstream->channels, samples_to_do, vgmstream->channels);
            break;
        case coding_RELIC:
            decode_relic(&vgmstream->ch[0], vgmstream->codec_data, buffer+samples_written*vgmstream->channels,
                    samples_to_do);
            break;
        case coding_CRI_HCA:
            decode_hca(vgmstream->codec_data, buffer+samples_written*vgmstream->channels,
                    samples_to_do);
            break;
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
            decode_ffmpeg(vgmstream,
                          buffer+samples_written*vgmstream->channels,samples_to_do,vgmstream->channels);
            break;
#endif
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
        case coding_MP4_AAC:
            decode_mp4_aac(vgmstream->codec_data, buffer+samples_written*vgmstream->channels,
                    samples_to_do,vgmstream->channels);
            break;
#endif
        case coding_SDX2:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_sdx2(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_SDX2_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_sdx2_int(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_CBD2:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_cbd2(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_CBD2_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_cbd2_int(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_DERF:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_derf(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_CIRCUS_ADPCM:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_circus_adpcm(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;

        case coding_IMA:
        case coding_IMA_int:
        case coding_DVI_IMA:
        case coding_DVI_IMA_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                int is_stereo = (vgmstream->channels > 1 && vgmstream->coding_type == coding_IMA)
                        || (vgmstream->channels > 1 && vgmstream->coding_type == coding_DVI_IMA);
                int is_high_first = vgmstream->coding_type == coding_DVI_IMA || vgmstream->coding_type == coding_DVI_IMA_int;

                decode_standard_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch,
                        is_stereo, is_high_first);
            }
            break;
        case coding_MTF_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                int is_stereo = (vgmstream->channels > 1);
                decode_mtf_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch,
                        is_stereo);
            }
            break;
        case coding_3DS_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_3ds_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_WV6_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_wv6_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_ALP_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_alp_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_FFTA2_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ffta2_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_BLITZ_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_blitz_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;

        case coding_APPLE_IMA4:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_apple_ima4(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_SNDS_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_snds_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_OTNS_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_otns_ima(vgmstream, &vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_FSB_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_fsb_ima(vgmstream, &vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_WWISE_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_wwise_ima(vgmstream,&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_REF_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ref_ima(vgmstream,&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_AWC_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_awc_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_UBI_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ubi_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch, vgmstream->codec_config);
            }
            break;
        case coding_H4M_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                uint16_t frame_format = (uint16_t)((vgmstream->codec_config >> 8) & 0xFFFF);

                decode_h4m_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch,
                        frame_format);
            }
            break;
        case coding_CD_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_cd_ima(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;

        case coding_WS:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ws(vgmstream,ch,buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;

#ifdef VGM_USE_MPEG
        case coding_MPEG_custom:
        case coding_MPEG_ealayer3:
        case coding_MPEG_layer1:
        case coding_MPEG_layer2:
        case coding_MPEG_layer3:
            decode_mpeg(vgmstream,buffer+samples_written*vgmstream->channels,
                    samples_to_do,vgmstream->channels);
            break;
#endif
#ifdef VGM_USE_G7221
        case coding_G7221C:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_g7221(vgmstream, buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,samples_to_do, ch);
            }
            break;
#endif
#ifdef VGM_USE_G719
        case coding_G719:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_g719(vgmstream, buffer+samples_written*vgmstream->channels+ch,
                    vgmstream->channels,samples_to_do, ch);
            }
            break;
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
        case coding_AT3plus:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_at3plus(vgmstream, buffer+samples_written*vgmstream->channels+ch,
                    vgmstream->channels,samples_to_do, ch);
            }
            break;
#endif
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9:
            decode_atrac9(vgmstream, buffer+samples_written*vgmstream->channels,
                    samples_to_do,vgmstream->channels);
            break;
#endif
#ifdef VGM_USE_CELT
        case coding_CELT_FSB:
            decode_celt_fsb(vgmstream, buffer+samples_written*vgmstream->channels,
                    samples_to_do,vgmstream->channels);
            break;
#endif
        case coding_ACM:
            decode_acm(vgmstream->codec_data, buffer+samples_written*vgmstream->channels,
                    samples_to_do, vgmstream->channels);
            break;
        case coding_NWA:
            decode_nwa(((nwa_codec_data*)vgmstream->codec_data)->nwa,
                    buffer+samples_written*vgmstream->channels, samples_to_do);
            break;
        case coding_MSADPCM:
        case coding_MSADPCM_int:
            if (vgmstream->channels == 1 || vgmstream->coding_type == coding_MSADPCM_int) {
                for (ch = 0; ch < vgmstream->channels; ch++) {
                    decode_msadpcm_mono(vgmstream,buffer+samples_written*vgmstream->channels+ch,
                            vgmstream->channels,vgmstream->samples_into_block, samples_to_do, ch);
                }
            }
            else if (vgmstream->channels == 2) {
                decode_msadpcm_stereo(vgmstream,buffer+samples_written*vgmstream->channels,
                        vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_MSADPCM_ck:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_msadpcm_ck(vgmstream,buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_AICA:
        case coding_AICA_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                int is_stereo = (vgmstream->channels > 1 && vgmstream->coding_type == coding_AICA);

                decode_aica(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch,
                        is_stereo);
            }
            break;
        case coding_ASKA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_aska(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;
        case coding_NXAP:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_nxap(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_TGC:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_tgc(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_NDS_PROCYON:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_nds_procyon(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_L5_555:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_l5_555(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_SASSC:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_sassc(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }

            break;
        case coding_LSF:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_lsf(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_MTAF:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_mtaf(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_MTA2:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_mta2(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_MC3:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_mc3(vgmstream, &vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_FADPCM:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_fadpcm(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_ASF:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_asf(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_DSA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_dsa(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_XMD:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_xmd(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do,
                        vgmstream->interleave_block_size);
            }
            break;
        case coding_PTADPCM:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ptadpcm(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do,
                        vgmstream->interleave_block_size);
            }
            break;
        case coding_PCFX:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcfx(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, vgmstream->codec_config);
            }
            break;
        case coding_OKI16:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_oki16(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;

        case coding_OKI4S:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_oki4s(&vgmstream->ch[ch],buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels,vgmstream->samples_into_block,samples_to_do, ch);
            }
            break;

        case coding_UBI_ADPCM:
            decode_ubi_adpcm(vgmstream, buffer+samples_written*vgmstream->channels, samples_to_do);
            break;

        case coding_IMUSE:
            decode_imuse(vgmstream, buffer+samples_written*vgmstream->channels, samples_to_do);
            break;

        case coding_EA_MT:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_mt(vgmstream, buffer+samples_written*vgmstream->channels+ch,
                        vgmstream->channels, samples_to_do, ch);
            }
            break;
        default:
            break;
    }
}

/* Calculate number of consecutive samples to do (taking into account stopping for loop start and end) */
int vgmstream_samples_to_do(int samples_this_block, int samples_per_frame, VGMSTREAM * vgmstream) {
    int samples_to_do;
    int samples_left_this_block;

    samples_left_this_block = samples_this_block - vgmstream->samples_into_block;
    samples_to_do = samples_left_this_block;

    /* fun loopy crap */
    /* Why did I think this would be any simpler? */
    if (vgmstream->loop_flag) {
        /* are we going to hit the loop end during this block? */
        if (vgmstream->current_sample + samples_left_this_block > vgmstream->loop_end_sample) {
            /* only do to just before it */
            samples_to_do = vgmstream->loop_end_sample - vgmstream->current_sample;
        }

        /* are we going to hit the loop start during this block? */
        if (!vgmstream->hit_loop && vgmstream->current_sample + samples_left_this_block > vgmstream->loop_start_sample) {
            /* only do to just before it */
            samples_to_do = vgmstream->loop_start_sample - vgmstream->current_sample;
        }

    }

    /* if it's a framed encoding don't do more than one frame */
    if (samples_per_frame > 1 && (vgmstream->samples_into_block % samples_per_frame) + samples_to_do > samples_per_frame)
        samples_to_do = samples_per_frame - (vgmstream->samples_into_block % samples_per_frame);

    return samples_to_do;
}

/* Detect loop start and save values, or detect loop end and restore (loop back). Returns 1 if loop was done. */
int vgmstream_do_loop(VGMSTREAM * vgmstream) {
    /*if (!vgmstream->loop_flag) return 0;*/

    /* is this the loop end? = new loop, continue from loop_start_sample */
    if (vgmstream->current_sample==vgmstream->loop_end_sample) {

        /* disable looping if target count reached and continue normally
         * (only needed with the "play stream end after looping N times" option enabled) */
        vgmstream->loop_count++;
        if (vgmstream->loop_target && vgmstream->loop_target == vgmstream->loop_count) {
            vgmstream->loop_flag = 0; /* could be improved but works ok, will be restored on resets */
            return 0;
        }

        /* against everything I hold sacred, preserve adpcm
         * history through loop for certain types */
        if (vgmstream->meta_type == meta_DSP_STD ||
            vgmstream->meta_type == meta_DSP_RS03 ||
            vgmstream->meta_type == meta_DSP_CSTR ||
            vgmstream->coding_type == coding_PSX ||
            vgmstream->coding_type == coding_PSX_badflags) {
            int i;
            for (i = 0; i < vgmstream->channels; i++) {
                vgmstream->loop_ch[i].adpcm_history1_16 = vgmstream->ch[i].adpcm_history1_16;
                vgmstream->loop_ch[i].adpcm_history2_16 = vgmstream->ch[i].adpcm_history2_16;
                vgmstream->loop_ch[i].adpcm_history1_32 = vgmstream->ch[i].adpcm_history1_32;
                vgmstream->loop_ch[i].adpcm_history2_32 = vgmstream->ch[i].adpcm_history2_32;
            }
        }


        /* prepare certain codecs' internal state for looping */

        if (vgmstream->coding_type == coding_CIRCUS_VQ) {
            seek_circus_vq(vgmstream->codec_data, vgmstream->loop_sample);
        }

        if (vgmstream->coding_type == coding_RELIC) {
            seek_relic(vgmstream->codec_data, vgmstream->loop_sample);
        }

        if (vgmstream->coding_type == coding_CRI_HCA) {
            loop_hca(vgmstream->codec_data, vgmstream->loop_sample);
        }

        if (vgmstream->coding_type == coding_UBI_ADPCM) {
            seek_ubi_adpcm(vgmstream->codec_data, vgmstream->loop_sample);
        }

        if (vgmstream->coding_type == coding_IMUSE) {
            seek_imuse(vgmstream->codec_data, vgmstream->loop_sample);
        }

        if (vgmstream->coding_type == coding_EA_MT) {
            seek_ea_mt(vgmstream, vgmstream->loop_sample);
        }

#ifdef VGM_USE_VORBIS
        if (vgmstream->coding_type == coding_OGG_VORBIS) {
            seek_ogg_vorbis(vgmstream, vgmstream->loop_sample);
        }

        if (vgmstream->coding_type == coding_VORBIS_custom) {
            seek_vorbis_custom(vgmstream, vgmstream->loop_sample);
        }
#endif

#ifdef VGM_USE_FFMPEG
        if (vgmstream->coding_type == coding_FFmpeg) {
            seek_ffmpeg(vgmstream, vgmstream->loop_sample);
        }
#endif

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
        if (vgmstream->coding_type == coding_MP4_AAC) {
            seek_mp4_aac(vgmstream, vgmstream->loop_sample);
        }
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
        if (vgmstream->coding_type == coding_AT3plus) {
            seek_at3plus(vgmstream, vgmstream->loop_sample);
        }
#endif

#ifdef VGM_USE_ATRAC9
        if (vgmstream->coding_type == coding_ATRAC9) {
            seek_atrac9(vgmstream, vgmstream->loop_sample);
        }
#endif

#ifdef VGM_USE_CELT
        if (vgmstream->coding_type == coding_CELT_FSB) {
            seek_celt_fsb(vgmstream, vgmstream->loop_sample);
        }
#endif

#ifdef VGM_USE_MPEG
        if (vgmstream->coding_type == coding_MPEG_custom ||
            vgmstream->coding_type == coding_MPEG_ealayer3 ||
            vgmstream->coding_type == coding_MPEG_layer1 ||
            vgmstream->coding_type == coding_MPEG_layer2 ||
            vgmstream->coding_type == coding_MPEG_layer3) {
            seek_mpeg(vgmstream, vgmstream->loop_sample);
        }
#endif

        if (vgmstream->coding_type == coding_NWA) {
            nwa_codec_data *data = vgmstream->codec_data;
            if (data)
                seek_nwa(data->nwa, vgmstream->loop_sample);
        }

        /* restore! */
        memcpy(vgmstream->ch, vgmstream->loop_ch, sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
        vgmstream->current_sample = vgmstream->loop_sample;
        vgmstream->samples_into_block = vgmstream->loop_samples_into_block;
        vgmstream->current_block_size = vgmstream->loop_block_size;
        vgmstream->current_block_samples = vgmstream->loop_block_samples;
        vgmstream->current_block_offset = vgmstream->loop_block_offset;
        vgmstream->next_block_offset = vgmstream->loop_next_block_offset;

        return 1; /* looped */
    }


    /* is this the loop start? */
    if (!vgmstream->hit_loop && vgmstream->current_sample == vgmstream->loop_start_sample) {
        /* save! */
        memcpy(vgmstream->loop_ch, vgmstream->ch, sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
        vgmstream->loop_sample = vgmstream->current_sample;
        vgmstream->loop_samples_into_block = vgmstream->samples_into_block;
        vgmstream->loop_block_size = vgmstream->current_block_size;
        vgmstream->loop_block_samples = vgmstream->current_block_samples;
        vgmstream->loop_block_offset = vgmstream->current_block_offset;
        vgmstream->loop_next_block_offset = vgmstream->next_block_offset;
        vgmstream->hit_loop = 1;
    }

    return 0; /* not looped */
}

/* Write a description of the stream into array pointed by desc, which must be length bytes long.
 * Will always be null-terminated if length > 0 */
void describe_vgmstream(VGMSTREAM * vgmstream, char * desc, int length) {
#define TEMPSIZE (256+32)
    char temp[TEMPSIZE];
    double time_mm, time_ss, seconds;

    if (!vgmstream) {
        snprintf(temp,TEMPSIZE, "NULL VGMSTREAM");
        concatn(length,desc,temp);
        return;
    }

    snprintf(temp,TEMPSIZE, "sample rate: %d Hz\n", vgmstream->sample_rate);
    concatn(length,desc,temp);

    snprintf(temp,TEMPSIZE, "channels: %d\n", vgmstream->channels);
    concatn(length,desc,temp);

    {
        int output_channels = 0;
        mixing_info(vgmstream, NULL, &output_channels);

        if (output_channels != vgmstream->channels) {
            snprintf(temp,TEMPSIZE, "input channels: %d\n", vgmstream->channels); /* repeated but mainly for plugins */
            concatn(length,desc,temp);
            snprintf(temp,TEMPSIZE, "output channels: %d\n", output_channels);
            concatn(length,desc,temp);
        }
    }

    if (vgmstream->channel_layout) {
        int cl = vgmstream->channel_layout;

        /* not "channel layout: " to avoid mixups with "layout: " */
        snprintf(temp,TEMPSIZE, "channel mask: 0x%x /", vgmstream->channel_layout);
        concatn(length,desc,temp);
        if (cl & speaker_FL)    concatn(length,desc," FL");
        if (cl & speaker_FR)    concatn(length,desc," FR");
        if (cl & speaker_FC)    concatn(length,desc," FC");
        if (cl & speaker_LFE)   concatn(length,desc," LFE");
        if (cl & speaker_BL)    concatn(length,desc," BL");
        if (cl & speaker_BR)    concatn(length,desc," BR");
        if (cl & speaker_FLC)   concatn(length,desc," FLC");
        if (cl & speaker_FRC)   concatn(length,desc," FRC");
        if (cl & speaker_BC)    concatn(length,desc," BC");
        if (cl & speaker_SL)    concatn(length,desc," SL");
        if (cl & speaker_SR)    concatn(length,desc," SR");
        if (cl & speaker_TC)    concatn(length,desc," TC");
        if (cl & speaker_TFL)   concatn(length,desc," TFL");
        if (cl & speaker_TFC)   concatn(length,desc," TFC");
        if (cl & speaker_TFR)   concatn(length,desc," TFR");
        if (cl & speaker_TBL)   concatn(length,desc," TBL");
        if (cl & speaker_TBC)   concatn(length,desc," TBC");
        if (cl & speaker_TBR)   concatn(length,desc," TBR");
        concatn(length,desc,"\n");
    }

    if (vgmstream->loop_start_sample >= 0 && vgmstream->loop_end_sample > vgmstream->loop_start_sample) {
        if (!vgmstream->loop_flag) {
            concatn(length,desc,"looping: disabled\n");
        }

        seconds = (double)vgmstream->loop_start_sample / vgmstream->sample_rate;
        time_mm = (int)(seconds / 60.0);
        time_ss = seconds - time_mm * 60.0f;
        snprintf(temp,TEMPSIZE, "loop start: %d samples (%1.0f:%06.3f seconds)\n", vgmstream->loop_start_sample, time_mm, time_ss);
        concatn(length,desc,temp);

        seconds = (double)vgmstream->loop_end_sample / vgmstream->sample_rate;
        time_mm = (int)(seconds / 60.0);
        time_ss = seconds - time_mm * 60.0f;
        snprintf(temp,TEMPSIZE, "loop end: %d samples (%1.0f:%06.3f seconds)\n", vgmstream->loop_end_sample, time_mm, time_ss);
        concatn(length,desc,temp);
    }

    seconds = (double)vgmstream->num_samples / vgmstream->sample_rate;
    time_mm = (int)(seconds / 60.0);
    time_ss = seconds - time_mm * 60.0;
    snprintf(temp,TEMPSIZE, "stream total samples: %d (%1.0f:%06.3f seconds)\n", vgmstream->num_samples, time_mm, time_ss);
    concatn(length,desc,temp);

    snprintf(temp,TEMPSIZE, "encoding: ");
    concatn(length,desc,temp);
    get_vgmstream_coding_description(vgmstream, temp, TEMPSIZE);
    concatn(length,desc,temp);
    concatn(length,desc,"\n");

    snprintf(temp,TEMPSIZE, "layout: ");
    concatn(length,desc,temp);
    get_vgmstream_layout_description(vgmstream, temp, TEMPSIZE);
    concatn(length, desc, temp);
    concatn(length,desc,"\n");

    if (vgmstream->layout_type == layout_interleave && vgmstream->channels > 1) {
        snprintf(temp,TEMPSIZE, "interleave: %#x bytes\n", (int32_t)vgmstream->interleave_block_size);
        concatn(length,desc,temp);

        if (vgmstream->interleave_first_block_size && vgmstream->interleave_first_block_size != vgmstream->interleave_block_size) {
            snprintf(temp,TEMPSIZE, "interleave first block: %#x bytes\n", (int32_t)vgmstream->interleave_first_block_size);
            concatn(length,desc,temp);
        }

        if (vgmstream->interleave_last_block_size && vgmstream->interleave_last_block_size != vgmstream->interleave_block_size) {
            snprintf(temp,TEMPSIZE, "interleave last block: %#x bytes\n", (int32_t)vgmstream->interleave_last_block_size);
            concatn(length,desc,temp);
        }
    }

    /* codecs with configurable frame size */
    if (vgmstream->frame_size > 0 || vgmstream->interleave_block_size > 0) {
        int32_t frame_size = vgmstream->frame_size > 0 ? vgmstream->frame_size : vgmstream->interleave_block_size;
        switch (vgmstream->coding_type) {
            case coding_MSADPCM:
            case coding_MSADPCM_int:
            case coding_MSADPCM_ck:
            case coding_MS_IMA:
            case coding_MC3:
            case coding_WWISE_IMA:
            case coding_REF_IMA:
            case coding_PSX_cfg:
                snprintf(temp,TEMPSIZE, "frame size: %#x bytes\n", frame_size);
                concatn(length,desc,temp);
                break;
            default:
                break;
        }
    }

    snprintf(temp,TEMPSIZE, "metadata from: ");
    concatn(length,desc,temp);
    get_vgmstream_meta_description(vgmstream, temp, TEMPSIZE);
    concatn(length,desc,temp);
    concatn(length,desc,"\n");

    snprintf(temp,TEMPSIZE, "bitrate: %d kbps\n", get_vgmstream_average_bitrate(vgmstream) / 1000); //todo \n?
    concatn(length,desc,temp);

    /* only interesting if more than one */
    if (vgmstream->num_streams > 1) {
        snprintf(temp,TEMPSIZE, "stream count: %d\n", vgmstream->num_streams);
        concatn(length,desc,temp);
    }

    if (vgmstream->num_streams > 1) {
        snprintf(temp,TEMPSIZE, "stream index: %d\n", vgmstream->stream_index == 0 ? 1 : vgmstream->stream_index);
        concatn(length,desc,temp);
    }

    if (vgmstream->stream_name[0] != '\0') {
        snprintf(temp,TEMPSIZE, "stream name: %s\n", vgmstream->stream_name);
        concatn(length,desc,temp);
    }
}


/* See if there is a second file which may be the second channel, given an already opened mono vgmstream.
 * If a suitable file is found, open it and change opened_vgmstream to a stereo vgmstream. */
static void try_dual_file_stereo(VGMSTREAM * opened_vgmstream, STREAMFILE *streamFile, VGMSTREAM*(*init_vgmstream_function)(STREAMFILE *)) {
    /* filename search pairs for dual file stereo */
    static const char * const dfs_pairs[][2] = {
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
    char * extension;
    int dfs_pair = -1; /* -1=no stereo, 0=opened_vgmstream is left, 1=opened_vgmstream is right */
    VGMSTREAM *new_vgmstream = NULL;
    STREAMFILE *dual_streamFile = NULL;
    int i,j, dfs_pair_count, extension_len, filename_len;

    if (opened_vgmstream->channels != 1)
        return;

    /* custom codec/layouts aren't designed for this (should never get here anyway) */
    if (opened_vgmstream->codec_data || opened_vgmstream->layout_data)
        return;

    //todo other layouts work but some stereo codecs do weird things
    //if (opened_vgmstream->layout != layout_none) return;

    get_streamfile_name(streamFile, new_filename, sizeof(new_filename));
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
            const char * this_suffix = dfs_pairs[i][j];
            const char * that_suffix = dfs_pairs[i][j^1];
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
                dual_streamFile = open_streamfile(streamFile, new_filename);
                if (!dual_streamFile) {
                    /* restore filename and keep trying (if found it'll break and init) */
                    dfs_pair = -1;
                    get_streamfile_name(streamFile, new_filename, sizeof(new_filename));
                }
            }
        }
    }

    /* filename didn't have a suitable L/R-pair name */
    if (dfs_pair == -1)
        goto fail;
    //;VGM_LOG("DFS: match %i filename=%s\n", dfs_pair, new_filename);

    new_vgmstream = init_vgmstream_function(dual_streamFile); /* use the init function that just worked */
    close_streamfile(dual_streamFile);

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
        VGMSTREAMCHANNEL * new_chans;
        VGMSTREAMCHANNEL * new_loop_chans = NULL;
        VGMSTREAMCHANNEL * new_start_chans = NULL;

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

        /* discard the second VGMSTREAM */
        mixing_close(new_vgmstream);
        free(new_vgmstream->start_vgmstream);
        free(new_vgmstream);

        mixing_update_channel(opened_vgmstream); /* notify of new channel hacked-in */
    }

    return;
fail:
    close_vgmstream(new_vgmstream);
    return;
}

/* average bitrate helper to get STREAMFILE for a channel, since some codecs may use their own */
static STREAMFILE * get_vgmstream_average_bitrate_channel_streamfile(VGMSTREAM * vgmstream, int channel) {

    if (vgmstream->coding_type == coding_NWA) {
        nwa_codec_data *data = vgmstream->codec_data;
        return (data && data->nwa) ? data->nwa->file : NULL;
    }

    if (vgmstream->coding_type == coding_ACM) {
        acm_codec_data *data = vgmstream->codec_data;
        return (data && data->handle) ? data->streamfile : NULL;
    }

#ifdef VGM_USE_VORBIS
    if (vgmstream->coding_type == coding_OGG_VORBIS) {
        return ogg_vorbis_get_streamfile(vgmstream->codec_data);
    }
#endif
    if (vgmstream->coding_type == coding_CRI_HCA) {
        hca_codec_data *data = vgmstream->codec_data;
        return data ? data->streamfile : NULL;
    }
#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type == coding_FFmpeg) {
        ffmpeg_codec_data *data = vgmstream->codec_data;
        return data ? data->streamfile : NULL;
    }
#endif
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type == coding_MP4_AAC) {
        mp4_aac_codec_data *data = vgmstream->codec_data;
        return data ? data->if_file.streamfile : NULL;
    }
#endif

    return vgmstream->ch[channel].streamfile;
}

static int get_vgmstream_file_bitrate_from_size(size_t size, int sample_rate, int length_samples) {
    if (sample_rate == 0 || length_samples == 0) return 0;
    if (length_samples < 100) return 0; /* ignore stupid bitrates caused by some segments */
    return (int)((int64_t)size * 8 * sample_rate / length_samples);
}
static int get_vgmstream_file_bitrate_from_streamfile(STREAMFILE * streamfile, int sample_rate, int length_samples) {
    if (streamfile == NULL) return 0;
    return get_vgmstream_file_bitrate_from_size(get_streamfile_size(streamfile), sample_rate, length_samples);
}

static int get_vgmstream_file_bitrate_main(VGMSTREAM * vgmstream, STREAMFILE **streamfile_pointers, int *pointers_count, int pointers_max) {
    int sub, ch;
    int bitrate = 0;

    /* Recursively get bitrate and fill the list of streamfiles if needed (to filter),
     * since layouts can include further vgmstreams that may also share streamfiles.
     *
     * Because of how data, layers and segments can be combined it's possible to
     * fool this in various ways; metas should report stream_size in complex cases
     * to get accurate bitrates (particularly for subsongs). */

    if (vgmstream->stream_size) {
        bitrate = get_vgmstream_file_bitrate_from_size(vgmstream->stream_size, vgmstream->sample_rate, vgmstream->num_samples);
    }
    else if (vgmstream->layout_type == layout_segmented) {
        segmented_layout_data *data = (segmented_layout_data *) vgmstream->layout_data;
        for (sub = 0; sub < data->segment_count; sub++) {
            bitrate += get_vgmstream_file_bitrate_main(data->segments[sub], streamfile_pointers, pointers_count, pointers_max);
        }
        bitrate = bitrate / data->segment_count;
    }
    else if (vgmstream->layout_type == layout_layered) {
        layered_layout_data *data = vgmstream->layout_data;
        for (sub = 0; sub < data->layer_count; sub++) {
            bitrate += get_vgmstream_file_bitrate_main(data->layers[sub], streamfile_pointers, pointers_count, pointers_max);
        }
        bitrate = bitrate / data->layer_count;
    }
    else {
        /* Add channel bitrate if streamfile hasn't been used before (comparing files
         * by absolute paths), so bitrate doesn't multiply when the same STREAMFILE is
         * reopened per channel, also skipping repeated pointers. */
        char path_current[PATH_LIMIT];
        char path_compare[PATH_LIMIT];
        int is_unique = 1;

        for (ch = 0; ch < vgmstream->channels; ch++) {
            STREAMFILE * currentFile = get_vgmstream_average_bitrate_channel_streamfile(vgmstream, ch);
            if (!currentFile) continue;
            get_streamfile_name(currentFile, path_current, sizeof(path_current));

            for (sub = 0; sub < *pointers_count; sub++) {
                STREAMFILE * compareFile = streamfile_pointers[sub];
                if (!compareFile) continue;
                if (currentFile == compareFile) {
                    is_unique = 0;
                    break;
                }
                get_streamfile_name(compareFile, path_compare, sizeof(path_compare));
                if (strcmp(path_current, path_compare) == 0) {
                    is_unique = 0;
                    break;
                }
            }

            if (is_unique) {
                if (*pointers_count >= pointers_max) goto fail;
                streamfile_pointers[*pointers_count] = currentFile;
                (*pointers_count)++;

                bitrate += get_vgmstream_file_bitrate_from_streamfile(currentFile, vgmstream->sample_rate, vgmstream->num_samples);
            }
        }
    }

    return bitrate;
fail:
    return 0;
}

/* Return the average bitrate in bps of all unique data contained within this stream.
 * This is the bitrate of the *file*, as opposed to the bitrate of the *codec*, meaning
 * it counts extra data like block headers and padding. While this can be surprising
 * sometimes (as it's often higher than common codec bitrates) it isn't wrong per se. */
int get_vgmstream_average_bitrate(VGMSTREAM * vgmstream) {
    const size_t pointers_max = 128; /* arbitrary max, but +100 segments have been observed */
    STREAMFILE *streamfile_pointers[128]; /* list already used streamfiles */
    int pointers_count = 0;

    return get_vgmstream_file_bitrate_main(vgmstream, streamfile_pointers, &pointers_count, pointers_max);
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

    if ((vgmstream->coding_type == coding_MSADPCM ||
            vgmstream->coding_type == coding_MSADPCM_ck ||
            vgmstream->coding_type == coding_MSADPCM_int) &&
            vgmstream->frame_size == 0) {
        vgmstream->frame_size = vgmstream->interleave_block_size;
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
                offset = start_offset + vgmstream->interleave_block_size*ch_mod;
            } else {
                offset = start_offset + vgmstream->interleave_block_size*ch;
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
