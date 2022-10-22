/*
 * vgmstream.h - definitions for VGMSTREAM, encapsulating a multi-channel, looped audio stream
 */

#ifndef _VGMSTREAM_H
#define _VGMSTREAM_H

/* reasonable limits */
enum { 
    /* Windows generally only allows 260 chars in path, but other OSs have higher limits, and we handle
     * UTF-8 (that typically uses 2-bytes for common non-latin codepages) plus player may append protocols
     * to paths, so it should be a bit higher. Most people wouldn't use huge paths though. */
    PATH_LIMIT = 4096, /* (256 * 8) * 2 = ~max_path * (other_os+extra) * codepage_bytes */
    STREAM_NAME_SIZE = 255,
    VGMSTREAM_MAX_CHANNELS = 64,
    VGMSTREAM_MIN_SAMPLE_RATE = 300, /* 300 is Wwise min */
    VGMSTREAM_MAX_SAMPLE_RATE = 192000, /* found in some FSB5 */
    VGMSTREAM_MAX_SUBSONGS = 65535, /* +20000 isn't that uncommon */
    VGMSTREAM_MAX_NUM_SAMPLES = 1000000000, /* no ~5h vgm hopefully */
};

#include "streamfile.h"
#include "util/log.h"

/* Due mostly to licensing issues, Vorbis, MPEG, G.722.1, etc decoding is done by external libraries.
 * Libs are disabled by default, defined on compile-time for builds that support it */
//#define VGM_USE_VORBIS
//#define VGM_USE_MPEG
//#define VGM_USE_G7221
//#define VGM_USE_G719
//#define VGM_USE_MP4V2
//#define VGM_USE_FDKAAC
//#define VGM_USE_MAIATRAC3PLUS
//#define VGM_USE_FFMPEG
//#define VGM_USE_ATRAC9
//#define VGM_USE_CELT
//#define VGM_USE_SPEEX


#ifdef VGM_USE_MP4V2
#define MP4V2_NO_STDINT_DEFS
#include <mp4v2/mp4v2.h>
#endif

#ifdef VGM_USE_FDKAAC
#include <aacdecoder_lib.h>
#endif

#include "coding/g72x_state.h"


