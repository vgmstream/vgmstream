#include "vgmstream.h"
#include "coding/coding.h"


/* Defines the list of accepted extensions. vgmstream doesn't use it internally so it's here
 * to inform plugins that need it. Common extensions are commented out to avoid stealing them
 * and possibly adding an unwanted association to the player. */

/* Common extensions (like .wav or .ogg) should go in the common_extension_list. It should only
 * contain common formats that vgmstream can also parse, to avoid hijacking them (since their
 * plugins typically are faster and have desirable features vgmstream won't handle). Extensions of
 * formats not parsed don't need to go there (for example .stm is a Scream Tracker Module elsewhere,
 * but our .stm is very different so there is no conflict). */

/* Some extensions require external libraries and could be #ifdef, not worth. */

/* Formats marked as "not parsed" mean they'll go through FFmpeg, the header/extension isn't
 * parsed by vgmstream and typically won't not be fully accurate. */


static const char* extension_list[] = {
    //"", /* vgmstream can play extensionless files too, but plugins must accept them manually */

    "208",
    "2dx9",
    "2pfs",
    "3do",
    "3ds", //txth/reserved [F1 2011 (3DS)] 
    "4", //for Game.com audio
    "8", //txth/reserved [Gungage (PS1)]
    "800",
    "9tav",

    //"aac", //common
    "aa3", //FFmpeg/not parsed (ATRAC3/ATRAC3PLUS/MP3/LPCM/WMA)
    "aax",
    "abc", //txth/reserved [Find My Own Way (PS2) tech demo]
    "abk",
    //"ac3", //common, FFmpeg/not parsed (AC3)
    "acb",
    "acm",
    "acx",
    "ad", //txth/reserved [Xenosaga Freaks (PS2)]
    "adc", //txth/reserved [Tomb Raider The Last Revelation (DC), Tomb Raider Chronicles (DC)]
    "adm",
    "adp",
    "adpcm",
    "adpcmx",
    "ads",
    "adw",
    "adx",
    "afc",
    "afs2",
    "agsc",
    "ahx",
    "ahv",
    "ai",
    //"aif", //common
    "aif-Loop",
    "aifc", //common?
    //"aiff", //common
    "aix",
    "akb",
    "al",
    "al2",
    "ams", //txth/reserved [Super Dragon Ball Z (PS2) ELF names]
    "amts", //fake extension/header id for .stm (renamed? to be removed?)
    "an2",
    "ao",
    "ap",
    "apc",
    "as4",
    "asd",
    "asf",
    "asr",
    "ass",
    "ast",
    "at3",
    "at9",
    "atsl",
    "atsl3",
    "atsl4",
    "atslx",
    "atx",
    "aud",
    "audio", //txth/reserved [Grimm Echoes (Android)]
    "audio_data",
    "aus",
    "awa", //txth/reserved [Missing Parts Side A (PS2)]
    "awb",
    "awc",

    "b1s",
    "baf",
    "baka",
    "bank",
    "bar",
    "bcstm",
    "bcwav",
    "bd3",
    "bdsp",
    "bfstm",
    "bfwav",
    "bg00",
    "bgm",
    "bgw",
    "bh2pcm",
    "bigrp",
    "bik",
    "bika", //fake extension for .bik (to be removed)
    "bik2",
    //"bin", //common
    "bk2",
    "bkr",  //txth/reserved [P.N.03 (GC), Viewtiful Joe (GC)]
    "blk",
    "bmdx",
    "bms",
    "bnk",
    "bnm",
    "bns",
    "bnsf",
    "bo2",
    "brstm",
    "brstmspm",
    "brwav",
    "brwsd", //fake extension for RWSD (non-format)
    "bsnd",
    "btsnd",
    "bvg",
    "bwav",

    "cads",
    "caf",
    "cbd2",
    "ccc", //fake extension (to be removed)
    "cd",
    "cfn", //fake extension for CAF (renamed, to be removed?)
    "chd", //txth/reserved [Donkey Konga (GC), Star Fox Assault (GC)]
    "chk",
    "ckb",
    "ckd",
    "cks",
    "cnk",
    "cpk",
    "cps",
    "csa", //txth/reserved [LEGO Racers 2 (PS2)]
    "csmp",
    "cvs", //txth/reserved [Aladdin in Nasira's Revenge (PS1)]
    "cwav",
    "cxs",

    "d2", //txth/reserved [Dodonpachi Dai-Ou-Jou (PS2)]
    "da",
    //"dat", //common
    "data",
    "dax",
    "dbm",
    "dct",
    "dcs",
    "ddsp",
    "de2",
    "dec",
    "diva",
    "dmsg", //fake extension/header id for .sgt (to be removed)
    "ds2", //txth/reserved [Star Wars Bounty Hunter (GC)]
    "dsb",
    "dsf",
    "dsp",
    "dspw",
    "dtk",
    "dvi",
    "dyx", //txth/reserved [Shrek 4 (iOS)]

    "e4x",
    "eam",
    "eas",
    "eda", //txth/reserved [Project Eden (PS2)]
    "emff", //fake extension for .mul (to be removed)
    "enm",
    "eno",
    "ens",
    "esf",
    "exa",
    "ezw",

    "fag",
    "fda",
    "ffw",
    "filp",
    //"flac", //common
    "flx",
    "fsb",
    "fsv",
    "fwav",
    "fwse",

    "g1l",
    "gbts",
    "gca",
    "gcm",
    "gcub",
    "gcw",
    "genh",
    "gin",
    "gms",
    "grn",
    "gsb",
    "gsf",
    "gtd",
    "gwm",

    "h4m",
    "hab",
    "hca",
    "hdr",
    "hgc1",
    "his",
    "hps",
    "hsf",
    "hvqm",
    "hwx", //txth/reserved [Star Wars Episode III (Xbox)]
    "hx2",
    "hx3",
    "hxc",
    "hxd",
    "hxg",
    "hxx",
    "hwas",

    "iab",
    "iadp",
    "idmsf",
    "idsp",
    "idvi", //fake extension/header id for .pcm (renamed, to be removed)
    "idwav",
    "idx",
    "idxma",
    "ifs",
    "ikm",
    "ild",
    "ilf", //txth/reserved [Madden NFL 98 (PS1)]
    "ilv", //txth/reserved [Star Wars Episode III (PS2)]
    "ima",
    "imc",
    "imx",
    "int",
    "is14",
    "isb",
    "isd",
    "isws",
    "itl",
    "ivaud",
    "ivag",
    "ivb",
    "ivs", //txth/reserved [Burnout 2 (PS2)]

    "joe",
    "jstm",

    "kat",
    "kces",
    "kcey", //fake extension/header id for .pcm (renamed, to be removed)
    "km9",
    "kmx",
    "kovs", //fake extension/header id for .kvs
    "kno",
    "kns",
    "koe",
    "kraw",
    "ktac",
    "ktsl2asbin",
    "ktss", //fake extension/header id for .kns
    "kvs",
    "kwa",

    "l",
    "l00", //txth/reserved [Disney's Dinosaur (PS2)]
    "laac", //fake extension for .aac (tri-Ace)
    "ladpcm", //not fake
    "laif", //fake extension for .aif (various)
    "laiff", //fake extension for .aiff
    "laifc", //fake extension for .aifc
    "lac3", //fake extension for .ac3, FFmpeg/not parsed
    "lasf", //fake extension for .asf (various)
    "lbin", //fake extension for .bin (various)
    "leg",
    "lep",
    "lflac", //fake extension for .flac, FFmpeg/not parsed
    "lin",
    "lm0",
    "lm1",
    "lm2",
    "lm3",
    "lm4",
    "lm5",
    "lm6",
    "lm7",
    "lmp2", //fake extension for .mp2, FFmpeg/not parsed
    "lmp3", //fake extension for .mp3, FFmpeg/not parsed
    "lmp4", //fake extension for .mp4
    "lmpc", //fake extension for .mpc, FFmpeg/not parsed
    "logg", //fake extension for .ogg
    "lopus", //fake extension for .opus, used by LOPU too
    "lp",
    "lpcm",
    "lpk",
    "lps",
    "lrmb",
    "lse",
    "lsf",
    "lstm", //fake extension for .stm
    "lwav", //fake extension for .wav
    "lwma", //fake extension for .wma, FFmpeg/not parsed

    "mab",
    "mad",
    "map",
    "matx",
    "mc3",
    "mca",
    "mcadpcm",
    "mcg",
    "mds",
    "mdsp",
    "med",
    "mjb",
    "mi4", //fake extension for .mib (renamed, to be removed)
    "mib",
    "mic",
    "mihb",
    "mnstr",
    "mogg",
    //"m4a", //common
    //"m4v", //common
    //"mp+", //common [Moonshine Runners (PC)]
    //"mp2", //common
    //"mp3", //common
    //"mp4", //common
    //"mpc", //common
    "mpdsp",
    "mpds",
    "mpf",
    "mps", //txth/reserved [Scandal (PS2)]
    "ms",
    "msa",
    "msb",
    "msd",
    "mse",
    "msf",
    "mss",
    "msv",
    "msvp", //fake extension/header id for .msv
    "mta2",
    "mtaf",
    "mul",
    "mups",
    "mus",
    "musc",
    "musx",
    "mvb", //txth/reserved [Porsche Challenge (PS1)]
    "mwa", //txth/reserved [Fatal Frame (Xbox)]
    "mwv",
    "mxst",
    "myspd",

    "n64",
    "naac",
    "nds",
    "ndp", //fake extension/header id for .nds
    "nlsd",
    "nop",
    "nps",
    "npsf", //fake extension/header id for .nps (in bigfiles)
    "nsa",
    "nsopus",
    "nub",
    "nub2",
    "nus3audio",
    "nus3bank",
    "nwa",
    "nwav",
    "nxa",

    //"ogg", //common
    "ogg_",
    "ogl",
    "ogv",
    "oma", //FFmpeg/not parsed (ATRAC3/ATRAC3PLUS/MP3/LPCM/WMA)
    "omu",
    //"opus", //common
    "opusx",
    "otm",
    "oto", //txth/reserved [Vampire Savior (SAT)]
    "ovb",

    "p04", //txth/reserved [Psychic Force 2012 (DC), Skies of Arcadia (DC)]
    "p16", //txth/reserved [Astal (SAT)]
    "p1d", //txth/reserved [Farming Simulator 18 (3DS)]
    "p2a", //txth/reserved [Thunderhawk Operation Phoenix (PS2)]
    "p2bt",
    "p3d",
    "past",
    "pcm",
    "pdt",
    "pk",
    "pnb",
    "pona",
    "pos",
    "ps3",
    "ps2stm", //fake extension for .stm (renamed? to be removed?)
    "psb",
    "psf",
    "psh", //fake extension for .vsv (to be removed)
    "psnd",

    "r",
    "rac", //txth/reserved [Manhunt (Xbox)]
    "rad",
    "rak",
    "ras",
    "raw", //txth/reserved [Madden NHL 97 (PC)-pcm8u]
    "rda", //FFmpeg/reserved [Rhythm Destruction (PC)]
    "res", //txth/reserved [Spider-Man: Web of Shadows (PSP)]
    "rkv",
    "rnd",
    "rof",
    "rpgmvo",
    "rrds",
    "rsd",
    "rsf",
    "rsm",
    "rsnd", //txth/reserved [Birushana: Ichijuu no Kaze (Switch)]
    "rsp",
    "rstm", //fake extension/header id for .rstm (in bigfiles)
    "rvws",
    "rwar",
    "rwav",
    "rws",
    "rwsd",
    "rwx",
    "rxw",
    "rxx", //txth/reserved [Full Auto (X360)]

    "s14",
    "s3s", //txth/reserved [DT Racer (PS2)]
    "s3v", //Sound Voltex (AC)
    "sab",
    "sad",
    "saf",
    "sag",
    "sam", //txth/reserved [Lost Kingdoms 2 (GC)]
    "sap",
    "sb0",
    "sb1",
    "sb2",
    "sb3",
    "sb4",
    "sb5",
    "sb6",
    "sb7",
    "sbk",
    "sbin",
    "sbr",
    "sbv",
    "sig",
    "sm0",
    "sm1",
    "sm2",
    "sm3",
    "sm4",
    "sm5",
    "sm6",
    "sm7",
    "sc",
    "scd",
    "sch",
    "sd9",
    "sdp", //txth/reserved [Metal Gear Arcade (AC)]
    "sdf",
    "sdt",
    "seb",
    "sed",
    "seg",
    "sem", //txth/reserved [Oretachi Game Center Zoku: Sonic Wings (PS2)]
    "sf0",
    "sfl",
    "sfs",
    "sfx",
    "sgb",
    "sgd",
    "sgt",
    "sgx",
    "sl3",
    "slb", //txth/reserved [THE Nekomura no Hitobito (PS2)]
    "sli",
    "smc",
    "smk",
    "smp",
    "smpl", //fake extension/header id for .v0/v1 (renamed, to be removed)
    "smv",
    "snd",
    "snds",
    "sng",
    "sngw",
    "snr",
    "sns",
    "snu",
    "snz", //txth/reserved [Killzone HD (PS3)]
    "sod",
    "son",
    "spd",
    "spm",
    "sps",
    "spsd",
    "spw",
    "ss2",
    "ssd", //txth/reserved [Zack & Wiki (Wii)]
    "ssm",
    "sspr",
    "ssp",
    "sss",
    "ster",
    "sth",
    "stm",
    "stma", //fake extension/header id for .stm
    "str",
    "stream",
    "strm",
    "sts",
    "sts_cp3",
    "stx",
    "svag",
    "svs",
    "svg",
    "swag",
    "swav",
    "swd",
    "switch", //txth/reserved (.m4a-x.switch) [Ikinari Maou (Switch)]
    "switch_audio",
    "sx",
    "sxd",
    "sxd2",
    "sxd3",
    "szd",
    "szd1",
    "szd3",

    "tad",
    "tgq",
    "tgv",
    "thp",
    "tk5",
    "tmx",
    "tra",
    "tun",
    "txth",
    "txtp",
    "tydsp",

    "u0",
    "ue4opus",
    "ulw",
    "um3",
    "utk",
    "uv",

    "v0",
    //"v1", //dual channel with v0
    "va3",
    "vab",
    "vag",
    "vai",
    "vam", //txth/reserved [Rocket Power: Beach Bandits (PS2)]
    "vas",
    "vawx",
    "vb", //txth/reserved [Tantei Jinguji Saburo: Mikan no Rupo (PS1)]
    "vbk",
    "vbx", //txth/reserved [THE Taxi 2 (PS2)]
    "vca", //txth/reserved [Pac-Man World (PS1)]
    "vcb", //txth/reserved [Pac-Man World (PS1)]
    "vds",
    "vdm",
    "vgi", //txth/reserved [Time Crisis II (PS2)]
    "vgm", //txth/reserved [Maximo (PS2)]
    "vgs",
    "vgv",
    "vh",
    "vid",
    "vig",
    "vis",
    "vm4", //txth/reserved [Elder Gate (PS1)]
    "vms",
    "vmu", //txth/reserved [Red Faction (PS2)]
    "voi",
    "vp6",
    "vpk",
    "vs",
    "vsf",
    "vsv",
    "vxn",

    "w",
    "waa",
    "wac",
    "wad",
    "waf",
    "wam",
    "was",
    //"wav", //common
    "wavc",
    "wave",
    "wavebatch",
    "wavm",
    "wavx", //txth/reserved [LEGO Star Wars (Xbox)]
    "way",
    "wb",
    "wb2",
    "wbd",
    "wbk",
    "wd",
    "wem",
    "wii",
    "wic", //txth/reserved [Road Rash (SAT)-videos]
    "wip", //txth/reserved [Colin McRae DiRT (PC)]
    "wlv", //txth/reserved [ToeJam & Earl III: Mission to Earth (DC)]
    "wmus", //fake extension (to be removed)
    "wp2",
    "wpd",
    "wsd",
    "wsi",
    "wst", //txth/reserved [3jigen Shoujo o Hogo Shimashita (PC)]
    "wua",
    "wv2",
    "wv6",
    "wve",
    "wvs",
    "wvx",
    "wxd",

    "x",
    "x360audio", //fake extension for Unreal Engine 3 XMA (real extension unknown)
    "xa",
    "xa2",
    "xa30",
    "xag", //txth/reserved [Tamsoft's PS2 games]
    "xau",
    "xav",
    "xb", //txth/reserved [Scooby-Doo! Unmasked (Xbox)]
    "xen",
    "xma",
    "xma2",
    "xmu",
    "xmv",
    "xnb",
    "xsh",
    "xsf",
    "xse",
    "xsew",
    "xss",
    "xvag",
    "xvas",
    "xwav", //fake extension for .wav (renamed, to be removed)
    "xwb",
    "xmd",
    "xopus",
    "xps",
    "xwc",
    "xwm",
    "xwma",
    "xws",
    "xwv",

    "ydsp",
    "ymf",

    "zic",
    "zsd",
    "zsm",
    "zss",
    "zwdsp",
    "zwv",

    "vgmstream" /* fake extension, catch-all for FFmpeg/txth/etc */

    //, NULL //end mark
};

