#ifndef _VGMSTREAM_CLI_H_
#define _VGMSTREAM_CLI_H_

#include "../src/api.h"
#include "../src/vgmstream.h"

#define CLI_PATH_LIMIT 4096

typedef struct {
    char** infilenames;
    int infilenames_count;
    const char* infilename;

    const char* outfilename_config;
    const char* outfilename;

    int sample_buffer_size;

    // playback config
    double loop_count;
    double fade_time;
    double fade_delay;
    bool ignore_fade;
    bool ignore_loop;
    bool force_loop;
    bool really_force_loop;

    bool play_forever;
    bool play_sdtout;
    bool play_wreckless;

    // subsongs
    int subsong_index;
    int subsong_end;

    // wav config
    bool write_lwav;
    bool write_original_wav;

    // print flags
    bool print_metaonly;
    bool print_adxencd;
    bool print_oggenc;
    bool print_batchvar;
    bool print_title;
    bool print_metajson;
    const char* tag_filename;

    // debug stuff
    bool decode_only;
    bool test_reset;
    bool validate_extensions;
    int seek_samples1;
    int seek_samples2;
    int downmix_channels;
    int stereo_track;


    /* not quite config but eh */
    int lwav_loop_start;
    int lwav_loop_end;
} cli_config_t;


void replace_filename(char* dst, size_t dstsize, cli_config_t* cfg, VGMSTREAM* vgmstream);
void print_info(VGMSTREAM* vgmstream, cli_config_t* cfg);
void print_tags(cli_config_t* cfg);
void print_title(VGMSTREAM* vgmstream, cli_config_t* cfg);

void print_json_version(const char* vgmstream_version);
void print_json_info(VGMSTREAM* vgm, cli_config_t* cfg, const char* vgmstream_version);


#endif