/* The encoding type specifies the format the sound data itself takes */
typedef enum {
    coding_SILENCE,         /* generates silence */

    /* PCM */
    coding_PCM16LE,         /* little endian 16-bit PCM */
    coding_PCM16BE,         /* big endian 16-bit PCM */
    coding_PCM16_int,       /* 16-bit PCM with sample-level interleave (for blocks) */

    coding_PCM8,            /* 8-bit PCM */
    coding_PCM8_int,        /* 8-bit PCM with sample-level interleave (for blocks) */
    coding_PCM8_U,          /* 8-bit PCM, unsigned (0x80 = 0) */
    coding_PCM8_U_int,      /* 8-bit PCM, unsigned (0x80 = 0) with sample-level interleave (for blocks) */
    coding_PCM8_SB,         /* 8-bit PCM, sign bit (others are 2's complement) */
    coding_PCM4,            /* 4-bit PCM, signed */
    coding_PCM4_U,          /* 4-bit PCM, unsigned */

    coding_ULAW,            /* 8-bit u-Law (non-linear PCM) */
    coding_ULAW_int,        /* 8-bit u-Law (non-linear PCM) with sample-level interleave (for blocks) */
    coding_ALAW,            /* 8-bit a-Law (non-linear PCM) */

    coding_PCMFLOAT,        /* 32-bit float PCM */
    coding_PCM24LE,         /* 24-bit PCM */

    /* ADPCM */
    coding_CRI_ADX,         /* CRI ADX */
    coding_CRI_ADX_fixed,   /* CRI ADX, encoding type 2 with fixed coefficients */
    coding_CRI_ADX_exp,     /* CRI ADX, encoding type 4 with exponential scale */
    coding_CRI_ADX_enc_8,   /* CRI ADX, type 8 encryption (God Hand) */
    coding_CRI_ADX_enc_9,   /* CRI ADX, type 9 encryption (PSO2) */

    coding_NGC_DSP,         /* Nintendo DSP ADPCM */
    coding_NGC_DSP_subint,  /* Nintendo DSP ADPCM with frame subinterframe */
    coding_NGC_DTK,         /* Nintendo DTK ADPCM (hardware disc), also called TRK or ADP */
    coding_NGC_AFC,         /* Nintendo AFC ADPCM */
    coding_VADPCM,          /* Silicon Graphics VADPCM */

    coding_G721,            /* CCITT G.721 */

    coding_XA,              /* CD-ROM XA 4-bit */
    coding_XA8,             /* CD-ROM XA 8-bit */
    coding_XA_EA,           /* EA's Saturn XA (not to be confused with EA-XA) */
    coding_PSX,             /* Sony PS ADPCM (VAG) */
    coding_PSX_badflags,    /* Sony PS ADPCM with custom flag byte */
    coding_PSX_cfg,         /* Sony PS ADPCM with configurable frame size (int math) */
    coding_PSX_pivotal,     /* Sony PS ADPCM with configurable frame size (float math) */
    coding_HEVAG,           /* Sony PSVita ADPCM */

    coding_EA_XA,           /* Electronic Arts EA-XA ADPCM v1 (stereo) aka "EA ADPCM" */
    coding_EA_XA_int,       /* Electronic Arts EA-XA ADPCM v1 (mono/interleave) */
    coding_EA_XA_V2,        /* Electronic Arts EA-XA ADPCM v2 */
    coding_MAXIS_XA,        /* Maxis EA-XA ADPCM */
    coding_EA_XAS_V0,       /* Electronic Arts EA-XAS ADPCM v0 */
    coding_EA_XAS_V1,       /* Electronic Arts EA-XAS ADPCM v1 */

    coding_IMA,             /* IMA ADPCM (stereo or mono, low nibble first) */
    coding_IMA_int,         /* IMA ADPCM (mono/interleave, low nibble first) */
    coding_DVI_IMA,         /* DVI IMA ADPCM (stereo or mono, high nibble first) */
    coding_DVI_IMA_int,     /* DVI IMA ADPCM (mono/interleave, high nibble first) */
    coding_NW_IMA,
    coding_SNDS_IMA,        /* Heavy Iron Studios .snds IMA ADPCM */
    coding_QD_IMA,
    coding_WV6_IMA,         /* Gorilla Systems WV6 4-bit IMA ADPCM */
    coding_HV_IMA,          /* High Voltage 4-bit IMA ADPCM */
    coding_FFTA2_IMA,       /* Final Fantasy Tactics A2 4-bit IMA ADPCM */
    coding_BLITZ_IMA,       /* Blitz Games 4-bit IMA ADPCM */

    coding_MS_IMA,          /* Microsoft IMA ADPCM */
    coding_MS_IMA_mono,     /* Microsoft IMA ADPCM (mono/interleave) */
    coding_XBOX_IMA,        /* XBOX IMA ADPCM */
    coding_XBOX_IMA_mch,    /* XBOX IMA ADPCM (multichannel) */
    coding_XBOX_IMA_int,    /* XBOX IMA ADPCM (mono/interleave) */
    coding_NDS_IMA,         /* IMA ADPCM w/ NDS layout */
    coding_DAT4_IMA,        /* Eurocom 'DAT4' IMA ADPCM */
    coding_RAD_IMA,         /* Radical IMA ADPCM */
    coding_RAD_IMA_mono,    /* Radical IMA ADPCM (mono/interleave) */
    coding_APPLE_IMA4,      /* Apple Quicktime IMA4 */
    coding_FSB_IMA,         /* FMOD's FSB multichannel IMA ADPCM */
    coding_WWISE_IMA,       /* Audiokinetic Wwise IMA ADPCM */
    coding_REF_IMA,         /* Reflections IMA ADPCM */
    coding_AWC_IMA,         /* Rockstar AWC IMA ADPCM */
    coding_UBI_IMA,         /* Ubisoft IMA ADPCM */
    coding_UBI_SCE_IMA,     /* Ubisoft SCE IMA ADPCM */
    coding_H4M_IMA,         /* H4M IMA ADPCM (stereo or mono, high nibble first) */
    coding_MTF_IMA,         /* Capcom MT Framework IMA ADPCM */
    coding_CD_IMA,          /* Crystal Dynamics IMA ADPCM */

    coding_MSADPCM,         /* Microsoft ADPCM (stereo/mono) */
    coding_MSADPCM_int,     /* Microsoft ADPCM (mono) */
    coding_MSADPCM_ck,      /* Microsoft ADPCM (Cricket Audio variation) */
    coding_WS,              /* Westwood Studios VBR ADPCM */

    coding_AICA,            /* Yamaha AICA ADPCM (stereo) */
    coding_AICA_int,        /* Yamaha AICA ADPCM (mono/interleave) */
    coding_CP_YM,           /* Capcom's Yamaha ADPCM (stereo/mono) */
    coding_ASKA,            /* Aska ADPCM */
    coding_NXAP,            /* NXAP ADPCM */

    coding_TGC,             /* Tiger Game.com 4-bit ADPCM */

    coding_NDS_PROCYON,     /* Procyon Studio ADPCM */
    coding_L5_555,          /* Level-5 0x555 ADPCM */
    coding_LSF,             /* lsf ADPCM (Fastlane Street Racing iPhone)*/
    coding_MTAF,            /* Konami MTAF ADPCM */
    coding_MTA2,            /* Konami MTA2 ADPCM */
    coding_MC3,             /* Paradigm MC3 3-bit ADPCM */
    coding_FADPCM,          /* FMOD FADPCM 4-bit ADPCM */
    coding_ASF,             /* Argonaut ASF 4-bit ADPCM */
    coding_DSA,             /* Ocean DSA 4-bit ADPCM */
    coding_XMD,             /* Konami XMD 4-bit ADPCM */
    coding_TANTALUS,        /* Tantalus 4-bit ADPCM */
    coding_PCFX,            /* PC-FX 4-bit ADPCM */
    coding_OKI16,           /* OKI 4-bit ADPCM with 16-bit output and modified expand */
    coding_OKI4S,           /* OKI 4-bit ADPCM with 16-bit output and cuadruple step */
    coding_PTADPCM,         /* Platinum 4-bit ADPCM */
    coding_IMUSE,           /* LucasArts iMUSE Variable ADPCM */
    coding_COMPRESSWAVE,    /* CompressWave Huffman ADPCM */

    /* others */
    coding_SDX2,            /* SDX2 2:1 Squareroot-Delta-Exact compression DPCM */
    coding_SDX2_int,        /* SDX2 2:1 Squareroot-Delta-Exact compression with sample-level interleave */
    coding_CBD2,            /* CBD2 2:1 Cuberoot-Delta-Exact compression DPCM */
    coding_CBD2_int,        /* CBD2 2:1 Cuberoot-Delta-Exact compression, with sample-level interleave */
    coding_SASSC,           /* Activision EXAKT SASSC 8-bit DPCM */
    coding_DERF,            /* DERF 8-bit DPCM */
    coding_WADY,            /* WADY 8-bit DPCM */
    coding_NWA,             /* VisualArt's NWA DPCM */
    coding_ACM,             /* InterPlay ACM */
    coding_CIRCUS_ADPCM,    /* Circus 8-bit ADPCM */
    coding_UBI_ADPCM,       /* Ubisoft 4/6-bit ADPCM */

    coding_EA_MT,           /* Electronic Arts MicroTalk (linear-predictive speech codec) */
    coding_CIRCUS_VQ,       /* Circus VQ */
    coding_RELIC,           /* Relic Codec (DCT-based) */
    coding_CRI_HCA,         /* CRI High Compression Audio (MDCT-based) */
    coding_TAC,             /* tri-Ace Codec (MDCT-based) */
    coding_ICE_RANGE,       /* Inti Creates "range" codec */
    coding_ICE_DCT,         /* Inti Creates "DCT" codec */

#ifdef VGM_USE_VORBIS
    coding_OGG_VORBIS,      /* Xiph Vorbis with Ogg layer (MDCT-based) */
    coding_VORBIS_custom,   /* Xiph Vorbis with custom layer (MDCT-based) */
#endif

#ifdef VGM_USE_MPEG
    coding_MPEG_custom,     /* MPEG audio with custom features (MDCT-based) */
    coding_MPEG_ealayer3,   /* EALayer3, custom MPEG frames */
    coding_MPEG_layer1,     /* MP1 MPEG audio (MDCT-based) */
    coding_MPEG_layer2,     /* MP2 MPEG audio (MDCT-based) */
    coding_MPEG_layer3,     /* MP3 MPEG audio (MDCT-based) */
#endif

#ifdef VGM_USE_G7221
    coding_G7221C,          /* ITU G.722.1 annex C (Polycom Siren 14) */
#endif

#ifdef VGM_USE_G719
    coding_G719,            /* ITU G.719 annex B (Polycom Siren 22) */
#endif

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    coding_MP4_AAC,         /* AAC (MDCT-based) */
#endif

#ifdef VGM_USE_MAIATRAC3PLUS
    coding_AT3plus,         /* Sony ATRAC3plus (MDCT-based) */
#endif

#ifdef VGM_USE_ATRAC9
    coding_ATRAC9,          /* Sony ATRAC9 (MDCT-based) */
#endif

#ifdef VGM_USE_CELT
    coding_CELT_FSB,        /* Custom Xiph CELT (MDCT-based) */
#endif

#ifdef VGM_USE_SPEEX
    coding_SPEEX,           /* Custom Speex (CELP-based) */
#endif

#ifdef VGM_USE_FFMPEG
    coding_FFmpeg,          /* Formats handled by FFmpeg (ATRAC3, XMA, AC3, etc) */
#endif
} coding_t;

/* The layout type specifies how the sound data is laid out in the file */
typedef enum {
    /* generic */
    layout_none,            /* straight data */

    /* interleave */
    layout_interleave,      /* equal interleave throughout the stream */

    /* headered blocks */
    layout_blocked_ast,
    layout_blocked_halpst,
    layout_blocked_xa,
    layout_blocked_ea_schl,
    layout_blocked_ea_1snh,
    layout_blocked_caf,
    layout_blocked_wsi,
    layout_blocked_str_snds,
    layout_blocked_ws_aud,
    layout_blocked_matx,
    layout_blocked_dec,
    layout_blocked_xvas,
    layout_blocked_vs,
    layout_blocked_mul,
    layout_blocked_gsb,
    layout_blocked_thp,
    layout_blocked_filp,
    layout_blocked_ea_swvr,
    layout_blocked_adm,
    layout_blocked_bdsp,
    layout_blocked_mxch,
    layout_blocked_ivaud,   /* GTA IV .ivaud blocks */
    layout_blocked_tra,     /* DefJam Rapstar .tra blocks */
    layout_blocked_ps2_iab,
    layout_blocked_vs_str,
    layout_blocked_rws,
    layout_blocked_hwas,
    layout_blocked_ea_sns,  /* newest Electronic Arts blocks, found in SNS/SNU/SPS/etc formats */
    layout_blocked_awc,     /* Rockstar AWC */
    layout_blocked_vgs,     /* Guitar Hero II (PS2) */
    layout_blocked_xwav,
    layout_blocked_xvag_subsong, /* XVAG subsongs [God of War III (PS4)] */
    layout_blocked_ea_wve_au00, /* EA WVE au00 blocks */
    layout_blocked_ea_wve_ad10, /* EA WVE Ad10 blocks */
    layout_blocked_sthd, /* Dream Factory STHD */
    layout_blocked_h4m, /* H4M video */
    layout_blocked_xa_aiff, /* XA in AIFF files [Crusader: No Remorse (SAT), Road Rash (3DO)] */
    layout_blocked_vs_square,
    layout_blocked_vid1,
    layout_blocked_ubi_sce,
    layout_blocked_tt_ad,

    /* otherwise odd */
    layout_segmented,       /* song divided in segments (song sections) */
    layout_layered,         /* song divided in layers (song channels) */

} layout_t;

