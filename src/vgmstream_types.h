#ifndef _VGMSTREAM_TYPES_H
#define _VGMSTREAM_TYPES_H


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
    coding_PCM24LE,         /* little endian 24-bit PCM */
    coding_PCM24BE,         /* big endian 24-bit PCM */
    coding_PCM32LE,         /* little endian 32-bit PCM */

    /* ADPCM */
    coding_CRI_ADX,         /* CRI ADX */
    coding_CRI_ADX_fixed,   /* CRI ADX, encoding type 2 with fixed coefficients */
    coding_CRI_ADX_exp,     /* CRI ADX, encoding type 4 with exponential scale */
    coding_CRI_ADX_enc_8,   /* CRI ADX, type 8 encryption (God Hand) */
    coding_CRI_ADX_enc_9,   /* CRI ADX, type 9 encryption (PSO2) */

    coding_NGC_DSP,         /* Nintendo DSP ADPCM */
    coding_NGC_DSP_subint,  /* Nintendo DSP ADPCM with frame subinterframe */
    coding_NGC_DTK,         /* Nintendo DTK ADPCM (hardware disc), also called TRK or ADP */
    coding_AFC,             /* Nintendo AFC ADPCM */
    coding_AFC_2bit,        /* Nintendo AFC ADPCM (2-bit) */
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
    coding_IMA_mono,        /* IMA ADPCM (mono, low nibble first) */
    coding_DVI_IMA,         /* DVI IMA ADPCM (stereo or mono, high nibble first) */
    coding_DVI_IMA_mono,    /* DVI IMA ADPCM (mono, high nibble first) */
    coding_CAMELOT_IMA,
    coding_SNDS_IMA,        /* Heavy Iron Studios .snds IMA ADPCM */
    coding_QD_IMA,
    coding_WV6_IMA,         /* Gorilla Systems WV6 4-bit IMA ADPCM */
    coding_HV_IMA,          /* High Voltage 4-bit IMA ADPCM */
    coding_SQEX_IMA,        /* Square Enix 4-bit IMA ADPCM */
    coding_BLITZ_IMA,       /* Blitz Games 4-bit IMA ADPCM */

    coding_MS_IMA,          /* Microsoft IMA ADPCM */
    coding_MS_IMA_mono,     /* Microsoft IMA ADPCM (mono) */
    coding_XBOX_IMA,        /* XBOX IMA ADPCM */
    coding_XBOX_IMA_mch,    /* XBOX IMA ADPCM (multichannel) */
    coding_XBOX_IMA_mono,   /* XBOX IMA ADPCM (mono) */
    coding_NDS_IMA,         /* IMA ADPCM w/ NDS layout */
    coding_DAT4_IMA,        /* Eurocom 'DAT4' IMA ADPCM */
    coding_RAD_IMA,         /* Radical IMA ADPCM */
    coding_RAD_IMA_mono,    /* Radical IMA ADPCM (mono) */
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
    coding_CRANKCASE_IMA,   /* CrankcaseAudio REV IMA ADPCM */

    coding_MSADPCM,         /* Microsoft ADPCM (stereo/mono) */
    coding_MSADPCM_mono,    /* Microsoft ADPCM (mono) */
    coding_MSADPCM_ck,      /* Microsoft ADPCM (Cricket Audio variation) */
    coding_WS,              /* Westwood Studios VBR ADPCM */

    coding_AICA,            /* Yamaha AICA ADPCM (stereo) */
    coding_AICA_int,        /* Yamaha AICA ADPCM (mono/interleave) */
    coding_CP_YM,           /* Capcom's Yamaha ADPCM (stereo/mono) */
    coding_ASKA,            /* Aska ADPCM */
    coding_NXAP,            /* NXAP ADPCM */

    coding_TGC,             /* Tiger Game.com 4-bit ADPCM */

    coding_NDS_PROCYON,     /* Procyon Studio ADPCM */
    coding_LEVEL5,          /* Level-5 ADPCM */
    coding_LSF,             /* lsf ADPCM (Fastlane Street Racing iPhone)*/
    coding_MTAF,            /* Konami MTAF ADPCM */
    coding_MTA2,            /* Konami MTA2 ADPCM */
    coding_MPC3,            /* Paradigm MPC3 3-bit ADPCM */
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
    coding_DPCM_KCEJ,       /* Konami Computer Entertainment Japan 8-bit DPCM */
    coding_NWA,             /* VisualArt's NWA DPCM */
    coding_ACM,             /* InterPlay ACM */
    coding_CIRCUS_ADPCM,    /* Circus 8-bit ADPCM */
    coding_UBI_ADPCM,       /* Ubisoft 4/6-bit ADPCM */
    coding_ONGAKUKAN_ADPCM, /* Ongakukan 4-bit ADPCM */

    coding_EA_MT,           /* Electronic Arts MicroTalk (linear-predictive speech codec) */

    coding_CIRCUS_VQ,       /* Circus VQ */
    coding_RELIC,           /* Relic Codec (transform-based) */
    coding_CRI_HCA,         /* CRI High Compression Audio (transform-based) */
    coding_TAC,             /* tri-Ace Codec (transform-based) */
    coding_ICE_RANGE,       /* Inti Creates "range" codec */
    coding_ICE_DCT,         /* Inti Creates "DCT" codec */
    coding_KA1A,            /* Koei Tecmo codec (transform-based) */
    coding_UBI_MPEG,        /* Ubisoft MPEG codec (transform-based) */
    coding_MIO,             /* Entis MIO codec (transform-based) */
    coding_BINKA,           /* RAD Game Tools Bink Audio codec (transform-based) */

    coding_CF_DF_ADPCM_V40, /* Cyberflix DreamFactory v4.0 ADPCM */
    coding_CF_DF_DPCM_V41,  /* Cyberflix DreamFactory v4.1 DPCM */

