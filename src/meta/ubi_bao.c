#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "ubi_bao_streamfile.h"


typedef enum { NONE = 0, UBI_IMA, RAW_PCM, RAW_PSX, RAW_XMA1, RAW_XMA2, RAW_AT3, FMT_AT3, RAW_DSP, FMT_OGG } ubi_bao_codec;
typedef enum { UBI_NONE = 0, UBI_AUDIO, UBI_LAYER, UBI_SEQUENCE  } ubi_bao_type;

typedef struct {
    size_t header_entry_size;
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

    int audio_external_and;
    int audio_loop_and;

    off_t sequence_sequence_loop;
    off_t sequence_sequence_single;
    off_t sequence_sequence_count;
    off_t sequence_entry_number;
    size_t sequence_entry_size;

    off_t layer_layer_count;
    off_t layer_stream_id;
    off_t layer_stream_size;
    off_t layer_prefetch_size;
    size_t layer_extra_size;
    off_t layer_sample_rate;
    off_t layer_channels;
    off_t layer_stream_type;
    off_t layer_num_samples;
    size_t layer_entry_size;

    off_t silence_duration_float;

    ubi_bao_codec codec_map[16];

} ubi_bao_config;

typedef struct {

    int version;
    ubi_bao_type type;
    ubi_bao_codec codec;
    int big_endian;
    int total_subsongs;

    int is_file;

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

static int parse_bao(ubi_bao_header * bao, STREAMFILE *streamFile, off_t offset, int target_subsong);
static int parse_pk_header(ubi_bao_header * bao, STREAMFILE *streamFile);
static VGMSTREAM * init_vgmstream_ubi_bao_main(ubi_bao_header * bao, STREAMFILE *streamFile);
static STREAMFILE * setup_bao_streamfile(ubi_bao_header *bao, STREAMFILE *streamFile);
static int config_bao_version(ubi_bao_header * bao, STREAMFILE *streamFile);
static void config_bao_endian(ubi_bao_header * bao, off_t offset, STREAMFILE *streamFile);
static void build_readable_name(char * buf, size_t buf_size, ubi_bao_header * bao);


/* .PK - packages with BAOs from Ubisoft's sound engine ("DARE") games in 2008+ */
VGMSTREAM * init_vgmstream_ubi_bao_pk(STREAMFILE *streamFile) {
    ubi_bao_header bao = { 0 };

    /* checks */
    if (!check_extensions(streamFile, "pk,lpk"))
        goto fail;

    /* .pk+spk (or .lpk+lspk) is a database-like format, evolved from Ubi sb0/sm0+sp0.
     * .pk has "BAO" headers pointing to internal or external .spk resources (also BAOs). */

     /* main parse */
    if (!parse_pk_header(&bao, streamFile))
        goto fail;

    return init_vgmstream_ubi_bao_main(&bao, streamFile);
fail:
    return NULL;
}

#if 0
/* .BAO - files with a single BAO from Ubisoft's sound engine ("DARE") games in 2008+ */
VGMSTREAM * init_vgmstream_ubi_bao_file(STREAMFILE *streamFile) {
    ubi_bao_header bao = { 0 };

    /* checks */
    if (!check_extensions(streamFile, "bao"))
        goto fail;

    /* single .bao+sbao found in .forge and similar bigfiles (containing compressed
     * "BAO_0xNNNNNNNN" headers/links, or "Common/English/(etc)_BAO_0xNNNNNNNN" streams).
     * The bigfile acts as index, but external files can be opened as are named after their id.
     * Extension isn't always given but is .bao in some games. */

     /* main parse */
    if (!parse_bao_header(&bao, streamFile))
        goto fail;

    return init_vgmstream_ubi_bao_main(&bao, streamFile);
fail:
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
            vgmstream->interleave_block_size = bao->stream_size / bao->channels;
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

        case RAW_AT3: {
            uint8_t buf[0x100];
            int32_t bytes, block_size, encoder_delay, joint_stereo;

            block_size = 0xc0 * vgmstream->channels;
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

        case UBI_NONE:
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
    /* index and resources always LE (except version) */

    if (target_subsong == 0) target_subsong = 1;

    bao->version = read_32bitBE(0x00, streamFile) & 0x00FFFFFF;
    /* 0x01(3): version, major/minor/release (numbering continues from .sb0/sm0) */
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

    //todo move
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
    else { //todo find_index_bao
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
    const char *res_name;
    uint32_t id;
    uint32_t type;
    int index;

    /* config */
    if (bao->is_file)
        grp_name = "file";
    else
        grp_name = "package";
    id = bao->header_id;
    type = bao->header_type;
    index = -1; //bao->header_index;

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
        if (bao->is_external)
            res_name = bao->resource_name;
        else
            res_name = NULL;
    }

    /* .pk can contain many subsongs, we need something helpful
     * (best done right after subsong detection, since some sequence re-parse types) */
    if (grp_name) {
        if (res_name && res_name[0]) {
            if (index >= 0)
                snprintf(buf,buf_size, "%s/%04d/%02x-%08x/%s", grp_name, index, type, id, res_name);
            else
                snprintf(buf,buf_size, "%s/%02x-%08x/%s", grp_name, type, id, res_name);
        }
        else {
            if (index >= 0)
                snprintf(buf,buf_size, "%s/%04d/%02x-%08x", grp_name, index, type, id);
            else
                snprintf(buf,buf_size, "%s/%02x-%08x", grp_name, type, id);
        }
    }
    else {
        if (res_name && res_name[0]) {
            if (index >= 0)
                snprintf(buf,buf_size, "%04d/%02x-%08x/%s", index, type, id, res_name);
            else
                snprintf(buf,buf_size, "%02x-%08x/%s", type, id, res_name);
        } else {
            if (index >= 0)
                snprintf(buf,buf_size, "%04d/%02x-%08x", index, type, id);
            else
                snprintf(buf,buf_size, "%02x-%08x", type, id);
        }
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
    table_offset = offset + bao->cfg.header_entry_size;
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
    bao->stream_size    = read_32bit(h_offset + bao->cfg.layer_stream_size, streamFile);
    bao->extra_size     = read_32bit(h_offset + bao->cfg.layer_extra_size, streamFile);

    /* prefetch data is inside this BAO and not in other internal stream */
    if (bao->cfg.layer_prefetch_size) {
        bao->prefetch_size  = read_32bit(h_offset + bao->cfg.layer_prefetch_size, streamFile);
        bao->prefetch_skip  = bao->cfg.header_entry_size + bao->extra_size;
    }

    if (bao->layer_count > 16) { /* arbitrary max */
        VGM_LOG("UBI BAO: incorrect layer count\n");
        goto fail;
    }

    /* get 1st layer header in extra table and validate all headers match */
    table_offset = offset + bao->cfg.header_entry_size;
    bao->channels       = read_32bit(table_offset + bao->cfg.layer_channels, streamFile);
    bao->sample_rate    = read_32bit(table_offset + bao->cfg.layer_sample_rate, streamFile);
    bao->stream_type    = read_32bit(table_offset + bao->cfg.layer_stream_type, streamFile);
    bao->num_samples    = read_32bit(table_offset + bao->cfg.layer_num_samples, streamFile);
    bao->stream_id      = read_32bit(table_offset + bao->cfg.layer_stream_id, streamFile);

    for (i = 0; i < bao->layer_count; i++) {
        int channels    = read_32bit(table_offset + bao->cfg.layer_channels, streamFile);
        int sample_rate = read_32bit(table_offset + bao->cfg.layer_sample_rate, streamFile);
        int stream_type = read_32bit(table_offset + bao->cfg.layer_stream_type, streamFile);
        int num_samples = read_32bit(table_offset + bao->cfg.layer_num_samples, streamFile);
        int stream_id   = read_32bit(table_offset + bao->cfg.layer_stream_id, streamFile);
        if (bao->channels != channels || bao->sample_rate != sample_rate || bao->stream_type != stream_type ||
            bao->num_samples != num_samples || bao->stream_id != stream_id) {
            VGM_LOG("UBI BAO: layer headers don't match at %x\n", (uint32_t)table_offset);
            goto fail;
        }

        table_offset += bao->cfg.layer_entry_size;
    }


    //bao->is_external = 1; //todo flag?

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

    if (bao->cfg.header_skip) {
        bao->header_skip = bao->cfg.header_skip;
        /* 0x04(10): descriptor GUID */
        /* 0x14: class 0x20000000 */
        /* 0x18: v2? */
    }
    else {
        bao->header_skip = read_32bit(offset + 0x04, streamFile); /* usually 0x28, rarely 0x24 */
        /* 0x08(10): descriptor GUID */
        /* 0x18: null */
        /* 0x1c: null */
        /* 0x20: class 0x20000000 */
        /* 0x24: v2? (later games) */
    }

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
    size_t header_skip;


  /*bao_version =*/ read_32bitBE(offset+0x00, streamFile); /* force buffer read */

    config_bao_endian(bao, offset, streamFile);
    read_32bit = bao->big_endian ? read_32bitBE : read_32bitLE;

    header_skip = read_32bit(offset+0x04, streamFile);

    bao_class = read_32bit(offset+0x20, streamFile);
    if (bao_class & 0x0FFFFFFF) {
        VGM_LOG("UBI BAO: unknown class %x at %x\n", bao_class, (uint32_t)offset);
        goto fail;
    }

    bao->classes[(bao_class >> 28) & 0xF]++;
    if (bao_class != 0x20000000) /* ignore non-header classes */
        return 1;

    header_type = read_32bit(offset+header_skip+0x04, streamFile);
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

static STREAMFILE * setup_bao_streamfile(ubi_bao_header *bao, STREAMFILE *streamFile) {
    STREAMFILE *new_streamFile = NULL;
    STREAMFILE *temp_streamFile = NULL;
    STREAMFILE *stream_segments[2] = { 0 };

    if (bao->is_prefetched) {
        /* need to join prefetched and streamed part as they're stored separately */
        new_streamFile = open_wrap_streamfile(streamFile);
        if (!new_streamFile) goto fail;
        temp_streamFile = new_streamFile;

        new_streamFile = open_clamp_streamfile(temp_streamFile, bao->prefetch_offset, bao->prefetch_size);
        if (!new_streamFile) goto fail;
        stream_segments[0] = new_streamFile;
        temp_streamFile = NULL;

        /* open external file */
        new_streamFile = open_streamfile_by_filename(streamFile, bao->resource_name);
        if (!new_streamFile) { VGM_LOG("UBI BAO: external stream '%s' not found\n", bao->resource_name); goto fail; }
        temp_streamFile = new_streamFile;

        new_streamFile = open_clamp_streamfile(temp_streamFile, bao->main_offset, bao->main_size);
        if (!new_streamFile) goto fail;
        stream_segments[1] = new_streamFile;
        temp_streamFile = NULL;

        new_streamFile = open_multifile_streamfile(stream_segments, 2);
        if (!new_streamFile) goto fail;
        temp_streamFile = new_streamFile;
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

    return temp_streamFile;

fail:
    close_streamfile(stream_segments[0]);
    close_streamfile(stream_segments[1]);
    close_streamfile(temp_streamFile);

    return NULL;
}


static void config_bao_endian(ubi_bao_header * bao, off_t offset, STREAMFILE *streamFile) {
    //todo this could be done once as all BAOs share endianness
    //todo fix for later versions without size field

    bao->big_endian = guess_endianness32bit(offset+0x04, streamFile);
}


static void config_bao_sequence(ubi_bao_header * bao, off_t sequence_count, off_t sequence_single, off_t sequence_loop, off_t entry_size) {
    /* sequence header and chain table */
    bao->cfg.sequence_sequence_count    = sequence_count;
    bao->cfg.sequence_sequence_single   = sequence_count;
    bao->cfg.sequence_sequence_loop     = sequence_count;
    bao->cfg.sequence_entry_size        = entry_size;
    bao->cfg.sequence_entry_number      = 0x00;
}
static void config_bao_layer_h(ubi_bao_header * bao, off_t layer_count, off_t stream_size, off_t extra_size, off_t prefetch_size) {
    /* layer header in the main BAO */
    bao->cfg.layer_layer_count          = layer_count;
    bao->cfg.layer_stream_size          = stream_size;
    bao->cfg.layer_extra_size           = extra_size;
    bao->cfg.layer_prefetch_size        = prefetch_size;
}
static void config_bao_layer_s(ubi_bao_header * bao, off_t entry_size, off_t sample_rate, off_t channels, off_t stream_type, off_t num_samples, off_t stream_id) {
    /* layer sub-headers in extra table */
    bao->cfg.layer_entry_size           = entry_size;
    bao->cfg.layer_sample_rate          = sample_rate;
    bao->cfg.layer_channels             = channels;
    bao->cfg.layer_stream_type          = stream_type;
    bao->cfg.layer_num_samples          = num_samples;
    bao->cfg.layer_stream_id            = stream_id;
}

static int config_bao_version(ubi_bao_header * bao, STREAMFILE *streamFile) {

    /* Ubi BAO evolved from Ubi SB and are conceptually quite similar, see that first.
     *
     * BAOs (binary audio objects) are first divided into "classes":
     * - 0x10000000: events (links by id to another event or header BAO)
     * - 0x20000000: stream header
     * - 0x30000000: internal stream (in .pk)
     * - 0x40000000: package info?
     * - 0x50000000: external stream (in .spk)
     * - 0x70000000: project info? (sometimes special id 0x7fffffff)
     * - 0x80000000: unknown (some id/info?)
     * Class 1/2/3 are roughly equivalent to Ubi SB's section1/2/3.
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
     * Right after base BAO size is the extra table for that BAO (what sectionX had).
     *
     * Most types + tables are pretty much the same (with config styles ported straight) but
     * later versions can "prefetch" part of the data (signaled by a size in the header).
     * The header points to a stream ID, normally external, but with prefetch enabled it also
     * exists as an internal stream too, so must join both internal and external streams.
     * For layers though the prefetch part is in the same BAO as the layer header, but the
     * ID still references both (layer header with prefetch and external stream).
     * I
     */

    bao->allowed_types[0x01] = 1;
  //bao->allowed_types[0x05] = 1;
  //bao->allowed_types[0x06] = 1;


    /* all this config is relative to header_skip */

    bao->cfg.header_id      = 0x00;
    bao->cfg.header_type    = 0x04;
    bao->cfg.header_skip    = 0x00; /* must set for later versions that removed the field */

    bao->cfg.audio_external_and     = 1;
    bao->cfg.audio_loop_and         = 1;

    switch(bao->version) {
        case 0x001F0008: /* Rayman Raving Rabbids: TV Party (Wii)-pk */
        case 0x001F0011: /* Naruto: The Broken Bond (X360)-pk */
        case 0x0022000D: /* Just Dance (Wii)-pk */
            bao->cfg.header_entry_size      = 0xa8;//todo: Naruto = 0xa4, Rayman/JD = 0xa8
            //skip 0x28

            bao->cfg.audio_stream_size      = 0x08;
            bao->cfg.audio_stream_id        = 0x1c;
            bao->cfg.audio_external_flag    = 0x28; /* maybe 0x30 */
            bao->cfg.audio_loop_flag        = 0x34;
            bao->cfg.audio_channels         = 0x44;
            bao->cfg.audio_sample_rate      = 0x4c;
            bao->cfg.audio_num_samples      = 0x54;
            bao->cfg.audio_num_samples2     = 0x5c;

            bao->cfg.audio_stream_type      = 0x64;
            bao->cfg.audio_prefetch_size    = 0x74;
            bao->cfg.audio_xma_offset       = 0x7c; /* only if internal */
            bao->cfg.audio_dsp_offset       = 0x80;

            bao->cfg.codec_map[0x01]        = RAW_PCM;
          //bao->cfg.codec_map[0x02]        = FMT_OGG;
            bao->cfg.codec_map[0x03]        = UBI_IMA;
            bao->cfg.codec_map[0x05]        = RAW_XMA1;
            bao->cfg.codec_map[0x09]        = RAW_DSP;

            return 1;

        case 0x00220015: /* James Cameron's Avatar: The Game (PSP)-pk */
        case 0x0022001E: /* Prince of Persia: The Forgotten Sands (PSP)-pk */
            bao->cfg.header_entry_size      = 0x84;
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
            bao->cfg.header_entry_size      = 0xb8;
            //skip 0x28

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
            bao->cfg.audio_xma_offset       = 0x8c; /* -1 if null */

            config_bao_sequence(bao, 0x34, 0x54, 0x58, 0x14);

            config_bao_layer_h(bao, 0x28, 0x50, 0x5c, 0x54);
            config_bao_layer_s(bao, 0x30, 0x00, 0x04, 0x08, 0x14, 0x2c);

            bao->cfg.codec_map[0x01] = RAW_PCM;
            bao->cfg.codec_map[0x02] = UBI_IMA;
            bao->cfg.codec_map[0x03] = FMT_OGG;
            bao->cfg.codec_map[0x04] = RAW_XMA2;

            return 1;

        case 0x00250108: /* Scott Pilgrim vs the World (PS3/X360)-pk */
        case 0x0025010A: /* Prince of Persia: The Forgotten Sands (PS3/X360)-file */
            bao->cfg.header_entry_size      = 0xb4;
            //skip 0x28

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

            config_bao_layer_h(bao, 0x28, 0x48, 0x50, 0x54);
            config_bao_layer_s(bao, 0x30, 0x00, 0x04, 0x08, 0x14, 0x2c);

            bao->cfg.codec_map[0x01] = RAW_PCM;
            bao->cfg.codec_map[0x02] = UBI_IMA; /* assumed */
            bao->cfg.codec_map[0x03] = FMT_OGG; /* assumed */
            bao->cfg.codec_map[0x04] = RAW_XMA2;
            bao->cfg.codec_map[0x05] = RAW_PSX;
            bao->cfg.codec_map[0x06] = RAW_AT3;

            return 1;

        case 0x001B0100: /* Assassin's Creed (PS3/X360/PC)-file */
        case 0x001B0200: /* Beowulf (PS3)-file */
        case 0x001C0000: /* Lost: Via Domus (PS3)-file */
        case 0x001F0010: /* Prince of Persia 2008 (PS3/X360)-file, Far Cry 2 (PS3)-file */
        case 0x00260102: /* Prince of Persia Trilogy HD (PS3)-pk */
        case 0x00280306: /* Far Cry 3: Blood Dragon (X360)-file */
        case 0x00290106: /* Splinter Cell: Blacklist (PS3)-file */
        default: /* others possibly using BAO: Avatar X360/PS3/PC, Just Dance, Watch_Dogs, Far Cry Primal, Far Cry 4 */
            VGM_LOG("UBI BAO: unknown BAO version %08x\n", bao->version);
            return 0;
    }

    VGM_LOG("UBI BAO: unknown BAO version %08x\n", bao->version);
    return 0;
}