static const char* common_extension_list[] = {
    "aac", //common
    "ac3", //common, FFmpeg/not parsed (AC3)
    "aif", //common
    "aiff", //common
    "bin", //common
    "dat", //common
    "flac", //common
    "m4a", //common
    "m4v", //common
    "mp+", //common [Moonshine Runners (PC)]
    "mp2", //common
    "mp3", //common
    "mp4", //common
    "mpc", //common
    "ogg", //common
    "opus", //common
    "wav", //common
    "wma", //common
};


/* List supported formats and return elements in the list, for plugins that need to know. */
const char ** vgmstream_get_formats(size_t * size) {
    *size = sizeof(extension_list) / sizeof(char*);
    return extension_list;
}

const char ** vgmstream_get_common_formats(size_t * size) {
    *size = sizeof(common_extension_list) / sizeof(char*);
    return common_extension_list;
}


/* internal description info */

typedef struct {
    coding_t type;
    const char *description;
} coding_info;

typedef struct {
    layout_t type;
    const char *description;
} layout_info;

typedef struct {
    meta_t type;
    const char *description;
} meta_info;


static const coding_info coding_info_list[] = {
        {coding_SILENCE,            "Silence"},

        {coding_PCM16LE,            "Little Endian 16-bit PCM"},
        {coding_PCM16BE,            "Big Endian 16-bit PCM"},
        {coding_PCM16_int,          "16-bit PCM with 2 byte interleave (block)"},
        {coding_PCM8,               "8-bit signed PCM"},
        {coding_PCM8_int,           "8-bit signed PCM with 1 byte interleave (block)"},
        {coding_PCM8_U,             "8-bit unsigned PCM"},
        {coding_PCM8_U_int,         "8-bit unsigned PCM with 1 byte interleave (block)"},
        {coding_PCM8_SB,            "8-bit PCM with sign bit"},
        {coding_PCM4,               "4-bit signed PCM"},
        {coding_PCM4_U,             "4-bit unsigned PCM"},
        {coding_ULAW,               "8-bit u-Law"},
        {coding_ULAW_int,           "8-bit u-Law with 1 byte interleave (block)"},
        {coding_ALAW,               "8-bit a-Law"},
        {coding_PCMFLOAT,           "32-bit float PCM"},
        {coding_PCM24LE,            "24-bit Little Endian PCM"},

        {coding_CRI_ADX,            "CRI ADX 4-bit ADPCM"},
        {coding_CRI_ADX_fixed,      "CRI ADX 4-bit ADPCM (fixed coefficients)"},
        {coding_CRI_ADX_exp,        "CRI ADX 4-bit ADPCM (exponential scale)"},
        {coding_CRI_ADX_enc_8,      "CRI ADX 4-bit ADPCM (type 8 encryption)"},
        {coding_CRI_ADX_enc_9,      "CRI ADX 4-bit ADPCM (type 9 encryption)"},

        {coding_NGC_DSP,            "Nintendo DSP 4-bit ADPCM"},
        {coding_NGC_DSP_subint,     "Nintendo DSP 4-bit ADPCM (subinterleave)"},
        {coding_NGC_DTK,            "Nintendo DTK 4-bit ADPCM"},
        {coding_NGC_AFC,            "Nintendo AFC 4-bit ADPCM"},
        {coding_VADPCM,             "Silicon Graphics VADPCM 4-bit ADPCM"},

        {coding_G721,               "CCITT G.721 4-bit ADPCM"},

        {coding_XA,                 "CD-ROM XA 4-bit ADPCM"},
        {coding_XA8,                "CD-ROM XA 8-bit ADPCM"},
        {coding_XA_EA,              "Electronic Arts XA 4-bit ADPCM"},
        {coding_PSX,                "Playstation 4-bit ADPCM"},
        {coding_PSX_badflags,       "Playstation 4-bit ADPCM (bad flags)"},
        {coding_PSX_cfg,            "Playstation 4-bit ADPCM (configurable)"},
        {coding_PSX_pivotal,        "Playstation 4-bit ADPCM (Pivotal)"},
        {coding_HEVAG,              "Sony HEVAG 4-bit ADPCM"},

        {coding_EA_XA,              "Electronic Arts EA-XA 4-bit ADPCM v1"},
        {coding_EA_XA_int,          "Electronic Arts EA-XA 4-bit ADPCM v1 (mono/interleave)"},
        {coding_EA_XA_V2,           "Electronic Arts EA-XA 4-bit ADPCM v2"},
        {coding_MAXIS_XA,           "Maxis EA-XA 4-bit ADPCM"},
        {coding_EA_XAS_V0,          "Electronic Arts EA-XAS 4-bit ADPCM v0"},
        {coding_EA_XAS_V1,          "Electronic Arts EA-XAS 4-bit ADPCM v1"},

        {coding_IMA,                "IMA 4-bit ADPCM"},
        {coding_IMA_int,            "IMA 4-bit ADPCM (mono/interleave)"},
        {coding_DVI_IMA,            "Intel DVI 4-bit IMA ADPCM"},
        {coding_DVI_IMA_int,        "Intel DVI 4-bit IMA ADPCM (mono/interleave)"},
        {coding_NW_IMA,             "NintendoWare IMA 4-bit ADPCM"},
        {coding_SNDS_IMA,           "Heavy Iron .snds 4-bit IMA ADPCM"},
        {coding_QD_IMA,             "Quantic Dream 4-bit IMA ADPCM"},
        {coding_WV6_IMA,            "Gorilla Systems WV6 4-bit IMA ADPCM"},
        {coding_HV_IMA,             "High Voltage 4-bit IMA ADPCM"},
        {coding_FFTA2_IMA,          "Final Fantasy Tactics A2 4-bit IMA ADPCM"},
        {coding_BLITZ_IMA,          "Blitz Games 4-bit IMA ADPCM"},
        {coding_MTF_IMA,            "MT Framework 4-bit IMA ADPCM"},

        {coding_MS_IMA,             "Microsoft 4-bit IMA ADPCM"},
        {coding_MS_IMA_mono,        "Microsoft 4-bit IMA ADPCM (mono/interleave)"},
        {coding_XBOX_IMA,           "XBOX 4-bit IMA ADPCM"},
        {coding_XBOX_IMA_mch,       "XBOX 4-bit IMA ADPCM (multichannel)"},
        {coding_XBOX_IMA_int,       "XBOX 4-bit IMA ADPCM (mono/interleave)"},
        {coding_NDS_IMA,            "NDS-style 4-bit IMA ADPCM"},
        {coding_DAT4_IMA,           "Eurocom DAT4 4-bit IMA ADPCM"},
        {coding_RAD_IMA,            "Radical 4-bit IMA ADPCM"},
        {coding_RAD_IMA_mono,       "Radical 4-bit IMA ADPCM (mono/interleave)"},
        {coding_APPLE_IMA4,         "Apple Quicktime 4-bit IMA ADPCM"},
        {coding_FSB_IMA,            "FSB 4-bit IMA ADPCM"},
        {coding_WWISE_IMA,          "Audiokinetic Wwise 4-bit IMA ADPCM"},
        {coding_REF_IMA,            "Reflections 4-bit IMA ADPCM"},
        {coding_AWC_IMA,            "Rockstar AWC 4-bit IMA ADPCM"},
        {coding_UBI_IMA,            "Ubisoft 4-bit IMA ADPCM"},
        {coding_UBI_SCE_IMA,        "Ubisoft 4-bit SCE IMA ADPCM"},
        {coding_H4M_IMA,            "Hudson HVQM4 4-bit IMA ADPCM"},
        {coding_CD_IMA,             "Crystal Dynamics 4-bit IMA ADPCM"},

        {coding_MSADPCM,            "Microsoft 4-bit ADPCM"},
        {coding_MSADPCM_int,        "Microsoft 4-bit ADPCM (mono/interleave)"},
        {coding_MSADPCM_ck,         "Microsoft 4-bit ADPCM (Cricket Audio)"},
        {coding_WS,                 "Westwood Studios VBR ADPCM"},
        {coding_AICA,               "Yamaha AICA 4-bit ADPCM"},
        {coding_AICA_int,           "Yamaha AICA 4-bit ADPCM (mono/interleave)"},
        {coding_CP_YM,              "Capcom Yamaha 4-bit ADPCM"},
        {coding_ASKA,               "tri-Ace Aska 4-bit ADPCM"},
        {coding_NXAP,               "Nex NXAP 4-bit ADPCM"},
        {coding_TGC,                "Tiger Game.com 4-bit ADPCM"},
        {coding_NDS_PROCYON,        "Procyon Studio Digital Sound Elements NDS 4-bit APDCM"},
        {coding_L5_555,             "Level-5 0x555 4-bit ADPCM"},
        {coding_LSF,                "lsf 4-bit ADPCM"},
        {coding_MTAF,               "Konami MTAF 4-bit ADPCM"},
        {coding_MTA2,               "Konami MTA2 4-bit ADPCM"},
        {coding_MC3,                "Paradigm MC3 3-bit ADPCM"},
        {coding_FADPCM,             "FMOD FADPCM 4-bit ADPCM"},
        {coding_ASF,                "Argonaut ASF 4-bit ADPCM"},
        {coding_TANTALUS,           "Tantalus 4-bit ADPCM"},
        {coding_DSA,                "Ocean DSA 4-bit ADPCM"},
        {coding_XMD,                "Konami XMD 4-bit ADPCM"},
        {coding_PCFX,               "PC-FX 4-bit ADPCM"},
        {coding_OKI16,              "OKI 4-bit ADPCM (16-bit output)"},
        {coding_OKI4S,              "OKI 4-bit ADPCM (4-shift)"},
        {coding_PTADPCM,            "Platinum 4-bit ADPCM"},
        {coding_IMUSE,              "LucasArts iMUSE VIMA ADPCM"},
        {coding_COMPRESSWAVE,       "CompressWave Huffman ADPCM"},

        {coding_SDX2,               "Squareroot-delta-exact (SDX2) 8-bit DPCM"},
        {coding_SDX2_int,           "Squareroot-delta-exact (SDX2) 8-bit DPCM with 1 byte interleave"},
        {coding_CBD2,               "Cuberoot-delta-exact (CBD2) 8-bit DPCM"},
        {coding_CBD2_int,           "Cuberoot-delta-exact (CBD2) 8-bit DPCM with 1 byte interleave"},
        {coding_SASSC,              "Activision / EXAKT SASSC 8-bit DPCM"},
        {coding_DERF,               "Xilam DERF 8-bit DPCM"},
        {coding_WADY,               "Marble WADY 8-bit DPCM"},
        {coding_NWA,                "VisualArt's NWA DPCM"},
        {coding_ACM,                "InterPlay ACM"},
        {coding_CIRCUS_ADPCM,       "Circus 8-bit ADPCM"},
        {coding_UBI_ADPCM,          "Ubisoft 4/6-bit ADPCM"},

        {coding_EA_MT,              "Electronic Arts MicroTalk"},
        {coding_CIRCUS_VQ,          "Circus VQ"},
        {coding_RELIC,              "Relic Codec"},
        {coding_CRI_HCA,            "CRI HCA"},
        {coding_TAC,                "tri-Ace Codec"},
        {coding_ICE_RANGE,          "Inti Creates Range Codec"},
        {coding_ICE_DCT,            "Inti Creates DCT Codec"},

#ifdef VGM_USE_VORBIS
        {coding_OGG_VORBIS,         "Ogg Vorbis"},
        {coding_VORBIS_custom,      "Custom Vorbis"},
#endif
#ifdef VGM_USE_MPEG
        {coding_MPEG_custom,        "Custom MPEG Audio"},
        {coding_MPEG_ealayer3,      "EALayer3"},
        {coding_MPEG_layer1,        "MPEG Layer I Audio (MP1)"},
        {coding_MPEG_layer2,        "MPEG Layer II Audio (MP2)"},
        {coding_MPEG_layer3,        "MPEG Layer III Audio (MP3)"},
#endif
#ifdef VGM_USE_G7221
        {coding_G7221C,             "ITU G.722.1 annex C (Polycom Siren 14)"},
#endif
#ifdef VGM_USE_G719
        {coding_G719,               "ITU G.719 annex B (Polycom Siren 22)"},
#endif
#ifdef VGM_USE_MAIATRAC3PLUS
        {coding_AT3plus,            "ATRAC3plus"},
#endif
#ifdef VGM_USE_ATRAC9
        {coding_ATRAC9,             "ATRAC9"},
#endif
#ifdef VGM_USE_CELT
        {coding_CELT_FSB,           "Custom CELT"},
#endif
#ifdef VGM_USE_SPEEX
        {coding_SPEEX,              "Custom Speex"},
#endif
#ifdef VGM_USE_FFMPEG
        {coding_FFmpeg,             "FFmpeg"},
#endif
#ifdef VGM_USE_FDKAAC
        {coding_MP4_AAC,            "MPEG-4 AAC"},
#endif
};

