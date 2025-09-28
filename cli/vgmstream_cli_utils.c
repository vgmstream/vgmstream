#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include "vgmstream_cli.h"
#include "vjson.h"
#include "../src/libvgmstream.h"


static void clean_filename(char* dst, int clean_paths) {
    for (int i = 0; i < strlen(dst); i++) {
        char c = dst[i];
        bool is_badchar = (clean_paths && (c == '\\' || c == '/'))
            || c == '*' || c == '?' || c == ':' /*|| c == '|'*/ || c == '<' || c == '>';
        if (is_badchar)
            dst[i] = '_';
    }
}

/* replaces a filename with "?n" (stream name), "?f" (infilename) or "?s" (subsong) wildcards
 * ("?" was chosen since it's not a valid Windows filename char and hopefully nobody uses it on Linux) */
void replace_filename(char* dst, size_t dstsize, cli_config_t* cfg, libvgmstream_t* vgmstream) {
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
    subsong = vgmstream->format->subsong_index;
    if (subsong > vgmstream->format->subsong_count || subsong != cfg->subsong_current_index) {
        subsong = 0; /* for games without subsongs / bad config */
    }

    if (vgmstream->format->stream_name[0] != '\0') {
        snprintf(stream_name, sizeof(stream_name), "%s", vgmstream->format->stream_name);
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
            // TO-DO: should move buf or swap "?" with "_"? may happen with non-ascii on Windows; for now break to avoid infinite loops
            break;
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


void print_info(libvgmstream_t* vgmstream, cli_config_t* cfg) {
    int channels = vgmstream->format->channels;
    bool loop_flag = vgmstream->format->loop_flag;
    int64_t num_samples = vgmstream->format->stream_samples;
    int64_t loop_start = vgmstream->format->loop_start;
    int64_t loop_end = vgmstream->format->loop_end;

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
        libvgmstream_format_describe(vgmstream, description, sizeof(description));
        printf("%s", description);
    }
}

void print_tags(cli_config_t* cfg) {
    libvgmstream_tags_t* tags = NULL;
    libstreamfile_t* sf_tags = NULL;

    if (!cfg->tag_filename)
        return;

    sf_tags = libstreamfile_open_from_stdio(cfg->tag_filename);
    if (!sf_tags) {
        printf("tag file %s not found\n", cfg->tag_filename);
        return;
    }

    printf("tags:\n");

    tags = libvgmstream_tags_init(sf_tags);
    libvgmstream_tags_find(tags, cfg->infilename);
    while (libvgmstream_tags_next_tag(tags)) {
        printf("- '%s'='%s'\n", tags->key, tags->val);
    }

    libvgmstream_tags_free(tags);
    libstreamfile_close(sf_tags);
}

void print_title(libvgmstream_t* vgmstream, cli_config_t* cfg) {
    char title[1024];
    libvgmstream_title_t tcfg = {0};

    if (!cfg->print_title)
        return;

    tcfg.force_title = false;
    tcfg.subsong_range = false;
    tcfg.remove_extension = true;
    tcfg.filename = cfg->infilename;

    libvgmstream_get_title(vgmstream, &tcfg, title, sizeof(title));

    printf("title: %s\n", title);
}

void print_json_version(const char* vgmstream_version) {
    int extension_list_len = 0;
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
            extension_list = libvgmstream_get_extensions(&extension_list_len);
            for (int i = 0; i < extension_list_len; i++) {
                vjson_str(&j, extension_list[i]);
            }
            vjson_arr_close(&j);

            vjson_key(&j, "common");
            vjson_arr_open(&j);
            extension_list = libvgmstream_get_common_extensions(&extension_list_len);
            for (int i = 0; i < extension_list_len; i++) {
                vjson_str(&j, extension_list[i]);
            }
            vjson_arr_close(&j);

        vjson_obj_close(&j);
    vjson_obj_close(&j);

    printf("%s\n", buf);
}

void print_json_info(libvgmstream_t* v, cli_config_t* cfg, const char* vgmstream_version) {
    char buf[0x1000]; // probably fine with ~0x400
    vjson_t j = {0};
    vjson_init(&j, buf, sizeof(buf));

    vjson_obj_open(&j);
        vjson_keystr(&j, "version", vgmstream_version);
        vjson_keyint(&j, "sampleRate", v->format->sample_rate);
        vjson_keyint(&j, "channels", v->format->channels);

        vjson_key(&j, "mixingInfo");
        if (v->format->input_channels > 0) {
            vjson_obj_open(&j);
                vjson_keyint(&j, "inputChannels", v->format->input_channels);
                vjson_keyint(&j, "outputChannels", v->format->channels);
            vjson_obj_close(&j);
        }
        else {
            vjson_null(&j);
        }

        vjson_keyintnull(&j, "channelLayout", v->format->channel_layout);

        vjson_key(&j, "loopingInfo");
        if (v->format->loop_end > v->format->loop_start) {
            vjson_obj_open(&j);
                vjson_keyint(&j, "start", v->format->loop_start);
                vjson_keyint(&j, "end", v->format->loop_end);
            vjson_obj_close(&j);
        }
        else {
            vjson_null(&j);
        }
#if 0
        vjson_key(&j, "interleaveInfo");
        if (info.interleave_info.last_block || info.interleave_info.first_block) {
            vjson_obj_open(&j);
                vjson_keyint(&j, "firstBlock", info.interleave_info.first_block);
                vjson_keyint(&j, "lastBlock", info.interleave_info.last_block);
            vjson_obj_close(&j);
        }
        else {
            vjson_null(&j);
        }
#endif
        vjson_keyint(&j, "numberOfSamples", v->format->stream_samples);
        vjson_keystr(&j, "encoding", v->format->codec_name);
        vjson_keystr(&j, "layout", v->format->layout_name);
#if 0
        vjson_keyintnull(&j, "frameSize", info.frame_size);
#endif
        vjson_keystr(&j, "metadataSource", v->format->meta_name);
        vjson_keyint(&j, "bitrate", v->format->stream_bitrate);

        vjson_key(&j, "streamInfo");
        vjson_obj_open(&j);
            vjson_keyint(&j, "index", v->format->subsong_index);
            vjson_keystr(&j, "name", v->format->stream_name);
            vjson_keyint(&j, "total", v->format->subsong_count);
        vjson_obj_close(&j);

        vjson_keyint(&j, "playSamples", v->format->play_samples);

    vjson_obj_close(&j);

    printf("%s\n", buf);
}
