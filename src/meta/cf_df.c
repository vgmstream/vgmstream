#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util/layout_utils.h"
#include <string.h>
#include <ctype.h>

/*
 * CF_DF - CyberFlix DreamFactory Engine (.SND, .SFX, .MOV)
 *
 * Structure:
 * - A main header (1024 bytes) containing file size and container count.
 * - A table of 32-bit little-endian offsets to each container.
 * - A series of data containers, each with its own header.
 *
 * Titles: Dust 1996 3.1/95, Titanic: Adventure Out of Time
 * Disney's Math/Reading Quest with Aladdin
 *
 * Audio Logic:
 * - Audio is stored in containers that are identified by a codec flag and sample rate.
 * - A separate metadata container lists explicitly named sound effects.
 * - The core logic partitions all audio into two groups:
 * 1. A segmented track, formed by concatenating chunks of a consistent size. (MOV only)
 * 2. Individually named or unnamed sound effects.
 */

#define DF_CHUNK_NAME_SIZE 15

typedef struct {
    int id;
    int is_named;
    int is_segmented_chunk;
    int codec_flag;
    off_t offset;
    int32_t size;
    int32_t sample_rate;
    int32_t uncompressed_size;
    char name[DF_CHUNK_NAME_SIZE + 1];
} df_chunk_t;

static int32_t find_segmented_chunk_size(df_chunk_t* chunks, int total_chunks) {
    if (total_chunks < 1) return -1;

    int32_t most_common_size = -1;
    int max_count = 0;

    for (int i = 0; i < total_chunks; i++) {
        if (chunks[i].is_named)
            continue;

        int current_count = 0;
        for (int j = 0; j < total_chunks; j++) {
            if (chunks[j].is_named)
                continue;
            if (chunks[i].uncompressed_size == chunks[j].uncompressed_size) {
                current_count++;
            }
        }

        if (current_count > max_count) {
            max_count = current_count;
            most_common_size = chunks[i].uncompressed_size;
        }
    }
    return most_common_size;
}

/* Configure a VGMSTREAM object based on a chunk's properties */
static void build_vgmstream_from_chunk(VGMSTREAM* vgmstream, df_chunk_t* chunk) {
    vgmstream->sample_rate = chunk->sample_rate;
    vgmstream->stream_size = chunk->size;
    vgmstream->meta_type = meta_CF_DF;

    if (chunk->codec_flag == 1) {
        vgmstream->coding_type = coding_CF_DF_ADPCM_V40;
        vgmstream->num_samples = chunk->uncompressed_size;
    } else {
        vgmstream->coding_type = coding_CF_DF_DPCM_V41;
        vgmstream->num_samples = chunk->uncompressed_size / 2;
    }
}