static const layout_info layout_info_list[] = {
        {layout_none,                   "flat"},
        {layout_interleave,             "interleave"},

        {layout_segmented,              "segmented"},
        {layout_layered,                "layered"},

        {layout_blocked_mxch,           "blocked (MxCh)"},
        {layout_blocked_ast,            "blocked (AST)"},
        {layout_blocked_halpst,         "blocked (HALPST)"},
        {layout_blocked_xa,             "blocked (XA)"},
        {layout_blocked_ea_schl,        "blocked (EA SCHl)"},
        {layout_blocked_ea_1snh,        "blocked (EA 1SNh)"},
        {layout_blocked_caf,            "blocked (CAF)"},
        {layout_blocked_wsi,            "blocked (WSI)"},
        {layout_blocked_xvas,           "blocked (.xvas)"},
        {layout_blocked_str_snds,       "blocked (.str SNDS)"},
        {layout_blocked_ws_aud,         "blocked (Westwood Studios .aud)"},
        {layout_blocked_matx,           "blocked (Matrix .matx)"},
        {layout_blocked_dec,            "blocked (DEC)"},
        {layout_blocked_vs,             "blocked (Melbourne House VS)"},
        {layout_blocked_mul,            "blocked (MUL)"},
        {layout_blocked_gsb,            "blocked (GSB)"},
        {layout_blocked_thp,            "blocked (THP)"},
        {layout_blocked_filp,           "blocked (FILP)"},
        {layout_blocked_ea_swvr,        "blocked (EA SWVR)"},
        {layout_blocked_adm,            "blocked (ADM)"},
        {layout_blocked_bdsp,           "blocked (BDSP)"},
        {layout_blocked_ivaud,          "blocked (IVAUD)"},
        {layout_blocked_ps2_iab,        "blocked (IAB)"},
        {layout_blocked_vs_str,         "blocked (STR VS)"},
        {layout_blocked_rws,            "blocked (RWS)"},
        {layout_blocked_hwas,           "blocked (HWAS)"},
        {layout_blocked_tra,            "blocked (TRA)"},
        {layout_blocked_ea_sns,         "blocked (EA SNS)"},
        {layout_blocked_awc,            "blocked (AWC)"},
        {layout_blocked_vgs,            "blocked (VGS)"},
        {layout_blocked_xwav,           "blocked (XWAV)"},
        {layout_blocked_xvag_subsong,   "blocked (XVAG subsong)"},
        {layout_blocked_ea_wve_au00,    "blocked (EA WVE au00)"},
        {layout_blocked_ea_wve_ad10,    "blocked (EA WVE Ad10)"},
        {layout_blocked_sthd,           "blocked (STHD)"},
        {layout_blocked_h4m,            "blocked (H4M)"},
        {layout_blocked_xa_aiff,        "blocked (XA AIFF)"},
        {layout_blocked_vs_square,      "blocked (Square VS)"},
        {layout_blocked_vid1,           "blocked (VID1)"},
        {layout_blocked_ubi_sce,        "blocked (Ubi SCE)"},
        {layout_blocked_tt_ad,          "blocked (TT AD)"},
};