#ifdef VGM_USE_VORBIS
    coding_OGG_VORBIS,      /* Xiph Vorbis with Ogg layer (transform-based) */
    coding_VORBIS_custom,   /* Xiph Vorbis with custom layer (transform-based) */
#endif

#ifdef VGM_USE_MPEG
    coding_MPEG_custom,     /* MPEG audio with custom features (transform-based) */
    coding_MPEG_ealayer3,   /* EALayer3, custom MPEG frames */
    coding_MPEG_layer1,     /* MP1 MPEG audio (transform-based) */
    coding_MPEG_layer2,     /* MP2 MPEG audio (transform-based) */
    coding_MPEG_layer3,     /* MP3 MPEG audio (transform-based) */
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
    layout_blocked_dec,
    layout_blocked_vas_kceo,
    layout_blocked_vs_mh,
    layout_blocked_mul,
    layout_blocked_gsnd,
    layout_blocked_thp,
    layout_blocked_filp,
    layout_blocked_ea_swvr,
    layout_blocked_adm,
    layout_blocked_mxch,
    layout_blocked_rage_aud,    /* Rockstar AUD blocks */
    layout_blocked_ps2_iab,
    layout_blocked_vs_str,
    layout_blocked_rws,
    layout_blocked_hwas,
    layout_blocked_ea_sns,      /* newest Electronic Arts blocks, found in SNS/SNU/SPS/etc formats */
    layout_blocked_awc,         /* Rockstar AWC */
    layout_blocked_vgs,         /* Guitar Hero II (PS2) */
    layout_blocked_xwav,
    layout_blocked_xvag_subsong,/* XVAG subsongs [God of War III (PS4)] */
    layout_blocked_ea_wve_au00, /* EA WVE au00 blocks */
    layout_blocked_ea_wve_ad10, /* EA WVE Ad10 blocks */
    layout_blocked_sthd,        /* Dream Factory STHD */
    layout_blocked_h4m,         /* H4M video */
    layout_blocked_xa_aiff,     /* XA in AIFF files [Crusader: No Remorse (SAT), Road Rash (3DO)] */
    layout_blocked_vs_square,
    layout_blocked_vid1,
    layout_blocked_ubi_sce,
    layout_blocked_tt_ad,
    layout_blocked_vas,

    /* otherwise odd */
    layout_segmented,       /* song divided in segments (song sections) */
    layout_layered,         /* song divided in layers (song channels) */

} layout_t;