/* The meta type specifies how we know what we know about the file.
 * We may know because of a header we read, some of it may have been guessed from filenames, etc. */
typedef enum {
    meta_SILENCE,

    meta_DSP_STD,           /* Nintendo standard GC ADPCM (DSP) header */
    meta_DSP_CSTR,          /* Star Fox Assault "Cstr" */
    meta_DSP_RS03,          /* Retro: Metroid Prime 2 "RS03" */
    meta_DSP_STM,           /* Paper Mario 2 STM */
    meta_AGSC,              /* Retro: Metroid Prime 2 title */
    meta_CSMP,              /* Retro: Metroid Prime 3 (Wii), Donkey Kong Country Returns (Wii) */
    meta_RFRM,              /* Retro: Donkey Kong Country Tropical Freeze (Wii U) */
    meta_DSP_MPDSP,         /* Monopoly Party single header stereo */
    meta_DSP_JETTERS,       /* Bomberman Jetters .dsp */
    meta_DSP_MSS,           /* Free Radical GC games */
    meta_DSP_GCM,           /* some of Traveller's Tales games */
    meta_DSP_STR,           /* Conan .str files */
    meta_DSP_SADB,          /* .sad */
    meta_DSP_WSI,           /* .wsi */
    meta_IDSP_TT,           /* Traveller's Tales games */
    meta_DSP_WII_MUS,       /* .mus */
    meta_DSP_WII_WSD,       /* Phantom Brave (WII) */
    meta_WII_NDP,           /* Vertigo (Wii) */
    meta_DSP_YGO,           /* Konami: Yu-Gi-Oh! The Falsebound Kingdom (NGC), Hikaru no Go 3 (NGC) */

    meta_STRM,              /* Nintendo STRM */
    meta_RSTM,              /* Nintendo RSTM (Revolution Stream, similar to STRM) */
    meta_AFC,               /* AFC */
    meta_AST,               /* AST */
    meta_RWSD,              /* single-stream RWSD */
    meta_RWAR,              /* single-stream RWAR */
    meta_RWAV,              /* contents of RWAR */
    meta_CWAV,              /* contents of CWAR */
    meta_FWAV,              /* contents of FWAR */
    meta_RSTM_SPM,          /* RSTM with 44->22khz hack */
    meta_THP,               /* THP movie files */
    meta_RSTM_shrunken,     /* Atlus' mutant shortened RSTM */
    meta_SWAV,
    meta_NDS_RRDS,          /* Ridge Racer DS */
    meta_WII_BNS,           /* Wii BNS Banner Sound (similar to RSTM) */
    meta_WIIU_BTSND,        /* Wii U Boot Sound */

    meta_ADX_03,            /* CRI ADX "type 03" */
    meta_ADX_04,            /* CRI ADX "type 04" */
    meta_ADX_05,            /* CRI ADX "type 05" */
    meta_AIX,               /* CRI AIX */
    meta_AAX,               /* CRI AAX */
    meta_UTF_DSP,           /* CRI ADPCM_WII, like AAX with DSP */

    meta_DTK,
    meta_RSF,
    meta_HALPST,            /* HAL Labs HALPST */
    meta_GCSW,              /* GCSW (PCM) */
    meta_CAF,               /* tri-Crescendo CAF */
    meta_MYSPD,             /* U-Sing .myspd */
    meta_HIS,               /* Her Ineractive .his */
    meta_BNSF,              /* Bandai Namco Sound Format */

    meta_XA,                /* CD-ROM XA */
    meta_ADS,
    meta_NPS,
    meta_RXWS,
    meta_RAW_INT,
    meta_EXST,
    meta_SVAG_KCET,
    meta_PS_HEADERLESS,     /* headerless PS-ADPCM */
    meta_MIB_MIH,
    meta_PS2_MIC,           /* KOEI MIC File */
    meta_PS2_VAGi,          /* VAGi Interleaved File */
    meta_PS2_VAGp,          /* VAGp Mono File */
    meta_PS2_pGAV,          /* VAGp with Little Endian Header */
    meta_PS2_VAGp_AAAP,     /* Acclaim Austin Audio VAG header */
    meta_SEB,
    meta_STR_WAV,           /* Blitz Games STR+WAV files */
    meta_ILD,
    meta_PS2_PNB,           /* PsychoNauts Bgm File */
    meta_VPK,               /* VPK Audio File */
    meta_PS2_BMDX,          /* Beatmania thing */
    meta_PS2_IVB,           /* Langrisser 3 IVB */
    meta_PS2_SND,           /* some Might & Magics SSND header */
    meta_SVS,               /* Square SVS */
    meta_XSS,               /* Dino Crisis 3 */
    meta_SL3,               /* Test Drive Unlimited */
    meta_HGC1,              /* Knights of the Temple 2 */
    meta_AUS,               /* Various Capcom games */
    meta_RWS,               /* RenderWare games (only when using RW Audio middleware) */
    meta_FSB1,              /* FMOD Sample Bank, version 1 */
    meta_FSB2,              /* FMOD Sample Bank, version 2 */
    meta_FSB3,              /* FMOD Sample Bank, version 3.0/3.1 */
    meta_FSB4,              /* FMOD Sample Bank, version 4 */
    meta_FSB5,              /* FMOD Sample Bank, version 5 */
    meta_RWX,               /* Air Force Delta Storm (XBOX) */
    meta_XWB,               /* Microsoft XACT framework (Xbox, X360, Windows) */
    meta_PS2_XA30,          /* Driver - Parallel Lines (PS2) */
    meta_MUSC,              /* Krome PS2 games */
    meta_MUSX,
    meta_LEG,               /* Legaia 2 [no header_id] */
    meta_FILP,              /* Resident Evil - Dead Aim */
    meta_IKM,
    meta_STER,
    meta_BG00,              /* Ibara, Mushihimesama */
    meta_PS2_RSTM,          /* Midnight Club 3 */
    meta_PS2_KCES,          /* Dance Dance Revolution */
    meta_HXD,
    meta_VSV,
    meta_SCD_PCM,           /* Lunar - Eternal Blue */
    meta_PS2_PCM,           /* Konami KCEJ East: Ephemeral Fantasia, Yu-Gi-Oh! The Duelists of the Roses, 7 Blades */
    meta_PS2_RKV,           /* Legacy of Kain - Blood Omen 2 (PS2) */
    meta_PS2_VAS,           /* Pro Baseball Spirits 5 */
    meta_PS2_ENTH,          /* Enthusia */
    meta_SDT,               /* Baldur's Gate - Dark Alliance */
    meta_NGC_TYDSP,         /* Ty - The Tasmanian Tiger */
    meta_DC_STR,            /* SEGA Stream Asset Builder */
    meta_DC_STR_V2,         /* variant of SEGA Stream Asset Builder */
    meta_NGC_BH2PCM,        /* Bio Hazard 2 */
    meta_SAP,
    meta_DC_IDVI,           /* Eldorado Gate */
    meta_KRAW,              /* Geometry Wars - Galaxies */
    meta_PS2_OMU,           /* PS2 Int file with Header */
    meta_PS2_XA2,           /* XG3 Extreme-G Racing */
    meta_NUB,
    meta_IDSP_NL,           /* Mario Strikers Charged (Wii) */
    meta_IDSP_IE,           /* Defencer (GC) */
    meta_SPT_SPD,           /* Various (SPT+SPT DSP) */
    meta_ISH_ISD,           /* Various (ISH+ISD DSP) */
    meta_GSP_GSB,           /* Tecmo games (Super Swing Golf 1 & 2, Quamtum Theory) */
    meta_YDSP,              /* WWE Day of Reckoning */
    meta_FFCC_STR,          /* Final Fantasy: Crystal Chronicles */
    meta_UBI_JADE,          /* Beyond Good & Evil, Rayman Raving Rabbids */
    meta_GCA,               /* Metal Slug Anthology */
    meta_NGC_SSM,           /* Golden Gashbell Full Power */
    meta_PS2_JOE,           /* Wall-E / Pixar games */
    meta_NGC_YMF,           /* WWE WrestleMania X8 */
    meta_SADL,
    meta_PS2_CCC,           /* Tokyo Xtreme Racer DRIFT 2 */
    meta_FAG,               /* Jackie Chan - Stuntmaster */
    meta_PS2_MIHB,          /* Merged MIH+MIB */
    meta_NGC_PDT,           /* Mario Party 6 */
    meta_DC_ASD,            /* Miss Moonligh */
    meta_NAOMI_SPSD,        /* Guilty Gear X */
    meta_RSD,
    meta_PS2_ASS,           /* ASS */
    meta_SEG,               /* Eragon */
    meta_NDS_STRM_FFTA2,    /* Final Fantasy Tactics A2 */
    meta_KNON,
    meta_ZWDSP,             /* Zack and Wiki */
    meta_VGS,               /* Guitar Hero Encore - Rocks the 80s */
    meta_DCS_WAV,
    meta_SMP,
    meta_WII_SNG,           /* Excite Trucks */
    meta_MUL,
    meta_SAT_BAKA,          /* Crypt Killer */
    meta_VSF,
    meta_PS2_VSF_TTA,       /* Tiny Toon Adventures: Defenders of the Universe */
    meta_ADS_MIDWAY,
    meta_PS2_SPS,           /* Ape Escape 2 */
    meta_PS2_XA2_RRP,       /* RC Revenge Pro */
    meta_NGC_DSP_KONAMI,    /* Konami DSP header, found in various games */
    meta_UBI_CKD,           /* Ubisoft CKD RIFF header (Rayman Origins Wii) */
    meta_RAW_WAVM,
    meta_WVS,
    meta_XBOX_MATX,         /* XBOX MATX */
    meta_XMU,
    meta_XVAS,
    meta_EA_SCHL,           /* Electronic Arts SCHl with variable header */
    meta_EA_SCHL_fixed,     /* Electronic Arts SCHl with fixed header */
    meta_EA_BNK,            /* Electronic Arts BNK */
    meta_EA_1SNH,           /* Electronic Arts 1SNh/EACS */
    meta_EA_EACS,
    meta_RAW_PCM,
    meta_GENH,              /* generic header */
    meta_AIFC,              /* Audio Interchange File Format AIFF-C */
    meta_AIFF,              /* Audio Interchange File Format */
    meta_STR_SNDS,          /* .str with SNDS blocks and SHDR header */
    meta_WS_AUD,            /* Westwood Studios .aud */
    meta_WS_AUD_old,        /* Westwood Studios .aud, old style */
    meta_RIFF_WAVE,         /* RIFF, for WAVs */
    meta_RIFF_WAVE_POS,     /* .wav + .pos for looping (Ys Complete PC) */
    meta_RIFF_WAVE_labl,    /* RIFF w/ loop Markers in LIST-adtl-labl */
    meta_RIFF_WAVE_smpl,    /* RIFF w/ loop data in smpl chunk */
    meta_RIFF_WAVE_wsmp,    /* RIFF w/ loop data in wsmp chunk */
    meta_RIFF_WAVE_MWV,     /* .mwv RIFF w/ loop data in ctrl chunk pflt */
    meta_RIFX_WAVE,         /* RIFX, for big-endian WAVs */
    meta_RIFX_WAVE_smpl,    /* RIFX w/ loop data in smpl chunk */
    meta_XNB,               /* XNA Game Studio 4.0 */
    meta_PC_MXST,           /* Lego Island MxSt */
    meta_SAB,               /* Worms 4 Mayhem SAB+SOB file */
    meta_NWA,               /* Visual Art's NWA */
    meta_NWA_NWAINFOINI,    /* Visual Art's NWA w/ NWAINFO.INI for looping */
    meta_NWA_GAMEEXEINI,    /* Visual Art's NWA w/ Gameexe.ini for looping */
    meta_SAT_DVI,           /* Konami KCE Nagoya DVI (SAT games) */
    meta_DC_KCEY,           /* Konami KCE Yokohama KCEYCOMP (DC games) */
    meta_ACM,               /* InterPlay ACM header */
    meta_MUS_ACM,           /* MUS playlist of InterPlay ACM files */
    meta_DEC,               /* Falcom PC games (Xanadu Next, Gurumin) */
    meta_VS,                /* Men in Black .vs */
    meta_FFXI_BGW,          /* FFXI (PC) BGW */
    meta_FFXI_SPW,          /* FFXI (PC) SPW */
    meta_STS,
    meta_PS2_P2BT,          /* Pop'n'Music 7 Audio File */
    meta_PS2_GBTS,          /* Pop'n'Music 9 Audio File */
    meta_NGC_DSP_IADP,      /* Gamecube Interleave DSP */
    meta_PS2_TK5,           /* Tekken 5 Stream Files */
    meta_PS2_MCG,           /* Gunvari MCG Files (was name .GCM on disk) */
    meta_ZSD,               /* Dragon Booster ZSD */
    meta_REDSPARK,          /* "RedSpark" RSD (MadWorld) */
    meta_IVAUD,             /* .ivaud GTA IV */
    meta_NDS_HWAS,          /* Spider-Man 3, Tony Hawk's Downhill Jam, possibly more... */
    meta_NGC_LPS,           /* Rave Master (Groove Adventure Rave)(GC) */
    meta_NAOMI_ADPCM,       /* NAOMI/NAOMI2 ARcade games */
    meta_SD9,               /* beatmaniaIIDX16 - EMPRESS (Arcade) */
    meta_2DX9,              /* beatmaniaIIDX16 - EMPRESS (Arcade) */
    meta_PS2_VGV,           /* Rune: Viking Warlord */
    meta_GCUB,
    meta_MAXIS_XA,          /* Sim City 3000 (PC) */
    meta_NGC_SCK_DSP,       /* Scorpion King (NGC) */
    meta_CAFF,              /* iPhone .caf */
    meta_EXAKT_SC,          /* Activision EXAKT .SC (PS2) */
    meta_WII_WAS,           /* DiRT 2 (WII) */
    meta_PONA_3DO,          /* Policenauts (3DO) */
    meta_PONA_PSX,          /* Policenauts (PSX) */
    meta_XBOX_HLWAV,        /* Half Life 2 (XBOX) */
    meta_AST_MV,
    meta_AST_MMV,
    meta_DMSG,              /* Nightcaster II - Equinox (XBOX) */
    meta_NGC_DSP_AAAP,      /* Turok: Evolution (NGC), Vexx (NGC) */
    meta_PS2_WB,            /* Shooting Love. ~TRIZEAL~ */
    meta_S14,               /* raw Siren 14, 24kbit mono */
    meta_SSS,               /* raw Siren 14, 48kbit stereo */
    meta_PS2_GCM,           /* NamCollection */
    meta_PS2_SMPL,          /* Homura */
    meta_PS2_MSA,           /* Psyvariar -Complete Edition- */
    meta_PS2_VOI,           /* RAW Danger (Zettaizetsumei Toshi 2 - Itetsuita Kiokutachi) [PS2] */
    meta_P3D,               /* Prototype P3D */
    meta_PS2_TK1,           /* Tekken (NamCollection) */
    meta_NGC_RKV,           /* Legacy of Kain - Blood Omen 2 (GC) */
    meta_DSP_DDSP,          /* Various (2 dsp files stuck together */
    meta_NGC_DSP_MPDS,      /* Big Air Freestyle, Terminator 3 */
    meta_DSP_STR_IG,        /* Micro Machines, Superman Superman: Shadow of Apokolis */
    meta_EA_SWVR,           /* Future Cop L.A.P.D., Freekstyle */
    meta_PS2_B1S,           /* 7 Wonders of the ancient world */
    meta_PS2_WAD,           /* The golden Compass */
    meta_DSP_XIII,          /* XIII, possibly more (Ubisoft header???) */
    meta_DSP_CABELAS,       /* Cabelas games */
    meta_PS2_ADM,           /* Dragon Quest V (PS2) */
    meta_LPCM_SHADE,
    meta_DSP_BDSP,          /* Ah! My Goddess */
    meta_PS2_VMS,           /* Autobahn Raser - Police Madness */
    meta_XAU,               /* XPEC Entertainment (Beat Down (PS2 Xbox), Spectral Force Chronicle (PS2)) */
    meta_GH3_BAR,           /* Guitar Hero III Mobile .bar */
    meta_FFW,               /* Freedom Fighters [NGC] */
    meta_DSP_DSPW,          /* Sengoku Basara 3 [WII] */
    meta_PS2_JSTM,          /* Tantei Jinguji Saburo - Kind of Blue (PS2) */
    meta_SQEX_SCD,          /* Square-Enix SCD */
    meta_NGC_NST_DSP,       /* Animaniacs [NGC] */
    meta_BAF,               /* Bizarre Creations (Blur, James Bond) */
    meta_XVAG,              /* Ratchet & Clank Future: Quest for Booty (PS3) */
    meta_PS3_CPS,           /* Eternal Sonata (PS3) */
    meta_MSF,
    meta_PS3_PAST,          /* Bakugan Battle Brawlers (PS3) */
    meta_SGXD,              /* Sony: Folklore, Genji, Tokyo Jungle (PS3), Brave Story, Kurohyo (PSP) */
    meta_WII_RAS,           /* Donkey Kong Country Returns (Wii) */
    meta_SPM,
    meta_X360_TRA,          /* Def Jam Rapstar */
    meta_VGS_PS,
    meta_PS2_IAB,           /* Ueki no Housoku - Taosu ze Robert Juudan!! (PS2) */
    meta_VS_STR,            /* The Bouncer */
    meta_LSF_N1NJ4N,        /* .lsf n1nj4n Fastlane Street Racing (iPhone) */
    meta_XWAV,
    meta_RAW_SNDS,
    meta_PS2_WMUS,          /* The Warriors (PS2) */
    meta_HYPERSCAN_KVAG,    /* Hyperscan KVAG/BVG */
    meta_IOS_PSND,          /* Crash Bandicoot Nitro Kart 2 (iOS) */
    meta_BOS_ADP,
    meta_QD_ADP,
    meta_EB_SFX,            /* Excitebots .sfx */
    meta_EB_SF0,            /* Excitebots .sf0 */
    meta_MTAF,
    meta_PS2_VAG1,          /* Metal Gear Solid 3 VAG1 */
    meta_PS2_VAG2,          /* Metal Gear Solid 3 VAG2 */
    meta_ALP,
    meta_WPD,               /* Shuffle! (PC) */
    meta_MN_STR,            /* Mini Ninjas (PC/PS3/WII) */
    meta_MSS,               /* Guerilla: ShellShock Nam '67 (PS2/Xbox), Killzone (PS2) */
    meta_PS2_HSF,           /* Lowrider (PS2) */
    meta_IVAG,
    meta_PS2_2PFS,          /* Konami: Mahoromatic: Moetto - KiraKira Maid-San, GANTZ (PS2) */
    meta_PS2_VBK,           /* Disney's Stitch - Experiment 626 */
    meta_OTM,               /* Otomedius (Arcade) */
    meta_CSTM,              /* Nintendo 3DS CSTM (Century Stream) */
    meta_FSTM,              /* Nintendo Wii U FSTM (caFe? Stream) */
    meta_IDSP_NAMCO,
    meta_KT_WIIBGM,         /* Koei Tecmo WiiBGM */
    meta_KTSS,              /* Koei Tecmo Nintendo Stream (KNS) */
    meta_MCA,               /* Capcom MCA "MADP" */
    meta_XB3D_ADX,          /* Xenoblade Chronicles 3D ADX */
    meta_HCA,               /* CRI HCA */
    meta_SVAG_SNK,
    meta_PS2_VDS_VDM,       /* Graffiti Kingdom */
    meta_FFMPEG,
    meta_FFMPEG_faulty,
    meta_X360_CXS,          /* Eternal Sonata (Xbox 360) */
    meta_AKB,               /* SQEX iOS */
    meta_X360_PASX,         /* Namco PASX (Soul Calibur II HD X360) */
    meta_XMA_RIFF,          /* Microsoft RIFF XMA */
    meta_X360_AST,          /* Dead Rising (X360) */
    meta_WWISE_RIFF,        /* Audiokinetic Wwise RIFF/RIFX */
    meta_UBI_RAKI,          /* Ubisoft RAKI header (Rayman Legends, Just Dance 2017) */
    meta_SXD,               /* Sony SXD (Gravity Rush, Freedom Wars PSV) */
    meta_OGL,               /* Shin'en Wii/WiiU (Jett Rocket (Wii), FAST Racing NEO (WiiU)) */
    meta_MC3,               /* Paradigm games (T3 PS2, MX Rider PS2, MI: Operation Surma PS2) */
    meta_GTD,               /* Knights Contract (X360/PS3), Valhalla Knights 3 (PSV) */
    meta_TA_AAC,
    meta_MTA2,
    meta_NGC_ULW,           /* Burnout 1 (GC only) */
    meta_XA_XA30,
    meta_XA_04SW,
    meta_TXTH,              /* generic text header */
    meta_SK_AUD,            /* Silicon Knights .AUD (Eternal Darkness GC) */
    meta_AHX,               /* CRI AHX header */
    meta_STM,               /* Angel Studios/Rockstar San Diego Games */
    meta_BINK,              /* RAD Game Tools BINK audio/video */
    meta_EA_SNU,            /* Electronic Arts SNU (Dead Space) */
    meta_AWC,               /* Rockstar AWC (GTA5, RDR) */
    meta_OPUS,              /* Nintendo Opus [Lego City Undercover (Switch)] */
    meta_RAW_AL,
    meta_PC_AST,            /* Dead Rising (PC) */
    meta_NAAC,              /* Namco AAC (3DS) */
    meta_UBI_SB,            /* Ubisoft banks */
    meta_EZW,               /* EZ2DJ (Arcade) EZWAV */
    meta_VXN,               /* Gameloft mobile games */
    meta_EA_SNR_SNS,        /* Electronic Arts SNR+SNS (Burnout Paradise) */
    meta_EA_SPS,            /* Electronic Arts SPS (Burnout Crash) */
    meta_VID1,
    meta_PC_FLX,            /* Ultima IX PC */
    meta_MOGG,              /* Harmonix Music Systems MOGG Vorbis */
    meta_OGG_VORBIS,        /* Ogg Vorbis */
    meta_OGG_SLI,           /* Ogg Vorbis file w/ companion .sli for looping */
    meta_OPUS_SLI,          /* Ogg Opus file w/ companion .sli for looping */
    meta_OGG_SFL,           /* Ogg Vorbis file w/ .sfl (RIFF SFPL) for looping */
    meta_OGG_KOVS,          /* Ogg Vorbis with header and encryption (Koei Tecmo Games) */
    meta_OGG_encrypted,     /* Ogg Vorbis with encryption */
    meta_KMA9,              /* Koei Tecmo [Nobunaga no Yabou - Souzou (Vita)] */
    meta_XWC,               /* Starbreeze games */
    meta_SQEX_SAB,          /* Square-Enix newest middleware (sound) */
    meta_SQEX_MAB,          /* Square-Enix newest middleware (music) */
    meta_WAF,               /* KID WAF [Ever 17 (PC)] */
    meta_WAVE,              /* EngineBlack games [Mighty Switch Force! (3DS)] */
    meta_WAVE_segmented,    /* EngineBlack games, segmented [Shantae and the Pirate's Curse (PC)] */
    meta_SMV,               /* Cho Aniki Zero (PSP) */
    meta_NXAP,              /* Nex Entertainment games [Time Crisis 4 (PS3), Time Crisis Razing Storm (PS3)] */
    meta_EA_WVE_AU00,       /* Electronic Arts PS movies [Future Cop - L.A.P.D. (PS), Supercross 2000 (PS)] */
    meta_EA_WVE_AD10,       /* Electronic Arts PS movies [Wing Commander 3/4 (PS)] */
    meta_STHD,              /* STHD .stx [Kakuto Chojin (Xbox)] */
    meta_MP4,               /* MP4/AAC */
    meta_PCM_SRE,           /* .PCM+SRE [Viewtiful Joe (PS2)] */
    meta_DSP_MCADPCM,       /* Skyrim (Switch) */
    meta_UBI_LYN,           /* Ubisoft LyN engine [The Adventures of Tintin (multi)] */
    meta_MSB_MSH,           /* sfx companion of MIH+MIB */
    meta_TXTP,              /* generic text playlist */
    meta_SMC_SMH,           /* Wangan Midnight (System 246) */
    meta_PPST,              /* PPST [Parappa the Rapper (PSP)] */
    meta_SPS_N1,
    meta_UBI_BAO,           /* Ubisoft BAO */
    meta_DSP_SWITCH_AUDIO,  /* Gal Gun 2 (Switch) */
    meta_H4M,               /* Hudson HVQM4 video [Resident Evil 0 (GC), Tales of Symphonia (GC)] */
    meta_ASF,               /* Argonaut ASF [Croc 2 (PC)] */
    meta_XMD,               /* Konami XMD [Silent Hill 4 (Xbox), Castlevania: Curse of Darkness (Xbox)] */
    meta_CKS,               /* Cricket Audio stream [Part Time UFO (Android), Mega Man 1-6 (Android)] */
    meta_CKB,               /* Cricket Audio bank [Fire Emblem Heroes (Android), Mega Man 1-6 (Android)] */
    meta_WV6,               /* Gorilla Systems PC games */
    meta_WAVEBATCH,         /* Firebrand Games */
    meta_HD3_BD3,           /* Sony PS3 bank */
    meta_BNK_SONY,          /* Sony Scream Tool bank */
    meta_SCD_SSCF,          /* Square Enix SCD old version */
    meta_DSP_VAG,           /* Penny-Punching Princess (Switch) sfx */
    meta_DSP_ITL,           /* Charinko Hero (GC) */
    meta_A2M,               /* Scooby-Doo! Unmasked (PS2) */
    meta_AHV,               /* Headhunter (PS2) */
    meta_MSV,
    meta_SDF,
    meta_SVG,               /* Hunter - The Reckoning - Wayward (PS2) */
    meta_VIS,               /* AirForce Delta Strike (PS2) */
    meta_VAI,               /* Ratatouille (GC) */
    meta_AIF_ASOBO,         /* Ratatouille (PC) */
    meta_AO,                /* Cloudphobia (PC) */
    meta_APC,               /* MegaRace 3 (PC) */
    meta_WV2,               /* Slave Zero (PC) */
    meta_XAU_KONAMI,        /* Yu-Gi-Oh - The Dawn of Destiny (Xbox) */
    meta_DERF,              /* Stupid Invaders (PC) */
    meta_SADF,
    meta_UTK,
    meta_NXA,
    meta_ADPCM_CAPCOM,
    meta_UE4OPUS,
    meta_XWMA,
    meta_VA3,               /* DDR Supernova 2 AC */
    meta_XOPUS,
    meta_VS_SQUARE,
    meta_NWAV,
    meta_XPCM,
    meta_MSF_TAMASOFT,
    meta_XPS_DAT,
    meta_ZSND,
    meta_DSP_ADPY,
    meta_DSP_ADPX,
    meta_OGG_OPUS,
    meta_IMC,
    meta_GIN,
    meta_DSF,
    meta_208,
    meta_DSP_DS2,
    meta_MUS_VC,
    meta_STRM_ABYLIGHT,
    meta_MSF_KONAMI,
    meta_XWMA_KONAMI,
    meta_9TAV,
    meta_BWAV,
    meta_RAD,
    meta_SMACKER,
    meta_MZRT,
    meta_XAVS,
    meta_PSF,
    meta_DSP_ITL_i,
    meta_IMA,
    meta_XMV_VALVE,
    meta_UBI_HX,
    meta_BMP_KONAMI,
    meta_ISB,
    meta_XSSB,
    meta_XMA_UE3,
    meta_FWSE,
    meta_FDA,
    meta_TGC,
    meta_KWB,
    meta_LRMD,
    meta_WWISE_FX,
    meta_DIVA,
    meta_IMUSE,
    meta_KTSR,
    meta_KAT,
    meta_PCM_SUCCESS,
    meta_ADP_KONAMI,
    meta_SDRH,
    meta_WADY,
    meta_DSP_SQEX,
    meta_DSP_WIIVOICE,
    meta_SBK,
    meta_DSP_WIIADPCM,
    meta_DSP_CWAC,
    meta_COMPRESSWAVE,
    meta_KTAC,
    meta_MJB_MJH,
    meta_BSNF,
    meta_TAC,
    meta_IDSP_TOSE,
    meta_DSP_KWA,
    meta_OGV_3RDEYE,
    meta_PIFF_TPCM,
    meta_WXD_WXH,
    meta_BNK_RELIC,
    meta_XSH_XSD_XSS,
    meta_PSB,
    meta_LOPU_FB,
    meta_LPCM_FB,
    meta_WBK,
    meta_WBK_NSLB,
    meta_DSP_APEX,
    meta_MPEG,
    meta_SSPF,
    meta_S3V,
    meta_ESF,
    meta_ADM3,
    meta_TT_AD,
    meta_SNDZ,
    meta_VAB,
    meta_BIGRP,

} meta_t;