static const meta_info meta_info_list[] = {
        {meta_SILENCE,              "Silence"},
        {meta_RSTM,                 "Nintendo RSTM header"},
        {meta_STRM,                 "Nintendo STRM header"},
        {meta_ADX_03,               "CRI ADX header type 03"},
        {meta_ADX_04,               "CRI ADX header type 04"},
        {meta_ADX_05,               "CRI ADX header type 05"},
        {meta_AIX,                  "CRI AIX header"},
        {meta_AAX,                  "CRI AAX header"},
        {meta_UTF_DSP,              "CRI ADPCM_WII header"},
        {meta_AGSC,                 "Retro Studios AGSC header"},
        {meta_CSMP,                 "Retro Studios CSMP header"},
        {meta_RFRM,                 "Retro Studios RFRM header"},
        {meta_DTK,                  "Nintendo DTK raw header"},
        {meta_RSF,                  "Retro Studios RSF raw header"},
        {meta_AFC,                  "Nintendo .AFC header"},
        {meta_AST,                  "Nintendo AST header"},
        {meta_HALPST,               "HAL Laboratory HALPST header"},
        {meta_DSP_RS03,             "Retro Studios RS03 header"},
        {meta_DSP_STD,              "Nintendo DSP header"},
        {meta_DSP_CSTR,             "Namco Cstr header"},
        {meta_GCSW,                 "MileStone GCSW header"},
        {meta_ADS,                  "Sony ADS header"},
        {meta_NPS,                  "Namco NPSF header"},
        {meta_RWSD,                 "Nintendo RWSD header (single stream)"},
        {meta_RWAR,                 "Nintendo RWAR header (single RWAV stream)"},
        {meta_RWAV,                 "Nintendo RWAV header"},
        {meta_CWAV,                 "Nintendo CWAV header"},
        {meta_FWAV,                 "Nintendo FWAV header"},
        {meta_XA,                   "Sony XA header"},
        {meta_RXWS,                 "Sony RXWS header"},
        {meta_RAW_INT,              "PS2 .int raw header"},
        {meta_PS2_OMU,              "Alter Echo OMU Header"},
        {meta_DSP_STM,              "Intelligent Systems STM header"},
        {meta_EXST,                 "Sony EXST header"},
        {meta_SVAG_KCET,            "Konami SVAG header"},
        {meta_PS_HEADERLESS,        "Headerless PS-ADPCM raw header"},
        {meta_MIB_MIH,              "Sony MultiStream MIH+MIB header"},
        {meta_DSP_MPDSP,            "Single DSP header stereo by .mpdsp extension"},
        {meta_PS2_MIC,              "KOEI .MIC header"},
        {meta_DSP_JETTERS,          "Double DSP header stereo by _lr.dsp extension"},
        {meta_DSP_MSS,              "Double DSP header stereo by .mss extension"},
        {meta_DSP_GCM,              "Double DSP header stereo by .gcm extension"},
        {meta_IDSP_TT,              "Traveller's Tales IDSP header"},
        {meta_RSTM_SPM,             "Nintendo RSTM header (brstmspm)"},
        {meta_RAW_PCM,              "PC .raw raw header"},
        {meta_PS2_VAGi,             "Sony VAGi header"},
        {meta_PS2_VAGp,             "Sony VAGp header"},
        {meta_PS2_pGAV,             "Sony pGAV header"},
        {meta_PS2_VAGp_AAAP,        "Acclaim Austin AAAp VAG header"},
        {meta_SEB,                  "Game Arts .SEB header"},
        {meta_STR_WAV,              "Blitz Games .STR+WAV header"},
        {meta_ILD,                  "Tose ILD header"},
        {meta_PS2_PNB,              "assumed PNB (PsychoNauts Bgm File) by .pnb extension"},
        {meta_RAW_WAVM,             "Xbox .wavm raw header"},
        {meta_DSP_STR,              "assumed Conan Gamecube STR File by .str extension"},
        {meta_EA_SCHL,              "Electronic Arts SCHl header (variable)"},
        {meta_EA_SCHL_fixed,        "Electronic Arts SCHl header (fixed)"},
        {meta_CAF,                  "tri-Crescendo CAF Header"},
        {meta_VPK,                  "SCE America VPK Header"},
        {meta_GENH,                 "GENH generic header"},
        {meta_DSP_SADB,             "Procyon Studio SADB header"},
        {meta_SADL,                 "Procyon Studio SADL header"},
        {meta_PS2_BMDX,             "Beatmania .bmdx header"},
        {meta_DSP_WSI,              "Alone in the Dark .WSI header"},
        {meta_AIFC,                 "Apple AIFF-C (Audio Interchange File Format) header"},
        {meta_AIFF,                 "Apple AIFF (Audio Interchange File Format) header"},
        {meta_STR_SNDS,             "3DO SNDS header"},
        {meta_WS_AUD,               "Westwood Studios .aud header"},
        {meta_WS_AUD_old,           "Westwood Studios .aud (old) header"},
        {meta_PS2_IVB,              "IVB/BVII header"},
        {meta_SVS,                  "Square SVS header"},
        {meta_RIFF_WAVE,            "RIFF WAVE header"},
        {meta_RIFF_WAVE_POS,        "RIFF WAVE header and .pos for looping"},
        {meta_NWA,                  "VisualArt's NWA header"},
        {meta_NWA_NWAINFOINI,       "VisualArt's NWA header (NWAINFO.INI looping)"},
        {meta_NWA_GAMEEXEINI,       "VisualArt's NWA header (Gameexe.ini looping)"},
        {meta_XSS,                  "Dino Crisis 3 XSS File"},
        {meta_HGC1,                 "Knights of the Temple 2 hgC1 Header"},
        {meta_AUS,                  "Capcom AUS Header"},
        {meta_RWS,                  "RenderWare RWS header"},
        {meta_EA_1SNH,              "Electronic Arts 1SNh header"},
        {meta_EA_EACS,              "Electronic Arts EACS header"},
        {meta_SL3,                  "Atari Melbourne House SL3 header"},
        {meta_FSB1,                 "FMOD FSB1 header"},
        {meta_FSB2,                 "FMOD FSB2 header"},
        {meta_FSB3,                 "FMOD FSB3 header"},
        {meta_FSB4,                 "FMOD FSB4 header"},
        {meta_FSB5,                 "FMOD FSB5 header"},
        {meta_RWX,                  "RWX Header"},
        {meta_XWB,                  "Microsoft XWB header"},
        {meta_PS2_XA30,             "Reflections XA30 PS2 header"},
        {meta_MUSC,                 "Krome MUSC header"},
        {meta_MUSX,                 "Eurocom MUSX header"},
        {meta_LEG,                  "Legaia 2 - Duel Saga LEG Header"},
        {meta_FILP,                 "Bio Hazard - Gun Survivor FILp Header"},
        {meta_IKM,                  "MiCROViSiON IKM header"},
        {meta_STER,                  "ALCHEMY STER header"},
        {meta_SAT_DVI,              "Konami KCEN DVI. header"},
        {meta_DC_KCEY,              "Konami KCEY KCEYCOMP header"},
        {meta_BG00,                 "Falcom BG00 Header"},
        {meta_PS2_RSTM,             "Rockstar Games RSTM Header"},
        {meta_ACM,                  "InterPlay ACM Header"},
        {meta_MUS_ACM,              "InterPlay MUS ACM header"},
        {meta_PS2_KCES,             "Konami KCES Header"},
        {meta_HXD,                  "Tecmo HXD Header"},
        {meta_VSV,                  "Square Enix .vsv Header"},
        {meta_RIFF_WAVE_labl,       "RIFF WAVE header with loop markers"},
        {meta_RIFF_WAVE_smpl,       "RIFF WAVE header with sample looping info"},
        {meta_RIFF_WAVE_wsmp,       "RIFF WAVE header with wsmp looping info"},
        {meta_RIFX_WAVE,            "RIFX WAVE header"},
        {meta_RIFX_WAVE_smpl,       "RIFX WAVE header with sample looping info"},
        {meta_XNB,                  "Microsoft XNA Game Studio 4.0 header"},
        {meta_SCD_PCM,              "Lunar: Eternal Blue .PCM header"},
        {meta_PS2_PCM,              "Konami KCEJ East .PCM header"},
        {meta_PS2_RKV,              "Legacy of Kain - Blood Omen 2 RKV PS2 header"},
        {meta_PS2_VAS,              "Konami .VAS header"},
        {meta_PS2_ENTH,             ".enth Header"},
        {meta_SDT,                  "High Voltage .sdt header"},
        {meta_NGC_TYDSP,            ".tydsp Header"},
        {meta_WVS,                  "Swingin' Ape .WVS header"},
        {meta_XBOX_MATX,            "assumed Matrix file by .matx extension"},
        {meta_DEC,                  "Falcom DEC RIFF header"},
        {meta_VS,                   "Melbourne House .VS header"},
        {meta_DC_STR,               "Sega Stream Asset Builder header"},
        {meta_DC_STR_V2,            "variant of Sega Stream Asset Builder header"},
        {meta_XMU,                  "Outrage XMU header"},
        {meta_XVAS,                 "Konami .XVAS header"},
        {meta_PS2_XA2,              "Acclaim XA2 Header"},
        {meta_SAP,                  "VING .SAP header"},
        {meta_DC_IDVI,              "Capcom IDVI header"},
        {meta_KRAW,                 "Geometry Wars: Galaxies KRAW header"},
        {meta_NGC_YMF,              "YMF DSP Header"},
        {meta_PS2_CCC,              "CCC Header"},
        {meta_FAG,                  "Radical .FAG Header"},
        {meta_PS2_MIHB,             "Sony MultiStream MIC header"},
        {meta_DSP_WII_MUS,          "mus header"},
        {meta_WII_SNG,              "SNG DSP Header"},
        {meta_RSD,                  "Radical RSD header"},
        {meta_DC_ASD,               "ASD Header"},
        {meta_NAOMI_SPSD,           "Naomi SPSD header"},
        {meta_FFXI_BGW,             "Square Enix .BGW header"},
        {meta_FFXI_SPW,             "Square Enix .SPW header"},
        {meta_PS2_ASS,              "SystemSoft .ASS header"},
        {meta_NUB,                  "Namco NUB header"},
        {meta_IDSP_NL,              "Next Level IDSP header"},
        {meta_IDSP_IE,              "Inevitable Entertainment IDSP Header"},
        {meta_UBI_JADE,             "Ubisoft Jade RIFF header"},
        {meta_SEG,                  "Stormfront SEG header"},
        {meta_NDS_STRM_FFTA2,       "Final Fantasy Tactics A2 RIFF Header"},
        {meta_KNON,                 "Paon KNON header"},
        {meta_ZWDSP,                "Zack and Wiki custom DSP Header"},
        {meta_GCA,                  "GCA DSP Header"},
        {meta_SPT_SPD,              "SPT+SPD DSP Header"},
        {meta_ISH_ISD,              "ISH+ISD DSP Header"},
        {meta_GSP_GSB,              "Tecmo GSP+GSB Header"},
        {meta_YDSP,                 "Yuke's DSP (YDSP) Header"},
        {meta_NGC_SSM,              "SSM DSP Header"},
        {meta_PS2_JOE,              "Asobo Studio .JOE header"},
        {meta_VGS,                  "Guitar Hero VGS Header"},
        {meta_DCS_WAV,              "In Utero DCS+WAV header"},
        {meta_SMP,                  "Infernal Engine .smp header"},
        {meta_MUL,                  "Crystal Dynamics .MUL header"},
        {meta_THP,                  "Nintendo THP header"},
        {meta_STS,                  "Alfa System .STS header"},
        {meta_PS2_P2BT,             "Pop'n'Music 7 Header"},
        {meta_PS2_GBTS,             "Pop'n'Music 9 Header"},
        {meta_NGC_DSP_IADP,         "IADP Header"},
        {meta_RSTM_shrunken,        "Nintendo RSTM header, corrupted by Atlus"},
        {meta_RIFF_WAVE_MWV,        "RIFF WAVE header with .mwv flavoring"},
        {meta_FFCC_STR,             "Final Fantasy: Crystal Chronicles STR header"},
        {meta_SAT_BAKA,             "Konami BAKA header"},
        {meta_SWAV,                 "Nintendo SWAV header"},
        {meta_VSF,                  "Square-Enix VSF header"},
        {meta_NDS_RRDS,             "Ridger Racer DS Header"},
        {meta_PS2_TK5,              "Tekken 5 Stream Header"},
        {meta_PS2_SND,              "Might and Magic SSND Header"},
        {meta_PS2_VSF_TTA,          "VSF with SMSS Header"},
        {meta_ADS_MIDWAY,           "Midway ADS header"},
        {meta_PS2_MCG,              "Gunvari MCG Header"},
        {meta_ZSD,                  "ZSD Header"},
        {meta_REDSPARK,             "RedSpark Header"},
        {meta_IVAUD,                "Rockstar .ivaud header"},
        {meta_DSP_WII_WSD,          ".WSD header"},
        {meta_WII_NDP,              "Icon Games NDP header"},
        {meta_PS2_SPS,              "Ape Escape 2 SPS Header"},
        {meta_PS2_XA2_RRP,          "Acclaim XA2 Header"},
        {meta_NDS_HWAS,             "Vicarious Visions HWAS header"},
        {meta_NGC_LPS,              "Rave Master LPS Header"},
        {meta_NAOMI_ADPCM,          "NAOMI/NAOMI2 Arcade games ADPCM header"},
        {meta_SD9,                  "beatmania IIDX SD9 header"},
        {meta_2DX9,                 "beatmania IIDX 2DX9 header"},
        {meta_DSP_YGO,              "Konami custom DSP Header"},
        {meta_PS2_VGV,              "Rune: Viking Warlord VGV Header"},
        {meta_GCUB,                 "Sega GCub header"},
        {meta_NGC_SCK_DSP,          "The Scorpion King SCK Header"},
        {meta_CAFF,                 "Apple Core Audio Format File header"},
        {meta_PC_MXST,              "Lego Island MxSt Header"},
        {meta_SAB,                  "Sensaura SAB header"},
        {meta_MAXIS_XA,             "Maxis XA Header"},
        {meta_EXAKT_SC,             "assumed Activision / EXAKT SC by extension"},
        {meta_WII_BNS,              "Nintendo BNS header"},
        {meta_WII_WAS,              "Sumo Digital iSWS header"},
        {meta_XBOX_HLWAV,           "Half-Life 2 .WAV header"},
        {meta_MYSPD,                "U-Sing .MYSPD header"},
        {meta_HIS,                  "Her Interactive HIS header"},
        {meta_AST_MV,               "MicroVision AST header"},
        {meta_AST_MMV,              "Marvelous AST header"},
        {meta_DMSG,                 "Microsoft RIFF DMSG header"},
        {meta_PONA_3DO,             "Policenauts BGM header"},
        {meta_PONA_PSX,             "Policenauts BGM header"},
        {meta_NGC_DSP_AAAP,         "Acclaim Austin AAAp DSP header"},
        {meta_NGC_DSP_KONAMI,       "Konami DSP header"},
        {meta_BNSF,                 "Namco Bandai BNSF header"},
        {meta_PS2_WB,               "Shooting Love. ~TRIZEAL~ WB header"},
        {meta_S14,                  "Namco .S14 raw header"},
        {meta_SSS,                  "Namco .SSS raw header"},
        {meta_PS2_GCM,              "Namco GCM header"},
        {meta_PS2_SMPL,             "Homura SMPL header"},
        {meta_PS2_MSA,              "Success .MSA header"},
        {meta_NGC_PDT,              "Hudson .PDT header"},
        {meta_NGC_RKV,              "Legacy of Kain - Blood Omen 2 RKV GC header"},
        {meta_DSP_DDSP,             ".DDSP header"},
        {meta_P3D,                  "Radical P3D header"},
        {meta_PS2_TK1,              "Tekken TK5STRM1 Header"},
        {meta_NGC_DSP_MPDS,         "MPDS DSP header"},
        {meta_DSP_STR_IG,           "Infogrames .DSP header"},
        {meta_EA_SWVR,              "Electronic Arts SWVR header"},
        {meta_PS2_B1S,              "B1S header"},
        {meta_PS2_WAD,              "WAD header"},
        {meta_DSP_XIII,             "XIII dsp header"},
        {meta_DSP_CABELAS,          "Cabelas games .DSP header"},
        {meta_PS2_ADM,              "Dragon Quest V .ADM raw header"},
        {meta_LPCM_SHADE,           "Shade LPCM header"},
        {meta_PS2_VMS,              "VMS Header"},
        {meta_XAU,                  "XPEC XAU header"},
        {meta_GH3_BAR,              "Guitar Hero III Mobile .bar"},
        {meta_FFW,                  "Freedom Fighters BGM header"},
        {meta_DSP_DSPW,             "Capcom DSPW header"},
        {meta_PS2_JSTM,             "JSTM Header"},
        {meta_XVAG,                 "Sony XVAG header"},
        {meta_PS3_CPS,              "tri-Crescendo CPS Header"},
        {meta_SQEX_SCD,             "Square-Enix SCD header"},
        {meta_NGC_NST_DSP,          "Animaniacs NST header"},
        {meta_BAF,                  "Bizarre Creations .baf header"},
        {meta_MSF,                  "Sony MSF header"},
        {meta_PS3_PAST,             "SNDP header"},
        {meta_SGXD,                 "Sony SGXD header"},
        {meta_WII_RAS,              "RAS header"},
        {meta_SPM,                  "Square SPM header"},
        {meta_X360_TRA,             "Terminal Reality .TRA raw header"},
        {meta_VGS_PS,               "Princess Soft VGS header"},
        {meta_PS2_IAB,              "Runtime .IAB header"},
        {meta_VS_STR,               "Square .VS STR* header"},
        {meta_LSF_N1NJ4N,           ".lsf !n1nj4n header"},
        {meta_XWAV,                 "feelplus XWAV header"},
        {meta_RAW_SNDS,             "PC .snds raw header"},
        {meta_PS2_WMUS,             "assumed The Warriors Sony ADPCM by .wmus extension"},
        {meta_HYPERSCAN_KVAG,       "Mattel Hyperscan KVAG"},
        {meta_IOS_PSND,             "PSND Header"},
        {meta_BOS_ADP,              "ADP! header"},
        {meta_QD_ADP,               "Quantic Dream .ADP header"},
        {meta_EB_SFX,               "Excitebots .sfx header"},
        {meta_EB_SF0,               "assumed Excitebots .sf0 by extension"},
        {meta_MTAF,                 "Konami MTAF header"},
        {meta_PS2_VAG1,             "Konami VAG1 header"},
        {meta_PS2_VAG2,             "Konami VAG2 header"},
        {meta_ALP,                  "High Voltage ALP header"},
        {meta_WPD,                  "WPD 'DPW' header"},
        {meta_MN_STR,               "Mini Ninjas 'STR' header"},
        {meta_MSS,                  "Guerilla MCSS header"},
        {meta_PS2_HSF,              "Lowrider 'HSF' header"},
        {meta_IVAG,                 "Namco IVAG header"},
        {meta_PS2_2PFS,             "Konami 2PFS header"},
        {meta_UBI_CKD,              "Ubisoft CKD RIFF header"},
        {meta_PS2_VBK,              "PS2 VBK Header"},
        {meta_OTM,                  "Otomedius OTM Header"},
        {meta_CSTM,                 "Nintendo CSTM Header"},
        {meta_FSTM,                 "Nintendo FSTM Header"},
        {meta_KT_WIIBGM,            "Koei Tecmo WiiBGM Header"},
        {meta_KTSS,                 "Koei Tecmo KTSS header"},
        {meta_IDSP_NAMCO,           "Namco IDSP header"},
        {meta_WIIU_BTSND,           "Nintendo Wii U Menu Boot Sound"},
        {meta_MCA,                  "Capcom MCA header"},
        {meta_XB3D_ADX,             "Xenoblade 3D ADX header"},
        {meta_HCA,                  "CRI HCA header"},
        {meta_SVAG_SNK,             "SNK SVAG header"},
        {meta_PS2_VDS_VDM,          "Procyon Studio VDS/VDM header"},
        {meta_FFMPEG,               "FFmpeg supported format"},
        {meta_FFMPEG_faulty,        "FFmpeg supported format (check log)"},
        {meta_X360_CXS,             "tri-Crescendo CXS header"},
        {meta_AKB,                  "Square-Enix AKB header"},
        {meta_X360_PASX,            "Premium Agency PASX header"},
        {meta_XMA_RIFF,             "Microsoft XMA RIFF header"},
        {meta_X360_AST,             "Capcom AST (X360) header"},
        {meta_WWISE_RIFF,           "Audiokinetic Wwise RIFF header"},
        {meta_UBI_RAKI,             "Ubisoft RAKI header"},
        {meta_SXD,                  "Sony SXD header"},
        {meta_OGL,                  "Shin'en OGL header"},
        {meta_MC3,                  "Paradigm MC3 header"},
        {meta_GTD,                  "GTD/GHS header"},
        {meta_TA_AAC,               "tri-Ace AAC header"},
        {meta_MTA2,                 "Konami MTA2 header"},
        {meta_NGC_ULW,              "Criterion ULW raw header"},
        {meta_XA_XA30,              "Reflections XA30 header"},
        {meta_XA_04SW,              "Reflections 04SW header"},
        {meta_TXTH,                 "TXTH generic header"},
        {meta_EA_BNK,               "Electronic Arts BNK header"},
        {meta_SK_AUD,               "Silicon Knights AUD header"},
        {meta_AHX,                  "CRI AHX header"},
        {meta_STM,                  "Angel Studios/Rockstar San Diego STMA header"},
        {meta_BINK,                 "RAD Game Tools Bink header"},
        {meta_EA_SNU,               "Electronic Arts SNU header"},
        {meta_AWC,                  "Rockstar AWC header"},
        {meta_OPUS,                 "Nintendo Switch OPUS header"},
        {meta_RAW_AL,               "Illwinter Game Design .AL raw header"},
        {meta_PC_AST,               "Capcom AST (PC) header"},
        {meta_UBI_SB,               "Ubisoft SBx header"},
        {meta_NAAC,                 "Namco NAAC header"},
        {meta_EZW,                  "EZ2DJ EZWAVE header"},
        {meta_VXN,                  "Gameloft VXN header"},
        {meta_EA_SNR_SNS,           "Electronic Arts SNR+SNS header"},
        {meta_EA_SPS,               "Electronic Arts SPS header"},
        {meta_VID1,                 "Factor 5 VID1 header"},
        {meta_PC_FLX,               "Ultima IX .FLX header"},
        {meta_MOGG,                 "Harmonix Music Systems MOGG Vorbis"},
        {meta_OGG_VORBIS,           "Ogg Vorbis header"},
        {meta_OGG_SLI,              "Ogg Vorbis header (.sli looping)"},
        {meta_OPUS_SLI,             "Ogg Opus header (.sli looping)"},
        {meta_OGG_SFL,              "Ogg Vorbis header (SFPL looping)"},
        {meta_OGG_KOVS,             "Ogg Vorbis header (KOVS)"},
        {meta_OGG_encrypted,        "Ogg Vorbis header (encrypted)"},
        {meta_KMA9,                 "Koei Tecmo KMA9 header"},
        {meta_XWC,                  "Starbreeze XWC header"},
        {meta_SQEX_SAB,             "Square-Enix SAB header"},
        {meta_SQEX_MAB,             "Square-Enix MAB header"},
        {meta_WAF,                  "KID WAF header"},
        {meta_WAVE,                 "EngineBlack .WAVE header"},
        {meta_WAVE_segmented,       "EngineBlack .WAVE header (segmented)"},
        {meta_SMV,                  "Cho Aniki Zero .SMV header"},
        {meta_NXAP,                 "Nex NXAP header"},
        {meta_EA_WVE_AU00,          "Electronic Arts WVE (au00) header"},
        {meta_EA_WVE_AD10,          "Electronic Arts WVE (Ad10) header"},
        {meta_STHD,                 "Dream Factory STHD header"},
        {meta_MP4,                  "MP4/AAC header"},
        {meta_PCM_SRE,              "Capcom .PCM+SRE header"},
        {meta_DSP_MCADPCM,          "Bethesda .mcadpcm header"},
        {meta_UBI_LYN,              "Ubisoft LyN RIFF header"},
        {meta_MSB_MSH,              "Sony MultiStream MSH+MSB header"},
        {meta_TXTP,                 "TXTP generic header"},
        {meta_SMC_SMH,              "Genki SMC+SMH header"},
        {meta_PPST,                 "Parappa PPST header"},
        {meta_SPS_N1,               "Nippon Ichi .SPS header"},
        {meta_UBI_BAO,              "Ubisoft BAO header"},
        {meta_DSP_SWITCH_AUDIO,     "UE4 Switch Audio header"},
        {meta_SADF,                 "Procyon Studio SADF header"},
        {meta_H4M,                  "Hudson HVQM4 header"},
        {meta_ASF,                  "Argonaut ASF header"},
        {meta_XMD,                  "Konami XMD header"},
        {meta_CKS,                  "Cricket Audio CKS header"},
        {meta_CKB,                  "Cricket Audio CKB header"},
        {meta_WV6,                  "Gorilla Systems WV6 header"},
        {meta_WAVEBATCH,            "Firebrand Games WBAT header"},
        {meta_HD3_BD3,              "Sony HD3+BD3 header"},
        {meta_BNK_SONY,             "Sony BNK header"},
        {meta_SCD_SSCF,             "Square-Enix SCD (SSCF) header"},
        {meta_DSP_VAG,              ".VAG DSP header"},
        {meta_DSP_ITL,              ".ITL DSP header"},
        {meta_A2M,                  "Artificial Mind & Movement A2M header"},
        {meta_AHV,                  "Amuze AHV header"},
        {meta_MSV,                  "Sony MultiStream MSV header"},
        {meta_SDF,                  "Beyond Reality SDF header"},
        {meta_SVG,                  "High Voltage SVG header"},
        {meta_VIS,                  "Konami VIS header"},
        {meta_VAI,                  "Asobo Studio .VAI header"},
        {meta_AIF_ASOBO,            "Asobo Studio .AIF header"},
        {meta_AO,                   "AlphaOgg .AO header"},
        {meta_APC,                  "Cryo APC header"},
        {meta_WV2,                  "Infogrames North America WAV2 header"},
        {meta_XAU_KONAMI,           "Konami XAU header"},
        {meta_DERF,                 "Xilam DERF header"},
        {meta_UTK,                  "Maxis UTK header"},
        {meta_NXA,                  "Entergram NXA header"},
        {meta_ADPCM_CAPCOM,         "Capcom .ADPCM header"},
        {meta_UE4OPUS,              "Epic Games UE4OPUS header"},
        {meta_XWMA,                 "Microsoft XWMA RIFF header"},
        {meta_VA3,                  "Konami VA3 header" },
        {meta_XOPUS,                "Exient XOPUS header"},
        {meta_VS_SQUARE,            "Square VS header"},
        {meta_NWAV,                 "Chunsoft NWAV header"},
        {meta_XPCM,                 "Circus XPCM header"},
        {meta_MSF_TAMASOFT,         "Tama-Soft MSF header"},
        {meta_XPS_DAT,              "From Software .XPS+DAT header"},
        {meta_ZSND,                 "Z-Axis ZSND header"},
        {meta_DSP_ADPY,             "AQUASTYLE ADPY header"},
        {meta_DSP_ADPX,             "AQUASTYLE ADPX header"},
        {meta_OGG_OPUS,             "Ogg Opus header"},
        {meta_IMC,                  "iNiS .IMC header"},
        {meta_GIN,                  "Electronic Arts Gnsu header"},
        {meta_DSF,                  "Ocean DSF header"},
        {meta_208,                  "Ocean .208 header"},
        {meta_DSP_DS2,              "LucasArts .DS2 header"},
        {meta_MUS_VC,               "Vicious Cycle .MUS header"},
        {meta_STRM_ABYLIGHT,        "Abylight STRM header"},
        {meta_MSF_KONAMI,           "Konami MSF header"},
        {meta_XWMA_KONAMI,          "Konami XWMA header"},
        {meta_9TAV,                 "Konami 9TAV header"},
        {meta_BWAV,                 "Nintendo BWAV header"},
        {meta_RAD,                  "Traveller's Tales .RAD header"},
        {meta_SMACKER,              "RAD Game Tools SMACKER header"},
        {meta_MZRT,                 "id Software MZRT header"},
        {meta_XAVS,                 "Reflections XAVS header"},
        {meta_PSF,                  "Pivotal PSF header"},
        {meta_DSP_ITL_i,            "Infernal .ITL DSP header"},
        {meta_IMA,                  "Blitz Games .IMA header"},
        {meta_XMV_VALVE,            "Valve XMV header"},
        {meta_UBI_HX,               "Ubisoft HXx header"},
        {meta_BMP_KONAMI,           "Konami BMP header"},
        {meta_ISB,                  "Creative ISACT header"},
        {meta_XSSB,                 "Artoon XSSB header"},
        {meta_XMA_UE3,              "Unreal Engine XMA header"},
        {meta_FWSE,                 "MT Framework FWSE header"},
        {meta_FDA,                  "Relic FDA header"},
        {meta_TGC,                  "Tiger Game.com .4 header"},
        {meta_KWB,                  "Koei Tecmo WaveBank header"},
        {meta_LRMD,                 "Sony LRMD header"},
        {meta_WWISE_FX,             "Audiokinetic Wwise FX header"},
        {meta_DIVA,                 "DIVA header"},
        {meta_IMUSE,                "LucasArts iMUSE header"},
        {meta_KTSR,                 "Koei Tecmo KTSR header"},
        {meta_KAT,                  "Sega KAT header"},
        {meta_PCM_SUCCESS,          "Success PCM header"},
        {meta_ADP_KONAMI,           "Konami ADP header"},
        {meta_SDRH,                 "feelplus SDRH header"},
        {meta_WADY,                 "Marble WADY header"},
        {meta_DSP_SQEX,             "Square Enix DSP header"},
        {meta_DSP_WIIVOICE,         "Koei Tecmo WiiVoice header"},
        {meta_SBK,                  "Team17 SBK header"},
        {meta_DSP_WIIADPCM,         "Exient WIIADPCM header"},
        {meta_DSP_CWAC,             "CRI CWAC header"},
        {meta_COMPRESSWAVE,         "CompressWave .cwav header"},
        {meta_KTAC,                 "Koei Tecmo KTAC header"},
        {meta_MJB_MJH,              "Sony MultiStream MJH+MJB header"},
        {meta_BSNF,                 "id Software BSNF header"},
        {meta_TAC,                  "tri-Ace Codec header"},
        {meta_IDSP_TOSE,            "TOSE .IDSP header"},
        {meta_DSP_KWA,              "Kuju London .KWA header"},
        {meta_OGV_3RDEYE,           "3rdEye .OGV header"},
        {meta_PIFF_TPCM,            "Tantalus PIFF TPCM header"},
        {meta_WXD_WXH,              "Relic WXD+WXH header"},
        {meta_BNK_RELIC,            "Relic BNK header"},
        {meta_XSH_XSD_XSS,          "Treyarch XSH+XSD/XSS header"},
        {meta_PSB,                  "M2 PSB header"},
        {meta_LOPU_FB,              "French-Bread LOPU header"},
        {meta_LPCM_FB,              "French-Bread LPCM header"},
        {meta_WBK,                  "Treyarch WBK header"},
        {meta_WBK_NSLB,             "Treyarch NSLB header"},
        {meta_DSP_APEX,             "Koei Tecmo APEX header"},
        {meta_MPEG,                 "MPEG header"},
        {meta_SSPF,                 "Konami SSPF header"},
        {meta_S3V,                  "Konami S3V header"},
        {meta_ESF,                  "Eurocom ESF header"},
        {meta_ADM3,                 "Crankcase ADM3 header"},
        {meta_TT_AD,                "Traveller's Tales AUDIO_DATA header"},
        {meta_SNDZ,                 "Sony SNDZ header"},
        {meta_VAB,                  "Sony VAB header"},
        {meta_BIGRP,                "Inti Creates .BIGRP header"},
};

