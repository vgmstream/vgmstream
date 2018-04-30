#include "meta.h"
#include "../coding/coding.h"


typedef enum { NONE = 0, UBI_ADPCM, RAW_PCM, RAW_PSX, RAW_XMA1, RAW_XMA2, RAW_AT3, FMT_AT3, RAW_DSP, FMT_OGG } ubi_bao_codec;
typedef struct {
    ubi_bao_codec codec;
    int big_endian;
    int total_subsongs;

    /* stream info */
    size_t header_size;
    size_t stream_size;
    off_t stream_offset;
    uint32_t stream_id;
    off_t extradata_offset;
    int is_external;

    int header_codec;
    int num_samples;
    int sample_rate;
    int channels;

    char resource_name[255];
    int types_count[9];
} ubi_bao_header;

static int parse_bao(ubi_bao_header * bao, STREAMFILE *streamFile, off_t offset);
static int parse_pk_header(ubi_bao_header * bao, STREAMFILE *streamFile);
static VGMSTREAM * init_vgmstream_ubi_bao_main(ubi_bao_header * bao, STREAMFILE *streamFile);


/* .PK - packages with BAOs from Ubisoft's sound engine ("DARE") games in 2008+ */
VGMSTREAM * init_vgmstream_ubi_bao_pk(STREAMFILE *streamFile) {
    ubi_bao_header bao = {0};

    /* checks */
    if (!check_extensions(streamFile, "pk,lpk"))
        goto fail;

    /* .pk+spk (or .lpk+lspk) is a database-like format, evolved from Ubi sb0/sm0+sp0. 
     * .pk has "BAO" headers pointing to internal or external .spk resources (also BAOs). */

    /* main parse */
    if ( !parse_pk_header(&bao, streamFile) )
        goto fail;

    return init_vgmstream_ubi_bao_main(&bao, streamFile);
fail:
    return NULL;
}

#if 0
/* .BAO - files with a single BAO from Ubisoft's sound engine ("DARE") games in 2008+ */
VGMSTREAM * init_vgmstream_ubi_bao_file(STREAMFILE *streamFile) {
    ubi_bao_header bao = {0};

    /* checks */
    if (!check_extensions(streamFile, "bao"))
        goto fail;

    /* single .bao+sbao found in .forge and similar bigfiles (containing compressed
     * "BAO_0xNNNNNNNN" headers/links, or "Common/English/(etc)_BAO_0xNNNNNNNN" streams).
     * The bigfile acts as index, but external files can be opened as are named after their id.
     * Extension isn't always given but is .bao in some games. */

    /* main parse */
    if ( !parse_bao_header(&bao, streamFile) )
        goto fail;

    return init_vgmstream_ubi_bao_main(&bao, streamFile);
fail:
    return NULL;
}
#endif


