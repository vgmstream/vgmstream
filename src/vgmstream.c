#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "formats.h"
#include "vgmstream.h"
#include "meta/meta.h"
#include "layout/layout.h"
#include "coding/coding.h"

/* See if there is a second file which may be the second channel, given
 * already opened mono opened_stream which was opened from filename.
 * If a suitable file is found, open it and change opened_stream to a stereo stream. */
static void try_dual_file_stereo(VGMSTREAM * opened_stream, STREAMFILE *streamFile);


/*
 * List of functions that will recognize files.
 */
VGMSTREAM * (*init_vgmstream_fcns[])(STREAMFILE *streamFile) = {
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
    init_vgmstream_ngc_mdsp_std,
	init_vgmstream_ngc_dsp_csmp,
    init_vgmstream_Cstr,
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
    init_vgmstream_xbox_xwav,
    init_vgmstream_ngc_str,
    init_vgmstream_ea,
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
	init_vgmstream_akb,
#endif
    init_vgmstream_sadb,
    init_vgmstream_ps2_bmdx,
    init_vgmstream_wsi,
    init_vgmstream_aifc,
    init_vgmstream_str_snds,
    init_vgmstream_ws_aud,
#ifdef VGM_USE_MPEG
    init_vgmstream_ahx,
#endif
    init_vgmstream_ivb,
    init_vgmstream_amts,
    init_vgmstream_svs,
    init_vgmstream_riff,
    init_vgmstream_rifx,
    init_vgmstream_pos,
    init_vgmstream_nwa,
    init_vgmstream_eacs,
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
    init_vgmstream_xa30,
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
    init_vgmstream_dvi,
    init_vgmstream_kcey,
    init_vgmstream_ps2_rstm,
    init_vgmstream_acm,
    init_vgmstream_mus_acm,
    init_vgmstream_ps2_kces,
    init_vgmstream_ps2_dxh,
    init_vgmstream_ps2_psh,
    init_vgmstream_pcm_scd,
	init_vgmstream_pcm_ps2,
    init_vgmstream_ps2_rkv,
    init_vgmstream_ps2_psw,
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
    init_vgmstream_xbox_stma,
    init_vgmstream_xbox_matx,
    init_vgmstream_de2,
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
    //init_vgmstream_idsp,
    init_vgmstream_idsp2,
    init_vgmstream_idsp3,
    init_vgmstream_idsp4,
    init_vgmstream_ngc_ymf,
    init_vgmstream_sadl,
    init_vgmstream_ps2_ccc,
    init_vgmstream_psx_fag,
    init_vgmstream_ps2_mihb,
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
    init_vgmstream_waa_wac_wad_wam,
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
    init_vgmstream_ss_stream,
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
    init_vgmstream_ps2_stm,
    init_vgmstream_myspd,
    init_vgmstream_his,
	init_vgmstream_ps2_ast,
	init_vgmstream_dmsg,
    init_vgmstream_ngc_dsp_aaap,
    init_vgmstream_ngc_dsp_konami,
    init_vgmstream_ps2_ster,
    init_vgmstream_ps2_wb,
    init_vgmstream_bnsf,
#ifdef VGM_USE_G7221
    init_vgmstream_s14_sss,
#endif
    init_vgmstream_ps2_gcm,
    init_vgmstream_ps2_smpl,
    init_vgmstream_ps2_msa,
    init_vgmstream_ps2_voi,
    init_vgmstream_ps2_khv,
    init_vgmstream_pc_smp,
    init_vgmstream_ngc_bo2,
    init_vgmstream_dsp_ddsp,
    init_vgmstream_p3d,
	init_vgmstream_ps2_tk1,
	init_vgmstream_ps2_adsc,
    init_vgmstream_ngc_dsp_mpds,
    init_vgmstream_dsp_str_ig,
    init_vgmstream_psx_mgav,
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
    init_vgmstream_gh3_bar,
    init_vgmstream_ffw,
    init_vgmstream_dsp_dspw,
    init_vgmstream_ps2_jstm,
    init_vgmstream_ps3_xvag,
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
    init_vgmstream_xnbm,
	init_vgmstream_rsd6oogv,
	init_vgmstream_ubi_ckd,
	init_vgmstream_ps2_vbk,
	init_vgmstream_otm,
	init_vgmstream_bcstm,
	init_vgmstream_3ds_idsp,
    init_vgmstream_kt_g1l,
    init_vgmstream_kt_wiibgm,
    init_vgmstream_hca,
    init_vgmstream_ps2_svag_snk,
    init_vgmstream_ps2_vds_vdm,
    init_vgmstream_x360_cxs,
    init_vgmstream_dsp_adx,
    init_vgmstream_akb_multi,
    init_vgmstream_akb2_multi,
    init_vgmstream_x360_ast,
    init_vgmstream_wwise,
    init_vgmstream_ubi_raki,
    init_vgmstream_x360_pasx,
    init_vgmstream_x360_nub,
    init_vgmstream_xma,
    init_vgmstream_sxd,
    init_vgmstream_ogl,
    init_vgmstream_mc3,
    init_vgmstream_gtd,
    init_vgmstream_rsd6xma,
    init_vgmstream_ta_aac_x360,
    init_vgmstream_ta_aac_ps3,
    init_vgmstream_ps3_mta2,
    init_vgmstream_ngc_ulw,

#ifdef VGM_USE_FFMPEG
    init_vgmstream_mp4_aac_ffmpeg,
    init_vgmstream_bik,

    init_vgmstream_ffmpeg, /* should go at the end */
#endif
};