void get_vgmstream_coding_description(VGMSTREAM* vgmstream, char* out, size_t out_size) {
    int i, list_length;
    const char *description;

#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type == coding_FFmpeg) {
        /* recurse down for FFmpeg, but metas should set prefered/main codec, or maybe print a list of codecs */
        if (vgmstream->layout_type == layout_layered) {
            layered_layout_data* layout_data = vgmstream->layout_data;
            get_vgmstream_coding_description(layout_data->layers[0], out, out_size);
            return;
        }
        else if (vgmstream->layout_type == layout_segmented) {
            segmented_layout_data* layout_data = vgmstream->layout_data;
            get_vgmstream_coding_description(layout_data->segments[0], out, out_size);
            return;
        }
    }
#endif

    description = "CANNOT DECODE";

    switch (vgmstream->coding_type) {
#ifdef VGM_USE_FFMPEG
        case coding_FFmpeg:
            description = ffmpeg_get_codec_name(vgmstream->codec_data);
            if (description == NULL)
                description = "FFmpeg";
            break;
#endif
        default:
            list_length = sizeof(coding_info_list) / sizeof(coding_info);
            for (i = 0; i < list_length; i++) {
                if (coding_info_list[i].type == vgmstream->coding_type)
                    description = coding_info_list[i].description;
            }
            break;
    }

    strncpy(out, description, out_size);
}

