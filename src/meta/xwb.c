#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"
#include <string.h>
#include "xwb_xsb.h"

/* most info from XWBtool, xactwb.h, xact2wb.h and xact3wb.h */

#define WAVEBANK_FLAGS_COMPACT              0x00020000  // Bank uses compact format
#define WAVEBANKENTRY_FLAGS_IGNORELOOP      0x00000008  // Used internally when the loop region can't be used (no idea...)

/* the x.x version is just to make it clearer, MS only classifies XACT as 1/2/3 */
#define XACT1_0_MAX     1   // 0x01 // Project Gotham Racing 2 (Xbox)-v01, Silent Hill 4 (Xbox)-v01, Shin Megami Tensei NINE (Xbox)-v01
#define XACT1_1_MAX     3   // 0x03 // Unreal Championship (Xbox)-v02, The King of Fighters 2003 (Xbox)-v03
#define XACT2_0_MAX     34  // 0x22 // Project Gotham Racing 3 (X360)-v22, Dead or Alive 4 (X360)-v23, Table Tennis (X360)-v34 // v35/36/37 too?
#define XACT2_1_MAX     38  // 0x26 // Prey (X360)-v38
#define XACT2_2_MAX     41  // 0x29 // Just Cause (X360)-v39, Blue Dragon (X360)-v40
#define XACT3_0_MAX     46  // 0x2E // Ninja Blade (X360)-t43-v42, Saints Row 2 (PC)-t44-v42, Persona 4 Ultimax NESSICA (PC)-t45-v43, BlazBlue (X360)-t46-v44
#define XACT_TECHLAND   0x10000     // Sniper Ghost Warrior (PS3/X360), Nail'd (PS3/X360), equivalent to XACT3_0 
#define XACT_CRACKDOWN  0x87        // Crackdown 1, equivalent to XACT2_2

static const int wma_avg_bps_index[7] = {
    12000, 24000, 4000, 6000, 8000, 20000, 2500
};
static const int wma_block_align_index[17] = {
    929, 1487, 1280, 2230, 8917, 8192, 4459, 5945, 2304, 1536, 1485, 1008, 2731, 4096, 6827, 5462, 1280
};


typedef enum { PCM, XBOX_ADPCM, MS_ADPCM, XMA1, XMA2, WMA, XWMA, ATRAC3, OGG, DSP, ATRAC9_RIFF } xact_codec;
typedef struct {
    int little_endian;
    int version;

    /* segments */
    uint32_t base_offset;
    uint32_t base_size;
    uint32_t entry_offset;
    uint32_t entry_size;
    uint32_t names_offset;
    uint32_t names_size;
    uint32_t names_entry_size;
    uint32_t extra_offset;
    uint32_t extra_size;
    uint32_t data_offset;
    uint32_t data_size;

    uint32_t stream_offset;
    uint32_t stream_size;

    uint32_t base_flags;
    uint32_t entry_elem_size;
    uint32_t entry_alignment;
    int total_subsongs;

    uint32_t entry_flags;
    uint32_t format;
    int tag;
    int channels;
    int sample_rate;
    int block_align;
    int bits_per_sample;
    xact_codec codec;

    int loop_flag;
    uint32_t num_samples;
    uint32_t loop_start;
    uint32_t loop_end;
    uint32_t loop_start_sample;
    uint32_t loop_end_sample;

    char wavebank_name[64+1];

    int is_crackdown;
    int fix_xma_num_samples;
    int fix_xma_loop_samples;
} xwb_header;

static void get_name(char* buf, size_t buf_size, int target_subsong, xwb_header* xwb, STREAMFILE* sf_xwb, STREAMFILE* sf_xsb);


VGMSTREAM* init_vgmstream_wbnd(STREAMFILE* sf) {
    return init_vgmstream_wbnd_sdbk(sf, NULL);
}

