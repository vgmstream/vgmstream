#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"


#define TXT_LINE_MAX 0x2000

typedef struct {
    char filename[TXT_LINE_MAX];
    int subsong;
    uint32_t channel_mask;
} txtp_entry;

typedef struct {
    txtp_entry *entry;
    size_t entry_count;
    size_t entry_max;

    size_t loop_start_segment;
    size_t loop_end_segment;
} txtp_header;

static txtp_header* parse_txtp(STREAMFILE* streamFile);
static void clean_txtp(txtp_header* txtp);


/* TXTP - an artificial playlist-like format to play segmented files with config */
VGMSTREAM * init_vgmstream_txtp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    txtp_header* txtp = NULL;
    segmented_layout_data *data = NULL;


    /* checks */
    if (!check_extensions(streamFile, "txtp"))
        goto fail;

    /* read .txtp text file to get segments */
    txtp = parse_txtp(streamFile);
    if (!txtp) goto fail;


    if (txtp->entry_count == 0)
        goto fail;


    if (txtp->entry_count == 1 && !txtp->loop_start_segment) {
        /* single file */
        STREAMFILE* temp_streamFile = open_streamfile_by_filename(streamFile, txtp->entry[0].filename);
        if (!temp_streamFile) goto fail;
        temp_streamFile->stream_index = txtp->entry[0].subsong;

        vgmstream = init_vgmstream_from_STREAMFILE(temp_streamFile);
        close_streamfile(temp_streamFile);
        if (!vgmstream) goto fail;

        vgmstream->channel_mask = txtp->entry[0].channel_mask;
    }
    else {
        /* multi file */
        int num_samples, loop_start_sample = 0, loop_end_sample = 0;
        int i;
        int loop_flag, channel_count;


        /* init layout */
        data = init_layout_segmented(txtp->entry_count);
        if (!data) goto fail;

        /* open each segment subfile */
        for (i = 0; i < txtp->entry_count; i++) {
            STREAMFILE* temp_streamFile = open_streamfile_by_filename(streamFile, txtp->entry[i].filename);
            if (!temp_streamFile) goto fail;
            temp_streamFile->stream_index = txtp->entry[i].subsong;

            data->segments[i] = init_vgmstream_from_STREAMFILE(temp_streamFile);
            close_streamfile(temp_streamFile);
            if (!data->segments[i]) goto fail;

            data->segments[i]->channel_mask = txtp->entry[0].channel_mask;
        }

        /* setup segmented VGMSTREAMs */
        if (!setup_layout_segmented(data))
            goto fail;

        /* get looping and samples */
        if (txtp->loop_start_segment && !txtp->loop_end_segment)
            txtp->loop_end_segment = txtp->entry_count;
        loop_flag = (txtp->loop_start_segment > 0 && txtp->loop_start_segment <= txtp->entry_count);
        num_samples = 0;
        for (i = 0; i < data->segment_count; i++) {

            if (loop_flag && txtp->loop_start_segment == i+1) {
                loop_start_sample = num_samples;
            }

            num_samples += data->segments[i]->num_samples;

            if (loop_flag && txtp->loop_end_segment == i+1) {
                loop_end_sample = num_samples;
            }
        }

        channel_count = data->segments[0]->channels;

        /* build the VGMSTREAM */
        vgmstream = allocate_vgmstream(channel_count,loop_flag);
        if (!vgmstream) goto fail;

        vgmstream->sample_rate = data->segments[0]->sample_rate;
        vgmstream->num_samples = num_samples;
        vgmstream->loop_start_sample = loop_start_sample;
        vgmstream->loop_end_sample = loop_end_sample;

        vgmstream->meta_type = meta_TXTP;
        vgmstream->coding_type = data->segments[0]->coding_type;
        vgmstream->layout_type = layout_segmented;

        vgmstream->layout_data = data;
        if (loop_flag)
            data->loop_segment = txtp->loop_start_segment-1;
    }


    clean_txtp(txtp);
    return vgmstream;

fail:
    clean_txtp(txtp);
    close_vgmstream(vgmstream);
    free_layout_segmented(data);
    return NULL;
}


