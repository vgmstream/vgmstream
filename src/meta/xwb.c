#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* most info from XWBtool, xactwb.h, xact2wb.h and xact3wb.h */

#define WAVEBANK_FLAGS_COMPACT              0x00020000  // Bank uses compact format
#define WAVEBANKENTRY_FLAGS_IGNORELOOP      0x00000008  // Used internally when the loop region can't be used

static const int32_t wma_avg_bps_index[] = { /*7*/
    12000, 24000, 4000, 6000, 8000, 20000, 2500
};
static const int32_t wma_block_align_index[] = { /*17*/
    929, 1487, 1280, 2230, 8917, 8192, 4459, 5945, 2304, 1536, 1485, 1008, 2731, 4096, 6827, 5462, 1280
};


typedef enum { PCM, XBOX_ADPCM, MS_ADPCM, XMA1, XMA2, WMA, XWMA, ATRAC3 } xact_codec;
typedef struct {
    int little_endian;
    int xact; /* rough XACT version (1/2/3) */

    int version;

    /* segments */
    off_t base_offset;
    size_t base_size;
    off_t entry_offset;
    size_t entry_size;
    off_t data_offset;
    size_t data_size;

    off_t stream_offset;
    size_t stream_size;

    uint32_t base_flags;
    size_t entry_elem_size;
    size_t entry_alignment;
    int streams;

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
} xwb_header;


