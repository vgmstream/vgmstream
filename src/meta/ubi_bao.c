#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/endianness.h"
#include "ubi_bao_streamfile.h"

// BAO's massive confg and variations are handled here
#include "ubi_bao_config.h"

#define BAO_MIN_VERSION 0x1B
#define BAO_MAX_VERSION 0x2B

#define BAO_MAX_LAYER_COUNT 16      // arbitrary max
#define BAO_MAX_CHAIN_COUNT 128     // POP:TFS goes up to ~100


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
    uint32_t header_skip;       // base header size
    uint32_t header_size;       // normal base size (not counting extra tables)
    uint32_t extra_size;        // extra tables size

    uint32_t stream_id;         // stream or memory BAO's id
    uint32_t stream_size;
    uint32_t stream_offset;
    uint32_t prefetch_id;       // memory BAO's id (may be the same as stream_id, but in other location)
    uint32_t prefetch_size;
    uint32_t prefetch_offset;

    uint32_t memory_skip;
    uint32_t stream_skip;

    bool is_stream;             // streamed data (external file) or memory data otherwise (external or internal)
    bool is_prefetch;           // memory data is to be used as part of the stream

    /* sound info */
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

    /* related info */
    char resource_name[255];
    char readable_name[255];

    int classes[16];
    int types[16];
} ubi_bao_header_t;

static bool parse_header(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset);
static bool parse_bao(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset, int target_subsong);
static bool parse_pk(ubi_bao_header_t* bao, STREAMFILE* sf);
static VGMSTREAM* init_vgmstream_ubi_bao_header(ubi_bao_header_t* bao, STREAMFILE* sf);
static STREAMFILE* setup_bao_streamfile(ubi_bao_header_t* bao, STREAMFILE* sf);
static STREAMFILE* open_atomic_bao(ubi_bao_file_t file, uint32_t file_id, bool is_stream, STREAMFILE* sf);
static bool find_package_bao(uint32_t target_id, STREAMFILE* sf, off_t* p_offset, size_t* p_size);

static void build_readable_name(char* buf, size_t buf_size, ubi_bao_header_t* bao);


/* .PK - packages with BAOs from Ubisoft's sound engine ("DARE") games in 2008+ */
VGMSTREAM* init_vgmstream_ubi_bao_pk(STREAMFILE* sf) {

    /* checks */
    if (read_u8(0x00, sf) != 0x01)
        return NULL;
    if (read_u8(0x01, sf) < BAO_MIN_VERSION || read_u8(0x01, sf) > BAO_MAX_VERSION)
        return NULL;

    if (!check_extensions(sf, "pk,lpk,cpk"))
        return NULL;

    /* "Package" .pk+spk (or .lpk+lspk for localized), a database-like format evolved from Ubi sbN/smN.
     * .pk has an index pointing to memory BAOs and tables with external stream BAOs in .spk. */
    ubi_bao_header_t bao = {0};
    bao.archive = ARCHIVE_PK;

     /* main parse */
    if (!parse_pk(&bao, sf))
        return NULL;

    build_readable_name(bao.readable_name, sizeof(bao.readable_name), &bao);
    return init_vgmstream_ubi_bao_header(&bao, sf);
}

/* .BAO - single BAO files from Ubisoft's sound engine ("DARE") games in 2007+ */
VGMSTREAM* init_vgmstream_ubi_bao_atomic(STREAMFILE* sf) {

    /* checks */
    if (read_u8(0x00, sf) != 0x01 && read_u8(0x00, sf) != 0x02) /* 0x01=AC1, 0x02=POP2008+ */
        return NULL;
    if (read_u8(0x01, sf) < BAO_MIN_VERSION || read_u8(0x01, sf) > BAO_MAX_VERSION)
        return NULL;

    if (!check_extensions(sf, "bao,"))
        return NULL;

    /* "Atomic" BAO found in .forge and similar bigfiles (rarely loose, too). The bigfile acts as
     * an index, but since BAOs reference each other by id and are named by it we can extract them.
     * Extension is .bao/sbao or extensionless in some games. */
    ubi_bao_header_t bao = {0};
    bao.archive = ARCHIVE_ATOMIC;

    uint32_t version = read_u32be(0x00, sf) & 0x00FFFFFF;
    if (!ubi_bao_config_version(&bao.cfg, sf, version))
        return NULL;

    /* main parse */
    if (!parse_bao(&bao, sf, 0x00, 1))
        return NULL;

    build_readable_name(bao.readable_name, sizeof(bao.readable_name), &bao);
    return init_vgmstream_ubi_bao_header(&bao, sf);
}

#if 0
/* .SPK - mini package with BAOs, used in Dunia engine games [Avatar (multi), Far Cry 4 (multi)] */
VGMSTREAM* init_vgmstream_ubi_bao_spk(STREAMFILE* sf) {
    ubi_bao_header_t bao = {0};

    /* checks */
    uint32_t header_id = read_u32le(0x00, sf); //always LE
    if ((header_id & 0xFFFFFF00) != get_id32be("SPK\0"))
        return NULL;

    if (!check_extensions(sf, "spk"))
        return NULL;

    /* .spk is a simpler package (unrelated from .pk+.spk), possibly evolved from the .FAT+BIN index
     * found in some Yeti engine games. It has loose .sbao for streams, which aren't named like 5xxxxxx. */
    ubi_bao_header_t bao = {0};
    bao.archive = ARCHIVE_SPK; //TODO define SPK1 or SPK4
    bao.archive_version = header_id & 0xFF;

    /* - 0x00: 0xNN4B5053 ("SPK\N" LE) (N: v1=Avatar/FC2, v4=FC3/FC4)
     * - 0x04: BAO count
     * - 0x08: BAO ids inside (0x04 * BAO count)
     * - (v1) per BAO:
     *   - 0x00: table count
     *   - 0x04: ids related to this BAO? (0x04 * table count)
     *   - 0x08/NN: BAO size
     *   - 0x0c/NN+: BAO data up to size + padding to 0x04
     * - (v4) per BAO:
     *   - 0x00: BAO size
     *   - 0xNN: BAO data up to size
     *
     * BAOs can be inside .spk (memory) or external .sbao, with hashed names
     * be considered a type of bigfile.
     */

    /* main parse */
    if (!parse_spk(&bao, sf))
        return NULL;

    build_readable_name(bao.readable_name, sizeof(bao.readable_name), &bao);
    return init_vgmstream_ubi_bao_header(&bao, sf);
}
#endif

/* ************************************************************************* */

