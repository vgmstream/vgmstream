#include "meta.h"
#include "../coding/coding.h"


typedef enum { NONE = 0, UBI_ADPCM, RAW_PCM, RAW_PSX, RAW_XMA1, RAW_XMA2, RAW_AT3, FMT_AT3, RAW_DSP, FMT_OGG } ubi_bao_codec;
typedef struct {
    int version;
    ubi_bao_codec codec;
    int big_endian;
    int total_subsongs;

    /* stream info */
    size_t header_size;
    size_t stream_size;
    off_t stream_offset;
    size_t prefetch_size;
    off_t prefetch_offset;
    size_t main_size;
    off_t main_offset;
    uint32_t stream_id;
    off_t extradata_offset;
    int is_external;
    int is_prefetched;

    int header_codec;
    int num_samples;
    int sample_rate;
    int channels;

    char resource_name[255];
    int types_count[9];
    int subtypes_count[9];
} ubi_bao_header;

static int parse_bao(ubi_bao_header * bao, STREAMFILE *streamFile, off_t offset);
static int parse_pk_header(ubi_bao_header * bao, STREAMFILE *streamFile);
static VGMSTREAM * init_vgmstream_ubi_bao_main(ubi_bao_header * bao, STREAMFILE *streamFile);
static STREAMFILE * setup_bao_streamfile(ubi_bao_header *bao, STREAMFILE *streamFile);


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