/* standard WAVEFORMATEXTENSIBLE speaker positions */
typedef enum {
    speaker_FL  = (1 << 0),     /* front left */
    speaker_FR  = (1 << 1),     /* front right */
    speaker_FC  = (1 << 2),     /* front center */
    speaker_LFE = (1 << 3),     /* low frequency effects */
    speaker_BL  = (1 << 4),     /* back left */
    speaker_BR  = (1 << 5),     /* back right */
    speaker_FLC = (1 << 6),     /* front left center */
    speaker_FRC = (1 << 7),     /* front right center */
    speaker_BC  = (1 << 8),     /* back center */
    speaker_SL  = (1 << 9),     /* side left */
    speaker_SR  = (1 << 10),    /* side right */

    speaker_TC  = (1 << 11),    /* top center*/
    speaker_TFL = (1 << 12),    /* top front left */
    speaker_TFC = (1 << 13),    /* top front center */
    speaker_TFR = (1 << 14),    /* top front right */
    speaker_TBL = (1 << 15),    /* top back left */
    speaker_TBC = (1 << 16),    /* top back center */
    speaker_TBR = (1 << 17),    /* top back left */

} speaker_t;

/* typical mappings that metas may use to set channel_layout (but plugin must actually use it)
 * (in order, so 3ch file could be mapped to FL FR FC or FL FR LFE but not LFE FL FR)
 * not too sure about names but no clear standards */