/* XWB - XACT Wave Bank (Microsoft SDK format for XBOX/XBOX360/Windows) */
VGMSTREAM* init_vgmstream_wbnd_sdbk(STREAMFILE* sf, STREAMFILE* sf_xsb) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, offset, suboffset;
    xwb_header xwb = {0};
    int target_subsong = sf->stream_index;
    read_u32_t read_u32 = NULL;
    read_s32_t read_s32 = NULL;
    char stream_name[STREAM_NAME_SIZE], file_name[STREAM_NAME_SIZE];


    /* checks */
    if (!is_id32be(0x00,sf, "WBND") &&
        !is_id32le(0x00,sf, "WBND")) /* X360 */
        return NULL;

    /* .xwb: standard
     * .xna: Touhou Makukasai ~ Fantasy Danmaku Festival (PC)
     * (extensionless): Ikaruga (X360/PC), Grabbed by the Ghoulies (Xbox)
     * .hwb: Burnout Revenge (X360)
     * .bd: Fatal Frame 2 (Xbox) */
    if (!check_extensions(sf,"xwb,xna,hwb,bd,"))
        return NULL;

    xwb.little_endian = is_id32be(0x00,sf, "WBND"); /* Xbox/PC */
    if (xwb.little_endian) {
        read_u32 = read_u32le;
        read_s32 = read_s32le;
    } else {
        read_u32 = read_u32be;
        read_s32 = read_s32be;
    }


    /* read main header (WAVEBANKHEADER) */
    xwb.version = read_u32(0x04, sf); /* XACT3: 0x04=tool version, 0x08=header version */

    /* Crackdown 1 (X360), essentially XACT2 but may have split header in some cases, compact entries change */
    if (xwb.version == XACT_CRACKDOWN) {
        xwb.version = XACT2_2_MAX;
        xwb.is_crackdown = 1;
    }

    /* read segment offsets (SEGIDX) */
    if (xwb.version <= XACT1_0_MAX) {
        xwb.total_subsongs = read_s32(0x0c, sf);
        read_string(xwb.wavebank_name,0x10+1, 0x10, sf); /* null-terminated */
        xwb.base_offset     = 0;
        xwb.base_size       = 0;
        xwb.entry_offset    = 0x50;
        xwb.entry_elem_size = 0x14;
        xwb.entry_size      = xwb.entry_elem_size * xwb.total_subsongs;
        xwb.data_offset     = xwb.entry_offset + xwb.entry_size;
        xwb.data_size       = get_streamfile_size(sf) - xwb.data_offset;

        xwb.names_offset    = 0;
        xwb.names_size      = 0;
        xwb.names_entry_size= 0;
        xwb.extra_offset    = 0;
        xwb.extra_size      = 0;
    }
    else {
        offset = xwb.version <= XACT2_2_MAX ? 0x08 : 0x0c;
        xwb.base_offset = read_s32(offset+0x00, sf);//BANKDATA
        xwb.base_size   = read_s32(offset+0x04, sf);
        xwb.entry_offset= read_s32(offset+0x08, sf);//ENTRYMETADATA
        xwb.entry_size  = read_s32(offset+0x0c, sf);

        /* read extra segments (values can be 0 == no segment) */
        if (xwb.version <= XACT1_1_MAX) {
            xwb.names_offset    = read_s32(offset+0x10, sf);//ENTRYNAMES
            xwb.names_size      = read_s32(offset+0x14, sf);
            xwb.names_entry_size= 0x40;
            xwb.extra_offset    = 0;
            xwb.extra_size      = 0;
            suboffset = 0x04*2;
        }
        else if (xwb.version <= XACT2_1_MAX) {
            xwb.names_offset    = read_s32(offset+0x10, sf);//ENTRYNAMES
            xwb.names_size      = read_s32(offset+0x14, sf);
            xwb.names_entry_size= 0x40;
            xwb.extra_offset    = read_s32(offset+0x18, sf);//EXTRA
            xwb.extra_size      = read_s32(offset+0x1c, sf);
            suboffset = 0x04*2 + 0x04*2;
        } else {
            xwb.extra_offset    = read_s32(offset+0x10, sf);//SEEKTABLES
            xwb.extra_size      = read_s32(offset+0x14, sf);
            xwb.names_offset    = read_s32(offset+0x18, sf);//ENTRYNAMES
            xwb.names_size      = read_s32(offset+0x1c, sf);
            xwb.names_entry_size= 0x40;
            suboffset = 0x04*2 + 0x04*2;
        }

        xwb.data_offset = read_s32(offset+0x10+suboffset, sf);//ENTRYWAVEDATA
        xwb.data_size   = read_s32(offset+0x14+suboffset, sf);

        /* for Techland's XWB with no data */
        if (xwb.base_offset == 0) goto fail;

        /* read base entry (WAVEBANKDATA) */
        offset = xwb.base_offset;
        xwb.base_flags = read_u32(offset+0x00, sf);
        xwb.total_subsongs       = read_s32(offset+0x04, sf);
        read_string(xwb.wavebank_name,0x40+1, offset+0x08, sf); /* null-terminated */
        suboffset = 0x08 + (xwb.version <= XACT1_1_MAX ? 0x10 : 0x40);
        xwb.entry_elem_size = read_s32(offset+suboffset+0x00, sf);
        /* suboff+0x04: meta name entry size */
        xwb.entry_alignment = read_s32(offset+suboffset+0x08, sf); /* usually 1 dvd sector */
        xwb.format          = read_s32(offset+suboffset+0x0c, sf); /* compact mode only */
        /* suboff+0x10: build time 64b (XACT2/3) */
    }

    //;VGM_LOG("XWB: wavebank name='%s'\n", xwb.wavebank_name);

    if (target_subsong == 0) target_subsong = 1; /* auto: default to 1 */
    if (target_subsong < 0 || target_subsong > xwb.total_subsongs || xwb.total_subsongs < 1) goto fail;


    /* read stream entry (WAVEBANKENTRY) */
    offset = xwb.entry_offset + (target_subsong-1) * xwb.entry_elem_size;


    if ((xwb.base_flags & WAVEBANK_FLAGS_COMPACT) && xwb.is_crackdown) {
        /* mutant compact (w/ entry_elem_size=0x08) [Crackdown (X360)] */
        uint32_t entry, size_sectors, sector_offset;

        entry = read_u32(offset+0x00, sf);
        size_sectors = ((entry >> 19) & 0x1FFF); /* 13b, exact size in sectors */
        sector_offset = (entry & 0x7FFFF); /* 19b, offset within data in sectors */
        xwb.stream_size = size_sectors * xwb.entry_alignment;
        xwb.num_samples = read_u32(offset+0x04, sf);

        xwb.stream_offset  = xwb.data_offset + sector_offset * xwb.entry_alignment;
    }
    else if (xwb.base_flags & WAVEBANK_FLAGS_COMPACT) {
        /* compact entry [NFL Fever 2004 demo from Amped 2 (Xbox)] */
        uint32_t entry, size_deviation, sector_offset;
        off_t next_stream_offset;

        entry = read_u32(offset+0x00, sf);
        size_deviation = ((entry >> 21) & 0x7FF); /* 11b, padding data for sector alignment in bytes*/
        sector_offset = (entry & 0x1FFFFF); /* 21b, offset within data in sectors */

        xwb.stream_offset  = xwb.data_offset + sector_offset * xwb.entry_alignment;

        /* find size using next offset */
        if (target_subsong < xwb.total_subsongs) {
            uint32_t next_entry = read_u32(offset + xwb.entry_elem_size, sf);
            next_stream_offset = xwb.data_offset + (next_entry & 0x1FFFFF) * xwb.entry_alignment;
        }
        else { /* for last entry (or first, when subsongs = 1) */
            next_stream_offset = xwb.data_offset + xwb.data_size;
        }
        xwb.stream_size = next_stream_offset - xwb.stream_offset - size_deviation;
    }
    else if (xwb.version <= XACT1_0_MAX) {
        xwb.format          = read_u32(offset+0x00, sf);
        xwb.stream_offset   = xwb.data_offset + read_u32(offset+0x04, sf);
        xwb.stream_size     = read_u32(offset+0x08, sf);

        xwb.loop_start      = read_u32(offset+0x0c, sf);
        xwb.loop_end        = read_u32(offset+0x10, sf);//length
    }
    else {
        uint32_t entry_info = read_u32(offset+0x00, sf);
        if (xwb.version <= XACT1_1_MAX) {
            xwb.entry_flags = entry_info;
        }
        else {
            xwb.entry_flags = (entry_info) & 0xF; /*4b*/
            xwb.num_samples = (entry_info >> 4) & 0x0FFFFFFF; /*28b*/
        }

        xwb.format          = read_u32(offset+0x04, sf);
        xwb.stream_offset   = xwb.data_offset + read_u32(offset+0x08, sf);
        xwb.stream_size     = read_u32(offset+0x0c, sf);

        if (xwb.version <= XACT2_1_MAX) { /* LoopRegion (bytes) */
            xwb.loop_start  = read_u32(offset+0x10, sf);
            xwb.loop_end    = read_u32(offset+0x14, sf);//length (LoopRegion) or offset (XMALoopRegion in late XACT2)
        } else { /* LoopRegion (samples) */
            xwb.loop_start_sample   = read_u32(offset+0x10, sf);
            xwb.loop_end_sample     = read_u32(offset+0x14, sf) + xwb.loop_start_sample;
        }
    }


    /* parse format */
    if (xwb.version <= XACT1_0_MAX) {
        xwb.bits_per_sample = (xwb.format >> 31) & 0x1; /*1b*/
        xwb.sample_rate     = (xwb.format >> 4) & 0x7FFFFFF; /*27b*/
        xwb.channels        = (xwb.format >> 1) & 0x7; /*3b*/
        xwb.tag             = (xwb.format) & 0x1; /*1b*/
    }
    else if (xwb.version <= XACT1_1_MAX) {
        xwb.bits_per_sample = (xwb.format >> 31) & 0x1; /*1b*/
        xwb.sample_rate     = (xwb.format >> 5) & 0x3FFFFFF; /*26b*/
        xwb.channels        = (xwb.format >> 2) & 0x7; /*3b*/
        xwb.tag             = (xwb.format) & 0x3; /*2b*/
    }
    else if (xwb.version <= XACT2_0_MAX) {
        xwb.bits_per_sample = (xwb.format >> 31) & 0x1; /*1b*/
        xwb.block_align     = (xwb.format >> 24) & 0xFF; /*8b*/
        xwb.sample_rate     = (xwb.format >> 4) & 0x7FFFF; /*19b*/
        xwb.channels        = (xwb.format >> 1) & 0x7; /*3b*/
        xwb.tag             = (xwb.format) & 0x1; /*1b*/
    }
    else {
        xwb.bits_per_sample = (xwb.format >> 31) & 0x1; /*1b*/
        xwb.block_align     = (xwb.format >> 23) & 0xFF; /*8b*/
        xwb.sample_rate     = (xwb.format >> 5) & 0x3FFFF; /*18b*/
        xwb.channels        = (xwb.format >> 2) & 0x7; /*3b*/
        xwb.tag             = (xwb.format) & 0x3; /*2b*/
    }

    /* standardize tag to codec */
    if (xwb.version <= XACT1_0_MAX) {
        switch(xwb.tag){
            case 0: xwb.codec = PCM; break;
            case 1: xwb.codec = XBOX_ADPCM; break;
            default: goto fail;
        }
    }
    else if (xwb.version <= XACT1_1_MAX) {
        switch(xwb.tag){
            case 0: xwb.codec = PCM; break;
            case 1: xwb.codec = XBOX_ADPCM; break;
            case 2: xwb.codec = WMA; break;
            case 3: xwb.codec = OGG; break; /* extension */
            default: goto fail;
        }
    }
    else if (xwb.version <= XACT2_2_MAX) {
        switch(xwb.tag) {
            case 0: xwb.codec = PCM; break;
            /* Table Tennis (v34)~Prey (v38): XMA1, Just Cause (v39): XMA2 */
            case 1: xwb.codec = xwb.version <= XACT2_1_MAX ? XMA1 : XMA2; break;
            case 2: xwb.codec = MS_ADPCM; break;
            default: goto fail;
        }
    }
    else {
        switch(xwb.tag) {
            case 0: xwb.codec = PCM; break;
            case 1: xwb.codec = XMA2; break;
            case 2: xwb.codec = MS_ADPCM; break;
            case 3: xwb.codec = XWMA; break;
            default: goto fail;
        }
    }


    /* format hijacks from creative devs, using non-official codecs */
    if (xwb.version == XACT_TECHLAND && xwb.codec == XMA2 /* XACT_TECHLAND used in their X360 games too */
            && (xwb.block_align == 0x60 || xwb.block_align == 0x98 || xwb.block_align == 0xc0) ) { /* standard ATRAC3 blocks sizes */
        /* Techland ATRAC3 [Nail'd (PS3), Sniper: Ghost Warrior (PS3)] */
        xwb.codec = ATRAC3;

        /* num samples uses a modified entry_info format (maybe skip samples + samples? sfx use the standard format)
         * ignore for now and just calc max samples */
        xwb.num_samples = atrac3_bytes_to_samples(xwb.stream_size, xwb.block_align * xwb.channels);
    }
    else if (xwb.codec == OGG) {
        /* Oddworld: Stranger's Wrath (iOS/Android) */
        xwb.num_samples = xwb.stream_size / (2 * xwb.channels); /* uncompressed bytes */
        xwb.stream_size = xwb.loop_end;
        xwb.loop_start = 0;
        xwb.loop_end = 0;
    }
    else if (xwb.version == XACT3_0_MAX && xwb.codec == XMA2
            && (xwb.bits_per_sample == 0x00 || xwb.bits_per_sample == 0x01) /* bps=0+ba=2 in mono? (Blossom Tales) */
            && (xwb.block_align == 0x02 || xwb.block_align == 0x04)
            && read_u32le(xwb.stream_offset + 0x08, sf) == xwb.sample_rate /* DSP header */
            && read_u16le(xwb.stream_offset + 0x0e, sf) == 0
            && read_u32le(xwb.stream_offset + 0x18, sf) == 2
            /*&& xwb.data_size == 0x55951c1c*/) { /* some kind of id in Stardew Valley? */
        /* Stardew Valley (Switch), Skulls of the Shogun (Switch): full interleaved DSPs (including headers) */
        xwb.codec = DSP;
    }
    else if (xwb.version == XACT3_0_MAX && (xwb.codec == XMA2 || xwb.codec == PCM)
            && xwb.bits_per_sample == 0x01 && xwb.block_align == 0x02*xwb.channels
            && is_id32be(xwb.stream_offset, sf, "RIFF") /* clashes with XMA2 */
            /*&& xwb.data_size == 0x4e0a1000*/) { /* some kind of id in Stardew Valley? */
        /* Stardew Valley (Vita), Owlboy (PS4): standard RIFF with ATRAC9 */
        xwb.codec = ATRAC9_RIFF;
    }
    else if (xwb.version == XACT1_1_MAX && xwb.codec == WMA
        && read_u32be(xwb.stream_offset, sf) != 0x3026B275) { /* WMA/asf tag */
        /* Jumper: Griffin's Story (X360): partial hijack (LE on X360 and early version + XMA2) */
        xwb.codec = XMA2;
    }


    /* test loop after the above fixes */
    xwb.loop_flag = (xwb.loop_end > 0 || xwb.loop_end_sample > xwb.loop_start)
        && !(xwb.entry_flags & WAVEBANKENTRY_FLAGS_IGNORELOOP);

    /* Oddworld OGG the data_size value is size of uncompressed bytes instead; DSP uses some id/config as value */
    if (xwb.codec != OGG && xwb.codec != DSP && xwb.codec != ATRAC9_RIFF) {
        /* some low-q rips don't remove padding, relax validation a bit */
        if (xwb.data_offset + xwb.stream_size > get_streamfile_size(sf))
            goto fail;
        //if (xwb.data_offset + xwb.data_size > get_streamfile_size(sf)) /* badly split */
        //    goto fail;
    }


    /* fix samples */
    if (xwb.version <= XACT2_2_MAX && xwb.codec == PCM) {
        int bits_per_sample = xwb.bits_per_sample == 0 ? 8 : 16;
        xwb.num_samples = pcm_bytes_to_samples(xwb.stream_size, xwb.channels, bits_per_sample);
        if (xwb.loop_flag) {
            xwb.loop_start_sample = pcm_bytes_to_samples(xwb.loop_start, xwb.channels, bits_per_sample);
            xwb.loop_end_sample   = pcm_bytes_to_samples(xwb.loop_start + xwb.loop_end, xwb.channels, bits_per_sample);
        }
    }
    else if (xwb.version <= XACT1_1_MAX && xwb.codec == XBOX_ADPCM) {
        xwb.block_align = 0x24 * xwb.channels; /* not really needed... */
        xwb.num_samples = xbox_ima_bytes_to_samples(xwb.stream_size, xwb.channels);
        if (xwb.loop_flag) {
            xwb.loop_start_sample = xbox_ima_bytes_to_samples(xwb.loop_start, xwb.channels);
            xwb.loop_end_sample   = xbox_ima_bytes_to_samples(xwb.loop_start + xwb.loop_end, xwb.channels);
        }
    }
    else if (xwb.version <= XACT2_2_MAX && xwb.codec == MS_ADPCM && xwb.loop_flag) {
        int block_size = (xwb.block_align + 22) * xwb.channels; /*22=CONVERSION_OFFSET (?)*/

        xwb.loop_start_sample = msadpcm_bytes_to_samples(xwb.loop_start, block_size, xwb.channels);
        xwb.loop_end_sample   = msadpcm_bytes_to_samples(xwb.loop_start + xwb.loop_end, block_size, xwb.channels);
    }
    else if ((xwb.version <= XACT2_1_MAX && (xwb.codec == XMA1 || xwb.codec == XMA2) && xwb.loop_flag)
                || (xwb.version == XACT_TECHLAND && xwb.codec == XMA2)) {
        /* v38: byte offset, v39/v40+: sample offset */
        /* need to manually find sample offsets, thanks to Microsoft's dumb headers */
        ms_sample_data msd = {0};

        msd.xma_version = xwb.codec == XMA1 ? 1 : 2;
        msd.channels    = xwb.channels;
        msd.data_offset = xwb.stream_offset;
        msd.data_size   = xwb.stream_size;
        msd.loop_flag   = xwb.loop_flag;
        msd.loop_start_b = xwb.loop_start; /* bit offset in the stream */
        msd.loop_end_b   = (xwb.loop_end >> 4); /*28b */
        /* XACT adds +1 to the subframe, but this means 0 can't be used? */
        msd.loop_end_subframe    = ((xwb.loop_end >> 2) & 0x3) + 1; /* 2b */
        msd.loop_start_subframe  = ((xwb.loop_end >> 0) & 0x3) + 1; /* 2b */

        xma_get_samples(&msd, sf);
        xwb.loop_start_sample = msd.loop_start_sample;
        xwb.loop_end_sample   = msd.loop_end_sample;

        /* if provided, xwb.num_samples is equal to msd.num_samples after proper adjustments (+ 128 - start_skip - end_skip) */
        xwb.fix_xma_loop_samples = 1;
        xwb.fix_xma_num_samples = 0;

        /* Techland's XMA in tool_version 0x2a (not 0x2c?) seems to use (entry_info >> 1) num_samples
         * for music banks, but not sfx [Nail'd (X360)-0x2a, Dead Island (X360)-0x2c] */
        if (xwb.version == XACT_TECHLAND) {
            xwb.num_samples = 0;
        }

        /* for XWB v22 (and below?) this seems normal [Project Gotham Racing (X360)] */
        if (xwb.num_samples == 0) {
            xwb.num_samples = msd.num_samples;
            xwb.fix_xma_num_samples = 1;
        }
    }
    else if ((xwb.codec == XMA1 || xwb.codec == XMA2) &&  xwb.loop_flag) {
        /* unlike prev versions, xwb.num_samples is the full size without adjustments */
        xwb.fix_xma_loop_samples = 1;
        xwb.fix_xma_num_samples = 1;

        /* Crackdown does use xwb.num_samples after adjustments (but not loops) */
        if (xwb.is_crackdown) {
            xwb.fix_xma_num_samples = 0;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(xwb.channels,xwb.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = xwb.sample_rate;
    vgmstream->num_samples = xwb.num_samples;
    vgmstream->loop_start_sample = xwb.loop_start_sample;
    vgmstream->loop_end_sample   = xwb.loop_end_sample;
    vgmstream->num_streams = xwb.total_subsongs;
    vgmstream->stream_size = xwb.stream_size;
    vgmstream->meta_type = meta_XWB;

    get_name(stream_name, STREAM_NAME_SIZE, target_subsong, &xwb, sf, sf_xsb);

    if (stream_name[0]) {
        get_streamfile_basename(sf, file_name, STREAM_NAME_SIZE);
        if (xwb.wavebank_name[0] && strcmp(file_name, xwb.wavebank_name) != 0)
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s/%s", xwb.wavebank_name, stream_name);
        else
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s", stream_name);
    }

    switch(xwb.codec) {
        case PCM: /* Unreal Championship (Xbox)[PCM8], KOF2003 (Xbox)[PCM16LE], Otomedius (X360)[PCM16BE] */
            vgmstream->coding_type = xwb.bits_per_sample == 0 ? coding_PCM8_U :
                    (xwb.little_endian ? coding_PCM16LE : coding_PCM16BE);
            vgmstream->layout_type = xwb.channels > 1 ? layout_interleave : layout_none;
            vgmstream->interleave_block_size = xwb.bits_per_sample == 0 ? 0x01 : 0x02;
            break;

        case XBOX_ADPCM: /* Silent Hill 4 (Xbox) */
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            break;

        case MS_ADPCM: /* Persona 4 Ultimax (AC) */
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = (xwb.block_align + 22) * xwb.channels; /*22=CONVERSION_OFFSET (?)*/
            break;

#ifdef VGM_USE_FFMPEG
        case XMA1: { /* Kameo (X360), Table Tennis (X360) */
            vgmstream->codec_data = init_ffmpeg_xma1_raw(sf, xwb.stream_offset, xwb.stream_size, vgmstream->channels, vgmstream->sample_rate, 0);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sf, xwb.stream_offset,xwb.stream_size, 0, xwb.fix_xma_num_samples,xwb.fix_xma_loop_samples);

            /* this fixes some XMA1, perhaps the above isn't reading end_skip correctly (doesn't happen for all files though) */
            if (vgmstream->loop_flag &&
                    vgmstream->loop_end_sample > vgmstream->num_samples) {
                VGM_LOG("XWB: fix XMA1 looping\n");
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }
            break;
        }

        case XMA2: { /* Blue Dragon (X360) */
            int block_size = 0x10000; /* XACT default */

            vgmstream->codec_data = init_ffmpeg_xma2_raw(sf, xwb.stream_offset, xwb.stream_size, vgmstream->num_samples, vgmstream->channels, vgmstream->sample_rate, block_size, 0);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sf, xwb.stream_offset,xwb.stream_size, 0, xwb.fix_xma_num_samples,xwb.fix_xma_loop_samples);
            break;
        }

        case WMA: { /* WMAudio1 (WMA v2): Prince of Persia 2 port (Xbox) */
            ffmpeg_codec_data *ffmpeg_data = NULL;

            ffmpeg_data = init_ffmpeg_offset(sf, xwb.stream_offset,xwb.stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* no wma_bytes_to_samples, this should be ok */
            if (!vgmstream->num_samples)
                vgmstream->num_samples = ffmpeg_get_samples(ffmpeg_data);
            break;
        }

        case XWMA: { /* WMAudio2 (WMA v2): BlazBlue (X360), WMAudio3 (WMA Pro): Bullet Witch (PC) voices */
            int bps_index, block_align, block_index, avg_bps, wma_codec;

            bps_index = (xwb.block_align >> 5);  /* upper 3b bytes-per-second index (docs say 2b+6b but are wrong) */
            block_index =  (xwb.block_align) & 0x1F; /*lower 5b block alignment index */
            if (bps_index >= 7) goto fail;
            if (block_index >= 17) goto fail;

            avg_bps = wma_avg_bps_index[bps_index];
            block_align = wma_block_align_index[block_index];
            wma_codec = xwb.bits_per_sample ? 0x162 : 0x161; /* 0=WMAudio2, 1=WMAudio3 */

            vgmstream->codec_data = init_ffmpeg_xwma(sf, xwb.stream_offset, xwb.stream_size, wma_codec, vgmstream->channels, vgmstream->sample_rate, avg_bps, block_align);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case ATRAC3: { /* Techland PS3 extension [Sniper Ghost Warrior (PS3)] */
            int block_align, encoder_delay;

            block_align = xwb.block_align * vgmstream->channels;
            encoder_delay = 1024; /* assumed */
            vgmstream->num_samples -= encoder_delay;

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(sf, xwb.stream_offset,xwb.stream_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
#ifdef VGM_USE_VORBIS
        case OGG: { /* Oddworld: Strangers Wrath (iOS/Android) extension */
            vgmstream->codec_data = init_ogg_vorbis(sf, xwb.stream_offset, xwb.stream_size, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_OGG_VORBIS;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        case DSP: { /* Stardew Valley (Switch) extension */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = xwb.stream_size / xwb.channels;

            dsp_read_coefs(vgmstream,sf,xwb.stream_offset + 0x1c,vgmstream->interleave_block_size,!xwb.little_endian);
            dsp_read_hist (vgmstream,sf,xwb.stream_offset + 0x3c,vgmstream->interleave_block_size,!xwb.little_endian);
            xwb.stream_offset += 0x60; /* skip DSP header */
            break;
        }

        case ATRAC9_RIFF: { /* Stardew Valley (Vita) extension */
            VGMSTREAM *temp_vgmstream = NULL;
            STREAMFILE* temp_sf = NULL;

            /* standard RIFF, use subfile (seems doesn't use xwb loops) */
            VGM_ASSERT(xwb.loop_flag, "XWB: RIFF ATRAC9 loop flag found\n");

            temp_sf = setup_subfile_streamfile(sf, xwb.stream_offset,xwb.stream_size, "at9");
            if (!temp_sf) goto fail;

            temp_vgmstream = init_vgmstream_riff(temp_sf);
            close_streamfile(temp_sf);
            if (!temp_vgmstream) goto fail;

            temp_vgmstream->num_streams = vgmstream->num_streams;
            temp_vgmstream->stream_size = vgmstream->stream_size;
            temp_vgmstream->meta_type = vgmstream->meta_type;
            strcpy(temp_vgmstream->stream_name, vgmstream->stream_name);

            close_vgmstream(vgmstream);
            return temp_vgmstream;
        }

        default:
            goto fail;
    }


    start_offset = xwb.stream_offset;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ****************************************************************************** */

static bool get_xwb_name(char* buf, size_t buf_size, int target_subsong, xwb_header* xwb, STREAMFILE* sf) {
    size_t read;

    if (!xwb->names_offset || !xwb->names_size || xwb->names_entry_size > buf_size)
        return false;

    read = read_string(buf,xwb->names_entry_size, xwb->names_offset + xwb->names_entry_size*(target_subsong-1),sf);
    if (read == 0) return false;

    return true;
}

static bool get_xsb_name(char* buf, size_t buf_size, int target_subsong, xwb_header* xwb, STREAMFILE* sf) {
    xsb_header xsb = {0};

    xsb.selected_stream = target_subsong - 1;
    if (!parse_xsb(&xsb, sf, xwb->wavebank_name))
        return false;

    if ((xwb->version <= XACT1_1_MAX && xsb.version > XSB_XACT1_2_MAX) ||
        (xwb->version <= XACT2_2_MAX && xsb.version > XSB_XACT2_2_MAX)) {
        VGM_LOG("XSB: mismatched XACT versions: xsb v%i vs xwb v%i\n", xsb.version, xwb->version);
        return false;
    }

    //;VGM_LOG("XSB: name found=%i at %lx\n", xsb.parse_found, xsb.name_offset);
    if (!xsb.name_len || xsb.name[0] == '\0')
        return false;

    snprintf(buf, buf_size, "%s", xsb.name);
    return true;
}

static bool get_wbh_name(char* buf, size_t maxsize, int target_subsong, xwb_header* xwb, STREAMFILE* sf) {
    int selected_stream = target_subsong - 1;

    if (!is_id32be(0x00, sf, "WBHD"))
        return false;
    int version     = read_u32le(0x04, sf);
    if (version != 1)
        return false;
    int name_count  = read_u32le(0x08, sf);

    if (selected_stream > name_count)
        return false;

    /* next table:
     * - 0x00: wave id? (ordered from 0 to N)
     * - 0x04: always 0 */
    off_t offset = 0x10 + 0x08 * name_count;

    int name_number = 0;
    while (offset < get_streamfile_size(sf)) {
        size_t name_len = read_string(buf, maxsize, offset, sf) + 1;

        if (name_len == 0)
            return false;
        if (name_number == selected_stream)
            break;

        name_number++;
        offset += name_len;
    }

    return true;
}

static void get_name(char* buf, size_t buf_size, int target_subsong, xwb_header* xwb, STREAMFILE* sf_xwb, STREAMFILE* sf_xsb) {
    STREAMFILE* sf_name = NULL;

    buf[0] = '\0';

    /* try to get the stream name in the .xwb, though they are very rarely included */
    bool name_found = get_xwb_name(buf, buf_size, target_subsong, xwb, sf_xwb);
    if (name_found) return;

    /* external companion file */
    if (sf_xsb) {
        get_xsb_name(buf, buf_size, target_subsong, xwb, sf_xsb);
        return;
    }

    /* open companion files */
    if (xwb->version == 1) {
        /* .wbh, a simple name container */
        sf_name = open_streamfile_by_ext(sf_xwb, "wbh");
        if (!sf_name) return; /* rarely found [Pac-Man World 2 (Xbox)] */

        get_wbh_name(buf, buf_size, target_subsong, xwb, sf_name);
        close_streamfile(sf_name);
    }
    else {
        /* .xsb, a comically complex cue format */
        sf_name = open_xsb_filename_pair(sf_xwb);
        if (!sf_name) return; /* not all xwb have xsb though */

        get_xsb_name(buf, buf_size, target_subsong, xwb, sf_name);
        close_streamfile(sf_name);
    }
}