/* XWB - XACT Wave Bank (Microsoft SDK format for XBOX/XBOX360/Windows) */
VGMSTREAM * init_vgmstream_xwb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, off, suboff;
    xwb_header xwb;
    int target_stream = 0;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;


    /* basic checks */
    if (!check_extensions(streamFile,"xwb")) goto fail;

    if ((read_32bitBE(0x00,streamFile) != 0x57424E44) &&    /* "WBND" (LE) */
        (read_32bitBE(0x00,streamFile) != 0x444E4257))      /* "DNBW" (BE) */
        goto fail;

    memset(&xwb,0,sizeof(xwb_header));

    xwb.little_endian = read_32bitBE(0x00,streamFile) == 0x57424E44;/* WBND */
    if (xwb.little_endian) {
        read_32bit = read_32bitLE;
    } else {
        read_32bit = read_32bitBE;
    }


    /* read main header (WAVEBANKHEADER) */
    xwb.version = read_32bit(0x04, streamFile);
    if (xwb.version <= 3) {
        xwb.xact = 1; /* XACT1: XBOX [The King of Fighters 2003] */
    } else if (xwb.version < 42) {
        xwb.xact = 2; /* XACT2: early XBOX360 [Kameo, Table Tennis, Blue Dragon], Windows */
    } else { /* highest seen: tool=v46, header=v44 */
        xwb.xact = 3; /* XACT3: late XBOX360, Windows */
    }

    /* read segment offsets (SEGIDX) */
    off = xwb.xact <= 2 ? 0x08 : 0x0c; /* XACT3: 0x04=tool version, 0x08=header version */
    xwb.base_offset = read_32bit(off+0x00, streamFile);//BANKDATA
    xwb.base_size   = read_32bit(off+0x04, streamFile);
    xwb.entry_offset= read_32bit(off+0x08, streamFile);//ENTRYMETADATA
    xwb.entry_size  = read_32bit(off+0x0c, streamFile);
    /* go to last segment (XACT2/3 have 5 segments, XACT1 4) */
    //0x10: XACT1/2: ENTRYNAMES,  XACT3: SEEKTABLES
    //0x14: XACT1: none (ENTRYWAVEDATA), XACT2: EXTRA, XACT3: ENTRYNAMES
    suboff = xwb.xact >= 2 ? 0x08+0x08 : 0x08;
    xwb.data_offset = read_32bit(off+0x10+suboff, streamFile);//ENTRYWAVEDATA
    xwb.data_size   = read_32bit(off+0x14+suboff, streamFile);

    /* for Silent Hill 4 Xbox fake XWB and Techland's XWB with no data */
    if (xwb.base_offset == 0) goto fail;
    if (xwb.data_offset + xwb.data_size != get_streamfile_size(streamFile)) goto fail;

    /* read base entry (WAVEBANKDATA) */
    off = xwb.base_offset;
    xwb.base_flags = (uint32_t)read_32bit(off+0x00, streamFile);
    xwb.streams = read_32bit(off+0x04, streamFile);
    /* 0x08 bank_name */
    suboff = 0x08 + (xwb.xact == 1 ? 0x10 : 0x40);
    xwb.entry_elem_size = read_32bit(off+suboff+0x00, streamFile);
    /* suboff+0x04: meta name entry size */
    xwb.entry_alignment = read_32bit(off+suboff+0x08, streamFile); /* usually 1 dvd sector */
    xwb.format = read_32bit(off+suboff+0x0c, streamFile); /* compact mode only */
    /* suboff+0x10: build time 64b (XACT2/3) */

    if (target_stream == 0) target_stream = 1; /* auto: default to 1 */
    if (target_stream < 0 || target_stream > xwb.streams || xwb.streams < 1) goto fail;


    /* read stream entry (WAVEBANKENTRY) */
    off = xwb.entry_offset + (target_stream-1) * xwb.entry_elem_size;

    if (xwb.base_flags & WAVEBANK_FLAGS_COMPACT) { /* compact entry */
        /* offset_in_sectors:21 and sector_alignment_in_bytes:11 */
        uint32_t entry      = (uint32_t)read_32bit(off+0x00, streamFile);
        xwb.stream_offset   = xwb.data_offset + (entry >> 11) * xwb.entry_alignment + (entry & 0x7FF);

        /* find size (up to next entry or data end) */
        if (xwb.streams > 1) {
            entry = (uint32_t)read_32bit(off+xwb.entry_size, streamFile);
            xwb.stream_size = xwb.stream_offset -
                    (xwb.data_offset + (entry >> 11) * xwb.entry_alignment + (entry & 0x7FF));
        } else {
            xwb.stream_size = xwb.data_size;
        }
    }
    else {
        uint32_t entry_info = (uint32_t)read_32bit(off+0x00, streamFile);
        if (xwb.xact == 1) {
            xwb.entry_flags = entry_info;
        } else {
            xwb.entry_flags = (entry_info) & 0xF; /*4*/
            xwb.num_samples = (entry_info >> 4) & 0x0FFFFFFF; /*28*/
        }
        xwb.format          = (uint32_t)read_32bit(off+0x04, streamFile);
        xwb.stream_offset   = xwb.data_offset + (uint32_t)read_32bit(off+0x08, streamFile);
        xwb.stream_size     = (uint32_t)read_32bit(off+0x0c, streamFile);

        if (xwb.xact == 1) { //LoopRegion (bytes within data)
            xwb.loop_start  = (uint32_t)read_32bit(off+0x10, streamFile);
            xwb.loop_end    = (uint32_t)read_32bit(off+0x14, streamFile);//length
        } else if (xwb.xact == 2 && xwb.version <= 38) {//LoopRegion (bytes within data) or XMALoopRegion (bits within data)
            xwb.loop_start  = (uint32_t)read_32bit(off+0x10, streamFile);
            xwb.loop_end    = (uint32_t)read_32bit(off+0x14, streamFile);//length (LoopRegion) or offset (XMALoopRegion)
        } else {//LoopRegion (samples)
            xwb.loop_start_sample   = (uint32_t)read_32bit(off+0x10, streamFile);
            xwb.loop_end_sample     = xwb.loop_start_sample + (uint32_t)read_32bit(off+0x14, streamFile);
        }

        xwb.loop_flag = (xwb.loop_end > 0 || xwb.loop_end_sample > xwb.loop_start)
            && !(xwb.entry_flags & WAVEBANKENTRY_FLAGS_IGNORELOOP);
    }


    /* parse format */
    if (xwb.xact == 1) {
        xwb.bits_per_sample = (xwb.format >> 31) & 0x1; /*1*/
        xwb.sample_rate     = (xwb.format >> 5) & 0x3FFFFFF; /*26*/
        xwb.channels        = (xwb.format >> 2) & 0x7; /*3*/
        xwb.tag             = (xwb.format) & 0x3; /*2*/
    }
    else if (xwb.version <= 34) { /* early XACT2, not sure if includes v35/36 */
        xwb.bits_per_sample = (xwb.format >> 31) & 0x1; /*1*/
        xwb.block_align     = (xwb.format >> 24) & 0xFF; /*8*/
        xwb.sample_rate     = (xwb.format >> 4) & 0x7FFFF; /*19*/
        xwb.channels        = (xwb.format >> 1) & 0x7; /*3*/
        xwb.tag             = (xwb.format) & 0x1; /*1*/
    }
    else {
        xwb.bits_per_sample = (xwb.format >> 31) & 0x1; /*1*/
        xwb.block_align     = (xwb.format >> 23) & 0xFF; /*8*/
        xwb.sample_rate     = (xwb.format >> 5) & 0x3FFFF; /*18*/
        xwb.channels        = (xwb.format >> 2) & 0x7; /*3*/
        xwb.tag             = (xwb.format) & 0x3; /*2*/
    }

    /* standardize tag to codec */
    if (xwb.xact == 1) {
        switch(xwb.tag){
            case 0: xwb.codec = PCM; break;
            case 1: xwb.codec = XBOX_ADPCM; break;
            case 2: xwb.codec = WMA; break;
            default: goto fail;
        }
    } else if (xwb.xact == 2) {
        switch(xwb.tag) {
            case 0: xwb.codec = PCM; break;
            /* Table Tennis (v34): XMA1, Prey (v38): XMA2, v35/36/37: ? */
            case 1: xwb.codec = xwb.version <= 34 ? XMA1 : XMA2; break;
            case 2: xwb.codec = MS_ADPCM; break;
            default: goto fail;
        }
    } else {
        switch(xwb.tag) {
            case 0: xwb.codec = PCM; break;
            case 1: xwb.codec = XMA2; break;
            case 2: xwb.codec = MS_ADPCM; break;
            case 3: xwb.codec = XWMA; break;
            default: goto fail;
        }
    }

    /* Techland's bizarre format hijack (Nail'd, Sniper: Ghost Warrior PS3).
     * Somehow they used XWB + ATRAC3 in their PS3 games, very creative */
    if (xwb.version == 0x10000 && xwb.codec == XMA2 /* v 0x10000 is used in their X360 games too */
            && (xwb.block_align == 0x60 || xwb.block_align == 0x98 || xwb.block_align == 0xc0) ) {
        xwb.codec = ATRAC3; /* standard ATRAC3 blocks sizes; no other way to identify (other than reading data) */

        /* num samples uses a modified entry_info format (maybe skip samples + samples? sfx use the standard format)
         * ignore for now and just calc max samples */ //todo
        xwb.num_samples = atrac3_bytes_to_samples(xwb.stream_size, xwb.block_align * xwb.channels);
    }


    /* fix samples */
    if ((xwb.xact == 1 || xwb.xact == 2) && xwb.codec == PCM) {
        xwb.num_samples = xwb.stream_size / 2 / xwb.channels;
        if (xwb.loop_flag) {
            xwb.loop_start_sample = (xwb.loop_start) / 2 / xwb.channels;
            xwb.loop_end_sample = (xwb.loop_start + xwb.loop_end) / 2 / xwb.channels;
        }
    }
    else if (xwb.xact == 1 && xwb.codec == XBOX_ADPCM) {
        xwb.num_samples = xwb.stream_size / 36 / xwb.channels*64;
        if (xwb.loop_flag) {
            xwb.loop_start_sample = xwb.loop_start / 36 / xwb.channels*64;
            xwb.loop_end_sample = (xwb.loop_start + xwb.loop_end) / 36 / xwb.channels*64;
        }
    }
    else if (xwb.xact == 2 && xwb.codec == MS_ADPCM && xwb.loop_flag) {
        int block_size, samples_per_frame;
        block_size = (xwb.block_align + 22) * xwb.channels; /*22=CONVERSION_OFFSET (?)*/
        samples_per_frame = (block_size-(7-1) * xwb.channels) * 2 / xwb.channels;

        xwb.loop_start_sample = (xwb.loop_start) / block_size / samples_per_frame;
        xwb.loop_end_sample = (xwb.loop_start + xwb.loop_end) / block_size / samples_per_frame;
    }
    else if (xwb.xact == 2 && xwb.version <= 38 /* v38: byte offset, v40+: sample offset, v39: ? */
            && (xwb.codec == XMA1 || xwb.codec == XMA2) &&  xwb.loop_flag) {
#ifdef VGM_USE_FFMPEG
        /* need to manually find sample offsets, thanks to Microsoft dumb headers */
        xma_sample_data xma_sd;
        memset(&xma_sd,0,sizeof(xma_sample_data));

        xma_sd.xma_version = xwb.codec == XMA1 ? 1 : 2;
        xma_sd.channels = xwb.channels;
        xma_sd.data_offset = xwb.stream_offset;
        xma_sd.data_size = xwb.stream_size;
        xma_sd.loop_flag = xwb.loop_flag;
        xma_sd.loop_start_b = xwb.loop_start; /* bit offset in the stream */
        xma_sd.loop_end_b   = (xwb.loop_end >> 4); /*28b */
        /* XACT adds +1 to the subframe, but this means 0 can't be used? */
        xma_sd.loop_end_subframe    = ((xwb.loop_end >> 2) & 0x3) + 1; /* 2b */
        xma_sd.loop_start_subframe  = ((xwb.loop_end >> 0) & 0x3) + 1; /* 2b */

        xma_get_samples(&xma_sd, streamFile);
        xwb.loop_start_sample = xma_sd.loop_start_sample;
        xwb.loop_end_sample = xma_sd.loop_end_sample;

        // todo fix properly (XWB loop_start/end seem to count padding samples while XMA1 RIFF doesn't)
        //this doesn't seem ok because can fall within 0 to 512 (ie.- first frame)
        //if (xwb.loop_start_sample) xwb.loop_start_sample -= 512;
        //if (xwb.loop_end_sample) xwb.loop_end_sample -= 512;

        //add padding back until it's fixed (affects looping)
        // (in rare cases this causes a glitch in FFmpeg since it has a bug where it's missing some samples)
        xwb.num_samples += 64 + 512;
#else
    goto fail;
#endif
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(xwb.channels,xwb.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = xwb.sample_rate;
    vgmstream->num_samples = xwb.num_samples;
    vgmstream->loop_start_sample = xwb.loop_start_sample;
    vgmstream->loop_end_sample = xwb.loop_end_sample;
    vgmstream->num_streams = xwb.streams;
    vgmstream->meta_type = meta_XWB;

    switch(xwb.codec) {
        case PCM:
            vgmstream->coding_type = xwb.bits_per_sample == 0 ? coding_PCM8 :
                    (xwb.little_endian ? coding_PCM16LE : coding_PCM16BE);
            vgmstream->layout_type = xwb.channels > 1 ? layout_interleave : layout_none;
            vgmstream->interleave_block_size = xwb.bits_per_sample == 0 ? 0x01 : 0x02;
            break;

        case XBOX_ADPCM:
            vgmstream->coding_type = coding_XBOX;
            vgmstream->layout_type = layout_none;
            break;

        case MS_ADPCM:
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = (xwb.block_align + 22) * xwb.channels; /*22=CONVERSION_OFFSET (?)*/
            break;

#ifdef VGM_USE_FFMPEG
        case XMA1: {
            ffmpeg_codec_data *ffmpeg_data = NULL;
            uint8_t buf[100];
            int bytes;

            bytes = ffmpeg_make_riff_xma1(buf, 100, vgmstream->num_samples, xwb.stream_size, vgmstream->channels, vgmstream->sample_rate, 0);
            if (bytes <= 0) goto fail;

            ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf,bytes, xwb.stream_offset,xwb.stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case XMA2: {
            ffmpeg_codec_data *ffmpeg_data = NULL;
            uint8_t buf[100];
            int bytes, block_size, block_count;

            block_size = 0x10000; /* XACT default */
            block_count = xwb.stream_size / block_size + (xwb.stream_size % block_size ? 1 : 0);

            bytes = ffmpeg_make_riff_xma2(buf, 100, vgmstream->num_samples, xwb.stream_size, vgmstream->channels, vgmstream->sample_rate, block_count, block_size);
            if (bytes <= 0) goto fail;

            ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf,bytes, xwb.stream_offset,xwb.stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case WMA: { /* WMAudio1 (WMA v1) */
            ffmpeg_codec_data *ffmpeg_data = NULL;

            ffmpeg_data = init_ffmpeg_offset(streamFile, xwb.stream_offset,xwb.stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case XWMA: { /* WMAudio2 (WMA v2), WMAudio3 (WMA Pro) */
            ffmpeg_codec_data *ffmpeg_data = NULL;
            uint8_t buf[100];
            int bytes, bps_index, block_align, block_index, avg_bps, wma_codec;

            bps_index = (xwb.block_align >> 5);  /* upper 3b bytes-per-second index */ //docs say 2b+6b but are wrong
            block_index =  (xwb.block_align) & 0x1F; /*lower 5b block alignment index */
            if (bps_index >= 7) goto fail;
            if (block_index >= 17) goto fail;

            avg_bps = wma_avg_bps_index[bps_index];
            block_align = wma_block_align_index[block_index];
            wma_codec = xwb.bits_per_sample ? 0x162 : 0x161; /* 0=WMAudio2, 1=WMAudio3 */

            bytes = ffmpeg_make_riff_xwma(buf, 100, wma_codec, xwb.stream_size, vgmstream->channels, vgmstream->sample_rate, avg_bps, block_align);
            if (bytes <= 0) goto fail;

            ffmpeg_data = init_ffmpeg_header_offset(streamFile, buf,bytes, xwb.stream_offset,xwb.stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case ATRAC3: { /* Techland extension */
            uint8_t buf[200];
            int bytes;

            int block_size = xwb.block_align * vgmstream->channels;
            int joint_stereo = xwb.block_align == 0x60; /* untested, ATRAC3 default */
            int skip_samples = 0; /* unknown */

            bytes = ffmpeg_make_riff_atrac3(buf, 200, vgmstream->num_samples, xwb.stream_size, vgmstream->channels, vgmstream->sample_rate, block_size, joint_stereo, skip_samples);
            if (bytes <= 0) goto fail;

            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, xwb.stream_offset,xwb.stream_size);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        default:
            goto fail;
    }


    start_offset = xwb.data_offset;

    if ( !vgmstream_open_stream(vgmstream,streamFile,start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
