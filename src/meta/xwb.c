#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"
#include <string.h>

/* most info from XWBtool, xactwb.h, xact2wb.h and xact3wb.h */

#define WAVEBANK_FLAGS_COMPACT              0x00020000  // Bank uses compact format
#define WAVEBANKENTRY_FLAGS_IGNORELOOP      0x00000008  // Used internally when the loop region can't be used (no idea...)

/* the x.x version is just to make it clearer, MS only classifies XACT as 1/2/3 */
#define XACT1_0_MAX     1           /* Project Gotham Racing 2 (v1), Silent Hill 4 (v1) */
#define XACT1_1_MAX     3           /* Unreal Championship (v2), The King of Fighters 2003 (v3) */
#define XACT2_0_MAX     34          /* Dead or Alive 4 (v17), Kameo (v23), Table Tennis (v34) */ // v35/36/37 too?
#define XACT2_1_MAX     38          /* Prey (v38) */ // v39 too?
#define XACT2_2_MAX     41          /* Blue Dragon (v40) */
#define XACT3_0_MAX     46          /* Ninja Blade (t43 v42), Persona 4 Ultimax NESSICA (t45 v43) */
#define XACT_TECHLAND   0x10000     /* Sniper Ghost Warrior, Nail'd (PS3/X360), equivalent to XACT3_0 */
#define XACT_CRACKDOWN  0x87        /* Crackdown 1, equivalent to XACT2_2 */

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
    off_t base_offset;
    size_t base_size;
    off_t entry_offset;
    size_t entry_size;
    off_t names_offset;
    size_t names_size;
    size_t names_entry_size;
    off_t extra_offset;
    size_t extra_size;
    off_t data_offset;
    size_t data_size;

    off_t stream_offset;
    size_t stream_size;

    uint32_t base_flags;
    size_t entry_elem_size;
    size_t entry_alignment;
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

    int is_crackdown;
    int fix_xma_num_samples;
    int fix_xma_loop_samples;
} xwb_header;

static void get_name(char * buf, size_t maxsize, int target_subsong, xwb_header * xwb, STREAMFILE *streamFile);