static const char* get_layout_name(layout_t layout_type) {
    int i, list_length;

    list_length = sizeof(layout_info_list) / sizeof(layout_info);
    for (i = 0; i < list_length; i++) {
        if (layout_info_list[i].type == layout_type)
            return layout_info_list[i].description;
    }

    return NULL;
}

static int has_sublayouts(VGMSTREAM** vgmstreams, int count) {
    int i;
    for (i = 0; i < count; i++) {
        if (vgmstreams[i]->layout_type == layout_segmented || vgmstreams[i]->layout_type == layout_layered)
            return 1;
    }
    return 0;
}

/* Makes a mixed description, considering a segments/layers can contain segments/layers infinitely, like:
 *
 * "(L3[S2L2]S3)"        "(S3[L2[S2S2]])"
 *  L3                    S3
 *    S2                    L2
 *      file                  S2
 *      file                    file
 *    file                      file
 *    L2                      file
 *      file                file
 *      file                file
 *
 * ("mixed" is added externally)
 */
static int get_layout_mixed_description(VGMSTREAM* vgmstream, char* dst, int dst_size) {
    int i, count, done = 0;
    VGMSTREAM** vgmstreams = NULL;

    if (vgmstream->layout_type == layout_layered) {
        layered_layout_data* data = vgmstream->layout_data;
        vgmstreams = data->layers;
        count = data->layer_count;
        done = snprintf(dst, dst_size, "L%i", count);
    }
    else if (vgmstream->layout_type == layout_segmented) {
        segmented_layout_data* data = vgmstream->layout_data;
        vgmstreams = data->segments;
        count = data->segment_count;
        done = snprintf(dst, dst_size, "S%i", count);
    }

    if (!vgmstreams || done == 0 || done >= dst_size)
        return 0;

    if (!has_sublayouts(vgmstreams, count))
        return done;

    if (done + 1 < dst_size) {
        dst[done++] = '[';
    }

    for (i = 0; i < count; i++) {
        done += get_layout_mixed_description(vgmstreams[i], dst + done, dst_size - done);
    }

    if (done + 1 < dst_size) {
        dst[done++] = ']';
    }

    return done;
}