static VGMSTREAM* init_vgmstream_ubi_bao_base(ubi_bao_header_t* bao, STREAMFILE* sf_head, STREAMFILE* sf_data) {
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
            vgmstream->coding_type = coding_PCM16LE; // always LE
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case RAW_PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;
            break;

        case RAW_PSX_new:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = bao->stream_size / bao->channels;
            break;

        case RAW_DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = bao->stream_size / bao->channels;

            /* mini DSP header (first 0x10 seem to contain DSP header fields like nibbles and format) */
            dsp_read_coefs_be(vgmstream, sf_head, bao->header_offset + bao->header_size + bao->extra_size + 0x10, 0x40);
            dsp_read_hist_be (vgmstream, sf_head, bao->header_offset + bao->header_size + bao->extra_size + 0x34, 0x40); // after gain/initial ps
            break;

#ifdef VGM_USE_FFMPEG
        //TODO: Ubi XMA1 (raw or fmt) is a bit strange, FFmpeg decodes some frames slightly wrong (see Ubi SB)
        case RAW_XMA1_mem:
        case RAW_XMA1_str:
        case RAW_XMA2_old:
        case RAW_XMA2_new: {
            size_t chunk_size, data_size;
            off_t chunk_offset;
            STREAMFILE* sf_xmah = NULL;
            STREAMFILE* sf_xmad = NULL;

            switch(bao->codec) {
                case RAW_XMA1_mem:
                case RAW_XMA1_str:  chunk_size = 0x20; break;
                case RAW_XMA2_old:  chunk_size = 0x2c; break;
                case RAW_XMA2_new:  chunk_size = 0x34; break;
                default: goto fail;
            }

            // XMA header chunk is stored in different places, setup and also find actual data start
            // - audio memory: in header
            // - audio stream: in data
            // - layer memory: in layer mem, right before audio (technically in header...)
            // - layer stream: in data
            //
            // Location in old versions also depends on codec:
            // - atomic v1 + xma1_mem: chunk in header
            // - atomic v1 + xma1_str: chunk in data (memory BAO or stream BAO)
            // - atomic v2 + xma1_mem: (not seen)
            // - atomic v2 + xma1_str: chunk in header (memory BAO)
            // - atomic v2 + xma1_str: chunk in data (stream BAO)
            // atomic v1 layers seem to only use RAW_XMA1_STR

            if (bao->is_stream || bao->type == TYPE_LAYER || (bao->cfg.v1_bao && bao->codec == RAW_XMA1_str)) {
                uint8_t flag, bits_per_frame;
                uint32_t sec1_num, sec2_num, sec3_num;
                size_t header_size, frame_size;

                /* skip custom XMA seek? table after standard XMA/fmt header chunk */
                off_t header_offset = start_offset + chunk_size;
                if (bao->codec == RAW_XMA1_mem || bao->codec == RAW_XMA1_str) {
                    flag        = read_u8(header_offset + 0x00, sf_data);
                    sec2_num    = read_u32be(header_offset + 0x04, sf_data); // number of XMA frames
                    frame_size  = 0x800;
                    sec1_num    = read_u32be(header_offset + 0x08, sf_data);
                    sec3_num    = read_u32be(header_offset + 0x0c, sf_data);
                    header_size = chunk_size + 0x10;
                }
                else {
                    flag        = read_u8(header_offset + 0x00, sf_data);
                    sec2_num    = read_u32be(header_offset + 0x04, sf_data); // number of XMA frames
                    frame_size  = 0x800; //read_u32be(header_offset + 0x08, sf_data); // not always present?
                    sec1_num    = read_u32be(header_offset + 0x0c, sf_data);
                    sec3_num    = read_u32be(header_offset + 0x10, sf_data); // assumed
                    header_size = chunk_size + 0x14;
                }

                bits_per_frame = 4;
                if (flag == 0x02 || flag == 0x04)
                    bits_per_frame = 2;
                else if (flag == 0x08)
                    bits_per_frame = 1;

                header_size += sec1_num * 0x04;
                header_size += align_size_to_block(sec2_num * bits_per_frame, 32) / 8; // bitstream seek table?
                header_size += sec3_num * 0x08;

                sf_xmah = sf_data;
                sf_xmad = sf_data;
                chunk_offset = 0x00;
                start_offset += header_size;
                data_size = sec2_num * frame_size;
            }
            else {
                if (bao->archive == ARCHIVE_ATOMIC) {
                    // atomic BAOs have xma header after extradata (XMA1_MEM in v1_bao or XMA1_STR / XMA2* otherwise)
                    sf_xmah = sf_head;
                    sf_xmad = sf_data;
                }
                else {
                    // package BAOs have xma header + memory data
                    sf_xmah = sf_head;
                    sf_xmad = sf_head;
                }
                chunk_offset = bao->header_offset + bao->header_size + bao->extra_size;
                start_offset = 0x00;
                data_size = bao->stream_size;
            }

            vgmstream->codec_data = init_ffmpeg_xma_chunk_split(sf_xmah, sf_xmad, start_offset, data_size, chunk_offset, chunk_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->stream_size = data_size;

            xma_fix_raw_samples(vgmstream, sf_data, start_offset,data_size,0, 0,0);
            break;
        }

        case RAW_AT3_105:
        case RAW_AT3: {
            int block_align = (bao->codec == RAW_AT3_105 ? 0x98 : 0xc0) * vgmstream->channels;
            int encoder_delay = 0; // num_samples is full bytes-to-samples (unlike FMT_AT3) and comparing X360 vs PS3 games seems ok

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(sf_data, start_offset,vgmstream->stream_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case FMT_AT3: {
            vgmstream->codec_data = init_ffmpeg_atrac3_riff(sf_data, start_offset, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
#ifdef VGM_USE_VORBIS
        case FMT_OGG: {
            vgmstream->codec_data = init_ogg_vorbis(sf_data, start_offset, bao->stream_size, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_OGG_VORBIS;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = bao->num_samples; /* same as Ogg samples */
            break;
        }
#endif
        default:
            VGM_LOG("UBI BAO: missing codec implementation\n");
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf_data, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);

    VGM_LOG("UBI BAO: failed init base\n");
    return NULL;
}

static VGMSTREAM* init_vgmstream_ubi_bao_audio(ubi_bao_header_t* bao, STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_data = NULL;

    sf_data = setup_bao_streamfile(bao, sf);
    if (!sf_data) goto fail;

    vgmstream = init_vgmstream_ubi_bao_base(bao, sf, sf_data);
    if (!vgmstream) goto fail;

    close_streamfile(sf_data);
    return vgmstream;

fail:
    close_streamfile(sf_data);
    close_vgmstream(vgmstream);

    VGM_LOG("UBI BAO: failed init audio\n");
    return NULL;
}

static VGMSTREAM* init_vgmstream_ubi_bao_layer(ubi_bao_header_t* bao, STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    layered_layout_data* data = NULL;
    STREAMFILE* temp_sf = NULL;
    STREAMFILE* sf_data = NULL;
    size_t full_stream_size = bao->stream_size;
    int total_channels = 0;

    sf_data = setup_bao_streamfile(bao, sf);
    if (!sf_data) goto fail;

    /* init layout */
    data = init_layout_layered(bao->layer_count);
    if (!data) goto fail;

    /* open all layers and mix */
    for (int i = 0; i < bao->layer_count; i++) {

        /* prepare streamfile from a single layer section */
        temp_sf = setup_ubi_bao_streamfile(sf_data, 0x00, full_stream_size, i, bao->layer_count, bao->cfg.big_endian);
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

    close_streamfile(sf_data);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_streamfile(sf_data);
    if (vgmstream)
        close_vgmstream(vgmstream);
    else
        free_layout_layered(data);

    VGM_LOG("UBI BAO: failed init layer\n");
    return NULL;
}

static VGMSTREAM* init_vgmstream_ubi_bao_sequence(ubi_bao_header_t* bao, STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_chain = NULL;
    segmented_layout_data* data = NULL;


    /* init layout */
    data = init_layout_segmented(bao->sequence_count);
    if (!data) goto fail;

    bao->channels = 0;
    bao->num_samples = 0;

    /* open all segments and mix */
    for (int i = 0; i < bao->sequence_count; i++) {
        ubi_bao_header_t temp_bao = *bao; /* memcpy'ed */
        int entry_id = bao->sequence_chain[i];

        if (bao->archive == ARCHIVE_ATOMIC) {
            /* open memory audio BAO */
            sf_chain = open_atomic_bao(bao->cfg.file, entry_id, false, sf);
            if (!sf_chain) {
                VGM_LOG("UBI BAO: chain BAO %08x not found\n", entry_id);
                goto fail;
            }

            /* parse BAO */
            if (!parse_header(&temp_bao, sf_chain, 0x00))
                goto fail;

            /* will open its companion BAOs later */
            close_streamfile(sf_chain);
            sf_chain = NULL;
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

        if (temp_bao.type == TYPE_NONE || temp_bao.type == TYPE_SEQUENCE) {
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

    //TODO: Rabbids 0x200000bd.pk#24 mixes 2ch audio with 2ch*3 layers

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
    close_streamfile(sf_chain);
    if (vgmstream)
        close_vgmstream(vgmstream);
    else
        free_layout_segmented(data);

    VGM_LOG("UBI BAO: failed init sequence\n");
    return NULL;
}


static VGMSTREAM* init_vgmstream_ubi_bao_silence(ubi_bao_header_t* bao) {
    VGMSTREAM* vgmstream = NULL;

    /* by default silences don't have settings */
    int channels = bao->channels;
    if (channels == 0)
        channels = 2;
    int sample_rate = bao->sample_rate;
    if (sample_rate == 0)
        sample_rate = 48000;
    int32_t num_samples = bao->duration * sample_rate;


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


static VGMSTREAM* init_vgmstream_ubi_bao_header(ubi_bao_header_t* bao, STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;

    if (bao->total_subsongs <= 0) {
        vgm_logi("UBI BAO: bank has no subsongs (ignore)\n");
        return NULL; // not uncommon
    }

    ;VGM_LOG("UBI BAO: target at %x, h_id=%08x, s_id=%08x, p_id=%08x\n", 
        bao->header_offset, bao->header_id, bao->stream_id, bao->prefetch_id);
    ;VGM_LOG("UBI BAO: stream=%x, size=%x, res=%s, stream=%i, prefetch=%i\n",
        bao->stream_offset, bao->stream_size, (bao->is_stream ? bao->resource_name : "internal"), bao->is_stream, bao->is_prefetch);
    ;VGM_LOG("UBI BAO: type=%i, header=%x, extra=%x, pre.of=%x, pre.sz=%x, codec=%i\n",
        bao->header_type, bao->header_size, bao->extra_size, bao->prefetch_offset, bao->prefetch_size, bao->header_type == 0x01 ? bao->stream_type : -1);


    switch(bao->type) {

        case TYPE_AUDIO:
            vgmstream = init_vgmstream_ubi_bao_audio(bao, sf);
            break;

        case TYPE_LAYER:
            vgmstream = init_vgmstream_ubi_bao_layer(bao, sf);
            break;

        case TYPE_SEQUENCE:
            vgmstream = init_vgmstream_ubi_bao_sequence(bao, sf);
            break;

        case TYPE_SILENCE:
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
static bool parse_pk(ubi_bao_header_t* bao, STREAMFILE* sf) {
    STREAMFILE* sf_index = NULL;
    STREAMFILE* sf_test = NULL;

    /* format: 0x01=package index, 0x02=package BAO */
    if (read_u8(0x00, sf) != 0x01)
        return NULL;
    // index and resources are always LE

    int target_subsong = sf->stream_index;
    if (target_subsong <= 0) target_subsong = 1;

    uint32_t version = read_u32be(0x00, sf) & 0x00FFFFFF;
    uint32_t index_size = read_u32le(0x04, sf); // can be 0, not including
    // 0x08: resource table offset, always found even if not used
    // 0x0c: always 0?
    // 0x10: unknown, null if no entries
    // 0x14: config/flags/time? (changes a bit between files), null if no entries
    // 0x18(10): file GUID? clones may share it
    // 0x24: unknown
    // 0x2c: unknown, may be same as 0x14, can be null
    // 0x30(10): parent GUID? may be same as 0x18, may be shared with other files
    // (the above values seem ignored by games, probably just info for their tools)

    if (!ubi_bao_config_version(&bao->cfg, sf, version))
        return false;


    int index_entries = index_size / 0x08;
    uint32_t index_header_size = 0x40;
    uint32_t bao_offset;

    /* pre-load to avoid too much I/O back and forth */
    if (index_size > (10000 * 0x08)) {
        VGM_LOG("BAO: index too big\n");
        return false;
    }

    /* use smaller I/O buffers for performance, as this read lots of small headers all over the place */
    sf_index = reopen_streamfile(sf, index_size);
    if (!sf_index) goto fail;

    sf_test = reopen_streamfile(sf, 0x100);
    if (!sf_test) goto fail;

    /* parse index to get target subsong N = Nth valid header BAO */
    bao_offset = index_header_size + index_size;
    for (int i = 0; i < index_entries; i++) {
      //uint32_t bao_id     = read_u32le(index_header_size + 0x08 * i + 0x00, sf_index);
        uint32_t bao_size   = read_u32le(index_header_size + 0x08 * i + 0x04, sf_index);

        //;VGM_LOG("UBI BAO: offset=%x, size=%x\n", bao_offset, bao_size);

        /* parse and continue to find out total_subsongs */
        if (!parse_bao(bao, sf_test, bao_offset, target_subsong))
            goto fail;

        bao_offset += bao_size; /* files simply concat BAOs */
    }

    //;VGM_LOG("UBI BAO: class "); {int i; for (i=0;i<16;i++){ VGM_ASSERT(bao->classes[i],"%02x=%i ",i,bao->classes[i]); }} VGM_LOG("\n");
    //;VGM_LOG("UBI BAO: types "); {int i; for (i=0;i<16;i++){ VGM_ASSERT(bao->types[i],"%02x=%i ",i,bao->types[i]); }} VGM_LOG("\n");

    close_streamfile(sf_index);
    close_streamfile(sf_test);
    return true;
fail:
    close_streamfile(sf_index);
    close_streamfile(sf_test);
    return false;
}

/* ************************************************************************* */

static void build_readable_name(char* buf, size_t buf_size, ubi_bao_header_t* bao) {
    const char* grp_name;
    const char* pft_name;
    const char* typ_name;
    const char* res_name;

    if (bao->type == TYPE_NONE)
        return;

    /* config */
    grp_name = "?";
    if (bao->archive == ARCHIVE_ATOMIC)
        grp_name = "atomic";
    if (bao->archive == ARCHIVE_PK)
        grp_name = "package";
    if (bao->archive == ARCHIVE_SPK)
        grp_name = "spackage";
    pft_name = bao->is_prefetch ? "p" : "n";
    typ_name = bao->is_stream ? "str" : "mem";

    if (bao->type == TYPE_SEQUENCE) {
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
        //if (!bao->is_atomic && bao->is_stream)
        //    res_name = bao->resource_name; /* too big? */
        //else
        //    res_name = NULL;
    }

    uint32_t h_id = bao->header_id;
    uint32_t s_id = bao->stream_id;
    uint32_t type = bao->header_type;

    /* .pk can contain many subsongs, we need something helpful
     * (best done right after subsong detection, since some sequence re-parse types) */
    if (res_name && res_name[0]) {
        snprintf(buf,buf_size, "%s/%s-%s/%02x-%08x/%08x/%s", grp_name, pft_name, typ_name, type, h_id, s_id, res_name);
    }
    else {
        snprintf(buf,buf_size, "%s/%s-%s/%02x-%08x/%08x", grp_name, pft_name, typ_name, type, h_id, s_id);
    }
}

static bool parse_type_audio(ubi_bao_header_t* bao, off_t offset, STREAMFILE* sf) {
    read_s32_t read_s32 = get_read_s32(bao->cfg.big_endian);
    read_u32_t read_u32 = get_read_u32(bao->cfg.big_endian);

    /* audio header */
    bao->type = TYPE_AUDIO;

    uint32_t h_offset = offset + bao->cfg.header_skip;
    bao->stream_size = read_u32(h_offset + bao->cfg.audio_stream_size, sf);
    bao->stream_id   = read_u32(h_offset + bao->cfg.audio_stream_id, sf);
    bao->is_stream   = read_s32(h_offset + bao->cfg.audio_stream_flag, sf) & bao->cfg.audio_stream_and;
    bao->loop_flag   = read_s32(h_offset + bao->cfg.audio_loop_flag, sf) & bao->cfg.audio_loop_and;
    bao->channels    = read_s32(h_offset + bao->cfg.audio_channels, sf);
    bao->sample_rate = read_s32(h_offset + bao->cfg.audio_sample_rate, sf);

    /* extra cue table, rare (found with XMA1/DSP) [Beowulf (X360), We Dare (Wii)] */
    uint32_t cues_size = 0;
    if (bao->cfg.audio_cue_count) {
        cues_size += read_u32(h_offset + bao->cfg.audio_cue_count, sf) * 0x08;
    }
    if (bao->cfg.audio_cue_labels) {
        cues_size += read_u32(h_offset + bao->cfg.audio_cue_labels, sf);
    }
    bao->extra_size = cues_size;

    /* prefetch data is in another internal BAO right after the base header */
    if (bao->cfg.audio_prefetch_size) {
        bao->prefetch_size = read_u32(h_offset + bao->cfg.audio_prefetch_size, sf);
        bao->is_prefetch = (bao->prefetch_size > 0);
    }

    if (bao->loop_flag) {
        bao->loop_start  = read_s32(h_offset + bao->cfg.audio_num_samples, sf);
        bao->num_samples = read_s32(h_offset + bao->cfg.audio_num_samples2, sf) + bao->loop_start;
    }
    else {
        bao->num_samples = read_s32(h_offset + bao->cfg.audio_num_samples, sf);
    }

    bao->stream_type = read_s32(h_offset + bao->cfg.audio_stream_type, sf);

    return true;
}

static bool parse_type_sequence(ubi_bao_header_t* bao, off_t offset, STREAMFILE* sf) {
    read_s32_t read_s32 = get_read_s32(bao->cfg.big_endian);
    read_u32_t read_u32 = get_read_u32(bao->cfg.big_endian);

    /* sequence chain */
    bao->type = TYPE_SEQUENCE;
    if (bao->cfg.sequence_entry_size == 0) {
        VGM_LOG("UBI BAO: sequence entry size not configured at %x\n", (uint32_t)offset);
        return false;
    }

    uint32_t h_offset = offset + bao->cfg.header_skip;
    bao->sequence_loop   = read_s32(h_offset + bao->cfg.sequence_sequence_loop, sf);
    bao->sequence_single = read_s32(h_offset + bao->cfg.sequence_sequence_single, sf);
    bao->sequence_count  = read_s32(h_offset + bao->cfg.sequence_sequence_count, sf);
    if (bao->sequence_count > BAO_MAX_CHAIN_COUNT) {
        VGM_LOG("UBI BAO: incorrect sequence count\n");
        return false;
    }

    /* get chain in extra table */
    uint32_t table_offset = offset + bao->header_size;
    for (int i = 0; i < bao->sequence_count; i++) {
        uint32_t entry_id = read_u32(table_offset + bao->cfg.sequence_entry_number, sf);

        bao->sequence_chain[i] = entry_id;

        table_offset += bao->cfg.sequence_entry_size;
    }

    return true;
}

static bool parse_type_layer(ubi_bao_header_t* bao, off_t offset, STREAMFILE* sf) {
    read_s32_t read_s32 = get_read_s32(bao->cfg.big_endian);
    read_u32_t read_u32 = get_read_u32(bao->cfg.big_endian);

    /* audio header */
    bao->type = TYPE_LAYER;
    if (bao->cfg.layer_entry_size == 0) {
        VGM_LOG("UBI BAO: layer entry size not configured at %x\n", (uint32_t)offset);
        return false;
    }

    uint32_t h_offset = offset + bao->cfg.header_skip;
    bao->layer_count    = read_s32(h_offset + bao->cfg.layer_layer_count, sf);
    bao->is_stream      = read_s32(h_offset + bao->cfg.layer_stream_flag, sf) & bao->cfg.layer_stream_and;
    bao->stream_size    = read_u32(h_offset + bao->cfg.layer_stream_size, sf);
    bao->stream_id      = read_u32(h_offset + bao->cfg.layer_stream_id, sf);
    if (bao->layer_count > BAO_MAX_LAYER_COUNT) {
        VGM_LOG("UBI BAO: incorrect layer count\n");
        return false;
    }

    if (bao->cfg.layer_prefetch_size) {
        bao->prefetch_size = read_u32(h_offset + bao->cfg.layer_prefetch_size, sf);
        bao->is_prefetch = (bao->prefetch_size > 0);
    }

    /* extra cue table (rare, has N variable-sized labels + cue table pointing to them) */
    uint32_t cues_size = 0;
    if (bao->cfg.layer_cue_labels) {
        cues_size += read_u32(h_offset + bao->cfg.layer_cue_labels, sf);
    }
    if (bao->cfg.layer_cue_count) {
        cues_size += read_u32(h_offset + bao->cfg.layer_cue_count, sf) * 0x08;
    }

    if (bao->cfg.layer_extra_size) {
        bao->extra_size = read_u32(h_offset + bao->cfg.layer_extra_size, sf);
    }
    else {
        bao->extra_size = cues_size + bao->layer_count * bao->cfg.layer_entry_size + cues_size;
    }

    /* get 1st layer header in extra table and validate all headers match */
    uint32_t table_offset = offset + bao->header_size + cues_size;
  //bao->channels       = read_s32(table_offset + bao->cfg.layer_channels, sf);
    bao->sample_rate    = read_s32(table_offset + bao->cfg.layer_sample_rate, sf);
    bao->stream_type    = read_s32(table_offset + bao->cfg.layer_stream_type, sf);
    bao->num_samples    = read_s32(table_offset + bao->cfg.layer_num_samples, sf);

    for (int i = 0; i < bao->layer_count; i++) {
        int channels    = read_s32(table_offset + bao->cfg.layer_channels, sf);
        int sample_rate = read_s32(table_offset + bao->cfg.layer_sample_rate, sf);
        int stream_type = read_s32(table_offset + bao->cfg.layer_stream_type, sf);
        int num_samples = read_s32(table_offset + bao->cfg.layer_num_samples, sf);
        if (bao->sample_rate != sample_rate || bao->stream_type != stream_type) {
            VGM_LOG("UBI BAO: layer headers don't match at %x\n", table_offset);

            if (!bao->cfg.layer_ignore_error) {
                return false;
            }
        }

        // uncommonly channels may vary per layer [Rayman Raving Rabbids: TV Party (Wii) ex. 0x22000cbc.pk]
        bao->layer_channels[i] = channels;

        // can be +-1
        if (bao->num_samples != num_samples && bao->num_samples + 1 == num_samples) {
            bao->num_samples -= 1;
        }

        table_offset += bao->cfg.layer_entry_size;
    }

    return true;
}

static bool parse_type_silence(ubi_bao_header_t* bao, off_t offset, STREAMFILE* sf) {
    read_f32_t read_f32 = get_read_f32(bao->cfg.big_endian);

    /* silence header */
    bao->type = TYPE_SILENCE;
    if (bao->cfg.silence_duration_float == 0) {
        VGM_LOG("UBI BAO: silence duration not configured at %x\n", (uint32_t)offset);
        return false;
    }

    uint32_t h_offset = offset + bao->cfg.header_skip;
    bao->duration = read_f32(h_offset + bao->cfg.silence_duration_float, sf);
    if (bao->duration <= 0.0f) {
        VGM_LOG("UBI BAO: bad duration %f at %x\n", bao->duration, (uint32_t)offset);
        return false;
    }

    return true;
}

/* adjust some common values */
static bool parse_values(ubi_bao_header_t* bao) {

    if (bao->type == TYPE_SEQUENCE || bao->type == TYPE_SILENCE)
        return true;

    /* common validations */
    if (bao->stream_size == 0) {
        VGM_LOG("UBI BAO: unknown stream_size at %x\n", bao->header_offset);
        return false;
    }

    /* set codec */
    if (bao->stream_type >= 0x10) {
        VGM_LOG("UBI BAO: unknown stream_type at %x\n", bao->header_offset);
        return false;
    }

    bao->codec = bao->cfg.codec_map[bao->stream_type];
    if (bao->codec == CODEC_NONE) {
        VGM_LOG("UBI BAO: unknown codec %x at %x\n", bao->stream_type, bao->header_offset);
        return false;
    }

    //TODO:: loop flag only?
    if (bao->type == TYPE_AUDIO && bao->codec == RAW_PSX && bao->cfg.v1_bao && bao->loop_flag) {
        bao->num_samples = bao->num_samples / bao->channels;
    }


    /* set prefetch id */
    if (bao->is_prefetch) {
        if (bao->cfg.v1_bao) {
            // header only defines stream_id, prefetch is implicitly a memory BAO
            // ex. AC1/Beowulf X360: stream=5NNNNNNN and memory prefetch=3NNNNNNN
            bao->prefetch_id = (bao->stream_id & 0x0FFFFFFF) | 0x30000000;
        }
        else {
            // shared id in index and resource table, or named atomic BAOs (that don't follow the 3NNNNNNN scheme)
            bao->prefetch_id = bao->stream_id;
        }
    }

    /* normalize base skips, as memory data (prefetch or not, atomic or package) can be
     * in a memory BAO after base header or audio layer BAO after the extra table */
    if (bao->stream_id == bao->header_id && (!bao->is_stream || bao->is_prefetch)) { /* layers with memory data */
        bao->memory_skip = bao->header_size + bao->extra_size;
        bao->stream_skip = bao->cfg.header_skip;
    }
    else {
        bao->memory_skip = bao->cfg.header_skip;
        bao->stream_skip = bao->cfg.header_skip;
    }


    return true;
}


/* set actual offsets in various places */
static bool parse_offsets(ubi_bao_header_t* bao, STREAMFILE* sf) {

    if (bao->type == TYPE_SEQUENCE || bao->type == TYPE_SILENCE)
        return true;

    if (!bao->is_stream && bao->is_prefetch) {
        VGM_LOG("UBI BAO: unexpected non-streamed prefetch at %x\n", bao->header_offset);
        return true;
    }

    /* Audio headers can point to audio data in multiple forms we must configure here:
     * - memory part (internal .pk BAO or separate atomic .bao)
     * - streamed part (external .spk BAO or separate atomic .sbao)
     * - prefetched memory part + streamed part (must join both during reads)
     *
     * Offsets are absolute (ignoring the index table that even .spk has) but point to BAO
     * base header start, that we must also skip to reach actual audio data.
     */

    if (bao->archive == ARCHIVE_ATOMIC) {
        if (bao->is_prefetch) {
            bao->prefetch_offset = bao->memory_skip;
        }

        if (bao->is_stream) {
            bao->stream_offset = bao->stream_skip;
        }
        else {
            bao->stream_offset = bao->memory_skip;
        }

        return true;
    }

    if (bao->archive == ARCHIVE_PK) {
        off_t bao_offset;
        size_t bao_size;

        if (bao->is_prefetch) {
            if (!find_package_bao(bao->prefetch_id, sf, &bao_offset, &bao_size)) {
                VGM_LOG("UBI BAO: expected prefetch id %08x not found\n", bao->prefetch_id);
                return false;
            }

            bao->prefetch_offset = bao_offset + bao->memory_skip;
            if (bao->prefetch_size + bao->memory_skip != bao_size) {
                VGM_LOG("UBI BAO: unexpected prefetch size %x vs %x\n", bao->prefetch_size + bao->memory_skip, bao_size);
                return false;
            }
        }

        if (bao->is_stream) {
            uint32_t resources_offset   = read_u32le(0x08, sf);
            int resources_count         = read_s32le(resources_offset + 0x00, sf);
            uint32_t strings_size       = read_u32le(resources_offset + 0x04, sf);

            /* parse resource table to external stream (may be empty, or exist even with nothing in the file) */
            uint32_t offset = resources_offset + 0x04+0x04 + strings_size;
            for (int i = 0; i < resources_count; i++) {
                uint32_t resource_id        = read_u32le(offset + 0x10 * i + 0x00, sf);
                uint32_t name_offset        = read_u32le(offset + 0x10 * i + 0x04, sf);
                uint32_t resource_offset    = read_u32le(offset + 0x10 * i + 0x08, sf);
                uint32_t resource_size      = read_u32le(offset + 0x10 * i + 0x0c, sf);

                if (resource_id == bao->stream_id) {
                    bao->stream_offset = resource_offset + bao->stream_skip;

                    read_string(bao->resource_name, sizeof(bao->resource_name), resources_offset + 0x04 + 0x04 + name_offset, sf);

                    if (bao->stream_size != resource_size - bao->stream_skip + bao->prefetch_size) {
                        VGM_LOG("UBI BAO: stream vs resource size mismatch at %x (res %x vs str=%x, skip=%x, pre=%x)\n", offset + 0x10 * i, resource_size, bao->stream_size, bao->stream_skip, bao->prefetch_size);

                        /* rarely resource has more data than stream (sometimes a few bytes, others +0x100000)
                         * sometimes short song versions, but not accessed? no samples/sizes/cues/etc in header seem to refer to that [Just Dance (Wii)]
                         * Michael Jackson The Experience also uses prefetch size + bad size (ignored) */
                        if (!bao->cfg.audio_ignore_resource_size && bao->prefetch_size)
                            return false;
                    }
                    break;
                }
            }

            if (bao->stream_offset == 0) {
                VGM_LOG("UBI BAO: expected external id %08x not found\n", bao->stream_id);
                return false;
            }
        }
        else {
            if (!find_package_bao(bao->stream_id, sf, &bao_offset, &bao_size)) {
                VGM_LOG("UBI BAO: expected internal id %08x not found\n", bao->stream_id);
                return false;
            }
            bao->stream_offset = bao_offset + bao->memory_skip;

            /* in some cases, stream size value from audio header can be bigger (~0x18)
             * than actual audio chunk o_O [Rayman Raving Rabbids: TV Party (Wii)] */
            if (bao->stream_size > bao_size - bao->memory_skip) {
                VGM_LOG("UBI BAO: bad stream size found: %x + %x vs %x\n", bao->stream_size, bao->memory_skip, bao_size);

                /* too big is usually bad config */
                if (bao->stream_size > bao_size + bao->header_size) {
                    VGM_LOG("UBI BAO: bad stream config at %x\n", bao->header_offset);
                    return false;
                }

                bao->stream_size = bao_size - bao->memory_skip;
            }
        }

        return true;
    }


    return false;
}

/* parse a single known header resource at offset (see config_bao for info) */
static bool parse_header(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset) {
    read_s32_t read_s32 = get_read_s32(bao->cfg.big_endian);
    read_u32_t read_u32 = get_read_u32(bao->cfg.big_endian);

    uint8_t header_format   = read_u8   (offset + 0x00, sf); // 0x01: atomic, 0x02: package
    uint32_t header_version = read_u32be(offset + 0x00, sf) & 0x00FFFFFF;
    if (bao->cfg.version != header_version || header_format > 0x03) {
        VGM_LOG("UBI BAO: mismatched header version at %x\n", (uint32_t)offset);
        return false;
    }

    bao->header_offset  = offset;

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
     * 0x1c: fixed value per type?
     */

    bao->header_id      = read_u32(offset + bao->cfg.header_skip + 0x00, sf);
    bao->header_type    = read_u32(offset + bao->cfg.header_skip + 0x04, sf);

    bao->header_size    = bao->cfg.header_base_size;

    /* hack for games with smaller size than standard
     * (can't use lowest size as other games also have extra unused field) */
    if (bao->cfg.header_less_le_flag && !bao->cfg.big_endian) {
        bao->header_size -= 0x04;
    }
    /* detect extra unused field in PC/Wii
     * (could be improved but no apparent flags or anything useful) */
    else if (get_streamfile_size(sf) > offset + bao->header_size) {
        // may read next BAO version, layer header, cues, resource table size, etc, always > 1
        int32_t end_field = read_s32(offset + bao->header_size, sf);

        if (end_field == -1 || end_field == 0 || end_field == 1) { // some count?
            bao->header_size += 0x04;
        }
    }

    switch(bao->header_type) {
        case 0x01:
            if (!parse_type_audio(bao, offset, sf))
                return false;
            break;
        case 0x05:
            if (!parse_type_sequence(bao, offset, sf))
                return false;
            break;
        case 0x06:
            if (!parse_type_layer(bao, offset, sf))
                return false;
            break;
        case 0x08:
            if (!parse_type_silence(bao, offset, sf))
                return false;
            break;
        default:
            VGM_LOG("UBI BAO: unknown header type at %x\n", (uint32_t)offset);
            return false;
    }

    if (!parse_values(bao))
        return false;

    if (!parse_offsets(bao, sf))
        return false;

    return true;
}

/* parse a full BAO, DARE's main audio format which can be inside other formats */
static bool parse_bao(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset, int target_subsong) {
    uint32_t bao_class, header_type;

    uint32_t bao_version = read_u32be(offset+0x00, sf); // force buffer read (check just in case it's optimized out)
    if (((bao_version >> 24) & 0xFF) > 0x02)
        return false;

    ubi_bao_config_endian(&bao->cfg, sf, offset);
    read_u32_t read_u32 = get_read_u32(bao->cfg.big_endian);

    bao_class = read_u32(offset + bao->cfg.bao_class, sf);
    if (bao_class & 0x0FFFFFFF) {
        VGM_LOG("UBI BAO: unknown class %x at %x\n", bao_class, (uint32_t)offset);
        return false;
    }

    bao->classes[(bao_class >> 28) & 0xF]++;
    if (bao_class != 0x20000000) // skip non-header classes
        return true;

    header_type = read_u32(offset + bao->cfg.header_skip + 0x04, sf);
    if (header_type > 9) {
        VGM_LOG("UBI BAO: unknown type %x at %x\n", header_type, (uint32_t)offset);
        return false;
    }

    //;VGM_ASSERT(header_type == 0x05 || header_type == 0x06, "UBI BAO: type %x at %x\n", header_type, (uint32_t)offset);

    bao->types[header_type]++;
    if (!bao->cfg.allowed_types[header_type])
        return true;

    bao->total_subsongs++;
    if (target_subsong != bao->total_subsongs)
        return true;

    if (!parse_header(bao, sf, offset))
        return false;

    return true;
}


/* find BAO within pk's index */
static bool find_package_bao(uint32_t target_id, STREAMFILE* sf, off_t* p_offset, size_t* p_size) {

    uint32_t index_size = read_u32le(0x04, sf);
    int index_entries = index_size / 0x08;
    uint32_t index_header_size = 0x40;

    /* parse index to get target BAO */
    uint32_t bao_offset = index_header_size + index_size;
    for (int i = 0; i < index_entries; i++) {
        uint32_t bao_id     = read_u32le(index_header_size + 0x08 * i + 0x00, sf);
        uint32_t bao_size   = read_u32le(index_header_size + 0x08 * i + 0x04, sf);

        if (bao_id == target_id) {
            if (p_offset) *p_offset = bao_offset;
            if (p_size) *p_size = bao_size;
            return true;
        }

        bao_offset += bao_size;
    }

    return false; // target not found
}

/* ************************************************************************* */

static const char* anvil_stream_baos[] = {
    // names as found in .forge, extensionless
    "Common_BAO_0x%08x",
    // mimics loading by internal ids
    "%08x.bao",     // Assassin's Creed / Shaun White Snowboarding (Windows Vista) exe
    "%08x.sbao",    // Shaun White Snowboarding (Windows Vista) exe
    // used?
    "Common_BAO_0x%08x.sbao",
    // language names, found in Assassin's Creed 1's exes and Shaun White Snowboarding (X360) exe in listed order
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
static const int anvil_stream_baos_count = sizeof(anvil_stream_baos) / sizeof(anvil_stream_baos[0]);

static const char* anvil_memory_baos[] = {
    "BAO_0x%08x",
    "Common_BAO_0x%08x",
    "%08x.bao",
    "BAO_0x%08x.bao",
};
static const int anvil_memory_baos_count = sizeof(anvil_memory_baos) / sizeof(anvil_memory_baos[0]);

static const char* common_memory_baos[] = {
    "%08x.bao",
};
static const int common_memory_baos_count = sizeof(common_memory_baos) / sizeof(common_memory_baos[0]);

#if 0
static const char* common_stream_baos[] = {
    "%08x.sbao",
};
static const int common_stream_baos_count = sizeof(common_stream_baos) / sizeof(common_stream_baos[0]);
#endif

// grotesque multi-opener until something matches, since engines are inconsistent
static STREAMFILE* open_atomic_bao_list(uint32_t file_id, const char** names, int count, char* buf, int buf_size, STREAMFILE* sf) {
    for (int i = 0; i < count; i++) {
        const char* format = names[i];

        snprintf(buf, buf_size, format, file_id);
        STREAMFILE* sf_bao = open_streamfile_by_filename(sf, buf);
        if (sf_bao) return sf_bao;
    }

    return NULL;
}

/* Opens a BAO's companion atomic BAO (memory or stream), often in different naming schemes.
 * Each engine+game handles it differently, but typically there is some generic bigfile that acts like 
 * a database/index, but must be extracted to get usable .bao/sbao or similar names. */
static STREAMFILE* open_atomic_bao(ubi_bao_file_t file, uint32_t file_id, bool is_stream, STREAMFILE* sf) {
    STREAMFILE* sf_bao = NULL;
    char buf[255];
    size_t buf_size = sizeof(buf);

    switch(file) {

        // Anvil engine exists inside .forge, uncompressed (stream BAOs) or compressed (subfiles per area with memory BAOs)
        case FILE_ANVIL_FORGE: {
            const char** names = is_stream ? anvil_stream_baos : anvil_memory_baos;
            int count = is_stream ? anvil_stream_baos_count : anvil_memory_baos_count;

            sf_bao = open_atomic_bao_list(file_id, names, count, buf, buf_size, sf);
            if (sf_bao) return sf_bao;

            goto fail;
        }

        // Yeti engine has BAOs in sound.fat + sound.bin + stream.bin, .fat being the index with BAO ids.
        // The X360 version has loose stream .bao instead of being inside stream.bin
        // .fat bigfile groups BAOs (possibly a simplified .forge or proto Dunia .spk).
        case FILE_YETI_FATBIN: {
            const char** names = common_memory_baos; // debug strings only use .bao
            int count = common_memory_baos_count;

            sf_bao = open_atomic_bao_list(file_id, names, count, buf, buf_size, sf);
            if (sf_bao) return sf_bao;

            goto fail;
        }

#if 0
        // Similar to Dunia's .fat+dat with the same CRC32 but unknown hash string, possibly sndbf:(something) or strmbf:(something)
        case FILE_YETI_GEAR: {
            break;
        }

        // Dunia engine uses .fat+.dat with hashed BAO names, though sometimes comes has companion .nfo files with original names.
        // See Gibbed.Dunia for the custom CRC32 hash algorithm
        case FILE_DUNIA_v5: {
            break;
        }

        // same as the above, but with a custom CRC64 hash
        case FILE_DUNIA_v9: {
            const char** names = is_stream ? common_stream_baos : common_memory_baos;
            int count = is_stream ? common_stream_baos_count : common_memory_baos_count;

            // try with extracted files (assumed to be in the same dir)
            sf_bao = open_atomic_bao_list(file_id, names, count, buf, buf_size, sf);
            if (!sf_bao) goto fail;

            // try again with hashed name (though should only use .sbao, .bao are part of dunia's .spk)
            //int len = snprintf(buf, buf_size, "soundbinary\%08x.%s", file_id, is_stream ? ".sbao" : "bao");

            //TODO: uint64_t hash = hash_dunia_crc64(buf, len);
            // assume lowercase, though depends on the extractor
            //snprintf(buf, buf_size, "%08x%08x", (uint32_t)(hash >> 32), (uint32_t)(hash >> 0));
            //sf_bao = open_streamfile_by_filename(sf, buf);
            //if (sf_bao) return sf_bao;

            goto fail;
        }
#endif

        default:
            goto fail;
    }

    return sf_bao; // may be NULL
fail:
    close_streamfile(sf_bao);

    vgm_logi("UBI BAO: failed opening atomic BAO id %08x\n", file_id);
    return NULL;
}

/* Create a usable streamfile by joining memory + streams if needed.
 *
 * Audio comes in "memory" and "streaming" BAOs, and when "prefetched" flag is
 * on we need to join memory and streamed parts as they're stored separately
 * (data may be split at any point and not at frame boundaries, too).
 *
 * The physical location of those depends on the format:
 * - file .bao: both in separate files, with different names per type
 * - bank .pk: memory BAO is in the pk, stream is in another file
 *
 * For some header BAOs, audio data can be in the same BAO too, which
 * can be considered memory BAO with different offset treatment.
 */
static STREAMFILE* setup_bao_streamfile(ubi_bao_header_t* bao, STREAMFILE* sf) {
    STREAMFILE* sf_pref = NULL;
    STREAMFILE* sf_main = NULL;

    // for pure memory/streams prefetch size is 0
    uint32_t real_stream_size = bao->stream_size - bao->prefetch_size;

    if (bao->archive == ARCHIVE_ATOMIC) {

        // prefetch part (memory file)
        if (bao->is_prefetch) {
            sf_pref = open_atomic_bao(bao->cfg.file, bao->prefetch_id, false, sf);
            if (!sf_pref) goto fail;

            sf_pref = open_clamp_streamfile_f(sf_pref, bao->prefetch_offset, bao->prefetch_size);
            if (!sf_pref) goto fail;
        }

        // memory or stream part (external file)
        if (!bao->is_prefetch || (bao->is_prefetch && real_stream_size != 0)) {
            bool is_companion_stream = bao->is_prefetch ? true : bao->is_stream;

            sf_main = open_atomic_bao(bao->cfg.file, bao->stream_id, is_companion_stream, sf);
            if (!sf_main) goto fail;

            sf_main = open_clamp_streamfile_f(sf_main, bao->stream_offset, real_stream_size);
            if (!sf_main) goto fail;
        }
    }

    if (bao->archive == ARCHIVE_PK) {
        // prefetch part (internal data)
        if (bao->is_prefetch) {
            sf_pref = open_wrap_streamfile(sf); // wrap current SF (memory BAO) to avoid it being closed
            if (!sf_pref) goto fail;

            sf_pref = open_clamp_streamfile_f(sf_pref, bao->prefetch_offset, bao->prefetch_size);
            if (!sf_pref) goto fail;
        }

        // stream part (external file)
        if ((bao->is_stream && !bao->is_prefetch) || (bao->is_prefetch && real_stream_size != 0)) {
            sf_main = open_streamfile_by_filename(sf, bao->resource_name); // name found in .pk's index
            if (!sf_main) {
                vgm_logi("UBI BAO: external file '%s' not found (put together)\n", bao->resource_name); 
                goto fail; 
            }

            sf_main = open_clamp_streamfile_f(sf_main, bao->stream_offset, real_stream_size);
            if (!sf_main) goto fail;
        }

        // memory part (internal data)
        if (!bao->is_stream && !bao->is_prefetch) {
            sf_main = open_wrap_streamfile(sf); // wrap current SF (memory BAO) to avoid it being closed
            if (!sf_main) goto fail;

            sf_main = open_clamp_streamfile_f(sf_main, bao->stream_offset, real_stream_size);
            if (!sf_main) goto fail;
        }
    }


    // memory or stream data with no prefetch, most common
    if (!sf_pref && sf_main) {
        return sf_main;
    }

    // prefetch only is weird but happens, streamed chunk is empty in this case
    if (sf_pref && !sf_main) {
        return sf_pref;
    }

    // join prefetch and memory as one
    if (sf_pref && sf_main) {
        STREAMFILE* temp_sf = NULL;
        STREAMFILE* sf_segments[2] = { sf_pref, sf_main };

        temp_sf = open_multifile_streamfile(sf_segments, 2);
        if (!temp_sf) goto fail;

        return temp_sf;
    }

    // shouldn't happen
    return NULL;
fail:
    close_streamfile(sf_pref);
    close_streamfile(sf_main);

    VGM_LOG("UBI BAO: failed streamfile setup\n");
    return NULL;
}
