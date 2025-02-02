#include "meta.h"
#include "../coding/coding.h"
#include <string.h>
#include <ctype.h>


static int get_loops_nwainfo_ini(STREAMFILE *sf, int *p_loop_flag, int32_t *p_loop_start);
static int get_loops_gameexe_ini(STREAMFILE *sf, int *p_loop_flag, int32_t *p_loop_start, int32_t *p_loop_end);

/* .NWA - Visual Art's streams [Air (PC), Clannad (PC)] */
VGMSTREAM* init_vgmstream_nwa(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int channels, sample_rate, bps, loop_flag = 0;
    int32_t loop_start_sample = 0, loop_end_sample = 0;
    bool nwainfo_ini_found = false, gameexe_ini_found = false;
    int compression_level;


    /* checks */
    if (!check_extensions(sf, "nwa"))
        return NULL;

    channels = read_s16le(0x00,sf);
    bps = read_s16le(0x02,sf);
    sample_rate = read_s32le(0x04,sf);
    if (channels != 1 && channels != 2)
        return NULL;
    if (bps != 0 && bps != 8 && bps != 16)
        return NULL;
    if (sample_rate < 8000 || sample_rate > 48000) //unsure if can go below 44100
        return NULL;

    /* check if we're using raw pcm */
    if ( read_s32le(0x08,sf) == -1 || /* compression level */
         read_s32le(0x10,sf) == 0  || /* block count */
         read_s32le(0x18,sf) == 0  || /* compressed data size */
         read_s32le(0x20,sf) == 0  || /* block size */
         read_s32le(0x24,sf) == 0 ) { /* restsize */
        compression_level = -1;
    }
    else {
        compression_level = read_s32le(0x08,sf);
    }

    if (compression_level > 5)
        return NULL;

    /* loop points come from external files */
    nwainfo_ini_found = get_loops_nwainfo_ini(sf, &loop_flag, &loop_start_sample);
    gameexe_ini_found = !nwainfo_ini_found && get_loops_gameexe_ini(sf, &loop_flag, &loop_start_sample, &loop_end_sample);

    start_offset = 0x2c;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32le(0x04,sf);
    vgmstream->num_samples = read_s32le(0x1c,sf) / channels;

    switch(compression_level) {
        case -1:
            switch (bps) {
                case 8:
                    vgmstream->coding_type = coding_PCM8;
                    vgmstream->interleave_block_size = 0x01;
                    break;
                case 16:
                    vgmstream->coding_type = coding_PCM16LE;
                    vgmstream->interleave_block_size = 0x02;
                    break;
                default:
                    goto fail;
            }
            vgmstream->layout_type = layout_interleave;
            break;

        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            vgmstream->coding_type = coding_NWA;
            vgmstream->layout_type = layout_none;
            vgmstream->codec_data = init_nwa(sf);
            if (!vgmstream->codec_data) goto fail;
            break;

        default:
            goto fail;
            break;
    }


    if (nwainfo_ini_found) {
        vgmstream->meta_type = meta_NWA_NWAINFOINI;
        if (loop_flag) {
            vgmstream->loop_start_sample = loop_start_sample;
            vgmstream->loop_end_sample = vgmstream->num_samples;
        }
    }
    else if (gameexe_ini_found) {
        vgmstream->meta_type = meta_NWA_GAMEEXEINI;
        if (loop_flag) {
            vgmstream->loop_start_sample = loop_start_sample;
            vgmstream->loop_end_sample = loop_end_sample;
        }
    }
    else {
        vgmstream->meta_type = meta_NWA;
    }


    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* try to locate NWAINFO.INI in the same directory */
static int get_loops_nwainfo_ini(STREAMFILE *sf, int *p_loop_flag, int32_t *p_loop_start) {
    STREAMFILE *sf_loop;
    char namebase[PATH_LIMIT];
    const char * ext;
    int length;
    int found;
    off_t offset;
    size_t file_size;
    off_t found_off = -1;
    int loop_flag = 0;
    int32_t loop_start_sample = 0;


    sf_loop = open_streamfile_by_filename(sf, "NWAINFO.INI");
    if (!sf_loop) goto fail;

    get_streamfile_filename(sf,namebase,PATH_LIMIT);

    /* ini found, try to find our name */
    ext = filename_extension(namebase);
    length = ext - 1 - namebase;
    file_size = get_streamfile_size(sf_loop);

    for (found = 0, offset = 0; !found && offset < file_size; offset++) {
        off_t suboffset;
        /* Go for an n*m search 'cause it's easier than building an
         * FSA for the search string. Just wanted to make the point that
         * I'm not ignorant, just lazy. */
        for (suboffset = offset;
                suboffset < file_size &&
                suboffset-offset < length &&
                read_8bit(suboffset,sf_loop) == namebase[suboffset-offset];
                suboffset++) {
            /* skip */
        }

        if (suboffset-offset==length && read_8bit(suboffset,sf_loop)==0x09) { /* tab */
            found = 1;
            found_off = suboffset+1;
        }
    }

    /* if found file name in INI */
    if (found) {
        char loopstring[9] = {0};

        if (read_streamfile((uint8_t*)loopstring,found_off,8,sf_loop) == 8) {
            loop_start_sample = atol(loopstring);
            if (loop_start_sample > 0)
                loop_flag = 1;
        }
    }


    *p_loop_flag = loop_flag;
    *p_loop_start = loop_start_sample;

    close_streamfile(sf_loop);
    return 1;

fail:
    close_streamfile(sf_loop);
    return 0;
}

/* try to locate Gameexe.ini in the same directory */
static int get_loops_gameexe_ini(STREAMFILE* sf, int* p_loop_flag, int32_t* p_loop_start, int32_t* p_loop_end) {
    STREAMFILE*sf_loop;
    char namebase[PATH_LIMIT];
    const char* ext;
    int length;
    int found;
    off_t offset;
    off_t file_size;
    off_t found_off = -1;
    int loop_flag = 0;
    int32_t loop_start_sample = 0, loop_end_sample = 0;


    sf_loop = open_streamfile_by_filename(sf, "Gameexe.ini");
    if (!sf_loop) goto fail;

    get_streamfile_filename(sf,namebase,PATH_LIMIT);

    /* ini found, try to find our name */
    ext = filename_extension(namebase);
    length = ext-1-namebase;
    file_size = get_streamfile_size(sf_loop);

    /* According to the official documentation of RealLiveMax (the public version of RealLive), format of line is:
     * #DSTRACK = 00000000 - eeeeeeee - ssssssss = "filename" = "alias for game script"
     *                       ^22        ^33         ^45          ^57?
     *
     * Original text from the documentation (written in Japanese) is:
     * ; ■ＢＧＭの登録：ＤｉｒｅｃｔＳｏｕｎｄ
     * ;（※必要ない場合は登録しないで下さい。）
     * ;   終了位置の設定が 99999999 なら最後まで演奏します。
     * ;   ※設定値はサンプル数で指定して下さい。（旧システムではバイト指定でしたので注意してください。）
     * ;=========================================================================================================
     * ;          開始位置 - 終了位置 - リピート = ﾌｧｲﾙ名     = 登録名
     * #DSTRACK = 00000000 - 01896330 - 00088270 = "b_manuke"           = "b_manuke"
     * #DSTRACK = 00000000 - 01918487 - 00132385 = "c_happy"            = "c_happy"
     */

    for (found = 0, offset = 0; !found && offset<file_size; offset++) {
        off_t suboffset;
        uint8_t buf[10];

        if (read_8bit(offset,sf_loop)!='#') continue;
        if (read_streamfile(buf,offset+1,10,sf_loop)!=10) break;
        if (memcmp("DSTRACK = ",buf,10)) continue;
        if (read_8bit(offset+44,sf_loop)!='\"') continue;

        for (suboffset = offset+45;
                suboffset < file_size &&
                suboffset-offset-45 < length &&
                tolower(read_8bit(suboffset,sf_loop)) == tolower(namebase[suboffset-offset-45]);
                suboffset++) {
            /* skip */
        }

        if (suboffset-offset-45==length && read_8bit(suboffset,sf_loop)=='\"') { /* tab */
            found = 1;
            found_off = offset+22; /* loop end */
        }
    }

    if (found) {
        char loopstring[9] = {0};
        int start_ok = 0, end_ok = 0;
        int32_t total_samples = read_32bitLE(0x1c,sf) / read_16bitLE(0x00,sf);

        if (read_streamfile((uint8_t*)loopstring,found_off,8,sf_loop)==8)
        {
            if (!memcmp("99999999",loopstring,8)) {
                loop_end_sample = total_samples;
            }
            else {
                loop_end_sample = atol(loopstring);
            }
            end_ok = 1;
        }
        if (read_streamfile((uint8_t*)loopstring,found_off+11,8,sf_loop)==8)
        {
            if (!memcmp("99999999",loopstring,8)) {
                /* not ok to start at last sample,
                 * don't set start_ok flag */
            }
            else if (!memcmp("00000000",loopstring,8)) {
                /* loops from the start aren't really loops */
            }
            else {
                loop_start_sample = atol(loopstring);
                start_ok = 1;
            }
        }

        if (start_ok && end_ok) loop_flag = 1;
    }   /* if found file name in INI */


    *p_loop_flag = loop_flag;
    *p_loop_start = loop_start_sample;
    *p_loop_end = loop_end_sample;

    close_streamfile(sf_loop);
    return 1;

fail:
    close_streamfile(sf_loop);
    return 0;
}