static VGMSTREAM * init_vgmstream_ubi_bao_main(ubi_bao_header * bao, STREAMFILE * streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamData = NULL;
    off_t start_offset = 0x00;
    int loop_flag = 0;

    streamData = setup_bao_streamfile(bao, streamFile);
    if (!streamData) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(bao->channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = bao->num_samples;
    vgmstream->sample_rate = bao->sample_rate;
    vgmstream->num_streams = bao->total_subsongs;
    vgmstream->stream_size = bao->stream_size;
    vgmstream->meta_type = meta_UBI_BAO;

    switch (bao->codec) {
        case UBI_ADPCM: {
            vgmstream->coding_type = coding_UBI_IMA;
            vgmstream->layout_type = layout_none;
            break;
        }

        case RAW_PCM:
            vgmstream->coding_type = coding_PCM16LE; /* always LE even on Wii */
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
            dsp_read_coefs_be(vgmstream, streamFile, bao->extradata_offset + 0x10, 0x40);
            break;

#ifdef VGM_USE_FFMPEG
        case RAW_XMA1:
        case RAW_XMA2: {
            uint8_t buf[0x100];
            uint32_t num_frames;
            size_t bytes, chunk_size, frame_size, data_size;
            int is_xma2_old;
            STREAMFILE *header_data;
            off_t header_offset;

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
                header_offset = 0x00;
            }
            else {
                header_data = streamFile;
                header_offset = bao->extradata_offset;
            }

            if (is_xma2_old) {
                bytes = ffmpeg_make_riff_xma2_from_xma2_chunk(buf, 0x100, header_offset, chunk_size, data_size, header_data);
            }
            else {
                bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf, 0x100, header_offset, chunk_size, data_size, header_data, 1);
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

    /* open the file for reading (can be an external stream, different from the current .pk) */
    if (!vgmstream_open_stream(vgmstream, streamData, start_offset))
        goto fail;

    close_streamfile(streamData);
    return vgmstream;

fail:
    close_streamfile(streamData);
    close_vgmstream(vgmstream);
    return NULL;
}

/* parse a .pk (package) file: index + BAOs + external .spk resource table. We want header
 * BAOs pointing to internal/external stream BAOs (.spk is the same, with stream BAOs only). */
static int parse_pk_header(ubi_bao_header * bao, STREAMFILE *streamFile) {
    int i;
    int index_entries;
    size_t index_size, index_header_size;
    off_t bao_offset, resources_offset;
    int target_subsong = streamFile->stream_index;
    uint8_t *index_buffer = NULL;
    STREAMFILE *streamTest = NULL;


    /* class: 0x01=index, 0x02=BAO */
    if (read_8bit(0x00, streamFile) != 0x01)
        goto fail;
    /* index and resources always LE */

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

    index_entries = index_size / 0x08;
    index_header_size = 0x40;

    /* pre-load to avoid too much I/O back and forth */
    if (index_size > (10000*0x08)) {
        VGM_LOG("BAO: index too big\n");
        goto fail;
    }
    index_buffer = malloc(index_size);
    read_streamfile(index_buffer, index_header_size, index_size, streamFile);

    /* use smaller I/O buffer for performance, as this read lots of small BAO headers all over the place */
    streamTest = reopen_streamfile(streamFile, 0x100);
    if (!streamTest) goto fail;

    /* parse index to get target subsong N = Nth audio header BAO */
    bao_offset = index_header_size + index_size;
    for (i = 0; i < index_entries; i++) {
        //uint32_t bao_id = get_32bitLE(index_buffer + 0x08*i+ 0x00);
        size_t bao_size = get_32bitLE(index_buffer + 0x08*i + 0x04);

        /* parse and continue to find out total_subsongs */
        if (!parse_bao(bao, streamTest, bao_offset))
            goto fail;

        bao_offset += bao_size; /* files simply concat BAOs */
    }

    ;VGM_LOG("BAO types: 10=%i,20=%i,30=%i,40=%i,50=%i,70=%i,80=%i\n",
            bao->types_count[1],bao->types_count[2],bao->types_count[3],bao->types_count[4],bao->types_count[5],bao->types_count[7],bao->types_count[8]);
    ;VGM_LOG("BAO 0x20 subtypes: 01=%i,02=%i,03=%i,04=%i,05=%i,06=%i,07=%i,08=%i\n",
            bao->types_count[1],bao->subtypes_count[2],bao->subtypes_count[3],bao->subtypes_count[4],bao->subtypes_count[5],bao->subtypes_count[6],bao->subtypes_count[7],bao->subtypes_count[8]);

    if (bao->total_subsongs == 0) {
        VGM_LOG("UBI BAO: no streams\n");
        goto fail; /* not uncommon */
    }
    if (target_subsong < 0 || target_subsong > bao->total_subsongs || bao->total_subsongs < 1) goto fail;


    /* get stream pointed by header */
    if (bao->is_external) {
        off_t offset;
        int resources_count;
        size_t strings_size;

        /* some sounds have a prefetched bit stored internally with the remaining streamed part stored externally */
        bao_offset = index_header_size + index_size;
        for (i = 0; i < index_entries; i++) {
            uint32_t bao_id = read_32bitLE(index_header_size + 0x08 * i + 0x00, streamFile);
            size_t bao_size = read_32bitLE(index_header_size + 0x08 * i + 0x04, streamFile);

            if (bao_id == bao->stream_id) {
                bao->prefetch_offset = bao_offset + bao->header_size;
                break;
            }

            bao_offset += bao_size;
        }

        if (bao->prefetch_size) {
            if (bao->prefetch_offset == 0) goto fail;
            bao->is_prefetched = 1;
        }
        else {
            if (bao->prefetch_offset != 0) goto fail;
        }

        /* parse resource table, LE (may be empty, or exist even with nothing in the file) */
        resources_count = read_32bitLE(resources_offset+0x00, streamFile);
        strings_size = read_32bitLE(resources_offset+0x04, streamFile);

        offset = resources_offset + 0x04+0x04 + strings_size;
        for (i = 0; i < resources_count; i++) {
            uint32_t resource_id  = read_32bitLE(offset+0x10*i+0x00, streamFile);
            off_t name_offset     = read_32bitLE(offset+0x10*i+0x04, streamFile);
            off_t resource_offset = read_32bitLE(offset+0x10*i+0x08, streamFile);
            size_t resource_size  = read_32bitLE(offset+0x10*i+0x0c, streamFile);

            if (resource_id == bao->stream_id) {
                bao->stream_offset = resource_offset + bao->header_size;
                read_string(bao->resource_name,255, resources_offset + 0x04+0x04 + name_offset, streamFile);

                if (bao->is_prefetched) {
                    bao->main_offset = resource_offset + bao->header_size;
                    bao->main_size = resource_size - bao->header_size;
                    VGM_ASSERT(bao->stream_size != bao->main_size + bao->prefetch_size, "UBI BAO: stream vs resource size mismatch\n");
                }
                else {
                    VGM_ASSERT(bao->stream_size != resource_size - bao->header_size, "UBI BAO: stream vs resource size mismatch\n");
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
                bao->stream_size = bao_size - bao->header_size;
                bao->stream_offset = bao_offset + bao->header_size; /* relative, adjust to skip descriptor */
                break;
            }

            bao_offset += bao_size;
        }
    }

    if (!bao->stream_offset) {
        VGM_LOG("UBI BAO: stream not found (id=%08x, external=%i)\n", bao->stream_id, bao->is_external);
        goto fail;
    }

    ;VGM_LOG("BAO stream: id=%x, offset=%x, size=%x, res=%s\n", bao->stream_id, (uint32_t)bao->stream_offset, bao->stream_size, (bao->is_external ? bao->resource_name : "internal"));

    free(index_buffer);
    close_streamfile(streamTest);
    return 1;
fail:
    free(index_buffer);
    close_streamfile(streamTest);
    return 0;
}

/* parse a single BAO (binary audio object) descriptor */
static int parse_bao(ubi_bao_header * bao, STREAMFILE *streamFile, off_t offset) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    uint32_t bao_version, descriptor_type, descriptor_subtype;
    size_t header_size;
    int target_subsong = streamFile->stream_index;
    

    /* 0x00(1): class? usually 0x02 but older BAOs have 0x01 too */
    bao_version = read_32bitBE(offset+0x00, streamFile) & 0x00FFFFFF;

    /* this could be done once as all BAOs share endianness */
    if (guess_endianness32bit(offset+0x04, streamFile)) {
        read_32bit = read_32bitBE;
        bao->big_endian = 1;
    } else {
        read_32bit = read_32bitLE;
    }

    header_size = read_32bit(offset+0x04, streamFile); /* mainly 0x28, rarely 0x24 */
    /* 0x08(10): descriptor GUID? */
    /* 0x18: null */
    /* 0x1c: null */
    descriptor_type = read_32bit(offset+0x20, streamFile);
    descriptor_subtype = read_32bit(offset+header_size+0x04, streamFile);
    
    /* for debugging purposes */
    switch(descriptor_type) {
        case 0x10000000: bao->types_count[1]++; break; /* link by id to another descriptor (link or header) */
        case 0x20000000: bao->types_count[2]++; break; /* stream header (and subtypes) */
        case 0x30000000: bao->types_count[3]++; break; /* internal stream (in .pk) */
        case 0x40000000: bao->types_count[4]++; break; /* package info? */
        case 0x50000000: bao->types_count[5]++; break; /* external stream (in .spk) */
        case 0x70000000: bao->types_count[7]++; break; /* project info? (sometimes special id 0x7fffffff) */
        case 0x80000000: bao->types_count[8]++; break; /* unknown (some id/info?) */
        default:
            VGM_LOG("UBI BAO: unknown type %x at %x + %x\n", descriptor_type, (uint32_t)offset, 0x20);
            goto fail;
    }

    /* only parse headers */
    if (descriptor_type != 0x20000000)
        return 1;

    /* for debugging purposes */
    switch(descriptor_subtype) {
        case 0x00000001: bao->subtypes_count[1]++; break; /* standard */
        case 0x00000002: bao->subtypes_count[2]++; break; /* multilayer??? related to other header BAOs? */
        case 0x00000003: bao->subtypes_count[3]++; break; /* related to other header BAOs? */
        case 0x00000004: bao->subtypes_count[4]++; break; /* related to other header BAOs? */
        case 0x00000005: bao->subtypes_count[5]++; break; /* related to other header BAOs? */
        case 0x00000006: bao->subtypes_count[6]++; break; /* some multilayer/table? may contain sounds??? */
        case 0x00000007: bao->subtypes_count[7]++; break; /* related to other header BAOs? */
        case 0x00000008: bao->subtypes_count[8]++; break; /* ? (almost empty with some unknown value) */
        default:
            VGM_LOG("UBI BAO: unknown subtype %x at %x + %x\n", descriptor_subtype, (uint32_t)offset, header_size+0x04);
            goto fail;
    }
    //;VGM_ASSERT(descriptor_subtype != 0x01, "UBI BAO: subtype %x at %lx (%lx)\n", descriptor_subtype, offset, offset+header_size+0x04);

    /* ignore unknown subtypes */
    if (descriptor_subtype != 0x00000001)
        return 1;

    bao->total_subsongs++;
    if (target_subsong == 0) target_subsong = 1;

    if (target_subsong != bao->total_subsongs)
        return 1;

    /* parse BAO per version. Structure is mostly the same with some extra fields.
     * - descriptor id (ignored by game)
     * - subtype (may crash on game startup if changed)
     * - stream size
     * - stream id, corresponding to an internal (0x30) or external (0x50) stream
     * - various flags/config fields
     * - channels, ?, sample rate, average bit rate?, samples, full stream_size?, codec, etc
     * - subtable entries, subtable size (may contain offsets/ids, cues, etc)
     * - extra data per codec (ex. XMA header in some versions) */
    ;VGM_LOG("BAO header at %x\n", (uint32_t)offset);
    bao->version = bao_version;

    switch(bao->version) {
        case 0x001F0008: /* Rayman Raving Rabbids: TV Party (Wii)-pk */
        case 0x001F0011: /* Naruto: The Broken Bond (X360)-pk */
        case 0x0022000D: /* Just Dance (Wii)-pk */
            bao->stream_size  = read_32bit(offset+header_size+0x08, streamFile);
            bao->stream_id    = read_32bit(offset+header_size+0x1c, streamFile);
            bao->is_external  = read_32bit(offset+header_size+0x28, streamFile); /* maybe 0x30 */
            bao->channels     = read_32bit(offset+header_size+0x44, streamFile);
            bao->sample_rate  = read_32bit(offset+header_size+0x4c, streamFile);
            if (read_32bit(offset + header_size + 0x34, streamFile) & 0x01) { /* single flag? */
                bao->num_samples = read_32bit(offset + header_size + 0x5c, streamFile);
            }
            else {
                bao->num_samples = read_32bit(offset + header_size + 0x54, streamFile);
            }
            bao->header_codec = read_32bit(offset+header_size+0x64, streamFile);

            switch(bao->header_codec) {
                case 0x01: bao->codec = RAW_PCM; break;
                //case 0x02: bao->codec = FMT_OGG; break;
                case 0x03: bao->codec = UBI_ADPCM; break;
                case 0x05: bao->codec = RAW_XMA1; break;
                case 0x09: bao->codec = RAW_DSP; break;
                default: VGM_LOG("UBI BAO: unknown codec at %x\n", (uint32_t)offset); goto fail;
            }

            bao->prefetch_size = read_32bit(offset + header_size + 0x74, streamFile);

            if (bao->codec == RAW_DSP) {
                bao->extradata_offset = offset+header_size+0x80; /* mini DSP header */
            }
            if (bao->codec == RAW_XMA1 && !bao->is_external) {
                bao->extradata_offset = offset+header_size + 0x7c; /* XMA header */
            }

            break;

        case 0x00220015: /* James Cameron's Avatar: The Game (PSP)-pk */
        case 0x0022001E: /* Prince of Persia: The Forgotten Sands (PSP)-pk */
            bao->stream_size  = read_32bit(offset+header_size+0x08, streamFile);
            bao->stream_id    = read_32bit(offset+header_size+0x1c, streamFile);
            bao->is_external  = read_32bit(offset+header_size+0x20, streamFile) & 0x04;
            bao->channels     = read_32bit(offset+header_size+0x28, streamFile);
            bao->sample_rate  = read_32bit(offset+header_size+0x30, streamFile);
            if (read_32bit(offset+header_size+0x20, streamFile) & 0x20) {
                bao->num_samples  = read_32bit(offset+header_size+0x40, streamFile);
            }
            else {
                bao->num_samples  = read_32bit(offset+header_size+0x38, streamFile); /* from "fact" if AT3 */
            }
            bao->header_codec = read_32bit(offset+header_size+0x48, streamFile);

            switch(bao->header_codec) {
                case 0x06: bao->codec = RAW_PSX; break;
                case 0x07: bao->codec = FMT_AT3; break;
                default: VGM_LOG("UBI BAO: unknown codec at %x\n", (uint32_t)offset); goto fail;
            }

            if (read_32bit(offset+header_size+0x20, streamFile) & 0x10) {
                VGM_LOG("UBI BAO: possible full loop at %x\n", (uint32_t)offset);
                /* RIFFs may have "smpl" and this flag, even when data shouldn't loop... */
            }

            break;

        case 0x00230008: /* Splinter Cell: Conviction (X360/PC)-pk */
            bao->stream_size  = read_32bit(offset+header_size+0x08, streamFile);
            bao->stream_id    = read_32bit(offset+header_size+0x24, streamFile);
            bao->is_external  = read_32bit(offset+header_size+0x38, streamFile);
            bao->channels     = read_32bit(offset+header_size+0x54, streamFile);
            bao->sample_rate  = read_32bit(offset+header_size+0x5c, streamFile);
            if (read_32bit(offset+header_size+0x44, streamFile) & 0x01) { /* single flag? */
                bao->num_samples  = read_32bit(offset+header_size+0x6c, streamFile);
            }
            else {
                bao->num_samples  = read_32bit(offset+header_size+0x64, streamFile);
            }
            bao->header_codec = read_32bit(offset+header_size+0x74, streamFile);

            switch (bao->header_codec) {
                case 0x01: bao->codec = RAW_PCM; break;
                case 0x02: bao->codec = UBI_ADPCM; break;
                case 0x03: bao->codec = FMT_OGG; break;
                case 0x04: bao->codec = RAW_XMA2; break;
                default: VGM_LOG("UBI BAO: unknown codec at %x\n", (uint32_t)offset); goto fail;
            }

            bao->prefetch_size = read_32bit(offset+header_size+0x84, streamFile);

            if (bao->codec == RAW_XMA2 && !bao->is_external) {
                bao->extradata_offset = offset + header_size + 0x8c; /* XMA header */
            }

            break;

        case 0x00250108: /* Scott Pilgrim vs the World (PS3/X360)-pk */
        case 0x0025010A: /* Prince of Persia: The Forgotten Sands (PS3/X360)-file */
            bao->stream_size  = read_32bit(offset+header_size+0x08, streamFile);
            bao->stream_id    = read_32bit(offset+header_size+0x24, streamFile);
            bao->is_external  = read_32bit(offset+header_size+0x30, streamFile);
            bao->channels     = read_32bit(offset+header_size+0x48, streamFile);
            bao->sample_rate  = read_32bit(offset+header_size+0x50, streamFile);
            if (read_32bit(offset+header_size+0x38, streamFile) & 0x01) { /* single flag? */
                bao->num_samples  = read_32bit(offset+header_size+0x60, streamFile);
            }
            else {
                bao->num_samples  = read_32bit(offset+header_size+0x58, streamFile);
            }
            bao->header_codec = read_32bit(offset+header_size+0x68, streamFile);
            /* when is internal+external (flag 0x2c?), 0xa0: internal data size */

            switch(bao->header_codec) {
                case 0x01: bao->codec = RAW_PCM; break;
                case 0x02: bao->codec = UBI_ADPCM; break; /* assumed */
                case 0x03: bao->codec = FMT_OGG; break; /* assumed */
                case 0x04: bao->codec = RAW_XMA2; break;
                case 0x05: bao->codec = RAW_PSX; break;
                case 0x06: bao->codec = RAW_AT3; break;
                default: VGM_LOG("UBI BAO: unknown codec at %x\n", (uint32_t)offset); goto fail;
            }

            bao->prefetch_size = read_32bit(offset + header_size + 0x78, streamFile);

            if (bao->codec == RAW_XMA2 && !bao->is_external) {
                bao->extradata_offset = offset+header_size + 0x8c; /* XMA header */
            }

            break;

        case 0x001B0100: /* Assassin's Creed (PS3/X360/PC)-file */
        case 0x001B0200: /* Beowulf (PS3)-file */
        case 0x001F0010: /* Prince of Persia 2008 (PS3/X360)-file, Far Cry 2 (PS3)-file */
        case 0x00280306: /* Far Cry 3: Blood Dragon (X360)-file */
        case 0x00290106: /* Splinter Cell Blacklist? */
        default: /* others possibly using BAO: Avatar X360/PS3/PC, Just Dance, Watch_Dogs, Far Cry Primal, Far Cry 4 */
            VGM_LOG("UBI BAO: unknown BAO version at %x\n", (uint32_t)offset);
            goto fail;
    }

    bao->header_size = header_size;

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
