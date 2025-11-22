#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "ubi_bao_streamfile.h"

// BAO's massive confg and variations are handled here
#include "ubi_bao_config.h"
#include "ubi_bao_parser.h"

#define BAO_MIN_VERSION 0x1B
#define BAO_MAX_VERSION 0x2B


static bool parse_header(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset);
static bool parse_bao(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset, int target_subsong);
static bool parse_pk(ubi_bao_header_t* bao, STREAMFILE* sf);
static bool parse_spk(ubi_bao_header_t* bao, STREAMFILE* sf);
static VGMSTREAM* init_vgmstream_ubi_bao_header(ubi_bao_header_t* bao, STREAMFILE* sf);
static STREAMFILE* setup_bao_streamfile(ubi_bao_header_t* bao, STREAMFILE* sf);
static STREAMFILE* open_atomic_bao(uint32_t file_id, bool is_stream, STREAMFILE* sf);
static bool find_package_bao(uint32_t target_id, STREAMFILE* sf, uint32_t* p_offset, uint32_t* p_size);
static bool find_spk_bao(uint32_t target_id, STREAMFILE* sf, uint32_t* p_offset, uint32_t* p_size);

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

    return init_vgmstream_ubi_bao_header(&bao, sf);
}


/* .SPK - mini package with BAOs, used in Dunia engine games [Avatar (multi), Far Cry 4 (multi)] */
VGMSTREAM* init_vgmstream_ubi_bao_spk(STREAMFILE* sf) {

    /* checks */
    uint32_t header_id = read_u32le(0x00, sf); //always LE
    if (header_id != get_id32be("SPK\1") && header_id != get_id32be("SPK\4"))
        return NULL;

    uint8_t type = header_id & 0xFF;
    if (type != 0x01 && type != 0x04)
        return NULL;

    if (!check_extensions(sf, "spk"))
        return NULL;

    /* .spk is a simpler package (unrelated from .pk+.spk), possibly evolved from the .FAT+BIN index
     * found in some Yeti engine games. It often has header and cue BAOs together (typically only 1 header)
     * and loose .sbao streams. Internally it's considered ATOMIC. */
    ubi_bao_header_t bao = {0};
    bao.archive = ARCHIVE_SPK;

    /* main parse */
    if (!parse_spk(&bao, sf))
        return NULL;

    return init_vgmstream_ubi_bao_header(&bao, sf);
}

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
    vgmstream->stream_size = bao->prefetch_size + bao->stream_size;

    vgmstream->num_samples = bao->num_samples;
    vgmstream->loop_start_sample = bao->loop_start;
    vgmstream->loop_end_sample = bao->num_samples;

    switch(bao->codec) {
        case UBI_IMA:
        case UBI_IMA_seek: 
        case UBI_IMA_mark: {
            //TODO: IMA seekable has a seek table in the v6 frame header and blocks are preceded by adpcm hist+step
            //TODO: IMA with markers doesn't have the v6 header but has seek table (in machine endianness)
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
            bool is_xma_stream;

            // TODO: fix, this varies when multistreams are used (ok 99% of the time)
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

            is_xma_stream = bao->is_stream || bao->type == TYPE_LAYER || (bao->cfg.v1_bao && bao->codec == RAW_XMA1_str);

            if (bao->cfg.audio_fix_xma_memory_baos && !is_xma_stream) {
                // for unfathomable reasons, certain XMA2 memory BAOs behave like streams (but also include XMA header in BAO).
                // No apparent diffs/flags in header BAOs, memory BAOs only seem to differ in value at 0x24
                // (0x02=has header, but doesn't seem true for other versions).
                uint16_t xma_id = read_u16be(start_offset, sf_data);
                if (xma_id == 0x166)
                    is_xma_stream = true;
            }

            if (is_xma_stream) {
                uint8_t flag, bits_per_frame;
                uint32_t sec1_num, sec2_num, sec3_num;
                size_t header_size, frame_size;

                // skip XMA/fmt header chunk
                off_t header_offset = start_offset + chunk_size;

                // skip custom XMA seek(?) table
                if (bao->codec == RAW_XMA1_mem || bao->codec == RAW_XMA1_str) {
                    flag        = read_u8   (header_offset + 0x00, sf_data);
                    sec2_num    = read_u32be(header_offset + 0x04, sf_data); // number of XMA frames
                    frame_size  = 0x800;
                    sec1_num    = read_u32be(header_offset + 0x08, sf_data);
                    sec3_num    = read_u32be(header_offset + 0x0c, sf_data);
                    header_size = chunk_size + 0x10;
                }
                else {
                    flag        = read_u8   (header_offset + 0x00, sf_data);
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

                // xma header in BAO data
                sf_xmah = sf_data;
                sf_xmad = sf_data;
                chunk_offset = 0x00;
                start_offset += header_size;
                data_size = sec2_num * frame_size;
            }
            else {
                // xma header in header BAO after extradata (XMA1_MEM in v1_bao or XMA1_STR / XMA2* otherwise)
                sf_xmah = sf_head;
                sf_xmad = sf_data;
                if (bao->extradata_size) {
                    // extradata format (v29+):
                    //  null
                    //  0x09000000?
                    //  chunk size (always 0x34)
                    //  -1
                    chunk_offset = bao->extradata_offset + 0x10;
                }
                else {
                    chunk_offset = bao->header_offset + bao->header_size + bao->extra_size;
                }
                start_offset = 0x00;
                data_size = bao->stream_size;
            }

            vgmstream->codec_data = init_ffmpeg_xma_chunk_split(sf_xmah, sf_xmad, start_offset, data_size, chunk_offset, chunk_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->stream_size = data_size;

            // somehow num_samples may contain garbage (probably DARE just plays XMA data and ignores num_samples)
            if (bao->cfg.audio_fix_xma_samples && bao->codec == RAW_XMA2_new && 
                    (vgmstream->num_samples > 0x00FFFFFF || vgmstream->num_samples < -0x00FFFFFF)) {
                VGM_LOG("UBI BAO: wrong xma samples\n");
                vgmstream->num_samples = read_s32be(chunk_offset + 0x18, sf_xmah);
                xma_fix_raw_samples(vgmstream, sf_data, start_offset, data_size,0, true, false);
            }
            else {
                xma_fix_raw_samples(vgmstream, sf_data, start_offset, data_size,0, false, false);

            }
            break;
        }

        case RAW_AT3: {
            // ATRAC3 sizes: low (66 kbps) / mid (105 kbps) / high (132 kbps)
            static const int block_types[3] = { 0x60, 0x98, 0xC0 };
            int block_align, encoder_delay;

            if (bao->stream_subtype < 1 || bao->stream_subtype > 2) {
                VGM_LOG("UBI BAO: unknown ATRAC3 mode\n"); // mode 0 doesn't seem used
                goto fail;
            }

            block_align = block_types[bao->stream_subtype] * vgmstream->channels;
            encoder_delay = 0; // num_samples is full bytes-to-samples (unlike FMT_AT3) and comparing X360 vs PS3 games seems ok

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(sf_data, start_offset, vgmstream->stream_size, vgmstream->num_samples, vgmstream->channels, vgmstream->sample_rate, block_align, encoder_delay);
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

            // num samples is the same as Ogg samples
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case RAW_MP3: {
            mpeg_custom_config cfg = {0};
            // has extradata in v29 (size 0x2c), seem to be some kind of config per channel?

            cfg.data_size = bao->stream_size;

            vgmstream->codec_data = init_mpeg_custom(sf_data, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            break;
        }
#endif
#ifdef VGM_USE_ATRAC9
        case RAW_AT9: {
            atrac9_config cfg = {0};

            uint32_t extradata_suboffset = bao->extradata_offset;
            if (bao->type == TYPE_LAYER) {
                extradata_suboffset += 0x0C; //-1 x3 (seen in mono layers)
                // no diffs in v2B
            }
            else {
                extradata_suboffset += 0x34; //-1 x13 (mono or stereo layers)
                if (bao->cfg.engine_version >= 0x2B00)
                    extradata_suboffset += 0x04; // -1
            }

            // extradata format (v2A+)
            //  00: low numbers and 0x81811C24 (v2A) or 0x96B58C11 (v2B)
            //  10: frame size
            //  14: low numbers and 0x81811C24 (v2A) or 0x96B58C11 (v2B) (similar to the prev ones)
            //  24: ATRAC9 config
            //  28: fixed? 0x0F
            //  2c: flag 1
            //  30: data size
            //  34: -1
            //  38+: 'preroll' data (repeat of the first 2 stream/memory frames, to setup the decoder?)

            cfg.channels = bao->channels;
            cfg.config_data = read_u32be(extradata_suboffset + 0x24, sf_head);
            cfg.encoder_delay = 0; //TODO: research default, doesn't seem to be 2 frames

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;
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

        //TODO: improve (overwrites standar values with current layer)
        bao->stream_size = get_streamfile_size(temp_sf);
        bao->sample_rate = bao->layer[i].sample_rate;
        bao->channels = bao->layer[i].channels;
        bao->num_samples = bao->layer[i].num_samples;
        bao->extradata_offset = bao->layer[i].extradata_offset;
        bao->extradata_size = bao->layer[i].extradata_size;

        total_channels += bao->layer[i].channels;

        /* build the layer VGMSTREAM (standard with custom streamfile) */
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
        // TODO: improve, this also copies the big layer/sequence arrays (though not that common)
        ubi_bao_header_t temp_bao = *bao; // memcpy'ed
        int entry_id = bao->sequence_chain[i];

        if (bao->archive == ARCHIVE_ATOMIC) {
            /* open memory audio BAO */
            sf_chain = open_atomic_bao(entry_id, false, sf);
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
        else if (bao->archive == ARCHIVE_PK) {
            /* find memory audio BAO */
            uint32_t entry_offset;
            if (!find_package_bao(entry_id, sf, &entry_offset, NULL)) {
                VGM_LOG("UBI BAO: expected chain id %08x not found\n", entry_id);
                goto fail;
            }

            /* parse BAO */
            if (!parse_header(&temp_bao, sf, entry_offset))
                goto fail;
        }
        else if (bao->archive == ARCHIVE_SPK) {
            /* find memory audio BAO */
            uint32_t entry_offset;
            if (!find_spk_bao(entry_id, sf, &entry_offset, NULL)) {
                VGM_LOG("UBI BAO: expected chain id %08x not found\n", entry_id);
                goto fail;
            }

            /* parse BAO */
            if (!parse_header(&temp_bao, sf, entry_offset))
                goto fail;
        }
        else {
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

        /* save current (silences don't have values, so this ensures they know when memcpy'ed) */
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
    int32_t num_samples = bao->silence_duration * sample_rate;


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


    ;VGM_LOG("UBI BAO: target at %x + %x (type=%i, codec=%i), h_id=%08x, s_id=%08x (s=%x), str=%i, pft=%i (s=%x)\n",
        bao->header_offset, bao->header_size, bao->header_type, bao->type == TYPE_SEQUENCE ? -1 : bao->stream_type,
        bao->header_id, bao->stream_id, bao->stream_size, bao->is_stream, bao->is_prefetch, bao->prefetch_size);

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

    if (!vgmstream)
        goto fail;

    // create usable name, except if it's a sequence parsing its individual entries
    if (!(bao->sequence_count && bao->type != TYPE_SEQUENCE)) {
        char readable_name[STREAM_NAME_SIZE];
        build_readable_name(readable_name, sizeof(readable_name), bao);
        strcpy(vgmstream->stream_name, readable_name);
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ************************************************************************* */

/* find BAO resource within pk's external resource table */
static bool find_package_external(uint32_t target_id, STREAMFILE* sf, uint32_t* p_offset, uint32_t* p_size, char* external_name, int external_name_len) {

    uint32_t externals_offset   = read_u32le(0x08, sf);
    int externals_count         = read_s32le(externals_offset + 0x00, sf);
    uint32_t strings_size       = read_u32le(externals_offset + 0x04, sf);

    /* parse resource table to external stream (may be empty, or exist even with nothing in the file) */
    uint32_t offset = externals_offset + 0x04+0x04 + strings_size;
    for (int i = 0; i < externals_count; i++) {
        uint32_t external_id        = read_u32le(offset + 0x00, sf);
        uint32_t name_offset        = read_u32le(offset + 0x04, sf);
        uint32_t external_offset    = read_u32le(offset + 0x08, sf); // absolute offset within external .spk (skipping index tables) 
        uint32_t external_size      = read_u32le(offset + 0x0c, sf);

        if (external_id == target_id) {
            read_string(external_name, external_name_len, externals_offset + 0x04 + 0x04 + name_offset, sf);
            if (p_offset) *p_offset = external_offset;
            if (p_size) *p_size = external_size;
            return true;
        }

        offset += 0x10;
    }

    return false;
}
/* find BAO within pk's index */
static bool find_package_bao(uint32_t target_id, STREAMFILE* sf, uint32_t* p_offset, uint32_t* p_size) {

    uint32_t index_size = read_u32le(0x04, sf);
    int index_entries = index_size / 0x08;
    uint32_t index_header_size = 0x40;

    /* parse index to get target BAO */
    uint32_t offset = index_header_size + index_size;
    for (int i = 0; i < index_entries; i++) {
        uint32_t bao_id     = read_u32le(index_header_size + 0x08 * i + 0x00, sf);
        uint32_t bao_size   = read_u32le(index_header_size + 0x08 * i + 0x04, sf);

        if (bao_id == target_id) {
            if (p_offset) *p_offset = offset;
            if (p_size) *p_size = bao_size;
            return true;
        }

        offset += bao_size;
    }

    return false;
}

/* parse a .pk (package) file: index + BAOs + external .spk resource table. We want header
 * BAOs pointing to internal/external stream BAOs (.spk is the same, with stream BAOs only).
 * A fun feature of .pk is that different BAOs in a .pk can point to different .spk BAOs
 * that actually hold the same data, with different GUID too, somehow. */
static bool parse_pk(ubi_bao_header_t* bao, STREAMFILE* sf) {
    STREAMFILE* sf_index = NULL;
    STREAMFILE* sf_test = NULL;

    /* format: 0x01=package index, 0x02=package BAO */
    if (read_u8(0x00, sf) != 0x01)
        return false;
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

        bao_offset += bao_size; // files simply concat BAOs
    }

    close_streamfile(sf_index);
    close_streamfile(sf_test);
    return true;
fail:
    close_streamfile(sf_index);
    close_streamfile(sf_test);
    return false;
}

/* ************************************************************************* */

/* find BAO within .spk */
static bool find_spk_bao(uint32_t target_id, STREAMFILE* sf, uint32_t* p_offset, uint32_t* p_size) {
    //TODO: unify with parse_spk?

    uint8_t type = read_u8(0x00, sf);
    if (type != 0x01 && type != 0x02 && type != 0x04)
        return false;

    bool has_related_baos = type == 0x01;

    int entries = read_s32le(0x04, sf);

    // check if ID is list
    int target_entry = -1;
    for (int i = 0; i < entries; i++) {
        uint32_t entry_id = read_u32le(0x08 + 0x04 * i, sf);
        if (entry_id == target_id) {
            target_entry = i;
            break;
        }
    }

    // not part of this .spk (rare)
    if (target_entry < 0) {
        return false;
    }

    uint32_t offset = 0x08 + entries * 0x04;
    for (int i = 0; i < entries; i++) {
        if (has_related_baos) {
            int related_entries = read_s32le(offset, sf);
            offset += 0x04 + related_entries * 0x04;
        }

        uint32_t bao_size = read_u32le(offset, sf);
        offset += 0x04;

        if (i == target_entry) {
            if (p_offset) *p_offset = offset;
            if (p_size) *p_size = bao_size;
            return true;
        }

        offset += align_size_to_block(bao_size, 0x04);
    }

    return false; //??
}


/* parse a .spk (package) file: index + BAOs, similar to .pk but simpler. 
 * - 0x00: 0xNN4B5053 ("SPK\N" LE) (N: v1=Avatar/FC2, v2=Watch Dogs, v4=FC3/FC4)
 * - 0x04: BAO count
 * - 0x08: BAO ids inside (0x04 * BAO count)
 * - (v1) per BAO:
 *   - 0x00: table count
 *   - 0x04: ids related to this BAO? (0x04 * table count)
 *   - 0x08/NN: BAO size
 *   - 0x0c/NN+: BAO data up to size + padding to 0x04
 * - (v2/v4) per BAO:
 *   - 0x00: BAO size
 *   - 0xNN: BAO data up to size
 *
 * BAOs can be inside .spk (memory) or external .sbao. Header is always LE
 */
static bool parse_spk(ubi_bao_header_t* bao, STREAMFILE* sf) {

    uint8_t type = read_u8(0x00, sf);
    if (type != 0x01 && type != 0x02 && type != 0x04)
        return false;

    int target_subsong = sf->stream_index;
    if (target_subsong <= 0) target_subsong = 1;

    bool has_related_baos = type == 0x01;
    bool is_version_parsed = false;


    uint32_t offset = 0x04;
    int entries = read_s32le(offset, sf);

    offset += 0x04 + entries * 0x04;
    for (int i = 0; i < entries; i++) {
        if (has_related_baos) {
            int related_entries = read_s32le(offset, sf);
            offset += 0x04 + related_entries * 0x04;
        }

        uint32_t bao_size = read_u32le(offset, sf);
        offset += 0x04;

        if (!is_version_parsed) {
            uint32_t version = read_u32be(offset, sf) & 0x00FFFFFF;
            if (!ubi_bao_config_version(&bao->cfg, sf, version))
                return false;
            is_version_parsed = true;
        }

        //;VGM_LOG("UBI BAO: spk index %i: %x + %x\n", i, offset, bao_size);

        /* parse and continue to find out total_subsongs */
        if (!parse_bao(bao, sf, offset, target_subsong))
            return false;

        // memory Ogg need extra padding (other codecs are already aligned) [Avatar (PC)]
        bao_size = align_size_to_block(bao_size, 0x04);
        offset += bao_size;
    }

    //;VGM_LOG("UBI BAO: class "); {int i; for (i=0;i<16;i++){ VGM_ASSERT(bao->classes[i],"%02x=%i ",i,bao->classes[i]); }} VGM_LOG("\n");
    //;VGM_LOG("UBI BAO: types "); {int i; for (i=0;i<16;i++){ VGM_ASSERT(bao->types[i],"%02x=%i ",i,bao->types[i]); }} VGM_LOG("\n");

    return true;
}

/* ************************************************************************* */

// .pk/spk can contain many subsongs, we need something helpful that shows stream file used.
static void build_readable_name(char* buf, size_t buf_size, ubi_bao_header_t* bao) {
    const char* fmt_name = NULL;
    const char* loc_name = NULL;
    const char* res_name = NULL;

    if (bao->type == TYPE_NONE || bao->type == TYPE_IGNORED || bao->total_subsongs <= 0)
        return;

    /* config */
    if (bao->archive == ARCHIVE_ATOMIC)
        fmt_name = "atomic";
    else if (bao->archive == ARCHIVE_PK)
        fmt_name = "package";
    else if (bao->archive == ARCHIVE_SPK)
        fmt_name = "spk";
    else
        fmt_name = "?";

    if (bao->is_inline)
        loc_name = "inline";
    else if (bao->is_prefetch)
        loc_name = "p-strm";
    else if (bao->is_stream)
        loc_name = "stream";
    else
        loc_name = "memory";

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
    uint32_t s_id = bao->is_inline ? 0 : bao->stream_id;
    uint32_t type = bao->header_type;

    if (res_name && res_name[0]) {
        snprintf(buf,buf_size, "%s/%s/%02x-%08x/%08x/%s", fmt_name, loc_name, type, h_id, s_id, res_name);
    }
    else {
        snprintf(buf,buf_size, "%s/%s/%02x-%08x/%08x", fmt_name, loc_name, type, h_id, s_id);
    }
}

static bool parse_header(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset) {
    return ubi_bao_parse_header(bao, sf, offset);
}


/* parse a full BAO, DARE's main audio format which can be inside other formats */
static bool parse_bao(ubi_bao_header_t* bao, STREAMFILE* sf, off_t offset, int target_subsong) {
    return ubi_bao_parse_bao(bao, sf, offset, target_subsong);
}

/* ************************************************************************* */
// External atomic BAOs have varying names, sometimes hashed.
// Each engine+game typically has a generic bigfile that acts like a database/index,
// but must be extracted to get usable .bao/sbao or similar names.
//
// - Anvil .forge: full names found in bigfile (extensionless), but seem unused and engine loads by id
//   (.bao from debug strings in AC1, .bao/.sbao in Shaun White Snowboarding).
//   Within .forge memory BAOs are compressed (subfiles per 'area') and stream BAOs uncompressed.
// - Yeti .fat+.bin: ids in bigfile, always %08x.bao (streamed or memory) from loose X360 files and debug strings
//   Streams can be files or inside stream.bin
// - Dunia .pak: %08x.sbao (streams, memory files are in .spk)
// - Dunia .fat+dat: %08x.sbao (streams, memory files are in .spk)
//   Hashed custom CRC32 (v5 .fat) or CRC64 (v9 .fat) from "soundbinary\%08x.sbao" (see Gibbed.Dunia for the algorithm)
// - GEAR bigfile: same CRC32 hash, varies per game (sometimes just %08x.bao/sbao, packages\(name).spk, 
//   packages\(lang)\(name).lspk, etc)
// - Opal lin+fat: ids in bigfile, internal names are similar to 'DARE_FFFFFFFF_20004118.BAO'
// Could try to limit names per type to avoid extra fopens but config is getting rather complex.

static const char* atomic_memory_baos[] = {
    "%08x.bao", // common
    // .forge names
    "BAO_0x%08x",
    "Common_BAO_0x%08x",
    "BAO_0x%08x.bao", // used?
};
static const int atomic_memory_baos_count = sizeof(atomic_memory_baos) / sizeof(atomic_memory_baos[0]);

static const char* atomic_stream_baos[] = {
    "%08x.sbao", // common
    "%08x.bao", // .fat+bin
    // .forge names (unused)
    "Common_BAO_0x%08x",
    "Common_BAO_0x%08x.sbao", // used?
    // .forge language names, found in Assassin's Creed 1's exes and Shaun White Snowboarding (X360) exe in listed order
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
static const int atomic_stream_baos_count = sizeof(atomic_stream_baos) / sizeof(atomic_stream_baos[0]);


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

/* Opens a BAO's companion atomic BAO (memory or stream), often in different naming schemes. */
static STREAMFILE* open_atomic_bao(uint32_t file_id, bool is_stream, STREAMFILE* sf) {
    STREAMFILE* sf_bao = NULL;
    char buf[255];
    size_t buf_size = sizeof(buf);

    const char** names = is_stream ? atomic_stream_baos : atomic_memory_baos;
    int count = is_stream ? atomic_stream_baos_count : atomic_memory_baos_count;

    sf_bao = open_atomic_bao_list(file_id, names, count, buf, buf_size, sf);
    if (sf_bao) return sf_bao;

#if 0
    // could try to open different hashed names, but if we now the exact hash it may be 
    // simpler to exernally rename files instead
    if (bao->cfg.hash == ...) {
        ...
    }
#endif

    goto fail;
fail:
    close_streamfile(sf_bao);

    vgm_logi("UBI BAO: failed opening atomic BAO id %08x\n", file_id);
    return NULL;
}


static STREAMFILE* setup_atomic_bao_common(uint32_t resource_id, bool is_stream, uint32_t clamp_offset, uint32_t clamp_size, STREAMFILE* sf) {
    STREAMFILE* temp_sf = NULL;

    temp_sf = open_atomic_bao(resource_id, is_stream, sf);
    if (!temp_sf) goto fail;

    temp_sf = open_clamp_streamfile_f(temp_sf, clamp_offset, clamp_size);
    if (!temp_sf) goto fail;

    return temp_sf;
fail:
    close_streamfile(temp_sf);
    return NULL;
}

// opens BAO data within header BAO (probably any BAO data, seen ubi-ima, mp3 and atrac9 layers)
static STREAMFILE* open_inline_bao(ubi_bao_header_t* bao, STREAMFILE* sf) {
    STREAMFILE* temp_sf = NULL;
    uint32_t clamp_offset = bao->inline_offset;
    uint32_t clamp_size = bao->inline_size;

    if (bao->cfg.engine_version <= 0x2900) {
        // v29 mini header, same as XMA extradata format
        //  null
        //  0x09000000?
        //  chunk size (always 0x34)
        //  -1
        clamp_offset += 0x10;
        clamp_size -= 0x10;
    }

    temp_sf = open_wrap_streamfile(sf); // wrap current SF (header) to avoid it being closed later
    if (!temp_sf) goto fail;

    temp_sf = open_clamp_streamfile_f(temp_sf, clamp_offset, clamp_size);
    if (!temp_sf) goto fail;

    return temp_sf;
fail:
    close_streamfile(temp_sf);
    return NULL;
}


static STREAMFILE* open_memory_bao_atomic(ubi_bao_header_t* bao, STREAMFILE* sf) {
    uint32_t memory_offset = bao->memory_skip;
    uint32_t memory_size = bao->is_prefetch ? bao->prefetch_size : bao->stream_size;
    uint32_t internal_id = bao->stream_id;

    if (bao->is_prefetch && bao->cfg.v1_bao) {
        // header only defines stream_id, prefetch is implicitly a memory BAO
        // ex. AC1/Beowulf X360: stream=5NNNNNNN and memory prefetch=3NNNNNNN
        internal_id = (internal_id & 0x0FFFFFFF) | 0x30000000;
    }

    return setup_atomic_bao_common(internal_id, false, memory_offset, memory_size, sf);
}

static STREAMFILE* open_stream_bao_atomic(ubi_bao_header_t* bao, STREAMFILE* sf) {
    uint32_t stream_offset = bao->stream_skip;
    uint32_t stream_size = bao->stream_size - bao->prefetch_size;
    uint32_t external_id = bao->stream_id;

    return setup_atomic_bao_common(external_id, true, stream_offset, stream_size, sf);
}


static STREAMFILE* open_memory_bao_package(ubi_bao_header_t* bao, STREAMFILE* sf) {
    STREAMFILE* temp_sf = NULL;
    uint32_t memory_offset = bao->memory_skip;
    uint32_t memory_size = bao->is_prefetch ? bao->prefetch_size : bao->stream_size;
    uint32_t internal_id = bao->stream_id;

    uint32_t resource_offset = 0;
    uint32_t resource_size = 0;

    // find in current .pk
    if (!find_package_bao(internal_id, sf, &resource_offset, &resource_size)) {
        VGM_LOG("UBI BAO: expected internal id %08x not found in .pk\n", internal_id);
        return NULL;
    }

    // catch bad config
    if (bao->is_prefetch && bao->prefetch_size + bao->memory_skip != resource_size) {
        VGM_LOG("UBI BAO: unexpected prefetch size %x vs %x\n", bao->prefetch_size + bao->memory_skip, resource_size);
        return NULL;
    }


    memory_offset += resource_offset;

    // in some cases, stream size value from audio header can be bigger (~0x18)
    // than actual audio chunk o_O [Rayman Raving Rabbids: TV Party (Wii)] */
    if (!bao->is_prefetch && bao->stream_size > resource_size - bao->memory_skip) {
        VGM_LOG("UBI BAO: bad stream size found: %x + %x vs %x\n", bao->stream_size, bao->memory_skip, resource_size);

        // too big is usually bad config
        if (bao->stream_size > resource_size + bao->header_size) {
            VGM_LOG("UBI BAO: bad stream config at %x\n", bao->header_offset);
            return false;
        }

        memory_size = resource_size - bao->memory_skip;
    }

    temp_sf = open_wrap_streamfile(sf); // wrap current SF (header) to avoid it being closed later
    if (!temp_sf) goto fail;

    temp_sf = open_clamp_streamfile_f(temp_sf, memory_offset, memory_size);
    if (!temp_sf) goto fail;

    return temp_sf;
fail:
    close_streamfile(temp_sf);
    return NULL;
}

static STREAMFILE* open_stream_bao_package(ubi_bao_header_t* bao, STREAMFILE* sf) {
    STREAMFILE* temp_sf = NULL;
    uint32_t stream_offset = bao->stream_skip;
    uint32_t stream_size = bao->stream_size - bao->prefetch_size;
    uint32_t external_id = bao->stream_id;

    char external_name[256];
    uint32_t resource_offset = 0;
    uint32_t resource_size = 0;

    // find external .spk
    if (!find_package_external(external_id, sf, &resource_offset, &resource_size, external_name, sizeof(external_name))) {
        VGM_LOG("UBI BAO: expected resource id %08x not found in .pk\n", external_id);
        goto fail;
    }

    if (bao->stream_size != resource_size - bao->stream_skip + bao->prefetch_size) {
        VGM_LOG("UBI BAO: stream vs resource size mismatch (res %x vs str=%x, skip=%x, pre=%x)\n", resource_size, bao->stream_size, bao->stream_skip, bao->prefetch_size);

        /* rarely resource has more data than stream (sometimes a few bytes, others +0x100000)
         * sometimes short song versions, but not accessed? no samples/sizes/cues/etc in header seem to refer to that [Just Dance (Wii)]
         * Michael Jackson The Experience also uses prefetch size + bad size (ignored) */
        if (!bao->cfg.audio_ignore_external_size && bao->prefetch_size)
            goto fail;
    }

    temp_sf = open_streamfile_by_filename(sf, external_name);
    if (!temp_sf) {
        vgm_logi("UBI BAO: external file '%s' not found (put together)\n", external_name); 
        goto fail; 
    }


    stream_offset += resource_offset;

    temp_sf = open_clamp_streamfile_f(temp_sf, stream_offset, stream_size);
    if (!temp_sf) goto fail;

    return temp_sf;
fail:
    close_streamfile(temp_sf);
    return NULL;
}


static STREAMFILE* open_memory_bao_spk(ubi_bao_header_t* bao, STREAMFILE* sf) {
    STREAMFILE* temp_sf = NULL;
    uint32_t memory_offset = bao->memory_skip;
    uint32_t memory_size = bao->is_prefetch ? bao->prefetch_size : bao->stream_size;
    uint32_t internal_id = bao->stream_id;

    uint32_t resource_offset = 0;
    uint32_t resource_size = 0;

    // find in current .spk
    if (find_spk_bao(internal_id, sf, &resource_offset, &resource_size)) {
        temp_sf = open_wrap_streamfile(sf); // wrap current SF (memory BAO) to avoid it being closed
        if (!temp_sf) goto fail;
    }
    else {
        // find in external .spk; ex. Avatar PC/X360 000970b1.spk: id 00462159 > 80462159.spk (vs stream 00462159.sbao in PS3)
        // 8xxxxxxx.spk may have header BAOs, that can point to others 8xxxxxxx.spk too.
        // TODO: Avatar has some .spk with IDs that don't seem to exist internal or externally
        char resource_name[32];
        uint32_t external_id = internal_id | 0x80000000; // implicit
        snprintf(resource_name, sizeof(resource_name), "%08x.spk", external_id);

        temp_sf = open_streamfile_by_filename(sf, resource_name);
        if (!temp_sf) {
            vgm_logi("UBI BAO: memory .spk BAO %08x / %08x not found internally or externally\n", internal_id, external_id);
            goto fail;
        }

        ;VGM_LOG("UBI BAO: using external file '%s'\n", resource_name);
        if (!find_spk_bao(internal_id, temp_sf, &resource_offset, &resource_size)) {
            vgm_logi("UBI BAO: memory .spk BAO %08x / %08x not found internally or externally\n", internal_id, external_id);
            goto fail;
        }
    }

    // catch bad config
    if (bao->is_prefetch && bao->prefetch_size + bao->memory_skip != resource_size) {
        VGM_LOG("UBI BAO: unexpected prefetch size %x vs %x\n", bao->prefetch_size + bao->memory_skip, resource_size);
        goto fail;
    }

    memory_offset += resource_offset;

    temp_sf = open_clamp_streamfile_f(temp_sf, memory_offset, memory_size);
    if (!temp_sf) goto fail;

    return temp_sf;
fail:
    close_streamfile(temp_sf);
    return NULL;
}

static STREAMFILE* open_stream_bao_spk(ubi_bao_header_t* bao, STREAMFILE* sf) {
    // no differences, uses .sbao extension
    return open_stream_bao_atomic(bao, sf);
}


/* Create a usable streamfile by joining memory + streams and setting up offsets as needed.
 *
 * Audio comes in "memory" and "streaming" BAOs, and when "prefetched" flag is set we need to join
 *  memory and streamed parts as they're stored separately (data may be split at any point).
 * 
 * The physical location of those depends on the format. memory BAOs may be within current file (packages/spk)
 * or external file (atomic/spk). Stream BAOs are always separate file. Later BAOs allow "inline" memory data
 * within the header BAO itself. Prefetch + memory data shouldn't happen at once (in theory).
 */
static STREAMFILE* setup_bao_streamfile(ubi_bao_header_t* bao, STREAMFILE* sf) {
    STREAMFILE* sf_memory = NULL;
    STREAMFILE* sf_stream = NULL;

    // for pure memory/streams prefetch size is 0
    uint32_t real_stream_size = bao->stream_size - bao->prefetch_size;
    bool load_inline = bao->is_inline;
    bool load_memory = ((bao->is_prefetch) || (!bao->is_prefetch && !bao->is_stream)) && (!bao->is_inline);
    bool load_stream = (bao->is_stream && !bao->is_prefetch) || (bao->is_prefetch && real_stream_size != 0);

    // seen in atomic/spk BAOs v29+
    if (load_inline) {
        sf_memory = open_inline_bao(bao, sf); // within header BAO
        if (!sf_memory) goto fail;
    }

    if (bao->archive == ARCHIVE_ATOMIC) {
        if (load_memory) {
            sf_memory = open_memory_bao_atomic(bao, sf); // external file
            if (!sf_memory) goto fail;
        }

        if (load_stream) {
            sf_stream = open_stream_bao_atomic(bao, sf); // external file
            if (!sf_stream) goto fail;
        }
    }

    if (bao->archive == ARCHIVE_PK) {
        if (load_memory) {
            sf_memory = open_memory_bao_package(bao, sf); // internal data
            if (!sf_memory) goto fail;
        }

        if (load_stream) {
            sf_stream = open_stream_bao_package(bao, sf); // external file
            if (!sf_stream) goto fail;
        }
    }

    if (bao->archive == ARCHIVE_SPK) {
        if (load_memory) {
            sf_memory = open_memory_bao_spk(bao, sf); // internal data or external file
            if (!sf_memory) goto fail;
        }

        if (load_stream) {
            sf_stream = open_stream_bao_spk(bao, sf); // external file
            if (!sf_stream) goto fail;
        }
    }


    // memory only, or prefetch with no stream (odd but streamed flag and 0 streamed size files do exist)
    if (sf_memory && !sf_stream) {
        return sf_memory;
    }

    // stream only
    if (!sf_memory && sf_stream) {
        return sf_stream;
    }

    // prefetch and stream, join as one
    if (sf_memory && sf_stream) {
        STREAMFILE* temp_sf = NULL;
        STREAMFILE* sf_segments[2] = { sf_memory, sf_stream };

        temp_sf = open_multifile_streamfile(sf_segments, 2);
        if (!temp_sf) goto fail;

        return temp_sf;
    }

    VGM_LOG("UBI BAO: memory nor stream found: inline=%i, memory=%i, stream=%i\n", load_inline, load_memory, load_stream);
    goto fail; // shouldn't happen
fail:
    close_streamfile(sf_memory);
    close_streamfile(sf_stream);

    VGM_LOG("UBI BAO: failed streamfile setup\n");
    return NULL;
}
