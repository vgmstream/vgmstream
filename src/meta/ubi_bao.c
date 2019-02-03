#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "ubi_bao_streamfile.h"


typedef enum { CODEC_NONE = 0, UBI_IMA, RAW_PCM, RAW_PSX, RAW_XMA1, RAW_XMA2, RAW_AT3, RAW_AT3_105, FMT_AT3, RAW_DSP, FMT_OGG } ubi_bao_codec;
typedef enum { TYPE_NONE = 0, UBI_AUDIO, UBI_LAYER, UBI_SEQUENCE } ubi_bao_type;
typedef enum { FILE_NONE = 0, UBI_FORGE } ubi_bao_file;

typedef struct {
    size_t bao_class;
    size_t header_base_size;
    size_t header_skip;

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
    off_t audio_xma_offset; //todo remove, depends on extra table
    off_t audio_dsp_offset;
    size_t audio_interleave;
    int audio_channel_samples;
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
    size_t layer_extra_size;
    off_t layer_sample_rate;
    off_t layer_channels;
    off_t layer_stream_type;
    off_t layer_num_samples;
    off_t layer_entry_id;
    size_t layer_entry_size;
    int layer_external_and;

    off_t silence_duration_float;

    ubi_bao_codec codec_map[16];
    ubi_bao_file file_type;

} ubi_bao_config;

typedef struct {

    int version;
    ubi_bao_type type;
    ubi_bao_codec codec;
    int big_endian;
    int total_subsongs;

    int is_atomic;

    /* config */
    ubi_bao_config cfg;

    /* header info */
    off_t header_offset;
    uint8_t header_format;
    uint32_t header_version;
    size_t header_skip;
    uint32_t header_id;
    uint32_t header_type;

    uint32_t stream_id;
    size_t stream_size;
    off_t stream_offset;
    off_t prefetch_skip;
    size_t prefetch_size;
    off_t prefetch_offset;
    size_t main_size;
    off_t main_offset;
    off_t xma_offset;
    off_t dsp_offset;
    size_t extra_size;
    int is_prefetched;
    int is_external;

    int loop_flag;
    int num_samples;
    int loop_start;
    int sample_rate;
    int channels;
    int stream_type;

    int layer_count;
    int sequence_count;
    uint32_t sequence_chain[64];
    int sequence_loop;
    int sequence_single;

    float duration;

    char resource_name[255];

    char readable_name[255];
    int classes[16];
    int types[16];
    int allowed_types[16];
} ubi_bao_header;

static int parse_header(ubi_bao_header * bao, STREAMFILE *streamFile, off_t offset);
static int parse_bao(ubi_bao_header * bao, STREAMFILE *streamFile, off_t offset, int target_subsong);
static int parse_pk_header(ubi_bao_header * bao, STREAMFILE *streamFile);
static VGMSTREAM * init_vgmstream_ubi_bao_main(ubi_bao_header * bao, STREAMFILE *streamFile);
static STREAMFILE * setup_bao_streamfile(ubi_bao_header *bao, STREAMFILE *streamFile);
static int config_bao_version(ubi_bao_header * bao, STREAMFILE *streamFile);
static void config_bao_endian(ubi_bao_header * bao, off_t offset, STREAMFILE *streamFile);
static void build_readable_name(char * buf, size_t buf_size, ubi_bao_header * bao);
static STREAMFILE * open_atomic_bao(ubi_bao_file file_type, uint32_t file_id, int is_stream, STREAMFILE *streamFile);


/* .PK - packages with BAOs from Ubisoft's sound engine ("DARE") games in 2008+ */
VGMSTREAM * init_vgmstream_ubi_bao_pk(STREAMFILE *streamFile) {
    ubi_bao_header bao = { 0 };

    /* checks */
    if (!check_extensions(streamFile, "pk,lpk"))
        goto fail;

    /* package .pk+spk (or .lpk+lspk for localized) database-like format, evolved from Ubi sbN/smN.
     * .pk has an index pointing to memory BAOs and tables with external stream BAOs in .spk */

     /* main parse */
    if (!parse_pk_header(&bao, streamFile))
        goto fail;

    return init_vgmstream_ubi_bao_main(&bao, streamFile);
fail:
    return NULL;
}

/* .BAO - single BAO files from Ubisoft's sound engine ("DARE") games in 2008+ */
VGMSTREAM * init_vgmstream_ubi_bao_atomic(STREAMFILE *streamFile) {
    ubi_bao_header bao = { 0 };
    STREAMFILE * streamData = NULL;

    /* checks */
    if (!check_extensions(streamFile, "bao,"))
        goto fail;

    /* atomic .bao+bao/sbao found in .forge and similar bigfiles. The bigfile acts as index, but
     * since BAOs reference each other by id and are named by it (though the internal BAO id may
     * be other) we can simulate it. Extension is .bao/sbao or extensionaless in some games. */

    /* format: 0x01=base BAO (all atomic BAOs */
    if (read_8bit(0x00, streamFile) != 0x01)
        goto fail;

    bao.is_atomic = 1;

    bao.version = read_32bitBE(0x00, streamFile) & 0x00FFFFFF;
    if (!config_bao_version(&bao, streamFile))
        goto fail;

    /* main parse */
    if (!parse_bao(&bao, streamFile, 0x00, 1))
        goto fail;

    if (bao.total_subsongs == 0) {
        VGM_LOG("UBI BAO: no streams\n");
        goto fail; /* not uncommon */
    }

    build_readable_name(bao.readable_name, sizeof(bao.readable_name), &bao);

    return init_vgmstream_ubi_bao_main(&bao, streamFile);
fail:
    close_streamfile(streamData);
    return NULL;
}

#if 0
/* .SPK - special mini package with BAOs [Avatar (PS3)] */
VGMSTREAM * init_vgmstream_ubi_bao_spk(STREAMFILE *streamFile) {
    ubi_bao_header bao = { 0 };

    /* checks */
    if (!check_extensions(streamFile, "spk"))
        goto fail;

    /* Variation of .pk:
     * - 0x00: 0x014B5053 ("SPK\01" LE)
     * - 0x04: BAO count
     * - 0x08 * count: BAO ids inside
     * - per BAO count
     *   - 0x00: 1?
     *   - 0x04: id that references this? (ex. id of an event BAO)
     *   - 0x08: BAO size
     *   - 0x0c+: BAO data
     *
     * BAOs reference .sbao by name (are considered atomic) so perhaps could
     * be considered a type of bigfile.
     */

    return NULL;
}
#endif

/* ************************************************************************* */

