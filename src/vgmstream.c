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

static void try_dual_file_stereo(VGMSTREAM * opened_vgmstream, STREAMFILE *streamFile, VGMSTREAM* (*init_vgmstream_function)(STREAMFILE*));


/* List of functions that will recognize files */
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
    init_vgmstream_ngc_dsp_csmp,
    init_vgmstream_cstr,
    init_vgmstream_gcsw,
    init_vgmstream_ps2_ads,
    init_vgmstream_ps2_npsf,
    init_vgmstream_rwsd,
    init_vgmstream_cdxa,
    init_vgmstream_ps2_rxws,
    init_vgmstream_ps2_rxw,
    init_vgmstream_ps2_int,
    init_vgmstream_ngc_dsp_stm,
    init_vgmstream_ps2_exst,
    init_vgmstream_ps2_svag,
    init_vgmstream_ps2_mib,
    init_vgmstream_ngc_mpdsp,
    init_vgmstream_ps2_mic,
    init_vgmstream_ngc_dsp_std_int,
    init_vgmstream_raw,
    init_vgmstream_ps2_vag,
    init_vgmstream_psx_gms,
    init_vgmstream_ps2_str,
    init_vgmstream_ps2_ild,
    init_vgmstream_ps2_pnb,
    init_vgmstream_xbox_wavm,
    init_vgmstream_ngc_str,
    init_vgmstream_ea_schl,
    init_vgmstream_caf,
    init_vgmstream_ps2_vpk,
    init_vgmstream_genh,
#ifdef VGM_USE_VORBIS
    init_vgmstream_ogg_vorbis,
    init_vgmstream_sli_ogg,
    init_vgmstream_sfl,
#endif
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
    init_vgmstream_musx_v004,
    init_vgmstream_musx_v005,
    init_vgmstream_musx_v006,
    init_vgmstream_musx_v010,
    init_vgmstream_musx_v201,
    init_vgmstream_leg,
    init_vgmstream_filp,
    init_vgmstream_ikm,
    init_vgmstream_sfs,
    init_vgmstream_bg00,
    init_vgmstream_sat_dvi,
    init_vgmstream_dc_kcey,
    init_vgmstream_ps2_rstm,
    init_vgmstream_acm,
    init_vgmstream_mus_acm,
    init_vgmstream_ps2_kces,
    init_vgmstream_ps2_dxh,
    init_vgmstream_ps2_psh,
    init_vgmstream_scd_pcm,
    init_vgmstream_ps2_pcm,
    init_vgmstream_ps2_rkv,
    init_vgmstream_ps2_vas,
    init_vgmstream_ps2_tec,
    init_vgmstream_ps2_enth,
    init_vgmstream_sdt,
    init_vgmstream_aix,
    init_vgmstream_ngc_tydsp,
    init_vgmstream_ngc_swd,
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
    init_vgmstream_xbox_xmu,
    init_vgmstream_xbox_xvas,
    init_vgmstream_ngc_bh2pcm,
    init_vgmstream_sat_sap,
    init_vgmstream_dc_idvi,
    init_vgmstream_ps2_rnd,
    init_vgmstream_wii_idsp,
    init_vgmstream_kraw,
    init_vgmstream_ps2_omu,
    init_vgmstream_ps2_xa2,
    init_vgmstream_idsp2,
    init_vgmstream_idsp3,
    init_vgmstream_idsp4,
    init_vgmstream_ngc_ymf,
    init_vgmstream_sadl,
    init_vgmstream_ps2_ccc,
    init_vgmstream_psx_fag,
    init_vgmstream_ps2_mihb,
    init_vgmstream_ngc_pdt_split,
    init_vgmstream_ngc_pdt,
    init_vgmstream_wii_mus,
    init_vgmstream_dc_asd,
    init_vgmstream_naomi_spsd,
    init_vgmstream_rsd2vag,
    init_vgmstream_rsd2pcmb,
    init_vgmstream_rsd2xadp,
    init_vgmstream_rsd3vag,
    init_vgmstream_rsd3gadp,
    init_vgmstream_rsd3pcm,
    init_vgmstream_rsd3pcmb,
    init_vgmstream_rsd4pcmb,
    init_vgmstream_rsd4pcm,
    init_vgmstream_rsd4radp,
    init_vgmstream_rsd4vag,
    init_vgmstream_rsd6vag,
    init_vgmstream_rsd6wadp,
    init_vgmstream_rsd6xadp,
    init_vgmstream_rsd6radp,
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
    init_vgmstream_dc_dcsw_dcs,
    init_vgmstream_wii_smp,
    init_vgmstream_emff_ps2,
    init_vgmstream_emff_ngc,
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
    init_vgmstream_nds_swav,
    init_vgmstream_ps2_vsf,
    init_vgmstream_nds_rrds,
    init_vgmstream_ps2_tk5,
    init_vgmstream_ps2_vsf_tta,
    init_vgmstream_ads,
    init_vgmstream_wii_str,
    init_vgmstream_ps2_mcg,
    init_vgmstream_zsd,
    init_vgmstream_ps2_vgs,
    init_vgmstream_RedSpark,
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
    init_vgmstream_stx,
    init_vgmstream_myspd,
    init_vgmstream_his,
    init_vgmstream_ps2_ast,
    init_vgmstream_dmsg,
    init_vgmstream_ngc_dsp_aaap,
    init_vgmstream_ngc_dsp_konami,
    init_vgmstream_ps2_ster,
    init_vgmstream_ps2_wb,
    init_vgmstream_bnsf,
    init_vgmstream_s14_sss,
    init_vgmstream_ps2_gcm,
    init_vgmstream_ps2_smpl,
    init_vgmstream_ps2_msa,
    init_vgmstream_ps2_voi,
    init_vgmstream_ps2_khv,
    init_vgmstream_pc_smp,
    init_vgmstream_ngc_rkv,
    init_vgmstream_dsp_ddsp,
    init_vgmstream_p3d,
    init_vgmstream_ps2_tk1,
    init_vgmstream_ngc_dsp_mpds,
    init_vgmstream_dsp_str_ig,
    init_vgmstream_ea_swvr,
    init_vgmstream_ngc_dsp_sth_str1,
    init_vgmstream_ngc_dsp_sth_str2,
    init_vgmstream_ngc_dsp_sth_str3,
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
    init_vgmstream_ps2_jstm,
    init_vgmstream_xvag,
    init_vgmstream_ps3_cps,
    init_vgmstream_sqex_scd,
    init_vgmstream_ngc_nst_dsp,
    init_vgmstream_baf,
    init_vgmstream_ps3_msf,
    init_vgmstream_nub_vag,
    init_vgmstream_ps3_past,
    init_vgmstream_sgxd,
    init_vgmstream_ngca,
    init_vgmstream_wii_ras,
    init_vgmstream_ps2_spm,
    init_vgmstream_x360_tra,
    init_vgmstream_ps2_iab,
    init_vgmstream_ps2_strlr,
    init_vgmstream_lsf_n1nj4n,
    init_vgmstream_vawx,
    init_vgmstream_pc_snds,
    init_vgmstream_ps2_wmus,
    init_vgmstream_hyperscan_kvag,
    init_vgmstream_ios_psnd,
    init_vgmstream_pc_adp_bos,
    init_vgmstream_pc_adp_otns,
    init_vgmstream_eb_sfx,
    init_vgmstream_eb_sf0,
    init_vgmstream_ps3_klbs,
    init_vgmstream_ps2_mtaf,
    init_vgmstream_tun,
    init_vgmstream_wpd,
    init_vgmstream_mn_str,
    init_vgmstream_mss,
    init_vgmstream_ps2_hsf,
    init_vgmstream_ps3_ivag,
    init_vgmstream_ps2_2pfs,
    init_vgmstream_xnb,
    init_vgmstream_rsd6oogv,
    init_vgmstream_ubi_ckd,
    init_vgmstream_ps2_vbk,
    init_vgmstream_otm,
    init_vgmstream_bcstm,
    init_vgmstream_3ds_idsp,
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
    init_vgmstream_nub_xma,
    init_vgmstream_xma,
    init_vgmstream_sxd,
    init_vgmstream_ogl,
    init_vgmstream_mc3,
    init_vgmstream_gtd,
    init_vgmstream_rsd6xma,
    init_vgmstream_ta_aac_x360,
    init_vgmstream_ta_aac_ps3,
    init_vgmstream_ta_aac_mobile,
    init_vgmstream_ta_aac_mobile_vorbis,
    init_vgmstream_ta_aac_vita,
    init_vgmstream_ps3_mta2,
    init_vgmstream_ngc_ulw,
    init_vgmstream_pc_xa30,
    init_vgmstream_wii_04sw,
    init_vgmstream_ea_bnk,
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
    init_vgmstream_opus_nlsd,
    init_vgmstream_pc_al2,
    init_vgmstream_pc_ast,
    init_vgmstream_naac,
    init_vgmstream_ubi_sb,
    init_vgmstream_ezw,
    init_vgmstream_vxn,
    init_vgmstream_ea_snr_sns,
    init_vgmstream_ea_sps,
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
    init_vgmstream_rsd6at3p,
    init_vgmstream_rsd6wma,
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
    init_vgmstream_ea_sps_fb,
    init_vgmstream_ppst,
    init_vgmstream_opus_ppp,
    init_vgmstream_ubi_bao_pk,
    init_vgmstream_dsp_switch_audio,
    init_vgmstream_dsp_sadf,
    init_vgmstream_h4m,
    init_vgmstream_ps2_ads_container,

    init_vgmstream_txth,  /* should go at the end (lower priority) */
