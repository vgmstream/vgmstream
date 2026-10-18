#ifndef _PROTO_H
#define _PROTO_H
#include <stdio.h>

// OG: seemingly removed
//#define CODEC_VERSION   "0.5b"

#define FRAME_LENGTH                960
#define MAX_SEGMENT_LENGTH          480
//#define MAX_BITS_PER_FRAME        2560

#define  STOP_BAND      800
#define  STOP_BAND4     200

#define  NB_SFM         44
#define  WID_G1         8
#define  WID_G2         16
#define  WID_G3         24
#define  WID_GX         32
#define  NUMC_G1        128
#define  NUMC_G1G2      256
#define  NUMC_N         544


// OG: reference's G192 bit array changed to C++ bitreader
typedef struct {
    const unsigned char* src;
    int src_size;
    const unsigned char* curr;
    int bitpos;
    int error;
} bitstream_t;

typedef struct {
    float old_out[FRAME_LENGTH];
    float old_coeffs[FRAME_LENGTH];

    short num_bits;
    short num_bits_spectrum_stationary;
    short num_bits_spectrum_transient;

    short old_is_transient;
} DecoderState;

void decoder_init(DecoderState *d, short num_bits);

void decode_frame(unsigned char* src, int src_size, short bfi, short out16[], DecoderState *d);

void flvqdec(bitstream_t* is, float *coefsq, float *coefsq_norm, short *R, short NumSpectumBits, short *ynrm, short is_transient);

void bits2idxn(bitstream_t *is, short N, short *x);

void bits2idxc(bitstream_t *is, short N, short L, short *x);

// bitreader 'class'
void bg_init(bitstream_t* is, const unsigned char *src, int src_size);
int bg_tell(bitstream_t* is);
void bg_seek(bitstream_t* is, int pos);
void bg_skip(bitstream_t* is, int bits);
//int bg_peek1(bitstream_t* is);
int bg_read1(bitstream_t* is);


// transform
typedef struct {
    float r;
    float i;
} complex_t;

// transform
extern float window[1920];
extern float short_window[480];

extern float fft120cnst[144];
extern complex_t ptwdf[480];
extern complex_t dct480_table_1[480];
extern complex_t dct480_table_2[480];
extern complex_t dct120_table_1[120];
extern complex_t dct120_table_2[120];

extern int indxPre[120];
extern int indxPost[120];

// unpack
extern float dicn[40];
extern short dicnlg2[40];

extern short RV[10];
extern short FacLVQ2Qv[10];
extern short FacLVQ2Mask[10];
extern short FacLVQ2HalfQv[10];

extern short dic4[256][8];

extern short huffsizn[32];
extern short huffsizc[60];
extern short huffoffset[6];

extern short sfmsize[44];
extern short sfm_start[44];
extern short sfm_end[44];
extern short sfm_width[20];

extern short sfm_a[20];

#endif