static VGMSTREAM * init_vgmstream_ubi_bao_base(ubi_bao_header * bao, STREAMFILE *streamHead, STREAMFILE * streamData) {
    VGMSTREAM * vgmstream = NULL;
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
            dsp_read_coefs_be(vgmstream, streamHead, bao->dsp_offset + 0x10, 0x40);
            break;

#ifdef VGM_USE_FFMPEG
        case RAW_XMA1:
        case RAW_XMA2: {
            uint8_t buf[0x100];
            uint32_t num_frames;
            size_t bytes, chunk_size, frame_size, data_size;
            int is_xma2_old;
            STREAMFILE *header_data;
            off_t xma_offset;

            if (bao->version == 0x00230008) {
                is_xma2_old = 1;
                chunk_size = 0x2c;
            }
            else {
                is_xma2_old = 0;
                chunk_size = (bao->codec == RAW_XMA1) ? 0x20 : 0x34;
            }

            if (bao->is_external) {
                /* external XMA sounds have a custom header */
                /* first there's XMA2/FMT chunk, after that: */
                /* 0x00: some low number like 0x01 or 0x04 */
                /* 0x04: number of frames */
                /* 0x08: frame size (not always present?) */
                /* then there's a set of rising numbers followed by some weird data?.. */
                /* calculate true XMA size and use that get data start offset */
                //todo see Ubi SB
                num_frames = read_32bitBE(start_offset + chunk_size + 0x04, streamData);
                //frame_size = read_32bitBE(start_offset + chunk_size + 0x08, streamData);
                frame_size = 0x800;

                data_size = num_frames * frame_size;
                start_offset = bao->stream_size - data_size;
            }
            else {
                data_size = bao->stream_size;
                start_offset = 0x00;
            }

            /* XMA header is stored in 0x20 header for internal sounds and before audio data for external sounds */
            if (bao->is_external) {
                header_data = streamData;
                xma_offset = 0x00;
            }
            else {
                header_data = streamHead;
                xma_offset = bao->xma_offset;
            }

            if (is_xma2_old) {
                bytes = ffmpeg_make_riff_xma2_from_xma2_chunk(buf,sizeof(buf), xma_offset, chunk_size, data_size, header_data);
            }
            else {
                bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf,sizeof(buf), xma_offset, chunk_size, data_size, header_data, 1);
            }

            vgmstream->codec_data = init_ffmpeg_header_offset(streamData, buf, bytes, start_offset, data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->stream_size = data_size;

            xma_fix_raw_samples(vgmstream, streamData, start_offset,data_size, 0, 0,0);
            break;
        }

        case RAW_AT3_105:
        case RAW_AT3: {
            uint8_t buf[0x100];
            int32_t bytes, block_size, encoder_delay, joint_stereo;

            block_size = (bao->codec == RAW_AT3_105 ? 0x98 : 0xc0) * vgmstream->channels;
            joint_stereo = 0;
            encoder_delay = 0x00;//todo not correct

            bytes = ffmpeg_make_riff_atrac3(buf, 0x100, vgmstream->num_samples, vgmstream->stream_size, vgmstream->channels, vgmstream->sample_rate, block_size, joint_stereo, encoder_delay);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamData, buf, bytes, start_offset, bao->stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case FMT_AT3: {
            ffmpeg_codec_data *ffmpeg_data;

            ffmpeg_data = init_ffmpeg_offset(streamData, start_offset, bao->stream_size);
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            if (ffmpeg_data->skipSamples <= 0) /* in case FFmpeg didn't get them */
                ffmpeg_set_skip_samples(ffmpeg_data, riff_get_fact_skip_samples(streamData, start_offset));
            break;
        }

        case FMT_OGG: {
            ffmpeg_codec_data *ffmpeg_data;

            ffmpeg_data = init_ffmpeg_offset(streamData, start_offset, bao->stream_size);
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = bao->num_samples; /* ffmpeg_data->totalSamples */
            VGM_ASSERT(bao->num_samples != ffmpeg_data->totalSamples, "UBI BAO: header samples differ\n");
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
    return NULL;
}

static VGMSTREAM * init_vgmstream_ubi_bao_audio(ubi_bao_header * bao, STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamData = NULL;

    streamData = setup_bao_streamfile(bao, streamFile);
    if (!streamData) goto fail;

    vgmstream = init_vgmstream_ubi_bao_base(bao, streamFile, streamData);
    if (!vgmstream) goto fail;

    close_streamfile(streamData);
    return vgmstream;

fail:
    close_streamfile(streamData);
    close_vgmstream(vgmstream);
    return NULL;
}

static VGMSTREAM * init_vgmstream_ubi_bao_layer(ubi_bao_header *bao, STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    layered_layout_data* data = NULL;
    STREAMFILE* temp_streamFile = NULL;
    STREAMFILE * streamData = NULL;
    int i;

    streamData = setup_bao_streamfile(bao, streamFile);
    if (!streamData) goto fail;

    /* init layout */
    data = init_layout_layered(bao->layer_count);
    if (!data) goto fail;

    /* open all layers and mix */
    for (i = 0; i < bao->layer_count; i++) {
        /* prepare streamfile from a single layer section */
        temp_streamFile = setup_ubi_bao_streamfile(streamData, 0x00, bao->stream_size, i, bao->layer_count, bao->big_endian);
        if (!temp_streamFile) goto fail;

        /* build the layer VGMSTREAM (standard sb with custom streamfile) */
        data->layers[i] = init_vgmstream_ubi_bao_base(bao, streamFile, temp_streamFile);
        if (!data->layers[i]) goto fail;

        close_streamfile(temp_streamFile);
        temp_streamFile = NULL;
    }

    if (!setup_layout_layered(data))
        goto fail;

    /* build the base VGMSTREAM */
    vgmstream = allocate_vgmstream(bao->channels * bao->layer_count, bao->loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_SB;
    vgmstream->sample_rate = bao->sample_rate;
    vgmstream->num_streams = bao->total_subsongs;
    vgmstream->stream_size = bao->stream_size;

    vgmstream->num_samples = bao->num_samples;
    vgmstream->loop_start_sample = bao->loop_start;
    vgmstream->loop_end_sample = bao->num_samples;

    vgmstream->coding_type = data->layers[0]->coding_type;
    vgmstream->layout_type = layout_layered;
    vgmstream->layout_data = data;

    close_streamfile(streamData);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_streamfile(streamData);
    if (vgmstream)
        close_vgmstream(vgmstream);
    else
        free_layout_layered(data);
    return NULL;
}

static VGMSTREAM * init_vgmstream_ubi_bao_sequence(ubi_bao_header *bao, STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
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
            /* get the base memory BAO */
            streamChain = open_atomic_bao(bao->cfg.file_type, entry_id, 0, streamFile);
            if (!streamChain) {
                VGM_LOG("UBI BAO: chain BAO %08x not found\n", entry_id);
                goto fail;
            }

            /* parse BAO */
            if (!parse_header(&temp_bao, streamChain, 0x00))
                goto fail;

            /* should be ready, will open its BAOs in ubi_bao_main */
            close_streamfile(streamChain);
            streamChain = NULL;
        }
        else {
            //todo find base BAO offset in index and parse
            goto fail;
        }

        if (temp_bao.type == TYPE_NONE || temp_bao.type == UBI_SEQUENCE) {
            VGM_LOG("UBI BAO: unexpected sequence entry type\n");
            goto fail; /* technically ok but too much recursiveness? */
        }

        /* build the layer VGMSTREAM (current sb entry config) */
        data->segments[i] = init_vgmstream_ubi_bao_main(&temp_bao, streamFile);
        if (!data->segments[i]) goto fail;

        if (i == bao->sequence_loop)
            bao->loop_start = bao->num_samples;
        bao->num_samples += data->segments[i]->num_samples;

        /* save current (silences don't have values, so this unsures they know when memcpy'ed) */
        bao->channels = temp_bao.channels;
        bao->sample_rate = temp_bao.sample_rate;
    }

    if (!setup_layout_segmented(data))
        goto fail;

    /* build the base VGMSTREAM */
    vgmstream = allocate_vgmstream(data->segments[0]->channels, !bao->sequence_single);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_SB;
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
    return NULL;
}

//static VGMSTREAM * init_vgmstream_ubi_bao_silence(ubi_bao_header *bao, STREAMFILE *streamFile) {
//    return NULL;
//}


static VGMSTREAM * init_vgmstream_ubi_bao_main(ubi_bao_header * bao, STREAMFILE * streamFile) {
    VGMSTREAM * vgmstream = NULL;

    if (bao->total_subsongs == 0) {
        VGM_LOG("UBI BAO: no subsongs\n");
        goto fail;
    }

    ;VGM_LOG("UBI BAO: target at %x, id=%x, s_id=%x\n",
        (uint32_t)bao->header_offset, bao->header_id, bao->stream_id);
    ;VGM_LOG("UBI BAO: stream=%x, size=%x, res=%s\n",
            (uint32_t)bao->stream_offset, bao->stream_size, (bao->is_external ? bao->resource_name : "internal"));
    ;VGM_LOG("UBI BAO: prefetch=%x, size=%x, main=%x, size=%x\n",
            (uint32_t)bao->prefetch_offset, bao->prefetch_size, (uint32_t)bao->main_offset, bao->main_size);


    switch(bao->type) {

        case UBI_AUDIO:
            vgmstream = init_vgmstream_ubi_bao_audio(bao, streamFile);
            break;

        case UBI_LAYER:
            vgmstream = init_vgmstream_ubi_bao_layer(bao, streamFile);
            break;

        case UBI_SEQUENCE:
            vgmstream = init_vgmstream_ubi_bao_sequence(bao, streamFile);
            break;

        //case UBI_SILENCE:
        //    vgmstream = init_vgmstream_ubi_bao_silence(bao, streamFile);
        //    break;

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
 * BAOs pointing to internal/external stream BAOs (.spk is the same, with stream BAOs only). */
static int parse_pk_header(ubi_bao_header * bao, STREAMFILE *streamFile) {
    int i;
    int index_entries;
    size_t index_size, index_header_size;
    off_t bao_offset, resources_offset;
    int target_subsong = streamFile->stream_index;
    STREAMFILE *streamIndex = NULL;
    STREAMFILE *streamTest = NULL;

    /* format: 0x01=index, 0x02=BAO */
    if (read_8bit(0x00, streamFile) != 0x01)
        goto fail;
    /* index and resources are always LE */

    if (target_subsong == 0) target_subsong = 1;

    bao->version = read_32bitBE(0x00, streamFile) & 0x00FFFFFF;
    index_size = read_32bitLE(0x04, streamFile); /* can be 0, not including  */
    resources_offset = read_32bitLE(0x08, streamFile); /* always found even if not used */
    /* 0x0c: always 0? */
    /* 0x10: unknown, null if no entries */
    /* 0x14: config/flags/time? (changes a bit between files), null if no entries */
    /* 0x18(10): file GUID? clones may share it */
    /* 0x24: unknown */
    /* 0x2c: unknown, may be same as 0x14, can be null */
    /* 0x30(10): parent GUID? may be same as 0x18, may be shared with other files */
    /* (the above values seem ignored by games, probably just info for their tools) */

    if (!config_bao_version(bao, streamFile))
        goto fail;


    index_entries = index_size / 0x08;
    index_header_size = 0x40;

    /* pre-load to avoid too much I/O back and forth */
    if (index_size > (10000*0x08)) {
        VGM_LOG("BAO: index too big\n");
        goto fail;
    }

    /* use smaller I/O buffers for performance, as this read lots of small headers all over the place */
    streamIndex = reopen_streamfile(streamFile, index_size);
    if (!streamIndex) goto fail;

    streamTest = reopen_streamfile(streamFile, 0x100);
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

    ;VGM_LOG("UBI BAO: class "); {int i; for (i=0;i<16;i++){ VGM_ASSERT(bao->classes[i],"%02x=%i ",i,bao->classes[i]); }} VGM_LOG("\n");
    ;VGM_LOG("UBI BAO: types "); {int i; for (i=0;i<16;i++){ VGM_ASSERT(bao->types[i],"%02x=%i ",i,bao->types[i]); }} VGM_LOG("\n");

    if (bao->total_subsongs == 0) {
        VGM_LOG("UBI BAO: no streams\n");
        goto fail; /* not uncommon */
    }
    if (target_subsong < 0 || target_subsong > bao->total_subsongs || bao->total_subsongs < 1) goto fail;

    //todo prefetch in layers
    /* get stream pointed by header */
    if (bao->is_external) {
        off_t offset;
        int resources_count;
        size_t strings_size;

        /* Some sounds have a prefetched part stored internally with the remaining streamed part stored externally.
         * Both share stream ID in the .pk and outside, so first we find this prefetch */
        bao_offset = index_header_size + index_size;
        for (i = 0; i < index_entries; i++) {
            uint32_t bao_id = read_32bitLE(index_header_size + 0x08 * i + 0x00, streamFile);
            size_t bao_size = read_32bitLE(index_header_size + 0x08 * i + 0x04, streamFile);

            if (bao_id == bao->stream_id) {
                bao->prefetch_offset = bao_offset + bao->prefetch_skip;
                break;
            }

            bao_offset += bao_size;
        }

        if (bao->prefetch_size) {
            if (bao->prefetch_offset == 0) {
                VGM_LOG("UBI BAO: couldn't find expected prefetch\n");
                goto fail;
            }
            bao->is_prefetched = 1;
        }
        else {
            if (bao->prefetch_offset != 0) {
                VGM_LOG("UBI BAO: unexpected prefetch for stream id found\n");
                goto fail;
            }
        }

        /* parse resource table to external stream (may be empty, or exist even with nothing in the file) */
        resources_count = read_32bitLE(resources_offset+0x00, streamFile);
        strings_size = read_32bitLE(resources_offset+0x04, streamFile);

        offset = resources_offset + 0x04+0x04 + strings_size;
        for (i = 0; i < resources_count; i++) {
            uint32_t resource_id  = read_32bitLE(offset+0x10*i+0x00, streamFile);
            off_t name_offset     = read_32bitLE(offset+0x10*i+0x04, streamFile);
            off_t resource_offset = read_32bitLE(offset+0x10*i+0x08, streamFile);
            size_t resource_size  = read_32bitLE(offset+0x10*i+0x0c, streamFile);

            if (resource_id == bao->stream_id) {
                bao->stream_offset = resource_offset + bao->header_skip;
                read_string(bao->resource_name,255, resources_offset + 0x04+0x04 + name_offset, streamFile);

                if (bao->is_prefetched) {
                    bao->main_offset = resource_offset + bao->header_skip;
                    bao->main_size = resource_size - bao->header_skip;
                    VGM_ASSERT(bao->stream_size != bao->main_size + bao->prefetch_size, "UBI BAO: stream vs resource size mismatch\n");
                }
                else {
                    VGM_ASSERT(bao->stream_size != resource_size - bao->header_skip, "UBI BAO: stream vs resource size mismatch\n");
                }
                break;
            }
        }
    }
    else {
        /* find within index */
        bao_offset = index_header_size + index_size;
        for (i = 0; i < index_entries; i++) {
            uint32_t bao_id = read_32bitLE(index_header_size+0x08*i+0x00, streamFile);
            size_t bao_size = read_32bitLE(index_header_size+0x08*i+0x04, streamFile);

            if (bao_id == bao->stream_id) {
                /* in some cases, stream size value from 0x20 header can be bigger than */
                /* the actual audio chunk o_O [Rayman Raving Rabbids: TV Party (Wii)] */
                bao->stream_size = bao_size - bao->header_skip;
                bao->stream_offset = bao_offset + bao->header_skip; /* relative, adjust to skip descriptor */
                break;
            }

            bao_offset += bao_size;
        }
    }

    if (!bao->stream_offset) {
        VGM_LOG("UBI BAO: stream not found (id=%08x, external=%i)\n", bao->stream_id, bao->is_external);
        goto fail;
    }

    build_readable_name(bao->readable_name, sizeof(bao->readable_name), bao);

    close_streamfile(streamIndex);
    close_streamfile(streamTest);
    return 1;
fail:
    close_streamfile(streamIndex);
    close_streamfile(streamTest);
    return 0;
}

/* ************************************************************************* */

static void build_readable_name(char * buf, size_t buf_size, ubi_bao_header * bao) {
    const char *grp_name;
    const char *pft_name;
    const char *typ_name;
    const char *res_name;
    uint32_t h_id, s_id, type;

    /* config */
    if (bao->is_atomic)
        grp_name = "atomic";
    else
        grp_name = "package";
    pft_name = bao->prefetch_size ? "p" : "n";
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
        if (!bao->is_atomic && bao->is_external)
            res_name = bao->resource_name;
        else
            res_name = NULL;
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

static int parse_type_audio(ubi_bao_header * bao, off_t offset, STREAMFILE* streamFile) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = bao->big_endian ? read_32bitBE : read_32bitLE;
    off_t h_offset = offset + bao->header_skip;

    /* audio header */
    bao->type = UBI_AUDIO;

    bao->stream_size = read_32bit(h_offset + bao->cfg.audio_stream_size, streamFile);
    bao->stream_id   = read_32bit(h_offset + bao->cfg.audio_stream_id, streamFile);
    bao->is_external = read_32bit(h_offset + bao->cfg.audio_external_flag, streamFile) & bao->cfg.audio_external_and;
    bao->loop_flag   = read_32bit(h_offset + bao->cfg.audio_loop_flag, streamFile) & bao->cfg.audio_loop_and;
    bao->channels    = read_32bit(h_offset + bao->cfg.audio_channels, streamFile);
    bao->sample_rate = read_32bit(h_offset + bao->cfg.audio_sample_rate, streamFile);

    /* prefetch data is in another internal BAO right after the base header */
    if (bao->cfg.audio_prefetch_size) {
        bao->prefetch_size = read_32bit(h_offset + bao->cfg.audio_prefetch_size, streamFile);
        bao->prefetch_skip = bao->header_skip;
    }

    if (bao->loop_flag) {
        bao->loop_start  = read_32bit(h_offset + bao->cfg.audio_num_samples, streamFile);
        bao->num_samples = read_32bit(h_offset + bao->cfg.audio_num_samples2, streamFile) + bao->loop_start;
    }
    else {
        bao->num_samples = read_32bit(h_offset + bao->cfg.audio_num_samples, streamFile);
    }

    bao->stream_type = read_32bit(h_offset + bao->cfg.audio_stream_type, streamFile);
    if (bao->stream_type > 0x10) {
        VGM_LOG("UBI BAO: unknown stream_type at %x\n", (uint32_t)offset); goto fail;
        goto fail;
    }

    bao->codec = bao->cfg.codec_map[bao->stream_type];
    if (bao->codec == 0x00) {
        VGM_LOG("UBI BAO: unknown codec at %x\n", (uint32_t)offset); goto fail;
        goto fail;
    }

    if (bao->loop_flag && bao->cfg.audio_channel_samples) {
        bao->num_samples = bao->num_samples / bao->channels;
    }

    bao->dsp_offset = bao->cfg.audio_dsp_offset;
    bao->xma_offset = bao->cfg.audio_xma_offset;

    return 1;
fail:
    return 0;
}

static int parse_type_sequence(ubi_bao_header * bao, off_t offset, STREAMFILE* streamFile) {
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

    bao->sequence_loop   = read_32bit(h_offset + bao->cfg.sequence_sequence_loop, streamFile);
    bao->sequence_single = read_32bit(h_offset + bao->cfg.sequence_sequence_single, streamFile);
    bao->sequence_count  = read_32bit(h_offset + bao->cfg.sequence_sequence_count, streamFile);

    if (bao->sequence_count > sizeof(bao->sequence_chain)) { /* arbitrary max */
        VGM_LOG("UBI BAO: incorrect sequence count\n");
        goto fail;
    }

    /* get chain in extra table */
    table_offset = offset + bao->cfg.header_base_size;
    if (read_32bit(table_offset + 0x00, streamFile) == -1 || read_32bit(table_offset + 0x00, streamFile) == 0)
        table_offset += 0x04;
    for (i = 0; i < bao->sequence_count; i++) {
        uint32_t entry_id = (uint32_t)read_32bit(table_offset + bao->cfg.sequence_entry_number, streamFile);

        bao->sequence_chain[i] = entry_id;

        table_offset += bao->cfg.sequence_entry_size;
    }

    return 1;
fail:
    return 0;
}


static int parse_type_layer(ubi_bao_header * bao, off_t offset, STREAMFILE* streamFile) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = bao->big_endian ? read_32bitBE : read_32bitLE;
    off_t h_offset = offset + bao->header_skip;
    off_t table_offset;
    int i;

    /* audio header */
    bao->type = UBI_LAYER;
    if (bao->cfg.layer_entry_size == 0) {
        VGM_LOG("UBI BAO: layer entry size not configured at %x\n", (uint32_t)offset);
        goto fail;
    }

    bao->layer_count    = read_32bit(h_offset + bao->cfg.layer_layer_count, streamFile);
    if (bao->layer_count > 16) { /* arbitrary max */
        VGM_LOG("UBI BAO: incorrect layer count\n");
        goto fail;
    }

    bao->is_external = read_32bit(h_offset + bao->cfg.layer_external_flag, streamFile) & bao->cfg.layer_external_and;

    bao->stream_size    = read_32bit(h_offset + bao->cfg.layer_stream_size, streamFile);
    if (bao->cfg.layer_stream_id) {
        bao->stream_id  = read_32bit(h_offset + bao->cfg.layer_stream_id, streamFile);
    }

    if (bao->cfg.layer_extra_size) {
        bao->extra_size = read_32bit(h_offset + bao->cfg.layer_extra_size, streamFile);
    }
    else {
        bao->extra_size = bao->layer_count * bao->cfg.layer_entry_size;
    }

    /* prefetch data can be in this BAO or in another memory BAO */ //todo find correct flag?
    if (bao->cfg.layer_prefetch_size) {
        bao->prefetch_size  = read_32bit(h_offset + bao->cfg.layer_prefetch_size, streamFile);
        bao->prefetch_skip  = bao->cfg.header_base_size + bao->extra_size;
    }

    /* get 1st layer header in extra table and validate all headers match */
    table_offset = offset + bao->cfg.header_base_size;
    if (read_32bit(table_offset + 0x00, streamFile) == -1 || read_32bit(table_offset + 0x00, streamFile) == 0) //todo improve
        table_offset += 0x04;
    bao->channels       = read_32bit(table_offset + bao->cfg.layer_channels, streamFile);
    bao->sample_rate    = read_32bit(table_offset + bao->cfg.layer_sample_rate, streamFile);
    bao->stream_type    = read_32bit(table_offset + bao->cfg.layer_stream_type, streamFile);
    bao->num_samples    = read_32bit(table_offset + bao->cfg.layer_num_samples, streamFile);
    if (bao->cfg.layer_entry_id) {
        bao->stream_id      = read_32bit(table_offset + bao->cfg.layer_entry_id, streamFile);
    }

    for (i = 0; i < bao->layer_count; i++) {
        int channels    = read_32bit(table_offset + bao->cfg.layer_channels, streamFile);
        int sample_rate = read_32bit(table_offset + bao->cfg.layer_sample_rate, streamFile);
        int stream_type = read_32bit(table_offset + bao->cfg.layer_stream_type, streamFile);
        int num_samples = read_32bit(table_offset + bao->cfg.layer_num_samples, streamFile);
        if (bao->channels != channels || bao->sample_rate != sample_rate || bao->stream_type != stream_type) {
            VGM_LOG("UBI BAO: layer headers don't match at %x\n", (uint32_t)table_offset);
            goto fail;
        }

        if (bao->cfg.layer_entry_id) {
            int stream_id   = read_32bit(table_offset + bao->cfg.layer_entry_id, streamFile);
            if (bao->stream_id != stream_id) {
                VGM_LOG("UBI BAO: layer stream ids don't match at %x\n", (uint32_t)table_offset);
                goto fail;
            }
        }

        /* can be +-1 */
        if (bao->num_samples != num_samples && bao->num_samples + 1 == num_samples) {
            bao->num_samples -= 1;
        }

        table_offset += bao->cfg.layer_entry_size;
    }


    bao->codec = bao->cfg.codec_map[bao->stream_type];
    if (bao->codec == 0x00) {
        VGM_LOG("UBI BAO: unknown codec at %x\n", (uint32_t)offset); goto fail;
        goto fail;
    }


    return 1;
fail:
    return 0;
}

static int parse_header(ubi_bao_header * bao, STREAMFILE *streamFile, off_t offset) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = bao->big_endian ? read_32bitBE : read_32bitLE;

    ;VGM_LOG("UBI BAO: header at %x\n", (uint32_t)offset);

    /* parse known headers (see config_bao for info) */
    bao->header_offset  = offset;

    bao->header_format  = read_8bit (offset + 0x00, streamFile); /* usually 0x02 but older BAOs have 0x01 too */
    bao->header_version = read_32bitBE(offset + 0x00, streamFile) & 0x00FFFFFF;
    /* early versions:
     * 0x04: header skip (usually 0x28, rarely 0x24), can be LE unlike other fields (ex. Assassin's Creed PS3)
     * 0x08(10): GUID, or id-like fields in early versions
     * 0x18: null
     * 0x1c: null
     * 0x20: class
     * 0x24: extra config? (0x00/0x02)
     *
     * later versions:
     * 0x04(10): GUID
     * 0x14: class
     * 0x18: extra config? (0x02) */

    bao->header_skip    = bao->cfg.header_skip;
    bao->header_id      = read_32bit(offset + bao->header_skip + 0x00, streamFile);
    bao->header_type    = read_32bit(offset + bao->header_skip + 0x04, streamFile);

    if (bao->version != bao->header_version) {
        VGM_LOG("UBI BAO: mismatched header version at %x: %08x vs %08x\n", (uint32_t)offset, bao->version, bao->header_version);
        goto fail;
    }

    switch(bao->header_type) {
        case 0x01:
            if (!parse_type_audio(bao, offset, streamFile))
                goto fail;
            break;
        case 0x05:
            if (!parse_type_sequence(bao, offset, streamFile))
                goto fail;
            break;
        case 0x06:
            if (!parse_type_layer(bao, offset, streamFile))
                goto fail;
            break;
        default:
            VGM_LOG("UBI BAO: unknown header type at %x\n", (uint32_t)offset);
            goto fail;
    }

    return 1;
fail:
    return 0;
}

static int parse_bao(ubi_bao_header * bao, STREAMFILE *streamFile, off_t offset, int target_subsong) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    uint32_t bao_class, header_type;

  /*bao_version =*/ read_32bitBE(offset+0x00, streamFile); /* force buffer read */

    config_bao_endian(bao, offset, streamFile);
    read_32bit = bao->big_endian ? read_32bitBE : read_32bitLE;

    bao_class = read_32bit(offset+bao->cfg.bao_class, streamFile);
    if (bao_class & 0x0FFFFFFF) {
        VGM_LOG("UBI BAO: unknown class %x at %x\n", bao_class, (uint32_t)offset);
        goto fail;
    }

    bao->classes[(bao_class >> 28) & 0xF]++;
    if (bao_class != 0x20000000) /* ignore non-header classes */
        return 1;

    header_type = read_32bit(offset + bao->cfg.header_skip + 0x04, streamFile);
    if (header_type > 8) {
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

    if (!parse_header(bao, streamFile, offset))
        goto fail;

    return 1;
fail:
    return 0;
}

/* opens a file BAO's companion BAO (memory or stream) */
static STREAMFILE * open_atomic_bao(ubi_bao_file file_type, uint32_t file_id, int is_stream, STREAMFILE *streamFile) {
    STREAMFILE *streamBAO = NULL;
    char buf[255];
    size_t buf_size = sizeof(buf);

    /* Get referenced BAOs, in different naming styles for "internal" (=memory) or "external" (=stream). */
    switch(file_type) {

        case UBI_FORGE:
            /* Try default extensionless (as extracted from .forge bigfile) and with common extension.
             * .forge data can be uncompressed (stream BAOs) and compressed (subfiles per area with memory BAOs).
             * They are named after their class too (0x1NNNNNNN: events, 0x2NNNNNNN: headers, etc) */
            if (is_stream) {
                snprintf(buf,buf_size, "Common_BAO_0x%08x", file_id);
                streamBAO = open_streamfile_by_filename(streamFile, buf);
                if (streamBAO) return streamBAO;

                strcat(buf,".sbao");
                streamBAO = open_streamfile_by_filename(streamFile, buf);
                if (streamBAO) return streamBAO;

                /* there are many per language but whatevs, could be renamed */
                snprintf(buf,buf_size, "English_BAO_0x%08x", file_id);
                streamBAO = open_streamfile_by_filename(streamFile, buf);
                if (streamBAO) return streamBAO;
            }
            else {
                snprintf(buf,buf_size, "BAO_0x%08x", file_id);
                streamBAO = open_streamfile_by_filename(streamFile, buf);
                if (streamBAO) return streamBAO;

                strcat(buf,".bao");
                streamBAO = open_streamfile_by_filename(streamFile, buf);
                if (streamBAO) return streamBAO;
            }

            goto fail;

        default:
            goto fail;
    }

    //todo try default naming scheme?

    return streamBAO; /* may be NULL */
fail:
    VGM_LOG("UBI BAO: can't find BAO id %08x\n", file_id);
    close_streamfile(streamBAO);
    return NULL;
}

/* create a usable streamfile */
static STREAMFILE * setup_bao_streamfile(ubi_bao_header *bao, STREAMFILE *streamFile) {
    STREAMFILE *new_streamFile = NULL;
    STREAMFILE *temp_streamFile = NULL;
    STREAMFILE *stream_segments[2] = { 0 };

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
        //todo prefetch_id naming convention isn't probably always true
        //todo AC1 can't contain prefetch in the base/layer BAO but what about others?
        //todo do all this right after parse?

        /* file BAOs re-open new STREAMFILEs so no need to wrap them */
        bao->is_prefetched = bao->prefetch_size; //

        if (bao->is_prefetched) {
            /* stream BAO is 0x5NNNNNNN and prefetch memory BAO 0x3NNNNNNN */
            uint32_t prefetch_id    = (bao->stream_id & 0x0FFFFFFF) | 0x30000000;
            uint32_t main_id        = (bao->stream_id);
            size_t prefetch_offset  = bao->header_skip;
            size_t prefetch_size    = bao->prefetch_size;
            size_t main_offset      = bao->header_skip;
            size_t main_size        = bao->stream_size - bao->prefetch_size;

            new_streamFile = open_atomic_bao(bao->cfg.file_type, prefetch_id, 0, streamFile);
            if (!new_streamFile) goto fail;
            stream_segments[0] = new_streamFile;

            new_streamFile = open_clamp_streamfile(stream_segments[0], prefetch_offset, prefetch_size);
            if (!new_streamFile) goto fail;
            stream_segments[0] = new_streamFile;

            new_streamFile = open_atomic_bao(bao->cfg.file_type, main_id, 1, streamFile);
            if (!new_streamFile) goto fail;
            stream_segments[1] = new_streamFile;

            new_streamFile = open_clamp_streamfile(stream_segments[1], main_offset, main_size);
            if (!new_streamFile) goto fail;
            stream_segments[1] = new_streamFile;

            new_streamFile = open_multifile_streamfile(stream_segments, 2);
            if (!new_streamFile) goto fail;
            temp_streamFile = new_streamFile;
            stream_segments[0] = NULL;
            stream_segments[1] = NULL;
        }
        else {
            new_streamFile = open_atomic_bao(bao->cfg.file_type, bao->stream_id, bao->is_external, streamFile);
            if (!new_streamFile) goto fail;
            temp_streamFile = new_streamFile;

            new_streamFile = open_clamp_streamfile(temp_streamFile, bao->header_skip, bao->stream_size);
            if (!new_streamFile) goto fail;
            temp_streamFile = new_streamFile;
        }
    }
    else {
        if (bao->is_prefetched) {
            new_streamFile = open_wrap_streamfile(streamFile);
            if (!new_streamFile) goto fail;
            stream_segments[0] = new_streamFile;

            new_streamFile = open_clamp_streamfile(stream_segments[0], bao->prefetch_offset, bao->prefetch_size);
            if (!new_streamFile) goto fail;
            stream_segments[0] = new_streamFile;

            new_streamFile = open_streamfile_by_filename(streamFile, bao->resource_name);
            if (!new_streamFile) { VGM_LOG("UBI BAO: external stream '%s' not found\n", bao->resource_name); goto fail; }
            stream_segments[1] = new_streamFile;

            new_streamFile = open_clamp_streamfile(stream_segments[1], bao->main_offset, bao->main_size);
            if (!new_streamFile) goto fail;
            stream_segments[1] = new_streamFile;
            temp_streamFile = NULL;

            new_streamFile = open_multifile_streamfile(stream_segments, 2);
            if (!new_streamFile) goto fail;
            temp_streamFile = new_streamFile;
            stream_segments[0] = NULL;
            stream_segments[1] = NULL;
        }
        else if (bao->is_external) {
            /* open external file */
            new_streamFile = open_streamfile_by_filename(streamFile, bao->resource_name);
            if (!new_streamFile) { VGM_LOG("UBI BAO: external stream '%s' not found\n", bao->resource_name); goto fail; }
            temp_streamFile = new_streamFile;

            new_streamFile = open_clamp_streamfile(temp_streamFile, bao->stream_offset, bao->stream_size);
            if (!new_streamFile) goto fail;
            temp_streamFile = new_streamFile;
        }
        else {
            new_streamFile = open_wrap_streamfile(streamFile);
            if (!new_streamFile) goto fail;
            temp_streamFile = new_streamFile;

            new_streamFile = open_clamp_streamfile(temp_streamFile, bao->stream_offset, bao->stream_size);
            if (!new_streamFile) goto fail;
            temp_streamFile = new_streamFile;
        }

    }

    return temp_streamFile;

fail:
    close_streamfile(stream_segments[0]);
    close_streamfile(stream_segments[1]);
    close_streamfile(temp_streamFile);

    return NULL;
}


