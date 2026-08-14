#include "meta.h"
#include "../layout/layout.h"
#include "../util/reader_text.h"
#include "../util/string_utils.h"

static bool get_falcom_looping(STREAMFILE* sf, int* p_loop_start, int* p_loop_end);

/* .DEC/DE2 - from Falcom PC games [Xanadu Next (PC), Zwei!! (PC), VM Japan (PC), Gurumin (PC)] */
VGMSTREAM* init_vgmstream_dec(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    off_t riff_offset = 0x00;
    size_t pcm_size = 0;
    int loop_flag, channels, sample_rate, loop_start = 0, loop_end = 0;


    /* checks
     * .dec: main,
     * .de2: Gurumin (PC) */
    if (!check_extensions(sf,"dec,de2"))
        return NULL;

    // Gurumin has extra data, maybe related to rhythm (~0x50000)
    if (check_extensions(sf,"de2")) {
        uint32_t unk1 = read_u32le(0x00,sf);
        uint32_t unk2 = read_u32le(0x04,sf);
        if (unk1 < 0x5A0 || unk1 > 0x5F0) // typically ~0x5c0
            return NULL;
        if (unk2 != 0x0B)
            return NULL;
        // 08: close to data size ^ unk2, but sometimes slightly smaller
        uint32_t head_size = read_u32le(0x0c,sf) ^ unk2; // legitimate! really!

        riff_offset = 0x10 + head_size;
    }


    /* fake PCM RIFF header (the original WAV's) wrapping MS-ADPCM */
    if (!is_id32be(riff_offset+0x00,sf, "RIFF") ||
        !is_id32be(riff_offset+0x08,sf, "WAVE"))
            return NULL;

    if (is_id32be(riff_offset+0x0c,sf, "PAD ")) { // blank data with wrong chunk size [Zwei!! (PC)]
        sample_rate = 44100;
        channels = 2;
        pcm_size = read_u32le(riff_offset+0x04,sf) - 0x24;
        // somehow there is garbage at the beginning of some tracks
    }
    else if (is_id32be(riff_offset+0x0c,sf, "fmt ")) {
        // 0x10: chunk size (usually 0x12, 0x10 in some files)
        if (read_u16le(riff_offset+0x14,sf) != 0x01) // PCM (actually MS-ADPCM)
            return NULL;
        if (read_u16le(riff_offset+0x20,sf) != 4 || read_u16le(riff_offset+0x22,sf) != 16)
            return NULL;

        channels = read_16bitLE(riff_offset+0x16,sf);
        sample_rate = read_s32le(riff_offset+0x18,sf);

        if (is_id32be(riff_offset+0x24,sf, "data")) {
            pcm_size = read_u32le(riff_offset+0x28,sf);
        }
        else {
            pcm_size = read_u32le(riff_offset+0x04,sf) - 0x24; // some Zwei!! files don't keep "data" chunk
        }
    }
    else {
        return NULL;
    }

    if (channels != 2)
        return NULL;

    start_offset = riff_offset + 0x2c;
    loop_flag = get_falcom_looping(sf, &loop_start, &loop_end);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_DEC;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = pcm_size / 2 / channels;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_MSADPCM;
    vgmstream->frame_size = 0x800;
    vgmstream->layout_type = layout_blocked_dec;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

#define TXT_LINE_MAX 256

/* Falcom loves loop points in external text files, here we parse them */
typedef enum { XANADU_NEXT, ZWEI, DINOSAUR_RESURRECTION, GURUMIN } falcom_loop_t;
static bool get_falcom_looping(STREAMFILE* sf, int* p_loop_start, int* p_loop_end) {
    STREAMFILE* sf_text;
    off_t txt_offset = 0x00;
    falcom_loop_t type;
    int loop_start = 0, loop_end = 0;
    bool loop_flag = false;
    char filename[TXT_LINE_MAX];


    /* try one of the many loop files */
    if ((sf_text = open_streamfile_by_filename(sf,"bgm.tbl")) != NULL) {
        type = XANADU_NEXT;
    }
    else if ((sf_text = open_streamfile_by_filename(sf,"bgm.scr")) != NULL) {
        type = ZWEI;
    }
    else if ((sf_text = open_streamfile_by_filename(sf,"loop.txt")) != NULL) { // actual name in Shift JIS, 0x838B815B8376
        type = DINOSAUR_RESURRECTION;
    }
    else if ((sf_text = open_streamfile_by_filename(sf,"map.itm")) != NULL) {
        type = GURUMIN;
    }
    else {
        goto end;
    }

    get_streamfile_filename(sf, filename, TXT_LINE_MAX);

    /* read line by line */
    while (txt_offset < get_streamfile_size(sf_text)) {
        char line[TXT_LINE_MAX];
        char name[TXT_LINE_MAX];
        int ok, line_ok, loop, bytes_read;

        bytes_read = read_line(line, TXT_LINE_MAX, txt_offset, sf_text, &line_ok);
        if (!line_ok) goto end;

        txt_offset += bytes_read;

        if (line[0]=='/' || line[0]=='#' || line[0]=='[' || line[0]=='\0') /* comment/empty */
            continue;

        /* each game changes line format, wee */
        switch(type) {
            case XANADU_NEXT: /* "XANA000",          0,      0,99999990,0 */
                ok = sscanf(line, "\"%[^\"]\", %*d, %d, %d, %d", name, &loop_start, &loop_end, &loop);
                if (ok == 4 && str_startswith_ci(filename, name)) {
                    loop_flag = (loop && loop_end != 0);
                    goto end;
                }
                break;

            case ZWEI: /* 1,.\wav\bgm01.wav,497010,7386720;//comment */
                ok = sscanf(line, "%*i,.\\wav\\%[^.].dec,%d,%d;%*s", name, &loop_start, &loop_end);
                if (ok == 3 && str_startswith_ci(filename, name)) {
                    loop_flag = (loop_end != 9000000);
                    goto end;
                }
                break;

            case DINOSAUR_RESURRECTION: /* 01   970809 - 8015852 */
                // names are always dinow_xx.dec, while file only has XX
                ok = sscanf(line, "%[^ ] %d - %d", name, &loop_start, &loop_end);
                if (ok == 3 && str_startswith_ci(filename, "dinow_") && str_startswith_ci(filename + 6, name) && str_startswith_ci(filename + 8, ".dec")) {
                    loop_flag = true;
                    goto end;
                }
                break;

            case GURUMIN: /* 0003 BGM03      dec 00211049    02479133    00022050    00000084    //comment */
                ok = sscanf(line,"%*i %[^ \t] %*[^ \t] %d %d %*d %*d %*s", name, &loop_start, &loop_end);
                if (ok == 3 && str_startswith_ci(filename, name)) {
                    loop_flag = (loop_end != 99999999 && loop_end != 10000000);
                    goto end;
                }
                break;

            default:
                break;
        }
    }

end:
    if (loop_flag) {
        *p_loop_start = loop_start;
        *p_loop_end = loop_end;
    }

    close_streamfile(sf_text);
    return loop_flag;
}
