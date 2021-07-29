#include "meta.h"
#include "../layout/layout.h"

#define TXT_LINE_MAX 0x1000
static int get_falcom_looping(STREAMFILE* sf, int* p_loop_start, int* p_loop_end);

/* .DEC/DE2 - from Falcom PC games (Xanadu Next, Zwei!!, VM Japan, Gurumin) */
VGMSTREAM* init_vgmstream_dec(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    off_t riff_off = 0x00;
    size_t pcm_size = 0;
    int loop_flag, channel_count, sample_rate, loop_start = 0, loop_end = 0;


    /* checks
     * .dec: main,
     * .de2: Gurumin (PC) */
    if (!check_extensions(sf,"dec,de2"))
        goto fail;

    /* Gurumin has extra data, maybe related to rhythm (~0x50000) */
    if (check_extensions(sf,"de2")) {
        /* still not sure what this is for, but consistently 0xb */
        if (read_32bitLE(0x04,sf) != 0x0b) goto fail;

        /* legitimate! really! */
        riff_off = 0x10 + (read_32bitLE(0x0c,sf) ^ read_32bitLE(0x04,sf));
    }


    /* fake PCM RIFF header (the original WAV's) wrapping MS-ADPCM */
    if (!is_id32be(riff_off+0x00,sf, "RIFF") ||
        !is_id32be(riff_off+0x08,sf, "WAVE"))
        goto fail;

    if (is_id32be(riff_off+0x0c,sf, "PAD ")) { /* blank with wrong chunk size [Zwei!! ())PC)]*/
        sample_rate = 44100;
        channel_count = 2;
        pcm_size = read_32bitLE(riff_off+0x04,sf) - 0x24;
        /* somehow there is garbage at the beginning of some tracks */
    }
    else if (is_id32be(riff_off+0x0c,sf, "fmt ")) {
        //if (read_32bitLE(riff_off+0x10,sf) != 0x12) goto fail; /* 0x10 in some */
        if (read_16bitLE(riff_off+0x14,sf) != 0x01) goto fail; /* PCM (actually MS-ADPCM) */
        if (read_16bitLE(riff_off+0x20,sf) != 4 ||
            read_16bitLE(riff_off+0x22,sf) != 16) goto fail; /* 16-bit */

        channel_count = read_16bitLE(riff_off+0x16,sf);
        sample_rate = read_32bitLE(riff_off+0x18,sf);
        if (read_32bitBE(riff_off+0x24,sf) == 0x64617461) { /* "data" size except in some Zwei!! */
            pcm_size = read_32bitLE(riff_off+0x28,sf);
        } else {
            pcm_size = read_32bitLE(riff_off+0x04,sf) - 0x24;
        }
    }
    else {
        goto fail;
    }

    if (channel_count != 2)
        goto fail;

    start_offset = riff_off + 0x2c;
    loop_flag = get_falcom_looping(sf, &loop_start, &loop_end);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_DEC;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = pcm_size / 2 / channel_count;
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


/* Falcom loves loop points in external text files, here we parse them */
typedef enum { XANADU_NEXT, ZWEI, DINOSAUR_RESURRECTION, GURUMIN } falcom_loop_t;
static int get_falcom_looping(STREAMFILE *sf, int *out_loop_start, int *out_loop_end) {
    STREAMFILE *streamText;
    off_t txt_offset = 0x00;
    falcom_loop_t type;
    int loop_start = 0, loop_end = 0, loop_flag = 0;
    char filename[TXT_LINE_MAX];


    /* try one of the many loop files */
    if ((streamText = open_streamfile_by_filename(sf,"bgm.tbl")) != NULL) {
        type = XANADU_NEXT;
    }
    else if ((streamText = open_streamfile_by_filename(sf,"bgm.scr")) != NULL) {
        type = ZWEI;
    }
    else if ((streamText = open_streamfile_by_filename(sf,"loop.txt")) != NULL) { /* actual name in Shift JIS, 0x838B815B8376 */
        type = DINOSAUR_RESURRECTION;
    }
    else if ((streamText = open_streamfile_by_filename(sf,"map.itm")) != NULL) {
        type = GURUMIN;
    }
    else {
        goto end;
    }

    get_streamfile_filename(sf,filename,TXT_LINE_MAX);

    /* read line by line */
    while (txt_offset < get_streamfile_size(streamText)) {
        char line[TXT_LINE_MAX];
        char name[TXT_LINE_MAX];
        int ok, line_ok, loop, bytes_read;

        bytes_read = read_line(line, TXT_LINE_MAX, txt_offset, streamText, &line_ok);
        if (!line_ok) goto end;

        txt_offset += bytes_read;

        if (line[0]=='/' || line[0]=='#' || line[0]=='[' || line[0]=='\0') /* comment/empty */
            continue;

        /* each game changes line format, wee */
        switch(type) {
            case XANADU_NEXT: /* "XANA000",          0,      0,99999990,0 */
                ok = sscanf(line,"\"%[^\"]\", %*d, %d, %d, %d", name,&loop_start,&loop_end,&loop);
                if (ok == 4 && strncasecmp(filename,name,strlen(name)) == 0) {
                    loop_flag = (loop && loop_end != 0);
                    goto end;
                }
                break;

            case ZWEI: /* 1,.\wav\bgm01.wav,497010,7386720;//comment */
                ok = sscanf(line,"%*i,.\\wav\\%[^.].dec,%d,%d;%*s", name,&loop_start,&loop_end);
                if (ok == 3 && strncasecmp(filename,name,strlen(name)) == 0) {
                    loop_flag = (loop_end != 9000000);
                    goto end;
                }
                break;

            case DINOSAUR_RESURRECTION: /* 01   970809 - 8015852 */
                strcpy(name,"dinow_"); /* for easier comparison */
                ok = sscanf(line,"%[^ ] %d - %d", (name+6), &loop_start,&loop_end);
                if (ok == 3 && strncasecmp(filename,name,strlen(name)) == 0) {
                    loop_flag = 1;
                    goto end;
                }
                break;

            case GURUMIN: /* 0003 BGM03      dec 00211049    02479133    00022050    00000084    //comment */
                ok = sscanf(line,"%*i %[^ \t] %*[^ \t] %d %d %*d %*d %*s", name,&loop_start,&loop_end);
                if (ok == 3 && strncasecmp(filename,name,strlen(name)) == 0) {
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
        *out_loop_start = loop_start;
        *out_loop_end = loop_end;
    }

    if (streamText) close_streamfile(streamText);
    return loop_flag;
}