/* XWB - XACT Wave Bank (Microsoft SDK format for XBOX/XBOX360/Windows) */
VGMSTREAM * init_vgmstream_xwb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, off, suboff;
    xwb_header xwb = {0};
    int target_subsong = streamFile->stream_index;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    /* .xwb: standard
     * .xna: Touhou Makukasai ~ Fantasy Danmaku Festival (PC)
     * (extensionless): Grabbed by the Ghoulies (Xbox) */
    if (!check_extensions(streamFile,"xwb,xna,"))
        goto fail;
    if ((read_32bitBE(0x00,streamFile) != 0x57424E44) &&    /* "WBND" (LE) */
        (read_32bitBE(0x00,streamFile) != 0x444E4257))      /* "DNBW" (BE) */
        goto fail;

    xwb.little_endian = read_32bitBE(0x00,streamFile) == 0x57424E44; /* WBND */
    if (xwb.little_endian) {
        read_32bit = read_32bitLE;
    } else {
        read_32bit = read_32bitBE;
    }


    /* read main header (WAVEBANKHEADER) */
    xwb.version = read_32bit(0x04, streamFile); /* XACT3: 0x04=tool version, 0x08=header version */

    /* Crackdown 1 (X360), essentially XACT2 but may have split header in some cases */
    if (xwb.version == XACT_CRACKDOWN) {
        xwb.version = XACT2_2_MAX;
        xwb.is_crackdown = 1;
    }

    /* read segment offsets (SEGIDX) */
    if (xwb.version <= XACT1_0_MAX) {
        xwb.total_subsongs = read_32bit(0x0c, streamFile);
        /* 0x10: bank name (size 0x10) */
        xwb.base_offset     = 0;
        xwb.base_size       = 0;
        xwb.entry_offset    = 0x50;
        xwb.entry_elem_size = 0x14;
        xwb.entry_size      = xwb.entry_elem_size * xwb.total_subsongs;
        xwb.data_offset     = xwb.entry_offset + xwb.entry_size;
        xwb.data_size       = get_streamfile_size(streamFile) - xwb.data_offset;

        xwb.names_offset    = 0;
        xwb.names_size      = 0;
        xwb.names_entry_size= 0;
        xwb.extra_offset    = 0;
        xwb.extra_size      = 0;
    }
    else {
        off = xwb.version <= XACT2_2_MAX ? 0x08 : 0x0c;
        xwb.base_offset = read_32bit(off+0x00, streamFile);//BANKDATA
        xwb.base_size   = read_32bit(off+0x04, streamFile);
        xwb.entry_offset= read_32bit(off+0x08, streamFile);//ENTRYMETADATA
        xwb.entry_size  = read_32bit(off+0x0c, streamFile);

        /* read extra segments (values can be 0 == no segment) */
        if (xwb.version <= XACT1_1_MAX) {
            xwb.names_offset    = read_32bit(off+0x10, streamFile);//ENTRYNAMES
            xwb.names_size      = read_32bit(off+0x14, streamFile);
            xwb.names_entry_size= 0x40;
            xwb.extra_offset    = 0;
            xwb.extra_size      = 0;
            suboff = 0x04*2;
        }
        else if (xwb.version <= XACT2_1_MAX) {
            xwb.names_offset    = read_32bit(off+0x10, streamFile);//ENTRYNAMES
            xwb.names_size      = read_32bit(off+0x14, streamFile);
            xwb.names_entry_size= 0x40;
            xwb.extra_offset    = read_32bit(off+0x18, streamFile);//EXTRA
            xwb.extra_size      = read_32bit(off+0x1c, streamFile);
            suboff = 0x04*2 + 0x04*2;
        } else {
            xwb.extra_offset    = read_32bit(off+0x10, streamFile);//SEEKTABLES
            xwb.extra_size      = read_32bit(off+0x14, streamFile);
            xwb.names_offset    = read_32bit(off+0x18, streamFile);//ENTRYNAMES
            xwb.names_size      = read_32bit(off+0x1c, streamFile);
            xwb.names_entry_size= 0x40;
            suboff = 0x04*2 + 0x04*2;
        }

        xwb.data_offset = read_32bit(off+0x10+suboff, streamFile);//ENTRYWAVEDATA
        xwb.data_size   = read_32bit(off+0x14+suboff, streamFile);

        /* for Techland's XWB with no data */
        if (xwb.base_offset == 0) goto fail;

        /* read base entry (WAVEBANKDATA) */
        off = xwb.base_offset;
        xwb.base_flags  = (uint32_t)read_32bit(off+0x00, streamFile);
        xwb.total_subsongs = read_32bit(off+0x04, streamFile);
        /* 0x08: bank name (size 0x40) */
        suboff = 0x08 + (xwb.version <= XACT1_1_MAX ? 0x10 : 0x40);
        xwb.entry_elem_size = read_32bit(off+suboff+0x00, streamFile);
        /* suboff+0x04: meta name entry size */
        xwb.entry_alignment = read_32bit(off+suboff+0x08, streamFile); /* usually 1 dvd sector */
        xwb.format = read_32bit(off+suboff+0x0c, streamFile); /* compact mode only */
        /* suboff+0x10: build time 64b (XACT2/3) */
    }

    if (target_subsong == 0) target_subsong = 1; /* auto: default to 1 */
    if (target_subsong < 0 || target_subsong > xwb.total_subsongs || xwb.total_subsongs < 1) goto fail;


    /* read stream entry (WAVEBANKENTRY) */
    off = xwb.entry_offset + (target_subsong-1) * xwb.entry_elem_size;

    if (xwb.base_flags & WAVEBANK_FLAGS_COMPACT) { /* compact entry [NFL Fever 2004 demo from Amped 2 (Xbox)] */
        uint32_t entry, size_deviation, sector_offset;
        off_t next_stream_offset;

        entry = (uint32_t)read_32bit(off+0x00, streamFile);
        size_deviation = ((entry >> 21) & 0x7FF); /* 11b, padding data for sector alignment in bytes*/
        sector_offset = (entry & 0x1FFFFF); /* 21b, offset within data in sectors */

        xwb.stream_offset  = xwb.data_offset + sector_offset*xwb.entry_alignment;

        /* find size using next offset */
        if (target_subsong < xwb.total_subsongs) {
            uint32_t next_entry = (uint32_t)read_32bit(off+0x04, streamFile);
            next_stream_offset = xwb.data_offset + (next_entry & 0x1FFFFF)*xwb.entry_alignment;
        }
        else { /* for last entry (or first, when subsongs = 1) */
            next_stream_offset = xwb.data_offset + xwb.data_size;
        }
        xwb.stream_size = next_stream_offset - xwb.stream_offset - size_deviation;
    }
    else if (xwb.version <= XACT1_0_MAX) {
        xwb.format          = (uint32_t)read_32bit(off+0x00, streamFile);
        xwb.stream_offset   = xwb.data_offset + (uint32_t)read_32bit(off+0x04, streamFile);
        xwb.stream_size     = (uint32_t)read_32bit(off+0x08, streamFile);

        xwb.loop_start      = (uint32_t)read_32bit(off+0x0c, streamFile);
        xwb.loop_end        = (uint32_t)read_32bit(off+0x10, streamFile);//length
    }
    else {
        uint32_t entry_info = (uint32_t)read_32bit(off+0x00, streamFile);
        if (xwb.version <= XACT1_1_MAX) {
            xwb.entry_flags = entry_info;
        } else {
            xwb.entry_flags = (entry_info) & 0xF; /*4b*/
            xwb.num_samples = (entry_info >> 4) & 0x0FFFFFFF; /*28b*/
        }
        xwb.format          = (uint32_t)read_32bit(off+0x04, streamFile);
        xwb.stream_offset   = xwb.data_offset + (uint32_t)read_32bit(off+0x08, streamFile);
        xwb.stream_size     = (uint32_t)read_32bit(off+0x0c, streamFile);

        if (xwb.version <= XACT2_1_MAX) { /* LoopRegion (bytes) */
            xwb.loop_start  = (uint32_t)read_32bit(off+0x10, streamFile);
            xwb.loop_end    = (uint32_t)read_32bit(off+0x14, streamFile);//length (LoopRegion) or offset (XMALoopRegion in late XACT2)
        } else { /* LoopRegion (samples) */
            xwb.loop_start_sample   = (uint32_t)read_32bit(off+0x10, streamFile);
            xwb.loop_end_sample     = (uint32_t)read_32bit(off+0x14, streamFile) + xwb.loop_start_sample;
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
            /* Table Tennis (v34): XMA1, Prey (v38): XMA2, v35/36/37: ? */
            case 1: xwb.codec = xwb.version <= XACT2_0_MAX ? XMA1 : XMA2; break;
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
            && xwb.bits_per_sample == 0x01 && xwb.block_align == 0x04
            && xwb.data_size == 0x55951c1c) { /* some kind of id? */
        /* Stardew Valley (Switch), full interleaved DSPs (including headers) */
        xwb.codec = DSP;
    }
    else if (xwb.version == XACT3_0_MAX && xwb.codec == XMA2
            && xwb.bits_per_sample == 0x01 && xwb.block_align == 0x04
            && xwb.data_size == 0x4e0a1000) { /* some kind of id? */
        /* Stardew Valley (Vita), standard RIFF with ATRAC9 */
        xwb.codec = ATRAC9_RIFF;
    }


    /* test loop after the above fixes */
    xwb.loop_flag = (xwb.loop_end > 0 || xwb.loop_end_sample > xwb.loop_start)
        && !(xwb.entry_flags & WAVEBANKENTRY_FLAGS_IGNORELOOP);

    /* Oddworld OGG the data_size value is size of uncompressed bytes instead;  DSP uses some id/config as value */
    if (xwb.codec != OGG && xwb.codec != DSP && xwb.codec != ATRAC9_RIFF) {
        /* some low-q rips don't remove padding, relax validation a bit */
        if (xwb.data_offset + xwb.data_size > get_streamfile_size(streamFile))
            goto fail;
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
    else if (xwb.version <= XACT2_1_MAX && (xwb.codec == XMA1 || xwb.codec == XMA2) && xwb.loop_flag) {
        /* v38: byte offset, v40+: sample offset, v39: ? */
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

        xma_get_samples(&msd, streamFile);
        xwb.loop_start_sample = msd.loop_start_sample;
        xwb.loop_end_sample   = msd.loop_end_sample;

        /* if provided, xwb.num_samples is equal to msd.num_samples after proper adjustments (+ 128 - start_skip - end_skip) */
        xwb.fix_xma_loop_samples = 1;
        xwb.fix_xma_num_samples = 0;

        /* for XWB v22 (and below?) this seems normal [Project Gotham Racing (X360)] */
        if (xwb.num_samples == 0) {
            xwb.num_samples   = msd.num_samples;
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
    get_name(vgmstream->stream_name,STREAM_NAME_SIZE, target_subsong, &xwb, streamFile);

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
            vgmstream->interleave_block_size = (xwb.block_align + 22) * xwb.channels; /*22=CONVERSION_OFFSET (?)*/
            break;

#ifdef VGM_USE_FFMPEG
        case XMA1: { /* Kameo (X360) */
            uint8_t buf[0x100];
            int bytes;

            bytes = ffmpeg_make_riff_xma1(buf,0x100, vgmstream->num_samples, xwb.stream_size, vgmstream->channels, vgmstream->sample_rate, 0);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, xwb.stream_offset,xwb.stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, streamFile, xwb.stream_offset,xwb.stream_size, 0, xwb.fix_xma_num_samples,xwb.fix_xma_loop_samples);

            /* this fixes some XMA1, perhaps the above isn't reading end_skip correctly (doesn't happen for all files though) */
            if (vgmstream->loop_flag &&
                    vgmstream->loop_end_sample > vgmstream->num_samples) {
                VGM_LOG("XWB: fix XMA1 looping\n");
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }
            break;
        }

        case XMA2: { /* Blue Dragon (X360) */
            uint8_t buf[0x100];
            int bytes, block_size, block_count;

            block_size = 0x10000; /* XACT default */
            block_count = xwb.stream_size / block_size + (xwb.stream_size % block_size ? 1 : 0);

            bytes = ffmpeg_make_riff_xma2(buf,0x100, vgmstream->num_samples, xwb.stream_size, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, xwb.stream_offset,xwb.stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, streamFile, xwb.stream_offset,xwb.stream_size, 0, xwb.fix_xma_num_samples,xwb.fix_xma_loop_samples);
            break;
        }

        case WMA: { /* WMAudio1 (WMA v2): Prince of Persia 2 port (Xbox) */
            ffmpeg_codec_data *ffmpeg_data = NULL;

            ffmpeg_data = init_ffmpeg_offset(streamFile, xwb.stream_offset,xwb.stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* no wma_bytes_to_samples, this should be ok */
            if (!vgmstream->num_samples)
                vgmstream->num_samples = (int32_t)ffmpeg_data->totalSamples;
            break;
        }

        case XWMA: { /* WMAudio2 (WMA v2): BlazBlue (X360), WMAudio3 (WMA Pro): Bullet Witch (PC) voices */
            uint8_t buf[0x100];
            int bytes, bps_index, block_align, block_index, avg_bps, wma_codec;

            bps_index = (xwb.block_align >> 5);  /* upper 3b bytes-per-second index (docs say 2b+6b but are wrong) */
            block_index =  (xwb.block_align) & 0x1F; /*lower 5b block alignment index */
            if (bps_index >= 7) goto fail;
            if (block_index >= 17) goto fail;

            avg_bps = wma_avg_bps_index[bps_index];
            block_align = wma_block_align_index[block_index];
            wma_codec = xwb.bits_per_sample ? 0x162 : 0x161; /* 0=WMAudio2, 1=WMAudio3 */

            bytes = ffmpeg_make_riff_xwma(buf,0x100, wma_codec, xwb.stream_size, vgmstream->channels, vgmstream->sample_rate, avg_bps, block_align);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, xwb.stream_offset,xwb.stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case ATRAC3: { /* Techland PS3 extension [Sniper Ghost Warrior (PS3)] */
            uint8_t buf[0x100];
            int bytes;

            int block_size = xwb.block_align * vgmstream->channels;
            int joint_stereo = xwb.block_align == 0x60; /* untested, ATRAC3 default */
            int skip_samples = 0; /* unknown */

            bytes = ffmpeg_make_riff_atrac3(buf,0x100, vgmstream->num_samples, xwb.stream_size, vgmstream->channels, vgmstream->sample_rate, block_size, joint_stereo, skip_samples);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, xwb.stream_offset,xwb.stream_size);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case OGG: { /* Oddworld: Strangers Wrath (iOS/Android) extension */
            vgmstream->codec_data = init_ffmpeg_offset(streamFile, xwb.stream_offset, xwb.stream_size);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        case DSP: { /* Stardew Valley (Switch) extension */
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = xwb.stream_size / xwb.channels;

            dsp_read_coefs(vgmstream,streamFile,xwb.stream_offset + 0x1c,vgmstream->interleave_block_size,!xwb.little_endian);
            dsp_read_hist (vgmstream,streamFile,xwb.stream_offset + 0x3c,vgmstream->interleave_block_size,!xwb.little_endian);
            xwb.stream_offset += 0x60; /* skip DSP header */
            break;
        }

#ifdef VGM_USE_ATRAC9
        case ATRAC9_RIFF: { /* Stardew Valley (Vita) extension */
            VGMSTREAM *temp_vgmstream = NULL;
            STREAMFILE *temp_streamFile = NULL;

            /* standard RIFF, use subfile (seems doesn't use xwb loops) */
            VGM_ASSERT(xwb.loop_flag, "XWB: RIFF ATRAC9 loop flag found\n");

            temp_streamFile = setup_subfile_streamfile(streamFile, xwb.stream_offset,xwb.stream_size, "at9");
            if (!temp_streamFile) goto fail;

            temp_vgmstream = init_vgmstream_riff(temp_streamFile);
            close_streamfile(temp_streamFile);
            if (!temp_vgmstream) goto fail;

            temp_vgmstream->num_streams = vgmstream->num_streams;
            temp_vgmstream->stream_size = vgmstream->stream_size;
            temp_vgmstream->meta_type = vgmstream->meta_type;

            close_vgmstream(vgmstream);
            return temp_vgmstream;
        }
#endif

        default:
            goto fail;
    }


    start_offset = xwb.stream_offset;

    if ( !vgmstream_open_stream(vgmstream,streamFile,start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ****************************************************************************** */

/* try to get the stream name in the .xwb, though they are very rarely included */
static int get_xwb_name(char * buf, size_t maxsize, int target_subsong, xwb_header * xwb, STREAMFILE *streamFile) {
    size_t read;

    if (!xwb->names_offset || !xwb->names_size || xwb->names_entry_size > maxsize)
        goto fail;

    read = read_string(buf,xwb->names_entry_size, xwb->names_offset + xwb->names_entry_size*(target_subsong-1),streamFile);
    if (read == 0) goto fail;

    return 1;

fail:
    return 0;
}

/* ****************************************************************************** */
/* XSB parsing from xwb_split (mostly untouched), could be improved */

#define XSB_XACT1_MAX   11
#define XSB_XACT2_MAX   41

/**
 * XWB contain stream info (channels, loop, data etc), often from multiple streams.
 * XSBs contain info about how to play sounds (volume, pitch, name, etc) from XWBs (music or SFX).
 * We only need to parse the XSB for the stream names.
 */
typedef struct {
    int sound_count;
} xsb_wavebank;

typedef struct {
    int stream_index; /* stream id in the xwb (doesn't need to match xsb sound order) */
    int wavebank; /* xwb id, if the xsb has multiple wavebanks */
    off_t name_index; /* name order */
    off_t name_offset; /* global offset to the name string */
    off_t sound_offset; /* global offset to the xsb sound */
    off_t unk_index; /* some kind of number up to sound_count or 0xffff */
} xsb_sound;

typedef struct {
    /* XSB header info */
    xsb_sound * xsb_sounds; /* array of sounds info from the xsb, simplified */
    xsb_wavebank * xsb_wavebanks; /* array of wavebank info from the xsb, simplified */

    off_t xsb_sounds_offset;
    size_t xsb_sounds_count;

    size_t xsb_simple_sounds_offset; /* sound cues */
    size_t xsb_simple_sounds_count;
    size_t xsb_complex_sounds_offset;
    size_t xsb_complex_sounds_count;

    size_t xsb_wavebanks_count;
    off_t xsb_nameoffsets_offset;
} xsb_header;


/* try to find the stream name in a companion XSB file, a comically complex cue format. */
static int get_xsb_name(char * buf, size_t maxsize, int target_subsong, xwb_header * xwb, STREAMFILE *streamXwb, char* filename) {
    STREAMFILE *streamFile = NULL;
    int i,j, start_sound, cfg__start_sound = 0, cfg__selected_wavebank = 0;
    int xsb_version;
    off_t off, suboff, name_offset = 0;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
    xsb_header xsb = {0};


    if (filename)
        streamFile = open_streamfile_by_filename(streamXwb, filename);
    else
        streamFile = open_streamfile_by_ext(streamXwb, "xsb");
    if (!streamFile) goto fail;

    /* check header */
    if ((read_32bitBE(0x00,streamFile) != 0x5344424B) &&    /* "SDBK" (LE) */
        (read_32bitBE(0x00,streamFile) != 0x4B424453))      /* "KBDS" (BE) */
        goto fail;

    if (read_32bitBE(0x00,streamFile) == 0x5344424B) { /* SDBK */
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    } else {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    }



    /* read main header (SoundBankHeader) */
    xsb_version = read_16bit(0x04, streamFile);
    if ((xwb->version <= XACT1_1_MAX && xsb_version > XSB_XACT1_MAX) || (xwb->version <= XACT2_2_MAX && xsb_version > XSB_XACT2_MAX)) {
        VGM_LOG("XSB: xsb and xwb are from different XACT versions (xsb v%i vs xwb v%i)\n", xsb_version, xwb->version);
        goto fail;
    }


    off = 0;
    if (xsb_version <= XSB_XACT1_MAX) {
        xsb.xsb_wavebanks_count = 1; //(uint8_t)read_8bit(0x22, streamFile);
        xsb.xsb_sounds_count = (uint16_t)read_16bit(0x1e, streamFile);//@ 0x1a? 0x1c?
        //xsb.xsb_names_size   = 0;
        //xsb.xsb_names_offset = 0;
        xsb.xsb_nameoffsets_offset = 0;
        xsb.xsb_sounds_offset = 0x38;
    } else if (xsb_version <= XSB_XACT2_MAX) {
        xsb.xsb_simple_sounds_count = (uint16_t)read_16bit(0x09, streamFile);
        xsb.xsb_complex_sounds_count = (uint16_t)read_16bit(0x0B, streamFile);
        xsb.xsb_wavebanks_count = (uint8_t)read_8bit(0x11, streamFile);
        xsb.xsb_sounds_count = (uint16_t)read_16bit(0x12, streamFile);
        //0x14: 16b unk
        //xsb.xsb_names_size   = read_32bit(0x16, streamFile);
        xsb.xsb_simple_sounds_offset = read_32bit(0x1a, streamFile);
        xsb.xsb_complex_sounds_offset = read_32bit(0x1e, streamFile); //todo 0x1e?
        //xsb.xsb_names_offset = read_32bit(0x22, streamFile);
        xsb.xsb_nameoffsets_offset = read_32bit(0x3a, streamFile);
        xsb.xsb_sounds_offset = read_32bit(0x3e, streamFile);
    } else {
        xsb.xsb_simple_sounds_count = (uint16_t)read_16bit(0x13, streamFile);
        xsb.xsb_complex_sounds_count = (uint16_t)read_16bit(0x15, streamFile);
        xsb.xsb_wavebanks_count = (uint8_t)read_8bit(0x1b, streamFile);
        xsb.xsb_sounds_count = read_16bit(0x1c, streamFile);
        //xsb.xsb_names_size   = read_32bit(0x1e, streamFile);
        xsb.xsb_simple_sounds_offset = read_32bit(0x22, streamFile);
        xsb.xsb_complex_sounds_offset = read_32bit(0x26, streamFile);
        //xsb.xsb_names_offset = read_32bit(0x2a, streamFile);
        xsb.xsb_nameoffsets_offset = read_32bit(0x42, streamFile);
        xsb.xsb_sounds_offset = read_32bit(0x46, streamFile);
    }

    VGM_ASSERT(xsb.xsb_sounds_count < xwb->total_subsongs,
               "XSB: number of streams in xsb lower than xwb (xsb %i vs xwb %i)\n", xsb.xsb_sounds_count, xwb->total_subsongs);

    VGM_ASSERT(xsb.xsb_simple_sounds_count + xsb.xsb_complex_sounds_count != xsb.xsb_sounds_count,
               "XSB: number of xsb sounds doesn't match simple + complex sounds (simple %i, complex %i, total %i)\n", xsb.xsb_simple_sounds_count, xsb.xsb_complex_sounds_count, xsb.xsb_sounds_count);


    /* init stuff */
    xsb.xsb_sounds = calloc(xsb.xsb_sounds_count, sizeof(xsb_sound));
    if (!xsb.xsb_sounds) goto fail;

    xsb.xsb_wavebanks = calloc(xsb.xsb_wavebanks_count, sizeof(xsb_wavebank));
    if (!xsb.xsb_wavebanks) goto fail;

    /* The following is a bizarre soup of flags, tables, offsets to offsets and stuff, just to get the actual name.
     * info: https://wiki.multimedia.cx/index.php/XACT */

    /* parse xsb sounds */
    off = xsb.xsb_sounds_offset;
    for (i = 0; i < xsb.xsb_sounds_count; i++) {
        xsb_sound *s = &(xsb.xsb_sounds[i]);
        uint32_t flag;
        size_t size;

        if (xsb_version <= XSB_XACT1_MAX) {
            /* The format seems constant */
            flag = read_8bit(off+0x00, streamFile);
            size = 0x14;

            if (flag != 0x01) {
                //VGM_LOG("XSB: xsb flag 0x%x at offset 0x%08lx not implemented\n", flag, off);
                goto fail;
            }

            s->wavebank     = 0; //(uint8_t)read_8bit(off+suboff + 0x02, streamFile);
            s->stream_index = (uint16_t)read_16bit(off+0x02, streamFile);
            s->sound_offset = off;
            s->name_offset  = (uint16_t)read_16bit(off+0x04, streamFile);
        }
        else {
            /* Each XSB sound has a variable size and somewhere inside is the stream/wavebank index.
             * Various flags control the sound layout, but I can't make sense of them so quick hack instead */
            flag = read_8bit(off+0x00, streamFile);
            //0x01 16b unk, 0x03: 8b unk 04: 16b unk, 06: 8b unk
            size = (uint16_t)read_16bit(off+0x07, streamFile);

            if (!(flag & 0x01)) { /* simple sound */
                suboff = 0x09;
            } else { /* complex sound */
                /* not very exact but seems to work */
                if (flag==0x01 || flag==0x03 || flag==0x05 || flag==0x07) {
                    if (size == 0x49) { //grotesque hack for Eschatos (these flags are way too complex)
                        suboff = 0x23;
                    } else if (size % 2 == 1 && (uint16_t)read_16bit(off+size-0x2, streamFile)!=0) {
                        suboff = size - 0x08 - 0x07; //7 unk bytes at the end
                    } else {
                        suboff = size - 0x08;
                    }
                //} else if (flag==0x11) { /* Stardew Valley (Switch) */
                //    suboff = size; //???
                } else {
                    //VGM_LOG("XSB: xsb flag 0x%x (size=%x) at offset 0x%08lx not implemented\n", flag, size, off);
                    goto fail;
                }
            }

            s->stream_index = (uint16_t)read_16bit(off+suboff + 0x00, streamFile);
            s->wavebank     =   (uint8_t)read_8bit(off+suboff + 0x02, streamFile);
            s->sound_offset = off;
        }

        if (s->wavebank+1 > xsb.xsb_wavebanks_count) {
            //VGM_LOG("XSB: unknown xsb wavebank id %i at offset 0x%lx\n", s->wavebank, off);
            goto fail;
        }

        xsb.xsb_wavebanks[s->wavebank].sound_count += 1;
        off += size;
    }


    /* parse name offsets */
    if (xsb_version > XSB_XACT1_MAX) {
        /* "cue" name order: first simple sounds, then complex sounds
         * Both aren't ordered like the sound entries, instead use a global offset to the entry
         *
         * ex. of a possible XSB:
         *   name 1 = simple  sound 1 > sound entry 2 (points to xwb stream 4): stream 4 uses name 1
         *   name 2 = simple  sound 2 > sound entry 1 (points to xwb stream 1): stream 1 uses name 2
         *   name 3 = complex sound 1 > sound entry 3 (points to xwb stream 3): stream 3 uses name 3
         *   name 4 = complex sound 2 > sound entry 4 (points to xwb stream 2): stream 2 uses name 4
         *
         * Multiple cues can point to the same sound entry but we only use the first name (meaning some won't be used) */
        off_t n_off = xsb.xsb_nameoffsets_offset;

        off = xsb.xsb_simple_sounds_offset;
        for (i = 0; i < xsb.xsb_simple_sounds_count; i++) {
            off_t sound_offset = read_32bit(off + 0x01, streamFile);
            off += 0x05;

            /* find sound by offset */
            for (j = 0; j < xsb.xsb_sounds_count; j++) {
                xsb_sound *s = &(xsb.xsb_sounds[j]);;
                /* update with the current name offset */
                if (!s->name_offset && sound_offset == s->sound_offset) {
                    s->name_offset = read_32bit(n_off + 0x00, streamFile);
                    s->unk_index  = read_16bit(n_off + 0x04, streamFile);
                    n_off += 0x06;
                    break;
                }
            }
        }

        off = xsb.xsb_complex_sounds_offset;
        for (i = 0; i < xsb.xsb_complex_sounds_count; i++) {
            off_t sound_offset = read_32bit(off + 0x01, streamFile);
            off += 0x0f;

            /* find sound by offset */
            for (j = 0; j < xsb.xsb_sounds_count; j++) {
                xsb_sound *s = &(xsb.xsb_sounds[j]);;
                /* update with the current name offset */
                if (!s->name_offset && sound_offset == s->sound_offset) {
                    s->name_offset = read_32bit(n_off + 0x00, streamFile);
                    s->unk_index  = read_16bit(n_off + 0x04, streamFile);
                    n_off += 0x06;
                    break;
                }
            }
        }
    }

    // todo: it's possible to find the wavebank using the name
    /* try to find correct wavebank, in cases of multiple */
    if (!cfg__selected_wavebank) {
        for (i = 0; i < xsb.xsb_wavebanks_count; i++) {
            xsb_wavebank *w = &(xsb.xsb_wavebanks[i]);

            //CHECK_EXIT(w->sound_count == 0, "ERROR: xsb wavebank %i has no sounds", i); //Ikaruga PC

            if (w->sound_count == xwb->total_subsongs) {
                if (!cfg__selected_wavebank) {
                    //VGM_LOG("XSB: multiple xsb wavebanks with the same number of sounds, use -w to specify one of the wavebanks\n");
                    goto fail;
                }

                cfg__selected_wavebank = i+1;
            }
        }
    }

    /* banks with different number of sounds but only one wavebank, just select the first */
    if (!cfg__selected_wavebank && xsb.xsb_wavebanks_count==1) {
        cfg__selected_wavebank = 1;
    }

    if (!cfg__selected_wavebank) {
        //VGM_LOG("XSB: multiple xsb wavebanks but autodetect didn't work\n");
        goto fail;
    }
    if (xsb.xsb_wavebanks[cfg__selected_wavebank-1].sound_count == 0) {
        //VGM_LOG("XSB: xsb selected wavebank %i has no sounds\n", cfg__selected_wavebank);
        goto fail;
    }

    if (cfg__start_sound) {
        if (xsb.xsb_wavebanks[cfg__selected_wavebank-1].sound_count - (cfg__start_sound-1) < xwb->total_subsongs) {
            //VGM_LOG("XSB: starting sound too high (max in selected wavebank is %i)\n", xsb.xsb_wavebanks[cfg__selected_wavebank-1].sound_count - xwb->total_subsongs + 1);
            goto fail;
        }

    } else {
        /*
        if (!cfg->ignore_names_not_found)
            CHECK_EXIT(xwb->xsb_wavebanks[cfg->selected_wavebank-1].sound_count > xwb->streams_count, "ERROR: number of streams in xsb wavebank bigger than xwb (xsb %i vs xwb %i), use -s to specify (1=first)", xwb->xsb_wavebanks[cfg->selected_wavebank-1].sound_count, xwb->total_subsongs);
        if (!cfg->ignore_names_not_found)
            CHECK_EXIT(xwb->xsb_wavebanks[cfg->selected_wavebank-1].sound_count < xwb->streams_count, "ERROR: number of streams in xsb wavebank lower than xwb (xsb %i vs xwb %i), use -n to ignore (some names won't be extracted)", xwb->xsb_wavebanks[cfg->selected_wavebank-1].sound_count, xwb->total_subsongs);
        */


        //if (!cfg->ignore_names_not_found)
        //    CHECK_EXIT(xwb->xsb_wavebanks[cfg->selected_wavebank-1].sound_count != xwb->streams_count, "ERROR: number of streams in xsb wavebank different than xwb (xsb %i vs xwb %i), use -s to specify (1=first)", xwb->xsb_wavebanks[cfg->selected_wavebank-1].sound_count, xwb->total_subsongs);
    }

    /* *************************** */

    start_sound = cfg__start_sound ? cfg__start_sound-1 : 0;

    /* get name offset */
    for (i = start_sound; i < xsb.xsb_sounds_count; i++) {
        xsb_sound *s = &(xsb.xsb_sounds[i]);
        if (s->wavebank == cfg__selected_wavebank-1
                && s->stream_index == target_subsong-1){
            name_offset = s->name_offset;
            break;
        }
    }

    if (name_offset)
        read_string(buf,maxsize, name_offset,streamFile);

    //return; /* no return, let free */

fail:
    free(xsb.xsb_sounds);
    free(xsb.xsb_wavebanks);
    close_streamfile(streamFile);

    return (name_offset);
}

static void get_name(char * buf, size_t maxsize, int target_subsong, xwb_header * xwb, STREAMFILE *streamFile) {
    int name_found;
    char xwb_filename[PATH_LIMIT];
    char xsb_filename[PATH_LIMIT];

    /* try inside this xwb */
    name_found = get_xwb_name(buf, maxsize, target_subsong, xwb, streamFile);
    if (name_found) return;


    /* try again in external .xsb, using a bunch of possible name pairs */
    get_streamfile_filename(streamFile,xwb_filename,PATH_LIMIT);

    if (strcmp(xwb_filename,"Wave Bank.xwb")==0) {
        strcpy(xsb_filename,"Sound Bank.xsb");
    }
    else if (strcmp(xwb_filename,"UIMusicBank.xwb")==0) {
        strcpy(xsb_filename,"UISoundBank.xsb");
    }
    else {
        xsb_filename[0] = '\0';
    }
    //todo try others: InGameMusic.xwb + ingamemusic.xsb, NB_BGM_m0100_WB.xwb + NB_BGM_m0100_SB.xsb, etc

    if (xsb_filename[0] != '\0') {
        name_found = get_xsb_name(buf, maxsize, target_subsong, xwb, streamFile, xsb_filename);
        if (name_found) return;
    }


    /* one last time with same name */
    get_xsb_name(buf, maxsize, target_subsong, xwb, streamFile, NULL);
}
