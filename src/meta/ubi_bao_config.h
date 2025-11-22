#ifndef _UBI_BAO_CONFIG_H_
#define _UBI_BAO_CONFIG_H_

#include "../streamtypes.h"
#include "../streamfile.h"

#define BAO_MAX_TYPES  10
#define BAO_MAX_CODECS  0x10

// affects how BAOs are referenced
typedef enum { ARCHIVE_NONE = 0, ARCHIVE_ATOMIC, ARCHIVE_PK, ARCHIVE_SPK } ubi_bao_archive_t;
// main playable audio (there are others like randoms and chains but aren't that useful)
typedef enum { TYPE_NONE = 0, TYPE_AUDIO, TYPE_SEQUENCE, TYPE_LAYER, TYPE_SILENCE, TYPE_IGNORED } ubi_bao_type_t;
typedef enum { 
  CODEC_NONE = 0, 
  RAW_PCM,
  UBI_IMA, UBI_IMA_seek, UBI_IMA_mark,
  RAW_PSX, RAW_PSX_new,
  RAW_AT3, FMT_AT3,
  RAW_XMA1_mem, RAW_XMA1_str, RAW_XMA2_old, RAW_XMA2_new,
  FMT_OGG,
  RAW_DSP,
  RAW_MP3,
  RAW_AT9,
} ubi_bao_codec_t;

typedef enum { PARSER_1B, PARSER_29 } ubi_bao_parser_t;

// config and offset for each field (since they move around depending on version)
typedef struct {
    ubi_bao_parser_t parser;

    bool big_endian;
    bool allowed_types[BAO_MAX_TYPES];
    uint32_t version;
    int engine_version; // approximate version used, for some feature checks

    ubi_bao_codec_t codec_map[BAO_MAX_CODECS];  // BAO ID > codec
    bool v1_bao;                    // first versions handle some codecs slightly differently

    bool header_less_le_flag; // horrid but not sure what to do

    // location of various fields in the header, since it's fairly inconsistent
    off_t bao_class;
    off_t header_id;
    off_t header_type;
    off_t header_skip;        // where payload starts (header, memory data, stream data, etc)
    off_t header_base_size;   // location of extradata for some codecs (depends on certain fields in the middle, not always accurate)

    off_t audio_stream_size;
    off_t audio_stream_id;
    off_t audio_stream_flag;
    off_t audio_loop_flag;
    off_t audio_channels;
    off_t audio_sample_rate;
    off_t audio_num_samples;
    off_t audio_num_samples2;
    off_t audio_stream_type;
    off_t audio_stream_subtype;
    off_t audio_prefetch_size;
    off_t audio_cue_count;      // total points
    off_t audio_cue_labels;     // size of strings
  //off_t audio_cue_size;       // size of labels + points, but sometimes wrong (X360 vs PS3, garbage field?)
    int audio_stream_and;
    int audio_loop_and;
    bool audio_ignore_external_size;
    bool audio_fix_xma_samples;
    bool audio_fix_xma_memory_baos;

    // layer config within base BAO
    off_t sequence_sequence_loop;
    off_t sequence_sequence_single;
    off_t sequence_sequence_count;
    off_t sequence_entry_number;
    size_t sequence_entry_size;

    // layer config within base BAO
    off_t layer_layer_count;
    off_t layer_stream_flag;
    off_t layer_stream_id;
    off_t layer_stream_size;
    off_t layer_prefetch_size;
    off_t layer_extra_size;
    // between BAO's base size and layer table, sometimes there is a cue table with N strings + N points
    off_t layer_cue_labels;     //size of strings
    off_t layer_cue_count;      //total points
    // layer table of size N after base BAO + cues, format varies slightly 
    size_t layer_entry_size;
    off_t layer_sample_rate;
    off_t layer_channels;
    off_t layer_stream_type;
    off_t layer_num_samples;
    int layer_stream_and;
    int layer_default_subtype;
    bool layer_ignore_error;

    off_t silence_duration_float;

} ubi_bao_config_t;


bool ubi_bao_config_version(ubi_bao_config_t* cfg, STREAMFILE* sf, uint32_t version);
void ubi_bao_config_endian(ubi_bao_config_t* cfg, STREAMFILE* sf, off_t offset);

#endif
