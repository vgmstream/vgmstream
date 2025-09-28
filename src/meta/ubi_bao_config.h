#ifndef _UBI_BAO_CONFIG_H_
#define _UBI_BAO_CONFIG_H_

#include "../streamtypes.h"
#include "../streamfile.h"


typedef enum { FILE_NONE = 0, FILE_FORGE, FILE_FORGE_b, FILE_FAT, FILE_DUNIA_CRC64 } ubi_bao_file_t;
typedef enum { ARCHIVE_NONE = 0, ARCHIVE_ATOMIC, ARCHIVE_PK, ARCHIVE_SPK } ubi_bao_archive_t;
typedef enum { TYPE_NONE = 0, TYPE_AUDIO, TYPE_LAYER, TYPE_SEQUENCE, TYPE_SILENCE } ubi_bao_type_t;
typedef enum { CODEC_NONE = 0, UBI_IMA, RAW_PCM, RAW_PSX, RAW_XMA1, RAW_XMA2_OLD, RAW_XMA2_NEW, RAW_AT3, RAW_AT3_105, FMT_AT3, RAW_DSP, FMT_OGG } ubi_bao_codec_t;

// config and offset for each field (since they move around depending on version)
typedef struct {
    bool big_endian;
    bool allowed_types[16];
    uint32_t version;

    off_t bao_class;
    size_t header_base_size;
    size_t header_skip;

    bool header_less_le_flag; // horrid but not sure what to do

    off_t header_id;
    off_t header_type;

    off_t audio_stream_size;
    off_t audio_stream_id;
    off_t audio_external_flag;
    off_t audio_loop_flag;
    off_t audio_channels;
    off_t audio_sample_rate;
    off_t audio_num_samples;
    off_t audio_num_samples2;
    off_t audio_stream_type;
    off_t audio_prefetch_size;
    size_t audio_interleave;
    off_t audio_cue_count;
    off_t audio_cue_size;
    bool audio_ignore_resource_size;
    bool audio_fix_psx_samples;
    int audio_external_and;
    int audio_loop_and;

    off_t sequence_sequence_loop;
    off_t sequence_sequence_single;
    off_t sequence_sequence_count;
    off_t sequence_entry_number;
    size_t sequence_entry_size;

    off_t layer_layer_count;
    off_t layer_external_flag;
    off_t layer_stream_id;
    off_t layer_stream_size;
    off_t layer_prefetch_size;
    off_t layer_extra_size;
    off_t layer_cue_count;
    off_t layer_cue_labels;
    off_t layer_sample_rate;
    off_t layer_channels;
    off_t layer_stream_type;
    off_t layer_num_samples;
    size_t layer_entry_size;
    bool layer_external_and;
    bool layer_ignore_error;

    off_t silence_duration_float;

    ubi_bao_codec_t codec_map[16];
    ubi_bao_file_t file;

} ubi_bao_config_t;


bool ubi_bao_config_version(ubi_bao_config_t* cfg, STREAMFILE* sf, uint32_t version);
void ubi_bao_config_endian(ubi_bao_config_t* cfg, STREAMFILE* sf, off_t offset);

#endif