static VGMSTREAM * init_vgmstream_ubi_bao_main(ubi_bao_header * bao, STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *streamData = NULL;
    off_t start_offset;
    int loop_flag = 0;


    /* open external stream if needed */
    if (bao->is_external) {
        streamData = open_streamfile_by_filename(streamFile,bao->resource_name);
        if (!streamData) {
            VGM_LOG("UBI BAO: external stream '%s' not found\n", bao->resource_name);
            goto fail;
        }
    }
    else {
        streamData = streamFile;
    }

    start_offset = bao->stream_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(bao->channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = bao->num_samples;
    vgmstream->sample_rate = bao->sample_rate;
    vgmstream->num_streams = bao->total_subsongs;
    vgmstream->stream_size = bao->stream_size;
    vgmstream->meta_type = meta_UBI_BAO;

    switch(bao->codec) {
#if 0
        case UBI_ADPCM: {
            vgmstream->coding_type = coding_UBI_IMA;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

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
            dsp_read_coefs_be(vgmstream,streamFile,bao->extradata_offset+0x10, 0x40);
            break;

#ifdef VGM_USE_FFMPEG
        case RAW_XMA1:
        case RAW_XMA2: {
            uint8_t buf[0x100];
            size_t bytes, chunk_size;

            chunk_size = (bao->codec == RAW_XMA1) ? 0x20 : 0x34;

            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf,0x100, bao->extradata_offset,chunk_size, bao->stream_size, streamFile, 1);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,bao->stream_size);
            if ( !vgmstream->codec_data ) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case RAW_AT3: {
            uint8_t buf[0x100];
            int32_t bytes, block_size, encoder_delay, joint_stereo;

            block_size = 0xc0 * vgmstream->channels;
            joint_stereo = 0;
            encoder_delay = 0x00;//todo not correct

            bytes = ffmpeg_make_riff_atrac3(buf,0x100, vgmstream->num_samples, bao->stream_size, vgmstream->channels, vgmstream->sample_rate, block_size, joint_stereo, encoder_delay);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamFile, buf,bytes, start_offset,bao->stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }

        case FMT_AT3: {
            ffmpeg_codec_data *ffmpeg_data;

            ffmpeg_data = init_ffmpeg_offset(streamData, start_offset, bao->stream_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* manually read skip_samples if FFmpeg didn't do it */
            if (ffmpeg_data->skipSamples <= 0) {
                off_t chunk_offset;
                size_t chunk_size, fact_skip_samples = 0;
                if (!find_chunk_le(streamData, 0x66616374,start_offset+0xc,0, &chunk_offset,&chunk_size)) /* find "fact" */
                    goto fail;
                if (chunk_size == 0x8) {
                    fact_skip_samples  = read_32bitLE(chunk_offset+0x4, streamData);
                } else if (chunk_size == 0xc) {
                    fact_skip_samples  = read_32bitLE(chunk_offset+0x8, streamData);
                }
                ffmpeg_set_skip_samples(ffmpeg_data, fact_skip_samples);
            }
            break;
        }

        case FMT_OGG: {
            ffmpeg_codec_data *ffmpeg_data;

            ffmpeg_data = init_ffmpeg_offset(streamData, start_offset, bao->stream_size);
            if ( !ffmpeg_data ) goto fail;
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
    if ( !vgmstream_open_stream(vgmstream, streamData, start_offset) )
        goto fail;

    if (bao->is_external && streamData) close_streamfile(streamData);
    return vgmstream;

fail:
    if (bao->is_external && streamData) close_streamfile(streamData);
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


    /* class: 0x01=index, 0x02=BAO */
    if (read_8bit(0x00, streamFile) != 0x01)
        goto fail;
    /* index and resources always LE */

    /* 0x01(3): version, major/minor/release (numbering continues from .sb0/sm0) */
    index_size = read_32bitLE(0x04, streamFile); /* can be 0 */
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

    /* parse index to get target subsong N = Nth header BAO */
    bao_offset = index_header_size + index_size;
    for (i = 0; i < index_entries; i++) {
        //uint32_t bao_id = read_32bitLE(index_header_size+0x08*i+0x00, streamFile);
        size_t bao_size = read_32bitLE(index_header_size+0x08*i+0x04, streamFile);

        /* parse and continue to find out total_subsongs */
        if (!parse_bao(bao, streamFile, bao_offset))
            goto fail;

        bao_offset += bao_size; /* files simply concat BAOs */
    }

    ;VGM_LOG("BAO types: 10=%i,20=%i,30=%i,40=%i,50=%i,70=%i,80=%i\n",
            bao->types_count[1],bao->types_count[2],bao->types_count[3],bao->types_count[4],bao->types_count[5],bao->types_count[7],bao->types_count[8]);

    if (bao->total_subsongs == 0) {
        VGM_LOG("UBI BAO: no streams\n");
        goto fail; /* not uncommon */
    }
    if (target_subsong < 0 || target_subsong > bao->total_subsongs || bao->total_subsongs < 1) goto fail;


    /* get stream pointed by header */
    if (bao->is_external) {
        /* parse resource table, LE (may be empty, or exist even with nothing in the file) */
        off_t offset;
        int resources_count = read_32bitLE(resources_offset+0x00, streamFile);
        size_t strings_size = read_32bitLE(resources_offset+0x04, streamFile);

        offset = resources_offset + 0x04+0x04 + strings_size;
        for (i = 0; i < resources_count; i++) {
            uint32_t resource_id  = read_32bitLE(offset+0x10*i+0x00, streamFile);
            off_t name_offset     = read_32bitLE(offset+0x10*i+0x04, streamFile);
            off_t resource_offset = read_32bitLE(offset+0x10*i+0x08, streamFile);
            size_t resource_size  = read_32bitLE(offset+0x10*i+0x0c, streamFile);

            if (resource_id == bao->stream_id) {
                bao->stream_offset = resource_offset + bao->header_size;
                read_string(bao->resource_name,255, resources_offset + 0x04+0x04 + name_offset, streamFile);

                VGM_ASSERT(bao->stream_size != resource_size - bao->header_size, "UBI BAO: stream vs resource size mismatch\n");
                break;
            }
        }


        //todo find flag and fix
        /* some songs divide data in internal+external resource and data may be split arbitrarily,
         * must join on reads (needs multifile_streamfile); resources may use block layout in XMA too */
        bao_offset = index_header_size + index_size;
        for (i = 0; i < index_entries; i++) {
            uint32_t bao_id = read_32bitLE(index_header_size+0x08*i+0x00, streamFile);
            size_t bao_size = read_32bitLE(index_header_size+0x08*i+0x04, streamFile);

            if (bao_id == bao->stream_id) {
                VGM_LOG("UBI BAO: found internal+external at offset=%lx\n",bao_offset);
                goto fail;
            }

            bao_offset += bao_size;
        }
    }
    else {
        /* find within index */

        bao_offset = index_header_size + index_size;
        for (i = 0; i < index_entries; i++) {
            uint32_t bao_id = read_32bitLE(index_header_size+0x08*i+0x00, streamFile);
            size_t bao_size = read_32bitLE(index_header_size+0x08*i+0x04, streamFile);

            if (bao_id == bao->stream_id) {
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

    ;VGM_LOG("BAO stream: id=%x, offset=%lx, size=%x, res=%s\n", bao->stream_id, bao->stream_offset, bao->stream_size, (bao->is_external ? bao->resource_name : "internal"));

    return 1;
fail:
    return 0;
}

/* parse a single BAO (binary audio object) descriptor */
static int parse_bao(ubi_bao_header * bao, STREAMFILE *streamFile, off_t offset) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    uint32_t bao_version, descriptor_type;
    size_t header_size;
    int target_subsong = streamFile->stream_index;
    

    /* 0x00(1): class? usually 0x02 but older BAOs have 0x01 too */
    bao_version = read_32bitBE(offset + 0x00, streamFile) & 0x00FFFFFF;

    /* detect endianness */
    if (read_32bitLE(offset+0x04, streamFile) < 0x0000FFFF) {
        read_32bit = read_32bitLE;
    } else {
        read_32bit = read_32bitBE;
        bao->big_endian = 1;
    }

    header_size = read_32bit(offset+0x04, streamFile); /* mainly 0x28, rarely 0x24 */
    /* 0x08(10): descriptor GUID? */
    /* 0x18: null */
    /* 0x1c: null */
    descriptor_type = read_32bit(offset+0x20, streamFile);
    /* 0x28: subtype? usually 0x02/0x01, games may crash if changed */
    
    /* for debugging purposes */
    switch(descriptor_type) {
        case 0x10000000: bao->types_count[1]++; break; /* link by id to another descriptor (link or header) */
        case 0x20000000: bao->types_count[2]++; break; /* stream header (and subtypes) */
        case 0x30000000: bao->types_count[3]++; break; /* internal stream (in .pk) */
        case 0x40000000: bao->types_count[4]++; break; /* package info? */
        case 0x50000000: bao->types_count[5]++; break; /* external stream (in .spk) */
        case 0x70000000: bao->types_count[7]++; break; /* project info? (sometimes special id 0x7fffffff)*/
        case 0x80000000: bao->types_count[8]++; break; /* unknown (some id/info?) */
        default:
            VGM_LOG("UBI BAO: unknown descriptor type at %lx\n", offset);
            goto fail;
    }

    /* only parse headers */
    if (descriptor_type != 0x20000000)
        return 1;
    /* ignore other header subtypes, 0x01=sound header, 0x04=info? (like Ubi .sb0) */
    if (read_32bit(offset+header_size+0x04, streamFile) != 0x01)
        return 1;

    bao->total_subsongs++;
    if (target_subsong == 0) target_subsong = 1;

    if (target_subsong != bao->total_subsongs)
        return 1;

    /* parse BAO per version. Structure is mostly the same with some extra fields.
     * - descriptor id (ignored by game)
     * - type (may crash on game startup if changed)
     * - stream size
     * - stream id, corresponding to an internal (0x30) or external (0x50) stream
     * - various flags/config fields
     * - channels, ?, sample rate, average bit rate?, samples, full stream_size?, codec, etc
     * - subtable entries, subtable size (may contain offsets/ids, cues, etc)
     * - extra data per codec (ex. XMA header in some versions) */
    //todo skip tables when getting extradata
    ;VGM_LOG("BAO header at %lx\n", offset);

    switch(bao_version) {

        case 0x001F0011: /* Naruto: The Broken Bond (X360)-pk */
        case 0x0022000D: /* Just Dance (Wii)-pk */
            bao->stream_size  = read_32bit(offset+header_size+0x08, streamFile);
            bao->stream_id    = read_32bit(offset+header_size+0x1c, streamFile);
            bao->is_external  = read_32bit(offset+header_size+0x28, streamFile); /* maybe 0x30 */
            bao->channels     = read_32bit(offset+header_size+0x44, streamFile);
            bao->sample_rate  = read_32bit(offset+header_size+0x4c, streamFile);
            bao->num_samples  = read_32bit(offset+header_size+0x54, streamFile);
            bao->header_codec = read_32bit(offset+header_size+0x64, streamFile);

            switch(bao->header_codec) {
                case 0x01: bao->codec = RAW_PCM; break;
                case 0x05: bao->codec = RAW_XMA1; break;
                case 0x09: bao->codec = RAW_DSP; break;
                default: VGM_LOG("UBI BAO: unknown codec at %lx\n", offset); goto fail;
            }

            //todo use flags?
            if (bao->header_codec == 0x09) {
                bao->extradata_offset = offset+header_size+0x80; /* mini DSP header */
            }
            if (bao->header_codec == 0x05 && !bao->is_external) {
                bao->extradata_offset = offset+header_size + 0x7c; /* XMA header */
            }
            //todo external XMA may use blocked layout + layered layout

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
                default: VGM_LOG("UBI BAO: unknown codec at %lx\n", offset); goto fail;
            }

            if (read_32bit(offset+header_size+0x20, streamFile) & 0x10) {
                VGM_LOG("UBI BAO: possible full loop at %lx\n", offset);
                /* RIFFs may have "smpl" and this flag, even when data shouldn't loop... */
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
                case 0x04: bao->codec = RAW_XMA2; break;
                case 0x05: bao->codec = RAW_PSX; break;
                case 0x06: bao->codec = RAW_AT3; break;
                default: VGM_LOG("UBI BAO: unknown codec at %lx\n", offset); goto fail;
            }

            if (bao->header_codec == 0x04 && !bao->is_external) {
                bao->extradata_offset = offset+header_size + 0x8c; /* XMA header */
            }

            break;

        case 0x001B0100: /* Assassin's Creed (PS3/X360/PC)-file */
        case 0x001B0200: /* Beowulf (PS3)-file */
        case 0x001F0010: /* Prince of Persia 2008 (PS3/X360)-file, Far Cry 2 (PS3)-file */
        case 0x00280306: /* Far Cry 3: Blood Dragon (X360)-file */
        case 0x00290106: /* Splinter Cell Blacklist? */
        default:
            VGM_LOG("UBI BAO: unknown BAO version at %lx\n", offset);
            goto fail;
    }

    bao->header_size = header_size;

    return 1;
fail:
    return 0;
}