typedef enum {
    mapping_MONO             = speaker_FC,
    mapping_STEREO           = speaker_FL | speaker_FR,
    mapping_2POINT1          = speaker_FL | speaker_FR | speaker_LFE,
    mapping_2POINT1_xiph     = speaker_FL | speaker_FR | speaker_FC, /* aka 3STEREO? */
    mapping_QUAD             = speaker_FL | speaker_FR | speaker_BL  | speaker_BR,
    mapping_QUAD_surround    = speaker_FL | speaker_FR | speaker_FC  | speaker_BC,
    mapping_QUAD_side        = speaker_FL | speaker_FR | speaker_SL  | speaker_SR,
    mapping_5POINT0          = speaker_FL | speaker_FR | speaker_LFE | speaker_BL | speaker_BR,
    mapping_5POINT0_xiph     = speaker_FL | speaker_FR | speaker_FC  | speaker_BL | speaker_BR,
    mapping_5POINT0_surround = speaker_FL | speaker_FR | speaker_FC  | speaker_SL | speaker_SR,
    mapping_5POINT1          = speaker_FL | speaker_FR | speaker_FC  | speaker_LFE | speaker_BL | speaker_BR,
    mapping_5POINT1_surround = speaker_FL | speaker_FR | speaker_FC  | speaker_LFE | speaker_SL | speaker_SR,
    mapping_7POINT0          = speaker_FL | speaker_FR | speaker_FC  | speaker_LFE | speaker_BC | speaker_FLC | speaker_FRC,
    mapping_7POINT1          = speaker_FL | speaker_FR | speaker_FC  | speaker_LFE | speaker_BL | speaker_BR  | speaker_FLC | speaker_FRC,
    mapping_7POINT1_surround = speaker_FL | speaker_FR | speaker_FC  | speaker_LFE | speaker_BL | speaker_BR  | speaker_SL  | speaker_SR,
} mapping_t;