static int add_filename(txtp_header * txtp, char *filename) {
    int i;
    uint32_t channel_mask = 0;
    size_t range_start, range_end;

    //;VGM_LOG("TXTP: filename=%s\n", filename);

    /* parse config:
     * - file.ext#2 = play subsong 2
     * - file.ext#2~10 = play subsongs in 2 to 10 range
     * - file.ext#c1,2 = play channels 1,2 */
    {
        char *config;

        /* position in base extension */
        config = strrchr(filename,'.');
        if (!config) /* needed...? */
            config = filename;

        range_start = 0;
        range_end = 1;
        do {
            /* get config pointer but remove config from filename */
            config = strchr(config, '#');
            if (!config)
                continue;

            config[0] = '\0';
            config++;


            if (config[0] == 'c') {
                /* mask channels */
                int n, ch;

                config++;
                channel_mask = 0;
                while (sscanf(config, "%d%n", &ch,&n) == 1) {
                    if (ch > 0 && ch < 32)
                        channel_mask |= (1 << (ch-1));

                    config += n;
                    if (config[0]== ',' || config[0]== '-') /* "-" for PowerShell, may have problems with "," */
                        config++;
                    else if (config[0] != '\0')
                        break;
                };
            }
            else {
                /* subsong range */
                int subsong_start = 0, subsong_end = 0;

                if (sscanf(config, "%d~%d", &subsong_start, &subsong_end) == 2) {
                    if (subsong_start > 0 && subsong_end > 0) {
                        range_start = subsong_start-1;
                        range_end = subsong_end-1;
                    }
                }
                else if (sscanf(config, "%u", &subsong_start) == 1) {
                    if (subsong_start > 0) {
                        range_start = subsong_start-1;
                        range_end = subsong_start;
                    }
                }
                else {
                    config = NULL; /* wrong config, ignore */
                }
            }

        } while (config != NULL);

        //;VGM_LOG("TXTP: config: range %i~%i, mask=%x\n", range_start, range_end, channel_mask);
    }


    /* hack to allow relative paths in various OSs */
    {
        char c;

        i = 0;
        while ((c = filename[i]) != '\0') {
            if ((c == '\\' && DIR_SEPARATOR == '/') || (c == '/' && DIR_SEPARATOR == '\\'))
                filename[i] = DIR_SEPARATOR;
            i++;
        }
    }


    /* add filesnames */
    for (i = range_start; i < range_end; i++){
        /* resize in steps if not enough */
        if (txtp->entry_count+1 > txtp->entry_max) {
            txtp_entry *temp_entry;

            txtp->entry_max += 5;
            temp_entry = realloc(txtp->entry, sizeof(txtp_entry) * txtp->entry_max);
            if (!temp_entry) goto fail;
            txtp->entry = temp_entry;
        }

        /* new entry */
        memset(&txtp->entry[txtp->entry_count],0, sizeof(txtp_entry));

        strcpy(txtp->entry[txtp->entry_count].filename, filename);
        txtp->entry[txtp->entry_count].channel_mask = channel_mask;
        txtp->entry[txtp->entry_count].subsong = (i+1);
        txtp->entry_count++;
    }

    return 1;
fail:
    return 0;
}

static int parse_num(const char * val, uint32_t * out_value) {
    int hex = (val[0]=='0' && val[1]=='x');
    if (sscanf(val, hex ? "%x" : "%u", out_value)!=1)
        goto fail;

    return 1;
fail:
    return 0;
}

static int parse_keyval(txtp_header * txtp, const char * key, const char * val) {
    //;VGM_LOG("TXTP: key %s = val %s\n", key,val);

    if (0==strcmp(key,"loop_start_segment")) {
        if (!parse_num(val, &txtp->loop_start_segment)) goto fail;
    }
    else if (0==strcmp(key,"loop_end_segment")) {
        if (!parse_num(val, &txtp->loop_end_segment)) goto fail;
    }
    else {
        VGM_LOG("TXTP: unknown key=%s, val=%s\n", key,val);
        goto fail;
    }

    return 1;
fail:
    return 0;
}

static txtp_header* parse_txtp(STREAMFILE* streamFile) {
    txtp_header* txtp = NULL;
    off_t txt_offset = 0x00;
    off_t file_size = get_streamfile_size(streamFile);


    txtp = calloc(1,sizeof(txtp_header));
    if (!txtp) goto fail;


    /* empty file: use filename with config (ex. "song.ext#3.txtp") */
    if (get_streamfile_size(streamFile) == 0) {
        char filename[PATH_LIMIT] = {0};
        char* ext;
        get_streamfile_filename(streamFile, filename,PATH_LIMIT);

        /* remove ".txtp" */
        ext = strrchr(filename,'.');
        if (!ext) goto fail; /* ??? */
        ext[0] = '\0';

        if (!add_filename(txtp, filename))
            goto fail;

        return txtp;
    }


    /* skip BOM if needed */
    if (read_16bitLE(0x00, streamFile) == 0xFFFE || read_16bitLE(0x00, streamFile) == 0xFEFF)
        txt_offset = 0x02;

    /* read lines */
    while (txt_offset < file_size) {
        char line[TXT_LINE_MAX] = {0};
        char key[TXT_LINE_MAX] = {0}, val[TXT_LINE_MAX] = {0}; /* at least as big as a line to avoid overflows (I hope) */
        char filename[TXT_LINE_MAX] = {0};
        int ok, bytes_read, line_done;

        bytes_read = get_streamfile_text_line(TXT_LINE_MAX,line, txt_offset,streamFile, &line_done);
        if (!line_done) goto fail;

        txt_offset += bytes_read;

        /* get key/val (ignores lead/trail spaces, stops at space/comment/separator) */
        ok = sscanf(line, " %[^ \t#=] = %[^ \t#\r\n] ", key,val);
        if (ok == 2) { /* no key=val */
            if (!parse_keyval(txtp, key, val)) /* read key/val */
                goto fail;
            continue;
        }

        /* must be a filename (only remove spaces from start/end, as filenames con contain mid spaces/#/etc) */
        ok = sscanf(line, " %[^\t\r\n] ", filename);
        if (ok != 1) /* not a filename either */
            continue;
        if (filename[0] == '#')
            continue; /* simple comment */

        /* filename with config */
        if (!add_filename(txtp, filename))
            goto fail;
    }


    return txtp;
fail:
    clean_txtp(txtp);
    return NULL;
}

static void clean_txtp(txtp_header* txtp) {
    if (!txtp)
        return;

    free(txtp->entry);
    free(txtp);
}
