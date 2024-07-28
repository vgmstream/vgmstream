#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include "vgmstream_cli.h"
#include "../src/api.h"
#include "../src/vgmstream.h"

#include "vjson.h"


static void clean_filename(char* dst, int clean_paths) {
    for (int i = 0; i < strlen(dst); i++) {
        char c = dst[i];
        int is_badchar = (clean_paths && (c == '\\' || c == '/'))
            || c == '*' || c == '?' || c == ':' /*|| c == '|'*/ || c == '<' || c == '>';
        if (is_badchar)
            dst[i] = '_';
    }
}

/* replaces a filename with "?n" (stream name), "?f" (infilename) or "?s" (subsong) wildcards
 * ("?" was chosen since it's not a valid Windows filename char and hopefully nobody uses it on Linux) */
void replace_filename(char* dst, size_t dstsize, cli_config_t* cfg, VGMSTREAM* vgmstream) {
    int subsong;
    char stream_name[CLI_PATH_LIMIT];
    char buf[CLI_PATH_LIMIT];
    char tmp[CLI_PATH_LIMIT];


    /* file has a "%" > temp replace for sprintf */
    strcpy(buf, cfg->outfilename_config);
    for (int i = 0; i < strlen(buf); i++) {
        if (buf[i] == '%')
            buf[i] = '|'; /* non-valid filename, not used in format */
    }

    /* init config */
    subsong = vgmstream->stream_index;
    if (subsong > vgmstream->num_streams || subsong != cfg->subsong_index) {
        subsong = 0; /* for games without subsongs / bad config */
    }

    if (vgmstream->stream_name[0] != '\0') {
        snprintf(stream_name, sizeof(stream_name), "%s", vgmstream->stream_name);
        clean_filename(stream_name, 1); /* clean subsong name's subdirs */
    }
    else {
        snprintf(stream_name, sizeof(stream_name), "%s", cfg->infilename);
        clean_filename(stream_name, 0); /* don't clean user's subdirs */
    }

    /* do controlled replaces of each wildcard (in theory could appear N times) */
    do {
        char* pos = strchr(buf, '?');
        if (!pos)
            break;

        /* use buf as format and copy formatted result to tmp (assuming sprintf's format must not overlap with dst) */
        if (pos[1] == 'n') {
            pos[0] = '%';
            pos[1] = 's'; /* use %s */
            snprintf(tmp, sizeof(tmp), buf, stream_name);
        }
        else if (pos[1] == 'f') {
            pos[0] = '%';
            pos[1] = 's'; /* use %s */
            snprintf(tmp, sizeof(tmp), buf, cfg->infilename);
        }
        else if (pos[1] == 's') {
            pos[0] = '%';
            pos[1] = 'i'; /* use %i */
            snprintf(tmp, sizeof(tmp), buf, subsong);
        }
        else if ((pos[1] == '0' && pos[2] >= '1' && pos[2] <= '9' && pos[3] == 's')) {
            pos[0] = '%';
            pos[3] = 'i'; /* use %0Ni */
            snprintf(tmp, sizeof(tmp), buf, subsong);
        }
        else {
            /* not recognized */
            continue;
        }

        /* copy result to buf again, so it can be used as format in next replace
         * (can be optimized with some pointer swapping but who cares about a few extra nanoseconds) */
        strcpy(buf, tmp);
    }
    while (1);

    /* replace % back */
    for (int i = 0; i < strlen(buf); i++) {
        if (buf[i] == '|')
            buf[i] = '%';
    }

    snprintf(dst, dstsize, "%s", buf);
}


void print_info(VGMSTREAM* vgmstream, cli_config_t* cfg) {
    int channels = vgmstream->channels;
    int64_t num_samples = vgmstream->num_samples;
    bool loop_flag = vgmstream->loop_flag;
    int64_t loop_start = vgmstream->loop_start_sample;
    int64_t loop_end = vgmstream->loop_start_sample;

    if (!cfg->play_sdtout) {
        if (cfg->print_adxencd) {
            printf("adxencd");
            if (!cfg->print_metaonly)
                printf(" \"%s\"", cfg->outfilename);
            if (loop_flag)
                printf(" -lps%"PRId64" -lpe%"PRId64, loop_start, loop_end);
            printf("\n");
        }
        else if (cfg->print_oggenc) {
            printf("oggenc");
            if (!cfg->print_metaonly)
                printf(" \"%s\"", cfg->outfilename);
            if (loop_flag)
                printf(" -c LOOPSTART=%"PRId64" -c LOOPLENGTH=%"PRId64, loop_start, loop_end - loop_start);
            printf("\n");
        }
        else if (cfg->print_batchvar) {
            if (!cfg->print_metaonly)
                printf("set fname=\"%s\"\n", cfg->outfilename);
            printf("set tsamp=%"PRId64"\nset chan=%d\n", num_samples, channels);
            if (loop_flag)
                printf("set lstart=%"PRId64"\nset lend=%"PRId64"\nset loop=1\n", loop_start, loop_end);
            else
                printf("set loop=0\n");
        }
        else if (cfg->print_metaonly) {
            printf("metadata for %s\n", cfg->infilename);
        }
        else {
            printf("decoding %s\n", cfg->infilename);
        }
    }

    if (!cfg->play_sdtout && !cfg->print_adxencd && !cfg->print_oggenc && !cfg->print_batchvar) {
        char description[1024];
        describe_vgmstream(vgmstream, description, 1024);
        printf("%s", description);
    }
}