typedef struct {
    int config_set; /* some of the mods below are set */

    /* modifiers */
    int play_forever;
    int ignore_loop;
    int force_loop;
    int really_force_loop;
    int ignore_fade;

    /* processing */
    double loop_count;
    int32_t pad_begin;
    int32_t trim_begin;
    int32_t body_time;
    int32_t trim_end;
    double fade_delay; /* not in samples for backwards compatibility */
    double fade_time;
    int32_t pad_end;

    double pad_begin_s;
    double trim_begin_s;
    double body_time_s;
    double trim_end_s;
  //double fade_delay_s;
  //double fade_time_s;
    double pad_end_s;

    /* internal flags */
    int pad_begin_set;
    int trim_begin_set;
    int body_time_set;
    int loop_count_set;
    int trim_end_set;
    int fade_delay_set;
    int fade_time_set;
    int pad_end_set;

    /* for lack of a better place... */
    int is_txtp;
    int is_mini_txtp;

} play_config_t;


typedef struct {
    int input_channels;
    int output_channels;

    int32_t pad_begin_duration;
    int32_t pad_begin_left;
    int32_t trim_begin_duration;
    int32_t trim_begin_left;
    int32_t body_duration;
    int32_t fade_duration;
    int32_t fade_left;
    int32_t fade_start;
    int32_t pad_end_duration;
  //int32_t pad_end_left;
    int32_t pad_end_start;

    int32_t play_duration;      /* total samples that the stream lasts (after applying all config) */
    int32_t play_position;      /* absolute sample where stream is */

} play_state_t;