static void config_bao_endian(ubi_bao_header * bao, off_t offset, STREAMFILE *streamFile) {
    //todo this could be done once as all BAOs share endianness (would save a few checks)

    /* detect endianness using the 'class' field (the 'header skip' field is LE in early
     * versions, and was removed in later versions) */

    /* negate as fields looks like LE (0xN0000000) */
    bao->big_endian = !guess_endianness32bit(offset+bao->cfg.bao_class, streamFile);
}


static void config_bao_sequence(ubi_bao_header * bao, off_t sequence_count, off_t sequence_single, off_t sequence_loop, off_t entry_size) {
    /* sequence header and chain table */
    bao->cfg.sequence_sequence_count    = sequence_count;
    bao->cfg.sequence_sequence_single   = sequence_count;
    bao->cfg.sequence_sequence_loop     = sequence_count;
    bao->cfg.sequence_entry_size        = entry_size;
    bao->cfg.sequence_entry_number      = 0x00;
}
static void config_bao_layer_h1(ubi_bao_header * bao, off_t layer_count, off_t external_flag, off_t stream_size, off_t stream_id, off_t prefetch_size) {
    /* layer header in the main BAO */
    bao->cfg.layer_layer_count          = layer_count;
    bao->cfg.layer_external_flag        = external_flag;
    bao->cfg.layer_stream_size          = stream_size;
    bao->cfg.layer_stream_id            = stream_id;
    bao->cfg.layer_prefetch_size        = prefetch_size;
    bao->cfg.layer_external_and         = 1;
}
static void config_bao_layer_h2(ubi_bao_header * bao, off_t layer_count, off_t stream_size, off_t extra_size, off_t prefetch_size) {
    /* layer header in the main BAO with extra size */
    bao->cfg.layer_layer_count          = layer_count;
    bao->cfg.layer_stream_size          = stream_size;
    bao->cfg.layer_extra_size           = extra_size;
    bao->cfg.layer_prefetch_size        = prefetch_size;
}
static void config_bao_layer_s1(ubi_bao_header * bao, off_t entry_size, off_t sample_rate, off_t channels, off_t stream_type, off_t num_samples) {
    /* layer sub-headers in extra table */
    bao->cfg.layer_entry_size           = entry_size;
    bao->cfg.layer_sample_rate          = sample_rate;
    bao->cfg.layer_channels             = channels;
    bao->cfg.layer_stream_type          = stream_type;
    bao->cfg.layer_num_samples          = num_samples;
}
static void config_bao_layer_s2(ubi_bao_header * bao, off_t entry_size, off_t sample_rate, off_t channels, off_t stream_type, off_t num_samples, off_t entry_id) {
    /* layer sub-headers in extra table + stream id */
    bao->cfg.layer_entry_size           = entry_size;
    bao->cfg.layer_sample_rate          = sample_rate;
    bao->cfg.layer_channels             = channels;
    bao->cfg.layer_stream_type          = stream_type;
    bao->cfg.layer_num_samples          = num_samples;
    bao->cfg.layer_entry_id             = entry_id;
}