void get_vgmstream_layout_description(VGMSTREAM* vgmstream, char* out, size_t out_size) {
    const char* description;
    int mixed = 0;

    description = get_layout_name(vgmstream->layout_type);
    if (!description) description = "INCONCEIVABLE";

    if (vgmstream->layout_type == layout_layered) {
        layered_layout_data* data = vgmstream->layout_data;
        mixed = has_sublayouts(data->layers, data->layer_count);
        if (!mixed)
            snprintf(out, out_size, "%s (%i layers)", description, data->layer_count);
    }
    else if (vgmstream->layout_type == layout_segmented) {
        segmented_layout_data* data = vgmstream->layout_data;
        mixed = has_sublayouts(data->segments, data->segment_count);
        if (!mixed)
            snprintf(out, out_size, "%s (%i segments)", description, data->segment_count);
    }
    else {
        snprintf(out, out_size, "%s", description);
    }

    if (mixed) {
        char tmp[256] = {0};

        get_layout_mixed_description(vgmstream, tmp, sizeof(tmp) - 1);
        snprintf(out, out_size, "mixed (%s)", tmp);
        return;
    }
}

void get_vgmstream_meta_description(VGMSTREAM* vgmstream, char* out, size_t out_size) {
    int i, list_length;
    const char* description;

    description = "THEY SHOULD HAVE SENT A POET";

    list_length = sizeof(meta_info_list) / sizeof(meta_info);
    for (i=0; i < list_length; i++) {
        if (meta_info_list[i].type == vgmstream->meta_type)
            description = meta_info_list[i].description;
    }

    strncpy(out, description, out_size);
}
