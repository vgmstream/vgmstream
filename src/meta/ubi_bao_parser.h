#ifndef _UBI_BAO_PARSER_H_
#define _UBI_BAO_PARSER_H_

#include "../streamtypes.h"
#include "../streamfile.h"

#include "ubi_bao_config.h"

#define BAO_MAX_LAYER_COUNT 16      // arbitrary max (seen 12 in SC:C)
#define BAO_MAX_CHAIN_COUNT 128     // POP:TFS goes up to ~100

typedef struct {
    int channels;
    int sample_rate;
    int stream_type;
    int num_samples;
    uint32_t extradata_offset;
    uint32_t extradata_size;    
} ubi_bao_layer_t;

typedef struct {
    ubi_bao_archive_t archive;  // source format, which affects how related files are located
    ubi_bao_type_t type;
    ubi_bao_codec_t codec;
    int total_subsongs;

    ubi_bao_config_t cfg;

    /* header info */
    uint32_t header_offset;     // base BAO offset 
    uint32_t header_id;         // current BAO's id
    uint32_t header_type;       // type of audio
    uint32_t header_size;       // normal base size (not counting extra tables)
    uint32_t extra_size;        // extra tables size

    //TODO rename (resource_id/size)
    uint32_t stream_id;         // stream or memory BAO's id
    uint32_t stream_size;       // stream or memory BAO's playable data
    uint32_t prefetch_size;

    uint32_t memory_skip;       // must skip a bit after base memory BAO offset (class 0x30000000), calculated
    uint32_t stream_skip;       // same for stream BAO offset (class 0x50000000)

    bool is_stream;             // stream data (external file) is used for audio, memory data (external or internal) otherwise
    bool is_prefetch;           // memory data is to be joined as part of the stream
    bool is_inline;             // memory data is in the header BAO rather than a separate memory BAO

    /* sound info */
    int loop_flag;
    int num_samples;
    int loop_start;
    int sample_rate;
    int channels;
    int stream_type;
    int stream_subtype;

    int layer_count;
    ubi_bao_layer_t layer[BAO_MAX_LAYER_COUNT];
    int sequence_count;
    uint32_t sequence_chain[BAO_MAX_CHAIN_COUNT];
    int sequence_loop;
    int sequence_single;

    float silence_duration;

    uint32_t inline_offset;
    uint32_t inline_size;

    uint32_t extradata_offset;
    uint32_t extradata_size;


} ubi_bao_header_t;

bool ubi_bao_parse_header(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset);
bool ubi_bao_parse_bao(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset, int target_subsong);

#endif