/* The meta type specifies how we know what we know about the file. */
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
    meta_MUS_KROME,
    meta_DSP_WII_WSD,       /* Phantom Brave (WII) */
    meta_WII_NDP,           /* Vertigo (Wii) */
    meta_DSP_KCEJE,

    meta_STRM,              /* Nintendo STRM */
    meta_RSTM,              /* Nintendo RSTM (Revolution Stream, similar to STRM) */
    meta_AFC,               /* AFC */
    meta_AST,               /* AST */
    meta_RWSD,              /* single-stream RWSD */
    meta_RWAR,              /* single-stream RWAR */
    meta_RWAV,              /* contents of RWAR */
    meta_CWAV,              /* contents of CWAR */
    meta_FWAV,              /* contents of FWAR */
    meta_THP,               /* THP movie files */
    meta_SWAV,
    meta_NDS_RRDS,          /* Ridge Racer DS */
    meta_BNS,
    meta_BTSND,

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

    meta_XA,
    meta_ADS,
    meta_NPS,
    meta_RXWS,
    meta_RAW_INT,
    meta_EXST,
    meta_SVAG_KCET,
    meta_PS_HEADERLESS,     /* headerless PS-ADPCM */
    meta_MIB_MIH,
    meta_MIC_KOEI,
    meta_VAG,
    meta_VAG_custom,
    meta_VAG_footer,
    meta_AAAP,
    meta_SEB,
    meta_STR_WAV,           /* Blitz Games STR+WAV files */
    meta_ILD,
    meta_PWB,
    meta_VPK,               /* VPK Audio File */
    meta_PS2_BMDX,          /* Beatmania thing */
    meta_IIVB,
    meta_SSND,
    meta_SVS,
    meta_XSS,               /* Dino Crisis 3 */
    meta_SL3,
    meta_HGC1,
    meta_AUS,
    meta_RWS,
    meta_FSB1,
    meta_FSB2,
    meta_FSB3,
    meta_FSB4,
    meta_FSB5,
    meta_RWAX,
    meta_XWB,
    meta_MUSC,
    meta_MUSX,
    meta_FILP,
    meta_IKM,
    meta_STER,
    meta_BG00,
    meta_RSTM_ROCKSTAR,
    meta_VIG_KCES,
    meta_HXD,
    meta_VSV,
    meta_SCD_PCM,           /* Lunar - Eternal Blue */
    meta_PCM_KCEJE,
    meta_PS2_RKV,           /* Legacy of Kain - Blood Omen 2 (PS2) */
    meta_VAS_KCEO,
    meta_LP_AP_LEP,
    meta_SDT,               /* Baldur's Gate - Dark Alliance */
    meta_STR_SEGA,
    meta_STR_SEGA_custom,
    meta_SAP,
    meta_IDVI,
    meta_KRAW,
    meta_OMU,
    meta_XA2_ACCLAIM,
    meta_NUB,
    meta_IDSP_NL,           /* Mario Strikers Charged (Wii) */
    meta_IDSP_IE,           /* Defencer (GC) */
    meta_SPT_SPD,           /* Various (SPT+SPT DSP) */
    meta_ISH_ISD,           /* Various (ISH+ISD DSP) */
    meta_GSND,
    meta_YDSP,
    meta_STR_SQEX,
    meta_UBI_JADE,          /* Beyond Good & Evil, Rayman Raving Rabbids */
    meta_GCA,               /* Metal Slug Anthology */
    meta_SSM,
    meta_PS2_JOE,           /* Wall-E / Pixar games */
    meta_YMF,
    meta_SADL,
    meta_FAG,               /* Jackie Chan - Stuntmaster */
    meta_MIC,
    meta_PDT,
    meta_ASD_NAXAT,
    meta_SPSD,
    meta_RSD,
    meta_SEG,
    meta_RIFF_IMA,
    meta_KNON,
    meta_VGS,               /* Guitar Hero Encore - Rocks the 80s */
    meta_DCS_WAV,
    meta_SMP,
    meta_WII_SNG,           /* Excite Trucks */
    meta_MUL,
    meta_BAKA,
    meta_VSF,
    meta_SMSS,
    meta_ADS_MIDWAY,
    meta_UBI_CKD,           /* Ubisoft CKD RIFF header (Rayman Origins Wii) */
    meta_RAW_WAVM,
    meta_WVS,
    meta_XMU,
    meta_EA_SCHL,           /* Electronic Arts SCHl with variable header */
    meta_EA_SCHL_fixed,     /* Electronic Arts SCHl with fixed header */
    meta_EA_BNK,            /* Electronic Arts BNK with variable header */
    meta_EA_BNK_fixed,      /* Electronic Arts BNK with fixed header */
    meta_EA_1SNH,           /* Electronic Arts 1SNh/EACS */
    meta_EA_EACS,
    meta_RAW_PCM,
    meta_GENH,              /* generic header */
    meta_AIFC,              /* Audio Interchange File Format AIFF-C */
    meta_AIFF,              /* Audio Interchange File Format */
    meta_STR_SNDS,          /* .str with SNDS blocks and SHDR header */
    meta_WS_AUD,
    meta_RIFF_WAVE,         /* RIFF, for WAVs */
    meta_RIFF_WAVE_POS,     /* .wav + .pos for looping (Ys Complete PC) */
    meta_RIFF_WAVE_labl,
    meta_RIFF_WAVE_smpl,
    meta_RIFF_WAVE_wsmp,
    meta_RIFF_WAVE_ctrl,
    meta_RIFF_WAVE_cue,
    meta_RIFX_WAVE,         /* RIFX, for big-endian WAVs */
    meta_RIFX_WAVE_smpl,    /* RIFX w/ loop data in smpl chunk */
    meta_XNB,               /* XNA Game Studio 4.0 */
    meta_PC_MXST,           /* Lego Island MxSt */
    meta_SAB,               /* Worms 4 Mayhem SAB+SOB file */
    meta_NWA,               /* Visual Art's NWA */
    meta_NWA_NWAINFOINI,    /* Visual Art's NWA w/ NWAINFO.INI for looping */
    meta_NWA_GAMEEXEINI,    /* Visual Art's NWA w/ Gameexe.ini for looping */
    meta_DVI,
    meta_KCEY,
    meta_ACM,               /* InterPlay ACM header */
    meta_MUS_ACM,           /* MUS playlist of InterPlay ACM files */
    meta_DEC,               /* Falcom PC games (Xanadu Next, Gurumin) */
    meta_VS_MH,
    meta_BGW,
    meta_SPW,
    meta_STS,
    meta_P2BT_MOVE_VISA,
    meta_GBTS,
    meta_NGC_DSP_IADP,      /* Gamecube Interleave DSP */
    meta_ZSD,
    meta_REDSPARK,
    meta_RAGE_AUD,          /* Rockstar AUD - MC:LA, GTA IV */
    meta_NDS_HWAS,          /* Spider-Man 3, Tony Hawk's Downhill Jam, possibly more... */
    meta_LPS,
    meta_NAOMI_ADPCM,       /* NAOMI/NAOMI2 Arcade games */
    meta_SD9,
    meta_2DX9,
    meta_VGV,
    meta_GCUB,
    meta_MAXIS_XA,
    meta_CAFF,
    meta_EXAKT_SC,          /* Activision EXAKT .SC (PS2) */
    meta_WII_WAS,           /* DiRT 2 (WII) */
    meta_PONA_3DO,          /* Policenauts (3DO) */
    meta_PONA_PSX,          /* Policenauts (PSX) */
    meta_XBOX_HLWAV,        /* Half Life 2 (XBOX) */
    meta_AST_MV,
    meta_AST_MMV,
    meta_DMSG,              /* Nightcaster II - Equinox (XBOX) */
    meta_NGC_DSP_AAAP,      /* Turok: Evolution (NGC), Vexx (NGC) */
    meta_WB,
    meta_S14,               /* raw Siren 14, 24kbit mono */
    meta_SSS,               /* raw Siren 14, 48kbit stereo */
    meta_MCG,
    meta_SMPL,
    meta_MSA,
    meta_VOI,
    meta_P3D,               /* Prototype P3D */
    meta_NGC_RKV,           /* Legacy of Kain - Blood Omen 2 (GC) */
    meta_DSP_DDSP,          /* Various (2 dsp files stuck together */
    meta_MPDS,
    meta_DSP_STR_IG,        /* Micro Machines, Superman Superman: Shadow of Apokolis */
    meta_EA_SWVR,           /* Future Cop L.A.P.D., Freekstyle */
    meta_DSP_XIII,          /* XIII, possibly more (Ubisoft header???) */
    meta_DSP_CABELAS,       /* Cabelas games */
    meta_PS2_ADM,           /* Dragon Quest V (PS2) */
    meta_LPCM_SHADE,
    meta_VMS,
    meta_XAU,               /* XPEC Entertainment (Beat Down (PS2 Xbox), Spectral Force Chronicle (PS2)) */
    meta_GH3_BAR,           /* Guitar Hero III Mobile .bar */
    meta_DSP_DSPW,          /* Sengoku Basara 3 [WII] */
    meta_PS2_JSTM,          /* Tantei Jinguji Saburo - Kind of Blue (PS2) */
    meta_SQEX_SCD,
    meta_NST_MONSTER,
    meta_BAF,
    meta_XVAG,
    meta_CPS,
    meta_MSF,
    meta_SNDP,
    meta_SGXD,
    meta_RAS,
    meta_SPM,
    meta_VGS_PS,
    meta_IAB,
    meta_VS_STR,            /* The Bouncer */
    meta_LSF_N1NJ4N,        /* .lsf n1nj4n Fastlane Street Racing (iPhone) */
    meta_XWAV,
    meta_RAW_SNDS,
    meta_KVAG,
    meta_PSND,
    meta_ADP_WILDFIRE,
    meta_QD_ADP,
    meta_SFX0_MONSTER,
    meta_SONG_MONSTER,
    meta_MTAF,
    meta_ALP,
    meta_WPD,
    meta_MCSS,
    meta_HSF,
    meta_IVAG,
    meta_2PFS,
    meta_PS2_VBK,           /* Disney's Stitch - Experiment 626 */
    meta_XWB_KONAMI,
    meta_CSTM,              /* Nintendo 3DS CSTM (Century Stream) */
    meta_FSTM,              /* Nintendo Wii U FSTM (caFe? Stream) */
    meta_IDSP_NAMCO,
    meta_KT_WIIBGM,         /* Koei Tecmo WiiBGM */
    meta_KTSS,              /* Koei Tecmo Nintendo Stream (KNS) */
    meta_MADP,
    meta_ADX_MONSTER,
    meta_HCA,
    meta_SVAG_SNK,
    meta_VDS_VDM,
    meta_FFMPEG,
    meta_FFMPEG_faulty,
    meta_CXS,
    meta_AKB,
    meta_PASX,
    meta_XMA_RIFF,
    meta_ASTB,
    meta_WWISE_RIFF,        /* Audiokinetic Wwise RIFF/RIFX */
    meta_UBI_RAKI,          /* Ubisoft RAKI header (Rayman Legends, Just Dance 2017) */
    meta_SNDX,
    meta_OGL,
    meta_MPC3,
    meta_GHS,
    meta_AAC_TRIACE,
    meta_MTA2,
    meta_XA_XA30,
    meta_XA_04SW,
    meta_TXTH,
    meta_SK_AUD,            /* Silicon Knights .AUD (Eternal Darkness GC) */
    meta_AHX,
    meta_STMA,
    meta_BINK,              /* RAD Game Tools BINK audio/video */
    meta_EA_SNU,            /* Electronic Arts SNU (Dead Space) */
    meta_AWC,               /* Rockstar AWC (GTA5, RDR) */
    meta_OPUS,              /* Nintendo Opus [Lego City Undercover (Switch)] */
    meta_ASTL,
    meta_NAAC,
    meta_UBI_SB,
    meta_UBI_APM,
    meta_EZW,
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
    meta_SMV,
    meta_NXAP,              /* Nex Entertainment games [Time Crisis 4 (PS3), Time Crisis Razing Storm (PS3)] */
    meta_EA_WVE_AU00,       /* Electronic Arts PS movies [Future Cop - L.A.P.D. (PS), Supercross 2000 (PS)] */
    meta_EA_WVE_AD10,       /* Electronic Arts PS movies [Wing Commander 3/4 (PS)] */
    meta_STHD,              /* STHD .stx [Kakuto Chojin (Xbox)] */
    meta_MP4,               /* MP4/AAC */
    meta_SRE_PCM,
    meta_DSP_MCADPCM,       /* Skyrim (Switch) */
    meta_UBI_LYN,           /* Ubisoft LyN engine [The Adventures of Tintin (multi)] */
    meta_MSH_MSB,
    meta_TXTP,
    meta_SMH_SMC,
    meta_PPST,
    meta_SPS_N1,
    meta_UBI_BAO,
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
    meta_SSCF,
    meta_DSP_ITL,           /* Charinko Hero (GC) */
    meta_A2M,               /* Scooby-Doo! Unmasked (PS2) */
    meta_AHV,               /* Headhunter (PS2) */
    meta_MSV,
    meta_SDF,
    meta_SVGP,
    meta_VAI,               /* Ratatouille (GC) */
    meta_AIF_ASOBO,         /* Ratatouille (PC) */
    meta_AO,                /* Cloudphobia (PC) */
    meta_APC,               /* MegaRace 3 (PC) */
    meta_WAV2,
    meta_SFXB,
    meta_DERF,
    meta_SADF,
    meta_UTK,
    meta_NXA1,
    meta_ADPCM_CAPCOM,
    meta_UE4OPUS,
    meta_XWMA,
    meta_VA3,
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
    meta_XWV_VALVE,
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
    meta_MJH_MJB,
    meta_BSNF,
    meta_TAC,
    meta_IDSP_TOSE,
    meta_DSP_KWA,
    meta_OGV_3RDEYE,
    meta_PIFF_TPCM,
    meta_WXH_WXD,
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
    meta_ADM,
    meta_TT_AD,
    meta_SNDZ,
    meta_VAB,
    meta_BIGRP,
    meta_DIC1,
    meta_AWD,
    meta_SQUEAKSTREAM,
    meta_SQUEAKSAMPLE,
    meta_SNDS,
    meta_NXOF,
    meta_GWB_GWD,
    meta_CBX,
    meta_VAS_ROCKSTAR,
    meta_EA_SBK,
    meta_DSP_ASURA,
    meta_ONGAKUKAN_RIFF_ADP,
    meta_SDD,
    meta_KA1A,
    meta_HD_BD,
    meta_PPHD,
    meta_XABP,
    meta_I3DS,
    meta_AXHD,
    meta_SHAA,
    meta_OOR,
    meta_MIO,
    meta_AUDIOPKG,
    meta_SWAR,
    meta_IVB,
    meta_SRCD,
    meta_MHWK,
    meta_CF_DF,
    meta_JAUDIO,
    meta_BCF1,
    meta_UEBA,
    meta_WD,
} meta_t;

#endif
