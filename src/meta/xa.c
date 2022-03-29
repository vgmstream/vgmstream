#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


static int xa_read_subsongs(STREAMFILE* sf, int target_subsong, off_t start, uint16_t* p_stream_config, off_t* p_stream_offset, size_t* p_stream_size, int* p_form2);
static int xa_check_format(STREAMFILE* sf, off_t offset, int is_blocked);

/* XA - from Sony PS1 and Philips CD-i CD audio */
VGMSTREAM* init_vgmstream_xa(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channels, sample_rate, bps;
    int is_riff = 0, is_form2 = 0, is_blocked;
    size_t stream_size = 0;
    int total_subsongs = 0, target_subsong = sf->stream_index;
    uint16_t target_config = 0;


    /* checks */
    if (read_u32be(0x00,sf) == 0x00FFFFFF && read_u32be(0x04,sf) == 0xFFFFFFFF && read_u32be(0x08,sf) == 0xFFFFFF00) {
        /* sector sync word = raw data */
        is_blocked = 1;
        start_offset = 0x00;
    }
    else if (is_id32be(0x00,sf, "RIFF") && is_id32be(0x08,sf, "CDXA") && is_id32be(0x0C,sf, "fmt ")) {
        /* RIFF header = raw with header (optional, added by CD drivers when copying and not part of the CD data) */
        is_blocked = 1;
        is_riff = 1;
        start_offset = 0x2c; /* after "data", ignore RIFF values as often are wrong */
    }
    else {
        /* non-blocked (ISO 2048 mode1/data) or incorrectly ripped: use TXTH */
        goto fail;
    }

    /* .xa: common
     * .str: often videos and sometimes speech/music
     * .pxa: Mortal Kombat 4 (PS1)
     * .grn: Micro Machines (CDi)
     * .an2: Croc (PS1) movies
     * (extensionless): bigfiles [Castlevania: Symphony of the Night (PS1)] */
    if (!check_extensions(sf,"xa,str,pxa,grn,an2,"))
        goto fail;

    /* Proper XA comes in raw (BIN 2352 mode2/form2) CD sectors, that contain XA subheaders.
     * For headerless XA (ISO 2048 mode1/data) mode use TXTH. */

    /* test for XA data, since format is raw-ish (with RIFF it's assumed to be ok) */
    if (!is_riff && !xa_check_format(sf, start_offset, is_blocked))
       goto fail;

    /* find subsongs as XA can interleave sectors using 'file' and 'channel' makers (see blocked_xa.c) */
    if (/*!is_riff &&*/ is_blocked) {
        total_subsongs = xa_read_subsongs(sf, target_subsong, start_offset, &target_config, &start_offset, &stream_size, &is_form2);
        if (total_subsongs <= 0) goto fail;
    }
    else {
        stream_size = get_streamfile_size(sf) - start_offset;
    }

    /* data is ok: parse header */
    if (is_blocked) {
        /* parse 0x18 sector header (also see blocked_xa.c)  */
        uint8_t xa_header = read_u8(start_offset + 0x13,sf);

        switch((xa_header >> 0) & 3) { /* 0..1: mono/stereo */
            case 0: channels = 1; break;
            case 1: channels = 2; break;
            default: goto fail;
        }
        switch((xa_header >> 2) & 3) { /* 2..3: sample rate */
            case 0: sample_rate = 37800; break;
            case 1: sample_rate = 18900; break;
            default: goto fail;
        }
        switch((xa_header >> 4) & 3) { /* 4..5: bits per sample */
            case 0: bps = 4; break; /* PS1 games only do 4-bit ADPCM */
            case 1: bps = 8; break; /* Micro Machines (CDi) */
            default: goto fail;
        }
        switch((xa_header >> 6) & 1) { /* 6: emphasis flag (should apply a de-emphasis filter) */
            case 0: break;
            default: /* very rare, waveform looks ok so maybe not needed [Croc (PS1) PACKx.str] */
                vgm_logi("XA: emphasis found\n");
                break;
        }
        switch((xa_header >> 7) & 1) { /* 7: reserved */
            case 0: break;
            default:
                vgm_logi("XA: unknown reserved bit found\n");
                goto fail;
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
    uint8_t frame_hdr[0x10];
    int i, j, sector = 0, skip = 0;
    off_t test_offset = offset;
    const size_t sector_size = (is_blocked ? 0x900 : 0x800);
    const size_t extra_size = (is_blocked ? 0x18 : 0x00);
    const size_t frame_size = 0x80;
    const int sector_max = 3;
    const int skip_max = 32; /* videos interleave 7 or 15 sectors + 1 audio sector, maybe 31 too */

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

        for (i = 0; i < (sector_size / frame_size); i++) {
            read_streamfile(frame_hdr, test_offset, sizeof(frame_hdr), sf);

            /* XA frame checks: filter indexes should be 0..3, and shifts 0..C */
            for (j = 0; j < 16; j++) {
                uint8_t header = get_u8(frame_hdr + j);
                if (((header >> 4) & 0xF) > 0x03)
                    goto fail;
                if (((header >> 0) & 0xF) > 0x0c)
                    goto fail;
            }

            /* XA headers pairs are repeated */
            if (get_u32be(frame_hdr+0x00) != get_u32be(frame_hdr+0x04) ||
                get_u32be(frame_hdr+0x08) != get_u32be(frame_hdr+0x0c))
                goto fail;
            /* blank frames should always use 0x0c0c0c0c (due to how shift works) */
            if (get_u32be(frame_hdr+0x00) == 0 &&
                get_u32be(frame_hdr+0x04) == 0 &&
                get_u32be(frame_hdr+0x08) == 0 &&
                get_u32be(frame_hdr+0x0c) == 0)
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


#define XA_SUBSONG_MAX  1024 /* +500 found in bigfiles like Castlevania SOTN */

typedef struct xa_subsong_t {
    uint16_t config;
    off_t start;
    int form2;
    int sectors;
    int end_flag;
} xa_subsong_t;

/* Get subsong info, as real XA data interleaves N sectors/subsongs (often 8/16). Extractors deinterleave
 * but we parse interleaved too for completeness. Even if we have a single deint'd XA this is useful to get
 * usable sectors for bytes-to-samples.
 *
 * Bigfiles that paste tons of XA together are slow to parse since we need to read every sector to
 * count totals, but XA subsong handling is mainly for educational purposes. */
static int xa_read_subsongs(STREAMFILE* sf, int target_subsong, off_t start, uint16_t* p_stream_config, off_t* p_stream_offset, size_t* p_stream_size, int* p_form2) {
    xa_subsong_t *cur_subsong = NULL;
    xa_subsong_t subsongs[XA_SUBSONG_MAX] = {0};
    const size_t sector_size = 0x930;
    uint16_t prev_config;
    int i, subsongs_count = 0;
    size_t file_size;
    off_t offset;
    STREAMFILE *sf_test = NULL;
    uint8_t header[4];


    /* buffer to speed up header reading; bigger (+0x8000) is actually faster than very small (~0x10),
     * even though we only need sector headers and will end up reading the whole file that way */
    sf_test = reopen_streamfile(sf, 0x10000);
    if (!sf_test) goto fail;

    prev_config = 0xFFFFu;
    file_size = get_streamfile_size(sf);
    offset = start;

    /* read XA sectors */
    while (offset < file_size) {
        uint16_t xa_config;
        uint8_t  xa_submode;
        int is_audio, is_eof;

        read_streamfile(header, offset + 0x10, sizeof(header), sf_test);
        xa_config    = get_u16be(header + 0x00); /* file+channel markers */
        xa_submode   = get_u8   (header + 0x02); /* flags */
        is_audio = !(xa_submode & 0x08) && (xa_submode & 0x04) && !(xa_submode & 0x02);
        is_eof = (xa_submode & 0x80);

        VGM_ASSERT((xa_submode & 0x01), "XA: end of audio at %lx\n", offset); /* rare, signals last sector [Tetris (CD-i)] */
        //;VGM_ASSERT(is_eof, "XA: eof at %lx\n", offset);
        //;VGM_ASSERT(!is_audio, "XA: not audio at %lx\n", offset);

        if (!is_audio) {
            offset += sector_size;
            continue;
        }

        /* detect file+channel change = sector of new subsong or existing subsong
         * (happens on every sector for interleaved XAs but only once for deint'd or video XAs) */
        if (xa_config != prev_config)
        {
            /* find if this sector/config belongs to known subsong that hasn't ended
             * (unsure if repeated configs+end flag is possible but probably in giant XAs) */
            cur_subsong = NULL;
            for (i = 0; i < subsongs_count; i++) { /* search algo could be improved, meh */
                if (subsongs[i].config == xa_config && !subsongs[i].end_flag) {
                    cur_subsong = &subsongs[i];
                    break;
                }
            }

            /* old subsong not found = add new to list */
            if (cur_subsong == NULL) {
                uint8_t xa_channel = get_u8(header + 0x01);

                /* when file+channel changes mark prev subsong of the same channel as finished
                 * (this ensures reused ids in bigfiles are handled properly, ex.: 0101... 0201... 0101...) */
                for (i = subsongs_count - 1; i >= 0; i--) {
                    uint16_t subsong_config = subsongs[i].config;
                    uint8_t subsong_channel = subsong_config & 0xFF;
                    if (xa_config != subsong_config && xa_channel == subsong_channel) {
                        subsongs[i].end_flag = 1;
                        break;
                    }
                }


                cur_subsong = &subsongs[subsongs_count];
                subsongs_count++;
                if (subsongs_count >= XA_SUBSONG_MAX) goto fail;

                cur_subsong->config = xa_config;
                cur_subsong->form2 = (xa_submode & 0x20);
                cur_subsong->start = offset;
              //cur_subsong->sectors = 0;
              //cur_subsong->end_flag = 0;

                //;VGM_LOG("XA: new subsong %i with config %04x at %lx\n", subsongs_count, xa_config, offset);
            }

            prev_config = xa_config;
        }
        else {
            if (cur_subsong == NULL) goto fail;
        }

        cur_subsong->sectors++;
        if (is_eof)
            cur_subsong->end_flag = 1;

        offset += sector_size;
    }

    VGM_ASSERT(subsongs_count < 1, "XA: no audio found\n");

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > subsongs_count || subsongs_count < 1) goto fail;

    cur_subsong = &subsongs[target_subsong - 1];
    *p_stream_config = cur_subsong->config;
    *p_stream_offset = cur_subsong->start;
    *p_stream_size   = cur_subsong->sectors * sector_size;
    *p_form2         = cur_subsong->form2;

    //;VGM_LOG("XA: subsong config=%x, offset=%lx, size=%x, form2=%i\n", *p_stream_config, *p_stream_offset, *p_stream_size, *p_form2);

    close_streamfile(sf_test);
    return subsongs_count;
fail:
    close_streamfile(sf_test);
    return 0;
}