/* Main function to build the VGMSTREAM object */
VGMSTREAM* init_vgmstream_cf_df(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    df_chunk_t* chunks = NULL;
    int i, j;
    int total_chunks = 0;
    int target_subsong = sf->stream_index;
    int is_mov_file = 0;
    const int HEADER_SIZE = 0x400;
    const int TRACK_SUFFIX_SIZE = (5 + 1 + 1); // Subsongs up to 5 digits plus # and \0


    /* Header Check:
     *
     * LPPALPPA more easily checked at 0x20.
     * TO-DO: Possibly also start with 0x100, 0 at 0x08/0x0C? Needs confirmation.
     *
     */
    if (!is_id32be(0x20, sf,"LPPA") || !is_id32be(0x24, sf,"LPPA"))
        return NULL;
    if (read_u32le(0x04, sf) != get_streamfile_size(sf))
        return NULL;

    if (!check_extensions(sf, "snd,sfx,mov"))
        return NULL;

    int containers = read_u32le(0x14, sf);
    if (containers <= 0 || containers > INT16_MAX) //Practically 2 bytes should be enough.
        return NULL;

    int segmented_chunks;
    int32_t segmented_chunk_size;
    int subsongs, current_subsong_idx;

    /* Find and catalog all physical audio containers */
    chunks = calloc(containers, sizeof(df_chunk_t));
    if (!chunks) goto fail;

    for (i = 0; i < containers; i++) {
        off_t container_pos = read_u32le(HEADER_SIZE + i * 0x04, sf);
        if (container_pos <= 0 || container_pos >= get_streamfile_size(sf))
            continue;

        off_t header_pos = container_pos + 0x08;
        if (header_pos + 0x30 > get_streamfile_size(sf))
            continue;

        uint16_t codec_flag = read_u16le(header_pos + 0x1A, sf);
        uint32_t hertz = read_u32le(header_pos + 0x1C, sf);

        if ((codec_flag == 1 || codec_flag == 2) && (hertz == 11025 || hertz == 22050 || hertz == 44100)) {
            chunks[total_chunks].id = i;
            chunks[total_chunks].codec_flag = codec_flag;
            chunks[total_chunks].sample_rate = hertz;
            chunks[total_chunks].uncompressed_size = read_u32le(header_pos + 0x24, sf);
            chunks[total_chunks].offset = header_pos + read_u32le(header_pos + 0x2C, sf);
            chunks[total_chunks].size = read_u32le(container_pos + 0x04, sf) - read_u32le(header_pos + 0x2C, sf);
            total_chunks++;
        }
    }

    if (total_chunks == 0)
        goto fail;

    /* Parse metadata to find named files (.SND/.SFX),
        * TO-DO: Metadata structure in .MOV files is inconsistent. */
    is_mov_file = check_extensions(sf, "mov");

    if (!is_mov_file) {
        off_t pointer_offset = 0x20; /* Standard offset for SND/SFX */

        off_t container0_pos = read_u32le(HEADER_SIZE + 0 * 0x04, sf);
        int md_container_id = -1;
        if (container0_pos > 0) {
            /* The pointer is relative to the data payload, which starts after the 8-byte container header */
            md_container_id = read_u32le(container0_pos + 0x08 + pointer_offset, sf);
        }



        if (md_container_id >= 0 && md_container_id < containers) {
            off_t md_pos = read_u32le(HEADER_SIZE + md_container_id * 0x04, sf);
            if (md_pos > 0) {
                /* Establish a base for the actual data, skipping the 8-byte container header */
                const off_t md_payload_pos = md_pos + 0x08;
                int records = read_u16le(md_payload_pos + 0x04, sf);
                off_t current_record_offset = md_payload_pos + 0x08;
                for (i = 0; i < records; i++) {
                    int chunk_id = read_u32le(current_record_offset + 0x04, sf);
                    uint8_t name_len = read_u8(current_record_offset + 10, sf);
                    char name_buffer[DF_CHUNK_NAME_SIZE + 1] = {0};

                    /* Needs byte for byte reading for named SND/SFX. */
                    for (int k = 0; k < DF_CHUNK_NAME_SIZE; k++) {
                        name_buffer[k] = read_u8(current_record_offset + 11 + k, sf);
                    }

                    if (name_len < DF_CHUNK_NAME_SIZE)
                        name_buffer[name_len] = '\0';

                    for (j = 0; j < total_chunks; j++) {
                        if (chunks[j].id == chunk_id) {
                            strncpy(chunks[j].name, name_buffer, sizeof(chunks[j].name) - 1);
                            chunks[j].name[sizeof(chunks[j].name) - 1] = '\0';
                            chunks[j].is_named = 1;
                            break;
                        }
                    }
                    current_record_offset += 0x1A;
                }
            }
        }
    }

    /* Partition unnamed audio into segmented chunks */
    segmented_chunks = 0;
    segmented_chunk_size = find_segmented_chunk_size(chunks, total_chunks);

    if (segmented_chunk_size > 0) {
        const int SEGMENT_LIMIT = 16;
        int candidates = 0;
        for (i = 0; i < total_chunks; i++) {
            if (!chunks[i].is_named && chunks[i].uncompressed_size == segmented_chunk_size) {
                candidates++;
            }
        }

        /* Only group chunks if not a MOV file, or if it is a MOV with many segments (avoids merging duplicate/reversed chunks) */
        if (!is_mov_file || (candidates > SEGMENT_LIMIT)) {
            for (i = 0; i < total_chunks; i++) {
                if (!chunks[i].is_named && chunks[i].uncompressed_size == segmented_chunk_size) {
                    chunks[i].is_segmented_chunk = 1;
                    segmented_chunks++;
                }
            }
        }
    }

    /* --- Stage 4: Build the VGMSTREAM object --- */
    subsongs = (total_chunks - segmented_chunks) + (segmented_chunks > 0 ? 1 : 0);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > subsongs || subsongs == 0) goto fail;

    current_subsong_idx = 0;

    /* Find the target subsong data, handling the segmented track first */
    if (segmented_chunks > 0) {
        current_subsong_idx++;
        if (current_subsong_idx == target_subsong) {
            char basename[STREAM_NAME_SIZE];
            int max_len;
            int segment_index = 0;

            segmented_layout_data *data = init_layout_segmented(segmented_chunks);
            if (!data) goto fail;

            for (i = 0; i < total_chunks; i++) {
                if (chunks[i].is_segmented_chunk) {
                    VGMSTREAM* segment_vgmstream = allocate_vgmstream(1, 0);
                    if (!segment_vgmstream) {
                        free_layout_segmented(data);
                        goto fail;
                    }

                    build_vgmstream_from_chunk(segment_vgmstream, &chunks[i]);

                    if (!vgmstream_open_stream(segment_vgmstream, sf, chunks[i].offset)) {
                        close_vgmstream(segment_vgmstream);
                        free_layout_segmented(data);
                        goto fail;
                    }

                    data->segments[segment_index++] = segment_vgmstream;
                }
            }

            if (!setup_layout_segmented(data)) {
                free_layout_segmented(data);
                goto fail;
            }

            vgmstream = allocate_segmented_vgmstream(data, 0, -1, -1);
            if (!vgmstream) {
                free_layout_segmented(data);
                goto fail;
            }

            get_streamfile_filename(sf, basename, sizeof(basename));
            max_len = STREAM_NAME_SIZE - TRACK_SUFFIX_SIZE;
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%.*s#%d", max_len, basename, target_subsong);
        }
    }

    /* Handle individual tracks if a segmented one wasn't chosen */
    if (!vgmstream) {
        df_chunk_t* target_chunk = NULL;

        vgmstream = allocate_vgmstream(1, 0);
        if (!vgmstream) goto fail;

        for (i = 0; i < total_chunks; i++) {
            if (chunks[i].is_segmented_chunk) continue;
            current_subsong_idx++;
            if (current_subsong_idx == target_subsong) {
                target_chunk = &chunks[i];
                break;
            }
        }

        if (!target_chunk)
            goto fail;

        build_vgmstream_from_chunk(vgmstream, target_chunk);

        if (target_chunk->is_named) {
            strcpy(vgmstream->stream_name, target_chunk->name);
        } else {
            char basename[STREAM_NAME_SIZE];
            get_streamfile_filename(sf,basename,sizeof(basename));

            /* Calculate the logical index for this unnamed track */
            int unnamed_track_idx = 0;
            if (segmented_chunks > 0) {
                unnamed_track_idx++; /* The segmented track is the first unnamed track */
            }

            for (j = 0; j < total_chunks; j++) {
                if (chunks[j].is_segmented_chunk) continue;
                if (!chunks[j].is_named) {
                    unnamed_track_idx++;
                }
                if (&chunks[j] == target_chunk) {
                    break; /* Found our chunk, the logical index is correct */
                }
            }

            int max_len = STREAM_NAME_SIZE - TRACK_SUFFIX_SIZE;
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%.*s#%d", max_len, basename, unnamed_track_idx);
        }

        if (!vgmstream_open_stream(vgmstream, sf, target_chunk->offset))
            goto fail;
    }

    vgmstream->num_streams = subsongs;

    free(chunks);
    return vgmstream;

fail:
    free(chunks);
    close_vgmstream(vgmstream);
    return NULL;
}
