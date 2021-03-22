#ifndef _TAC_DECODER_LIB_H_
#define _TAC_DECODER_LIB_H_

/* tri-Ace Codec (TAC) lib, found in PS2 games */

#include <stdint.h>

#define TAC_SAMPLE_RATE 48000
#define TAC_CHANNELS 2
#define TAC_FRAME_SAMPLES 1024
#define TAC_BLOCK_SIZE 0x4E000      /* size of a single block with N VBR frames */

#define TAC_PROCESS_OK              0  /* frame decoded correctly */
#define TAC_PROCESS_NEXT_BLOCK      1  /* must pass next block (didn't decode) */
#define TAC_PROCESS_DONE            2  /* no more frames to do (didn't decode) */
#define TAC_PROCESS_HEADER_ERROR    -1 /* file doesn't match expected header */
#define TAC_PROCESS_ERROR_SIZE      -2 /* buffer is smaller than needed */
#define TAC_PROCESS_ERROR_ID        -3 /* expected frame id mismatch */
#define TAC_PROCESS_ERROR_CRC       -4 /* expected frame crc mismatch */
#define TAC_PROCESS_ERROR_HUFFMAN   -5 /* expected huffman count mismatch */

typedef struct tac_handle_t tac_handle_t;

typedef struct {
    /* 0x20 header config */
    uint32_t huffman_offset;    /* setup */
    uint32_t unknown;           /* ignored? (may be CDVD stuff, divided/multiplied during PS2 process, not size related) */
    uint16_t loop_frame;        /* aligned to block start */
    uint16_t loop_discard;      /* discarded start samples in loop frame (lower = outputs more) */
    uint16_t frame_count;       /* number of valid frames ("block end" frames not included) */
    uint16_t frame_last;        /* valid samples in final frame - 1 (lower = outputs less, 0 = outputs 1), even for non-looped files */
    uint32_t loop_offset;       /* points to a block; file size if not looped */
    uint32_t file_size;         /* block aligned; actual file size can be a bit smaller if last block is truncated */
    uint32_t joint_stereo;      /* usually 0 and rarely 1 ("MSStereoMode") */
    uint32_t empty;             /* always null */
} tac_header_t;


/* inits codec with data from at least one block */
tac_handle_t* tac_init(const uint8_t* buf, int buf_size);

const tac_header_t* tac_get_header(tac_handle_t* handle);

void tac_reset(tac_handle_t* handle);

void tac_free(tac_handle_t* handle);

/* decodes a frame from current block (of TAC_BLOCK_SIZE), returning TAC_PROCESS_* codes */
int tac_decode_frame(tac_handle_t* handle, const uint8_t* block);

void tac_get_samples_pcm16(tac_handle_t* handle, int16_t* dst);

void tac_set_loop(tac_handle_t* handle);

#endif /* _TAC_DECODER_LIB_H_ */