static int config_bao_version(ubi_bao_header * bao, STREAMFILE *streamFile) {

    /* Ubi BAO evolved from Ubi SB and are conceptually quite similar, see that first.
     *
     * BAOs (binary audio objects) always start with:
     * - 0x00(1): format (meaning defined by mode)
     * - 0x01(3): 8b*3 version, major/minor/release (numbering continues from .sb0/sm0)
     * - 0x04+: mini header (varies with version, see parse_header)
     *
     * Then are divided into "classes":
     * - 0x10000000: event (links by id to another event or header BAO)
     * - 0x20000000: header
     * - 0x30000000: memory audio (in .pk/.bao)
     * - 0x40000000: project info
     * - 0x50000000: stream audio (in .spk/.sbao)
     * - 0x60000000: unused?
     * - 0x70000000: info? has a count+table of id-things
     * - 0x80000000: unknown (some id/info?)
     * Class 1/2/3 are roughly equivalent to Ubi SB's section1/2/3, and class 4 is
     * basically .spN project files.
     *
     * The project BAO (usually with special id 0x7FFFFFFF or 0x40000000) has version,
     * filenames (not complete) and current mode, "PACKAGE" (pk, index + BAOs with
     * external BAOs) or "ATOMIC" (file, separate BAOs).
     *
     * We want header classes, also similar to SB types:
     * - 01: single audio (samples, channels, bitrate, samples+size, etc)
     * - 02: unknown chain (has probability?)
     * - 03: unknown chain
     * - 04: random (count, etc) + BAO IDs and float probability to play
     * - 05: sequence (count, etc) + BAO IDs and unknown data
     * - 06: layer (count, etc) + layer headers
     * - 07: unknown chain
     * - 08: silence (duration, etc)
     *
     * Right after base BAO size is the extra table for that BAO (what sectionX had). This
     * can exist even for type 01 (some kind of cue-like table) though size calcs are hazy.
     *
     * Just to throw us off, the base BAO size may add +0x04 (with a field value of 0/-1) on
     * some game versions/platforms (PC/Wii?). Doesn't look like there is a header field
     * (comparing many BAOs from different platforms of the same games) so it's autodetected
     * as needed, for layers and sequences basically.
     *
     * Most types + tables are pretty much the same as SB (with config styles ported straight) but
     * now can "prefetch" part of the data (signaled by a size in the header, or perhaps a flag but
     * looks too erratic). The header points to a external/stream ID, and with prefetch enabled part
     * of the audio is in an internal/memory ID, and must join both during reads to get the full
     * stream. Prefetch may be used in some platforms of a game only (ex. AC1 PC does while PS3
     * doesn't, while Scott Pilgrim always does)
     */

    bao->allowed_types[0x01] = 1;
  //bao->allowed_types[0x05] = 1;
  //bao->allowed_types[0x06] = 1;

    /* absolute */
    bao->cfg.bao_class      = 0x20;

    /* relative to header_skip */
    bao->cfg.header_id      = 0x00;
    bao->cfg.header_type    = 0x04;

    bao->cfg.audio_external_and     = 1;
    bao->cfg.audio_loop_and         = 1;

    /* config per version*/
    switch(bao->version) {
        case 0x001B0100: /* Assassin's Creed (PS3/X360/PC)-file */
            bao->cfg.header_base_size       = 0xA4; /* 0xA8: PC */
            bao->cfg.header_skip            = 0x28;

            //todo call config_bao_audio
            bao->cfg.audio_stream_size      = 0x08;
            bao->cfg.audio_stream_id        = 0x1c;
            bao->cfg.audio_external_flag    = 0x28; /* 0x2c: prefetch flag */
            bao->cfg.audio_loop_flag        = 0x34;
            bao->cfg.audio_channels         = 0x44;
            bao->cfg.audio_sample_rate      = 0x4c;
            bao->cfg.audio_num_samples      = 0x50;
            bao->cfg.audio_num_samples2     = 0x58;
            bao->cfg.audio_stream_type      = 0x64;
            bao->cfg.audio_prefetch_size    = 0x74;

            bao->cfg.audio_interleave = 0x10;
            bao->cfg.audio_channel_samples = 1; //todo check all looping ps-adpcm

            config_bao_sequence(bao, 0x2c, 0x4c, 0x50, 0x14); //todo loop/single wrong?

            config_bao_layer_h1(bao, 0x20, 0x2c, 0x44 /*0x48?*/, 0x4c, 0x50);
            config_bao_layer_s1(bao, 0x30, 0x00, 0x04, 0x08, 0x10);
            bao->cfg.audio_external_flag    = 0x28;
            //0x28+0x2c may be set when full external? check layers again

            //silence: 0x1C

            bao->cfg.codec_map[0x02] = RAW_PSX;
            bao->cfg.codec_map[0x03] = UBI_IMA;
            bao->cfg.codec_map[0x04] = FMT_OGG;
            bao->cfg.codec_map[0x07] = RAW_AT3_105;

            //todo move
            bao->allowed_types[0x05] = 1;
            bao->allowed_types[0x06] = 1;

            bao->cfg.file_type = UBI_FORGE;
            return 1;


        case 0x001F0008: /* Rayman Raving Rabbids: TV Party (Wii)-pk */
        case 0x001F0011: /* Naruto: The Broken Bond (X360)-pk */
        case 0x0022000D: /* Just Dance (Wii)-pk */
            bao->cfg.header_base_size       = 0xA4; /* 0xA8: Wii */
            bao->cfg.header_skip            = 0x28;

            bao->cfg.audio_stream_size      = 0x08;
            bao->cfg.audio_stream_id        = 0x1c;
            bao->cfg.audio_external_flag    = 0x28;
            bao->cfg.audio_loop_flag        = 0x34;
            bao->cfg.audio_channels         = 0x44;
            bao->cfg.audio_sample_rate      = 0x4c;
            bao->cfg.audio_num_samples      = 0x54;
            bao->cfg.audio_num_samples2     = 0x5c;

            bao->cfg.audio_stream_type      = 0x64;
            bao->cfg.audio_prefetch_size    = 0x74;
            bao->cfg.audio_xma_offset       = 0x7c; /* only if internal */
            bao->cfg.audio_dsp_offset       = 0x80;

            bao->cfg.codec_map[0x01] = RAW_PCM;
          //bao->cfg.codec_map[0x02] = FMT_OGG;
            bao->cfg.codec_map[0x03] = UBI_IMA;
            bao->cfg.codec_map[0x05] = RAW_XMA1;
            bao->cfg.codec_map[0x09] = RAW_DSP;

            return 1;

        case 0x00220015: /* James Cameron's Avatar: The Game (PSP)-pk */
        case 0x0022001E: /* Prince of Persia: The Forgotten Sands (PSP)-pk */
            bao->cfg.header_base_size       = 0x84;
            //skip 0x28

            bao->cfg.audio_stream_size      = 0x08;
            bao->cfg.audio_stream_id        = 0x1c;
            bao->cfg.audio_external_flag    = 0x20;
            bao->cfg.audio_loop_flag        = 0x20; /* 0x10? */
            bao->cfg.audio_channels         = 0x28;
            bao->cfg.audio_sample_rate      = 0x30;
            bao->cfg.audio_num_samples      = 0x38;
            bao->cfg.audio_num_samples2     = 0x40;
            bao->cfg.audio_stream_type      = 0x48;

            bao->cfg.audio_external_and     = (1 << 2);
            bao->cfg.audio_loop_and         = (1 << 5);

            bao->cfg.codec_map[0x06] = RAW_PSX;
            bao->cfg.codec_map[0x07] = FMT_AT3;

            return 1;

        case 0x00230008: /* Splinter Cell: Conviction (X360/PC)-pk */
            bao->cfg.header_base_size       = 0xB4;
            bao->cfg.header_skip            = 0x28;

            bao->cfg.audio_stream_size      = 0x08;
            bao->cfg.audio_stream_id        = 0x24;
            bao->cfg.audio_external_flag    = 0x38;
            bao->cfg.audio_loop_flag        = 0x44;
            bao->cfg.audio_channels         = 0x54;
            bao->cfg.audio_sample_rate      = 0x5c;
            bao->cfg.audio_num_samples      = 0x64;
            bao->cfg.audio_num_samples2     = 0x6c;
            bao->cfg.audio_stream_type      = 0x74;
            bao->cfg.audio_prefetch_size    = 0x84;
            bao->cfg.audio_xma_offset       = 0x8c;

            config_bao_sequence(bao, 0x34, 0x54, 0x58, 0x14);

            config_bao_layer_h2(bao, 0x28, 0x50, 0x5c, 0x54);
            config_bao_layer_s2(bao, 0x30, 0x00, 0x04, 0x08, 0x14, 0x2c);

            bao->cfg.codec_map[0x01] = RAW_PCM;
            bao->cfg.codec_map[0x02] = UBI_IMA;
            bao->cfg.codec_map[0x03] = FMT_OGG;
            bao->cfg.codec_map[0x04] = RAW_XMA2;

            return 1;

        case 0x00250108: /* Scott Pilgrim vs the World (PS3/X360)-pk */
        case 0x0025010A: /* Prince of Persia: The Forgotten Sands (PS3/X360)-file */
            bao->cfg.header_base_size       = 0xB4;
            bao->cfg.header_skip            = 0x28;

            bao->cfg.audio_stream_size      = 0x08;
            bao->cfg.audio_stream_id        = 0x24;
            bao->cfg.audio_external_flag    = 0x30;
            bao->cfg.audio_loop_flag        = 0x38;
            bao->cfg.audio_channels         = 0x48;
            bao->cfg.audio_sample_rate      = 0x50;
            bao->cfg.audio_num_samples      = 0x58;
            bao->cfg.audio_num_samples2     = 0x60;
            bao->cfg.audio_stream_type      = 0x68;
            bao->cfg.audio_prefetch_size    = 0x78;
            bao->cfg.audio_xma_offset       = 0x8c;

            config_bao_layer_h2(bao, 0x28, 0x48, 0x50, 0x54);
            config_bao_layer_s2(bao, 0x30, 0x00, 0x04, 0x08, 0x14, 0x2c);

            bao->cfg.codec_map[0x01] = RAW_PCM;
            bao->cfg.codec_map[0x02] = UBI_IMA; /* assumed */
            bao->cfg.codec_map[0x03] = FMT_OGG; /* assumed */
            bao->cfg.codec_map[0x04] = RAW_XMA2;
            bao->cfg.codec_map[0x05] = RAW_PSX;
            bao->cfg.codec_map[0x06] = RAW_AT3;

            return 1;

        case 0x001B0200: /* Beowulf (PS3)-file */
        case 0x001C0000: /* Lost: Via Domus (PS3)-file */
        case 0x001D0A00: /* Shaun White Snowboarding (PSP)-file? */
        case 0x001F0010: /* Prince of Persia 2008 (PS3/X360)-file, Far Cry 2 (PS3)-file */
        case 0x00220018: /* Avatar (PS3)-file/spk */
        case 0x00260102: /* Prince of Persia Trilogy HD (PS3)-pk */
        case 0x00280306: /* Far Cry 3: Blood Dragon (X360)-file */
        case 0x00290106: /* Splinter Cell: Blacklist (PS3)-file */
        default: /* others possibly using BAO: Just Dance, Watch_Dogs, Far Cry Primal, Far Cry 4 */
            VGM_LOG("UBI BAO: unknown BAO version %08x\n", bao->version);
            return 0;
    }

    VGM_LOG("UBI BAO: unknown BAO version %08x\n", bao->version);
    return 0;
}
