#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "ubi_bao_streamfile.h"

#define BAO_MIN_VERSION 0x1B
#define BAO_MAX_VERSION 0x2A

#define BAO_MAX_LAYER_COUNT 16  /* arbitrary max */
#define BAO_MAX_CHAIN_COUNT 128 /* POP:TFS goes up to ~100 */

typedef enum { CODEC_NONE = 0, UBI_IMA, RAW_PCM, RAW_PSX, RAW_XMA1, RAW_XMA2_OLD, RAW_XMA2_NEW, RAW_AT3, RAW_AT3_105, FMT_AT3, RAW_DSP, FMT_OGG } ubi_bao_codec;
typedef enum { TYPE_NONE = 0, UBI_AUDIO, UBI_LAYER, UBI_SEQUENCE, UBI_SILENCE } ubi_bao_type;
typedef enum { FILE_NONE = 0, UBI_FORGE, UBI_FORGE_b, UBI_FAT } ubi_bao_file;

typedef struct {
    size_t bao_class;
    size_t header_base_size;
    size_t header_skip;

    int header_less_le_flag; /* horrid but not sure what to do */

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
    int audio_fix_psx_samples;
    int audio_external_and;
    int audio_loop_and;
    int audio_ignore_resource_size;

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
    int layer_external_and;
    int layer_ignore_error;

    off_t silence_duration_float;

    ubi_bao_codec codec_map[16];
    ubi_bao_file file_type;

} ubi_bao_config;

typedef struct {
    int is_atomic;

    int version;
    ubi_bao_type type;
    ubi_bao_codec codec;
    int big_endian;
    int total_subsongs;

    /* config */
    ubi_bao_config cfg;

    /* header info */
    uint32_t header_offset;
    uint8_t header_format;
    uint32_t header_version;
    uint32_t header_id;
    uint32_t header_type;
    uint32_t header_skip;     /* common sub-header size */
    uint32_t header_size;     /* normal base size (not counting extra tables) */
    uint32_t extra_size;      /* extra tables size */

    uint32_t stream_id;
    uint32_t stream_size;
    uint32_t stream_offset;
    uint32_t prefetch_id;
    uint32_t prefetch_size;
    uint32_t prefetch_offset;

    size_t memory_skip;
    size_t stream_skip;

    int is_prefetched;
    int is_external;

    int loop_flag;
    int num_samples;
    int loop_start;
    int sample_rate;
    int channels;
    int stream_type;

    int layer_count;
    int layer_channels[BAO_MAX_LAYER_COUNT];
    int sequence_count;
    uint32_t sequence_chain[BAO_MAX_CHAIN_COUNT];
    int sequence_loop;
    int sequence_single;

    float duration;

    char resource_name[255];

    char readable_name[255];
    int classes[16];
    int types[16];
    int allowed_types[16];
} ubi_bao_header;

static int parse_header(ubi_bao_header* bao, STREAMFILE* sf, off_t offset);
static int parse_bao(ubi_bao_header* bao, STREAMFILE* sf, off_t offset, int target_subsong);
static int parse_pk(ubi_bao_header* bao, STREAMFILE* sf);
static VGMSTREAM* init_vgmstream_ubi_bao_header(ubi_bao_header* bao, STREAMFILE* sf);
static STREAMFILE* setup_bao_streamfile(ubi_bao_header* bao, STREAMFILE* sf);
static STREAMFILE* open_atomic_bao(ubi_bao_file file_type, uint32_t file_id, int is_stream, STREAMFILE* sf);
static int find_package_bao(uint32_t target_id, STREAMFILE* sf, off_t* p_offset, size_t* p_size);

static int config_bao_version(ubi_bao_header* bao, STREAMFILE* sf);
static void config_bao_endian(ubi_bao_header* bao, off_t offset, STREAMFILE* sf);
static void build_readable_name(char* buf, size_t buf_size, ubi_bao_header* bao);


/* .PK - packages with BAOs from Ubisoft's sound engine ("DARE") games in 2008+ */
VGMSTREAM* init_vgmstream_ubi_bao_pk(STREAMFILE* sf) {
    ubi_bao_header bao = { 0 };

    /* checks */
    if (read_u8(0x00, sf) != 0x01)
        goto fail;
    if (read_u8(0x01, sf) < BAO_MIN_VERSION || read_u8(0x01, sf) > BAO_MAX_VERSION)
        goto fail;

    if (!check_extensions(sf, "pk,lpk,cpk"))
        goto fail;

    /* package .pk+spk (or .lpk+lspk for localized) database-like format, evolved from Ubi sbN/smN.
     * .pk has an index pointing to memory BAOs and tables with external stream BAOs in .spk. */

     /* main parse */
    if (!parse_pk(&bao, sf))
        goto fail;

    build_readable_name(bao.readable_name, sizeof(bao.readable_name), &bao);
    return init_vgmstream_ubi_bao_header(&bao, sf);
fail:
    return NULL;
}

/* .BAO - single BAO files from Ubisoft's sound engine ("DARE") games in 2007+ */
VGMSTREAM* init_vgmstream_ubi_bao_atomic(STREAMFILE* sf) {
    ubi_bao_header bao = { 0 };
    STREAMFILE* streamData = NULL;

    /* checks */
    if (read_u8(0x00, sf) != 0x01 && read_u8(0x00, sf) != 0x02) /* 0x01=AC1, 0x02=POP2008 */
        goto fail;
    if (read_u8(0x01, sf) < BAO_MIN_VERSION || read_u8(0x01, sf) > BAO_MAX_VERSION)
        goto fail;

    if (!check_extensions(sf, "bao,"))
        goto fail;

    /* atomic .bao+bao/sbao found in .forge and similar bigfiles. The bigfile acts as index, but
     * since BAOs reference each other by id and are named by it (though the internal BAO id may
     * be other) we can simulate it. Extension is .bao/sbao or extensionaless in some games. */

    bao.is_atomic = 1;

    bao.version = read_u32be(0x00, sf) & 0x00FFFFFF;
    if (!config_bao_version(&bao, sf))
        goto fail;

    /* main parse */
    if (!parse_bao(&bao, sf, 0x00, 1))
        goto fail;

    build_readable_name(bao.readable_name, sizeof(bao.readable_name), &bao);
    return init_vgmstream_ubi_bao_header(&bao, sf);
fail:
    close_streamfile(streamData);
    return NULL;
}

#if 0
/* .SPK - special mini package with BAOs [Avatar (PS3)] */
VGMSTREAM* init_vgmstream_ubi_bao_spk(STREAMFILE* sf) {
    ubi_bao_header bao = { 0 };

    /* checks */
    if (!check_extensions(sf, "spk"))
        goto fail;

    /* Variation of .pk:
     * - 0x00: 0x014B5053 ("SPK\01" LE)
     * - 0x04: BAO count
     * - 0x08: BAO ids inside (0x04 * BAO count)
     * - per BAO count
     *   - 0x00: table count
     *   - 0x04: ids related to this BAO? (0x04 * table count)
     *   - 0x08/NN: BAO size
     *   - 0x0c/NN+: BAO data up to size
     *
     * BAOs reference .sbao by name (are considered atomic) so perhaps could
     * be considered a type of bigfile.
     */

    return NULL;
}
#endif

/* ************************************************************************* */