/* info for a single vgmstream channel */
typedef struct {
    STREAMFILE* streamfile;     /* file used by this channel */
    off_t channel_start_offset; /* where data for this channel begins */
    off_t offset;               /* current location in the file */

    off_t frame_header_offset;  /* offset of the current frame header (for WS) */
    int samples_left_in_frame;  /* for WS */

    /* format specific */

    /* adpcm */
    int16_t adpcm_coef[16];             /* formats with decode coefficients built in (DSP, some ADX) */
    int32_t adpcm_coef_3by32[0x60];     /* Level-5 0x555 */
    int16_t vadpcm_coefs[8*2*8];        /* VADPCM: max 8 groups * max 2 order * fixed 8 subframe coefs */
    union {
        int16_t adpcm_history1_16;      /* previous sample */
        int32_t adpcm_history1_32;
    };
    union {
        int16_t adpcm_history2_16;      /* previous previous sample */
        int32_t adpcm_history2_32;
    };
    union {
        int16_t adpcm_history3_16;
        int32_t adpcm_history3_32;
    };
    union {
        int16_t adpcm_history4_16;
        int32_t adpcm_history4_32;
    };

    //double adpcm_history1_double;
    //double adpcm_history2_double;

    int adpcm_step_index;               /* for IMA */
    int adpcm_scale;                    /* for MS ADPCM */

    /* state for G.721 decoder, sort of big but we might as well keep it around */
    struct g72x_state g72x_state;

    /* ADX encryption */
    int adx_channels;
    uint16_t adx_xor;
    uint16_t adx_mult;
    uint16_t adx_add;

} VGMSTREAMCHANNEL;


/* main vgmstream info */
typedef struct {
    /* basic config */
    int32_t num_samples;            /* the actual max number of samples */
    int32_t sample_rate;            /* sample rate in Hz */
    int channels;                   /* number of channels */
    coding_t coding_type;           /* type of encoding */
    layout_t layout_type;           /* type of layout */
    meta_t meta_type;               /* type of metadata */

    /* loopin config */
    int loop_flag;                  /* is this stream looped? */
    int32_t loop_start_sample;      /* first sample of the loop (included in the loop) */
    int32_t loop_end_sample;        /* last sample of the loop (not included in the loop) */

    /* layouts/block config */
    size_t interleave_block_size;   /* interleave, or block/frame size (depending on the codec) */
    size_t interleave_first_block_size; /* different interleave for first block */
    size_t interleave_first_skip;   /* data skipped before interleave first (needed to skip other channels) */
    size_t interleave_last_block_size; /* smaller interleave for last block */
    size_t frame_size;              /* for codecs with configurable size */

    /* subsong config */
    int num_streams;                /* for multi-stream formats (0=not set/one stream, 1=one stream) */
    int stream_index;               /* selected subsong (also 1-based) */
    size_t stream_size;             /* info to properly calculate bitrate in case of subsongs */
    char stream_name[STREAM_NAME_SIZE]; /* name of the current stream (info), if the file stores it and it's filled */

    /* mapping config (info for plugins) */
    uint32_t channel_layout;        /* order: FL FR FC LFE BL BR FLC FRC BC SL SR etc (WAVEFORMATEX flags where FL=lowest bit set) */

    /* other config */
    int allow_dual_stereo;          /* search for dual stereo (file_L.ext + file_R.ext = single stereo file) */


    /* layout/block state */
    size_t full_block_size;         /* actual data size of an entire block (ie. may be fixed, include padding/headers, etc) */
    int32_t current_sample;         /* sample point within the file (for loop detection) */
    int32_t samples_into_block;     /* number of samples into the current block/interleave/segment/etc */
    off_t current_block_offset;     /* start of this block (offset of block header) */
    size_t current_block_size;      /* size in usable bytes of the block we're in now (used to calculate num_samples per block) */
    int32_t current_block_samples;  /* size in samples of the block we're in now (used over current_block_size if possible) */
    off_t next_block_offset;        /* offset of header of the next block */

    /* loop state (saved when loop is hit to restore later) */
    int32_t loop_current_sample;    /* saved from current_sample (same as loop_start_sample, but more state-like) */
    int32_t loop_samples_into_block;/* saved from samples_into_block */
    off_t loop_block_offset;        /* saved from current_block_offset */
    size_t loop_block_size;         /* saved from current_block_size */
    int32_t loop_block_samples;     /* saved from current_block_samples */
    off_t loop_next_block_offset;   /* saved from next_block_offset */
    int hit_loop;                   /* save config when loop is hit, but first time only */


    /* decoder config/state */
    int codec_endian;               /* little/big endian marker; name is left vague but usually means big endian */
    int codec_config;               /* flags for codecs or layouts with minor variations; meaning is up to them */
    int32_t ws_output_size;         /* WS ADPCM: output bytes for this block */


    /* main state */
    VGMSTREAMCHANNEL* ch;           /* array of channels */
    VGMSTREAMCHANNEL* start_ch;     /* shallow copy of channels as they were at the beginning of the stream (for resets) */
    VGMSTREAMCHANNEL* loop_ch;      /* shallow copy of channels as they were at the loop point (for loops) */
    void* start_vgmstream;          /* shallow copy of the VGMSTREAM as it was at the beginning of the stream (for resets) */

    void* mixing_data;              /* state for mixing effects */

    /* Optional data the codec needs for the whole stream. This is for codecs too
     * different from vgmstream's structure to be reasonably shoehorned.
     * Note also that support must be added for resetting, looping and
     * closing for every codec that uses this, as it will not be handled. */
    void* codec_data;
    /* Same, for special layouts. layout_data + codec_data may exist at the same time. */
    void* layout_data;


    /* play config/state */
    int config_enabled;             /* config can be used */
    play_config_t config;           /* player config (applied over decoding) */
    play_state_t pstate;            /* player state (applied over decoding) */
    int loop_count;                 /* counter of complete loops (1=looped once) */
    int loop_target;                /* max loops before continuing with the stream end (loops forever if not set) */
    sample_t* tmpbuf;               /* garbage buffer used for seeking/trimming */
    size_t tmpbuf_size;             /* for all channels (samples = tmpbuf_size / channels) */

} VGMSTREAM;


