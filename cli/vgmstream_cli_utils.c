#include "vgmstream_cli.h"

#include "../src/api.h"
#include "../src/vgmstream.h"

//todo use <>?
#ifdef HAVE_JSON
#include "jansson/jansson.h"
#endif


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
    char stream_name[PATH_LIMIT];
    char buf[PATH_LIMIT];
    char tmp[PATH_LIMIT];


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
    if (!cfg->play_sdtout) {
        if (cfg->print_adxencd) {
            printf("adxencd");
            if (!cfg->print_metaonly)
                printf(" \"%s\"",cfg->outfilename);
            if (vgmstream->loop_flag)
                printf(" -lps%d -lpe%d", vgmstream->loop_start_sample, vgmstream->loop_end_sample);
            printf("\n");
        }
        else if (cfg->print_oggenc) {
            printf("oggenc");
            if (!cfg->print_metaonly)
                printf(" \"%s\"", cfg->outfilename);
            if (vgmstream->loop_flag)
                printf(" -c LOOPSTART=%d -c LOOPLENGTH=%d", vgmstream->loop_start_sample, vgmstream->loop_end_sample-vgmstream->loop_start_sample);
            printf("\n");
        }
        else if (cfg->print_batchvar) {
            if (!cfg->print_metaonly)
                printf("set fname=\"%s\"\n", cfg->outfilename);
            printf("set tsamp=%d\nset chan=%d\n", vgmstream->num_samples, channels);
            if (vgmstream->loop_flag)
                printf("set lstart=%d\nset lend=%d\nset loop=1\n", vgmstream->loop_start_sample, vgmstream->loop_end_sample);
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

#ifdef HAVE_JSON
void print_json_version(const char* vgmstream_version) {
    size_t extension_list_len;
    size_t common_extension_list_len;
    const char** extension_list;
    const char** common_extension_list;
    extension_list = vgmstream_get_formats(&extension_list_len);
    common_extension_list = vgmstream_get_common_formats(&common_extension_list_len);

    json_t* ext_list = json_array();
    json_t* cext_list = json_array();

    for (size_t i = 0; i < extension_list_len; ++i) {
        json_t* ext = json_string(extension_list[i]);
        json_array_append(ext_list, ext);
    }

    for (size_t i = 0; i < common_extension_list_len; ++i) {
        json_t* cext = json_string(common_extension_list[i]);
        json_array_append(cext_list, cext);
    }

    json_t* version_string = json_string(vgmstream_version);

    json_t* final_object = json_object();
    json_object_set(final_object, "version", version_string);
    json_decref(version_string);

    json_object_set(final_object, "extensions",
                    json_pack("{soso}",
                              "vgm", ext_list,
                              "common", cext_list));

    json_dumpf(final_object, stdout, JSON_COMPACT);
}

void print_json_info(VGMSTREAM* vgm, cli_config_t* cfg, const char* vgmstream_version) {
    json_t* version_string = json_string(vgmstream_version);
    vgmstream_info info;
    describe_vgmstream_info(vgm, &info);

    json_t* mixing_info = NULL;

    // The JSON pack format string is defined here: https://jansson.readthedocs.io/en/latest/apiref.html#building-values

    if (info.mixing_info.input_channels > 0) {
        mixing_info = json_pack("{sisi}",
            "inputChannels", info.mixing_info.input_channels,
            "outputChannels", info.mixing_info.output_channels);
    }

    json_t* loop_info = NULL;

    if (info.loop_info.end > info.loop_info.start) {
        loop_info = json_pack("{sisi}",
            "start", info.loop_info.start,
            "end", info.loop_info.end);
    }

    json_t* interleave_info = NULL;

    if (info.interleave_info.last_block > info.interleave_info.first_block) {
        interleave_info = json_pack("{sisi}",
            "firstBlock", info.interleave_info.first_block,
            "lastBlock", info.interleave_info.last_block
        );
    }

    json_t* stream_info = json_pack("{sisssi}",
        "index", info.stream_info.current,
        "name", info.stream_info.name,
        "total", info.stream_info.total
    );

    if (info.stream_info.name[0] == '\0') {
        json_object_set(stream_info, "name", json_null());
    }

    json_t* final_object = json_pack(
        "{sssisiso?siso?so?sisssssisssiso?}",
        "version", version_string,
        "sampleRate", info.sample_rate,
        "channels", info.channels,
        "mixingInfo", mixing_info,
        "channelLayout", info.channel_layout,
        "loopingInfo", loop_info,
        "interleaveInfo", interleave_info,
        "numberOfSamples", info.num_samples,
        "encoding", info.encoding,
        "layout", info.layout,
        "frameSize", info.frame_size,
        "metadataSource", info.metadata,
        "bitrate", info.bitrate,
        "streamInfo", stream_info
    );

    if (info.frame_size == 0) {
        json_object_set(final_object, "frameSize", json_null());
    }

    if (info.channel_layout == 0) {
        json_object_set(final_object, "channelLayout", json_null());
    }

    json_dumpf(final_object, stdout, JSON_COMPACT);

    json_decref(final_object);

    printf("\n");
}
#endif