static VGMSTREAM* init_vgmstream_ubi_bao_base(ubi_bao_header* bao, STREAMFILE* streamHead, STREAMFILE* streamData) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset = 0x00;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(bao->channels, bao->loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_BAO;
    vgmstream->sample_rate = bao->sample_rate;
    vgmstream->num_streams = bao->total_subsongs;
    vgmstream->stream_size = bao->stream_size;

    vgmstream->num_samples = bao->num_samples;
    vgmstream->loop_start_sample = bao->loop_start;
    vgmstream->loop_end_sample = bao->num_samples;

    switch(bao->codec) {
        case UBI_IMA: {
            vgmstream->coding_type = coding_UBI_IMA;
            vgmstream->layout_type = layout_none;
            break;
        }

        case RAW_PCM:
            vgmstream->coding_type = coding_PCM16LE; /* always LE */
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case RAW_PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (bao->cfg.audio_interleave) ?
                    bao->cfg.audio_interleave :
                    bao->stream_size / bao->channels;
            break;

        case RAW_DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = bao->stream_size / bao->channels;
VGM_LOG("dsp=%x, %x, %x\n", bao->header_offset, bao->header_size, bao->extra_size);
            /* mini DSP header (first 0x10 seem to contain DSP header fields like nibbles and format) */
            dsp_read_coefs_be(vgmstream, streamHead, bao->header_offset + bao->header_size + bao->extra_size + 0x10, 0x40);
            dsp_read_hist_be (vgmstream, streamHead, bao->header_offset + bao->header_size + bao->extra_size + 0x34, 0x40); /* after gain/initial ps */
            break;

#ifdef VGM_USE_FFMPEG
        //TODO: Ubi XMA1 (raw or fmt) is a bit strange, FFmpeg decodes some frames slightly wrong (see Ubi SB)
        case RAW_XMA1:
        case RAW_XMA2_OLD:
        case RAW_XMA2_NEW: {
            uint8_t buf[0x100];
            size_t bytes, chunk_size, data_size;
            off_t chunk_offset;
            STREAMFILE* streamXMA;

            switch(bao->codec) {
                case RAW_XMA1:      chunk_size = 0x20; break;
                case RAW_XMA2_OLD:  chunk_size = 0x2c; break;
                case RAW_XMA2_NEW:  chunk_size = 0x34; break;
                default: goto fail;
            }

            //todo improve XMA subheader skip
            //- audio memory: in header
            //- audio stream: in data
            //- layer memory: in layer mem, right before audio (technically in header...)
            //- layer stream: same?

            /* XMA header chunk is stored in different places, setup and also find actual data start */
            if (bao->is_external || bao->type == UBI_LAYER) {
                uint8_t flag, bits_per_frame;
                uint32_t sec1_num, sec2_num, sec3_num;
                size_t header_size, frame_size;
                off_t header_offset = start_offset + chunk_size;

                /* skip custom XMA seek? table after standard XMA/fmt header chunk */
                if (bao->codec == RAW_XMA1) {
                    flag = read_8bit(header_offset + 0x00, streamData);
                    sec2_num    = read_32bitBE(header_offset + 0x04, streamData); /* number of XMA frames */
                    frame_size  = 0x800;
                    sec1_num    = read_32bitBE(header_offset + 0x08, streamData);
                    sec3_num    = read_32bitBE(header_offset + 0x0c, streamData);
                    header_size = chunk_size + 0x10;
                }
                else {
                    flag = read_8bit(header_offset + 0x00, streamData);
                    sec2_num    = read_32bitBE(header_offset + 0x04, streamData); /* number of XMA frames */
                    frame_size  = 0x800; //read_32bitBE(header_offset + 0x08, streamData); /* not always present? */
                    sec1_num    = read_32bitBE(header_offset + 0x0c, streamData);
                    sec3_num    = read_32bitBE(header_offset + 0x10, streamData); /* assumed */
                    header_size = chunk_size + 0x14;
                }

                bits_per_frame = 4;
                if (flag == 0x02 || flag == 0x04)
                    bits_per_frame = 2;
                else if (flag == 0x08)
                    bits_per_frame = 1;

                header_size += sec1_num * 0x04;
                header_size += align_size_to_block(sec2_num * bits_per_frame, 32) / 8; /* bitstream seek table? */
                header_size += sec3_num * 0x08;

                streamXMA = streamData;
                chunk_offset = 0x00;
                start_offset += header_size;
                data_size = sec2_num * frame_size;
            }
            else {
                streamXMA = streamHead;
                chunk_offset = bao->header_offset + bao->header_size;
                start_offset = 0x00;
                data_size = bao->stream_size;
            }

            if (bao->codec == RAW_XMA2_OLD) {
                bytes = ffmpeg_make_riff_xma2_from_xma2_chunk(buf,0x100, chunk_offset, chunk_size, data_size, streamXMA);
            }
            else {
                bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf,0x100, chunk_offset, chunk_size, data_size, streamXMA, 1);
            }

            vgmstream->codec_data = init_ffmpeg_header_offset(streamData, buf, bytes, start_offset, data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->stream_size = data_size;

            xma_fix_raw_samples(vgmstream, streamData, start_offset,data_size,0, 0,0);
            break;
        }

        case RAW_AT3_105:
        case RAW_AT3: {
            int block_align, encoder_delay;

            block_align = (bao->codec == RAW_AT3_105 ? 0x98 : 0xc0) * vgmstream->channels;
            encoder_delay = 0; /* num_samples is full bytes-to-samples (unlike FMT_AT3) and comparing X360 vs PS3 games seems ok */

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(streamData, start_offset,vgmstream->stream_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case FMT_AT3: {
            vgmstream->codec_data = init_ffmpeg_atrac3_riff(streamData, start_offset, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
#ifdef VGM_USE_VORBIS
        case FMT_OGG: {
            vgmstream->codec_data = init_ogg_vorbis(streamData, start_offset, bao->stream_size, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_OGG_VORBIS;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = bao->num_samples; /* same as Ogg samples */
            break;
        }
#endif
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, streamData, start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);

    VGM_LOG("UBI BAO: failed init base\n");
    return NULL;
}

static VGMSTREAM* init_vgmstream_ubi_bao_audio(ubi_bao_header* bao, STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* streamData = NULL;

    streamData = setup_bao_streamfile(bao, sf);
    if (!streamData) goto fail;

    vgmstream = init_vgmstream_ubi_bao_base(bao, sf, streamData);
    if (!vgmstream) goto fail;

    close_streamfile(streamData);
    return vgmstream;

fail:
    close_streamfile(streamData);
    close_vgmstream(vgmstream);

    VGM_LOG("UBI BAO: failed init audio\n");
    return NULL;
}

static VGMSTREAM* init_vgmstream_ubi_bao_layer(ubi_bao_header* bao, STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    layered_layout_data* data = NULL;
    STREAMFILE* temp_sf = NULL;
    STREAMFILE* streamData = NULL;
    size_t full_stream_size = bao->stream_size;
    int i, total_channels = 0;

    streamData = setup_bao_streamfile(bao, sf);
    if (!streamData) goto fail;

    /* init layout */
    data = init_layout_layered(bao->layer_count);
    if (!data) goto fail;

    /* open all layers and mix */
    for (i = 0; i < bao->layer_count; i++) {

        /* prepare streamfile from a single layer section */
        temp_sf = setup_ubi_bao_streamfile(streamData, 0x00, full_stream_size, i, bao->layer_count, bao->big_endian);
        if (!temp_sf) goto fail;

        bao->stream_size = get_streamfile_size(temp_sf);
        bao->channels = bao->layer_channels[i];
        total_channels += bao->layer_channels[i];

        /* build the layer VGMSTREAM (standard sb with custom streamfile) */
        data->layers[i] = init_vgmstream_ubi_bao_base(bao, sf, temp_sf);
        if (!data->layers[i]) goto fail;

        close_streamfile(temp_sf);
        temp_sf = NULL;
    }

    if (!setup_layout_layered(data))
        goto fail;

    /* build the base VGMSTREAM */
    vgmstream = allocate_vgmstream(total_channels, bao->loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_BAO;
    vgmstream->sample_rate = bao->sample_rate;
    vgmstream->num_streams = bao->total_subsongs;
    vgmstream->stream_size = full_stream_size;

    vgmstream->num_samples = bao->num_samples;
    vgmstream->loop_start_sample = bao->loop_start;
    vgmstream->loop_end_sample = bao->num_samples;

    vgmstream->coding_type = data->layers[0]->coding_type;
    vgmstream->layout_type = layout_layered;
    vgmstream->layout_data = data;

    close_streamfile(streamData);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_streamfile(streamData);
    if (vgmstream)
        close_vgmstream(vgmstream);
    else
        free_layout_layered(data);

    VGM_LOG("UBI BAO: failed init layer\n");
    return NULL;
}

static VGMSTREAM* init_vgmstream_ubi_bao_sequence(ubi_bao_header* bao, STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* streamChain = NULL;
    segmented_layout_data* data = NULL;
    int i;


    /* init layout */
    data = init_layout_segmented(bao->sequence_count);
    if (!data) goto fail;

    bao->channels = 0;
    bao->num_samples = 0;

    /* open all segments and mix */
    for (i = 0; i < bao->sequence_count; i++) {
        ubi_bao_header temp_bao = *bao; /* memcpy'ed */
        int entry_id = bao->sequence_chain[i];

        if (bao->is_atomic) {
            /* open memory audio BAO */
            streamChain = open_atomic_bao(bao->cfg.file_type, entry_id, 0, sf);
            if (!streamChain) {
                VGM_LOG("UBI BAO: chain BAO %08x not found\n", entry_id);
                goto fail;
            }

            /* parse BAO */
            if (!parse_header(&temp_bao, streamChain, 0x00))
                goto fail;

            /* will open its companion BAOs later */
            close_streamfile(streamChain);
            streamChain = NULL;
        }
        else {
            /* find memory audio BAO */
            off_t entry_offset;
            if (!find_package_bao(entry_id, sf, &entry_offset, NULL)) {
                VGM_LOG("UBI BAO: expected chain id %08x not found\n", entry_id);
                goto fail;
            }

            /* parse BAO */
            if (!parse_header(&temp_bao, sf, entry_offset))
                goto fail;
        }

        if (temp_bao.type == TYPE_NONE || temp_bao.type == UBI_SEQUENCE) {
            VGM_LOG("UBI BAO: unexpected sequence entry type\n");
            goto fail; /* technically ok but too much recursiveness? */
        }

        /* build the layer VGMSTREAM (current sb entry config) */
        data->segments[i] = init_vgmstream_ubi_bao_header(&temp_bao, sf);
        if (!data->segments[i]) goto fail;

        if (i == bao->sequence_loop)
            bao->loop_start = bao->num_samples;
        bao->num_samples += data->segments[i]->num_samples;

        /* save current (silences don't have values, so this unsures they know when memcpy'ed) */
        bao->channels = temp_bao.channels;
        bao->sample_rate = temp_bao.sample_rate;
    }

    //todo Rabbids 0x200000bd.pk#24 mixes 2ch audio with 2ch*3 layers

    if (!setup_layout_segmented(data))
        goto fail;


    /* build the base VGMSTREAM */
    vgmstream = allocate_vgmstream(data->output_channels, !bao->sequence_single);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_BAO;
    vgmstream->sample_rate = data->segments[0]->sample_rate;
    vgmstream->num_streams = bao->total_subsongs;
    //vgmstream->stream_size = bao->stream_size; /* auto when getting avg br */

    vgmstream->num_samples = bao->num_samples;
    vgmstream->loop_start_sample = bao->loop_start;
    vgmstream->loop_end_sample = bao->num_samples;

    vgmstream->coding_type = data->segments[0]->coding_type;
    vgmstream->layout_type = layout_segmented;
    vgmstream->layout_data = data;

    return vgmstream;
fail:
    close_streamfile(streamChain);
    if (vgmstream)
        close_vgmstream(vgmstream);
    else
        free_layout_segmented(data);

    VGM_LOG("UBI BAO: failed init sequence\n");
    return NULL;
}


static VGMSTREAM* init_vgmstream_ubi_bao_silence(ubi_bao_header* bao) {
    VGMSTREAM* vgmstream = NULL;
    int channels, sample_rate;
    int32_t num_samples;

    /* by default silences don't have settings */
    channels = bao->channels;
    if (channels == 0)
        channels = 2;
    sample_rate = bao->sample_rate;
    if (sample_rate == 0)
        sample_rate = 48000;
    num_samples = bao->duration * sample_rate;


    /* init the VGMSTREAM */
    vgmstream = init_vgmstream_silence(channels, sample_rate, num_samples);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_BAO;
    vgmstream->num_streams = bao->total_subsongs;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return vgmstream;
}


static VGMSTREAM* init_vgmstream_ubi_bao_header(ubi_bao_header* bao, STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;

    if (bao->total_subsongs <= 0) {
        vgm_logi("UBI BAO: bank has no subsongs (ignore)\n");
        goto fail; /* not uncommon */
    }

    ;VGM_LOG("UBI BAO: target at %x, h_id=%08x, s_id=%08x, p_id=%08x\n",
        (uint32_t)bao->header_offset, bao->header_id, bao->stream_id, bao->prefetch_id);
    ;VGM_LOG("UBI BAO: stream=%x, size=%x, res=%s\n",
            (uint32_t)bao->stream_offset, bao->stream_size, (bao->is_external ? bao->resource_name : "internal"));
    ;VGM_LOG("UBI BAO: type=%i, header=%x, extra=%x, pre.of=%x, pre.sz=%x\n",
            bao->header_type, bao->header_size, bao->extra_size, (uint32_t)bao->prefetch_offset, bao->prefetch_size);


    switch(bao->type) {

        case UBI_AUDIO:
            vgmstream = init_vgmstream_ubi_bao_audio(bao, sf);
            break;

        case UBI_LAYER:
            vgmstream = init_vgmstream_ubi_bao_layer(bao, sf);
            break;

        case UBI_SEQUENCE:
            vgmstream = init_vgmstream_ubi_bao_sequence(bao, sf);
            break;

        case UBI_SILENCE:
            vgmstream = init_vgmstream_ubi_bao_silence(bao);
            break;

        default:
            VGM_LOG("UBI BAO: subsong not found/parsed\n");
            goto fail;
    }

    if (!vgmstream) goto fail;

    strcpy(vgmstream->stream_name, bao->readable_name);
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ************************************************************************* */

/* parse a .pk (package) file: index + BAOs + external .spk resource table. We want header
 * BAOs pointing to internal/external stream BAOs (.spk is the same, with stream BAOs only).
 * A fun feature of .pk is that different BAOs in a .pk can point to different .spk BAOs
 * that actually hold the same data, with different GUID too, somehow. */
static int parse_pk(ubi_bao_header* bao, STREAMFILE* sf) {
    int i;
    int index_entries;
    size_t index_size, index_header_size;
    off_t bao_offset;
    int target_subsong = sf->stream_index;
    STREAMFILE* streamIndex = NULL;
    STREAMFILE* streamTest = NULL;

    /* format: 0x01=package index, 0x02=package BAO */
    if (read_8bit(0x00, sf) != 0x01)
        goto fail;
    /* index and resources are always LE */

    if (target_subsong <= 0) target_subsong = 1;

    bao->version = read_u32be(0x00, sf) & 0x00FFFFFF;
    index_size = read_u32le(0x04, sf); /* can be 0, not including  */
    /* 0x08: resource table offset, always found even if not used */
    /* 0x0c: always 0? */
    /* 0x10: unknown, null if no entries */
    /* 0x14: config/flags/time? (changes a bit between files), null if no entries */
    /* 0x18(10): file GUID? clones may share it */
    /* 0x24: unknown */
    /* 0x2c: unknown, may be same as 0x14, can be null */
    /* 0x30(10): parent GUID? may be same as 0x18, may be shared with other files */
    /* (the above values seem ignored by games, probably just info for their tools) */

    if (!config_bao_version(bao, sf))
        goto fail;


    index_entries = index_size / 0x08;
    index_header_size = 0x40;

    /* pre-load to avoid too much I/O back and forth */
    if (index_size > (10000*0x08)) {
        VGM_LOG("BAO: index too big\n");
        goto fail;
    }

    /* use smaller I/O buffers for performance, as this read lots of small headers all over the place */
    streamIndex = reopen_streamfile(sf, index_size);
    if (!streamIndex) goto fail;

    streamTest = reopen_streamfile(sf, 0x100);
    if (!streamTest) goto fail;

    /* parse index to get target subsong N = Nth valid header BAO */
    bao_offset = index_header_size + index_size;
    for (i = 0; i < index_entries; i++) {
      //uint32_t bao_id = read_32bitLE(index_header_size + 0x08*i + 0x00, streamIndex);
        size_t bao_size = read_32bitLE(index_header_size + 0x08*i + 0x04, streamIndex);

        //;VGM_LOG("UBI BAO: offset=%x, size=%x\n", (uint32_t)bao_offset, bao_size);

        /* parse and continue to find out total_subsongs */
        if (!parse_bao(bao, streamTest, bao_offset, target_subsong))
            goto fail;

        bao_offset += bao_size; /* files simply concat BAOs */
    }

    //;VGM_LOG("UBI BAO: class "); {int i; for (i=0;i<16;i++){ VGM_ASSERT(bao->classes[i],"%02x=%i ",i,bao->classes[i]); }} VGM_LOG("\n");
    //;VGM_LOG("UBI BAO: types "); {int i; for (i=0;i<16;i++){ VGM_ASSERT(bao->types[i],"%02x=%i ",i,bao->types[i]); }} VGM_LOG("\n");

    close_streamfile(streamIndex);
    close_streamfile(streamTest);
    return 1;
fail:
    close_streamfile(streamIndex);
    close_streamfile(streamTest);
    return 0;
}

/* ************************************************************************* */

static void build_readable_name(char* buf, size_t buf_size, ubi_bao_header* bao) {
    const char *grp_name;
    const char *pft_name;
    const char *typ_name;
    const char *res_name;
    uint32_t h_id, s_id, type;

    if (bao->type == TYPE_NONE)
        return;

    /* config */
    if (bao->is_atomic)
        grp_name = "atomic";
    else
        grp_name = "package";
    pft_name = bao->is_prefetched ? "p" : "n";
    typ_name = bao->is_external ? "str" : "mem";

    h_id = bao->header_id;
    s_id = bao->stream_id;
    type = bao->header_type;

    if (bao->type == UBI_SEQUENCE) {
        if (bao->sequence_single) {
            if (bao->sequence_count == 1)
                res_name = "single";
            else
                res_name = "multi";
        }
        else {
            if (bao->sequence_count == 1)
                res_name = "single-loop";
            else
                res_name = (bao->sequence_loop == 0) ? "multi-loop" : "intro-loop";
        }
    }
    else {
        res_name = NULL;
        //if (!bao->is_atomic && bao->is_external)
        //    res_name = bao->resource_name; /* too big? */
        //else
        //    res_name = NULL;
    }

    /* .pk can contain many subsongs, we need something helpful
     * (best done right after subsong detection, since some sequence re-parse types) */
    if (res_name && res_name[0]) {
        snprintf(buf,buf_size, "%s/%s-%s/%02x-%08x/%08x/%s", grp_name, pft_name, typ_name, type, h_id, s_id, res_name);
    }
    else {
        snprintf(buf,buf_size, "%s/%s-%s/%02x-%08x/%08x", grp_name, pft_name, typ_name, type, h_id, s_id);
    }
}

static int parse_type_audio(ubi_bao_header* bao, off_t offset, STREAMFILE* sf) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = bao->big_endian ? read_32bitBE : read_32bitLE;
    off_t h_offset = offset + bao->header_skip;

    /* audio header */
    bao->type = UBI_AUDIO;

    bao->stream_size = read_32bit(h_offset + bao->cfg.audio_stream_size, sf);
    bao->stream_id   = read_32bit(h_offset + bao->cfg.audio_stream_id, sf);
    bao->is_external = read_32bit(h_offset + bao->cfg.audio_external_flag, sf) & bao->cfg.audio_external_and;
    bao->loop_flag   = read_32bit(h_offset + bao->cfg.audio_loop_flag, sf) & bao->cfg.audio_loop_and;
    bao->channels    = read_32bit(h_offset + bao->cfg.audio_channels, sf);
    bao->sample_rate = read_32bit(h_offset + bao->cfg.audio_sample_rate, sf);

    /* extra cue table, rare (found with DSP) [We Dare (Wii)] */
    if (bao->cfg.audio_cue_size) {
        //bao->cfg.audio_cue_count //not needed?
        bao->extra_size = read_32bit(h_offset + bao->cfg.audio_cue_size, sf);
    }

    /* prefetch data is in another internal BAO right after the base header */
    if (bao->cfg.audio_prefetch_size) {
        bao->prefetch_size = read_32bit(h_offset + bao->cfg.audio_prefetch_size, sf);
        bao->is_prefetched = (bao->prefetch_size > 0);
    }

    if (bao->loop_flag) {
        bao->loop_start  = read_32bit(h_offset + bao->cfg.audio_num_samples, sf);
        bao->num_samples = read_32bit(h_offset + bao->cfg.audio_num_samples2, sf) + bao->loop_start;
    }
    else {
        bao->num_samples = read_32bit(h_offset + bao->cfg.audio_num_samples, sf);
    }

    bao->stream_type = read_32bit(h_offset + bao->cfg.audio_stream_type, sf);

    return 1;
//fail:
//    return 0;
}

static int parse_type_sequence(ubi_bao_header* bao, off_t offset, STREAMFILE* sf) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = bao->big_endian ? read_32bitBE : read_32bitLE;
    off_t h_offset = offset + bao->header_skip;
    off_t table_offset;
    int i;

    /* sequence chain */
    bao->type = UBI_SEQUENCE;
    if (bao->cfg.sequence_entry_size == 0) {
        VGM_LOG("UBI BAO: sequence entry size not configured at %x\n", (uint32_t)offset);
        goto fail;
    }

    bao->sequence_loop   = read_32bit(h_offset + bao->cfg.sequence_sequence_loop, sf);
    bao->sequence_single = read_32bit(h_offset + bao->cfg.sequence_sequence_single, sf);
    bao->sequence_count  = read_32bit(h_offset + bao->cfg.sequence_sequence_count, sf);
    if (bao->sequence_count > BAO_MAX_CHAIN_COUNT) {
        VGM_LOG("UBI BAO: incorrect sequence count\n");
        goto fail;
    }

    /* get chain in extra table */
    table_offset = offset + bao->header_size;
    for (i = 0; i < bao->sequence_count; i++) {
        uint32_t entry_id = (uint32_t)read_32bit(table_offset + bao->cfg.sequence_entry_number, sf);

        bao->sequence_chain[i] = entry_id;

        table_offset += bao->cfg.sequence_entry_size;
    }

    return 1;
fail:
    return 0;
}


static int parse_type_layer(ubi_bao_header* bao, off_t offset, STREAMFILE* sf) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = bao->big_endian ? read_32bitBE : read_32bitLE;
    off_t h_offset = offset + bao->header_skip;
    off_t table_offset;
    size_t cues_size = 0;
    int i;

    /* audio header */
    bao->type = UBI_LAYER;
    if (bao->cfg.layer_entry_size == 0) {
        VGM_LOG("UBI BAO: layer entry size not configured at %x\n", (uint32_t)offset);
        goto fail;
    }

    bao->layer_count    = read_32bit(h_offset + bao->cfg.layer_layer_count, sf);
    if (bao->layer_count > BAO_MAX_LAYER_COUNT) {
        VGM_LOG("UBI BAO: incorrect layer count\n");
        goto fail;
    }

    bao->is_external    = read_32bit(h_offset + bao->cfg.layer_external_flag, sf) & bao->cfg.layer_external_and;
    bao->stream_size    = read_32bit(h_offset + bao->cfg.layer_stream_size, sf);
    bao->stream_id      = read_32bit(h_offset + bao->cfg.layer_stream_id, sf);

    if (bao->cfg.layer_prefetch_size) {
        bao->prefetch_size  = read_32bit(h_offset + bao->cfg.layer_prefetch_size, sf);
        bao->is_prefetched = (bao->prefetch_size > 0);
    }

    /* extra cue table (rare, has N variable-sized labels + cue table pointing to them) */
    if (bao->cfg.layer_cue_labels) {
        cues_size += read_32bit(h_offset + bao->cfg.layer_cue_labels, sf);
    }
    if (bao->cfg.layer_cue_count) {
        cues_size += read_32bit(h_offset + bao->cfg.layer_cue_count, sf) * 0x08;
    }

    if (bao->cfg.layer_extra_size) {
        bao->extra_size = read_32bit(h_offset + bao->cfg.layer_extra_size, sf);
    }
    else {
        bao->extra_size = cues_size + bao->layer_count * bao->cfg.layer_entry_size + cues_size;
    }

    /* get 1st layer header in extra table and validate all headers match */
    table_offset = offset + bao->header_size + cues_size;
  //bao->channels       = read_32bit(table_offset + bao->cfg.layer_channels, sf);
    bao->sample_rate    = read_32bit(table_offset + bao->cfg.layer_sample_rate, sf);
    bao->stream_type    = read_32bit(table_offset + bao->cfg.layer_stream_type, sf);
    bao->num_samples    = read_32bit(table_offset + bao->cfg.layer_num_samples, sf);

    for (i = 0; i < bao->layer_count; i++) {
        int channels    = read_32bit(table_offset + bao->cfg.layer_channels, sf);
        int sample_rate = read_32bit(table_offset + bao->cfg.layer_sample_rate, sf);
        int stream_type = read_32bit(table_offset + bao->cfg.layer_stream_type, sf);
        int num_samples = read_32bit(table_offset + bao->cfg.layer_num_samples, sf);
        if (bao->sample_rate != sample_rate || bao->stream_type != stream_type) {
            VGM_LOG("UBI BAO: layer headers don't match at %x\n", (uint32_t)table_offset);

            if (!bao->cfg.layer_ignore_error) {
                goto fail;
            }
        }

        /* uncommonly channels may vary per layer [Rayman Raving Rabbids: TV Party (Wii) ex. 0x22000cbc.pk] */
        bao->layer_channels[i] = channels;

        /* can be +-1 */
        if (bao->num_samples != num_samples && bao->num_samples + 1 == num_samples) {
            bao->num_samples -= 1;
        }

        table_offset += bao->cfg.layer_entry_size;
    }

    return 1;
fail:
    return 0;
}

static int parse_type_silence(ubi_bao_header* bao, off_t offset, STREAMFILE* sf) {
    float (*read_f32)(off_t,STREAMFILE*) = bao->big_endian ? read_f32be : read_f32le;
    off_t h_offset = offset + bao->header_skip;

    /* silence header */
    bao->type = UBI_SILENCE;
    if (bao->cfg.silence_duration_float == 0) {
        VGM_LOG("UBI BAO: silence duration not configured at %x\n", (uint32_t)offset);
        goto fail;
    }

    bao->duration = read_f32(h_offset + bao->cfg.silence_duration_float, sf);
    if (bao->duration <= 0.0f) {
        VGM_LOG("UBI BAO: bad duration %f at %x\n", bao->duration, (uint32_t)offset);
        goto fail;
    }

    return 1;
fail:
    return 0;
}

/* adjust some common values */
static int parse_values(ubi_bao_header* bao) {

    if (bao->type == UBI_SEQUENCE || bao->type == UBI_SILENCE)
        return 1;

    /* common validations */
    if (bao->stream_size == 0) {
        VGM_LOG("UBI BAO: unknown stream_size at %x\n", (uint32_t)bao->header_offset); goto fail;
        goto fail;
    }

    /* set codec */
    if (bao->stream_type > 0x10) {
        VGM_LOG("UBI BAO: unknown stream_type at %x\n", (uint32_t)bao->header_offset); goto fail;
        goto fail;
    }
    bao->codec = bao->cfg.codec_map[bao->stream_type];
    if (bao->codec == 0x00) {
        VGM_LOG("UBI BAO: unknown codec %x at %x\n", bao->stream_type, (uint32_t)bao->header_offset); goto fail;
        goto fail;
    }

    if (bao->type == UBI_AUDIO && bao->codec == RAW_PSX && bao->cfg.audio_fix_psx_samples && bao->loop_flag) { //todo: loop flag only?
        bao->num_samples = bao->num_samples / bao->channels;
    }


    /* set prefetch id */
    if (bao->is_prefetched) {
        if (bao->is_atomic && bao->cfg.file_type == UBI_FORGE) {
            /* AC1's stream BAO are 0x5NNNNNNN and prefetch BAO 0x3NNNNNNN (all filenames include class) */
            bao->prefetch_id = (bao->stream_id & 0x0FFFFFFF) | 0x30000000;
        }
        else {
            /* shared id in index and resource table, or named atomic BAOs */
            bao->prefetch_id = bao->stream_id;
        }
    }

    /* normalize base skips, as memory data (prefetch or not, atomic or package) can be
     * in a memory BAO after base header or audio layer BAO after the extra table */
    if (bao->stream_id == bao->header_id && (!bao->is_external || bao->is_prefetched)) { /* layers with memory data */
        bao->memory_skip = bao->header_size + bao->extra_size;
        bao->stream_skip = bao->header_skip;
    }
    else {
        bao->memory_skip = bao->header_skip;
        bao->stream_skip = bao->header_skip;
    }


    return 1;
fail:
    return 0;
}


/* set actual offsets in various places */
static int parse_offsets(ubi_bao_header* bao, STREAMFILE* sf) {
    off_t bao_offset;
    size_t bao_size;

    if (bao->type == UBI_SEQUENCE || bao->type == UBI_SILENCE)
        return 1;

    if (!bao->is_external && bao->is_prefetched) {
        VGM_LOG("UBI BAO: unexpected non-streamed prefetch at %x\n", (uint32_t)bao->header_offset);
        goto fail;
    }

    /* Audio headers can point to audio data in multiple forms we must configure here:
     * - memory part (internal .pk BAO or separate atomic .bao)
     * - streamed part (external .spk BAO or separate atomic .sbao)
     * - prefetched memory part + streamed part (must join both during reads)
     *
     * Offsets are absolute (ignoring the index table that even .spk has) but point to BAO
     * base header start, that we must also skip to reach actual audio data.
     */

    if (bao->is_atomic) {
        if (bao->is_prefetched) {
            bao->prefetch_offset = bao->memory_skip;
        }

        if (bao->is_external) {
            bao->stream_offset = bao->stream_skip;
        }
        else {
            bao->stream_offset = bao->memory_skip;
        }
    }
    else {
        if (bao->is_prefetched) {
            if (!find_package_bao(bao->prefetch_id, sf, &bao_offset, &bao_size)) {
                VGM_LOG("UBI BAO: expected prefetch id %08x not found\n", bao->prefetch_id);
                goto fail;
            }

            bao->prefetch_offset = bao_offset + bao->memory_skip;
            if (bao->prefetch_size + bao->memory_skip != bao_size) {
                VGM_LOG("UBI BAO: unexpected prefetch size %x vs %x\n", bao->prefetch_size + bao->memory_skip, bao_size);
                goto fail;
            }
        }

        if (bao->is_external) {
            int i;
            off_t offset;
            off_t resources_offset  = read_32bitLE(0x08, sf);
            int resources_count     = read_32bitLE(resources_offset+0x00, sf);
            size_t strings_size     = read_32bitLE(resources_offset+0x04, sf);

            /* parse resource table to external stream (may be empty, or exist even with nothing in the file) */
            offset = resources_offset + 0x04+0x04 + strings_size;
            for (i = 0; i < resources_count; i++) {
                uint32_t resource_id  = read_32bitLE(offset+0x10*i+0x00, sf);
                off_t name_offset     = read_32bitLE(offset+0x10*i+0x04, sf);
                off_t resource_offset = read_32bitLE(offset+0x10*i+0x08, sf);
                size_t resource_size  = read_32bitLE(offset+0x10*i+0x0c, sf);

                if (resource_id == bao->stream_id) {
                    bao->stream_offset = resource_offset + bao->stream_skip;

                    read_string(bao->resource_name,255, resources_offset + 0x04+0x04 + name_offset, sf);

                    if (bao->stream_size != resource_size - bao->stream_skip + bao->prefetch_size) {
                        VGM_LOG("UBI BAO: stream vs resource size mismatch at %lx (res %x vs str=%x, skip=%x, pre=%x)\n", offset+0x10*i, resource_size, bao->stream_size, bao->stream_skip, bao->prefetch_size);

                        /* rarely resource has more data than stream (sometimes a few bytes, others +0x100000)
                         * sometimes short song versions, but not accessed? no samples/sizes/cues/etc in header seem to refer to that [Just Dance (Wii)]
                         * Michael Jackson The Experience also uses prefetch size + bad size (ignored) */
                        if (!bao->cfg.audio_ignore_resource_size && bao->prefetch_size)
                            goto fail;
                    }
                    break;
                }
            }

            if (bao->stream_offset == 0) {
                VGM_LOG("UBI BAO: expected external id %08x not found\n", bao->stream_id);
                goto fail;
            }
        }
        else {
            if (!find_package_bao(bao->stream_id, sf, &bao_offset, &bao_size)) {
                VGM_LOG("UBI BAO: expected internal id %08x not found\n", bao->stream_id);
                goto fail;
            }
            bao->stream_offset = bao_offset + bao->memory_skip;

            /* in some cases, stream size value from audio header can be bigger (~0x18)
             * than actual audio chunk o_O [Rayman Raving Rabbids: TV Party (Wii)] */
            if (bao->stream_size > bao_size - bao->memory_skip) {
                VGM_LOG("UBI BAO: bad stream size found: %x + %x vs %x\n", bao->stream_size, bao->memory_skip, bao_size);

                /* too big is usually bad config */
                if (bao->stream_size > bao_size + bao->header_size) {
                    VGM_LOG("UBI BAO: bad stream config at %x\n", (uint32_t)bao->header_offset);
                    goto fail;
                }

                bao->stream_size = bao_size - bao->memory_skip;
            }
        }

    }

    return 1;
fail:
    return 0;
}

/* parse a single known header resource at offset (see config_bao for info) */
static int parse_header(ubi_bao_header* bao, STREAMFILE* sf, off_t offset) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = bao->big_endian ? read_32bitBE : read_32bitLE;

    bao->header_offset  = offset;

    bao->header_format  = read_8bit (offset + 0x00, sf); /* 0x01: atomic, 0x02: package */
    bao->header_version = read_32bitBE(offset + 0x00, sf) & 0x00FFFFFF;
    if (bao->version != bao->header_version) {
        VGM_LOG("UBI BAO: mismatched header version at %x\n", (uint32_t)offset);
        goto fail;
    }

    /* - base part in early versions:
     * 0x04: header skip (usually 0x28, rarely 0x24), can be LE unlike other fields (ex. Assassin's Creed PS3)
     * 0x08(10): GUID, or id-like fields in early versions
     * 0x18: null
     * 0x1c: null
     * 0x20: class
     * 0x24: config/version? (0x00/0x01/0x02)
     *
     * - base part in later versions:
     * 0x04(10): GUID
     * 0x14: class
     * 0x18: config/version? (0x02)
     * 0x1c: fixed value?  */

    bao->header_skip    = bao->cfg.header_skip;
    bao->header_id      = read_32bit(offset + bao->header_skip + 0x00, sf);
    bao->header_type    = read_32bit(offset + bao->header_skip + 0x04, sf);

    bao->header_size    = bao->cfg.header_base_size;

    /* hack for games with smaller than standard
     * (can't use lowest size as other games also have extra unused field) */
    if (bao->cfg.header_less_le_flag && !bao->big_endian) {
        bao->header_size -= 0x04;
    }
    /* detect extra unused field in PC/Wii
     * (could be improved but no apparent flags or anything useful) */
    else if (get_streamfile_size(sf) > offset + bao->header_size) {
        /* may read next BAO version, layer header, cues, resource table size, etc, always > 1 */
        int32_t end_field = read_32bit(offset + bao->header_size, sf);

        if (end_field == -1 || end_field == 0 || end_field == 1) /* some count? */
            bao->header_size += 0x04;
    }

    switch(bao->header_type) {
        case 0x01:
            if (!parse_type_audio(bao, offset, sf))
                goto fail;
            break;
        case 0x05:
            if (!parse_type_sequence(bao, offset, sf))
                goto fail;
            break;
        case 0x06:
            if (!parse_type_layer(bao, offset, sf))
                goto fail;
            break;
        case 0x08:
            if (!parse_type_silence(bao, offset, sf))
                goto fail;
            break;
        default:
            VGM_LOG("UBI BAO: unknown header type at %x\n", (uint32_t)offset);
            goto fail;
    }

    if (!parse_values(bao))
        goto fail;

    if (!parse_offsets(bao, sf))
        goto fail;

    return 1;
fail:
    return 0;
}

static int parse_bao(ubi_bao_header* bao, STREAMFILE* sf, off_t offset, int target_subsong) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    uint32_t bao_class, header_type;

  /*bao_version =*/ read_32bitBE(offset+0x00, sf); /* force buffer read */

    config_bao_endian(bao, offset, sf);
    read_32bit = bao->big_endian ? read_32bitBE : read_32bitLE;

    bao_class = read_32bit(offset+bao->cfg.bao_class, sf);
    if (bao_class & 0x0FFFFFFF) {
        VGM_LOG("UBI BAO: unknown class %x at %x\n", bao_class, (uint32_t)offset);
        goto fail;
    }

    bao->classes[(bao_class >> 28) & 0xF]++;
    if (bao_class != 0x20000000) /* ignore non-header classes */
        return 1;

    header_type = read_32bit(offset + bao->cfg.header_skip + 0x04, sf);
    if (header_type > 9) {
        VGM_LOG("UBI BAO: unknown type %x at %x\n", header_type, (uint32_t)offset);
        goto fail;
    }

    //;VGM_ASSERT(header_type == 0x05 || header_type == 0x06, "UBI BAO: type %x at %x\n", header_type, (uint32_t)offset);

    bao->types[header_type]++;
    if (!bao->allowed_types[header_type])
        return 1;

    bao->total_subsongs++;
    if (target_subsong != bao->total_subsongs)
        return 1;

    if (!parse_header(bao, sf, offset))
        goto fail;

    return 1;
fail:
    return 0;
}

/* ************************************************************************* */

/* These are all of the languages that were referenced in Assassin's Creed exe (out of each platform). */
/* Also, additional languages were referenced in Shawn White Skateboarding (X360) exe in this order, there may be more. */
static const char* language_bao_formats[] = {
    "English_BAO_0x%08x",
    "French_BAO_0x%08x",
    "Spanish_BAO_0x%08x",
    "Polish_BAO_0x%08x",
    "German_BAO_0x%08x",
    "Chinese_BAO_0x%08x",
    "Hungarian_BAO_0x%08x",
    "Italian_BAO_0x%08x",
    "Japanese_BAO_0x%08x",
    "Czech_BAO_0x%08x",
    "Korean_BAO_0x%08x",
    "Russian_BAO_0x%08x",
    "Dutch_BAO_0x%08x",
    "Danish_BAO_0x%08x",
    "Norwegian_BAO_0x%08x",
    "Swedish_BAO_0x%08x",
};

/* opens a file BAO's companion BAO (memory or stream) */
static STREAMFILE* open_atomic_bao(ubi_bao_file file_type, uint32_t file_id, int is_stream, STREAMFILE* sf) {
    STREAMFILE* sf_bao = NULL;
    char buf[255];
    size_t buf_size = 255;

    /* Get referenced BAOs, in different naming styles for "internal" (=memory) or "external" (=stream). */
    switch(file_type) {

        case UBI_FORGE:
        case UBI_FORGE_b:
            /* Try default extensionless (as extracted from .forge bigfile) and with common extension.
             * .forge data can be uncompressed (stream BAOs) and compressed (subfiles per area with memory BAOs). */
            if (is_stream) {
                snprintf(buf,buf_size, "Common_BAO_0x%08x", file_id);
                sf_bao = open_streamfile_by_filename(sf, buf);
                if (sf_bao) return sf_bao;

                strcat(buf,".sbao");
                sf_bao = open_streamfile_by_filename(sf, buf);
                if (sf_bao) return sf_bao;

                {
                    int i;
                    int count = (sizeof(language_bao_formats) / sizeof(language_bao_formats[0]));
                    for (i = 0; i < count; i++) {
                        const char* format = language_bao_formats[i];

                        snprintf(buf,buf_size, format, file_id);
                        sf_bao = open_streamfile_by_filename(sf, buf);
                        if (sf_bao) return sf_bao;
                    }
                }
                
                /* If all else fails, try %08x.bao/%08x.sbao nomenclature. 
                 * (id).bao is for mimicking engine loading files by internal ID,
                 * original names (like Common_BAO_0x5NNNNNNN, French_BAO_0x5NNNNNNN and the like) are OK too. */
                if (file_type != UBI_FORGE_b) {
                    /* %08x.bao nomenclature present in Assassin's Creed (Windows Vista) exe. */
                    snprintf(buf,buf_size, "%08x.bao", file_id);
                    sf_bao = open_streamfile_by_filename(sf, buf);
                    if (sf_bao) return sf_bao;
				}
                else {
                    /* %08x.sbao nomenclature (in addition to %08x.bao) present in Shaun White Snowboarding (Windows Vista) exe. */
                    snprintf(buf,buf_size, "%08x.sbao", file_id);
                    sf_bao = open_streamfile_by_filename(sf, buf);
                    if (sf_bao) return sf_bao;
                }
            }
            else {
                snprintf(buf,buf_size, "BAO_0x%08x", file_id);
                sf_bao = open_streamfile_by_filename(sf, buf);
                if (sf_bao) return sf_bao;

                strcat(buf,".bao");
                sf_bao = open_streamfile_by_filename(sf, buf);
                if (sf_bao) return sf_bao;
                
                /* Ditto. */
                snprintf(buf,buf_size, "%08x.bao", file_id);
                sf_bao = open_streamfile_by_filename(sf, buf);
                if (sf_bao) return sf_bao;
            }

            goto fail;

        case UBI_FAT:
            snprintf(buf,buf_size, "%08x.bao", file_id);
            sf_bao = open_streamfile_by_filename(sf, buf);
            if (sf_bao) return sf_bao;

            goto fail;

        default:
            goto fail;
    }

    return sf_bao; /* may be NULL */
fail:
    close_streamfile(sf_bao);

    VGM_LOG("UBI BAO: failed opening atomic BAO id %08x\n", file_id);
    return NULL;
}

static int find_package_bao(uint32_t target_id, STREAMFILE* sf, off_t* p_offset, size_t* p_size) {
    int i;
    int index_entries;
    off_t bao_offset;
    size_t index_size, index_header_size;

    index_size = read_32bitLE(0x04, sf);
    index_entries = index_size / 0x08;
    index_header_size = 0x40;

    /* parse index to get target BAO */
    bao_offset = index_header_size + index_size;
    for (i = 0; i < index_entries; i++) {
        uint32_t bao_id = read_32bitLE(index_header_size + 0x08*i + 0x00, sf);
        size_t bao_size = read_32bitLE(index_header_size + 0x08*i + 0x04, sf);

        if (bao_id == target_id) {
            if (p_offset) *p_offset = bao_offset;
            if (p_size) *p_size = bao_size;
            return 1;
        }
        bao_offset += bao_size;
    }

    return 0;
}


/* create a usable streamfile */
static STREAMFILE* setup_bao_streamfile(ubi_bao_header* bao, STREAMFILE* sf) {
    STREAMFILE* new_sf = NULL;
    STREAMFILE* temp_sf = NULL;
    STREAMFILE* stream_segments[2] = { 0 };

    /* Audio comes in "memory" and "streaming" BAOs. When "prefetched" flag is
     * on we need to join memory and streamed part as they're stored separately.
     *
     * The physical location of those depends on the format:
     * - file .bao: both in separate files, with different names per type
     * - bank .pk: memory BAO is in the pk, stream is in another file
     *
     * For some header BAO audio data can be in the same BAO too, which
     * can be considered memory BAO with different offset treatment.
     */

    if (bao->is_atomic) {
        /* file BAOs re-open new STREAMFILEs so no need to wrap them */
        if (bao->is_prefetched) {
            new_sf = open_atomic_bao(bao->cfg.file_type, bao->prefetch_id, 0, sf);
            if (!new_sf) goto fail;
            stream_segments[0] = new_sf;

            new_sf = open_clamp_streamfile(stream_segments[0], bao->prefetch_offset, bao->prefetch_size);
            if (!new_sf) goto fail;
            stream_segments[0] = new_sf;

            if (bao->stream_size - bao->prefetch_size != 0) {
                new_sf = open_atomic_bao(bao->cfg.file_type, bao->stream_id, 1, sf);
                if (!new_sf) goto fail;
                stream_segments[1] = new_sf;

                new_sf = open_clamp_streamfile(stream_segments[1], bao->stream_offset, (bao->stream_size - bao->prefetch_size));
                if (!new_sf) goto fail;
                stream_segments[1] = new_sf;

                new_sf = open_multifile_streamfile(stream_segments, 2);
                if (!new_sf) goto fail;
                temp_sf = new_sf;
                stream_segments[0] = NULL;
                stream_segments[1] = NULL;
            }
            else {
                /* weird but happens, streamed chunk is empty in this case */
                temp_sf = new_sf;
                stream_segments[0] = NULL;
            }
        }
        else {
            new_sf = open_atomic_bao(bao->cfg.file_type, bao->stream_id, bao->is_external, sf);
            if (!new_sf) goto fail;
            temp_sf = new_sf;

            new_sf = open_clamp_streamfile(temp_sf, bao->stream_offset, bao->stream_size);
            if (!new_sf) goto fail;
            temp_sf = new_sf;
        }
    }
    else {
        if (bao->is_prefetched) {
            new_sf = open_wrap_streamfile(sf);
            if (!new_sf) goto fail;
            stream_segments[0] = new_sf;

            new_sf = open_clamp_streamfile(stream_segments[0], bao->prefetch_offset, bao->prefetch_size);
            if (!new_sf) goto fail;
            stream_segments[0] = new_sf;

            if (bao->stream_size - bao->prefetch_size != 0) {
                new_sf = open_streamfile_by_filename(sf, bao->resource_name);
                if (!new_sf) {
                    vgm_logi("UBI BAO: external file '%s' not found (put together)\n", bao->resource_name); 
                    goto fail; 
                }
                stream_segments[1] = new_sf;

                new_sf = open_clamp_streamfile(stream_segments[1], bao->stream_offset, (bao->stream_size - bao->prefetch_size));
                if (!new_sf) goto fail;
                stream_segments[1] = new_sf;
                temp_sf = NULL;

                new_sf = open_multifile_streamfile(stream_segments, 2);
                if (!new_sf) goto fail;
                temp_sf = new_sf;
                stream_segments[0] = NULL;
                stream_segments[1] = NULL;
            }
            else {
                /* weird but happens, streamed chunk is empty in this case */
                temp_sf = new_sf;
                stream_segments[0] = NULL;
            }
        }
        else if (bao->is_external) {
            new_sf = open_streamfile_by_filename(sf, bao->resource_name);
            if (!new_sf) {
                vgm_logi("UBI BAO: external file '%s' not found (put together)\n", bao->resource_name);
                goto fail;
            }
            temp_sf = new_sf;

            new_sf = open_clamp_streamfile(temp_sf, bao->stream_offset, bao->stream_size);
            if (!new_sf) goto fail;
            temp_sf = new_sf;
        }
        else {
            new_sf = open_wrap_streamfile(sf);
            if (!new_sf) goto fail;
            temp_sf = new_sf;

            new_sf = open_clamp_streamfile(temp_sf, bao->stream_offset, bao->stream_size);
            if (!new_sf) goto fail;
            temp_sf = new_sf;
        }

    }

    return temp_sf;

fail:
    close_streamfile(stream_segments[0]);
    close_streamfile(stream_segments[1]);
    close_streamfile(temp_sf);

    VGM_LOG("UBI BAO: failed streamfile setup\n");
    return NULL;
}

/* ************************************************************************* */

static void config_bao_endian(ubi_bao_header* bao, off_t offset, STREAMFILE* sf) {

    /* Detect endianness using the 'class' field (the 'header skip' field is LE in early
     * versions, and was removed in later versions).
     * This could be done once as all BAOs share endianness */

    /* negate as fields looks like LE (0xN0000000) */
    bao->big_endian = !guess_endianness32bit(offset+bao->cfg.bao_class, sf);
}


static void config_bao_entry(ubi_bao_header* bao, size_t header_base_size, size_t header_skip) {
    bao->cfg.header_base_size       = header_base_size;
    bao->cfg.header_skip            = header_skip;
}

static void config_bao_audio_b(ubi_bao_header* bao, off_t stream_size, off_t stream_id, off_t external_flag, off_t loop_flag, int external_and, int loop_and) {
    /* audio header base */
    bao->cfg.audio_stream_size      = stream_size;
    bao->cfg.audio_stream_id        = stream_id;
    bao->cfg.audio_external_flag    = external_flag;
    bao->cfg.audio_loop_flag        = loop_flag;
    bao->cfg.audio_external_and     = external_and;
    bao->cfg.audio_loop_and         = loop_and;
}
static void config_bao_audio_m(ubi_bao_header* bao, off_t channels, off_t sample_rate, off_t num_samples, off_t num_samples2, off_t stream_type, off_t prefetch_size) {
    /* audio header main */
    bao->cfg.audio_channels         = channels;
    bao->cfg.audio_sample_rate      = sample_rate;
    bao->cfg.audio_num_samples      = num_samples;
    bao->cfg.audio_num_samples2     = num_samples2;
    bao->cfg.audio_stream_type      = stream_type;
  //bao->cfg.audio_cue_count        = cue_count;
  //bao->cfg.audio_cue_labels       = cue_labels;
    bao->cfg.audio_prefetch_size    = prefetch_size;
}

static void config_bao_audio_c(ubi_bao_header* bao, off_t cue_count, off_t cue_size) {
    /* audio header extra */
    bao->cfg.audio_cue_count        = cue_count;
    bao->cfg.audio_cue_size         = cue_size;
}

static void config_bao_sequence(ubi_bao_header* bao, off_t sequence_count, off_t sequence_single, off_t sequence_loop, off_t entry_size) {
    /* sequence header and chain table */
    bao->cfg.sequence_sequence_count    = sequence_count;
    bao->cfg.sequence_sequence_single   = sequence_single;
    bao->cfg.sequence_sequence_loop     = sequence_loop;
    bao->cfg.sequence_entry_size        = entry_size;
    bao->cfg.sequence_entry_number      = 0x00;
}

static void config_bao_layer_m(ubi_bao_header* bao, off_t stream_id, off_t layer_count, off_t external_flag, off_t stream_size, off_t extra_size, off_t prefetch_size, off_t cue_count, off_t cue_labels, int external_and) {
    /* layer header in the main part */
    bao->cfg.layer_stream_id            = stream_id;
    bao->cfg.layer_layer_count          = layer_count;
    bao->cfg.layer_external_flag        = external_flag;
    bao->cfg.layer_stream_size          = stream_size;
    bao->cfg.layer_extra_size           = extra_size;
    bao->cfg.layer_prefetch_size        = prefetch_size;
    bao->cfg.layer_cue_count            = cue_count;
    bao->cfg.layer_cue_labels           = cue_labels;
    bao->cfg.layer_external_and         = external_and;
}
static void config_bao_layer_e(ubi_bao_header* bao, off_t entry_size, off_t sample_rate, off_t channels, off_t stream_type, off_t num_samples) {
    /* layer sub-headers in extra table */
    bao->cfg.layer_entry_size           = entry_size;
    bao->cfg.layer_sample_rate          = sample_rate;
    bao->cfg.layer_channels             = channels;
    bao->cfg.layer_stream_type          = stream_type;
    bao->cfg.layer_num_samples          = num_samples;
}

static void config_bao_silence_f(ubi_bao_header* bao, off_t duration) {
    /* silence headers in float value */
    bao->cfg.silence_duration_float     = duration;
}


static int config_bao_version(ubi_bao_header* bao, STREAMFILE* sf) {
    uint32_t version;

    /* Ubi BAO evolved from Ubi SB and are conceptually quite similar, see that first.
     *
     * BAOs (binary audio objects) always start with:
     * - 0x00(1): format (meaning defined by mode)
     * - 0x01(3): 8b*3 version, major/minor/release (numbering continues from .sb0/sm0)
     * - 0x04+: mini header (varies with version, see parse_header)
     *
     * Then are divided into "classes":
     * - 0x10000000: event (links by id to another event or header BAO, may set volume/reverb/filters/etc)
     * - 0x20000000: header
     * - 0x30000000: memory audio (in .pk/.bao)
     * - 0x40000000: project info
     * - 0x50000000: stream audio (in .spk/.sbao)
     * - 0x60000000: unused?
     * - 0x70000000: info? has a count+table of id-things
     * - 0x80000000: unknown (some floats?)
     * - 0x90000000: unknown (some kind of command config?), rare [Ghost Recon Future Soldier (PC), Drawsome (Wii)]
     * Class 1/2/3 are roughly equivalent to Ubi SB's section1/2/3, and class 4 is
     * basically .spN project files.
     *
     * The project BAO (usually with special id 0x7FFFFFFF or 0x40000000) has version,
     * filenames (not complete) and current mode, "PACKAGE" (pk, index + BAOs with
     * external BAOs) or "ATOMIC" (file, separate BAOs).
     *
     * We want header classes, also similar to SB types:
     * - 01: single audio (samples, channels, bitrate, samples+size, etc)
     * - 02: play chain with config? (ex. silence + audio, or rarely audio 2ch intro + layer 4ch body)
     * - 03: unknown chain
     * - 04: random (count, etc) + BAO IDs and float probability to play
     * - 05: sequence (count, etc) + BAO IDs and unknown data
     * - 06: layer (count, etc) + layer headers
     * - 07: unknown chain
     * - 08: silence (duration, etc)
     * - 09: silence with config? (channels, sample rate, etc), extremely rare [Shaun White Skateboarding (Wii)]
     *
     * Right after base BAO size is the extra table for that BAO (what sectionX had, plus
     * extra crap like cue-like labels, even for type 0x01).
     *
     * Just to throw us off, the base BAO size may add +0x04 (with a field value of 0/-1) on
     * some game versions/platforms (PC/Wii only?). Doesn't look like there is a header field
     * (comparing many BAOs from different platforms of the same games) so it's autodetected.
     *
     * Most types + tables are pretty much the same as SB (with config styles ported straight) but
     * now can "prefetch" part of the data (signaled by a size in the header, or perhaps a flag but
     * looks too erratic). The header points to a external/stream ID, and with prefetch enabled part
     * of the audio is in an internal/memory ID, and must join both during reads to get the full
     * stream. Prefetch may be used in some platforms of a game only (ex. AC1 PC does while PS3
     * doesn't, while Scott Pilgrim always does)
     */

    bao->allowed_types[0x01] = 1;
    bao->allowed_types[0x05] = 1;
    bao->allowed_types[0x06] = 1;

    /* absolute */
    bao->cfg.bao_class      = 0x20;

    /* relative to header_skip */
    bao->cfg.header_id      = 0x00;
    bao->cfg.header_type    = 0x04;

    version = bao->version;

    /* 2 configs with same ID, autodetect */
    if (version == 0x00220015) {
        off_t header_size = 0x40 + read_32bitLE(0x04, sf); /* first is always LE */

        /* next BAO uses machine endianness, entry should always exist
         * (maybe should use project BAO to detect?) */
        if (guess_endianness32bit(header_size + 0x04, sf)) {
            version |= 0xFF00; /* signal Wii=BE, but don't modify original */
        }
    }


    /* config per version*/
    switch(version) {

        case 0x001B0100: /* Assassin's Creed (PS3/X360/PC)-atomic-forge */
      //case 0x001B0100: /* My Fitness Coach (Wii)-atomic-forge */
      //case 0x001B0100: /* Your Shape featuring Jenny McCarthy (Wii)-atomic-forge */
            config_bao_entry(bao, 0xA4, 0x28); /* PC: 0xA8, PS3/X360: 0xA4 */

            config_bao_audio_b(bao, 0x08, 0x1c, 0x28, 0x34, 1, 1); /* 0x2c: prefetch flag? */
            config_bao_audio_m(bao, 0x44, 0x48, 0x50, 0x58, 0x64, 0x74);
            bao->cfg.audio_interleave = 0x10;
            bao->cfg.audio_fix_psx_samples = 1;

            config_bao_sequence(bao, 0x2c, 0x20, 0x1c, 0x14);

            config_bao_layer_m(bao, 0x4c, 0x20, 0x2c, 0x44, 0x00, 0x50, 0x00, 0x00, 1); /* stream size: 0x48? */
            config_bao_layer_e(bao, 0x30, 0x00, 0x04, 0x08, 0x10);

            config_bao_silence_f(bao, 0x1c);

            bao->cfg.codec_map[0x00] = RAW_XMA1;
            bao->cfg.codec_map[0x02] = RAW_PSX;
            bao->cfg.codec_map[0x03] = UBI_IMA;
            bao->cfg.codec_map[0x04] = FMT_OGG;
            bao->cfg.codec_map[0x05] = RAW_XMA1; /* same but streamed? */
            bao->cfg.codec_map[0x07] = RAW_AT3_105;

            bao->cfg.file_type = UBI_FORGE;
            return 1;

        case 0x001B0200: /* Beowulf (PS3/X360)-atomic-bin+fat */
            config_bao_entry(bao, 0xA0, 0x24);

            config_bao_audio_b(bao, 0x08, 0x1c, 0x28, 0x34, 1, 1); /* 0x2c: prefetch flag? */
            config_bao_audio_m(bao, 0x44, 0x48, 0x50, 0x58, 0x64, 0x74);
            bao->cfg.audio_interleave = 0x10;
            bao->cfg.audio_fix_psx_samples = 1;

            config_bao_sequence(bao, 0x2c, 0x20, 0x1c, 0x14);

            config_bao_layer_m(bao, 0x4c, 0x20, 0x2c, 0x44, 0x00, 0x50, 0x00, 0x00, 1); /* stream size: 0x48? */
            config_bao_layer_e(bao, 0x30, 0x00, 0x04, 0x08, 0x10);

            config_bao_silence_f(bao, 0x1c);

            bao->cfg.codec_map[0x00] = RAW_XMA1;
            bao->cfg.codec_map[0x02] = RAW_PSX;
            bao->cfg.codec_map[0x03] = UBI_IMA;
            bao->cfg.codec_map[0x04] = FMT_OGG;
            bao->cfg.codec_map[0x07] = RAW_AT3_105;

            bao->cfg.file_type = UBI_FAT;
            return 1;

        case 0x001F0008: /* Rayman Raving Rabbids: TV Party (Wii)-package */
        case 0x001F0010: /* Prince of Persia 2008 (PC/PS3/X360)-atomic-forge, Far Cry 2 (PS3)-atomic-dunia? */
        case 0x001F0011: /* Naruto: The Broken Bond (X360)-package */
        case 0x0021000C: /* Splinter Cell: Conviction (E3 2009 Demo)(X360)-package */
        case 0x0022000D: /* Just Dance (Wii)-package, We Dare (PS3/Wii)-package */
        case 0x0022FF15: /* James Cameron's Avatar: The Game (Wii)-package */
        case 0x0022001B: /* Prince of Persia: The Forgotten Sands (Wii)-package */
            config_bao_entry(bao, 0xA4, 0x28); /* PC/Wii: 0xA8 */

            config_bao_audio_b(bao, 0x08, 0x1c, 0x28, 0x34, 1, 1);
            config_bao_audio_m(bao, 0x44, 0x4c, 0x54, 0x5c, 0x64, 0x74); /* cues: 0x68, 0x6c */

            config_bao_sequence(bao, 0x2c, 0x20, 0x1c, 0x14);

            config_bao_layer_m(bao, 0x00, 0x20, 0x2c, 0x44, 0x4c, 0x50, 0x54, 0x58, 1); /* 0x1c: id-like, 0x3c: prefetch flag? */
            config_bao_layer_e(bao, 0x28, 0x00, 0x04, 0x08, 0x10);

            config_bao_silence_f(bao, 0x1c);

            bao->cfg.codec_map[0x01] = RAW_PCM;
            bao->cfg.codec_map[0x03] = UBI_IMA;
            bao->cfg.codec_map[0x04] = FMT_OGG;
            bao->cfg.codec_map[0x05] = RAW_XMA1;
            bao->cfg.codec_map[0x07] = RAW_AT3_105;
            bao->cfg.codec_map[0x09] = RAW_DSP;

            bao->cfg.file_type = UBI_FORGE_b;

            if (version == 0x0022000D) /* Just Dance (Wii) oddity */
                bao->cfg.audio_ignore_resource_size = 1;
            if (version == 0x0022000D) /* We Dare (Wii) */
                config_bao_audio_c(bao, 0x68, 0x78);

            return 1;

        case 0x00220015: /* James Cameron's Avatar: The Game (PSP)-package */
        case 0x0022001E: /* Prince of Persia: The Forgotten Sands (PSP)-package */
            config_bao_entry(bao, 0x84, 0x28); /* PSP: 0x84 */

            config_bao_audio_b(bao, 0x08, 0x1c, 0x20, 0x20, (1 << 2), (1 << 5)); /* (1 << 4): prefetch flag? */
            config_bao_audio_m(bao, 0x28, 0x30, 0x38, 0x40, 0x48, 0x58);

            config_bao_layer_m(bao, 0x00, 0x20, 0x24, 0x34, 0x3c, 0x40, 0x00, 0x00, (1 << 2)); /* 0x1c: id-like */
            config_bao_layer_e(bao, 0x28, 0x00, 0x04, 0x08, 0x10);

            bao->cfg.codec_map[0x06] = RAW_PSX;
            bao->cfg.codec_map[0x07] = FMT_AT3;

            return 1;

        case 0x00230008: /* Splinter Cell: Conviction (X360/PC)-package */
            config_bao_entry(bao, 0xB4, 0x28); /* PC: 0xB8, X360: 0xB4 */

            config_bao_audio_b(bao, 0x08, 0x24, 0x38, 0x44, 1, 1);
            config_bao_audio_m(bao, 0x54, 0x5c, 0x64, 0x6c, 0x74, 0x84);

            config_bao_sequence(bao, 0x34, 0x28, 0x24, 0x14);

            config_bao_layer_m(bao, 0x00, 0x28, 0x3c, 0x54, 0x5c, 0x00 /*0x60?*/, 0x00, 0x00, 1); /* 0x24: id-like */
            config_bao_layer_e(bao, 0x30, 0x00, 0x04, 0x08, 0x18);
            bao->cfg.layer_ignore_error = 1; //todo last sfx layer (bass) may have smaller sample rate

            bao->cfg.codec_map[0x01] = RAW_PCM;
            bao->cfg.codec_map[0x02] = UBI_IMA;
            bao->cfg.codec_map[0x03] = FMT_OGG;
            bao->cfg.codec_map[0x04] = RAW_XMA2_OLD;

            return 1;

        case 0x00250108: /* Scott Pilgrim vs the World (PS3/X360)-package */
        case 0x0025010A: /* Prince of Persia: The Forgotten Sands (PS3/X360)-atomic-forge */
        case 0x00250119: /* Shaun White Skateboarding (Wii)-package */
        case 0x0025011D: /* Shaun White Skateboarding (PC/PS3)-atomic-forge */
            config_bao_entry(bao, 0xB4, 0x28); /* PC: 0xB0, PS3/X360: 0xB4, Wii: 0xB8 */

            if (bao->version == 0x0025011D)
                bao->cfg.header_less_le_flag = 1;

            config_bao_audio_b(bao, 0x08, 0x24, 0x2c, 0x38, 1, 1);
            config_bao_audio_m(bao, 0x48, 0x50, 0x58, 0x60, 0x68, 0x78);
            bao->cfg.audio_interleave = 0x10;

            config_bao_sequence(bao, 0x34, 0x28, 0x24, 0x14);

            config_bao_layer_m(bao, 0x00, 0x28, 0x30, 0x48, 0x50, 0x54, 0x58, 0x5c, 1); /* 0x24: id-like */
            config_bao_layer_e(bao, 0x30, 0x00, 0x04, 0x08, 0x18);
            //todo some SPvsTW layers look like should loop (0x30 flag?)
            //todo some POP layers have different sample rates (ambience)

            config_bao_silence_f(bao, 0x24);

            bao->cfg.codec_map[0x01] = RAW_PCM;
            bao->cfg.codec_map[0x02] = UBI_IMA;
            bao->cfg.codec_map[0x03] = FMT_OGG;
            bao->cfg.codec_map[0x04] = RAW_XMA2_NEW;
            bao->cfg.codec_map[0x05] = RAW_PSX;
            bao->cfg.codec_map[0x06] = RAW_AT3;
            if (bao->version == 0x0025010A) /* no apparent flag */
                bao->cfg.codec_map[0x06] = RAW_AT3_105;

            bao->cfg.file_type = UBI_FORGE_b;
            
            return 1;

        case 0x00260000: /* Michael Jackson: The Experience (X360)-package */
            config_bao_entry(bao, 0xB8, 0x28);

            config_bao_audio_b(bao, 0x08, 0x28, 0x30, 0x3c, 1, 1); //loop?
            config_bao_audio_m(bao, 0x4c, 0x54, 0x5c, 0x64, 0x6c, 0x7c);

            config_bao_layer_m(bao, 0x00, 0x2c, 0x34,  0x4c, 0x54, 0x58,  0x00, 0x00, 1);
            config_bao_layer_e(bao, 0x34, 0x00, 0x04, 0x08, 0x1c);

            bao->cfg.codec_map[0x03] = FMT_OGG;
            bao->cfg.codec_map[0x04] = RAW_XMA2_NEW;

            bao->cfg.audio_ignore_resource_size = 1; /* leave_me_alone.pk */

            return 1;

        case 0x00270102: /* Drawsome (Wii)-package */
            config_bao_entry(bao, 0xAC, 0x28);

            config_bao_audio_b(bao, 0x08, 0x28, 0x2c, 0x38, 1, 1);
            config_bao_audio_m(bao, 0x44, 0x4c, 0x54, 0x5c, 0x64, 0x70);

            config_bao_sequence(bao, 0x38, 0x2c, 0x28, 0x14);

            bao->cfg.codec_map[0x02] = UBI_IMA;

            return 1;

        case 0x00280303: /* Tom Clancy's Ghost Recon Future Soldier (PC/PS3)-package */
            config_bao_entry(bao, 0xBC, 0x28); /* PC/PS3: 0xBC */

            config_bao_audio_b(bao, 0x08, 0x38, 0x3c, 0x48, 1, 1);
            config_bao_audio_m(bao, 0x54, 0x5c, 0x64, 0x6c, 0x74, 0x80);

            config_bao_sequence(bao, 0x48, 0x3c, 0x38, 0x14);

            config_bao_layer_m(bao, 0x00, 0x3c, 0x44, 0x58, 0x60, 0x64, 0x00, 0x00, 1);
            config_bao_layer_e(bao, 0x2c, 0x00, 0x04, 0x08, 0x1c);
            bao->cfg.layer_ignore_error = 1; //todo some layer sample rates don't match
            //todo some files have strange prefetch+stream of same size (2 segments?), ex. CEND_30_VOX.lpk

            config_bao_silence_f(bao, 0x38);

            bao->cfg.codec_map[0x01] = RAW_PCM;
            bao->cfg.codec_map[0x02] = UBI_IMA; /* v6 */
            bao->cfg.codec_map[0x04] = FMT_OGG;
            bao->cfg.codec_map[0x07] = RAW_AT3; //todo some layers use AT3_105

            bao->cfg.file_type = UBI_FORGE_b;
            return 1;

        case 0x001C0000: /* Lost: Via Domus (PS3)-atomic-gear */
            /* same as 0x001B0100 except:
             * - base 0xA0, skip 0x24, name style %08x.bao (not .sbao?) */
        case 0x001D0A00: /* Shaun White Snowboarding (PSP)-atomic-opal */
        case 0x00220017: /* Avatar (PS3)-atomic/spk */
        case 0x00220018: /* Avatar (PS3)-atomic/spk */
        case 0x00260102: /* Prince of Persia Trilogy HD (PS3)-package-gear */
            /* similar to 0x00250108 but most values are moved +4
             * - base 0xB8, skip 0x28 */
        case 0x00280306: /* Far Cry 3: Blood Dragon (X360)-atomic-hashed */
        case 0x00290106: /* Splinter Cell: Blacklist (PS3)-atomic-gear */
            /* quite different, lots of flags and random values
             * - base varies per type (0xF0=audio), skip 0x20
             * - 0x74: type, 0x78: channels, 0x7c: sample rate, 0x80: num_samples
             * - 0x94: stream id? 0x9C: extra size */
        case 0x002A0300: /* Watch Dogs (Wii U) */
            /* similar to SC:B */
        default: /* others possibly using BAO: Watch_Dogs, Far Cry Primal, Far Cry 4 */
            vgm_logi("UBI BAO: unknown BAO version %08x\n", bao->version);
            return 0;
    }

    return 0;
}
