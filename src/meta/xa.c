#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


static int xa_read_subsongs(STREAMFILE* sf, int target_subsong, uint32_t start, uint32_t* p_stream_offset, uint32_t* p_stream_size);
static int xa_check_format(STREAMFILE* sf, off_t offset, int is_blocked);

/* XA - from Sony PS1 and Philips CD-i CD audio */
VGMSTREAM* init_vgmstream_xa(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, stream_size = 0;
    int loop_flag = 0, channels, sample_rate, bps;
    bool is_riff = false, is_form2 = false, is_blocked;
    int total_subsongs = 0, target_subsong = sf->stream_index;
    uint16_t target_config = 0;


    /* checks */
    if (read_u32be(0x00,sf) == 0x00FFFFFF && read_u32be(0x04,sf) == 0xFFFFFFFF && read_u32be(0x08,sf) == 0xFFFFFF00) {
        /* sector sync word = raw data */
        is_blocked = true;
        start_offset = 0x00;
    }
    else if (is_id32be(0x00,sf, "RIFF") && is_id32be(0x08,sf, "CDXA") && is_id32be(0x0C,sf, "fmt ")) {
        /* RIFF header = raw with header (optional, added by CD drivers when copying and not part of the CD data) */
        is_blocked = true;
        is_riff = true;
        start_offset = 0x2c; /* after "data", ignore RIFF values as often are wrong */
    }
    else {
        /* non-blocked (ISO 2048 mode1/data) or incorrectly ripped: use TXTH */
        return NULL;
    }

    /* .xa: common
     * .str: often videos and sometimes speech/music
     * .pxa: Mortal Kombat 4 (PS1)
     * .grn: Micro Machines (CDi)
     * .an2: Croc (PS1) movies
     * .no: Incredible Crisis (PS1)
     * (extensionless): bigfiles [Castlevania: Symphony of the Night (PS1)]
     * .xai: Quake II (PS1)
     * .ixa: Wild Arms (PS1) */
    if (!check_extensions(sf,"xa,str,pxa,grn,an2,no,,xai,ixa"))
        return NULL;

    /* Proper XA comes in raw (BIN 2352 mode2/form2) CD sectors, that contain XA subheaders.
     * For headerless XA (ISO 2048 mode1/data) mode use TXTH. */

    /* test for XA data, since format is raw-ish (with RIFF it's assumed to be ok) */
    if (!is_riff && !xa_check_format(sf, start_offset, is_blocked))
       return NULL;

    /* find subsongs as XA can interleave sectors using 'file' and 'channel' makers (see blocked_xa.c) */
    if (/*!is_riff &&*/ is_blocked) {
        total_subsongs = xa_read_subsongs(sf, target_subsong, start_offset, &start_offset, &stream_size);
        if (total_subsongs <= 0) goto fail;
    }
    else {
        stream_size = get_streamfile_size(sf) - start_offset;
    }

    /* data is ok: parse header */
    if (is_blocked) {
        /* parse 0x18 sector header (also see blocked_xa.c)  */
        uint32_t curr_info = read_u32be(start_offset + 0x10, sf);
        uint16_t xa_config  = (curr_info >> 16) & 0xFFFF; /* file+channel markers */
        uint8_t  xa_submode = (curr_info >>  8) & 0xFF;
        uint8_t  xa_header  = (curr_info >>  0) & 0xFF;

        /* header is repeated at 0x14 and could check if matches, but some ripped XA patch byte 0x01
         * for some reason, and in rare cases has garbage [Incredible Crisis (PS1) XAPACK00.NO#5]
         * (probably means a real PS1 only uses the first header, if it can play such XA) */

        target_config = xa_config;
        is_form2 = (xa_submode & 0x20);

        switch((xa_header >> 0) & 3) { /* 0..1: mono/stereo */
            case 0: channels = 1; break;
            case 1: channels = 2; break;
            default:
                vgm_logi("XA: buggy data found\n");
                goto fail;
        }
        switch((xa_header >> 2) & 3) { /* 2..3: sample rate */
            case 0: sample_rate = 37800; break;
            case 1: sample_rate = 18900; break;
            default:
                vgm_logi("XA: buggy data found\n");
                goto fail;
        }
        switch((xa_header >> 4) & 3) { /* 4..5: bits per sample */
            case 0: bps = 4; break; /* PS1 games only do 4-bit ADPCM */
            case 1: bps = 8; break; /* Micro Machines (CDi) */
            default:
                vgm_logi("XA: buggy data found\n");
                goto fail;
        }
        switch((xa_header >> 6) & 1) { /* 6: emphasis flag (should apply a de-emphasis filter) */
            case 0: break;
            default:
                /* very rare, waveform looks ok so maybe not needed [Croc (PS1) PACKx.str] */
                vgm_logi("XA: emphasis found\n");
                break;
        }
        switch((xa_header >> 7) & 1) { /* 7: reserved */
            case 0: break;
            default:
                /* very rare, found in all regions and xa channel's headers but probably a mastering
                 * bug since repeated header is wrong [Incredible Crisis (PS1) XAPACK00.NO#5] */
                vgm_logi("XA: reserved bit found\n");
                break;
        }
    }
    else {
        goto fail;
    }

    /* untested */
    if (bps == 8 && channels == 1)
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XA;
    vgmstream->sample_rate = sample_rate;
    vgmstream->coding_type = bps == 8 ? coding_XA8 : coding_XA;
    vgmstream->layout_type = is_blocked ? layout_blocked_xa : layout_none;
    if (is_blocked) {
        vgmstream->codec_config = target_config;
        vgmstream->num_streams = total_subsongs;
        vgmstream->stream_size = stream_size;
        if (total_subsongs > 1) {
            /* useful at times if game uses many file+channel */
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%04x", target_config);
        }
    }

    vgmstream->num_samples = xa_bytes_to_samples(stream_size, channels, is_blocked, is_form2, bps);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static int xa_check_format(STREAMFILE *sf, off_t offset, int is_blocked) {
    const size_t sector_size = (is_blocked ? 0x900 : 0x800);
    const size_t extra_size = (is_blocked ? 0x18 : 0x00);
    const size_t frame_size = 0x80;
    const int sector_max = 3;
    const int skip_max = 32; /* videos interleave 7 or 15 sectors + 1 audio sector, maybe 31 too */

    uint8_t frame_hdr[0x10];
    int sector = 0, skip = 0;
    uint32_t test_offset = offset;

    /* test frames inside CD sectors */
    while (sector < sector_max) {
        if (is_blocked) {
            uint8_t xa_submode = read_u8(test_offset + 0x12, sf);
            int is_audio = !(xa_submode & 0x08) && (xa_submode & 0x04) && !(xa_submode & 0x02);

            if (is_blocked && !is_audio) {
                skip++;
                if (sector == 0 && skip > skip_max) /* no a single audio sector found */
                    goto fail;
                test_offset += sector_size + extra_size + extra_size;
                continue;
            }
        }

        test_offset += extra_size; /* header */

        for (int i = 0; i < (sector_size / frame_size); i++) {
            read_streamfile(frame_hdr, test_offset, sizeof(frame_hdr), sf);

            /* XA frame checks: filter indexes should be 0..3 and shifts 0..C, but somehow Planet Laika movies have D */
            for (int j = 0; j < 16; j++) {
                uint8_t header = get_u8(frame_hdr + j);
                if (((header >> 4) & 0xF) > 0x03)
                    goto fail;
                if (((header >> 0) & 0xF) > 0x0d)
                    goto fail;
            }

            /* XA headers pairs are repeated */
            if (get_u32be(frame_hdr+0x00) != get_u32be(frame_hdr+0x04) ||
                get_u32be(frame_hdr+0x08) != get_u32be(frame_hdr+0x0c))
                goto fail;
            /* blank frames should always use 0x0c0c0c0c due to how shift works, (in rare file-channels some frames may be blank though) */
            if (i == 0 &&
                get_u32be(frame_hdr+0x00) == 0 && get_u32be(frame_hdr+0x04) == 0 &&
                get_u32be(frame_hdr+0x08) == 0 && get_u32be(frame_hdr+0x0c) == 0)
                goto fail;

            test_offset += 0x80;
        }

        test_offset += extra_size; /* footer */
        sector++;
    }


    return 1;
fail:
    return 0;
}


#define XA_MAX_CHANNELS 32 /* usually 08-16, seen ~24 in Langrisser V (PS1) */

typedef struct {
   uint32_t info;
   int subsong;
} xa_subsong_t;

/* Get subsong info, as real XA data interleaves N sectors/subsongs (often 8/16). Extractors deinterleave
 * but we parse interleaved too for completeness. Even if we have a single deint'd XA this is useful to get
 * usable sectors for bytes-to-samples.
 *
 * Bigfiles that paste tons of XA together are slow to parse since we need to read every sector to
 * count totals, but XA subsong handling is mainly for educational purposes.
 *
 * Raw XA CD sectors are interleaved and classified into "files" and "channels" due to how CD driver/audio buffer works.
 * Devs select one file+channel (or just channel?) to play and CD ignores non-target sectors.
 * "files" can be any number in any order (but usually 00~64), and "channels" seem to be max 00~0F.
 * file+channel (=song) ends with a flag or when file changes; simplified example (upper=file, lower=channel):
 *   0101 0102 0101 0102 0201 0202 0201 0202 0101 0102
 * adapted to subsongs:
 *   0101 #1 (all 0101 sectors until file change = 2 sectors)
 *   0102 #2
 *   0201 #3
 *   0202 #4
 *   0101 #5 (different than first subsong since there was another file in between, 1 sector)
 *   0102 #6
 *
 * For video + audio the layout is the same with extra flags to detect video/audio sectors:
 *   0101v 0101v 0101v 0101v 0101v 0101v 0101v 0101a (usually 7 video sectors per 1 audio sector)
 *
 * CDs can't have 0101 0101 0101 ... audio sectors (need to interleave other channels, or repeat data),
 * but can be seen in demuxed XA. Combinations like a 0101 after 0201 probably only happen when devs
 * paste many XAs into a bigfile, which likely would jump via offsets in exe to the XA start (can be
 * split), but they are detected here for convenience.
 */
static int xa_read_subsongs(STREAMFILE* sf, int target_subsong, uint32_t start, uint32_t* p_stream_offset, uint32_t* p_stream_size) {
    const size_t sector_size = 0x930;
    int subsongs_count = 0;
    uint32_t offset, file_size;
    STREAMFILE* sf_test = NULL;

    xa_subsong_t xa_subsongs[XA_MAX_CHANNELS] = {0};
    uint32_t target_start = 0xFFFFFFFFu;
    int target_sectors = 0;


    /* buffer to speed up header reading; bigger (+0x8000) is actually faster than very small (~0x10),
     * even though we only need sector headers and will end up reading the whole file that way */
    sf_test = reopen_streamfile(sf, 0x10000);
    if (!sf_test) goto fail;

    file_size = get_streamfile_size(sf);
    offset = start;

    if (target_subsong == 0) target_subsong = 1;

    /* read XA sectors */
    while (offset < file_size) {
        uint32_t curr_info = read_u32be(offset + 0x10, sf_test);
      //uint8_t xa_file     = (curr_info >> 24) & 0xFF;
        uint8_t xa_chan     = (curr_info >> 16) & 0xFF;
        uint8_t xa_submode  = (curr_info >>  8) & 0xFF;
      //uint8_t xa_header   = (curr_info >>  0) & 0xFF;
        bool is_audio = !(xa_submode & 0x08) && (xa_submode & 0x04) && !(xa_submode & 0x02);
        bool is_eof = (xa_submode & 0x80);
        bool is_target = false;

        if (xa_chan >= XA_MAX_CHANNELS) {
            VGM_LOG("XA: too many channels: %x\n", xa_chan);
            goto fail;
        }

        //;VGM_ASSERT((xa_submode & 0x01), "XA: end of audio at %x\n", offset); /* rare, signals last sector [Tetris (CD-i), Langrisser V (PS1)] */
        //;VGM_ASSERT(is_eof, "XA: eof %02x%02x at %x\n", xa_file, xa_chan, offset); /* this sector still has data */
        //;VGM_ASSERT(!is_audio, "XA: not audio at %x\n", offset);

        if (!is_audio) {
            offset += sector_size;
            continue;
        }

        /* use info without submode to detect new subsongs */
        curr_info = curr_info & 0xFFFF00FF;

        /* changes for a current channel = new subsong */
        if (xa_subsongs[xa_chan].info != curr_info) {
            subsongs_count++;
            xa_subsongs[xa_chan].info = curr_info;
            xa_subsongs[xa_chan].subsong = subsongs_count;
        }

        /* current channel is still from expected subsong */
        is_target = xa_subsongs[xa_chan].subsong == target_subsong;
        if (is_target) {
            if (target_start == 0xFFFFFFFFu)
                target_start = offset;
            target_sectors++;
        }

        /* remove info (after handling sector) so next comparison for this channel adds a subsong */
        if (is_eof) {
            xa_subsongs[xa_chan].info = 0;
            xa_subsongs[xa_chan].subsong = 0;
        }

        offset += sector_size;
    }

    VGM_ASSERT(subsongs_count < 1, "XA: no audio found\n"); /* probably not possible even in videos */

    if (target_subsong < 0 || target_subsong > subsongs_count || subsongs_count < 1) goto fail;
    if (target_sectors == 0) goto fail;

    *p_stream_offset = target_start;
    *p_stream_size   = target_sectors * sector_size;

    //;VGM_LOG("XA: subsong offset=%x, size=%x\n", *p_stream_offset, *p_stream_size);

    close_streamfile(sf_test);
    return subsongs_count;
fail:
    close_streamfile(sf_test);
    return 0;
}