/* internal version with all parameters */
VGMSTREAM * init_vgmstream_internal(STREAMFILE *streamFile, int do_dfs) {
    int i, fcns_size;
    
    if (!streamFile)
        return NULL;

    fcns_size = (sizeof(init_vgmstream_fcns)/sizeof(init_vgmstream_fcns[0]));
    /* try a series of formats, see which works */
    for (i=0; i < fcns_size; i++) {
        /* call init function and see if valid VGMSTREAM was returned */
        VGMSTREAM * vgmstream = (init_vgmstream_fcns[i])(streamFile);
        if (vgmstream) {
            /* these are little hacky checks */

            /* fail if there is nothing to play (without this check vgmstream can generate empty files) */
            if (vgmstream->num_samples <= 0) {
                VGM_LOG("VGMSTREAM: wrong num_samples (ns=%i / 0x%08x)\n", vgmstream->num_samples, vgmstream->num_samples);
                close_vgmstream(vgmstream);
                continue;
            }

            /* everything should have a reasonable sample rate (a verification of the metadata) */
            if (!check_sample_rate(vgmstream->sample_rate)) {
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

            /* dual file stereo */
            if (do_dfs && (
                        (vgmstream->meta_type == meta_DSP_STD) ||
                        (vgmstream->meta_type == meta_PS2_VAGp) ||
                        (vgmstream->meta_type == meta_GENH) ||
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
                        ) && vgmstream->channels == 1) {
                try_dual_file_stereo(vgmstream, streamFile);
            }

#ifdef VGM_USE_FFMPEG
            /* check FFmpeg streams here, for lack of a better place */
            if (vgmstream->coding_type == coding_FFmpeg) {
                ffmpeg_codec_data *data = (ffmpeg_codec_data *) vgmstream->codec_data;
                if (data->streamCount && !vgmstream->num_streams) {
                    vgmstream->num_streams = data->streamCount;
                }
            }
#endif

            /* save start things so we can restart for seeking */
            /* copy the channels */
            memcpy(vgmstream->start_ch,vgmstream->ch,sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
            /* copy the whole VGMSTREAM */
            memcpy(vgmstream->start_vgmstream,vgmstream,sizeof(VGMSTREAM));

            return vgmstream;
        }
    }

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
    return init_vgmstream_internal(streamFile,1);
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
    if (vgmstream->coding_type==coding_ogg_vorbis) {
        reset_ogg_vorbis(vgmstream);
    }

    if (vgmstream->coding_type==coding_fsb_vorbis) {
        reset_fsb_vorbis(vgmstream);
    }

    if (vgmstream->coding_type==coding_wwise_vorbis) {
        reset_wwise_vorbis(vgmstream);
    }

    if (vgmstream->coding_type==coding_ogl_vorbis) {
        reset_ogl_vorbis(vgmstream);
    }
#endif

    if (vgmstream->coding_type==coding_CRI_HCA) {
        reset_hca(vgmstream);
    }

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
	if (vgmstream->coding_type==coding_MP4_AAC) {
	    reset_mp4_aac(vgmstream);
	}
#endif

#ifdef VGM_USE_MPEG
    if (vgmstream->layout_type==layout_mpeg ||
        vgmstream->layout_type==layout_fake_mpeg) {
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
    
#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type==coding_FFmpeg) {
        reset_ffmpeg(vgmstream);
    }
#endif

    if (vgmstream->coding_type==coding_ACM) {
        mus_acm_codec_data *data = vgmstream->codec_data;
        int i;

        data->current_file = 0;
        for (i=0;i<data->file_count;i++) {
            acm_reset(data->files[i]);
        }
    }

    if (vgmstream->layout_type==layout_aix) {
        aix_codec_data *data = vgmstream->codec_data;
        int i;

        data->current_segment = 0;
        for (i=0;i<data->segment_count*data->stream_count;i++)
        {
            reset_vgmstream(data->adxs[i]);
        }
    }

    if (vgmstream->layout_type==layout_aax) {
        aax_codec_data *data = vgmstream->codec_data;
        int i;

        data->current_segment = 0;
        for (i=0;i<data->segment_count;i++)
        {
            reset_vgmstream(data->adxs[i]);
        }
    }

    if (
            vgmstream->coding_type == coding_NWA0 ||
            vgmstream->coding_type == coding_NWA1 ||
            vgmstream->coding_type == coding_NWA2 ||
            vgmstream->coding_type == coding_NWA3 ||
            vgmstream->coding_type == coding_NWA4 ||
            vgmstream->coding_type == coding_NWA5
       ) {
        nwa_codec_data *data = vgmstream->codec_data;
        reset_nwa(data->nwa);
    }

    if (vgmstream->layout_type==layout_scd_int) {
        scd_int_codec_data *data = vgmstream->codec_data;
        int i;

        for (i=0;i<data->substream_count;i++)
        {
            reset_vgmstream(data->substreams[i]);
        }
    }

}

/* simply allocate memory for the VGMSTREAM and its channels */
VGMSTREAM * allocate_vgmstream(int channel_count, int looped) {
    VGMSTREAM * vgmstream;
    VGMSTREAM * start_vgmstream;
    VGMSTREAMCHANNEL * channels;
    VGMSTREAMCHANNEL * start_channels;
    VGMSTREAMCHANNEL * loop_channels;

    if (channel_count <= 0) return NULL;

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
    int i,j;
    if (!vgmstream)
        return;

#ifdef VGM_USE_VORBIS
    if (vgmstream->coding_type==coding_ogg_vorbis) {
        free_ogg_vorbis(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }

    if (vgmstream->coding_type==coding_fsb_vorbis) {
        free_fsb_vorbis(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }

    if (vgmstream->coding_type==coding_wwise_vorbis) {
        free_wwise_vorbis(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }

    if (vgmstream->coding_type==coding_ogl_vorbis) {
        free_ogl_vorbis(vgmstream->codec_data);
        vgmstream->codec_data = NULL;
    }
#endif

    if (vgmstream->coding_type==coding_CRI_HCA) {
        free_hca(vgmstream->codec_data);
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
    if (vgmstream->layout_type==layout_fake_mpeg ||
        vgmstream->layout_type==layout_mpeg) {
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

    if (vgmstream->coding_type==coding_ACM) {
        mus_acm_codec_data *data = (mus_acm_codec_data *) vgmstream->codec_data;

        if (data) {
            if (data->files) {
                int i;
                for (i=0; i<data->file_count; i++) {
                    /* shouldn't be duplicates */
                    if (data->files[i]) {
                        acm_close(data->files[i]);
                        data->files[i] = NULL;
                    }
                }
                free(data->files);
                data->files = NULL;
            }

            free(vgmstream->codec_data);
            vgmstream->codec_data = NULL;
        }
    }

    if (vgmstream->layout_type==layout_aix) {
        aix_codec_data *data = (aix_codec_data *) vgmstream->codec_data;

        if (data) {
            if (data->adxs) {
                int i;
                for (i=0;i<data->segment_count*data->stream_count;i++) {

                    /* note that the AIX close_streamfile won't do anything but
                     * deallocate itself, there is only one open file and that
                     * is in vgmstream->ch[0].streamfile  */
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
    if (vgmstream->layout_type==layout_aax) {
        aax_codec_data *data = (aax_codec_data *) vgmstream->codec_data;

        if (data) {
            if (data->adxs) {
                int i;
                for (i=0;i<data->segment_count;i++) {

                    /* note that the AAX close_streamfile won't do anything but
                     * deallocate itself, there is only one open file and that
                     * is in vgmstream->ch[0].streamfile  */
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

    if (
            vgmstream->coding_type == coding_NWA0 ||
            vgmstream->coding_type == coding_NWA1 ||
            vgmstream->coding_type == coding_NWA2 ||
            vgmstream->coding_type == coding_NWA3 ||
            vgmstream->coding_type == coding_NWA4 ||
            vgmstream->coding_type == coding_NWA5
       ) {
        nwa_codec_data *data = (nwa_codec_data *) vgmstream->codec_data;
        close_nwa(data->nwa);
        free(data);

        vgmstream->codec_data = NULL;
    }

    if (vgmstream->layout_type==layout_scd_int) {
        scd_int_codec_data *data = (scd_int_codec_data *) vgmstream->codec_data;

        if (data) {
            if (data->substreams) {
                int i;
                for (i=0;i<data->substream_count;i++) {

                    /* note that the scd_int close_streamfile won't do anything 
                     * but deallocate itself, there is only one open file and
                     * that is in vgmstream->ch[0].streamfile  */
                    close_vgmstream(data->substreams[i]);
                    if(data->intfiles[i]) close_streamfile(data->intfiles[i]);
                }
                free(data->substreams);
                free(data->intfiles);
            }

            free(data);
        }
        vgmstream->codec_data = NULL;
    }

    /* now that the special cases have had their chance, clean up the standard items */
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
            vgmstream->loop_target = loop_count;
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

/* decode data into sample buffer */
void render_vgmstream(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    switch (vgmstream->layout_type) {
        case layout_interleave:
        case layout_interleave_shortblock:
            render_vgmstream_interleave(buffer,sample_count,vgmstream);
            break;
#ifdef VGM_USE_VORBIS
        case layout_ogg_vorbis:
#endif
#ifdef VGM_USE_MPEG
        case layout_fake_mpeg:
        case layout_mpeg:
#endif
        case layout_none:
            render_vgmstream_nolayout(buffer,sample_count,vgmstream);
            break;
		case layout_mxch_blocked:
        case layout_ast_blocked:
        case layout_halpst_blocked:
        case layout_xa_blocked:
        case layout_ea_blocked:
        case layout_eacs_blocked:
        case layout_caf_blocked:
        case layout_wsi_blocked:
        case layout_str_snds_blocked:
        case layout_ws_aud_blocked:
        case layout_matx_blocked:
        case layout_de2_blocked:
        case layout_vs_blocked:
        case layout_emff_ps2_blocked:
        case layout_emff_ngc_blocked:
        case layout_gsb_blocked:
        case layout_xvas_blocked:
        case layout_thp_blocked:
        case layout_filp_blocked:
        case layout_ivaud_blocked:
        case layout_psx_mgav_blocked:
        case layout_ps2_adm_blocked:
        case layout_dsp_bdsp_blocked:
		case layout_tra_blocked:
		case layout_ps2_iab_blocked:
        case layout_ps2_strlr_blocked:
        case layout_rws_blocked:
            render_vgmstream_blocked(buffer,sample_count,vgmstream);
            break;
        case layout_interleave_byte:
            render_vgmstream_interleave_byte(buffer,sample_count,vgmstream);
            break;
        case layout_acm:
        case layout_mus_acm:
            render_vgmstream_mus_acm(buffer,sample_count,vgmstream);
            break;
        case layout_aix:
            render_vgmstream_aix(buffer,sample_count,vgmstream);
            break;
        case layout_aax:
            render_vgmstream_aax(buffer,sample_count,vgmstream);
            break;
        case layout_scd_int:
            render_vgmstream_scd_int(buffer,sample_count,vgmstream);
            break;
    }
}

/* get the size in samples of a single frame (1 or N channels), for interleaved/blocked layouts */
int get_vgmstream_samples_per_frame(VGMSTREAM * vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_CRI_ADX:
        case coding_CRI_ADX_fixed:
        case coding_CRI_ADX_exp:
        case coding_CRI_ADX_enc_8:
        case coding_CRI_ADX_enc_9:
			return (vgmstream->interleave_block_size - 2) * 2;
        case coding_L5_555:
            return 32;
        case coding_NGC_DSP:
            return 14;
        case coding_PCM16LE:
        case coding_PCM16LE_int:
        case coding_PCM16LE_XOR_int:
        case coding_PCM16BE:
        case coding_PCM8:
        case coding_PCM8_U:
        case coding_PCM8_int:
        case coding_PCM8_SB_int:
        case coding_PCM8_U_int:
        case coding_ULAW:
#ifdef VGM_USE_VORBIS
        case coding_ogg_vorbis:
        case coding_fsb_vorbis:
        case coding_wwise_vorbis:
        case coding_ogl_vorbis:
#endif
#ifdef VGM_USE_MPEG
        case coding_fake_MPEG2_L2:
        case coding_MPEG1_L1:
        case coding_MPEG1_L2:
        case coding_MPEG1_L3:
        case coding_MPEG2_L1:
        case coding_MPEG2_L2:
        case coding_MPEG2_L3:
        case coding_MPEG25_L1:
        case coding_MPEG25_L2:
        case coding_MPEG25_L3:
#endif
        case coding_SDX2:
        case coding_SDX2_int:
        case coding_CBD2:
        case coding_ACM:
        case coding_NWA0:
        case coding_NWA1:
        case coding_NWA2:
        case coding_NWA3:
        case coding_NWA4:
        case coding_NWA5:
        case coding_SASSC:
            return 1;
        case coding_NDS_IMA:
        case coding_DAT4_IMA:
                return (vgmstream->interleave_block_size-4)*2;
        case coding_NGC_DTK:
            return 28;
        case coding_G721:
        case coding_DVI_IMA:
        case coding_EACS_IMA:
        case coding_SNDS_IMA:
        case coding_IMA:
        case coding_OTNS_IMA:
            return 1;
        case coding_IMA_int:
        case coding_DVI_IMA_int:
        case coding_AICA:
            return 2;
        case coding_NGC_AFC:
            return 16;
        case coding_PSX:
        case coding_PSX_badflags:
        case coding_PSX_bmdx:
        case coding_HEVAG:
        case coding_XA:
            return 28;
        case coding_PSX_cfg:
            return (vgmstream->interleave_block_size - 1) * 2; /* decodes 1 byte into 2 bytes */
        case coding_XBOX:
		case coding_XBOX_int:
        case coding_FSB_IMA:
            return 64;
        case coding_EA_XA:
            return 28;
		case coding_MAXIS_ADPCM:
        case coding_EA_ADPCM:
			return 14*vgmstream->channels;
        case coding_WS:
            /* only works if output sample size is 8 bit, which always is for WS ADPCM */
            return vgmstream->ws_output_size;
        case coding_MSADPCM:
            return (vgmstream->interleave_block_size-(7-1)*vgmstream->channels)*2/vgmstream->channels;
        case coding_APPLE_IMA4:
            return 64;
        case coding_MS_IMA:
        case coding_RAD_IMA:
        case coding_WWISE_IMA:
            return (vgmstream->interleave_block_size-4*vgmstream->channels)*2/vgmstream->channels;
        case coding_RAD_IMA_mono:
            return 32;
        case coding_NDS_PROCYON:
            return 30;
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
        {
            ffmpeg_codec_data *data = (ffmpeg_codec_data *) vgmstream->codec_data;
            if (data) { 
	            /* must know the full block size for edge loops */
                return data->sampleBufferBlock;
            }
            return 0;
        }
            break;
#endif
        case coding_LSF:
            return 54;
        case coding_MTAF:
            return 0x80*2;
        case coding_MTA2:
            return 0x80*2;
        case coding_MC3:
            return 10;
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
        default:
            return 0;
    }
}

int get_vgmstream_samples_per_shortframe(VGMSTREAM * vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_NDS_IMA:
            return (vgmstream->interleave_smallblock_size-4)*2;
        default:
            return get_vgmstream_samples_per_frame(vgmstream);
    }
}

/* get the data size of a single frame (1 or N channels), for interleaved/blocked layouts */
int get_vgmstream_frame_size(VGMSTREAM * vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_CRI_ADX:
        case coding_CRI_ADX_fixed:
        case coding_CRI_ADX_exp:
        case coding_CRI_ADX_enc_8:
        case coding_CRI_ADX_enc_9:
            return vgmstream->interleave_block_size;
        case coding_L5_555:
            return 18;
        case coding_NGC_DSP:
            return 8;
        case coding_PCM16LE:
        case coding_PCM16LE_int:
        case coding_PCM16LE_XOR_int:
        case coding_PCM16BE:
            return 2;
        case coding_PCM8:
        case coding_PCM8_U:
        case coding_PCM8_int:
        case coding_PCM8_SB_int:
        case coding_PCM8_U_int:
        case coding_ULAW:
        case coding_SDX2:
        case coding_SDX2_int:
        case coding_CBD2:
        case coding_NWA0:
        case coding_NWA1:
        case coding_NWA2:
        case coding_NWA3:
        case coding_NWA4:
        case coding_NWA5:
        case coding_SASSC:
            return 1;
        case coding_MS_IMA:
        case coding_RAD_IMA:
        case coding_NDS_IMA:
        case coding_DAT4_IMA:
        case coding_WWISE_IMA:
            return vgmstream->interleave_block_size;
        case coding_RAD_IMA_mono:
            return 0x14;
        case coding_NGC_DTK:
            return 32;
        case coding_EACS_IMA:
            return 1;
        case coding_DVI_IMA:
        case coding_IMA:
        case coding_G721:
        case coding_SNDS_IMA:
        case coding_OTNS_IMA:
            return 0;
        case coding_NGC_AFC:
            return 9;
        case coding_PSX:
        case coding_PSX_badflags:
        case coding_PSX_bmdx:
        case coding_HEVAG:
        case coding_NDS_PROCYON:
            return 16;
        case coding_PSX_cfg:
            return vgmstream->interleave_block_size;
        case coding_XA:
            return 14*vgmstream->channels;
        case coding_XBOX:
		case coding_XBOX_int:
        case coding_FSB_IMA:
            return 36;
		case coding_MAXIS_ADPCM:
			return 15*vgmstream->channels;
        case coding_EA_ADPCM:
            return 30;
        case coding_EA_XA:
            return 1; // the frame is variant in size
        case coding_WS:
            return vgmstream->current_block_size;
        case coding_IMA_int:
        case coding_DVI_IMA_int:
        case coding_AICA:
            return 1; 
        case coding_APPLE_IMA4:
            return 34;
        case coding_LSF:
            return 28;
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
        case coding_MSADPCM:
        case coding_MTAF:
            return vgmstream->interleave_block_size;
        case coding_MTA2:
            return 0x90;
        case coding_MC3:
            return 4;
        default:
            return 0;
    }
}

int get_vgmstream_shortframe_size(VGMSTREAM * vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_NDS_IMA:
            return vgmstream->interleave_smallblock_size;
        default:
            return get_vgmstream_frame_size(vgmstream);
    }
}

void decode_vgmstream_mem(VGMSTREAM * vgmstream, int samples_written, int samples_to_do, sample * buffer, uint8_t * data, int channel) {

    switch (vgmstream->coding_type) {
        case coding_NGC_DSP:
            decode_ngc_dsp_mem(&vgmstream->ch[channel],
                    buffer+samples_written*vgmstream->channels+channel,
                    vgmstream->channels,vgmstream->samples_into_block,
                    samples_to_do, data);
            break;
        default:
            break;
    }
}

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
        case coding_PCM16LE:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm16LE(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCM16LE_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm16LE_int(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCM16LE_XOR_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm16LE_XOR_int(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
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
        case coding_XBOX:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_xbox_ima(vgmstream,&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_XBOX_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_int_xbox_ima(vgmstream,&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
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
        case coding_PSX_bmdx:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_psx_bmdx(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
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
        case coding_EA_ADPCM:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ea_adpcm(vgmstream,buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_MAXIS_ADPCM:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_maxis_adpcm(vgmstream,buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
#ifdef VGM_USE_VORBIS
        case coding_ogg_vorbis:
            decode_ogg_vorbis(vgmstream->codec_data,
                    buffer+samples_written*vgmstream->channels,samples_to_do,
                    vgmstream->channels);
            break;

        case coding_fsb_vorbis:
            decode_fsb_vorbis(vgmstream,
                    buffer+samples_written*vgmstream->channels,samples_to_do,
                    vgmstream->channels);
            break;

        case coding_wwise_vorbis:
            decode_wwise_vorbis(vgmstream,
                    buffer+samples_written*vgmstream->channels,samples_to_do,
                    vgmstream->channels);
            break;

        case coding_ogl_vorbis:
            decode_ogl_vorbis(vgmstream,
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
        case coding_DVI_IMA:
        case coding_DVI_IMA_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_dvi_ima(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_EACS_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_eacs_ima(vgmstream,buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_IMA:
        case coding_IMA_int:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ima(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
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

        case coding_WS:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ws(vgmstream,chan,buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;

#ifdef VGM_USE_MPEG
        case coding_fake_MPEG2_L2:
            decode_fake_mpeg2_l2(
                    &vgmstream->ch[0],
                    vgmstream->codec_data,
                    buffer+samples_written*vgmstream->channels,samples_to_do);
            break;
        case coding_MPEG1_L1:
        case coding_MPEG1_L2:
        case coding_MPEG1_L3:
        case coding_MPEG2_L1:
        case coding_MPEG2_L2:
        case coding_MPEG2_L3:
        case coding_MPEG25_L1:
        case coding_MPEG25_L2:
        case coding_MPEG25_L3:
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
        case coding_ACM:
            /* handled in its own layout, here to quiet compiler */
            break;
        case coding_NWA0:
        case coding_NWA1:
        case coding_NWA2:
        case coding_NWA3:
        case coding_NWA4:
        case coding_NWA5:
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
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_aica(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
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
    }
}

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

/* loop if end sample is reached, and return 1 if we did loop */
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
            vgmstream->coding_type == coding_PSX_bmdx ||
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

#ifdef VGM_USE_VORBIS
        if (vgmstream->coding_type==coding_ogg_vorbis) {
            seek_ogg_vorbis(vgmstream, vgmstream->loop_sample);
        }

        if (vgmstream->coding_type==coding_fsb_vorbis) {
            seek_fsb_vorbis(vgmstream, vgmstream->loop_start_sample);
        }

        if (vgmstream->coding_type==coding_wwise_vorbis) {
            seek_wwise_vorbis(vgmstream, vgmstream->loop_start_sample);
        }

        if (vgmstream->coding_type==coding_ogl_vorbis) {
            seek_ogl_vorbis(vgmstream, vgmstream->loop_start_sample);
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

#ifdef VGM_USE_MPEG
        if (vgmstream->layout_type==layout_mpeg) {
            seek_mpeg(vgmstream, vgmstream->loop_sample); /* won't work for fake MPEG */
        }
#endif

        if (vgmstream->coding_type == coding_NWA0 ||
            vgmstream->coding_type == coding_NWA1 ||
            vgmstream->coding_type == coding_NWA2 ||
            vgmstream->coding_type == coding_NWA3 ||
            vgmstream->coding_type == coding_NWA4 ||
            vgmstream->coding_type == coding_NWA5)
        {
            nwa_codec_data *data = vgmstream->codec_data;
            seek_nwa(data->nwa, vgmstream->loop_sample);
        }

        /* restore! */
        memcpy(vgmstream->ch,vgmstream->loop_ch,sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
        vgmstream->current_sample = vgmstream->loop_sample;
        vgmstream->samples_into_block = vgmstream->loop_samples_into_block;
        vgmstream->current_block_size = vgmstream->loop_block_size;
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
        vgmstream->loop_block_offset = vgmstream->current_block_offset;
        vgmstream->loop_next_block_offset = vgmstream->next_block_offset;
        vgmstream->hit_loop = 1;
    }

    return 0; /* not looped */
}

/* build a descriptive string */
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

    if (vgmstream->layout_type == layout_interleave
            || vgmstream->layout_type == layout_interleave_shortblock
            || vgmstream->layout_type == layout_interleave_byte) {
        snprintf(temp,TEMPSIZE,
                "interleave: %#x bytes\n",
                (int32_t)vgmstream->interleave_block_size);
        concatn(length,desc,temp);

        if (vgmstream->layout_type == layout_interleave_shortblock) {
            snprintf(temp,TEMPSIZE,
                    "last block interleave: %#x bytes\n",
                    (int32_t)vgmstream->interleave_smallblock_size);
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

    /* only interesting if more than one */
    if (vgmstream->num_streams > 1) {
        snprintf(temp,TEMPSIZE,
                "\nnumber of streams: %d",
                vgmstream->num_streams);
        concatn(length,desc,temp);
    }
}

/* filename search pairs for dual file stereo */
const char * const dfs_pairs[][2] = {
    {"L","R"},
    {"l","r"},
    {"_0","_1"},
    {"left","right"},
    {"Left","Right"},
};
#define DFS_PAIR_COUNT (sizeof(dfs_pairs)/sizeof(dfs_pairs[0]))

static void try_dual_file_stereo(VGMSTREAM * opened_stream, STREAMFILE *streamFile) {
    char filename[PATH_LIMIT];
    char filename2[PATH_LIMIT];
    char * ext;
    int dfs_name= -1; /*-1=no stereo, 0=opened_stream is left, 1=opened_stream is right */
    VGMSTREAM * new_stream = NULL;
    STREAMFILE *dual_stream = NULL;
    int i,j;

    if (opened_stream->channels != 1) return;
    
    streamFile->get_name(streamFile,filename,sizeof(filename));

    /* vgmstream's layout stuff currently assumes a single file */
    // fastelbja : no need ... this one works ok with dual file
    //if (opened_stream->layout != layout_none) return;

    /* we need at least a base and a name ending to replace */
    if (strlen(filename)<2) return;

    strcpy(filename2,filename);

    /* look relative to the extension; */
    ext = (char *)filename_extension(filename2);

    /* we treat the . as part of the extension */
    if (ext-filename2 >= 1 && ext[-1]=='.') ext--;

    for (i=0; dfs_name==-1 && i<DFS_PAIR_COUNT; i++) {
        for (j=0; dfs_name==-1 && j<2; j++) {
            /* find a postfix on the name */
            if (!memcmp(ext-strlen(dfs_pairs[i][j]),
                        dfs_pairs[i][j],
                        strlen(dfs_pairs[i][j]))) {
                int other_name=j^1;
                int moveby;
                dfs_name=j;

                /* move the extension */
                moveby = strlen(dfs_pairs[i][other_name]) -
                    strlen(dfs_pairs[i][dfs_name]);
                memmove(ext+moveby,ext,strlen(ext)+1); /* terminator, too */

                /* make the new name */
                memcpy(ext+moveby-strlen(dfs_pairs[i][other_name]),dfs_pairs[i][other_name],strlen(dfs_pairs[i][other_name]));
            }
        }
    }

    /* did we find a name for the other file? */
    if (dfs_name==-1) goto fail;

#if 0
    printf("input is:            %s\n"
            "other file would be: %s\n",
            filename,filename2);
#endif

    dual_stream = streamFile->open(streamFile,filename2,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!dual_stream) goto fail;

    new_stream = init_vgmstream_internal(dual_stream,
            0   /* don't do dual file on this, to prevent recursion */
            );
    close_streamfile(dual_stream);

    /* see if we were able to open the file, and if everything matched nicely */
    if (new_stream &&
            new_stream->channels == 1 &&
            /* we have seen legitimate pairs where these are off by one... */
            /* but leaving it commented out until I can find those and recheck */
            /* abs(new_stream->num_samples-opened_stream->num_samples <= 1) && */
            new_stream->num_samples == opened_stream->num_samples &&
            new_stream->sample_rate == opened_stream->sample_rate &&
            new_stream->meta_type == opened_stream->meta_type &&
            new_stream->coding_type == opened_stream->coding_type &&
            new_stream->layout_type == opened_stream->layout_type &&
            new_stream->loop_flag == opened_stream->loop_flag &&
            /* check these even if there is no loop, because they should then
             * be zero in both */
            new_stream->loop_start_sample == opened_stream->loop_start_sample &&
            new_stream->loop_end_sample == opened_stream->loop_end_sample &&
            /* check even if the layout doesn't use them, because it is
             * difficult to determine when it does, and they should be zero
             * otherwise, anyway */
            new_stream->interleave_block_size == opened_stream->interleave_block_size &&
            new_stream->interleave_smallblock_size == opened_stream->interleave_smallblock_size) {
        /* We seem to have a usable, matching file. Merge in the second channel. */
        VGMSTREAMCHANNEL * new_chans;
        VGMSTREAMCHANNEL * new_loop_chans = NULL;
        VGMSTREAMCHANNEL * new_start_chans = NULL;

        /* build the channels */
        new_chans = calloc(2,sizeof(VGMSTREAMCHANNEL));
        if (!new_chans) goto fail;

        memcpy(&new_chans[dfs_name],&opened_stream->ch[0],sizeof(VGMSTREAMCHANNEL));
        memcpy(&new_chans[dfs_name^1],&new_stream->ch[0],sizeof(VGMSTREAMCHANNEL));

        /* loop and start will be initialized later, we just need to
         * allocate them here */
        new_start_chans = calloc(2,sizeof(VGMSTREAMCHANNEL));
        if (!new_start_chans) {
            free(new_chans);
            goto fail;
        }

        if (opened_stream->loop_ch) {
            new_loop_chans = calloc(2,sizeof(VGMSTREAMCHANNEL));
            if (!new_loop_chans) {
                free(new_chans);
                free(new_start_chans);
                goto fail;
            }
        }

        /* remove the existing structures */
        /* not using close_vgmstream as that would close the file */
        free(opened_stream->ch);
        free(new_stream->ch);

        free(opened_stream->start_ch);
        free(new_stream->start_ch);

        if (opened_stream->loop_ch) {
            free(opened_stream->loop_ch);
            free(new_stream->loop_ch);
        }

        /* fill in the new structures */
        opened_stream->ch = new_chans;
        opened_stream->start_ch = new_start_chans;
        opened_stream->loop_ch = new_loop_chans;

        /* stereo! */
        opened_stream->channels = 2;

        /* discard the second VGMSTREAM */
        free(new_stream);
    }
fail:
    return;
}

static int get_vgmstream_channel_count(VGMSTREAM * vgmstream)
{
    if (vgmstream->layout_type==layout_scd_int) {
        scd_int_codec_data *data = (scd_int_codec_data *) vgmstream->codec_data;
        if (data) {
            return data->substream_count;
        }
        else {
            return 0;
        }
    }
#ifdef VGM_USE_VORBIS
    if (vgmstream->coding_type==coding_ogg_vorbis) {
        ogg_vorbis_codec_data *data = (ogg_vorbis_codec_data *) vgmstream->codec_data;

        if (data) {
            return 1;
        }
        else {
            return 0;
        }
    }
#endif
    if (vgmstream->coding_type==coding_CRI_HCA) {
        hca_codec_data *data = (hca_codec_data *) vgmstream->codec_data;
        
        if (data) {
            return 1;
        }
        else {
            return 0;
        }
    }
#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type==coding_FFmpeg) {
        ffmpeg_codec_data *data = (ffmpeg_codec_data *) vgmstream->codec_data;
        
        if (data) {
            return 1;
        }
        else {
            return 0;
        }
    }
#endif
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type==coding_MP4_AAC) {
        mp4_aac_codec_data *data = (mp4_aac_codec_data *) vgmstream->codec_data;
        if (data) {
            return 1;
        }
        else {
            return 0;
        }
    }
#endif
    return vgmstream->channels;
}

static STREAMFILE * get_vgmstream_streamfile(VGMSTREAM * vgmstream, int channel)
{
    if (vgmstream->layout_type==layout_scd_int) {
        scd_int_codec_data *data = (scd_int_codec_data *) vgmstream->codec_data;
        return data->intfiles[channel];
    }
#ifdef VGM_USE_VORBIS
    if (vgmstream->coding_type==coding_ogg_vorbis) {
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

static int get_vgmstream_channel_average_bitrate(STREAMFILE * streamfile, int sample_rate, int length_samples)
{
    return (int)((int64_t)get_streamfile_size(streamfile) * 8 * sample_rate / length_samples);
}

int get_vgmstream_average_bitrate(VGMSTREAM * vgmstream)
{
    char path_current[PATH_LIMIT];
    char path_compare[PATH_LIMIT];
    
    unsigned int i, j;
    int bitrate = 0;
    int sample_rate = vgmstream->sample_rate;
    int length_samples = vgmstream->num_samples;
    int channels = get_vgmstream_channel_count(vgmstream);
    STREAMFILE * streamFile;

    if (!sample_rate || !channels || !length_samples)
        return 0;
    
    if (channels >= 1) {
        streamFile = get_vgmstream_streamfile(vgmstream, 0);
        if (streamFile) {
            bitrate += get_vgmstream_channel_average_bitrate(streamFile, sample_rate, length_samples);
        }
    }

    for (i = 1; i < channels; ++i)
    {
        streamFile = get_vgmstream_streamfile(vgmstream, i);
        if (!streamFile)
            continue;
        streamFile->get_name(streamFile, path_current, sizeof(path_current));
        for (j = 0; j < i; ++j)
        {
            STREAMFILE * compareFile = get_vgmstream_streamfile(vgmstream, j);
            if (!compareFile)
                continue;
            streamFile->get_name(compareFile, path_compare, sizeof(path_compare));
            if (!strcmp(path_current, path_compare))
                break;
        }
        if (j == i)
            bitrate += get_vgmstream_channel_average_bitrate(streamFile, sample_rate, length_samples);
    }
    
    return bitrate;
}


/**
 * Inits vgmstreams' channels doing two things:
 * - sets the starting offset per channel (depending on the layout)
 * - opens its own streamfile from on a base one. One streamfile per channel may be open (to improve read/seeks).
 * Should be called in metas before returning the VGMSTREAM..
 */
int vgmstream_open_stream(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t start_offset) {
    STREAMFILE * file;
    char filename[PATH_LIMIT];
    int ch;
    int use_streamfile_per_channel = 0;
    int use_same_offset_per_channel = 0;


    /* stream/offsets not needed, scd manages itself */
    if (vgmstream->layout_type == layout_scd_int)
        return 1;

#ifdef VGM_USE_FFMPEG
    /* stream/offsets not needed, FFmpeg manages itself */
    if (vgmstream->coding_type == coding_FFmpeg)
        return 1;
#endif

    /* if interleave is big enough keep a buffer per channel */
    if (vgmstream->interleave_block_size >= STREAMFILE_DEFAULT_BUFFER_SIZE) {
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

    return 1;

fail:
    /* open streams will be closed in close_vgmstream(), hopefully called by the meta */
    return 0;
}
