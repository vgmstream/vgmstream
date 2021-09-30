#include "chunks.h"
//#include "log.h"


int next_chunk(chunk_t* chunk, STREAMFILE* sf) {
    uint32_t (*read_u32type)(off_t,STREAMFILE*) = !chunk->le_type ? read_u32be : read_u32le;
    uint32_t (*read_u32size)(off_t,STREAMFILE*) = chunk->be_size ? read_u32be : read_u32le;

    if (chunk->max == 0)
        chunk->max = get_streamfile_size(sf);

    if (chunk->current >= chunk->max)
        return 0;
    /* can be used to signal "stop" */
    if (chunk->current < 0)
        return 0;

    chunk->type = read_u32type(chunk->current + 0x00,sf);
    chunk->size = read_u32size(chunk->current + 0x04,sf);

    chunk->offset = chunk->current + 0x04 + 0x04;
    chunk->current += chunk->full_size ? chunk->size : 0x08 + chunk->size;
    //;VGM_LOG("CHUNK: %x, %x, %x\n", dc.offset, chunk->type, chunk->size);

    /* read past data */
    if (chunk->type == 0xFFFFFFFF || chunk->size == 0xFFFFFFFF)
        return 0;

    /* empty chunk with 0 size is ok, seen in some formats (XVAG uses it as end marker, Wwise in JUNK) */
    if (chunk->type == 0 /*|| chunk->size == 0*/)
        return 0;

    /* more chunks remain */
    return 1;
}