#ifdef VGM_USE_FFMPEG
    init_vgmstream_ffmpeg, /* should go at the end */
#endif
};


/* internal version with all parameters */
static VGMSTREAM * init_vgmstream_internal(STREAMFILE *streamFile) {
    int i, fcns_size;
    
    if (!streamFile)
        return NULL;

    fcns_size = (sizeof(init_vgmstream_functions)/sizeof(init_vgmstream_functions[0]));
    /* try a series of formats, see which works */
    for (i=0; i < fcns_size; i++) {
        /* call init function and see if valid VGMSTREAM was returned */
        VGMSTREAM * vgmstream = (init_vgmstream_functions[i])(streamFile);
        if (!vgmstream)
            continue;

        /* fail if there is nothing to play (without this check vgmstream can generate empty files) */
        if (vgmstream->num_samples <= 0) {
            VGM_LOG("VGMSTREAM: wrong num_samples (ns=%i / 0x%08x)\n", vgmstream->num_samples, vgmstream->num_samples);
            close_vgmstream(vgmstream);
            continue;
        }

        /* everything should have a reasonable sample rate (300 is Wwise min) */
        if (vgmstream->sample_rate < 300 || vgmstream->sample_rate > 96000) {
            VGM_LOG("VGMSTREAM: wrong sample rate (sr=%i)\n", vgmstream->sample_rate);
            close_vgmstream(vgmstream);
            continue;
        }
            
        /* Sanify loops! */
        if (vgmstream->loop_flag) {
            if ((vgmstream->loop_end_sample <= vgmstream->loop_start_sample)
                    || (vgmstream->loop_end_sample > vgmstream->num_samples)
                    || (vgmstream->loop_start_sample < 0) ) {
                vgmstream->loop_flag = 0;
                VGM_LOG("VGMSTREAM: wrong loops ignored (lss=%i, lse=%i, ns=%i)\n", vgmstream->loop_start_sample, vgmstream->loop_end_sample, vgmstream->num_samples);
            }
        }

        /* test if candidate for dual stereo */
        if (vgmstream->channels == 1 && (
                    (vgmstream->meta_type == meta_DSP_STD) ||
                    (vgmstream->meta_type == meta_PS2_VAGp) ||
                    (vgmstream->meta_type == meta_GENH) ||
                    (vgmstream->meta_type == meta_TXTH) ||
                    (vgmstream->meta_type == meta_KRAW) ||
                    (vgmstream->meta_type == meta_PS2_MIB) ||
                    (vgmstream->meta_type == meta_NGC_LPS) ||
                    (vgmstream->meta_type == meta_DSP_YGO) ||
                    (vgmstream->meta_type == meta_DSP_AGSC) ||
                    (vgmstream->meta_type == meta_PS2_SMPL) ||
                    (vgmstream->meta_type == meta_NGCA) ||
                    (vgmstream->meta_type == meta_NUB_VAG) ||
                    (vgmstream->meta_type == meta_SPT_SPD) ||
                    (vgmstream->meta_type == meta_EB_SFX) ||
                    (vgmstream->meta_type == meta_CWAV)
                    )) {
            try_dual_file_stereo(vgmstream, streamFile, init_vgmstream_functions[i]);
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

        /* save info */
        /* stream_index 0 may be used by plugins to signal "vgmstream default" (IOW don't force to 1) */
        if (!vgmstream->stream_index)
            vgmstream->stream_index = streamFile->stream_index;

        /* save start things so we can restart for seeking */
        memcpy(vgmstream->start_ch,vgmstream->ch,sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
        memcpy(vgmstream->start_vgmstream,vgmstream,sizeof(VGMSTREAM));

        return vgmstream;
    }

    /* not supported */
    return NULL;
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

/* Reset a VGMSTREAM to its state at the start of playback.
 * Note that this does not reset the constituent STREAMFILES. */
void reset_vgmstream(VGMSTREAM * vgmstream) {
    /* copy the vgmstream back into itself */
    memcpy(vgmstream,vgmstream->start_vgmstream,sizeof(VGMSTREAM));

    /* copy the initial channels */
    memcpy(vgmstream->ch,vgmstream->start_ch,sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);

    /* loop_ch is not zeroed here because there is a possibility of the
     * init_vgmstream_* function doing something tricky and precomputing it.
     * Otherwise hit_loop will be 0 and it will be copied over anyway when we
     * really hit the loop start. */

#ifdef VGM_USE_VORBIS
    if (vgmstream->coding_type==coding_OGG_VORBIS) {
        reset_ogg_vorbis(vgmstream);
    }

    if (vgmstream->coding_type==coding_VORBIS_custom) {
        reset_vorbis_custom(vgmstream);
    }
#endif

    if (vgmstream->coding_type==coding_CRI_HCA) {
        reset_hca(vgmstream);
    }

    if (vgmstream->coding_type==coding_EA_MT) {
        reset_ea_mt(vgmstream);
    }

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type==coding_MP4_AAC) {
        reset_mp4_aac(vgmstream);
    }
#endif

#ifdef VGM_USE_MPEG
    if (vgmstream->coding_type==coding_MPEG_custom ||
        vgmstream->coding_type==coding_MPEG_ealayer3 ||
        vgmstream->coding_type==coding_MPEG_layer1 ||
        vgmstream->coding_type==coding_MPEG_layer2 ||
        vgmstream->coding_type==coding_MPEG_layer3) {
        reset_mpeg(vgmstream);
    }
#endif

#ifdef VGM_USE_G7221
    if (vgmstream->coding_type==coding_G7221 ||
        vgmstream->coding_type==coding_G7221C) {
        reset_g7221(vgmstream);
    }
#endif

#ifdef VGM_USE_G719
    if (vgmstream->coding_type==coding_G719) {
        reset_g719(vgmstream);
    }
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
    if (vgmstream->coding_type==coding_AT3plus) {
        reset_at3plus(vgmstream);
    }
#endif

#ifdef VGM_USE_ATRAC9
    if (vgmstream->coding_type==coding_ATRAC9) {
        reset_atrac9(vgmstream);
    }
#endif

#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type==coding_FFmpeg) {
        reset_ffmpeg(vgmstream);
    }
#endif

    if (vgmstream->coding_type==coding_ACM) {
        reset_acm(vgmstream);
    }

    if (vgmstream->coding_type == coding_NWA) {
        nwa_codec_data *data = vgmstream->codec_data;
        if (data)
            reset_nwa(data->nwa);
    }


    if (vgmstream->layout_type==layout_aix) {
        aix_codec_data *data = vgmstream->codec_data;
        int i;

        data->current_segment = 0;
        for (i=0;i<data->segment_count*data->stream_count;i++) {
            reset_vgmstream(data->adxs[i]);
        }
    }

    if (vgmstream->layout_type==layout_segmented) {
        reset_layout_segmented(vgmstream->layout_data);
    }

    if (vgmstream->layout_type==layout_layered) {
        reset_layout_layered(vgmstream->layout_data);
    }
}

/* Allocate memory and setup a VGMSTREAM */
VGMSTREAM * allocate_vgmstream(int channel_count, int looped) {
    VGMSTREAM * vgmstream;
    VGMSTREAM * start_vgmstream;
    VGMSTREAMCHANNEL * channels;
    VGMSTREAMCHANNEL * start_channels;
    VGMSTREAMCHANNEL * loop_channels;

    /* up to ~16 aren't too rare for multilayered files, more is probably a bug */
    if (channel_count <= 0 || channel_count > 64)
        return NULL;

    vgmstream = calloc(1,sizeof(VGMSTREAM));
    if (!vgmstream) return NULL;
    
    vgmstream->ch = NULL;
    vgmstream->start_ch = NULL;
    vgmstream->loop_ch = NULL;
    vgmstream->start_vgmstream = NULL;
    vgmstream->codec_data = NULL;

    start_vgmstream = calloc(1,sizeof(VGMSTREAM));
    if (!start_vgmstream) {
        free(vgmstream);
        return NULL;
    }
    vgmstream->start_vgmstream = start_vgmstream;
    start_vgmstream->start_vgmstream = start_vgmstream;

    channels = calloc(channel_count,sizeof(VGMSTREAMCHANNEL));
    if (!channels) {
        free(vgmstream);
        free(start_vgmstream);
        return NULL;
    }
    vgmstream->ch = channels;
    vgmstream->channels = channel_count;

    start_channels = calloc(channel_count,sizeof(VGMSTREAMCHANNEL));
    if (!start_channels) {
        free(vgmstream);
        free(start_vgmstream);
        free(channels);
        return NULL;
    }
    vgmstream->start_ch = start_channels;

    if (looped) {
        loop_channels = calloc(channel_count,sizeof(VGMSTREAMCHANNEL));
        if (!loop_channels) {
            free(vgmstream);
            free(start_vgmstream);
            free(channels);
            free(start_channels);
            return NULL;
        }
        vgmstream->loop_ch = loop_channels;
    }

    vgmstream->loop_flag = looped;

    return vgmstream;
}

void close_vgmstream(VGMSTREAM * vgmstream) {
    if (!vgmstream)
        return;

#ifdef VGM_USE_VORBIS
    if (vgmstream->coding_type==coding_OGG_VORBIS) {
        free_ogg_vorbis(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }

    if (vgmstream->coding_type==coding_VORBIS_custom) {
        free_vorbis_custom(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }
#endif

    if (vgmstream->coding_type==coding_CRI_HCA) {
        free_hca(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }

    if (vgmstream->coding_type==coding_EA_MT) {
        free_ea_mt(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }

#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type==coding_FFmpeg) {
        free_ffmpeg(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }
#endif

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type==coding_MP4_AAC) {
        free_mp4_aac(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }
#endif

#ifdef VGM_USE_MPEG
    if (vgmstream->coding_type==coding_MPEG_custom ||
        vgmstream->coding_type==coding_MPEG_ealayer3 ||
        vgmstream->coding_type==coding_MPEG_layer1 ||
        vgmstream->coding_type==coding_MPEG_layer2 ||
        vgmstream->coding_type==coding_MPEG_layer3) {
        free_mpeg(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }
#endif

#ifdef VGM_USE_G7221
    if (vgmstream->coding_type == coding_G7221 ||
        vgmstream->coding_type == coding_G7221C) {
        free_g7221(vgmstream);
        vgmstream->codec_data = NULL;
    }
#endif

#ifdef VGM_USE_G719
    if (vgmstream->coding_type == coding_G719) {
        free_g719(vgmstream);
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

    if (vgmstream->coding_type==coding_ACM) {
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


    if (vgmstream->layout_type==layout_aix) {
        aix_codec_data *data = (aix_codec_data *) vgmstream->codec_data;

        if (data) {
            if (data->adxs) {
                int i;
                for (i=0;i<data->segment_count*data->stream_count;i++) {
                    /* note that the close_streamfile won't do anything but deallocate itself,
                     * there is only one open file in vgmstream->ch[0].streamfile */
                    close_vgmstream(data->adxs[i]);
                }
                free(data->adxs);
            }
            if (data->sample_counts) {
                free(data->sample_counts);
            }

            free(data);
        }
        vgmstream->codec_data = NULL;
    }

    if (vgmstream->layout_type==layout_segmented) {
        free_layout_segmented(vgmstream->layout_data);
        vgmstream->layout_data = NULL;
    }

    if (vgmstream->layout_type==layout_layered) {
        free_layout_layered(vgmstream->layout_data);
        vgmstream->layout_data = NULL;
    }


    /* now that the special cases have had their chance, clean up the standard items */
    {
        int i,j;

        for (i=0;i<vgmstream->channels;i++) {
            if (vgmstream->ch[i].streamfile) {
                close_streamfile(vgmstream->ch[i].streamfile);
                /* Multiple channels might have the same streamfile. Find the others
                 * that are the same as this and clear them so they won't be closed
                 * again. */
                for (j=0;j<vgmstream->channels;j++) {
                    if (i!=j && vgmstream->ch[j].streamfile ==
                                vgmstream->ch[i].streamfile) {
                        vgmstream->ch[j].streamfile = NULL;
                    }
                }
                vgmstream->ch[i].streamfile = NULL;
            }
        }
    }

    if (vgmstream->loop_ch) free(vgmstream->loop_ch);
    if (vgmstream->start_ch) free(vgmstream->start_ch);
    if (vgmstream->ch) free(vgmstream->ch);
    /* the start_vgmstream is considered just data */
    if (vgmstream->start_vgmstream) free(vgmstream->start_vgmstream);

    free(vgmstream);
}

/* calculate samples based on player's config */
int32_t get_vgmstream_play_samples(double looptimes, double fadeseconds, double fadedelayseconds, VGMSTREAM * vgmstream) {
    if (vgmstream->loop_flag) {
        if (fadeseconds < 0) { /* a bit hack-y to avoid signature change */
            /* Continue playing the file normally after looping, instead of fading.
             * Most files cut abruply after the loop, but some do have proper endings.
             * With looptimes = 1 this option should give the same output vs loop disabled */
            int loop_count = (int)looptimes; /* no half loops allowed */
            //vgmstream->loop_target = loop_count; /* handled externally, as this is into-only */
            return vgmstream->loop_start_sample
                + (vgmstream->loop_end_sample - vgmstream->loop_start_sample) * loop_count
                + (vgmstream->num_samples - vgmstream->loop_end_sample);
        }
        else {
            return (int32_t)(vgmstream->loop_start_sample
                + (vgmstream->loop_end_sample - vgmstream->loop_start_sample) * looptimes
                + (fadedelayseconds + fadeseconds) * vgmstream->sample_rate);
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
        /* loop_ch will be populated when decoded samples reach loop start */
    }
    vgmstream->loop_flag = loop_flag;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_start_sample;
        vgmstream->loop_end_sample = loop_end_sample;
    }
}

/* Decode data into sample buffer */
void render_vgmstream(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    switch (vgmstream->layout_type) {
        case layout_interleave:
            render_vgmstream_interleave(buffer,sample_count,vgmstream);
            break;
        case layout_none:
            render_vgmstream_nolayout(buffer,sample_count,vgmstream);
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
        case layout_blocked_emff_ps2:
        case layout_blocked_emff_ngc:
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
        case layout_blocked_ps2_strlr:
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
            render_vgmstream_blocked(buffer,sample_count,vgmstream);
            break;
        case layout_aix:
            render_vgmstream_aix(buffer,sample_count,vgmstream);
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


    /* channel bitmask to silence non-set channels (up to 32)
     * can be used for 'crossfading subsongs' or layered channels, where a set of channels make a song section */
    if (vgmstream->channel_mask) {
        int ch,s;
        for (s = 0; s < sample_count; s++) {
            for (ch = 0; ch < vgmstream->channels; ch++) {
                if ((vgmstream->channel_mask >> ch) & 1)
                    continue;
                buffer[s*vgmstream->channels + ch] = 0;
            }
        }
    }
}

/* Get the number of samples of a single frame (smallest self-contained sample group, 1/N channels) */
int get_vgmstream_samples_per_frame(VGMSTREAM * vgmstream) {
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
            return 16;
        case coding_NGC_DTK:
            return 28;
        case coding_G721:
            return 1;

        case coding_PCM16LE:
        case coding_PCM16BE:
        case coding_PCM16_int:
        case coding_PCM8:
        case coding_PCM8_U:
        case coding_PCM8_int:
        case coding_PCM8_SB_int:
        case coding_PCM8_U_int:
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
        case coding_NWA:
        case coding_SASSC:
            return 1;

        case coding_IMA:
        case coding_DVI_IMA:
        case coding_SNDS_IMA:
        case coding_OTNS_IMA:
        case coding_UBI_IMA:
            return 1;
        case coding_IMA_int:
        case coding_DVI_IMA_int:
        case coding_3DS_IMA:
            return 2;
        case coding_XBOX_IMA:
        case coding_XBOX_IMA_mch:
        case coding_XBOX_IMA_int:
        case coding_FSB_IMA:
        case coding_WWISE_IMA:
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

        case coding_XA:
        case coding_PSX:
        case coding_PSX_badflags:
        case coding_HEVAG:
            return 28;
        case coding_PSX_cfg:
            return (vgmstream->interleave_block_size - 1) * 2; /* decodes 1 byte into 2 bytes */

        case coding_EA_XA:
        case coding_EA_XA_int:
        case coding_EA_XA_V2:
        case coding_MAXIS_XA:
            return 28;
        case coding_EA_XAS:
            return 128;

        case coding_MSADPCM:
            return (vgmstream->interleave_block_size-(7-1)*vgmstream->channels)*2/vgmstream->channels;
        case coding_WS: /* only works if output sample size is 8 bit, which always is for WS ADPCM */
            return vgmstream->ws_output_size;
        case coding_AICA:
            return 1;
        case coding_AICA_int:
            return 2;
        case coding_YAMAHA:
            return (0x40-0x04*vgmstream->channels) * 2 / vgmstream->channels;
        case coding_YAMAHA_NXAP:
            return (0x40-0x04) * 2;
        case coding_NDS_PROCYON:
            return 30;
        case coding_L5_555:
            return 32;
        case coding_LSF:
            return 54;

#ifdef VGM_USE_G7221
        case coding_G7221C:
            return 32000/50;
        case coding_G7221:
            return 16000/50;
#endif
#ifdef VGM_USE_G719
        case coding_G719:
            return 48000/50;
#endif
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
            if (vgmstream->codec_data) {
                ffmpeg_codec_data *data = (ffmpeg_codec_data*)vgmstream->codec_data;
                return data->sampleBufferBlock; /* must know the full block size for edge loops */
            }
            else {
                return 0;
            }
            break;
#endif
        case coding_MTAF:
            return 128*2;
        case coding_MTA2:
            return 128*2;
        case coding_MC3:
            return 10;
        case coding_FADPCM:
            return 256; /* (0x8c - 0xc) * 2 */
        case coding_EA_MT:
            return 432;
        case coding_CRI_HCA:
            return clHCA_samplesPerBlock;
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
        case coding_PCM8_U:
        case coding_PCM8_int:
        case coding_PCM8_SB_int:
        case coding_PCM8_U_int:
        case coding_ULAW:
        case coding_ULAW_int:
        case coding_ALAW:
            return 0x01;
        case coding_PCMFLOAT:
            return 0x04;

        case coding_SDX2:
        case coding_SDX2_int:
        case coding_CBD2:
        case coding_NWA:
        case coding_SASSC:
            return 0x01;

        case coding_IMA:
        case coding_IMA_int:
        case coding_DVI_IMA:
        case coding_DVI_IMA_int:
        case coding_3DS_IMA:
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
            return 0;
        case coding_UBI_IMA: /* variable (PCM then IMA) */
            return 0;
        case coding_XBOX_IMA:
            //todo should be  0x48 when stereo, but blocked/interleave layout don't understand stereo codecs
            return 0x24; //vgmstream->channels==1 ? 0x24 : 0x48;
        case coding_XBOX_IMA_int:
        case coding_WWISE_IMA:
            return 0x24;
        case coding_XBOX_IMA_mch:
        case coding_FSB_IMA:
            return 0x24 * vgmstream->channels;
        case coding_APPLE_IMA4:
            return 0x22;

        case coding_XA:
            return 0x0e*vgmstream->channels;
        case coding_PSX:
        case coding_PSX_badflags:
        case coding_HEVAG:
            return 0x10;
        case coding_PSX_cfg:
            return vgmstream->interleave_block_size;

        case coding_EA_XA:
            return 0x1E;
        case coding_EA_XA_int:
            return 0x0F;
        case coding_MAXIS_XA:
            return 0x0F*vgmstream->channels;
        case coding_EA_XA_V2:
            return 0; /* variable (ADPCM frames of 0x0f or PCM frames of 0x3d) */
        case coding_EA_XAS:
            return 0x4c*vgmstream->channels;

        case coding_MSADPCM:
            return vgmstream->interleave_block_size;
        case coding_WS:
            return vgmstream->current_block_size;
        case coding_AICA:
        case coding_AICA_int:
            return 0x01;
        case coding_YAMAHA:
        case coding_YAMAHA_NXAP:
            return 0x40;
        case coding_NDS_PROCYON:
            return 0x10;
        case coding_L5_555:
            return 0x12;
        case coding_LSF:
            return 0x1C;

#ifdef VGM_USE_G7221
        case coding_G7221C:
        case coding_G7221:
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
        case coding_EA_MT:
            return 0; /* variable (frames of bit counts or PCM frames) */
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9:
            return 0; /* varies with config data, usually 0x100-200 */
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
void decode_vgmstream(VGMSTREAM * vgmstream, int samples_written, int samples_to_do, sample * buffer) {
    int chan;

    switch (vgmstream->coding_type) {
        case coding_CRI_ADX:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_adx(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,
                        vgmstream->interleave_block_size);
            }

            break;
        case coding_CRI_ADX_exp:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_adx_exp(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,
                        vgmstream->interleave_block_size);
            }

            break;
        case coding_CRI_ADX_fixed:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_adx_fixed(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,
                        vgmstream->interleave_block_size);
            }

            break;
        case coding_CRI_ADX_enc_8:
        case coding_CRI_ADX_enc_9:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_adx_enc(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,
                        vgmstream->interleave_block_size);
            }

            break;
        case coding_NGC_DSP:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ngc_dsp(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_NGC_DSP_subint:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ngc_dsp_subint(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,
                        chan, vgmstream->interleave_block_size);
            }
            break;
        case coding_PCM16LE:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm16LE(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCM16BE:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm16BE(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCM16_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm16_int(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,
                        vgmstream->codec_endian);
            }
            break;
        case coding_PCM8:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm8(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCM8_U:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm8_unsigned(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCM8_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm8_int(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCM8_SB_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm8_sb_int(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCM8_U_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm8_unsigned_int(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_ULAW:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ulaw(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_ULAW_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ulaw_int(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_ALAW:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_alaw(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCMFLOAT:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcmfloat(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,
                        vgmstream->codec_endian);
            }
            break;

        case coding_NDS_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_nds_ima(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_DAT4_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_dat4_ima(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_XBOX_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_xbox_ima(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_XBOX_IMA_mch:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_xbox_ima_mch(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_XBOX_IMA_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_xbox_ima_int(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_MS_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ms_ima(vgmstream,&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_RAD_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_rad_ima(vgmstream,&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_RAD_IMA_mono:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_rad_ima_mono(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_NGC_DTK:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ngc_dtk(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_G721:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_g721(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_NGC_AFC:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ngc_afc(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PSX:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_psx(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PSX_badflags:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_psx_badflags(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_HEVAG:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_hevag(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PSX_cfg:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_psx_configurable(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do, vgmstream->interleave_block_size);
            }
            break;
        case coding_XA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_xa(vgmstream,buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_EA_XA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ea_xa(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_EA_XA_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ea_xa_int(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_EA_XA_V2:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ea_xa_v2(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_MAXIS_XA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_maxis_xa(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_EA_XAS:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ea_xas(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
#ifdef VGM_USE_VORBIS
        case coding_OGG_VORBIS:
            decode_ogg_vorbis(vgmstream->codec_data,
                    buffer+samples_written*vgmstream->channels,samples_to_do,
                    vgmstream->channels);
            break;

        case coding_VORBIS_custom:
            decode_vorbis_custom(vgmstream,
                    buffer+samples_written*vgmstream->channels,samples_to_do,
                    vgmstream->channels);
            break;
#endif
        case coding_CRI_HCA:
            decode_hca(vgmstream->codec_data,
                buffer+samples_written*vgmstream->channels,samples_to_do,
                vgmstream->channels);
            break;
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
            decode_ffmpeg(vgmstream,
                          buffer+samples_written*vgmstream->channels,
                          samples_to_do,
                          vgmstream->channels);
            break;
#endif
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
        case coding_MP4_AAC:
            decode_mp4_aac(vgmstream->codec_data,
                buffer+samples_written*vgmstream->channels,samples_to_do,
                vgmstream->channels);
            break;
#endif
        case coding_SDX2:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_sdx2(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_SDX2_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_sdx2_int(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_CBD2:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_cbd2(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_CBD2_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_cbd2_int(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_IMA:
        case coding_IMA_int:
        case coding_DVI_IMA:
        case coding_DVI_IMA_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                int is_stereo = (vgmstream->channels > 1 && vgmstream->coding_type == coding_IMA)
                        || (vgmstream->channels > 1 && vgmstream->coding_type == coding_DVI_IMA);
                int is_high_first = vgmstream->coding_type == coding_DVI_IMA || vgmstream->coding_type == coding_DVI_IMA_int;

                decode_standard_ima(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do, chan, is_stereo, is_high_first);
            }
            break;
        case coding_3DS_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_3ds_ima(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_APPLE_IMA4:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_apple_ima4(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_SNDS_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_snds_ima(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_OTNS_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_otns_ima(vgmstream, &vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_FSB_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_fsb_ima(vgmstream, &vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_WWISE_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_wwise_ima(vgmstream,&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_REF_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ref_ima(vgmstream,&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_AWC_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_awc_ima(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_UBI_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ubi_ima(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;

        case coding_WS:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ws(vgmstream,chan,buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;

#ifdef VGM_USE_MPEG
        case coding_MPEG_custom:
        case coding_MPEG_ealayer3:
        case coding_MPEG_layer1:
        case coding_MPEG_layer2:
        case coding_MPEG_layer3:
            decode_mpeg(
                    vgmstream,
                    buffer+samples_written*vgmstream->channels,
                    samples_to_do,
                    vgmstream->channels);
            break;
#endif
#ifdef VGM_USE_G7221
        case coding_G7221:
        case coding_G7221C:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_g7221(vgmstream,
                    buffer+samples_written*vgmstream->channels+chan,
                    vgmstream->channels,
                    samples_to_do,
                    chan);
            }
            break;
#endif
#ifdef VGM_USE_G719
        case coding_G719:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_g719(vgmstream,
                    buffer+samples_written*vgmstream->channels+chan,
                    vgmstream->channels,
                    samples_to_do,
                    chan);
            }
            break;
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
        case coding_AT3plus:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_at3plus(vgmstream,
                    buffer+samples_written*vgmstream->channels+chan,
                    vgmstream->channels,
                    samples_to_do,
                    chan);
            }
            break;
#endif
#ifdef VGM_USE_ATRAC9
        case coding_ATRAC9:
            decode_atrac9(vgmstream,
                          buffer+samples_written*vgmstream->channels,
                          samples_to_do,
                          vgmstream->channels);
            break;
#endif
        case coding_ACM:
            decode_acm(vgmstream->codec_data,
                    buffer+samples_written*vgmstream->channels,
                    samples_to_do, vgmstream->channels);
            break;
        case coding_NWA:
            decode_nwa(((nwa_codec_data*)vgmstream->codec_data)->nwa,
                    buffer+samples_written*vgmstream->channels,
                    samples_to_do
                    );
            break;
        case coding_MSADPCM:
            if (vgmstream->channels == 2) {
                decode_msadpcm_stereo(vgmstream,
                        buffer+samples_written*vgmstream->channels,
                        vgmstream->samples_into_block,
                        samples_to_do);
            }
            else if (vgmstream->channels == 1)
            {
                decode_msadpcm_mono(vgmstream,
                        buffer+samples_written*vgmstream->channels,
                        vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_AICA:
        case coding_AICA_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                int is_stereo = (vgmstream->channels > 1 && vgmstream->coding_type == coding_AICA);

                decode_aica(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do, chan, is_stereo);
            }
            break;
        case coding_YAMAHA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_yamaha(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do, chan);
            }
            break;
        case coding_YAMAHA_NXAP:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_yamaha_nxap(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_NDS_PROCYON:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_nds_procyon(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_L5_555:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_l5_555(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }

            break;
        case coding_SASSC:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_SASSC(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }

            break;
        case coding_LSF:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_lsf(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_MTAF:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_mtaf(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        chan, vgmstream->channels);
            }
            break;
        case coding_MTA2:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_mta2(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        chan);
            }
            break;
        case coding_MC3:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_mc3(vgmstream, &vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        chan);
            }
            break;
        case coding_FADPCM:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_fadpcm(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_EA_MT:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ea_mt(vgmstream, buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        chan);
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
        if (vgmstream->current_sample+samples_left_this_block > vgmstream->loop_end_sample) {
            /* only do to just before it */
            samples_to_do = vgmstream->loop_end_sample-vgmstream->current_sample;
        }

        /* are we going to hit the loop start during this block? */
        if (!vgmstream->hit_loop && vgmstream->current_sample+samples_left_this_block > vgmstream->loop_start_sample) {
            /* only do to just before it */
            samples_to_do = vgmstream->loop_start_sample-vgmstream->current_sample;
        }

    }

    /* if it's a framed encoding don't do more than one frame */
    if (samples_per_frame>1 && (vgmstream->samples_into_block%samples_per_frame)+samples_to_do>samples_per_frame)
        samples_to_do = samples_per_frame - (vgmstream->samples_into_block%samples_per_frame);

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
            vgmstream->loop_flag = 0; /* could be improved but works ok */
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
            for (i=0;i<vgmstream->channels;i++) {
                vgmstream->loop_ch[i].adpcm_history1_16 = vgmstream->ch[i].adpcm_history1_16;
                vgmstream->loop_ch[i].adpcm_history2_16 = vgmstream->ch[i].adpcm_history2_16;
                vgmstream->loop_ch[i].adpcm_history1_32 = vgmstream->ch[i].adpcm_history1_32;
                vgmstream->loop_ch[i].adpcm_history2_32 = vgmstream->ch[i].adpcm_history2_32;
            }
        }


        /* prepare certain codecs' internal state for looping */

        if (vgmstream->coding_type==coding_CRI_HCA) {
            loop_hca(vgmstream);
        }

        if (vgmstream->coding_type==coding_EA_MT) {
            seek_ea_mt(vgmstream, vgmstream->loop_start_sample);
        }

#ifdef VGM_USE_VORBIS
        if (vgmstream->coding_type==coding_OGG_VORBIS) {
            seek_ogg_vorbis(vgmstream, vgmstream->loop_sample);
        }

        if (vgmstream->coding_type==coding_VORBIS_custom) {
            seek_vorbis_custom(vgmstream, vgmstream->loop_start_sample);
        }
#endif

#ifdef VGM_USE_FFMPEG
        if (vgmstream->coding_type==coding_FFmpeg) {
            seek_ffmpeg(vgmstream, vgmstream->loop_start_sample);
        }
#endif

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
        if (vgmstream->coding_type==coding_MP4_AAC) {
            seek_mp4_aac(vgmstream, vgmstream->loop_sample);
        }
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
        if (vgmstream->coding_type==coding_AT3plus) {
            seek_at3plus(vgmstream, vgmstream->loop_sample);
        }
#endif

#ifdef VGM_USE_ATRAC9
        if (vgmstream->coding_type==coding_ATRAC9) {
            seek_atrac9(vgmstream, vgmstream->loop_sample);
        }
#endif

#ifdef VGM_USE_MPEG
        if (vgmstream->coding_type==coding_MPEG_custom ||
            vgmstream->coding_type==coding_MPEG_ealayer3 ||
            vgmstream->coding_type==coding_MPEG_layer1 ||
            vgmstream->coding_type==coding_MPEG_layer2 ||
            vgmstream->coding_type==coding_MPEG_layer3) {
            seek_mpeg(vgmstream, vgmstream->loop_sample);
        }
#endif

        if (vgmstream->coding_type == coding_NWA) {
            nwa_codec_data *data = vgmstream->codec_data;
            if (data)
                seek_nwa(data->nwa, vgmstream->loop_sample);
        }

        /* restore! */
        memcpy(vgmstream->ch,vgmstream->loop_ch,sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
        vgmstream->current_sample = vgmstream->loop_sample;
        vgmstream->samples_into_block = vgmstream->loop_samples_into_block;
        vgmstream->current_block_size = vgmstream->loop_block_size;
        vgmstream->current_block_samples = vgmstream->loop_block_samples;
        vgmstream->current_block_offset = vgmstream->loop_block_offset;
        vgmstream->next_block_offset = vgmstream->loop_next_block_offset;

        return 1; /* looped */
    }


    /* is this the loop start? */
    if (!vgmstream->hit_loop && vgmstream->current_sample==vgmstream->loop_start_sample) {
        /* save! */
        memcpy(vgmstream->loop_ch,vgmstream->ch,sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);

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
#define TEMPSIZE 256
    char temp[TEMPSIZE];
    const char* description;

    if (!vgmstream) {
        snprintf(temp,TEMPSIZE,
                "NULL VGMSTREAM");
        concatn(length,desc,temp);
        return;
    }

    snprintf(temp,TEMPSIZE,
            "sample rate: %d Hz\n"
            "channels: %d\n",
            vgmstream->sample_rate,
            vgmstream->channels);
    concatn(length,desc,temp);

    if (vgmstream->loop_flag) {
        snprintf(temp,TEMPSIZE,
                "loop start: %d samples (%.4f seconds)\n"
                "loop end: %d samples (%.4f seconds)\n",
                vgmstream->loop_start_sample,
                (double)vgmstream->loop_start_sample/vgmstream->sample_rate,
                vgmstream->loop_end_sample,
                (double)vgmstream->loop_end_sample/vgmstream->sample_rate);
        concatn(length,desc,temp);
    }

    snprintf(temp,TEMPSIZE,
            "stream total samples: %d (%.4f seconds)\n",
            vgmstream->num_samples,
            (double)vgmstream->num_samples/vgmstream->sample_rate);
    concatn(length,desc,temp);

    snprintf(temp,TEMPSIZE,
            "encoding: ");
    concatn(length,desc,temp);
    switch (vgmstream->coding_type) {
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg: {
            ffmpeg_codec_data *data = (ffmpeg_codec_data *) vgmstream->codec_data;
            if (vgmstream->codec_data) {
                if (data->codec && data->codec->long_name) {
                    snprintf(temp,TEMPSIZE,"%s",data->codec->long_name);
                } else if (data->codec && data->codec->name) {
                    snprintf(temp,TEMPSIZE,"%s",data->codec->name);
                } else {
                    snprintf(temp,TEMPSIZE,"FFmpeg (unknown codec)");
                }
            } else {
                snprintf(temp,TEMPSIZE,"FFmpeg");
            }
            break;
        }
#endif
        default:
            description = get_vgmstream_coding_description(vgmstream->coding_type);
            if (!description)
                description = "CANNOT DECODE";
            snprintf(temp,TEMPSIZE,"%s",description);
            break;
    }
    concatn(length,desc,temp);

    snprintf(temp,TEMPSIZE,
            "\nlayout: ");
    concatn(length,desc,temp);
    switch (vgmstream->layout_type) {
        default:
            description = get_vgmstream_layout_description(vgmstream->layout_type);
            if (!description)
                description = "INCONCEIVABLE";
            snprintf(temp,TEMPSIZE,"%s",description);
            break;
    }
    concatn(length,desc,temp);

    snprintf(temp,TEMPSIZE,
            "\n");
    concatn(length,desc,temp);

    if (vgmstream->layout_type == layout_interleave) {
        snprintf(temp,TEMPSIZE,
                "interleave: %#x bytes\n",
                (int32_t)vgmstream->interleave_block_size);
        concatn(length,desc,temp);

        if (vgmstream->interleave_last_block_size) {
            snprintf(temp,TEMPSIZE,
                    "interleave last block: %#x bytes\n",
                    (int32_t)vgmstream->interleave_last_block_size);
            concatn(length,desc,temp);
        }
    }

    /* codecs with blocks + headers (there are more, this is a start) */
    if (vgmstream->layout_type == layout_none && vgmstream->interleave_block_size > 0) {
        switch (vgmstream->coding_type) {
            case coding_MSADPCM:
            case coding_MS_IMA:
            case coding_MC3:
            case coding_WWISE_IMA:
            case coding_REF_IMA:
                snprintf(temp,TEMPSIZE,
                        "block size: %#x bytes\n",
                        (int32_t)vgmstream->interleave_block_size);
                concatn(length,desc,temp);
                break;
            default:
                break;
        }
    }

    snprintf(temp,TEMPSIZE,
            "metadata from: ");
    concatn(length,desc,temp);
    switch (vgmstream->meta_type) {
        default:
            description = get_vgmstream_meta_description(vgmstream->meta_type);
            if (!description)
                description = "THEY SHOULD HAVE SENT A POET";
            snprintf(temp,TEMPSIZE,"%s",description);
            break;
    }
    concatn(length,desc,temp);

    snprintf(temp,TEMPSIZE,
            "\nbitrate: %d kbps",
            get_vgmstream_average_bitrate(vgmstream) / 1000);
    concatn(length,desc,temp);

    /* only interesting if more than one */
    if (vgmstream->num_streams > 1) {
        snprintf(temp,TEMPSIZE,
                "\nstream count: %d",
                vgmstream->num_streams);
        concatn(length,desc,temp);
    }

    if (vgmstream->num_streams > 1 && vgmstream->stream_index > 0) {
        snprintf(temp,TEMPSIZE,
                "\nstream index: %d",
                vgmstream->stream_index);
        concatn(length,desc,temp);
    }
    if (vgmstream->stream_name[0] != '\0') {
        snprintf(temp,TEMPSIZE,
                "\nstream name: %s",
                vgmstream->stream_name);
        concatn(length,desc,temp);
    }
}


/* See if there is a second file which may be the second channel, given an already opened mono vgmstream.
 * If a suitable file is found, open it and change opened_vgmstream to a stereo vgmstream. */
static void try_dual_file_stereo(VGMSTREAM * opened_vgmstream, STREAMFILE *streamFile, VGMSTREAM*(*init_vgmstream_function)(STREAMFILE *)) {
    /* filename search pairs for dual file stereo */
    static const char * const dfs_pairs[][2] = {
        {"L","R"},
        {"l","r"},
        {"left","right"},
        {"Left","Right"},
        {".V0",".V1"}, /* Homura (PS2) */
        {".L",".R"}, /* Crash Nitro Racing (PS2) */
        {"_0","_1"}, //unneeded?
    };
    char new_filename[PATH_LIMIT];
    char * ext;
    int dfs_pair = -1; /* -1=no stereo, 0=opened_vgmstream is left, 1=opened_vgmstream is right */
    VGMSTREAM *new_vgmstream = NULL;
    STREAMFILE *dual_streamFile = NULL;
    int i,j, dfs_pair_count;

    if (opened_vgmstream->channels != 1)
        return;

    /* vgmstream's layout stuff currently assumes a single file */
    // fastelbja : no need ... this one works ok with dual file
    //if (opened_vgmstream->layout != layout_none) return;
    //todo force layout_none if layout_interleave?

    streamFile->get_name(streamFile,new_filename,sizeof(new_filename));
    if (strlen(new_filename)<2) return; /* we need at least a base and a name ending to replace */
    
    ext = (char *)filename_extension(new_filename);
    if (ext-new_filename >= 1 && ext[-1]=='.') ext--; /* including "." */

    /* find pair from base name and modify new_filename with the opposite */
    dfs_pair_count = (sizeof(dfs_pairs)/sizeof(dfs_pairs[0]));
    for (i = 0; dfs_pair == -1 && i< dfs_pair_count; i++) {
        for (j=0; dfs_pair==-1 && j<2; j++) {
            const char * this_suffix = dfs_pairs[i][j];
            size_t this_suffix_len = strlen(dfs_pairs[i][j]);
            const char * other_suffix = dfs_pairs[i][j^1];
            size_t other_suffix_len = strlen(dfs_pairs[i][j^1]);

            /* if suffix matches copy opposite to ext pointer (thus to new_filename) */
            if (this_suffix[0] == '.' && strlen(ext) == this_suffix_len) { /* dual extension (ex. Homura PS2) */
                if ( !memcmp(ext,this_suffix,this_suffix_len) ) {
                    dfs_pair = j;
                    memcpy (ext, other_suffix,other_suffix_len); /* overwrite with new extension */
                }
            }
            else { /* dual suffix */
                if ( !memcmp(ext - this_suffix_len,this_suffix,this_suffix_len) ) {
                    dfs_pair = j;
                    memmove(ext + other_suffix_len - this_suffix_len, ext,strlen(ext)+1); /* move the extension and terminator, too */
                    memcpy (ext - this_suffix_len, other_suffix,other_suffix_len); /* overwrite with new suffix */
                }
            }

        }
    }

    /* see if the filename had a suitable L/R-pair name */
    if (dfs_pair == -1)
        goto fail;


    /* try to init other channel (new_filename now has the opposite name) */
    dual_streamFile = streamFile->open(streamFile,new_filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!dual_streamFile) goto fail;

    new_vgmstream = init_vgmstream_function(dual_streamFile); /* use the init that just worked, no other should work */
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
     * Homura PS2 right channel doesn't have loop points so it's ignored */
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
        free(new_vgmstream);
    }

fail:
    return;
}

/* average bitrate helper */
static int get_vgmstream_average_bitrate_channel_count(VGMSTREAM * vgmstream)
{
    //AAX, AIX, ACM?

    if (vgmstream->layout_type==layout_layered) {
        layered_layout_data *data = (layered_layout_data *) vgmstream->layout_data;
        return (data) ? data->layer_count : 0;
    }
#ifdef VGM_USE_VORBIS
    if (vgmstream->coding_type==coding_OGG_VORBIS) {
        ogg_vorbis_codec_data *data = (ogg_vorbis_codec_data *) vgmstream->codec_data;
        return (data) ? 1 : 0;
    }
#endif
    if (vgmstream->coding_type==coding_CRI_HCA) {
        hca_codec_data *data = (hca_codec_data *) vgmstream->codec_data;
        return (data) ? 1 : 0;
    }
#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type==coding_FFmpeg) {
        ffmpeg_codec_data *data = (ffmpeg_codec_data *) vgmstream->codec_data;
        return (data) ? 1 : 0;
    }
#endif
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type==coding_MP4_AAC) {
        mp4_aac_codec_data *data = (mp4_aac_codec_data *) vgmstream->codec_data;
        return (data) ? 1 : 0;
    }
#endif
    return vgmstream->channels;
}

/* average bitrate helper */
static STREAMFILE * get_vgmstream_average_bitrate_channel_streamfile(VGMSTREAM * vgmstream, int channel)
{
    //AAX, AIX?

    if (vgmstream->coding_type==coding_NWA) {
        nwa_codec_data *data = (nwa_codec_data *) vgmstream->codec_data;
        if (data && data->nwa)
        return data->nwa->file;
    }

    if (vgmstream->coding_type==coding_ACM) {
        acm_codec_data *data = (acm_codec_data *) vgmstream->codec_data;
        if (data && data->file)
        return data->file->streamfile;
    }

#ifdef VGM_USE_VORBIS
    if (vgmstream->coding_type==coding_OGG_VORBIS) {
        ogg_vorbis_codec_data *data = (ogg_vorbis_codec_data *) vgmstream->codec_data;
        return data->ov_streamfile.streamfile;
    }
#endif
    if (vgmstream->coding_type==coding_CRI_HCA) {
        hca_codec_data *data = (hca_codec_data *) vgmstream->codec_data;
        return data->streamfile;
    }
#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type==coding_FFmpeg) {
        ffmpeg_codec_data *data = (ffmpeg_codec_data *) vgmstream->codec_data;
        return data->streamfile;
    }
#endif
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type==coding_MP4_AAC) {
        mp4_aac_codec_data *data = (mp4_aac_codec_data *) vgmstream->codec_data;
        return data->if_file.streamfile;
    }
#endif
    return vgmstream->ch[channel].streamfile;
}

static int get_vgmstream_average_bitrate_from_size(size_t size, int sample_rate, int length_samples) {
    return (int)((int64_t)size * 8 * sample_rate / length_samples);
}
static int get_vgmstream_average_bitrate_from_streamfile(STREAMFILE * streamfile, int sample_rate, int length_samples) {
    return get_vgmstream_average_bitrate_from_size(get_streamfile_size(streamfile), sample_rate, length_samples);
}

/* Return the average bitrate in bps of all unique files contained within this stream. */
int get_vgmstream_average_bitrate(VGMSTREAM * vgmstream) {
    char path_current[PATH_LIMIT];
    char path_compare[PATH_LIMIT];

    unsigned int i, j;
    int bitrate = 0;
    int sample_rate = vgmstream->sample_rate;
    int length_samples = vgmstream->num_samples;
    int channels;
    STREAMFILE * streamFile;

    if (!sample_rate || !length_samples)
        return 0;

    /* subsongs need to report this to properly calculate */
    if (vgmstream->stream_size) {
        return get_vgmstream_average_bitrate_from_size(vgmstream->stream_size, sample_rate, length_samples);
    }

    /* segmented layout is handled differently as it has multiple sub-VGMSTREAMs (may include special codecs) */
    //todo not correct with multifile segments (ex. .ACM Ogg)
    if (vgmstream->layout_type==layout_segmented) {
        segmented_layout_data *data = (segmented_layout_data *) vgmstream->layout_data;
        return get_vgmstream_average_bitrate(data->segments[0]);
    }
    if (vgmstream->layout_type==layout_layered) {
        layered_layout_data *data = (layered_layout_data *) vgmstream->layout_data;
        return get_vgmstream_average_bitrate(data->layers[0]);
    }


    channels = get_vgmstream_average_bitrate_channel_count(vgmstream);
    if (!channels) return 0;

    if (channels >= 1) {
        streamFile = get_vgmstream_average_bitrate_channel_streamfile(vgmstream, 0);
        if (streamFile) {
            bitrate += get_vgmstream_average_bitrate_from_streamfile(streamFile, sample_rate, length_samples);
        }
    }

    /* Compares files by absolute paths, so bitrate doesn't multiply when the same STREAMFILE is reopened per channel */
    for (i = 1; i < channels; ++i) {
        streamFile = get_vgmstream_average_bitrate_channel_streamfile(vgmstream, i);
        if (!streamFile)
            continue;
        streamFile->get_name(streamFile, path_current, sizeof(path_current));
        for (j = 0; j < i; ++j) {
            STREAMFILE * compareFile = get_vgmstream_average_bitrate_channel_streamfile(vgmstream, j);
            if (!compareFile)
                continue;
            streamFile->get_name(compareFile, path_compare, sizeof(path_compare));
            if (!strcmp(path_current, path_compare))
                break;
        }
        if (j == i)
            bitrate += get_vgmstream_average_bitrate_from_streamfile(streamFile, sample_rate, length_samples);
    }

    return bitrate;
}


/**
 * Inits vgmstream, doing two things:
 * - sets the starting offset per channel (depending on the layout)
 * - opens its own streamfile from on a base one. One streamfile per channel may be open (to improve read/seeks).
 * Should be called in metas before returning the VGMSTREAM.
 */
int vgmstream_open_stream(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t start_offset) {
    STREAMFILE * file;
    char filename[PATH_LIMIT];
    int ch;
    int use_streamfile_per_channel = 0;
    int use_same_offset_per_channel = 0;


    /* stream/offsets not needed, managed by layout */
    if (vgmstream->layout_type == layout_aix ||
        vgmstream->layout_type == layout_segmented ||
        vgmstream->layout_type == layout_layered)
        return 1;

    /* stream/offsets not needed, managed by decoder */
    if (vgmstream->coding_type == coding_NWA)
        return 1;

#ifdef VGM_USE_FFMPEG
    /* stream/offsets not needed, managed by decoder */
    if (vgmstream->coding_type == coding_FFmpeg)
        return 1;
#endif

    /* if interleave is big enough keep a buffer per channel */
    if (vgmstream->interleave_block_size * vgmstream->channels >= STREAMFILE_DEFAULT_BUFFER_SIZE) {
        use_streamfile_per_channel = 1;
    }

    /* for mono or codecs like IMA (XBOX, MS IMA, MS ADPCM) where channels work with the same bytes */
    if (vgmstream->layout_type == layout_none) {
        use_same_offset_per_channel = 1;
    }


    streamFile->get_name(streamFile,filename,sizeof(filename));
    /* open the file for reading by each channel */
    {
        if (!use_streamfile_per_channel) {
            file = streamFile->open(streamFile,filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
            if (!file) goto fail;
        }

        for (ch=0; ch < vgmstream->channels; ch++) {
            off_t offset;
            if (use_same_offset_per_channel) {
                offset = start_offset;
            } else {
                offset = start_offset + vgmstream->interleave_block_size*ch;
            }

            /* open new one if needed */
            if (use_streamfile_per_channel) {
                file = streamFile->open(streamFile,filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
                if (!file) goto fail;
            }

            vgmstream->ch[ch].streamfile = file;
            vgmstream->ch[ch].channel_start_offset =
                    vgmstream->ch[ch].offset = offset;
        }
    }

    /* EA-MT decoder is a bit finicky and needs this when channel offsets change */
    if (vgmstream->coding_type == coding_EA_MT) {
        flush_ea_mt(vgmstream);
    }

    return 1;

fail:
    /* open streams will be closed in close_vgmstream(), hopefully called by the meta */
    return 0;
}