void print_tags(cli_config_t* cfg) {
    VGMSTREAM_TAGS* tags = NULL;
    STREAMFILE* sf_tags = NULL;
    const char *tag_key, *tag_val;

    if (!cfg->tag_filename)
        return;

    sf_tags = open_stdio_streamfile(cfg->tag_filename);
    if (!sf_tags) {
        printf("tag file %s not found\n", cfg->tag_filename);
        return;
    }

    printf("tags:\n");

    tags = vgmstream_tags_init(&tag_key, &tag_val);
    vgmstream_tags_reset(tags, cfg->infilename);
    while (vgmstream_tags_next_tag(tags, sf_tags)) {
        printf("- '%s'='%s'\n", tag_key, tag_val);
    }

    vgmstream_tags_close(tags);
    close_streamfile(sf_tags);
}

void print_title(VGMSTREAM* vgmstream, cli_config_t* cfg) {
    char title[1024];
    vgmstream_title_t tcfg = {0};

    if (!cfg->print_title)
        return;

    tcfg.force_title = 0;
    tcfg.subsong_range = 0;
    tcfg.remove_extension = 0;

    vgmstream_get_title(title, sizeof(title), cfg->infilename, vgmstream, &tcfg);

    printf("title: %s\n", title);
}

void print_json_version(const char* vgmstream_version) {
    size_t extension_list_len = 0;
    const char** extension_list;

    vjson_t j = {0};

    char buf[0x4000]; // exts need ~0x1400
    vjson_init(&j, buf, sizeof(buf));

    vjson_obj_open(&j);
        vjson_keystr(&j, "version", vgmstream_version);

        vjson_key(&j, "extensions");
        vjson_obj_open(&j);

            vjson_key(&j, "vgm");
            vjson_arr_open(&j);
            extension_list = vgmstream_get_formats(&extension_list_len);
            for (int i = 0; i < extension_list_len; i++) {
                vjson_str(&j, extension_list[i]);
            }
            vjson_arr_close(&j);

            vjson_key(&j, "common");
            vjson_arr_open(&j);
            extension_list = vgmstream_get_common_formats(&extension_list_len);
            for (int i = 0; i < extension_list_len; i++) {
                vjson_str(&j, extension_list[i]);
            }
            vjson_arr_close(&j);

        vjson_obj_close(&j);
    vjson_obj_close(&j);

    printf("%s\n", buf);
}

void print_json_info(VGMSTREAM* vgm, cli_config_t* cfg, const char* vgmstream_version) {
    char buf[0x1000]; // probably fine with ~0x400
    vjson_t j = {0};
    vjson_init(&j, buf, sizeof(buf));

    vgmstream_info info;
    describe_vgmstream_info(vgm, &info);
    
    vjson_obj_open(&j);
        vjson_keystr(&j, "version", vgmstream_version);
        vjson_keyint(&j, "sampleRate", info.sample_rate);
        vjson_keyint(&j, "channels", info.channels);

        vjson_key(&j, "mixingInfo");
        if (info.mixing_info.input_channels > 0) {
            vjson_obj_open(&j);
                vjson_keyint(&j, "inputChannels", info.mixing_info.input_channels);
                vjson_keyint(&j, "outputChannels", info.mixing_info.output_channels);
            vjson_obj_close(&j);
        }
        else {
            vjson_null(&j);
        }

        vjson_keyintnull(&j, "channelLayout", info.channel_layout);

        vjson_key(&j, "loopingInfo");
        if (info.loop_info.end > info.loop_info.start) {
            vjson_obj_open(&j);
                vjson_keyint(&j, "start", info.loop_info.start);
                vjson_keyint(&j, "end", info.loop_info.start);
            vjson_obj_close(&j);
        }
        else {
            vjson_null(&j);
        }

        vjson_key(&j, "interleaveInfo");
        if (info.interleave_info.last_block > info.interleave_info.first_block) {
            vjson_obj_open(&j);
                vjson_keyint(&j, "firstBlock", info.interleave_info.last_block);
                vjson_keyint(&j, "lastBlock", info.interleave_info.first_block);
            vjson_obj_close(&j);
        }
        else {
            vjson_null(&j);
        }

        vjson_keyint(&j, "numberOfSamples", info.num_samples);
        vjson_keystr(&j, "encoding", info.encoding);
        vjson_keystr(&j, "layout", info.layout);
        vjson_keyintnull(&j, "frameSize", info.frame_size);
        vjson_keystr(&j, "metadataSource", info.metadata);
        vjson_keyint(&j, "bitrate", info.bitrate);

        vjson_key(&j, "streamInfo");
        vjson_obj_open(&j);
            vjson_keyint(&j, "index", info.stream_info.current);
            vjson_keystr(&j, "name", info.stream_info.name);
            vjson_keyint(&j, "total", info.stream_info.total);
        vjson_obj_close(&j);

    vjson_obj_close(&j);

    printf("%s\n", buf);
}
