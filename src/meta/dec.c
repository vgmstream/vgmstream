#include "meta.h"
#include "../layout/layout.h"

#define TXT_LINE_MAX 0x1000
static int get_falcom_looping(STREAMFILE *streamFile, int *out_loop_start, int *out_loop_end);

/* .DEC/DE2 - from Falcom PC games (Xanadu Next, Zwei!!, VM Japan, Gurumin) */
VGMSTREAM * init_vgmstream_dec(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    off_t riff_off = 0x00;
    size_t pcm_size = 0;
    int loop_flag, channel_count, sample_rate, loop_start = 0, loop_end = 0;


    /* checks
     * .dec: main,
     * .de2: Gurumin (PC) */
    if ( !check_extensions(streamFile,"dec,de2") )
        goto fail;

    /* Gurumin has extra data, maybe related to rhythm (~0x50000) */
    if (check_extensions(streamFile,"de2")) {
        /* still not sure what this is for, but consistently 0xb */
        if (read_32bitLE(0x04,streamFile) != 0x0b) goto fail;

        /* legitimate! really! */
        riff_off = 0x10 + (read_32bitLE(0x0c,streamFile) ^ read_32bitLE(0x04,streamFile));
    }


    /* fake PCM RIFF header (the original WAV's) wrapping MS-ADPCM */
    if (read_32bitBE(riff_off+0x00,streamFile) != 0x52494646 || /* "RIFF" */
        read_32bitBE(riff_off+0x08,streamFile) != 0x57415645)   /* "WAVE" */
        goto fail;

    if (read_32bitBE(riff_off+0x0c,streamFile) == 0x50414420) { /* "PAD " (Zwei!!), blank with wrong chunk size */
        sample_rate = 44100;
        channel_count = 2;
        pcm_size = read_32bitLE(riff_off+0x04,streamFile) - 0x24;
        /* somehow there is garbage at the beginning of some tracks */
    }
    else if (read_32bitBE(riff_off+0x0c,streamFile) == 0x666D7420) { /* "fmt " (rest) */
        //if (read_32bitLE(riff_off+0x10,streamFile) != 0x12) goto fail; /* 0x10 in some */
        if (read_16bitLE(riff_off+0x14,streamFile) != 0x01) goto fail; /* PCM (actually MS-ADPCM) */
        if (read_16bitLE(riff_off+0x20,streamFile) != 4 ||
            read_16bitLE(riff_off+0x22,streamFile) != 16) goto fail; /* 16-bit */

        channel_count = read_16bitLE(riff_off+0x16,streamFile);
        sample_rate = read_32bitLE(riff_off+0x18,streamFile);
        if (read_32bitBE(riff_off+0x24,streamFile) == 0x64617461) { /* "data" size except in some Zwei!! */
            pcm_size = read_32bitLE(riff_off+0x28,streamFile);
        } else {
            pcm_size = read_32bitLE(riff_off+0x04,streamFile) - 0x24;
        }
    }
    else {
        goto fail;
    }

    if (channel_count != 2)
        goto fail;

    start_offset = riff_off + 0x2c;
    loop_flag = get_falcom_looping(streamFile, &loop_start, &loop_end);


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

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* Falcom loves loop points in external text files, here we parse them */
typedef enum { XANADU_NEXT, ZWEI, DINOSAUR_RESURRECTION, GURUMIN } falcom_loop_t;
static int get_falcom_looping(STREAMFILE *streamFile, int *out_loop_start, int *out_loop_end) {
    STREAMFILE *streamText;
    off_t txt_offset = 0x00;
    falcom_loop_t type;
    int loop_start = 0, loop_end = 0, loop_flag = 0;
    char filename[TXT_LINE_MAX];


    /* try one of the many loop files */
    if ((streamText = open_streamfile_by_filename(streamFile,"bgm.tbl")) != NULL) {
        type = XANADU_NEXT;
    }
    else if ((streamText = open_streamfile_by_filename(streamFile,"bgm.scr")) != NULL) {
        type = ZWEI;
    }
    else if ((streamText = open_streamfile_by_filename(streamFile,"loop.txt")) != NULL) { /* actual name in Shift JIS, 0x838B815B8376 */
        type = DINOSAUR_RESURRECTION;
    }
    else if ((streamText = open_streamfile_by_filename(streamFile,"map.itm")) != NULL) {
        type = GURUMIN;
    }
    else {
        goto end;
    }

    get_streamfile_filename(streamFile,filename,TXT_LINE_MAX);

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