/* for files made of "continuous" segments, one per section of a song (using a complete sub-VGMSTREAM) */
typedef struct {
    int segment_count;
    VGMSTREAM** segments;
    int current_segment;
    sample_t* buffer;
    int input_channels;     /* internal buffer channels */
    int output_channels;    /* resulting channels (after mixing, if applied) */
    int mixed_channels;     /* segments have different number of channels */
} segmented_layout_data;

/* for files made of "parallel" layers, one per group of channels (using a complete sub-VGMSTREAM) */
typedef struct {
    int layer_count;
    VGMSTREAM** layers;
    sample_t* buffer;
    int input_channels;     /* internal buffer channels */
    int output_channels;    /* resulting channels (after mixing, if applied) */
    int external_looping;   /* don't loop using per-layer loops, but layout's own looping */
} layered_layout_data;



/* libacm interface */
typedef struct {
    STREAMFILE* streamfile;
    void* handle;
    void* io_config;
} acm_codec_data;


#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
typedef struct {
    STREAMFILE* streamfile;
    uint64_t start;
    uint64_t offset;
    uint64_t size;
} mp4_streamfile;

typedef struct {
    mp4_streamfile if_file;
    MP4FileHandle h_mp4file;
    MP4TrackId track_id;
    unsigned long sampleId, numSamples;
    UINT codec_init_data_size;
    HANDLE_AACDECODER h_aacdecoder;
    unsigned int sample_ptr, samples_per_frame, samples_discard;
    INT_PCM sample_buffer[( (6) * (2048)*4 )];
} mp4_aac_codec_data;
#endif

// VGMStream description in structure format
typedef struct {
    int sample_rate;
    int channels;
    struct mixing_info {
        int input_channels;
        int output_channels;
    } mixing_info;
    int channel_layout;
    struct loop_info {
        int start;
        int end;
    } loop_info;
    size_t num_samples;
    char encoding[128];
    char layout[128];
    struct interleave_info {
        int value;
        int first_block;
        int last_block;
    } interleave_info;
    int frame_size;
    char metadata[128];
    int bitrate;
    struct stream_info {
        int current;
        int total;
        char name[128];
    } stream_info;
} vgmstream_info;

/* -------------------------------------------------------------------------*/
/* vgmstream "public" API                                                   */
/* -------------------------------------------------------------------------*/

/* do format detection, return pointer to a usable VGMSTREAM, or NULL on failure */
VGMSTREAM* init_vgmstream(const char* const filename);

/* init with custom IO via streamfile */
VGMSTREAM* init_vgmstream_from_STREAMFILE(STREAMFILE* sf);

/* reset a VGMSTREAM to start of stream */
void reset_vgmstream(VGMSTREAM* vgmstream);

/* close an open vgmstream */
void close_vgmstream(VGMSTREAM* vgmstream);

/* calculate the number of samples to be played based on looping parameters */
int32_t get_vgmstream_play_samples(double looptimes, double fadeseconds, double fadedelayseconds, VGMSTREAM* vgmstream);

/* Decode data into sample buffer. Returns < sample_count on stream end */
int render_vgmstream(sample_t* buffer, int32_t sample_count, VGMSTREAM* vgmstream);

/* Seek to sample position (next render starts from that point). Use only after config is set (vgmstream_apply_config) */
void seek_vgmstream(VGMSTREAM* vgmstream, int32_t seek_sample);

/* Write a description of the stream into array pointed by desc, which must be length bytes long.
 * Will always be null-terminated if length > 0 */
void describe_vgmstream(VGMSTREAM* vgmstream, char* desc, int length);
void describe_vgmstream_info(VGMSTREAM* vgmstream, vgmstream_info* desc);

/* Return the average bitrate in bps of all unique files contained within this stream. */
int get_vgmstream_average_bitrate(VGMSTREAM* vgmstream);

/* List supported formats and return elements in the list, for plugins that need to know.
 * The list disables some common formats that may conflict (.wav, .ogg, etc). */
const char** vgmstream_get_formats(size_t* size);

/* same, but for common-but-disabled formats in the above list. */
const char** vgmstream_get_common_formats(size_t* size);

/* Force enable/disable internal looping. Should be done before playing anything (or after reset),
 * and not all codecs support arbitrary loop values ATM. */
void vgmstream_force_loop(VGMSTREAM* vgmstream, int loop_flag, int loop_start_sample, int loop_end_sample);

/* Set number of max loops to do, then play up to stream end (for songs with proper endings) */
void vgmstream_set_loop_target(VGMSTREAM* vgmstream, int loop_target);

/* Return 1 if vgmstream detects from the filename that said file can be used even if doesn't physically exist */
int vgmstream_is_virtual_filename(const char* filename);

/* -------------------------------------------------------------------------*/
/* vgmstream "private" API                                                  */
/* -------------------------------------------------------------------------*/

/* Allocate initial memory for the VGMSTREAM */
VGMSTREAM* allocate_vgmstream(int channel_count, int looped);

/* Prepare the VGMSTREAM's initial state once parsed and ready, but before playing. */
void setup_vgmstream(VGMSTREAM* vgmstream);

/* Open the stream for reading at offset (taking into account layouts, channels and so on).
 * Returns 0 on failure */
int vgmstream_open_stream(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t start_offset);
int vgmstream_open_stream_bf(VGMSTREAM* vgmstream, STREAMFILE* sf, off_t start_offset, int force_multibuffer);

/* Get description info */
void get_vgmstream_coding_description(VGMSTREAM* vgmstream, char* out, size_t out_size);
void get_vgmstream_layout_description(VGMSTREAM* vgmstream, char* out, size_t out_size);
void get_vgmstream_meta_description(VGMSTREAM* vgmstream, char* out, size_t out_size);

void setup_state_vgmstream(VGMSTREAM* vgmstream);
#endif
