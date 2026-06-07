/* SPDX-License-Identifier: 0BSD */
#ifndef MAAC_INCLUDE_GUARD
#define MAAC_INCLUDE_GUARD

/*

# M(ini)aac

- a single-file, no allocation AAC decoder.

## Building

To use:

In one C file define MAAC_IMPLEMENTATION before including this header:

    #define MAAC_IMPLEMENTATION
    #include "maac.h"

You can customize parts of the build with a few more defines:
    #define MAAC_DOUBLE_PRECISION

    - By default math is done with 32-bit floats, this enables 64-bit doubles.

    #define MAAC_NO_STDMATH

	- this will disable all standard math library functions.
	- If you define this - I think there may be some undefined
	  behavior involved (the library will manually create some floats
	  when computing scale factors).

	#define MAAC_NO_STDSTRING
	- disables the use of memset and memcpy

    #define MAAC_ENABLE_ASSERT
	- enables assertions throughout the code

    #define MAAC_COMPACT_CODEBOOKS
	- attempts to reduce the size of huffman codebooks, last I checked
	  this reduces the storage from about 11kb to 4kb give or take.

	#define MAAC_COMPACT
	- tries to reduce some struct sizes by using bitfields. In hindsight
	  I don't think this really saves all that much space.

	#define MAAC_INVQUANT_TABLES
    - replaces inverse quantization functions with a lookup table. This
      is another one where in hindsight, I don't know if it actually
      speeds anything up, and it winds up making the library about 32kb
      larger. It may not be worth it.

You'll want to ensure you have the same defines enabled anytime you
include the header - whether or not you have MAAC_IMPLEMENTATION defined -
because some of those defines affect struct definitions.

There's also an "maac_extras.h" library, which has functions that aren't
100% essential for decoding, but which may make your life easier. It
provides function for turning various enums into strings, and for FFI-type
purposes, querying the sizes and alignments of structs, functions to
ensure structs are aligned, and setters and getters for struct fields.

    #define MAAC_EXTRAS_IMPLEMENTATION
    #include "maac_extras.h"

## Using

You'll need a total of 3 structs which you can allocate however
you like:

1. Either an maac_adts or maac_raw, depending on if you plan to decode
   AAC in ADTS, or raw data blocks of AAC respectively.

   - If you use maac_raw, you have to configure the sampling frequency index.
     This can be done via suppling AudioSpecificConfig bytes to maac_raw_config,
     or just setting the sf_index value directly. This should be the only config
     needed.
   - `maac_adts` will pick up settings from the ADTS headers automatically.

2. An maac_bitreader, which needs three parameters set:
   - a pointer to a data buffer (maac_bitreader.data)
   - a length value (maac_bitreader.len)

   Throughout decoding you'll "refill" the buffer, usually by loading new
   data into your buffer and updating the "pos" and "len" values.

3. An array of maac_channel objects, one for each channel of audio you plan
   to decode.

You'll call sync and decode functions. If they return 0 (MAAC_CONTINUE),
that means they require more data. Refill the bitreader and try again.

Otherwise - 1 (MAAC_OK) indicates that things were successful and you can now do stuff.

There's two general ways to decode:

* high-level interface, where you associate your output channels to the decoder, feed
it bytes, and it automatically puts samples into your output channels.
* low-level interface - you do not associate your output channels to the decoder.
You feed it bytes and it returns `MAAC_OK` at the start of every AAC element type,
and you then call the appropriate decoding function. This is the only way to
access data stream elements and program config elements.

I have examples for decoding ADTS streams and raw AAC data blocks in the demos folder,
but a high-level pseudocode overview of high-level decoding is:

MAAC_RESULT res;

maac_adts a;
maac_bitreader br;
maac_channel ch[2];

maac_adts_init(&a);
maac_bitreader_init(&br);
maac_channel_init(&ch[0]);
maac_channel_init(&ch[1]);

a.raw.out_channels = ch;
// or maac_adts_set_out_channels(&a, ch);
a.raw.num_out_channels = 2
// or maac_adts_set_num_out_channels(&a, 2);

while(i_have_data()) {
    while( (res = maac_adts_decode(&adts, &br)) == MAAC_CONTINUE) {
        br.data = get_data_somehow();
        br.len = length_of_that_data();
        br.pos = 0;
    }
    if(res != MAAC_OK) {
        ... error out
    }
    for(c=0;c<2;c++) {
        for(i=0;i<ch[c].n_samples;i++) {
            // do something with samples in ch[c].samples[i]
        }
    }
}


And for low-level decoding:

MAAC_RESULT res;

maac_adts a;
maac_bitreader br;
maac_channel ch[2];

maac_adts_init(&a);
maac_bitreader_init(&br);
maac_channel_init(&ch[0]);
maac_channel_init(&ch[1]);

maac_u8 rdb;

while(i_have_data()) {

    // maac_adts_sync will return MAAC_OK after parsing an ADTS header
    while( (res = maac_adts_sync(&adts, &br)) == MAAC_CONTINUE) {
        br.data = get_data_somehow();
        br.len = length_of_that_data();
        br.pos = 0;
    }

    if(res != MAAC_OK) {
        ... error out
    }

    for(rdb=0;rdb<maac_adts_raw_data_blocks(&adts);rdb++) {

        // maac_adts_raw_sync returns MAAC_OK after parsing the next element ID, plus
        // any data needed to make a decision about how to handle the element (like
        // an instance tag, or FIL extension type)

        while( (res = maac_adts_raw_sync(&adts, &br)) == MAAC_CONTINUE) {
            // refill bitreader, as above
        }
        if(res != MAAC_OK) ... error out

        switch(maac_adts_raw_ele_id(&adts)) {
            case MAAC_RAW_DATA_BLOCK_ID_SCE: {
                while((res = maac_adts_raw_decode_sce(&adts, &br, &ch[0])) == MAAC_CONTINUE) {
                    // again refill bitreader
                }
                break;
            }
            case MAAC_RAW_DATA_BLOCK_ID_CPE: {
                while( (res = maac_adts_raw_decode_cpe(&adts, &br, &ch[0], &ch[1])) == MAAC_CONTINUE) {
                    // refill bitreader
                }
                break;
            }
            case MAAC_RAW_DATA_BLOCK_ID_END: {
                rdb++;
                // write out samples, etc
                break;
            }
        }
    }

}

## Limitations

Right now pulse data is implemented but not well-test. I'm unsure how to
create files that uses pulse data.  I did find some
sample files with pulse data but the scale factors were all zero.
As far as I can tell, nobody really uses this feature of AAC?

LC profile only. This does not support any other profiles, though
I do like the idea of trying to implement SBR and/or PS to support
HE-AAC and HE-AACv2.

## Contributors

The following people have contributed code to miniaac:

John Regan <john@jrjrtech.com>

## LICENSE

0BSD

*/

#define MAAC_VERSION_MAJOR 1
#define MAAC_VERSION_MINOR 0
#define MAAC_VERSION_PATCH 0

#ifndef MAAC_PUBLIC

#if defined(__GNUC__) && __GNUC__ > 4
#define MAAC_PUBLIC __attribute__ ((visibility("default")))
#else
#define MAAC_PUBLIC
#endif

#endif /* MAAC_PUBLIC */

#ifndef MAAC_PRIVATE

#if defined(__GNUC__) && __GNUC__ > 4
#define MAAC_PRIVATE __attribute__ ((visibility("hidden")))
#else
#define MAAC_PRIVATE
#endif

#endif /* MAAC_PRIVATE */

#ifndef maac_const

#if defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 5))
#define maac_const __attribute__((__const__))
#elif defined(__has_attribute)
#if __has_attribute(const)
#define maac_const __attribute__((const))
#endif
#else
#define maac_const
#endif

#endif /* maac_const */

#ifndef maac_pure

#if defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 5))
#define maac_pure __attribute__((__pure__))
#elif defined(__has_attribute)
#if __has_attribute(pure)
#define maac_pure __attribute__((pure))
#endif
#else
#define maac_pure
#endif

#endif /* maac_pure */


#ifdef __cplusplus
#include <cstddef>
#else
#include <stddef.h>
#endif

#if defined(__cplusplus) && __cplusplus >= 201103L

#include <cstdint>

typedef uint8_t   maac_u8;
typedef  int8_t   maac_s8;
typedef uint16_t  maac_u16;
typedef  int16_t  maac_s16;
typedef uint32_t  maac_u32;
typedef  int32_t  maac_s32;

#define MAAC_U8_C(x)   UINT8_C(x)
#define MAAC_U16_C(x) UINT16_C(x)
#define MAAC_U32_C(x) UINT32_C(x)

#define MAAC_S8_C(x)   INT8_C(x)
#define MAAC_S16_C(x) INT16_C(x)
#define MAAC_S32_C(x) INT32_C(x)

#define MAAC_S8_MAX INT8_MAX
#define MAAC_S8_MIN INT8_MIN

#define MAAC_S16_MAX INT16_MAX
#define MAAC_S16_MIN INT16_MIN

#define MAAC_S32_MAX INT32_MAX
#define MAAC_S32_MIN INT32_MIN

#define MAAC_U8_MAX  UINT8_MAX
#define MAAC_U16_MAX UINT16_MAX
#define MAAC_U32_MAX UINT32_MAX

#elif (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || (defined(__GNUC__) && (__GNUC__ > 3 || defined(_STDINT_H_)))

#include <stdint.h>

typedef uint8_t   maac_u8;
typedef  int8_t   maac_s8;
typedef uint16_t  maac_u16;
typedef  int16_t  maac_s16;
typedef uint32_t  maac_u32;
typedef  int32_t  maac_s32;

#define MAAC_U8_C(x)   UINT8_C(x)
#define MAAC_U16_C(x) UINT16_C(x)
#define MAAC_U32_C(x) UINT32_C(x)

#define MAAC_S8_C(x)   INT8_C(x)
#define MAAC_S16_C(x) INT16_C(x)
#define MAAC_S32_C(x) INT32_C(x)

#define MAAC_S8_MAX INT8_MAX
#define MAAC_S8_MIN INT8_MIN

#define MAAC_S16_MAX INT16_MAX
#define MAAC_S16_MIN INT16_MIN

#define MAAC_S32_MAX INT32_MAX
#define MAAC_S32_MIN INT32_MIN

#define MAAC_U8_MAX  UINT8_MAX
#define MAAC_U16_MAX UINT16_MAX
#define MAAC_U32_MAX UINT32_MAX

#else /* pre C99/C++11/GCC support */

#if defined(__cplusplus)
#include <climits>
#else
#include <limits.h>
#endif

#define MAAC_U8_MAX  0xFF
#define MAAC_S8_MAX  0x7F
#define MAAC_U16_MAX 0xFFFF
#define MAAC_S16_MAX 0x7FFF
#define MAAC_U32_MAX 0xFFFFFFFFUL
#define MAAC_S32_MAX 0x7FFFFFFFL

#if (MAAC_U8_MAX == UCHAR_MAX)
typedef unsigned char maac_u8;
#define MAAC_U8_C(x) ((maac_u8)x)
#else
#error "Unable to determine suitable u8 type"
#endif

#if (MAAC_S8_MAX == SCHAR_MAX)
typedef signed char maac_s8;
#define MAAC_S8_C(x) ((maac_s8)x)
#define MAAC_S8_MIN SCHAR_MIN
#else
#error "Unable to determine suitable s8 type"
#endif


#if (MAAC_U16_MAX == USHRT_MAX)
typedef unsigned short maac_u16;
#define MAAC_U16_C(x) ((maac_u16)x)
#else
#error "Unable to determine suitable u16 type"
#endif

#if (MAAC_S16_MAX == SHRT_MAX)
typedef signed short maac_s16;
#define MAAC_S16_C(x) ((maac_s16)x)
#define MAAC_S16_MIN SHRT_MIN
#else
#error "Unable to determine suitable s16 type"
#endif


#if (MAAC_U32_MAX == UINT_MAX)
typedef unsigned int maac_u32;
#define MAAC_U32_C(x) (x ## U)
#elif (MAAC_U32_MAX == ULONG_MAX)
typedef unsigned long maac_u32;
#define MAAC_U32_C(x) (x ## UL)
#else
#error "Unable to determine suitable u32 type"
#endif

#if (MAAC_S32_MAX == INT_MAX)
typedef signed int maac_s32;
#define MAAC_S32_C(x) (x)
#define MAAC_S32_MIN SHRT_MIN
#elif (MAAC_S32_MAX == LONG_MAX)
typedef signed long maac_s32;
#define MAAC_S32_C(x) (x ## L)
#define MAAC_S32_MIN LONG_MIN
#else
#error "Unable to determine suitable s32 type"
#endif

#endif /* pre-C99/C++11 support */

#define MAAC_UNREACHABLE                       (-99) /* strictly used in MAAC_UNREACHABLE_RETURN */
#define MAAC_HUFFMAN_DECODE_ERROR              (-15)
#define MAAC_ADTS_RDB_NOT_CALLED               (-14)
#define MAAC_ADTS_SYNCWORD_NOT_FOUND           (-13)
#define MAAC_PREDICTOR_DATA_NOT_IMPLEMENTED    (-12)
#define MAAC_GAIN_CONTROL_DATA_NOT_IMPLEMENTED (-11)
#define MAAC_UNSUPPORTED_AOT                   (-10)
#define MAAC_PULSE_DATA_NOT_IMPLEMENTED        ( -9)
#define MAAC_PCE_NOT_IMPLEMENTED               ( -8)
#define MAAC_DSE_NOT_IMPLEMENTED               ( -7)
#define MAAC_LFE_NOT_IMPLEMENTED               ( -6)
#define MAAC_CCE_NOT_IMPLEMENTED               ( -5)
#define MAAC_SF_INDEX_NOT_SET                  ( -4)
#define MAAC_OUT_OF_SEQUENCE                   ( -3)
#define MAAC_NOT_IMPLEMENTED                   ( -2)
#define MAAC_ERROR                             ( -1) /* generic error, likely in an invalid state */
#define MAAC_CONTINUE                          (  0) /* needs more data, otherwise fine */
#define MAAC_OK                                (  1) /* generic "OK" */

#define MAAC_RESULT_MIN MAAC_HUFFMAN_DECODE_ERROR
#define MAAC_RESULT_MAX MAAC_OK

typedef maac_s32 MAAC_RESULT;

#ifndef maac_restrict

#if defined(__STDC_VERSION__) && __STDC__VERSION__ >= 199901L
#define maac_restrict restrict
#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
#define maac_restrict __restrict
#elif defined(_MSC_VER) && (_MSC_VER >= 1400)
#define maac_restrict __restrict
#else
#define maac_restrict
#endif

#endif /* maac_restrict */

/* TODO - maybe add integer-based math options? */
#ifdef __cplusplus
#include <cfloat>
#else
#include <float.h>
#endif

typedef float  maac_f32;
typedef double maac_f64;

#define MAAC_F32_C(x) (x ## f)
#define MAAC_F64_C(x) (x)

#define maac_f32_cast(x) ((maac_f32)x)
#define maac_f64_cast(x) ((maac_f64)x)

#ifdef MAAC_DOUBLE_PRECISION
typedef maac_f64 maac_flt;
#define MAAC_FLT_C(x) MAAC_F64_C(x)
#else
typedef maac_f32 maac_flt;
#define MAAC_FLT_C(x) MAAC_F32_C(x)
#endif

#define maac_flt_cast(x) ((maac_flt)x)

#ifndef maac_inline

#ifdef __cplusplus
#define maac_inline inline
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define maac_inline inline
#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define maac_inline __inline__
#elif defined(_MSC_VER) && _MSC_VER >= 1200
#define maac_inline __inline
#else
#define maac_inline
#endif

#endif /* maac_inline */

#ifdef MAAC_COMPACT
#define MAAC_BITFIELD(t,x,bits) unsigned int x: bits
#else
#define MAAC_BITFIELD(t, x,bits) t x
#endif

struct maac_bitreader {
    const maac_u8* data;
    maac_u32 tot;
    maac_u32 val;
    maac_u32 pos;
    maac_u32 len;
    maac_u8 bits;
};

typedef struct maac_bitreader maac_bitreader;


/* tracks what's needed to decode a single channel */


struct maac_channel {
    maac_flt samples[3072];
      /* 0    -> 1023: audio samples
         1024 -> 2047: space for IMDCT, maybe SBR samples in the future?
         2048 -> 3071: overlap from previous frame.
       a heads-up that the audio samples in 0-1023 have not been
       clamped and may be outside the signed 16-bit range, you'll want to
       make sure they're in range before writing them out. */
    maac_u8 window_shape_prev;
    maac_u16 n_samples; /* the number of samples */
    maac_u16 _n; /* used for tracking samples written out */
};

typedef struct maac_channel maac_channel;


enum MAAC_FIL_STATE {
    MAAC_FIL_STATE_COUNT              = 0,
    MAAC_FIL_STATE_ESC_COUNT          = 1,
    MAAC_FIL_STATE_PAYLOAD_TYPE       = 2,
    MAAC_FIL_STATE_PAYLOAD_OTHER_BITS = 3
};

typedef enum MAAC_FIL_STATE MAAC_FIL_STATE;

enum MAAC_FIL_EXT {
    MAAC_FIL_EXT_FILL          = 0x00,
    MAAC_FIL_EXT_FILL_DATA     = 0x01,
    MAAC_FIL_EXT_DYNAMIC_RANGE = 0x0b,
    MAAC_FIL_EXT_SBR_DATA      = 0x0d,
    MAAC_FIL_EXT_SBR_DATA_CRC  = 0x0e
};

typedef enum MAAC_FIL_EXT MAAC_FIL_EXT;

struct maac_fil {
    MAAC_FIL_STATE state;
    maac_u16 count;
    maac_u8  extension_type;
    maac_u16 _bits;
};

typedef struct maac_fil maac_fil;


#define MAAC_WINDOW_SEQUENCE_ONLY_LONG   0x00
#define MAAC_WINDOW_SEQUENCE_LONG_START  0x01
#define MAAC_WINDOW_SEQUENCE_EIGHT_SHORT 0x02
#define MAAC_WINDOW_SEQUENCE_LONG_STOP   0x03

#define MAAX_MAX_WINDOW_GROUPS MAAC_U32_C(8)

/* 32 kHz has the highest swb value of 50 */
#define MAAC_MAX_SWB_OFFSET_LONG_WINDOW  MAAC_U32_C(51)

/* a few scale factor band tables have a max short swb of 14 */
#define MAAC_MAX_SWB_OFFSET_SHORT_WINDOW MAAC_U32_C(15)

#define MAAC_MAX_SECTIONS 120

#define MAAC_ZERO_HCB 0
#define MAAC_PAIR_LEN 2
#define MAAC_QUAD_LEN 4
#define MAAC_FIRST_PAIR_HCB 5
#define MAAC_ESC_HCB 11
#define MAAC_NOISE_HCB 13
#define MAAC_INTENSITY_HCB2 14
#define MAAC_INTENSITY_HCB 15
#define MAAC_ESC_FLAG 16

/* in the main profile, TNS_MAX_ORDER_1024 is 20 - but
   we currently only support the LC profile, where the
   max order is 12 */
#define MAAC_TNS_MAX_ORDER_1024 12
#define MAAC_TNS_MAX_ORDER_128 7

/* various order processing stuff includes the max order value */
#define MAAC_TNS_MAX_ORDER 12
#define MAAC_TNS_TOTAL_ORDER (MAAC_TNS_MAX_ORDER_1024+1)


enum MAAC_ICS_INFO_STATE {
    MAAC_ICS_INFO_STATE_RESERVED_BIT                   = 0,
    MAAC_ICS_INFO_STATE_WINDOW_SEQUENCE                = 1,
    MAAC_ICS_INFO_STATE_WINDOW_SHAPE                   = 2,
    MAAC_ICS_INFO_STATE_MAX_SFB                        = 3,
    MAAC_ICS_INFO_STATE_SCALE_FACTOR_GROUPING          = 4,
    MAAC_ICS_INFO_STATE_PREDICTOR_DATA_PRESENT         = 5,
#if MAAC_ENABLE_MAINPROFILE
    MAAC_ICS_INFO_STATE_PREDICTOR_RESET                = 6,
    MAAC_ICS_INFO_STATE_PREDICTOR_RESET_GROUP_NUMBER   = 7,
    MAAC_ICS_INFO_STATE_PREDICTION_USED                = 8,
    MAAC_ICS_INFO_STATE_LAST = MAAC_ICS_INFO_STATE_PREDICTION_USED
#else
    MAAC_ICS_INFO_STATE_LAST = MAAC_ICS_INFO_STATE_PREDICTOR_DATA_PRESENT
#endif
};

typedef enum MAAC_ICS_INFO_STATE MAAC_ICS_INFO_STATE;

struct maac_ics_info {
    MAAC_ICS_INFO_STATE state;
    MAAC_BITFIELD(maac_u8,window_sequence,2);
    MAAC_BITFIELD(maac_u8,window_shape,1);
    MAAC_BITFIELD(maac_u8,max_sfb,6);
    MAAC_BITFIELD(maac_u8,scale_factor_grouping,7);
    MAAC_BITFIELD(maac_u8,predictor_data_present,1);
#if MAAC_ENABLE_MAINPROFILE
    MAAC_BITFIELD(maac_u8,predictor_reset,1);
    MAAC_BITFIELD(maac_u8,predictor_reset_group_number,5);
    maac_u32 prediction_used[2];

    maac_u8 _sfb;
#endif
};

typedef struct maac_ics_info maac_ics_info;


enum MAAC_TNS_STATE {
    MAAC_TNS_STATE_N_FILT           = 0,
    MAAC_TNS_STATE_COEF_RES         = 1,
    MAAC_TNS_STATE_LENGTH           = 2,
    MAAC_TNS_STATE_ORDER            = 3,
    MAAC_TNS_STATE_DIRECTION        = 4,
    MAAC_TNS_STATE_COEF_COMPRESS    = 5,
    MAAC_TNS_STATE_COEF             = 6
};

typedef enum MAAC_TNS_STATE MAAC_TNS_STATE;

struct maac_tns_filter {
    MAAC_BITFIELD(maac_u8, length, 6);
    MAAC_BITFIELD(maac_u8, direction, 1);
    MAAC_BITFIELD(maac_u8, coef_compress, 1);
    MAAC_BITFIELD(maac_u8, order, 5);
    /* we use 4 bits per coeff */
    maac_u8 coef[ (MAAC_TNS_TOTAL_ORDER/2) + (MAAC_TNS_TOTAL_ORDER % 2)];
};
typedef struct maac_tns_filter maac_tns_filter;

struct maac_tns_window {
    MAAC_BITFIELD(maac_u8, n_filt, 2);
    MAAC_BITFIELD(maac_u8, coef_res, 1);
    maac_tns_filter filt[3];
};
typedef struct maac_tns_window maac_tns_window;

struct maac_tns {
    MAAC_TNS_STATE state;
    maac_tns_window window[8];

    maac_u8 _g;
    maac_u8 _k;
    maac_u8 _i;
};
typedef struct maac_tns maac_tns;

struct maac_tns_params {
    const maac_ics_info* info;
    maac_u8 sf_index;
};
typedef struct maac_tns_params maac_tns_params;

MAAC_PRIVATE
MAAC_RESULT
maac_tns_parse(maac_tns* maac_restrict tns, maac_bitreader* maac_restrict br, maac_u8 window_sequence);

MAAC_PRIVATE
void
maac_tns_process(maac_tns* maac_restrict tns, maac_flt* maac_restrict samples, const maac_tns_params* maac_restrict p);

/* the state is really just used for decoding spectral data,
for scalefactor we just always decode from book "0" and process
the index directly (no sign bits, escape codes) */
enum MAAC_HUFFMAN_STATE {
    MAAC_HUFFMAN_STATE_CODEWORD,
    MAAC_HUFFMAN_STATE_SIGN_BITS,
    MAAC_HUFFMAN_STATE_ESC_PREFIX,
    MAAC_HUFFMAN_STATE_ESC
};

typedef enum MAAC_HUFFMAN_STATE MAAC_HUFFMAN_STATE;

struct maac_huffman {
    MAAC_HUFFMAN_STATE state;
    maac_u8 bits; /* the index of how many bits we're trying to read */
    maac_u8 esc;
    maac_u16 offset; /* our current offset into the codebook */
    maac_u32 codeword; /* the decoded codeword */
    maac_u32 index; /* the codeword's index */
};

typedef struct maac_huffman maac_huffman;

MAAC_PRIVATE
void
maac_huffman_init(maac_huffman* h);

MAAC_PRIVATE
MAAC_RESULT
maac_huffman_decode(maac_huffman* h, maac_bitreader* br, maac_u8 codebook);

MAAC_PRIVATE
MAAC_RESULT
maac_huffman_decode_spectral(maac_huffman* h, maac_bitreader* br, maac_u8 codebook, maac_s16 out[4]);

enum MAAC_PULSE_STATE {
    MAAC_PULSE_STATE_NUMBER    = 0,
    MAAC_PULSE_STATE_START_SFB = 1,
    MAAC_PULSE_STATE_OFFSET    = 2,
    MAAC_PULSE_STATE_AMP       = 3
};

typedef enum MAAC_PULSE_STATE MAAC_PULSE_STATE;

struct maac_pulse {
    MAAC_PULSE_STATE state;
    maac_u8 num_pulse;
    maac_u8 start_sfb;
    maac_u16 pulses[4]; /* packed 5 bits offset, 4 bits amp */
    maac_u8 _n;
};

typedef struct maac_pulse maac_pulse;

MAAC_PRIVATE
void
maac_pulse_init(maac_pulse* p);

MAAC_PRIVATE
MAAC_RESULT
maac_pulse_parse(maac_pulse* maac_restrict p, maac_bitreader* maac_restrict br);



enum MAAC_ICS_STATE {
    MAAC_ICS_STATE_GLOBAL_GAIN               =  0,
    MAAC_ICS_STATE_ICS_INFO                  =  1,
    MAAC_ICS_STATE_SECTION_CODEBOOK          =  2,
    MAAC_ICS_STATE_SECTION_CODEBOOK_LENGTH   =  3,
    MAAC_ICS_STATE_SCALE_FACTOR_DATA         =  4,
    MAAC_ICS_STATE_PULSE_DATA_PRESENT        =  5,
    MAAC_ICS_STATE_PULSE_DATA                =  6,
    MAAC_ICS_STATE_TNS_DATA_PRESENT          =  7,
    MAAC_ICS_STATE_TNS_DATA                  =  8,
    MAAC_ICS_STATE_GAIN_CONTROL_DATA_PRESENT =  9,
    MAAC_ICS_STATE_GAIN_CONTROL_DATA         = 10,
    MAAC_ICS_STATE_SPECTRAL_DATA             = 11
};

typedef enum MAAC_ICS_STATE MAAC_ICS_STATE;

struct maac_section_data {
    maac_u8 codebook; /* 4 bits */
    maac_u8      end; /* 6 bits */
};
typedef struct maac_section_data maac_section_data;

struct maac_ics {
    MAAC_ICS_STATE state;

    maac_ics_info info;
    maac_huffman _huffman;

    maac_section_data section_data[MAAC_MAX_SECTIONS];

    maac_s16 scalefactors[MAAC_MAX_SECTIONS];

    maac_pulse pulse;
    maac_tns tns;

    maac_s16 spectra_tmp[4];

    maac_u8 global_gain;
    maac_u8 pulse_data_present;
    maac_u8 tns_data_present;
    maac_u8 gain_control_data_present;

    maac_u8  _g; /* used to iterate num_window_groups */
    maac_u8  _w;
    maac_u16 _i;
    maac_u16 _k;
    maac_u16 _p;
    maac_u16 _off;
    maac_u16 _group_off;
    maac_u8 _noise_flag;
    maac_s16 _dpcm_is_position;
    maac_u8 _scale_factor;
    maac_s32 _noise_energy;
};

typedef struct maac_ics maac_ics;

struct maac_ics_decode_params {
    maac_u32 sf_index;
    maac_u8 common_window;
    maac_channel *ch;
};

typedef struct maac_ics_decode_params maac_ics_decode_params;


enum MAAC_SCE_STATE {
    MAAC_SCE_STATE_TAG  = 0,
    MAAC_SCE_STATE_ICS  = 1
};

typedef enum MAAC_SCE_STATE MAAC_SCE_STATE;

struct maac_sce {
    MAAC_SCE_STATE state;
    maac_u8 element_instance_tag;
    maac_ics ics;
};

typedef struct maac_sce maac_sce;

struct maac_sce_decode_params {
    maac_u32 sf_index;
    maac_channel* ch;
    maac_u32* rand_state;
};
typedef struct maac_sce_decode_params maac_sce_decode_params;


enum MAAC_CPE_STATE {
    MAAC_CPE_STATE_TAG             = 0,
    MAAC_CPE_STATE_COMMON_WINDOW   = 1,
    MAAC_CPE_STATE_ICS_INFO        = 2,
    MAAC_CPE_STATE_MS_MASK_PRESENT = 3,
    MAAC_CPE_STATE_MS_USED         = 4,
    MAAC_CPE_STATE_ICS_LEFT        = 5,
    MAAC_CPE_STATE_ICS_RIGHT       = 6
};

typedef enum MAAC_CPE_STATE MAAC_CPE_STATE;

struct maac_cpe {
    MAAC_CPE_STATE state;
    maac_u8 element_instance_tag;
    maac_u8 common_window;
    maac_u8 ms_mask_present;
    maac_u32 ms_used[4];
    maac_ics_info info;
    maac_ics ics_l;
    maac_ics ics_r;
    maac_u32* rand_state;

    maac_u8 _g;
    maac_u8 _sfb;
};

typedef struct maac_cpe maac_cpe;

struct maac_cpe_decode_params {
    maac_u32 sf_index;
    maac_channel* l;
    maac_channel* r;
    maac_u32* rand_state;
};
typedef struct maac_cpe_decode_params maac_cpe_decode_params;




enum MAAC_PCE_STATE {
    MAAC_PCE_STATE_ELEMENT_INSTANCE_TAG          = 0,
    MAAC_PCE_STATE_PROFILE                       = 1,
    MAAC_PCE_STATE_SAMPLING_FREQUENCY_INDEX      = 2,
    MAAC_PCE_STATE_NUM_FRONT_CHANNEL_ELEMENTS    = 3,
    MAAC_PCE_STATE_NUM_SIDE_CHANNEL_ELEMENTS     = 4,
    MAAC_PCE_STATE_NUM_BACK_CHANNEL_ELEMENTS     = 5,
    MAAC_PCE_STATE_NUM_LFE_CHANNEL_ELEMENTS      = 6,
    MAAC_PCE_STATE_NUM_ASSOC_DATA_ELEMENTS       = 7,
    MAAC_PCE_STATE_NUM_VALID_CC_ELEMENTS         = 8,
    MAAC_PCE_STATE_MONO_MIXDOWN_PRESENT          = 9,
    MAAC_PCE_STATE_MONO_MIXDOWN_ELEMENT_NUMBER   = 10,
    MAAC_PCE_STATE_STEREO_MIXDOWN_PRESENT        = 11,
    MAAC_PCE_STATE_STEREO_MIXDOWN_ELEMENT_NUMBER = 12,
    MAAC_PCE_STATE_MATRIX_MIXDOWN_IDX_PRESENT    = 13,
    MAAC_PCE_STATE_MATRIX_MIXDOWN_IDX            = 14,
    MAAC_PCE_STATE_PSEUDO_SURROUND_ENABLE        = 15,
    MAAC_PCE_STATE_FRONT_ELEMENT_IS_CPE          = 16,
    MAAC_PCE_STATE_FRONT_ELEMENT_TAG_SELECT      = 17,
    MAAC_PCE_STATE_SIDE_ELEMENT_IS_CPE           = 18,
    MAAC_PCE_STATE_SIDE_ELEMENT_TAG_SELECT       = 19,
    MAAC_PCE_STATE_BACK_ELEMENT_IS_CPE           = 20,
    MAAC_PCE_STATE_BACK_ELEMENT_TAG_SELECT       = 21,
    MAAC_PCE_STATE_LFE_ELEMENT_TAG_SELECT        = 22,
    MAAC_PCE_STATE_ASSOC_DATA_ELEMENT_TAG_SELECT = 23,
    MAAC_PCE_STATE_CC_ELEMENT_IS_IND_SW          = 24,
    MAAC_PCE_STATE_VALID_CC_ELEMENT_TAG_SELECT   = 25,
    MAAC_PCE_STATE_COMMENT_FIELD_BYTES           = 26,
    MAAC_PCE_STATE_COMMENT_FIELD_DATA            = 27
};

typedef enum MAAC_PCE_STATE MAAC_PCE_STATE;

struct maac_pce {
    MAAC_PCE_STATE state;
    maac_u8 element_instance_tag;
    maac_u8 profile;
    maac_u8 sampling_frequency_index;
    maac_u8 num_front_channel_elements;
    maac_u8 num_side_channel_elements;
    maac_u8 num_back_channel_elements;
    maac_u8 num_lfe_channel_elements;
    maac_u8 num_assoc_data_elements;
    maac_u8 num_valid_cc_elements;
    maac_u8 mono_mixdown_present;
    maac_u8 mono_mixdown_element_number;
    maac_u8 stereo_mixdown_present;
    maac_u8 stereo_mixdown_element_number;
    maac_u8 matrix_mixdown_idx_present;
    maac_u8 matrix_mixdown_idx;
    maac_u8 pseudo_surround_enable;
    maac_u16 front_element_is_cpe; /* 16 bitflags */
    maac_u8 front_element_tag_select[8]; /* 4bits each, 2 per */
    maac_u16 side_element_is_cpe; /* 16 bitflags */
    maac_u8 side_element_tag_select[8]; /* 4bits each, 2 per */
    maac_u16 back_element_is_cpe; /* 16 bitflags */
    maac_u8 back_element_tag_select[8]; /* 4bits each, 2 per */
    maac_u8 lfe_element_tag_select[8]; /* 4bits each, 2 per */
    maac_u8 assoc_data_element_tag_select[8]; /* 4bits each, 2 per */
    maac_u16 cc_element_is_ind_sw; /* 16 bitflags */
    maac_u8 valid_cc_element_tag_select[8]; /* 4bits each, 2 per */
    maac_u8 comment_field_bytes;
    maac_u8 comment_field_data[256];
    maac_u8 _i;
};

typedef struct maac_pce maac_pce;



enum MAAC_DSE_STATE {
    MAAC_DSE_STATE_ELEMENT_INSTANCE_TAG = 0,
    MAAC_DSE_STATE_DATA_BYTE_ALIGN_FLAG = 1,
    MAAC_DSE_STATE_COUNT                = 2,
    MAAC_DSE_STATE_ESC_COUNT            = 3,
    MAAC_DSE_STATE_DATA_STREAM_BYTE     = 4
};

typedef enum MAAC_DSE_STATE MAAC_DSE_STATE;

struct maac_dse {
    MAAC_DSE_STATE state;
    maac_u8  element_instance_tag;
    maac_u8 data_byte_align_flag;
    maac_u16 count;
    maac_u8 data_stream_byte[510];
    maac_u16 _i;
};

typedef struct maac_dse maac_dse;



/*

This is our main decoder for raw AAC data.

Setup is roughly:

* Call maac_raw_init().
* Configure the sampling frequency index (sf_index).
  - this can be done with maac_raw_config(), which
    expects to receive a full AudioSpecifcConfig
* Assign output channels (channels, num_channels)

Then to decode:

* Call maac_raw_decode() until it returns MAAC_OK
* Do things with your samples, repeat.

It will automatically track which audio channel should
receive output and handle it appropriately.

There are lower-level APIs too, in case you want
to involve yourself with managing individual channel
elements.

Setup is:

* Call maac_raw_init()
* Configure the sampling frequency index (sf_index).

Then to decode:

* Call maac_raw_sync() until it returns MAAC_OK
* Inspect the current element ID (ele_id)
  Call one of the following, based on element ID:
  - maac_raw_decode_fil()
  - maac_raw_decode_sce()
  - maac_raw_decode_cpe()
* Repeat the maac_raw_sync + maac_raw_decode* combo until
  ele_id is MAAC_RAW_DATA_BLOCK_ID_END
  Handle any post-decode steps you need to, repeat.

After a decode call you can retrieve the element's struct if
you wish to inspect further:
  raw.ele.fil, raw.ele.sce, raw.ele.cpe

*/

enum MAAC_RAW_DATA_BLOCK_ID {
    MAAC_RAW_DATA_BLOCK_ID_SCE = 0x00,
    MAAC_RAW_DATA_BLOCK_ID_CPE = 0x01,
    MAAC_RAW_DATA_BLOCK_ID_CCE = 0x02,
    MAAC_RAW_DATA_BLOCK_ID_LFE = 0x03,
    MAAC_RAW_DATA_BLOCK_ID_DSE = 0x04,
    MAAC_RAW_DATA_BLOCK_ID_PCE = 0x05,
    MAAC_RAW_DATA_BLOCK_ID_FIL = 0x06,
    MAAC_RAW_DATA_BLOCK_ID_END = 0x07
};

typedef enum MAAC_RAW_DATA_BLOCK_ID MAAC_RAW_DATA_BLOCK_ID;

enum MAAC_RAW_STATE {
    MAAC_RAW_STATE_BLOCK_ID = 0,
    MAAC_RAW_STATE_FIL      = 1,
    MAAC_RAW_STATE_SCE      = 2,
    MAAC_RAW_STATE_CPE      = 3,
    MAAC_RAW_STATE_LFE      = 4,
    MAAC_RAW_STATE_PCE      = 5,
    MAAC_RAW_STATE_DSE      = 6
};

typedef enum MAAC_RAW_STATE MAAC_RAW_STATE;

union maac_raw_element {
    maac_fil fil;
    maac_sce sce; /* also used for lfe */
    maac_cpe cpe;
    maac_pce pce;
    maac_dse dse;
};

typedef union maac_raw_element maac_raw_element;

struct maac_raw {
    /* these are intended to be inspected/set by the user */

    /* our current sync/decode state */
    MAAC_RAW_STATE state;

    /* the sampling frequency index */
    maac_u8 sf_index;

    /* the current element ID (after a call to sync()) */
    maac_u8 ele_id;

    /* Storage for the element being decoded */
    maac_raw_element ele;

    /* output audio channels */
    maac_channel* out_channels;
    maac_u8 num_out_channels;

    /* these are intended to be read-only fields (not set by user) */
    maac_u32 sample_rate;
    maac_u8 channel_configuration;

    /* these are intended to be internal fields */
    maac_u32 rand_state;
    maac_u8 _c; /* current channel being decoded to */
};

typedef struct maac_raw maac_raw;


/*

This is a decoder for ADTS-encapsulated AAC data.

Setup is roughly:

* call maac_adts_init()
* call maac_adts_sync() to read the first header
* Assign output channel objects

Then to decode:
* Call maac_adts_decode() until it returns MAAC_OK
* Do things with your samples, repeat.

It will automatically track which audio channel
should receive output, and handle multiple data
blocks if your ADTS has multiple data blocks (this
is very rare).

If you want to manage manage individual elements
you can do that too.

After a call to call maac_adts_sync() - you can
call maac_adts_raw_sync(), then inspect the
raw data block's element id (raw.ele_id).

Then call one of the following:
  - maac_adts_decode_fil()
  - maac_adts_decode_sce()
  - maac_adts_decode_cpe()
* Repeat  until raw.ele_id is MAAC_RAW_DATA_BLOCK_ID_END
* If you have more then 1 raw data block, you'll need to
  repeat this process again (and track how many raw data
  blocks you've handled).
* Once you've handled all raw data blocks, you need
  to manually set the state back to MAAC_ADTS_STATE_SYNCWORD
  and then call the next maac_adts_sync().
*/


enum MAAC_ADTS_STATE {
    MAAC_ADTS_STATE_SYNCWORD,
    MAAC_ADTS_STATE_VERSION,
    MAAC_ADTS_STATE_LAYER,
    MAAC_ADTS_STATE_PROTECTION,
    MAAC_ADTS_STATE_PROFILE,
    MAAC_ADTS_STATE_FREQ_INDEX,
    MAAC_ADTS_STATE_PRIVATE,
    MAAC_ADTS_STATE_CHANNEL_INDEX,
    MAAC_ADTS_STATE_ORIG,
    MAAC_ADTS_STATE_HOME,
    MAAC_ADTS_STATE_COPYRIGHT,
    MAAC_ADTS_STATE_COPYRIGHT_START,
    MAAC_ADTS_STATE_LENGTH,
    MAAC_ADTS_STATE_BUFFER_FULLNESS,
    MAAC_ADTS_STATE_RAW_DATA_BLOCKS,
    MAAC_ADTS_STATE_CRC16,
    MAAC_ADTS_STATE_RAW_DATA_BLOCK_POSITION,
    MAAC_ADTS_STATE_RAW_DATA_BLOCK,
    MAAC_ADTS_STATE_RAW_DATA_BLOCK_CRC16
};

typedef enum MAAC_ADTS_STATE MAAC_ADTS_STATE;

struct maac_adts {
    MAAC_ADTS_STATE state;

    /* this should all pack into a single 32-bit int */
    struct {
        MAAC_BITFIELD(maac_u16,frame_length,13);
        MAAC_BITFIELD(maac_u16,buffer_fullness,11);
        MAAC_BITFIELD(maac_u8,copyright_id_bit,1);
        MAAC_BITFIELD(maac_u8,copyright_id_start,1);
        MAAC_BITFIELD(maac_u8,raw_data_blocks,2);
    } variable_header;

    struct {
        MAAC_BITFIELD(maac_u8, version, 1);
        MAAC_BITFIELD(maac_u8, layer, 2);
        MAAC_BITFIELD(maac_u8, protection_absent, 1);
        MAAC_BITFIELD(maac_u8, profile, 2);
        MAAC_BITFIELD(maac_u8, sampling_frequency_index, 4);
        MAAC_BITFIELD(maac_u8, channel_configuration, 3);
        MAAC_BITFIELD(maac_u8, original_copy, 1);
        MAAC_BITFIELD(maac_u8, home, 1);
    } fixed_header;

    /* used for both adts_error_check and adts_header_error_check */
    maac_u16 crc;
    maac_u16 raw_data_block_position[3];

    /* how many bytes we're willing to discard while finding
       the first syncword, defaults to 0 */
    maac_u32 tolerance;

    maac_raw raw;

    maac_u32 _i;
};

typedef struct maac_adts maac_adts;


#ifdef __cplusplus
extern "C" {
#endif


MAAC_PUBLIC
void
maac_bitreader_init(maac_bitreader* br);


MAAC_PUBLIC
void
maac_channel_init(maac_channel *ch);


MAAC_PUBLIC
void
maac_fil_init(maac_fil* f);

MAAC_PUBLIC
MAAC_RESULT
maac_fil_decode(maac_fil* maac_restrict f, maac_bitreader* maac_restrict br);


MAAC_PUBLIC
void
maac_ics_info_init(maac_ics_info* ics_info);

MAAC_PUBLIC
MAAC_RESULT
maac_ics_info_parse(maac_ics_info* maac_restrict ics_info, maac_bitreader* maac_restrict br, const maac_u32 sf_index);


MAAC_PUBLIC
void
maac_ics_init(maac_ics* ics);

MAAC_PUBLIC
MAAC_RESULT
maac_ics_decode(maac_ics* ics, maac_bitreader* maac_restrict br, const maac_ics_decode_params* p);


MAAC_PUBLIC
void
maac_sce_init(maac_sce* s);

MAAC_PUBLIC
MAAC_RESULT
maac_sce_decode(maac_sce* maac_restrict s, maac_bitreader* maac_restrict br, const maac_sce_decode_params* maac_restrict p);


MAAC_PUBLIC
void
maac_cpe_init(maac_cpe* s);

MAAC_PUBLIC
MAAC_RESULT
maac_cpe_decode(maac_cpe* maac_restrict s, maac_bitreader* maac_restrict br, const maac_cpe_decode_params* maac_restrict p);


MAAC_PUBLIC
void
maac_pce_init(maac_pce* p);

MAAC_PUBLIC
MAAC_RESULT
maac_pce_decode(maac_pce* maac_restrict p, maac_bitreader* maac_restrict br);


MAAC_PUBLIC
void
maac_dse_init(maac_dse* d);

MAAC_PUBLIC
MAAC_RESULT
maac_dse_decode(maac_dse* maac_restrict d, maac_bitreader* maac_restrict br);


MAAC_PUBLIC
void
maac_raw_init(maac_raw* r);

/* instead of manually configuring the sf_index, you can
use this function with a buffer of AudioSpecificConfig */
MAAC_PUBLIC
MAAC_RESULT
maac_raw_config(maac_raw* r, const maac_u8* data, maac_u32 len);

/* the more "managed" API - returns MAAC_OK after decoding the full
raw data block */
MAAC_PUBLIC
MAAC_RESULT
maac_raw_decode(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br);

/* the lower-level API - returns MAAC_OK after decoding the next element ID */
MAAC_PUBLIC
MAAC_RESULT
maac_raw_sync(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br);

/* Returns MAAC_OK after decoding a FIL element */
MAAC_PUBLIC
MAAC_RESULT
maac_raw_decode_fil(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br);

/* Returns MAAC_OK after decoding a SCE element */
MAAC_PUBLIC
MAAC_RESULT
maac_raw_decode_sce(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br, maac_channel* maac_restrict c);

/* Returns MAAC_OK after decoding a CPE element */
MAAC_PUBLIC
MAAC_RESULT
maac_raw_decode_cpe(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br, maac_channel* maac_restrict left, maac_channel* maac_restrict right);

/* Returns MAAC_OK after decoding an LFE element */
MAAC_PUBLIC
MAAC_RESULT
maac_raw_decode_lfe(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br, maac_channel* maac_restrict c);

/* Returns MAAC_OK after decoding a PCE element */
MAAC_PUBLIC
MAAC_RESULT
maac_raw_decode_pce(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br);

/* Returns MAAC_OK after decoding a DSE element */
MAAC_PUBLIC
MAAC_RESULT
maac_raw_decode_dse(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br);


MAAC_PUBLIC
void
maac_adts_init(maac_adts* a);

/* returns MAAC_OK just before starting to decode the next raw data block.
so you can inspect ADTS frame headers for info like sample rate, channels, etc.
   returns MAAC_CONTINUE if more data is needed (so you'll need to refill the
   bitreader) */
MAAC_PUBLIC
MAAC_RESULT
maac_adts_sync(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br);

/* returns MAAC_OK just after decoding a raw data block. Automatically
calls maac_adts_sync() if needed. */
MAAC_PUBLIC
MAAC_RESULT
maac_adts_decode(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br);

/* lower-level API - after a successful call to maac_adts_sync(),
use this to read the next element id in the raw data block */
MAAC_PUBLIC
MAAC_RESULT
maac_adts_raw_sync(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br);

/* lower-level API - after a call to maac_adts_raw_sync(), decode a FIL element */
MAAC_PUBLIC
MAAC_RESULT
maac_adts_raw_decode_fil(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br);

/* lower-level API - after a call to maac_adts_raw_sync(), decode a SCE element */
MAAC_PUBLIC
MAAC_RESULT
maac_adts_raw_decode_sce(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br, maac_channel* maac_restrict  c);

/* lower-level API - after a call to maac_adts_raw_sync(), decode a CPE element */
MAAC_PUBLIC
MAAC_RESULT
maac_adts_raw_decode_cpe(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br, maac_channel* maac_restrict left, maac_channel* maac_restrict right);

/* lower-level API - after a call to maac_adts_raw_sync(), decode a LFE element */
MAAC_PUBLIC
MAAC_RESULT
maac_adts_raw_decode_lfe(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br, maac_channel* maac_restrict  c);

/* lower-level API - after a call to maac_adts_raw_sync(), decode a PCE element */
MAAC_PUBLIC
MAAC_RESULT
maac_adts_raw_decode_pce(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br);

/* lower-level API - after a call to maac_adts_raw_sync(), decode a DSE element */
MAAC_PUBLIC
MAAC_RESULT
maac_adts_raw_decode_DSE(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_GUARD */

#ifdef MAAC_IMPLEMENTATION
#ifndef MAAC_IMPLEMENETATION_DEFINED
#define MAAC_IMPLEMENETATION_DEFINED

MAAC_PRIVATE
void* maac_memset(void* _dest, int val, size_t len);


#ifndef MAAC_UNREACHABLE_RETURN

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
#define MAAC_UNREACHABLE_RETURN(x) __builtin_unreachable()
#else
#define MAAC_UNREACHABLE_RETURN(x) return x
#endif

#endif /* MAAC_UNREACHABLE_RETURN */

MAAC_PRIVATE
MAAC_RESULT
maac_bitreader_fill(maac_bitreader* br, maac_u8 bits);

MAAC_PRIVATE
maac_u32
maac_bitreader_peek(const maac_bitreader* br, maac_u8 bits);

MAAC_PRIVATE
void
maac_bitreader_discard(maac_bitreader* br, maac_u8 bits);

MAAC_PRIVATE
maac_u32
maac_bitreader_read(maac_bitreader* br, maac_u8 bits);

MAAC_PRIVATE
void
maac_bitreader_byte_align(maac_bitreader* br);

maac_const
MAAC_PUBLIC
maac_u32
maac_sampling_frequency_index(maac_u32 sample_rate);

maac_const
MAAC_PUBLIC
maac_u32
maac_sampling_frequency(maac_u32 sample_frequency_index);

MAAC_PUBLIC
void
maac_adts_init(maac_adts* a) {
    a->state = MAAC_ADTS_STATE_SYNCWORD;
    maac_raw_init(&a->raw);
    /* maac_memset(a, 0, sizeof *a); */
}

MAAC_PUBLIC
MAAC_RESULT
maac_adts_sync(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;
    maac_u32 tmp;

    switch(a->state) {
        case MAAC_ADTS_STATE_SYNCWORD: {
            maac_adts_state_syncword:
            if( (res = maac_bitreader_fill(br,12)) != MAAC_OK) return res;
            tmp = (maac_u32)maac_bitreader_peek(br,12);
            if(tmp == 0x0fff) {
                maac_bitreader_discard(br,12);
                a->_i = 0;
                a->state = MAAC_ADTS_STATE_VERSION;
                maac_memset(&a->fixed_header, 0, sizeof(a->fixed_header));
                maac_memset(&a->variable_header, 0, sizeof(a->variable_header));
                goto maac_adts_state_version;
            }
                
            if(a->_i == a->tolerance) {
                return MAAC_ADTS_SYNCWORD_NOT_FOUND;
            }

            maac_bitreader_discard(br,8);
            a->_i++;
            goto maac_adts_state_syncword;
        }
        /* fall-through */
        case MAAC_ADTS_STATE_VERSION: {
            maac_adts_state_version:
            if((res = maac_bitreader_fill(br,1)) != MAAC_OK) return res;
            a->fixed_header.version = maac_bitreader_read(br,1);
            a->state = MAAC_ADTS_STATE_LAYER;
        }
        /* fall-through */
        case MAAC_ADTS_STATE_LAYER: {
            if((res = maac_bitreader_fill(br,2)) != MAAC_OK) return res;
            a->fixed_header.layer = maac_bitreader_read(br,2);
            a->state = MAAC_ADTS_STATE_PROTECTION;
        }
        /* fall-through */
        case MAAC_ADTS_STATE_PROTECTION: {
            if((res = maac_bitreader_fill(br,1)) != MAAC_OK) return res;
            a->fixed_header.protection_absent = maac_bitreader_read(br,1);
            a->state = MAAC_ADTS_STATE_PROFILE;
        }
        /* fall-through */
        case MAAC_ADTS_STATE_PROFILE: {
            if((res = maac_bitreader_fill(br,2)) != MAAC_OK) return res;
            a->fixed_header.profile = maac_bitreader_read(br,2);
            a->state = MAAC_ADTS_STATE_FREQ_INDEX;
        }
        /* fall-through */
        case MAAC_ADTS_STATE_FREQ_INDEX: {
            if((res = maac_bitreader_fill(br,4)) != MAAC_OK) return res;
            a->fixed_header.sampling_frequency_index = maac_bitreader_read(br,4);
            a->state = MAAC_ADTS_STATE_PRIVATE;
        }
        /* fall-through */
        case MAAC_ADTS_STATE_PRIVATE: {
            if((res = maac_bitreader_fill(br,1)) != MAAC_OK) return res;
            maac_bitreader_discard(br,1);
            a->state = MAAC_ADTS_STATE_CHANNEL_INDEX;
        }
        /* fall-through */
        case MAAC_ADTS_STATE_CHANNEL_INDEX: {
            if((res = maac_bitreader_fill(br,3)) != MAAC_OK) return res;
            a->fixed_header.channel_configuration = maac_bitreader_read(br,3);
            a->state = MAAC_ADTS_STATE_ORIG;
        }
        /* fall-through */
        case MAAC_ADTS_STATE_ORIG: {
            if((res = maac_bitreader_fill(br,1)) != MAAC_OK) return res;
            a->fixed_header.original_copy = maac_bitreader_read(br,1);
            a->state = MAAC_ADTS_STATE_HOME;
        }
        /* fall-through */
        case MAAC_ADTS_STATE_HOME: {
            if((res = maac_bitreader_fill(br,1)) != MAAC_OK) return res;
            a->fixed_header.home = maac_bitreader_read(br,1);
            a->state = MAAC_ADTS_STATE_COPYRIGHT;
        }
        /* fall-through */
        case MAAC_ADTS_STATE_COPYRIGHT: {
            if((res = maac_bitreader_fill(br,1)) != MAAC_OK) return res;
            a->variable_header.copyright_id_bit = maac_bitreader_read(br,1);
            a->state = MAAC_ADTS_STATE_COPYRIGHT_START;
        }
        /* fall-through */
        case MAAC_ADTS_STATE_COPYRIGHT_START: {
            if((res = maac_bitreader_fill(br,1)) != MAAC_OK) return res;
            a->variable_header.copyright_id_start = maac_bitreader_read(br,1);
            a->state = MAAC_ADTS_STATE_LENGTH;
        }
        /* fall-through */
        case MAAC_ADTS_STATE_LENGTH: {
            if((res = maac_bitreader_fill(br,13)) != MAAC_OK) return res;
            a->variable_header.frame_length = maac_bitreader_read(br,13);
            a->state = MAAC_ADTS_STATE_BUFFER_FULLNESS;
        }
        /* fall-through */
        case MAAC_ADTS_STATE_BUFFER_FULLNESS: {
            if((res = maac_bitreader_fill(br,11)) != MAAC_OK) return res;
            a->variable_header.buffer_fullness = maac_bitreader_read(br,11);
            a->state = MAAC_ADTS_STATE_RAW_DATA_BLOCKS;
        }
        /* fall-through */
        case MAAC_ADTS_STATE_RAW_DATA_BLOCKS: {
            if((res = maac_bitreader_fill(br,2)) != MAAC_OK) return res;
            a->variable_header.raw_data_blocks = maac_bitreader_read(br,2);

            /* if the protection_absent flag is set - there's no CRC
            data so we're ready to roll */
            if(a->fixed_header.protection_absent) {
                a->state = MAAC_ADTS_STATE_RAW_DATA_BLOCK;
                a->_i = 0;
                /* configure the raw data block */
                a->raw.sf_index = a->fixed_header.sampling_frequency_index;
                a->raw.sample_rate = maac_sampling_frequency(a->raw.sf_index);
                return MAAC_OK;
            }

            if(a->variable_header.raw_data_blocks == 0) {
                a->state = MAAC_ADTS_STATE_CRC16;
                goto maac_adts_state_crc16;
            }
            /* we have multiple data blocks, and positions to read */
            a->state = MAAC_ADTS_STATE_RAW_DATA_BLOCK_POSITION;
            a->_i = 0;
            goto maac_adts_state_raw_data_block_position;
        }

        case MAAC_ADTS_STATE_RAW_DATA_BLOCK_POSITION: {
            maac_adts_state_raw_data_block_position:
            while(a->_i < a->variable_header.raw_data_blocks) {
                if( (res = maac_bitreader_fill(br,16)) != MAAC_OK) return res;
                a->raw_data_block_position[a->_i] = maac_bitreader_read(br,16);
                a->_i++;
            }
            a->state = MAAC_ADTS_STATE_CRC16;
            goto maac_adts_state_crc16;
        }

        case MAAC_ADTS_STATE_CRC16: {
            maac_adts_state_crc16:
            if((res = maac_bitreader_fill(br,16)) != MAAC_OK) return res;
            a->crc = (maac_u16)maac_bitreader_read(br,16);

            a->state = MAAC_ADTS_STATE_RAW_DATA_BLOCK;
            a->_i = 0;
            a->raw.sf_index = a->fixed_header.sampling_frequency_index;
            a->raw.sample_rate = maac_sampling_frequency(a->raw.sf_index);
            return MAAC_OK;
        }

        case MAAC_ADTS_STATE_RAW_DATA_BLOCK: {
            return MAAC_OUT_OF_SEQUENCE;
#if 0
            if(a->_i <= a->variable_header.raw_data_blocks) {
                /* this means maac_adts_rdb wasn't called to count off a
                   raw data block */
                return MAAC_ADTS_RDB_NOT_CALLED;
            }
            break;
#endif
        }
        case MAAC_ADTS_STATE_RAW_DATA_BLOCK_CRC16: {
            return MAAC_OUT_OF_SEQUENCE;
        }
    }

    MAAC_UNREACHABLE_RETURN(MAAC_UNREACHABLE);
}

MAAC_PUBLIC
MAAC_RESULT
maac_adts_raw_sync(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;

    if(a->state == MAAC_ADTS_STATE_RAW_DATA_BLOCK_CRC16) {
        maac_adts_raw_sync_crc16:
        if( (res = maac_bitreader_fill(br, 16)) != MAAC_OK) return res;
        maac_bitreader_discard(br, 16);
        goto maac_adts_raw_sync_nextrdb;
    }

    if(a->state != MAAC_ADTS_STATE_RAW_DATA_BLOCK) {
        return MAAC_OUT_OF_SEQUENCE;
    }

    if( (res = maac_raw_sync(&a->raw, br)) != MAAC_OK) {
        return res;
    }

    if(a->raw.ele_id == MAAC_RAW_DATA_BLOCK_ID_END) {
        /* if we have to read a trailing CRC16 we don't
        want to return until we've read it */
        if(a->variable_header.raw_data_blocks && !(a->fixed_header.protection_absent)) {
            a->state = MAAC_ADTS_STATE_RAW_DATA_BLOCK_CRC16;
            goto maac_adts_raw_sync_crc16;
        }

        maac_adts_raw_sync_nextrdb:
        a->_i++;
        if(a->_i <= a->variable_header.raw_data_blocks) {
            a->state = MAAC_ADTS_STATE_RAW_DATA_BLOCK;
        } else {
            a->state = MAAC_ADTS_STATE_SYNCWORD;
            a->_i = 0;
        }
    }

    return MAAC_OK;
}

MAAC_PUBLIC
MAAC_RESULT
maac_adts_raw_decode_fil(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br) {
    if(a->state != MAAC_ADTS_STATE_RAW_DATA_BLOCK) {
        return MAAC_OUT_OF_SEQUENCE;
    }
    return maac_raw_decode_fil(&a->raw, br);
}

MAAC_PUBLIC
MAAC_RESULT
maac_adts_raw_decode_sce(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br, maac_channel* maac_restrict c) {
    if(a->state != MAAC_ADTS_STATE_RAW_DATA_BLOCK) {
        return MAAC_OUT_OF_SEQUENCE;
    }
    return maac_raw_decode_sce(&a->raw, br, c);
}

MAAC_PUBLIC
MAAC_RESULT
maac_adts_raw_decode_cpe(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br, maac_channel* maac_restrict left, maac_channel* maac_restrict right) {
    if(a->state != MAAC_ADTS_STATE_RAW_DATA_BLOCK) {
        return MAAC_OUT_OF_SEQUENCE;
    }
    return maac_raw_decode_cpe(&a->raw, br, left, right);
}

MAAC_PUBLIC
MAAC_RESULT
maac_adts_raw_decode_lfe(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br, maac_channel* maac_restrict c) {
    if(a->state != MAAC_ADTS_STATE_RAW_DATA_BLOCK) {
        return MAAC_OUT_OF_SEQUENCE;
    }
    return maac_raw_decode_lfe(&a->raw, br, c);
}

MAAC_PUBLIC
MAAC_RESULT
maac_adts_raw_decode_pce(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br) {
    if(a->state != MAAC_ADTS_STATE_RAW_DATA_BLOCK) {
        return MAAC_OUT_OF_SEQUENCE;
    }
    return maac_raw_decode_pce(&a->raw, br);
}

MAAC_PUBLIC
MAAC_RESULT
maac_adts_raw_decode_dse(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br) {
    if(a->state != MAAC_ADTS_STATE_RAW_DATA_BLOCK) {
        return MAAC_OUT_OF_SEQUENCE;
    }
    return maac_raw_decode_dse(&a->raw, br);
}

MAAC_PUBLIC
MAAC_RESULT
maac_adts_decode(maac_adts* maac_restrict a, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;

    switch(a->state) {
        case MAAC_ADTS_STATE_RAW_DATA_BLOCK: {
            maac_adts_decode_raw_data_block:
            if( (res = maac_raw_decode(&a->raw, br)) !=  MAAC_OK) return res;
            /* maac_raw_decode only returns OK after decoding the END element, so
            check if we have a CRC16 value to read */
            if(a->variable_header.raw_data_blocks && !(a->fixed_header.protection_absent)) {
                a->state = MAAC_ADTS_STATE_RAW_DATA_BLOCK_CRC16;
                goto maac_adts_decode_crc16;
            }
            break;
        }
        case MAAC_ADTS_STATE_RAW_DATA_BLOCK_CRC16: {
            maac_adts_decode_crc16:
            if( (res = maac_bitreader_fill(br, 16)) != MAAC_OK) return res;
            maac_bitreader_discard(br, 16);
            break;
        }
        default: {
            if( (res = maac_adts_sync(a, br)) != MAAC_OK) return res;
            goto maac_adts_decode_raw_data_block;
        }
    }

    /* either way we're at the end of a raw data block so we'll return MAAC_OK, but
    if we have more blocks to read we'll go back to reading, else go back to looking
    for the syncword */
    a->_i++;
    if(a->_i <= a->variable_header.raw_data_blocks) {
        a->state = MAAC_ADTS_STATE_RAW_DATA_BLOCK;
    } else {
        a->state = MAAC_ADTS_STATE_SYNCWORD;
        a->_i = 0;
    }
    return MAAC_OK;
}

#ifdef MAAC_ENABLE_ASSERT

#ifdef __cplusplus
#include <cassert>
#else
#include <assert.h>
#endif

#define maac_assert(x) assert( (x) )

#else

#define maac_assert(x)

#endif


MAAC_PUBLIC
void
maac_bitreader_init(maac_bitreader* br) {
    maac_memset(br, 0, sizeof *br);
}

MAAC_PRIVATE
MAAC_RESULT
maac_bitreader_fill(maac_bitreader* br, maac_u8 bits) {
    maac_u8 byte = 0;
    maac_assert(bits <= 32);

    if(bits == 0) return MAAC_OK;

    while(br->bits < bits && br->pos < br->len) {
        byte = br->data[br->pos++];
        br->val = (br->val << 8) | byte;
        br->bits += 8;
        br->tot++;
    }

    return (MAAC_RESULT) (br->bits >= bits);
}

MAAC_PRIVATE
maac_u32
maac_bitreader_peek(const maac_bitreader* br, maac_u8 bits) {
    uint32_t mask  = MAAC_U32_C(0xFFFFFFFF);
    uint32_t r = 0;
    maac_assert(bits <= 32);

    if(bits == 0) return r;
    mask >>= (32 - bits);

    bits = br->bits - bits;

    r = br->val >> bits & mask;

    return r;
}

MAAC_PRIVATE
void
maac_bitreader_discard(maac_bitreader* br, maac_u8 bits) {
    uint32_t imask = MAAC_U32_C(0xFFFFFFFF);
    maac_assert(bits <= 32);

    if(bits == 0) return;

    br->bits -= bits;
    if(br->bits == 0) {
        imask = 0;
    } else {
        imask >>= (32 - br->bits);
    }
    br->val &= imask;
    return;
}

MAAC_PRIVATE
maac_u32
maac_bitreader_read(maac_bitreader* br, maac_u8 bits) {
    uint32_t r = maac_bitreader_peek(br, bits);
    maac_bitreader_discard(br,bits);
    return r;
}

MAAC_PRIVATE
void
maac_bitreader_byte_align(maac_bitreader* br) {
    maac_bitreader_discard(br, br->bits % 8);
}

MAAC_PUBLIC
void
maac_channel_init(maac_channel *ch) {
    maac_memset(ch, 0, sizeof *ch);
}

maac_const
MAAC_PUBLIC
maac_u32 maac_channel_config_channels(maac_u32 channel_config);

maac_const
MAAC_PUBLIC
maac_u32 maac_channel_config_channels(maac_u32 channel_config) {
    switch(channel_config) {
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        case 4: return 4;
        case 5: return 5;
        case 6: return 6;
        case 7: return 8;
        default: break;
    }
    return 0;
}

struct maac_pns_params {
    maac_flt* spectra;
    const maac_ics* ics;
    maac_u8 sf_index;
    maac_u32* rand_state;
};
typedef struct maac_pns_params maac_pns_params;

MAAC_PRIVATE
void
maac_pns_process(const maac_pns_params* para);


MAAC_PRIVATE
void* maac_memcpy(void* maac_restrict _dest, const void* maac_restrict _src, size_t len);

#define maac_clamp(val, minv, maxv) ( (val) > (maxv) ? (maxv) : (val) < (minv) ? (minv) : (val) )


/* returns 1.0 / sqrt(x) */
MAAC_PRIVATE
maac_flt maac_inv_sqrt(maac_flt x);

/* returns pow(2, x/4.0), used for scaling */
MAAC_PRIVATE
maac_flt maac_pow2_xdiv4(maac_s16 x);

/* returns the cube root of x, used when
   inverse quantizing */
MAAC_PRIVATE
maac_flt maac_cbrt(maac_u16 x);

#define MAAC_M_PI MAAC_FLT_C(3.14159265358979323846)


/* get the default srand seed */

MAAC_PRIVATE
maac_u32
maac_rand_seed(void);

/* classic ranqd1 - quick and dirty rand from
   Numerical Recipes */
static maac_inline maac_u32
maac_rand(maac_u32* s) {
#if 0
    return *s = (*s) * (MAAC_U32_C(1664525) + MAAC_U32_C(1013904223));
#endif
    return *s = (*s) * MAAC_U32_C(1664525) + MAAC_U32_C(1013904223);
}

maac_const
MAAC_PRIVATE
maac_u32 maac_window_group_lengths(maac_u8 window_sequence, maac_u8 scale_factor_grouping);


struct maac_scalefactor_bands {
    const maac_u16* offsets;
    maac_u16 len;
};

typedef struct maac_scalefactor_bands maac_scalefactor_bands;

maac_pure
MAAC_PRIVATE
maac_scalefactor_bands maac_scalefactor_bands_long(maac_u8 sf_index);

maac_pure
MAAC_PRIVATE
maac_scalefactor_bands maac_scalefactor_bands_short(maac_u8 sf_index);

maac_pure
MAAC_PRIVATE
maac_scalefactor_bands maac_scalefactor_bandsf(maac_u8 window_sequence, maac_u8 sf_index);


#if defined(__GNUC__) && __GNUC__ >= 4
#define maac_popcnt(x) __builtin_popcount(x)
#elif defined(_MSC_VER)
#include <intrin.h>
#define maac_popcnt(x) __popcnt(x)
#else


static maac_inline
maac_u32 maac_popcnt(maac_u32 val) {
    maac_u32 c = 0;
    while(val) {
        val &= val - 1;
        ++c;
    }
    return c;
}

#endif


maac_const static maac_inline
maac_u32
maac_sfg_num_window_groups(maac_u8 scale_factor_grouping) {
    return 1 + (7 - maac_popcnt((maac_u32)scale_factor_grouping));
}

/* max is 120 */
#define maac_section_idx(g,i) ( ((MAAC_MAX_SWB_OFFSET_SHORT_WINDOW) * (g)) + (i))

struct maac_filterbank_params {
    maac_u8 window_sequence;
    maac_u8 window_shape;
    maac_u8 window_shape_prev;
};

typedef struct maac_filterbank_params maac_filterbank_params;

MAAC_PRIVATE
void
maac_filterbank(maac_flt* samples, maac_flt* overlap, const maac_filterbank_params* p);

/* returns MAAC_OK just after parsing the element
 * instance tag and prepare for maac_cpe_decode */

MAAC_PRIVATE
MAAC_RESULT
maac_cpe_sync(maac_cpe* maac_restrict s, maac_bitreader* maac_restrict br);


#define maac_cpe_section_idx(g,i) ( ((MAAC_MAX_SWB_OFFSET_SHORT_WINDOW) * (g)) + (i))
#define maac_ms_used_idx(g,i) ( ((MAAC_MAX_SWB_OFFSET_SHORT_WINDOW) * (g)) + (i))


MAAC_PUBLIC
void
maac_cpe_init(maac_cpe* c) {
    /* TODO - remove memset with garbage data once we verify this works correctly with mostly garbage data */
    maac_memset(c, 0xdf, sizeof *c);
    c->state = MAAC_CPE_STATE_TAG;
}

static void maac_cpe_ms(maac_cpe* maac_restrict c, maac_flt* maac_restrict l_samples, maac_flt* maac_restrict r_samples, maac_u8 sf_index) {
    const maac_u8 num_groups = maac_sfg_num_window_groups(c->info.scale_factor_grouping);
    const maac_u8 max_sfb = c->info.max_sfb;
    const maac_scalefactor_bands b = maac_scalefactor_bandsf(c->info.window_sequence, sf_index);

    maac_u32 group_lengths = maac_window_group_lengths(c->info.window_sequence, c->info.scale_factor_grouping);

    maac_u8 cb_l;
    maac_u8 cb_r;
    maac_u8 g;
    maac_u8 w;
    maac_u8 sfb;
    maac_u32 idx;
    maac_u8 section_idx_l;
    maac_u8 section_idx_r;
    maac_u8 group_len;
    maac_u16 i;
    maac_u16 group_off;
    maac_u16 off;
    maac_flt tmp;

    off = 0;
    group_off = 0;

    for(g = 0; g < num_groups; g++) {
        group_len = group_lengths & 0x0f;
        section_idx_l = 0;
        section_idx_r = 0;
        sfb = 0;

        while(sfb < max_sfb) {
            idx = maac_cpe_section_idx(g, sfb);
            if( (c->ms_used[idx/32] >> (idx % 32)) & 0x01) {
                cb_l = c->ics_l.section_data[maac_cpe_section_idx(g, section_idx_l)].codebook;
                cb_r = c->ics_r.section_data[maac_cpe_section_idx(g, section_idx_r)].codebook;

                if(cb_r < MAAC_NOISE_HCB && cb_l < MAAC_NOISE_HCB) {
                    for(w=0 ; w < group_len ; w++) {
                        off = group_off + (w * 128);
                        for(i=b.offsets[sfb];i<b.offsets[sfb+1];i++) {
                            tmp = l_samples[off + i] - r_samples[off + i];
                            l_samples[off + i] = l_samples[off + i] + r_samples[off + i];
                            r_samples[off + i] = tmp;
                        }

                    }
#if 0
                /* TODO Find a sample that actually does this */
                } else if(cb_r == MAAC_NOISE_HCB && cb_l == MAAC_NOISE_HCB) {
                    for(w=0 ; w < group_len ; w++) {
                        off = group_off + (w * 128);
                        for(i=b.offsets[sfb];i<b.offsets[sfb+1];i++) {
                            r_samples[off + i] = l_samples[off + i];
                        }
                    }
#endif
                }
            }
            sfb++;
            if(sfb == c->ics_l.section_data[maac_cpe_section_idx(g, section_idx_l)].end) {
                section_idx_l++;
            }
            if(sfb == c->ics_r.section_data[maac_cpe_section_idx(g, section_idx_r)].end) {
                section_idx_r++;
            }
        }

        group_off += group_len * 128;
        group_lengths >>= 4;
    }
}

/* we specifically want to use ics_r's info since intensity stereo can be used with
or without a common window */
static void maac_cpe_is(maac_cpe* maac_restrict c, maac_flt* maac_restrict l_samples, maac_flt* maac_restrict r_samples, maac_u8 sf_index) {
    const maac_u8 num_groups = maac_sfg_num_window_groups(c->ics_r.info.scale_factor_grouping);
    const maac_u8 max_sfb = c->ics_r.info.max_sfb;
    const maac_scalefactor_bands b = maac_scalefactor_bandsf(c->ics_r.info.window_sequence, sf_index);

    maac_u32 group_lengths = maac_window_group_lengths(c->ics_r.info.window_sequence, c->ics_r.info.scale_factor_grouping);

    maac_u8 cb_r;
    maac_u8 g;
    maac_u8 w;
    maac_u8 sfb;
    maac_u32 idx;
    maac_u8 section_idx_r;
    maac_u8 group_len;
    maac_u16 i;
    maac_u16 group_off;
    maac_u16 off;
    maac_flt tmp;

    off = 0;
    group_off = 0;

    for(g = 0; g < num_groups; g++) {
        group_len = group_lengths & 0x0f;
        section_idx_r = 0;
        sfb = 0;

        while(sfb < max_sfb) {
            cb_r = c->ics_r.section_data[maac_cpe_section_idx(g, section_idx_r)].codebook;

            idx = maac_cpe_section_idx(g, sfb);

            if((cb_r == MAAC_INTENSITY_HCB || cb_r == MAAC_INTENSITY_HCB2)) {
                /* negating the value lets us do pow2(-x/4) instead of pow(0.5,x/4) */
                tmp = maac_pow2_xdiv4(-c->ics_r.scalefactors[maac_cpe_section_idx(g,sfb)]);

                if(cb_r == MAAC_INTENSITY_HCB2) tmp *= -1;
                if(c->ms_mask_present == 1) {
                    tmp *= (((c->ms_used[idx/32] >> (idx % 32)) & 0x01) ? MAAC_FLT_C(-1.0) : MAAC_FLT_C(1.0));
                }

                for(w=0 ; w < group_len ; w++) {
                    off = group_off + (w * 128);
                    for(i=b.offsets[sfb];i<b.offsets[sfb+1];i++) {
                        r_samples[off + i] = l_samples[off + i] * tmp;
                    }

                }
            }
            sfb++;
            if(sfb == c->ics_r.section_data[maac_cpe_section_idx(g, section_idx_r)].end) {
                section_idx_r++;
            }
        }

        group_off += group_len * 128;
        group_lengths >>= 4;
    }
}

static
MAAC_RESULT
maac_cpe_decode_mask_used(maac_cpe* maac_restrict c, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;
    maac_u32 i;
    const maac_u8 num_groups = (maac_u8)maac_sfg_num_window_groups(c->info.scale_factor_grouping);

    while(c->_g < num_groups) {
        while(c->_sfb < c->info.max_sfb) {

            if( (res = maac_bitreader_fill(br,1)) != MAAC_OK) return res;

            i = maac_ms_used_idx( ((maac_u32)c->_g), ((maac_u32)c->_sfb) );
            c->ms_used[i/32] |= maac_bitreader_read(br, 1) << (i % 32);
            c->_sfb++;

        }
        c->_sfb = 0;
        c->_g++;
    }
    return MAAC_OK;
}

MAAC_PRIVATE
MAAC_RESULT
maac_cpe_sync(maac_cpe* maac_restrict s, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;

    /* this function MUST be called with s->state == MAAC_CPE_STATE_TAG, this is
     * checked by cpe_decode/raw_sync so we just assume that's true */
    maac_assert(s->state == MAAC_CPE_STATE_TAG);

    if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
    s->element_instance_tag = (maac_u8)maac_bitreader_read(br, 4);

    s->state = MAAC_CPE_STATE_COMMON_WINDOW;

    return MAAC_OK;
}

MAAC_PUBLIC
MAAC_RESULT
maac_cpe_decode(maac_cpe* maac_restrict c, maac_bitreader* maac_restrict br, const maac_cpe_decode_params* maac_restrict p) {
    MAAC_RESULT res;
    maac_ics_decode_params ics_p;
    maac_filterbank_params fb_p;
    maac_tns_params tns_p;
    maac_pns_params pns_p;

    ics_p.common_window = 0;
    ics_p.sf_index = p->sf_index;
    ics_p.ch = NULL;

    pns_p.sf_index = p->sf_index;
    pns_p.rand_state = p->rand_state;

    switch(c->state) {

        case MAAC_CPE_STATE_TAG: {
            if( (res = maac_cpe_sync(c, br)) != MAAC_OK) return res;
            goto maac_cpe_state_common_window;
        }

        case MAAC_CPE_STATE_COMMON_WINDOW: {
            maac_cpe_state_common_window:
            if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
            c->common_window = maac_bitreader_read(br, 1);

            /* blank out mid-side to fill in (or not fill in) later */
            c->ms_mask_present = 0;
            c->ms_used[0] = 0;
            c->ms_used[1] = 0;
            c->ms_used[2] = 0;
            c->ms_used[3] = 0;

            if(c->common_window) {
                c->state = MAAC_CPE_STATE_ICS_INFO;
                maac_ics_info_init(&c->info);
                goto maac_cpe_state_ics_info;
            }

            c->state = MAAC_CPE_STATE_ICS_LEFT;
            maac_ics_init(&c->ics_l);
            goto maac_cpe_state_ics_left;
        }

        case MAAC_CPE_STATE_ICS_INFO: {
            maac_cpe_state_ics_info:
            if( (res = maac_ics_info_parse(&c->info, br, p->sf_index)) != MAAC_OK) return res;
            c->state = MAAC_CPE_STATE_MS_MASK_PRESENT;
            goto maac_cpe_state_ms_mask_present;
        }

        case MAAC_CPE_STATE_MS_MASK_PRESENT: {
            maac_cpe_state_ms_mask_present:
            if( (res = maac_bitreader_fill(br, 2)) != MAAC_OK) return res;
            c->ms_mask_present = maac_bitreader_read(br, 2);
            switch(c->ms_mask_present) {
                case 0x01: {
                    c->state = MAAC_CPE_STATE_MS_USED;
                    c->_g = 0;
                    c->_sfb = 0;
                    c->ms_used[0] = 0;
                    c->ms_used[1] = 0;
                    c->ms_used[2] = 0;
                    c->ms_used[3] = 0;
                    goto maac_cpe_state_ms_used;
                }
                case 0x02: {
                    c->ms_used[0] = ~0;
                    c->ms_used[1] = ~0;
                    c->ms_used[2] = ~0;
                    c->ms_used[3] = ~0;
                }
                /* fall-through */
                default: break;
            }

            c->state = MAAC_CPE_STATE_ICS_LEFT;
            maac_ics_init(&c->ics_l);
            maac_memcpy(&c->ics_l.info, &c->info, sizeof c->info);
            goto maac_cpe_state_ics_left;
        }

        case MAAC_CPE_STATE_MS_USED: {
            maac_cpe_state_ms_used:
            if( (res = maac_cpe_decode_mask_used(c,br)) != MAAC_OK) return res;

            c->state = MAAC_CPE_STATE_ICS_LEFT;
            maac_ics_init(&c->ics_l);
            maac_memcpy(&c->ics_l.info, &c->info, sizeof c->info);
            goto maac_cpe_state_ics_left;
        }


        case MAAC_CPE_STATE_ICS_LEFT: {
            maac_cpe_state_ics_left:
            ics_p.common_window = c->common_window;
            ics_p.ch = p->l;
            if( (res = maac_ics_decode(&c->ics_l, br, &ics_p)) != MAAC_OK) return res;
            c->state = MAAC_CPE_STATE_ICS_RIGHT;
            maac_ics_init(&c->ics_r);
            if(c->common_window) {
                maac_memcpy(&c->ics_r.info, &c->info, sizeof c->info);
            }
            goto maac_cpe_state_ics_right;
        }

        case MAAC_CPE_STATE_ICS_RIGHT: {
            maac_cpe_state_ics_right:
            ics_p.common_window = c->common_window;
            ics_p.ch = p->r;
            if( (res = maac_ics_decode(&c->ics_r, br, &ics_p)) != MAAC_OK) return res;
            c->state = MAAC_CPE_STATE_TAG;
            break;
        }

        default: MAAC_UNREACHABLE_RETURN(MAAC_UNREACHABLE);
    }

    /* the high-level order of a decoder is:
      1. huffman decoding
      2. inverse quantization
      3. rescaling
      - that's all handled in ics
      4. mid-side
      5. prediction (not used in LC)
      6. intensity stereo - done at same time as mid-side
      7. dependently switched coupling (??)
      8. TNS
      9. dependently switched coupling (??)
      10. filterbank
      11. gain control (not used in LC)
      12. independently switched coupling (I don't know what this is?)
    */

    /* perceptual noise is really kind of partof inverse quantization + rescaling,
    but we want to maintain random state between channels so we do it here,
    plus if there's mid-side flags on a band you're supposed to use the same
    state for both channels (or what I'll probably do is just copy samples over,
    same thing.)

    I don't know if any encoders actually do all of that though? */

    if(! (p->l == NULL || p->r == NULL) ) {
        pns_p.spectra = p->l->samples;
        pns_p.ics = &c->ics_l;
        maac_pns_process(&pns_p);

        pns_p.spectra = p->r->samples;
        pns_p.ics = &c->ics_r;
        maac_pns_process(&pns_p);

        if(c->ms_mask_present != 0) maac_cpe_ms(c, p->l->samples, p->r->samples, p->sf_index);

        maac_cpe_is(c, p->l->samples, p->r->samples, p->sf_index);

        if(c->ics_l.tns_data_present) {
            tns_p.sf_index = p->sf_index;
            tns_p.info = &c->ics_l.info;
            maac_tns_process(&c->ics_l.tns, &p->l->samples[0], &tns_p);
        }

        if(c->ics_r.tns_data_present) {
            tns_p.sf_index = p->sf_index;
            tns_p.info = (c->common_window ? &c->ics_l.info : &c->ics_r.info);
            maac_tns_process(&c->ics_r.tns, &p->r->samples[0], &tns_p);
        }

        fb_p.window_sequence   = c->ics_l.info.window_sequence;
        fb_p.window_shape      = c->ics_l.info.window_shape;
        fb_p.window_shape_prev = p->l->window_shape_prev;
        maac_filterbank(&p->l->samples[0], &p->l->samples[2048], &fb_p);
        p->l->window_shape_prev = fb_p.window_shape;

        if(!c->common_window) {
            fb_p.window_sequence   = c->ics_r.info.window_sequence;
            fb_p.window_shape      = c->ics_r.info.window_shape;
        }
        fb_p.window_shape_prev = p->r->window_shape_prev;
        maac_filterbank(&p->r->samples[0], &p->r->samples[2048], &fb_p);
        p->r->window_shape_prev = fb_p.window_shape;

        p->l->n_samples = 1024;
        p->l->_n = 0;

        p->r->n_samples = 1024;
        p->r->_n = 0;
    }

    return MAAC_OK;
}

/* returns MAAC_OK just after parsing the element
 * instance tag and prepare for maac_dse_decode */

MAAC_PRIVATE
MAAC_RESULT
maac_dse_sync(maac_dse* maac_restrict d, maac_bitreader* maac_restrict br);



MAAC_PUBLIC
void
maac_dse_init(maac_dse* d) {
    d->state = MAAC_DSE_STATE_ELEMENT_INSTANCE_TAG;
}

MAAC_PRIVATE
MAAC_RESULT
maac_dse_sync(maac_dse* maac_restrict d, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;

    /* this function MUST be called with d->state == MAAC_SCE_STATE_ELEMENT_INSTANCE_TAG, this is
     * checked by dse_decode/raw_sync so we just assume that's true */
    maac_assert(d->state == MAAC_DSE_STATE_ELEMENT_INSTANCE_TAG);

    if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
    d->element_instance_tag = (maac_u8)maac_bitreader_read(br, 4);

    d->data_byte_align_flag = 0;
    d->count = 0;
    d->_i = 0;

    d->state = MAAC_DSE_STATE_DATA_BYTE_ALIGN_FLAG;

    return MAAC_OK;
}

MAAC_PUBLIC
MAAC_RESULT
maac_dse_decode(maac_dse* maac_restrict d, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;

    switch(d->state) {
        case MAAC_DSE_STATE_ELEMENT_INSTANCE_TAG: {
            if( (res = maac_dse_sync(d, br)) != MAAC_OK) return res;
            goto maac_dse_state_data_byte_align_flag;
        }

        case MAAC_DSE_STATE_DATA_BYTE_ALIGN_FLAG: {
            maac_dse_state_data_byte_align_flag:
            if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
            d->data_byte_align_flag = maac_bitreader_read(br, 1);
            d->state = MAAC_DSE_STATE_COUNT;
            goto maac_dse_state_count;
        }

        case MAAC_DSE_STATE_COUNT: {
            maac_dse_state_count:
            if( (res = maac_bitreader_fill(br, 8)) != MAAC_OK) return res;
            d->count = (maac_u16)maac_bitreader_read(br, 8);

            if(d->count == 255) {
                d->state = MAAC_DSE_STATE_ESC_COUNT;
                goto maac_dse_state_esc_count;
            }
            if(d->data_byte_align_flag) maac_bitreader_byte_align(br);
            d->state = MAAC_DSE_STATE_DATA_STREAM_BYTE;
            goto maac_dse_state_data_stream_byte;
        }

        case MAAC_DSE_STATE_ESC_COUNT: {
            maac_dse_state_esc_count:
            if( (res = maac_bitreader_fill(br, 8)) != MAAC_OK) return res;
            d->count += (maac_u16)maac_bitreader_read(br, 8);
            if(d->data_byte_align_flag) maac_bitreader_byte_align(br);
            d->state = MAAC_DSE_STATE_DATA_STREAM_BYTE;
            goto maac_dse_state_data_stream_byte;
        }

        case MAAC_DSE_STATE_DATA_STREAM_BYTE: {
            maac_dse_state_data_stream_byte:
            while(d->_i < d->count) {
                if( (res = maac_bitreader_fill(br, 8)) != MAAC_OK) return res;
                d->data_stream_byte[d->_i++] = maac_bitreader_read(br,8);
            }
            d->_i = 0;
            d->state = MAAC_DSE_STATE_ELEMENT_INSTANCE_TAG;
            break;
        }
    }

    return MAAC_OK;
}

/* returns MAAC_OK just after parsing the extension_type */

MAAC_PRIVATE
MAAC_RESULT
maac_fil_sync(maac_fil* maac_restrict s, maac_bitreader* maac_restrict br);



MAAC_PUBLIC
void
maac_fil_init(maac_fil* f) {
    /* TODO - remove memset with garbage data once we verify this works correctly with mostly garbage data */
    maac_memset(f, 0xcc, sizeof *f);
    f->state = MAAC_FIL_STATE_COUNT;
}

MAAC_PRIVATE
MAAC_RESULT
maac_fil_sync(maac_fil* maac_restrict f, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;

    switch(f->state) {
        case MAAC_FIL_STATE_COUNT: {
            if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
            f->count = (maac_u16)maac_bitreader_read(br, 4);
            if(f->count == 15) {
                f->state = MAAC_FIL_STATE_ESC_COUNT;
                goto maac_fil_state_esc_count;
            }
            f->state = MAAC_FIL_STATE_PAYLOAD_TYPE;
            goto maac_fil_state_payload_type;
        }

        case MAAC_FIL_STATE_ESC_COUNT: {
            maac_fil_state_esc_count:
            if( (res = maac_bitreader_fill(br, 8)) != MAAC_OK) return res;
            f->count += (maac_u16)(maac_bitreader_read(br,8) - 1);
            f->state = MAAC_FIL_STATE_PAYLOAD_TYPE;
            goto maac_fil_state_payload_type;
        }

        case MAAC_FIL_STATE_PAYLOAD_TYPE: {
            maac_fil_state_payload_type:
            f->_bits = 0;
            f->extension_type = MAAC_FIL_EXT_FILL;
            if(f->count > 0) {
                if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
                f->extension_type = maac_bitreader_read(br, 4);
                /* TODO SBR? */

                f->_bits = (8 * (f->count-1)) + 4;
            }

            f->state = MAAC_FIL_STATE_PAYLOAD_OTHER_BITS;
            break;
        }

        default: return MAAC_ERROR;
    }

    return MAAC_OK;
}


MAAC_PUBLIC
MAAC_RESULT
maac_fil_decode(maac_fil* maac_restrict f, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;
    maac_u8 bits;

    switch(f->state) {
        case MAAC_FIL_STATE_COUNT: 
        case MAAC_FIL_STATE_ESC_COUNT:
        case MAAC_FIL_STATE_PAYLOAD_TYPE: {
            if( (res = maac_fil_sync(f, br)) != MAAC_OK) return res;
        }
        /* fall-through */
        case MAAC_FIL_STATE_PAYLOAD_OTHER_BITS: {
            maac_fil_state_payload_other_bits:
            if(f->_bits == 0) break;

            bits = f->_bits > 32 ? 32 : f->_bits;
            if( (res = maac_bitreader_fill(br, bits)) != MAAC_OK) return res;
            maac_bitreader_discard(br, bits);
            f->_bits -= bits;
            goto maac_fil_state_payload_other_bits;
        }

        default: MAAC_UNREACHABLE_RETURN(MAAC_UNREACHABLE);
    }

    f->state = MAAC_FIL_STATE_COUNT;
    return MAAC_OK;
}

/* len is total window size, ie - if we have 1024
   coefficients, we'll have 2048 samples get output
   - so 2048 is the value for len */
MAAC_PRIVATE
void
maac_imdct(maac_flt* out, maac_u16 len);


static const maac_flt maac_window_kbd_1024[1024] = {
  MAAC_FLT_C(0.00029256153483765002),
  MAAC_FLT_C(0.00042998567122549665),
  MAAC_FLT_C(0.00054674074276789828),
  MAAC_FLT_C(0.00065482303740835356),
  MAAC_FLT_C(0.00075870194229219531),
  MAAC_FLT_C(0.00086059330630213892),
  MAAC_FLT_C(0.00096177540163038834),
  MAAC_FLT_C(0.0010630609266967137),
  MAAC_FLT_C(0.0011650036147703847),
  MAAC_FLT_C(0.0012680012014550891),
  MAAC_FLT_C(0.0013723517030785803),
  MAAC_FLT_C(0.0014782863882110866),
  MAAC_FLT_C(0.0015859901725111816),
  MAAC_FLT_C(0.0016956147979581308),
  MAAC_FLT_C(0.0018072876615600414),
  MAAC_FLT_C(0.0019211179109736957),
  MAAC_FLT_C(0.0020372007626876698),
  MAAC_FLT_C(0.0021556206295778569),
  MAAC_FLT_C(0.0022764534302540185),
  MAAC_FLT_C(0.0023997683234184999),
  MAAC_FLT_C(0.0025256290301161896),
  MAAC_FLT_C(0.0026540948553966056),
  MAAC_FLT_C(0.002785221487266753),
  MAAC_FLT_C(0.0029190616282906198),
  MAAC_FLT_C(0.0030556654998147356),
  MAAC_FLT_C(0.0031950812481144984),
  MAAC_FLT_C(0.0033373552742114176),
  MAAC_FLT_C(0.0034825325037056462),
  MAAC_FLT_C(0.0036306566090426274),
  MAAC_FLT_C(0.0037817701937467516),
  MAAC_FLT_C(0.0039359149460089747),
  MAAC_FLT_C(0.0040931317674028503),
  MAAC_FLT_C(0.0042534608812798205),
  MAAC_FLT_C(0.0044169419244576317),
  MAAC_FLT_C(0.0045836140250921954),
  MAAC_FLT_C(0.0047535158690599228),
  MAAC_FLT_C(0.0049266857567358624),
  MAAC_FLT_C(0.0051031616517040902),
  MAAC_FLT_C(0.0052829812226595141),
  MAAC_FLT_C(0.005466181879538368),
  MAAC_FLT_C(0.0056528008047362173),
  MAAC_FLT_C(0.0058428749801278301),
  MAAC_FLT_C(0.0060364412104858302),
  MAAC_FLT_C(0.0062335361437989775),
  MAAC_FLT_C(0.006434196288912084),
  MAAC_FLT_C(0.0066384580308444449),
  MAAC_FLT_C(0.0068463576440897927),
  MAAC_FLT_C(0.0070579313041558402),
  MAAC_FLT_C(0.0072732150975640597),
  MAAC_FLT_C(0.0074922450304988628),
  MAAC_FLT_C(0.007715057036268928),
  MAAC_FLT_C(0.0079416869817210672),
  MAAC_FLT_C(0.0081721706727281209),
  MAAC_FLT_C(0.0084065438588562507),
  MAAC_FLT_C(0.0086448422373033608),
  MAAC_FLT_C(0.0088871014561885484),
  MAAC_FLT_C(0.0091333571172625451),
  MAAC_FLT_C(0.0093836447781004001),
  MAAC_FLT_C(0.0096379999538302088),
  MAAC_FLT_C(0.0098964581184452938),
  MAAC_FLT_C(0.010159054705741627),
  MAAC_FLT_C(0.010425825109917471),
  MAAC_FLT_C(0.010696804685867955),
  MAAC_FLT_C(0.010972028749203718),
  MAAC_FLT_C(0.011251532576019406),
  MAAC_FLT_C(0.011535351402435124),
  MAAC_FLT_C(0.011823520423931397),
  MAAC_FLT_C(0.012116074794496046),
  MAAC_FLT_C(0.01241304962559947),
  MAAC_FLT_C(0.012714479985013143),
  MAAC_FLT_C(0.013020400895484618),
  MAAC_FLT_C(0.013330847333281071),
  MAAC_FLT_C(0.013645854226612131),
  MAAC_FLT_C(0.013965456453941847),
  MAAC_FLT_C(0.01428968884219858),
  MAAC_FLT_C(0.014618586164890878),
  MAAC_FLT_C(0.014952183140136589),
  MAAC_FLT_C(0.015290514428611884),
  MAAC_FLT_C(0.015633614631426148),
  MAAC_FLT_C(0.015981518287928344),
  MAAC_FLT_C(0.016334259873449792),
  MAAC_FLT_C(0.016691873796988024),
  MAAC_FLT_C(0.017054394398835927),
  MAAC_FLT_C(0.01742185594816003),
  MAAC_FLT_C(0.01779429264053153),
  MAAC_FLT_C(0.018171738595413323),
  MAAC_FLT_C(0.018554227853606085),
  MAAC_FLT_C(0.018941794374656165),
  MAAC_FLT_C(0.019334472034227956),
  MAAC_FLT_C(0.019732294621443058),
  MAAC_FLT_C(0.020135295836188533),
  MAAC_FLT_C(0.020543509286396301),
  MAAC_FLT_C(0.020956968485295588),
  MAAC_FLT_C(0.021375706848640295),
  MAAC_FLT_C(0.021799757691912905),
  MAAC_FLT_C(0.022229154227506542),
  MAAC_FLT_C(0.022663929561886672),
  MAAC_FLT_C(0.023104116692733812),
  MAAC_FLT_C(0.023549748506068589),
  MAAC_FLT_C(0.024000857773360359),
  MAAC_FLT_C(0.024457477148620566),
  MAAC_FLT_C(0.024919639165481941),
  MAAC_FLT_C(0.025387376234264597),
  MAAC_FLT_C(0.02586072063903001),
  MAAC_FLT_C(0.026339704534623823),
  MAAC_FLT_C(0.026824359943708381),
  MAAC_FLT_C(0.027314718753785883),
  MAAC_FLT_C(0.027810812714212894),
  MAAC_FLT_C(0.02831267343320713),
  MAAC_FLT_C(0.02882033237484713),
  MAAC_FLT_C(0.02933382085606568),
  MAAC_FLT_C(0.029853170043637547),
  MAAC_FLT_C(0.030378410951162334),
  MAAC_FLT_C(0.030909574436042982),
  MAAC_FLT_C(0.031446691196460652),
  MAAC_FLT_C(0.031989791768346522),
  MAAC_FLT_C(0.032538906522351106),
  MAAC_FLT_C(0.033094065660811696),
  MAAC_FLT_C(0.033655299214718465),
  MAAC_FLT_C(0.034222637040679731),
  MAAC_FLT_C(0.034796108817887),
  MAAC_FLT_C(0.035375744045080225),
  MAAC_FLT_C(0.035961572037513809),
  MAAC_FLT_C(0.036553621923923862),
  MAAC_FLT_C(0.037151922643497141),
  MAAC_FLT_C(0.037756502942842232),
  MAAC_FLT_C(0.038367391372963362),
  MAAC_FLT_C(0.038984616286237304),
  MAAC_FLT_C(0.039608205833393868),
  MAAC_FLT_C(0.040238187960500361),
  MAAC_FLT_C(0.040874590405950505),
  MAAC_FLT_C(0.041517440697458149),
  MAAC_FLT_C(0.042166766149056352),
  MAAC_FLT_C(0.042822593858102061),
  MAAC_FLT_C(0.043484950702286911),
  MAAC_FLT_C(0.044153863336654567),
  MAAC_FLT_C(0.044829358190624911),
  MAAC_FLT_C(0.045511461465025542),
  MAAC_FLT_C(0.046200199129130974),
  MAAC_FLT_C(0.046895596917709892),
  MAAC_FLT_C(0.047597680328080823),
  MAAC_FLT_C(0.048306474617176728),
  MAAC_FLT_C(0.049022004798618679),
  MAAC_FLT_C(0.049744295639799235),
  MAAC_FLT_C(0.050473371658975662),
  MAAC_FLT_C(0.051209257122373515),
  MAAC_FLT_C(0.05195197604130089),
  MAAC_FLT_C(0.052701552169273698),
  MAAC_FLT_C(0.053458008999152332),
  MAAC_FLT_C(0.054221369760290086),
  MAAC_FLT_C(0.054991657415693632),
  MAAC_FLT_C(0.055768894659196017),
  MAAC_FLT_C(0.056553103912642379),
  MAAC_FLT_C(0.05734430732308881),
  MAAC_FLT_C(0.058142526760014773),
  MAAC_FLT_C(0.058947783812549233),
  MAAC_FLT_C(0.05976009978671102),
  MAAC_FLT_C(0.06057949570266366),
  MAAC_FLT_C(0.061405992291985019),
  MAAC_FLT_C(0.06223960999495215),
  MAAC_FLT_C(0.063080368957841582),
  MAAC_FLT_C(0.063928289030245472),
  MAAC_FLT_C(0.064783389762403873),
  MAAC_FLT_C(0.065645690402553428),
  MAAC_FLT_C(0.066515209894293006),
  MAAC_FLT_C(0.06739196687396623),
  MAAC_FLT_C(0.068275979668061634),
  MAAC_FLT_C(0.069167266290630441),
  MAAC_FLT_C(0.070065844440722447),
  MAAC_FLT_C(0.07097173149984029),
  MAAC_FLT_C(0.071884944529412367),
  MAAC_FLT_C(0.072805500268284751),
  MAAC_FLT_C(0.073733415130232474),
  MAAC_FLT_C(0.074668705201490285),
  MAAC_FLT_C(0.075611386238303477),
  MAAC_FLT_C(0.076561473664498766),
  MAAC_FLT_C(0.077518982569075814),
  MAAC_FLT_C(0.078483927703819442),
  MAAC_FLT_C(0.079456323480933067),
  MAAC_FLT_C(0.080436183970693437),
  MAAC_FLT_C(0.081423522899127063),
  MAAC_FLT_C(0.082418353645708764),
  MAAC_FLT_C(0.083420689241082258),
  MAAC_FLT_C(0.084430542364803449),
  MAAC_FLT_C(0.085447925343106593),
  MAAC_FLT_C(0.086472850146693425),
  MAAC_FLT_C(0.0875053283885458),
  MAAC_FLT_C(0.088545371321762018),
  MAAC_FLT_C(0.089592989837416967),
  MAAC_FLT_C(0.090648194462446721),
  MAAC_FLT_C(0.09171099535755739),
  MAAC_FLT_C(0.092781402315158884),
  MAAC_FLT_C(0.093859424757323673),
  MAAC_FLT_C(0.094945071733770842),
  MAAC_FLT_C(0.096038351919875697),
  MAAC_FLT_C(0.097139273614705215),
  MAAC_FLT_C(0.098247844739079548),
  MAAC_FLT_C(0.099364072833659889),
  MAAC_FLT_C(0.10048796505706284),
  MAAC_FLT_C(0.1016195281840018),
  MAAC_FLT_C(0.10275876860345523),
  MAAC_FLT_C(0.10390569231686241),
  MAAC_FLT_C(0.10506030493634666),
  MAAC_FLT_C(0.10622261168296653),
  MAAC_FLT_C(0.10739261738499484),
  MAAC_FLT_C(0.10857032647622618),
  MAAC_FLT_C(0.10975574299431295),
  MAAC_FLT_C(0.11094887057913001),
  MAAC_FLT_C(0.11214971247116848),
  MAAC_FLT_C(0.11335827150995861),
  MAAC_FLT_C(0.1145745501325223),
  MAAC_FLT_C(0.11579855037185502),
  MAAC_FLT_C(0.11703027385543785),
  MAAC_FLT_C(0.11826972180377944),
  MAAC_FLT_C(0.11951689502898843),
  MAAC_FLT_C(0.12077179393337624),
  MAAC_FLT_C(0.1220344185080907),
  MAAC_FLT_C(0.12330476833178058),
  MAAC_FLT_C(0.12458284256929127),
  MAAC_FLT_C(0.12586863997039172),
  MAAC_FLT_C(0.12716215886853308),
  MAAC_FLT_C(0.12846339717963884),
  MAAC_FLT_C(0.1297723524009271),
  MAAC_FLT_C(0.13108902160976479),
  MAAC_FLT_C(0.13241340146255434),
  MAAC_FLT_C(0.13374548819365256),
  MAAC_FLT_C(0.13508527761432249),
  MAAC_FLT_C(0.1364327651117179),
  MAAC_FLT_C(0.1377879456479007),
  MAAC_FLT_C(0.13915081375889182),
  MAAC_FLT_C(0.14052136355375505),
  MAAC_FLT_C(0.14189958871371477),
  MAAC_FLT_C(0.14328548249130699),
  MAAC_FLT_C(0.14467903770956439),
  MAAC_FLT_C(0.14608024676123549),
  MAAC_FLT_C(0.14748910160803752),
  MAAC_FLT_C(0.14890559377994395),
  MAAC_FLT_C(0.15032971437450635),
  MAAC_FLT_C(0.15176145405621067),
  MAAC_FLT_C(0.15320080305586831),
  MAAC_FLT_C(0.15464775117004209),
  MAAC_FLT_C(0.15610228776050719),
  MAAC_FLT_C(0.15756440175374695),
  MAAC_FLT_C(0.15903408164048416),
  MAAC_FLT_C(0.16051131547524763),
  MAAC_FLT_C(0.16199609087597422),
  MAAC_FLT_C(0.1634883950236462),
  MAAC_FLT_C(0.16498821466196489),
  MAAC_FLT_C(0.1664955360970593),
  MAAC_FLT_C(0.16801034519723146),
  MAAC_FLT_C(0.16953262739273711),
  MAAC_FLT_C(0.17106236767560304),
  MAAC_FLT_C(0.17259955059948015),
  MAAC_FLT_C(0.17414416027953311),
  MAAC_FLT_C(0.17569618039236623),
  MAAC_FLT_C(0.17725559417598596),
  MAAC_FLT_C(0.17882238442979972),
  MAAC_FLT_C(0.18039653351465154),
  MAAC_FLT_C(0.18197802335289426),
  MAAC_FLT_C(0.18356683542849855),
  MAAC_FLT_C(0.1851629507871988),
  MAAC_FLT_C(0.18676635003667572),
  MAAC_FLT_C(0.18837701334677615),
  MAAC_FLT_C(0.18999492044976971),
  MAAC_FLT_C(0.19162005064064255),
  MAAC_FLT_C(0.19325238277742815),
  MAAC_FLT_C(0.19489189528157541),
  MAAC_FLT_C(0.19653856613835374),
  MAAC_FLT_C(0.19819237289729555),
  MAAC_FLT_C(0.19985329267267601),
  MAAC_FLT_C(0.20152130214402986),
  MAAC_FLT_C(0.20319637755670583),
  MAAC_FLT_C(0.20487849472245828),
  MAAC_FLT_C(0.20656762902007614),
  MAAC_FLT_C(0.20826375539604941),
  MAAC_FLT_C(0.20996684836527277),
  MAAC_FLT_C(0.21167688201178683),
  MAAC_FLT_C(0.21339382998955675),
  MAAC_FLT_C(0.21511766552328807),
  MAAC_FLT_C(0.21684836140928024),
  MAAC_FLT_C(0.21858589001631715),
  MAAC_FLT_C(0.22033022328659546),
  MAAC_FLT_C(0.22208133273669003),
  MAAC_FLT_C(0.2238391894585566),
  MAAC_FLT_C(0.22560376412057223),
  MAAC_FLT_C(0.22737502696861248),
  MAAC_FLT_C(0.22915294782716628),
  MAAC_FLT_C(0.23093749610048789),
  MAAC_FLT_C(0.23272864077378588),
  MAAC_FLT_C(0.23452635041444966),
  MAAC_FLT_C(0.23633059317331273),
  MAAC_FLT_C(0.23814133678595309),
  MAAC_FLT_C(0.23995854857403096),
  MAAC_FLT_C(0.24178219544666285),
  MAAC_FLT_C(0.2436122439018332),
  MAAC_FLT_C(0.24544866002784235),
  MAAC_FLT_C(0.24729140950479139),
  MAAC_FLT_C(0.24914045760610384),
  MAAC_FLT_C(0.25099576920008382),
  MAAC_FLT_C(0.2528573087515108),
  MAAC_FLT_C(0.25472504032327054),
  MAAC_FLT_C(0.25659892757802305),
  MAAC_FLT_C(0.25847893377990594),
  MAAC_FLT_C(0.26036502179627474),
  MAAC_FLT_C(0.26225715409947881),
  MAAC_FLT_C(0.26415529276867328),
  MAAC_FLT_C(0.26605939949166713),
  MAAC_FLT_C(0.26796943556680686),
  MAAC_FLT_C(0.26988536190489554),
  MAAC_FLT_C(0.27180713903114806),
  MAAC_FLT_C(0.273734727087181),
  MAAC_FLT_C(0.27566808583303842),
  MAAC_FLT_C(0.2776071746492525),
  MAAC_FLT_C(0.27955195253893916),
  MAAC_FLT_C(0.2815023781299289),
  MAAC_FLT_C(0.28345840967693203),
  MAAC_FLT_C(0.28542000506373855),
  MAAC_FLT_C(0.28738712180545278),
  MAAC_FLT_C(0.28935971705076202),
  MAAC_FLT_C(0.29133774758423953),
  MAAC_FLT_C(0.29332116982868134),
  MAAC_FLT_C(0.295309939847477),
  MAAC_FLT_C(0.29730401334701434),
  MAAC_FLT_C(0.29930334567911671),
  MAAC_FLT_C(0.30130789184351503),
  MAAC_FLT_C(0.30331760649035167),
  MAAC_FLT_C(0.30533244392271808),
  MAAC_FLT_C(0.30735235809922501),
  MAAC_FLT_C(0.30937730263660523),
  MAAC_FLT_C(0.31140723081234867),
  MAAC_FLT_C(0.31344209556737029),
  MAAC_FLT_C(0.31548184950870917),
  MAAC_FLT_C(0.31752644491225984),
  MAAC_FLT_C(0.3195758337255355),
  MAAC_FLT_C(0.32162996757046214),
  MAAC_FLT_C(0.32368879774620435),
  MAAC_FLT_C(0.3257522752320216),
  MAAC_FLT_C(0.3278203506901558),
  MAAC_FLT_C(0.32989297446874921),
  MAAC_FLT_C(0.33197009660479243),
  MAAC_FLT_C(0.33405166682710308),
  MAAC_FLT_C(0.33613763455933376),
  MAAC_FLT_C(0.33822794892300995),
  MAAC_FLT_C(0.34032255874059736),
  MAAC_FLT_C(0.34242141253859815),
  MAAC_FLT_C(0.34452445855067654),
  MAAC_FLT_C(0.34663164472081248),
  MAAC_FLT_C(0.34874291870648422),
  MAAC_FLT_C(0.35085822788187893),
  MAAC_FLT_C(0.35297751934113097),
  MAAC_FLT_C(0.35510073990158841),
  MAAC_FLT_C(0.35722783610710612),
  MAAC_FLT_C(0.35935875423136654),
  MAAC_FLT_C(0.36149344028122726),
  MAAC_FLT_C(0.36363184000009502),
  MAAC_FLT_C(0.36577389887132589),
  MAAC_FLT_C(0.36791956212165189),
  MAAC_FLT_C(0.37006877472463295),
  MAAC_FLT_C(0.37222148140413452),
  MAAC_FLT_C(0.37437762663783031),
  MAAC_FLT_C(0.37653715466072979),
  MAAC_FLT_C(0.3787000094687305),
  MAAC_FLT_C(0.3808661348221945),
  MAAC_FLT_C(0.38303547424954887),
  MAAC_FLT_C(0.38520797105090965),
  MAAC_FLT_C(0.38738356830172982),
  MAAC_FLT_C(0.38956220885646947),
  MAAC_FLT_C(0.39174383535228929),
  MAAC_FLT_C(0.39392839021276654),
  MAAC_FLT_C(0.3961158156516329),
  MAAC_FLT_C(0.39830605367653427),
  MAAC_FLT_C(0.40049904609281212),
  MAAC_FLT_C(0.40269473450730575),
  MAAC_FLT_C(0.40489306033217587),
  MAAC_FLT_C(0.40709396478874821),
  MAAC_FLT_C(0.40929738891137751),
  MAAC_FLT_C(0.41150327355133182),
  MAAC_FLT_C(0.41371155938069509),
  MAAC_FLT_C(0.41592218689629057),
  MAAC_FLT_C(0.41813509642362179),
  MAAC_FLT_C(0.4203502281208325),
  MAAC_FLT_C(0.42256752198268482),
  MAAC_FLT_C(0.42478691784455508),
  MAAC_FLT_C(0.42700835538644721),
  MAAC_FLT_C(0.42923177413702318),
  MAAC_FLT_C(0.43145711347764998),
  MAAC_FLT_C(0.43368431264646351),
  MAAC_FLT_C(0.43591331074244805),
  MAAC_FLT_C(0.4381440467295315),
  MAAC_FLT_C(0.44037645944069603),
  MAAC_FLT_C(0.44261048758210381),
  MAAC_FLT_C(0.44484606973723689),
  MAAC_FLT_C(0.44708314437105168),
  MAAC_FLT_C(0.44932164983414707),
  MAAC_FLT_C(0.4515615243669463),
  MAAC_FLT_C(0.45380270610389151),
  MAAC_FLT_C(0.45604513307765127),
  MAAC_FLT_C(0.45828874322334046),
  MAAC_FLT_C(0.46053347438275211),
  MAAC_FLT_C(0.46277926430860061),
  MAAC_FLT_C(0.46502605066877645),
  MAAC_FLT_C(0.46727377105061163),
  MAAC_FLT_C(0.46952236296515526),
  MAAC_FLT_C(0.47177176385145969),
  MAAC_FLT_C(0.47402191108087588),
  MAAC_FLT_C(0.4762727419613581),
  MAAC_FLT_C(0.4785241937417779),
  MAAC_FLT_C(0.48077620361624596),
  MAAC_FLT_C(0.48302870872844234),
  MAAC_FLT_C(0.48528164617595437),
  MAAC_FLT_C(0.48753495301462207),
  MAAC_FLT_C(0.48978856626288991),
  MAAC_FLT_C(0.49204242290616551),
  MAAC_FLT_C(0.49429645990118443),
  MAAC_FLT_C(0.49655061418038021),
  MAAC_FLT_C(0.4988048226562603),
  MAAC_FLT_C(0.50105902222578647),
  MAAC_FLT_C(0.50331314977475961),
  MAAC_FLT_C(0.50556714218220933),
  MAAC_FLT_C(0.50782093632478575),
  MAAC_FLT_C(0.5100744690811565),
  MAAC_FLT_C(0.51232767733640461),
  MAAC_FLT_C(0.51458049798643068),
  MAAC_FLT_C(0.51683286794235517),
  MAAC_FLT_C(0.51908472413492401),
  MAAC_FLT_C(0.5213360035189144),
  MAAC_FLT_C(0.52358664307754166),
  MAAC_FLT_C(0.52583657982686638),
  MAAC_FLT_C(0.52808575082020204),
  MAAC_FLT_C(0.53033409315252111),
  MAAC_FLT_C(0.53258154396486146),
  MAAC_FLT_C(0.53482804044873078),
  MAAC_FLT_C(0.53707351985050911),
  MAAC_FLT_C(0.53931791947585039),
  MAAC_FLT_C(0.54156117669407977),
  MAAC_FLT_C(0.54380322894258959),
  MAAC_FLT_C(0.5460440137312309),
  MAAC_FLT_C(0.54828346864670197),
  MAAC_FLT_C(0.55052153135693149),
  MAAC_FLT_C(0.55275813961545872),
  MAAC_FLT_C(0.55499323126580735),
  MAAC_FLT_C(0.55722674424585383),
  MAAC_FLT_C(0.55945861659219143),
  MAAC_FLT_C(0.56168878644448583),
  MAAC_FLT_C(0.56391719204982527),
  MAAC_FLT_C(0.56614377176706376),
  MAAC_FLT_C(0.56836846407115615),
  MAAC_FLT_C(0.57059120755748494),
  MAAC_FLT_C(0.57281194094617982),
  MAAC_FLT_C(0.57503060308642739),
  MAAC_FLT_C(0.57724713296077212),
  MAAC_FLT_C(0.57946146968940793),
  MAAC_FLT_C(0.58167355253445885),
  MAAC_FLT_C(0.58388332090425021),
  MAAC_FLT_C(0.58609071435756821),
  MAAC_FLT_C(0.58829567260790883),
  MAAC_FLT_C(0.59049813552771457),
  MAAC_FLT_C(0.592698043152599),
  MAAC_FLT_C(0.59489533568555941),
  MAAC_FLT_C(0.59708995350117577),
  MAAC_FLT_C(0.5992818371497971),
  MAAC_FLT_C(0.60147092736171281),
  MAAC_FLT_C(0.60365716505131206),
  MAAC_FLT_C(0.60584049132122586),
  MAAC_FLT_C(0.60802084746645635),
  MAAC_FLT_C(0.61019817497848949),
  MAAC_FLT_C(0.61237241554939248),
  MAAC_FLT_C(0.61454351107589511),
  MAAC_FLT_C(0.61671140366345345),
  MAAC_FLT_C(0.61887603563029814),
  MAAC_FLT_C(0.62103734951146372),
  MAAC_FLT_C(0.62319528806280156),
  MAAC_FLT_C(0.62534979426497317),
  MAAC_FLT_C(0.62750081132742641),
  MAAC_FLT_C(0.62964828269235107),
  MAAC_FLT_C(0.63179215203861649),
  MAAC_FLT_C(0.63393236328568858),
  MAAC_FLT_C(0.6360688605975271),
  MAAC_FLT_C(0.63820158838646168),
  MAAC_FLT_C(0.64033049131704789),
  MAAC_FLT_C(0.642455514309901),
  MAAC_FLT_C(0.64457660254550875),
  MAAC_FLT_C(0.6466937014680213),
  MAAC_FLT_C(0.64880675678901967),
  MAAC_FLT_C(0.65091571449125996),
  MAAC_FLT_C(0.6530205208323957),
  MAAC_FLT_C(0.6551211223486757),
  MAAC_FLT_C(0.65721746585861796),
  MAAC_FLT_C(0.65930949846665954),
  MAAC_FLT_C(0.66139716756678124),
  MAAC_FLT_C(0.66348042084610803),
  MAAC_FLT_C(0.66555920628848253),
  MAAC_FLT_C(0.66763347217801428),
  MAAC_FLT_C(0.66970316710260136),
  MAAC_FLT_C(0.67176823995742663),
  MAAC_FLT_C(0.6738286399484259),
  MAAC_FLT_C(0.67588431659573001),
  MAAC_FLT_C(0.67793521973707838),
  MAAC_FLT_C(0.67998129953120445),
  MAAC_FLT_C(0.68202250646119389),
  MAAC_FLT_C(0.684058791337813),
  MAAC_FLT_C(0.68609010530280812),
  MAAC_FLT_C(0.68811639983217698),
  MAAC_FLT_C(0.69013762673940837),
  MAAC_FLT_C(0.69215373817869341),
  MAAC_FLT_C(0.69416468664810516),
  MAAC_FLT_C(0.69617042499274895),
  MAAC_FLT_C(0.69817090640788015),
  MAAC_FLT_C(0.70016608444199147),
  MAAC_FLT_C(0.70215591299986857),
  MAAC_FLT_C(0.70414034634561318),
  MAAC_FLT_C(0.70611933910563407),
  MAAC_FLT_C(0.70809284627160585),
  MAAC_FLT_C(0.71006082320339392),
  MAAC_FLT_C(0.71202322563194709),
  MAAC_FLT_C(0.71398000966215547),
  MAAC_FLT_C(0.71593113177567624),
  MAAC_FLT_C(0.71787654883372309),
  MAAC_FLT_C(0.71981621807982321),
  MAAC_FLT_C(0.72175009714253757),
  MAAC_FLT_C(0.72367814403814801),
  MAAC_FLT_C(0.72560031717330786),
  MAAC_FLT_C(0.72751657534765701),
  MAAC_FLT_C(0.72942687775640136),
  MAAC_FLT_C(0.73133118399285579),
  MAAC_FLT_C(0.73322945405095119),
  MAAC_FLT_C(0.73512164832770421),
  MAAC_FLT_C(0.73700772762565014),
  MAAC_FLT_C(0.73888765315523863),
  MAAC_FLT_C(0.74076138653719181),
  MAAC_FLT_C(0.74262888980482489),
  MAAC_FLT_C(0.7444901254063282),
  MAAC_FLT_C(0.74634505620701119),
  MAAC_FLT_C(0.74819364549150846),
  MAAC_FLT_C(0.75003585696594577),
  MAAC_FLT_C(0.75187165476006834),
  MAAC_FLT_C(0.75370100342932933),
  MAAC_FLT_C(0.75552386795693816),
  MAAC_FLT_C(0.75734021375587091),
  MAAC_FLT_C(0.75915000667083854),
  MAAC_FLT_C(0.76095321298021701),
  MAAC_FLT_C(0.76274979939793586),
  MAAC_FLT_C(0.76453973307532608),
  MAAC_FLT_C(0.76632298160292833),
  MAAC_FLT_C(0.76809951301225909),
  MAAC_FLT_C(0.76986929577753582),
  MAAC_FLT_C(0.77163229881736106),
  MAAC_FLT_C(0.77338849149636513),
  MAAC_FLT_C(0.77513784362680638),
  MAAC_FLT_C(0.77688032547012975),
  MAAC_FLT_C(0.77861590773848388),
  MAAC_FLT_C(0.7803445615961947),
  MAAC_FLT_C(0.78206625866119772),
  MAAC_FLT_C(0.78378097100642707),
  MAAC_FLT_C(0.78548867116116161),
  MAAC_FLT_C(0.78718933211232889),
  MAAC_FLT_C(0.78888292730576492),
  MAAC_FLT_C(0.79056943064743135),
  MAAC_FLT_C(0.79224881650458923),
  MAAC_FLT_C(0.7939210597069295),
  MAAC_FLT_C(0.79558613554765845),
  MAAC_FLT_C(0.79724401978454162),
  MAAC_FLT_C(0.7988946886409013),
  MAAC_FLT_C(0.80053811880657233),
  MAAC_FLT_C(0.8021742874388117),
  MAAC_FLT_C(0.80380317216316532),
  MAAC_FLT_C(0.80542475107428968),
  MAAC_FLT_C(0.80703900273672924),
  MAAC_FLT_C(0.80864590618564913),
  MAAC_FLT_C(0.81024544092752349),
  MAAC_FLT_C(0.81183758694077846),
  MAAC_FLT_C(0.81342232467639075),
  MAAC_FLT_C(0.81499963505844153),
  MAAC_FLT_C(0.81656949948462454),
  MAAC_FLT_C(0.81813189982670964),
  MAAC_FLT_C(0.81968681843096136),
  MAAC_FLT_C(0.82123423811851193),
  MAAC_FLT_C(0.82277414218568901),
  MAAC_FLT_C(0.8243065144042987),
  MAAC_FLT_C(0.82583133902186234),
  MAAC_FLT_C(0.82734860076180849),
  MAAC_FLT_C(0.82885828482361978),
  MAAC_FLT_C(0.83036037688293352),
  MAAC_FLT_C(0.83185486309159717),
  MAAC_FLT_C(0.83334173007767864),
  MAAC_FLT_C(0.83482096494543112),
  MAAC_FLT_C(0.83629255527521151),
  MAAC_FLT_C(0.83775648912335443),
  MAAC_FLT_C(0.83921275502199999),
  MAAC_FLT_C(0.84066134197887654),
  MAAC_FLT_C(0.84210223947703766),
  MAAC_FLT_C(0.84353543747455362),
  MAAC_FLT_C(0.84496092640415776),
  MAAC_FLT_C(0.84637869717284708),
  MAAC_FLT_C(0.84778874116143743),
  MAAC_FLT_C(0.8491910502240736),
  MAAC_FLT_C(0.85058561668769428),
  MAAC_FLT_C(0.85197243335145101),
  MAAC_FLT_C(0.85335149348608275),
  MAAC_FLT_C(0.85472279083324509),
  MAAC_FLT_C(0.85608631960479409),
  MAAC_FLT_C(0.85744207448202503),
  MAAC_FLT_C(0.85879005061486724),
  MAAC_FLT_C(0.86013024362103319),
  MAAC_FLT_C(0.86146264958512231),
  MAAC_FLT_C(0.86278726505768222),
  MAAC_FLT_C(0.8641040870542227),
  MAAC_FLT_C(0.86541311305418767),
  MAAC_FLT_C(0.86671434099988098),
  MAAC_FLT_C(0.86800776929534929),
  MAAC_FLT_C(0.86929339680522022),
  MAAC_FLT_C(0.87057122285349597),
  MAAC_FLT_C(0.87184124722230461),
  MAAC_FLT_C(0.87310347015060674),
  MAAC_FLT_C(0.87435789233285766),
  MAAC_FLT_C(0.87560451491762803),
  MAAC_FLT_C(0.87684333950618065),
  MAAC_FLT_C(0.8780743681510027),
  MAAC_FLT_C(0.87929760335429719),
  MAAC_FLT_C(0.88051304806643016),
  MAAC_FLT_C(0.88172070568433614),
  MAAC_FLT_C(0.88292058004988094),
  MAAC_FLT_C(0.88411267544818162),
  MAAC_FLT_C(0.88529699660588579),
  MAAC_FLT_C(0.88647354868940775),
  MAAC_FLT_C(0.8876423373031237),
  MAAC_FLT_C(0.88880336848752572),
  MAAC_FLT_C(0.88995664871733426),
  MAAC_FLT_C(0.89110218489956938),
  MAAC_FLT_C(0.89223998437158214),
  MAAC_FLT_C(0.8933700548990442),
  MAAC_FLT_C(0.89449240467389912),
  MAAC_FLT_C(0.89560704231227128),
  MAAC_FLT_C(0.89671397685233689),
  MAAC_FLT_C(0.89781321775215484),
  MAAC_FLT_C(0.8989047748874579),
  MAAC_FLT_C(0.8999886585494058),
  MAAC_FLT_C(0.9010648794422994),
  MAAC_FLT_C(0.90213344868125545),
  MAAC_FLT_C(0.90319437778984468),
  MAAC_FLT_C(0.90424767869769096),
  MAAC_FLT_C(0.90529336373803393),
  MAAC_FLT_C(0.90633144564525303),
  MAAC_FLT_C(0.90736193755235572),
  MAAC_FLT_C(0.90838485298842886),
  MAAC_FLT_C(0.9094002058760533),
  MAAC_FLT_C(0.91040801052868303),
  MAAC_FLT_C(0.91140828164798837),
  MAAC_FLT_C(0.91240103432116382),
  MAAC_FLT_C(0.91338628401820143),
  MAAC_FLT_C(0.91436404658912862),
  MAAC_FLT_C(0.91533433826121247),
  MAAC_FLT_C(0.91629717563612989),
  MAAC_FLT_C(0.91725257568710428),
  MAAC_FLT_C(0.91820055575600834),
  MAAC_FLT_C(0.91914113355043547),
  MAAC_FLT_C(0.92007432714073678),
  MAAC_FLT_C(0.92100015495702803),
  MAAC_FLT_C(0.92191863578616318),
  MAAC_FLT_C(0.92282978876867794),
  MAAC_FLT_C(0.92373363339570158),
  MAAC_FLT_C(0.92463018950583875),
  MAAC_FLT_C(0.92551947728202055),
  MAAC_FLT_C(0.92640151724832731),
  MAAC_FLT_C(0.92727633026678002),
  MAAC_FLT_C(0.92814393753410507),
  MAAC_FLT_C(0.92900436057846902),
  MAAC_FLT_C(0.92985762125618621),
  MAAC_FLT_C(0.93070374174839876),
  MAAC_FLT_C(0.93154274455772912),
  MAAC_FLT_C(0.93237465250490592),
  MAAC_FLT_C(0.9331994887253644),
  MAAC_FLT_C(0.93401727666582046),
  MAAC_FLT_C(0.93482804008081921),
  MAAC_FLT_C(0.93563180302925941),
  MAAC_FLT_C(0.93642858987089284),
  MAAC_FLT_C(0.93721842526280075),
  MAAC_FLT_C(0.9380013341558453),
  MAAC_FLT_C(0.93877734179110039),
  MAAC_FLT_C(0.93954647369625754),
  MAAC_FLT_C(0.94030875568201211),
  MAAC_FLT_C(0.94106421383842609),
  MAAC_FLT_C(0.94181287453127172),
  MAAC_FLT_C(0.94255476439835328),
  MAAC_FLT_C(0.94328991034580978),
  MAAC_FLT_C(0.94401833954439796),
  MAAC_FLT_C(0.94474007942575633),
  MAAC_FLT_C(0.94545515767865074),
  MAAC_FLT_C(0.94616360224520268),
  MAAC_FLT_C(0.94686544131709904),
  MAAC_FLT_C(0.94756070333178666),
  MAAC_FLT_C(0.94824941696864906),
  MAAC_FLT_C(0.94893161114516855),
  MAAC_FLT_C(0.94960731501307249),
  MAAC_FLT_C(0.95027655795446508),
  MAAC_FLT_C(0.95093936957794511),
  MAAC_FLT_C(0.9515957797147101),
  MAAC_FLT_C(0.95224581841464728),
  MAAC_FLT_C(0.95288951594241256),
  MAAC_FLT_C(0.95352690277349694),
  MAAC_FLT_C(0.95415800959028174),
  MAAC_FLT_C(0.95478286727808404),
  MAAC_FLT_C(0.95540150692118997),
  MAAC_FLT_C(0.95601395979888015),
  MAAC_FLT_C(0.95662025738144552),
  MAAC_FLT_C(0.95722043132619361),
  MAAC_FLT_C(0.95781451347344848),
  MAAC_FLT_C(0.95840253584254276),
  MAAC_FLT_C(0.95898453062780198),
  MAAC_FLT_C(0.95956053019452381),
  MAAC_FLT_C(0.96013056707495148),
  MAAC_FLT_C(0.96069467396424169),
  MAAC_FLT_C(0.96125288371642847),
  MAAC_FLT_C(0.96180522934038348),
  MAAC_FLT_C(0.96235174399577172),
  MAAC_FLT_C(0.96289246098900616),
  MAAC_FLT_C(0.96342741376919938),
  MAAC_FLT_C(0.96395663592411385),
  MAAC_FLT_C(0.96448016117611124),
  MAAC_FLT_C(0.9649980233781017),
  MAAC_FLT_C(0.96551025650949263),
  MAAC_FLT_C(0.9660168946721398),
  MAAC_FLT_C(0.96651797208629775),
  MAAC_FLT_C(0.96701352308657462),
  MAAC_FLT_C(0.96750358211788756),
  MAAC_FLT_C(0.96798818373142292),
  MAAC_FLT_C(0.96846736258059929),
  MAAC_FLT_C(0.96894115341703513),
  MAAC_FLT_C(0.9694095910865218),
  MAAC_FLT_C(0.96987271052500212),
  MAAC_FLT_C(0.97033054675455443),
  MAAC_FLT_C(0.97078313487938439),
  MAAC_FLT_C(0.97123051008182404),
  MAAC_FLT_C(0.97167270761833746),
  MAAC_FLT_C(0.97210976281553696),
  MAAC_FLT_C(0.97254171106620757),
  MAAC_FLT_C(0.97296858782534079),
  MAAC_FLT_C(0.97339042860617897),
  MAAC_FLT_C(0.97380726897627101),
  MAAC_FLT_C(0.97421914455353942),
  MAAC_FLT_C(0.97462609100235831),
  MAAC_FLT_C(0.9750281440296461),
  MAAC_FLT_C(0.97542533938096965),
  MAAC_FLT_C(0.97581771283666263),
  MAAC_FLT_C(0.97620530020795881),
  MAAC_FLT_C(0.97658813733313987),
  MAAC_FLT_C(0.97696626007369824),
  MAAC_FLT_C(0.97733970431051709),
  MAAC_FLT_C(0.97770850594006564),
  MAAC_FLT_C(0.97807270087061282),
  MAAC_FLT_C(0.97843232501845778),
  MAAC_FLT_C(0.97878741430417893),
  MAAC_FLT_C(0.9791380046489021),
  MAAC_FLT_C(0.97948413197058759),
  MAAC_FLT_C(0.97982583218033736),
  MAAC_FLT_C(0.98016314117872239),
  MAAC_FLT_C(0.98049609485213185),
  MAAC_FLT_C(0.98082472906914231),
  MAAC_FLT_C(0.9811490796769099),
  MAAC_FLT_C(0.98146918249758541),
  MAAC_FLT_C(0.98178507332475107),
  MAAC_FLT_C(0.98209678791988197),
  MAAC_FLT_C(0.98240436200883141),
  MAAC_FLT_C(0.98270783127834005),
  MAAC_FLT_C(0.98300723137257118),
  MAAC_FLT_C(0.98330259788967023),
  MAAC_FLT_C(0.98359396637835173),
  MAAC_FLT_C(0.98388137233451167),
  MAAC_FLT_C(0.98416485119786679),
  MAAC_FLT_C(0.98444443834862216),
  MAAC_FLT_C(0.98472016910416604),
  MAAC_FLT_C(0.98499207871579286),
  MAAC_FLT_C(0.98526020236545531),
  MAAC_FLT_C(0.98552457516254577),
  MAAC_FLT_C(0.98578523214070735),
  MAAC_FLT_C(0.98604220825467404),
  MAAC_FLT_C(0.98629553837714312),
  MAAC_FLT_C(0.98654525729567688),
  MAAC_FLT_C(0.98679139970963692),
  MAAC_FLT_C(0.98703400022714849),
  MAAC_FLT_C(0.98727309336209956),
  MAAC_FLT_C(0.98750871353116931),
  MAAC_FLT_C(0.98774089505089191),
  MAAC_FLT_C(0.98796967213475229),
  MAAC_FLT_C(0.98819507889031621),
  MAAC_FLT_C(0.98841714931639357),
  MAAC_FLT_C(0.98863591730023681),
  MAAC_FLT_C(0.98885141661477316),
  MAAC_FLT_C(0.98906368091587271),
  MAAC_FLT_C(0.98927274373965135),
  MAAC_FLT_C(0.98947863849980899),
  MAAC_FLT_C(0.9896813984850048),
  MAAC_FLT_C(0.98988105685626693),
  MAAC_FLT_C(0.99007764664444109),
  MAAC_FLT_C(0.99027120074767383),
  MAAC_FLT_C(0.99046175192893371),
  MAAC_FLT_C(0.99064933281357048),
  MAAC_FLT_C(0.99083397588691058),
  MAAC_FLT_C(0.99101571349189166),
  MAAC_FLT_C(0.99119457782673426),
  MAAC_FLT_C(0.99137060094265261),
  MAAC_FLT_C(0.99154381474160336),
  MAAC_FLT_C(0.99171425097407362),
  MAAC_FLT_C(0.99188194123690721),
  MAAC_FLT_C(0.99204691697117109),
  MAAC_FLT_C(0.99220920946005975),
  MAAC_FLT_C(0.99236884982684115),
  MAAC_FLT_C(0.99252586903283979),
  MAAC_FLT_C(0.99268029787546186),
  MAAC_FLT_C(0.99283216698625931),
  MAAC_FLT_C(0.99298150682903363),
  MAAC_FLT_C(0.99312834769798142),
  MAAC_FLT_C(0.99327271971587872),
  MAAC_FLT_C(0.99341465283230634),
  MAAC_FLT_C(0.99355417682191649),
  MAAC_FLT_C(0.99369132128273907),
  MAAC_FLT_C(0.99382611563452938),
  MAAC_FLT_C(0.99395858911715629),
  MAAC_FLT_C(0.99408877078903135),
  MAAC_FLT_C(0.99421668952557918),
  MAAC_FLT_C(0.99434237401774817),
  MAAC_FLT_C(0.99446585277056332),
  MAAC_FLT_C(0.99458715410171838),
  MAAC_FLT_C(0.99470630614021105),
  MAAC_FLT_C(0.99482333682501767),
  MAAC_FLT_C(0.99493827390380973),
  MAAC_FLT_C(0.99505114493171098),
  MAAC_FLT_C(0.99516197727009625),
  MAAC_FLT_C(0.99527079808543029),
  MAAC_FLT_C(0.9953776343481483),
  MAAC_FLT_C(0.99548251283157707),
  MAAC_FLT_C(0.99558546011089688),
  MAAC_FLT_C(0.99568650256214408),
  MAAC_FLT_C(0.99578566636125465),
  MAAC_FLT_C(0.99588297748314802),
  MAAC_FLT_C(0.99597846170085147),
  MAAC_FLT_C(0.99607214458466475),
  MAAC_FLT_C(0.99616405150136522),
  MAAC_FLT_C(0.99625420761345318),
  MAAC_FLT_C(0.99634263787843624),
  MAAC_FLT_C(0.9964293670481551),
  MAAC_FLT_C(0.99651441966814736),
  MAAC_FLT_C(0.99659782007705233),
  MAAC_FLT_C(0.99667959240605353),
  MAAC_FLT_C(0.99675976057836246),
  MAAC_FLT_C(0.99683834830873908),
  MAAC_FLT_C(0.99691537910305272),
  MAAC_FLT_C(0.99699087625788074),
  MAAC_FLT_C(0.99706486286014606),
  MAAC_FLT_C(0.99713736178679202),
  MAAC_FLT_C(0.99720839570449604),
  MAAC_FLT_C(0.99727798706942017),
  MAAC_FLT_C(0.99734615812699978),
  MAAC_FLT_C(0.99741293091176886),
  MAAC_FLT_C(0.99747832724722219),
  MAAC_FLT_C(0.997542368745714),
  MAAC_FLT_C(0.9976050768083945),
  MAAC_FLT_C(0.99766647262517949),
  MAAC_FLT_C(0.99772657717475877),
  MAAC_FLT_C(0.99778541122463782),
  MAAC_FLT_C(0.99784299533121623),
  MAAC_FLT_C(0.99789934983989947),
  MAAC_FLT_C(0.99795449488524623),
  MAAC_FLT_C(0.99800845039114983),
  MAAC_FLT_C(0.99806123607105279),
  MAAC_FLT_C(0.99811287142819582),
  MAAC_FLT_C(0.9981633757558992),
  MAAC_FLT_C(0.9982127681378774),
  MAAC_FLT_C(0.99826106744858611),
  MAAC_FLT_C(0.99830829235360075),
  MAAC_FLT_C(0.99835446131002814),
  MAAC_FLT_C(0.99839959256694777),
  MAAC_FLT_C(0.99844370416588479),
  MAAC_FLT_C(0.998486813941314),
  MAAC_FLT_C(0.99852893952119337),
  MAAC_FLT_C(0.99857009832752741),
  MAAC_FLT_C(0.99861030757696001),
  MAAC_FLT_C(0.99864958428139616),
  MAAC_FLT_C(0.99868794524865268),
  MAAC_FLT_C(0.99872540708313629),
  MAAC_FLT_C(0.99876198618654954),
  MAAC_FLT_C(0.99879769875862512),
  MAAC_FLT_C(0.99883256079788529),
  MAAC_FLT_C(0.99886658810242879),
  MAAC_FLT_C(0.99889979627074355),
  MAAC_FLT_C(0.99893220070254518),
  MAAC_FLT_C(0.99896381659963984),
  MAAC_FLT_C(0.9989946589668125),
  MAAC_FLT_C(0.99902474261273899),
  MAAC_FLT_C(0.99905408215092184),
  MAAC_FLT_C(0.99908269200064981),
  MAAC_FLT_C(0.99911058638797945),
  MAAC_FLT_C(0.99913777934673964),
  MAAC_FLT_C(0.99916428471955854),
  MAAC_FLT_C(0.99919011615891029),
  MAAC_FLT_C(0.99921528712818486),
  MAAC_FLT_C(0.99923981090277636),
  MAAC_FLT_C(0.99926370057119307),
  MAAC_FLT_C(0.99928696903618652),
  MAAC_FLT_C(0.99930962901589859),
  MAAC_FLT_C(0.999331693045029),
  MAAC_FLT_C(0.99935317347601982),
  MAAC_FLT_C(0.99937408248025839),
  MAAC_FLT_C(0.99939443204929601),
  MAAC_FLT_C(0.99941423399608587),
  MAAC_FLT_C(0.99943349995623465),
  MAAC_FLT_C(0.99945224138927169),
  MAAC_FLT_C(0.99947046957993257),
  MAAC_FLT_C(0.99948819563945768),
  MAAC_FLT_C(0.99950543050690599),
  MAAC_FLT_C(0.99952218495048106),
  MAAC_FLT_C(0.99953846956887227),
  MAAC_FLT_C(0.99955429479260738),
  MAAC_FLT_C(0.99956967088541915),
  MAAC_FLT_C(0.99958460794562276),
  MAAC_FLT_C(0.99959911590750494),
  MAAC_FLT_C(0.99961320454272506),
  MAAC_FLT_C(0.99962688346172568),
  MAAC_FLT_C(0.99964016211515394),
  MAAC_FLT_C(0.99965304979529246),
  MAAC_FLT_C(0.9996655556374997),
  MAAC_FLT_C(0.99967768862165807),
  MAAC_FLT_C(0.99968945757363226),
  MAAC_FLT_C(0.99970087116673334),
  MAAC_FLT_C(0.99971193792319191),
  MAAC_FLT_C(0.99972266621563699),
  MAAC_FLT_C(0.99973306426858244),
  MAAC_FLT_C(0.99974314015991861),
  MAAC_FLT_C(0.99975290182241072),
  MAAC_FLT_C(0.99976235704520044),
  MAAC_FLT_C(0.99977151347531501),
  MAAC_FLT_C(0.99978037861917823),
  MAAC_FLT_C(0.99978895984412663),
  MAAC_FLT_C(0.99979726437992922),
  MAAC_FLT_C(0.99980529932030948),
  MAAC_FLT_C(0.99981307162447053),
  MAAC_FLT_C(0.99982058811862295),
  MAAC_FLT_C(0.99982785549751374),
  MAAC_FLT_C(0.99983488032595702),
  MAAC_FLT_C(0.99984166904036553),
  MAAC_FLT_C(0.99984822795028361),
  MAAC_FLT_C(0.99985456323991917),
  MAAC_FLT_C(0.99986068096967695),
  MAAC_FLT_C(0.99986658707768972),
  MAAC_FLT_C(0.99987228738135014),
  MAAC_FLT_C(0.99987778757884038),
  MAAC_FLT_C(0.9998830932506606),
  MAAC_FLT_C(0.99988820986115479),
  MAAC_FLT_C(0.99989314276003538),
  MAAC_FLT_C(0.99989789718390443),
  MAAC_FLT_C(0.99990247825777179),
  MAAC_FLT_C(0.99990689099657015),
  MAAC_FLT_C(0.99991114030666617),
  MAAC_FLT_C(0.99991523098736801),
  MAAC_FLT_C(0.99991916773242795),
  MAAC_FLT_C(0.99992295513154072),
  MAAC_FLT_C(0.99992659767183656),
  MAAC_FLT_C(0.99993009973936964),
  MAAC_FLT_C(0.9999334656205997),
  MAAC_FLT_C(0.99993669950386932),
  MAAC_FLT_C(0.99993980548087369),
  MAAC_FLT_C(0.99994278754812427),
  MAAC_FLT_C(0.99994564960840659),
  MAAC_FLT_C(0.99994839547222958),
  MAAC_FLT_C(0.99995102885926901),
  MAAC_FLT_C(0.99995355339980196),
  MAAC_FLT_C(0.99995597263613512),
  MAAC_FLT_C(0.99995829002402359),
  MAAC_FLT_C(0.99996050893408184),
  MAAC_FLT_C(0.99996263265318708),
  MAAC_FLT_C(0.99996466438587206),
  MAAC_FLT_C(0.99996660725571052),
  MAAC_FLT_C(0.99996846430669251),
  MAAC_FLT_C(0.99997023850459055),
  MAAC_FLT_C(0.99997193273831619),
  MAAC_FLT_C(0.99997354982126663),
  MAAC_FLT_C(0.99997509249266125),
  MAAC_FLT_C(0.9999765634188682),
  MAAC_FLT_C(0.99997796519472026),
  MAAC_FLT_C(0.9999793003448203),
  MAAC_FLT_C(0.99998057132483553),
  MAAC_FLT_C(0.99998178052278175),
  MAAC_FLT_C(0.99998293026029561),
  MAAC_FLT_C(0.99998402279389575),
  MAAC_FLT_C(0.99998506031623313),
  MAAC_FLT_C(0.99998604495732912),
  MAAC_FLT_C(0.99998697878580256),
  MAAC_FLT_C(0.99998786381008387),
  MAAC_FLT_C(0.99998870197961887),
  MAAC_FLT_C(0.99998949518605917),
  MAAC_FLT_C(0.99999024526444069),
  MAAC_FLT_C(0.99999095399435067),
  MAAC_FLT_C(0.99999162310108125),
  MAAC_FLT_C(0.99999225425677074),
  MAAC_FLT_C(0.99999284908153319),
  MAAC_FLT_C(0.99999340914457402),
  MAAC_FLT_C(0.99999393596529418),
  MAAC_FLT_C(0.99999443101438013),
  MAAC_FLT_C(0.99999489571488209),
  MAAC_FLT_C(0.99999533144327901),
  MAAC_FLT_C(0.99999573953052934),
  MAAC_FLT_C(0.99999612126311122),
  MAAC_FLT_C(0.99999647788404666),
  MAAC_FLT_C(0.99999681059391499),
  MAAC_FLT_C(0.9999971205518513),
  MAAC_FLT_C(0.99999740887653299),
  MAAC_FLT_C(0.99999767664715167),
  MAAC_FLT_C(0.99999792490437323),
  MAAC_FLT_C(0.99999815465128339),
  MAAC_FLT_C(0.99999836685432064),
  MAAC_FLT_C(0.99999856244419527),
  MAAC_FLT_C(0.99999874231679553),
  MAAC_FLT_C(0.99999890733408037),
  MAAC_FLT_C(0.99999905832495828),
  MAAC_FLT_C(0.99999919608615351),
  MAAC_FLT_C(0.99999932138305858),
  MAAC_FLT_C(0.99999943495057353),
  MAAC_FLT_C(0.99999953749393156),
  MAAC_FLT_C(0.99999962968951206),
  MAAC_FLT_C(0.99999971218563999),
  MAAC_FLT_C(0.99999978560337188),
  MAAC_FLT_C(0.99999985053726892),
  MAAC_FLT_C(0.99999990755615709),
  MAAC_FLT_C(0.99999995720387325)
};

static const maac_flt maac_window_kbd_128[128] = {
  MAAC_FLT_C(4.3795704094127481e-05),
  MAAC_FLT_C(0.00011867384648224463),
  MAAC_FLT_C(0.00023071658332418464),
  MAAC_FLT_C(0.00038947283691585777),
  MAAC_FLT_C(0.00060581273966941551),
  MAAC_FLT_C(0.0008919969751945198),
  MAAC_FLT_C(0.0012617254688987022),
  MAAC_FLT_C(0.0017301724656015947),
  MAAC_FLT_C(0.0023140072268486253),
  MAAC_FLT_C(0.0030313990120177845),
  MAAC_FLT_C(0.0039020050422920102),
  MAAC_FLT_C(0.0049469402870961649),
  MAAC_FLT_C(0.0061887280908610879),
  MAAC_FLT_C(0.0076512308612142095),
  MAAC_FLT_C(0.0093595602641038684),
  MAAC_FLT_C(0.011339966614474834),
  MAAC_FLT_C(0.013619707410347416),
  MAAC_FLT_C(0.016226895230568687),
  MAAC_FLT_C(0.019190325498790638),
  MAAC_FLT_C(0.022539284904756533),
  MAAC_FLT_C(0.026303341564858772),
  MAAC_FLT_C(0.030512118293090758),
  MAAC_FLT_C(0.035195050636747285),
  MAAC_FLT_C(0.040381131604266006),
  MAAC_FLT_C(0.046098645271187837),
  MAAC_FLT_C(0.052374891690158652),
  MAAC_FLT_C(0.059235905748154773),
  MAAC_FLT_C(0.066706172804850694),
  MAAC_FLT_C(0.074808344106694555),
  MAAC_FLT_C(0.083562955098580902),
  MAAC_FLT_C(0.092988149846168919),
  MAAC_FLT_C(0.10309941483448867),
  MAAC_FLT_C(0.11390932542059629),
  MAAC_FLT_C(0.12542730818830364),
  MAAC_FLT_C(0.137659422380596),
  MAAC_FLT_C(0.15060816347003489),
  MAAC_FLT_C(0.16427229176958602),
  MAAC_FLT_C(0.17864668878689699),
  MAAC_FLT_C(0.1937222437856552),
  MAAC_FLT_C(0.20948577274046346),
  MAAC_FLT_C(0.22591997155942556),
  MAAC_FLT_C(0.24300340510463592),
  MAAC_FLT_C(0.26071053316880333),
  MAAC_FLT_C(0.27901177417057249),
  MAAC_FLT_C(0.29787360691638726),
  MAAC_FLT_C(0.31725871034796677),
  MAAC_FLT_C(0.33712614075691516),
  MAAC_FLT_C(0.35743154550713024),
  MAAC_FLT_C(0.37812741186712484),
  MAAC_FLT_C(0.39916334912378643),
  MAAC_FLT_C(0.42048640173210577),
  MAAC_FLT_C(0.44204139085753968),
  MAAC_FLT_C(0.46377128129427936),
  MAAC_FLT_C(0.48561757039888709),
  MAAC_FLT_C(0.50752069536930999),
  MAAC_FLT_C(0.52942045492858281),
  MAAC_FLT_C(0.55125644124455075),
  MAAC_FLT_C(0.57296847773515147),
  MAAC_FLT_C(0.5944970582761202),
  MAAC_FLT_C(0.61578378324682725),
  MAAC_FLT_C(0.63677178782208976),
  MAAC_FLT_C(0.65740615794443991),
  MAAC_FLT_C(0.67763432949305724),
  MAAC_FLT_C(0.69740646630238134),
  MAAC_FLT_C(0.71667581287470927),
  MAAC_FLT_C(0.73539901787566631),
  MAAC_FLT_C(0.75353642479758731),
  MAAC_FLT_C(0.77105232652126743),
  MAAC_FLT_C(0.7879151808984417),
  MAAC_FLT_C(0.80409778491240669),
  MAAC_FLT_C(0.81957740544860247),
  MAAC_FLT_C(0.83433586521639647),
  MAAC_FLT_C(0.84835958290291757),
  MAAC_FLT_C(0.8616395672042062),
  MAAC_FLT_C(0.87417136496220327),
  MAAC_FLT_C(0.88595496423162623),
  MAAC_FLT_C(0.89699465370130915),
  MAAC_FLT_C(0.90729884049214238),
  MAAC_FLT_C(0.91687982893958464),
  MAAC_FLT_C(0.92575356353333549),
  MAAC_FLT_C(0.93393933971986853),
  MAAC_FLT_C(0.94145948676421998),
  MAAC_FLT_C(0.94833902730423614),
  MAAC_FLT_C(0.95460531860168329),
  MAAC_FLT_C(0.96028768078851723),
  MAAC_FLT_C(0.9654170176119945),
  MAAC_FLT_C(0.97002543528896801),
  MAAC_FLT_C(0.97414586507903833),
  MAAC_FLT_C(0.97781169507187349),
  MAAC_FLT_C(0.98105641645251518),
  MAAC_FLT_C(0.98391328915991239),
  MAAC_FLT_C(0.98641503139234843),
  MAAC_FLT_C(0.98859353684726414),
  MAAC_FLT_C(0.99047962292509606),
  MAAC_FLT_C(0.99210281239427811),
  MAAC_FLT_C(0.99349115022843792),
  MAAC_FLT_C(0.99467105651103871),
  MAAC_FLT_C(0.99566721548325898),
  MAAC_FLT_C(0.99650250001452201),
  MAAC_FLT_C(0.99719793002794299),
  MAAC_FLT_C(0.99777266273922804),
  MAAC_FLT_C(0.99824401198814894),
  MAAC_FLT_C(0.99862749347313884),
  MAAC_FLT_C(0.9989368923531462),
  MAAC_FLT_C(0.99918434946227952),
  MAAC_FLT_C(0.99938046229185251),
  MAAC_FLT_C(0.99953439692552271),
  MAAC_FLT_C(0.99965400725577158),
  MAAC_FLT_C(0.99974595804933475),
  MAAC_FLT_C(0.99981584874778329),
  MAAC_FLT_C(0.99986833526778718),
  MAAC_FLT_C(0.99990724748351345),
  MAAC_FLT_C(0.99993570051137903),
  MAAC_FLT_C(0.99995619835653926),
  MAAC_FLT_C(0.99997072890475558),
  MAAC_FLT_C(0.99998084963894041),
  MAAC_FLT_C(0.99998776381603571),
  MAAC_FLT_C(0.99999238714934713),
  MAAC_FLT_C(0.99999540529945918),
  MAAC_FLT_C(0.99999732268169295),
  MAAC_FLT_C(0.99999850325049944),
  MAAC_FLT_C(0.9999992040241038),
  MAAC_FLT_C(0.9999996021706189),
  MAAC_FLT_C(0.99999981649544534),
  MAAC_FLT_C(0.99999992415545169),
  MAAC_FLT_C(0.99999997338492863),
  MAAC_FLT_C(0.99999999295825903),
  MAAC_FLT_C(0.99999999904096815)
};

static const maac_flt maac_window_sin_1024[1024] = {
  MAAC_FLT_C(0.00076699031874270449),
  MAAC_FLT_C(0.002300969151425805),
  MAAC_FLT_C(0.0038349425697062275),
  MAAC_FLT_C(0.0053689069639963425),
  MAAC_FLT_C(0.0069028587247297558),
  MAAC_FLT_C(0.0084367942423697988),
  MAAC_FLT_C(0.0099707099074180308),
  MAAC_FLT_C(0.011504602110422714),
  MAAC_FLT_C(0.013038467241987334),
  MAAC_FLT_C(0.014572301692779064),
  MAAC_FLT_C(0.016106101853537287),
  MAAC_FLT_C(0.017639864115082053),
  MAAC_FLT_C(0.019173584868322623),
  MAAC_FLT_C(0.020707260504265895),
  MAAC_FLT_C(0.022240887414024961),
  MAAC_FLT_C(0.023774461988827555),
  MAAC_FLT_C(0.025307980620024571),
  MAAC_FLT_C(0.026841439699098531),
  MAAC_FLT_C(0.028374835617672099),
  MAAC_FLT_C(0.029908164767516555),
  MAAC_FLT_C(0.031441423540560301),
  MAAC_FLT_C(0.032974608328897335),
  MAAC_FLT_C(0.03450771552479575),
  MAAC_FLT_C(0.036040741520706229),
  MAAC_FLT_C(0.037573682709270494),
  MAAC_FLT_C(0.039106535483329888),
  MAAC_FLT_C(0.040639296235933736),
  MAAC_FLT_C(0.042171961360347947),
  MAAC_FLT_C(0.043704527250063421),
  MAAC_FLT_C(0.04523699029880459),
  MAAC_FLT_C(0.046769346900537863),
  MAAC_FLT_C(0.048301593449480144),
  MAAC_FLT_C(0.049833726340107277),
  MAAC_FLT_C(0.051365741967162593),
  MAAC_FLT_C(0.052897636725665324),
  MAAC_FLT_C(0.054429407010919133),
  MAAC_FLT_C(0.055961049218520569),
  MAAC_FLT_C(0.057492559744367566),
  MAAC_FLT_C(0.059023934984667931),
  MAAC_FLT_C(0.060555171335947788),
  MAAC_FLT_C(0.062086265195060088),
  MAAC_FLT_C(0.063617212959193106),
  MAAC_FLT_C(0.065148011025878833),
  MAAC_FLT_C(0.066678655793001557),
  MAAC_FLT_C(0.068209143658806329),
  MAAC_FLT_C(0.069739471021907307),
  MAAC_FLT_C(0.071269634281296401),
  MAAC_FLT_C(0.072799629836351673),
  MAAC_FLT_C(0.074329454086845756),
  MAAC_FLT_C(0.075859103432954447),
  MAAC_FLT_C(0.077388574275265049),
  MAAC_FLT_C(0.078917863014784942),
  MAAC_FLT_C(0.080446966052950014),
  MAAC_FLT_C(0.081975879791633066),
  MAAC_FLT_C(0.083504600633152432),
  MAAC_FLT_C(0.085033124980280275),
  MAAC_FLT_C(0.08656144923625117),
  MAAC_FLT_C(0.088089569804770507),
  MAAC_FLT_C(0.089617483090022959),
  MAAC_FLT_C(0.091145185496681005),
  MAAC_FLT_C(0.09267267342991331),
  MAAC_FLT_C(0.094199943295393204),
  MAAC_FLT_C(0.095726991499307162),
  MAAC_FLT_C(0.097253814448363271),
  MAAC_FLT_C(0.098780408549799623),
  MAAC_FLT_C(0.10030677021139286),
  MAAC_FLT_C(0.10183289584146653),
  MAAC_FLT_C(0.10335878184889961),
  MAAC_FLT_C(0.10488442464313497),
  MAAC_FLT_C(0.10640982063418768),
  MAAC_FLT_C(0.10793496623265365),
  MAAC_FLT_C(0.10945985784971798),
  MAAC_FLT_C(0.11098449189716339),
  MAAC_FLT_C(0.11250886478737869),
  MAAC_FLT_C(0.1140329729333672),
  MAAC_FLT_C(0.11555681274875526),
  MAAC_FLT_C(0.11708038064780059),
  MAAC_FLT_C(0.11860367304540072),
  MAAC_FLT_C(0.1201266863571015),
  MAAC_FLT_C(0.12164941699910553),
  MAAC_FLT_C(0.12317186138828048),
  MAAC_FLT_C(0.12469401594216764),
  MAAC_FLT_C(0.12621587707899035),
  MAAC_FLT_C(0.12773744121766231),
  MAAC_FLT_C(0.12925870477779614),
  MAAC_FLT_C(0.13077966417971171),
  MAAC_FLT_C(0.13230031584444465),
  MAAC_FLT_C(0.13382065619375472),
  MAAC_FLT_C(0.13534068165013421),
  MAAC_FLT_C(0.13686038863681638),
  MAAC_FLT_C(0.13837977357778389),
  MAAC_FLT_C(0.13989883289777721),
  MAAC_FLT_C(0.14141756302230302),
  MAAC_FLT_C(0.14293596037764267),
  MAAC_FLT_C(0.14445402139086047),
  MAAC_FLT_C(0.14597174248981221),
  MAAC_FLT_C(0.14748912010315357),
  MAAC_FLT_C(0.14900615066034845),
  MAAC_FLT_C(0.1505228305916774),
  MAAC_FLT_C(0.15203915632824605),
  MAAC_FLT_C(0.15355512430199345),
  MAAC_FLT_C(0.15507073094570051),
  MAAC_FLT_C(0.15658597269299843),
  MAAC_FLT_C(0.15810084597837698),
  MAAC_FLT_C(0.15961534723719306),
  MAAC_FLT_C(0.16112947290567881),
  MAAC_FLT_C(0.16264321942095031),
  MAAC_FLT_C(0.16415658322101581),
  MAAC_FLT_C(0.16566956074478412),
  MAAC_FLT_C(0.16718214843207294),
  MAAC_FLT_C(0.16869434272361733),
  MAAC_FLT_C(0.17020614006107807),
  MAAC_FLT_C(0.17171753688704997),
  MAAC_FLT_C(0.17322852964507032),
  MAAC_FLT_C(0.1747391147796272),
  MAAC_FLT_C(0.17624928873616791),
  MAAC_FLT_C(0.17775904796110717),
  MAAC_FLT_C(0.17926838890183575),
  MAAC_FLT_C(0.18077730800672859),
  MAAC_FLT_C(0.1822858017251533),
  MAAC_FLT_C(0.18379386650747845),
  MAAC_FLT_C(0.1853014988050819),
  MAAC_FLT_C(0.18680869507035927),
  MAAC_FLT_C(0.18831545175673212),
  MAAC_FLT_C(0.18982176531865641),
  MAAC_FLT_C(0.1913276322116309),
  MAAC_FLT_C(0.19283304889220523),
  MAAC_FLT_C(0.1943380118179886),
  MAAC_FLT_C(0.19584251744765785),
  MAAC_FLT_C(0.19734656224096592),
  MAAC_FLT_C(0.19885014265875009),
  MAAC_FLT_C(0.20035325516294045),
  MAAC_FLT_C(0.20185589621656805),
  MAAC_FLT_C(0.20335806228377332),
  MAAC_FLT_C(0.20485974982981442),
  MAAC_FLT_C(0.20636095532107551),
  MAAC_FLT_C(0.20786167522507507),
  MAAC_FLT_C(0.20936190601047416),
  MAAC_FLT_C(0.21086164414708486),
  MAAC_FLT_C(0.21236088610587842),
  MAAC_FLT_C(0.21385962835899375),
  MAAC_FLT_C(0.21535786737974555),
  MAAC_FLT_C(0.21685559964263262),
  MAAC_FLT_C(0.21835282162334632),
  MAAC_FLT_C(0.2198495297987787),
  MAAC_FLT_C(0.22134572064703081),
  MAAC_FLT_C(0.22284139064742112),
  MAAC_FLT_C(0.2243365362804936),
  MAAC_FLT_C(0.22583115402802617),
  MAAC_FLT_C(0.22732524037303886),
  MAAC_FLT_C(0.22881879179980222),
  MAAC_FLT_C(0.23031180479384544),
  MAAC_FLT_C(0.23180427584196478),
  MAAC_FLT_C(0.23329620143223159),
  MAAC_FLT_C(0.23478757805400097),
  MAAC_FLT_C(0.23627840219791957),
  MAAC_FLT_C(0.23776867035593419),
  MAAC_FLT_C(0.23925837902129998),
  MAAC_FLT_C(0.24074752468858843),
  MAAC_FLT_C(0.24223610385369601),
  MAAC_FLT_C(0.24372411301385216),
  MAAC_FLT_C(0.24521154866762754),
  MAAC_FLT_C(0.24669840731494241),
  MAAC_FLT_C(0.24818468545707478),
  MAAC_FLT_C(0.24967037959666855),
  MAAC_FLT_C(0.25115548623774192),
  MAAC_FLT_C(0.25264000188569552),
  MAAC_FLT_C(0.25412392304732062),
  MAAC_FLT_C(0.25560724623080738),
  MAAC_FLT_C(0.25708996794575312),
  MAAC_FLT_C(0.25857208470317034),
  MAAC_FLT_C(0.26005359301549519),
  MAAC_FLT_C(0.26153448939659552),
  MAAC_FLT_C(0.263014770361779),
  MAAC_FLT_C(0.26449443242780163),
  MAAC_FLT_C(0.26597347211287559),
  MAAC_FLT_C(0.26745188593667762),
  MAAC_FLT_C(0.26892967042035726),
  MAAC_FLT_C(0.27040682208654482),
  MAAC_FLT_C(0.27188333745935972),
  MAAC_FLT_C(0.27335921306441868),
  MAAC_FLT_C(0.27483444542884394),
  MAAC_FLT_C(0.27630903108127108),
  MAAC_FLT_C(0.27778296655185769),
  MAAC_FLT_C(0.27925624837229118),
  MAAC_FLT_C(0.28072887307579719),
  MAAC_FLT_C(0.28220083719714756),
  MAAC_FLT_C(0.28367213727266843),
  MAAC_FLT_C(0.28514276984024867),
  MAAC_FLT_C(0.28661273143934779),
  MAAC_FLT_C(0.28808201861100413),
  MAAC_FLT_C(0.28955062789784303),
  MAAC_FLT_C(0.29101855584408509),
  MAAC_FLT_C(0.29248579899555388),
  MAAC_FLT_C(0.29395235389968466),
  MAAC_FLT_C(0.29541821710553201),
  MAAC_FLT_C(0.29688338516377827),
  MAAC_FLT_C(0.2983478546267414),
  MAAC_FLT_C(0.29981162204838335),
  MAAC_FLT_C(0.30127468398431795),
  MAAC_FLT_C(0.30273703699181914),
  MAAC_FLT_C(0.30419867762982911),
  MAAC_FLT_C(0.30565960245896612),
  MAAC_FLT_C(0.3071198080415331),
  MAAC_FLT_C(0.30857929094152509),
  MAAC_FLT_C(0.31003804772463789),
  MAAC_FLT_C(0.31149607495827591),
  MAAC_FLT_C(0.3129533692115602),
  MAAC_FLT_C(0.31440992705533666),
  MAAC_FLT_C(0.31586574506218396),
  MAAC_FLT_C(0.31732081980642174),
  MAAC_FLT_C(0.31877514786411848),
  MAAC_FLT_C(0.32022872581309986),
  MAAC_FLT_C(0.32168155023295658),
  MAAC_FLT_C(0.32313361770505233),
  MAAC_FLT_C(0.32458492481253215),
  MAAC_FLT_C(0.32603546814033024),
  MAAC_FLT_C(0.327485244275178),
  MAAC_FLT_C(0.3289342498056122),
  MAAC_FLT_C(0.33038248132198278),
  MAAC_FLT_C(0.33182993541646111),
  MAAC_FLT_C(0.33327660868304793),
  MAAC_FLT_C(0.33472249771758122),
  MAAC_FLT_C(0.33616759911774452),
  MAAC_FLT_C(0.33761190948307462),
  MAAC_FLT_C(0.33905542541496964),
  MAAC_FLT_C(0.34049814351669716),
  MAAC_FLT_C(0.34194006039340219),
  MAAC_FLT_C(0.34338117265211504),
  MAAC_FLT_C(0.34482147690175929),
  MAAC_FLT_C(0.34626096975316001),
  MAAC_FLT_C(0.34769964781905138),
  MAAC_FLT_C(0.34913750771408497),
  MAAC_FLT_C(0.35057454605483751),
  MAAC_FLT_C(0.35201075945981908),
  MAAC_FLT_C(0.35344614454948081),
  MAAC_FLT_C(0.35488069794622279),
  MAAC_FLT_C(0.35631441627440241),
  MAAC_FLT_C(0.3577472961603419),
  MAAC_FLT_C(0.3591793342323365),
  MAAC_FLT_C(0.36061052712066227),
  MAAC_FLT_C(0.36204087145758418),
  MAAC_FLT_C(0.36347036387736376),
  MAAC_FLT_C(0.36489900101626732),
  MAAC_FLT_C(0.36632677951257359),
  MAAC_FLT_C(0.36775369600658198),
  MAAC_FLT_C(0.36917974714062002),
  MAAC_FLT_C(0.37060492955905167),
  MAAC_FLT_C(0.37202923990828501),
  MAAC_FLT_C(0.3734526748367803),
  MAAC_FLT_C(0.37487523099505754),
  MAAC_FLT_C(0.37629690503570479),
  MAAC_FLT_C(0.37771769361338564),
  MAAC_FLT_C(0.37913759338484732),
  MAAC_FLT_C(0.38055660100892852),
  MAAC_FLT_C(0.38197471314656722),
  MAAC_FLT_C(0.38339192646080866),
  MAAC_FLT_C(0.38480823761681288),
  MAAC_FLT_C(0.38622364328186298),
  MAAC_FLT_C(0.38763814012537273),
  MAAC_FLT_C(0.38905172481889438),
  MAAC_FLT_C(0.39046439403612659),
  MAAC_FLT_C(0.39187614445292235),
  MAAC_FLT_C(0.3932869727472964),
  MAAC_FLT_C(0.39469687559943356),
  MAAC_FLT_C(0.39610584969169627),
  MAAC_FLT_C(0.39751389170863233),
  MAAC_FLT_C(0.39892099833698291),
  MAAC_FLT_C(0.40032716626569009),
  MAAC_FLT_C(0.40173239218590501),
  MAAC_FLT_C(0.4031366727909953),
  MAAC_FLT_C(0.404540004776553),
  MAAC_FLT_C(0.40594238484040251),
  MAAC_FLT_C(0.40734380968260797),
  MAAC_FLT_C(0.40874427600548136),
  MAAC_FLT_C(0.41014378051359024),
  MAAC_FLT_C(0.41154231991376522),
  MAAC_FLT_C(0.41293989091510808),
  MAAC_FLT_C(0.4143364902289991),
  MAAC_FLT_C(0.41573211456910536),
  MAAC_FLT_C(0.41712676065138787),
  MAAC_FLT_C(0.4185204251941097),
  MAAC_FLT_C(0.41991310491784362),
  MAAC_FLT_C(0.42130479654547964),
  MAAC_FLT_C(0.42269549680223295),
  MAAC_FLT_C(0.42408520241565156),
  MAAC_FLT_C(0.4254739101156238),
  MAAC_FLT_C(0.42686161663438643),
  MAAC_FLT_C(0.42824831870653196),
  MAAC_FLT_C(0.42963401306901638),
  MAAC_FLT_C(0.43101869646116703),
  MAAC_FLT_C(0.43240236562469014),
  MAAC_FLT_C(0.43378501730367852),
  MAAC_FLT_C(0.43516664824461926),
  MAAC_FLT_C(0.4365472551964012),
  MAAC_FLT_C(0.43792683491032286),
  MAAC_FLT_C(0.43930538414009995),
  MAAC_FLT_C(0.4406828996418729),
  MAAC_FLT_C(0.4420593781742147),
  MAAC_FLT_C(0.44343481649813848),
  MAAC_FLT_C(0.44480921137710488),
  MAAC_FLT_C(0.44618255957703007),
  MAAC_FLT_C(0.44755485786629301),
  MAAC_FLT_C(0.44892610301574326),
  MAAC_FLT_C(0.45029629179870861),
  MAAC_FLT_C(0.45166542099100249),
  MAAC_FLT_C(0.45303348737093158),
  MAAC_FLT_C(0.45440048771930358),
  MAAC_FLT_C(0.45576641881943464),
  MAAC_FLT_C(0.45713127745715698),
  MAAC_FLT_C(0.45849506042082627),
  MAAC_FLT_C(0.45985776450132954),
  MAAC_FLT_C(0.46121938649209238),
  MAAC_FLT_C(0.46257992318908681),
  MAAC_FLT_C(0.46393937139083852),
  MAAC_FLT_C(0.4652977278984346),
  MAAC_FLT_C(0.46665498951553092),
  MAAC_FLT_C(0.46801115304835983),
  MAAC_FLT_C(0.46936621530573752),
  MAAC_FLT_C(0.4707201730990716),
  MAAC_FLT_C(0.47207302324236866),
  MAAC_FLT_C(0.47342476255224153),
  MAAC_FLT_C(0.47477538784791712),
  MAAC_FLT_C(0.47612489595124358),
  MAAC_FLT_C(0.47747328368669806),
  MAAC_FLT_C(0.47882054788139389),
  MAAC_FLT_C(0.48016668536508839),
  MAAC_FLT_C(0.48151169297018986),
  MAAC_FLT_C(0.48285556753176567),
  MAAC_FLT_C(0.48419830588754903),
  MAAC_FLT_C(0.48553990487794696),
  MAAC_FLT_C(0.48688036134604734),
  MAAC_FLT_C(0.48821967213762679),
  MAAC_FLT_C(0.48955783410115744),
  MAAC_FLT_C(0.49089484408781509),
  MAAC_FLT_C(0.49223069895148602),
  MAAC_FLT_C(0.49356539554877477),
  MAAC_FLT_C(0.49489893073901126),
  MAAC_FLT_C(0.49623130138425825),
  MAAC_FLT_C(0.49756250434931915),
  MAAC_FLT_C(0.49889253650174459),
  MAAC_FLT_C(0.50022139471184068),
  MAAC_FLT_C(0.50154907585267539),
  MAAC_FLT_C(0.50287557680008699),
  MAAC_FLT_C(0.50420089443269034),
  MAAC_FLT_C(0.50552502563188539),
  MAAC_FLT_C(0.50684796728186321),
  MAAC_FLT_C(0.5081697162696146),
  MAAC_FLT_C(0.50949026948493636),
  MAAC_FLT_C(0.51080962382043904),
  MAAC_FLT_C(0.51212777617155469),
  MAAC_FLT_C(0.51344472343654346),
  MAAC_FLT_C(0.5147604625165012),
  MAAC_FLT_C(0.51607499031536663),
  MAAC_FLT_C(0.51738830373992906),
  MAAC_FLT_C(0.51870039969983495),
  MAAC_FLT_C(0.52001127510759604),
  MAAC_FLT_C(0.52132092687859566),
  MAAC_FLT_C(0.52262935193109661),
  MAAC_FLT_C(0.5239365471862486),
  MAAC_FLT_C(0.52524250956809471),
  MAAC_FLT_C(0.52654723600357944),
  MAAC_FLT_C(0.52785072342255523),
  MAAC_FLT_C(0.52915296875779061),
  MAAC_FLT_C(0.53045396894497632),
  MAAC_FLT_C(0.53175372092273332),
  MAAC_FLT_C(0.53305222163261945),
  MAAC_FLT_C(0.53434946801913752),
  MAAC_FLT_C(0.53564545702974109),
  MAAC_FLT_C(0.53694018561484291),
  MAAC_FLT_C(0.5382336507278217),
  MAAC_FLT_C(0.53952584932502889),
  MAAC_FLT_C(0.54081677836579667),
  MAAC_FLT_C(0.54210643481244392),
  MAAC_FLT_C(0.5433948156302848),
  MAAC_FLT_C(0.54468191778763453),
  MAAC_FLT_C(0.54596773825581757),
  MAAC_FLT_C(0.54725227400917409),
  MAAC_FLT_C(0.54853552202506739),
  MAAC_FLT_C(0.54981747928389091),
  MAAC_FLT_C(0.55109814276907543),
  MAAC_FLT_C(0.55237750946709607),
  MAAC_FLT_C(0.55365557636747931),
  MAAC_FLT_C(0.55493234046281037),
  MAAC_FLT_C(0.55620779874873993),
  MAAC_FLT_C(0.55748194822399155),
  MAAC_FLT_C(0.55875478589036831),
  MAAC_FLT_C(0.56002630875276038),
  MAAC_FLT_C(0.56129651381915147),
  MAAC_FLT_C(0.56256539810062656),
  MAAC_FLT_C(0.56383295861137817),
  MAAC_FLT_C(0.56509919236871398),
  MAAC_FLT_C(0.56636409639306384),
  MAAC_FLT_C(0.56762766770798623),
  MAAC_FLT_C(0.56888990334017586),
  MAAC_FLT_C(0.5701508003194703),
  MAAC_FLT_C(0.57141035567885723),
  MAAC_FLT_C(0.57266856645448116),
  MAAC_FLT_C(0.57392542968565075),
  MAAC_FLT_C(0.57518094241484508),
  MAAC_FLT_C(0.57643510168772183),
  MAAC_FLT_C(0.5776879045531228),
  MAAC_FLT_C(0.57893934806308178),
  MAAC_FLT_C(0.58018942927283168),
  MAAC_FLT_C(0.58143814524081017),
  MAAC_FLT_C(0.58268549302866846),
  MAAC_FLT_C(0.58393146970127618),
  MAAC_FLT_C(0.58517607232673041),
  MAAC_FLT_C(0.5864192979763605),
  MAAC_FLT_C(0.58766114372473666),
  MAAC_FLT_C(0.58890160664967572),
  MAAC_FLT_C(0.59014068383224882),
  MAAC_FLT_C(0.59137837235678758),
  MAAC_FLT_C(0.59261466931089113),
  MAAC_FLT_C(0.59384957178543363),
  MAAC_FLT_C(0.59508307687456996),
  MAAC_FLT_C(0.59631518167574371),
  MAAC_FLT_C(0.59754588328969316),
  MAAC_FLT_C(0.59877517882045872),
  MAAC_FLT_C(0.60000306537538894),
  MAAC_FLT_C(0.6012295400651485),
  MAAC_FLT_C(0.60245460000372375),
  MAAC_FLT_C(0.60367824230843037),
  MAAC_FLT_C(0.60490046409991982),
  MAAC_FLT_C(0.60612126250218612),
  MAAC_FLT_C(0.60734063464257293),
  MAAC_FLT_C(0.60855857765177945),
  MAAC_FLT_C(0.60977508866386843),
  MAAC_FLT_C(0.61099016481627166),
  MAAC_FLT_C(0.61220380324979795),
  MAAC_FLT_C(0.61341600110863859),
  MAAC_FLT_C(0.61462675554037505),
  MAAC_FLT_C(0.61583606369598509),
  MAAC_FLT_C(0.61704392272984976),
  MAAC_FLT_C(0.61825032979976025),
  MAAC_FLT_C(0.61945528206692402),
  MAAC_FLT_C(0.62065877669597214),
  MAAC_FLT_C(0.62186081085496536),
  MAAC_FLT_C(0.62306138171540126),
  MAAC_FLT_C(0.62426048645222065),
  MAAC_FLT_C(0.62545812224381436),
  MAAC_FLT_C(0.62665428627202935),
  MAAC_FLT_C(0.62784897572217646),
  MAAC_FLT_C(0.629042187783036),
  MAAC_FLT_C(0.63023391964686437),
  MAAC_FLT_C(0.63142416850940186),
  MAAC_FLT_C(0.63261293156987741),
  MAAC_FLT_C(0.63380020603101728),
  MAAC_FLT_C(0.63498598909904946),
  MAAC_FLT_C(0.63617027798371217),
  MAAC_FLT_C(0.63735306989825913),
  MAAC_FLT_C(0.63853436205946679),
  MAAC_FLT_C(0.63971415168764045),
  MAAC_FLT_C(0.64089243600662138),
  MAAC_FLT_C(0.64206921224379254),
  MAAC_FLT_C(0.64324447763008585),
  MAAC_FLT_C(0.64441822939998838),
  MAAC_FLT_C(0.64559046479154869),
  MAAC_FLT_C(0.64676118104638392),
  MAAC_FLT_C(0.64793037540968534),
  MAAC_FLT_C(0.64909804513022595),
  MAAC_FLT_C(0.65026418746036585),
  MAAC_FLT_C(0.65142879965605982),
  MAAC_FLT_C(0.65259187897686244),
  MAAC_FLT_C(0.65375342268593606),
  MAAC_FLT_C(0.65491342805005603),
  MAAC_FLT_C(0.6560718923396176),
  MAAC_FLT_C(0.65722881282864254),
  MAAC_FLT_C(0.65838418679478505),
  MAAC_FLT_C(0.65953801151933866),
  MAAC_FLT_C(0.6606902842872423),
  MAAC_FLT_C(0.66184100238708687),
  MAAC_FLT_C(0.66299016311112147),
  MAAC_FLT_C(0.66413776375526001),
  MAAC_FLT_C(0.66528380161908718),
  MAAC_FLT_C(0.66642827400586524),
  MAAC_FLT_C(0.66757117822254031),
  MAAC_FLT_C(0.66871251157974798),
  MAAC_FLT_C(0.66985227139182102),
  MAAC_FLT_C(0.67099045497679422),
  MAAC_FLT_C(0.67212705965641173),
  MAAC_FLT_C(0.67326208275613297),
  MAAC_FLT_C(0.67439552160513905),
  MAAC_FLT_C(0.67552737353633852),
  MAAC_FLT_C(0.67665763588637495),
  MAAC_FLT_C(0.6777863059956315),
  MAAC_FLT_C(0.67891338120823841),
  MAAC_FLT_C(0.68003885887207893),
  MAAC_FLT_C(0.68116273633879543),
  MAAC_FLT_C(0.68228501096379557),
  MAAC_FLT_C(0.68340568010625868),
  MAAC_FLT_C(0.6845247411291423),
  MAAC_FLT_C(0.68564219139918747),
  MAAC_FLT_C(0.68675802828692589),
  MAAC_FLT_C(0.68787224916668555),
  MAAC_FLT_C(0.68898485141659704),
  MAAC_FLT_C(0.69009583241859995),
  MAAC_FLT_C(0.69120518955844845),
  MAAC_FLT_C(0.69231292022571822),
  MAAC_FLT_C(0.69341902181381176),
  MAAC_FLT_C(0.69452349171996552),
  MAAC_FLT_C(0.69562632734525487),
  MAAC_FLT_C(0.6967275260946012),
  MAAC_FLT_C(0.69782708537677729),
  MAAC_FLT_C(0.69892500260441415),
  MAAC_FLT_C(0.70002127519400625),
  MAAC_FLT_C(0.70111590056591866),
  MAAC_FLT_C(0.70220887614439187),
  MAAC_FLT_C(0.70330019935754873),
  MAAC_FLT_C(0.70438986763740041),
  MAAC_FLT_C(0.7054778784198521),
  MAAC_FLT_C(0.70656422914470951),
  MAAC_FLT_C(0.70764891725568435),
  MAAC_FLT_C(0.70873194020040065),
  MAAC_FLT_C(0.70981329543040084),
  MAAC_FLT_C(0.71089298040115168),
  MAAC_FLT_C(0.71197099257204999),
  MAAC_FLT_C(0.71304732940642923),
  MAAC_FLT_C(0.71412198837156471),
  MAAC_FLT_C(0.71519496693868001),
  MAAC_FLT_C(0.71626626258295312),
  MAAC_FLT_C(0.71733587278352173),
  MAAC_FLT_C(0.71840379502348972),
  MAAC_FLT_C(0.71947002678993299),
  MAAC_FLT_C(0.72053456557390527),
  MAAC_FLT_C(0.72159740887044366),
  MAAC_FLT_C(0.72265855417857561),
  MAAC_FLT_C(0.72371799900132339),
  MAAC_FLT_C(0.72477574084571128),
  MAAC_FLT_C(0.72583177722277037),
  MAAC_FLT_C(0.72688610564754497),
  MAAC_FLT_C(0.72793872363909862),
  MAAC_FLT_C(0.72898962872051931),
  MAAC_FLT_C(0.73003881841892615),
  MAAC_FLT_C(0.73108629026547423),
  MAAC_FLT_C(0.73213204179536129),
  MAAC_FLT_C(0.73317607054783274),
  MAAC_FLT_C(0.73421837406618817),
  MAAC_FLT_C(0.73525894989778673),
  MAAC_FLT_C(0.73629779559405306),
  MAAC_FLT_C(0.73733490871048279),
  MAAC_FLT_C(0.73837028680664851),
  MAAC_FLT_C(0.73940392744620576),
  MAAC_FLT_C(0.74043582819689802),
  MAAC_FLT_C(0.74146598663056329),
  MAAC_FLT_C(0.74249440032313918),
  MAAC_FLT_C(0.74352106685466912),
  MAAC_FLT_C(0.74454598380930725),
  MAAC_FLT_C(0.74556914877532543),
  MAAC_FLT_C(0.74659055934511731),
  MAAC_FLT_C(0.74761021311520515),
  MAAC_FLT_C(0.74862810768624533),
  MAAC_FLT_C(0.74964424066303348),
  MAAC_FLT_C(0.75065860965451059),
  MAAC_FLT_C(0.75167121227376843),
  MAAC_FLT_C(0.75268204613805523),
  MAAC_FLT_C(0.75369110886878121),
  MAAC_FLT_C(0.75469839809152439),
  MAAC_FLT_C(0.75570391143603588),
  MAAC_FLT_C(0.75670764653624567),
  MAAC_FLT_C(0.75770960103026808),
  MAAC_FLT_C(0.75870977256040739),
  MAAC_FLT_C(0.75970815877316344),
  MAAC_FLT_C(0.76070475731923692),
  MAAC_FLT_C(0.76169956585353527),
  MAAC_FLT_C(0.76269258203517787),
  MAAC_FLT_C(0.76368380352750187),
  MAAC_FLT_C(0.76467322799806714),
  MAAC_FLT_C(0.76566085311866239),
  MAAC_FLT_C(0.76664667656531038),
  MAAC_FLT_C(0.76763069601827327),
  MAAC_FLT_C(0.76861290916205827),
  MAAC_FLT_C(0.76959331368542294),
  MAAC_FLT_C(0.7705719072813807),
  MAAC_FLT_C(0.7715486876472063),
  MAAC_FLT_C(0.77252365248444133),
  MAAC_FLT_C(0.77349679949889905),
  MAAC_FLT_C(0.77446812640067086),
  MAAC_FLT_C(0.77543763090413043),
  MAAC_FLT_C(0.77640531072794039),
  MAAC_FLT_C(0.7773711635950562),
  MAAC_FLT_C(0.77833518723273309),
  MAAC_FLT_C(0.7792973793725303),
  MAAC_FLT_C(0.78025773775031659),
  MAAC_FLT_C(0.78121626010627609),
  MAAC_FLT_C(0.7821729441849129),
  MAAC_FLT_C(0.78312778773505731),
  MAAC_FLT_C(0.78408078850986995),
  MAAC_FLT_C(0.78503194426684808),
  MAAC_FLT_C(0.78598125276783015),
  MAAC_FLT_C(0.7869287117790017),
  MAAC_FLT_C(0.78787431907090011),
  MAAC_FLT_C(0.78881807241842017),
  MAAC_FLT_C(0.78975996960081907),
  MAAC_FLT_C(0.79070000840172161),
  MAAC_FLT_C(0.79163818660912577),
  MAAC_FLT_C(0.79257450201540758),
  MAAC_FLT_C(0.79350895241732666),
  MAAC_FLT_C(0.79444153561603059),
  MAAC_FLT_C(0.79537224941706119),
  MAAC_FLT_C(0.79630109163035911),
  MAAC_FLT_C(0.7972280600702687),
  MAAC_FLT_C(0.79815315255554375),
  MAAC_FLT_C(0.79907636690935235),
  MAAC_FLT_C(0.79999770095928191),
  MAAC_FLT_C(0.8009171525373443),
  MAAC_FLT_C(0.80183471947998131),
  MAAC_FLT_C(0.80275039962806916),
  MAAC_FLT_C(0.80366419082692409),
  MAAC_FLT_C(0.804576090926307),
  MAAC_FLT_C(0.80548609778042912),
  MAAC_FLT_C(0.80639420924795624),
  MAAC_FLT_C(0.80730042319201445),
  MAAC_FLT_C(0.80820473748019472),
  MAAC_FLT_C(0.80910714998455813),
  MAAC_FLT_C(0.81000765858164114),
  MAAC_FLT_C(0.81090626115245967),
  MAAC_FLT_C(0.81180295558251536),
  MAAC_FLT_C(0.81269773976179949),
  MAAC_FLT_C(0.81359061158479851),
  MAAC_FLT_C(0.81448156895049861),
  MAAC_FLT_C(0.81537060976239129),
  MAAC_FLT_C(0.81625773192847739),
  MAAC_FLT_C(0.81714293336127297),
  MAAC_FLT_C(0.81802621197781344),
  MAAC_FLT_C(0.81890756569965895),
  MAAC_FLT_C(0.81978699245289899),
  MAAC_FLT_C(0.82066449016815746),
  MAAC_FLT_C(0.82154005678059761),
  MAAC_FLT_C(0.82241369022992639),
  MAAC_FLT_C(0.82328538846040011),
  MAAC_FLT_C(0.82415514942082857),
  MAAC_FLT_C(0.82502297106458022),
  MAAC_FLT_C(0.82588885134958678),
  MAAC_FLT_C(0.82675278823834852),
  MAAC_FLT_C(0.8276147796979384),
  MAAC_FLT_C(0.82847482370000713),
  MAAC_FLT_C(0.82933291822078825),
  MAAC_FLT_C(0.83018906124110237),
  MAAC_FLT_C(0.83104325074636232),
  MAAC_FLT_C(0.83189548472657759),
  MAAC_FLT_C(0.83274576117635946),
  MAAC_FLT_C(0.83359407809492514),
  MAAC_FLT_C(0.83444043348610319),
  MAAC_FLT_C(0.83528482535833737),
  MAAC_FLT_C(0.83612725172469216),
  MAAC_FLT_C(0.83696771060285702),
  MAAC_FLT_C(0.83780620001515094),
  MAAC_FLT_C(0.8386427179885273),
  MAAC_FLT_C(0.83947726255457855),
  MAAC_FLT_C(0.84030983174954077),
  MAAC_FLT_C(0.84114042361429808),
  MAAC_FLT_C(0.84196903619438768),
  MAAC_FLT_C(0.84279566754000412),
  MAAC_FLT_C(0.84362031570600404),
  MAAC_FLT_C(0.84444297875191066),
  MAAC_FLT_C(0.84526365474191822),
  MAAC_FLT_C(0.84608234174489694),
  MAAC_FLT_C(0.84689903783439735),
  MAAC_FLT_C(0.84771374108865427),
  MAAC_FLT_C(0.84852644959059265),
  MAAC_FLT_C(0.84933716142783067),
  MAAC_FLT_C(0.85014587469268521),
  MAAC_FLT_C(0.85095258748217573),
  MAAC_FLT_C(0.85175729789802912),
  MAAC_FLT_C(0.85256000404668397),
  MAAC_FLT_C(0.85336070403929543),
  MAAC_FLT_C(0.85415939599173873),
  MAAC_FLT_C(0.85495607802461482),
  MAAC_FLT_C(0.85575074826325392),
  MAAC_FLT_C(0.85654340483771996),
  MAAC_FLT_C(0.85733404588281559),
  MAAC_FLT_C(0.85812266953808602),
  MAAC_FLT_C(0.8589092739478239),
  MAAC_FLT_C(0.85969385726107261),
  MAAC_FLT_C(0.86047641763163207),
  MAAC_FLT_C(0.86125695321806206),
  MAAC_FLT_C(0.86203546218368721),
  MAAC_FLT_C(0.86281194269660033),
  MAAC_FLT_C(0.86358639292966799),
  MAAC_FLT_C(0.86435881106053403),
  MAAC_FLT_C(0.86512919527162369),
  MAAC_FLT_C(0.86589754375014882),
  MAAC_FLT_C(0.86666385468811102),
  MAAC_FLT_C(0.86742812628230692),
  MAAC_FLT_C(0.86819035673433131),
  MAAC_FLT_C(0.86895054425058238),
  MAAC_FLT_C(0.86970868704226556),
  MAAC_FLT_C(0.87046478332539767),
  MAAC_FLT_C(0.8712188313208109),
  MAAC_FLT_C(0.8719708292541577),
  MAAC_FLT_C(0.8727207753559143),
  MAAC_FLT_C(0.87346866786138488),
  MAAC_FLT_C(0.8742145050107063),
  MAAC_FLT_C(0.87495828504885154),
  MAAC_FLT_C(0.8757000062256346),
  MAAC_FLT_C(0.87643966679571361),
  MAAC_FLT_C(0.87717726501859594),
  MAAC_FLT_C(0.87791279915864173),
  MAAC_FLT_C(0.87864626748506813),
  MAAC_FLT_C(0.87937766827195318),
  MAAC_FLT_C(0.88010699979824036),
  MAAC_FLT_C(0.88083426034774204),
  MAAC_FLT_C(0.88155944820914378),
  MAAC_FLT_C(0.8822825616760086),
  MAAC_FLT_C(0.88300359904678072),
  MAAC_FLT_C(0.88372255862478966),
  MAAC_FLT_C(0.8844394387182537),
  MAAC_FLT_C(0.88515423764028511),
  MAAC_FLT_C(0.88586695370889279),
  MAAC_FLT_C(0.88657758524698704),
  MAAC_FLT_C(0.88728613058238315),
  MAAC_FLT_C(0.88799258804780556),
  MAAC_FLT_C(0.88869695598089171),
  MAAC_FLT_C(0.88939923272419552),
  MAAC_FLT_C(0.89009941662519221),
  MAAC_FLT_C(0.89079750603628149),
  MAAC_FLT_C(0.89149349931479138),
  MAAC_FLT_C(0.89218739482298248),
  MAAC_FLT_C(0.89287919092805168),
  MAAC_FLT_C(0.89356888600213602),
  MAAC_FLT_C(0.89425647842231604),
  MAAC_FLT_C(0.89494196657062075),
  MAAC_FLT_C(0.89562534883403),
  MAAC_FLT_C(0.89630662360447966),
  MAAC_FLT_C(0.89698578927886397),
  MAAC_FLT_C(0.89766284425904075),
  MAAC_FLT_C(0.89833778695183419),
  MAAC_FLT_C(0.89901061576903907),
  MAAC_FLT_C(0.89968132912742393),
  MAAC_FLT_C(0.9003499254487356),
  MAAC_FLT_C(0.90101640315970233),
  MAAC_FLT_C(0.90168076069203773),
  MAAC_FLT_C(0.9023429964824442),
  MAAC_FLT_C(0.90300310897261704),
  MAAC_FLT_C(0.90366109660924798),
  MAAC_FLT_C(0.90431695784402832),
  MAAC_FLT_C(0.90497069113365325),
  MAAC_FLT_C(0.90562229493982516),
  MAAC_FLT_C(0.90627176772925766),
  MAAC_FLT_C(0.90691910797367803),
  MAAC_FLT_C(0.90756431414983252),
  MAAC_FLT_C(0.9082073847394887),
  MAAC_FLT_C(0.90884831822943912),
  MAAC_FLT_C(0.90948711311150543),
  MAAC_FLT_C(0.91012376788254157),
  MAAC_FLT_C(0.91075828104443757),
  MAAC_FLT_C(0.91139065110412232),
  MAAC_FLT_C(0.91202087657356823),
  MAAC_FLT_C(0.9126489559697939),
  MAAC_FLT_C(0.91327488781486776),
  MAAC_FLT_C(0.91389867063591168),
  MAAC_FLT_C(0.91452030296510445),
  MAAC_FLT_C(0.91513978333968526),
  MAAC_FLT_C(0.91575711030195672),
  MAAC_FLT_C(0.91637228239928914),
  MAAC_FLT_C(0.91698529818412289),
  MAAC_FLT_C(0.91759615621397295),
  MAAC_FLT_C(0.9182048550514309),
  MAAC_FLT_C(0.91881139326416994),
  MAAC_FLT_C(0.91941576942494696),
  MAAC_FLT_C(0.92001798211160657),
  MAAC_FLT_C(0.92061802990708386),
  MAAC_FLT_C(0.92121591139940873),
  MAAC_FLT_C(0.92181162518170812),
  MAAC_FLT_C(0.92240516985220988),
  MAAC_FLT_C(0.92299654401424625),
  MAAC_FLT_C(0.92358574627625656),
  MAAC_FLT_C(0.9241727752517912),
  MAAC_FLT_C(0.92475762955951391),
  MAAC_FLT_C(0.9253403078232062),
  MAAC_FLT_C(0.92592080867176996),
  MAAC_FLT_C(0.92649913073923051),
  MAAC_FLT_C(0.9270752726647401),
  MAAC_FLT_C(0.92764923309258118),
  MAAC_FLT_C(0.92822101067216944),
  MAAC_FLT_C(0.92879060405805702),
  MAAC_FLT_C(0.9293580119099355),
  MAAC_FLT_C(0.92992323289263956),
  MAAC_FLT_C(0.93048626567614978),
  MAAC_FLT_C(0.93104710893559517),
  MAAC_FLT_C(0.93160576135125783),
  MAAC_FLT_C(0.93216222160857432),
  MAAC_FLT_C(0.93271648839814025),
  MAAC_FLT_C(0.93326856041571205),
  MAAC_FLT_C(0.93381843636221096),
  MAAC_FLT_C(0.9343661149437259),
  MAAC_FLT_C(0.93491159487151609),
  MAAC_FLT_C(0.93545487486201462),
  MAAC_FLT_C(0.9359959536368313),
  MAAC_FLT_C(0.9365348299227555),
  MAAC_FLT_C(0.93707150245175919),
  MAAC_FLT_C(0.93760596996099999),
  MAAC_FLT_C(0.93813823119282436),
  MAAC_FLT_C(0.93866828489477017),
  MAAC_FLT_C(0.9391961298195699),
  MAAC_FLT_C(0.93972176472515334),
  MAAC_FLT_C(0.94024518837465088),
  MAAC_FLT_C(0.94076639953639607),
  MAAC_FLT_C(0.94128539698392866),
  MAAC_FLT_C(0.94180217949599765),
  MAAC_FLT_C(0.94231674585656378),
  MAAC_FLT_C(0.94282909485480271),
  MAAC_FLT_C(0.94333922528510772),
  MAAC_FLT_C(0.94384713594709269),
  MAAC_FLT_C(0.94435282564559475),
  MAAC_FLT_C(0.94485629319067721),
  MAAC_FLT_C(0.94535753739763229),
  MAAC_FLT_C(0.94585655708698391),
  MAAC_FLT_C(0.94635335108449059),
  MAAC_FLT_C(0.946847918221148),
  MAAC_FLT_C(0.94734025733319194),
  MAAC_FLT_C(0.94783036726210101),
  MAAC_FLT_C(0.94831824685459909),
  MAAC_FLT_C(0.94880389496265838),
  MAAC_FLT_C(0.94928731044350201),
  MAAC_FLT_C(0.94976849215960668),
  MAAC_FLT_C(0.95024743897870523),
  MAAC_FLT_C(0.95072414977378961),
  MAAC_FLT_C(0.95119862342311323),
  MAAC_FLT_C(0.95167085881019386),
  MAAC_FLT_C(0.95214085482381583),
  MAAC_FLT_C(0.95260861035803324),
  MAAC_FLT_C(0.9530741243121722),
  MAAC_FLT_C(0.95353739559083328),
  MAAC_FLT_C(0.95399842310389449),
  MAAC_FLT_C(0.95445720576651349),
  MAAC_FLT_C(0.95491374249913052),
  MAAC_FLT_C(0.95536803222747024),
  MAAC_FLT_C(0.95582007388254542),
  MAAC_FLT_C(0.95626986640065814),
  MAAC_FLT_C(0.95671740872340305),
  MAAC_FLT_C(0.9571626997976701),
  MAAC_FLT_C(0.95760573857564624),
  MAAC_FLT_C(0.9580465240148186),
  MAAC_FLT_C(0.9584850550779761),
  MAAC_FLT_C(0.95892133073321306),
  MAAC_FLT_C(0.95935534995393079),
  MAAC_FLT_C(0.9597871117188399),
  MAAC_FLT_C(0.96021661501196343),
  MAAC_FLT_C(0.96064385882263847),
  MAAC_FLT_C(0.96106884214551935),
  MAAC_FLT_C(0.961491563980579),
  MAAC_FLT_C(0.9619120233331121),
  MAAC_FLT_C(0.9623302192137374),
  MAAC_FLT_C(0.96274615063839941),
  MAAC_FLT_C(0.96315981662837136),
  MAAC_FLT_C(0.96357121621025721),
  MAAC_FLT_C(0.96398034841599411),
  MAAC_FLT_C(0.96438721228285429),
  MAAC_FLT_C(0.9647918068534479),
  MAAC_FLT_C(0.96519413117572472),
  MAAC_FLT_C(0.96559418430297683),
  MAAC_FLT_C(0.96599196529384057),
  MAAC_FLT_C(0.96638747321229879),
  MAAC_FLT_C(0.96678070712768327),
  MAAC_FLT_C(0.96717166611467664),
  MAAC_FLT_C(0.96756034925331436),
  MAAC_FLT_C(0.9679467556289878),
  MAAC_FLT_C(0.9683308843324453),
  MAAC_FLT_C(0.96871273445979478),
  MAAC_FLT_C(0.9690923051125061),
  MAAC_FLT_C(0.96946959539741295),
  MAAC_FLT_C(0.96984460442671483),
  MAAC_FLT_C(0.97021733131797916),
  MAAC_FLT_C(0.97058777519414363),
  MAAC_FLT_C(0.97095593518351797),
  MAAC_FLT_C(0.97132181041978616),
  MAAC_FLT_C(0.97168540004200854),
  MAAC_FLT_C(0.9720467031946235),
  MAAC_FLT_C(0.97240571902744977),
  MAAC_FLT_C(0.97276244669568857),
  MAAC_FLT_C(0.97311688535992513),
  MAAC_FLT_C(0.97346903418613095),
  MAAC_FLT_C(0.9738188923456661),
  MAAC_FLT_C(0.97416645901528032),
  MAAC_FLT_C(0.97451173337711572),
  MAAC_FLT_C(0.97485471461870843),
  MAAC_FLT_C(0.97519540193299037),
  MAAC_FLT_C(0.97553379451829136),
  MAAC_FLT_C(0.97586989157834103),
  MAAC_FLT_C(0.97620369232227056),
  MAAC_FLT_C(0.97653519596461447),
  MAAC_FLT_C(0.97686440172531264),
  MAAC_FLT_C(0.97719130882971228),
  MAAC_FLT_C(0.97751591650856928),
  MAAC_FLT_C(0.97783822399805043),
  MAAC_FLT_C(0.97815823053973505),
  MAAC_FLT_C(0.97847593538061683),
  MAAC_FLT_C(0.97879133777310567),
  MAAC_FLT_C(0.97910443697502925),
  MAAC_FLT_C(0.97941523224963478),
  MAAC_FLT_C(0.97972372286559117),
  MAAC_FLT_C(0.98002990809698998),
  MAAC_FLT_C(0.98033378722334796),
  MAAC_FLT_C(0.98063535952960812),
  MAAC_FLT_C(0.98093462430614164),
  MAAC_FLT_C(0.98123158084874973),
  MAAC_FLT_C(0.98152622845866466),
  MAAC_FLT_C(0.9818185664425525),
  MAAC_FLT_C(0.98210859411251361),
  MAAC_FLT_C(0.98239631078608469),
  MAAC_FLT_C(0.98268171578624086),
  MAAC_FLT_C(0.98296480844139644),
  MAAC_FLT_C(0.98324558808540707),
  MAAC_FLT_C(0.98352405405757126),
  MAAC_FLT_C(0.98380020570263149),
  MAAC_FLT_C(0.98407404237077645),
  MAAC_FLT_C(0.9843455634176419),
  MAAC_FLT_C(0.9846147682043126),
  MAAC_FLT_C(0.9848816560973237),
  MAAC_FLT_C(0.98514622646866223),
  MAAC_FLT_C(0.98540847869576842),
  MAAC_FLT_C(0.98566841216153755),
  MAAC_FLT_C(0.98592602625432113),
  MAAC_FLT_C(0.98618132036792827),
  MAAC_FLT_C(0.98643429390162707),
  MAAC_FLT_C(0.98668494626014669),
  MAAC_FLT_C(0.98693327685367771),
  MAAC_FLT_C(0.98717928509787434),
  MAAC_FLT_C(0.98742297041385541),
  MAAC_FLT_C(0.98766433222820571),
  MAAC_FLT_C(0.98790336997297779),
  MAAC_FLT_C(0.98814008308569257),
  MAAC_FLT_C(0.98837447100934128),
  MAAC_FLT_C(0.98860653319238645),
  MAAC_FLT_C(0.98883626908876354),
  MAAC_FLT_C(0.98906367815788154),
  MAAC_FLT_C(0.98928875986462517),
  MAAC_FLT_C(0.98951151367935519),
  MAAC_FLT_C(0.98973193907791057),
  MAAC_FLT_C(0.98995003554160899),
  MAAC_FLT_C(0.9901658025572484),
  MAAC_FLT_C(0.99037923961710816),
  MAAC_FLT_C(0.99059034621895015),
  MAAC_FLT_C(0.99079912186602037),
  MAAC_FLT_C(0.99100556606704937),
  MAAC_FLT_C(0.99120967833625406),
  MAAC_FLT_C(0.99141145819333854),
  MAAC_FLT_C(0.99161090516349537),
  MAAC_FLT_C(0.99180801877740643),
  MAAC_FLT_C(0.99200279857124452),
  MAAC_FLT_C(0.99219524408667392),
  MAAC_FLT_C(0.99238535487085167),
  MAAC_FLT_C(0.99257313047642881),
  MAAC_FLT_C(0.99275857046155114),
  MAAC_FLT_C(0.99294167438986047),
  MAAC_FLT_C(0.99312244183049558),
  MAAC_FLT_C(0.99330087235809328),
  MAAC_FLT_C(0.99347696555278919),
  MAAC_FLT_C(0.99365072100021912),
  MAAC_FLT_C(0.99382213829151966),
  MAAC_FLT_C(0.99399121702332938),
  MAAC_FLT_C(0.99415795679778973),
  MAAC_FLT_C(0.99432235722254581),
  MAAC_FLT_C(0.9944844179107476),
  MAAC_FLT_C(0.99464413848105071),
  MAAC_FLT_C(0.99480151855761711),
  MAAC_FLT_C(0.99495655777011638),
  MAAC_FLT_C(0.99510925575372611),
  MAAC_FLT_C(0.99525961214913339),
  MAAC_FLT_C(0.9954076266025349),
  MAAC_FLT_C(0.99555329876563847),
  MAAC_FLT_C(0.99569662829566352),
  MAAC_FLT_C(0.99583761485534161),
  MAAC_FLT_C(0.99597625811291779),
  MAAC_FLT_C(0.99611255774215113),
  MAAC_FLT_C(0.99624651342231552),
  MAAC_FLT_C(0.99637812483820021),
  MAAC_FLT_C(0.99650739168011082),
  MAAC_FLT_C(0.9966343136438699),
  MAAC_FLT_C(0.996758890430818),
  MAAC_FLT_C(0.99688112174781385),
  MAAC_FLT_C(0.99700100730723529),
  MAAC_FLT_C(0.99711854682697998),
  MAAC_FLT_C(0.99723374003046616),
  MAAC_FLT_C(0.99734658664663323),
  MAAC_FLT_C(0.99745708640994191),
  MAAC_FLT_C(0.99756523906037575),
  MAAC_FLT_C(0.997671044343441),
  MAAC_FLT_C(0.99777450201016782),
  MAAC_FLT_C(0.99787561181711015),
  MAAC_FLT_C(0.99797437352634699),
  MAAC_FLT_C(0.99807078690548234),
  MAAC_FLT_C(0.99816485172764624),
  MAAC_FLT_C(0.99825656777149518),
  MAAC_FLT_C(0.99834593482121237),
  MAAC_FLT_C(0.99843295266650844),
  MAAC_FLT_C(0.99851762110262221),
  MAAC_FLT_C(0.99859993993032037),
  MAAC_FLT_C(0.99867990895589909),
  MAAC_FLT_C(0.99875752799118334),
  MAAC_FLT_C(0.99883279685352799),
  MAAC_FLT_C(0.99890571536581829),
  MAAC_FLT_C(0.99897628335646982),
  MAAC_FLT_C(0.99904450065942929),
  MAAC_FLT_C(0.99911036711417489),
  MAAC_FLT_C(0.99917388256571638),
  MAAC_FLT_C(0.99923504686459585),
  MAAC_FLT_C(0.99929385986688779),
  MAAC_FLT_C(0.99935032143419944),
  MAAC_FLT_C(0.9994044314336713),
  MAAC_FLT_C(0.99945618973797734),
  MAAC_FLT_C(0.99950559622532531),
  MAAC_FLT_C(0.99955265077945699),
  MAAC_FLT_C(0.99959735328964838),
  MAAC_FLT_C(0.9996397036507102),
  MAAC_FLT_C(0.99967970176298793),
  MAAC_FLT_C(0.99971734753236219),
  MAAC_FLT_C(0.99975264087024884),
  MAAC_FLT_C(0.99978558169359921),
  MAAC_FLT_C(0.99981616992490041),
  MAAC_FLT_C(0.99984440549217524),
  MAAC_FLT_C(0.99987028832898295),
  MAAC_FLT_C(0.99989381837441849),
  MAAC_FLT_C(0.99991499557311347),
  MAAC_FLT_C(0.999933819875236),
  MAAC_FLT_C(0.99995029123649048),
  MAAC_FLT_C(0.99996440961811828),
  MAAC_FLT_C(0.99997617498689761),
  MAAC_FLT_C(0.9999855873151432),
  MAAC_FLT_C(0.99999264658070719),
  MAAC_FLT_C(0.99999735276697821),
  MAAC_FLT_C(0.99999970586288223)
};

static const maac_flt maac_window_sin_128[128] = {
  MAAC_FLT_C(0.0061358846491544753),
  MAAC_FLT_C(0.01840672990580482),
  MAAC_FLT_C(0.030674803176636626),
  MAAC_FLT_C(0.04293825693494082),
  MAAC_FLT_C(0.055195244349689934),
  MAAC_FLT_C(0.067443919563664051),
  MAAC_FLT_C(0.079682437971430126),
  MAAC_FLT_C(0.091908956497132724),
  MAAC_FLT_C(0.10412163387205459),
  MAAC_FLT_C(0.11631863091190475),
  MAAC_FLT_C(0.12849811079379317),
  MAAC_FLT_C(0.14065823933284921),
  MAAC_FLT_C(0.15279718525844344),
  MAAC_FLT_C(0.16491312048996992),
  MAAC_FLT_C(0.17700422041214875),
  MAAC_FLT_C(0.18906866414980619),
  MAAC_FLT_C(0.2011046348420919),
  MAAC_FLT_C(0.21311031991609136),
  MAAC_FLT_C(0.22508391135979283),
  MAAC_FLT_C(0.2370236059943672),
  MAAC_FLT_C(0.24892760574572015),
  MAAC_FLT_C(0.26079411791527551),
  MAAC_FLT_C(0.27262135544994898),
  MAAC_FLT_C(0.28440753721127188),
  MAAC_FLT_C(0.29615088824362379),
  MAAC_FLT_C(0.30784964004153487),
  MAAC_FLT_C(0.31950203081601569),
  MAAC_FLT_C(0.33110630575987643),
  MAAC_FLT_C(0.34266071731199438),
  MAAC_FLT_C(0.35416352542049034),
  MAAC_FLT_C(0.36561299780477385),
  MAAC_FLT_C(0.37700741021641826),
  MAAC_FLT_C(0.38834504669882625),
  MAAC_FLT_C(0.39962419984564679),
  MAAC_FLT_C(0.41084317105790391),
  MAAC_FLT_C(0.42200027079979968),
  MAAC_FLT_C(0.43309381885315196),
  MAAC_FLT_C(0.4441221445704292),
  MAAC_FLT_C(0.45508358712634384),
  MAAC_FLT_C(0.46597649576796618),
  MAAC_FLT_C(0.47679923006332209),
  MAAC_FLT_C(0.487550160148436),
  MAAC_FLT_C(0.49822766697278187),
  MAAC_FLT_C(0.50883014254310699),
  MAAC_FLT_C(0.51935599016558964),
  MAAC_FLT_C(0.52980362468629461),
  MAAC_FLT_C(0.54017147272989285),
  MAAC_FLT_C(0.55045797293660481),
  MAAC_FLT_C(0.56066157619733603),
  MAAC_FLT_C(0.57078074588696726),
  MAAC_FLT_C(0.58081395809576453),
  MAAC_FLT_C(0.59075970185887416),
  MAAC_FLT_C(0.60061647938386897),
  MAAC_FLT_C(0.61038280627630948),
  MAAC_FLT_C(0.6200572117632891),
  MAAC_FLT_C(0.62963823891492698),
  MAAC_FLT_C(0.63912444486377573),
  MAAC_FLT_C(0.64851440102211244),
  MAAC_FLT_C(0.65780669329707864),
  MAAC_FLT_C(0.66699992230363747),
  MAAC_FLT_C(0.67609270357531592),
  MAAC_FLT_C(0.68508366777270036),
  MAAC_FLT_C(0.693971460889654),
  MAAC_FLT_C(0.7027547444572253),
  MAAC_FLT_C(0.71143219574521643),
  MAAC_FLT_C(0.72000250796138165),
  MAAC_FLT_C(0.7284643904482252),
  MAAC_FLT_C(0.73681656887736979),
  MAAC_FLT_C(0.74505778544146595),
  MAAC_FLT_C(0.75318679904361241),
  MAAC_FLT_C(0.76120238548426178),
  MAAC_FLT_C(0.76910333764557959),
  MAAC_FLT_C(0.77688846567323244),
  MAAC_FLT_C(0.78455659715557524),
  MAAC_FLT_C(0.79210657730021239),
  MAAC_FLT_C(0.79953726910790501),
  MAAC_FLT_C(0.80684755354379922),
  MAAC_FLT_C(0.8140363297059483),
  MAAC_FLT_C(0.82110251499110465),
  MAAC_FLT_C(0.8280450452577558),
  MAAC_FLT_C(0.83486287498638001),
  MAAC_FLT_C(0.84155497743689833),
  MAAC_FLT_C(0.84812034480329712),
  MAAC_FLT_C(0.85455798836540053),
  MAAC_FLT_C(0.86086693863776731),
  MAAC_FLT_C(0.86704624551569265),
  MAAC_FLT_C(0.87309497841829009),
  MAAC_FLT_C(0.87901222642863341),
  MAAC_FLT_C(0.88479709843093779),
  MAAC_FLT_C(0.89044872324475788),
  MAAC_FLT_C(0.89596624975618511),
  MAAC_FLT_C(0.90134884704602203),
  MAAC_FLT_C(0.90659570451491533),
  MAAC_FLT_C(0.91170603200542988),
  MAAC_FLT_C(0.9166790599210427),
  MAAC_FLT_C(0.9215140393420419),
  MAAC_FLT_C(0.92621024213831127),
  MAAC_FLT_C(0.93076696107898371),
  MAAC_FLT_C(0.9351835099389475),
  MAAC_FLT_C(0.93945922360218992),
  MAAC_FLT_C(0.94359345816196039),
  MAAC_FLT_C(0.94758559101774109),
  MAAC_FLT_C(0.95143502096900834),
  MAAC_FLT_C(0.95514116830577067),
  MAAC_FLT_C(0.9587034748958716),
  MAAC_FLT_C(0.96212140426904158),
  MAAC_FLT_C(0.9653944416976894),
  MAAC_FLT_C(0.96852209427441727),
  MAAC_FLT_C(0.97150389098625178),
  MAAC_FLT_C(0.97433938278557586),
  MAAC_FLT_C(0.97702814265775439),
  MAAC_FLT_C(0.97956976568544052),
  MAAC_FLT_C(0.98196386910955524),
  MAAC_FLT_C(0.98421009238692903),
  MAAC_FLT_C(0.98630809724459867),
  MAAC_FLT_C(0.98825756773074946),
  MAAC_FLT_C(0.99005821026229712),
  MAAC_FLT_C(0.99170975366909953),
  MAAC_FLT_C(0.9932119492347945),
  MAAC_FLT_C(0.99456457073425542),
  MAAC_FLT_C(0.99576741446765982),
  MAAC_FLT_C(0.99682029929116567),
  MAAC_FLT_C(0.99772306664419164),
  MAAC_FLT_C(0.99847558057329477),
  MAAC_FLT_C(0.99907772775264536),
  MAAC_FLT_C(0.99952941750109314),
  MAAC_FLT_C(0.9998305817958234),
  MAAC_FLT_C(0.99998117528260111)
};

MAAC_PRIVATE
void
maac_filterbank(maac_flt* samples, maac_flt* overlap, const maac_filterbank_params* p) {
    maac_u16 i = 0;
    const maac_flt* window = NULL;
    const maac_flt* window_prev = NULL;

    const maac_u16 long_len = 1024;
    const maac_u16 short_len = long_len / 8;

    const maac_u16 mid_len = (long_len - short_len) / 2; /* 448 */
    const maac_u16 trans_len = short_len/2; /* 64 */

    maac_u8 wseq = p->window_sequence;
    maac_u8 window_shape = p->window_shape;
    maac_u8 window_shape_prev = p->window_shape_prev;

    switch(wseq) {
        case MAAC_WINDOW_SEQUENCE_ONLY_LONG: {
            maac_imdct(samples, long_len*2);

            window = window_shape == 1 ? maac_window_kbd_1024 : maac_window_sin_1024;
            window_prev = window_shape_prev == 1 ? maac_window_kbd_1024 : maac_window_sin_1024;

            /* add windowed overlap from previous frame into output of this frame */
            for(i=0;i<long_len;i++) {
                samples[i] = overlap[i] + samples[i] * window_prev[i];
            }

            /* copy windowed imdct output to overlap */
            for(i=0;i<long_len;i++) {
                overlap[i] = samples[i+long_len] * window[long_len - 1 - i];
            }
            break;
        }

        case MAAC_WINDOW_SEQUENCE_LONG_START: {
            maac_imdct(samples, long_len*2);

            /* this is something of a transitional sequence, it goes
               between an ONLY_LONG and an EIGHT_SHORT, so our current window
               is a short window */
            window = window_shape == 1 ? maac_window_kbd_128 : maac_window_sin_128;
            window_prev = window_shape == 1 ? maac_window_kbd_1024 : maac_window_sin_1024;

            /* add windowed overlap from previous long frame to our output */
            for(i=0;i<long_len;i++) {
                samples[i] = overlap[i] + samples[i] * window_prev[i];
            }

            /* then from the spec - our window is
                  1.0 for samples 1024 <= n < 1472 (1024 - 128 / 2)
                  kbd/sin_right window for 1472 <= n < 1600
                  0 for 1600 <= n < 2048 */

            for(i=0;i<mid_len;i++) {
                overlap[i] = samples[long_len+i];
            }
            for(i=0;i<short_len;i++) {
                overlap[mid_len+i] = samples[long_len+mid_len+i] * window[short_len-i-1];
            }
            for(i=0;i<mid_len;i++) {
                overlap[mid_len+short_len+i] = MAAC_FLT_C(0.0);
            }

            break;
        }

        case MAAC_WINDOW_SEQUENCE_EIGHT_SHORT: {
            /* TODO should I just have cofficients decoder just write to
             window offsets of 256 instead of 128? Unsure if that would make
             things like mid/side processing more complex */
            maac_memcpy(&samples[7 * 2 * short_len], &samples[7 * short_len], sizeof(maac_flt) * short_len);
            maac_memcpy(&samples[6 * 2 * short_len], &samples[6 * short_len], sizeof(maac_flt) * short_len);
            maac_memcpy(&samples[5 * 2 * short_len], &samples[5 * short_len], sizeof(maac_flt) * short_len);
            maac_memcpy(&samples[4 * 2 * short_len], &samples[4 * short_len], sizeof(maac_flt) * short_len);
            maac_memcpy(&samples[3 * 2 * short_len], &samples[3 * short_len], sizeof(maac_flt) * short_len);
            maac_memcpy(&samples[2 * 2 * short_len], &samples[2 * short_len], sizeof(maac_flt) * short_len);
            maac_memcpy(&samples[1 * 2 * short_len], &samples[1 * short_len], sizeof(maac_flt) * short_len);
            /* 0 just stays in-place */

            /* now for the 8 small IMDCTs */
            maac_imdct(&samples[0 * 2 * short_len], 2 * short_len);
            maac_imdct(&samples[1 * 2 * short_len], 2 * short_len);
            maac_imdct(&samples[2 * 2 * short_len], 2 * short_len);
            maac_imdct(&samples[3 * 2 * short_len], 2 * short_len);
            maac_imdct(&samples[4 * 2 * short_len], 2 * short_len);
            maac_imdct(&samples[5 * 2 * short_len], 2 * short_len);
            maac_imdct(&samples[6 * 2 * short_len], 2 * short_len);
            maac_imdct(&samples[7 * 2 * short_len], 2 * short_len);

            window = window_shape == 1 ? maac_window_kbd_128 : maac_window_sin_128;
            window_prev = window_shape_prev == 1 ? maac_window_kbd_128 : maac_window_sin_128;

            /* these are the overlap and add values - which means what to add to overlap
            * for    0 <= n <  448, 0
            * for  448 <= n <  576, samples[n-488]  * W0[ n-448] *** w0 is previous window here and only here
            * for  576 <= n <  704, samples[n-488]  * W0[ n-448] + samples[ n-576] * W1[ n-576]
            * for  704 <= n <  832, samples[n-576]  * W1[ n-576] + samples[ n-704] * W2[ n-704]
            * for  832 <= n <  960, samples[n-704]  * W2[ n-704] + samples[ n-832] * W3[ n-832]
            * for  960 <= n < 1088, samples[n-832]  * W3[ n-832] + samples[ n-960] * W4[ n-960]
            * for 1088 <= n < 1216, samples[n-960]  * W4[ n-960] + samples[n-1088] * W5[n-1088]
            * for 1216 <= n < 1344, samples[n-1088] * W5[n-1088] + samples[n-1216] * W6[n-1216]
            * for 1344 <= n < 1472, samples[n-1216] * W6[n-1216] + samples[n-1344] * W7[n-1344]
            * for 1472 <= n < 1600, samples[n-1344] * W7[n-1344]
            * for 1600 <= n < 2047, 0
            * What's confusing is MPEG notates all of this as like:
                samples[i,n-448] (for first sample)
                samples[i,n-576] (for second sample)
                but - they *really* mean
                samples[ (window * 128) + (n-448) ]
                samples[ (window * 128) + (n-576) ]
                so you end up reading out of
                samples[(1 * 128) + 0] and
                samples[(2 * 128) + 0]
                I think they wrote all this assuming you're really tracking 8 different windows
                with individual indexes which, no.

                So anyways we can express all of these as (1 * 128), (2 * 128), etc

              To spell it out more explicitly in terms of what samples are written/read from
              for each of those chunks:

              - write to    0 <  448, read from     overlap
              - write to  448 <  576, read from     0 < 128
              - write to  576 <  704, read from   128 < 384
              - write to  704 <  832, read from   384 < 640
              - write to  832 <  960, read from   640 < 896
              - write to  960 < 1088, read from   896 < 960
              - write to 1088 < 1216, read from 1152 < 1408
              - write to 1216 < 1344, read from 1408 < 1664
              - write to 1344 < 1472, read from 1664 < 1920
              - write to 1472 < 1600, read from 1920 < 2048
              - write to 1600 < 2047, read from nowhere (just write 0s)

              In order to do this in-place with a single buffer, we need to avoid writing to somewhere
              we want to read from later.

            */

            for(i=0;i<short_len;i++) {
                /* write to 0 < 128, read from 0 < 128 */
                samples[i+(0*short_len)] =
                    overlap[mid_len+i+(0*short_len)] +
                    samples[i+(0*short_len)] * window_prev[i]
                ;

                /* write to 128 < 256, read from 128 < 384
                   final destination is 576 < 704 */
                samples[i+(1*short_len)] =
                    overlap[mid_len+i+(1*short_len)] +
                    samples[i+(1*short_len)] * window[short_len-1-i] +
                    samples[i+(2*short_len)] * window[i]
                ;

                /* write to 384 < 512, read from 384 < 640
                   final destination is samples[704 -> 832] */
                samples[i+(3*short_len)] =
                    overlap[mid_len+i+(2*short_len)] +
                    samples[i+(3*short_len)] * window[short_len-1-i] + 
                    samples[i+(4*short_len)] * window[i]
                ;

                /* write to 640 < 768, read from 640 < 896  
                   final destination is samples[832 -> 960] */
                samples[i+(5*short_len)] =
                    overlap[mid_len+i+(3*short_len)] +
                    samples[i+(5*short_len)] * window[short_len-1-i] +
                    samples[i+(6*short_len)] * window[i]
                ;

                /* write to 896 < 1088, read from 896 < 1152 */
                /* final destination is split:
                  samples[960 -> 1024]
                  overlap[0-64] */
                samples[i+(7*short_len)] =
                    (i < trans_len ? overlap[mid_len+i+(4*short_len)] : MAAC_FLT_C(0.0)) +
                    samples[i+(7*short_len)] * window[short_len-1-i] +
                    samples[i+(8*short_len)] * window[i]
                ;

                /* write to 1152 - 1280, read from 1152 - 1408 */
                /* final destination is overlap[64-192] */
                samples[i+(9*short_len)] =
                    samples[i+(9*short_len)] * window[short_len-1-i] +
                    samples[i+(10*short_len)] * window[i]
                ;

                /* write to 1408 - 1536, read from 1408 - 1664 */
                /* final destination is overlap[192-320] */
                samples[i+(11*short_len)] =
                    samples[i+(11*short_len)] * window[short_len-1-i] +
                    samples[i+(12*short_len)] * window[i]
                ;

                /* write to 1664 < 1792, read from 1664 < 1920,
                   final destination is overlap [320-448] */
                samples[i+(13*short_len)] =
                    samples[i+(13*short_len)] * window[short_len-1-i] +
                    samples[i+(14*short_len)] * window[i]
                ;

                /* write to 1920 < 2048, read from 1920 < 2048,
                   final destination is overlap[448-576] */
                samples[i+(15*short_len)] =
                    samples[i+(15*short_len)] * window[short_len-1-i]
                ;
            }

            /* first we scooch all the higher parts up together to clear some room */
            maac_memcpy(&samples[(14 * short_len)], &samples[(13 * short_len)], sizeof(maac_flt) * short_len);
            maac_memcpy(&samples[(13 * short_len)], &samples[(11 * short_len)], sizeof(maac_flt) * short_len);
            maac_memcpy(&samples[(12 * short_len)], &samples[(9 * short_len)],  sizeof(maac_flt) * short_len);
            maac_memcpy(&samples[trans_len + (11 * short_len)],  &samples[trans_len + (7 * short_len)],  sizeof(maac_flt) * trans_len);

            /* now samples[1472 - 2048] is together, and will become overlap[0-576] later */
            /* so we can bump up and collapse buffer[0-960] to buffer[448-1024] */

            /* copies 896 - 960 to 960 - 1024 */
            maac_memcpy(&samples[mid_len + (4 * short_len)],  &samples[(7 * short_len)],  sizeof(maac_flt) * trans_len);

            /* copies 640 < 768 to 832 < 960 */
            maac_memcpy(&samples[mid_len + (3 * short_len)],  &samples[(5 * short_len)],  sizeof(maac_flt) * short_len);

            /* copies 384 < 512 to 704 < 832 */
            maac_memcpy(&samples[mid_len + (2 * short_len)],  &samples[(3 * short_len)],  sizeof(maac_flt) * short_len);

            /* copies 128 < 256 to 576 < 704 */
            maac_memcpy(&samples[mid_len + (1 * short_len)],  &samples[(1 * short_len)],  sizeof(maac_flt) * short_len);

            /* copies   0 < 128 to 448 < 576 */
            maac_memcpy(&samples[mid_len + (0 * short_len)],  &samples[(0 * short_len)],  sizeof(maac_flt) * short_len);

            /* write to 0 < 448, read from overlap */
            for(i=0;i<mid_len;i++) {
                samples[i] = overlap[i];
            }

            /* overlap[0-576] is located in [trans_len + mid_len + (4 * 128)] */
            maac_memcpy(&overlap[0], &samples[trans_len + (11 * short_len)], sizeof(maac_flt) * (mid_len + short_len));

            /* clear out the rest of overlap (576 - 1024) */
            for(i=0;i<mid_len;i++) {
                overlap[i+mid_len+short_len] = MAAC_FLT_C(0.0);
            }

            break;
        }

        case MAAC_WINDOW_SEQUENCE_LONG_STOP: {
            maac_imdct(samples, long_len*2);

            /* this is used to go from an eight-short back to an only-long */

            window = p->window_shape == 1 ? maac_window_kbd_1024 : maac_window_sin_1024;
            window_prev = p->window_shape_prev == 1 ? maac_window_kbd_128 : maac_window_sin_128;

            for(i=0;i<mid_len;i++) {
                samples[i] = overlap[i];
            }
            for(i=0;i<short_len;i++) {
                samples[i+mid_len] =
                    overlap[i+mid_len] +
                    samples[i+mid_len] * window_prev[i]
                ;
            }
            for(i=0;i<mid_len;i++) {
                samples[i+mid_len+short_len] =
                    overlap[i+mid_len+short_len] +
                    samples[i+mid_len+short_len]
                ;
            }
            for(i=0;i<long_len;i++) {
                overlap[i] = samples[long_len+i] * window[long_len-1-i];
            }
            break;
        }

    }

}

/* we use index 0 for the scalefactors codebook */
struct maac_codebook_bits_index_entry {
  maac_u8 start;
  maac_u8 end;
};

typedef struct maac_codebook_bits_index_entry maac_codebook_bits_index_entry;

static const maac_codebook_bits_index_entry maac_codebook_bits_indexes[12] = {
  { 0, 19 },
  { 19, 30 },
  { 30, 39 },
  { 39, 55 },
  { 55, 67 },
  { 67, 80 },
  { 80, 91 },
  { 91, 103 },
  { 103, 113 },
  { 113, 128 },
  { 128, 140 },
  { 140, 152 }
};

static const maac_u8 maac_codebook_bits[152] = {
  /* scalefactor codebook */
    1, 0, 1, 3, 2, 4, 3, 5,
    4, 6, 6, 6, 5, 8, 4, 7,
    3, 7, 46,
  /* codebook 1 */
    1, 0, 0, 0, 8, 0, 24, 0,
    24, 8, 16,
  /* codebook 2 */
    0, 0, 1, 1, 7, 24, 15, 19,
    14,
  /* codebook 3 */
    1, 0, 0, 4, 2, 6, 3, 5,
    15, 15, 8, 9, 3, 3, 5, 2,
  /* codebook 4 */
    0, 0, 0, 10, 6, 0, 9, 21,
    8, 14, 11, 2,
  /* codebook 5 */
    1, 0, 0, 4, 4, 0, 4, 12,
    12, 12, 18, 10, 4,
  /* codebook 6 */
    0, 0, 0, 9, 0, 16, 13, 8,
    23, 8, 4,
  /* codebook 7 */
    1, 0, 2, 1, 0, 4, 5, 10,
    14, 15, 8, 4,
  /* codebook 8 */
    0, 0, 1, 5, 7, 10, 14, 15,
    8, 4,
  /* codebook 9 */
    1, 0, 2, 1, 0, 4, 3, 8,
    11, 20, 31, 38, 32, 14, 4,
  /* codebook 10 */
    0, 0, 0, 3, 8, 14, 17, 25,
    31, 41, 22, 8,
  /* codebook 11 */
    0, 0, 0, 2, 6, 7, 16, 59,
    55, 95, 43, 6
};

struct maac_codebook_index_entry {
  maac_u16 start;
  maac_u16 end;
};

typedef struct maac_codebook_index_entry maac_codebook_index_entry;

#ifdef MAAC_COMPACT_CODEBOOKS

typedef maac_u8 maac_codebook_entry;

/* indexed by byte value, which we'll manually unpack into the proper types */
static const maac_codebook_index_entry maac_codebook_indexes[12] = {
  { 0, 484 },
  { 484, 727 },
  { 727, 970 },
  { 970, 1213 },
  { 1213, 1456 },
  { 1456, 1699 },
  { 1699, 1942 },
  { 1942, 2134 },
  { 2134, 2326 },
  { 2326, 2833 },
  { 2833, 3340 },
  { 3340, 4496 }
};

static const maac_codebook_entry maac_codebook[4496] = {
  /* scalefactor codebook -- 3-byte codeword, 1-byte index */
  /*  1 bits */ 0x00, 0x00, 0x00, 0x3c,
  /*  3 bits */ 0x00, 0x00, 0x04, 0x3b,
  /*  4 bits */ 0x00, 0x00, 0x0a, 0x3d,
  /*  4 bits */ 0x00, 0x00, 0x0b, 0x3a,
  /*  4 bits */ 0x00, 0x00, 0x0c, 0x3e,
  /*  5 bits */ 0x00, 0x00, 0x1a, 0x39,
  /*  5 bits */ 0x00, 0x00, 0x1b, 0x3f,
  /*  6 bits */ 0x00, 0x00, 0x38, 0x38,
  /*  6 bits */ 0x00, 0x00, 0x39, 0x40,
  /*  6 bits */ 0x00, 0x00, 0x3a, 0x37,
  /*  6 bits */ 0x00, 0x00, 0x3b, 0x41,
  /*  7 bits */ 0x00, 0x00, 0x78, 0x42,
  /*  7 bits */ 0x00, 0x00, 0x79, 0x36,
  /*  7 bits */ 0x00, 0x00, 0x7a, 0x43,
  /*  8 bits */ 0x00, 0x00, 0xf6, 0x35,
  /*  8 bits */ 0x00, 0x00, 0xf7, 0x44,
  /*  8 bits */ 0x00, 0x00, 0xf8, 0x34,
  /*  8 bits */ 0x00, 0x00, 0xf9, 0x45,
  /*  8 bits */ 0x00, 0x00, 0xfa, 0x33,
  /*  9 bits */ 0x00, 0x01, 0xf6, 0x46,
  /*  9 bits */ 0x00, 0x01, 0xf7, 0x32,
  /*  9 bits */ 0x00, 0x01, 0xf8, 0x31,
  /*  9 bits */ 0x00, 0x01, 0xf9, 0x47,
  /* 10 bits */ 0x00, 0x03, 0xf4, 0x48,
  /* 10 bits */ 0x00, 0x03, 0xf5, 0x30,
  /* 10 bits */ 0x00, 0x03, 0xf6, 0x49,
  /* 10 bits */ 0x00, 0x03, 0xf7, 0x2f,
  /* 10 bits */ 0x00, 0x03, 0xf8, 0x4a,
  /* 10 bits */ 0x00, 0x03, 0xf9, 0x2e,
  /* 11 bits */ 0x00, 0x07, 0xf4, 0x4c,
  /* 11 bits */ 0x00, 0x07, 0xf5, 0x4b,
  /* 11 bits */ 0x00, 0x07, 0xf6, 0x4d,
  /* 11 bits */ 0x00, 0x07, 0xf7, 0x4e,
  /* 11 bits */ 0x00, 0x07, 0xf8, 0x2d,
  /* 11 bits */ 0x00, 0x07, 0xf9, 0x2b,
  /* 12 bits */ 0x00, 0x0f, 0xf4, 0x2c,
  /* 12 bits */ 0x00, 0x0f, 0xf5, 0x4f,
  /* 12 bits */ 0x00, 0x0f, 0xf6, 0x2a,
  /* 12 bits */ 0x00, 0x0f, 0xf7, 0x29,
  /* 12 bits */ 0x00, 0x0f, 0xf8, 0x50,
  /* 12 bits */ 0x00, 0x0f, 0xf9, 0x28,
  /* 13 bits */ 0x00, 0x1f, 0xf4, 0x51,
  /* 13 bits */ 0x00, 0x1f, 0xf5, 0x27,
  /* 13 bits */ 0x00, 0x1f, 0xf6, 0x52,
  /* 13 bits */ 0x00, 0x1f, 0xf7, 0x26,
  /* 13 bits */ 0x00, 0x1f, 0xf8, 0x53,
  /* 14 bits */ 0x00, 0x3f, 0xf2, 0x25,
  /* 14 bits */ 0x00, 0x3f, 0xf3, 0x23,
  /* 14 bits */ 0x00, 0x3f, 0xf4, 0x55,
  /* 14 bits */ 0x00, 0x3f, 0xf5, 0x21,
  /* 14 bits */ 0x00, 0x3f, 0xf6, 0x24,
  /* 14 bits */ 0x00, 0x3f, 0xf7, 0x22,
  /* 14 bits */ 0x00, 0x3f, 0xf8, 0x54,
  /* 14 bits */ 0x00, 0x3f, 0xf9, 0x20,
  /* 15 bits */ 0x00, 0x7f, 0xf4, 0x57,
  /* 15 bits */ 0x00, 0x7f, 0xf5, 0x59,
  /* 15 bits */ 0x00, 0x7f, 0xf6, 0x1e,
  /* 15 bits */ 0x00, 0x7f, 0xf7, 0x1f,
  /* 16 bits */ 0x00, 0xff, 0xf0, 0x56,
  /* 16 bits */ 0x00, 0xff, 0xf1, 0x1d,
  /* 16 bits */ 0x00, 0xff, 0xf2, 0x1a,
  /* 16 bits */ 0x00, 0xff, 0xf3, 0x1b,
  /* 16 bits */ 0x00, 0xff, 0xf4, 0x1c,
  /* 16 bits */ 0x00, 0xff, 0xf5, 0x18,
  /* 16 bits */ 0x00, 0xff, 0xf6, 0x58,
  /* 17 bits */ 0x01, 0xff, 0xee, 0x19,
  /* 17 bits */ 0x01, 0xff, 0xef, 0x16,
  /* 17 bits */ 0x01, 0xff, 0xf0, 0x17,
  /* 18 bits */ 0x03, 0xff, 0xe2, 0x5a,
  /* 18 bits */ 0x03, 0xff, 0xe3, 0x15,
  /* 18 bits */ 0x03, 0xff, 0xe4, 0x13,
  /* 18 bits */ 0x03, 0xff, 0xe5, 0x03,
  /* 18 bits */ 0x03, 0xff, 0xe6, 0x01,
  /* 18 bits */ 0x03, 0xff, 0xe7, 0x02,
  /* 18 bits */ 0x03, 0xff, 0xe8, 0x00,
  /* 19 bits */ 0x07, 0xff, 0xd2, 0x62,
  /* 19 bits */ 0x07, 0xff, 0xd3, 0x63,
  /* 19 bits */ 0x07, 0xff, 0xd4, 0x64,
  /* 19 bits */ 0x07, 0xff, 0xd5, 0x65,
  /* 19 bits */ 0x07, 0xff, 0xd6, 0x66,
  /* 19 bits */ 0x07, 0xff, 0xd7, 0x75,
  /* 19 bits */ 0x07, 0xff, 0xd8, 0x61,
  /* 19 bits */ 0x07, 0xff, 0xd9, 0x5b,
  /* 19 bits */ 0x07, 0xff, 0xda, 0x5c,
  /* 19 bits */ 0x07, 0xff, 0xdb, 0x5d,
  /* 19 bits */ 0x07, 0xff, 0xdc, 0x5e,
  /* 19 bits */ 0x07, 0xff, 0xdd, 0x5f,
  /* 19 bits */ 0x07, 0xff, 0xde, 0x60,
  /* 19 bits */ 0x07, 0xff, 0xdf, 0x68,
  /* 19 bits */ 0x07, 0xff, 0xe0, 0x6f,
  /* 19 bits */ 0x07, 0xff, 0xe1, 0x70,
  /* 19 bits */ 0x07, 0xff, 0xe2, 0x71,
  /* 19 bits */ 0x07, 0xff, 0xe3, 0x72,
  /* 19 bits */ 0x07, 0xff, 0xe4, 0x73,
  /* 19 bits */ 0x07, 0xff, 0xe5, 0x74,
  /* 19 bits */ 0x07, 0xff, 0xe6, 0x6e,
  /* 19 bits */ 0x07, 0xff, 0xe7, 0x69,
  /* 19 bits */ 0x07, 0xff, 0xe8, 0x6a,
  /* 19 bits */ 0x07, 0xff, 0xe9, 0x6b,
  /* 19 bits */ 0x07, 0xff, 0xea, 0x6c,
  /* 19 bits */ 0x07, 0xff, 0xeb, 0x6d,
  /* 19 bits */ 0x07, 0xff, 0xec, 0x76,
  /* 19 bits */ 0x07, 0xff, 0xed, 0x06,
  /* 19 bits */ 0x07, 0xff, 0xee, 0x08,
  /* 19 bits */ 0x07, 0xff, 0xef, 0x09,
  /* 19 bits */ 0x07, 0xff, 0xf0, 0x0a,
  /* 19 bits */ 0x07, 0xff, 0xf1, 0x05,
  /* 19 bits */ 0x07, 0xff, 0xf2, 0x67,
  /* 19 bits */ 0x07, 0xff, 0xf3, 0x78,
  /* 19 bits */ 0x07, 0xff, 0xf4, 0x77,
  /* 19 bits */ 0x07, 0xff, 0xf5, 0x04,
  /* 19 bits */ 0x07, 0xff, 0xf6, 0x07,
  /* 19 bits */ 0x07, 0xff, 0xf7, 0x0f,
  /* 19 bits */ 0x07, 0xff, 0xf8, 0x10,
  /* 19 bits */ 0x07, 0xff, 0xf9, 0x12,
  /* 19 bits */ 0x07, 0xff, 0xfa, 0x14,
  /* 19 bits */ 0x07, 0xff, 0xfb, 0x11,
  /* 19 bits */ 0x07, 0xff, 0xfc, 0x0b,
  /* 19 bits */ 0x07, 0xff, 0xfd, 0x0c,
  /* 19 bits */ 0x07, 0xff, 0xfe, 0x0e,
  /* 19 bits */ 0x07, 0xff, 0xff, 0x0d,
  /* codebook 1 -- 2-byte codeword, 1-byte index */
  /*  1 bits */ 0x00, 0x00, 0x28,
  /*  5 bits */ 0x00, 0x10, 0x43,
  /*  5 bits */ 0x00, 0x11, 0x0d,
  /*  5 bits */ 0x00, 0x12, 0x27,
  /*  5 bits */ 0x00, 0x13, 0x31,
  /*  5 bits */ 0x00, 0x14, 0x29,
  /*  5 bits */ 0x00, 0x15, 0x25,
  /*  5 bits */ 0x00, 0x16, 0x2b,
  /*  5 bits */ 0x00, 0x17, 0x1f,
  /*  7 bits */ 0x00, 0x60, 0x3a,
  /*  7 bits */ 0x00, 0x61, 0x16,
  /*  7 bits */ 0x00, 0x62, 0x26,
  /*  7 bits */ 0x00, 0x63, 0x2e,
  /*  7 bits */ 0x00, 0x64, 0x22,
  /*  7 bits */ 0x00, 0x65, 0x2a,
  /*  7 bits */ 0x00, 0x66, 0x4c,
  /*  7 bits */ 0x00, 0x67, 0x24,
  /*  7 bits */ 0x00, 0x68, 0x04,
  /*  7 bits */ 0x00, 0x69, 0x1c,
  /*  7 bits */ 0x00, 0x6a, 0x40,
  /*  7 bits */ 0x00, 0x6b, 0x30,
  /*  7 bits */ 0x00, 0x6c, 0x10,
  /*  7 bits */ 0x00, 0x6d, 0x2c,
  /*  7 bits */ 0x00, 0x6e, 0x46,
  /*  7 bits */ 0x00, 0x6f, 0x20,
  /*  7 bits */ 0x00, 0x70, 0x34,
  /*  7 bits */ 0x00, 0x71, 0x32,
  /*  7 bits */ 0x00, 0x72, 0x0a,
  /*  7 bits */ 0x00, 0x73, 0x44,
  /*  7 bits */ 0x00, 0x74, 0x0c,
  /*  7 bits */ 0x00, 0x75, 0x42,
  /*  7 bits */ 0x00, 0x76, 0x0e,
  /*  7 bits */ 0x00, 0x77, 0x1e,
  /*  9 bits */ 0x01, 0xe0, 0x49,
  /*  9 bits */ 0x01, 0xe1, 0x13,
  /*  9 bits */ 0x01, 0xe2, 0x3d,
  /*  9 bits */ 0x01, 0xe3, 0x33,
  /*  9 bits */ 0x01, 0xe4, 0x2f,
  /*  9 bits */ 0x01, 0xe5, 0x23,
  /*  9 bits */ 0x01, 0xe6, 0x21,
  /*  9 bits */ 0x01, 0xe7, 0x37,
  /*  9 bits */ 0x01, 0xe8, 0x41,
  /*  9 bits */ 0x01, 0xe9, 0x2d,
  /*  9 bits */ 0x01, 0xea, 0x19,
  /*  9 bits */ 0x01, 0xeb, 0x0f,
  /*  9 bits */ 0x01, 0xec, 0x07,
  /*  9 bits */ 0x01, 0xed, 0x1d,
  /*  9 bits */ 0x01, 0xee, 0x3b,
  /*  9 bits */ 0x01, 0xef, 0x39,
  /*  9 bits */ 0x01, 0xf0, 0x15,
  /*  9 bits */ 0x01, 0xf1, 0x01,
  /*  9 bits */ 0x01, 0xf2, 0x1b,
  /*  9 bits */ 0x01, 0xf3, 0x35,
  /*  9 bits */ 0x01, 0xf4, 0x45,
  /*  9 bits */ 0x01, 0xf5, 0x4d,
  /*  9 bits */ 0x01, 0xf6, 0x17,
  /*  9 bits */ 0x01, 0xf7, 0x4f,
  /* 10 bits */ 0x03, 0xf0, 0x05,
  /* 10 bits */ 0x03, 0xf1, 0x09,
  /* 10 bits */ 0x03, 0xf2, 0x4b,
  /* 10 bits */ 0x03, 0xf3, 0x3f,
  /* 10 bits */ 0x03, 0xf4, 0x0b,
  /* 10 bits */ 0x03, 0xf5, 0x03,
  /* 10 bits */ 0x03, 0xf6, 0x11,
  /* 10 bits */ 0x03, 0xf7, 0x47,
  /* 11 bits */ 0x07, 0xf0, 0x3c,
  /* 11 bits */ 0x07, 0xf1, 0x14,
  /* 11 bits */ 0x07, 0xf2, 0x18,
  /* 11 bits */ 0x07, 0xf3, 0x38,
  /* 11 bits */ 0x07, 0xf4, 0x50,
  /* 11 bits */ 0x07, 0xf5, 0x08,
  /* 11 bits */ 0x07, 0xf6, 0x48,
  /* 11 bits */ 0x07, 0xf7, 0x06,
  /* 11 bits */ 0x07, 0xf8, 0x00,
  /* 11 bits */ 0x07, 0xf9, 0x4a,
  /* 11 bits */ 0x07, 0xfa, 0x3e,
  /* 11 bits */ 0x07, 0xfb, 0x1a,
  /* 11 bits */ 0x07, 0xfc, 0x12,
  /* 11 bits */ 0x07, 0xfd, 0x02,
  /* 11 bits */ 0x07, 0xfe, 0x36,
  /* 11 bits */ 0x07, 0xff, 0x4e,
  /* codebook 2 -- 2-byte codeword, 1-byte index */
  /*  3 bits */ 0x00, 0x00, 0x28,
  /*  4 bits */ 0x00, 0x02, 0x43,
  /*  5 bits */ 0x00, 0x06, 0x0d,
  /*  5 bits */ 0x00, 0x07, 0x29,
  /*  5 bits */ 0x00, 0x08, 0x25,
  /*  5 bits */ 0x00, 0x09, 0x27,
  /*  5 bits */ 0x00, 0x0a, 0x1f,
  /*  5 bits */ 0x00, 0x0b, 0x2b,
  /*  5 bits */ 0x00, 0x0c, 0x31,
  /*  6 bits */ 0x00, 0x1a, 0x22,
  /*  6 bits */ 0x00, 0x1b, 0x16,
  /*  6 bits */ 0x00, 0x1c, 0x2e,
  /*  6 bits */ 0x00, 0x1d, 0x2a,
  /*  6 bits */ 0x00, 0x1e, 0x30,
  /*  6 bits */ 0x00, 0x1f, 0x26,
  /*  6 bits */ 0x00, 0x20, 0x0c,
  /*  6 bits */ 0x00, 0x21, 0x3a,
  /*  6 bits */ 0x00, 0x22, 0x40,
  /*  6 bits */ 0x00, 0x23, 0x04,
  /*  6 bits */ 0x00, 0x24, 0x24,
  /*  6 bits */ 0x00, 0x25, 0x46,
  /*  6 bits */ 0x00, 0x26, 0x44,
  /*  6 bits */ 0x00, 0x27, 0x20,
  /*  6 bits */ 0x00, 0x28, 0x10,
  /*  6 bits */ 0x00, 0x29, 0x32,
  /*  6 bits */ 0x00, 0x2a, 0x1c,
  /*  6 bits */ 0x00, 0x2b, 0x0e,
  /*  6 bits */ 0x00, 0x2c, 0x1e,
  /*  6 bits */ 0x00, 0x2d, 0x0a,
  /*  6 bits */ 0x00, 0x2e, 0x4c,
  /*  6 bits */ 0x00, 0x2f, 0x34,
  /*  6 bits */ 0x00, 0x30, 0x2c,
  /*  6 bits */ 0x00, 0x31, 0x42,
  /*  7 bits */ 0x00, 0x64, 0x2f,
  /*  7 bits */ 0x00, 0x65, 0x41,
  /*  7 bits */ 0x00, 0x66, 0x13,
  /*  7 bits */ 0x00, 0x67, 0x21,
  /*  7 bits */ 0x00, 0x68, 0x3d,
  /*  7 bits */ 0x00, 0x69, 0x4b,
  /*  7 bits */ 0x00, 0x6a, 0x47,
  /*  7 bits */ 0x00, 0x6b, 0x19,
  /*  7 bits */ 0x00, 0x6c, 0x1d,
  /*  7 bits */ 0x00, 0x6d, 0x4f,
  /*  7 bits */ 0x00, 0x6e, 0x0f,
  /*  7 bits */ 0x00, 0x6f, 0x01,
  /*  7 bits */ 0x00, 0x70, 0x0b,
  /*  7 bits */ 0x00, 0x71, 0x37,
  /*  7 bits */ 0x00, 0x72, 0x49,
  /*  8 bits */ 0x00, 0xe6, 0x3b,
  /*  8 bits */ 0x00, 0xe7, 0x15,
  /*  8 bits */ 0x00, 0xe8, 0x07,
  /*  8 bits */ 0x00, 0xe9, 0x11,
  /*  8 bits */ 0x00, 0xea, 0x05,
  /*  8 bits */ 0x00, 0xeb, 0x03,
  /*  8 bits */ 0x00, 0xec, 0x1b,
  /*  8 bits */ 0x00, 0xed, 0x45,
  /*  8 bits */ 0x00, 0xee, 0x3f,
  /*  8 bits */ 0x00, 0xef, 0x2d,
  /*  8 bits */ 0x00, 0xf0, 0x35,
  /*  8 bits */ 0x00, 0xf1, 0x17,
  /*  8 bits */ 0x00, 0xf2, 0x09,
  /*  8 bits */ 0x00, 0xf3, 0x33,
  /*  8 bits */ 0x00, 0xf4, 0x39,
  /*  8 bits */ 0x00, 0xf5, 0x23,
  /*  8 bits */ 0x00, 0xf6, 0x4d,
  /*  8 bits */ 0x00, 0xf7, 0x3c,
  /*  8 bits */ 0x00, 0xf8, 0x14,
  /*  9 bits */ 0x01, 0xf2, 0x38,
  /*  9 bits */ 0x01, 0xf3, 0x00,
  /*  9 bits */ 0x01, 0xf4, 0x18,
  /*  9 bits */ 0x01, 0xf5, 0x1a,
  /*  9 bits */ 0x01, 0xf6, 0x50,
  /*  9 bits */ 0x01, 0xf7, 0x06,
  /*  9 bits */ 0x01, 0xf8, 0x3e,
  /*  9 bits */ 0x01, 0xf9, 0x12,
  /*  9 bits */ 0x01, 0xfa, 0x08,
  /*  9 bits */ 0x01, 0xfb, 0x48,
  /*  9 bits */ 0x01, 0xfc, 0x36,
  /*  9 bits */ 0x01, 0xfd, 0x02,
  /*  9 bits */ 0x01, 0xfe, 0x4a,
  /*  9 bits */ 0x01, 0xff, 0x4e,
  /* codebook 3 -- 2-byte codeword, 1-byte index */
  /*  1 bits */ 0x00, 0x00, 0x00,
  /*  4 bits */ 0x00, 0x08, 0x1b,
  /*  4 bits */ 0x00, 0x09, 0x01,
  /*  4 bits */ 0x00, 0x0a, 0x09,
  /*  4 bits */ 0x00, 0x0b, 0x03,
  /*  5 bits */ 0x00, 0x18, 0x24,
  /*  5 bits */ 0x00, 0x19, 0x04,
  /*  6 bits */ 0x00, 0x34, 0x0c,
  /*  6 bits */ 0x00, 0x35, 0x0a,
  /*  6 bits */ 0x00, 0x36, 0x1e,
  /*  6 bits */ 0x00, 0x37, 0x0d,
  /*  6 bits */ 0x00, 0x38, 0x1c,
  /*  6 bits */ 0x00, 0x39, 0x27,
  /*  7 bits */ 0x00, 0x74, 0x28,
  /*  7 bits */ 0x00, 0x75, 0x1f,
  /*  7 bits */ 0x00, 0x76, 0x25,
  /*  8 bits */ 0x00, 0xee, 0x36,
  /*  8 bits */ 0x00, 0xef, 0x02,
  /*  8 bits */ 0x00, 0xf0, 0x05,
  /*  8 bits */ 0x00, 0xf1, 0x3f,
  /*  8 bits */ 0x00, 0xf2, 0x30,
  /*  9 bits */ 0x01, 0xe6, 0x07,
  /*  9 bits */ 0x01, 0xe7, 0x10,
  /*  9 bits */ 0x01, 0xe8, 0x2d,
  /*  9 bits */ 0x01, 0xe9, 0x0e,
  /*  9 bits */ 0x01, 0xea, 0x42,
  /*  9 bits */ 0x01, 0xeb, 0x06,
  /*  9 bits */ 0x01, 0xec, 0x15,
  /*  9 bits */ 0x01, 0xed, 0x0f,
  /*  9 bits */ 0x01, 0xee, 0x12,
  /*  9 bits */ 0x01, 0xef, 0x0b,
  /*  9 bits */ 0x01, 0xf0, 0x39,
  /*  9 bits */ 0x01, 0xf1, 0x31,
  /*  9 bits */ 0x01, 0xf2, 0x16,
  /*  9 bits */ 0x01, 0xf3, 0x2a,
  /*  9 bits */ 0x01, 0xf4, 0x2b,
  /* 10 bits */ 0x03, 0xea, 0x2e,
  /* 10 bits */ 0x03, 0xeb, 0x21,
  /* 10 bits */ 0x03, 0xec, 0x22,
  /* 10 bits */ 0x03, 0xed, 0x13,
  /* 10 bits */ 0x03, 0xee, 0x43,
  /* 10 bits */ 0x03, 0xef, 0x29,
  /* 10 bits */ 0x03, 0xf0, 0x40,
  /* 10 bits */ 0x03, 0xf1, 0x20,
  /* 10 bits */ 0x03, 0xf2, 0x08,
  /* 10 bits */ 0x03, 0xf3, 0x11,
  /* 10 bits */ 0x03, 0xf4, 0x4b,
  /* 10 bits */ 0x03, 0xf5, 0x33,
  /* 10 bits */ 0x03, 0xf6, 0x1d,
  /* 10 bits */ 0x03, 0xf7, 0x37,
  /* 10 bits */ 0x03, 0xf8, 0x19,
  /* 11 bits */ 0x07, 0xf2, 0x48,
  /* 11 bits */ 0x07, 0xf3, 0x34,
  /* 11 bits */ 0x07, 0xf4, 0x26,
  /* 11 bits */ 0x07, 0xf5, 0x3a,
  /* 11 bits */ 0x07, 0xf6, 0x2c,
  /* 11 bits */ 0x07, 0xf7, 0x4c,
  /* 11 bits */ 0x07, 0xf8, 0x18,
  /* 11 bits */ 0x07, 0xf9, 0x17,
  /* 12 bits */ 0x0f, 0xf4, 0x23,
  /* 12 bits */ 0x0f, 0xf5, 0x49,
  /* 12 bits */ 0x0f, 0xf6, 0x45,
  /* 12 bits */ 0x0f, 0xf7, 0x4e,
  /* 12 bits */ 0x0f, 0xf8, 0x1a,
  /* 12 bits */ 0x0f, 0xf9, 0x4f,
  /* 12 bits */ 0x0f, 0xfa, 0x46,
  /* 12 bits */ 0x0f, 0xfb, 0x32,
  /* 12 bits */ 0x0f, 0xfc, 0x35,
  /* 13 bits */ 0x1f, 0xfa, 0x14,
  /* 13 bits */ 0x1f, 0xfb, 0x3c,
  /* 13 bits */ 0x1f, 0xfc, 0x2f,
  /* 14 bits */ 0x3f, 0xfa, 0x3d,
  /* 14 bits */ 0x3f, 0xfb, 0x44,
  /* 14 bits */ 0x3f, 0xfc, 0x41,
  /* 15 bits */ 0x7f, 0xfa, 0x50,
  /* 15 bits */ 0x7f, 0xfb, 0x4d,
  /* 15 bits */ 0x7f, 0xfc, 0x47,
  /* 15 bits */ 0x7f, 0xfd, 0x3b,
  /* 15 bits */ 0x7f, 0xfe, 0x38,
  /* 16 bits */ 0xff, 0xfe, 0x4a,
  /* 16 bits */ 0xff, 0xff, 0x3e,
  /* codebook 4 -- 2-byte codeword, 1-byte index */
  /*  4 bits */ 0x00, 0x00, 0x28,
  /*  4 bits */ 0x00, 0x01, 0x0d,
  /*  4 bits */ 0x00, 0x02, 0x25,
  /*  4 bits */ 0x00, 0x03, 0x27,
  /*  4 bits */ 0x00, 0x04, 0x1f,
  /*  4 bits */ 0x00, 0x05, 0x1b,
  /*  4 bits */ 0x00, 0x06, 0x24,
  /*  4 bits */ 0x00, 0x07, 0x00,
  /*  4 bits */ 0x00, 0x08, 0x04,
  /*  4 bits */ 0x00, 0x09, 0x1e,
  /*  5 bits */ 0x00, 0x14, 0x1c,
  /*  5 bits */ 0x00, 0x15, 0x0c,
  /*  5 bits */ 0x00, 0x16, 0x01,
  /*  5 bits */ 0x00, 0x17, 0x0a,
  /*  5 bits */ 0x00, 0x18, 0x03,
  /*  5 bits */ 0x00, 0x19, 0x09,
  /*  7 bits */ 0x00, 0x68, 0x43,
  /*  7 bits */ 0x00, 0x69, 0x2b,
  /*  7 bits */ 0x00, 0x6a, 0x31,
  /*  7 bits */ 0x00, 0x6b, 0x29,
  /*  7 bits */ 0x00, 0x6c, 0x42,
  /*  7 bits */ 0x00, 0x6d, 0x40,
  /*  7 bits */ 0x00, 0x6e, 0x30,
  /*  7 bits */ 0x00, 0x6f, 0x3a,
  /*  7 bits */ 0x00, 0x70, 0x10,
  /*  8 bits */ 0x00, 0xe2, 0x0e,
  /*  8 bits */ 0x00, 0xe3, 0x2a,
  /*  8 bits */ 0x00, 0xe4, 0x16,
  /*  8 bits */ 0x00, 0xe5, 0x20,
  /*  8 bits */ 0x00, 0xe6, 0x2e,
  /*  8 bits */ 0x00, 0xe7, 0x26,
  /*  8 bits */ 0x00, 0xe8, 0x22,
  /*  8 bits */ 0x00, 0xe9, 0x3f,
  /*  8 bits */ 0x00, 0xea, 0x39,
  /*  8 bits */ 0x00, 0xeb, 0x2d,
  /*  8 bits */ 0x00, 0xec, 0x37,
  /*  8 bits */ 0x00, 0xed, 0x0b,
  /*  8 bits */ 0x00, 0xee, 0x15,
  /*  8 bits */ 0x00, 0xef, 0x05,
  /*  8 bits */ 0x00, 0xf0, 0x0f,
  /*  8 bits */ 0x00, 0xf1, 0x13,
  /*  8 bits */ 0x00, 0xf2, 0x1d,
  /*  8 bits */ 0x00, 0xf3, 0x07,
  /*  8 bits */ 0x00, 0xf4, 0x21,
  /*  8 bits */ 0x00, 0xf5, 0x36,
  /*  8 bits */ 0x00, 0xf6, 0x02,
  /*  9 bits */ 0x01, 0xee, 0x12,
  /*  9 bits */ 0x01, 0xef, 0x06,
  /*  9 bits */ 0x01, 0xf0, 0x34,
  /*  9 bits */ 0x01, 0xf1, 0x4c,
  /*  9 bits */ 0x01, 0xf2, 0x46,
  /*  9 bits */ 0x01, 0xf3, 0x2c,
  /*  9 bits */ 0x01, 0xf4, 0x32,
  /*  9 bits */ 0x01, 0xf5, 0x44,
  /* 10 bits */ 0x03, 0xec, 0x33,
  /* 10 bits */ 0x03, 0xed, 0x4b,
  /* 10 bits */ 0x03, 0xee, 0x45,
  /* 10 bits */ 0x03, 0xef, 0x19,
  /* 10 bits */ 0x03, 0xf0, 0x11,
  /* 10 bits */ 0x03, 0xf1, 0x49,
  /* 10 bits */ 0x03, 0xf2, 0x17,
  /* 10 bits */ 0x03, 0xf3, 0x3d,
  /* 10 bits */ 0x03, 0xf4, 0x23,
  /* 10 bits */ 0x03, 0xf5, 0x4f,
  /* 10 bits */ 0x03, 0xf6, 0x2f,
  /* 10 bits */ 0x03, 0xf7, 0x3b,
  /* 10 bits */ 0x03, 0xf8, 0x41,
  /* 10 bits */ 0x03, 0xf9, 0x35,
  /* 11 bits */ 0x07, 0xf4, 0x47,
  /* 11 bits */ 0x07, 0xf5, 0x4d,
  /* 11 bits */ 0x07, 0xf6, 0x18,
  /* 11 bits */ 0x07, 0xf7, 0x48,
  /* 11 bits */ 0x07, 0xf8, 0x08,
  /* 11 bits */ 0x07, 0xf9, 0x3c,
  /* 11 bits */ 0x07, 0xfa, 0x14,
  /* 11 bits */ 0x07, 0xfb, 0x38,
  /* 11 bits */ 0x07, 0xfc, 0x50,
  /* 11 bits */ 0x07, 0xfd, 0x1a,
  /* 11 bits */ 0x07, 0xfe, 0x4e,
  /* 12 bits */ 0x0f, 0xfe, 0x4a,
  /* 12 bits */ 0x0f, 0xff, 0x3e,
  /* codebook 5 -- 2-byte codeword, 1-byte index */
  /*  1 bits */ 0x00, 0x00, 0x28,
  /*  4 bits */ 0x00, 0x08, 0x1f,
  /*  4 bits */ 0x00, 0x09, 0x31,
  /*  4 bits */ 0x00, 0x0a, 0x29,
  /*  4 bits */ 0x00, 0x0b, 0x27,
  /*  5 bits */ 0x00, 0x18, 0x30,
  /*  5 bits */ 0x00, 0x19, 0x20,
  /*  5 bits */ 0x00, 0x1a, 0x1e,
  /*  5 bits */ 0x00, 0x1b, 0x32,
  /*  7 bits */ 0x00, 0x70, 0x16,
  /*  7 bits */ 0x00, 0x71, 0x2a,
  /*  7 bits */ 0x00, 0x72, 0x3a,
  /*  7 bits */ 0x00, 0x73, 0x26,
  /*  8 bits */ 0x00, 0xe8, 0x15,
  /*  8 bits */ 0x00, 0xe9, 0x3b,
  /*  8 bits */ 0x00, 0xea, 0x1d,
  /*  8 bits */ 0x00, 0xeb, 0x33,
  /*  8 bits */ 0x00, 0xec, 0x17,
  /*  8 bits */ 0x00, 0xed, 0x39,
  /*  8 bits */ 0x00, 0xee, 0x21,
  /*  8 bits */ 0x00, 0xef, 0x2f,
  /*  8 bits */ 0x00, 0xf0, 0x0d,
  /*  8 bits */ 0x00, 0xf1, 0x43,
  /*  8 bits */ 0x00, 0xf2, 0x25,
  /*  8 bits */ 0x00, 0xf3, 0x2b,
  /*  9 bits */ 0x01, 0xe8, 0x0c,
  /*  9 bits */ 0x01, 0xe9, 0x34,
  /*  9 bits */ 0x01, 0xea, 0x44,
  /*  9 bits */ 0x01, 0xeb, 0x1c,
  /*  9 bits */ 0x01, 0xec, 0x0e,
  /*  9 bits */ 0x01, 0xed, 0x42,
  /*  9 bits */ 0x01, 0xee, 0x2e,
  /*  9 bits */ 0x01, 0xef, 0x22,
  /*  9 bits */ 0x01, 0xf0, 0x18,
  /*  9 bits */ 0x01, 0xf1, 0x3c,
  /*  9 bits */ 0x01, 0xf2, 0x14,
  /*  9 bits */ 0x01, 0xf3, 0x38,
  /* 10 bits */ 0x03, 0xe8, 0x0b,
  /* 10 bits */ 0x03, 0xe9, 0x41,
  /* 10 bits */ 0x03, 0xea, 0x19,
  /* 10 bits */ 0x03, 0xeb, 0x37,
  /* 10 bits */ 0x03, 0xec, 0x45,
  /* 10 bits */ 0x03, 0xed, 0x3d,
  /* 10 bits */ 0x03, 0xee, 0x0f,
  /* 10 bits */ 0x03, 0xef, 0x13,
  /* 10 bits */ 0x03, 0xf0, 0x24,
  /* 10 bits */ 0x03, 0xf1, 0x04,
  /* 10 bits */ 0x03, 0xf2, 0x4d,
  /* 10 bits */ 0x03, 0xf3, 0x4c,
  /* 11 bits */ 0x07, 0xe8, 0x03,
  /* 11 bits */ 0x07, 0xe9, 0x2c,
  /* 11 bits */ 0x07, 0xea, 0x4b,
  /* 11 bits */ 0x07, 0xeb, 0x1b,
  /* 11 bits */ 0x07, 0xec, 0x35,
  /* 11 bits */ 0x07, 0xed, 0x23,
  /* 11 bits */ 0x07, 0xee, 0x05,
  /* 11 bits */ 0x07, 0xef, 0x2d,
  /* 11 bits */ 0x07, 0xf0, 0x40,
  /* 11 bits */ 0x07, 0xf1, 0x0a,
  /* 11 bits */ 0x07, 0xf2, 0x10,
  /* 11 bits */ 0x07, 0xf3, 0x1a,
  /* 11 bits */ 0x07, 0xf4, 0x02,
  /* 11 bits */ 0x07, 0xf5, 0x4e,
  /* 11 bits */ 0x07, 0xf6, 0x36,
  /* 11 bits */ 0x07, 0xf7, 0x3e,
  /* 11 bits */ 0x07, 0xf8, 0x46,
  /* 11 bits */ 0x07, 0xf9, 0x06,
  /* 12 bits */ 0x0f, 0xf4, 0x12,
  /* 12 bits */ 0x0f, 0xf5, 0x4a,
  /* 12 bits */ 0x0f, 0xf6, 0x3f,
  /* 12 bits */ 0x0f, 0xf7, 0x01,
  /* 12 bits */ 0x0f, 0xf8, 0x07,
  /* 12 bits */ 0x0f, 0xf9, 0x47,
  /* 12 bits */ 0x0f, 0xfa, 0x11,
  /* 12 bits */ 0x0f, 0xfb, 0x4f,
  /* 12 bits */ 0x0f, 0xfc, 0x49,
  /* 12 bits */ 0x0f, 0xfd, 0x09,
  /* 13 bits */ 0x1f, 0xfc, 0x48,
  /* 13 bits */ 0x1f, 0xfd, 0x08,
  /* 13 bits */ 0x1f, 0xfe, 0x50,
  /* 13 bits */ 0x1f, 0xff, 0x00,
  /* codebook 6 -- 2-byte codeword, 1-byte index */
  /*  4 bits */ 0x00, 0x00, 0x28,
  /*  4 bits */ 0x00, 0x01, 0x31,
  /*  4 bits */ 0x00, 0x02, 0x27,
  /*  4 bits */ 0x00, 0x03, 0x29,
  /*  4 bits */ 0x00, 0x04, 0x1f,
  /*  4 bits */ 0x00, 0x05, 0x32,
  /*  4 bits */ 0x00, 0x06, 0x20,
  /*  4 bits */ 0x00, 0x07, 0x30,
  /*  4 bits */ 0x00, 0x08, 0x1e,
  /*  6 bits */ 0x00, 0x24, 0x39,
  /*  6 bits */ 0x00, 0x25, 0x3b,
  /*  6 bits */ 0x00, 0x26, 0x17,
  /*  6 bits */ 0x00, 0x27, 0x15,
  /*  6 bits */ 0x00, 0x28, 0x16,
  /*  6 bits */ 0x00, 0x29, 0x21,
  /*  6 bits */ 0x00, 0x2a, 0x3a,
  /*  6 bits */ 0x00, 0x2b, 0x2f,
  /*  6 bits */ 0x00, 0x2c, 0x33,
  /*  6 bits */ 0x00, 0x2d, 0x26,
  /*  6 bits */ 0x00, 0x2e, 0x1d,
  /*  6 bits */ 0x00, 0x2f, 0x2a,
  /*  6 bits */ 0x00, 0x30, 0x38,
  /*  6 bits */ 0x00, 0x31, 0x18,
  /*  6 bits */ 0x00, 0x32, 0x14,
  /*  6 bits */ 0x00, 0x33, 0x3c,
  /*  7 bits */ 0x00, 0x68, 0x0e,
  /*  7 bits */ 0x00, 0x69, 0x44,
  /*  7 bits */ 0x00, 0x6a, 0x42,
  /*  7 bits */ 0x00, 0x6b, 0x22,
  /*  7 bits */ 0x00, 0x6c, 0x0c,
  /*  7 bits */ 0x00, 0x6d, 0x34,
  /*  7 bits */ 0x00, 0x6e, 0x2e,
  /*  7 bits */ 0x00, 0x6f, 0x1c,
  /*  7 bits */ 0x00, 0x70, 0x43,
  /*  7 bits */ 0x00, 0x71, 0x0d,
  /*  7 bits */ 0x00, 0x72, 0x25,
  /*  7 bits */ 0x00, 0x73, 0x2b,
  /*  7 bits */ 0x00, 0x74, 0x45,
  /*  8 bits */ 0x00, 0xea, 0x0b,
  /*  8 bits */ 0x00, 0xeb, 0x19,
  /*  8 bits */ 0x00, 0xec, 0x3d,
  /*  8 bits */ 0x00, 0xed, 0x41,
  /*  8 bits */ 0x00, 0xee, 0x37,
  /*  8 bits */ 0x00, 0xef, 0x13,
  /*  8 bits */ 0x00, 0xf0, 0x0f,
  /*  8 bits */ 0x00, 0xf1, 0x46,
  /*  9 bits */ 0x01, 0xe4, 0x40,
  /*  9 bits */ 0x01, 0xe5, 0x0a,
  /*  9 bits */ 0x01, 0xe6, 0x10,
  /*  9 bits */ 0x01, 0xe7, 0x2d,
  /*  9 bits */ 0x01, 0xe8, 0x1b,
  /*  9 bits */ 0x01, 0xe9, 0x4d,
  /*  9 bits */ 0x01, 0xea, 0x05,
  /*  9 bits */ 0x01, 0xeb, 0x03,
  /*  9 bits */ 0x01, 0xec, 0x35,
  /*  9 bits */ 0x01, 0xed, 0x4b,
  /*  9 bits */ 0x01, 0xee, 0x23,
  /*  9 bits */ 0x01, 0xef, 0x24,
  /*  9 bits */ 0x01, 0xf0, 0x06,
  /*  9 bits */ 0x01, 0xf1, 0x02,
  /*  9 bits */ 0x01, 0xf2, 0x3e,
  /*  9 bits */ 0x01, 0xf3, 0x12,
  /*  9 bits */ 0x01, 0xf4, 0x04,
  /*  9 bits */ 0x01, 0xf5, 0x4e,
  /*  9 bits */ 0x01, 0xf6, 0x4a,
  /*  9 bits */ 0x01, 0xf7, 0x1a,
  /*  9 bits */ 0x01, 0xf8, 0x4c,
  /*  9 bits */ 0x01, 0xf9, 0x36,
  /*  9 bits */ 0x01, 0xfa, 0x2c,
  /* 10 bits */ 0x03, 0xf6, 0x09,
  /* 10 bits */ 0x03, 0xf7, 0x11,
  /* 10 bits */ 0x03, 0xf8, 0x3f,
  /* 10 bits */ 0x03, 0xf9, 0x49,
  /* 10 bits */ 0x03, 0xfa, 0x47,
  /* 10 bits */ 0x03, 0xfb, 0x4f,
  /* 10 bits */ 0x03, 0xfc, 0x07,
  /* 10 bits */ 0x03, 0xfd, 0x01,
  /* 11 bits */ 0x07, 0xfc, 0x50,
  /* 11 bits */ 0x07, 0xfd, 0x08,
  /* 11 bits */ 0x07, 0xfe, 0x00,
  /* 11 bits */ 0x07, 0xff, 0x48,
  /* codebook 7 -- 2-byte codeword, 1-byte index */
  /*  1 bits */ 0x00, 0x00, 0x00,
  /*  3 bits */ 0x00, 0x04, 0x08,
  /*  3 bits */ 0x00, 0x05, 0x01,
  /*  4 bits */ 0x00, 0x0c, 0x09,
  /*  6 bits */ 0x00, 0x34, 0x11,
  /*  6 bits */ 0x00, 0x35, 0x0a,
  /*  6 bits */ 0x00, 0x36, 0x10,
  /*  6 bits */ 0x00, 0x37, 0x02,
  /*  7 bits */ 0x00, 0x70, 0x19,
  /*  7 bits */ 0x00, 0x71, 0x0b,
  /*  7 bits */ 0x00, 0x72, 0x12,
  /*  7 bits */ 0x00, 0x73, 0x18,
  /*  7 bits */ 0x00, 0x74, 0x03,
  /*  8 bits */ 0x00, 0xea, 0x13,
  /*  8 bits */ 0x00, 0xeb, 0x1a,
  /*  8 bits */ 0x00, 0xec, 0x0c,
  /*  8 bits */ 0x00, 0xed, 0x21,
  /*  8 bits */ 0x00, 0xee, 0x0d,
  /*  8 bits */ 0x00, 0xef, 0x29,
  /*  8 bits */ 0x00, 0xf0, 0x1b,
  /*  8 bits */ 0x00, 0xf1, 0x14,
  /*  8 bits */ 0x00, 0xf2, 0x04,
  /*  8 bits */ 0x00, 0xf3, 0x20,
  /*  9 bits */ 0x01, 0xe8, 0x22,
  /*  9 bits */ 0x01, 0xe9, 0x15,
  /*  9 bits */ 0x01, 0xea, 0x2a,
  /*  9 bits */ 0x01, 0xeb, 0x05,
  /*  9 bits */ 0x01, 0xec, 0x31,
  /*  9 bits */ 0x01, 0xed, 0x28,
  /*  9 bits */ 0x01, 0xee, 0x0e,
  /*  9 bits */ 0x01, 0xef, 0x23,
  /*  9 bits */ 0x01, 0xf0, 0x1d,
  /*  9 bits */ 0x01, 0xf1, 0x1c,
  /*  9 bits */ 0x01, 0xf2, 0x2b,
  /*  9 bits */ 0x01, 0xf3, 0x16,
  /*  9 bits */ 0x01, 0xf4, 0x32,
  /*  9 bits */ 0x01, 0xf5, 0x0f,
  /* 10 bits */ 0x03, 0xec, 0x1e,
  /* 10 bits */ 0x03, 0xed, 0x06,
  /* 10 bits */ 0x03, 0xee, 0x30,
  /* 10 bits */ 0x03, 0xef, 0x24,
  /* 10 bits */ 0x03, 0xf0, 0x39,
  /* 10 bits */ 0x03, 0xf1, 0x25,
  /* 10 bits */ 0x03, 0xf2, 0x3a,
  /* 10 bits */ 0x03, 0xf3, 0x2c,
  /* 10 bits */ 0x03, 0xf4, 0x33,
  /* 10 bits */ 0x03, 0xf5, 0x17,
  /* 10 bits */ 0x03, 0xf6, 0x3b,
  /* 10 bits */ 0x03, 0xf7, 0x34,
  /* 10 bits */ 0x03, 0xf8, 0x2d,
  /* 10 bits */ 0x03, 0xf9, 0x26,
  /* 10 bits */ 0x03, 0xfa, 0x1f,
  /* 11 bits */ 0x07, 0xf6, 0x38,
  /* 11 bits */ 0x07, 0xf7, 0x07,
  /* 11 bits */ 0x07, 0xf8, 0x35,
  /* 11 bits */ 0x07, 0xf9, 0x2e,
  /* 11 bits */ 0x07, 0xfa, 0x3c,
  /* 11 bits */ 0x07, 0xfb, 0x27,
  /* 11 bits */ 0x07, 0xfc, 0x2f,
  /* 11 bits */ 0x07, 0xfd, 0x3d,
  /* 12 bits */ 0x0f, 0xfc, 0x3e,
  /* 12 bits */ 0x0f, 0xfd, 0x36,
  /* 12 bits */ 0x0f, 0xfe, 0x37,
  /* 12 bits */ 0x0f, 0xff, 0x3f,
  /* codebook 8 -- 2-byte codeword, 1-byte index */
  /*  3 bits */ 0x00, 0x00, 0x09,
  /*  4 bits */ 0x00, 0x02, 0x11,
  /*  4 bits */ 0x00, 0x03, 0x08,
  /*  4 bits */ 0x00, 0x04, 0x0a,
  /*  4 bits */ 0x00, 0x05, 0x01,
  /*  4 bits */ 0x00, 0x06, 0x12,
  /*  5 bits */ 0x00, 0x0e, 0x00,
  /*  5 bits */ 0x00, 0x0f, 0x10,
  /*  5 bits */ 0x00, 0x10, 0x02,
  /*  5 bits */ 0x00, 0x11, 0x19,
  /*  5 bits */ 0x00, 0x12, 0x0b,
  /*  5 bits */ 0x00, 0x13, 0x1a,
  /*  5 bits */ 0x00, 0x14, 0x13,
  /*  6 bits */ 0x00, 0x2a, 0x1b,
  /*  6 bits */ 0x00, 0x2b, 0x21,
  /*  6 bits */ 0x00, 0x2c, 0x0c,
  /*  6 bits */ 0x00, 0x2d, 0x22,
  /*  6 bits */ 0x00, 0x2e, 0x14,
  /*  6 bits */ 0x00, 0x2f, 0x18,
  /*  6 bits */ 0x00, 0x30, 0x03,
  /*  6 bits */ 0x00, 0x31, 0x23,
  /*  6 bits */ 0x00, 0x32, 0x1c,
  /*  6 bits */ 0x00, 0x33, 0x2a,
  /*  7 bits */ 0x00, 0x68, 0x29,
  /*  7 bits */ 0x00, 0x69, 0x15,
  /*  7 bits */ 0x00, 0x6a, 0x0d,
  /*  7 bits */ 0x00, 0x6b, 0x2b,
  /*  7 bits */ 0x00, 0x6c, 0x1d,
  /*  7 bits */ 0x00, 0x6d, 0x24,
  /*  7 bits */ 0x00, 0x6e, 0x2c,
  /*  7 bits */ 0x00, 0x6f, 0x04,
  /*  7 bits */ 0x00, 0x70, 0x25,
  /*  7 bits */ 0x00, 0x71, 0x20,
  /*  7 bits */ 0x00, 0x72, 0x16,
  /*  7 bits */ 0x00, 0x73, 0x32,
  /*  7 bits */ 0x00, 0x74, 0x31,
  /*  7 bits */ 0x00, 0x75, 0x0e,
  /*  8 bits */ 0x00, 0xec, 0x1e,
  /*  8 bits */ 0x00, 0xed, 0x33,
  /*  8 bits */ 0x00, 0xee, 0x2d,
  /*  8 bits */ 0x00, 0xef, 0x28,
  /*  8 bits */ 0x00, 0xf0, 0x34,
  /*  8 bits */ 0x00, 0xf1, 0x05,
  /*  8 bits */ 0x00, 0xf2, 0x26,
  /*  8 bits */ 0x00, 0xf3, 0x39,
  /*  8 bits */ 0x00, 0xf4, 0x3a,
  /*  8 bits */ 0x00, 0xf5, 0x17,
  /*  8 bits */ 0x00, 0xf6, 0x35,
  /*  8 bits */ 0x00, 0xf7, 0x3b,
  /*  8 bits */ 0x00, 0xf8, 0x0f,
  /*  8 bits */ 0x00, 0xf9, 0x2e,
  /*  8 bits */ 0x00, 0xfa, 0x1f,
  /*  9 bits */ 0x01, 0xf6, 0x36,
  /*  9 bits */ 0x01, 0xf7, 0x3c,
  /*  9 bits */ 0x01, 0xf8, 0x30,
  /*  9 bits */ 0x01, 0xf9, 0x27,
  /*  9 bits */ 0x01, 0xfa, 0x06,
  /*  9 bits */ 0x01, 0xfb, 0x3d,
  /*  9 bits */ 0x01, 0xfc, 0x3e,
  /*  9 bits */ 0x01, 0xfd, 0x37,
  /* 10 bits */ 0x03, 0xfc, 0x2f,
  /* 10 bits */ 0x03, 0xfd, 0x38,
  /* 10 bits */ 0x03, 0xfe, 0x07,
  /* 10 bits */ 0x03, 0xff, 0x3f,
  /* codebook 9 -- 2-byte codeword, 1-byte index */
  /*  1 bits */ 0x00, 0x00, 0x00,
  /*  3 bits */ 0x00, 0x04, 0x0d,
  /*  3 bits */ 0x00, 0x05, 0x01,
  /*  4 bits */ 0x00, 0x0c, 0x0e,
  /*  6 bits */ 0x00, 0x34, 0x1b,
  /*  6 bits */ 0x00, 0x35, 0x0f,
  /*  6 bits */ 0x00, 0x36, 0x1a,
  /*  6 bits */ 0x00, 0x37, 0x02,
  /*  7 bits */ 0x00, 0x70, 0x28,
  /*  7 bits */ 0x00, 0x71, 0x1c,
  /*  7 bits */ 0x00, 0x72, 0x10,
  /*  8 bits */ 0x00, 0xe6, 0x27,
  /*  8 bits */ 0x00, 0xe7, 0x03,
  /*  8 bits */ 0x00, 0xe8, 0x1d,
  /*  8 bits */ 0x00, 0xe9, 0x29,
  /*  8 bits */ 0x00, 0xea, 0x11,
  /*  8 bits */ 0x00, 0xeb, 0x35,
  /*  8 bits */ 0x00, 0xec, 0x1e,
  /*  8 bits */ 0x00, 0xed, 0x12,
  /*  9 bits */ 0x01, 0xdc, 0x36,
  /*  9 bits */ 0x01, 0xdd, 0x2a,
  /*  9 bits */ 0x01, 0xde, 0x04,
  /*  9 bits */ 0x01, 0xdf, 0x34,
  /*  9 bits */ 0x01, 0xe0, 0x42,
  /*  9 bits */ 0x01, 0xe1, 0x1f,
  /*  9 bits */ 0x01, 0xe2, 0x13,
  /*  9 bits */ 0x01, 0xe3, 0x2b,
  /*  9 bits */ 0x01, 0xe4, 0x43,
  /*  9 bits */ 0x01, 0xe5, 0x4f,
  /*  9 bits */ 0x01, 0xe6, 0x37,
  /* 10 bits */ 0x03, 0xce, 0x05,
  /* 10 bits */ 0x03, 0xcf, 0x20,
  /* 10 bits */ 0x03, 0xd0, 0x41,
  /* 10 bits */ 0x03, 0xd1, 0x14,
  /* 10 bits */ 0x03, 0xd2, 0x2c,
  /* 10 bits */ 0x03, 0xd3, 0x15,
  /* 10 bits */ 0x03, 0xd4, 0x69,
  /* 10 bits */ 0x03, 0xd5, 0x38,
  /* 10 bits */ 0x03, 0xd6, 0x44,
  /* 10 bits */ 0x03, 0xd7, 0x50,
  /* 10 bits */ 0x03, 0xd8, 0x5c,
  /* 10 bits */ 0x03, 0xd9, 0x06,
  /* 10 bits */ 0x03, 0xda, 0x6a,
  /* 10 bits */ 0x03, 0xdb, 0x22,
  /* 10 bits */ 0x03, 0xdc, 0x2d,
  /* 10 bits */ 0x03, 0xdd, 0x21,
  /* 10 bits */ 0x03, 0xde, 0x39,
  /* 10 bits */ 0x03, 0xdf, 0x76,
  /* 10 bits */ 0x03, 0xe0, 0x16,
  /* 10 bits */ 0x03, 0xe1, 0x5d,
  /* 11 bits */ 0x07, 0xc4, 0x4e,
  /* 11 bits */ 0x07, 0xc5, 0x45,
  /* 11 bits */ 0x07, 0xc6, 0x51,
  /* 11 bits */ 0x07, 0xc7, 0x6b,
  /* 11 bits */ 0x07, 0xc8, 0x07,
  /* 11 bits */ 0x07, 0xc9, 0x77,
  /* 11 bits */ 0x07, 0xca, 0x2f,
  /* 11 bits */ 0x07, 0xcb, 0x3a,
  /* 11 bits */ 0x07, 0xcc, 0x2e,
  /* 11 bits */ 0x07, 0xcd, 0x08,
  /* 11 bits */ 0x07, 0xce, 0x83,
  /* 11 bits */ 0x07, 0xcf, 0x52,
  /* 11 bits */ 0x07, 0xd0, 0x23,
  /* 11 bits */ 0x07, 0xd1, 0x46,
  /* 11 bits */ 0x07, 0xd2, 0x68,
  /* 11 bits */ 0x07, 0xd3, 0x5b,
  /* 11 bits */ 0x07, 0xd4, 0x5e,
  /* 11 bits */ 0x07, 0xd5, 0x84,
  /* 11 bits */ 0x07, 0xd6, 0x78,
  /* 11 bits */ 0x07, 0xd7, 0x6c,
  /* 11 bits */ 0x07, 0xd8, 0x17,
  /* 11 bits */ 0x07, 0xd9, 0x5f,
  /* 11 bits */ 0x07, 0xda, 0x53,
  /* 11 bits */ 0x07, 0xdb, 0x47,
  /* 11 bits */ 0x07, 0xdc, 0x3c,
  /* 11 bits */ 0x07, 0xdd, 0x3b,
  /* 11 bits */ 0x07, 0xde, 0x30,
  /* 11 bits */ 0x07, 0xdf, 0x90,
  /* 11 bits */ 0x07, 0xe0, 0x49,
  /* 11 bits */ 0x07, 0xe1, 0x75,
  /* 11 bits */ 0x07, 0xe2, 0x6d,
  /* 12 bits */ 0x0f, 0xc6, 0x85,
  /* 12 bits */ 0x0f, 0xc7, 0x24,
  /* 12 bits */ 0x0f, 0xc8, 0x09,
  /* 12 bits */ 0x0f, 0xc9, 0x91,
  /* 12 bits */ 0x0f, 0xca, 0x79,
  /* 12 bits */ 0x0f, 0xcb, 0x54,
  /* 12 bits */ 0x0f, 0xcc, 0x9d,
  /* 12 bits */ 0x0f, 0xcd, 0x3d,
  /* 12 bits */ 0x0f, 0xce, 0x6e,
  /* 12 bits */ 0x0f, 0xcf, 0x18,
  /* 12 bits */ 0x0f, 0xd0, 0x7a,
  /* 12 bits */ 0x0f, 0xd1, 0x86,
  /* 12 bits */ 0x0f, 0xd2, 0x48,
  /* 12 bits */ 0x0f, 0xd3, 0x60,
  /* 12 bits */ 0x0f, 0xd4, 0x25,
  /* 12 bits */ 0x0f, 0xd5, 0x19,
  /* 12 bits */ 0x0f, 0xd6, 0x9e,
  /* 12 bits */ 0x0f, 0xd7, 0x92,
  /* 12 bits */ 0x0f, 0xd8, 0x31,
  /* 12 bits */ 0x0f, 0xd9, 0x4a,
  /* 12 bits */ 0x0f, 0xda, 0x55,
  /* 12 bits */ 0x0f, 0xdb, 0x6f,
  /* 12 bits */ 0x0f, 0xdc, 0x93,
  /* 12 bits */ 0x0f, 0xdd, 0x0a,
  /* 12 bits */ 0x0f, 0xde, 0x61,
  /* 12 bits */ 0x0f, 0xdf, 0x9f,
  /* 12 bits */ 0x0f, 0xe0, 0x82,
  /* 12 bits */ 0x0f, 0xe1, 0x87,
  /* 12 bits */ 0x0f, 0xe2, 0x3e,
  /* 12 bits */ 0x0f, 0xe3, 0x56,
  /* 12 bits */ 0x0f, 0xe4, 0x26,
  /* 12 bits */ 0x0f, 0xe5, 0x7b,
  /* 12 bits */ 0x0f, 0xe6, 0x7c,
  /* 12 bits */ 0x0f, 0xe7, 0x3f,
  /* 12 bits */ 0x0f, 0xe8, 0x8f,
  /* 12 bits */ 0x0f, 0xe9, 0x57,
  /* 12 bits */ 0x0f, 0xea, 0x32,
  /* 12 bits */ 0x0f, 0xeb, 0x4b,
  /* 13 bits */ 0x1f, 0xd8, 0x70,
  /* 13 bits */ 0x1f, 0xd9, 0x63,
  /* 13 bits */ 0x1f, 0xda, 0xa1,
  /* 13 bits */ 0x1f, 0xdb, 0x33,
  /* 13 bits */ 0x1f, 0xdc, 0x94,
  /* 13 bits */ 0x1f, 0xdd, 0x62,
  /* 13 bits */ 0x1f, 0xde, 0xa0,
  /* 13 bits */ 0x1f, 0xdf, 0x95,
  /* 13 bits */ 0x1f, 0xe0, 0x88,
  /* 13 bits */ 0x1f, 0xe1, 0x40,
  /* 13 bits */ 0x1f, 0xe2, 0x64,
  /* 13 bits */ 0x1f, 0xe3, 0x4c,
  /* 13 bits */ 0x1f, 0xe4, 0x0b,
  /* 13 bits */ 0x1f, 0xe5, 0xa2,
  /* 13 bits */ 0x1f, 0xe6, 0x58,
  /* 13 bits */ 0x1f, 0xe7, 0x9c,
  /* 13 bits */ 0x1f, 0xe8, 0x89,
  /* 13 bits */ 0x1f, 0xe9, 0x4d,
  /* 13 bits */ 0x1f, 0xea, 0x65,
  /* 13 bits */ 0x1f, 0xeb, 0x7d,
  /* 13 bits */ 0x1f, 0xec, 0x0c,
  /* 13 bits */ 0x1f, 0xed, 0x96,
  /* 13 bits */ 0x1f, 0xee, 0x71,
  /* 13 bits */ 0x1f, 0xef, 0x7e,
  /* 13 bits */ 0x1f, 0xf0, 0x8a,
  /* 13 bits */ 0x1f, 0xf1, 0x66,
  /* 13 bits */ 0x1f, 0xf2, 0xa3,
  /* 13 bits */ 0x1f, 0xf3, 0x59,
  /* 13 bits */ 0x1f, 0xf4, 0x73,
  /* 13 bits */ 0x1f, 0xf5, 0x97,
  /* 13 bits */ 0x1f, 0xf6, 0x67,
  /* 13 bits */ 0x1f, 0xf7, 0x5a,
  /* 14 bits */ 0x3f, 0xf0, 0x72,
  /* 14 bits */ 0x3f, 0xf1, 0x8b,
  /* 14 bits */ 0x3f, 0xf2, 0x74,
  /* 14 bits */ 0x3f, 0xf3, 0x7f,
  /* 14 bits */ 0x3f, 0xf4, 0x80,
  /* 14 bits */ 0x3f, 0xf5, 0x81,
  /* 14 bits */ 0x3f, 0xf6, 0x8d,
  /* 14 bits */ 0x3f, 0xf7, 0xa5,
  /* 14 bits */ 0x3f, 0xf8, 0x8c,
  /* 14 bits */ 0x3f, 0xf9, 0x98,
  /* 14 bits */ 0x3f, 0xfa, 0xa4,
  /* 14 bits */ 0x3f, 0xfb, 0x99,
  /* 14 bits */ 0x3f, 0xfc, 0xa6,
  /* 14 bits */ 0x3f, 0xfd, 0xa7,
  /* 15 bits */ 0x7f, 0xfc, 0x8e,
  /* 15 bits */ 0x7f, 0xfd, 0x9a,
  /* 15 bits */ 0x7f, 0xfe, 0x9b,
  /* 15 bits */ 0x7f, 0xff, 0xa8,
  /* codebook 10 -- 2-byte codeword, 1-byte index */
  /*  4 bits */ 0x00, 0x00, 0x0e,
  /*  4 bits */ 0x00, 0x01, 0x0f,
  /*  4 bits */ 0x00, 0x02, 0x1b,
  /*  5 bits */ 0x00, 0x06, 0x1c,
  /*  5 bits */ 0x00, 0x07, 0x0d,
  /*  5 bits */ 0x00, 0x08, 0x01,
  /*  5 bits */ 0x00, 0x09, 0x10,
  /*  5 bits */ 0x00, 0x0a, 0x29,
  /*  5 bits */ 0x00, 0x0b, 0x28,
  /*  5 bits */ 0x00, 0x0c, 0x1d,
  /*  5 bits */ 0x00, 0x0d, 0x2a,
  /*  6 bits */ 0x00, 0x1c, 0x1a,
  /*  6 bits */ 0x00, 0x1d, 0x02,
  /*  6 bits */ 0x00, 0x1e, 0x1e,
  /*  6 bits */ 0x00, 0x1f, 0x36,
  /*  6 bits */ 0x00, 0x20, 0x11,
  /*  6 bits */ 0x00, 0x21, 0x35,
  /*  6 bits */ 0x00, 0x22, 0x00,
  /*  6 bits */ 0x00, 0x23, 0x37,
  /*  6 bits */ 0x00, 0x24, 0x2b,
  /*  6 bits */ 0x00, 0x25, 0x27,
  /*  6 bits */ 0x00, 0x26, 0x03,
  /*  6 bits */ 0x00, 0x27, 0x38,
  /*  6 bits */ 0x00, 0x28, 0x1f,
  /*  6 bits */ 0x00, 0x29, 0x43,
  /*  7 bits */ 0x00, 0x54, 0x12,
  /*  7 bits */ 0x00, 0x55, 0x42,
  /*  7 bits */ 0x00, 0x56, 0x44,
  /*  7 bits */ 0x00, 0x57, 0x2c,
  /*  7 bits */ 0x00, 0x58, 0x45,
  /*  7 bits */ 0x00, 0x59, 0x39,
  /*  7 bits */ 0x00, 0x5a, 0x50,
  /*  7 bits */ 0x00, 0x5b, 0x20,
  /*  7 bits */ 0x00, 0x5c, 0x51,
  /*  7 bits */ 0x00, 0x5d, 0x34,
  /*  7 bits */ 0x00, 0x5e, 0x4f,
  /*  7 bits */ 0x00, 0x5f, 0x04,
  /*  7 bits */ 0x00, 0x60, 0x13,
  /*  7 bits */ 0x00, 0x61, 0x2d,
  /*  7 bits */ 0x00, 0x62, 0x46,
  /*  7 bits */ 0x00, 0x63, 0x52,
  /*  7 bits */ 0x00, 0x64, 0x3a,
  /*  8 bits */ 0x00, 0xca, 0x53,
  /*  8 bits */ 0x00, 0xcb, 0x5d,
  /*  8 bits */ 0x00, 0xcc, 0x2e,
  /*  8 bits */ 0x00, 0xcd, 0x21,
  /*  8 bits */ 0x00, 0xce, 0x47,
  /*  8 bits */ 0x00, 0xcf, 0x6a,
  /*  8 bits */ 0x00, 0xd0, 0x5e,
  /*  8 bits */ 0x00, 0xd1, 0x41,
  /*  8 bits */ 0x00, 0xd2, 0x5c,
  /*  8 bits */ 0x00, 0xd3, 0x05,
  /*  8 bits */ 0x00, 0xd4, 0x69,
  /*  8 bits */ 0x00, 0xd5, 0x14,
  /*  8 bits */ 0x00, 0xd6, 0x6b,
  /*  8 bits */ 0x00, 0xd7, 0x5f,
  /*  8 bits */ 0x00, 0xd8, 0x3b,
  /*  8 bits */ 0x00, 0xd9, 0x22,
  /*  8 bits */ 0x00, 0xda, 0x54,
  /*  8 bits */ 0x00, 0xdb, 0x60,
  /*  8 bits */ 0x00, 0xdc, 0x15,
  /*  8 bits */ 0x00, 0xdd, 0x2f,
  /*  8 bits */ 0x00, 0xde, 0x6c,
  /*  8 bits */ 0x00, 0xdf, 0x3c,
  /*  8 bits */ 0x00, 0xe0, 0x48,
  /*  8 bits */ 0x00, 0xe1, 0x6d,
  /*  8 bits */ 0x00, 0xe2, 0x49,
  /*  9 bits */ 0x01, 0xc6, 0x61,
  /*  9 bits */ 0x01, 0xc7, 0x55,
  /*  9 bits */ 0x01, 0xc8, 0x77,
  /*  9 bits */ 0x01, 0xc9, 0x4e,
  /*  9 bits */ 0x01, 0xca, 0x56,
  /*  9 bits */ 0x01, 0xcb, 0x78,
  /*  9 bits */ 0x01, 0xcc, 0x30,
  /*  9 bits */ 0x01, 0xcd, 0x76,
  /*  9 bits */ 0x01, 0xce, 0x23,
  /*  9 bits */ 0x01, 0xcf, 0x06,
  /*  9 bits */ 0x01, 0xd0, 0x6e,
  /*  9 bits */ 0x01, 0xd1, 0x79,
  /*  9 bits */ 0x01, 0xd2, 0x3d,
  /*  9 bits */ 0x01, 0xd3, 0x84,
  /*  9 bits */ 0x01, 0xd4, 0x16,
  /*  9 bits */ 0x01, 0xd5, 0x62,
  /*  9 bits */ 0x01, 0xd6, 0x6f,
  /*  9 bits */ 0x01, 0xd7, 0x7a,
  /*  9 bits */ 0x01, 0xd8, 0x63,
  /*  9 bits */ 0x01, 0xd9, 0x85,
  /*  9 bits */ 0x01, 0xda, 0x4a,
  /*  9 bits */ 0x01, 0xdb, 0x86,
  /*  9 bits */ 0x01, 0xdc, 0x24,
  /*  9 bits */ 0x01, 0xdd, 0x83,
  /*  9 bits */ 0x01, 0xde, 0x31,
  /*  9 bits */ 0x01, 0xdf, 0x7b,
  /*  9 bits */ 0x01, 0xe0, 0x57,
  /*  9 bits */ 0x01, 0xe1, 0x68,
  /*  9 bits */ 0x01, 0xe2, 0x3e,
  /*  9 bits */ 0x01, 0xe3, 0x5b,
  /*  9 bits */ 0x01, 0xe4, 0x91,
  /* 10 bits */ 0x03, 0xca, 0x64,
  /* 10 bits */ 0x03, 0xcb, 0x92,
  /* 10 bits */ 0x03, 0xcc, 0x88,
  /* 10 bits */ 0x03, 0xcd, 0x17,
  /* 10 bits */ 0x03, 0xce, 0x90,
  /* 10 bits */ 0x03, 0xcf, 0x7c,
  /* 10 bits */ 0x03, 0xd0, 0x07,
  /* 10 bits */ 0x03, 0xd1, 0x70,
  /* 10 bits */ 0x03, 0xd2, 0x87,
  /* 10 bits */ 0x03, 0xd3, 0x32,
  /* 10 bits */ 0x03, 0xd4, 0x4b,
  /* 10 bits */ 0x03, 0xd5, 0x71,
  /* 10 bits */ 0x03, 0xd6, 0x94,
  /* 10 bits */ 0x03, 0xd7, 0x08,
  /* 10 bits */ 0x03, 0xd8, 0x93,
  /* 10 bits */ 0x03, 0xd9, 0x25,
  /* 10 bits */ 0x03, 0xda, 0x65,
  /* 10 bits */ 0x03, 0xdb, 0x58,
  /* 10 bits */ 0x03, 0xdc, 0x89,
  /* 10 bits */ 0x03, 0xdd, 0x3f,
  /* 10 bits */ 0x03, 0xde, 0x18,
  /* 10 bits */ 0x03, 0xdf, 0x9e,
  /* 10 bits */ 0x03, 0xe0, 0x7d,
  /* 10 bits */ 0x03, 0xe1, 0x9f,
  /* 10 bits */ 0x03, 0xe2, 0x95,
  /* 10 bits */ 0x03, 0xe3, 0x4c,
  /* 10 bits */ 0x03, 0xe4, 0xa0,
  /* 10 bits */ 0x03, 0xe5, 0x96,
  /* 10 bits */ 0x03, 0xe6, 0xa1,
  /* 10 bits */ 0x03, 0xe7, 0x33,
  /* 10 bits */ 0x03, 0xe8, 0x59,
  /* 10 bits */ 0x03, 0xe9, 0x75,
  /* 10 bits */ 0x03, 0xea, 0x8a,
  /* 10 bits */ 0x03, 0xeb, 0x82,
  /* 10 bits */ 0x03, 0xec, 0x9d,
  /* 10 bits */ 0x03, 0xed, 0x09,
  /* 10 bits */ 0x03, 0xee, 0x40,
  /* 10 bits */ 0x03, 0xef, 0x7e,
  /* 10 bits */ 0x03, 0xf0, 0xa2,
  /* 10 bits */ 0x03, 0xf1, 0x26,
  /* 10 bits */ 0x03, 0xf2, 0x72,
  /* 11 bits */ 0x07, 0xe6, 0x7f,
  /* 11 bits */ 0x07, 0xe7, 0x19,
  /* 11 bits */ 0x07, 0xe8, 0x97,
  /* 11 bits */ 0x07, 0xe9, 0xa3,
  /* 11 bits */ 0x07, 0xea, 0x66,
  /* 11 bits */ 0x07, 0xeb, 0x4d,
  /* 11 bits */ 0x07, 0xec, 0x5a,
  /* 11 bits */ 0x07, 0xed, 0x8b,
  /* 11 bits */ 0x07, 0xee, 0x73,
  /* 11 bits */ 0x07, 0xef, 0xa4,
  /* 11 bits */ 0x07, 0xf0, 0x0a,
  /* 11 bits */ 0x07, 0xf1, 0x67,
  /* 11 bits */ 0x07, 0xf2, 0x8f,
  /* 11 bits */ 0x07, 0xf3, 0x8c,
  /* 11 bits */ 0x07, 0xf4, 0x98,
  /* 11 bits */ 0x07, 0xf5, 0x99,
  /* 11 bits */ 0x07, 0xf6, 0x0b,
  /* 11 bits */ 0x07, 0xf7, 0x9a,
  /* 11 bits */ 0x07, 0xf8, 0x80,
  /* 11 bits */ 0x07, 0xf9, 0x8d,
  /* 11 bits */ 0x07, 0xfa, 0x9c,
  /* 11 bits */ 0x07, 0xfb, 0x74,
  /* 12 bits */ 0x0f, 0xf8, 0xa5,
  /* 12 bits */ 0x0f, 0xf9, 0x8e,
  /* 12 bits */ 0x0f, 0xfa, 0x81,
  /* 12 bits */ 0x0f, 0xfb, 0x9b,
  /* 12 bits */ 0x0f, 0xfc, 0xa7,
  /* 12 bits */ 0x0f, 0xfd, 0x0c,
  /* 12 bits */ 0x0f, 0xfe, 0xa6,
  /* 12 bits */ 0x0f, 0xff, 0xa8,
  /* codebook 11 -- 2-byte codeword, 2-byte index */
  /*  4 bits */ 0x00, 0x00, 0x00, 0x00,
  /*  4 bits */ 0x00, 0x01, 0x00, 0x12,
  /*  5 bits */ 0x00, 0x04, 0x01, 0x20,
  /*  5 bits */ 0x00, 0x05, 0x00, 0x11,
  /*  5 bits */ 0x00, 0x06, 0x00, 0x01,
  /*  5 bits */ 0x00, 0x07, 0x00, 0x23,
  /*  5 bits */ 0x00, 0x08, 0x00, 0x13,
  /*  5 bits */ 0x00, 0x09, 0x00, 0x24,
  /*  6 bits */ 0x00, 0x14, 0x00, 0x14,
  /*  6 bits */ 0x00, 0x15, 0x00, 0x34,
  /*  6 bits */ 0x00, 0x16, 0x00, 0x35,
  /*  6 bits */ 0x00, 0x17, 0x00, 0x22,
  /*  6 bits */ 0x00, 0x18, 0x00, 0x25,
  /*  6 bits */ 0x00, 0x19, 0x00, 0x02,
  /*  6 bits */ 0x00, 0x1a, 0x00, 0x36,
  /*  7 bits */ 0x00, 0x36, 0x00, 0x45,
  /*  7 bits */ 0x00, 0x37, 0x00, 0x15,
  /*  7 bits */ 0x00, 0x38, 0x00, 0x46,
  /*  7 bits */ 0x00, 0x39, 0x00, 0x26,
  /*  7 bits */ 0x00, 0x3a, 0x00, 0x47,
  /*  7 bits */ 0x00, 0x3b, 0x00, 0x37,
  /*  7 bits */ 0x00, 0x3c, 0x00, 0x33,
  /*  7 bits */ 0x00, 0x3d, 0x00, 0x03,
  /*  7 bits */ 0x00, 0x3e, 0x00, 0x56,
  /*  7 bits */ 0x00, 0x3f, 0x00, 0x57,
  /*  7 bits */ 0x00, 0x40, 0x00, 0x27,
  /*  7 bits */ 0x00, 0x41, 0x00, 0x48,
  /*  7 bits */ 0x00, 0x42, 0x00, 0x16,
  /*  7 bits */ 0x00, 0x43, 0x00, 0x58,
  /*  7 bits */ 0x00, 0x44, 0x00, 0x38,
  /*  7 bits */ 0x00, 0x45, 0x00, 0x59,
  /*  8 bits */ 0x00, 0x8c, 0x00, 0x49,
  /*  8 bits */ 0x00, 0x8d, 0x00, 0x68,
  /*  8 bits */ 0x00, 0x8e, 0x00, 0x28,
  /*  8 bits */ 0x00, 0x8f, 0x00, 0x67,
  /*  8 bits */ 0x00, 0x90, 0x00, 0x69,
  /*  8 bits */ 0x00, 0x91, 0x00, 0x39,
  /*  8 bits */ 0x00, 0x92, 0x00, 0x17,
  /*  8 bits */ 0x00, 0x93, 0x00, 0x54,
  /*  8 bits */ 0x00, 0x94, 0x00, 0x43,
  /*  8 bits */ 0x00, 0x95, 0x01, 0x15,
  /*  8 bits */ 0x00, 0x96, 0x01, 0x13,
  /*  8 bits */ 0x00, 0x97, 0x01, 0x14,
  /*  8 bits */ 0x00, 0x98, 0x00, 0x6a,
  /*  8 bits */ 0x00, 0x99, 0x01, 0x16,
  /*  8 bits */ 0x00, 0x9a, 0x00, 0x44,
  /*  8 bits */ 0x00, 0x9b, 0x00, 0x4a,
  /*  8 bits */ 0x00, 0x9c, 0x00, 0x04,
  /*  8 bits */ 0x00, 0x9d, 0x00, 0x32,
  /*  8 bits */ 0x00, 0x9e, 0x00, 0x5a,
  /*  8 bits */ 0x00, 0x9f, 0x00, 0x65,
  /*  8 bits */ 0x00, 0xa0, 0x01, 0x17,
  /*  8 bits */ 0x00, 0xa1, 0x01, 0x12,
  /*  8 bits */ 0x00, 0xa2, 0x01, 0x18,
  /*  8 bits */ 0x00, 0xa3, 0x00, 0x29,
  /*  8 bits */ 0x00, 0xa4, 0x00, 0x79,
  /*  8 bits */ 0x00, 0xa5, 0x00, 0x3a,
  /*  8 bits */ 0x00, 0xa6, 0x00, 0x6b,
  /*  8 bits */ 0x00, 0xa7, 0x00, 0x5b,
  /*  8 bits */ 0x00, 0xa8, 0x00, 0x76,
  /*  8 bits */ 0x00, 0xa9, 0x01, 0x1a,
  /*  8 bits */ 0x00, 0xaa, 0x00, 0x7a,
  /*  8 bits */ 0x00, 0xab, 0x00, 0x78,
  /*  8 bits */ 0x00, 0xac, 0x01, 0x19,
  /*  8 bits */ 0x00, 0xad, 0x00, 0x87,
  /*  8 bits */ 0x00, 0xae, 0x00, 0x21,
  /*  8 bits */ 0x00, 0xaf, 0x00, 0x18,
  /*  8 bits */ 0x00, 0xb0, 0x00, 0x4b,
  /*  8 bits */ 0x00, 0xb1, 0x01, 0x1b,
  /*  8 bits */ 0x00, 0xb2, 0x00, 0x7b,
  /*  8 bits */ 0x00, 0xb3, 0x01, 0x1c,
  /*  8 bits */ 0x00, 0xb4, 0x00, 0x98,
  /*  8 bits */ 0x00, 0xb5, 0x01, 0x11,
  /*  8 bits */ 0x00, 0xb6, 0x00, 0x6c,
  /*  8 bits */ 0x00, 0xb7, 0x00, 0xa9,
  /*  8 bits */ 0x00, 0xb8, 0x00, 0x2a,
  /*  8 bits */ 0x00, 0xb9, 0x00, 0x5c,
  /*  8 bits */ 0x00, 0xba, 0x00, 0xba,
  /*  8 bits */ 0x00, 0xbb, 0x01, 0x1d,
  /*  8 bits */ 0x00, 0xbc, 0x00, 0x8b,
  /*  8 bits */ 0x00, 0xbd, 0x00, 0x8a,
  /*  8 bits */ 0x00, 0xbe, 0x00, 0x3b,
  /*  8 bits */ 0x00, 0xbf, 0x00, 0x55,
  /*  8 bits */ 0x00, 0xc0, 0x01, 0x1e,
  /*  8 bits */ 0x00, 0xc1, 0x00, 0xcb,
  /*  8 bits */ 0x00, 0xc2, 0x00, 0x7c,
  /*  8 bits */ 0x00, 0xc3, 0x00, 0x4c,
  /*  8 bits */ 0x00, 0xc4, 0x00, 0x6d,
  /*  8 bits */ 0x00, 0xc5, 0x00, 0x7d,
  /*  8 bits */ 0x00, 0xc6, 0x00, 0x05,
  /*  9 bits */ 0x01, 0x8e, 0x00, 0x8c,
  /*  9 bits */ 0x01, 0x8f, 0x01, 0x1f,
  /*  9 bits */ 0x01, 0x90, 0x00, 0xdc,
  /*  9 bits */ 0x01, 0x91, 0x00, 0x19,
  /*  9 bits */ 0x01, 0x92, 0x00, 0x89,
  /*  9 bits */ 0x01, 0x93, 0x00, 0xfe,
  /*  9 bits */ 0x01, 0x94, 0x00, 0x5d,
  /*  9 bits */ 0x01, 0x95, 0x00, 0xed,
  /*  9 bits */ 0x01, 0x96, 0x00, 0x3c,
  /*  9 bits */ 0x01, 0x97, 0x00, 0x8d,
  /*  9 bits */ 0x01, 0x98, 0x00, 0x7e,
  /*  9 bits */ 0x01, 0x99, 0x00, 0x2b,
  /*  9 bits */ 0x01, 0x9a, 0x00, 0x8e,
  /*  9 bits */ 0x01, 0x9b, 0x00, 0x9b,
  /*  9 bits */ 0x01, 0x9c, 0x00, 0x9c,
  /*  9 bits */ 0x01, 0x9d, 0x01, 0x0f,
  /*  9 bits */ 0x01, 0x9e, 0x00, 0x4d,
  /*  9 bits */ 0x01, 0x9f, 0x00, 0x6e,
  /*  9 bits */ 0x01, 0xa0, 0x00, 0x66,
  /*  9 bits */ 0x01, 0xa1, 0x00, 0x9d,
  /*  9 bits */ 0x01, 0xa2, 0x00, 0x5e,
  /*  9 bits */ 0x01, 0xa3, 0x00, 0x8f,
  /*  9 bits */ 0x01, 0xa4, 0x00, 0x7f,
  /*  9 bits */ 0x01, 0xa5, 0x00, 0x1a,
  /*  9 bits */ 0x01, 0xa6, 0x00, 0xad,
  /*  9 bits */ 0x01, 0xa7, 0x00, 0x06,
  /*  9 bits */ 0x01, 0xa8, 0x00, 0xac,
  /*  9 bits */ 0x01, 0xa9, 0x00, 0x9a,
  /*  9 bits */ 0x01, 0xaa, 0x00, 0x9e,
  /*  9 bits */ 0x01, 0xab, 0x00, 0x4e,
  /*  9 bits */ 0x01, 0xac, 0x00, 0x2c,
  /*  9 bits */ 0x01, 0xad, 0x00, 0x9f,
  /*  9 bits */ 0x01, 0xae, 0x00, 0x3d,
  /*  9 bits */ 0x01, 0xaf, 0x00, 0x6f,
  /*  9 bits */ 0x01, 0xb0, 0x00, 0xae,
  /*  9 bits */ 0x01, 0xb1, 0x00, 0x90,
  /*  9 bits */ 0x01, 0xb2, 0x00, 0xaf,
  /*  9 bits */ 0x01, 0xb3, 0x00, 0xa0,
  /*  9 bits */ 0x01, 0xb4, 0x00, 0xbe,
  /*  9 bits */ 0x01, 0xb5, 0x00, 0x1b,
  /*  9 bits */ 0x01, 0xb6, 0x00, 0x77,
  /*  9 bits */ 0x01, 0xb7, 0x00, 0xb0,
  /*  9 bits */ 0x01, 0xb8, 0x00, 0x80,
  /*  9 bits */ 0x01, 0xb9, 0x00, 0x3e,
  /*  9 bits */ 0x01, 0xba, 0x00, 0x5f,
  /*  9 bits */ 0x01, 0xbb, 0x00, 0xab,
  /*  9 bits */ 0x01, 0xbc, 0x00, 0x4f,
  /*  9 bits */ 0x01, 0xbd, 0x00, 0xbd,
  /*  9 bits */ 0x01, 0xbe, 0x00, 0xdf,
  /*  9 bits */ 0x01, 0xbf, 0x00, 0x70,
  /*  9 bits */ 0x01, 0xc0, 0x00, 0xe0,
  /*  9 bits */ 0x01, 0xc1, 0x00, 0x2d,
  /*  9 bits */ 0x01, 0xc2, 0x01, 0x10,
  /*  9 bits */ 0x01, 0xc3, 0x00, 0x60,
  /*  9 bits */ 0x01, 0xc4, 0x00, 0xc0,
  /* 10 bits */ 0x03, 0x8a, 0x00, 0xbf,
  /* 10 bits */ 0x03, 0x8b, 0x00, 0xa1,
  /* 10 bits */ 0x03, 0x8c, 0x00, 0x81,
  /* 10 bits */ 0x03, 0x8d, 0x00, 0x91,
  /* 10 bits */ 0x03, 0x8e, 0x00, 0x10,
  /* 10 bits */ 0x03, 0x8f, 0x00, 0x51,
  /* 10 bits */ 0x03, 0x90, 0x00, 0x07,
  /* 10 bits */ 0x03, 0x91, 0x00, 0x40,
  /* 10 bits */ 0x03, 0x92, 0x00, 0xc1,
  /* 10 bits */ 0x03, 0x93, 0x00, 0xde,
  /* 10 bits */ 0x03, 0x94, 0x00, 0xe1,
  /* 10 bits */ 0x03, 0x95, 0x00, 0xcf,
  /* 10 bits */ 0x03, 0x96, 0x00, 0x2f,
  /* 10 bits */ 0x03, 0x97, 0x00, 0xe2,
  /* 10 bits */ 0x03, 0x98, 0x00, 0x92,
  /* 10 bits */ 0x03, 0x99, 0x00, 0x71,
  /* 10 bits */ 0x03, 0x9a, 0x00, 0xb2,
  /* 10 bits */ 0x03, 0x9b, 0x00, 0xb1,
  /* 10 bits */ 0x03, 0x9c, 0x00, 0xf0,
  /* 10 bits */ 0x03, 0x9d, 0x00, 0xd0,
  /* 10 bits */ 0x03, 0x9e, 0x00, 0x1c,
  /* 10 bits */ 0x03, 0x9f, 0x00, 0x50,
  /* 10 bits */ 0x03, 0xa0, 0x00, 0xbc,
  /* 10 bits */ 0x03, 0xa1, 0x00, 0x3f,
  /* 10 bits */ 0x03, 0xa2, 0x00, 0x1e,
  /* 10 bits */ 0x03, 0xa3, 0x00, 0xce,
  /* 10 bits */ 0x03, 0xa4, 0x00, 0x82,
  /* 10 bits */ 0x03, 0xa5, 0x00, 0x41,
  /* 10 bits */ 0x03, 0xa6, 0x00, 0x61,
  /* 10 bits */ 0x03, 0xa7, 0x00, 0x62,
  /* 10 bits */ 0x03, 0xa8, 0x00, 0xf2,
  /* 10 bits */ 0x03, 0xa9, 0x00, 0x52,
  /* 10 bits */ 0x03, 0xaa, 0x00, 0xc2,
  /* 10 bits */ 0x03, 0xab, 0x00, 0xf1,
  /* 10 bits */ 0x03, 0xac, 0x00, 0xd1,
  /* 10 bits */ 0x03, 0xad, 0x00, 0xe3,
  /* 10 bits */ 0x03, 0xae, 0x00, 0xd2,
  /* 10 bits */ 0x03, 0xaf, 0x00, 0x88,
  /* 10 bits */ 0x03, 0xb0, 0x00, 0xc3,
  /* 10 bits */ 0x03, 0xb1, 0x00, 0x2e,
  /* 10 bits */ 0x03, 0xb2, 0x00, 0xa2,
  /* 10 bits */ 0x03, 0xb3, 0x00, 0xf3,
  /* 10 bits */ 0x03, 0xb4, 0x00, 0x73,
  /* 10 bits */ 0x03, 0xb5, 0x00, 0xb4,
  /* 10 bits */ 0x03, 0xb6, 0x01, 0x01,
  /* 10 bits */ 0x03, 0xb7, 0x00, 0x93,
  /* 10 bits */ 0x03, 0xb8, 0x00, 0xa3,
  /* 10 bits */ 0x03, 0xb9, 0x00, 0xf4,
  /* 10 bits */ 0x03, 0xba, 0x00, 0xb3,
  /* 10 bits */ 0x03, 0xbb, 0x00, 0x63,
  /* 10 bits */ 0x03, 0xbc, 0x00, 0xc4,
  /* 10 bits */ 0x03, 0xbd, 0x00, 0xef,
  /* 10 bits */ 0x03, 0xbe, 0x00, 0x30,
  /* 10 bits */ 0x03, 0xbf, 0x00, 0x72,
  /* 10 bits */ 0x03, 0xc0, 0x00, 0x1d,
  /* 10 bits */ 0x03, 0xc1, 0x00, 0xe5,
  /* 10 bits */ 0x03, 0xc2, 0x00, 0x08,
  /* 10 bits */ 0x03, 0xc3, 0x00, 0xe4,
  /* 10 bits */ 0x03, 0xc4, 0x00, 0x83,
  /* 10 bits */ 0x03, 0xc5, 0x00, 0xd3,
  /* 10 bits */ 0x03, 0xc6, 0x00, 0x84,
  /* 10 bits */ 0x03, 0xc7, 0x01, 0x02,
  /* 10 bits */ 0x03, 0xc8, 0x00, 0xcd,
  /* 10 bits */ 0x03, 0xc9, 0x00, 0x74,
  /* 10 bits */ 0x03, 0xca, 0x00, 0x31,
  /* 10 bits */ 0x03, 0xcb, 0x01, 0x04,
  /* 10 bits */ 0x03, 0xcc, 0x01, 0x03,
  /* 10 bits */ 0x03, 0xcd, 0x00, 0x1f,
  /* 10 bits */ 0x03, 0xce, 0x00, 0xa4,
  /* 10 bits */ 0x03, 0xcf, 0x00, 0x53,
  /* 10 bits */ 0x03, 0xd0, 0x00, 0xf5,
  /* 10 bits */ 0x03, 0xd1, 0x00, 0x95,
  /* 10 bits */ 0x03, 0xd2, 0x00, 0xe6,
  /* 10 bits */ 0x03, 0xd3, 0x00, 0x94,
  /* 10 bits */ 0x03, 0xd4, 0x00, 0x64,
  /* 10 bits */ 0x03, 0xd5, 0x00, 0x42,
  /* 10 bits */ 0x03, 0xd6, 0x00, 0xb5,
  /* 10 bits */ 0x03, 0xd7, 0x00, 0xc5,
  /* 10 bits */ 0x03, 0xd8, 0x00, 0xd4,
  /* 10 bits */ 0x03, 0xd9, 0x01, 0x05,
  /* 10 bits */ 0x03, 0xda, 0x01, 0x06,
  /* 10 bits */ 0x03, 0xdb, 0x00, 0x96,
  /* 10 bits */ 0x03, 0xdc, 0x01, 0x00,
  /* 10 bits */ 0x03, 0xdd, 0x00, 0x85,
  /* 10 bits */ 0x03, 0xde, 0x00, 0x99,
  /* 10 bits */ 0x03, 0xdf, 0x00, 0x09,
  /* 10 bits */ 0x03, 0xe0, 0x00, 0xa6,
  /* 10 bits */ 0x03, 0xe1, 0x00, 0xa5,
  /* 10 bits */ 0x03, 0xe2, 0x00, 0xd5,
  /* 10 bits */ 0x03, 0xe3, 0x00, 0xf6,
  /* 10 bits */ 0x03, 0xe4, 0x00, 0xb7,
  /* 10 bits */ 0x03, 0xe5, 0x00, 0xf7,
  /* 10 bits */ 0x03, 0xe6, 0x00, 0xd6,
  /* 10 bits */ 0x03, 0xe7, 0x00, 0x75,
  /* 10 bits */ 0x03, 0xe8, 0x00, 0x86,
  /* 11 bits */ 0x07, 0xd2, 0x00, 0xa7,
  /* 11 bits */ 0x07, 0xd3, 0x01, 0x07,
  /* 11 bits */ 0x07, 0xd4, 0x00, 0xc6,
  /* 11 bits */ 0x07, 0xd5, 0x00, 0xc9,
  /* 11 bits */ 0x07, 0xd6, 0x00, 0x20,
  /* 11 bits */ 0x07, 0xd7, 0x00, 0xb6,
  /* 11 bits */ 0x07, 0xd8, 0x00, 0xb8,
  /* 11 bits */ 0x07, 0xd9, 0x00, 0xe8,
  /* 11 bits */ 0x07, 0xda, 0x00, 0xe7,
  /* 11 bits */ 0x07, 0xdb, 0x00, 0xc8,
  /* 11 bits */ 0x07, 0xdc, 0x00, 0xc7,
  /* 11 bits */ 0x07, 0xdd, 0x00, 0x97,
  /* 11 bits */ 0x07, 0xde, 0x00, 0xf9,
  /* 11 bits */ 0x07, 0xdf, 0x00, 0xe9,
  /* 11 bits */ 0x07, 0xe0, 0x00, 0xd9,
  /* 11 bits */ 0x07, 0xe1, 0x01, 0x08,
  /* 11 bits */ 0x07, 0xe2, 0x00, 0xf8,
  /* 11 bits */ 0x07, 0xe3, 0x00, 0xaa,
  /* 11 bits */ 0x07, 0xe4, 0x00, 0xd7,
  /* 11 bits */ 0x07, 0xe5, 0x00, 0xa8,
  /* 11 bits */ 0x07, 0xe6, 0x00, 0x0a,
  /* 11 bits */ 0x07, 0xe7, 0x00, 0xd8,
  /* 11 bits */ 0x07, 0xe8, 0x00, 0xbb,
  /* 11 bits */ 0x07, 0xe9, 0x00, 0xda,
  /* 11 bits */ 0x07, 0xea, 0x00, 0xb9,
  /* 11 bits */ 0x07, 0xeb, 0x00, 0xea,
  /* 11 bits */ 0x07, 0xec, 0x00, 0x0d,
  /* 11 bits */ 0x07, 0xed, 0x00, 0xfa,
  /* 11 bits */ 0x07, 0xee, 0x01, 0x09,
  /* 11 bits */ 0x07, 0xef, 0x01, 0x0a,
  /* 11 bits */ 0x07, 0xf0, 0x00, 0xca,
  /* 11 bits */ 0x07, 0xf1, 0x00, 0xfb,
  /* 11 bits */ 0x07, 0xf2, 0x00, 0xdd,
  /* 11 bits */ 0x07, 0xf3, 0x00, 0x0b,
  /* 11 bits */ 0x07, 0xf4, 0x00, 0xeb,
  /* 11 bits */ 0x07, 0xf5, 0x01, 0x0b,
  /* 11 bits */ 0x07, 0xf6, 0x01, 0x0c,
  /* 11 bits */ 0x07, 0xf7, 0x00, 0xdb,
  /* 11 bits */ 0x07, 0xf8, 0x00, 0xee,
  /* 11 bits */ 0x07, 0xf9, 0x00, 0xfc,
  /* 11 bits */ 0x07, 0xfa, 0x00, 0xec,
  /* 11 bits */ 0x07, 0xfb, 0x00, 0xcc,
  /* 11 bits */ 0x07, 0xfc, 0x00, 0xfd,
  /* 12 bits */ 0x0f, 0xfa, 0x00, 0x0e,
  /* 12 bits */ 0x0f, 0xfb, 0x00, 0x0c,
  /* 12 bits */ 0x0f, 0xfc, 0x01, 0x0d,
  /* 12 bits */ 0x0f, 0xfd, 0x00, 0xff,
  /* 12 bits */ 0x0f, 0xfe, 0x00, 0x0f,
  /* 12 bits */ 0x0f, 0xff, 0x01, 0x0e
};

/* for scalefactor codebook and codebook 11, each entry is 4 bytes. 3 bytes otherwise */
static const maac_u32 maac_codebook_stride_tbl[12] = {
  4,
  3,
  3,
  3,
  3,
  3,
  3,
  3,
  3,
  3,
  3,
  4
};

#define maac_codebook_stride(x) maac_codebook_stride_tbl[x]

maac_pure static maac_inline maac_u32 maac_unpack_u24be(const maac_u8* x) {
  return (((maac_u32)(x[0])) << 16) |
         (((maac_u32)(x[1])) << 8) |
         (((maac_u32)(x[2])));
}

maac_pure static maac_inline maac_u16 maac_unpack_u16be(const maac_u8* x) {
  return (((maac_u16)(x[0])) << 8) |
         (((maac_u16)(x[1])));
}

maac_pure static maac_inline maac_u32 maac_codebook_codeword(maac_u32 cb, maac_u32 index) {
    const maac_u8* b = &maac_codebook[maac_codebook_indexes[cb].start + (index * maac_codebook_stride(cb))];
    return (maac_u32)(cb == 0 ? maac_unpack_u24be(b) : maac_unpack_u16be(b));
}

maac_pure static maac_inline maac_u32 maac_codebook_index(maac_u32 cb, maac_u32 index) {
    const maac_u8* b = &maac_codebook[maac_codebook_indexes[cb].start + (index * maac_codebook_stride(cb))];
    b += (cb == 0 ? 3 : 2);
    return (maac_u32)(cb == 11 ? maac_unpack_u16be(b) : b[0]);
}

#else

struct maac_codebook_entry {
  maac_u32 code;
  maac_u16 index;
};

typedef struct maac_codebook_entry maac_codebook_entry;

/* indexed by struct index */
static const maac_codebook_index_entry maac_codebook_indexes[12] = {
  { 0, 121 },
  { 121, 202 },
  { 202, 283 },
  { 283, 364 },
  { 364, 445 },
  { 445, 526 },
  { 526, 607 },
  { 607, 671 },
  { 671, 735 },
  { 735, 904 },
  { 904, 1073 },
  { 1073, 1362 }
};

static const maac_codebook_entry maac_codebook[1362] = {
  /* scalefactor codebook -- 3-byte codeword, 1-byte index */
  { /*  1 bits */ 0x00000, 60 },
  { /*  3 bits */ 0x00004, 59 },
  { /*  4 bits */ 0x0000a, 61 },
  { /*  4 bits */ 0x0000b, 58 },
  { /*  4 bits */ 0x0000c, 62 },
  { /*  5 bits */ 0x0001a, 57 },
  { /*  5 bits */ 0x0001b, 63 },
  { /*  6 bits */ 0x00038, 56 },
  { /*  6 bits */ 0x00039, 64 },
  { /*  6 bits */ 0x0003a, 55 },
  { /*  6 bits */ 0x0003b, 65 },
  { /*  7 bits */ 0x00078, 66 },
  { /*  7 bits */ 0x00079, 54 },
  { /*  7 bits */ 0x0007a, 67 },
  { /*  8 bits */ 0x000f6, 53 },
  { /*  8 bits */ 0x000f7, 68 },
  { /*  8 bits */ 0x000f8, 52 },
  { /*  8 bits */ 0x000f9, 69 },
  { /*  8 bits */ 0x000fa, 51 },
  { /*  9 bits */ 0x001f6, 70 },
  { /*  9 bits */ 0x001f7, 50 },
  { /*  9 bits */ 0x001f8, 49 },
  { /*  9 bits */ 0x001f9, 71 },
  { /* 10 bits */ 0x003f4, 72 },
  { /* 10 bits */ 0x003f5, 48 },
  { /* 10 bits */ 0x003f6, 73 },
  { /* 10 bits */ 0x003f7, 47 },
  { /* 10 bits */ 0x003f8, 74 },
  { /* 10 bits */ 0x003f9, 46 },
  { /* 11 bits */ 0x007f4, 76 },
  { /* 11 bits */ 0x007f5, 75 },
  { /* 11 bits */ 0x007f6, 77 },
  { /* 11 bits */ 0x007f7, 78 },
  { /* 11 bits */ 0x007f8, 45 },
  { /* 11 bits */ 0x007f9, 43 },
  { /* 12 bits */ 0x00ff4, 44 },
  { /* 12 bits */ 0x00ff5, 79 },
  { /* 12 bits */ 0x00ff6, 42 },
  { /* 12 bits */ 0x00ff7, 41 },
  { /* 12 bits */ 0x00ff8, 80 },
  { /* 12 bits */ 0x00ff9, 40 },
  { /* 13 bits */ 0x01ff4, 81 },
  { /* 13 bits */ 0x01ff5, 39 },
  { /* 13 bits */ 0x01ff6, 82 },
  { /* 13 bits */ 0x01ff7, 38 },
  { /* 13 bits */ 0x01ff8, 83 },
  { /* 14 bits */ 0x03ff2, 37 },
  { /* 14 bits */ 0x03ff3, 35 },
  { /* 14 bits */ 0x03ff4, 85 },
  { /* 14 bits */ 0x03ff5, 33 },
  { /* 14 bits */ 0x03ff6, 36 },
  { /* 14 bits */ 0x03ff7, 34 },
  { /* 14 bits */ 0x03ff8, 84 },
  { /* 14 bits */ 0x03ff9, 32 },
  { /* 15 bits */ 0x07ff4, 87 },
  { /* 15 bits */ 0x07ff5, 89 },
  { /* 15 bits */ 0x07ff6, 30 },
  { /* 15 bits */ 0x07ff7, 31 },
  { /* 16 bits */ 0x0fff0, 86 },
  { /* 16 bits */ 0x0fff1, 29 },
  { /* 16 bits */ 0x0fff2, 26 },
  { /* 16 bits */ 0x0fff3, 27 },
  { /* 16 bits */ 0x0fff4, 28 },
  { /* 16 bits */ 0x0fff5, 24 },
  { /* 16 bits */ 0x0fff6, 88 },
  { /* 17 bits */ 0x1ffee, 25 },
  { /* 17 bits */ 0x1ffef, 22 },
  { /* 17 bits */ 0x1fff0, 23 },
  { /* 18 bits */ 0x3ffe2, 90 },
  { /* 18 bits */ 0x3ffe3, 21 },
  { /* 18 bits */ 0x3ffe4, 19 },
  { /* 18 bits */ 0x3ffe5, 3 },
  { /* 18 bits */ 0x3ffe6, 1 },
  { /* 18 bits */ 0x3ffe7, 2 },
  { /* 18 bits */ 0x3ffe8, 0 },
  { /* 19 bits */ 0x7ffd2, 98 },
  { /* 19 bits */ 0x7ffd3, 99 },
  { /* 19 bits */ 0x7ffd4, 100 },
  { /* 19 bits */ 0x7ffd5, 101 },
  { /* 19 bits */ 0x7ffd6, 102 },
  { /* 19 bits */ 0x7ffd7, 117 },
  { /* 19 bits */ 0x7ffd8, 97 },
  { /* 19 bits */ 0x7ffd9, 91 },
  { /* 19 bits */ 0x7ffda, 92 },
  { /* 19 bits */ 0x7ffdb, 93 },
  { /* 19 bits */ 0x7ffdc, 94 },
  { /* 19 bits */ 0x7ffdd, 95 },
  { /* 19 bits */ 0x7ffde, 96 },
  { /* 19 bits */ 0x7ffdf, 104 },
  { /* 19 bits */ 0x7ffe0, 111 },
  { /* 19 bits */ 0x7ffe1, 112 },
  { /* 19 bits */ 0x7ffe2, 113 },
  { /* 19 bits */ 0x7ffe3, 114 },
  { /* 19 bits */ 0x7ffe4, 115 },
  { /* 19 bits */ 0x7ffe5, 116 },
  { /* 19 bits */ 0x7ffe6, 110 },
  { /* 19 bits */ 0x7ffe7, 105 },
  { /* 19 bits */ 0x7ffe8, 106 },
  { /* 19 bits */ 0x7ffe9, 107 },
  { /* 19 bits */ 0x7ffea, 108 },
  { /* 19 bits */ 0x7ffeb, 109 },
  { /* 19 bits */ 0x7ffec, 118 },
  { /* 19 bits */ 0x7ffed, 6 },
  { /* 19 bits */ 0x7ffee, 8 },
  { /* 19 bits */ 0x7ffef, 9 },
  { /* 19 bits */ 0x7fff0, 10 },
  { /* 19 bits */ 0x7fff1, 5 },
  { /* 19 bits */ 0x7fff2, 103 },
  { /* 19 bits */ 0x7fff3, 120 },
  { /* 19 bits */ 0x7fff4, 119 },
  { /* 19 bits */ 0x7fff5, 4 },
  { /* 19 bits */ 0x7fff6, 7 },
  { /* 19 bits */ 0x7fff7, 15 },
  { /* 19 bits */ 0x7fff8, 16 },
  { /* 19 bits */ 0x7fff9, 18 },
  { /* 19 bits */ 0x7fffa, 20 },
  { /* 19 bits */ 0x7fffb, 17 },
  { /* 19 bits */ 0x7fffc, 11 },
  { /* 19 bits */ 0x7fffd, 12 },
  { /* 19 bits */ 0x7fffe, 14 },
  { /* 19 bits */ 0x7ffff, 13 },
  /* codebook 1 -- 2-byte codeword, 1-byte index */
  { /*  1 bits */ 0x00000, 40 },
  { /*  5 bits */ 0x00010, 67 },
  { /*  5 bits */ 0x00011, 13 },
  { /*  5 bits */ 0x00012, 39 },
  { /*  5 bits */ 0x00013, 49 },
  { /*  5 bits */ 0x00014, 41 },
  { /*  5 bits */ 0x00015, 37 },
  { /*  5 bits */ 0x00016, 43 },
  { /*  5 bits */ 0x00017, 31 },
  { /*  7 bits */ 0x00060, 58 },
  { /*  7 bits */ 0x00061, 22 },
  { /*  7 bits */ 0x00062, 38 },
  { /*  7 bits */ 0x00063, 46 },
  { /*  7 bits */ 0x00064, 34 },
  { /*  7 bits */ 0x00065, 42 },
  { /*  7 bits */ 0x00066, 76 },
  { /*  7 bits */ 0x00067, 36 },
  { /*  7 bits */ 0x00068, 4 },
  { /*  7 bits */ 0x00069, 28 },
  { /*  7 bits */ 0x0006a, 64 },
  { /*  7 bits */ 0x0006b, 48 },
  { /*  7 bits */ 0x0006c, 16 },
  { /*  7 bits */ 0x0006d, 44 },
  { /*  7 bits */ 0x0006e, 70 },
  { /*  7 bits */ 0x0006f, 32 },
  { /*  7 bits */ 0x00070, 52 },
  { /*  7 bits */ 0x00071, 50 },
  { /*  7 bits */ 0x00072, 10 },
  { /*  7 bits */ 0x00073, 68 },
  { /*  7 bits */ 0x00074, 12 },
  { /*  7 bits */ 0x00075, 66 },
  { /*  7 bits */ 0x00076, 14 },
  { /*  7 bits */ 0x00077, 30 },
  { /*  9 bits */ 0x001e0, 73 },
  { /*  9 bits */ 0x001e1, 19 },
  { /*  9 bits */ 0x001e2, 61 },
  { /*  9 bits */ 0x001e3, 51 },
  { /*  9 bits */ 0x001e4, 47 },
  { /*  9 bits */ 0x001e5, 35 },
  { /*  9 bits */ 0x001e6, 33 },
  { /*  9 bits */ 0x001e7, 55 },
  { /*  9 bits */ 0x001e8, 65 },
  { /*  9 bits */ 0x001e9, 45 },
  { /*  9 bits */ 0x001ea, 25 },
  { /*  9 bits */ 0x001eb, 15 },
  { /*  9 bits */ 0x001ec, 7 },
  { /*  9 bits */ 0x001ed, 29 },
  { /*  9 bits */ 0x001ee, 59 },
  { /*  9 bits */ 0x001ef, 57 },
  { /*  9 bits */ 0x001f0, 21 },
  { /*  9 bits */ 0x001f1, 1 },
  { /*  9 bits */ 0x001f2, 27 },
  { /*  9 bits */ 0x001f3, 53 },
  { /*  9 bits */ 0x001f4, 69 },
  { /*  9 bits */ 0x001f5, 77 },
  { /*  9 bits */ 0x001f6, 23 },
  { /*  9 bits */ 0x001f7, 79 },
  { /* 10 bits */ 0x003f0, 5 },
  { /* 10 bits */ 0x003f1, 9 },
  { /* 10 bits */ 0x003f2, 75 },
  { /* 10 bits */ 0x003f3, 63 },
  { /* 10 bits */ 0x003f4, 11 },
  { /* 10 bits */ 0x003f5, 3 },
  { /* 10 bits */ 0x003f6, 17 },
  { /* 10 bits */ 0x003f7, 71 },
  { /* 11 bits */ 0x007f0, 60 },
  { /* 11 bits */ 0x007f1, 20 },
  { /* 11 bits */ 0x007f2, 24 },
  { /* 11 bits */ 0x007f3, 56 },
  { /* 11 bits */ 0x007f4, 80 },
  { /* 11 bits */ 0x007f5, 8 },
  { /* 11 bits */ 0x007f6, 72 },
  { /* 11 bits */ 0x007f7, 6 },
  { /* 11 bits */ 0x007f8, 0 },
  { /* 11 bits */ 0x007f9, 74 },
  { /* 11 bits */ 0x007fa, 62 },
  { /* 11 bits */ 0x007fb, 26 },
  { /* 11 bits */ 0x007fc, 18 },
  { /* 11 bits */ 0x007fd, 2 },
  { /* 11 bits */ 0x007fe, 54 },
  { /* 11 bits */ 0x007ff, 78 },
  /* codebook 2 -- 2-byte codeword, 1-byte index */
  { /*  3 bits */ 0x00000, 40 },
  { /*  4 bits */ 0x00002, 67 },
  { /*  5 bits */ 0x00006, 13 },
  { /*  5 bits */ 0x00007, 41 },
  { /*  5 bits */ 0x00008, 37 },
  { /*  5 bits */ 0x00009, 39 },
  { /*  5 bits */ 0x0000a, 31 },
  { /*  5 bits */ 0x0000b, 43 },
  { /*  5 bits */ 0x0000c, 49 },
  { /*  6 bits */ 0x0001a, 34 },
  { /*  6 bits */ 0x0001b, 22 },
  { /*  6 bits */ 0x0001c, 46 },
  { /*  6 bits */ 0x0001d, 42 },
  { /*  6 bits */ 0x0001e, 48 },
  { /*  6 bits */ 0x0001f, 38 },
  { /*  6 bits */ 0x00020, 12 },
  { /*  6 bits */ 0x00021, 58 },
  { /*  6 bits */ 0x00022, 64 },
  { /*  6 bits */ 0x00023, 4 },
  { /*  6 bits */ 0x00024, 36 },
  { /*  6 bits */ 0x00025, 70 },
  { /*  6 bits */ 0x00026, 68 },
  { /*  6 bits */ 0x00027, 32 },
  { /*  6 bits */ 0x00028, 16 },
  { /*  6 bits */ 0x00029, 50 },
  { /*  6 bits */ 0x0002a, 28 },
  { /*  6 bits */ 0x0002b, 14 },
  { /*  6 bits */ 0x0002c, 30 },
  { /*  6 bits */ 0x0002d, 10 },
  { /*  6 bits */ 0x0002e, 76 },
  { /*  6 bits */ 0x0002f, 52 },
  { /*  6 bits */ 0x00030, 44 },
  { /*  6 bits */ 0x00031, 66 },
  { /*  7 bits */ 0x00064, 47 },
  { /*  7 bits */ 0x00065, 65 },
  { /*  7 bits */ 0x00066, 19 },
  { /*  7 bits */ 0x00067, 33 },
  { /*  7 bits */ 0x00068, 61 },
  { /*  7 bits */ 0x00069, 75 },
  { /*  7 bits */ 0x0006a, 71 },
  { /*  7 bits */ 0x0006b, 25 },
  { /*  7 bits */ 0x0006c, 29 },
  { /*  7 bits */ 0x0006d, 79 },
  { /*  7 bits */ 0x0006e, 15 },
  { /*  7 bits */ 0x0006f, 1 },
  { /*  7 bits */ 0x00070, 11 },
  { /*  7 bits */ 0x00071, 55 },
  { /*  7 bits */ 0x00072, 73 },
  { /*  8 bits */ 0x000e6, 59 },
  { /*  8 bits */ 0x000e7, 21 },
  { /*  8 bits */ 0x000e8, 7 },
  { /*  8 bits */ 0x000e9, 17 },
  { /*  8 bits */ 0x000ea, 5 },
  { /*  8 bits */ 0x000eb, 3 },
  { /*  8 bits */ 0x000ec, 27 },
  { /*  8 bits */ 0x000ed, 69 },
  { /*  8 bits */ 0x000ee, 63 },
  { /*  8 bits */ 0x000ef, 45 },
  { /*  8 bits */ 0x000f0, 53 },
  { /*  8 bits */ 0x000f1, 23 },
  { /*  8 bits */ 0x000f2, 9 },
  { /*  8 bits */ 0x000f3, 51 },
  { /*  8 bits */ 0x000f4, 57 },
  { /*  8 bits */ 0x000f5, 35 },
  { /*  8 bits */ 0x000f6, 77 },
  { /*  8 bits */ 0x000f7, 60 },
  { /*  8 bits */ 0x000f8, 20 },
  { /*  9 bits */ 0x001f2, 56 },
  { /*  9 bits */ 0x001f3, 0 },
  { /*  9 bits */ 0x001f4, 24 },
  { /*  9 bits */ 0x001f5, 26 },
  { /*  9 bits */ 0x001f6, 80 },
  { /*  9 bits */ 0x001f7, 6 },
  { /*  9 bits */ 0x001f8, 62 },
  { /*  9 bits */ 0x001f9, 18 },
  { /*  9 bits */ 0x001fa, 8 },
  { /*  9 bits */ 0x001fb, 72 },
  { /*  9 bits */ 0x001fc, 54 },
  { /*  9 bits */ 0x001fd, 2 },
  { /*  9 bits */ 0x001fe, 74 },
  { /*  9 bits */ 0x001ff, 78 },
  /* codebook 3 -- 2-byte codeword, 1-byte index */
  { /*  1 bits */ 0x00000, 0 },
  { /*  4 bits */ 0x00008, 27 },
  { /*  4 bits */ 0x00009, 1 },
  { /*  4 bits */ 0x0000a, 9 },
  { /*  4 bits */ 0x0000b, 3 },
  { /*  5 bits */ 0x00018, 36 },
  { /*  5 bits */ 0x00019, 4 },
  { /*  6 bits */ 0x00034, 12 },
  { /*  6 bits */ 0x00035, 10 },
  { /*  6 bits */ 0x00036, 30 },
  { /*  6 bits */ 0x00037, 13 },
  { /*  6 bits */ 0x00038, 28 },
  { /*  6 bits */ 0x00039, 39 },
  { /*  7 bits */ 0x00074, 40 },
  { /*  7 bits */ 0x00075, 31 },
  { /*  7 bits */ 0x00076, 37 },
  { /*  8 bits */ 0x000ee, 54 },
  { /*  8 bits */ 0x000ef, 2 },
  { /*  8 bits */ 0x000f0, 5 },
  { /*  8 bits */ 0x000f1, 63 },
  { /*  8 bits */ 0x000f2, 48 },
  { /*  9 bits */ 0x001e6, 7 },
  { /*  9 bits */ 0x001e7, 16 },
  { /*  9 bits */ 0x001e8, 45 },
  { /*  9 bits */ 0x001e9, 14 },
  { /*  9 bits */ 0x001ea, 66 },
  { /*  9 bits */ 0x001eb, 6 },
  { /*  9 bits */ 0x001ec, 21 },
  { /*  9 bits */ 0x001ed, 15 },
  { /*  9 bits */ 0x001ee, 18 },
  { /*  9 bits */ 0x001ef, 11 },
  { /*  9 bits */ 0x001f0, 57 },
  { /*  9 bits */ 0x001f1, 49 },
  { /*  9 bits */ 0x001f2, 22 },
  { /*  9 bits */ 0x001f3, 42 },
  { /*  9 bits */ 0x001f4, 43 },
  { /* 10 bits */ 0x003ea, 46 },
  { /* 10 bits */ 0x003eb, 33 },
  { /* 10 bits */ 0x003ec, 34 },
  { /* 10 bits */ 0x003ed, 19 },
  { /* 10 bits */ 0x003ee, 67 },
  { /* 10 bits */ 0x003ef, 41 },
  { /* 10 bits */ 0x003f0, 64 },
  { /* 10 bits */ 0x003f1, 32 },
  { /* 10 bits */ 0x003f2, 8 },
  { /* 10 bits */ 0x003f3, 17 },
  { /* 10 bits */ 0x003f4, 75 },
  { /* 10 bits */ 0x003f5, 51 },
  { /* 10 bits */ 0x003f6, 29 },
  { /* 10 bits */ 0x003f7, 55 },
  { /* 10 bits */ 0x003f8, 25 },
  { /* 11 bits */ 0x007f2, 72 },
  { /* 11 bits */ 0x007f3, 52 },
  { /* 11 bits */ 0x007f4, 38 },
  { /* 11 bits */ 0x007f5, 58 },
  { /* 11 bits */ 0x007f6, 44 },
  { /* 11 bits */ 0x007f7, 76 },
  { /* 11 bits */ 0x007f8, 24 },
  { /* 11 bits */ 0x007f9, 23 },
  { /* 12 bits */ 0x00ff4, 35 },
  { /* 12 bits */ 0x00ff5, 73 },
  { /* 12 bits */ 0x00ff6, 69 },
  { /* 12 bits */ 0x00ff7, 78 },
  { /* 12 bits */ 0x00ff8, 26 },
  { /* 12 bits */ 0x00ff9, 79 },
  { /* 12 bits */ 0x00ffa, 70 },
  { /* 12 bits */ 0x00ffb, 50 },
  { /* 12 bits */ 0x00ffc, 53 },
  { /* 13 bits */ 0x01ffa, 20 },
  { /* 13 bits */ 0x01ffb, 60 },
  { /* 13 bits */ 0x01ffc, 47 },
  { /* 14 bits */ 0x03ffa, 61 },
  { /* 14 bits */ 0x03ffb, 68 },
  { /* 14 bits */ 0x03ffc, 65 },
  { /* 15 bits */ 0x07ffa, 80 },
  { /* 15 bits */ 0x07ffb, 77 },
  { /* 15 bits */ 0x07ffc, 71 },
  { /* 15 bits */ 0x07ffd, 59 },
  { /* 15 bits */ 0x07ffe, 56 },
  { /* 16 bits */ 0x0fffe, 74 },
  { /* 16 bits */ 0x0ffff, 62 },
  /* codebook 4 -- 2-byte codeword, 1-byte index */
  { /*  4 bits */ 0x00000, 40 },
  { /*  4 bits */ 0x00001, 13 },
  { /*  4 bits */ 0x00002, 37 },
  { /*  4 bits */ 0x00003, 39 },
  { /*  4 bits */ 0x00004, 31 },
  { /*  4 bits */ 0x00005, 27 },
  { /*  4 bits */ 0x00006, 36 },
  { /*  4 bits */ 0x00007, 0 },
  { /*  4 bits */ 0x00008, 4 },
  { /*  4 bits */ 0x00009, 30 },
  { /*  5 bits */ 0x00014, 28 },
  { /*  5 bits */ 0x00015, 12 },
  { /*  5 bits */ 0x00016, 1 },
  { /*  5 bits */ 0x00017, 10 },
  { /*  5 bits */ 0x00018, 3 },
  { /*  5 bits */ 0x00019, 9 },
  { /*  7 bits */ 0x00068, 67 },
  { /*  7 bits */ 0x00069, 43 },
  { /*  7 bits */ 0x0006a, 49 },
  { /*  7 bits */ 0x0006b, 41 },
  { /*  7 bits */ 0x0006c, 66 },
  { /*  7 bits */ 0x0006d, 64 },
  { /*  7 bits */ 0x0006e, 48 },
  { /*  7 bits */ 0x0006f, 58 },
  { /*  7 bits */ 0x00070, 16 },
  { /*  8 bits */ 0x000e2, 14 },
  { /*  8 bits */ 0x000e3, 42 },
  { /*  8 bits */ 0x000e4, 22 },
  { /*  8 bits */ 0x000e5, 32 },
  { /*  8 bits */ 0x000e6, 46 },
  { /*  8 bits */ 0x000e7, 38 },
  { /*  8 bits */ 0x000e8, 34 },
  { /*  8 bits */ 0x000e9, 63 },
  { /*  8 bits */ 0x000ea, 57 },
  { /*  8 bits */ 0x000eb, 45 },
  { /*  8 bits */ 0x000ec, 55 },
  { /*  8 bits */ 0x000ed, 11 },
  { /*  8 bits */ 0x000ee, 21 },
  { /*  8 bits */ 0x000ef, 5 },
  { /*  8 bits */ 0x000f0, 15 },
  { /*  8 bits */ 0x000f1, 19 },
  { /*  8 bits */ 0x000f2, 29 },
  { /*  8 bits */ 0x000f3, 7 },
  { /*  8 bits */ 0x000f4, 33 },
  { /*  8 bits */ 0x000f5, 54 },
  { /*  8 bits */ 0x000f6, 2 },
  { /*  9 bits */ 0x001ee, 18 },
  { /*  9 bits */ 0x001ef, 6 },
  { /*  9 bits */ 0x001f0, 52 },
  { /*  9 bits */ 0x001f1, 76 },
  { /*  9 bits */ 0x001f2, 70 },
  { /*  9 bits */ 0x001f3, 44 },
  { /*  9 bits */ 0x001f4, 50 },
  { /*  9 bits */ 0x001f5, 68 },
  { /* 10 bits */ 0x003ec, 51 },
  { /* 10 bits */ 0x003ed, 75 },
  { /* 10 bits */ 0x003ee, 69 },
  { /* 10 bits */ 0x003ef, 25 },
  { /* 10 bits */ 0x003f0, 17 },
  { /* 10 bits */ 0x003f1, 73 },
  { /* 10 bits */ 0x003f2, 23 },
  { /* 10 bits */ 0x003f3, 61 },
  { /* 10 bits */ 0x003f4, 35 },
  { /* 10 bits */ 0x003f5, 79 },
  { /* 10 bits */ 0x003f6, 47 },
  { /* 10 bits */ 0x003f7, 59 },
  { /* 10 bits */ 0x003f8, 65 },
  { /* 10 bits */ 0x003f9, 53 },
  { /* 11 bits */ 0x007f4, 71 },
  { /* 11 bits */ 0x007f5, 77 },
  { /* 11 bits */ 0x007f6, 24 },
  { /* 11 bits */ 0x007f7, 72 },
  { /* 11 bits */ 0x007f8, 8 },
  { /* 11 bits */ 0x007f9, 60 },
  { /* 11 bits */ 0x007fa, 20 },
  { /* 11 bits */ 0x007fb, 56 },
  { /* 11 bits */ 0x007fc, 80 },
  { /* 11 bits */ 0x007fd, 26 },
  { /* 11 bits */ 0x007fe, 78 },
  { /* 12 bits */ 0x00ffe, 74 },
  { /* 12 bits */ 0x00fff, 62 },
  /* codebook 5 -- 2-byte codeword, 1-byte index */
  { /*  1 bits */ 0x00000, 40 },
  { /*  4 bits */ 0x00008, 31 },
  { /*  4 bits */ 0x00009, 49 },
  { /*  4 bits */ 0x0000a, 41 },
  { /*  4 bits */ 0x0000b, 39 },
  { /*  5 bits */ 0x00018, 48 },
  { /*  5 bits */ 0x00019, 32 },
  { /*  5 bits */ 0x0001a, 30 },
  { /*  5 bits */ 0x0001b, 50 },
  { /*  7 bits */ 0x00070, 22 },
  { /*  7 bits */ 0x00071, 42 },
  { /*  7 bits */ 0x00072, 58 },
  { /*  7 bits */ 0x00073, 38 },
  { /*  8 bits */ 0x000e8, 21 },
  { /*  8 bits */ 0x000e9, 59 },
  { /*  8 bits */ 0x000ea, 29 },
  { /*  8 bits */ 0x000eb, 51 },
  { /*  8 bits */ 0x000ec, 23 },
  { /*  8 bits */ 0x000ed, 57 },
  { /*  8 bits */ 0x000ee, 33 },
  { /*  8 bits */ 0x000ef, 47 },
  { /*  8 bits */ 0x000f0, 13 },
  { /*  8 bits */ 0x000f1, 67 },
  { /*  8 bits */ 0x000f2, 37 },
  { /*  8 bits */ 0x000f3, 43 },
  { /*  9 bits */ 0x001e8, 12 },
  { /*  9 bits */ 0x001e9, 52 },
  { /*  9 bits */ 0x001ea, 68 },
  { /*  9 bits */ 0x001eb, 28 },
  { /*  9 bits */ 0x001ec, 14 },
  { /*  9 bits */ 0x001ed, 66 },
  { /*  9 bits */ 0x001ee, 46 },
  { /*  9 bits */ 0x001ef, 34 },
  { /*  9 bits */ 0x001f0, 24 },
  { /*  9 bits */ 0x001f1, 60 },
  { /*  9 bits */ 0x001f2, 20 },
  { /*  9 bits */ 0x001f3, 56 },
  { /* 10 bits */ 0x003e8, 11 },
  { /* 10 bits */ 0x003e9, 65 },
  { /* 10 bits */ 0x003ea, 25 },
  { /* 10 bits */ 0x003eb, 55 },
  { /* 10 bits */ 0x003ec, 69 },
  { /* 10 bits */ 0x003ed, 61 },
  { /* 10 bits */ 0x003ee, 15 },
  { /* 10 bits */ 0x003ef, 19 },
  { /* 10 bits */ 0x003f0, 36 },
  { /* 10 bits */ 0x003f1, 4 },
  { /* 10 bits */ 0x003f2, 77 },
  { /* 10 bits */ 0x003f3, 76 },
  { /* 11 bits */ 0x007e8, 3 },
  { /* 11 bits */ 0x007e9, 44 },
  { /* 11 bits */ 0x007ea, 75 },
  { /* 11 bits */ 0x007eb, 27 },
  { /* 11 bits */ 0x007ec, 53 },
  { /* 11 bits */ 0x007ed, 35 },
  { /* 11 bits */ 0x007ee, 5 },
  { /* 11 bits */ 0x007ef, 45 },
  { /* 11 bits */ 0x007f0, 64 },
  { /* 11 bits */ 0x007f1, 10 },
  { /* 11 bits */ 0x007f2, 16 },
  { /* 11 bits */ 0x007f3, 26 },
  { /* 11 bits */ 0x007f4, 2 },
  { /* 11 bits */ 0x007f5, 78 },
  { /* 11 bits */ 0x007f6, 54 },
  { /* 11 bits */ 0x007f7, 62 },
  { /* 11 bits */ 0x007f8, 70 },
  { /* 11 bits */ 0x007f9, 6 },
  { /* 12 bits */ 0x00ff4, 18 },
  { /* 12 bits */ 0x00ff5, 74 },
  { /* 12 bits */ 0x00ff6, 63 },
  { /* 12 bits */ 0x00ff7, 1 },
  { /* 12 bits */ 0x00ff8, 7 },
  { /* 12 bits */ 0x00ff9, 71 },
  { /* 12 bits */ 0x00ffa, 17 },
  { /* 12 bits */ 0x00ffb, 79 },
  { /* 12 bits */ 0x00ffc, 73 },
  { /* 12 bits */ 0x00ffd, 9 },
  { /* 13 bits */ 0x01ffc, 72 },
  { /* 13 bits */ 0x01ffd, 8 },
  { /* 13 bits */ 0x01ffe, 80 },
  { /* 13 bits */ 0x01fff, 0 },
  /* codebook 6 -- 2-byte codeword, 1-byte index */
  { /*  4 bits */ 0x00000, 40 },
  { /*  4 bits */ 0x00001, 49 },
  { /*  4 bits */ 0x00002, 39 },
  { /*  4 bits */ 0x00003, 41 },
  { /*  4 bits */ 0x00004, 31 },
  { /*  4 bits */ 0x00005, 50 },
  { /*  4 bits */ 0x00006, 32 },
  { /*  4 bits */ 0x00007, 48 },
  { /*  4 bits */ 0x00008, 30 },
  { /*  6 bits */ 0x00024, 57 },
  { /*  6 bits */ 0x00025, 59 },
  { /*  6 bits */ 0x00026, 23 },
  { /*  6 bits */ 0x00027, 21 },
  { /*  6 bits */ 0x00028, 22 },
  { /*  6 bits */ 0x00029, 33 },
  { /*  6 bits */ 0x0002a, 58 },
  { /*  6 bits */ 0x0002b, 47 },
  { /*  6 bits */ 0x0002c, 51 },
  { /*  6 bits */ 0x0002d, 38 },
  { /*  6 bits */ 0x0002e, 29 },
  { /*  6 bits */ 0x0002f, 42 },
  { /*  6 bits */ 0x00030, 56 },
  { /*  6 bits */ 0x00031, 24 },
  { /*  6 bits */ 0x00032, 20 },
  { /*  6 bits */ 0x00033, 60 },
  { /*  7 bits */ 0x00068, 14 },
  { /*  7 bits */ 0x00069, 68 },
  { /*  7 bits */ 0x0006a, 66 },
  { /*  7 bits */ 0x0006b, 34 },
  { /*  7 bits */ 0x0006c, 12 },
  { /*  7 bits */ 0x0006d, 52 },
  { /*  7 bits */ 0x0006e, 46 },
  { /*  7 bits */ 0x0006f, 28 },
  { /*  7 bits */ 0x00070, 67 },
  { /*  7 bits */ 0x00071, 13 },
  { /*  7 bits */ 0x00072, 37 },
  { /*  7 bits */ 0x00073, 43 },
  { /*  7 bits */ 0x00074, 69 },
  { /*  8 bits */ 0x000ea, 11 },
  { /*  8 bits */ 0x000eb, 25 },
  { /*  8 bits */ 0x000ec, 61 },
  { /*  8 bits */ 0x000ed, 65 },
  { /*  8 bits */ 0x000ee, 55 },
  { /*  8 bits */ 0x000ef, 19 },
  { /*  8 bits */ 0x000f0, 15 },
  { /*  8 bits */ 0x000f1, 70 },
  { /*  9 bits */ 0x001e4, 64 },
  { /*  9 bits */ 0x001e5, 10 },
  { /*  9 bits */ 0x001e6, 16 },
  { /*  9 bits */ 0x001e7, 45 },
  { /*  9 bits */ 0x001e8, 27 },
  { /*  9 bits */ 0x001e9, 77 },
  { /*  9 bits */ 0x001ea, 5 },
  { /*  9 bits */ 0x001eb, 3 },
  { /*  9 bits */ 0x001ec, 53 },
  { /*  9 bits */ 0x001ed, 75 },
  { /*  9 bits */ 0x001ee, 35 },
  { /*  9 bits */ 0x001ef, 36 },
  { /*  9 bits */ 0x001f0, 6 },
  { /*  9 bits */ 0x001f1, 2 },
  { /*  9 bits */ 0x001f2, 62 },
  { /*  9 bits */ 0x001f3, 18 },
  { /*  9 bits */ 0x001f4, 4 },
  { /*  9 bits */ 0x001f5, 78 },
  { /*  9 bits */ 0x001f6, 74 },
  { /*  9 bits */ 0x001f7, 26 },
  { /*  9 bits */ 0x001f8, 76 },
  { /*  9 bits */ 0x001f9, 54 },
  { /*  9 bits */ 0x001fa, 44 },
  { /* 10 bits */ 0x003f6, 9 },
  { /* 10 bits */ 0x003f7, 17 },
  { /* 10 bits */ 0x003f8, 63 },
  { /* 10 bits */ 0x003f9, 73 },
  { /* 10 bits */ 0x003fa, 71 },
  { /* 10 bits */ 0x003fb, 79 },
  { /* 10 bits */ 0x003fc, 7 },
  { /* 10 bits */ 0x003fd, 1 },
  { /* 11 bits */ 0x007fc, 80 },
  { /* 11 bits */ 0x007fd, 8 },
  { /* 11 bits */ 0x007fe, 0 },
  { /* 11 bits */ 0x007ff, 72 },
  /* codebook 7 -- 2-byte codeword, 1-byte index */
  { /*  1 bits */ 0x00000, 0 },
  { /*  3 bits */ 0x00004, 8 },
  { /*  3 bits */ 0x00005, 1 },
  { /*  4 bits */ 0x0000c, 9 },
  { /*  6 bits */ 0x00034, 17 },
  { /*  6 bits */ 0x00035, 10 },
  { /*  6 bits */ 0x00036, 16 },
  { /*  6 bits */ 0x00037, 2 },
  { /*  7 bits */ 0x00070, 25 },
  { /*  7 bits */ 0x00071, 11 },
  { /*  7 bits */ 0x00072, 18 },
  { /*  7 bits */ 0x00073, 24 },
  { /*  7 bits */ 0x00074, 3 },
  { /*  8 bits */ 0x000ea, 19 },
  { /*  8 bits */ 0x000eb, 26 },
  { /*  8 bits */ 0x000ec, 12 },
  { /*  8 bits */ 0x000ed, 33 },
  { /*  8 bits */ 0x000ee, 13 },
  { /*  8 bits */ 0x000ef, 41 },
  { /*  8 bits */ 0x000f0, 27 },
  { /*  8 bits */ 0x000f1, 20 },
  { /*  8 bits */ 0x000f2, 4 },
  { /*  8 bits */ 0x000f3, 32 },
  { /*  9 bits */ 0x001e8, 34 },
  { /*  9 bits */ 0x001e9, 21 },
  { /*  9 bits */ 0x001ea, 42 },
  { /*  9 bits */ 0x001eb, 5 },
  { /*  9 bits */ 0x001ec, 49 },
  { /*  9 bits */ 0x001ed, 40 },
  { /*  9 bits */ 0x001ee, 14 },
  { /*  9 bits */ 0x001ef, 35 },
  { /*  9 bits */ 0x001f0, 29 },
  { /*  9 bits */ 0x001f1, 28 },
  { /*  9 bits */ 0x001f2, 43 },
  { /*  9 bits */ 0x001f3, 22 },
  { /*  9 bits */ 0x001f4, 50 },
  { /*  9 bits */ 0x001f5, 15 },
  { /* 10 bits */ 0x003ec, 30 },
  { /* 10 bits */ 0x003ed, 6 },
  { /* 10 bits */ 0x003ee, 48 },
  { /* 10 bits */ 0x003ef, 36 },
  { /* 10 bits */ 0x003f0, 57 },
  { /* 10 bits */ 0x003f1, 37 },
  { /* 10 bits */ 0x003f2, 58 },
  { /* 10 bits */ 0x003f3, 44 },
  { /* 10 bits */ 0x003f4, 51 },
  { /* 10 bits */ 0x003f5, 23 },
  { /* 10 bits */ 0x003f6, 59 },
  { /* 10 bits */ 0x003f7, 52 },
  { /* 10 bits */ 0x003f8, 45 },
  { /* 10 bits */ 0x003f9, 38 },
  { /* 10 bits */ 0x003fa, 31 },
  { /* 11 bits */ 0x007f6, 56 },
  { /* 11 bits */ 0x007f7, 7 },
  { /* 11 bits */ 0x007f8, 53 },
  { /* 11 bits */ 0x007f9, 46 },
  { /* 11 bits */ 0x007fa, 60 },
  { /* 11 bits */ 0x007fb, 39 },
  { /* 11 bits */ 0x007fc, 47 },
  { /* 11 bits */ 0x007fd, 61 },
  { /* 12 bits */ 0x00ffc, 62 },
  { /* 12 bits */ 0x00ffd, 54 },
  { /* 12 bits */ 0x00ffe, 55 },
  { /* 12 bits */ 0x00fff, 63 },
  /* codebook 8 -- 2-byte codeword, 1-byte index */
  { /*  3 bits */ 0x00000, 9 },
  { /*  4 bits */ 0x00002, 17 },
  { /*  4 bits */ 0x00003, 8 },
  { /*  4 bits */ 0x00004, 10 },
  { /*  4 bits */ 0x00005, 1 },
  { /*  4 bits */ 0x00006, 18 },
  { /*  5 bits */ 0x0000e, 0 },
  { /*  5 bits */ 0x0000f, 16 },
  { /*  5 bits */ 0x00010, 2 },
  { /*  5 bits */ 0x00011, 25 },
  { /*  5 bits */ 0x00012, 11 },
  { /*  5 bits */ 0x00013, 26 },
  { /*  5 bits */ 0x00014, 19 },
  { /*  6 bits */ 0x0002a, 27 },
  { /*  6 bits */ 0x0002b, 33 },
  { /*  6 bits */ 0x0002c, 12 },
  { /*  6 bits */ 0x0002d, 34 },
  { /*  6 bits */ 0x0002e, 20 },
  { /*  6 bits */ 0x0002f, 24 },
  { /*  6 bits */ 0x00030, 3 },
  { /*  6 bits */ 0x00031, 35 },
  { /*  6 bits */ 0x00032, 28 },
  { /*  6 bits */ 0x00033, 42 },
  { /*  7 bits */ 0x00068, 41 },
  { /*  7 bits */ 0x00069, 21 },
  { /*  7 bits */ 0x0006a, 13 },
  { /*  7 bits */ 0x0006b, 43 },
  { /*  7 bits */ 0x0006c, 29 },
  { /*  7 bits */ 0x0006d, 36 },
  { /*  7 bits */ 0x0006e, 44 },
  { /*  7 bits */ 0x0006f, 4 },
  { /*  7 bits */ 0x00070, 37 },
  { /*  7 bits */ 0x00071, 32 },
  { /*  7 bits */ 0x00072, 22 },
  { /*  7 bits */ 0x00073, 50 },
  { /*  7 bits */ 0x00074, 49 },
  { /*  7 bits */ 0x00075, 14 },
  { /*  8 bits */ 0x000ec, 30 },
  { /*  8 bits */ 0x000ed, 51 },
  { /*  8 bits */ 0x000ee, 45 },
  { /*  8 bits */ 0x000ef, 40 },
  { /*  8 bits */ 0x000f0, 52 },
  { /*  8 bits */ 0x000f1, 5 },
  { /*  8 bits */ 0x000f2, 38 },
  { /*  8 bits */ 0x000f3, 57 },
  { /*  8 bits */ 0x000f4, 58 },
  { /*  8 bits */ 0x000f5, 23 },
  { /*  8 bits */ 0x000f6, 53 },
  { /*  8 bits */ 0x000f7, 59 },
  { /*  8 bits */ 0x000f8, 15 },
  { /*  8 bits */ 0x000f9, 46 },
  { /*  8 bits */ 0x000fa, 31 },
  { /*  9 bits */ 0x001f6, 54 },
  { /*  9 bits */ 0x001f7, 60 },
  { /*  9 bits */ 0x001f8, 48 },
  { /*  9 bits */ 0x001f9, 39 },
  { /*  9 bits */ 0x001fa, 6 },
  { /*  9 bits */ 0x001fb, 61 },
  { /*  9 bits */ 0x001fc, 62 },
  { /*  9 bits */ 0x001fd, 55 },
  { /* 10 bits */ 0x003fc, 47 },
  { /* 10 bits */ 0x003fd, 56 },
  { /* 10 bits */ 0x003fe, 7 },
  { /* 10 bits */ 0x003ff, 63 },
  /* codebook 9 -- 2-byte codeword, 1-byte index */
  { /*  1 bits */ 0x00000, 0 },
  { /*  3 bits */ 0x00004, 13 },
  { /*  3 bits */ 0x00005, 1 },
  { /*  4 bits */ 0x0000c, 14 },
  { /*  6 bits */ 0x00034, 27 },
  { /*  6 bits */ 0x00035, 15 },
  { /*  6 bits */ 0x00036, 26 },
  { /*  6 bits */ 0x00037, 2 },
  { /*  7 bits */ 0x00070, 40 },
  { /*  7 bits */ 0x00071, 28 },
  { /*  7 bits */ 0x00072, 16 },
  { /*  8 bits */ 0x000e6, 39 },
  { /*  8 bits */ 0x000e7, 3 },
  { /*  8 bits */ 0x000e8, 29 },
  { /*  8 bits */ 0x000e9, 41 },
  { /*  8 bits */ 0x000ea, 17 },
  { /*  8 bits */ 0x000eb, 53 },
  { /*  8 bits */ 0x000ec, 30 },
  { /*  8 bits */ 0x000ed, 18 },
  { /*  9 bits */ 0x001dc, 54 },
  { /*  9 bits */ 0x001dd, 42 },
  { /*  9 bits */ 0x001de, 4 },
  { /*  9 bits */ 0x001df, 52 },
  { /*  9 bits */ 0x001e0, 66 },
  { /*  9 bits */ 0x001e1, 31 },
  { /*  9 bits */ 0x001e2, 19 },
  { /*  9 bits */ 0x001e3, 43 },
  { /*  9 bits */ 0x001e4, 67 },
  { /*  9 bits */ 0x001e5, 79 },
  { /*  9 bits */ 0x001e6, 55 },
  { /* 10 bits */ 0x003ce, 5 },
  { /* 10 bits */ 0x003cf, 32 },
  { /* 10 bits */ 0x003d0, 65 },
  { /* 10 bits */ 0x003d1, 20 },
  { /* 10 bits */ 0x003d2, 44 },
  { /* 10 bits */ 0x003d3, 21 },
  { /* 10 bits */ 0x003d4, 105 },
  { /* 10 bits */ 0x003d5, 56 },
  { /* 10 bits */ 0x003d6, 68 },
  { /* 10 bits */ 0x003d7, 80 },
  { /* 10 bits */ 0x003d8, 92 },
  { /* 10 bits */ 0x003d9, 6 },
  { /* 10 bits */ 0x003da, 106 },
  { /* 10 bits */ 0x003db, 34 },
  { /* 10 bits */ 0x003dc, 45 },
  { /* 10 bits */ 0x003dd, 33 },
  { /* 10 bits */ 0x003de, 57 },
  { /* 10 bits */ 0x003df, 118 },
  { /* 10 bits */ 0x003e0, 22 },
  { /* 10 bits */ 0x003e1, 93 },
  { /* 11 bits */ 0x007c4, 78 },
  { /* 11 bits */ 0x007c5, 69 },
  { /* 11 bits */ 0x007c6, 81 },
  { /* 11 bits */ 0x007c7, 107 },
  { /* 11 bits */ 0x007c8, 7 },
  { /* 11 bits */ 0x007c9, 119 },
  { /* 11 bits */ 0x007ca, 47 },
  { /* 11 bits */ 0x007cb, 58 },
  { /* 11 bits */ 0x007cc, 46 },
  { /* 11 bits */ 0x007cd, 8 },
  { /* 11 bits */ 0x007ce, 131 },
  { /* 11 bits */ 0x007cf, 82 },
  { /* 11 bits */ 0x007d0, 35 },
  { /* 11 bits */ 0x007d1, 70 },
  { /* 11 bits */ 0x007d2, 104 },
  { /* 11 bits */ 0x007d3, 91 },
  { /* 11 bits */ 0x007d4, 94 },
  { /* 11 bits */ 0x007d5, 132 },
  { /* 11 bits */ 0x007d6, 120 },
  { /* 11 bits */ 0x007d7, 108 },
  { /* 11 bits */ 0x007d8, 23 },
  { /* 11 bits */ 0x007d9, 95 },
  { /* 11 bits */ 0x007da, 83 },
  { /* 11 bits */ 0x007db, 71 },
  { /* 11 bits */ 0x007dc, 60 },
  { /* 11 bits */ 0x007dd, 59 },
  { /* 11 bits */ 0x007de, 48 },
  { /* 11 bits */ 0x007df, 144 },
  { /* 11 bits */ 0x007e0, 73 },
  { /* 11 bits */ 0x007e1, 117 },
  { /* 11 bits */ 0x007e2, 109 },
  { /* 12 bits */ 0x00fc6, 133 },
  { /* 12 bits */ 0x00fc7, 36 },
  { /* 12 bits */ 0x00fc8, 9 },
  { /* 12 bits */ 0x00fc9, 145 },
  { /* 12 bits */ 0x00fca, 121 },
  { /* 12 bits */ 0x00fcb, 84 },
  { /* 12 bits */ 0x00fcc, 157 },
  { /* 12 bits */ 0x00fcd, 61 },
  { /* 12 bits */ 0x00fce, 110 },
  { /* 12 bits */ 0x00fcf, 24 },
  { /* 12 bits */ 0x00fd0, 122 },
  { /* 12 bits */ 0x00fd1, 134 },
  { /* 12 bits */ 0x00fd2, 72 },
  { /* 12 bits */ 0x00fd3, 96 },
  { /* 12 bits */ 0x00fd4, 37 },
  { /* 12 bits */ 0x00fd5, 25 },
  { /* 12 bits */ 0x00fd6, 158 },
  { /* 12 bits */ 0x00fd7, 146 },
  { /* 12 bits */ 0x00fd8, 49 },
  { /* 12 bits */ 0x00fd9, 74 },
  { /* 12 bits */ 0x00fda, 85 },
  { /* 12 bits */ 0x00fdb, 111 },
  { /* 12 bits */ 0x00fdc, 147 },
  { /* 12 bits */ 0x00fdd, 10 },
  { /* 12 bits */ 0x00fde, 97 },
  { /* 12 bits */ 0x00fdf, 159 },
  { /* 12 bits */ 0x00fe0, 130 },
  { /* 12 bits */ 0x00fe1, 135 },
  { /* 12 bits */ 0x00fe2, 62 },
  { /* 12 bits */ 0x00fe3, 86 },
  { /* 12 bits */ 0x00fe4, 38 },
  { /* 12 bits */ 0x00fe5, 123 },
  { /* 12 bits */ 0x00fe6, 124 },
  { /* 12 bits */ 0x00fe7, 63 },
  { /* 12 bits */ 0x00fe8, 143 },
  { /* 12 bits */ 0x00fe9, 87 },
  { /* 12 bits */ 0x00fea, 50 },
  { /* 12 bits */ 0x00feb, 75 },
  { /* 13 bits */ 0x01fd8, 112 },
  { /* 13 bits */ 0x01fd9, 99 },
  { /* 13 bits */ 0x01fda, 161 },
  { /* 13 bits */ 0x01fdb, 51 },
  { /* 13 bits */ 0x01fdc, 148 },
  { /* 13 bits */ 0x01fdd, 98 },
  { /* 13 bits */ 0x01fde, 160 },
  { /* 13 bits */ 0x01fdf, 149 },
  { /* 13 bits */ 0x01fe0, 136 },
  { /* 13 bits */ 0x01fe1, 64 },
  { /* 13 bits */ 0x01fe2, 100 },
  { /* 13 bits */ 0x01fe3, 76 },
  { /* 13 bits */ 0x01fe4, 11 },
  { /* 13 bits */ 0x01fe5, 162 },
  { /* 13 bits */ 0x01fe6, 88 },
  { /* 13 bits */ 0x01fe7, 156 },
  { /* 13 bits */ 0x01fe8, 137 },
  { /* 13 bits */ 0x01fe9, 77 },
  { /* 13 bits */ 0x01fea, 101 },
  { /* 13 bits */ 0x01feb, 125 },
  { /* 13 bits */ 0x01fec, 12 },
  { /* 13 bits */ 0x01fed, 150 },
  { /* 13 bits */ 0x01fee, 113 },
  { /* 13 bits */ 0x01fef, 126 },
  { /* 13 bits */ 0x01ff0, 138 },
  { /* 13 bits */ 0x01ff1, 102 },
  { /* 13 bits */ 0x01ff2, 163 },
  { /* 13 bits */ 0x01ff3, 89 },
  { /* 13 bits */ 0x01ff4, 115 },
  { /* 13 bits */ 0x01ff5, 151 },
  { /* 13 bits */ 0x01ff6, 103 },
  { /* 13 bits */ 0x01ff7, 90 },
  { /* 14 bits */ 0x03ff0, 114 },
  { /* 14 bits */ 0x03ff1, 139 },
  { /* 14 bits */ 0x03ff2, 116 },
  { /* 14 bits */ 0x03ff3, 127 },
  { /* 14 bits */ 0x03ff4, 128 },
  { /* 14 bits */ 0x03ff5, 129 },
  { /* 14 bits */ 0x03ff6, 141 },
  { /* 14 bits */ 0x03ff7, 165 },
  { /* 14 bits */ 0x03ff8, 140 },
  { /* 14 bits */ 0x03ff9, 152 },
  { /* 14 bits */ 0x03ffa, 164 },
  { /* 14 bits */ 0x03ffb, 153 },
  { /* 14 bits */ 0x03ffc, 166 },
  { /* 14 bits */ 0x03ffd, 167 },
  { /* 15 bits */ 0x07ffc, 142 },
  { /* 15 bits */ 0x07ffd, 154 },
  { /* 15 bits */ 0x07ffe, 155 },
  { /* 15 bits */ 0x07fff, 168 },
  /* codebook 10 -- 2-byte codeword, 1-byte index */
  { /*  4 bits */ 0x00000, 14 },
  { /*  4 bits */ 0x00001, 15 },
  { /*  4 bits */ 0x00002, 27 },
  { /*  5 bits */ 0x00006, 28 },
  { /*  5 bits */ 0x00007, 13 },
  { /*  5 bits */ 0x00008, 1 },
  { /*  5 bits */ 0x00009, 16 },
  { /*  5 bits */ 0x0000a, 41 },
  { /*  5 bits */ 0x0000b, 40 },
  { /*  5 bits */ 0x0000c, 29 },
  { /*  5 bits */ 0x0000d, 42 },
  { /*  6 bits */ 0x0001c, 26 },
  { /*  6 bits */ 0x0001d, 2 },
  { /*  6 bits */ 0x0001e, 30 },
  { /*  6 bits */ 0x0001f, 54 },
  { /*  6 bits */ 0x00020, 17 },
  { /*  6 bits */ 0x00021, 53 },
  { /*  6 bits */ 0x00022, 0 },
  { /*  6 bits */ 0x00023, 55 },
  { /*  6 bits */ 0x00024, 43 },
  { /*  6 bits */ 0x00025, 39 },
  { /*  6 bits */ 0x00026, 3 },
  { /*  6 bits */ 0x00027, 56 },
  { /*  6 bits */ 0x00028, 31 },
  { /*  6 bits */ 0x00029, 67 },
  { /*  7 bits */ 0x00054, 18 },
  { /*  7 bits */ 0x00055, 66 },
  { /*  7 bits */ 0x00056, 68 },
  { /*  7 bits */ 0x00057, 44 },
  { /*  7 bits */ 0x00058, 69 },
  { /*  7 bits */ 0x00059, 57 },
  { /*  7 bits */ 0x0005a, 80 },
  { /*  7 bits */ 0x0005b, 32 },
  { /*  7 bits */ 0x0005c, 81 },
  { /*  7 bits */ 0x0005d, 52 },
  { /*  7 bits */ 0x0005e, 79 },
  { /*  7 bits */ 0x0005f, 4 },
  { /*  7 bits */ 0x00060, 19 },
  { /*  7 bits */ 0x00061, 45 },
  { /*  7 bits */ 0x00062, 70 },
  { /*  7 bits */ 0x00063, 82 },
  { /*  7 bits */ 0x00064, 58 },
  { /*  8 bits */ 0x000ca, 83 },
  { /*  8 bits */ 0x000cb, 93 },
  { /*  8 bits */ 0x000cc, 46 },
  { /*  8 bits */ 0x000cd, 33 },
  { /*  8 bits */ 0x000ce, 71 },
  { /*  8 bits */ 0x000cf, 106 },
  { /*  8 bits */ 0x000d0, 94 },
  { /*  8 bits */ 0x000d1, 65 },
  { /*  8 bits */ 0x000d2, 92 },
  { /*  8 bits */ 0x000d3, 5 },
  { /*  8 bits */ 0x000d4, 105 },
  { /*  8 bits */ 0x000d5, 20 },
  { /*  8 bits */ 0x000d6, 107 },
  { /*  8 bits */ 0x000d7, 95 },
  { /*  8 bits */ 0x000d8, 59 },
  { /*  8 bits */ 0x000d9, 34 },
  { /*  8 bits */ 0x000da, 84 },
  { /*  8 bits */ 0x000db, 96 },
  { /*  8 bits */ 0x000dc, 21 },
  { /*  8 bits */ 0x000dd, 47 },
  { /*  8 bits */ 0x000de, 108 },
  { /*  8 bits */ 0x000df, 60 },
  { /*  8 bits */ 0x000e0, 72 },
  { /*  8 bits */ 0x000e1, 109 },
  { /*  8 bits */ 0x000e2, 73 },
  { /*  9 bits */ 0x001c6, 97 },
  { /*  9 bits */ 0x001c7, 85 },
  { /*  9 bits */ 0x001c8, 119 },
  { /*  9 bits */ 0x001c9, 78 },
  { /*  9 bits */ 0x001ca, 86 },
  { /*  9 bits */ 0x001cb, 120 },
  { /*  9 bits */ 0x001cc, 48 },
  { /*  9 bits */ 0x001cd, 118 },
  { /*  9 bits */ 0x001ce, 35 },
  { /*  9 bits */ 0x001cf, 6 },
  { /*  9 bits */ 0x001d0, 110 },
  { /*  9 bits */ 0x001d1, 121 },
  { /*  9 bits */ 0x001d2, 61 },
  { /*  9 bits */ 0x001d3, 132 },
  { /*  9 bits */ 0x001d4, 22 },
  { /*  9 bits */ 0x001d5, 98 },
  { /*  9 bits */ 0x001d6, 111 },
  { /*  9 bits */ 0x001d7, 122 },
  { /*  9 bits */ 0x001d8, 99 },
  { /*  9 bits */ 0x001d9, 133 },
  { /*  9 bits */ 0x001da, 74 },
  { /*  9 bits */ 0x001db, 134 },
  { /*  9 bits */ 0x001dc, 36 },
  { /*  9 bits */ 0x001dd, 131 },
  { /*  9 bits */ 0x001de, 49 },
  { /*  9 bits */ 0x001df, 123 },
  { /*  9 bits */ 0x001e0, 87 },
  { /*  9 bits */ 0x001e1, 104 },
  { /*  9 bits */ 0x001e2, 62 },
  { /*  9 bits */ 0x001e3, 91 },
  { /*  9 bits */ 0x001e4, 145 },
  { /* 10 bits */ 0x003ca, 100 },
  { /* 10 bits */ 0x003cb, 146 },
  { /* 10 bits */ 0x003cc, 136 },
  { /* 10 bits */ 0x003cd, 23 },
  { /* 10 bits */ 0x003ce, 144 },
  { /* 10 bits */ 0x003cf, 124 },
  { /* 10 bits */ 0x003d0, 7 },
  { /* 10 bits */ 0x003d1, 112 },
  { /* 10 bits */ 0x003d2, 135 },
  { /* 10 bits */ 0x003d3, 50 },
  { /* 10 bits */ 0x003d4, 75 },
  { /* 10 bits */ 0x003d5, 113 },
  { /* 10 bits */ 0x003d6, 148 },
  { /* 10 bits */ 0x003d7, 8 },
  { /* 10 bits */ 0x003d8, 147 },
  { /* 10 bits */ 0x003d9, 37 },
  { /* 10 bits */ 0x003da, 101 },
  { /* 10 bits */ 0x003db, 88 },
  { /* 10 bits */ 0x003dc, 137 },
  { /* 10 bits */ 0x003dd, 63 },
  { /* 10 bits */ 0x003de, 24 },
  { /* 10 bits */ 0x003df, 158 },
  { /* 10 bits */ 0x003e0, 125 },
  { /* 10 bits */ 0x003e1, 159 },
  { /* 10 bits */ 0x003e2, 149 },
  { /* 10 bits */ 0x003e3, 76 },
  { /* 10 bits */ 0x003e4, 160 },
  { /* 10 bits */ 0x003e5, 150 },
  { /* 10 bits */ 0x003e6, 161 },
  { /* 10 bits */ 0x003e7, 51 },
  { /* 10 bits */ 0x003e8, 89 },
  { /* 10 bits */ 0x003e9, 117 },
  { /* 10 bits */ 0x003ea, 138 },
  { /* 10 bits */ 0x003eb, 130 },
  { /* 10 bits */ 0x003ec, 157 },
  { /* 10 bits */ 0x003ed, 9 },
  { /* 10 bits */ 0x003ee, 64 },
  { /* 10 bits */ 0x003ef, 126 },
  { /* 10 bits */ 0x003f0, 162 },
  { /* 10 bits */ 0x003f1, 38 },
  { /* 10 bits */ 0x003f2, 114 },
  { /* 11 bits */ 0x007e6, 127 },
  { /* 11 bits */ 0x007e7, 25 },
  { /* 11 bits */ 0x007e8, 151 },
  { /* 11 bits */ 0x007e9, 163 },
  { /* 11 bits */ 0x007ea, 102 },
  { /* 11 bits */ 0x007eb, 77 },
  { /* 11 bits */ 0x007ec, 90 },
  { /* 11 bits */ 0x007ed, 139 },
  { /* 11 bits */ 0x007ee, 115 },
  { /* 11 bits */ 0x007ef, 164 },
  { /* 11 bits */ 0x007f0, 10 },
  { /* 11 bits */ 0x007f1, 103 },
  { /* 11 bits */ 0x007f2, 143 },
  { /* 11 bits */ 0x007f3, 140 },
  { /* 11 bits */ 0x007f4, 152 },
  { /* 11 bits */ 0x007f5, 153 },
  { /* 11 bits */ 0x007f6, 11 },
  { /* 11 bits */ 0x007f7, 154 },
  { /* 11 bits */ 0x007f8, 128 },
  { /* 11 bits */ 0x007f9, 141 },
  { /* 11 bits */ 0x007fa, 156 },
  { /* 11 bits */ 0x007fb, 116 },
  { /* 12 bits */ 0x00ff8, 165 },
  { /* 12 bits */ 0x00ff9, 142 },
  { /* 12 bits */ 0x00ffa, 129 },
  { /* 12 bits */ 0x00ffb, 155 },
  { /* 12 bits */ 0x00ffc, 167 },
  { /* 12 bits */ 0x00ffd, 12 },
  { /* 12 bits */ 0x00ffe, 166 },
  { /* 12 bits */ 0x00fff, 168 },
  /* codebook 11 -- 2-byte codeword, 2-byte index */
  { /*  4 bits */ 0x00000, 0 },
  { /*  4 bits */ 0x00001, 18 },
  { /*  5 bits */ 0x00004, 288 },
  { /*  5 bits */ 0x00005, 17 },
  { /*  5 bits */ 0x00006, 1 },
  { /*  5 bits */ 0x00007, 35 },
  { /*  5 bits */ 0x00008, 19 },
  { /*  5 bits */ 0x00009, 36 },
  { /*  6 bits */ 0x00014, 20 },
  { /*  6 bits */ 0x00015, 52 },
  { /*  6 bits */ 0x00016, 53 },
  { /*  6 bits */ 0x00017, 34 },
  { /*  6 bits */ 0x00018, 37 },
  { /*  6 bits */ 0x00019, 2 },
  { /*  6 bits */ 0x0001a, 54 },
  { /*  7 bits */ 0x00036, 69 },
  { /*  7 bits */ 0x00037, 21 },
  { /*  7 bits */ 0x00038, 70 },
  { /*  7 bits */ 0x00039, 38 },
  { /*  7 bits */ 0x0003a, 71 },
  { /*  7 bits */ 0x0003b, 55 },
  { /*  7 bits */ 0x0003c, 51 },
  { /*  7 bits */ 0x0003d, 3 },
  { /*  7 bits */ 0x0003e, 86 },
  { /*  7 bits */ 0x0003f, 87 },
  { /*  7 bits */ 0x00040, 39 },
  { /*  7 bits */ 0x00041, 72 },
  { /*  7 bits */ 0x00042, 22 },
  { /*  7 bits */ 0x00043, 88 },
  { /*  7 bits */ 0x00044, 56 },
  { /*  7 bits */ 0x00045, 89 },
  { /*  8 bits */ 0x0008c, 73 },
  { /*  8 bits */ 0x0008d, 104 },
  { /*  8 bits */ 0x0008e, 40 },
  { /*  8 bits */ 0x0008f, 103 },
  { /*  8 bits */ 0x00090, 105 },
  { /*  8 bits */ 0x00091, 57 },
  { /*  8 bits */ 0x00092, 23 },
  { /*  8 bits */ 0x00093, 84 },
  { /*  8 bits */ 0x00094, 67 },
  { /*  8 bits */ 0x00095, 277 },
  { /*  8 bits */ 0x00096, 275 },
  { /*  8 bits */ 0x00097, 276 },
  { /*  8 bits */ 0x00098, 106 },
  { /*  8 bits */ 0x00099, 278 },
  { /*  8 bits */ 0x0009a, 68 },
  { /*  8 bits */ 0x0009b, 74 },
  { /*  8 bits */ 0x0009c, 4 },
  { /*  8 bits */ 0x0009d, 50 },
  { /*  8 bits */ 0x0009e, 90 },
  { /*  8 bits */ 0x0009f, 101 },
  { /*  8 bits */ 0x000a0, 279 },
  { /*  8 bits */ 0x000a1, 274 },
  { /*  8 bits */ 0x000a2, 280 },
  { /*  8 bits */ 0x000a3, 41 },
  { /*  8 bits */ 0x000a4, 121 },
  { /*  8 bits */ 0x000a5, 58 },
  { /*  8 bits */ 0x000a6, 107 },
  { /*  8 bits */ 0x000a7, 91 },
  { /*  8 bits */ 0x000a8, 118 },
  { /*  8 bits */ 0x000a9, 282 },
  { /*  8 bits */ 0x000aa, 122 },
  { /*  8 bits */ 0x000ab, 120 },
  { /*  8 bits */ 0x000ac, 281 },
  { /*  8 bits */ 0x000ad, 135 },
  { /*  8 bits */ 0x000ae, 33 },
  { /*  8 bits */ 0x000af, 24 },
  { /*  8 bits */ 0x000b0, 75 },
  { /*  8 bits */ 0x000b1, 283 },
  { /*  8 bits */ 0x000b2, 123 },
  { /*  8 bits */ 0x000b3, 284 },
  { /*  8 bits */ 0x000b4, 152 },
  { /*  8 bits */ 0x000b5, 273 },
  { /*  8 bits */ 0x000b6, 108 },
  { /*  8 bits */ 0x000b7, 169 },
  { /*  8 bits */ 0x000b8, 42 },
  { /*  8 bits */ 0x000b9, 92 },
  { /*  8 bits */ 0x000ba, 186 },
  { /*  8 bits */ 0x000bb, 285 },
  { /*  8 bits */ 0x000bc, 139 },
  { /*  8 bits */ 0x000bd, 138 },
  { /*  8 bits */ 0x000be, 59 },
  { /*  8 bits */ 0x000bf, 85 },
  { /*  8 bits */ 0x000c0, 286 },
  { /*  8 bits */ 0x000c1, 203 },
  { /*  8 bits */ 0x000c2, 124 },
  { /*  8 bits */ 0x000c3, 76 },
  { /*  8 bits */ 0x000c4, 109 },
  { /*  8 bits */ 0x000c5, 125 },
  { /*  8 bits */ 0x000c6, 5 },
  { /*  9 bits */ 0x0018e, 140 },
  { /*  9 bits */ 0x0018f, 287 },
  { /*  9 bits */ 0x00190, 220 },
  { /*  9 bits */ 0x00191, 25 },
  { /*  9 bits */ 0x00192, 137 },
  { /*  9 bits */ 0x00193, 254 },
  { /*  9 bits */ 0x00194, 93 },
  { /*  9 bits */ 0x00195, 237 },
  { /*  9 bits */ 0x00196, 60 },
  { /*  9 bits */ 0x00197, 141 },
  { /*  9 bits */ 0x00198, 126 },
  { /*  9 bits */ 0x00199, 43 },
  { /*  9 bits */ 0x0019a, 142 },
  { /*  9 bits */ 0x0019b, 155 },
  { /*  9 bits */ 0x0019c, 156 },
  { /*  9 bits */ 0x0019d, 271 },
  { /*  9 bits */ 0x0019e, 77 },
  { /*  9 bits */ 0x0019f, 110 },
  { /*  9 bits */ 0x001a0, 102 },
  { /*  9 bits */ 0x001a1, 157 },
  { /*  9 bits */ 0x001a2, 94 },
  { /*  9 bits */ 0x001a3, 143 },
  { /*  9 bits */ 0x001a4, 127 },
  { /*  9 bits */ 0x001a5, 26 },
  { /*  9 bits */ 0x001a6, 173 },
  { /*  9 bits */ 0x001a7, 6 },
  { /*  9 bits */ 0x001a8, 172 },
  { /*  9 bits */ 0x001a9, 154 },
  { /*  9 bits */ 0x001aa, 158 },
  { /*  9 bits */ 0x001ab, 78 },
  { /*  9 bits */ 0x001ac, 44 },
  { /*  9 bits */ 0x001ad, 159 },
  { /*  9 bits */ 0x001ae, 61 },
  { /*  9 bits */ 0x001af, 111 },
  { /*  9 bits */ 0x001b0, 174 },
  { /*  9 bits */ 0x001b1, 144 },
  { /*  9 bits */ 0x001b2, 175 },
  { /*  9 bits */ 0x001b3, 160 },
  { /*  9 bits */ 0x001b4, 190 },
  { /*  9 bits */ 0x001b5, 27 },
  { /*  9 bits */ 0x001b6, 119 },
  { /*  9 bits */ 0x001b7, 176 },
  { /*  9 bits */ 0x001b8, 128 },
  { /*  9 bits */ 0x001b9, 62 },
  { /*  9 bits */ 0x001ba, 95 },
  { /*  9 bits */ 0x001bb, 171 },
  { /*  9 bits */ 0x001bc, 79 },
  { /*  9 bits */ 0x001bd, 189 },
  { /*  9 bits */ 0x001be, 223 },
  { /*  9 bits */ 0x001bf, 112 },
  { /*  9 bits */ 0x001c0, 224 },
  { /*  9 bits */ 0x001c1, 45 },
  { /*  9 bits */ 0x001c2, 272 },
  { /*  9 bits */ 0x001c3, 96 },
  { /*  9 bits */ 0x001c4, 192 },
  { /* 10 bits */ 0x0038a, 191 },
  { /* 10 bits */ 0x0038b, 161 },
  { /* 10 bits */ 0x0038c, 129 },
  { /* 10 bits */ 0x0038d, 145 },
  { /* 10 bits */ 0x0038e, 16 },
  { /* 10 bits */ 0x0038f, 81 },
  { /* 10 bits */ 0x00390, 7 },
  { /* 10 bits */ 0x00391, 64 },
  { /* 10 bits */ 0x00392, 193 },
  { /* 10 bits */ 0x00393, 222 },
  { /* 10 bits */ 0x00394, 225 },
  { /* 10 bits */ 0x00395, 207 },
  { /* 10 bits */ 0x00396, 47 },
  { /* 10 bits */ 0x00397, 226 },
  { /* 10 bits */ 0x00398, 146 },
  { /* 10 bits */ 0x00399, 113 },
  { /* 10 bits */ 0x0039a, 178 },
  { /* 10 bits */ 0x0039b, 177 },
  { /* 10 bits */ 0x0039c, 240 },
  { /* 10 bits */ 0x0039d, 208 },
  { /* 10 bits */ 0x0039e, 28 },
  { /* 10 bits */ 0x0039f, 80 },
  { /* 10 bits */ 0x003a0, 188 },
  { /* 10 bits */ 0x003a1, 63 },
  { /* 10 bits */ 0x003a2, 30 },
  { /* 10 bits */ 0x003a3, 206 },
  { /* 10 bits */ 0x003a4, 130 },
  { /* 10 bits */ 0x003a5, 65 },
  { /* 10 bits */ 0x003a6, 97 },
  { /* 10 bits */ 0x003a7, 98 },
  { /* 10 bits */ 0x003a8, 242 },
  { /* 10 bits */ 0x003a9, 82 },
  { /* 10 bits */ 0x003aa, 194 },
  { /* 10 bits */ 0x003ab, 241 },
  { /* 10 bits */ 0x003ac, 209 },
  { /* 10 bits */ 0x003ad, 227 },
  { /* 10 bits */ 0x003ae, 210 },
  { /* 10 bits */ 0x003af, 136 },
  { /* 10 bits */ 0x003b0, 195 },
  { /* 10 bits */ 0x003b1, 46 },
  { /* 10 bits */ 0x003b2, 162 },
  { /* 10 bits */ 0x003b3, 243 },
  { /* 10 bits */ 0x003b4, 115 },
  { /* 10 bits */ 0x003b5, 180 },
  { /* 10 bits */ 0x003b6, 257 },
  { /* 10 bits */ 0x003b7, 147 },
  { /* 10 bits */ 0x003b8, 163 },
  { /* 10 bits */ 0x003b9, 244 },
  { /* 10 bits */ 0x003ba, 179 },
  { /* 10 bits */ 0x003bb, 99 },
  { /* 10 bits */ 0x003bc, 196 },
  { /* 10 bits */ 0x003bd, 239 },
  { /* 10 bits */ 0x003be, 48 },
  { /* 10 bits */ 0x003bf, 114 },
  { /* 10 bits */ 0x003c0, 29 },
  { /* 10 bits */ 0x003c1, 229 },
  { /* 10 bits */ 0x003c2, 8 },
  { /* 10 bits */ 0x003c3, 228 },
  { /* 10 bits */ 0x003c4, 131 },
  { /* 10 bits */ 0x003c5, 211 },
  { /* 10 bits */ 0x003c6, 132 },
  { /* 10 bits */ 0x003c7, 258 },
  { /* 10 bits */ 0x003c8, 205 },
  { /* 10 bits */ 0x003c9, 116 },
  { /* 10 bits */ 0x003ca, 49 },
  { /* 10 bits */ 0x003cb, 260 },
  { /* 10 bits */ 0x003cc, 259 },
  { /* 10 bits */ 0x003cd, 31 },
  { /* 10 bits */ 0x003ce, 164 },
  { /* 10 bits */ 0x003cf, 83 },
  { /* 10 bits */ 0x003d0, 245 },
  { /* 10 bits */ 0x003d1, 149 },
  { /* 10 bits */ 0x003d2, 230 },
  { /* 10 bits */ 0x003d3, 148 },
  { /* 10 bits */ 0x003d4, 100 },
  { /* 10 bits */ 0x003d5, 66 },
  { /* 10 bits */ 0x003d6, 181 },
  { /* 10 bits */ 0x003d7, 197 },
  { /* 10 bits */ 0x003d8, 212 },
  { /* 10 bits */ 0x003d9, 261 },
  { /* 10 bits */ 0x003da, 262 },
  { /* 10 bits */ 0x003db, 150 },
  { /* 10 bits */ 0x003dc, 256 },
  { /* 10 bits */ 0x003dd, 133 },
  { /* 10 bits */ 0x003de, 153 },
  { /* 10 bits */ 0x003df, 9 },
  { /* 10 bits */ 0x003e0, 166 },
  { /* 10 bits */ 0x003e1, 165 },
  { /* 10 bits */ 0x003e2, 213 },
  { /* 10 bits */ 0x003e3, 246 },
  { /* 10 bits */ 0x003e4, 183 },
  { /* 10 bits */ 0x003e5, 247 },
  { /* 10 bits */ 0x003e6, 214 },
  { /* 10 bits */ 0x003e7, 117 },
  { /* 10 bits */ 0x003e8, 134 },
  { /* 11 bits */ 0x007d2, 167 },
  { /* 11 bits */ 0x007d3, 263 },
  { /* 11 bits */ 0x007d4, 198 },
  { /* 11 bits */ 0x007d5, 201 },
  { /* 11 bits */ 0x007d6, 32 },
  { /* 11 bits */ 0x007d7, 182 },
  { /* 11 bits */ 0x007d8, 184 },
  { /* 11 bits */ 0x007d9, 232 },
  { /* 11 bits */ 0x007da, 231 },
  { /* 11 bits */ 0x007db, 200 },
  { /* 11 bits */ 0x007dc, 199 },
  { /* 11 bits */ 0x007dd, 151 },
  { /* 11 bits */ 0x007de, 249 },
  { /* 11 bits */ 0x007df, 233 },
  { /* 11 bits */ 0x007e0, 217 },
  { /* 11 bits */ 0x007e1, 264 },
  { /* 11 bits */ 0x007e2, 248 },
  { /* 11 bits */ 0x007e3, 170 },
  { /* 11 bits */ 0x007e4, 215 },
  { /* 11 bits */ 0x007e5, 168 },
  { /* 11 bits */ 0x007e6, 10 },
  { /* 11 bits */ 0x007e7, 216 },
  { /* 11 bits */ 0x007e8, 187 },
  { /* 11 bits */ 0x007e9, 218 },
  { /* 11 bits */ 0x007ea, 185 },
  { /* 11 bits */ 0x007eb, 234 },
  { /* 11 bits */ 0x007ec, 13 },
  { /* 11 bits */ 0x007ed, 250 },
  { /* 11 bits */ 0x007ee, 265 },
  { /* 11 bits */ 0x007ef, 266 },
  { /* 11 bits */ 0x007f0, 202 },
  { /* 11 bits */ 0x007f1, 251 },
  { /* 11 bits */ 0x007f2, 221 },
  { /* 11 bits */ 0x007f3, 11 },
  { /* 11 bits */ 0x007f4, 235 },
  { /* 11 bits */ 0x007f5, 267 },
  { /* 11 bits */ 0x007f6, 268 },
  { /* 11 bits */ 0x007f7, 219 },
  { /* 11 bits */ 0x007f8, 238 },
  { /* 11 bits */ 0x007f9, 252 },
  { /* 11 bits */ 0x007fa, 236 },
  { /* 11 bits */ 0x007fb, 204 },
  { /* 11 bits */ 0x007fc, 253 },
  { /* 12 bits */ 0x00ffa, 14 },
  { /* 12 bits */ 0x00ffb, 12 },
  { /* 12 bits */ 0x00ffc, 269 },
  { /* 12 bits */ 0x00ffd, 255 },
  { /* 12 bits */ 0x00ffe, 15 },
  { /* 12 bits */ 0x00fff, 270 }
};

#define maac_codebook_stride(x) (1)
#define maac_codebook_codeword(cb, i) (maac_codebook[maac_codebook_indexes[(cb)].start + (i)].code)
#define maac_codebook_index(cb, i) (maac_codebook[maac_codebook_indexes[(cb)].start + (i)].index)
#endif

#define maac_abs(a) ( (a) < 0 ? (-a) : (a) )


static const maac_u8 maac_codebook_params[12] = {
  0x00,
  0x61,
  0x61,
  0xe2,
  0xe2,
  0x24,
  0x24,
  0xa7,
  0xa7,
  0xac,
  0xac,
  0xb0
};

#define maac_codebook_unsigned(x) (maac_codebook_params[(x)] >> 7)
#define maac_codebook_dimension(x) (1 + ((maac_codebook_params[(x)] >> 5) & 0x03))
#define maac_codebook_lav(x) (maac_codebook_params[(x)] & 0x1f)

MAAC_PRIVATE
void
maac_huffman_init(maac_huffman* h) {
    maac_memset(h, 0, sizeof *h);
}

MAAC_PRIVATE
MAAC_RESULT
maac_huffman_decode(maac_huffman* h, maac_bitreader* br, maac_u8 cb) {
    MAAC_RESULT res;
    maac_u16 i;
    maac_u16 cw_start;
    maac_u16 cw_end;

    maac_u32 cw;

    /* TODO with compact codebooks we do multiplying by stride to get to indexes, I think
    this could be tweaked to where h->offset is the actual number of bytes and we do
    everything by h->offset += maac_codebook_stride(cb) */

    while(maac_codebook_bits_indexes[cb].start + h->bits < maac_codebook_bits_indexes[cb].end) {

        if(maac_codebook_bits[maac_codebook_bits_indexes[cb].start + h->bits]) {

            if( (res = maac_bitreader_fill(br, h->bits + 1)) != MAAC_OK) return res;

            cw = maac_bitreader_peek(br, h->bits + 1);
            cw_start = h->offset;
            cw_end = h->offset + ((maac_u16)maac_codebook_bits[maac_codebook_bits_indexes[cb].start + h->bits]);

            for(i=cw_start; i<cw_end; i++) {
                if(cw == maac_codebook_codeword(cb, i)) {
                    /* reset some fields for the next go-around */
                    maac_bitreader_discard(br, h->bits + 1);
                    h->bits = 0;
                    h->offset = 0;
                    h->codeword = cw;
                    h->index = maac_codebook_index(cb, i);
                    return MAAC_OK;
                }
            }
            h->offset = cw_end;
        }
        h->bits++;
    }

    return MAAC_HUFFMAN_DECODE_ERROR;
}

#define maac_huffman_esc(val) ( maac_abs(val) == MAAC_ESC_FLAG ? 1 : 0 )

static void maac_spectral_data_calc(maac_u8 cb, maac_u32 index, maac_s16* out) {
    maac_s32 sidx = 0;
    maac_s32 mod = 0;
    maac_s32 off = 0;
    maac_s32 lav = 0;
    maac_s32 _w = 0;
    maac_s32 _x = 0;
    maac_s32 _y = 0;
    maac_s32 _z = 0;

    sidx = (maac_s32)index;
    lav = maac_codebook_lav(cb);

    if(maac_codebook_unsigned(cb)) {
        mod = lav + 1;
    } else {
        mod = 2 * lav + 1;
        off = lav;
    }

    if(maac_codebook_dimension(cb) == 4) {
        _w = (sidx / (mod * mod * mod) ) - off;
        sidx -= (_w + off) * (mod * mod * mod);

        _x = (sidx / (mod * mod) ) - off;
        sidx -= (_x + off) * (mod * mod);
    }

    _y = (sidx / mod) - off;
    sidx -= (_y + off) * mod;

    _z = sidx - off;

    if(maac_codebook_dimension(cb) == 4) {
        out[0] = _w;
        out[1] = _x;
        out[2] = _y;
        out[3] = _z;
    } else {
        out[0] = _y;
        out[1] = _z;
    }
}

MAAC_PRIVATE
MAAC_RESULT
maac_huffman_decode_spectral(maac_huffman* h, maac_bitreader* br, maac_u8 cb, maac_s16 out[4]) {
    MAAC_RESULT res;
    maac_u8 signbits;
    maac_u8 i;
    maac_s16 esc;

    switch(h->state) {
        case MAAC_HUFFMAN_STATE_CODEWORD: {
            if( (res = maac_huffman_decode(h, br, cb)) != MAAC_OK) return res;
            maac_spectral_data_calc(cb, h->index, out);

            if(maac_codebook_unsigned(cb)) {
                h->state = MAAC_HUFFMAN_STATE_SIGN_BITS;
                goto maac_huffman_state_sign_bits;
            }

            if(cb == MAAC_ESC_HCB) goto maac_check_esc_y;
            break;
        }

        case MAAC_HUFFMAN_STATE_SIGN_BITS: {
            maac_huffman_state_sign_bits:
            signbits = 0;
            for(i = 0; i < maac_codebook_dimension(cb); i++) {
                if(out[i]) signbits++;
            }
            if(signbits) {
                if( (res = maac_bitreader_fill(br, signbits)) != MAAC_OK) return res;
                for(i = 0; i < maac_codebook_dimension(cb); i++) {
                    if(out[i]) {
                        if(maac_bitreader_read(br,1)) {
                            out[i] = -out[i];
                        }
                        if(--signbits == 0) break;
                    }
                }
            }

            if(cb == MAAC_ESC_HCB) goto maac_check_esc_y;
            break;
        }

        case MAAC_HUFFMAN_STATE_ESC_PREFIX: {
            maac_huffman_state_esc_prefix:
            if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
            if(maac_bitreader_read(br,1)) {
                h->bits++;
                goto maac_huffman_state_esc_prefix;
            }
            h->state = MAAC_HUFFMAN_STATE_ESC;
        }
        /* fall-through */
        case MAAC_HUFFMAN_STATE_ESC: {
            if( (res = maac_bitreader_fill(br, h->bits + 4)) != MAAC_OK) return res;
            esc = (maac_s16) ((1 << (h->bits + 4)) + maac_bitreader_read(br, h->bits + 4));
            h->bits = 0;
            out[h->esc] = out[h->esc] < 0 ? -esc : esc;
            if(h->esc == 0) goto maac_check_esc_z;
            break;
        }
    }

    goto maac_huffman_spectral_complete;

    maac_check_esc_y: 
    if(maac_huffman_esc(out[0])) {
        h->state = MAAC_HUFFMAN_STATE_ESC_PREFIX;
        h->esc = 0;
        goto maac_huffman_state_esc_prefix;
    }

    maac_check_esc_z: 
    if(maac_huffman_esc(out[1])) {
        h->state = MAAC_HUFFMAN_STATE_ESC_PREFIX;
        h->esc = 1;
        goto maac_huffman_state_esc_prefix;
    }

    maac_huffman_spectral_complete:
    h->state = MAAC_HUFFMAN_STATE_CODEWORD;
    h->bits = 0;
    h->esc = 0;
    return MAAC_OK;
}

#include <stdio.h>


/* you can define MAAC_INVQUANT_TABLES to use lookup tables
for inverse quantization. I'm not sure if I recommend this -
it adds about 32kB of const data to the build. */
#ifdef MAAC_INVQUANT_TABLES
static const maac_flt MAAC_INV_QUANT[8192] = {
    MAAC_FLT_C(0.0000000000000000),MAAC_FLT_C(1.0000000000000000),MAAC_FLT_C(2.5198420997897464),MAAC_FLT_C(4.3267487109222245),
    MAAC_FLT_C(6.3496042078727974),MAAC_FLT_C(8.5498797333834844),MAAC_FLT_C(10.902723556992836),MAAC_FLT_C(13.390518279406722),
    MAAC_FLT_C(15.999999999999998),MAAC_FLT_C(18.720754407467133),MAAC_FLT_C(21.544346900318832),MAAC_FLT_C(24.463780996262468),
    MAAC_FLT_C(27.473141821279960),MAAC_FLT_C(30.567350940369842),MAAC_FLT_C(33.741991698453212),MAAC_FLT_C(36.993181114957046),
    MAAC_FLT_C(40.317473596635935),MAAC_FLT_C(43.711787041189993),MAAC_FLT_C(47.173345095760126),MAAC_FLT_C(50.699631325716943),
    MAAC_FLT_C(54.288352331898118),MAAC_FLT_C(57.937407704003519),MAAC_FLT_C(61.644865274418500),MAAC_FLT_C(65.408940536585988),
    MAAC_FLT_C(69.227979374755591),MAAC_FLT_C(73.100443455321638),MAAC_FLT_C(77.024897778591608),MAAC_FLT_C(80.999999999999986),
    MAAC_FLT_C(85.024491212518527),MAAC_FLT_C(89.097187944889555),MAAC_FLT_C(93.216975178615741),MAAC_FLT_C(97.382800224133163),
    MAAC_FLT_C(101.59366732596474),MAAC_FLT_C(105.84863288986224),MAAC_FLT_C(110.14680124343441),MAAC_FLT_C(114.48732085660060),
    MAAC_FLT_C(118.86938096020653),MAAC_FLT_C(123.29220851090024),MAAC_FLT_C(127.75506545836058),MAAC_FLT_C(132.25724627755247),
    MAAC_FLT_C(136.79807573413572),MAAC_FLT_C(141.37690685569191),MAAC_FLT_C(145.99311908523086),MAAC_FLT_C(150.64611659662910),
    MAAC_FLT_C(155.33532675434674),MAAC_FLT_C(160.06019870205279),MAAC_FLT_C(164.82020206673349),MAAC_FLT_C(169.61482576651861),
    MAAC_FLT_C(174.44357691188534),MAAC_FLT_C(179.30597979112557),MAAC_FLT_C(184.20157493201927),MAAC_FLT_C(189.12991823257562),
    MAAC_FLT_C(194.09058015449685),MAAC_FLT_C(199.08314497371677),MAAC_FLT_C(204.10721008296940),MAAC_FLT_C(209.16238534187647),
    MAAC_FLT_C(214.24829247050752),MAAC_FLT_C(219.36456448277784),MAAC_FLT_C(224.51084515641213),MAAC_FLT_C(229.68678853652230),
    MAAC_FLT_C(234.89205847013176),MAAC_FLT_C(240.12632816923249),MAAC_FLT_C(245.38927980018505),MAAC_FLT_C(250.68060409747261),
    MAAC_FLT_C(255.99999999999991),MAAC_FLT_C(261.34717430828869),MAAC_FLT_C(266.72184136106449),MAAC_FLT_C(272.12372272986045),
    MAAC_FLT_C(277.55254693037961),MAAC_FLT_C(283.00804914946190),MAAC_FLT_C(288.48997098659891),MAAC_FLT_C(293.99806020902247),
    MAAC_FLT_C(299.53207051947408),MAAC_FLT_C(305.09176133582986),MAAC_FLT_C(310.67689758182206),MAAC_FLT_C(316.28724948815585),
    MAAC_FLT_C(321.92259240337177),MAAC_FLT_C(327.58270661385535),MAAC_FLT_C(333.26737717243742),MAAC_FLT_C(338.97639373507025),
    MAAC_FLT_C(344.70955040510131),MAAC_FLT_C(350.46664558470013),MAAC_FLT_C(356.24748183302609),MAAC_FLT_C(362.05186573075139),
    MAAC_FLT_C(367.87960775058258),MAAC_FLT_C(373.73052213344511),MAAC_FLT_C(379.60442677002078),MAAC_FLT_C(385.50114308734607),
    MAAC_FLT_C(391.42049594019943),MAAC_FLT_C(397.36231350702371),MAAC_FLT_C(403.32642719014467),MAAC_FLT_C(409.31267152006262),
    MAAC_FLT_C(415.32088406360799),MAAC_FLT_C(421.35090533576471),MAAC_FLT_C(427.40257871497619),MAAC_FLT_C(433.47575036176170),
    MAAC_FLT_C(439.57026914047930),MAAC_FLT_C(445.68598654408271),MAAC_FLT_C(451.82275662172759),MAAC_FLT_C(457.98043590909128),
    MAAC_FLT_C(464.15888336127773),MAAC_FLT_C(470.35796028818726),MAAC_FLT_C(476.57753029223630),MAAC_FLT_C(482.81745920832043),
    MAAC_FLT_C(489.07761504591741),MAAC_FLT_C(495.35786793323581),MAAC_FLT_C(501.65809006331688),MAAC_FLT_C(507.97815564200368),
    MAAC_FLT_C(514.31794083769648),MAAC_FLT_C(520.67732373281672),MAAC_FLT_C(527.05618427690604),MAAC_FLT_C(533.45440424129174),
    MAAC_FLT_C(539.87186717525128),MAAC_FLT_C(546.30845836361505),MAAC_FLT_C(552.76406478574609),MAAC_FLT_C(559.23857507584194),
    MAAC_FLT_C(565.73187948450413),MAAC_FLT_C(572.24386984152341),MAAC_FLT_C(578.77443951983378),MAAC_FLT_C(585.32348340058843),
    MAAC_FLT_C(591.89089783931263),MAAC_FLT_C(598.47658063309257),MAAC_FLT_C(605.08043098876044),MAAC_FLT_C(611.70234949203643),
    MAAC_FLT_C(618.34223807759190),MAAC_FLT_C(624.99999999999977),MAAC_FLT_C(631.67553980553748),MAAC_FLT_C(638.36876330481164),
    MAAC_FLT_C(645.07957754617485),MAAC_FLT_C(651.80789078990415),MAAC_FLT_C(658.55361248311499),MAAC_FLT_C(665.31665323538357),
    MAAC_FLT_C(672.09692479505225),MAAC_FLT_C(678.89434002619430),MAAC_FLT_C(685.70881288621422),MAAC_FLT_C(692.54025840406200),
    MAAC_FLT_C(699.38859265903977),MAAC_FLT_C(706.25373276018058),MAAC_FLT_C(713.13559682617972),MAAC_FLT_C(720.03410396586037),
    MAAC_FLT_C(726.94917425915435),MAAC_FLT_C(733.88072873858209),MAAC_FLT_C(740.82868937121543),MAAC_FLT_C(747.79297904110535),
    MAAC_FLT_C(754.77352153216191),MAAC_FLT_C(761.77024151147043),MAAC_FLT_C(768.78306451302956),MAAC_FLT_C(775.81191692189896),
    MAAC_FLT_C(782.85672595874246),MAAC_FLT_C(789.91741966475445),MAAC_FLT_C(796.99392688695798),MAAC_FLT_C(804.08617726386274),
    MAAC_FLT_C(811.19410121147098),MAAC_FLT_C(818.31762990962227),MAAC_FLT_C(825.45669528866563),MAAC_FLT_C(832.61123001644864),
    MAAC_FLT_C(839.78116748561604),MAAC_FLT_C(846.96644180120552),MAAC_FLT_C(854.16698776853514),MAAC_FLT_C(861.38274088137143),
    MAAC_FLT_C(868.61363731036977),MAAC_FLT_C(875.85961389178203),MAAC_FLT_C(883.12060811641959),MAAC_FLT_C(890.39655811886757),
    MAAC_FLT_C(897.68740266694181),MAAC_FLT_C(904.99308115138172),MAAC_FLT_C(912.31353357577188),MAAC_FLT_C(919.64870054668756),
    MAAC_FLT_C(926.99852326405619),MAAC_FLT_C(934.36294351172899),MAAC_FLT_C(941.74190364825859),MAAC_FLT_C(949.13534659787422),
    MAAC_FLT_C(956.54321584165211),MAAC_FLT_C(963.96545540887348),MAAC_FLT_C(971.40200986856541),MAAC_FLT_C(978.85282432122176),
    MAAC_FLT_C(986.31784439069588),MAAC_FLT_C(993.79701621626350),MAAC_FLT_C(1001.2902864448500),MAAC_FLT_C(1008.7976022234180),
    MAAC_FLT_C(1016.3189111915103),MAAC_FLT_C(1023.8541614739464),MAAC_FLT_C(1031.4033016736653),MAAC_FLT_C(1038.9662808647138),
    MAAC_FLT_C(1046.5430485853758),MAAC_FLT_C(1054.1335548314366),MAAC_FLT_C(1061.7377500495838),MAAC_FLT_C(1069.3555851309357),
    MAAC_FLT_C(1076.9870114046978),MAAC_FLT_C(1084.6319806319441),MAAC_FLT_C(1092.2904449995174),MAAC_FLT_C(1099.9623571140482),
    MAAC_FLT_C(1107.6476699960892),MAAC_FLT_C(1115.3463370743607),MAAC_FLT_C(1123.0583121801060),MAAC_FLT_C(1130.7835495415541),
    MAAC_FLT_C(1138.5220037784857),MAAC_FLT_C(1146.2736298969010),MAAC_FLT_C(1154.0383832837879),MAAC_FLT_C(1161.8162197019860),
    MAAC_FLT_C(1169.6070952851460),MAAC_FLT_C(1177.4109665327808),MAAC_FLT_C(1185.2277903054078),MAAC_FLT_C(1193.0575238197798),
    MAAC_FLT_C(1200.9001246442001),MAAC_FLT_C(1208.7555506939248),MAAC_FLT_C(1216.6237602266442),MAAC_FLT_C(1224.5047118380478),
    MAAC_FLT_C(1232.3983644574657),MAAC_FLT_C(1240.3046773435874),MAAC_FLT_C(1248.2236100802568),MAAC_FLT_C(1256.1551225723395),
    MAAC_FLT_C(1264.0991750416620),MAAC_FLT_C(1272.0557280230228),MAAC_FLT_C(1280.0247423602691),MAAC_FLT_C(1288.0061792024444),
    MAAC_FLT_C(1295.9999999999995),MAAC_FLT_C(1304.0061665010680),MAAC_FLT_C(1312.0246407478062),MAAC_FLT_C(1320.0553850727929),
    MAAC_FLT_C(1328.0983620954903),MAAC_FLT_C(1336.1535347187651),MAAC_FLT_C(1344.2208661254647),MAAC_FLT_C(1352.3003197750522),
    MAAC_FLT_C(1360.3918594002962),MAAC_FLT_C(1368.4954490040145),MAAC_FLT_C(1376.6110528558709),MAAC_FLT_C(1384.7386354892244),
    MAAC_FLT_C(1392.8781616980295),MAAC_FLT_C(1401.0295965337855),MAAC_FLT_C(1409.1929053025353),MAAC_FLT_C(1417.3680535619119),
    MAAC_FLT_C(1425.5550071182327),MAAC_FLT_C(1433.7537320236374),MAAC_FLT_C(1441.9641945732744),MAAC_FLT_C(1450.1863613025282),
    MAAC_FLT_C(1458.4201989842913),MAAC_FLT_C(1466.6656746262797),MAAC_FLT_C(1474.9227554683875),MAAC_FLT_C(1483.1914089800841),
    MAAC_FLT_C(1491.4716028578516),MAAC_FLT_C(1499.7633050226596),MAAC_FLT_C(1508.0664836174794),MAAC_FLT_C(1516.3811070048375),
    MAAC_FLT_C(1524.7071437644029),MAAC_FLT_C(1533.0445626906128),MAAC_FLT_C(1541.3933327903342),MAAC_FLT_C(1549.7534232805581),
    MAAC_FLT_C(1558.1248035861304),MAAC_FLT_C(1566.5074433375150),MAAC_FLT_C(1574.9013123685909),MAAC_FLT_C(1583.3063807144795),
    MAAC_FLT_C(1591.7226186094069),MAAC_FLT_C(1600.1499964845941),MAAC_FLT_C(1608.5884849661800),MAAC_FLT_C(1617.0380548731737),
    MAAC_FLT_C(1625.4986772154357),MAAC_FLT_C(1633.9703231916887),MAAC_FLT_C(1642.4529641875577),MAAC_FLT_C(1650.9465717736346),
    MAAC_FLT_C(1659.4511177035752),MAAC_FLT_C(1667.9665739122186),MAAC_FLT_C(1676.4929125137353),MAAC_FLT_C(1685.0301057998010),
    MAAC_FLT_C(1693.5781262377957),MAAC_FLT_C(1702.1369464690270),MAAC_FLT_C(1710.7065393069795),MAAC_FLT_C(1719.2868777355877),
    MAAC_FLT_C(1727.8779349075323),MAAC_FLT_C(1736.4796841425596),MAAC_FLT_C(1745.0920989258250),MAAC_FLT_C(1753.7151529062583),
    MAAC_FLT_C(1762.3488198949503),MAAC_FLT_C(1770.9930738635630),MAAC_FLT_C(1779.6478889427597),MAAC_FLT_C(1788.3132394206564),
    MAAC_FLT_C(1796.9890997412947),MAAC_FLT_C(1805.6754445031333),MAAC_FLT_C(1814.3722484575621),MAAC_FLT_C(1823.0794865074322),
    MAAC_FLT_C(1831.7971337056094),MAAC_FLT_C(1840.5251652535437),MAAC_FLT_C(1849.2635564998579),MAAC_FLT_C(1858.0122829389563),
    MAAC_FLT_C(1866.7713202096493),MAAC_FLT_C(1875.5406440937966),MAAC_FLT_C(1884.3202305149687),MAAC_FLT_C(1893.1100555371240),
    MAAC_FLT_C(1901.9100953633042),MAAC_FLT_C(1910.7203263343454),MAAC_FLT_C(1919.5407249276057),MAAC_FLT_C(1928.3712677557098),
    MAAC_FLT_C(1937.2119315653083),MAAC_FLT_C(1946.0626932358525),MAAC_FLT_C(1954.9235297783860),MAAC_FLT_C(1963.7944183343500),
    MAAC_FLT_C(1972.6753361744036),MAAC_FLT_C(1981.5662606972594),MAAC_FLT_C(1990.4671694285330),MAAC_FLT_C(1999.3780400196069),
    MAAC_FLT_C(2008.2988502465078),MAAC_FLT_C(2017.2295780087982),MAAC_FLT_C(2026.1702013284819),MAAC_FLT_C(2035.1206983489212),
    MAAC_FLT_C(2044.0810473337688),MAAC_FLT_C(2053.0512266659125),MAAC_FLT_C(2062.0312148464309),MAAC_FLT_C(2071.0209904935646),
    MAAC_FLT_C(2080.0205323416958),MAAC_FLT_C(2089.0298192403443),MAAC_FLT_C(2098.0488301531714),MAAC_FLT_C(2107.0775441569995),
    MAAC_FLT_C(2116.1159404408390),MAAC_FLT_C(2125.1639983049317),MAAC_FLT_C(2134.2216971597995),MAAC_FLT_C(2143.2890165253098),
    MAAC_FLT_C(2152.3659360297484),MAAC_FLT_C(2161.4524354089031),MAAC_FLT_C(2170.5484945051617),MAAC_FLT_C(2179.6540932666144),
    MAAC_FLT_C(2188.7692117461711),MAAC_FLT_C(2197.8938301006888),MAAC_FLT_C(2207.0279285901042),MAAC_FLT_C(2216.1714875765838),
    MAAC_FLT_C(2225.3244875236760),MAAC_FLT_C(2234.4869089954782),MAAC_FLT_C(2243.6587326558101),MAAC_FLT_C(2252.8399392673982),
    MAAC_FLT_C(2262.0305096910702),MAAC_FLT_C(2271.2304248849537),MAAC_FLT_C(2280.4396659036897),MAAC_FLT_C(2289.6582138976523),
    MAAC_FLT_C(2298.8860501121762),MAAC_FLT_C(2308.1231558867926),MAAC_FLT_C(2317.3695126544767),MAAC_FLT_C(2326.6251019409005),
    MAAC_FLT_C(2335.8899053636933),MAAC_FLT_C(2345.1639046317132),MAAC_FLT_C(2354.4470815443233),MAAC_FLT_C(2363.7394179906792),
    MAAC_FLT_C(2373.0408959490205),MAAC_FLT_C(2382.3514974859731),MAAC_FLT_C(2391.6712047558558),MAAC_FLT_C(2400.9999999999991),
    MAAC_FLT_C(2410.3378655460651),MAAC_FLT_C(2419.6847838073813),MAAC_FLT_C(2429.0407372822747),MAAC_FLT_C(2438.4057085534191),
    MAAC_FLT_C(2447.7796802871858),MAAC_FLT_C(2457.1626352330009),MAAC_FLT_C(2466.5545562227112),MAAC_FLT_C(2475.9554261699564),
    MAAC_FLT_C(2485.3652280695474),MAAC_FLT_C(2494.7839449968487),MAAC_FLT_C(2504.2115601071737),MAAC_FLT_C(2513.6480566351788),
    MAAC_FLT_C(2523.0934178942675),MAAC_FLT_C(2532.5476272760025),MAAC_FLT_C(2542.0106682495189),MAAC_FLT_C(2551.4825243609480),
    MAAC_FLT_C(2560.9631792328441),MAAC_FLT_C(2570.4526165636184),MAAC_FLT_C(2579.9508201269791),MAAC_FLT_C(2589.4577737713744),
    MAAC_FLT_C(2598.9734614194458),MAAC_FLT_C(2608.4978670674823),MAAC_FLT_C(2618.0309747848837),MAAC_FLT_C(2627.5727687136259),
    MAAC_FLT_C(2637.1232330677353),MAAC_FLT_C(2646.6823521327647),MAAC_FLT_C(2656.2501102652768),MAAC_FLT_C(2665.8264918923328),
    MAAC_FLT_C(2675.4114815109842),MAAC_FLT_C(2685.0050636877722),MAAC_FLT_C(2694.6072230582295),MAAC_FLT_C(2704.2179443263894),
    MAAC_FLT_C(2713.8372122642972),MAAC_FLT_C(2723.4650117115279),MAAC_FLT_C(2733.1013275747096),MAAC_FLT_C(2742.7461448270483),
    MAAC_FLT_C(2752.3994485078601),MAAC_FLT_C(2762.0612237221085),MAAC_FLT_C(2771.7314556399419),MAAC_FLT_C(2781.4101294962406),
    MAAC_FLT_C(2791.0972305901655),MAAC_FLT_C(2800.7927442847094),MAAC_FLT_C(2810.4966560062589),MAAC_FLT_C(2820.2089512441521),
    MAAC_FLT_C(2829.9296155502466),MAAC_FLT_C(2839.6586345384894),MAAC_FLT_C(2849.3959938844919),MAAC_FLT_C(2859.1416793251065),
    MAAC_FLT_C(2868.8956766580086),MAAC_FLT_C(2878.6579717412847),MAAC_FLT_C(2888.4285504930212),MAAC_FLT_C(2898.2073988908974),
    MAAC_FLT_C(2907.9945029717837),MAAC_FLT_C(2917.7898488313440),MAAC_FLT_C(2927.5934226236377),MAAC_FLT_C(2937.4052105607311),
    MAAC_FLT_C(2947.2251989123079),MAAC_FLT_C(2957.0533740052865),MAAC_FLT_C(2966.8897222234368),MAAC_FLT_C(2976.7342300070050),
    MAAC_FLT_C(2986.5868838523397),MAAC_FLT_C(2996.4476703115197),MAAC_FLT_C(3006.3165759919889),MAAC_FLT_C(3016.1935875561908),
    MAAC_FLT_C(3026.0786917212095),MAAC_FLT_C(3035.9718752584108),MAAC_FLT_C(3045.8731249930902),MAAC_FLT_C(3055.7824278041207),
    MAAC_FLT_C(3065.6997706236039),MAAC_FLT_C(3075.6251404365280),MAAC_FLT_C(3085.5585242804245),MAAC_FLT_C(3095.4999092450298),
    MAAC_FLT_C(3105.4492824719491),MAAC_FLT_C(3115.4066311543256),MAAC_FLT_C(3125.3719425365089),MAAC_FLT_C(3135.3452039137287),
    MAAC_FLT_C(3145.3264026317715),MAAC_FLT_C(3155.3155260866592),MAAC_FLT_C(3165.3125617243300),MAAC_FLT_C(3175.3174970403229),
    MAAC_FLT_C(3185.3303195794674),MAAC_FLT_C(3195.3510169355700),MAAC_FLT_C(3205.3795767511078),MAAC_FLT_C(3215.4159867169251),
    MAAC_FLT_C(3225.4602345719290),MAAC_FLT_C(3235.5123081027928),MAAC_FLT_C(3245.5721951436558),MAAC_FLT_C(3255.6398835758300),
    MAAC_FLT_C(3265.7153613275100),MAAC_FLT_C(3275.7986163734795),MAAC_FLT_C(3285.8896367348289),MAAC_FLT_C(3295.9884104786665),
    MAAC_FLT_C(3306.0949257178395),MAAC_FLT_C(3316.2091706106517),MAAC_FLT_C(3326.3311333605880),MAAC_FLT_C(3336.4608022160382),
    MAAC_FLT_C(3346.5981654700231),MAAC_FLT_C(3356.7432114599264),MAAC_FLT_C(3366.8959285672249),MAAC_FLT_C(3377.0563052172211),
    MAAC_FLT_C(3387.2243298787821),MAAC_FLT_C(3397.3999910640764),MAAC_FLT_C(3407.5832773283128),MAAC_FLT_C(3417.7741772694862),
    MAAC_FLT_C(3427.9726795281199),MAAC_FLT_C(3438.1787727870123),MAAC_FLT_C(3448.3924457709873),MAAC_FLT_C(3458.6136872466445),
    MAAC_FLT_C(3468.8424860221107),MAAC_FLT_C(3479.0788309467976),MAAC_FLT_C(3489.3227109111554),MAAC_FLT_C(3499.5741148464344),
    MAAC_FLT_C(3509.8330317244445),MAAC_FLT_C(3520.0994505573185),MAAC_FLT_C(3530.3733603972751),MAAC_FLT_C(3540.6547503363886),
    MAAC_FLT_C(3550.9436095063534),MAAC_FLT_C(3561.2399270782580),MAAC_FLT_C(3571.5436922623535),MAAC_FLT_C(3581.8548943078308),
    MAAC_FLT_C(3592.1735225025936),MAAC_FLT_C(3602.4995661730372),MAAC_FLT_C(3612.8330146838275),MAAC_FLT_C(3623.1738574376814),
    MAAC_FLT_C(3633.5220838751502),MAAC_FLT_C(3643.8776834744031),MAAC_FLT_C(3654.2406457510142),MAAC_FLT_C(3664.6109602577494),
    MAAC_FLT_C(3674.9886165843564),MAAC_FLT_C(3685.3736043573545),MAAC_FLT_C(3695.7659132398294),MAAC_FLT_C(3706.1655329312248),
    MAAC_FLT_C(3716.5724531671399),MAAC_FLT_C(3726.9866637191262),MAAC_FLT_C(3737.4081543944876),MAAC_FLT_C(3747.8369150360782),
    MAAC_FLT_C(3758.2729355221072),MAAC_FLT_C(3768.7162057659411),MAAC_FLT_C(3779.1667157159077),MAAC_FLT_C(3789.6244553551055),
    MAAC_FLT_C(3800.0894147012082),MAAC_FLT_C(3810.5615838062768),MAAC_FLT_C(3821.0409527565694),MAAC_FLT_C(3831.5275116723533),
    MAAC_FLT_C(3842.0212507077194),MAAC_FLT_C(3852.5221600503960),MAAC_FLT_C(3863.0302299215673),MAAC_FLT_C(3873.5454505756893),
    MAAC_FLT_C(3884.0678123003108),MAAC_FLT_C(3894.5973054158922),MAAC_FLT_C(3905.1339202756285),MAAC_FLT_C(3915.6776472652732),
    MAAC_FLT_C(3926.2284768029604),MAAC_FLT_C(3936.7863993390338),MAAC_FLT_C(3947.3514053558706),MAAC_FLT_C(3957.9234853677135),
    MAAC_FLT_C(3968.5026299204969),MAAC_FLT_C(3979.0888295916798),MAAC_FLT_C(3989.6820749900776),MAAC_FLT_C(4000.2823567556948),
    MAAC_FLT_C(4010.8896655595613),MAAC_FLT_C(4021.5039921035655),MAAC_FLT_C(4032.1253271202945),MAAC_FLT_C(4042.7536613728694),
    MAAC_FLT_C(4053.3889856547858),MAAC_FLT_C(4064.0312907897551),MAAC_FLT_C(4074.6805676315448),MAAC_FLT_C(4085.3368070638221),
    MAAC_FLT_C(4095.9999999999982),MAAC_FLT_C(4106.6701373830711),MAAC_FLT_C(4117.3472101854750),MAAC_FLT_C(4128.0312094089259),
    MAAC_FLT_C(4138.7221260842680),MAAC_FLT_C(4149.4199512713267),MAAC_FLT_C(4160.1246760587583),MAAC_FLT_C(4170.8362915638982),
    MAAC_FLT_C(4181.5547889326181),MAAC_FLT_C(4192.2801593391769),MAAC_FLT_C(4203.0123939860741),MAAC_FLT_C(4213.7514841039101),
    MAAC_FLT_C(4224.4974209512384),MAAC_FLT_C(4235.2501958144258),MAAC_FLT_C(4246.0098000075095),MAAC_FLT_C(4256.7762248720574),
    MAAC_FLT_C(4267.5494617770310),MAAC_FLT_C(4278.3295021186423),MAAC_FLT_C(4289.1163373202198),MAAC_FLT_C(4299.9099588320714),
    MAAC_FLT_C(4310.7103581313495),MAAC_FLT_C(4321.5175267219138),MAAC_FLT_C(4332.3314561342004),MAAC_FLT_C(4343.1521379250880),
    MAAC_FLT_C(4353.9795636777671),MAAC_FLT_C(4364.8137250016052),MAAC_FLT_C(4375.6546135320232),MAAC_FLT_C(4386.5022209303588),
    MAAC_FLT_C(4397.3565388837469),MAAC_FLT_C(4408.2175591049827),MAAC_FLT_C(4419.0852733324018),MAAC_FLT_C(4429.9596733297531),
    MAAC_FLT_C(4440.8407508860728),MAAC_FLT_C(4451.7284978155603),MAAC_FLT_C(4462.6229059574571),MAAC_FLT_C(4473.5239671759227),
    MAAC_FLT_C(4484.4316733599126),MAAC_FLT_C(4495.3460164230582),MAAC_FLT_C(4506.2669883035496),MAAC_FLT_C(4517.1945809640119),
    MAAC_FLT_C(4528.1287863913894),MAAC_FLT_C(4539.0695965968280),MAAC_FLT_C(4550.0170036155587),MAAC_FLT_C(4560.9709995067806),
    MAAC_FLT_C(4571.9315763535460),MAAC_FLT_C(4582.8987262626470),MAAC_FLT_C(4593.8724413645004),MAAC_FLT_C(4604.8527138130348),
    MAAC_FLT_C(4615.8395357855816),MAAC_FLT_C(4626.8328994827571),MAAC_FLT_C(4637.8327971283588),MAAC_FLT_C(4648.8392209692511),
    MAAC_FLT_C(4659.8521632752563),MAAC_FLT_C(4670.8716163390473),MAAC_FLT_C(4681.8975724760394),MAAC_FLT_C(4692.9300240242837),
    MAAC_FLT_C(4703.9689633443595),MAAC_FLT_C(4715.0143828192668),MAAC_FLT_C(4726.0662748543255),MAAC_FLT_C(4737.1246318770682),
    MAAC_FLT_C(4748.1894463371373),MAAC_FLT_C(4759.2607107061804),MAAC_FLT_C(4770.3384174777493),MAAC_FLT_C(4781.4225591671993),
    MAAC_FLT_C(4792.5131283115852),MAAC_FLT_C(4803.6101174695614),MAAC_FLT_C(4814.7135192212854),MAAC_FLT_C(4825.8233261683154),
    MAAC_FLT_C(4836.9395309335096),MAAC_FLT_C(4848.0621261609349),MAAC_FLT_C(4859.1911045157631),MAAC_FLT_C(4870.3264586841779),
    MAAC_FLT_C(4881.4681813732768),MAAC_FLT_C(4892.6162653109768),MAAC_FLT_C(4903.7707032459193),MAAC_FLT_C(4914.9314879473750),
    MAAC_FLT_C(4926.0986122051509),MAAC_FLT_C(4937.2720688294967),MAAC_FLT_C(4948.4518506510112),MAAC_FLT_C(4959.6379505205550),
    MAAC_FLT_C(4970.8303613091521),MAAC_FLT_C(4982.0290759079044),MAAC_FLT_C(4993.2340872278974),MAAC_FLT_C(5004.4453882001153),
    MAAC_FLT_C(5015.6629717753467),MAAC_FLT_C(5026.8868309241007),MAAC_FLT_C(5038.1169586365131),MAAC_FLT_C(5049.3533479222660),
    MAAC_FLT_C(5060.5959918104927),MAAC_FLT_C(5071.8448833496996),MAAC_FLT_C(5083.1000156076734),MAAC_FLT_C(5094.3613816713996),
    MAAC_FLT_C(5105.6289746469747),MAAC_FLT_C(5116.9027876595246),MAAC_FLT_C(5128.1828138531200),MAAC_FLT_C(5139.4690463906918),
    MAAC_FLT_C(5150.7614784539473),MAAC_FLT_C(5162.0601032432933),MAAC_FLT_C(5173.3649139777472),MAAC_FLT_C(5184.6759038948594),
    MAAC_FLT_C(5195.9930662506322),MAAC_FLT_C(5207.3163943194386),MAAC_FLT_C(5218.6458813939435),MAAC_FLT_C(5229.9815207850233),
    MAAC_FLT_C(5241.3233058216847),MAAC_FLT_C(5252.6712298509919),MAAC_FLT_C(5264.0252862379830),MAAC_FLT_C(5275.3854683655954),
    MAAC_FLT_C(5286.7517696345876),MAAC_FLT_C(5298.1241834634639),MAAC_FLT_C(5309.5027032883954),MAAC_FLT_C(5320.8873225631460),
    MAAC_FLT_C(5332.2780347589978),MAAC_FLT_C(5343.6748333646756),MAAC_FLT_C(5355.0777118862716),MAAC_FLT_C(5366.4866638471722),
    MAAC_FLT_C(5377.9016827879850),MAAC_FLT_C(5389.3227622664635),MAAC_FLT_C(5400.7498958574370),MAAC_FLT_C(5412.1830771527357),
    MAAC_FLT_C(5423.6222997611230),MAAC_FLT_C(5435.0675573082190),MAAC_FLT_C(5446.5188434364318),MAAC_FLT_C(5457.9761518048872),
    MAAC_FLT_C(5469.4394760893592),MAAC_FLT_C(5480.9088099821975),MAAC_FLT_C(5492.3841471922606),MAAC_FLT_C(5503.8654814448455),
    MAAC_FLT_C(5515.3528064816201),MAAC_FLT_C(5526.8461160605520),MAAC_FLT_C(5538.3454039558465),MAAC_FLT_C(5549.8506639578736),
    MAAC_FLT_C(5561.3618898731029),MAAC_FLT_C(5572.8790755240361),MAAC_FLT_C(5584.4022147491451),MAAC_FLT_C(5595.9313014027975),
    MAAC_FLT_C(5607.4663293552012),MAAC_FLT_C(5619.0072924923297),MAAC_FLT_C(5630.5541847158656),MAAC_FLT_C(5642.1069999431284),
    MAAC_FLT_C(5653.6657321070170),MAAC_FLT_C(5665.2303751559430),MAAC_FLT_C(5676.8009230537655),MAAC_FLT_C(5688.3773697797333),
    MAAC_FLT_C(5699.9597093284165),MAAC_FLT_C(5711.5479357096474),MAAC_FLT_C(5723.1420429484588),MAAC_FLT_C(5734.7420250850209),
    MAAC_FLT_C(5746.3478761745810),MAAC_FLT_C(5757.9595902874016),MAAC_FLT_C(5769.5771615087006),MAAC_FLT_C(5781.2005839385911),
    MAAC_FLT_C(5792.8298516920213),MAAC_FLT_C(5804.4649588987149),MAAC_FLT_C(5816.1058997031114),MAAC_FLT_C(5827.7526682643065),
    MAAC_FLT_C(5839.4052587559972),MAAC_FLT_C(5851.0636653664196),MAAC_FLT_C(5862.7278822982908),MAAC_FLT_C(5874.3979037687541),
    MAAC_FLT_C(5886.0737240093204),MAAC_FLT_C(5897.7553372658094),MAAC_FLT_C(5909.4427377982956),MAAC_FLT_C(5921.1359198810505),
    MAAC_FLT_C(5932.8348778024874),MAAC_FLT_C(5944.5396058651031),MAAC_FLT_C(5956.2500983854261),MAAC_FLT_C(5967.9663496939575),
    MAAC_FLT_C(5979.6883541351208),MAAC_FLT_C(5991.4161060672022),MAAC_FLT_C(6003.1495998623004),MAAC_FLT_C(6014.8888299062701),
    MAAC_FLT_C(6026.6337905986675),MAAC_FLT_C(6038.3844763527031),MAAC_FLT_C(6050.1408815951781),MAAC_FLT_C(6061.9030007664414),
    MAAC_FLT_C(6073.6708283203316),MAAC_FLT_C(6085.4443587241267),MAAC_FLT_C(6097.2235864584891),MAAC_FLT_C(6109.0085060174197),
    MAAC_FLT_C(6120.7991119081998),MAAC_FLT_C(6132.5953986513450),MAAC_FLT_C(6144.3973607805519),MAAC_FLT_C(6156.2049928426459),
    MAAC_FLT_C(6168.0182893975361),MAAC_FLT_C(6179.8372450181578),MAAC_FLT_C(6191.6618542904307),MAAC_FLT_C(6203.4921118132024),
    MAAC_FLT_C(6215.3280121982016),MAAC_FLT_C(6227.1695500699925),MAAC_FLT_C(6239.0167200659189),MAAC_FLT_C(6250.8695168360628),
    MAAC_FLT_C(6262.7279350431891),MAAC_FLT_C(6274.5919693627056),MAAC_FLT_C(6286.4616144826068),MAAC_FLT_C(6298.3368651034325),
    MAAC_FLT_C(6310.2177159382172),MAAC_FLT_C(6322.1041617124456),MAAC_FLT_C(6333.9961971640032),MAAC_FLT_C(6345.8938170431311),
    MAAC_FLT_C(6357.7970161123785),MAAC_FLT_C(6369.7057891465583),MAAC_FLT_C(6381.6201309327016),MAAC_FLT_C(6393.5400362700075),
    MAAC_FLT_C(6405.4654999698032),MAAC_FLT_C(6417.3965168554978),MAAC_FLT_C(6429.3330817625329),MAAC_FLT_C(6441.2751895383453),
    MAAC_FLT_C(6453.2228350423138),MAAC_FLT_C(6465.1760131457240),MAAC_FLT_C(6477.1347187317160),MAAC_FLT_C(6489.0989466952469),
    MAAC_FLT_C(6501.0686919430445),MAAC_FLT_C(6513.0439493935628),MAAC_FLT_C(6525.0247139769417),MAAC_FLT_C(6537.0109806349610),
    MAAC_FLT_C(6549.0027443210010),MAAC_FLT_C(6560.9999999999964),MAAC_FLT_C(6573.0027426483985),MAAC_FLT_C(6585.0109672541284),
    MAAC_FLT_C(6597.0246688165371),MAAC_FLT_C(6609.0438423463656),MAAC_FLT_C(6621.0684828657004),MAAC_FLT_C(6633.0985854079354),
    MAAC_FLT_C(6645.1341450177270),MAAC_FLT_C(6657.1751567509573),MAAC_FLT_C(6669.2216156746899),MAAC_FLT_C(6681.2735168671343),
    MAAC_FLT_C(6693.3308554176001),MAAC_FLT_C(6705.3936264264594),MAAC_FLT_C(6717.4618250051080),MAAC_FLT_C(6729.5354462759260),
    MAAC_FLT_C(6741.6144853722335),MAAC_FLT_C(6753.6989374382601),MAAC_FLT_C(6765.7887976290967),MAAC_FLT_C(6777.8840611106634),
    MAAC_FLT_C(6789.9847230596661),MAAC_FLT_C(6802.0907786635626),MAAC_FLT_C(6814.2022231205201),MAAC_FLT_C(6826.3190516393797),
    MAAC_FLT_C(6838.4412594396181),MAAC_FLT_C(6850.5688417513074),MAAC_FLT_C(6862.7017938150830),MAAC_FLT_C(6874.8401108820990),
    MAAC_FLT_C(6886.9837882139991),MAAC_FLT_C(6899.1328210828724),MAAC_FLT_C(6911.2872047712208),MAAC_FLT_C(6923.4469345719199),
    MAAC_FLT_C(6935.6120057881863),MAAC_FLT_C(6947.7824137335365),MAAC_FLT_C(6959.9581537317536),MAAC_FLT_C(6972.1392211168532),
    MAAC_FLT_C(6984.3256112330409),MAAC_FLT_C(6996.5173194346862),MAAC_FLT_C(7008.7143410862773),MAAC_FLT_C(7020.9166715623942),
    MAAC_FLT_C(7033.1243062476678),MAAC_FLT_C(7045.3372405367481),MAAC_FLT_C(7057.5554698342685),MAAC_FLT_C(7069.7789895548103),
    MAAC_FLT_C(7082.0077951228714),MAAC_FLT_C(7094.2418819728273),MAAC_FLT_C(7106.4812455489018),MAAC_FLT_C(7118.7258813051285),
    MAAC_FLT_C(7130.9757847053224),MAAC_FLT_C(7143.2309512230395),MAAC_FLT_C(7155.4913763415516),MAAC_FLT_C(7167.7570555538032),
    MAAC_FLT_C(7180.0279843623894),MAAC_FLT_C(7192.3041582795131),MAAC_FLT_C(7204.5855728269571),MAAC_FLT_C(7216.8722235360519),
    MAAC_FLT_C(7229.1641059476406),MAAC_FLT_C(7241.4612156120493),MAAC_FLT_C(7253.7635480890503),MAAC_FLT_C(7266.0710989478375),
    MAAC_FLT_C(7278.3838637669869),MAAC_FLT_C(7290.7018381344296),MAAC_FLT_C(7303.0250176474174),MAAC_FLT_C(7315.3533979124932),
    MAAC_FLT_C(7327.6869745454596),MAAC_FLT_C(7340.0257431713462),MAAC_FLT_C(7352.3696994243801),MAAC_FLT_C(7364.7188389479543),
    MAAC_FLT_C(7377.0731573945968),MAAC_FLT_C(7389.4326504259416),MAAC_FLT_C(7401.7973137126937),MAAC_FLT_C(7414.1671429346061),
    MAAC_FLT_C(7426.5421337804428),MAAC_FLT_C(7438.9222819479510),MAAC_FLT_C(7451.3075831438346),MAAC_FLT_C(7463.6980330837177),
    MAAC_FLT_C(7476.0936274921214),MAAC_FLT_C(7488.4943621024304),MAAC_FLT_C(7500.9002326568652),MAAC_FLT_C(7513.3112349064522),
    MAAC_FLT_C(7525.7273646109943),MAAC_FLT_C(7538.1486175390446),MAAC_FLT_C(7550.5749894678729),MAAC_FLT_C(7563.0064761834419),
    MAAC_FLT_C(7575.4430734803736),MAAC_FLT_C(7587.8847771619248),MAAC_FLT_C(7600.3315830399597),MAAC_FLT_C(7612.7834869349153),
    MAAC_FLT_C(7625.2404846757800),MAAC_FLT_C(7637.7025721000637),MAAC_FLT_C(7650.1697450537677),MAAC_FLT_C(7662.6419993913596),
    MAAC_FLT_C(7675.1193309757446),MAAC_FLT_C(7687.6017356782404),MAAC_FLT_C(7700.0892093785442),MAAC_FLT_C(7712.5817479647112),
    MAAC_FLT_C(7725.0793473331250),MAAC_FLT_C(7737.5820033884729),MAAC_FLT_C(7750.0897120437139),MAAC_FLT_C(7762.6024692200581),
    MAAC_FLT_C(7775.1202708469355),MAAC_FLT_C(7787.6431128619733),MAAC_FLT_C(7800.1709912109645),MAAC_FLT_C(7812.7039018478481),
    MAAC_FLT_C(7825.2418407346768),MAAC_FLT_C(7837.7848038415968),MAAC_FLT_C(7850.3327871468155),MAAC_FLT_C(7862.8857866365806),
    MAAC_FLT_C(7875.4437983051539),MAAC_FLT_C(7888.0068181547840),MAAC_FLT_C(7900.5748421956805),MAAC_FLT_C(7913.1478664459901),
    MAAC_FLT_C(7925.7258869317720),MAAC_FLT_C(7938.3088996869719),MAAC_FLT_C(7950.8969007533951),MAAC_FLT_C(7963.4898861806851),
    MAAC_FLT_C(7976.0878520262959),MAAC_FLT_C(7988.6907943554688),MAAC_FLT_C(8001.2987092412086),MAAC_FLT_C(8013.9115927642570),
    MAAC_FLT_C(8026.5294410130691),MAAC_FLT_C(8039.1522500837891),MAAC_FLT_C(8051.7800160802271),MAAC_FLT_C(8064.4127351138350),
    MAAC_FLT_C(8077.0504033036796),MAAC_FLT_C(8089.6930167764222),MAAC_FLT_C(8102.3405716662946),MAAC_FLT_C(8114.9930641150731),
    MAAC_FLT_C(8127.6504902720571),MAAC_FLT_C(8140.3128462940449),MAAC_FLT_C(8152.9801283453098),MAAC_FLT_C(8165.6523325975786),
    MAAC_FLT_C(8178.3294552300049),MAAC_FLT_C(8191.0114924291529),MAAC_FLT_C(8203.6984403889655),MAAC_FLT_C(8216.3902953107463),
    MAAC_FLT_C(8229.0870534031419),MAAC_FLT_C(8241.7887108821069),MAAC_FLT_C(8254.4952639708936),MAAC_FLT_C(8267.2067089000211),
    MAAC_FLT_C(8279.9230419072574),MAAC_FLT_C(8292.6442592375952),MAAC_FLT_C(8305.3703571432306),MAAC_FLT_C(8318.1013318835430),
    MAAC_FLT_C(8330.8371797250657),MAAC_FLT_C(8343.5778969414750),MAAC_FLT_C(8356.3234798135582),MAAC_FLT_C(8369.0739246291978),
    MAAC_FLT_C(8381.8292276833508),MAAC_FLT_C(8394.5893852780209),MAAC_FLT_C(8407.3543937222421),MAAC_FLT_C(8420.1242493320588),
    MAAC_FLT_C(8432.8989484304948),MAAC_FLT_C(8445.6784873475499),MAAC_FLT_C(8458.4628624201578),MAAC_FLT_C(8471.2520699921806),
    MAAC_FLT_C(8484.0461064143838),MAAC_FLT_C(8496.8449680444082),MAAC_FLT_C(8509.6486512467636),MAAC_FLT_C(8522.4571523927953),
    MAAC_FLT_C(8535.2704678606660),MAAC_FLT_C(8548.0885940353437),MAAC_FLT_C(8560.9115273085663),MAAC_FLT_C(8573.7392640788403),
    MAAC_FLT_C(8586.5718007514006),MAAC_FLT_C(8599.4091337382069),MAAC_FLT_C(8612.2512594579148),MAAC_FLT_C(8625.0981743358552),
    MAAC_FLT_C(8637.9498748040205),MAAC_FLT_C(8650.8063573010386),MAAC_FLT_C(8663.6676182721567),MAAC_FLT_C(8676.5336541692250),
    MAAC_FLT_C(8689.4044614506638),MAAC_FLT_C(8702.2800365814601),MAAC_FLT_C(8715.1603760331418),MAAC_FLT_C(8728.0454762837508),
    MAAC_FLT_C(8740.9353338178389),MAAC_FLT_C(8753.8299451264356),MAAC_FLT_C(8766.7293067070332),MAAC_FLT_C(8779.6334150635721),
    MAAC_FLT_C(8792.5422667064158),MAAC_FLT_C(8805.4558581523324),MAAC_FLT_C(8818.3741859244819),MAAC_FLT_C(8831.2972465523908),
    MAAC_FLT_C(8844.2250365719356),MAAC_FLT_C(8857.1575525253265),MAAC_FLT_C(8870.0947909610841),MAAC_FLT_C(8883.0367484340295),
    MAAC_FLT_C(8895.9834215052524),MAAC_FLT_C(8908.9348067421070),MAAC_FLT_C(8921.8909007181846),MAAC_FLT_C(8934.8517000132997),
    MAAC_FLT_C(8947.8172012134710),MAAC_FLT_C(8960.7874009109000),MAAC_FLT_C(8973.7622957039603),MAAC_FLT_C(8986.7418821971733),
    MAAC_FLT_C(8999.7261570011924),MAAC_FLT_C(9012.7151167327884),MAAC_FLT_C(9025.7087580148236),MAAC_FLT_C(9038.7070774762469),
    MAAC_FLT_C(9051.7100717520643),MAAC_FLT_C(9064.7177374833282),MAAC_FLT_C(9077.7300713171153),MAAC_FLT_C(9090.7470699065179),
    MAAC_FLT_C(9103.7687299106146),MAAC_FLT_C(9116.7950479944648),MAAC_FLT_C(9129.8260208290812),MAAC_FLT_C(9142.8616450914233),
    MAAC_FLT_C(9155.9019174643727),MAAC_FLT_C(9168.9468346367157),MAAC_FLT_C(9181.9963933031358),MAAC_FLT_C(9195.0505901641845),
    MAAC_FLT_C(9208.1094219262741),MAAC_FLT_C(9221.1728853016557),MAAC_FLT_C(9234.2409770084050),MAAC_FLT_C(9247.3136937704076),
    MAAC_FLT_C(9260.3910323173386),MAAC_FLT_C(9273.4729893846470),MAAC_FLT_C(9286.5595617135423),MAAC_FLT_C(9299.6507460509747),
    MAAC_FLT_C(9312.7465391496207),MAAC_FLT_C(9325.8469377678684),MAAC_FLT_C(9338.9519386698012),MAAC_FLT_C(9352.0615386251757),
    MAAC_FLT_C(9365.1757344094131),MAAC_FLT_C(9378.2945228035842),MAAC_FLT_C(9391.4179005943843),MAAC_FLT_C(9404.5458645741273),
    MAAC_FLT_C(9417.6784115407263),MAAC_FLT_C(9430.8155382976747),MAAC_FLT_C(9443.9572416540359),MAAC_FLT_C(9457.1035184244265),
    MAAC_FLT_C(9470.2543654290002),MAAC_FLT_C(9483.4097794934296),MAAC_FLT_C(9496.5697574488931),MAAC_FLT_C(9509.7342961320664),
    MAAC_FLT_C(9522.9033923850911),MAAC_FLT_C(9536.0770430555804),MAAC_FLT_C(9549.2552449965824),MAAC_FLT_C(9562.4379950665825),
    MAAC_FLT_C(9575.6252901294793),MAAC_FLT_C(9588.8171270545736),MAAC_FLT_C(9602.0135027165488),MAAC_FLT_C(9615.2144139954635),
    MAAC_FLT_C(9628.4198577767256),MAAC_FLT_C(9641.6298309510930),MAAC_FLT_C(9654.8443304146440),MAAC_FLT_C(9668.0633530687719),
    MAAC_FLT_C(9681.2868958201670),MAAC_FLT_C(9694.5149555808002),MAAC_FLT_C(9707.7475292679192),MAAC_FLT_C(9720.9846138040157),
    MAAC_FLT_C(9734.2262061168276),MAAC_FLT_C(9747.4723031393187),MAAC_FLT_C(9760.7229018096641),MAAC_FLT_C(9773.9779990712323),
    MAAC_FLT_C(9787.2375918725811),MAAC_FLT_C(9800.5016771674327),MAAC_FLT_C(9813.7702519146696),MAAC_FLT_C(9827.0433130783094),
    MAAC_FLT_C(9840.3208576275028),MAAC_FLT_C(9853.6028825365120),MAAC_FLT_C(9866.8893847846994),MAAC_FLT_C(9880.1803613565116),
    MAAC_FLT_C(9893.4758092414686),MAAC_FLT_C(9906.7757254341523),MAAC_FLT_C(9920.0801069341851),MAAC_FLT_C(9933.3889507462245),
    MAAC_FLT_C(9946.7022538799429),MAAC_FLT_C(9960.0200133500221),MAAC_FLT_C(9973.3422261761298),MAAC_FLT_C(9986.6688893829159),
    MAAC_FLT_C(9999.9999999999945),MAAC_FLT_C(10013.335555061929),MAAC_FLT_C(10026.675551608221),MAAC_FLT_C(10040.019986683301),
    MAAC_FLT_C(10053.368857336509),MAAC_FLT_C(10066.722160622081),MAAC_FLT_C(10080.079893599144),MAAC_FLT_C(10093.442053331697),
    MAAC_FLT_C(10106.808636888598),MAAC_FLT_C(10120.179641343550),MAAC_FLT_C(10133.555063775095),MAAC_FLT_C(10146.934901266595),
    MAAC_FLT_C(10160.319150906220),MAAC_FLT_C(10173.707809786936),MAAC_FLT_C(10187.100875006496),MAAC_FLT_C(10200.498343667417),
    MAAC_FLT_C(10213.900212876984),MAAC_FLT_C(10227.306479747222),MAAC_FLT_C(10240.717141394889),MAAC_FLT_C(10254.132194941467),
    MAAC_FLT_C(10267.551637513146),MAAC_FLT_C(10280.975466240814),MAAC_FLT_C(10294.403678260040),MAAC_FLT_C(10307.836270711066),
    MAAC_FLT_C(10321.273240738796),MAAC_FLT_C(10334.714585492780),MAAC_FLT_C(10348.160302127204),MAAC_FLT_C(10361.610387800878),
    MAAC_FLT_C(10375.064839677221),MAAC_FLT_C(10388.523654924256),MAAC_FLT_C(10401.986830714593),MAAC_FLT_C(10415.454364225412),
    MAAC_FLT_C(10428.926252638465),MAAC_FLT_C(10442.402493140049),MAAC_FLT_C(10455.883082921007),MAAC_FLT_C(10469.368019176709),
    MAAC_FLT_C(10482.857299107040),MAAC_FLT_C(10496.350919916393),MAAC_FLT_C(10509.848878813653),MAAC_FLT_C(10523.351173012188),
    MAAC_FLT_C(10536.857799729838),MAAC_FLT_C(10550.368756188900),MAAC_FLT_C(10563.884039616121),MAAC_FLT_C(10577.403647242685),
    MAAC_FLT_C(10590.927576304197),MAAC_FLT_C(10604.455824040679),MAAC_FLT_C(10617.988387696556),MAAC_FLT_C(10631.525264520642),
    MAAC_FLT_C(10645.066451766135),MAAC_FLT_C(10658.611946690598),MAAC_FLT_C(10672.161746555956),MAAC_FLT_C(10685.715848628475),
    MAAC_FLT_C(10699.274250178762),MAAC_FLT_C(10712.836948481747),MAAC_FLT_C(10726.403940816675),MAAC_FLT_C(10739.975224467091),
    MAAC_FLT_C(10753.550796720834),MAAC_FLT_C(10767.130654870027),MAAC_FLT_C(10780.714796211058),MAAC_FLT_C(10794.303218044579),
    MAAC_FLT_C(10807.895917675487),MAAC_FLT_C(10821.492892412922),MAAC_FLT_C(10835.094139570248),MAAC_FLT_C(10848.699656465047),
    MAAC_FLT_C(10862.309440419109),MAAC_FLT_C(10875.923488758415),MAAC_FLT_C(10889.541798813138),MAAC_FLT_C(10903.164367917620),
    MAAC_FLT_C(10916.791193410372),MAAC_FLT_C(10930.422272634056),MAAC_FLT_C(10944.057602935480),MAAC_FLT_C(10957.697181665582),
    MAAC_FLT_C(10971.341006179427),MAAC_FLT_C(10984.989073836190),MAAC_FLT_C(10998.641381999149),MAAC_FLT_C(11012.297928035676),
    MAAC_FLT_C(11025.958709317223),MAAC_FLT_C(11039.623723219316),MAAC_FLT_C(11053.292967121541),MAAC_FLT_C(11066.966438407539),
    MAAC_FLT_C(11080.644134464990),MAAC_FLT_C(11094.326052685608),MAAC_FLT_C(11108.012190465128),MAAC_FLT_C(11121.702545203296),
    MAAC_FLT_C(11135.397114303863),MAAC_FLT_C(11149.095895174571),MAAC_FLT_C(11162.798885227143),MAAC_FLT_C(11176.506081877278),
    MAAC_FLT_C(11190.217482544635),MAAC_FLT_C(11203.933084652828),MAAC_FLT_C(11217.652885629415),MAAC_FLT_C(11231.376882905886),
    MAAC_FLT_C(11245.105073917659),MAAC_FLT_C(11258.837456104062),MAAC_FLT_C(11272.574026908333),MAAC_FLT_C(11286.314783777601),
    MAAC_FLT_C(11300.059724162888),MAAC_FLT_C(11313.808845519083),MAAC_FLT_C(11327.562145304952),MAAC_FLT_C(11341.319620983111),
    MAAC_FLT_C(11355.081270020033),MAAC_FLT_C(11368.847089886023),MAAC_FLT_C(11382.617078055218),MAAC_FLT_C(11396.391232005579),
    MAAC_FLT_C(11410.169549218874),MAAC_FLT_C(11423.952027180676),MAAC_FLT_C(11437.738663380349),MAAC_FLT_C(11451.529455311042),
    MAAC_FLT_C(11465.324400469679),MAAC_FLT_C(11479.123496356951),MAAC_FLT_C(11492.926740477304),MAAC_FLT_C(11506.734130338931),
    MAAC_FLT_C(11520.545663453764),MAAC_FLT_C(11534.361337337466),MAAC_FLT_C(11548.181149509423),MAAC_FLT_C(11562.005097492724),
    MAAC_FLT_C(11575.833178814170),MAAC_FLT_C(11589.665391004253),MAAC_FLT_C(11603.501731597149),MAAC_FLT_C(11617.342198130715),
    MAAC_FLT_C(11631.186788146468),MAAC_FLT_C(11645.035499189589),MAAC_FLT_C(11658.888328808911),MAAC_FLT_C(11672.745274556904),
    MAAC_FLT_C(11686.606333989675),MAAC_FLT_C(11700.471504666955),MAAC_FLT_C(11714.340784152086),MAAC_FLT_C(11728.214170012021),
    MAAC_FLT_C(11742.091659817312),MAAC_FLT_C(11755.973251142101),MAAC_FLT_C(11769.858941564111),MAAC_FLT_C(11783.748728664636),
    MAAC_FLT_C(11797.642610028539),MAAC_FLT_C(11811.540583244237),MAAC_FLT_C(11825.442645903697),MAAC_FLT_C(11839.348795602420),
    MAAC_FLT_C(11853.259029939445),MAAC_FLT_C(11867.173346517333),MAAC_FLT_C(11881.091742942155),MAAC_FLT_C(11895.014216823492),
    MAAC_FLT_C(11908.940765774427),MAAC_FLT_C(11922.871387411526),MAAC_FLT_C(11936.806079354839),MAAC_FLT_C(11950.744839227897),
    MAAC_FLT_C(11964.687664657684),MAAC_FLT_C(11978.634553274653),MAAC_FLT_C(11992.585502712700),MAAC_FLT_C(12006.540510609168),
    MAAC_FLT_C(12020.499574604828),MAAC_FLT_C(12034.462692343879),MAAC_FLT_C(12048.429861473938),MAAC_FLT_C(12062.401079646032),
    MAAC_FLT_C(12076.376344514589),MAAC_FLT_C(12090.355653737432),MAAC_FLT_C(12104.339004975769),MAAC_FLT_C(12118.326395894188),
    MAAC_FLT_C(12132.317824160644),MAAC_FLT_C(12146.313287446457),MAAC_FLT_C(12160.312783426303),MAAC_FLT_C(12174.316309778205),
    MAAC_FLT_C(12188.323864183525),MAAC_FLT_C(12202.335444326955),MAAC_FLT_C(12216.351047896511),MAAC_FLT_C(12230.370672583531),
    MAAC_FLT_C(12244.394316082657),MAAC_FLT_C(12258.421976091831),MAAC_FLT_C(12272.453650312296),MAAC_FLT_C(12286.489336448574),
    MAAC_FLT_C(12300.529032208471),MAAC_FLT_C(12314.572735303058),MAAC_FLT_C(12328.620443446678),MAAC_FLT_C(12342.672154356922),
    MAAC_FLT_C(12356.727865754638),MAAC_FLT_C(12370.787575363909),MAAC_FLT_C(12384.851280912055),MAAC_FLT_C(12398.918980129623),
    MAAC_FLT_C(12412.990670750381),MAAC_FLT_C(12427.066350511306),MAAC_FLT_C(12441.146017152581),MAAC_FLT_C(12455.229668417589),
    MAAC_FLT_C(12469.317302052901),MAAC_FLT_C(12483.408915808270),MAAC_FLT_C(12497.504507436630),MAAC_FLT_C(12511.604074694078),
    MAAC_FLT_C(12525.707615339878),MAAC_FLT_C(12539.815127136444),MAAC_FLT_C(12553.926607849342),MAAC_FLT_C(12568.042055247275),
    MAAC_FLT_C(12582.161467102082),MAAC_FLT_C(12596.284841188726),MAAC_FLT_C(12610.412175285290),MAAC_FLT_C(12624.543467172971),
    MAAC_FLT_C(12638.678714636069),MAAC_FLT_C(12652.817915461985),MAAC_FLT_C(12666.961067441209),MAAC_FLT_C(12681.108168367316),
    MAAC_FLT_C(12695.259216036962),MAAC_FLT_C(12709.414208249869),MAAC_FLT_C(12723.573142808827),MAAC_FLT_C(12737.736017519681),
    MAAC_FLT_C(12751.902830191326),MAAC_FLT_C(12766.073578635704),MAAC_FLT_C(12780.248260667788),MAAC_FLT_C(12794.426874105588),
    MAAC_FLT_C(12808.609416770132),MAAC_FLT_C(12822.795886485468),MAAC_FLT_C(12836.986281078653),MAAC_FLT_C(12851.180598379744),
    MAAC_FLT_C(12865.378836221802),MAAC_FLT_C(12879.580992440871),MAAC_FLT_C(12893.787064875984),MAAC_FLT_C(12907.997051369144),
    MAAC_FLT_C(12922.210949765335),MAAC_FLT_C(12936.428757912496),MAAC_FLT_C(12950.650473661524),MAAC_FLT_C(12964.876094866273),
    MAAC_FLT_C(12979.105619383534),MAAC_FLT_C(12993.339045073039),MAAC_FLT_C(13007.576369797454),MAAC_FLT_C(13021.817591422368),
    MAAC_FLT_C(13036.062707816285),MAAC_FLT_C(13050.311716850629),MAAC_FLT_C(13064.564616399723),MAAC_FLT_C(13078.821404340792),
    MAAC_FLT_C(13093.082078553954),MAAC_FLT_C(13107.346636922217),MAAC_FLT_C(13121.615077331466),MAAC_FLT_C(13135.887397670458),
    MAAC_FLT_C(13150.163595830827),MAAC_FLT_C(13164.443669707060),MAAC_FLT_C(13178.727617196502),MAAC_FLT_C(13193.015436199352),
    MAAC_FLT_C(13207.307124618648),MAAC_FLT_C(13221.602680360265),MAAC_FLT_C(13235.902101332911),MAAC_FLT_C(13250.205385448118),
    MAAC_FLT_C(13264.512530620239),MAAC_FLT_C(13278.823534766434),MAAC_FLT_C(13293.138395806676),MAAC_FLT_C(13307.457111663734),
    MAAC_FLT_C(13321.779680263176),MAAC_FLT_C(13336.106099533356),MAAC_FLT_C(13350.436367405409),MAAC_FLT_C(13364.770481813250),
    MAAC_FLT_C(13379.108440693562),MAAC_FLT_C(13393.450241985796),MAAC_FLT_C(13407.795883632158),MAAC_FLT_C(13422.145363577607),
    MAAC_FLT_C(13436.498679769855),MAAC_FLT_C(13450.855830159346),MAAC_FLT_C(13465.216812699266),MAAC_FLT_C(13479.581625345529),
    MAAC_FLT_C(13493.950266056772),MAAC_FLT_C(13508.322732794350),MAAC_FLT_C(13522.699023522329),MAAC_FLT_C(13537.079136207483),
    MAAC_FLT_C(13551.463068819286),MAAC_FLT_C(13565.850819329906),MAAC_FLT_C(13580.242385714200),MAAC_FLT_C(13594.637765949710),
    MAAC_FLT_C(13609.036958016657),MAAC_FLT_C(13623.439959897927),MAAC_FLT_C(13637.846769579081),MAAC_FLT_C(13652.257385048333),
    MAAC_FLT_C(13666.671804296560),MAAC_FLT_C(13681.090025317284),MAAC_FLT_C(13695.512046106669),MAAC_FLT_C(13709.937864663521),
    MAAC_FLT_C(13724.367478989278),MAAC_FLT_C(13738.800887088004),MAAC_FLT_C(13753.238086966385),MAAC_FLT_C(13767.679076633727),
    MAAC_FLT_C(13782.123854101939),MAAC_FLT_C(13796.572417385545),MAAC_FLT_C(13811.024764501659),MAAC_FLT_C(13825.480893469998),
    MAAC_FLT_C(13839.940802312860),MAAC_FLT_C(13854.404489055134),MAAC_FLT_C(13868.871951724283),MAAC_FLT_C(13883.343188350342),
    MAAC_FLT_C(13897.818196965914),MAAC_FLT_C(13912.296975606168),MAAC_FLT_C(13926.779522308825),MAAC_FLT_C(13941.265835114160),
    MAAC_FLT_C(13955.755912064991),MAAC_FLT_C(13970.249751206682),MAAC_FLT_C(13984.747350587126),MAAC_FLT_C(13999.248708256751),
    MAAC_FLT_C(14013.753822268511),MAAC_FLT_C(14028.262690677873),MAAC_FLT_C(14042.775311542828),MAAC_FLT_C(14057.291682923867),
    MAAC_FLT_C(14071.811802883994),MAAC_FLT_C(14086.335669488704),MAAC_FLT_C(14100.863280805994),MAAC_FLT_C(14115.394634906341),
    MAAC_FLT_C(14129.929729862710),MAAC_FLT_C(14144.468563750548),MAAC_FLT_C(14159.011134647770),MAAC_FLT_C(14173.557440634760),
    MAAC_FLT_C(14188.107479794369),MAAC_FLT_C(14202.661250211901),MAAC_FLT_C(14217.218749975118),MAAC_FLT_C(14231.779977174227),
    MAAC_FLT_C(14246.344929901879),MAAC_FLT_C(14260.913606253163),MAAC_FLT_C(14275.486004325601),MAAC_FLT_C(14290.062122219146),
    MAAC_FLT_C(14304.641958036171),MAAC_FLT_C(14319.225509881466),MAAC_FLT_C(14333.812775862236),MAAC_FLT_C(14348.403754088098),
    MAAC_FLT_C(14362.998442671067),MAAC_FLT_C(14377.596839725560),MAAC_FLT_C(14392.198943368388),MAAC_FLT_C(14406.804751718748),
    MAAC_FLT_C(14421.414262898223),MAAC_FLT_C(14436.027475030774),MAAC_FLT_C(14450.644386242740),MAAC_FLT_C(14465.264994662828),
    MAAC_FLT_C(14479.889298422106),MAAC_FLT_C(14494.517295654005),MAAC_FLT_C(14509.148984494313),MAAC_FLT_C(14523.784363081166),
    MAAC_FLT_C(14538.423429555049),MAAC_FLT_C(14553.066182058781),MAAC_FLT_C(14567.712618737527),MAAC_FLT_C(14582.362737738777),
    MAAC_FLT_C(14597.016537212348),MAAC_FLT_C(14611.674015310382),MAAC_FLT_C(14626.335170187340),MAAC_FLT_C(14640.999999999993),
    MAAC_FLT_C(14655.668502907418),MAAC_FLT_C(14670.340677071003),MAAC_FLT_C(14685.016520654426),MAAC_FLT_C(14699.696031823671),
    MAAC_FLT_C(14714.379208746999),MAAC_FLT_C(14729.066049594967),MAAC_FLT_C(14743.756552540408),MAAC_FLT_C(14758.450715758430),
    MAAC_FLT_C(14773.148537426418),MAAC_FLT_C(14787.850015724018),MAAC_FLT_C(14802.555148833142),MAAC_FLT_C(14817.263934937961),
    MAAC_FLT_C(14831.976372224897),MAAC_FLT_C(14846.692458882624),MAAC_FLT_C(14861.412193102060),MAAC_FLT_C(14876.135573076363),
    MAAC_FLT_C(14890.862597000923),MAAC_FLT_C(14905.593263073371),MAAC_FLT_C(14920.327569493558),MAAC_FLT_C(14935.065514463557),
    MAAC_FLT_C(14949.807096187662),MAAC_FLT_C(14964.552312872382),MAAC_FLT_C(14979.301162726431),MAAC_FLT_C(14994.053643960735),
    MAAC_FLT_C(15008.809754788414),MAAC_FLT_C(15023.569493424788),MAAC_FLT_C(15038.332858087369),MAAC_FLT_C(15053.099846995858),
    MAAC_FLT_C(15067.870458372134),MAAC_FLT_C(15082.644690440264),MAAC_FLT_C(15097.422541426484),MAAC_FLT_C(15112.204009559202),
    MAAC_FLT_C(15126.989093068994),MAAC_FLT_C(15141.777790188597),MAAC_FLT_C(15156.570099152905),MAAC_FLT_C(15171.366018198967),
    MAAC_FLT_C(15186.165545565986),MAAC_FLT_C(15200.968679495301),MAAC_FLT_C(15215.775418230402),MAAC_FLT_C(15230.585760016909),
    MAAC_FLT_C(15245.399703102579),MAAC_FLT_C(15260.217245737298),MAAC_FLT_C(15275.038386173073),MAAC_FLT_C(15289.863122664035),
    MAAC_FLT_C(15304.691453466432),MAAC_FLT_C(15319.523376838621),MAAC_FLT_C(15334.358891041069),MAAC_FLT_C(15349.197994336346),
    MAAC_FLT_C(15364.040684989128),MAAC_FLT_C(15378.886961266177),MAAC_FLT_C(15393.736821436356),MAAC_FLT_C(15408.590263770609),
    MAAC_FLT_C(15423.447286541972),MAAC_FLT_C(15438.307888025554),MAAC_FLT_C(15453.172066498542),MAAC_FLT_C(15468.039820240196),
    MAAC_FLT_C(15482.911147531840),MAAC_FLT_C(15497.786046656869),MAAC_FLT_C(15512.664515900733),MAAC_FLT_C(15527.546553550939),
    MAAC_FLT_C(15542.432157897045),MAAC_FLT_C(15557.321327230660),MAAC_FLT_C(15572.214059845435),MAAC_FLT_C(15587.110354037064),
    MAAC_FLT_C(15602.010208103273),MAAC_FLT_C(15616.913620343823),MAAC_FLT_C(15631.820589060506),MAAC_FLT_C(15646.731112557136),
    MAAC_FLT_C(15661.645189139546),MAAC_FLT_C(15676.562817115593),MAAC_FLT_C(15691.483994795139),MAAC_FLT_C(15706.408720490062),
    MAAC_FLT_C(15721.336992514242),MAAC_FLT_C(15736.268809183561),MAAC_FLT_C(15751.204168815901),MAAC_FLT_C(15766.143069731135),
    MAAC_FLT_C(15781.085510251132),MAAC_FLT_C(15796.031488699740),MAAC_FLT_C(15810.981003402798),MAAC_FLT_C(15825.934052688119),
    MAAC_FLT_C(15840.890634885489),MAAC_FLT_C(15855.850748326673),MAAC_FLT_C(15870.814391345401),MAAC_FLT_C(15885.781562277361),
    MAAC_FLT_C(15900.752259460214),MAAC_FLT_C(15915.726481233565),MAAC_FLT_C(15930.704225938984),MAAC_FLT_C(15945.685491919978),
    MAAC_FLT_C(15960.670277522009),MAAC_FLT_C(15975.658581092481),MAAC_FLT_C(15990.650400980730),MAAC_FLT_C(16005.645735538035),
    MAAC_FLT_C(16020.644583117599),MAAC_FLT_C(16035.646942074556),MAAC_FLT_C(16050.652810765967),MAAC_FLT_C(16065.662187550806),
    MAAC_FLT_C(16080.675070789974),MAAC_FLT_C(16095.691458846273),MAAC_FLT_C(16110.711350084424),MAAC_FLT_C(16125.734742871053),
    MAAC_FLT_C(16140.761635574685),MAAC_FLT_C(16155.792026565747),MAAC_FLT_C(16170.825914216561),MAAC_FLT_C(16185.863296901338),
    MAAC_FLT_C(16200.904172996183),MAAC_FLT_C(16215.948540879079),MAAC_FLT_C(16230.996398929899),MAAC_FLT_C(16246.047745530386),
    MAAC_FLT_C(16261.102579064163),MAAC_FLT_C(16276.160897916721),MAAC_FLT_C(16291.222700475420),MAAC_FLT_C(16306.287985129484),
    MAAC_FLT_C(16321.356750269995),MAAC_FLT_C(16336.428994289896),MAAC_FLT_C(16351.504715583982),MAAC_FLT_C(16366.583912548900),
    MAAC_FLT_C(16381.666583583141),MAAC_FLT_C(16396.752727087041),MAAC_FLT_C(16411.842341462776),MAAC_FLT_C(16426.935425114363),
    MAAC_FLT_C(16442.031976447644),MAAC_FLT_C(16457.131993870298),MAAC_FLT_C(16472.235475791829),MAAC_FLT_C(16487.342420623561),
    MAAC_FLT_C(16502.452826778641),MAAC_FLT_C(16517.566692672033),MAAC_FLT_C(16532.684016720516),MAAC_FLT_C(16547.804797342676),
    MAAC_FLT_C(16562.929032958902),MAAC_FLT_C(16578.056721991394),MAAC_FLT_C(16593.187862864150),MAAC_FLT_C(16608.322454002962),
    MAAC_FLT_C(16623.460493835417),MAAC_FLT_C(16638.601980790896),MAAC_FLT_C(16653.746913300558),MAAC_FLT_C(16668.895289797354),
    MAAC_FLT_C(16684.047108716015),MAAC_FLT_C(16699.202368493046),MAAC_FLT_C(16714.361067566726),MAAC_FLT_C(16729.523204377107),
    MAAC_FLT_C(16744.688777366009),MAAC_FLT_C(16759.857784977012),MAAC_FLT_C(16775.030225655464),MAAC_FLT_C(16790.206097848466),
    MAAC_FLT_C(16805.385400004874),MAAC_FLT_C(16820.568130575302),MAAC_FLT_C(16835.754288012104),MAAC_FLT_C(16850.943870769381),
    MAAC_FLT_C(16866.136877302983),MAAC_FLT_C(16881.333306070494),MAAC_FLT_C(16896.533155531230),MAAC_FLT_C(16911.736424146249),
    MAAC_FLT_C(16926.943110378332),MAAC_FLT_C(16942.153212691992),MAAC_FLT_C(16957.366729553454),MAAC_FLT_C(16972.583659430682),
    MAAC_FLT_C(16987.804000793338),MAAC_FLT_C(17003.027752112816),MAAC_FLT_C(17018.254911862205),MAAC_FLT_C(17033.485478516312),
    MAAC_FLT_C(17048.719450551645),MAAC_FLT_C(17063.956826446421),MAAC_FLT_C(17079.197604680547),MAAC_FLT_C(17094.441783735630),
    MAAC_FLT_C(17109.689362094967),MAAC_FLT_C(17124.940338243552),MAAC_FLT_C(17140.194710668064),MAAC_FLT_C(17155.452477856852),
    MAAC_FLT_C(17170.713638299967),MAAC_FLT_C(17185.978190489128),MAAC_FLT_C(17201.246132917724),MAAC_FLT_C(17216.517464080825),
    MAAC_FLT_C(17231.792182475165),MAAC_FLT_C(17247.070286599141),MAAC_FLT_C(17262.351774952826),MAAC_FLT_C(17277.636646037936),
    MAAC_FLT_C(17292.924898357855),MAAC_FLT_C(17308.216530417623),MAAC_FLT_C(17323.511540723921),MAAC_FLT_C(17338.809927785089),
    MAAC_FLT_C(17354.111690111105),MAAC_FLT_C(17369.416826213594),MAAC_FLT_C(17384.725334605821),MAAC_FLT_C(17400.037213802683),
    MAAC_FLT_C(17415.352462320716),MAAC_FLT_C(17430.671078678090),MAAC_FLT_C(17445.993061394587),MAAC_FLT_C(17461.318408991636),
    MAAC_FLT_C(17476.647119992274),MAAC_FLT_C(17491.979192921168),MAAC_FLT_C(17507.314626304586),MAAC_FLT_C(17522.653418670423),
    MAAC_FLT_C(17537.995568548187),MAAC_FLT_C(17553.341074468986),MAAC_FLT_C(17568.689934965536),MAAC_FLT_C(17584.042148572156),
    MAAC_FLT_C(17599.397713824768),MAAC_FLT_C(17614.756629260890),MAAC_FLT_C(17630.118893419625),MAAC_FLT_C(17645.484504841683),
    MAAC_FLT_C(17660.853462069354),MAAC_FLT_C(17676.225763646511),MAAC_FLT_C(17691.601408118619),MAAC_FLT_C(17706.980394032718),
    MAAC_FLT_C(17722.362719937424),MAAC_FLT_C(17737.748384382936),MAAC_FLT_C(17753.137385921014),MAAC_FLT_C(17768.529723104999),
    MAAC_FLT_C(17783.925394489790),MAAC_FLT_C(17799.324398631856),MAAC_FLT_C(17814.726734089225),MAAC_FLT_C(17830.132399421480),
    MAAC_FLT_C(17845.541393189767),MAAC_FLT_C(17860.953713956780),MAAC_FLT_C(17876.369360286772),MAAC_FLT_C(17891.788330745530),
    MAAC_FLT_C(17907.210623900395),MAAC_FLT_C(17922.636238320254),MAAC_FLT_C(17938.065172575527),MAAC_FLT_C(17953.497425238176),
    MAAC_FLT_C(17968.932994881692),MAAC_FLT_C(17984.371880081104),MAAC_FLT_C(17999.814079412972),MAAC_FLT_C(18015.259591455371),
    MAAC_FLT_C(18030.708414787914),MAAC_FLT_C(18046.160547991731),MAAC_FLT_C(18061.615989649465),MAAC_FLT_C(18077.074738345284),
    MAAC_FLT_C(18092.536792664861),MAAC_FLT_C(18108.002151195393),MAAC_FLT_C(18123.470812525571),MAAC_FLT_C(18138.942775245599),
    MAAC_FLT_C(18154.418037947191),MAAC_FLT_C(18169.896599223546),MAAC_FLT_C(18185.378457669380),MAAC_FLT_C(18200.863611880886),
    MAAC_FLT_C(18216.352060455767),MAAC_FLT_C(18231.843801993204),MAAC_FLT_C(18247.338835093873),MAAC_FLT_C(18262.837158359936),
    MAAC_FLT_C(18278.338770395032),MAAC_FLT_C(18293.843669804290),MAAC_FLT_C(18309.351855194309),MAAC_FLT_C(18324.863325173166),
    MAAC_FLT_C(18340.378078350412),MAAC_FLT_C(18355.896113337069),MAAC_FLT_C(18371.417428745623),MAAC_FLT_C(18386.942023190033),
    MAAC_FLT_C(18402.469895285718),MAAC_FLT_C(18418.001043649550),MAAC_FLT_C(18433.535466899870),MAAC_FLT_C(18449.073163656474),
    MAAC_FLT_C(18464.614132540602),MAAC_FLT_C(18480.158372174956),MAAC_FLT_C(18495.705881183676),MAAC_FLT_C(18511.256658192357),
    MAAC_FLT_C(18526.810701828035),MAAC_FLT_C(18542.368010719183),MAAC_FLT_C(18557.928583495715),MAAC_FLT_C(18573.492418788985),
    MAAC_FLT_C(18589.059515231773),MAAC_FLT_C(18604.629871458303),MAAC_FLT_C(18620.203486104212),MAAC_FLT_C(18635.780357806580),
    MAAC_FLT_C(18651.360485203899),MAAC_FLT_C(18666.943866936086),MAAC_FLT_C(18682.530501644480),MAAC_FLT_C(18698.120387971841),
    MAAC_FLT_C(18713.713524562332),MAAC_FLT_C(18729.309910061540),MAAC_FLT_C(18744.909543116457),MAAC_FLT_C(18760.512422375479),
    MAAC_FLT_C(18776.118546488418),MAAC_FLT_C(18791.727914106479),MAAC_FLT_C(18807.340523882274),MAAC_FLT_C(18822.956374469810),
    MAAC_FLT_C(18838.575464524489),MAAC_FLT_C(18854.197792703111),MAAC_FLT_C(18869.823357663863),MAAC_FLT_C(18885.452158066328),
    MAAC_FLT_C(18901.084192571470),MAAC_FLT_C(18916.719459841639),MAAC_FLT_C(18932.357958540564),MAAC_FLT_C(18947.999687333362),
    MAAC_FLT_C(18963.644644886521),MAAC_FLT_C(18979.292829867907),MAAC_FLT_C(18994.944240946759),MAAC_FLT_C(19010.598876793687),
    MAAC_FLT_C(19026.256736080668),MAAC_FLT_C(19041.917817481048),MAAC_FLT_C(19057.582119669532),MAAC_FLT_C(19073.249641322200),
    MAAC_FLT_C(19088.920381116473),MAAC_FLT_C(19104.594337731145),MAAC_FLT_C(19120.271509846356),MAAC_FLT_C(19135.951896143604),
    MAAC_FLT_C(19151.635495305738),MAAC_FLT_C(19167.322306016948),MAAC_FLT_C(19183.012326962784),MAAC_FLT_C(19198.705556830122),
    MAAC_FLT_C(19214.401994307198),MAAC_FLT_C(19230.101638083579),MAAC_FLT_C(19245.804486850167),MAAC_FLT_C(19261.510539299208),
    MAAC_FLT_C(19277.219794124274),MAAC_FLT_C(19292.932250020265),MAAC_FLT_C(19308.647905683421),MAAC_FLT_C(19324.366759811302),
    MAAC_FLT_C(19340.088811102793),MAAC_FLT_C(19355.814058258100),MAAC_FLT_C(19371.542499978754),MAAC_FLT_C(19387.274134967600),
    MAAC_FLT_C(19403.008961928797),MAAC_FLT_C(19418.746979567823),MAAC_FLT_C(19434.488186591469),MAAC_FLT_C(19450.232581707827),
    MAAC_FLT_C(19465.980163626304),MAAC_FLT_C(19481.730931057613),MAAC_FLT_C(19497.484882713761),MAAC_FLT_C(19513.242017308068),
    MAAC_FLT_C(19529.002333555141),MAAC_FLT_C(19544.765830170898),MAAC_FLT_C(19560.532505872539),MAAC_FLT_C(19576.302359378566),
    MAAC_FLT_C(19592.075389408761),MAAC_FLT_C(19607.851594684209),MAAC_FLT_C(19623.630973927269),MAAC_FLT_C(19639.413525861590),
    MAAC_FLT_C(19655.199249212103),MAAC_FLT_C(19670.988142705017),MAAC_FLT_C(19686.780205067826),MAAC_FLT_C(19702.575435029288),
    MAAC_FLT_C(19718.373831319448),MAAC_FLT_C(19734.175392669615),MAAC_FLT_C(19749.980117812371),MAAC_FLT_C(19765.788005481569),
    MAAC_FLT_C(19781.599054412323),MAAC_FLT_C(19797.413263341008),MAAC_FLT_C(19813.230631005274),MAAC_FLT_C(19829.051156144014),
    MAAC_FLT_C(19844.874837497395),MAAC_FLT_C(19860.701673806827),MAAC_FLT_C(19876.531663814985),MAAC_FLT_C(19892.364806265789),
    MAAC_FLT_C(19908.201099904407),MAAC_FLT_C(19924.040543477258),MAAC_FLT_C(19939.883135732012),MAAC_FLT_C(19955.728875417579),
    MAAC_FLT_C(19971.577761284105),MAAC_FLT_C(19987.429792082985),MAAC_FLT_C(20003.284966566847),MAAC_FLT_C(20019.143283489560),
    MAAC_FLT_C(20035.004741606219),MAAC_FLT_C(20050.869339673161),MAAC_FLT_C(20066.737076447942),MAAC_FLT_C(20082.607950689362),
    MAAC_FLT_C(20098.481961157428),MAAC_FLT_C(20114.359106613385),MAAC_FLT_C(20130.239385819699),MAAC_FLT_C(20146.122797540054),
    MAAC_FLT_C(20162.009340539353),MAAC_FLT_C(20177.899013583716),MAAC_FLT_C(20193.791815440476),MAAC_FLT_C(20209.687744878182),
    MAAC_FLT_C(20225.586800666591),MAAC_FLT_C(20241.488981576669),MAAC_FLT_C(20257.394286380597),MAAC_FLT_C(20273.302713851754),
    MAAC_FLT_C(20289.214262764715),MAAC_FLT_C(20305.128931895277),MAAC_FLT_C(20321.046720020415),MAAC_FLT_C(20336.967625918318),
    MAAC_FLT_C(20352.891648368361),MAAC_FLT_C(20368.818786151114),MAAC_FLT_C(20384.749038048347),MAAC_FLT_C(20400.682402843009),
    MAAC_FLT_C(20416.618879319249),MAAC_FLT_C(20432.558466262391),MAAC_FLT_C(20448.501162458953),MAAC_FLT_C(20464.446966696629),
    MAAC_FLT_C(20480.395877764302),MAAC_FLT_C(20496.347894452025),MAAC_FLT_C(20512.303015551031),MAAC_FLT_C(20528.261239853735),
    MAAC_FLT_C(20544.222566153720),MAAC_FLT_C(20560.186993245741),MAAC_FLT_C(20576.154519925720),MAAC_FLT_C(20592.125144990758),
    MAAC_FLT_C(20608.098867239107),MAAC_FLT_C(20624.075685470198),MAAC_FLT_C(20640.055598484618),MAAC_FLT_C(20656.038605084115),
    MAAC_FLT_C(20672.024704071595),MAAC_FLT_C(20688.013894251126),MAAC_FLT_C(20704.006174427926),MAAC_FLT_C(20720.001543408373),
    MAAC_FLT_C(20735.999999999989),MAAC_FLT_C(20752.001543011454),MAAC_FLT_C(20768.006171252597),MAAC_FLT_C(20784.013883534382),
    MAAC_FLT_C(20800.024678668931),MAAC_FLT_C(20816.038555469506),MAAC_FLT_C(20832.055512750507),MAAC_FLT_C(20848.075549327474),
    MAAC_FLT_C(20864.098664017085),MAAC_FLT_C(20880.124855637161),MAAC_FLT_C(20896.154123006647),MAAC_FLT_C(20912.186464945626),
    MAAC_FLT_C(20928.221880275312),MAAC_FLT_C(20944.260367818049),MAAC_FLT_C(20960.301926397311),MAAC_FLT_C(20976.346554837684),
    MAAC_FLT_C(20992.394251964895),MAAC_FLT_C(21008.445016605787),MAAC_FLT_C(21024.498847588318),MAAC_FLT_C(21040.555743741574),
    MAAC_FLT_C(21056.615703895754),MAAC_FLT_C(21072.678726882168),MAAC_FLT_C(21088.744811533252),MAAC_FLT_C(21104.813956682538),
    MAAC_FLT_C(21120.886161164683),MAAC_FLT_C(21136.961423815439),MAAC_FLT_C(21153.039743471683),MAAC_FLT_C(21169.121118971379),
    MAAC_FLT_C(21185.205549153605),MAAC_FLT_C(21201.293032858535),MAAC_FLT_C(21217.383568927453),MAAC_FLT_C(21233.477156202731),
    MAAC_FLT_C(21249.573793527841),MAAC_FLT_C(21265.673479747358),MAAC_FLT_C(21281.776213706937),MAAC_FLT_C(21297.881994253334),
    MAAC_FLT_C(21313.990820234398),MAAC_FLT_C(21330.102690499054),MAAC_FLT_C(21346.217603897330),MAAC_FLT_C(21362.335559280327),
    MAAC_FLT_C(21378.456555500241),MAAC_FLT_C(21394.580591410333),MAAC_FLT_C(21410.707665864964),MAAC_FLT_C(21426.837777719556),
    MAAC_FLT_C(21442.970925830628),MAAC_FLT_C(21459.107109055756),MAAC_FLT_C(21475.246326253604),MAAC_FLT_C(21491.388576283895),
    MAAC_FLT_C(21507.533858007431),MAAC_FLT_C(21523.682170286087),MAAC_FLT_C(21539.833511982797),MAAC_FLT_C(21555.987881961566),
    MAAC_FLT_C(21572.145279087461),MAAC_FLT_C(21588.305702226615),MAAC_FLT_C(21604.469150246216),MAAC_FLT_C(21620.635622014521),
    MAAC_FLT_C(21636.805116400832),MAAC_FLT_C(21652.977632275521),MAAC_FLT_C(21669.153168510009),MAAC_FLT_C(21685.331723976764),
    MAAC_FLT_C(21701.513297549318),MAAC_FLT_C(21717.697888102244),MAAC_FLT_C(21733.885494511167),MAAC_FLT_C(21750.076115652759),
    MAAC_FLT_C(21766.269750404736),MAAC_FLT_C(21782.466397645861),MAAC_FLT_C(21798.666056255934),MAAC_FLT_C(21814.868725115801),
    MAAC_FLT_C(21831.074403107345),MAAC_FLT_C(21847.283089113484),MAAC_FLT_C(21863.494782018177),MAAC_FLT_C(21879.709480706417),
    MAAC_FLT_C(21895.927184064229),MAAC_FLT_C(21912.147890978667),MAAC_FLT_C(21928.371600337818),MAAC_FLT_C(21944.598311030797),
    MAAC_FLT_C(21960.828021947746),MAAC_FLT_C(21977.060731979829),MAAC_FLT_C(21993.296440019243),MAAC_FLT_C(22009.535144959198),
    MAAC_FLT_C(22025.776845693930),MAAC_FLT_C(22042.021541118691),MAAC_FLT_C(22058.269230129757),MAAC_FLT_C(22074.519911624411),
    MAAC_FLT_C(22090.773584500959),MAAC_FLT_C(22107.030247658717),MAAC_FLT_C(22123.289899998013),MAAC_FLT_C(22139.552540420187),
    MAAC_FLT_C(22155.818167827587),MAAC_FLT_C(22172.086781123569),MAAC_FLT_C(22188.358379212495),MAAC_FLT_C(22204.632960999730),
    MAAC_FLT_C(22220.910525391639),MAAC_FLT_C(22237.191071295601),MAAC_FLT_C(22253.474597619981),MAAC_FLT_C(22269.761103274148),
    MAAC_FLT_C(22286.050587168469),MAAC_FLT_C(22302.343048214312),MAAC_FLT_C(22318.638485324027),MAAC_FLT_C(22334.936897410968),
    MAAC_FLT_C(22351.238283389470),MAAC_FLT_C(22367.542642174871),MAAC_FLT_C(22383.849972683482),MAAC_FLT_C(22400.160273832618),
    MAAC_FLT_C(22416.473544540568),MAAC_FLT_C(22432.789783726603),MAAC_FLT_C(22449.108990310986),MAAC_FLT_C(22465.431163214958),
    MAAC_FLT_C(22481.756301360740),MAAC_FLT_C(22498.084403671528),MAAC_FLT_C(22514.415469071497),MAAC_FLT_C(22530.749496485802),
    MAAC_FLT_C(22547.086484840562),MAAC_FLT_C(22563.426433062879),MAAC_FLT_C(22579.769340080824),MAAC_FLT_C(22596.115204823436),
    MAAC_FLT_C(22612.464026220721),MAAC_FLT_C(22628.815803203655),MAAC_FLT_C(22645.170534704179),MAAC_FLT_C(22661.528219655200),
    MAAC_FLT_C(22677.888856990587),MAAC_FLT_C(22694.252445645168),MAAC_FLT_C(22710.618984554734),MAAC_FLT_C(22726.988472656034),
    MAAC_FLT_C(22743.360908886778),MAAC_FLT_C(22759.736292185622),MAAC_FLT_C(22776.114621492186),MAAC_FLT_C(22792.495895747044),
    MAAC_FLT_C(22808.880113891719),MAAC_FLT_C(22825.267274868678),MAAC_FLT_C(22841.657377621348),MAAC_FLT_C(22858.050421094096),
    MAAC_FLT_C(22874.446404232243),MAAC_FLT_C(22890.845325982053),MAAC_FLT_C(22907.247185290722),MAAC_FLT_C(22923.651981106406),
    MAAC_FLT_C(22940.059712378195),MAAC_FLT_C(22956.470378056114),MAAC_FLT_C(22972.883977091129),MAAC_FLT_C(22989.300508435150),
    MAAC_FLT_C(23005.719971041017),MAAC_FLT_C(23022.142363862498),MAAC_FLT_C(23038.567685854305),MAAC_FLT_C(23054.995935972078),
    MAAC_FLT_C(23071.427113172387),MAAC_FLT_C(23087.861216412730),MAAC_FLT_C(23104.298244651531),MAAC_FLT_C(23120.738196848146),
    MAAC_FLT_C(23137.181071962848),MAAC_FLT_C(23153.626868956846),MAAC_FLT_C(23170.075586792263),MAAC_FLT_C(23186.527224432142),
    MAAC_FLT_C(23202.981780840448),MAAC_FLT_C(23219.439254982066),MAAC_FLT_C(23235.899645822796),MAAC_FLT_C(23252.362952329357),
    MAAC_FLT_C(23268.829173469378),MAAC_FLT_C(23285.298308211408),MAAC_FLT_C(23301.770355524899),MAAC_FLT_C(23318.245314380223),
    MAAC_FLT_C(23334.723183748658),MAAC_FLT_C(23351.203962602387),MAAC_FLT_C(23367.687649914504),MAAC_FLT_C(23384.174244659007),
    MAAC_FLT_C(23400.663745810798),MAAC_FLT_C(23417.156152345680),MAAC_FLT_C(23433.651463240367),MAAC_FLT_C(23450.149677472462),
    MAAC_FLT_C(23466.650794020472),MAAC_FLT_C(23483.154811863806),MAAC_FLT_C(23499.661729982763),MAAC_FLT_C(23516.171547358539),
    MAAC_FLT_C(23532.684262973231),MAAC_FLT_C(23549.199875809823),MAAC_FLT_C(23565.718384852185),MAAC_FLT_C(23582.239789085092),
    MAAC_FLT_C(23598.764087494197),MAAC_FLT_C(23615.291279066041),MAAC_FLT_C(23631.821362788058),MAAC_FLT_C(23648.354337648565),
    MAAC_FLT_C(23664.890202636761),MAAC_FLT_C(23681.428956742733),MAAC_FLT_C(23697.970598957443),MAAC_FLT_C(23714.515128272738),
    MAAC_FLT_C(23731.062543681343),MAAC_FLT_C(23747.612844176863),MAAC_FLT_C(23764.166028753778),MAAC_FLT_C(23780.722096407440),
    MAAC_FLT_C(23797.281046134085),MAAC_FLT_C(23813.842876930816),MAAC_FLT_C(23830.407587795606),MAAC_FLT_C(23846.975177727301),
    MAAC_FLT_C(23863.545645725622),MAAC_FLT_C(23880.118990791150),MAAC_FLT_C(23896.695211925336),MAAC_FLT_C(23913.274308130498),
    MAAC_FLT_C(23929.856278409821),MAAC_FLT_C(23946.441121767348),MAAC_FLT_C(23963.028837207989),MAAC_FLT_C(23979.619423737513),
    MAAC_FLT_C(23996.212880362549),MAAC_FLT_C(24012.809206090584),MAAC_FLT_C(24029.408399929966),MAAC_FLT_C(24046.010460889898),
    MAAC_FLT_C(24062.615387980433),MAAC_FLT_C(24079.223180212492),MAAC_FLT_C(24095.833836597827),MAAC_FLT_C(24112.447356149063),
    MAAC_FLT_C(24129.063737879667),MAAC_FLT_C(24145.682980803951),MAAC_FLT_C(24162.305083937081),MAAC_FLT_C(24178.930046295067),
    MAAC_FLT_C(24195.557866894767),MAAC_FLT_C(24212.188544753884),MAAC_FLT_C(24228.822078890960),MAAC_FLT_C(24245.458468325389),
    MAAC_FLT_C(24262.097712077397),MAAC_FLT_C(24278.739809168048),MAAC_FLT_C(24295.384758619261),MAAC_FLT_C(24312.032559453768),
    MAAC_FLT_C(24328.683210695162),MAAC_FLT_C(24345.336711367858),MAAC_FLT_C(24361.993060497109),MAAC_FLT_C(24378.652257108995),
    MAAC_FLT_C(24395.314300230442),MAAC_FLT_C(24411.979188889192),MAAC_FLT_C(24428.646922113825),MAAC_FLT_C(24445.317498933746),
    MAAC_FLT_C(24461.990918379193),MAAC_FLT_C(24478.667179481225),MAAC_FLT_C(24495.346281271726),MAAC_FLT_C(24512.028222783407),
    MAAC_FLT_C(24528.713003049801),MAAC_FLT_C(24545.400621105266),MAAC_FLT_C(24562.091075984976),MAAC_FLT_C(24578.784366724925),
    MAAC_FLT_C(24595.480492361927),MAAC_FLT_C(24612.179451933614),MAAC_FLT_C(24628.881244478438),MAAC_FLT_C(24645.585869035654),
    MAAC_FLT_C(24662.293324645343),MAAC_FLT_C(24679.003610348398),MAAC_FLT_C(24695.716725186514),MAAC_FLT_C(24712.432668202211),
    MAAC_FLT_C(24729.151438438807),MAAC_FLT_C(24745.873034940436),MAAC_FLT_C(24762.597456752032),MAAC_FLT_C(24779.324702919344),
    MAAC_FLT_C(24796.054772488926),MAAC_FLT_C(24812.787664508123),MAAC_FLT_C(24829.523378025100),MAAC_FLT_C(24846.261912088819),
    MAAC_FLT_C(24863.003265749034),MAAC_FLT_C(24879.747438056307),MAAC_FLT_C(24896.494428062004),MAAC_FLT_C(24913.244234818278),
    MAAC_FLT_C(24929.996857378082),MAAC_FLT_C(24946.752294795166),MAAC_FLT_C(24963.510546124078),MAAC_FLT_C(24980.271610420157),
    MAAC_FLT_C(24997.035486739525),MAAC_FLT_C(25013.802174139113),MAAC_FLT_C(25030.571671676629),MAAC_FLT_C(25047.343978410572),
    MAAC_FLT_C(25064.119093400237),MAAC_FLT_C(25080.897015705697),MAAC_FLT_C(25097.677744387813),MAAC_FLT_C(25114.461278508239),
    MAAC_FLT_C(25131.247617129400),MAAC_FLT_C(25148.036759314517),MAAC_FLT_C(25164.828704127583),MAAC_FLT_C(25181.623450633375),
    MAAC_FLT_C(25198.420997897450),MAAC_FLT_C(25215.221344986145),MAAC_FLT_C(25232.024490966574),MAAC_FLT_C(25248.830434906627),
    MAAC_FLT_C(25265.639175874974),MAAC_FLT_C(25282.450712941049),MAAC_FLT_C(25299.265045175071),MAAC_FLT_C(25316.082171648024),
    MAAC_FLT_C(25332.902091431668),MAAC_FLT_C(25349.724803598532),MAAC_FLT_C(25366.550307221914),MAAC_FLT_C(25383.378601375884),
    MAAC_FLT_C(25400.209685135269),MAAC_FLT_C(25417.043557575678),MAAC_FLT_C(25433.880217773472),MAAC_FLT_C(25450.719664805783),
    MAAC_FLT_C(25467.561897750507),MAAC_FLT_C(25484.406915686297),MAAC_FLT_C(25501.254717692573),MAAC_FLT_C(25518.105302849512),
    MAAC_FLT_C(25534.958670238051),MAAC_FLT_C(25551.814818939889),MAAC_FLT_C(25568.673748037480),MAAC_FLT_C(25585.535456614027),
    MAAC_FLT_C(25602.399943753502),MAAC_FLT_C(25619.267208540619),MAAC_FLT_C(25636.137250060852),MAAC_FLT_C(25653.010067400432),
    MAAC_FLT_C(25669.885659646327),MAAC_FLT_C(25686.764025886270),MAAC_FLT_C(25703.645165208734),MAAC_FLT_C(25720.529076702947),
    MAAC_FLT_C(25737.415759458876),MAAC_FLT_C(25754.305212567244),MAAC_FLT_C(25771.197435119517),MAAC_FLT_C(25788.092426207899),
    MAAC_FLT_C(25804.990184925344),MAAC_FLT_C(25821.890710365547),MAAC_FLT_C(25838.794001622944),MAAC_FLT_C(25855.700057792717),
    MAAC_FLT_C(25872.608877970775),MAAC_FLT_C(25889.520461253778),MAAC_FLT_C(25906.434806739118),MAAC_FLT_C(25923.351913524923),
    MAAC_FLT_C(25940.271780710063),MAAC_FLT_C(25957.194407394138),MAAC_FLT_C(25974.119792677477),MAAC_FLT_C(25991.047935661154),
    MAAC_FLT_C(26007.978835446964),MAAC_FLT_C(26024.912491137442),MAAC_FLT_C(26041.848901835841),MAAC_FLT_C(26058.788066646161),
    MAAC_FLT_C(26075.729984673108),MAAC_FLT_C(26092.674655022132),MAAC_FLT_C(26109.622076799409),MAAC_FLT_C(26126.572249111829),
    MAAC_FLT_C(26143.525171067016),MAAC_FLT_C(26160.480841773315),MAAC_FLT_C(26177.439260339790),MAAC_FLT_C(26194.400425876229),
    MAAC_FLT_C(26211.364337493145),MAAC_FLT_C(26228.330994301767),MAAC_FLT_C(26245.300395414040),MAAC_FLT_C(26262.272539942627),
    MAAC_FLT_C(26279.247427000919),MAAC_FLT_C(26296.225055703006),MAAC_FLT_C(26313.205425163702),MAAC_FLT_C(26330.188534498539),
    MAAC_FLT_C(26347.174382823756),MAAC_FLT_C(26364.162969256307),MAAC_FLT_C(26381.154292913852),MAAC_FLT_C(26398.148352914774),
    MAAC_FLT_C(26415.145148378149),MAAC_FLT_C(26432.144678423778),MAAC_FLT_C(26449.146942172156),MAAC_FLT_C(26466.151938744493),
    MAAC_FLT_C(26483.159667262702),MAAC_FLT_C(26500.170126849403),MAAC_FLT_C(26517.183316627921),MAAC_FLT_C(26534.199235722277),
    MAAC_FLT_C(26551.217883257199),MAAC_FLT_C(26568.239258358120),MAAC_FLT_C(26585.263360151173),MAAC_FLT_C(26602.290187763181),
    MAAC_FLT_C(26619.319740321676),MAAC_FLT_C(26636.352016954883),MAAC_FLT_C(26653.387016791727),MAAC_FLT_C(26670.424738961825),
    MAAC_FLT_C(26687.465182595493),MAAC_FLT_C(26704.508346823739),MAAC_FLT_C(26721.554230778267),MAAC_FLT_C(26738.602833591467),
    MAAC_FLT_C(26755.654154396430),MAAC_FLT_C(26772.708192326929),MAAC_FLT_C(26789.764946517433),MAAC_FLT_C(26806.824416103096),
    MAAC_FLT_C(26823.886600219761),MAAC_FLT_C(26840.951498003960),MAAC_FLT_C(26858.019108592915),MAAC_FLT_C(26875.089431124517),
    MAAC_FLT_C(26892.162464737365),MAAC_FLT_C(26909.238208570721),MAAC_FLT_C(26926.316661764547),MAAC_FLT_C(26943.397823459472),
    MAAC_FLT_C(26960.481692796813),MAAC_FLT_C(26977.568268918574),MAAC_FLT_C(26994.657550967422),MAAC_FLT_C(27011.749538086722),
    MAAC_FLT_C(27028.844229420498),MAAC_FLT_C(27045.941624113464),MAAC_FLT_C(27063.041721311005),MAAC_FLT_C(27080.144520159181),
    MAAC_FLT_C(27097.250019804727),MAAC_FLT_C(27114.358219395050),MAAC_FLT_C(27131.469118078236),MAAC_FLT_C(27148.582715003031),
    MAAC_FLT_C(27165.699009318858),MAAC_FLT_C(27182.818000175816),MAAC_FLT_C(27199.939686724665),MAAC_FLT_C(27217.064068116837),
    MAAC_FLT_C(27234.191143504428),MAAC_FLT_C(27251.320912040203),MAAC_FLT_C(27268.453372877593),MAAC_FLT_C(27285.588525170693),
    MAAC_FLT_C(27302.726368074269),MAAC_FLT_C(27319.866900743735),MAAC_FLT_C(27337.010122335181),MAAC_FLT_C(27354.156032005358),
    MAAC_FLT_C(27371.304628911668),MAAC_FLT_C(27388.455912212183),MAAC_FLT_C(27405.609881065626),MAAC_FLT_C(27422.766534631388),
    MAAC_FLT_C(27439.925872069507),MAAC_FLT_C(27457.087892540683),MAAC_FLT_C(27474.252595206275),MAAC_FLT_C(27491.419979228293),
    MAAC_FLT_C(27508.590043769400),MAAC_FLT_C(27525.762787992917),MAAC_FLT_C(27542.938211062810),MAAC_FLT_C(27560.116312143706),
    MAAC_FLT_C(27577.297090400876),MAAC_FLT_C(27594.480545000242),MAAC_FLT_C(27611.666675108383),MAAC_FLT_C(27628.855479892518),
    MAAC_FLT_C(27646.046958520514),MAAC_FLT_C(27663.241110160889),MAAC_FLT_C(27680.437933982801),MAAC_FLT_C(27697.637429156068),
    MAAC_FLT_C(27714.839594851132),MAAC_FLT_C(27732.044430239090),MAAC_FLT_C(27749.251934491687),MAAC_FLT_C(27766.462106781299),
    MAAC_FLT_C(27783.674946280949),MAAC_FLT_C(27800.890452164302),MAAC_FLT_C(27818.108623605654),MAAC_FLT_C(27835.329459779954),
    MAAC_FLT_C(27852.552959862780),MAAC_FLT_C(27869.779123030345),MAAC_FLT_C(27887.007948459504),MAAC_FLT_C(27904.239435327745),
    MAAC_FLT_C(27921.473582813196),MAAC_FLT_C(27938.710390094613),MAAC_FLT_C(27955.949856351392),MAAC_FLT_C(27973.191980763550),
    MAAC_FLT_C(27990.436762511745),MAAC_FLT_C(28007.684200777272),MAAC_FLT_C(28024.934294742041),MAAC_FLT_C(28042.187043588601),
    MAAC_FLT_C(28059.442446500128),MAAC_FLT_C(28076.700502660427),MAAC_FLT_C(28093.961211253929),MAAC_FLT_C(28111.224571465693),
    MAAC_FLT_C(28128.490582481401),MAAC_FLT_C(28145.759243487362),MAAC_FLT_C(28163.030553670509),MAAC_FLT_C(28180.304512218394),
    MAAC_FLT_C(28197.581118319198),MAAC_FLT_C(28214.860371161725),MAAC_FLT_C(28232.142269935390),MAAC_FLT_C(28249.426813830240),
    MAAC_FLT_C(28266.714002036930),MAAC_FLT_C(28284.003833746745),MAAC_FLT_C(28301.296308151585),MAAC_FLT_C(28318.591424443959),
    MAAC_FLT_C(28335.889181817001),MAAC_FLT_C(28353.189579464462),MAAC_FLT_C(28370.492616580705),MAAC_FLT_C(28387.798292360701),
    MAAC_FLT_C(28405.106606000048),MAAC_FLT_C(28422.417556694945),MAAC_FLT_C(28439.731143642206),MAAC_FLT_C(28457.047366039264),
    MAAC_FLT_C(28474.366223084147),MAAC_FLT_C(28491.687713975512),MAAC_FLT_C(28509.011837912611),MAAC_FLT_C(28526.338594095305),
    MAAC_FLT_C(28543.667981724069),MAAC_FLT_C(28560.999999999985),MAAC_FLT_C(28578.334648124732),MAAC_FLT_C(28595.671925300605),
    MAAC_FLT_C(28613.011830730498),MAAC_FLT_C(28630.354363617909),MAAC_FLT_C(28647.699523166943),MAAC_FLT_C(28665.047308582300),
    MAAC_FLT_C(28682.397719069289),MAAC_FLT_C(28699.750753833818),MAAC_FLT_C(28717.106412082390),MAAC_FLT_C(28734.464693022121),
    MAAC_FLT_C(28751.825595860711),MAAC_FLT_C(28769.189119806462),MAAC_FLT_C(28786.555264068280),MAAC_FLT_C(28803.924027855664),
    MAAC_FLT_C(28821.295410378701),MAAC_FLT_C(28838.669410848088),MAAC_FLT_C(28856.046028475103),MAAC_FLT_C(28873.425262471628),
    MAAC_FLT_C(28890.807112050130),MAAC_FLT_C(28908.191576423673),MAAC_FLT_C(28925.578654805915),MAAC_FLT_C(28942.968346411097),
    MAAC_FLT_C(28960.360650454055),MAAC_FLT_C(28977.755566150216),MAAC_FLT_C(28995.153092715591),MAAC_FLT_C(29012.553229366786),
    MAAC_FLT_C(29029.955975320987),MAAC_FLT_C(29047.361329795975),MAAC_FLT_C(29064.769292010107),MAAC_FLT_C(29082.179861182336),
    MAAC_FLT_C(29099.593036532187),MAAC_FLT_C(29117.008817279780),MAAC_FLT_C(29134.427202645813),MAAC_FLT_C(29151.848191851572),
    MAAC_FLT_C(29169.271784118911),MAAC_FLT_C(29186.697978670283),MAAC_FLT_C(29204.126774728706),MAAC_FLT_C(29221.558171517790),
    MAAC_FLT_C(29238.992168261717),MAAC_FLT_C(29256.428764185250),MAAC_FLT_C(29273.867958513725),MAAC_FLT_C(29291.309750473058),
    MAAC_FLT_C(29308.754139289747),MAAC_FLT_C(29326.201124190855),MAAC_FLT_C(29343.650704404030),MAAC_FLT_C(29361.102879157483),
    MAAC_FLT_C(29378.557647680012),MAAC_FLT_C(29396.015009200975),MAAC_FLT_C(29413.474962950309),MAAC_FLT_C(29430.937508158524),
    MAAC_FLT_C(29448.402644056692),MAAC_FLT_C(29465.870369876469),MAAC_FLT_C(29483.340684850071),MAAC_FLT_C(29500.813588210280),
    MAAC_FLT_C(29518.289079190454),MAAC_FLT_C(29535.767157024511),MAAC_FLT_C(29553.247820946945),MAAC_FLT_C(29570.731070192807),
    MAAC_FLT_C(29588.216903997723),MAAC_FLT_C(29605.705321597870),MAAC_FLT_C(29623.196322230000),MAAC_FLT_C(29640.689905131429),
    MAAC_FLT_C(29658.186069540028),MAAC_FLT_C(29675.684814694236),MAAC_FLT_C(29693.186139833047),MAAC_FLT_C(29710.690044196028),
    MAAC_FLT_C(29728.196527023298),MAAC_FLT_C(29745.705587555527),MAAC_FLT_C(29763.217225033964),MAAC_FLT_C(29780.731438700397),
    MAAC_FLT_C(29798.248227797183),MAAC_FLT_C(29815.767591567230),MAAC_FLT_C(29833.289529254005),MAAC_FLT_C(29850.814040101530),
    MAAC_FLT_C(29868.341123354381),MAAC_FLT_C(29885.870778257693),MAAC_FLT_C(29903.403004057145),MAAC_FLT_C(29920.937799998974),
    MAAC_FLT_C(29938.475165329975),MAAC_FLT_C(29956.015099297481),MAAC_FLT_C(29973.557601149394),MAAC_FLT_C(29991.102670134147),
    MAAC_FLT_C(30008.650305500742),MAAC_FLT_C(30026.200506498710),MAAC_FLT_C(30043.753272378144),MAAC_FLT_C(30061.308602389683),
    MAAC_FLT_C(30078.866495784507),MAAC_FLT_C(30096.426951814352),MAAC_FLT_C(30113.989969731494),MAAC_FLT_C(30131.555548788750),
    MAAC_FLT_C(30149.123688239491),MAAC_FLT_C(30166.694387337629),MAAC_FLT_C(30184.267645337608),MAAC_FLT_C(30201.843461494434),
    MAAC_FLT_C(30219.421835063640),MAAC_FLT_C(30237.002765301309),MAAC_FLT_C(30254.586251464058),MAAC_FLT_C(30272.172292809046),
    MAAC_FLT_C(30289.760888593977),MAAC_FLT_C(30307.352038077090),MAAC_FLT_C(30324.945740517160),MAAC_FLT_C(30342.541995173502),
    MAAC_FLT_C(30360.140801305966),MAAC_FLT_C(30377.742158174944),MAAC_FLT_C(30395.346065041358),MAAC_FLT_C(30412.952521166666),
    MAAC_FLT_C(30430.561525812864),MAAC_FLT_C(30448.173078242475),MAAC_FLT_C(30465.787177718561),MAAC_FLT_C(30483.403823504719),
    MAAC_FLT_C(30501.023014865070),MAAC_FLT_C(30518.644751064272),MAAC_FLT_C(30536.269031367516),MAAC_FLT_C(30553.895855040515),
    MAAC_FLT_C(30571.525221349519),MAAC_FLT_C(30589.157129561307),MAAC_FLT_C(30606.791578943175),MAAC_FLT_C(30624.428568762964),
    MAAC_FLT_C(30642.068098289030),MAAC_FLT_C(30659.710166790261),MAAC_FLT_C(30677.354773536070),MAAC_FLT_C(30695.001917796391),
    MAAC_FLT_C(30712.651598841687),MAAC_FLT_C(30730.303815942945),MAAC_FLT_C(30747.958568371676),MAAC_FLT_C(30765.615855399912),
    MAAC_FLT_C(30783.275676300211),MAAC_FLT_C(30800.938030345646),MAAC_FLT_C(30818.602916809814),MAAC_FLT_C(30836.270334966837),
    MAAC_FLT_C(30853.940284091354),MAAC_FLT_C(30871.612763458521),MAAC_FLT_C(30889.287772344011),MAAC_FLT_C(30906.965310024025),
    MAAC_FLT_C(30924.645375775272),MAAC_FLT_C(30942.327968874983),MAAC_FLT_C(30960.013088600903),MAAC_FLT_C(30977.700734231294),
    MAAC_FLT_C(30995.390905044929),MAAC_FLT_C(31013.083600321101),MAAC_FLT_C(31030.778819339619),MAAC_FLT_C(31048.476561380798),
    MAAC_FLT_C(31066.176825725470),MAAC_FLT_C(31083.879611654978),MAAC_FLT_C(31101.584918451179),MAAC_FLT_C(31119.292745396440),
    MAAC_FLT_C(31137.003091773637),MAAC_FLT_C(31154.715956866155),MAAC_FLT_C(31172.431339957893),MAAC_FLT_C(31190.149240333260),
    MAAC_FLT_C(31207.869657277162),MAAC_FLT_C(31225.592590075023),MAAC_FLT_C(31243.318038012771),MAAC_FLT_C(31261.046000376838),
    MAAC_FLT_C(31278.776476454172),MAAC_FLT_C(31296.509465532210),MAAC_FLT_C(31314.244966898910),MAAC_FLT_C(31331.982979842720),
    MAAC_FLT_C(31349.723503652600),MAAC_FLT_C(31367.466537618013),MAAC_FLT_C(31385.212081028923),MAAC_FLT_C(31402.960133175795),
    MAAC_FLT_C(31420.710693349596),MAAC_FLT_C(31438.463760841791),MAAC_FLT_C(31456.219334944351),MAAC_FLT_C(31473.977414949743),
    MAAC_FLT_C(31491.738000150934),MAAC_FLT_C(31509.501089841389),MAAC_FLT_C(31527.266683315069),MAAC_FLT_C(31545.034779866437),
    MAAC_FLT_C(31562.805378790450),MAAC_FLT_C(31580.578479382562),MAAC_FLT_C(31598.354080938720),MAAC_FLT_C(31616.132182755369),
    MAAC_FLT_C(31633.912784129450),MAAC_FLT_C(31651.695884358396),MAAC_FLT_C(31669.481482740131),MAAC_FLT_C(31687.269578573076),
    MAAC_FLT_C(31705.060171156143),MAAC_FLT_C(31722.853259788735),MAAC_FLT_C(31740.648843770748),MAAC_FLT_C(31758.446922402567),
    MAAC_FLT_C(31776.247494985066),MAAC_FLT_C(31794.050560819614),MAAC_FLT_C(31811.856119208060),MAAC_FLT_C(31829.664169452753),
    MAAC_FLT_C(31847.474710856521),MAAC_FLT_C(31865.287742722685),MAAC_FLT_C(31883.103264355046),MAAC_FLT_C(31900.921275057899),
    MAAC_FLT_C(31918.741774136019),MAAC_FLT_C(31936.564760894671),MAAC_FLT_C(31954.390234639599),MAAC_FLT_C(31972.218194677040),
    MAAC_FLT_C(31990.048640313704),MAAC_FLT_C(32007.881570856793),MAAC_FLT_C(32025.716985613984),MAAC_FLT_C(32043.554883893445),
    MAAC_FLT_C(32061.395265003815),MAAC_FLT_C(32079.238128254223),MAAC_FLT_C(32097.083472954269),MAAC_FLT_C(32114.931298414049),
    MAAC_FLT_C(32132.781603944117),MAAC_FLT_C(32150.634388855524),MAAC_FLT_C(32168.489652459790),MAAC_FLT_C(32186.347394068915),
    MAAC_FLT_C(32204.207612995371),MAAC_FLT_C(32222.070308552120),MAAC_FLT_C(32239.935480052583),MAAC_FLT_C(32257.803126810672),
    MAAC_FLT_C(32275.673248140767),MAAC_FLT_C(32293.545843357719),MAAC_FLT_C(32311.420911776862),MAAC_FLT_C(32329.298452713996),
    MAAC_FLT_C(32347.178465485395),MAAC_FLT_C(32365.060949407813),MAAC_FLT_C(32382.945903798463),MAAC_FLT_C(32400.833327975040),
    MAAC_FLT_C(32418.723221255706),MAAC_FLT_C(32436.615582959093),MAAC_FLT_C(32454.510412404306),MAAC_FLT_C(32472.407708910916),
    MAAC_FLT_C(32490.307471798966),MAAC_FLT_C(32508.209700388961),MAAC_FLT_C(32526.114394001877),MAAC_FLT_C(32544.021551959166),
    MAAC_FLT_C(32561.931173582732),MAAC_FLT_C(32579.843258194956),MAAC_FLT_C(32597.757805118679),MAAC_FLT_C(32615.674813677211),
    MAAC_FLT_C(32633.594283194328),MAAC_FLT_C(32651.516212994258),MAAC_FLT_C(32669.440602401712),MAAC_FLT_C(32687.367450741847),
    MAAC_FLT_C(32705.296757340297),MAAC_FLT_C(32723.228521523146),MAAC_FLT_C(32741.162742616943),MAAC_FLT_C(32759.099419948703),
    MAAC_FLT_C(32777.038552845901),MAAC_FLT_C(32794.980140636464),MAAC_FLT_C(32812.924182648792),MAAC_FLT_C(32830.870678211730),
    MAAC_FLT_C(32848.819626654593),MAAC_FLT_C(32866.771027307150),MAAC_FLT_C(32884.724879499619),MAAC_FLT_C(32902.681182562686),
    MAAC_FLT_C(32920.639935827494),MAAC_FLT_C(32938.601138625643),MAAC_FLT_C(32956.564790289180),MAAC_FLT_C(32974.530890150607),
    MAAC_FLT_C(32992.499437542894),MAAC_FLT_C(33010.470431799447),MAAC_FLT_C(33028.443872254145),MAAC_FLT_C(33046.419758241311),
    MAAC_FLT_C(33064.398089095710),MAAC_FLT_C(33082.378864152583),MAAC_FLT_C(33100.362082747590),MAAC_FLT_C(33118.347744216881),
    MAAC_FLT_C(33136.335847897026),MAAC_FLT_C(33154.326393125062),MAAC_FLT_C(33172.319379238470),MAAC_FLT_C(33190.314805575174),
    MAAC_FLT_C(33208.312671473555),MAAC_FLT_C(33226.312976272442),MAAC_FLT_C(33244.315719311111),MAAC_FLT_C(33262.320899929284),
    MAAC_FLT_C(33280.328517467125),MAAC_FLT_C(33298.338571265260),MAAC_FLT_C(33316.351060664747),MAAC_FLT_C(33334.365985007091),
    MAAC_FLT_C(33352.383343634239),MAAC_FLT_C(33370.403135888591),MAAC_FLT_C(33388.425361112990),MAAC_FLT_C(33406.450018650721),
    MAAC_FLT_C(33424.477107845501),MAAC_FLT_C(33442.506628041512),MAAC_FLT_C(33460.538578583350),MAAC_FLT_C(33478.572958816083),
    MAAC_FLT_C(33496.609768085189),MAAC_FLT_C(33514.649005736617),MAAC_FLT_C(33532.690671116739),MAAC_FLT_C(33550.734763572356),
    MAAC_FLT_C(33568.781282450735),MAAC_FLT_C(33586.830227099563),MAAC_FLT_C(33604.881596866973),MAAC_FLT_C(33622.935391101528),
    MAAC_FLT_C(33640.991609152239),MAAC_FLT_C(33659.050250368542),MAAC_FLT_C(33677.111314100322),MAAC_FLT_C(33695.174799697881),
    MAAC_FLT_C(33713.240706511984),MAAC_FLT_C(33731.309033893805),MAAC_FLT_C(33749.379781194970),MAAC_FLT_C(33767.452947767531),
    MAAC_FLT_C(33785.528532963974),MAAC_FLT_C(33803.606536137209),MAAC_FLT_C(33821.686956640602),MAAC_FLT_C(33839.769793827938),
    MAAC_FLT_C(33857.855047053425),MAAC_FLT_C(33875.942715671707),MAAC_FLT_C(33894.032799037872),MAAC_FLT_C(33912.125296507431),
    MAAC_FLT_C(33930.220207436316),MAAC_FLT_C(33948.317531180888),MAAC_FLT_C(33966.417267097961),MAAC_FLT_C(33984.519414544746),
    MAAC_FLT_C(34002.623972878901),MAAC_FLT_C(34020.730941458511),MAAC_FLT_C(34038.840319642077),MAAC_FLT_C(34056.952106788536),
    MAAC_FLT_C(34075.066302257255),MAAC_FLT_C(34093.182905408015),MAAC_FLT_C(34111.301915601027),MAAC_FLT_C(34129.423332196930),
    MAAC_FLT_C(34147.547154556785),MAAC_FLT_C(34165.673382042078),MAAC_FLT_C(34183.802014014720),MAAC_FLT_C(34201.933049837033),
    MAAC_FLT_C(34220.066488871780),MAAC_FLT_C(34238.202330482141),MAAC_FLT_C(34256.340574031703),MAAC_FLT_C(34274.481218884495),
    MAAC_FLT_C(34292.624264404949),MAAC_FLT_C(34310.769709957938),MAAC_FLT_C(34328.917554908730),MAAC_FLT_C(34347.067798623029),
    MAAC_FLT_C(34365.220440466954),MAAC_FLT_C(34383.375479807051),MAAC_FLT_C(34401.532916010263),MAAC_FLT_C(34419.692748443973),
    MAAC_FLT_C(34437.854976475966),MAAC_FLT_C(34456.019599474450),MAAC_FLT_C(34474.186616808060),MAAC_FLT_C(34492.356027845817),
    MAAC_FLT_C(34510.527831957188),MAAC_FLT_C(34528.702028512052),MAAC_FLT_C(34546.878616880676),MAAC_FLT_C(34565.057596433770),
    MAAC_FLT_C(34583.238966542449),MAAC_FLT_C(34601.422726578232),MAAC_FLT_C(34619.608875913065),MAAC_FLT_C(34637.797413919296),
    MAAC_FLT_C(34655.988339969692),MAAC_FLT_C(34674.181653437423),MAAC_FLT_C(34692.377353696080),MAAC_FLT_C(34710.575440119668),
    MAAC_FLT_C(34728.775912082579),MAAC_FLT_C(34746.978768959649),MAAC_FLT_C(34765.184010126082),MAAC_FLT_C(34783.391634957537),
    MAAC_FLT_C(34801.601642830050),MAAC_FLT_C(34819.814033120063),MAAC_FLT_C(34838.028805204456),MAAC_FLT_C(34856.245958460480),
    MAAC_FLT_C(34874.465492265823),MAAC_FLT_C(34892.687405998557),MAAC_FLT_C(34910.911699037177),MAAC_FLT_C(34929.138370760564),
    MAAC_FLT_C(34947.367420548027),MAAC_FLT_C(34965.598847779271),MAAC_FLT_C(34983.832651834389),MAAC_FLT_C(35002.068832093908),
    MAAC_FLT_C(35020.307387938738),MAAC_FLT_C(35038.548318750189),MAAC_FLT_C(35056.791623909980),MAAC_FLT_C(35075.037302800250),
    MAAC_FLT_C(35093.285354803513),MAAC_FLT_C(35111.535779302685),MAAC_FLT_C(35129.788575681116),MAAC_FLT_C(35148.043743322516),
    MAAC_FLT_C(35166.301281611013),MAAC_FLT_C(35184.561189931141),MAAC_FLT_C(35202.823467667826),MAAC_FLT_C(35221.088114206388),
    MAAC_FLT_C(35239.355128932555),MAAC_FLT_C(35257.624511232447),MAAC_FLT_C(35275.896260492584),MAAC_FLT_C(35294.170376099886),
    MAAC_FLT_C(35312.446857441668),MAAC_FLT_C(35330.725703905628),MAAC_FLT_C(35349.006914879887),MAAC_FLT_C(35367.290489752944),
    MAAC_FLT_C(35385.576427913686),MAAC_FLT_C(35403.864728751418),MAAC_FLT_C(35422.155391655811),MAAC_FLT_C(35440.448416016967),
    MAAC_FLT_C(35458.743801225341),MAAC_FLT_C(35477.041546671804),MAAC_FLT_C(35495.341651747622),MAAC_FLT_C(35513.644115844436),
    MAAC_FLT_C(35531.948938354304),MAAC_FLT_C(35550.256118669655),MAAC_FLT_C(35568.565656183309),MAAC_FLT_C(35586.877550288496),
    MAAC_FLT_C(35605.191800378816),MAAC_FLT_C(35623.508405848268),MAAC_FLT_C(35641.827366091238),MAAC_FLT_C(35660.148680502505),
    MAAC_FLT_C(35678.472348477233),MAAC_FLT_C(35696.798369410979),MAAC_FLT_C(35715.126742699678),MAAC_FLT_C(35733.457467739659),
    MAAC_FLT_C(35751.790543927644),MAAC_FLT_C(35770.125970660738),MAAC_FLT_C(35788.463747336420),MAAC_FLT_C(35806.803873352568),
    MAAC_FLT_C(35825.146348107453),MAAC_FLT_C(35843.491170999710),MAAC_FLT_C(35861.838341428367),MAAC_FLT_C(35880.187858792851),
    MAAC_FLT_C(35898.539722492955),MAAC_FLT_C(35916.893931928862),MAAC_FLT_C(35935.250486501129),MAAC_FLT_C(35953.609385610718),
    MAAC_FLT_C(35971.970628658957),MAAC_FLT_C(35990.334215047558),MAAC_FLT_C(36008.700144178612),MAAC_FLT_C(36027.068415454596),
    MAAC_FLT_C(36045.439028278372),MAAC_FLT_C(36063.811982053165),MAAC_FLT_C(36082.187276182609),MAAC_FLT_C(36100.564910070694),
    MAAC_FLT_C(36118.944883121789),MAAC_FLT_C(36137.327194740654),MAAC_FLT_C(36155.711844332429),MAAC_FLT_C(36174.098831302617),
    MAAC_FLT_C(36192.488155057115),MAAC_FLT_C(36210.879815002190),MAAC_FLT_C(36229.273810544473),MAAC_FLT_C(36247.670141091003),
    MAAC_FLT_C(36266.068806049167),MAAC_FLT_C(36284.469804826738),MAAC_FLT_C(36302.873136831862),MAAC_FLT_C(36321.278801473069),
    MAAC_FLT_C(36339.686798159251),MAAC_FLT_C(36358.097126299683),MAAC_FLT_C(36376.509785304013),MAAC_FLT_C(36394.924774582265),
    MAAC_FLT_C(36413.342093544816),MAAC_FLT_C(36431.761741602444),MAAC_FLT_C(36450.183718166292),MAAC_FLT_C(36468.608022647859),
    MAAC_FLT_C(36487.034654459028),MAAC_FLT_C(36505.463613012063),MAAC_FLT_C(36523.894897719583),MAAC_FLT_C(36542.328507994578),
    MAAC_FLT_C(36560.764443250409),MAAC_FLT_C(36579.202702900831),MAAC_FLT_C(36597.643286359926),MAAC_FLT_C(36616.086193042182),
    MAAC_FLT_C(36634.531422362437),MAAC_FLT_C(36652.978973735895),MAAC_FLT_C(36671.428846578143),MAAC_FLT_C(36689.881040305125),
    MAAC_FLT_C(36708.335554333149),MAAC_FLT_C(36726.792388078902),MAAC_FLT_C(36745.251540959427),MAAC_FLT_C(36763.713012392138),
    MAAC_FLT_C(36782.176801794812),MAAC_FLT_C(36800.642908585593),MAAC_FLT_C(36819.111332182991),MAAC_FLT_C(36837.582072005869),
    MAAC_FLT_C(36856.055127473483),MAAC_FLT_C(36874.530498005421),MAAC_FLT_C(36893.008183021651),MAAC_FLT_C(36911.488181942506),
    MAAC_FLT_C(36929.970494188674),MAAC_FLT_C(36948.455119181206),MAAC_FLT_C(36966.942056341519),MAAC_FLT_C(36985.431305091392),
    MAAC_FLT_C(37003.922864852961),MAAC_FLT_C(37022.416735048733),MAAC_FLT_C(37040.912915101559),MAAC_FLT_C(37059.411404434657),
    MAAC_FLT_C(37077.912202471620),MAAC_FLT_C(37096.415308636388),MAAC_FLT_C(37114.920722353243),MAAC_FLT_C(37133.428443046862),
    MAAC_FLT_C(37151.938470142253),MAAC_FLT_C(37170.450803064785),MAAC_FLT_C(37188.965441240209),MAAC_FLT_C(37207.482384094597),
    MAAC_FLT_C(37226.001631054402),MAAC_FLT_C(37244.523181546429),MAAC_FLT_C(37263.047034997842),MAAC_FLT_C(37281.573190836149),
    MAAC_FLT_C(37300.101648489224),MAAC_FLT_C(37318.632407385296),MAAC_FLT_C(37337.165466952945),MAAC_FLT_C(37355.700826621112),
    MAAC_FLT_C(37374.238485819085),MAAC_FLT_C(37392.778443976509),MAAC_FLT_C(37411.320700523385),MAAC_FLT_C(37429.865254890057),
    MAAC_FLT_C(37448.412106507232),MAAC_FLT_C(37466.961254805967),MAAC_FLT_C(37485.512699217681),MAAC_FLT_C(37504.066439174116),
    MAAC_FLT_C(37522.622474107404),MAAC_FLT_C(37541.180803449992),MAAC_FLT_C(37559.741426634697),MAAC_FLT_C(37578.304343094693),
    MAAC_FLT_C(37596.869552263488),MAAC_FLT_C(37615.437053574940),MAAC_FLT_C(37634.006846463271),MAAC_FLT_C(37652.578930363044),
    MAAC_FLT_C(37671.153304709165),MAAC_FLT_C(37689.729968936896),MAAC_FLT_C(37708.308922481847),MAAC_FLT_C(37726.890164779965),
    MAAC_FLT_C(37745.473695267559),MAAC_FLT_C(37764.059513381275),MAAC_FLT_C(37782.647618558112),MAAC_FLT_C(37801.238010235415),
    MAAC_FLT_C(37819.830687850859),MAAC_FLT_C(37838.425650842495),MAAC_FLT_C(37857.022898648691),MAAC_FLT_C(37875.622430708172),
    MAAC_FLT_C(37894.224246460013),MAAC_FLT_C(37912.828345343616),MAAC_FLT_C(37931.434726798747),MAAC_FLT_C(37950.043390265506),
    MAAC_FLT_C(37968.654335184328),MAAC_FLT_C(37987.267560995999),MAAC_FLT_C(38005.883067141665),MAAC_FLT_C(38024.500853062775),
    MAAC_FLT_C(38043.120918201159),MAAC_FLT_C(38061.743261998963),MAAC_FLT_C(38080.367883898682),MAAC_FLT_C(38098.994783343158),
    MAAC_FLT_C(38117.623959775563),MAAC_FLT_C(38136.255412639417),MAAC_FLT_C(38154.889141378575),MAAC_FLT_C(38173.525145437234),
    MAAC_FLT_C(38192.163424259939),MAAC_FLT_C(38210.803977291551),MAAC_FLT_C(38229.446803977284),MAAC_FLT_C(38248.091903762703),
    MAAC_FLT_C(38266.739276093685),MAAC_FLT_C(38285.388920416466),MAAC_FLT_C(38304.040836177606),MAAC_FLT_C(38322.695022824002),
    MAAC_FLT_C(38341.351479802899),MAAC_FLT_C(38360.010206561863),MAAC_FLT_C(38378.671202548816),MAAC_FLT_C(38397.334467211993),
    MAAC_FLT_C(38415.999999999978),MAAC_FLT_C(38434.667800361683),MAAC_FLT_C(38453.337867746370),MAAC_FLT_C(38472.010201603611),
    MAAC_FLT_C(38490.684801383337),MAAC_FLT_C(38509.361666535784),MAAC_FLT_C(38528.040796511552),MAAC_FLT_C(38546.722190761553),
    MAAC_FLT_C(38565.405848737035),MAAC_FLT_C(38584.091769889594),MAAC_FLT_C(38602.779953671132),MAAC_FLT_C(38621.470399533908),
    MAAC_FLT_C(38640.163106930486),MAAC_FLT_C(38658.858075313794),MAAC_FLT_C(38677.555304137059),MAAC_FLT_C(38696.254792853862),
    MAAC_FLT_C(38714.956540918094),MAAC_FLT_C(38733.660547783991),MAAC_FLT_C(38752.366812906112),MAAC_FLT_C(38771.075335739348),
    MAAC_FLT_C(38789.786115738920),MAAC_FLT_C(38808.499152360368),MAAC_FLT_C(38827.214445059573),MAAC_FLT_C(38845.931993292739),
    MAAC_FLT_C(38864.651796516388),MAAC_FLT_C(38883.373854187383),MAAC_FLT_C(38902.098165762916),MAAC_FLT_C(38920.824730700486),
    MAAC_FLT_C(38939.553548457938),MAAC_FLT_C(38958.284618493431),MAAC_FLT_C(38977.017940265461),MAAC_FLT_C(38995.753513232834),
    MAAC_FLT_C(39014.491336854699),MAAC_FLT_C(39033.231410590517),MAAC_FLT_C(39051.973733900079),MAAC_FLT_C(39070.718306243485),
    MAAC_FLT_C(39089.465127081188),MAAC_FLT_C(39108.214195873945),MAAC_FLT_C(39126.965512082832),MAAC_FLT_C(39145.719075169261),
    MAAC_FLT_C(39164.474884594965),MAAC_FLT_C(39183.232939821988),MAAC_FLT_C(39201.993240312710),MAAC_FLT_C(39220.755785529815),
    MAAC_FLT_C(39239.520574936330),MAAC_FLT_C(39258.287607995589),MAAC_FLT_C(39277.056884171245),MAAC_FLT_C(39295.828402927291),
    MAAC_FLT_C(39314.602163728006),MAAC_FLT_C(39333.378166038019),MAAC_FLT_C(39352.156409322270),MAAC_FLT_C(39370.936893046004),
    MAAC_FLT_C(39389.719616674811),MAAC_FLT_C(39408.504579674584),MAAC_FLT_C(39427.291781511522),MAAC_FLT_C(39446.081221652174),
    MAAC_FLT_C(39464.872899563372),MAAC_FLT_C(39483.666814712291),MAAC_FLT_C(39502.462966566411),MAAC_FLT_C(39521.261354593538),
    MAAC_FLT_C(39540.061978261780),MAAC_FLT_C(39558.864837039568),MAAC_FLT_C(39577.669930395656),MAAC_FLT_C(39596.477257799110),
    MAAC_FLT_C(39615.286818719302),MAAC_FLT_C(39634.098612625923),MAAC_FLT_C(39652.912638988993),MAAC_FLT_C(39671.728897278823),
    MAAC_FLT_C(39690.547386966064),MAAC_FLT_C(39709.368107521652),MAAC_FLT_C(39728.191058416858),MAAC_FLT_C(39747.016239123259),
    MAAC_FLT_C(39765.843649112750),MAAC_FLT_C(39784.673287857528),MAAC_FLT_C(39803.505154830105),MAAC_FLT_C(39822.339249503319),
    MAAC_FLT_C(39841.175571350293),MAAC_FLT_C(39860.014119844498),MAAC_FLT_C(39878.854894459677),MAAC_FLT_C(39897.697894669909),
    MAAC_FLT_C(39916.543119949580),MAAC_FLT_C(39935.390569773372),MAAC_FLT_C(39954.240243616303),MAAC_FLT_C(39973.092140953675),
    MAAC_FLT_C(39991.946261261117),MAAC_FLT_C(40010.802604014549),MAAC_FLT_C(40029.661168690225),MAAC_FLT_C(40048.521954764678),
    MAAC_FLT_C(40067.384961714779),MAAC_FLT_C(40086.250189017679),MAAC_FLT_C(40105.117636150855),MAAC_FLT_C(40123.987302592090),
    MAAC_FLT_C(40142.859187819471),MAAC_FLT_C(40161.733291311379),MAAC_FLT_C(40180.609612546526),MAAC_FLT_C(40199.488151003912),
    MAAC_FLT_C(40218.368906162854),MAAC_FLT_C(40237.251877502960),MAAC_FLT_C(40256.137064504153),MAAC_FLT_C(40275.024466646668),
    MAAC_FLT_C(40293.914083411029),MAAC_FLT_C(40312.805914278084),MAAC_FLT_C(40331.699958728961),MAAC_FLT_C(40350.596216245103),
    MAAC_FLT_C(40369.494686308273),MAAC_FLT_C(40388.395368400510),MAAC_FLT_C(40407.298262004173),MAAC_FLT_C(40426.203366601920),
    MAAC_FLT_C(40445.110681676706),MAAC_FLT_C(40464.020206711793),MAAC_FLT_C(40482.931941190756),MAAC_FLT_C(40501.845884597446),
    MAAC_FLT_C(40520.762036416032),MAAC_FLT_C(40539.680396130985),MAAC_FLT_C(40558.600963227072),MAAC_FLT_C(40577.523737189367),
    MAAC_FLT_C(40596.448717503234),MAAC_FLT_C(40615.375903654342),MAAC_FLT_C(40634.305295128659),MAAC_FLT_C(40653.236891412453),
    MAAC_FLT_C(40672.170691992294),MAAC_FLT_C(40691.106696355047),MAAC_FLT_C(40710.044903987873),MAAC_FLT_C(40728.985314378238),
    MAAC_FLT_C(40747.927927013901),MAAC_FLT_C(40766.872741382918),MAAC_FLT_C(40785.819756973651),MAAC_FLT_C(40804.768973274746),
    MAAC_FLT_C(40823.720389775161),MAAC_FLT_C(40842.674005964131),MAAC_FLT_C(40861.629821331211),MAAC_FLT_C(40880.587835366226),
    MAAC_FLT_C(40899.548047559329),MAAC_FLT_C(40918.510457400931),MAAC_FLT_C(40937.475064381761),MAAC_FLT_C(40956.441867992849),
    MAAC_FLT_C(40975.410867725499),MAAC_FLT_C(40994.382063071324),MAAC_FLT_C(41013.355453522236),MAAC_FLT_C(41032.331038570417),
    MAAC_FLT_C(41051.308817708363),MAAC_FLT_C(41070.288790428858),MAAC_FLT_C(41089.270956224987),MAAC_FLT_C(41108.255314590111),
    MAAC_FLT_C(41127.241865017888),MAAC_FLT_C(41146.230607002290),MAAC_FLT_C(41165.221540037543),MAAC_FLT_C(41184.214663618193),
    MAAC_FLT_C(41203.209977239079),MAAC_FLT_C(41222.207480395307),MAAC_FLT_C(41241.207172582297),MAAC_FLT_C(41260.209053295752),
    MAAC_FLT_C(41279.213122031659),MAAC_FLT_C(41298.219378286303),MAAC_FLT_C(41317.227821556255),MAAC_FLT_C(41336.238451338380),
    MAAC_FLT_C(41355.251267129832),MAAC_FLT_C(41374.266268428037),MAAC_FLT_C(41393.283454730743),MAAC_FLT_C(41412.302825535953),
    MAAC_FLT_C(41431.324380341983),MAAC_FLT_C(41450.348118647416),MAAC_FLT_C(41469.374039951144),MAAC_FLT_C(41488.402143752326),
    MAAC_FLT_C(41507.432429550427),MAAC_FLT_C(41526.464896845187),MAAC_FLT_C(41545.499545136627),MAAC_FLT_C(41564.536373925075),
    MAAC_FLT_C(41583.575382711126),MAAC_FLT_C(41602.616570995662),MAAC_FLT_C(41621.659938279874),MAAC_FLT_C(41640.705484065205),
    MAAC_FLT_C(41659.753207853406),MAAC_FLT_C(41678.803109146495),MAAC_FLT_C(41697.855187446803),MAAC_FLT_C(41716.909442256911),
    MAAC_FLT_C(41735.965873079709),MAAC_FLT_C(41755.024479418360),MAAC_FLT_C(41774.085260776315),MAAC_FLT_C(41793.148216657297),
    MAAC_FLT_C(41812.213346565331),MAAC_FLT_C(41831.280650004708),MAAC_FLT_C(41850.350126480014),MAAC_FLT_C(41869.421775496106),
    MAAC_FLT_C(41888.495596558132),MAAC_FLT_C(41907.571589171515),MAAC_FLT_C(41926.649752841957),MAAC_FLT_C(41945.730087075463),
    MAAC_FLT_C(41964.812591378286),MAAC_FLT_C(41983.897265256979),MAAC_FLT_C(42002.984108218378),MAAC_FLT_C(42022.073119769593),
    MAAC_FLT_C(42041.164299418007),MAAC_FLT_C(42060.257646671307),MAAC_FLT_C(42079.353161037419),MAAC_FLT_C(42098.450842024591),
    MAAC_FLT_C(42117.550689141324),MAAC_FLT_C(42136.652701896404),MAAC_FLT_C(42155.756879798893),MAAC_FLT_C(42174.863222358137),
    MAAC_FLT_C(42193.971729083758),MAAC_FLT_C(42213.082399485655),MAAC_FLT_C(42232.195233074002),MAAC_FLT_C(42251.310229359246),
    MAAC_FLT_C(42270.427387852127),MAAC_FLT_C(42289.546708063644),MAAC_FLT_C(42308.668189505079),MAAC_FLT_C(42327.791831687995),
    MAAC_FLT_C(42346.917634124227),MAAC_FLT_C(42366.045596325886),MAAC_FLT_C(42385.175717805352),MAAC_FLT_C(42404.307998075295),
    MAAC_FLT_C(42423.442436648642),MAAC_FLT_C(42442.579033038608),MAAC_FLT_C(42461.717786758672),MAAC_FLT_C(42480.858697322597),
    MAAC_FLT_C(42500.001764244422),MAAC_FLT_C(42519.146987038446),MAAC_FLT_C(42538.294365219248),MAAC_FLT_C(42557.443898301688),
    MAAC_FLT_C(42576.595585800882),MAAC_FLT_C(42595.749427232236),MAAC_FLT_C(42614.905422111420),MAAC_FLT_C(42634.063569954371),
    MAAC_FLT_C(42653.223870277317),MAAC_FLT_C(42672.386322596729),MAAC_FLT_C(42691.550926429380),MAAC_FLT_C(42710.717681292292),
    MAAC_FLT_C(42729.886586702763),MAAC_FLT_C(42749.057642178363),MAAC_FLT_C(42768.230847236940),MAAC_FLT_C(42787.406201396610),
    MAAC_FLT_C(42806.583704175740),MAAC_FLT_C(42825.763355092990),MAAC_FLT_C(42844.945153667286),MAAC_FLT_C(42864.129099417805),
    MAAC_FLT_C(42883.315191864014),MAAC_FLT_C(42902.503430525649),MAAC_FLT_C(42921.693814922692),MAAC_FLT_C(42940.886344575410),
    MAAC_FLT_C(42960.081019004348),MAAC_FLT_C(42979.277837730297),MAAC_FLT_C(42998.476800274322),MAAC_FLT_C(43017.677906157769),
    MAAC_FLT_C(43036.881154902228),MAAC_FLT_C(43056.086546029583),MAAC_FLT_C(43075.294079061961),MAAC_FLT_C(43094.503753521763),
    MAAC_FLT_C(43113.715568931664),MAAC_FLT_C(43132.929524814601),MAAC_FLT_C(43152.145620693766),MAAC_FLT_C(43171.363856092619),
    MAAC_FLT_C(43190.584230534907),MAAC_FLT_C(43209.806743544621),MAAC_FLT_C(43229.031394646016),MAAC_FLT_C(43248.258183363621),
    MAAC_FLT_C(43267.487109222224),MAAC_FLT_C(43286.718171746885),MAAC_FLT_C(43305.951370462906),MAAC_FLT_C(43325.186704895881),
    MAAC_FLT_C(43344.424174571650),MAAC_FLT_C(43363.663779016322),MAAC_FLT_C(43382.905517756262),MAAC_FLT_C(43402.149390318104),
    MAAC_FLT_C(43421.395396228749),MAAC_FLT_C(43440.643535015348),MAAC_FLT_C(43459.893806205320),MAAC_FLT_C(43479.146209326347),
    MAAC_FLT_C(43498.400743906379),MAAC_FLT_C(43517.657409473606),MAAC_FLT_C(43536.916205556496),MAAC_FLT_C(43556.177131683784),
    MAAC_FLT_C(43575.440187384440),MAAC_FLT_C(43594.705372187724),MAAC_FLT_C(43613.972685623135),MAAC_FLT_C(43633.242127220437),
    MAAC_FLT_C(43652.513696509668),MAAC_FLT_C(43671.787393021099),MAAC_FLT_C(43691.063216285271),MAAC_FLT_C(43710.341165833001),
    MAAC_FLT_C(43729.621241195346),MAAC_FLT_C(43748.903441903625),MAAC_FLT_C(43768.187767489413),MAAC_FLT_C(43787.474217484552),
    MAAC_FLT_C(43806.762791421126),MAAC_FLT_C(43826.053488831501),MAAC_FLT_C(43845.346309248278),MAAC_FLT_C(43864.641252204325),
    MAAC_FLT_C(43883.938317232765),MAAC_FLT_C(43903.237503866978),MAAC_FLT_C(43922.538811640596),MAAC_FLT_C(43941.842240087513),
    MAAC_FLT_C(43961.147788741881),MAAC_FLT_C(43980.455457138101),MAAC_FLT_C(43999.765244810835),MAAC_FLT_C(44019.077151295001),
    MAAC_FLT_C(44038.391176125755),MAAC_FLT_C(44057.707318838540),MAAC_FLT_C(44077.025578969020),MAAC_FLT_C(44096.345956053141),
    MAAC_FLT_C(44115.668449627083),MAAC_FLT_C(44134.993059227287),MAAC_FLT_C(44154.319784390456),MAAC_FLT_C(44173.648624653535),
    MAAC_FLT_C(44192.979579553728),MAAC_FLT_C(44212.312648628489),MAAC_FLT_C(44231.647831415532),MAAC_FLT_C(44250.985127452805),
    MAAC_FLT_C(44270.324536278538),MAAC_FLT_C(44289.666057431183),MAAC_FLT_C(44309.009690449464),MAAC_FLT_C(44328.355434872356),
    MAAC_FLT_C(44347.703290239064),MAAC_FLT_C(44367.053256089079),MAAC_FLT_C(44386.405331962109),MAAC_FLT_C(44405.759517398139),
    MAAC_FLT_C(44425.115811937387),MAAC_FLT_C(44444.474215120332),MAAC_FLT_C(44463.834726487694),MAAC_FLT_C(44483.197345580462),
    MAAC_FLT_C(44502.562071939843),MAAC_FLT_C(44521.928905107328),MAAC_FLT_C(44541.297844624634),MAAC_FLT_C(44560.668890033732),
    MAAC_FLT_C(44580.042040876848),MAAC_FLT_C(44599.417296696454),MAAC_FLT_C(44618.794657035272),MAAC_FLT_C(44638.174121436256),
    MAAC_FLT_C(44657.555689442641),MAAC_FLT_C(44676.939360597869),MAAC_FLT_C(44696.325134445673),MAAC_FLT_C(44715.713010530002),
    MAAC_FLT_C(44735.102988395054),MAAC_FLT_C(44754.495067585296),MAAC_FLT_C(44773.889247645420),MAAC_FLT_C(44793.285528120374),
    MAAC_FLT_C(44812.683908555344),MAAC_FLT_C(44832.084388495779),MAAC_FLT_C(44851.486967487355),MAAC_FLT_C(44870.891645076015),
    MAAC_FLT_C(44890.298420807914),MAAC_FLT_C(44909.707294229491),MAAC_FLT_C(44929.118264887409),MAAC_FLT_C(44948.531332328566),
    MAAC_FLT_C(44967.946496100136),MAAC_FLT_C(44987.363755749502),MAAC_FLT_C(45006.783110824326),MAAC_FLT_C(45026.204560872473),
    MAAC_FLT_C(45045.628105442098),MAAC_FLT_C(45065.053744081561),MAAC_FLT_C(45084.481476339490),MAAC_FLT_C(45103.911301764740),
    MAAC_FLT_C(45123.343219906426),MAAC_FLT_C(45142.777230313885),MAAC_FLT_C(45162.213332536710),MAAC_FLT_C(45181.651526124733),
    MAAC_FLT_C(45201.091810628037),MAAC_FLT_C(45220.534185596924),MAAC_FLT_C(45239.978650581965),MAAC_FLT_C(45259.425205133957),
    MAAC_FLT_C(45278.873848803938),MAAC_FLT_C(45298.324581143192),MAAC_FLT_C(45317.777401703235),MAAC_FLT_C(45337.232310035848),
    MAAC_FLT_C(45356.689305693020),MAAC_FLT_C(45376.148388226997),MAAC_FLT_C(45395.609557190270),MAAC_FLT_C(45415.072812135557),
    MAAC_FLT_C(45434.538152615823),MAAC_FLT_C(45454.005578184282),MAAC_FLT_C(45473.475088394356),MAAC_FLT_C(45492.946682799746),
    MAAC_FLT_C(45512.420360954362),MAAC_FLT_C(45531.896122412363),MAAC_FLT_C(45551.373966728155),MAAC_FLT_C(45570.853893456362),
    MAAC_FLT_C(45590.335902151870),MAAC_FLT_C(45609.819992369776),MAAC_FLT_C(45629.306163665438),MAAC_FLT_C(45648.794415594442),
    MAAC_FLT_C(45668.284747712612),MAAC_FLT_C(45687.777159576006),MAAC_FLT_C(45707.271650740920),MAAC_FLT_C(45726.768220763894),
    MAAC_FLT_C(45746.266869201696),MAAC_FLT_C(45765.767595611323),MAAC_FLT_C(45785.270399550034),MAAC_FLT_C(45804.775280575297),
    MAAC_FLT_C(45824.282238244821),MAAC_FLT_C(45843.791272116570),MAAC_FLT_C(45863.302381748719),MAAC_FLT_C(45882.815566699683),
    MAAC_FLT_C(45902.330826528130),MAAC_FLT_C(45921.848160792935),MAAC_FLT_C(45941.367569053225),MAAC_FLT_C(45960.889050868354),
    MAAC_FLT_C(45980.412605797930),MAAC_FLT_C(45999.938233401757),MAAC_FLT_C(46019.465933239902),MAAC_FLT_C(46038.995704872657),
    MAAC_FLT_C(46058.527547860547),MAAC_FLT_C(46078.061461764330),MAAC_FLT_C(46097.597446145002),MAAC_FLT_C(46117.135500563774),
    MAAC_FLT_C(46136.675624582109),MAAC_FLT_C(46156.217817761702),MAAC_FLT_C(46175.762079664462),MAAC_FLT_C(46195.308409852543),
    MAAC_FLT_C(46214.856807888333),MAAC_FLT_C(46234.407273334444),MAAC_FLT_C(46253.959805753715),MAAC_FLT_C(46273.514404709240),
    MAAC_FLT_C(46293.071069764315),MAAC_FLT_C(46312.629800482478),MAAC_FLT_C(46332.190596427499),MAAC_FLT_C(46351.753457163381),
    MAAC_FLT_C(46371.318382254351),MAAC_FLT_C(46390.885371264863),MAAC_FLT_C(46410.454423759620),MAAC_FLT_C(46430.025539303526),
    MAAC_FLT_C(46449.598717461733),MAAC_FLT_C(46469.173957799620),MAAC_FLT_C(46488.751259882782),MAAC_FLT_C(46508.330623277070),
    MAAC_FLT_C(46527.912047548532),MAAC_FLT_C(46547.495532263471),MAAC_FLT_C(46567.081076988397),MAAC_FLT_C(46586.668681290059),
    MAAC_FLT_C(46606.258344735434),MAAC_FLT_C(46625.850066891719),MAAC_FLT_C(46645.443847326351),MAAC_FLT_C(46665.039685606986),
    MAAC_FLT_C(46684.637581301497),MAAC_FLT_C(46704.237533978005),MAAC_FLT_C(46723.839543204842),MAAC_FLT_C(46743.443608550573),
    MAAC_FLT_C(46763.049729583989),MAAC_FLT_C(46782.657905874104),MAAC_FLT_C(46802.268136990162),MAAC_FLT_C(46821.880422501628),
    MAAC_FLT_C(46841.494761978196),MAAC_FLT_C(46861.111154989776),MAAC_FLT_C(46880.729601106526),MAAC_FLT_C(46900.350099898795),
    MAAC_FLT_C(46919.972650937190),MAAC_FLT_C(46939.597253792526),MAAC_FLT_C(46959.223908035841),MAAC_FLT_C(46978.852613238399),
    MAAC_FLT_C(46998.483368971691),MAAC_FLT_C(47018.116174807430),MAAC_FLT_C(47037.751030317551),MAAC_FLT_C(47057.387935074214),
    MAAC_FLT_C(47077.026888649809),MAAC_FLT_C(47096.667890616940),MAAC_FLT_C(47116.310940548428),MAAC_FLT_C(47135.956038017328),
    MAAC_FLT_C(47155.603182596918),MAAC_FLT_C(47175.252373860698),MAAC_FLT_C(47194.903611382375),MAAC_FLT_C(47214.556894735899),
    MAAC_FLT_C(47234.212223495422),MAAC_FLT_C(47253.869597235338),MAAC_FLT_C(47273.529015530250),MAAC_FLT_C(47293.190477954980),
    MAAC_FLT_C(47312.853984084577),MAAC_FLT_C(47332.519533494306),MAAC_FLT_C(47352.187125759658),MAAC_FLT_C(47371.856760456343),
    MAAC_FLT_C(47391.528437160297),MAAC_FLT_C(47411.202155447652),MAAC_FLT_C(47430.877914894787),MAAC_FLT_C(47450.555715078299),
    MAAC_FLT_C(47470.235555574982),MAAC_FLT_C(47489.917435961863),MAAC_FLT_C(47509.601355816201),MAAC_FLT_C(47529.287314715453),
    MAAC_FLT_C(47548.975312237308),MAAC_FLT_C(47568.665347959672),MAAC_FLT_C(47588.357421460656),MAAC_FLT_C(47608.051532318605),
    MAAC_FLT_C(47627.747680112072),MAAC_FLT_C(47647.445864419846),MAAC_FLT_C(47667.146084820910),MAAC_FLT_C(47686.848340894474),
    MAAC_FLT_C(47706.552632219973),MAAC_FLT_C(47726.258958377046),MAAC_FLT_C(47745.967318945557),MAAC_FLT_C(47765.677713505582),
    MAAC_FLT_C(47785.390141637428),MAAC_FLT_C(47805.104602921601),MAAC_FLT_C(47824.821096938824),MAAC_FLT_C(47844.539623270044),
    MAAC_FLT_C(47864.260181496429),MAAC_FLT_C(47883.982771199349),MAAC_FLT_C(47903.707391960394),MAAC_FLT_C(47923.434043361376),
    MAAC_FLT_C(47943.162724984308),MAAC_FLT_C(47962.893436411439),MAAC_FLT_C(47982.626177225218),MAAC_FLT_C(48002.360947008310),
    MAAC_FLT_C(48022.097745343599),MAAC_FLT_C(48041.836571814172),MAAC_FLT_C(48061.577426003350),MAAC_FLT_C(48081.320307494650),
    MAAC_FLT_C(48101.065215871815),MAAC_FLT_C(48120.812150718790),MAAC_FLT_C(48140.561111619740),MAAC_FLT_C(48160.312098159047),
    MAAC_FLT_C(48180.065109921306),MAAC_FLT_C(48199.820146491307),MAAC_FLT_C(48219.577207454073),MAAC_FLT_C(48239.336292394844),
    MAAC_FLT_C(48259.097400899045),MAAC_FLT_C(48278.860532552339),MAAC_FLT_C(48298.625686940592),MAAC_FLT_C(48318.392863649875),
    MAAC_FLT_C(48338.162062266485),MAAC_FLT_C(48357.933282376915),MAAC_FLT_C(48377.706523567889),MAAC_FLT_C(48397.481785426316),
    MAAC_FLT_C(48417.259067539344),MAAC_FLT_C(48437.038369494308),MAAC_FLT_C(48456.819690878765),MAAC_FLT_C(48476.603031280487),
    MAAC_FLT_C(48496.388390287451),MAAC_FLT_C(48516.175767487839),MAAC_FLT_C(48535.965162470042),MAAC_FLT_C(48555.756574822684),
    MAAC_FLT_C(48575.550004134566),MAAC_FLT_C(48595.345449994718),MAAC_FLT_C(48615.142911992378),MAAC_FLT_C(48634.942389716991),
    MAAC_FLT_C(48654.743882758201),MAAC_FLT_C(48674.547390705877),MAAC_FLT_C(48694.352913150091),MAAC_FLT_C(48714.160449681112),
    MAAC_FLT_C(48733.969999889436),MAAC_FLT_C(48753.781563365759),MAAC_FLT_C(48773.595139700985),MAAC_FLT_C(48793.410728486211),
    MAAC_FLT_C(48813.228329312769),MAAC_FLT_C(48833.047941772187),MAAC_FLT_C(48852.869565456189),MAAC_FLT_C(48872.693199956717),
    MAAC_FLT_C(48892.518844865925),MAAC_FLT_C(48912.346499776155),MAAC_FLT_C(48932.176164279976),MAAC_FLT_C(48952.007837970152),
    MAAC_FLT_C(48971.841520439659),MAAC_FLT_C(48991.677211281676),MAAC_FLT_C(49011.514910089587),MAAC_FLT_C(49031.354616456978),
    MAAC_FLT_C(49051.196329977654),MAAC_FLT_C(49071.040050245610),MAAC_FLT_C(49090.885776855059),MAAC_FLT_C(49110.733509400408),
    MAAC_FLT_C(49130.583247476279),MAAC_FLT_C(49150.434990677488),MAAC_FLT_C(49170.288738599062),MAAC_FLT_C(49190.144490836239),
    MAAC_FLT_C(49210.002246984441),MAAC_FLT_C(49229.862006639327),MAAC_FLT_C(49249.723769396718),MAAC_FLT_C(49269.587534852675),
    MAAC_FLT_C(49289.453302603448),MAAC_FLT_C(49309.321072245482),MAAC_FLT_C(49329.190843375451),MAAC_FLT_C(49349.062615590192),
    MAAC_FLT_C(49368.936388486785),MAAC_FLT_C(49388.812161662492),MAAC_FLT_C(49408.689934714785),MAAC_FLT_C(49428.569707241324),
    MAAC_FLT_C(49448.451478839990),MAAC_FLT_C(49468.335249108866),MAAC_FLT_C(49488.221017646210),MAAC_FLT_C(49508.108784050521),
    MAAC_FLT_C(49527.998547920470),MAAC_FLT_C(49547.890308854934),MAAC_FLT_C(49567.784066453009),MAAC_FLT_C(49587.679820313977),
    MAAC_FLT_C(49607.577570037312),MAAC_FLT_C(49627.477315222721),MAAC_FLT_C(49647.379055470075),MAAC_FLT_C(49667.282790379460),
    MAAC_FLT_C(49687.188519551179),MAAC_FLT_C(49707.096242585707),MAAC_FLT_C(49727.005959083741),MAAC_FLT_C(49746.917668646165),
    MAAC_FLT_C(49766.831370874068),MAAC_FLT_C(49786.747065368734),MAAC_FLT_C(49806.664751731660),MAAC_FLT_C(49826.584429564515),
    MAAC_FLT_C(49846.506098469203),MAAC_FLT_C(49866.429758047794),MAAC_FLT_C(49886.355407902578),MAAC_FLT_C(49906.283047636032),
    MAAC_FLT_C(49926.212676850846),MAAC_FLT_C(49946.144295149883),MAAC_FLT_C(49966.077902136225),MAAC_FLT_C(49986.013497413151),
    MAAC_FLT_C(50005.951080584135),MAAC_FLT_C(50025.890651252834),MAAC_FLT_C(50045.832209023123),MAAC_FLT_C(50065.775753499067),
    MAAC_FLT_C(50085.721284284933),MAAC_FLT_C(50105.668800985164),MAAC_FLT_C(50125.618303204428),MAAC_FLT_C(50145.569790547575),
    MAAC_FLT_C(50165.523262619652),MAAC_FLT_C(50185.478719025901),MAAC_FLT_C(50205.436159371769),MAAC_FLT_C(50225.395583262893),
    MAAC_FLT_C(50245.356990305103),MAAC_FLT_C(50265.320380104429),MAAC_FLT_C(50285.285752267104),MAAC_FLT_C(50305.253106399534),
    MAAC_FLT_C(50325.222442108337),MAAC_FLT_C(50345.193759000329),MAAC_FLT_C(50365.167056682520),MAAC_FLT_C(50385.142334762102),
    MAAC_FLT_C(50405.119592846473),MAAC_FLT_C(50425.098830543218),MAAC_FLT_C(50445.080047460127),MAAC_FLT_C(50465.063243205179),
    MAAC_FLT_C(50485.048417386541),MAAC_FLT_C(50505.035569612577),MAAC_FLT_C(50525.024699491856),MAAC_FLT_C(50545.015806633128),
    MAAC_FLT_C(50565.008890645338),MAAC_FLT_C(50585.003951137631),MAAC_FLT_C(50605.000987719330),MAAC_FLT_C(50624.999999999971),
    MAAC_FLT_C(50645.000987589265),MAAC_FLT_C(50665.003950097132),MAAC_FLT_C(50685.008887133677),MAAC_FLT_C(50705.015798309192),
    MAAC_FLT_C(50725.024683234165),MAAC_FLT_C(50745.035541519283),MAAC_FLT_C(50765.048372775411),MAAC_FLT_C(50785.063176613621),
    MAAC_FLT_C(50805.079952645159),MAAC_FLT_C(50825.098700481489),MAAC_FLT_C(50845.119419734241),MAAC_FLT_C(50865.142110015244),
    MAAC_FLT_C(50885.166770936521),MAAC_FLT_C(50905.193402110279),MAAC_FLT_C(50925.222003148934),MAAC_FLT_C(50945.252573665071),
    MAAC_FLT_C(50965.285113271471),MAAC_FLT_C(50985.319621581119),MAAC_FLT_C(51005.356098207172),MAAC_FLT_C(51025.394542762981),
    MAAC_FLT_C(51045.434954862096),MAAC_FLT_C(51065.477334118244),MAAC_FLT_C(51085.521680145357),MAAC_FLT_C(51105.567992557546),
    MAAC_FLT_C(51125.616270969113),MAAC_FLT_C(51145.666514994540),MAAC_FLT_C(51165.718724248523),MAAC_FLT_C(51185.772898345916),
    MAAC_FLT_C(51205.829036901778),MAAC_FLT_C(51225.887139531362),MAAC_FLT_C(51245.947205850105),MAAC_FLT_C(51266.009235473619),
    MAAC_FLT_C(51286.073228017718),MAAC_FLT_C(51306.139183098399),MAAC_FLT_C(51326.207100331856),MAAC_FLT_C(51346.276979334449),
    MAAC_FLT_C(51366.348819722756),MAAC_FLT_C(51386.422621113510),MAAC_FLT_C(51406.498383123653),MAAC_FLT_C(51426.576105370310),
    MAAC_FLT_C(51446.655787470787),MAAC_FLT_C(51466.737429042587),MAAC_FLT_C(51486.821029703380),MAAC_FLT_C(51506.906589071048),
    MAAC_FLT_C(51526.994106763632),MAAC_FLT_C(51547.083582399391),MAAC_FLT_C(51567.175015596738),MAAC_FLT_C(51587.268405974297),
    MAAC_FLT_C(51607.363753150858),MAAC_FLT_C(51627.461056745415),MAAC_FLT_C(51647.560316377130),MAAC_FLT_C(51667.661531665362),
    MAAC_FLT_C(51687.764702229651),MAAC_FLT_C(51707.869827689727),MAAC_FLT_C(51727.976907665499),MAAC_FLT_C(51748.085941777055),
    MAAC_FLT_C(51768.196929644677),MAAC_FLT_C(51788.309870888836),MAAC_FLT_C(51808.424765130170),MAAC_FLT_C(51828.541611989524),
    MAAC_FLT_C(51848.660411087905),MAAC_FLT_C(51868.781162046515),MAAC_FLT_C(51888.903864486740),MAAC_FLT_C(51909.028518030143),
    MAAC_FLT_C(51929.155122298485),MAAC_FLT_C(51949.283676913685),MAAC_FLT_C(51969.414181497872),MAAC_FLT_C(51989.546635673345),
    MAAC_FLT_C(52009.681039062591),MAAC_FLT_C(52029.817391288263),MAAC_FLT_C(52049.955691973213),MAAC_FLT_C(52070.095940740481),
    MAAC_FLT_C(52090.238137213273),MAAC_FLT_C(52110.382281014987),MAAC_FLT_C(52130.528371769200),MAAC_FLT_C(52150.676409099666),
    MAAC_FLT_C(52170.826392630333),MAAC_FLT_C(52190.978321985320),MAAC_FLT_C(52211.132196788931),MAAC_FLT_C(52231.288016665654),
    MAAC_FLT_C(52251.445781240145),MAAC_FLT_C(52271.605490137270),MAAC_FLT_C(52291.767142982040),MAAC_FLT_C(52311.930739399664),
    MAAC_FLT_C(52332.096279015546),MAAC_FLT_C(52352.263761455244),MAAC_FLT_C(52372.433186344519),MAAC_FLT_C(52392.604553309284),
    MAAC_FLT_C(52412.777861975665),MAAC_FLT_C(52432.953111969946),MAAC_FLT_C(52453.130302918595),MAAC_FLT_C(52473.309434448274),
    MAAC_FLT_C(52493.490506185793),MAAC_FLT_C(52513.673517758180),MAAC_FLT_C(52533.858468792605),MAAC_FLT_C(52554.045358916446),
    MAAC_FLT_C(52574.234187757254),MAAC_FLT_C(52594.424954942740),MAAC_FLT_C(52614.617660100812),MAAC_FLT_C(52634.812302859558),
    MAAC_FLT_C(52655.008882847229),MAAC_FLT_C(52675.207399692270),MAAC_FLT_C(52695.407853023295),MAAC_FLT_C(52715.610242469098),
    MAAC_FLT_C(52735.814567658657),MAAC_FLT_C(52756.020828221110),MAAC_FLT_C(52776.229023785803),MAAC_FLT_C(52796.439153982225),
    MAAC_FLT_C(52816.651218440056),MAAC_FLT_C(52836.865216789171),MAAC_FLT_C(52857.081148659599),MAAC_FLT_C(52877.299013681550),
    MAAC_FLT_C(52897.518811485425),MAAC_FLT_C(52917.740541701773),MAAC_FLT_C(52937.964203961354),MAAC_FLT_C(52958.189797895080),
    MAAC_FLT_C(52978.417323134046),MAAC_FLT_C(52998.646779309529),MAAC_FLT_C(53018.878166052978),MAAC_FLT_C(53039.111482996006),
    MAAC_FLT_C(53059.346729770419),MAAC_FLT_C(53079.583906008193),MAAC_FLT_C(53099.823011341483),MAAC_FLT_C(53120.064045402600),
    MAAC_FLT_C(53140.307007824063),MAAC_FLT_C(53160.551898238533),MAAC_FLT_C(53180.798716278870),MAAC_FLT_C(53201.047461578091),
    MAAC_FLT_C(53221.298133769400),MAAC_FLT_C(53241.550732486176),MAAC_FLT_C(53261.805257361964),MAAC_FLT_C(53282.061708030487),
    MAAC_FLT_C(53302.320084125640),MAAC_FLT_C(53322.580385281493),MAAC_FLT_C(53342.842611132299),MAAC_FLT_C(53363.106761312469),
    MAAC_FLT_C(53383.372835456597),MAAC_FLT_C(53403.640833199453),MAAC_FLT_C(53423.910754175973),MAAC_FLT_C(53444.182598021260),
    MAAC_FLT_C(53464.456364370613),MAAC_FLT_C(53484.732052859479),MAAC_FLT_C(53505.009663123499),MAAC_FLT_C(53525.289194798468),
    MAAC_FLT_C(53545.570647520362),MAAC_FLT_C(53565.854020925333),MAAC_FLT_C(53586.139314649699),MAAC_FLT_C(53606.426528329954),
    MAAC_FLT_C(53626.715661602764),MAAC_FLT_C(53647.006714104959),MAAC_FLT_C(53667.299685473547),MAAC_FLT_C(53687.594575345720),
    MAAC_FLT_C(53707.891383358816),MAAC_FLT_C(53728.190109150361),MAAC_FLT_C(53748.490752358055),MAAC_FLT_C(53768.793312619753),
    MAAC_FLT_C(53789.097789573498),MAAC_FLT_C(53809.404182857485),MAAC_FLT_C(53829.712492110106),MAAC_FLT_C(53850.022716969899),
    MAAC_FLT_C(53870.334857075584),MAAC_FLT_C(53890.648912066055),MAAC_FLT_C(53910.964881580367),MAAC_FLT_C(53931.282765257740),
    MAAC_FLT_C(53951.602562737586),MAAC_FLT_C(53971.924273659461),MAAC_FLT_C(53992.247897663110),MAAC_FLT_C(54012.573434388440),
    MAAC_FLT_C(54032.900883475530),MAAC_FLT_C(54053.230244564620),MAAC_FLT_C(54073.561517296133),MAAC_FLT_C(54093.894701310644),
    MAAC_FLT_C(54114.229796248910),MAAC_FLT_C(54134.566801751855),MAAC_FLT_C(54154.905717460570),MAAC_FLT_C(54175.246543016314),
    MAAC_FLT_C(54195.589278060506),MAAC_FLT_C(54215.933922234755),MAAC_FLT_C(54236.280475180814),MAAC_FLT_C(54256.628936540626),
    MAAC_FLT_C(54276.979305956280),MAAC_FLT_C(54297.331583070045),MAAC_FLT_C(54317.685767524359),MAAC_FLT_C(54338.041858961828),
    MAAC_FLT_C(54358.399857025215),MAAC_FLT_C(54378.759761357454),MAAC_FLT_C(54399.121571601667),MAAC_FLT_C(54419.485287401105),
    MAAC_FLT_C(54439.850908399225),MAAC_FLT_C(54460.218434239614),MAAC_FLT_C(54480.587864566056),MAAC_FLT_C(54500.959199022480),
    MAAC_FLT_C(54521.332437252997),MAAC_FLT_C(54541.707578901878),MAAC_FLT_C(54562.084623613555),MAAC_FLT_C(54582.463571032640),
    MAAC_FLT_C(54602.844420803885),MAAC_FLT_C(54623.227172572246),MAAC_FLT_C(54643.611825982807),MAAC_FLT_C(54663.998380680838),
    MAAC_FLT_C(54684.386836311773),MAAC_FLT_C(54704.777192521207),MAAC_FLT_C(54725.169448954897),MAAC_FLT_C(54745.563605258772),
    MAAC_FLT_C(54765.959661078923),MAAC_FLT_C(54786.357616061614),MAAC_FLT_C(54806.757469853255),MAAC_FLT_C(54827.159222100439),
    MAAC_FLT_C(54847.562872449904),MAAC_FLT_C(54867.968420548583),MAAC_FLT_C(54888.375866043534),MAAC_FLT_C(54908.785208582012),
    MAAC_FLT_C(54929.196447811417),MAAC_FLT_C(54949.609583379322),MAAC_FLT_C(54970.024614933463),MAAC_FLT_C(54990.441542121727),
    MAAC_FLT_C(55010.860364592190),MAAC_FLT_C(55031.281081993060),MAAC_FLT_C(55051.703693972733),MAAC_FLT_C(55072.128200179759),
    MAAC_FLT_C(55092.554600262840),MAAC_FLT_C(55112.982893870874),MAAC_FLT_C(55133.413080652877),MAAC_FLT_C(55153.845160258061),
    MAAC_FLT_C(55174.279132335789),MAAC_FLT_C(55194.714996535586),MAAC_FLT_C(55215.152752507143),MAAC_FLT_C(55235.592399900299),
    MAAC_FLT_C(55256.033938365079),MAAC_FLT_C(55276.477367551655),MAAC_FLT_C(55296.922687110360),MAAC_FLT_C(55317.369896691685),
    MAAC_FLT_C(55337.818995946305),MAAC_FLT_C(55358.269984525024),MAAC_FLT_C(55378.722862078830),MAAC_FLT_C(55399.177628258869),
    MAAC_FLT_C(55419.634282716441),MAAC_FLT_C(55440.092825103013),MAAC_FLT_C(55460.553255070205),MAAC_FLT_C(55481.015572269804),
    MAAC_FLT_C(55501.479776353764),MAAC_FLT_C(55521.945866974187),MAAC_FLT_C(55542.413843783339),MAAC_FLT_C(55562.883706433655),
    MAAC_FLT_C(55583.355454577715),MAAC_FLT_C(55603.829087868260),MAAC_FLT_C(55624.304605958219),MAAC_FLT_C(55644.782008500639),
    MAAC_FLT_C(55665.261295148754),MAAC_FLT_C(55685.742465555952),MAAC_FLT_C(55706.225519375774),MAAC_FLT_C(55726.710456261928),
    MAAC_FLT_C(55747.197275868275),MAAC_FLT_C(55767.685977848843),MAAC_FLT_C(55788.176561857814),MAAC_FLT_C(55808.669027549528),
    MAAC_FLT_C(55829.163374578478),MAAC_FLT_C(55849.659602599328),MAAC_FLT_C(55870.157711266889),MAAC_FLT_C(55890.657700236145),
    MAAC_FLT_C(55911.159569162221),MAAC_FLT_C(55931.663317700411),MAAC_FLT_C(55952.168945506164),MAAC_FLT_C(55972.676452235086),
    MAAC_FLT_C(55993.185837542944),MAAC_FLT_C(56013.697101085651),MAAC_FLT_C(56034.210242519301),MAAC_FLT_C(56054.725261500120),
    MAAC_FLT_C(56075.242157684508),MAAC_FLT_C(56095.760930729011),MAAC_FLT_C(56116.281580290342),MAAC_FLT_C(56136.804106025367),
    MAAC_FLT_C(56157.328507591104),MAAC_FLT_C(56177.854784644740),MAAC_FLT_C(56198.382936843598),MAAC_FLT_C(56218.912963845185),
    MAAC_FLT_C(56239.444865307138),MAAC_FLT_C(56259.978640887268),MAAC_FLT_C(56280.514290243525),MAAC_FLT_C(56301.051813034042),
    MAAC_FLT_C(56321.591208917082),MAAC_FLT_C(56342.132477551080),MAAC_FLT_C(56362.675618594614),MAAC_FLT_C(56383.220631706419),
    MAAC_FLT_C(56403.767516545398),MAAC_FLT_C(56424.316272770608),MAAC_FLT_C(56444.866900041241),MAAC_FLT_C(56465.419398016667),
    MAAC_FLT_C(56485.973766356394),MAAC_FLT_C(56506.530004720102),MAAC_FLT_C(56527.088112767611),MAAC_FLT_C(56547.648090158902),
    MAAC_FLT_C(56568.209936554107),MAAC_FLT_C(56588.773651613519),MAAC_FLT_C(56609.339234997584),MAAC_FLT_C(56629.906686366900),
    MAAC_FLT_C(56650.476005382210),MAAC_FLT_C(56671.047191704420),MAAC_FLT_C(56691.620244994599),MAAC_FLT_C(56712.195164913959),
    MAAC_FLT_C(56732.771951123868),MAAC_FLT_C(56753.350603285835),MAAC_FLT_C(56773.931121061541),MAAC_FLT_C(56794.513504112823),
    MAAC_FLT_C(56815.097752101647),MAAC_FLT_C(56835.683864690152),MAAC_FLT_C(56856.271841540627),MAAC_FLT_C(56876.861682315510),
    MAAC_FLT_C(56897.453386677393),MAAC_FLT_C(56918.046954289028),MAAC_FLT_C(56938.642384813298),MAAC_FLT_C(56959.239677913261),
    MAAC_FLT_C(56979.838833252121),MAAC_FLT_C(57000.439850493225),MAAC_FLT_C(57021.042729300090),MAAC_FLT_C(57041.647469336371),
    MAAC_FLT_C(57062.254070265873),MAAC_FLT_C(57082.862531752558),MAAC_FLT_C(57103.472853460553),MAAC_FLT_C(57124.085035054108),
    MAAC_FLT_C(57144.699076197649),MAAC_FLT_C(57165.314976555739),MAAC_FLT_C(57185.932735793103),MAAC_FLT_C(57206.552353574611),
    MAAC_FLT_C(57227.173829565276),MAAC_FLT_C(57247.797163430281),MAAC_FLT_C(57268.422354834940),MAAC_FLT_C(57289.049403444733),
    MAAC_FLT_C(57309.678308925286),MAAC_FLT_C(57330.309070942370),MAAC_FLT_C(57350.941689161911),MAAC_FLT_C(57371.576163249985),
    MAAC_FLT_C(57392.212492872815),MAAC_FLT_C(57412.850677696784),MAAC_FLT_C(57433.490717388406),MAAC_FLT_C(57454.132611614368),
    MAAC_FLT_C(57474.776360041491),MAAC_FLT_C(57495.421962336746),MAAC_FLT_C(57516.069418167266),MAAC_FLT_C(57536.718727200314),
    MAAC_FLT_C(57557.369889103320),MAAC_FLT_C(57578.022903543861),MAAC_FLT_C(57598.677770189643),MAAC_FLT_C(57619.334488708548),
    MAAC_FLT_C(57639.993058768589),MAAC_FLT_C(57660.653480037938),MAAC_FLT_C(57681.315752184906),MAAC_FLT_C(57701.979874877965),
    MAAC_FLT_C(57722.645847785730),MAAC_FLT_C(57743.313670576950),MAAC_FLT_C(57763.983342920546),MAAC_FLT_C(57784.654864485572),
    MAAC_FLT_C(57805.328234941233),MAAC_FLT_C(57826.003453956881),MAAC_FLT_C(57846.680521202026),MAAC_FLT_C(57867.359436346313),
    MAAC_FLT_C(57888.040199059527),MAAC_FLT_C(57908.722809011633),MAAC_FLT_C(57929.407265872709),MAAC_FLT_C(57950.093569313001),
    MAAC_FLT_C(57970.781719002895),MAAC_FLT_C(57991.471714612911),MAAC_FLT_C(58012.163555813750),MAAC_FLT_C(58032.857242276223),
    MAAC_FLT_C(58053.552773671312),MAAC_FLT_C(58074.250149670130),MAAC_FLT_C(58094.949369943948),MAAC_FLT_C(58115.650434164185),
    MAAC_FLT_C(58136.353342002389),MAAC_FLT_C(58157.058093130276),MAAC_FLT_C(58177.764687219693),MAAC_FLT_C(58198.473123942640),
    MAAC_FLT_C(58219.183402971255),MAAC_FLT_C(58239.895523977837),MAAC_FLT_C(58260.609486634821),MAAC_FLT_C(58281.325290614775),
    MAAC_FLT_C(58302.042935590434),MAAC_FLT_C(58322.762421234678),MAAC_FLT_C(58343.483747220511),MAAC_FLT_C(58364.206913221096),
    MAAC_FLT_C(58384.931918909751),MAAC_FLT_C(58405.658763959924),MAAC_FLT_C(58426.387448045200),MAAC_FLT_C(58447.117970839339),
    MAAC_FLT_C(58467.850332016213),MAAC_FLT_C(58488.584531249864),MAAC_FLT_C(58509.320568214462),MAAC_FLT_C(58530.058442584334),
    MAAC_FLT_C(58550.798154033931),MAAC_FLT_C(58571.539702237875),MAAC_FLT_C(58592.283086870906),MAAC_FLT_C(58613.028307607929),
    MAAC_FLT_C(58633.775364123983),MAAC_FLT_C(58654.524256094250),MAAC_FLT_C(58675.274983194053),MAAC_FLT_C(58696.027545098877),
    MAAC_FLT_C(58716.781941484325),MAAC_FLT_C(58737.538172026158),MAAC_FLT_C(58758.296236400274),MAAC_FLT_C(58779.056134282728),
    MAAC_FLT_C(58799.817865349694),MAAC_FLT_C(58820.581429277503),MAAC_FLT_C(58841.346825742643),MAAC_FLT_C(58862.114054421712),
    MAAC_FLT_C(58882.883114991484),MAAC_FLT_C(58903.654007128847),MAAC_FLT_C(58924.426730510851),MAAC_FLT_C(58945.201284814684),
    MAAC_FLT_C(58965.977669717664),MAAC_FLT_C(58986.755884897269),MAAC_FLT_C(59007.535930031117),MAAC_FLT_C(59028.317804796949),
    MAAC_FLT_C(59049.101508872664),MAAC_FLT_C(59069.887041936301),MAAC_FLT_C(59090.674403666046),MAAC_FLT_C(59111.463593740213),
    MAAC_FLT_C(59132.254611837263),MAAC_FLT_C(59153.047457635803),MAAC_FLT_C(59173.842130814570),MAAC_FLT_C(59194.638631052469),
    MAAC_FLT_C(59215.436958028506),MAAC_FLT_C(59236.237111421855),MAAC_FLT_C(59257.039090911829),MAAC_FLT_C(59277.842896177877),
    MAAC_FLT_C(59298.648526899589),MAAC_FLT_C(59319.455982756685),MAAC_FLT_C(59340.265263429050),MAAC_FLT_C(59361.076368596696),
    MAAC_FLT_C(59381.889297939757),MAAC_FLT_C(59402.704051138542),MAAC_FLT_C(59423.520627873477),MAAC_FLT_C(59444.339027825139),
    MAAC_FLT_C(59465.159250674231),MAAC_FLT_C(59485.981296101600),MAAC_FLT_C(59506.805163788253),MAAC_FLT_C(59527.630853415314),
    MAAC_FLT_C(59548.458364664046),MAAC_FLT_C(59569.287697215863),MAAC_FLT_C(59590.118850752318),MAAC_FLT_C(59610.951824955089),
    MAAC_FLT_C(59631.786619506012),MAAC_FLT_C(59652.623234087048),MAAC_FLT_C(59673.461668380311),MAAC_FLT_C(59694.301922068029),
    MAAC_FLT_C(59715.143994832593),MAAC_FLT_C(59735.987886356525),MAAC_FLT_C(59756.833596322482),MAAC_FLT_C(59777.681124413255),
    MAAC_FLT_C(59798.530470311794),MAAC_FLT_C(59819.381633701159),MAAC_FLT_C(59840.234614264569),MAAC_FLT_C(59861.089411685381),
    MAAC_FLT_C(59881.946025647070),MAAC_FLT_C(59902.804455833269),MAAC_FLT_C(59923.664701927744),MAAC_FLT_C(59944.526763614384),
    MAAC_FLT_C(59965.390640577243),MAAC_FLT_C(59986.256332500488),MAAC_FLT_C(60007.123839068438),MAAC_FLT_C(60027.993159965539),
    MAAC_FLT_C(60048.864294876381),MAAC_FLT_C(60069.737243485688),MAAC_FLT_C(60090.612005478324),MAAC_FLT_C(60111.488580539284),
    MAAC_FLT_C(60132.366968353708),MAAC_FLT_C(60153.247168606867),MAAC_FLT_C(60174.129180984164),MAAC_FLT_C(60195.013005171153),
    MAAC_FLT_C(60215.898640853513),MAAC_FLT_C(60236.786087717061),MAAC_FLT_C(60257.675345447751),MAAC_FLT_C(60278.566413731671),
    MAAC_FLT_C(60299.459292255044),MAAC_FLT_C(60320.353980704247),MAAC_FLT_C(60341.250478765760),MAAC_FLT_C(60362.148786126229),
    MAAC_FLT_C(60383.048902472416),MAAC_FLT_C(60403.950827491237),MAAC_FLT_C(60424.854560869717),MAAC_FLT_C(60445.760102295040),
    MAAC_FLT_C(60466.667451454516),MAAC_FLT_C(60487.576608035590),MAAC_FLT_C(60508.487571725847),MAAC_FLT_C(60529.400342212997),
    MAAC_FLT_C(60550.314919184893),MAAC_FLT_C(60571.231302329521),MAAC_FLT_C(60592.149491335003),MAAC_FLT_C(60613.069485889588),
    MAAC_FLT_C(60633.991285681674),MAAC_FLT_C(60654.914890399785),MAAC_FLT_C(60675.840299732568),MAAC_FLT_C(60696.767513368832),
    MAAC_FLT_C(60717.696530997484),MAAC_FLT_C(60738.627352307602),MAAC_FLT_C(60759.559976988370),MAAC_FLT_C(60780.494404729128),
    MAAC_FLT_C(60801.430635219323),MAAC_FLT_C(60822.368668148556),MAAC_FLT_C(60843.308503206565),MAAC_FLT_C(60864.250140083204),
    MAAC_FLT_C(60885.193578468468),MAAC_FLT_C(60906.138818052495),MAAC_FLT_C(60927.085858525541),MAAC_FLT_C(60948.034699578006),
    MAAC_FLT_C(60968.985340900421),MAAC_FLT_C(60989.937782183442),MAAC_FLT_C(61010.892023117864),MAAC_FLT_C(61031.848063394616),
    MAAC_FLT_C(61052.805902704764),MAAC_FLT_C(61073.765540739492),MAAC_FLT_C(61094.726977190134),MAAC_FLT_C(61115.690211748137),
    MAAC_FLT_C(61136.655244105103),MAAC_FLT_C(61157.622073952742),MAAC_FLT_C(61178.590700982917),MAAC_FLT_C(61199.561124887616),
    MAAC_FLT_C(61220.533345358948),MAAC_FLT_C(61241.507362089171),MAAC_FLT_C(61262.483174770663),MAAC_FLT_C(61283.460783095943),
    MAAC_FLT_C(61304.440186757645),MAAC_FLT_C(61325.421385448557),MAAC_FLT_C(61346.404378861582),MAAC_FLT_C(61367.389166689762),
    MAAC_FLT_C(61388.375748626262),MAAC_FLT_C(61409.364124364387),MAAC_FLT_C(61430.354293597571),MAAC_FLT_C(61451.346256019373),
    MAAC_FLT_C(61472.340011323497),MAAC_FLT_C(61493.335559203762),MAAC_FLT_C(61514.332899354122),MAAC_FLT_C(61535.332031468672),
    MAAC_FLT_C(61556.332955241618),MAAC_FLT_C(61577.335670367313),MAAC_FLT_C(61598.340176540238),MAAC_FLT_C(61619.346473454993),
    MAAC_FLT_C(61640.354560806329),MAAC_FLT_C(61661.364438289100),MAAC_FLT_C(61682.376105598312),MAAC_FLT_C(61703.389562429089),
    MAAC_FLT_C(61724.404808476691),MAAC_FLT_C(61745.421843436510),MAAC_FLT_C(61766.440667004063),MAAC_FLT_C(61787.461278874987),
    MAAC_FLT_C(61808.483678745062),MAAC_FLT_C(61829.507866310203),MAAC_FLT_C(61850.533841266435),MAAC_FLT_C(61871.561603309929),
    MAAC_FLT_C(61892.591152136971),MAAC_FLT_C(61913.622487443987),MAAC_FLT_C(61934.655608927525),MAAC_FLT_C(61955.690516284267),
    MAAC_FLT_C(61976.727209211022),MAAC_FLT_C(61997.765687404724),MAAC_FLT_C(62018.805950562448),MAAC_FLT_C(62039.847998381381),
    MAAC_FLT_C(62060.891830558845),MAAC_FLT_C(62081.937446792290),MAAC_FLT_C(62102.984846779298),MAAC_FLT_C(62124.034030217575),
    MAAC_FLT_C(62145.084996804966),MAAC_FLT_C(62166.137746239416),MAAC_FLT_C(62187.192278219030),MAAC_FLT_C(62208.248592442025),
    MAAC_FLT_C(62229.306688606739),MAAC_FLT_C(62250.366566411656),MAAC_FLT_C(62271.428225555377),MAAC_FLT_C(62292.491665736627),
    MAAC_FLT_C(62313.556886654267),MAAC_FLT_C(62334.623888007271),MAAC_FLT_C(62355.692669494762),MAAC_FLT_C(62376.763230815974),
    MAAC_FLT_C(62397.835571670272),MAAC_FLT_C(62418.909691757144),MAAC_FLT_C(62439.985590776210),MAAC_FLT_C(62461.063268427220),
    MAAC_FLT_C(62482.142724410049),MAAC_FLT_C(62503.223958424685),MAAC_FLT_C(62524.306970171267),MAAC_FLT_C(62545.391759350030),
    MAAC_FLT_C(62566.478325661366),MAAC_FLT_C(62587.566668805768),MAAC_FLT_C(62608.656788483881),MAAC_FLT_C(62629.748684396451),
    MAAC_FLT_C(62650.842356244357),MAAC_FLT_C(62671.937803728622),MAAC_FLT_C(62693.035026550366),MAAC_FLT_C(62714.134024410858),
    MAAC_FLT_C(62735.234797011479),MAAC_FLT_C(62756.337344053733),MAAC_FLT_C(62777.441665239276),MAAC_FLT_C(62798.547760269852),
    MAAC_FLT_C(62819.655628847358),MAAC_FLT_C(62840.765270673801),MAAC_FLT_C(62861.876685451323),MAAC_FLT_C(62882.989872882186),
    MAAC_FLT_C(62904.104832668774),MAAC_FLT_C(62925.221564513602),MAAC_FLT_C(62946.340068119309),MAAC_FLT_C(62967.460343188657),
    MAAC_FLT_C(62988.582389424526),MAAC_FLT_C(63009.706206529940),MAAC_FLT_C(63030.831794208018),MAAC_FLT_C(63051.959152162039),
    MAAC_FLT_C(63073.088280095370),MAAC_FLT_C(63094.219177711537),MAAC_FLT_C(63115.351844714154),MAAC_FLT_C(63136.486280806988),
    MAAC_FLT_C(63157.622485693922),MAAC_FLT_C(63178.760459078956),MAAC_FLT_C(63199.900200666219),MAAC_FLT_C(63221.041710159967),
    MAAC_FLT_C(63242.184987264569),MAAC_FLT_C(63263.330031684534),MAAC_FLT_C(63284.476843124474),MAAC_FLT_C(63305.625421289144),
    MAAC_FLT_C(63326.775765883409),MAAC_FLT_C(63347.927876612259),MAAC_FLT_C(63369.081753180813),MAAC_FLT_C(63390.237395294316),
    MAAC_FLT_C(63411.394802658120),MAAC_FLT_C(63432.553974977716),MAAC_FLT_C(63453.714911958712),MAAC_FLT_C(63474.877613306839),
    MAAC_FLT_C(63496.042078727944),MAAC_FLT_C(63517.208307927998),MAAC_FLT_C(63538.376300613119),MAAC_FLT_C(63559.546056489504),
    MAAC_FLT_C(63580.717575263516),MAAC_FLT_C(63601.890856641607),MAAC_FLT_C(63623.065900330374),MAAC_FLT_C(63644.242706036515),
    MAAC_FLT_C(63665.421273466869),MAAC_FLT_C(63686.601602328381),MAAC_FLT_C(63707.783692328136),MAAC_FLT_C(63728.967543173334),
    MAAC_FLT_C(63750.153154571279),MAAC_FLT_C(63771.340526229418),MAAC_FLT_C(63792.529657855317),MAAC_FLT_C(63813.720549156649),
    MAAC_FLT_C(63834.913199841227),MAAC_FLT_C(63856.107609616978),MAAC_FLT_C(63877.303778191941),MAAC_FLT_C(63898.501705274284),
    MAAC_FLT_C(63919.701390572300),MAAC_FLT_C(63940.902833794404),MAAC_FLT_C(63962.106034649114),MAAC_FLT_C(63983.310992845094),
    MAAC_FLT_C(64004.517708091109),MAAC_FLT_C(64025.726180096048),MAAC_FLT_C(64046.936408568938),MAAC_FLT_C(64068.148393218900),
    MAAC_FLT_C(64089.362133755196),MAAC_FLT_C(64110.577629887193),MAAC_FLT_C(64131.794881324393),MAAC_FLT_C(64153.013887776404),
    MAAC_FLT_C(64174.234648952966),MAAC_FLT_C(64195.457164563937),MAAC_FLT_C(64216.681434319289),MAAC_FLT_C(64237.907457929112),
    MAAC_FLT_C(64259.135235103626),MAAC_FLT_C(64280.364765553160),MAAC_FLT_C(64301.596048988169),MAAC_FLT_C(64322.829085119236),
    MAAC_FLT_C(64344.063873657040),MAAC_FLT_C(64365.300414312398),MAAC_FLT_C(64386.538706796251),MAAC_FLT_C(64407.778750819634),
    MAAC_FLT_C(64429.020546093721),MAAC_FLT_C(64450.264092329810),MAAC_FLT_C(64471.509389239291),MAAC_FLT_C(64492.756436533709),
    MAAC_FLT_C(64514.005233924705),MAAC_FLT_C(64535.255781124033),MAAC_FLT_C(64556.508077843580),MAAC_FLT_C(64577.762123795357),
    MAAC_FLT_C(64599.017918691468),MAAC_FLT_C(64620.275462244172),MAAC_FLT_C(64641.534754165805),MAAC_FLT_C(64662.795794168844),
    MAAC_FLT_C(64684.058581965895),MAAC_FLT_C(64705.323117269661),MAAC_FLT_C(64726.589399792974),MAAC_FLT_C(64747.857429248776),
    MAAC_FLT_C(64769.127205350138),MAAC_FLT_C(64790.398727810236),MAAC_FLT_C(64811.671996342375),MAAC_FLT_C(64832.947010659969),
    MAAC_FLT_C(64854.223770476558),MAAC_FLT_C(64875.502275505794),MAAC_FLT_C(64896.782525461451),MAAC_FLT_C(64918.064520057414),
    MAAC_FLT_C(64939.348259007682),MAAC_FLT_C(64960.633742026388),MAAC_FLT_C(64981.920968827762),MAAC_FLT_C(65003.209939126165),
    MAAC_FLT_C(65024.500652636067),MAAC_FLT_C(65045.793109072067),MAAC_FLT_C(65067.087308148861),MAAC_FLT_C(65088.383249581282),
    MAAC_FLT_C(65109.680933084259),MAAC_FLT_C(65130.980358372864),MAAC_FLT_C(65152.281525162260),MAAC_FLT_C(65173.584433167736),
    MAAC_FLT_C(65194.889082104703),MAAC_FLT_C(65216.195471688683),MAAC_FLT_C(65237.503601635319),MAAC_FLT_C(65258.813471660353),
    MAAC_FLT_C(65280.125081479666),MAAC_FLT_C(65301.438430809241),MAAC_FLT_C(65322.753519365178),MAAC_FLT_C(65344.070346863708),
    MAAC_FLT_C(65365.388913021146),MAAC_FLT_C(65386.709217553958),MAAC_FLT_C(65408.031260178701),MAAC_FLT_C(65429.355040612056),
    MAAC_FLT_C(65450.680558570821),MAAC_FLT_C(65472.007813771910),MAAC_FLT_C(65493.336805932355),MAAC_FLT_C(65514.667534769280),
    MAAC_FLT_C(65535.999999999956),MAAC_FLT_C(65557.334201341757),MAAC_FLT_C(65578.670138512171),MAAC_FLT_C(65600.007811228788),
    MAAC_FLT_C(65621.347219209332),MAAC_FLT_C(65642.688362171626),MAAC_FLT_C(65664.031239833639),MAAC_FLT_C(65685.375851913413),
    MAAC_FLT_C(65706.722198129137),MAAC_FLT_C(65728.070278199084),MAAC_FLT_C(65749.420091841661),MAAC_FLT_C(65770.771638775404),
    MAAC_FLT_C(65792.124918718939),MAAC_FLT_C(65813.479931391004),MAAC_FLT_C(65834.836676510458),MAAC_FLT_C(65856.195153796303),
    MAAC_FLT_C(65877.555362967600),MAAC_FLT_C(65898.917303743554),MAAC_FLT_C(65920.280975843489),MAAC_FLT_C(65941.646378986843),
    MAAC_FLT_C(65963.013512893158),MAAC_FLT_C(65984.382377282076),MAAC_FLT_C(66005.752971873386),MAAC_FLT_C(66027.125296386963),
    MAAC_FLT_C(66048.499350542799),MAAC_FLT_C(66069.875134061018),MAAC_FLT_C(66091.252646661844),MAAC_FLT_C(66112.631888065618),
    MAAC_FLT_C(66134.012857992770),MAAC_FLT_C(66155.395556163887),MAAC_FLT_C(66176.779982299631),MAAC_FLT_C(66198.166136120795),
    MAAC_FLT_C(66219.554017348273),MAAC_FLT_C(66240.943625703105),MAAC_FLT_C(66262.334960906388),MAAC_FLT_C(66283.728022679396),
    MAAC_FLT_C(66305.122810743444),MAAC_FLT_C(66326.519324820023),MAAC_FLT_C(66347.917564630698),MAAC_FLT_C(66369.317529897162),
    MAAC_FLT_C(66390.719220341227),MAAC_FLT_C(66412.122635684791),MAAC_FLT_C(66433.527775649884),MAAC_FLT_C(66454.934639958636),
    MAAC_FLT_C(66476.343228333324),MAAC_FLT_C(66497.753540496284),MAAC_FLT_C(66519.165576169995),MAAC_FLT_C(66540.579335077040),
    MAAC_FLT_C(66561.994816940118),MAAC_FLT_C(66583.412021482043),MAAC_FLT_C(66604.830948425733),MAAC_FLT_C(66626.251597494222),
    MAAC_FLT_C(66647.673968410629),MAAC_FLT_C(66669.098060898235),MAAC_FLT_C(66690.523874680381),MAAC_FLT_C(66711.951409480564),
    MAAC_FLT_C(66733.380665022371),MAAC_FLT_C(66754.811641029475),MAAC_FLT_C(66776.244337225711),MAAC_FLT_C(66797.678753334985),
    MAAC_FLT_C(66819.114889081320),MAAC_FLT_C(66840.552744188884),MAAC_FLT_C(66861.992318381905),MAAC_FLT_C(66883.433611384738),
    MAAC_FLT_C(66904.876622921889),MAAC_FLT_C(66926.321352717903),MAAC_FLT_C(66947.767800497502),MAAC_FLT_C(66969.215965985466),
    MAAC_FLT_C(66990.665848906734),MAAC_FLT_C(67012.117448986304),MAAC_FLT_C(67033.570765949335),MAAC_FLT_C(67055.025799521056),
    MAAC_FLT_C(67076.482549426815),MAAC_FLT_C(67097.941015392076),MAAC_FLT_C(67119.401197142433),MAAC_FLT_C(67140.863094403554),
    MAAC_FLT_C(67162.326706901222),MAAC_FLT_C(67183.792034361351),MAAC_FLT_C(67205.259076509959),MAAC_FLT_C(67226.727833073150),
    MAAC_FLT_C(67248.198303777172),MAAC_FLT_C(67269.670488348347),MAAC_FLT_C(67291.144386513144),MAAC_FLT_C(67312.619997998088),
    MAAC_FLT_C(67334.097322529880),MAAC_FLT_C(67355.576359835293),MAAC_FLT_C(67377.057109641188),MAAC_FLT_C(67398.539571674570),
    MAAC_FLT_C(67420.023745662547),MAAC_FLT_C(67441.509631332330),MAAC_FLT_C(67462.997228411230),MAAC_FLT_C(67484.486536626689),
    MAAC_FLT_C(67505.977555706224),MAAC_FLT_C(67527.470285377494),MAAC_FLT_C(67548.964725368263),MAAC_FLT_C(67570.460875406367),
    MAAC_FLT_C(67591.958735219800),MAAC_FLT_C(67613.458304536631),MAAC_FLT_C(67634.959583085030),MAAC_FLT_C(67656.462570593329),
    MAAC_FLT_C(67677.967266789899),MAAC_FLT_C(67699.473671403248),MAAC_FLT_C(67720.981784162024),MAAC_FLT_C(67742.491604794923),
    MAAC_FLT_C(67764.003133030797),MAAC_FLT_C(67785.516368598575),MAAC_FLT_C(67807.031311227314),MAAC_FLT_C(67828.547960646174),
    MAAC_FLT_C(67850.066316584402),MAAC_FLT_C(67871.586378771390),MAAC_FLT_C(67893.108146936589),MAAC_FLT_C(67914.631620809610),
    MAAC_FLT_C(67936.156800120138),MAAC_FLT_C(67957.683684597971),MAAC_FLT_C(67979.212273973011),MAAC_FLT_C(68000.742567975263),
    MAAC_FLT_C(68022.274566334876),MAAC_FLT_C(68043.808268782057),MAAC_FLT_C(68065.343675047145),MAAC_FLT_C(68086.880784860579),
    MAAC_FLT_C(68108.419597952918),MAAC_FLT_C(68129.960114054789),MAAC_FLT_C(68151.502332896969),MAAC_FLT_C(68173.046254210320),
    MAAC_FLT_C(68194.591877725834),MAAC_FLT_C(68216.139203174564),MAAC_FLT_C(68237.688230287706),MAAC_FLT_C(68259.238958796544),
    MAAC_FLT_C(68280.791388432481),MAAC_FLT_C(68302.345518927032),MAAC_FLT_C(68323.901350011787),MAAC_FLT_C(68345.458881418483),
    MAAC_FLT_C(68367.018112878912),MAAC_FLT_C(68388.579044125028),MAAC_FLT_C(68410.141674888844),MAAC_FLT_C(68431.706004902502),
    MAAC_FLT_C(68453.272033898262),MAAC_FLT_C(68474.839761608455),MAAC_FLT_C(68496.409187765545),MAAC_FLT_C(68517.980312102081),
    MAAC_FLT_C(68539.553134350732),MAAC_FLT_C(68561.127654244279),MAAC_FLT_C(68582.703871515580),MAAC_FLT_C(68604.281785897634),
    MAAC_FLT_C(68625.861397123503),MAAC_FLT_C(68647.442704926390),MAAC_FLT_C(68669.025709039604),MAAC_FLT_C(68690.610409196524),
    MAAC_FLT_C(68712.196805130661),MAAC_FLT_C(68733.784896575627),MAAC_FLT_C(68755.374683265123),MAAC_FLT_C(68776.966164932994),
    MAAC_FLT_C(68798.559341313128),MAAC_FLT_C(68820.154212139591),MAAC_FLT_C(68841.750777146473),MAAC_FLT_C(68863.349036068044),
    MAAC_FLT_C(68884.948988638629),MAAC_FLT_C(68906.550634592684),MAAC_FLT_C(68928.153973664739),MAAC_FLT_C(68949.759005589440),
    MAAC_FLT_C(68971.365730101577),MAAC_FLT_C(68992.974146935987),MAAC_FLT_C(69014.584255827634),MAAC_FLT_C(69036.196056511588),
    MAAC_FLT_C(69057.809548723017),MAAC_FLT_C(69079.424732197207),MAAC_FLT_C(69101.041606669532),MAAC_FLT_C(69122.660171875468),
    MAAC_FLT_C(69144.280427550606),MAAC_FLT_C(69165.902373430625),MAAC_FLT_C(69187.526009251334),MAAC_FLT_C(69209.151334748618),
    MAAC_FLT_C(69230.778349658474),MAAC_FLT_C(69252.407053716990),MAAC_FLT_C(69274.037446660412),MAAC_FLT_C(69295.669528225000),
    MAAC_FLT_C(69317.303298147192),MAAC_FLT_C(69338.938756163494),MAAC_FLT_C(69360.575902010532),MAAC_FLT_C(69382.214735425005),
    MAAC_FLT_C(69403.855256143754),MAAC_FLT_C(69425.497463903681),MAAC_FLT_C(69447.141358441833),MAAC_FLT_C(69468.786939495330),
    MAAC_FLT_C(69490.434206801394),MAAC_FLT_C(69512.083160097391),MAAC_FLT_C(69533.733799120717),MAAC_FLT_C(69555.386123608929),
    MAAC_FLT_C(69577.040133299670),MAAC_FLT_C(69598.695827930685),MAAC_FLT_C(69620.353207239794),MAAC_FLT_C(69642.012270964973),
    MAAC_FLT_C(69663.673018844260),MAAC_FLT_C(69685.335450615792),MAAC_FLT_C(69706.999566017839),MAAC_FLT_C(69728.665364788743),
    MAAC_FLT_C(69750.332846666963),MAAC_FLT_C(69772.002011391058),MAAC_FLT_C(69793.672858699691),MAAC_FLT_C(69815.345388331611),
    MAAC_FLT_C(69837.019600025669),MAAC_FLT_C(69858.695493520849),MAAC_FLT_C(69880.373068556204),MAAC_FLT_C(69902.052324870907),
    MAAC_FLT_C(69923.733262204216),MAAC_FLT_C(69945.415880295492),MAAC_FLT_C(69967.100178884211),MAAC_FLT_C(69988.786157709939),
    MAAC_FLT_C(70010.473816512356),MAAC_FLT_C(70032.163155031216),MAAC_FLT_C(70053.854173006403),MAAC_FLT_C(70075.546870177874),
    MAAC_FLT_C(70097.241246285717),MAAC_FLT_C(70118.937301070109),MAAC_FLT_C(70140.635034271298),MAAC_FLT_C(70162.334445629691),
    MAAC_FLT_C(70184.035534885741),MAAC_FLT_C(70205.738301780017),MAAC_FLT_C(70227.442746053217),MAAC_FLT_C(70249.148867446100),
    MAAC_FLT_C(70270.856665699539),MAAC_FLT_C(70292.566140554511),MAAC_FLT_C(70314.277291752107),MAAC_FLT_C(70335.990119033493),
    MAAC_FLT_C(70357.704622139936),MAAC_FLT_C(70379.420800812819),MAAC_FLT_C(70401.138654793613),MAAC_FLT_C(70422.858183823890),
    MAAC_FLT_C(70444.579387645339),MAAC_FLT_C(70466.302265999722),MAAC_FLT_C(70488.026818628918),MAAC_FLT_C(70509.753045274876),
    MAAC_FLT_C(70531.480945679708),MAAC_FLT_C(70553.210519585555),MAAC_FLT_C(70574.941766734701),MAAC_FLT_C(70596.674686869505),
    MAAC_FLT_C(70618.409279732456),MAAC_FLT_C(70640.145545066101),MAAC_FLT_C(70661.883482613106),MAAC_FLT_C(70683.623092116264),
    MAAC_FLT_C(70705.364373318414),MAAC_FLT_C(70727.107325962526),MAAC_FLT_C(70748.851949791671),MAAC_FLT_C(70770.598244549008),
    MAAC_FLT_C(70792.346209977783),MAAC_FLT_C(70814.095845821372),MAAC_FLT_C(70835.847151823225),MAAC_FLT_C(70857.600127726895),
    MAAC_FLT_C(70879.354773276034),MAAC_FLT_C(70901.111088214413),MAAC_FLT_C(70922.869072285859),MAAC_FLT_C(70944.628725234332),
    MAAC_FLT_C(70966.390046803877),MAAC_FLT_C(70988.153036738629),MAAC_FLT_C(71009.917694782853),MAAC_FLT_C(71031.684020680885),
    MAAC_FLT_C(71053.452014177150),MAAC_FLT_C(71075.221675016204),MAAC_FLT_C(71096.993002942661),MAAC_FLT_C(71118.765997701266),
    MAAC_FLT_C(71140.540659036851),MAAC_FLT_C(71162.316986694335),MAAC_FLT_C(71184.094980418740),MAAC_FLT_C(71205.874639955218),
    MAAC_FLT_C(71227.655965048951),MAAC_FLT_C(71249.438955445294),MAAC_FLT_C(71271.223610889632),MAAC_FLT_C(71293.009931127483),
    MAAC_FLT_C(71314.797915904477),MAAC_FLT_C(71336.587564966307),MAAC_FLT_C(71358.378878058764),MAAC_FLT_C(71380.171854927772),
    MAAC_FLT_C(71401.966495319313),MAAC_FLT_C(71423.762798979486),MAAC_FLT_C(71445.560765654489),MAAC_FLT_C(71467.360395090596),
    MAAC_FLT_C(71489.161687034211),MAAC_FLT_C(71510.964641231811),MAAC_FLT_C(71532.769257429973),MAAC_FLT_C(71554.575535375363),
    MAAC_FLT_C(71576.383474814749),MAAC_FLT_C(71598.193075495030),MAAC_FLT_C(71620.004337163133),MAAC_FLT_C(71641.817259566145),
    MAAC_FLT_C(71663.631842451214),MAAC_FLT_C(71685.448085565600),MAAC_FLT_C(71707.265988656640),MAAC_FLT_C(71729.085551471784),
    MAAC_FLT_C(71750.906773758586),MAAC_FLT_C(71772.729655264673),MAAC_FLT_C(71794.554195737772),MAAC_FLT_C(71816.380394925713),
    MAAC_FLT_C(71838.208252576442),MAAC_FLT_C(71860.037768437964),MAAC_FLT_C(71881.868942258385),MAAC_FLT_C(71903.701773785942),
    MAAC_FLT_C(71925.536262768932),MAAC_FLT_C(71947.372408955751),MAAC_FLT_C(71969.210212094898),MAAC_FLT_C(71991.049671934976),
    MAAC_FLT_C(72012.890788224686),MAAC_FLT_C(72034.733560712790),MAAC_FLT_C(72056.577989148165),MAAC_FLT_C(72078.424073279821),
    MAAC_FLT_C(72100.271812856794),MAAC_FLT_C(72122.121207628254),MAAC_FLT_C(72143.972257343470),MAAC_FLT_C(72165.824961751801),
    MAAC_FLT_C(72187.679320602692),MAAC_FLT_C(72209.535333645690),MAAC_FLT_C(72231.393000630429),MAAC_FLT_C(72253.252321306645),
    MAAC_FLT_C(72275.113295424177),MAAC_FLT_C(72296.975922732949),MAAC_FLT_C(72318.840202982959),MAAC_FLT_C(72340.706135924338),
    MAAC_FLT_C(72362.573721307272),MAAC_FLT_C(72384.442958882093),MAAC_FLT_C(72406.313848399179),MAAC_FLT_C(72428.186389609036),
    MAAC_FLT_C(72450.060582262216),MAAC_FLT_C(72471.936426109431),MAAC_FLT_C(72493.813920901433),MAAC_FLT_C(72515.693066389096),
    MAAC_FLT_C(72537.573862323392),MAAC_FLT_C(72559.456308455352),MAAC_FLT_C(72581.340404536139),MAAC_FLT_C(72603.226150316987),
    MAAC_FLT_C(72625.113545549248),MAAC_FLT_C(72647.002589984331),MAAC_FLT_C(72668.893283373764),MAAC_FLT_C(72690.785625469172),
    MAAC_FLT_C(72712.679616022273),MAAC_FLT_C(72734.575254784853),MAAC_FLT_C(72756.472541508818),MAAC_FLT_C(72778.371475946144),
    MAAC_FLT_C(72800.272057848939),MAAC_FLT_C(72822.174286969355),MAAC_FLT_C(72844.078163059690),MAAC_FLT_C(72865.983685872285),
    MAAC_FLT_C(72887.890855159596),MAAC_FLT_C(72909.799670674183),MAAC_FLT_C(72931.710132168693),MAAC_FLT_C(72953.622239395845),
    MAAC_FLT_C(72975.535992108475),MAAC_FLT_C(72997.451390059519),MAAC_FLT_C(73019.368433001961),MAAC_FLT_C(73041.287120688925),
    MAAC_FLT_C(73063.207452873612),MAAC_FLT_C(73085.129429309294),MAAC_FLT_C(73107.053049749389),MAAC_FLT_C(73128.978313947344),
    MAAC_FLT_C(73150.905221656736),MAAC_FLT_C(73172.833772631217),MAAC_FLT_C(73194.763966624567),MAAC_FLT_C(73216.695803390612),
    MAAC_FLT_C(73238.629282683280),MAAC_FLT_C(73260.564404256627),MAAC_FLT_C(73282.501167864757),MAAC_FLT_C(73304.439573261901),
    MAAC_FLT_C(73326.379620202337),MAAC_FLT_C(73348.321308440485),MAAC_FLT_C(73370.264637730841),MAAC_FLT_C(73392.209607827957),
    MAAC_FLT_C(73414.156218486532),MAAC_FLT_C(73436.104469461323),MAAC_FLT_C(73458.054360507173),MAAC_FLT_C(73480.005891379056),
    MAAC_FLT_C(73501.959061831993),MAAC_FLT_C(73523.913871621116),MAAC_FLT_C(73545.870320501665),MAAC_FLT_C(73567.828408228932),
    MAAC_FLT_C(73589.788134558330),MAAC_FLT_C(73611.749499245358),MAAC_FLT_C(73633.712502045615),MAAC_FLT_C(73655.677142714747),
    MAAC_FLT_C(73677.643421008557),MAAC_FLT_C(73699.611336682879),MAAC_FLT_C(73721.580889493693),MAAC_FLT_C(73743.552079197019),
    MAAC_FLT_C(73765.524905548999),MAAC_FLT_C(73787.499368305856),MAAC_FLT_C(73809.475467223907),MAAC_FLT_C(73831.453202059551),
    MAAC_FLT_C(73853.432572569291),MAAC_FLT_C(73875.413578509717),MAAC_FLT_C(73897.396219637507),MAAC_FLT_C(73919.380495709411),
    MAAC_FLT_C(73941.366406482310),MAAC_FLT_C(73963.353951713143),MAAC_FLT_C(73985.343131158952),MAAC_FLT_C(74007.333944576865),
    MAAC_FLT_C(74029.326391724113),MAAC_FLT_C(74051.320472357969),MAAC_FLT_C(74073.316186235883),MAAC_FLT_C(74095.313533115303),
    MAAC_FLT_C(74117.312512753837),MAAC_FLT_C(74139.313124909138),MAAC_FLT_C(74161.315369338976),MAAC_FLT_C(74183.319245801191),
    MAAC_FLT_C(74205.324754053727),MAAC_FLT_C(74227.331893854629),MAAC_FLT_C(74249.340664961986),MAAC_FLT_C(74271.351067134034),
    MAAC_FLT_C(74293.363100129049),MAAC_FLT_C(74315.376763705441),MAAC_FLT_C(74337.392057621662),MAAC_FLT_C(74359.408981636298),
    MAAC_FLT_C(74381.427535508003),MAAC_FLT_C(74403.447718995507),MAAC_FLT_C(74425.469531857671),MAAC_FLT_C(74447.492973853383),
    MAAC_FLT_C(74469.518044741693),MAAC_FLT_C(74491.544744281680),MAAC_FLT_C(74513.573072232539),MAAC_FLT_C(74535.603028353551),
    MAAC_FLT_C(74557.634612404087),MAAC_FLT_C(74579.667824143602),MAAC_FLT_C(74601.702663331642),MAAC_FLT_C(74623.739129727837),
    MAAC_FLT_C(74645.777223091936),MAAC_FLT_C(74667.816943183716),MAAC_FLT_C(74689.858289763113),MAAC_FLT_C(74711.901262590094),
    MAAC_FLT_C(74733.945861424741),MAAC_FLT_C(74755.992086027225),MAAC_FLT_C(74778.039936157802),MAAC_FLT_C(74800.089411576817),
    MAAC_FLT_C(74822.140512044702),MAAC_FLT_C(74844.193237321961),MAAC_FLT_C(74866.247587169230),MAAC_FLT_C(74888.303561347187),
    MAAC_FLT_C(74910.361159616630),MAAC_FLT_C(74932.420381738411),MAAC_FLT_C(74954.481227473501),MAAC_FLT_C(74976.543696582972),
    MAAC_FLT_C(74998.607788827925),MAAC_FLT_C(75020.673503969607),MAAC_FLT_C(75042.740841769322),MAAC_FLT_C(75064.809801988464),
    MAAC_FLT_C(75086.880384388540),MAAC_FLT_C(75108.952588731103),MAAC_FLT_C(75131.026414777836),MAAC_FLT_C(75153.101862290467),
    MAAC_FLT_C(75175.178931030852),MAAC_FLT_C(75197.257620760924),MAAC_FLT_C(75219.337931242670),MAAC_FLT_C(75241.419862238225),
    MAAC_FLT_C(75263.503413509738),MAAC_FLT_C(75285.588584819503),MAAC_FLT_C(75307.675375929874),MAAC_FLT_C(75329.763786603318),
    MAAC_FLT_C(75351.853816602365),MAAC_FLT_C(75373.945465689612),MAAC_FLT_C(75396.038733627807),MAAC_FLT_C(75418.133620179724),
    MAAC_FLT_C(75440.230125108254),MAAC_FLT_C(75462.328248176360),MAAC_FLT_C(75484.427989147109),MAAC_FLT_C(75506.529347783653),
    MAAC_FLT_C(75528.632323849190),MAAC_FLT_C(75550.736917107075),MAAC_FLT_C(75572.843127320695),MAAC_FLT_C(75594.950954253538),
    MAAC_FLT_C(75617.060397669193),MAAC_FLT_C(75639.171457331307),MAAC_FLT_C(75661.284133003646),MAAC_FLT_C(75683.398424450032),
    MAAC_FLT_C(75705.514331434402),MAAC_FLT_C(75727.631853720741),MAAC_FLT_C(75749.750991073175),MAAC_FLT_C(75771.871743255862),
    MAAC_FLT_C(75793.994110033076),MAAC_FLT_C(75816.118091169177),MAAC_FLT_C(75838.243686428585),MAAC_FLT_C(75860.370895575848),
    MAAC_FLT_C(75882.499718375562),MAAC_FLT_C(75904.630154592422),MAAC_FLT_C(75926.762203991224),MAAC_FLT_C(75948.895866336825),
    MAAC_FLT_C(75971.031141394182),MAAC_FLT_C(75993.168028928325),MAAC_FLT_C(76015.306528704401),MAAC_FLT_C(76037.446640487600),
    MAAC_FLT_C(76059.588364043215),MAAC_FLT_C(76081.731699136653),MAAC_FLT_C(76103.876645533353),MAAC_FLT_C(76126.023202998884),
    MAAC_FLT_C(76148.171371298871),MAAC_FLT_C(76170.321150199044),MAAC_FLT_C(76192.472539465205),MAAC_FLT_C(76214.625538863256),
    MAAC_FLT_C(76236.780148159174),MAAC_FLT_C(76258.936367119008),MAAC_FLT_C(76281.094195508922),MAAC_FLT_C(76303.253633095141),
    MAAC_FLT_C(76325.414679643975),MAAC_FLT_C(76347.577334921851),MAAC_FLT_C(76369.741598695226),MAAC_FLT_C(76391.907470730686),
    MAAC_FLT_C(76414.074950794879),MAAC_FLT_C(76436.244038654564),MAAC_FLT_C(76458.414734076548),MAAC_FLT_C(76480.587036827754),
    MAAC_FLT_C(76502.760946675175),MAAC_FLT_C(76524.936463385893),MAAC_FLT_C(76547.113586727050),MAAC_FLT_C(76569.292316465915),
    MAAC_FLT_C(76591.472652369819),MAAC_FLT_C(76613.654594206164),MAAC_FLT_C(76635.838141742468),MAAC_FLT_C(76658.023294746308),
    MAAC_FLT_C(76680.210052985349),MAAC_FLT_C(76702.398416227341),MAAC_FLT_C(76724.588384240138),MAAC_FLT_C(76746.779956791637),
    MAAC_FLT_C(76768.973133649866),MAAC_FLT_C(76791.167914582897),MAAC_FLT_C(76813.364299358902),MAAC_FLT_C(76835.562287746157),
    MAAC_FLT_C(76857.761879512967),MAAC_FLT_C(76879.963074427797),MAAC_FLT_C(76902.165872259109),MAAC_FLT_C(76924.370272775530),
    MAAC_FLT_C(76946.576275745727),MAAC_FLT_C(76968.783880938441),MAAC_FLT_C(76990.993088122515),MAAC_FLT_C(77013.203897066895),
    MAAC_FLT_C(77035.416307540567),MAAC_FLT_C(77057.630319312622),MAAC_FLT_C(77079.845932152239),MAAC_FLT_C(77102.063145828695),
    MAAC_FLT_C(77124.281960111301),MAAC_FLT_C(77146.502374769480),MAAC_FLT_C(77168.724389572759),MAAC_FLT_C(77190.948004290723),
    MAAC_FLT_C(77213.173218693031),MAAC_FLT_C(77235.400032549442),MAAC_FLT_C(77257.628445629802),MAAC_FLT_C(77279.858457704031),
    MAAC_FLT_C(77302.090068542122),MAAC_FLT_C(77324.323277914169),MAAC_FLT_C(77346.558085590339),MAAC_FLT_C(77368.794491340886),
    MAAC_FLT_C(77391.032494936138),MAAC_FLT_C(77413.272096146524),MAAC_FLT_C(77435.513294742530),MAAC_FLT_C(77457.756090494731),
    MAAC_FLT_C(77480.000483173804),MAAC_FLT_C(77502.246472550498),MAAC_FLT_C(77524.494058395634),MAAC_FLT_C(77546.743240480107),
    MAAC_FLT_C(77568.994018574944),MAAC_FLT_C(77591.246392451198),MAAC_FLT_C(77613.500361880026),MAAC_FLT_C(77635.755926632657),
    MAAC_FLT_C(77658.013086480438),MAAC_FLT_C(77680.271841194757),MAAC_FLT_C(77702.532190547092),MAAC_FLT_C(77724.794134309021),
    MAAC_FLT_C(77747.057672252195),MAAC_FLT_C(77769.322804148323),MAAC_FLT_C(77791.589529769248),MAAC_FLT_C(77813.857848886837),
    MAAC_FLT_C(77836.127761273063),MAAC_FLT_C(77858.399266699998),MAAC_FLT_C(77880.672364939790),MAAC_FLT_C(77902.947055764627),
    MAAC_FLT_C(77925.223338946831),MAAC_FLT_C(77947.501214258780),MAAC_FLT_C(77969.780681472927),MAAC_FLT_C(77992.061740361838),
    MAAC_FLT_C(78014.344390698127),MAAC_FLT_C(78036.628632254491),MAAC_FLT_C(78058.914464803747),MAAC_FLT_C(78081.201888118725),
    MAAC_FLT_C(78103.490901972415),MAAC_FLT_C(78125.781506137821),MAAC_FLT_C(78148.073700388064),MAAC_FLT_C(78170.367484496339),
    MAAC_FLT_C(78192.662858235926),MAAC_FLT_C(78214.959821380166),MAAC_FLT_C(78237.258373702498),MAAC_FLT_C(78259.558514976452),
    MAAC_FLT_C(78281.860244975614),MAAC_FLT_C(78304.163563473659),MAAC_FLT_C(78326.468470244363),MAAC_FLT_C(78348.774965061530),
    MAAC_FLT_C(78371.083047699125),MAAC_FLT_C(78393.392717931099),MAAC_FLT_C(78415.703975531578),MAAC_FLT_C(78438.016820274701),
    MAAC_FLT_C(78460.331251934695),MAAC_FLT_C(78482.647270285903),MAAC_FLT_C(78504.964875102727),MAAC_FLT_C(78527.284066159627),
    MAAC_FLT_C(78549.604843231195),MAAC_FLT_C(78571.927206092048),MAAC_FLT_C(78594.251154516911),MAAC_FLT_C(78616.576688280606),
    MAAC_FLT_C(78638.903807157985),MAAC_FLT_C(78661.232510924034),MAAC_FLT_C(78683.562799353778),MAAC_FLT_C(78705.894672222348),
    MAAC_FLT_C(78728.228129304945),MAAC_FLT_C(78750.563170376859),MAAC_FLT_C(78772.899795213423),MAAC_FLT_C(78795.238003590101),
    MAAC_FLT_C(78817.577795282399),MAAC_FLT_C(78839.919170065928),MAAC_FLT_C(78862.262127716356),MAAC_FLT_C(78884.606668009452),
    MAAC_FLT_C(78906.952790721043),MAAC_FLT_C(78929.300495627045),MAAC_FLT_C(78951.649782503460),MAAC_FLT_C(78974.000651126378),
    MAAC_FLT_C(78996.353101271932),MAAC_FLT_C(79018.707132716358),MAAC_FLT_C(79041.062745235977),MAAC_FLT_C(79063.419938607185),
    MAAC_FLT_C(79085.778712606436),MAAC_FLT_C(79108.139067010285),MAAC_FLT_C(79130.501001595389),MAAC_FLT_C(79152.864516138419),
    MAAC_FLT_C(79175.229610416180),MAAC_FLT_C(79197.596284205531),MAAC_FLT_C(79219.964537283420),MAAC_FLT_C(79242.334369426870),
    MAAC_FLT_C(79264.705780412987),MAAC_FLT_C(79287.078770018954),MAAC_FLT_C(79309.453338022009),MAAC_FLT_C(79331.829484199508),
    MAAC_FLT_C(79354.207208328866),MAAC_FLT_C(79376.586510187582),MAAC_FLT_C(79398.967389553218),MAAC_FLT_C(79421.349846203433),
    MAAC_FLT_C(79443.733879915948),MAAC_FLT_C(79466.119490468584),MAAC_FLT_C(79488.506677639220),MAAC_FLT_C(79510.895441205823),
    MAAC_FLT_C(79533.285780946433),MAAC_FLT_C(79555.677696639163),MAAC_FLT_C(79578.071188062226),MAAC_FLT_C(79600.466254993895),
    MAAC_FLT_C(79622.862897212515),MAAC_FLT_C(79645.261114496549),MAAC_FLT_C(79667.660906624471),MAAC_FLT_C(79690.062273374875),
    MAAC_FLT_C(79712.465214526455),MAAC_FLT_C(79734.869729857935),MAAC_FLT_C(79757.275819148126),MAAC_FLT_C(79779.683482175955),
    MAAC_FLT_C(79802.092718720378),MAAC_FLT_C(79824.503528560454),MAAC_FLT_C(79846.915911475327),MAAC_FLT_C(79869.329867244189),
    MAAC_FLT_C(79891.745395646343),MAAC_FLT_C(79914.162496461155),MAAC_FLT_C(79936.581169468045),MAAC_FLT_C(79959.001414446553),
    MAAC_FLT_C(79981.423231176261),MAAC_FLT_C(80003.846619436852),MAAC_FLT_C(80026.271579008084),MAAC_FLT_C(80048.698109669771),
    MAAC_FLT_C(80071.126211201830),MAAC_FLT_C(80093.555883384237),MAAC_FLT_C(80115.987125997053),MAAC_FLT_C(80138.419938820414),
    MAAC_FLT_C(80160.854321634528),MAAC_FLT_C(80183.290274219689),MAAC_FLT_C(80205.727796356281),MAAC_FLT_C(80228.166887824715),
    MAAC_FLT_C(80250.607548405533),MAAC_FLT_C(80273.049777879336),MAAC_FLT_C(80295.493576026798),MAAC_FLT_C(80317.938942628651),
    MAAC_FLT_C(80340.385877465727),MAAC_FLT_C(80362.834380318949),MAAC_FLT_C(80385.284450969280),MAAC_FLT_C(80407.736089197788),
    MAAC_FLT_C(80430.189294785596),MAAC_FLT_C(80452.644067513917),MAAC_FLT_C(80475.100407164035),MAAC_FLT_C(80497.558313517322),
    MAAC_FLT_C(80520.017786355209),MAAC_FLT_C(80542.478825459213),MAAC_FLT_C(80564.941430610925),MAAC_FLT_C(80587.405601592007),
    MAAC_FLT_C(80609.871338184195),MAAC_FLT_C(80632.338640169328),MAAC_FLT_C(80654.807507329300),MAAC_FLT_C(80677.277939446067),
    MAAC_FLT_C(80699.749936301683),MAAC_FLT_C(80722.223497678278),MAAC_FLT_C(80744.698623358039),MAAC_FLT_C(80767.175313123240),
    MAAC_FLT_C(80789.653566756242),MAAC_FLT_C(80812.133384039465),MAAC_FLT_C(80834.614764755403),MAAC_FLT_C(80857.097708686648),
    MAAC_FLT_C(80879.582215615854),MAAC_FLT_C(80902.068285325731),MAAC_FLT_C(80924.555917599093),MAAC_FLT_C(80947.045112218824),
    MAAC_FLT_C(80969.535868967883),MAAC_FLT_C(80992.028187629272),MAAC_FLT_C(81014.522067986123),MAAC_FLT_C(81037.017509821613),
    MAAC_FLT_C(81059.514512919006),MAAC_FLT_C(81082.013077061609),MAAC_FLT_C(81104.513202032831),MAAC_FLT_C(81127.014887616184),
    MAAC_FLT_C(81149.518133595193),MAAC_FLT_C(81172.022939753500),MAAC_FLT_C(81194.529305874807),MAAC_FLT_C(81217.037231742899),
    MAAC_FLT_C(81239.546717141639),MAAC_FLT_C(81262.057761854958),MAAC_FLT_C(81284.570365666848),MAAC_FLT_C(81307.084528361403),
    MAAC_FLT_C(81329.600249722775),MAAC_FLT_C(81352.117529535186),MAAC_FLT_C(81374.636367582949),MAAC_FLT_C(81397.156763650448),
    MAAC_FLT_C(81419.678717522125),MAAC_FLT_C(81442.202228982511),MAAC_FLT_C(81464.727297816222),MAAC_FLT_C(81487.253923807933),
    MAAC_FLT_C(81509.782106742379),MAAC_FLT_C(81532.311846404395),MAAC_FLT_C(81554.843142578902),MAAC_FLT_C(81577.375995050839),
    MAAC_FLT_C(81599.910403605274),MAAC_FLT_C(81622.446368027333),MAAC_FLT_C(81644.983888102215),MAAC_FLT_C(81667.522963615178),
    MAAC_FLT_C(81690.063594351581),MAAC_FLT_C(81712.605780096841),MAAC_FLT_C(81735.149520636449),MAAC_FLT_C(81757.694815755967),
    MAAC_FLT_C(81780.241665241047),MAAC_FLT_C(81802.790068877410),MAAC_FLT_C(81825.340026450824),MAAC_FLT_C(81847.891537747171),
    MAAC_FLT_C(81870.444602552379),MAAC_FLT_C(81892.999220652477),MAAC_FLT_C(81915.555391833506),MAAC_FLT_C(81938.113115881672),
    MAAC_FLT_C(81960.672392583176),MAAC_FLT_C(81983.233221724338),MAAC_FLT_C(82005.795603091537),MAAC_FLT_C(82028.359536471224),
    MAAC_FLT_C(82050.925021649906),MAAC_FLT_C(82073.492058414209),MAAC_FLT_C(82096.060646550788),MAAC_FLT_C(82118.630785846399),
    MAAC_FLT_C(82141.202476087841),MAAC_FLT_C(82163.775717062032),MAAC_FLT_C(82186.350508555930),MAAC_FLT_C(82208.926850356569),
    MAAC_FLT_C(82231.504742251054),MAAC_FLT_C(82254.084184026578),MAAC_FLT_C(82276.665175470393),MAAC_FLT_C(82299.247716369850),
    MAAC_FLT_C(82321.831806512317),MAAC_FLT_C(82344.417445685307),MAAC_FLT_C(82367.004633676348),MAAC_FLT_C(82389.593370273054),
    MAAC_FLT_C(82412.183655263143),MAAC_FLT_C(82434.775488434374),MAAC_FLT_C(82457.368869574595),MAAC_FLT_C(82479.963798471697),
    MAAC_FLT_C(82502.560274913689),MAAC_FLT_C(82525.158298688606),MAAC_FLT_C(82547.757869584602),MAAC_FLT_C(82570.358987389860),
    MAAC_FLT_C(82592.961651892678),MAAC_FLT_C(82615.565862881398),MAAC_FLT_C(82638.171620144421),MAAC_FLT_C(82660.778923470265),
    MAAC_FLT_C(82683.387772647475),MAAC_FLT_C(82705.998167464713),MAAC_FLT_C(82728.610107710658),MAAC_FLT_C(82751.223593174116),
    MAAC_FLT_C(82773.838623643940),MAAC_FLT_C(82796.455198909040),MAAC_FLT_C(82819.073318758441),MAAC_FLT_C(82841.692982981185),
    MAAC_FLT_C(82864.314191366429),MAAC_FLT_C(82886.936943703375),MAAC_FLT_C(82909.561239781324),MAAC_FLT_C(82932.187079389638),
    MAAC_FLT_C(82954.814462317736),MAAC_FLT_C(82977.443388355125),MAAC_FLT_C(83000.073857291369),MAAC_FLT_C(83022.705868916120),
    MAAC_FLT_C(83045.339423019104),MAAC_FLT_C(83067.974519390089),MAAC_FLT_C(83090.611157818959),MAAC_FLT_C(83113.249338095629),
    MAAC_FLT_C(83135.889060010100),MAAC_FLT_C(83158.530323352461),MAAC_FLT_C(83181.173127912858),MAAC_FLT_C(83203.817473481497),
    MAAC_FLT_C(83226.463359848669),MAAC_FLT_C(83249.110786804740),MAAC_FLT_C(83271.759754140134),MAAC_FLT_C(83294.410261645375),
    MAAC_FLT_C(83317.062309111003),MAAC_FLT_C(83339.715896327703),MAAC_FLT_C(83362.371023086162),MAAC_FLT_C(83385.027689177165),
    MAAC_FLT_C(83407.685894391587),MAAC_FLT_C(83430.345638520361),MAAC_FLT_C(83453.006921354478),MAAC_FLT_C(83475.669742685001),
    MAAC_FLT_C(83498.334102303095),MAAC_FLT_C(83520.999999999942),MAAC_FLT_C(83543.667435566866),MAAC_FLT_C(83566.336408795192),
    MAAC_FLT_C(83589.006919476349),MAAC_FLT_C(83611.678967401851),MAAC_FLT_C(83634.352552363242),MAAC_FLT_C(83657.027674152167),
    MAAC_FLT_C(83679.704332560359),MAAC_FLT_C(83702.382527379552),MAAC_FLT_C(83725.062258401638),MAAC_FLT_C(83747.743525418511),
    MAAC_FLT_C(83770.426328222180),MAAC_FLT_C(83793.110666604684),MAAC_FLT_C(83815.796540358162),MAAC_FLT_C(83838.483949274829),
    MAAC_FLT_C(83861.172893146941),MAAC_FLT_C(83883.863371766842),MAAC_FLT_C(83906.555384926964),MAAC_FLT_C(83929.248932419752),
    MAAC_FLT_C(83951.944014037799),MAAC_FLT_C(83974.640629573696),MAAC_FLT_C(83997.338778820151),MAAC_FLT_C(84020.038461569929),
    MAAC_FLT_C(84042.739677615857),MAAC_FLT_C(84065.442426750829),MAAC_FLT_C(84088.146708767847),MAAC_FLT_C(84110.852523459922),
    MAAC_FLT_C(84133.559870620171),MAAC_FLT_C(84156.268750041796),MAAC_FLT_C(84178.979161518029),MAAC_FLT_C(84201.691104842204),
    MAAC_FLT_C(84224.404579807713),MAAC_FLT_C(84247.119586208006),MAAC_FLT_C(84269.836123836620),MAAC_FLT_C(84292.554192487150),
    MAAC_FLT_C(84315.273791953281),MAAC_FLT_C(84337.994922028738),MAAC_FLT_C(84360.717582507335),MAAC_FLT_C(84383.441773182945),
    MAAC_FLT_C(84406.167493849513),MAAC_FLT_C(84428.894744301069),MAAC_FLT_C(84451.623524331691),MAAC_FLT_C(84474.353833735542),
    MAAC_FLT_C(84497.085672306828),MAAC_FLT_C(84519.819039839873),MAAC_FLT_C(84542.553936128999),MAAC_FLT_C(84565.290360968676),
    MAAC_FLT_C(84588.028314153402),MAAC_FLT_C(84610.767795477717),MAAC_FLT_C(84633.508804736295),MAAC_FLT_C(84656.251341723822),
    MAAC_FLT_C(84678.995406235073),MAAC_FLT_C(84701.740998064910),MAAC_FLT_C(84724.488117008252),MAAC_FLT_C(84747.236762860062),
    MAAC_FLT_C(84769.986935415407),MAAC_FLT_C(84792.738634469410),MAAC_FLT_C(84815.491859817252),MAAC_FLT_C(84838.246611254188),
    MAAC_FLT_C(84861.002888575560),MAAC_FLT_C(84883.760691576768),MAAC_FLT_C(84906.520020053256),MAAC_FLT_C(84929.280873800570),
    MAAC_FLT_C(84952.043252614312),MAAC_FLT_C(84974.807156290146),MAAC_FLT_C(84997.572584623806),MAAC_FLT_C(85020.339537411113),
    MAAC_FLT_C(85043.108014447949),MAAC_FLT_C(85065.878015530237),MAAC_FLT_C(85088.649540453989),MAAC_FLT_C(85111.422589015303),
    MAAC_FLT_C(85134.197161010321),MAAC_FLT_C(85156.973256235244),MAAC_FLT_C(85179.750874486374),MAAC_FLT_C(85202.530015560071),
    MAAC_FLT_C(85225.310679252740),MAAC_FLT_C(85248.092865360857),MAAC_FLT_C(85270.876573681016),MAAC_FLT_C(85293.661804009811),
    MAAC_FLT_C(85316.448556143951),MAAC_FLT_C(85339.236829880188),MAAC_FLT_C(85362.026625015351),MAAC_FLT_C(85384.817941346351),
    MAAC_FLT_C(85407.610778670132),MAAC_FLT_C(85430.405136783724),MAAC_FLT_C(85453.201015484257),MAAC_FLT_C(85475.998414568865),
    MAAC_FLT_C(85498.797333834795),MAAC_FLT_C(85521.597773079353),MAAC_FLT_C(85544.399732099904),MAAC_FLT_C(85567.203210693886),
    MAAC_FLT_C(85590.008208658808),MAAC_FLT_C(85612.814725792239),MAAC_FLT_C(85635.622761891820),MAAC_FLT_C(85658.432316755265),
    MAAC_FLT_C(85681.243390180331),MAAC_FLT_C(85704.055981964877),MAAC_FLT_C(85726.870091906807),MAAC_FLT_C(85749.685719804082),
    MAAC_FLT_C(85772.502865454764),MAAC_FLT_C(85795.321528656961),MAAC_FLT_C(85818.141709208852),MAAC_FLT_C(85840.963406908675),
    MAAC_FLT_C(85863.786621554740),MAAC_FLT_C(85886.611352945445),MAAC_FLT_C(85909.437600879217),MAAC_FLT_C(85932.265365154570),
    MAAC_FLT_C(85955.094645570091),MAAC_FLT_C(85977.925441924410),MAAC_FLT_C(86000.757754016275),MAAC_FLT_C(86023.591581644432),
    MAAC_FLT_C(86046.426924607746),MAAC_FLT_C(86069.263782705122),MAAC_FLT_C(86092.102155735556),MAAC_FLT_C(86114.942043498071),
    MAAC_FLT_C(86137.783445791807),MAAC_FLT_C(86160.626362415918),MAAC_FLT_C(86183.470793169676),MAAC_FLT_C(86206.316737852379),
    MAAC_FLT_C(86229.164196263402),MAAC_FLT_C(86252.013168202204),MAAC_FLT_C(86274.863653468303),MAAC_FLT_C(86297.715651861261),
    MAAC_FLT_C(86320.569163180728),MAAC_FLT_C(86343.424187226425),MAAC_FLT_C(86366.280723798118),MAAC_FLT_C(86389.138772695675),
    MAAC_FLT_C(86411.998333718977),MAAC_FLT_C(86434.859406668009),MAAC_FLT_C(86457.721991342827),MAAC_FLT_C(86480.586087543532),
    MAAC_FLT_C(86503.451695070296),MAAC_FLT_C(86526.318813723352),MAAC_FLT_C(86549.187443303032),MAAC_FLT_C(86572.057583609683),
    MAAC_FLT_C(86594.929234443771),MAAC_FLT_C(86617.802395605773),MAAC_FLT_C(86640.677066896271),MAAC_FLT_C(86663.553248115903),
    MAAC_FLT_C(86686.430939065380),MAAC_FLT_C(86709.310139545443),MAAC_FLT_C(86732.190849356964),MAAC_FLT_C(86755.073068300801),
    MAAC_FLT_C(86777.956796177954),MAAC_FLT_C(86800.842032789442),MAAC_FLT_C(86823.728777936354),MAAC_FLT_C(86846.617031419853),
    MAAC_FLT_C(86869.506793041175),MAAC_FLT_C(86892.398062601613),MAAC_FLT_C(86915.290839902518),MAAC_FLT_C(86938.185124745316),
    MAAC_FLT_C(86961.080916931489),MAAC_FLT_C(86983.978216262607),MAAC_FLT_C(87006.877022540270),MAAC_FLT_C(87029.777335566177),
    MAAC_FLT_C(87052.679155142090),MAAC_FLT_C(87075.582481069796),MAAC_FLT_C(87098.487313151185),MAAC_FLT_C(87121.393651188220),
    MAAC_FLT_C(87144.301494982894),MAAC_FLT_C(87167.210844337285),MAAC_FLT_C(87190.121699053532),MAAC_FLT_C(87213.034058933845),
    MAAC_FLT_C(87235.947923780506),MAAC_FLT_C(87258.863293395829),MAAC_FLT_C(87281.780167582241),MAAC_FLT_C(87304.698546142172),
    MAAC_FLT_C(87327.618428878181),MAAC_FLT_C(87350.539815592856),MAAC_FLT_C(87373.462706088845),MAAC_FLT_C(87396.387100168897),
    MAAC_FLT_C(87419.312997635774),MAAC_FLT_C(87442.240398292357),MAAC_FLT_C(87465.169301941540),MAAC_FLT_C(87488.099708386319),
    MAAC_FLT_C(87511.031617429733),MAAC_FLT_C(87533.965028874896),MAAC_FLT_C(87556.899942525008),MAAC_FLT_C(87579.836358183282),
    MAAC_FLT_C(87602.774275653021),MAAC_FLT_C(87625.713694737613),MAAC_FLT_C(87648.654615240492),MAAC_FLT_C(87671.597036965148),
    MAAC_FLT_C(87694.540959715145),MAAC_FLT_C(87717.486383294105),MAAC_FLT_C(87740.433307505737),MAAC_FLT_C(87763.381732153779),
    MAAC_FLT_C(87786.331657042057),MAAC_FLT_C(87809.283081974456),MAAC_FLT_C(87832.236006754916),MAAC_FLT_C(87855.190431187453),
    MAAC_FLT_C(87878.146355076155),MAAC_FLT_C(87901.103778225151),MAAC_FLT_C(87924.062700438633),MAAC_FLT_C(87947.023121520891),
    MAAC_FLT_C(87969.985041276246),MAAC_FLT_C(87992.948459509105),MAAC_FLT_C(88015.913376023906),MAAC_FLT_C(88038.879790625171),
    MAAC_FLT_C(88061.847703117513),MAAC_FLT_C(88084.817113305573),MAAC_FLT_C(88107.788020994049),MAAC_FLT_C(88130.760425987726),
    MAAC_FLT_C(88153.734328091465),MAAC_FLT_C(88176.709727110137),MAAC_FLT_C(88199.686622848749),MAAC_FLT_C(88222.665015112303),
    MAAC_FLT_C(88245.644903705906),MAAC_FLT_C(88268.626288434709),MAAC_FLT_C(88291.609169103947),MAAC_FLT_C(88314.593545518903),
    MAAC_FLT_C(88337.579417484914),MAAC_FLT_C(88360.566784807408),MAAC_FLT_C(88383.555647291854),MAAC_FLT_C(88406.546004743795),
    MAAC_FLT_C(88429.537856968818),MAAC_FLT_C(88452.531203772611),MAAC_FLT_C(88475.526044960890),MAAC_FLT_C(88498.522380339447),
    MAAC_FLT_C(88521.520209714130),MAAC_FLT_C(88544.519532890874),MAAC_FLT_C(88567.520349675659),MAAC_FLT_C(88590.522659874507),
    MAAC_FLT_C(88613.526463293543),MAAC_FLT_C(88636.531759738922),MAAC_FLT_C(88659.538549016899),MAAC_FLT_C(88682.546830933745),
    MAAC_FLT_C(88705.556605295846),MAAC_FLT_C(88728.567871909589),MAAC_FLT_C(88751.580630581491),MAAC_FLT_C(88774.594881118071),
    MAAC_FLT_C(88797.610623325963),MAAC_FLT_C(88820.627857011830),MAAC_FLT_C(88843.646581982393),MAAC_FLT_C(88866.666798044462),
    MAAC_FLT_C(88889.688505004888),MAAC_FLT_C(88912.711702670611),MAAC_FLT_C(88935.736390848600),MAAC_FLT_C(88958.762569345898),
    MAAC_FLT_C(88981.790237969632),MAAC_FLT_C(89004.819396526960),MAAC_FLT_C(89027.850044825114),MAAC_FLT_C(89050.882182671412),
    MAAC_FLT_C(89073.915809873200),MAAC_FLT_C(89096.950926237885),MAAC_FLT_C(89119.987531572973),MAAC_FLT_C(89143.025625686001),
    MAAC_FLT_C(89166.065208384563),MAAC_FLT_C(89189.106279476357),MAAC_FLT_C(89212.148838769106),MAAC_FLT_C(89235.192886070581),
    MAAC_FLT_C(89258.238421188667),MAAC_FLT_C(89281.285443931265),MAAC_FLT_C(89304.333954106376),MAAC_FLT_C(89327.383951522017),
    MAAC_FLT_C(89350.435435986306),MAAC_FLT_C(89373.488407307406),MAAC_FLT_C(89396.542865293537),MAAC_FLT_C(89419.598809753006),
    MAAC_FLT_C(89442.656240494165),MAAC_FLT_C(89465.715157325394),MAAC_FLT_C(89488.775560055219),MAAC_FLT_C(89511.837448492137),
    MAAC_FLT_C(89534.900822444746),MAAC_FLT_C(89557.965681721733),MAAC_FLT_C(89581.032026131812),MAAC_FLT_C(89604.099855483742),
    MAAC_FLT_C(89627.169169586399),MAAC_FLT_C(89650.239968248672),MAAC_FLT_C(89673.312251279538),MAAC_FLT_C(89696.386018488018),
    MAAC_FLT_C(89719.461269683205),MAAC_FLT_C(89742.538004674250),MAAC_FLT_C(89765.616223270365),MAAC_FLT_C(89788.695925280830),
    MAAC_FLT_C(89811.777110514988),MAAC_FLT_C(89834.859778782207),MAAC_FLT_C(89857.943929891975),MAAC_FLT_C(89881.029563653807),
    MAAC_FLT_C(89904.116679877261),MAAC_FLT_C(89927.205278372014),MAAC_FLT_C(89950.295358947740),MAAC_FLT_C(89973.386921414218),
    MAAC_FLT_C(89996.479965581268),MAAC_FLT_C(90019.574491258783),MAAC_FLT_C(90042.670498256688),MAAC_FLT_C(90065.767986385021),
    MAAC_FLT_C(90088.866955453836),MAAC_FLT_C(90111.967405273259),MAAC_FLT_C(90135.069335653476),MAAC_FLT_C(90158.172746404758),
    MAAC_FLT_C(90181.277637337407),MAAC_FLT_C(90204.384008261797),MAAC_FLT_C(90227.491858988360),MAAC_FLT_C(90250.601189327586),
    MAAC_FLT_C(90273.711999090039),MAAC_FLT_C(90296.824288086325),MAAC_FLT_C(90319.938056127125),MAAC_FLT_C(90343.053303023189),
    MAAC_FLT_C(90366.170028585286),MAAC_FLT_C(90389.288232624298),MAAC_FLT_C(90412.407914951138),MAAC_FLT_C(90435.529075376777),
    MAAC_FLT_C(90458.651713712257),MAAC_FLT_C(90481.775829768681),MAAC_FLT_C(90504.901423357209),MAAC_FLT_C(90528.028494289058),
    MAAC_FLT_C(90551.157042375504),MAAC_FLT_C(90574.287067427911),MAAC_FLT_C(90597.418569257643),MAAC_FLT_C(90620.551547676194),
    MAAC_FLT_C(90643.686002495073),MAAC_FLT_C(90666.821933525847),MAAC_FLT_C(90689.959340580186),MAAC_FLT_C(90713.098223469773),
    MAAC_FLT_C(90736.238582006365),MAAC_FLT_C(90759.380416001804),MAAC_FLT_C(90782.523725267951),MAAC_FLT_C(90805.668509616764),
    MAAC_FLT_C(90828.814768860233),MAAC_FLT_C(90851.962502810435),MAAC_FLT_C(90875.111711279460),MAAC_FLT_C(90898.262394079531),
    MAAC_FLT_C(90921.414551022855),MAAC_FLT_C(90944.568181921743),MAAC_FLT_C(90967.723286588560),MAAC_FLT_C(90990.879864835719),
    MAAC_FLT_C(91014.037916475718),MAAC_FLT_C(91037.197441321070),MAAC_FLT_C(91060.358439184391),MAAC_FLT_C(91083.520909878338),
    MAAC_FLT_C(91106.684853215629),MAAC_FLT_C(91129.850269009039),MAAC_FLT_C(91153.017157071401),MAAC_FLT_C(91176.185517215621),
    MAAC_FLT_C(91199.355349254649),MAAC_FLT_C(91222.526653001492),MAAC_FLT_C(91245.699428269247),MAAC_FLT_C(91268.873674871036),
    MAAC_FLT_C(91292.049392620058),MAAC_FLT_C(91315.226581329553),MAAC_FLT_C(91338.405240812834),MAAC_FLT_C(91361.585370883287),
    MAAC_FLT_C(91384.766971354344),MAAC_FLT_C(91407.950042039476),MAAC_FLT_C(91431.134582752245),MAAC_FLT_C(91454.320593306256),
    MAAC_FLT_C(91477.508073515171),MAAC_FLT_C(91500.697023192726),MAAC_FLT_C(91523.887442152685),MAAC_FLT_C(91547.079330208930),
    MAAC_FLT_C(91570.272687175326),MAAC_FLT_C(91593.467512865856),MAAC_FLT_C(91616.663807094534),MAAC_FLT_C(91639.861569675442),
    MAAC_FLT_C(91663.060800422725),MAAC_FLT_C(91686.261499150554),MAAC_FLT_C(91709.463665673218),MAAC_FLT_C(91732.667299805020),
    MAAC_FLT_C(91755.872401360321),MAAC_FLT_C(91779.078970153569),MAAC_FLT_C(91802.287005999257),MAAC_FLT_C(91825.496508711920),
    MAAC_FLT_C(91848.707478106167),MAAC_FLT_C(91871.919913996680),MAAC_FLT_C(91895.133816198169),MAAC_FLT_C(91918.349184525418),
    MAAC_FLT_C(91941.566018793281),MAAC_FLT_C(91964.784318816659),MAAC_FLT_C(91988.004084410495),MAAC_FLT_C(92011.225315389820),
    MAAC_FLT_C(92034.448011569708),MAAC_FLT_C(92057.672172765291),MAAC_FLT_C(92080.897798791746),MAAC_FLT_C(92104.124889464365),
    MAAC_FLT_C(92127.353444598411),MAAC_FLT_C(92150.583464009280),MAAC_FLT_C(92173.814947512379),MAAC_FLT_C(92197.047894923220),
    MAAC_FLT_C(92220.282306057314),MAAC_FLT_C(92243.518180730272),MAAC_FLT_C(92266.755518757753),MAAC_FLT_C(92289.994319955469),
    MAAC_FLT_C(92313.234584139194),MAAC_FLT_C(92336.476311124774),MAAC_FLT_C(92359.719500728082),MAAC_FLT_C(92382.964152765067),
    MAAC_FLT_C(92406.210267051749),MAAC_FLT_C(92429.457843404161),MAAC_FLT_C(92452.706881638471),MAAC_FLT_C(92475.957381570814),
    MAAC_FLT_C(92499.209343017443),MAAC_FLT_C(92522.462765794669),MAAC_FLT_C(92545.717649718819),MAAC_FLT_C(92568.973994606305),
    MAAC_FLT_C(92592.231800273614),MAAC_FLT_C(92615.491066537259),MAAC_FLT_C(92638.751793213829),MAAC_FLT_C(92662.013980119940),
    MAAC_FLT_C(92685.277627072326),MAAC_FLT_C(92708.542733887720),MAAC_FLT_C(92731.809300382956),MAAC_FLT_C(92755.077326374871),
    MAAC_FLT_C(92778.346811680414),MAAC_FLT_C(92801.617756116582),MAAC_FLT_C(92824.890159500384),MAAC_FLT_C(92848.164021648947),
    MAAC_FLT_C(92871.439342379424),MAAC_FLT_C(92894.716121509016),MAAC_FLT_C(92917.994358855023),MAAC_FLT_C(92941.274054234746),
    MAAC_FLT_C(92964.555207465572),MAAC_FLT_C(92987.837818364962),MAAC_FLT_C(93011.121886750407),MAAC_FLT_C(93034.407412439468),
    MAAC_FLT_C(93057.694395249768),MAAC_FLT_C(93080.982834998955),MAAC_FLT_C(93104.272731504767),MAAC_FLT_C(93127.564084584999),
    MAAC_FLT_C(93150.856894057491),MAAC_FLT_C(93174.151159740140),MAAC_FLT_C(93197.446881450916),MAAC_FLT_C(93220.744059007804),
    MAAC_FLT_C(93244.042692228890),MAAC_FLT_C(93267.342780932304),MAAC_FLT_C(93290.644324936235),MAAC_FLT_C(93313.947324058914),
    MAAC_FLT_C(93337.251778118633),MAAC_FLT_C(93360.557686933767),MAAC_FLT_C(93383.865050322696),MAAC_FLT_C(93407.173868103928),
    MAAC_FLT_C(93430.484140095941),MAAC_FLT_C(93453.795866117362),MAAC_FLT_C(93477.109045986799),MAAC_FLT_C(93500.423679522952),
    MAAC_FLT_C(93523.739766544561),MAAC_FLT_C(93547.057306870454),MAAC_FLT_C(93570.376300319491),MAAC_FLT_C(93593.696746710571),
    MAAC_FLT_C(93617.018645862699),MAAC_FLT_C(93640.341997594893),MAAC_FLT_C(93663.666801726227),MAAC_FLT_C(93686.993058075881),
    MAAC_FLT_C(93710.320766463032),MAAC_FLT_C(93733.649926706930),MAAC_FLT_C(93756.980538626914),MAAC_FLT_C(93780.312602042337),
    MAAC_FLT_C(93803.646116772637),MAAC_FLT_C(93826.981082637285),MAAC_FLT_C(93850.317499455836),MAAC_FLT_C(93873.655367047861),
    MAAC_FLT_C(93896.994685233032),MAAC_FLT_C(93920.335453831038),MAAC_FLT_C(93943.677672661666),MAAC_FLT_C(93967.021341544707),
    MAAC_FLT_C(93990.366460300051),MAAC_FLT_C(94013.713028747632),MAAC_FLT_C(94037.061046707429),MAAC_FLT_C(94060.410513999494),
    MAAC_FLT_C(94083.761430443919),MAAC_FLT_C(94107.113795860845),MAAC_FLT_C(94130.467610070511),MAAC_FLT_C(94153.822872893157),
    MAAC_FLT_C(94177.179584149111),MAAC_FLT_C(94200.537743658759),MAAC_FLT_C(94223.897351242529),MAAC_FLT_C(94247.258406720910),
    MAAC_FLT_C(94270.620909914433),MAAC_FLT_C(94293.984860643730),MAAC_FLT_C(94317.350258729421),MAAC_FLT_C(94340.717103992240),
    MAAC_FLT_C(94364.085396252936),MAAC_FLT_C(94387.455135332348),MAAC_FLT_C(94410.826321051340),MAAC_FLT_C(94434.198953230851),
    MAAC_FLT_C(94457.573031691878),MAAC_FLT_C(94480.948556255447),MAAC_FLT_C(94504.325526742658),MAAC_FLT_C(94527.703942974680),
    MAAC_FLT_C(94551.083804772716),MAAC_FLT_C(94574.465111958023),MAAC_FLT_C(94597.847864351934),MAAC_FLT_C(94621.232061775823),
    MAAC_FLT_C(94644.617704051096),MAAC_FLT_C(94668.004790999272),MAAC_FLT_C(94691.393322441872),MAAC_FLT_C(94714.783298200506),
    MAAC_FLT_C(94738.174718096794),MAAC_FLT_C(94761.567581952477),MAAC_FLT_C(94784.961889589307),MAAC_FLT_C(94808.357640829097),
    MAAC_FLT_C(94831.754835493703),MAAC_FLT_C(94855.153473405066),MAAC_FLT_C(94878.553554385173),MAAC_FLT_C(94901.955078256055),
    MAAC_FLT_C(94925.358044839784),MAAC_FLT_C(94948.762453958523),MAAC_FLT_C(94972.168305434476),MAAC_FLT_C(94995.575599089891),
    MAAC_FLT_C(95018.984334747074),MAAC_FLT_C(95042.394512228391),MAAC_FLT_C(95065.806131356265),MAAC_FLT_C(95089.219191953176),
    MAAC_FLT_C(95112.633693841635),MAAC_FLT_C(95136.049636844240),MAAC_FLT_C(95159.467020783617),MAAC_FLT_C(95182.885845482466),
    MAAC_FLT_C(95206.306110763529),MAAC_FLT_C(95229.727816449609),MAAC_FLT_C(95253.150962363565),MAAC_FLT_C(95276.575548328314),
    MAAC_FLT_C(95300.001574166803),MAAC_FLT_C(95323.429039702052),MAAC_FLT_C(95346.857944757154),MAAC_FLT_C(95370.288289155214),
    MAAC_FLT_C(95393.720072719429),MAAC_FLT_C(95417.153295273019),MAAC_FLT_C(95440.587956639298),MAAC_FLT_C(95464.024056641589),
    MAAC_FLT_C(95487.461595103305),MAAC_FLT_C(95510.900571847902),MAAC_FLT_C(95534.340986698866),MAAC_FLT_C(95557.782839479783),
    MAAC_FLT_C(95581.226130014256),MAAC_FLT_C(95604.670858125959),MAAC_FLT_C(95628.117023638595),MAAC_FLT_C(95651.564626375985),
    MAAC_FLT_C(95675.013666161918),MAAC_FLT_C(95698.464142820303),MAAC_FLT_C(95721.916056175076),MAAC_FLT_C(95745.369406050231),
    MAAC_FLT_C(95768.824192269807),MAAC_FLT_C(95792.280414657915),MAAC_FLT_C(95815.738073038709),MAAC_FLT_C(95839.197167236387),
    MAAC_FLT_C(95862.657697075221),MAAC_FLT_C(95886.119662379540),MAAC_FLT_C(95909.583062973688),MAAC_FLT_C(95933.047898682111),
    MAAC_FLT_C(95956.514169329268),MAAC_FLT_C(95979.981874739708),MAAC_FLT_C(96003.451014738006),MAAC_FLT_C(96026.921589148798),
    MAAC_FLT_C(96050.393597796792),MAAC_FLT_C(96073.867040506724),MAAC_FLT_C(96097.341917103375),MAAC_FLT_C(96120.818227411626),
    MAAC_FLT_C(96144.295971256375),MAAC_FLT_C(96167.775148462577),MAAC_FLT_C(96191.255758855244),MAAC_FLT_C(96214.737802259449),
    MAAC_FLT_C(96238.221278500307),MAAC_FLT_C(96261.706187402990),MAAC_FLT_C(96285.192528792715),MAAC_FLT_C(96308.680302494788),
    MAAC_FLT_C(96332.169508334526),MAAC_FLT_C(96355.660146137321),MAAC_FLT_C(96379.152215728609),MAAC_FLT_C(96402.645716933868),
    MAAC_FLT_C(96426.140649578665),MAAC_FLT_C(96449.637013488609),MAAC_FLT_C(96473.134808489311),MAAC_FLT_C(96496.634034406510),
    MAAC_FLT_C(96520.134691065963),MAAC_FLT_C(96543.636778293469),MAAC_FLT_C(96567.140295914884),MAAC_FLT_C(96590.645243756153),
    MAAC_FLT_C(96614.151621643221),MAAC_FLT_C(96637.659429402134),MAAC_FLT_C(96661.168666858954),MAAC_FLT_C(96684.679333839798),
    MAAC_FLT_C(96708.191430170875),MAAC_FLT_C(96731.704955678390),MAAC_FLT_C(96755.219910188665),MAAC_FLT_C(96778.736293528011),
    MAAC_FLT_C(96802.254105522836),MAAC_FLT_C(96825.773345999580),MAAC_FLT_C(96849.294014784740),MAAC_FLT_C(96872.816111704873),
    MAAC_FLT_C(96896.339636586577),MAAC_FLT_C(96919.864589256511),MAAC_FLT_C(96943.390969541389),MAAC_FLT_C(96966.918777267958),
    MAAC_FLT_C(96990.448012263048),MAAC_FLT_C(97013.978674353522),MAAC_FLT_C(97037.510763366285),MAAC_FLT_C(97061.044279128328),
    MAAC_FLT_C(97084.579221466673),MAAC_FLT_C(97108.115590208385),MAAC_FLT_C(97131.653385180587),MAAC_FLT_C(97155.192606210490),
    MAAC_FLT_C(97178.733253125291),MAAC_FLT_C(97202.275325752300),MAAC_FLT_C(97225.818823918860),MAAC_FLT_C(97249.363747452342),
    MAAC_FLT_C(97272.910096180189),MAAC_FLT_C(97296.457869929916),MAAC_FLT_C(97320.007068529041),MAAC_FLT_C(97343.557691805196),
    MAAC_FLT_C(97367.109739586012),MAAC_FLT_C(97390.663211699197),MAAC_FLT_C(97414.218107972498),MAAC_FLT_C(97437.774428233737),
    MAAC_FLT_C(97461.332172310766),MAAC_FLT_C(97484.891340031507),MAAC_FLT_C(97508.451931223899),MAAC_FLT_C(97532.013945715982),
    MAAC_FLT_C(97555.577383335811),MAAC_FLT_C(97579.142243911512),MAAC_FLT_C(97602.708527271257),MAAC_FLT_C(97626.276233243261),
    MAAC_FLT_C(97649.845361655811),MAAC_FLT_C(97673.415912337223),MAAC_FLT_C(97696.987885115886),MAAC_FLT_C(97720.561279820206),
    MAAC_FLT_C(97744.136096278700),MAAC_FLT_C(97767.712334319876),MAAC_FLT_C(97791.289993772341),MAAC_FLT_C(97814.869074464703),
    MAAC_FLT_C(97838.449576225685),MAAC_FLT_C(97862.031498883996),MAAC_FLT_C(97885.614842268449),MAAC_FLT_C(97909.199606207883),
    MAAC_FLT_C(97932.785790531183),MAAC_FLT_C(97956.373395067320),MAAC_FLT_C(97979.962419645264),MAAC_FLT_C(98003.552864094076),
    MAAC_FLT_C(98027.144728242856),MAAC_FLT_C(98050.738011920766),MAAC_FLT_C(98074.332714956996),MAAC_FLT_C(98097.928837180807),
    MAAC_FLT_C(98121.526378421506),MAAC_FLT_C(98145.125338508456),MAAC_FLT_C(98168.725717271067),MAAC_FLT_C(98192.327514538774),
    MAAC_FLT_C(98215.930730141132),MAAC_FLT_C(98239.535363907664),MAAC_FLT_C(98263.141415668011),MAAC_FLT_C(98286.748885251814),
    MAAC_FLT_C(98310.357772488816),MAAC_FLT_C(98333.968077208759),MAAC_FLT_C(98357.579799241488),MAAC_FLT_C(98381.192938416847),
    MAAC_FLT_C(98404.807494564782),MAAC_FLT_C(98428.423467515240),MAAC_FLT_C(98452.040857098269),MAAC_FLT_C(98475.659663143917),
    MAAC_FLT_C(98499.279885482320),MAAC_FLT_C(98522.901523943656),MAAC_FLT_C(98546.524578358163),MAAC_FLT_C(98570.149048556093),
    MAAC_FLT_C(98593.774934367786),MAAC_FLT_C(98617.402235623624),MAAC_FLT_C(98641.030952154048),MAAC_FLT_C(98664.661083789513),
    MAAC_FLT_C(98688.292630360564),MAAC_FLT_C(98711.925591697771),MAAC_FLT_C(98735.559967631794),MAAC_FLT_C(98759.195757993293),
    MAAC_FLT_C(98782.832962613014),MAAC_FLT_C(98806.471581321734),MAAC_FLT_C(98830.111613950285),MAAC_FLT_C(98853.753060329575),
    MAAC_FLT_C(98877.395920290510),MAAC_FLT_C(98901.040193664099),MAAC_FLT_C(98924.685880281380),MAAC_FLT_C(98948.332979973420),
    MAAC_FLT_C(98971.981492571387),MAAC_FLT_C(98995.631417906450),MAAC_FLT_C(99019.282755809851),MAAC_FLT_C(99042.935506112874),
    MAAC_FLT_C(99066.589668646877),MAAC_FLT_C(99090.245243243233),MAAC_FLT_C(99113.902229733401),MAAC_FLT_C(99137.560627948857),
    MAAC_FLT_C(99161.220437721146),MAAC_FLT_C(99184.881658881859),MAAC_FLT_C(99208.544291262631),MAAC_FLT_C(99232.208334695169),
    MAAC_FLT_C(99255.873789011210),MAAC_FLT_C(99279.540654042547),MAAC_FLT_C(99303.208929621032),MAAC_FLT_C(99326.878615578520),
    MAAC_FLT_C(99350.549711746993),MAAC_FLT_C(99374.222217958435),MAAC_FLT_C(99397.896134044888),MAAC_FLT_C(99421.571459838422),
    MAAC_FLT_C(99445.248195171211),MAAC_FLT_C(99468.926339875441),MAAC_FLT_C(99492.605893783344),MAAC_FLT_C(99516.286856727209),
    MAAC_FLT_C(99539.969228539398),MAAC_FLT_C(99563.653009052287),MAAC_FLT_C(99587.338198098325),MAAC_FLT_C(99611.024795510006),
    MAAC_FLT_C(99634.712801119866),MAAC_FLT_C(99658.402214760499),MAAC_FLT_C(99682.093036264545),MAAC_FLT_C(99705.785265464699),
    MAAC_FLT_C(99729.478902193689),MAAC_FLT_C(99753.173946284325),MAAC_FLT_C(99776.870397569437),MAAC_FLT_C(99800.568255881910),
    MAAC_FLT_C(99824.267521054688),MAAC_FLT_C(99847.968192920773),MAAC_FLT_C(99871.670271313182),MAAC_FLT_C(99895.373756065004),
    MAAC_FLT_C(99919.078647009388),MAAC_FLT_C(99942.784943979525),MAAC_FLT_C(99966.492646808634),MAAC_FLT_C(99990.201755330010),
    MAAC_FLT_C(100013.91226937699),MAAC_FLT_C(100037.62418878295),MAAC_FLT_C(100061.33751338134),MAAC_FLT_C(100085.05224300563),
    MAAC_FLT_C(100108.76837748935),MAAC_FLT_C(100132.48591666610),MAAC_FLT_C(100156.20486036950),MAAC_FLT_C(100179.92520843323),
    MAAC_FLT_C(100203.64696069101),MAAC_FLT_C(100227.37011697664),MAAC_FLT_C(100251.09467712394),MAAC_FLT_C(100274.82064096678),
    MAAC_FLT_C(100298.54800833909),MAAC_FLT_C(100322.27677907483),MAAC_FLT_C(100346.00695300807),MAAC_FLT_C(100369.73852997283),
    MAAC_FLT_C(100393.47150980328),MAAC_FLT_C(100417.20589233354),MAAC_FLT_C(100440.94167739789),MAAC_FLT_C(100464.67886483055),
    MAAC_FLT_C(100488.41745446586),MAAC_FLT_C(100512.15744613820),MAAC_FLT_C(100535.89883968196),MAAC_FLT_C(100559.64163493161),
    MAAC_FLT_C(100583.38583172169),MAAC_FLT_C(100607.13142988674),MAAC_FLT_C(100630.87842926137),MAAC_FLT_C(100654.62682968024),
    MAAC_FLT_C(100678.37663097809),MAAC_FLT_C(100702.12783298964),MAAC_FLT_C(100725.88043554971),MAAC_FLT_C(100749.63443849316),
    MAAC_FLT_C(100773.38984165491),MAAC_FLT_C(100797.14664486986),MAAC_FLT_C(100820.90484797307),MAAC_FLT_C(100844.66445079957),
    MAAC_FLT_C(100868.42545318443),MAAC_FLT_C(100892.18785496285),MAAC_FLT_C(100915.95165596998),MAAC_FLT_C(100939.71685604109),
    MAAC_FLT_C(100963.48345501146),MAAC_FLT_C(100987.25145271645),MAAC_FLT_C(101011.02084899142),MAAC_FLT_C(101034.79164367182),
    MAAC_FLT_C(101058.56383659317),MAAC_FLT_C(101082.33742759094),MAAC_FLT_C(101106.11241650078),MAAC_FLT_C(101129.88880315828),
    MAAC_FLT_C(101153.66658739912),MAAC_FLT_C(101177.44576905905),MAAC_FLT_C(101201.22634797383),MAAC_FLT_C(101225.00832397929),
    MAAC_FLT_C(101248.79169691130),MAAC_FLT_C(101272.57646660579),MAAC_FLT_C(101296.36263289873),MAAC_FLT_C(101320.15019562612),
    MAAC_FLT_C(101343.93915462404),MAAC_FLT_C(101367.72950972860),MAAC_FLT_C(101391.52126077596),MAAC_FLT_C(101415.31440760233),
    MAAC_FLT_C(101439.10895004397),MAAC_FLT_C(101462.90488793720),MAAC_FLT_C(101486.70222111834),MAAC_FLT_C(101510.50094942382),
    MAAC_FLT_C(101534.30107269008),MAAC_FLT_C(101558.10259075362),MAAC_FLT_C(101581.90550345098),MAAC_FLT_C(101605.70981061876),
    MAAC_FLT_C(101629.51551209360),MAAC_FLT_C(101653.32260771218),MAAC_FLT_C(101677.13109731126),MAAC_FLT_C(101700.94098072760),
    MAAC_FLT_C(101724.75225779804),MAAC_FLT_C(101748.56492835947),MAAC_FLT_C(101772.37899224881),MAAC_FLT_C(101796.19444930303),
    MAAC_FLT_C(101820.01129935916),MAAC_FLT_C(101843.82954225427),MAAC_FLT_C(101867.64917782549),MAAC_FLT_C(101891.47020590997),
    MAAC_FLT_C(101915.29262634492),MAAC_FLT_C(101939.11643896763),MAAC_FLT_C(101962.94164361537),MAAC_FLT_C(101986.76824012553),
    MAAC_FLT_C(102010.59622833549),MAAC_FLT_C(102034.42560808272),MAAC_FLT_C(102058.25637920471),MAAC_FLT_C(102082.08854153901),
    MAAC_FLT_C(102105.92209492320),MAAC_FLT_C(102129.75703919494),MAAC_FLT_C(102153.59337419191),MAAC_FLT_C(102177.43109975185),
    MAAC_FLT_C(102201.27021571253),MAAC_FLT_C(102225.11072191180),MAAC_FLT_C(102248.95261818753),MAAC_FLT_C(102272.79590437764),
    MAAC_FLT_C(102296.64058032009),MAAC_FLT_C(102320.48664585293),MAAC_FLT_C(102344.33410081422),MAAC_FLT_C(102368.18294504205),
    MAAC_FLT_C(102392.03317837461),MAAC_FLT_C(102415.88480065008),MAAC_FLT_C(102439.73781170673),MAAC_FLT_C(102463.59221138287),
    MAAC_FLT_C(102487.44799951684),MAAC_FLT_C(102511.30517594704),MAAC_FLT_C(102535.16374051190),MAAC_FLT_C(102559.02369304992),
    MAAC_FLT_C(102582.88503339965),MAAC_FLT_C(102606.74776139967),MAAC_FLT_C(102630.61187688859),MAAC_FLT_C(102654.47737970512),
    MAAC_FLT_C(102678.34426968795),MAAC_FLT_C(102702.21254667587),MAAC_FLT_C(102726.08221050771),MAAC_FLT_C(102749.95326102231),
    MAAC_FLT_C(102773.82569805860),MAAC_FLT_C(102797.69952145554),MAAC_FLT_C(102821.57473105213),MAAC_FLT_C(102845.45132668743),
    MAAC_FLT_C(102869.32930820051),MAAC_FLT_C(102893.20867543056),MAAC_FLT_C(102917.08942821674),MAAC_FLT_C(102940.97156639832),
    MAAC_FLT_C(102964.85508981455),MAAC_FLT_C(102988.73999830478),MAAC_FLT_C(103012.62629170840),MAAC_FLT_C(103036.51396986481),
    MAAC_FLT_C(103060.40303261351),MAAC_FLT_C(103084.29347979400),MAAC_FLT_C(103108.18531124585),MAAC_FLT_C(103132.07852680866),
    MAAC_FLT_C(103155.97312632212),MAAC_FLT_C(103179.86910962590),MAAC_FLT_C(103203.76647655977),MAAC_FLT_C(103227.66522696352),
    MAAC_FLT_C(103251.56536067701),MAAC_FLT_C(103275.46687754011),MAAC_FLT_C(103299.36977739276),MAAC_FLT_C(103323.27406007495),
    MAAC_FLT_C(103347.17972542670),MAAC_FLT_C(103371.08677328810),MAAC_FLT_C(103394.99520349925),MAAC_FLT_C(103418.90501590034),
    MAAC_FLT_C(103442.81621033157),MAAC_FLT_C(103466.72878663319),MAAC_FLT_C(103490.64274464553),MAAC_FLT_C(103514.55808420894),
    MAAC_FLT_C(103538.47480516380),MAAC_FLT_C(103562.39290735057),MAAC_FLT_C(103586.31239060973),MAAC_FLT_C(103610.23325478184),
    MAAC_FLT_C(103634.15549970744),MAAC_FLT_C(103658.07912522719),MAAC_FLT_C(103682.00413118176),MAAC_FLT_C(103705.93051741188),
    MAAC_FLT_C(103729.85828375829),MAAC_FLT_C(103753.78743006183),MAAC_FLT_C(103777.71795616334),MAAC_FLT_C(103801.64986190372),
    MAAC_FLT_C(103825.58314712394),MAAC_FLT_C(103849.51781166498),MAAC_FLT_C(103873.45385536790),MAAC_FLT_C(103897.39127807376),
    MAAC_FLT_C(103921.33007962372),MAAC_FLT_C(103945.27025985894),MAAC_FLT_C(103969.21181862066),MAAC_FLT_C(103993.15475575013),
    MAAC_FLT_C(104017.09907108870),MAAC_FLT_C(104041.04476447770),MAAC_FLT_C(104064.99183575854),MAAC_FLT_C(104088.94028477269),
    MAAC_FLT_C(104112.89011136163),MAAC_FLT_C(104136.84131536692),MAAC_FLT_C(104160.79389663014),MAAC_FLT_C(104184.74785499295),
    MAAC_FLT_C(104208.70319029699),MAAC_FLT_C(104232.65990238401),MAAC_FLT_C(104256.61799109577),MAAC_FLT_C(104280.57745627411),
    MAAC_FLT_C(104304.53829776087),MAAC_FLT_C(104328.50051539797),MAAC_FLT_C(104352.46410902737),MAAC_FLT_C(104376.42907849104),
    MAAC_FLT_C(104400.39542363105),MAAC_FLT_C(104424.36314428948),MAAC_FLT_C(104448.33224030846),MAAC_FLT_C(104472.30271153020),
    MAAC_FLT_C(104496.27455779689),MAAC_FLT_C(104520.24777895081),MAAC_FLT_C(104544.22237483428),MAAC_FLT_C(104568.19834528965),
    MAAC_FLT_C(104592.17569015936),MAAC_FLT_C(104616.15440928582),MAAC_FLT_C(104640.13450251156),MAAC_FLT_C(104664.11596967910),
    MAAC_FLT_C(104688.09881063103),MAAC_FLT_C(104712.08302520998),MAAC_FLT_C(104736.06861325864),MAAC_FLT_C(104760.05557461972),
    MAAC_FLT_C(104784.04390913600),MAAC_FLT_C(104808.03361665027),MAAC_FLT_C(104832.02469700541),MAAC_FLT_C(104856.01715004431),
    MAAC_FLT_C(104880.01097560991),MAAC_FLT_C(104904.00617354522),MAAC_FLT_C(104928.00274369326),MAAC_FLT_C(104952.00068589713),
    MAAC_FLT_C(104975.99999999993),MAAC_FLT_C(105000.00068584486),MAAC_FLT_C(105024.00274327511),MAAC_FLT_C(105048.00617213396),
    MAAC_FLT_C(105072.01097226470),MAAC_FLT_C(105096.01714351070),MAAC_FLT_C(105120.02468571534),MAAC_FLT_C(105144.03359872208),
    MAAC_FLT_C(105168.04388237436),MAAC_FLT_C(105192.05553651575),MAAC_FLT_C(105216.06856098982),MAAC_FLT_C(105240.08295564017),
    MAAC_FLT_C(105264.09872031047),MAAC_FLT_C(105288.11585484444),MAAC_FLT_C(105312.13435908582),MAAC_FLT_C(105336.15423287840),
    MAAC_FLT_C(105360.17547606604),MAAC_FLT_C(105384.19808849262),MAAC_FLT_C(105408.22207000206),MAAC_FLT_C(105432.24742043833),
    MAAC_FLT_C(105456.27413964547),MAAC_FLT_C(105480.30222746753),MAAC_FLT_C(105504.33168374863),MAAC_FLT_C(105528.36250833291),
    MAAC_FLT_C(105552.39470106458),MAAC_FLT_C(105576.42826178786),MAAC_FLT_C(105600.46319034706),MAAC_FLT_C(105624.49948658649),
    MAAC_FLT_C(105648.53715035053),MAAC_FLT_C(105672.57618148360),MAAC_FLT_C(105696.61657983017),MAAC_FLT_C(105720.65834523473),
    MAAC_FLT_C(105744.70147754184),MAAC_FLT_C(105768.74597659610),MAAC_FLT_C(105792.79184224214),MAAC_FLT_C(105816.83907432464),
    MAAC_FLT_C(105840.88767268835),MAAC_FLT_C(105864.93763717801),MAAC_FLT_C(105888.98896763846),MAAC_FLT_C(105913.04166391456),
    MAAC_FLT_C(105937.09572585119),MAAC_FLT_C(105961.15115329332),MAAC_FLT_C(105985.20794608595),MAAC_FLT_C(106009.26610407409),
    MAAC_FLT_C(106033.32562710284),MAAC_FLT_C(106057.38651501729),MAAC_FLT_C(106081.44876766266),MAAC_FLT_C(106105.51238488412),
    MAAC_FLT_C(106129.57736652695),MAAC_FLT_C(106153.64371243643),MAAC_FLT_C(106177.71142245791),MAAC_FLT_C(106201.78049643680),
    MAAC_FLT_C(106225.85093421848),MAAC_FLT_C(106249.92273564848),MAAC_FLT_C(106273.99590057228),MAAC_FLT_C(106298.07042883546),
    MAAC_FLT_C(106322.14632028362),MAAC_FLT_C(106346.22357476241),MAAC_FLT_C(106370.30219211751),MAAC_FLT_C(106394.38217219469),
    MAAC_FLT_C(106418.46351483969),MAAC_FLT_C(106442.54621989837),MAAC_FLT_C(106466.63028721658),MAAC_FLT_C(106490.71571664025),
    MAAC_FLT_C(106514.80250801529),MAAC_FLT_C(106538.89066118775),MAAC_FLT_C(106562.98017600364),MAAC_FLT_C(106587.07105230905),
    MAAC_FLT_C(106611.16328995011),MAAC_FLT_C(106635.25688877302),MAAC_FLT_C(106659.35184862395),MAAC_FLT_C(106683.44816934918),
    MAAC_FLT_C(106707.54585079502),MAAC_FLT_C(106731.64489280782),MAAC_FLT_C(106755.74529523395),MAAC_FLT_C(106779.84705791986),
    MAAC_FLT_C(106803.95018071201),MAAC_FLT_C(106828.05466345693),MAAC_FLT_C(106852.16050600118),MAAC_FLT_C(106876.26770819137),
    MAAC_FLT_C(106900.37626987413),MAAC_FLT_C(106924.48619089619),MAAC_FLT_C(106948.59747110425),MAAC_FLT_C(106972.71011034511),
    MAAC_FLT_C(106996.82410846559),MAAC_FLT_C(107020.93946531253),MAAC_FLT_C(107045.05618073288),MAAC_FLT_C(107069.17425457356),
    MAAC_FLT_C(107093.29368668159),MAAC_FLT_C(107117.41447690397),MAAC_FLT_C(107141.53662508781),MAAC_FLT_C(107165.66013108024),
    MAAC_FLT_C(107189.78499472840),MAAC_FLT_C(107213.91121587952),MAAC_FLT_C(107238.03879438085),MAAC_FLT_C(107262.16773007967),
    MAAC_FLT_C(107286.29802282334),MAAC_FLT_C(107310.42967245923),MAAC_FLT_C(107334.56267883476),MAAC_FLT_C(107358.69704179741),
    MAAC_FLT_C(107382.83276119467),MAAC_FLT_C(107406.96983687412),MAAC_FLT_C(107431.10826868335),MAAC_FLT_C(107455.24805646999),
    MAAC_FLT_C(107479.38920008171),MAAC_FLT_C(107503.53169936627),MAAC_FLT_C(107527.67555417139),MAAC_FLT_C(107551.82076434493),
    MAAC_FLT_C(107575.96732973469),MAAC_FLT_C(107600.11525018861),MAAC_FLT_C(107624.26452555461),MAAC_FLT_C(107648.41515568066),
    MAAC_FLT_C(107672.56714041480),MAAC_FLT_C(107696.72047960508),MAAC_FLT_C(107720.87517309963),MAAC_FLT_C(107745.03122074658),
    MAAC_FLT_C(107769.18862239414),MAAC_FLT_C(107793.34737789053),MAAC_FLT_C(107817.50748708403),MAAC_FLT_C(107841.66894982298),
    MAAC_FLT_C(107865.83176595572),MAAC_FLT_C(107889.99593533068),MAAC_FLT_C(107914.16145779629),MAAC_FLT_C(107938.32833320105),
    MAAC_FLT_C(107962.49656139348),MAAC_FLT_C(107986.66614222217),MAAC_FLT_C(108010.83707553572),MAAC_FLT_C(108035.00936118282),
    MAAC_FLT_C(108059.18299901215),MAAC_FLT_C(108083.35798887245),MAAC_FLT_C(108107.53433061253),MAAC_FLT_C(108131.71202408121),
    MAAC_FLT_C(108155.89106912735),MAAC_FLT_C(108180.07146559987),MAAC_FLT_C(108204.25321334775),MAAC_FLT_C(108228.43631221996),
    MAAC_FLT_C(108252.62076206553),MAAC_FLT_C(108276.80656273357),MAAC_FLT_C(108300.99371407321),MAAC_FLT_C(108325.18221593359),
    MAAC_FLT_C(108349.37206816394),MAAC_FLT_C(108373.56327061349),MAAC_FLT_C(108397.75582313156),MAAC_FLT_C(108421.94972556747),
    MAAC_FLT_C(108446.14497777060),MAAC_FLT_C(108470.34157959036),MAAC_FLT_C(108494.53953087622),MAAC_FLT_C(108518.73883147769),
    MAAC_FLT_C(108542.93948124432),MAAC_FLT_C(108567.14148002566),MAAC_FLT_C(108591.34482767139),MAAC_FLT_C(108615.54952403114),
    MAAC_FLT_C(108639.75556895464),MAAC_FLT_C(108663.96296229165),MAAC_FLT_C(108688.17170389196),MAAC_FLT_C(108712.38179360541),
    MAAC_FLT_C(108736.59323128188),MAAC_FLT_C(108760.80601677128),MAAC_FLT_C(108785.02014992358),MAAC_FLT_C(108809.23563058881),
    MAAC_FLT_C(108833.45245861699),MAAC_FLT_C(108857.67063385822),MAAC_FLT_C(108881.89015616261),MAAC_FLT_C(108906.11102538036),
    MAAC_FLT_C(108930.33324136169),MAAC_FLT_C(108954.55680395682),MAAC_FLT_C(108978.78171301607),MAAC_FLT_C(109003.00796838976),
    MAAC_FLT_C(109027.23556992831),MAAC_FLT_C(109051.46451748211),MAAC_FLT_C(109075.69481090162),MAAC_FLT_C(109099.92645003737),
    MAAC_FLT_C(109124.15943473989),MAAC_FLT_C(109148.39376485976),MAAC_FLT_C(109172.62944024763),MAAC_FLT_C(109196.86646075416),
    MAAC_FLT_C(109221.10482623006),MAAC_FLT_C(109245.34453652610),MAAC_FLT_C(109269.58559149304),MAAC_FLT_C(109293.82799098175),
    MAAC_FLT_C(109318.07173484311),MAAC_FLT_C(109342.31682292801),MAAC_FLT_C(109366.56325508743),MAAC_FLT_C(109390.81103117237),
    MAAC_FLT_C(109415.06015103387),MAAC_FLT_C(109439.31061452301),MAAC_FLT_C(109463.56242149093),MAAC_FLT_C(109487.81557178880),
    MAAC_FLT_C(109512.07006526781),MAAC_FLT_C(109536.32590177920),MAAC_FLT_C(109560.58308117429),MAAC_FLT_C(109584.84160330440),
    MAAC_FLT_C(109609.10146802090),MAAC_FLT_C(109633.36267517522),MAAC_FLT_C(109657.62522461878),MAAC_FLT_C(109681.88911620309),
    MAAC_FLT_C(109706.15434977971),MAAC_FLT_C(109730.42092520020),MAAC_FLT_C(109754.68884231619),MAAC_FLT_C(109778.95810097932),
    MAAC_FLT_C(109803.22870104130),MAAC_FLT_C(109827.50064235389),MAAC_FLT_C(109851.77392476884),MAAC_FLT_C(109876.04854813800),
    MAAC_FLT_C(109900.32451231324),MAAC_FLT_C(109924.60181714644),MAAC_FLT_C(109948.88046248957),MAAC_FLT_C(109973.16044819460),
    MAAC_FLT_C(109997.44177411357),MAAC_FLT_C(110021.72444009855),MAAC_FLT_C(110046.00844600164),MAAC_FLT_C(110070.29379167501),
    MAAC_FLT_C(110094.58047697082),MAAC_FLT_C(110118.86850174134),MAAC_FLT_C(110143.15786583882),MAAC_FLT_C(110167.44856911557),
    MAAC_FLT_C(110191.74061142397),MAAC_FLT_C(110216.03399261639),MAAC_FLT_C(110240.32871254528),MAAC_FLT_C(110264.62477106311),
    MAAC_FLT_C(110288.92216802240),MAAC_FLT_C(110313.22090327571),MAAC_FLT_C(110337.52097667565),MAAC_FLT_C(110361.82238807483),
    MAAC_FLT_C(110386.12513732594),MAAC_FLT_C(110410.42922428172),MAAC_FLT_C(110434.73464879491),MAAC_FLT_C(110459.04141071832),
    MAAC_FLT_C(110483.34950990479),MAAC_FLT_C(110507.65894620719),MAAC_FLT_C(110531.96971947847),MAAC_FLT_C(110556.28182957157),
    MAAC_FLT_C(110580.59527633950),MAAC_FLT_C(110604.91005963532),MAAC_FLT_C(110629.22617931209),MAAC_FLT_C(110653.54363522294),
    MAAC_FLT_C(110677.86242722106),MAAC_FLT_C(110702.18255515961),MAAC_FLT_C(110726.50401889188),MAAC_FLT_C(110750.82681827113),
    MAAC_FLT_C(110775.15095315070),MAAC_FLT_C(110799.47642338395),MAAC_FLT_C(110823.80322882428),MAAC_FLT_C(110848.13136932514),
    MAAC_FLT_C(110872.46084474004),MAAC_FLT_C(110896.79165492248),MAAC_FLT_C(110921.12379972603),MAAC_FLT_C(110945.45727900430),
    MAAC_FLT_C(110969.79209261097),MAAC_FLT_C(110994.12824039967),MAAC_FLT_C(111018.46572222417),MAAC_FLT_C(111042.80453793822),
    MAAC_FLT_C(111067.14468739566),MAAC_FLT_C(111091.48617045028),MAAC_FLT_C(111115.82898695602),MAAC_FLT_C(111140.17313676680),
    MAAC_FLT_C(111164.51861973657),MAAC_FLT_C(111188.86543571933),MAAC_FLT_C(111213.21358456917),MAAC_FLT_C(111237.56306614014),
    MAAC_FLT_C(111261.91388028639),MAAC_FLT_C(111286.26602686207),MAAC_FLT_C(111310.61950572141),MAAC_FLT_C(111334.97431671864),
    MAAC_FLT_C(111359.33045970804),MAAC_FLT_C(111383.68793454397),MAAC_FLT_C(111408.04674108078),MAAC_FLT_C(111432.40687917286),
    MAAC_FLT_C(111456.76834867468),MAAC_FLT_C(111481.13114944073),MAAC_FLT_C(111505.49528132551),MAAC_FLT_C(111529.86074418361),
    MAAC_FLT_C(111554.22753786964),MAAC_FLT_C(111578.59566223821),MAAC_FLT_C(111602.96511714405),MAAC_FLT_C(111627.33590244185),
    MAAC_FLT_C(111651.70801798640),MAAC_FLT_C(111676.08146363248),MAAC_FLT_C(111700.45623923496),MAAC_FLT_C(111724.83234464870),
    MAAC_FLT_C(111749.20977972864),MAAC_FLT_C(111773.58854432974),MAAC_FLT_C(111797.96863830699),MAAC_FLT_C(111822.35006151545),
    MAAC_FLT_C(111846.73281381019),MAAC_FLT_C(111871.11689504633),MAAC_FLT_C(111895.50230507903),MAAC_FLT_C(111919.88904376350),
    MAAC_FLT_C(111944.27711095495),MAAC_FLT_C(111968.66650650870),MAAC_FLT_C(111993.05723028004),MAAC_FLT_C(112017.44928212435),
    MAAC_FLT_C(112041.84266189700),MAAC_FLT_C(112066.23736945343),MAAC_FLT_C(112090.63340464912),MAAC_FLT_C(112115.03076733962),
    MAAC_FLT_C(112139.42945738042),MAAC_FLT_C(112163.82947462716),MAAC_FLT_C(112188.23081893545),MAAC_FLT_C(112212.63349016097),
    MAAC_FLT_C(112237.03748815943),MAAC_FLT_C(112261.44281278658),MAAC_FLT_C(112285.84946389822),MAAC_FLT_C(112310.25744135017),
    MAAC_FLT_C(112334.66674499829),MAAC_FLT_C(112359.07737469849),MAAC_FLT_C(112383.48933030672),MAAC_FLT_C(112407.90261167898),
    MAAC_FLT_C(112432.31721867126),MAAC_FLT_C(112456.73315113965),MAAC_FLT_C(112481.15040894024),MAAC_FLT_C(112505.56899192919),
    MAAC_FLT_C(112529.98889996266),MAAC_FLT_C(112554.41013289688),MAAC_FLT_C(112578.83269058811),MAAC_FLT_C(112603.25657289263),
    MAAC_FLT_C(112627.68177966679),MAAC_FLT_C(112652.10831076698),MAAC_FLT_C(112676.53616604958),MAAC_FLT_C(112700.96534537108),
    MAAC_FLT_C(112725.39584858794),MAAC_FLT_C(112749.82767555672),MAAC_FLT_C(112774.26082613398),MAAC_FLT_C(112798.69530017630),
    MAAC_FLT_C(112823.13109754038),MAAC_FLT_C(112847.56821808286),MAAC_FLT_C(112872.00666166049),MAAC_FLT_C(112896.44642813003),
    MAAC_FLT_C(112920.88751734827),MAAC_FLT_C(112945.32992917208),MAAC_FLT_C(112969.77366345831),MAAC_FLT_C(112994.21872006389),
    MAAC_FLT_C(113018.66509884578),MAAC_FLT_C(113043.11279966099),MAAC_FLT_C(113067.56182236652),MAAC_FLT_C(113092.01216681948),
    MAAC_FLT_C(113116.46383287695),MAAC_FLT_C(113140.91682039610),MAAC_FLT_C(113165.37112923413),MAAC_FLT_C(113189.82675924824),
    MAAC_FLT_C(113214.28371029573),MAAC_FLT_C(113238.74198223387),MAAC_FLT_C(113263.20157492002),MAAC_FLT_C(113287.66248821157),
    MAAC_FLT_C(113312.12472196593),MAAC_FLT_C(113336.58827604055),MAAC_FLT_C(113361.05315029295),MAAC_FLT_C(113385.51934458067),
    MAAC_FLT_C(113409.98685876124),MAAC_FLT_C(113434.45569269233),MAAC_FLT_C(113458.92584623155),MAAC_FLT_C(113483.39731923661),
    MAAC_FLT_C(113507.87011156522),MAAC_FLT_C(113532.34422307517),MAAC_FLT_C(113556.81965362425),MAAC_FLT_C(113581.29640307030),
    MAAC_FLT_C(113605.77447127122),MAAC_FLT_C(113630.25385808491),MAAC_FLT_C(113654.73456336933),MAAC_FLT_C(113679.21658698250),
    MAAC_FLT_C(113703.69992878241),MAAC_FLT_C(113728.18458862718),MAAC_FLT_C(113752.67056637487),MAAC_FLT_C(113777.15786188368),
    MAAC_FLT_C(113801.64647501177),MAAC_FLT_C(113826.13640561736),MAAC_FLT_C(113850.62765355874),MAAC_FLT_C(113875.12021869418),
    MAAC_FLT_C(113899.61410088204),MAAC_FLT_C(113924.10929998070),MAAC_FLT_C(113948.60581584855),MAAC_FLT_C(113973.10364834407),
    MAAC_FLT_C(113997.60279732574),MAAC_FLT_C(114022.10326265210),MAAC_FLT_C(114046.60504418171),MAAC_FLT_C(114071.10814177318),
    MAAC_FLT_C(114095.61255528514),MAAC_FLT_C(114120.11828457628),MAAC_FLT_C(114144.62532950533),MAAC_FLT_C(114169.13368993104),
    MAAC_FLT_C(114193.64336571220),MAAC_FLT_C(114218.15435670764),MAAC_FLT_C(114242.66666277626),MAAC_FLT_C(114267.18028377694),
    MAAC_FLT_C(114291.69521956862),MAAC_FLT_C(114316.21147001031),MAAC_FLT_C(114340.72903496103),MAAC_FLT_C(114365.24791427983),
    MAAC_FLT_C(114389.76810782580),MAAC_FLT_C(114414.28961545810),MAAC_FLT_C(114438.81243703589),MAAC_FLT_C(114463.33657241837),
    MAAC_FLT_C(114487.86202146480),MAAC_FLT_C(114512.38878403448),MAAC_FLT_C(114536.91685998671),MAAC_FLT_C(114561.44624918087),
    MAAC_FLT_C(114585.97695147636),MAAC_FLT_C(114610.50896673260),MAAC_FLT_C(114635.04229480909),MAAC_FLT_C(114659.57693556532),
    MAAC_FLT_C(114684.11288886084),MAAC_FLT_C(114708.65015455526),MAAC_FLT_C(114733.18873250818),MAAC_FLT_C(114757.72862257928),
    MAAC_FLT_C(114782.26982462825),MAAC_FLT_C(114806.81233851484),MAAC_FLT_C(114831.35616409882),MAAC_FLT_C(114855.90130123998),
    MAAC_FLT_C(114880.44774979822),MAAC_FLT_C(114904.99550963337),MAAC_FLT_C(114929.54458060540),MAAC_FLT_C(114954.09496257425),
    MAAC_FLT_C(114978.64665539992),MAAC_FLT_C(115003.19965894247),MAAC_FLT_C(115027.75397306195),MAAC_FLT_C(115052.30959761847),
    MAAC_FLT_C(115076.86653247218),MAAC_FLT_C(115101.42477748329),MAAC_FLT_C(115125.98433251200),MAAC_FLT_C(115150.54519741859),
    MAAC_FLT_C(115175.10737206334),MAAC_FLT_C(115199.67085630659),MAAC_FLT_C(115224.23565000873),MAAC_FLT_C(115248.80175303014),
    MAAC_FLT_C(115273.36916523130),MAAC_FLT_C(115297.93788647266),MAAC_FLT_C(115322.50791661476),MAAC_FLT_C(115347.07925551817),
    MAAC_FLT_C(115371.65190304347),MAAC_FLT_C(115396.22585905129),MAAC_FLT_C(115420.80112340231),MAAC_FLT_C(115445.37769595724),
    MAAC_FLT_C(115469.95557657682),MAAC_FLT_C(115494.53476512182),MAAC_FLT_C(115519.11526145306),MAAC_FLT_C(115543.69706543141),
    MAAC_FLT_C(115568.28017691776),MAAC_FLT_C(115592.86459577303),MAAC_FLT_C(115617.45032185820),MAAC_FLT_C(115642.03735503425),
    MAAC_FLT_C(115666.62569516223),MAAC_FLT_C(115691.21534210323),MAAC_FLT_C(115715.80629571836),MAAC_FLT_C(115740.39855586876),
    MAAC_FLT_C(115764.99212241563),MAAC_FLT_C(115789.58699522018),MAAC_FLT_C(115814.18317414368),MAAC_FLT_C(115838.78065904744),
    MAAC_FLT_C(115863.37944979276),MAAC_FLT_C(115887.97954624105),MAAC_FLT_C(115912.58094825370),MAAC_FLT_C(115937.18365569216),
    MAAC_FLT_C(115961.78766841792),MAAC_FLT_C(115986.39298629249),MAAC_FLT_C(116010.99960917742),MAAC_FLT_C(116035.60753693432),
    MAAC_FLT_C(116060.21676942479),MAAC_FLT_C(116084.82730651053),MAAC_FLT_C(116109.43914805322),MAAC_FLT_C(116134.05229391460),
    MAAC_FLT_C(116158.66674395646),MAAC_FLT_C(116183.28249804060),MAAC_FLT_C(116207.89955602886),MAAC_FLT_C(116232.51791778316),
    MAAC_FLT_C(116257.13758316539),MAAC_FLT_C(116281.75855203751),MAAC_FLT_C(116306.38082426153),MAAC_FLT_C(116331.00439969949),
    MAAC_FLT_C(116355.62927821343),MAAC_FLT_C(116380.25545966547),MAAC_FLT_C(116404.88294391775),MAAC_FLT_C(116429.51173083246),
    MAAC_FLT_C(116454.14182027178),MAAC_FLT_C(116478.77321209799),MAAC_FLT_C(116503.40590617337),MAAC_FLT_C(116528.03990236025),
    MAAC_FLT_C(116552.67520052097),MAAC_FLT_C(116577.31180051794),MAAC_FLT_C(116601.94970221359),MAAC_FLT_C(116626.58890547040),
    MAAC_FLT_C(116651.22941015086),MAAC_FLT_C(116675.87121611751),MAAC_FLT_C(116700.51432323294),MAAC_FLT_C(116725.15873135976),
    MAAC_FLT_C(116749.80444036060),MAAC_FLT_C(116774.45145009817),MAAC_FLT_C(116799.09976043520),MAAC_FLT_C(116823.74937123443),
    MAAC_FLT_C(116848.40028235866),MAAC_FLT_C(116873.05249367072),MAAC_FLT_C(116897.70600503348),MAAC_FLT_C(116922.36081630984),
    MAAC_FLT_C(116947.01692736275),MAAC_FLT_C(116971.67433805518),MAAC_FLT_C(116996.33304825013),MAAC_FLT_C(117020.99305781067),
    MAAC_FLT_C(117045.65436659988),MAAC_FLT_C(117070.31697448085),MAAC_FLT_C(117094.98088131678),MAAC_FLT_C(117119.64608697084),
    MAAC_FLT_C(117144.31259130624),MAAC_FLT_C(117168.98039418629),MAAC_FLT_C(117193.64949547425),MAAC_FLT_C(117218.31989503348),
    MAAC_FLT_C(117242.99159272733),MAAC_FLT_C(117267.66458841923),MAAC_FLT_C(117292.33888197262),MAAC_FLT_C(117317.01447325097),
    MAAC_FLT_C(117341.69136211780),MAAC_FLT_C(117366.36954843666),MAAC_FLT_C(117391.04903207115),MAAC_FLT_C(117415.72981288488),
    MAAC_FLT_C(117440.41189074151),MAAC_FLT_C(117465.09526550474),MAAC_FLT_C(117489.77993703831),MAAC_FLT_C(117514.46590520597),
    MAAC_FLT_C(117539.15316987153),MAAC_FLT_C(117563.84173089883),MAAC_FLT_C(117588.53158815173),MAAC_FLT_C(117613.22274149416),
    MAAC_FLT_C(117637.91519079007),MAAC_FLT_C(117662.60893590341),MAAC_FLT_C(117687.30397669821),MAAC_FLT_C(117712.00031303853),
    MAAC_FLT_C(117736.69794478847),MAAC_FLT_C(117761.39687181212),MAAC_FLT_C(117786.09709397367),MAAC_FLT_C(117810.79861113730),
    MAAC_FLT_C(117835.50142316725),MAAC_FLT_C(117860.20552992777),MAAC_FLT_C(117884.91093128319),MAAC_FLT_C(117909.61762709780),
    MAAC_FLT_C(117934.32561723603),MAAC_FLT_C(117959.03490156225),MAAC_FLT_C(117983.74547994092),MAAC_FLT_C(118008.45735223651),
    MAAC_FLT_C(118033.17051831353),MAAC_FLT_C(118057.88497803656),MAAC_FLT_C(118082.60073127014),MAAC_FLT_C(118107.31777787892),
    MAAC_FLT_C(118132.03611772758),MAAC_FLT_C(118156.75575068076),MAAC_FLT_C(118181.47667660321),MAAC_FLT_C(118206.19889535972),
    MAAC_FLT_C(118230.92240681504),MAAC_FLT_C(118255.64721083404),MAAC_FLT_C(118280.37330728157),MAAC_FLT_C(118305.10069602253),
    MAAC_FLT_C(118329.82937692187),MAAC_FLT_C(118354.55934984458),MAAC_FLT_C(118379.29061465565),MAAC_FLT_C(118404.02317122012),
    MAAC_FLT_C(118428.75701940308),MAAC_FLT_C(118453.49215906965),MAAC_FLT_C(118478.22859008498),MAAC_FLT_C(118502.96631231424),
    MAAC_FLT_C(118527.70532562268),MAAC_FLT_C(118552.44562987552),MAAC_FLT_C(118577.18722493808),MAAC_FLT_C(118601.93011067568),
    MAAC_FLT_C(118626.67428695368),MAAC_FLT_C(118651.41975363747),MAAC_FLT_C(118676.16651059251),MAAC_FLT_C(118700.91455768421),
    MAAC_FLT_C(118725.66389477813),MAAC_FLT_C(118750.41452173979),MAAC_FLT_C(118775.16643843475),MAAC_FLT_C(118799.91964472862),
    MAAC_FLT_C(118824.67414048706),MAAC_FLT_C(118849.42992557574),MAAC_FLT_C(118874.18699986035),MAAC_FLT_C(118898.94536320666),
    MAAC_FLT_C(118923.70501548043),MAAC_FLT_C(118948.46595654752),MAAC_FLT_C(118973.22818627374),MAAC_FLT_C(118997.99170452499),
    MAAC_FLT_C(119022.75651116720),MAAC_FLT_C(119047.52260606633),MAAC_FLT_C(119072.28998908834),MAAC_FLT_C(119097.05866009930),
    MAAC_FLT_C(119121.82861896523),MAAC_FLT_C(119146.59986555226),MAAC_FLT_C(119171.37239972650),MAAC_FLT_C(119196.14622135412),
    MAAC_FLT_C(119220.92133030134),MAAC_FLT_C(119245.69772643436),MAAC_FLT_C(119270.47540961947),MAAC_FLT_C(119295.25437972297),
    MAAC_FLT_C(119320.03463661121),MAAC_FLT_C(119344.81618015055),MAAC_FLT_C(119369.59901020740),MAAC_FLT_C(119394.38312664822),
    MAAC_FLT_C(119419.16852933947),MAAC_FLT_C(119443.95521814766),MAAC_FLT_C(119468.74319293935),MAAC_FLT_C(119493.53245358112),
    MAAC_FLT_C(119518.32299993958),MAAC_FLT_C(119543.11483188139),MAAC_FLT_C(119567.90794927324),MAAC_FLT_C(119592.70235198183),
    MAAC_FLT_C(119617.49803987393),MAAC_FLT_C(119642.29501281632),MAAC_FLT_C(119667.09327067583),MAAC_FLT_C(119691.89281331931),
    MAAC_FLT_C(119716.69364061367),MAAC_FLT_C(119741.49575242584),MAAC_FLT_C(119766.29914862274),MAAC_FLT_C(119791.10382907141),
    MAAC_FLT_C(119815.90979363887),MAAC_FLT_C(119840.71704219218),MAAC_FLT_C(119865.52557459843),MAAC_FLT_C(119890.33539072477),
    MAAC_FLT_C(119915.14649043836),MAAC_FLT_C(119939.95887360642),MAAC_FLT_C(119964.77254009615),MAAC_FLT_C(119989.58748977486),
    MAAC_FLT_C(120014.40372250983),MAAC_FLT_C(120039.22123816841),MAAC_FLT_C(120064.04003661797),MAAC_FLT_C(120088.86011772591),
    MAAC_FLT_C(120113.68148135970),MAAC_FLT_C(120138.50412738678),MAAC_FLT_C(120163.32805567470),MAAC_FLT_C(120188.15326609099),
    MAAC_FLT_C(120212.97975850321),MAAC_FLT_C(120237.80753277900),MAAC_FLT_C(120262.63658878600),MAAC_FLT_C(120287.46692639188),
    MAAC_FLT_C(120312.29854546436),MAAC_FLT_C(120337.13144587121),MAAC_FLT_C(120361.96562748020),MAAC_FLT_C(120386.80109015913),
    MAAC_FLT_C(120411.63783377589),MAAC_FLT_C(120436.47585819835),MAAC_FLT_C(120461.31516329442),MAAC_FLT_C(120486.15574893207),
    MAAC_FLT_C(120510.99761497928),MAAC_FLT_C(120535.84076130406),MAAC_FLT_C(120560.68518777451),MAAC_FLT_C(120585.53089425867),
    MAAC_FLT_C(120610.37788062470),MAAC_FLT_C(120635.22614674074),MAAC_FLT_C(120660.07569247499),MAAC_FLT_C(120684.92651769568),
    MAAC_FLT_C(120709.77862227106),MAAC_FLT_C(120734.63200606944),MAAC_FLT_C(120759.48666895913),MAAC_FLT_C(120784.34261080850),
    MAAC_FLT_C(120809.19983148595),MAAC_FLT_C(120834.05833085992),MAAC_FLT_C(120858.91810879884),MAAC_FLT_C(120883.77916517125),
    MAAC_FLT_C(120908.64149984565),MAAC_FLT_C(120933.50511269060),MAAC_FLT_C(120958.37000357473),MAAC_FLT_C(120983.23617236665),
    MAAC_FLT_C(121008.10361893504),MAAC_FLT_C(121032.97234314861),MAAC_FLT_C(121057.84234487606),MAAC_FLT_C(121082.71362398617),
    MAAC_FLT_C(121107.58618034775),MAAC_FLT_C(121132.46001382964),MAAC_FLT_C(121157.33512430069),MAAC_FLT_C(121182.21151162982),
    MAAC_FLT_C(121207.08917568595),MAAC_FLT_C(121231.96811633807),MAAC_FLT_C(121256.84833345517),MAAC_FLT_C(121281.72982690629),
    MAAC_FLT_C(121306.61259656049),MAAC_FLT_C(121331.49664228689),MAAC_FLT_C(121356.38196395461),MAAC_FLT_C(121381.26856143285),
    MAAC_FLT_C(121406.15643459078),MAAC_FLT_C(121431.04558329767),MAAC_FLT_C(121455.93600742276),MAAC_FLT_C(121480.82770683539),
    MAAC_FLT_C(121505.72068140487),MAAC_FLT_C(121530.61493100057),MAAC_FLT_C(121555.51045549192),MAAC_FLT_C(121580.40725474835),
    MAAC_FLT_C(121605.30532863933),MAAC_FLT_C(121630.20467703436),MAAC_FLT_C(121655.10529980299),MAAC_FLT_C(121680.00719681478),
    MAAC_FLT_C(121704.91036793934),MAAC_FLT_C(121729.81481304632),MAAC_FLT_C(121754.72053200539),MAAC_FLT_C(121779.62752468624),
    MAAC_FLT_C(121804.53579095862),MAAC_FLT_C(121829.44533069231),MAAC_FLT_C(121854.35614375710),MAAC_FLT_C(121879.26823002285),
    MAAC_FLT_C(121904.18158935940),MAAC_FLT_C(121929.09622163669),MAAC_FLT_C(121954.01212672464),MAAC_FLT_C(121978.92930449323),
    MAAC_FLT_C(122003.84775481246),MAAC_FLT_C(122028.76747755238),MAAC_FLT_C(122053.68847258303),MAAC_FLT_C(122078.61073977455),
    MAAC_FLT_C(122103.53427899707),MAAC_FLT_C(122128.45909012076),MAAC_FLT_C(122153.38517301581),MAAC_FLT_C(122178.31252755247),
    MAAC_FLT_C(122203.24115360099),MAAC_FLT_C(122228.17105103172),MAAC_FLT_C(122253.10221971494),MAAC_FLT_C(122278.03465952107),
    MAAC_FLT_C(122302.96837032049),MAAC_FLT_C(122327.90335198362),MAAC_FLT_C(122352.83960438096),MAAC_FLT_C(122377.77712738300),
    MAAC_FLT_C(122402.71592086025),MAAC_FLT_C(122427.65598468333),MAAC_FLT_C(122452.59731872278),MAAC_FLT_C(122477.53992284928),
    MAAC_FLT_C(122502.48379693348),MAAC_FLT_C(122527.42894084606),MAAC_FLT_C(122552.37535445779),MAAC_FLT_C(122577.32303763942),
    MAAC_FLT_C(122602.27199026172),MAAC_FLT_C(122627.22221219557),MAAC_FLT_C(122652.17370331181),MAAC_FLT_C(122677.12646348133),
    MAAC_FLT_C(122702.08049257506),MAAC_FLT_C(122727.03579046397),MAAC_FLT_C(122751.99235701906),MAAC_FLT_C(122776.95019211136),
    MAAC_FLT_C(122801.90929561190),MAAC_FLT_C(122826.86966739180),MAAC_FLT_C(122851.83130732219),MAAC_FLT_C(122876.79421527422),
    MAAC_FLT_C(122901.75839111909),MAAC_FLT_C(122926.72383472799),MAAC_FLT_C(122951.69054597223),MAAC_FLT_C(122976.65852472307),
    MAAC_FLT_C(123001.62777085182),MAAC_FLT_C(123026.59828422987),MAAC_FLT_C(123051.57006472857),MAAC_FLT_C(123076.54311221937),
    MAAC_FLT_C(123101.51742657372),MAAC_FLT_C(123126.49300766307),MAAC_FLT_C(123151.46985535898),MAAC_FLT_C(123176.44796953299),
    MAAC_FLT_C(123201.42735005668),MAAC_FLT_C(123226.40799680166),MAAC_FLT_C(123251.38990963959),MAAC_FLT_C(123276.37308844214),
    MAAC_FLT_C(123301.35753308103),MAAC_FLT_C(123326.34324342800),MAAC_FLT_C(123351.33021935483),MAAC_FLT_C(123376.31846073334),
    MAAC_FLT_C(123401.30796743535),MAAC_FLT_C(123426.29873933276),MAAC_FLT_C(123451.29077629748),MAAC_FLT_C(123476.28407820144),
    MAAC_FLT_C(123501.27864491660),MAAC_FLT_C(123526.27447631498),MAAC_FLT_C(123551.27157226863),MAAC_FLT_C(123576.26993264959),
    MAAC_FLT_C(123601.26955732999),MAAC_FLT_C(123626.27044618195),MAAC_FLT_C(123651.27259907764),MAAC_FLT_C(123676.27601588926),
    MAAC_FLT_C(123701.28069648903),MAAC_FLT_C(123726.28664074924),MAAC_FLT_C(123751.29384854218),MAAC_FLT_C(123776.30231974016),
    MAAC_FLT_C(123801.31205421555),MAAC_FLT_C(123826.32305184075),MAAC_FLT_C(123851.33531248817),MAAC_FLT_C(123876.34883603029),
    MAAC_FLT_C(123901.36362233957),MAAC_FLT_C(123926.37967128855),MAAC_FLT_C(123951.39698274979),MAAC_FLT_C(123976.41555659588),
    MAAC_FLT_C(124001.43539269941),MAAC_FLT_C(124026.45649093305),MAAC_FLT_C(124051.47885116948),MAAC_FLT_C(124076.50247328142),
    MAAC_FLT_C(124101.52735714160),MAAC_FLT_C(124126.55350262282),MAAC_FLT_C(124151.58090959788),MAAC_FLT_C(124176.60957793961),
    MAAC_FLT_C(124201.63950752091),MAAC_FLT_C(124226.67069821467),MAAC_FLT_C(124251.70314989384),MAAC_FLT_C(124276.73686243138),
    MAAC_FLT_C(124301.77183570030),MAAC_FLT_C(124326.80806957364),MAAC_FLT_C(124351.84556392446),MAAC_FLT_C(124376.88431862585),
    MAAC_FLT_C(124401.92433355095),MAAC_FLT_C(124426.96560857292),MAAC_FLT_C(124452.00814356498),MAAC_FLT_C(124477.05193840031),
    MAAC_FLT_C(124502.09699295220),MAAC_FLT_C(124527.14330709392),MAAC_FLT_C(124552.19088069882),MAAC_FLT_C(124577.23971364023),
    MAAC_FLT_C(124602.28980579154),MAAC_FLT_C(124627.34115702618),MAAC_FLT_C(124652.39376721760),MAAC_FLT_C(124677.44763623926),
    MAAC_FLT_C(124702.50276396469),MAAC_FLT_C(124727.55915026742),MAAC_FLT_C(124752.61679502104),MAAC_FLT_C(124777.67569809916),
    MAAC_FLT_C(124802.73585937542),MAAC_FLT_C(124827.79727872348),MAAC_FLT_C(124852.85995601704),MAAC_FLT_C(124877.92389112986),
    MAAC_FLT_C(124902.98908393568),MAAC_FLT_C(124928.05553430831),MAAC_FLT_C(124953.12324212160),MAAC_FLT_C(124978.19220724938),
    MAAC_FLT_C(125003.26242956554),MAAC_FLT_C(125028.33390894404),MAAC_FLT_C(125053.40664525882),MAAC_FLT_C(125078.48063838384),
    MAAC_FLT_C(125103.55588819318),MAAC_FLT_C(125128.63239456083),MAAC_FLT_C(125153.71015736091),MAAC_FLT_C(125178.78917646752),
    MAAC_FLT_C(125203.86945175481),MAAC_FLT_C(125228.95098309696),MAAC_FLT_C(125254.03377036817),MAAC_FLT_C(125279.11781344270),
    MAAC_FLT_C(125304.20311219479),MAAC_FLT_C(125329.28966649878),MAAC_FLT_C(125354.37747622898),MAAC_FLT_C(125379.46654125977),
    MAAC_FLT_C(125404.55686146552),MAAC_FLT_C(125429.64843672070),MAAC_FLT_C(125454.74126689974),MAAC_FLT_C(125479.83535187715),
    MAAC_FLT_C(125504.93069152744),MAAC_FLT_C(125530.02728572517),MAAC_FLT_C(125555.12513434493),MAAC_FLT_C(125580.22423726133),
    MAAC_FLT_C(125605.32459434902),MAAC_FLT_C(125630.42620548270),MAAC_FLT_C(125655.52907053704),MAAC_FLT_C(125680.63318938682),
    MAAC_FLT_C(125705.73856190679),MAAC_FLT_C(125730.84518797178),MAAC_FLT_C(125755.95306745660),MAAC_FLT_C(125781.06220023613),
    MAAC_FLT_C(125806.17258618528),MAAC_FLT_C(125831.28422517896),MAAC_FLT_C(125856.39711709213),MAAC_FLT_C(125881.51126179981),
    MAAC_FLT_C(125906.62665917698),MAAC_FLT_C(125931.74330909875),MAAC_FLT_C(125956.86121144016),MAAC_FLT_C(125981.98036607634),
    MAAC_FLT_C(126007.10077288245),MAAC_FLT_C(126032.22243173365),MAAC_FLT_C(126057.34534250517),MAAC_FLT_C(126082.46950507225),
    MAAC_FLT_C(126107.59491931014),MAAC_FLT_C(126132.72158509417),MAAC_FLT_C(126157.84950229966),MAAC_FLT_C(126182.97867080198),
    MAAC_FLT_C(126208.10909047653),MAAC_FLT_C(126233.24076119871),MAAC_FLT_C(126258.37368284403),MAAC_FLT_C(126283.50785528794),
    MAAC_FLT_C(126308.64327840599),MAAC_FLT_C(126333.77995207370),MAAC_FLT_C(126358.91787616667),MAAC_FLT_C(126384.05705056050),
    MAAC_FLT_C(126409.19747513086),MAAC_FLT_C(126434.33914975340),MAAC_FLT_C(126459.48207430386),MAAC_FLT_C(126484.62624865794),
    MAAC_FLT_C(126509.77167269142),MAAC_FLT_C(126534.91834628010),MAAC_FLT_C(126560.06626929982),MAAC_FLT_C(126585.21544162642),
    MAAC_FLT_C(126610.36586313581),MAAC_FLT_C(126635.51753370393),MAAC_FLT_C(126660.67045320668),MAAC_FLT_C(126685.82462152008),
    MAAC_FLT_C(126710.98003852014),MAAC_FLT_C(126736.13670408291),MAAC_FLT_C(126761.29461808444),MAAC_FLT_C(126786.45378040087),
    MAAC_FLT_C(126811.61419090834),MAAC_FLT_C(126836.77584948298),MAAC_FLT_C(126861.93875600102),MAAC_FLT_C(126887.10291033868),
    MAAC_FLT_C(126912.26831237224),MAAC_FLT_C(126937.43496197795),MAAC_FLT_C(126962.60285903217),MAAC_FLT_C(126987.77200341123),
    MAAC_FLT_C(127012.94239499152),MAAC_FLT_C(127038.11403364947),MAAC_FLT_C(127063.28691926150),MAAC_FLT_C(127088.46105170409),
    MAAC_FLT_C(127113.63643085376),MAAC_FLT_C(127138.81305658702),MAAC_FLT_C(127163.99092878048),MAAC_FLT_C(127189.17004731069),
    MAAC_FLT_C(127214.35041205429),MAAC_FLT_C(127239.53202288797),MAAC_FLT_C(127264.71487968838),MAAC_FLT_C(127289.89898233226),
    MAAC_FLT_C(127315.08433069635),MAAC_FLT_C(127340.27092465744),MAAC_FLT_C(127365.45876409234),MAAC_FLT_C(127390.64784887788),
    MAAC_FLT_C(127415.83817889093),MAAC_FLT_C(127441.02975400841),MAAC_FLT_C(127466.22257410725),MAAC_FLT_C(127491.41663906439),
    MAAC_FLT_C(127516.61194875685),MAAC_FLT_C(127541.80850306165),MAAC_FLT_C(127567.00630185583),MAAC_FLT_C(127592.20534501647),
    MAAC_FLT_C(127617.40563242070),MAAC_FLT_C(127642.60716394568),MAAC_FLT_C(127667.80993946856),MAAC_FLT_C(127693.01395886653),
    MAAC_FLT_C(127718.21922201688),MAAC_FLT_C(127743.42572879682),MAAC_FLT_C(127768.63347908368),MAAC_FLT_C(127793.84247275478),
    MAAC_FLT_C(127819.05270968749),MAAC_FLT_C(127844.26418975917),MAAC_FLT_C(127869.47691284724),MAAC_FLT_C(127894.69087882918),
    MAAC_FLT_C(127919.90608758242),MAAC_FLT_C(127945.12253898452),MAAC_FLT_C(127970.34023291297),MAAC_FLT_C(127995.55916924537),
    MAAC_FLT_C(128020.77934785932),MAAC_FLT_C(128046.00076863244),MAAC_FLT_C(128071.22343144237),MAAC_FLT_C(128096.44733616684),
    MAAC_FLT_C(128121.67248268353),MAAC_FLT_C(128146.89887087021),MAAC_FLT_C(128172.12650060465),MAAC_FLT_C(128197.35537176467),
    MAAC_FLT_C(128222.58548422810),MAAC_FLT_C(128247.81683787282),MAAC_FLT_C(128273.04943257671),MAAC_FLT_C(128298.28326821771),
    MAAC_FLT_C(128323.51834467379),MAAC_FLT_C(128348.75466182294),MAAC_FLT_C(128373.99221954316),MAAC_FLT_C(128399.23101771252),
    MAAC_FLT_C(128424.47105620909),MAAC_FLT_C(128449.71233491098),MAAC_FLT_C(128474.95485369631),MAAC_FLT_C(128500.19861244329),
    MAAC_FLT_C(128525.44361103009),MAAC_FLT_C(128550.68984933494),MAAC_FLT_C(128575.93732723613),MAAC_FLT_C(128601.18604461191),
    MAAC_FLT_C(128626.43600134061),MAAC_FLT_C(128651.68719730059),MAAC_FLT_C(128676.93963237021),MAAC_FLT_C(128702.19330642790),
    MAAC_FLT_C(128727.44821935208),MAAC_FLT_C(128752.70437102125),MAAC_FLT_C(128777.96176131385),MAAC_FLT_C(128803.22039010846),
    MAAC_FLT_C(128828.48025728362),MAAC_FLT_C(128853.74136271792),MAAC_FLT_C(128879.00370628996),MAAC_FLT_C(128904.26728787841),
    MAAC_FLT_C(128929.53210736193),MAAC_FLT_C(128954.79816461923),MAAC_FLT_C(128980.06545952905),MAAC_FLT_C(129005.33399197015),
    MAAC_FLT_C(129030.60376182134),MAAC_FLT_C(129055.87476896142),MAAC_FLT_C(129081.14701326926),MAAC_FLT_C(129106.42049462376),
    MAAC_FLT_C(129131.69521290380),MAAC_FLT_C(129156.97116798835),MAAC_FLT_C(129182.24835975636),MAAC_FLT_C(129207.52678808685),
    MAAC_FLT_C(129232.80645285884),MAAC_FLT_C(129258.08735395141),MAAC_FLT_C(129283.36949124365),MAAC_FLT_C(129308.65286461466),
    MAAC_FLT_C(129333.93747394360),MAAC_FLT_C(129359.22331910966),MAAC_FLT_C(129384.51039999202),MAAC_FLT_C(129409.79871646997),
    MAAC_FLT_C(129435.08826842274),MAAC_FLT_C(129460.37905572963),MAAC_FLT_C(129485.67107826998),MAAC_FLT_C(129510.96433592314),
    MAAC_FLT_C(129536.25882856851),MAAC_FLT_C(129561.55455608548),MAAC_FLT_C(129586.85151835352),MAAC_FLT_C(129612.14971525209),
    MAAC_FLT_C(129637.44914666070),MAAC_FLT_C(129662.74981245887),MAAC_FLT_C(129688.05171252620),MAAC_FLT_C(129713.35484674224),
    MAAC_FLT_C(129738.65921498663),MAAC_FLT_C(129763.96481713903),MAAC_FLT_C(129789.27165307909),MAAC_FLT_C(129814.57972268655),
    MAAC_FLT_C(129839.88902584116),MAAC_FLT_C(129865.19956242264),MAAC_FLT_C(129890.51133231082),MAAC_FLT_C(129915.82433538554),
    MAAC_FLT_C(129941.13857152662),MAAC_FLT_C(129966.45404061397),MAAC_FLT_C(129991.77074252750),MAAC_FLT_C(130017.08867714716),
    MAAC_FLT_C(130042.40784435290),MAAC_FLT_C(130067.72824402474),MAAC_FLT_C(130093.04987604271),MAAC_FLT_C(130118.37274028687),
    MAAC_FLT_C(130143.69683663732),MAAC_FLT_C(130169.02216497416),MAAC_FLT_C(130194.34872517755),MAAC_FLT_C(130219.67651712766),
    MAAC_FLT_C(130245.00554070470),MAAC_FLT_C(130270.33579578891),MAAC_FLT_C(130295.66728226055),MAAC_FLT_C(130320.99999999991),
    MAAC_FLT_C(130346.33394888733),MAAC_FLT_C(130371.66912880314),MAAC_FLT_C(130397.00553962773),MAAC_FLT_C(130422.34318124152),
    MAAC_FLT_C(130447.68205352494),MAAC_FLT_C(130473.02215635845),MAAC_FLT_C(130498.36348962256),MAAC_FLT_C(130523.70605319779),
    MAAC_FLT_C(130549.04984696470),MAAC_FLT_C(130574.39487080388),MAAC_FLT_C(130599.74112459592),MAAC_FLT_C(130625.08860822149),
    MAAC_FLT_C(130650.43732156123),MAAC_FLT_C(130675.78726449587),MAAC_FLT_C(130701.13843690613),MAAC_FLT_C(130726.49083867275),
    MAAC_FLT_C(130751.84446967654),MAAC_FLT_C(130777.19932979831),MAAC_FLT_C(130802.55541891890),MAAC_FLT_C(130827.91273691918),
    MAAC_FLT_C(130853.27128368006),MAAC_FLT_C(130878.63105908247),MAAC_FLT_C(130903.99206300738),MAAC_FLT_C(130929.35429533575),
    MAAC_FLT_C(130954.71775594862),MAAC_FLT_C(130980.08244472703),MAAC_FLT_C(131005.44836155206),MAAC_FLT_C(131030.81550630482),
    MAAC_FLT_C(131056.18387886642),MAAC_FLT_C(131081.55347911804),MAAC_FLT_C(131106.92430694087),MAAC_FLT_C(131132.29636221612),
    MAAC_FLT_C(131157.66964482504),MAAC_FLT_C(131183.04415464890),MAAC_FLT_C(131208.41989156904),MAAC_FLT_C(131233.79685546676),
    MAAC_FLT_C(131259.17504622342),MAAC_FLT_C(131284.55446372041),MAAC_FLT_C(131309.93510783918),MAAC_FLT_C(131335.31697846117),
    MAAC_FLT_C(131360.70007546784),MAAC_FLT_C(131386.08439874070),MAAC_FLT_C(131411.46994816128),MAAC_FLT_C(131436.85672361116),
    MAAC_FLT_C(131462.24472497194),MAAC_FLT_C(131487.63395212521),MAAC_FLT_C(131513.02440495262),MAAC_FLT_C(131538.41608333588),
    MAAC_FLT_C(131563.80898715663),MAAC_FLT_C(131589.20311629670),MAAC_FLT_C(131614.59847063778),MAAC_FLT_C(131639.99505006170),
    MAAC_FLT_C(131665.39285445024),MAAC_FLT_C(131690.79188368531),MAAC_FLT_C(131716.19213764873),MAAC_FLT_C(131741.59361622241),
    MAAC_FLT_C(131766.99631928830),MAAC_FLT_C(131792.40024672839),MAAC_FLT_C(131817.80539842462),MAAC_FLT_C(131843.21177425905),
    MAAC_FLT_C(131868.61937411371),MAAC_FLT_C(131894.02819787065),MAAC_FLT_C(131919.43824541202),MAAC_FLT_C(131944.84951661993),
    MAAC_FLT_C(131970.26201137656),MAAC_FLT_C(131995.67572956407),MAAC_FLT_C(132021.09067106468),MAAC_FLT_C(132046.50683576067),
    MAAC_FLT_C(132071.92422353430),MAAC_FLT_C(132097.34283426782),MAAC_FLT_C(132122.76266784366),MAAC_FLT_C(132148.18372414410),
    MAAC_FLT_C(132173.60600305157),MAAC_FLT_C(132199.02950444847),MAAC_FLT_C(132224.45422821722),MAAC_FLT_C(132249.88017424036),
    MAAC_FLT_C(132275.30734240031),MAAC_FLT_C(132300.73573257966),MAAC_FLT_C(132326.16534466096),MAAC_FLT_C(132351.59617852676),
    MAAC_FLT_C(132377.02823405969),MAAC_FLT_C(132402.46151114244),MAAC_FLT_C(132427.89600965759),MAAC_FLT_C(132453.33172948789),
    MAAC_FLT_C(132478.76867051609),MAAC_FLT_C(132504.20683262491),MAAC_FLT_C(132529.64621569714),MAAC_FLT_C(132555.08681961559),
    MAAC_FLT_C(132580.52864426310),MAAC_FLT_C(132605.97168952253),MAAC_FLT_C(132631.41595527678),MAAC_FLT_C(132656.86144140881),
    MAAC_FLT_C(132682.30814780149),MAAC_FLT_C(132707.75607433787),MAAC_FLT_C(132733.20522090094),MAAC_FLT_C(132758.65558737374),
    MAAC_FLT_C(132784.10717363929),MAAC_FLT_C(132809.55997958075),MAAC_FLT_C(132835.01400508118),MAAC_FLT_C(132860.46925002377),
    MAAC_FLT_C(132885.92571429166),MAAC_FLT_C(132911.38339776811),MAAC_FLT_C(132936.84230033628),MAAC_FLT_C(132962.30242187946),
    MAAC_FLT_C(132987.76376228096),MAAC_FLT_C(133013.22632142407),MAAC_FLT_C(133038.69009919214),MAAC_FLT_C(133064.15509546854),
    MAAC_FLT_C(133089.62131013666),MAAC_FLT_C(133115.08874307995),MAAC_FLT_C(133140.55739418184),MAAC_FLT_C(133166.02726332581),
    MAAC_FLT_C(133191.49835039541),MAAC_FLT_C(133216.97065527414),MAAC_FLT_C(133242.44417784561),MAAC_FLT_C(133267.91891799335),
    MAAC_FLT_C(133293.39487560102),MAAC_FLT_C(133318.87205055228),MAAC_FLT_C(133344.35044273079),MAAC_FLT_C(133369.83005202023),
    MAAC_FLT_C(133395.31087830439),MAAC_FLT_C(133420.79292146701),MAAC_FLT_C(133446.27618139185),MAAC_FLT_C(133471.76065796276),
    MAAC_FLT_C(133497.24635106357),MAAC_FLT_C(133522.73326057816),MAAC_FLT_C(133548.22138639039),MAAC_FLT_C(133573.71072838426),
    MAAC_FLT_C(133599.20128644365),MAAC_FLT_C(133624.69306045261),MAAC_FLT_C(133650.18605029510),MAAC_FLT_C(133675.68025585517),
    MAAC_FLT_C(133701.17567701690),MAAC_FLT_C(133726.67231366437),MAAC_FLT_C(133752.17016568172),MAAC_FLT_C(133777.66923295305),
    MAAC_FLT_C(133803.16951536259),MAAC_FLT_C(133828.67101279454),MAAC_FLT_C(133854.17372513309),MAAC_FLT_C(133879.67765226253),
    MAAC_FLT_C(133905.18279406714),MAAC_FLT_C(133930.68915043125),MAAC_FLT_C(133956.19672123916),MAAC_FLT_C(133981.70550637526),
    MAAC_FLT_C(134007.21550572399),MAAC_FLT_C(134032.72671916970),MAAC_FLT_C(134058.23914659690),MAAC_FLT_C(134083.75278789000),
    MAAC_FLT_C(134109.26764293358),MAAC_FLT_C(134134.78371161217),MAAC_FLT_C(134160.30099381026),MAAC_FLT_C(134185.81948941250),
    MAAC_FLT_C(134211.33919830353),MAAC_FLT_C(134236.86012036790),MAAC_FLT_C(134262.38225549037),MAAC_FLT_C(134287.90560355558),
    MAAC_FLT_C(134313.43016444831),MAAC_FLT_C(134338.95593805326),MAAC_FLT_C(134364.48292425525),MAAC_FLT_C(134390.01112293909),
    MAAC_FLT_C(134415.54053398955),MAAC_FLT_C(134441.07115729159),MAAC_FLT_C(134466.60299273001),MAAC_FLT_C(134492.13604018980),
    MAAC_FLT_C(134517.67029955584),MAAC_FLT_C(134543.20577071316),MAAC_FLT_C(134568.74245354676),MAAC_FLT_C(134594.28034794159),
    MAAC_FLT_C(134619.81945378278),MAAC_FLT_C(134645.35977095537),MAAC_FLT_C(134670.90129934452),MAAC_FLT_C(134696.44403883530),
    MAAC_FLT_C(134721.98798931291),MAAC_FLT_C(134747.53315066252),MAAC_FLT_C(134773.07952276937),MAAC_FLT_C(134798.62710551871),
    MAAC_FLT_C(134824.17589879577),MAAC_FLT_C(134849.72590248589),MAAC_FLT_C(134875.27711647438),MAAC_FLT_C(134900.82954064661),
    MAAC_FLT_C(134926.38317488792),MAAC_FLT_C(134951.93801908373),MAAC_FLT_C(134977.49407311951),MAAC_FLT_C(135003.05133688069),
    MAAC_FLT_C(135028.60981025276),MAAC_FLT_C(135054.16949312127),MAAC_FLT_C(135079.73038537172),MAAC_FLT_C(135105.29248688967),
    MAAC_FLT_C(135130.85579756077),MAAC_FLT_C(135156.42031727062),MAAC_FLT_C(135181.98604590484),MAAC_FLT_C(135207.55298334916),
    MAAC_FLT_C(135233.12112948924),MAAC_FLT_C(135258.69048421088),MAAC_FLT_C(135284.26104739975),MAAC_FLT_C(135309.83281894168),
    MAAC_FLT_C(135335.40579872250),MAAC_FLT_C(135360.97998662802),MAAC_FLT_C(135386.55538254412),MAAC_FLT_C(135412.13198635669),
    MAAC_FLT_C(135437.70979795168),MAAC_FLT_C(135463.28881721498),MAAC_FLT_C(135488.86904403262),MAAC_FLT_C(135514.45047829056),
    MAAC_FLT_C(135540.03311987486),MAAC_FLT_C(135565.61696867159),MAAC_FLT_C(135591.20202456677),MAAC_FLT_C(135616.78828744654),
    MAAC_FLT_C(135642.37575719706),MAAC_FLT_C(135667.96443370447),MAAC_FLT_C(135693.55431685498),MAAC_FLT_C(135719.14540653475),
    MAAC_FLT_C(135744.73770263011),MAAC_FLT_C(135770.33120502727),MAAC_FLT_C(135795.92591361253),MAAC_FLT_C(135821.52182827223),
    MAAC_FLT_C(135847.11894889272),MAAC_FLT_C(135872.71727536040),MAAC_FLT_C(135898.31680756161),MAAC_FLT_C(135923.91754538284),
    MAAC_FLT_C(135949.51948871053),MAAC_FLT_C(135975.12263743114),MAAC_FLT_C(136000.72699143123),MAAC_FLT_C(136026.33255059729),
    MAAC_FLT_C(136051.93931481591),MAAC_FLT_C(136077.54728397369),MAAC_FLT_C(136103.15645795723),MAAC_FLT_C(136128.76683665317),
    MAAC_FLT_C(136154.37841994822),MAAC_FLT_C(136179.99120772901),MAAC_FLT_C(136205.60519988232),MAAC_FLT_C(136231.22039629490),
    MAAC_FLT_C(136256.83679685349),MAAC_FLT_C(136282.45440144493),MAAC_FLT_C(136308.07320995603),MAAC_FLT_C(136333.69322227367),
    MAAC_FLT_C(136359.31443828469),MAAC_FLT_C(136384.93685787608),MAAC_FLT_C(136410.56048093468),MAAC_FLT_C(136436.18530734754),
    MAAC_FLT_C(136461.81133700156),MAAC_FLT_C(136487.43856978384),MAAC_FLT_C(136513.06700558143),MAAC_FLT_C(136538.69664428130),
    MAAC_FLT_C(136564.32748577066),MAAC_FLT_C(136589.95952993655),MAAC_FLT_C(136615.59277666616),MAAC_FLT_C(136641.22722584667),
    MAAC_FLT_C(136666.86287736523),MAAC_FLT_C(136692.49973110916),MAAC_FLT_C(136718.13778696564),MAAC_FLT_C(136743.77704482197),
    MAAC_FLT_C(136769.41750456547),MAAC_FLT_C(136795.05916608346),MAAC_FLT_C(136820.70202926331),MAAC_FLT_C(136846.34609399244),
    MAAC_FLT_C(136871.99136015819),MAAC_FLT_C(136897.63782764805),MAAC_FLT_C(136923.28549634948),MAAC_FLT_C(136948.93436614997),
    MAAC_FLT_C(136974.58443693706),MAAC_FLT_C(137000.23570859825),MAAC_FLT_C(137025.88818102115),MAAC_FLT_C(137051.54185409332),
    MAAC_FLT_C(137077.19672770242),MAAC_FLT_C(137102.85280173609),MAAC_FLT_C(137128.51007608202),MAAC_FLT_C(137154.16855062786),
    MAAC_FLT_C(137179.82822526142),MAAC_FLT_C(137205.48909987041),MAAC_FLT_C(137231.15117434258),MAAC_FLT_C(137256.81444856580),
    MAAC_FLT_C(137282.47892242789),MAAC_FLT_C(137308.14459581667),MAAC_FLT_C(137333.81146862009),MAAC_FLT_C(137359.47954072602),
    MAAC_FLT_C(137385.14881202241),MAAC_FLT_C(137410.81928239719),MAAC_FLT_C(137436.49095173844),MAAC_FLT_C(137462.16381993407),
    MAAC_FLT_C(137487.83788687221),MAAC_FLT_C(137513.51315244089),MAAC_FLT_C(137539.18961652822),MAAC_FLT_C(137564.86727902229),
    MAAC_FLT_C(137590.54613981131),MAAC_FLT_C(137616.22619878338),MAAC_FLT_C(137641.90745582676),MAAC_FLT_C(137667.58991082967),
    MAAC_FLT_C(137693.27356368033),MAAC_FLT_C(137718.95841426702),MAAC_FLT_C(137744.64446247809),MAAC_FLT_C(137770.33170820182),
    MAAC_FLT_C(137796.02015132661),MAAC_FLT_C(137821.70979174081),MAAC_FLT_C(137847.40062933284),MAAC_FLT_C(137873.09266399115),
    MAAC_FLT_C(137898.78589560417),MAAC_FLT_C(137924.48032406042),MAAC_FLT_C(137950.17594924837),MAAC_FLT_C(137975.87277105660),
    MAAC_FLT_C(138001.57078937365),MAAC_FLT_C(138027.27000408815),MAAC_FLT_C(138052.97041508864),MAAC_FLT_C(138078.67202226384),
    MAAC_FLT_C(138104.37482550240),MAAC_FLT_C(138130.07882469296),MAAC_FLT_C(138155.78401972432),MAAC_FLT_C(138181.49041048516),
    MAAC_FLT_C(138207.19799686430),MAAC_FLT_C(138232.90677875050),MAAC_FLT_C(138258.61675603263),MAAC_FLT_C(138284.32792859949),
    MAAC_FLT_C(138310.04029633995),MAAC_FLT_C(138335.75385914298),MAAC_FLT_C(138361.46861689744),MAAC_FLT_C(138387.18456949232),
    MAAC_FLT_C(138412.90171681659),MAAC_FLT_C(138438.62005875923),MAAC_FLT_C(138464.33959520931),MAAC_FLT_C(138490.06032605586),
    MAAC_FLT_C(138515.78225118798),MAAC_FLT_C(138541.50537049473),MAAC_FLT_C(138567.22968386530),MAAC_FLT_C(138592.95519118884),
    MAAC_FLT_C(138618.68189235451),MAAC_FLT_C(138644.40978725153),MAAC_FLT_C(138670.13887576913),MAAC_FLT_C(138695.86915779658),
    MAAC_FLT_C(138721.60063322316),MAAC_FLT_C(138747.33330193823),MAAC_FLT_C(138773.06716383106),MAAC_FLT_C(138798.80221879104),
    MAAC_FLT_C(138824.53846670757),MAAC_FLT_C(138850.27590747006),MAAC_FLT_C(138876.01454096794),MAAC_FLT_C(138901.75436709070),
    MAAC_FLT_C(138927.49538572782),MAAC_FLT_C(138953.23759676880),MAAC_FLT_C(138978.98100010320),MAAC_FLT_C(139004.72559562061),
    MAAC_FLT_C(139030.47138321059),MAAC_FLT_C(139056.21836276280),MAAC_FLT_C(139081.96653416683),MAAC_FLT_C(139107.71589731239),
    MAAC_FLT_C(139133.46645208917),MAAC_FLT_C(139159.21819838689),MAAC_FLT_C(139184.97113609532),MAAC_FLT_C(139210.72526510421),
    MAAC_FLT_C(139236.48058530336),MAAC_FLT_C(139262.23709658257),MAAC_FLT_C(139287.99479883176),MAAC_FLT_C(139313.75369194071),
    MAAC_FLT_C(139339.51377579942),MAAC_FLT_C(139365.27505029776),MAAC_FLT_C(139391.03751532568),MAAC_FLT_C(139416.80117077316),
    MAAC_FLT_C(139442.56601653024),MAAC_FLT_C(139468.33205248689),MAAC_FLT_C(139494.09927853322),MAAC_FLT_C(139519.86769455927),
    MAAC_FLT_C(139545.63730045516),MAAC_FLT_C(139571.40809611100),MAAC_FLT_C(139597.18008141697),MAAC_FLT_C(139622.95325626322),
    MAAC_FLT_C(139648.72762054001),MAAC_FLT_C(139674.50317413750),MAAC_FLT_C(139700.27991694602),MAAC_FLT_C(139726.05784885579),
    MAAC_FLT_C(139751.83696975713),MAAC_FLT_C(139777.61727954043),MAAC_FLT_C(139803.39877809596),MAAC_FLT_C(139829.18146531415),
    MAAC_FLT_C(139854.96534108539),MAAC_FLT_C(139880.75040530015),MAAC_FLT_C(139906.53665784886),MAAC_FLT_C(139932.32409862199),
    MAAC_FLT_C(139958.11272751007),MAAC_FLT_C(139983.90254440365),MAAC_FLT_C(140009.69354919327),MAAC_FLT_C(140035.48574176949),
    MAAC_FLT_C(140061.27912202294),MAAC_FLT_C(140087.07368984428),MAAC_FLT_C(140112.86944512415),MAAC_FLT_C(140138.66638775321),
    MAAC_FLT_C(140164.46451762220),MAAC_FLT_C(140190.26383462184),MAAC_FLT_C(140216.06433864293),MAAC_FLT_C(140241.86602957622),
    MAAC_FLT_C(140267.66890731253),MAAC_FLT_C(140293.47297174268),MAAC_FLT_C(140319.27822275754),MAAC_FLT_C(140345.08466024802),
    MAAC_FLT_C(140370.89228410498),MAAC_FLT_C(140396.70109421943),MAAC_FLT_C(140422.51109048226),MAAC_FLT_C(140448.32227278448),
    MAAC_FLT_C(140474.13464101712),MAAC_FLT_C(140499.94819507122),MAAC_FLT_C(140525.76293483781),MAAC_FLT_C(140551.57886020801),
    MAAC_FLT_C(140577.39597107290),MAAC_FLT_C(140603.21426732364),MAAC_FLT_C(140629.03374885136),MAAC_FLT_C(140654.85441554731),
    MAAC_FLT_C(140680.67626730262),MAAC_FLT_C(140706.49930400858),MAAC_FLT_C(140732.32352555645),MAAC_FLT_C(140758.14893183750),
    MAAC_FLT_C(140783.97552274304),MAAC_FLT_C(140809.80329816442),MAAC_FLT_C(140835.63225799298),MAAC_FLT_C(140861.46240212015),
    MAAC_FLT_C(140887.29373043729),MAAC_FLT_C(140913.12624283586),MAAC_FLT_C(140938.95993920733),MAAC_FLT_C(140964.79481944317),
    MAAC_FLT_C(140990.63088343487),MAAC_FLT_C(141016.46813107401),MAAC_FLT_C(141042.30656225214),MAAC_FLT_C(141068.14617686081),
    MAAC_FLT_C(141093.98697479168),MAAC_FLT_C(141119.82895593636),MAAC_FLT_C(141145.67212018650),MAAC_FLT_C(141171.51646743377),
    MAAC_FLT_C(141197.36199756994),MAAC_FLT_C(141223.20871048668),MAAC_FLT_C(141249.05660607578),MAAC_FLT_C(141274.90568422904),
    MAAC_FLT_C(141300.75594483822),MAAC_FLT_C(141326.60738779520),MAAC_FLT_C(141352.46001299180),MAAC_FLT_C(141378.31382031992),
    MAAC_FLT_C(141404.16880967148),MAAC_FLT_C(141430.02498093838),MAAC_FLT_C(141455.88233401260),MAAC_FLT_C(141481.74086878612),
    MAAC_FLT_C(141507.60058515094),MAAC_FLT_C(141533.46148299909),MAAC_FLT_C(141559.32356222265),MAAC_FLT_C(141585.18682271364),
    MAAC_FLT_C(141611.05126436421),MAAC_FLT_C(141636.91688706650),MAAC_FLT_C(141662.78369071262),MAAC_FLT_C(141688.65167519479),
    MAAC_FLT_C(141714.52084040520),MAAC_FLT_C(141740.39118623605),MAAC_FLT_C(141766.26271257963),MAAC_FLT_C(141792.13541932820),
    MAAC_FLT_C(141818.00930637406),MAAC_FLT_C(141843.88437360956),MAAC_FLT_C(141869.76062092700),MAAC_FLT_C(141895.63804821880),
    MAAC_FLT_C(141921.51665537735),MAAC_FLT_C(141947.39644229505),MAAC_FLT_C(141973.27740886438),MAAC_FLT_C(141999.15955497778),
    MAAC_FLT_C(142025.04288052776),MAAC_FLT_C(142050.92738540689),MAAC_FLT_C(142076.81306950765),MAAC_FLT_C(142102.69993272264),
    MAAC_FLT_C(142128.58797494444),MAAC_FLT_C(142154.47719606571),MAAC_FLT_C(142180.36759597904),MAAC_FLT_C(142206.25917457714),
    MAAC_FLT_C(142232.15193175265),MAAC_FLT_C(142258.04586739838),MAAC_FLT_C(142283.94098140698),MAAC_FLT_C(142309.83727367126),
    MAAC_FLT_C(142335.73474408401),MAAC_FLT_C(142361.63339253806),MAAC_FLT_C(142387.53321892620),MAAC_FLT_C(142413.43422314132),
    MAAC_FLT_C(142439.33640507635),MAAC_FLT_C(142465.23976462413),MAAC_FLT_C(142491.14430167765),MAAC_FLT_C(142517.05001612983),
    MAAC_FLT_C(142542.95690787368),MAAC_FLT_C(142568.86497680223),MAAC_FLT_C(142594.77422280848),MAAC_FLT_C(142620.68464578551),
    MAAC_FLT_C(142646.59624562637),MAAC_FLT_C(142672.50902222423),MAAC_FLT_C(142698.42297547215),MAAC_FLT_C(142724.33810526333),
    MAAC_FLT_C(142750.25441149093),MAAC_FLT_C(142776.17189404817),MAAC_FLT_C(142802.09055282827),MAAC_FLT_C(142828.01038772447),
    MAAC_FLT_C(142853.93139863008),MAAC_FLT_C(142879.85358543837),MAAC_FLT_C(142905.77694804268),MAAC_FLT_C(142931.70148633636),
    MAAC_FLT_C(142957.62720021277),MAAC_FLT_C(142983.55408956532),MAAC_FLT_C(143009.48215428743),MAAC_FLT_C(143035.41139427255),
    MAAC_FLT_C(143061.34180941415),MAAC_FLT_C(143087.27339960571),MAAC_FLT_C(143113.20616474075),MAAC_FLT_C(143139.14010471283),
    MAAC_FLT_C(143165.07521941551),MAAC_FLT_C(143191.01150874238),MAAC_FLT_C(143216.94897258704),MAAC_FLT_C(143242.88761084314),
    MAAC_FLT_C(143268.82742340435),MAAC_FLT_C(143294.76841016437),MAAC_FLT_C(143320.71057101688),MAAC_FLT_C(143346.65390585564),
    MAAC_FLT_C(143372.59841457437),MAAC_FLT_C(143398.54409706692),MAAC_FLT_C(143424.49095322701),MAAC_FLT_C(143450.43898294857),
    MAAC_FLT_C(143476.38818612538),MAAC_FLT_C(143502.33856265133),MAAC_FLT_C(143528.29011242036),MAAC_FLT_C(143554.24283532638),
    MAAC_FLT_C(143580.19673126334),MAAC_FLT_C(143606.15180012520),MAAC_FLT_C(143632.10804180597),MAAC_FLT_C(143658.06545619969),
    MAAC_FLT_C(143684.02404320039),MAAC_FLT_C(143709.98380270213),MAAC_FLT_C(143735.94473459900),MAAC_FLT_C(143761.90683878519),
    MAAC_FLT_C(143787.87011515474),MAAC_FLT_C(143813.83456360188),MAAC_FLT_C(143839.80018402080),MAAC_FLT_C(143865.76697630569),
    MAAC_FLT_C(143891.73494035081),MAAC_FLT_C(143917.70407605040),MAAC_FLT_C(143943.67438329876),MAAC_FLT_C(143969.64586199020),
    MAAC_FLT_C(143995.61851201905),MAAC_FLT_C(144021.59233327967),MAAC_FLT_C(144047.56732566646),MAAC_FLT_C(144073.54348907378),
    MAAC_FLT_C(144099.52082339607),MAAC_FLT_C(144125.49932852783),MAAC_FLT_C(144151.47900436350),MAAC_FLT_C(144177.45985079758),
    MAAC_FLT_C(144203.44186772458),MAAC_FLT_C(144229.42505503909),MAAC_FLT_C(144255.40941263564),MAAC_FLT_C(144281.39494040885),
    MAAC_FLT_C(144307.38163825331),MAAC_FLT_C(144333.36950606373),MAAC_FLT_C(144359.35854373468),MAAC_FLT_C(144385.34875116093),
    MAAC_FLT_C(144411.34012823718),MAAC_FLT_C(144437.33267485813),MAAC_FLT_C(144463.32639091855),MAAC_FLT_C(144489.32127631325),
    MAAC_FLT_C(144515.31733093705),MAAC_FLT_C(144541.31455468474),MAAC_FLT_C(144567.31294745120),MAAC_FLT_C(144593.31250913130),
    MAAC_FLT_C(144619.31323961995),MAAC_FLT_C(144645.31513881206),MAAC_FLT_C(144671.31820660262),MAAC_FLT_C(144697.32244288657),
    MAAC_FLT_C(144723.32784755889),MAAC_FLT_C(144749.33442051467),MAAC_FLT_C(144775.34216164888),MAAC_FLT_C(144801.35107085665),
    MAAC_FLT_C(144827.36114803303),MAAC_FLT_C(144853.37239307314),MAAC_FLT_C(144879.38480587213),MAAC_FLT_C(144905.39838632516),
    MAAC_FLT_C(144931.41313432742),MAAC_FLT_C(144957.42904977410),MAAC_FLT_C(144983.44613256046),MAAC_FLT_C(145009.46438258173),
    MAAC_FLT_C(145035.48379973322),MAAC_FLT_C(145061.50438391021),MAAC_FLT_C(145087.52613500805),MAAC_FLT_C(145113.54905292206),
    MAAC_FLT_C(145139.57313754765),MAAC_FLT_C(145165.59838878017),MAAC_FLT_C(145191.62480651509),MAAC_FLT_C(145217.65239064783),
    MAAC_FLT_C(145243.68114107384),MAAC_FLT_C(145269.71105768863),MAAC_FLT_C(145295.74214038774),MAAC_FLT_C(145321.77438906668),
    MAAC_FLT_C(145347.80780362099),MAAC_FLT_C(145373.84238394629),MAAC_FLT_C(145399.87812993818),MAAC_FLT_C(145425.91504149229),
    MAAC_FLT_C(145451.95311850426),MAAC_FLT_C(145477.99236086980),MAAC_FLT_C(145504.03276848458),MAAC_FLT_C(145530.07434124436),
    MAAC_FLT_C(145556.11707904484),MAAC_FLT_C(145582.16098178181),MAAC_FLT_C(145608.20604935108),MAAC_FLT_C(145634.25228164849),
    MAAC_FLT_C(145660.29967856981),MAAC_FLT_C(145686.34824001096),MAAC_FLT_C(145712.39796586783),MAAC_FLT_C(145738.44885603630),
    MAAC_FLT_C(145764.50091041232),MAAC_FLT_C(145790.55412889185),MAAC_FLT_C(145816.60851137087),MAAC_FLT_C(145842.66405774537),
    MAAC_FLT_C(145868.72076791141),MAAC_FLT_C(145894.77864176501),MAAC_FLT_C(145920.83767920226),MAAC_FLT_C(145946.89788011924),
    MAAC_FLT_C(145972.95924441208),MAAC_FLT_C(145999.02177197693),MAAC_FLT_C(146025.08546270995),MAAC_FLT_C(146051.15031650732),
    MAAC_FLT_C(146077.21633326527),MAAC_FLT_C(146103.28351288004),MAAC_FLT_C(146129.35185524789),MAAC_FLT_C(146155.42136026506),
    MAAC_FLT_C(146181.49202782792),MAAC_FLT_C(146207.56385783272),MAAC_FLT_C(146233.63685017588),MAAC_FLT_C(146259.71100475377),
    MAAC_FLT_C(146285.78632146274),MAAC_FLT_C(146311.86280019928),MAAC_FLT_C(146337.94044085976),MAAC_FLT_C(146364.01924334071),
    MAAC_FLT_C(146390.09920753856),MAAC_FLT_C(146416.18033334985),MAAC_FLT_C(146442.26262067116),MAAC_FLT_C(146468.34606939898),
    MAAC_FLT_C(146494.43067942993),MAAC_FLT_C(146520.51645066062),MAAC_FLT_C(146546.60338298764),MAAC_FLT_C(146572.69147630769),
    MAAC_FLT_C(146598.78073051741),MAAC_FLT_C(146624.87114551352),MAAC_FLT_C(146650.96272119274),MAAC_FLT_C(146677.05545745179),
    MAAC_FLT_C(146703.14935418745),MAAC_FLT_C(146729.24441129650),MAAC_FLT_C(146755.34062867577),MAAC_FLT_C(146781.43800622207),
    MAAC_FLT_C(146807.53654383228),MAAC_FLT_C(146833.63624140329),MAAC_FLT_C(146859.73709883197),MAAC_FLT_C(146885.83911601527),
    MAAC_FLT_C(146911.94229285014),MAAC_FLT_C(146938.04662923355),MAAC_FLT_C(146964.15212506248),MAAC_FLT_C(146990.25878023397),
    MAAC_FLT_C(147016.36659464505),MAAC_FLT_C(147042.47556819281),MAAC_FLT_C(147068.58570077427),MAAC_FLT_C(147094.69699228660),
    MAAC_FLT_C(147120.80944262692),MAAC_FLT_C(147146.92305169237),MAAC_FLT_C(147173.03781938014),MAAC_FLT_C(147199.15374558745),
    MAAC_FLT_C(147225.27083021149),MAAC_FLT_C(147251.38907314953),MAAC_FLT_C(147277.50847429881),MAAC_FLT_C(147303.62903355664),
    MAAC_FLT_C(147329.75075082036),MAAC_FLT_C(147355.87362598727),MAAC_FLT_C(147381.99765895473),MAAC_FLT_C(147408.12284962015),
    MAAC_FLT_C(147434.24919788091),MAAC_FLT_C(147460.37670363448),MAAC_FLT_C(147486.50536677826),MAAC_FLT_C(147512.63518720976),
    MAAC_FLT_C(147538.76616482646),MAAC_FLT_C(147564.89829952587),MAAC_FLT_C(147591.03159120557),MAAC_FLT_C(147617.16603976308),
    MAAC_FLT_C(147643.30164509601),MAAC_FLT_C(147669.43840710199),MAAC_FLT_C(147695.57632567859),MAAC_FLT_C(147721.71540072354),
    MAAC_FLT_C(147747.85563213445),MAAC_FLT_C(147773.99701980909),MAAC_FLT_C(147800.13956364512),MAAC_FLT_C(147826.28326354033),
    MAAC_FLT_C(147852.42811939248),MAAC_FLT_C(147878.57413109933),MAAC_FLT_C(147904.72129855872),MAAC_FLT_C(147930.86962166851),
    MAAC_FLT_C(147957.01910032652),MAAC_FLT_C(147983.16973443062),MAAC_FLT_C(148009.32152387875),MAAC_FLT_C(148035.47446856883),
    MAAC_FLT_C(148061.62856839882),MAAC_FLT_C(148087.78382326665),MAAC_FLT_C(148113.94023307035),MAAC_FLT_C(148140.09779770792),
    MAAC_FLT_C(148166.25651707739),MAAC_FLT_C(148192.41639107687),MAAC_FLT_C(148218.57741960438),MAAC_FLT_C(148244.73960255808),
    MAAC_FLT_C(148270.90293983606),MAAC_FLT_C(148297.06743133650),MAAC_FLT_C(148323.23307695755),MAAC_FLT_C(148349.39987659742),
    MAAC_FLT_C(148375.56783015432),MAAC_FLT_C(148401.73693752653),MAAC_FLT_C(148427.90719861226),MAAC_FLT_C(148454.07861330983),
    MAAC_FLT_C(148480.25118151752),MAAC_FLT_C(148506.42490313368),MAAC_FLT_C(148532.59977805667),MAAC_FLT_C(148558.77580618486),
    MAAC_FLT_C(148584.95298741665),MAAC_FLT_C(148611.13132165043),MAAC_FLT_C(148637.31080878471),MAAC_FLT_C(148663.49144871789),
    MAAC_FLT_C(148689.67324134850),MAAC_FLT_C(148715.85618657502),MAAC_FLT_C(148742.04028429600),MAAC_FLT_C(148768.22553440998),
    MAAC_FLT_C(148794.41193681557),MAAC_FLT_C(148820.59949141133),MAAC_FLT_C(148846.78819809589),MAAC_FLT_C(148872.97805676793),
    MAAC_FLT_C(148899.16906732606),MAAC_FLT_C(148925.36122966901),MAAC_FLT_C(148951.55454369547),MAAC_FLT_C(148977.74900930419),
    MAAC_FLT_C(149003.94462639390),MAAC_FLT_C(149030.14139486340),MAAC_FLT_C(149056.33931461151),MAAC_FLT_C(149082.53838553699),
    MAAC_FLT_C(149108.73860753875),MAAC_FLT_C(149134.93998051560),MAAC_FLT_C(149161.14250436646),MAAC_FLT_C(149187.34617899026),
    MAAC_FLT_C(149213.55100428590),MAAC_FLT_C(149239.75698015234),MAAC_FLT_C(149265.96410648854),MAAC_FLT_C(149292.17238319354),
    MAAC_FLT_C(149318.38181016635),MAAC_FLT_C(149344.59238730598),MAAC_FLT_C(149370.80411451156),MAAC_FLT_C(149397.01699168212),
    MAAC_FLT_C(149423.23101871679),MAAC_FLT_C(149449.44619551473),MAAC_FLT_C(149475.66252197503),MAAC_FLT_C(149501.87999799693),
    MAAC_FLT_C(149528.09862347960),MAAC_FLT_C(149554.31839832227),MAAC_FLT_C(149580.53932242419),MAAC_FLT_C(149606.76139568459),
    MAAC_FLT_C(149632.98461800278),MAAC_FLT_C(149659.20898927809),MAAC_FLT_C(149685.43450940982),MAAC_FLT_C(149711.66117829733),
    MAAC_FLT_C(149737.88899584001),MAAC_FLT_C(149764.11796193724),MAAC_FLT_C(149790.34807648844),MAAC_FLT_C(149816.57933939309),
    MAAC_FLT_C(149842.81175055061),MAAC_FLT_C(149869.04530986046),MAAC_FLT_C(149895.28001722222),MAAC_FLT_C(149921.51587253538),
    MAAC_FLT_C(149947.75287569952),MAAC_FLT_C(149973.99102661415),MAAC_FLT_C(150000.23032517891),MAAC_FLT_C(150026.47077129342),
    MAAC_FLT_C(150052.71236485732),MAAC_FLT_C(150078.95510577026),MAAC_FLT_C(150105.19899393190),MAAC_FLT_C(150131.44402924200),
    MAAC_FLT_C(150157.69021160025),MAAC_FLT_C(150183.93754090639),MAAC_FLT_C(150210.18601706024),MAAC_FLT_C(150236.43563996154),
    MAAC_FLT_C(150262.68640951012),MAAC_FLT_C(150288.93832560582),MAAC_FLT_C(150315.19138814852),MAAC_FLT_C(150341.44559703805),
    MAAC_FLT_C(150367.70095217437),MAAC_FLT_C(150393.95745345735),MAAC_FLT_C(150420.21510078697),MAAC_FLT_C(150446.47389406321),
    MAAC_FLT_C(150472.73383318601),MAAC_FLT_C(150498.99491805542),MAAC_FLT_C(150525.25714857146),MAAC_FLT_C(150551.52052463419),
    MAAC_FLT_C(150577.78504614369),MAAC_FLT_C(150604.05071300003),MAAC_FLT_C(150630.31752510337),MAAC_FLT_C(150656.58548235384),
    MAAC_FLT_C(150682.85458465159),MAAC_FLT_C(150709.12483189680),MAAC_FLT_C(150735.39622398972),MAAC_FLT_C(150761.66876083051),
    MAAC_FLT_C(150787.94244231950),MAAC_FLT_C(150814.21726835691),MAAC_FLT_C(150840.49323884302),MAAC_FLT_C(150866.77035367821),
    MAAC_FLT_C(150893.04861276277),MAAC_FLT_C(150919.32801599705),MAAC_FLT_C(150945.60856328148),MAAC_FLT_C(150971.89025451642),
    MAAC_FLT_C(150998.17308960229),MAAC_FLT_C(151024.45706843957),MAAC_FLT_C(151050.74219092872),MAAC_FLT_C(151077.02845697021),
    MAAC_FLT_C(151103.31586646455),MAAC_FLT_C(151129.60441931229),MAAC_FLT_C(151155.89411541400),MAAC_FLT_C(151182.18495467020),
    MAAC_FLT_C(151208.47693698155),MAAC_FLT_C(151234.77006224863),MAAC_FLT_C(151261.06433037209),MAAC_FLT_C(151287.35974125259),
    MAAC_FLT_C(151313.65629479082),MAAC_FLT_C(151339.95399088747),MAAC_FLT_C(151366.25282944329),MAAC_FLT_C(151392.55281035902),
    MAAC_FLT_C(151418.85393353543),MAAC_FLT_C(151445.15619887330),MAAC_FLT_C(151471.45960627345),MAAC_FLT_C(151497.76415563675),
    MAAC_FLT_C(151524.06984686397),MAAC_FLT_C(151550.37667985607),MAAC_FLT_C(151576.68465451393),MAAC_FLT_C(151602.99377073845),
    MAAC_FLT_C(151629.30402843058),MAAC_FLT_C(151655.61542749128),MAAC_FLT_C(151681.92796782157),MAAC_FLT_C(151708.24164932242),
    MAAC_FLT_C(151734.55647189484),MAAC_FLT_C(151760.87243543993),MAAC_FLT_C(151787.18953985872),MAAC_FLT_C(151813.50778505235),
    MAAC_FLT_C(151839.82717092187),MAAC_FLT_C(151866.14769736846),MAAC_FLT_C(151892.46936429327),MAAC_FLT_C(151918.79217159748),
    MAAC_FLT_C(151945.11611918229),MAAC_FLT_C(151971.44120694889),MAAC_FLT_C(151997.76743479856),MAAC_FLT_C(152024.09480263255),
    MAAC_FLT_C(152050.42331035214),MAAC_FLT_C(152076.75295785864),MAAC_FLT_C(152103.08374505339),MAAC_FLT_C(152129.41567183772),
    MAAC_FLT_C(152155.74873811303),MAAC_FLT_C(152182.08294378067),MAAC_FLT_C(152208.41828874208),MAAC_FLT_C(152234.75477289871),
    MAAC_FLT_C(152261.09239615197),MAAC_FLT_C(152287.43115840337),MAAC_FLT_C(152313.77105955439),MAAC_FLT_C(152340.11209950657),
    MAAC_FLT_C(152366.45427816146),MAAC_FLT_C(152392.79759542056),MAAC_FLT_C(152419.14205118554),MAAC_FLT_C(152445.48764535793),
    MAAC_FLT_C(152471.83437783940),MAAC_FLT_C(152498.18224853161),MAAC_FLT_C(152524.53125733617),MAAC_FLT_C(152550.88140415482),
    MAAC_FLT_C(152577.23268888926),MAAC_FLT_C(152603.58511144121),MAAC_FLT_C(152629.93867171241),MAAC_FLT_C(152656.29336960468),
    MAAC_FLT_C(152682.64920501978),MAAC_FLT_C(152709.00617785956),MAAC_FLT_C(152735.36428802583),MAAC_FLT_C(152761.72353542043),
    MAAC_FLT_C(152788.08391994529),MAAC_FLT_C(152814.44544150229),MAAC_FLT_C(152840.80809999333),MAAC_FLT_C(152867.17189532038),
    MAAC_FLT_C(152893.53682738543),MAAC_FLT_C(152919.90289609041),MAAC_FLT_C(152946.27010133737),MAAC_FLT_C(152972.63844302832),
    MAAC_FLT_C(152999.00792106529),MAAC_FLT_C(153025.37853535041),MAAC_FLT_C(153051.75028578570),MAAC_FLT_C(153078.12317227334),
    MAAC_FLT_C(153104.49719471540),MAAC_FLT_C(153130.87235301410),MAAC_FLT_C(153157.24864707157),MAAC_FLT_C(153183.62607679001),
    MAAC_FLT_C(153210.00464207167),MAAC_FLT_C(153236.38434281875),MAAC_FLT_C(153262.76517893354),MAAC_FLT_C(153289.14715031828),
    MAAC_FLT_C(153315.53025687535),MAAC_FLT_C(153341.91449850702),MAAC_FLT_C(153368.29987511560),MAAC_FLT_C(153394.68638660354),
    MAAC_FLT_C(153421.07403287315),MAAC_FLT_C(153447.46281382689),MAAC_FLT_C(153473.85272936718),MAAC_FLT_C(153500.24377939643),
    MAAC_FLT_C(153526.63596381716),MAAC_FLT_C(153553.02928253182),MAAC_FLT_C(153579.42373544295),MAAC_FLT_C(153605.81932245308),
    MAAC_FLT_C(153632.21604346478),MAAC_FLT_C(153658.61389838057),MAAC_FLT_C(153685.01288710310),MAAC_FLT_C(153711.41300953497),
    MAAC_FLT_C(153737.81426557881),MAAC_FLT_C(153764.21665513728),MAAC_FLT_C(153790.62017811305),MAAC_FLT_C(153817.02483440886),
    MAAC_FLT_C(153843.43062392739),MAAC_FLT_C(153869.83754657139),MAAC_FLT_C(153896.24560224367),MAAC_FLT_C(153922.65479084692),
    MAAC_FLT_C(153949.06511228404),MAAC_FLT_C(153975.47656645780),MAAC_FLT_C(154001.88915327107),MAAC_FLT_C(154028.30287262669),
    MAAC_FLT_C(154054.71772442761),MAAC_FLT_C(154081.13370857667),MAAC_FLT_C(154107.55082497682),MAAC_FLT_C(154133.96907353101),
    MAAC_FLT_C(154160.38845414223),MAAC_FLT_C(154186.80896671346),MAAC_FLT_C(154213.23061114774),MAAC_FLT_C(154239.65338734805),
    MAAC_FLT_C(154266.07729521746),MAAC_FLT_C(154292.50233465908),MAAC_FLT_C(154318.92850557598),MAAC_FLT_C(154345.35580787127),
    MAAC_FLT_C(154371.78424144810),MAAC_FLT_C(154398.21380620965),MAAC_FLT_C(154424.64450205903),MAAC_FLT_C(154451.07632889951),
    MAAC_FLT_C(154477.50928663427),MAAC_FLT_C(154503.94337516659),MAAC_FLT_C(154530.37859439969),MAAC_FLT_C(154556.81494423689),
    MAAC_FLT_C(154583.25242458144),MAAC_FLT_C(154609.69103533673),MAAC_FLT_C(154636.13077640603),MAAC_FLT_C(154662.57164769279),
    MAAC_FLT_C(154689.01364910032),MAAC_FLT_C(154715.45678053208),MAAC_FLT_C(154741.90104189145),MAAC_FLT_C(154768.34643308193),
    MAAC_FLT_C(154794.79295400696),MAAC_FLT_C(154821.24060457002),MAAC_FLT_C(154847.68938467462),MAAC_FLT_C(154874.13929422433),
    MAAC_FLT_C(154900.59033312264),MAAC_FLT_C(154927.04250127316),MAAC_FLT_C(154953.49579857948),MAAC_FLT_C(154979.95022494521),
    MAAC_FLT_C(155006.40578027396),MAAC_FLT_C(155032.86246446942),MAAC_FLT_C(155059.32027743524),MAAC_FLT_C(155085.77921907514),
    MAAC_FLT_C(155112.23928929280),MAAC_FLT_C(155138.70048799197),MAAC_FLT_C(155165.16281507642),MAAC_FLT_C(155191.62627044989),
    MAAC_FLT_C(155218.09085401625),MAAC_FLT_C(155244.55656567923),MAAC_FLT_C(155271.02340534274),MAAC_FLT_C(155297.49137291059),
    MAAC_FLT_C(155323.96046828668),MAAC_FLT_C(155350.43069137490),MAAC_FLT_C(155376.90204207919),MAAC_FLT_C(155403.37452030348),
    MAAC_FLT_C(155429.84812595171),MAAC_FLT_C(155456.32285892789),MAAC_FLT_C(155482.79871913602),MAAC_FLT_C(155509.27570648011),
    MAAC_FLT_C(155535.75382086422),MAAC_FLT_C(155562.23306219239),MAAC_FLT_C(155588.71343036872),MAAC_FLT_C(155615.19492529731),
    MAAC_FLT_C(155641.67754688227),MAAC_FLT_C(155668.16129502779),MAAC_FLT_C(155694.64616963797),MAAC_FLT_C(155721.13217061706),
    MAAC_FLT_C(155747.61929786921),MAAC_FLT_C(155774.10755129869),MAAC_FLT_C(155800.59693080973),MAAC_FLT_C(155827.08743630661),
    MAAC_FLT_C(155853.57906769359),MAAC_FLT_C(155880.07182487496),MAAC_FLT_C(155906.56570775513),MAAC_FLT_C(155933.06071623837),
    MAAC_FLT_C(155959.55685022910),MAAC_FLT_C(155986.05410963166),MAAC_FLT_C(156012.55249435050),MAAC_FLT_C(156039.05200429002),
    MAAC_FLT_C(156065.55263935472),MAAC_FLT_C(156092.05439944900),MAAC_FLT_C(156118.55728447740),MAAC_FLT_C(156145.06129434443),
    MAAC_FLT_C(156171.56642895460),MAAC_FLT_C(156198.07268821247),MAAC_FLT_C(156224.58007202260),MAAC_FLT_C(156251.08858028959),
    MAAC_FLT_C(156277.59821291809),MAAC_FLT_C(156304.10896981266),MAAC_FLT_C(156330.62085087801),MAAC_FLT_C(156357.13385601880),
    MAAC_FLT_C(156383.64798513969),MAAC_FLT_C(156410.16323814544),MAAC_FLT_C(156436.67961494075),MAAC_FLT_C(156463.19711543040),
    MAAC_FLT_C(156489.71573951913),MAAC_FLT_C(156516.23548711176),MAAC_FLT_C(156542.75635811311),MAAC_FLT_C(156569.27835242799),
    MAAC_FLT_C(156595.80146996127),MAAC_FLT_C(156622.32571061782),MAAC_FLT_C(156648.85107430254),MAAC_FLT_C(156675.37756092031),
    MAAC_FLT_C(156701.90517037612),MAAC_FLT_C(156728.43390257491),MAAC_FLT_C(156754.96375742162),MAAC_FLT_C(156781.49473482129),
    MAAC_FLT_C(156808.02683467889),MAAC_FLT_C(156834.56005689953),MAAC_FLT_C(156861.09440138817),MAAC_FLT_C(156887.62986804993),
    MAAC_FLT_C(156914.16645678994),MAAC_FLT_C(156940.70416751326),MAAC_FLT_C(156967.24300012505),MAAC_FLT_C(156993.78295453047),
    MAAC_FLT_C(157020.32403063469),MAAC_FLT_C(157046.86622834290),MAAC_FLT_C(157073.40954756032),MAAC_FLT_C(157099.95398819220),
    MAAC_FLT_C(157126.49955014378),MAAC_FLT_C(157153.04623332032),MAAC_FLT_C(157179.59403762716),MAAC_FLT_C(157206.14296296958),
    MAAC_FLT_C(157232.69300925292),MAAC_FLT_C(157259.24417638258),MAAC_FLT_C(157285.79646426387),MAAC_FLT_C(157312.34987280221),
    MAAC_FLT_C(157338.90440190304),MAAC_FLT_C(157365.46005147175),MAAC_FLT_C(157392.01682141385),MAAC_FLT_C(157418.57471163478),
    MAAC_FLT_C(157445.13372204005),MAAC_FLT_C(157471.69385253513),MAAC_FLT_C(157498.25510302564),MAAC_FLT_C(157524.81747341706),
    MAAC_FLT_C(157551.38096361503),MAAC_FLT_C(157577.94557352510),MAAC_FLT_C(157604.51130305286),MAAC_FLT_C(157631.07815210402),
    MAAC_FLT_C(157657.64612058416),MAAC_FLT_C(157684.21520839902),MAAC_FLT_C(157710.78541545427),MAAC_FLT_C(157737.35674165559),
    MAAC_FLT_C(157763.92918690876),MAAC_FLT_C(157790.50275111952),MAAC_FLT_C(157817.07743419363),MAAC_FLT_C(157843.65323603692),
    MAAC_FLT_C(157870.23015655516),MAAC_FLT_C(157896.80819565422),MAAC_FLT_C(157923.38735323990),MAAC_FLT_C(157949.96762921812),
    MAAC_FLT_C(157976.54902349479),MAAC_FLT_C(158003.13153597576),MAAC_FLT_C(158029.71516656701),MAAC_FLT_C(158056.29991517449),
    MAAC_FLT_C(158082.88578170416),MAAC_FLT_C(158109.47276606198),MAAC_FLT_C(158136.06086815402),MAAC_FLT_C(158162.65008788629),
    MAAC_FLT_C(158189.24042516484),MAAC_FLT_C(158215.83187989573),MAAC_FLT_C(158242.42445198505),MAAC_FLT_C(158269.01814133892),
    MAAC_FLT_C(158295.61294786347),MAAC_FLT_C(158322.20887146486),MAAC_FLT_C(158348.80591204923),MAAC_FLT_C(158375.40406952280),
    MAAC_FLT_C(158402.00334379176),MAAC_FLT_C(158428.60373476235),MAAC_FLT_C(158455.20524234080),MAAC_FLT_C(158481.80786643337),
    MAAC_FLT_C(158508.41160694641),MAAC_FLT_C(158535.01646378616),MAAC_FLT_C(158561.62243685898),MAAC_FLT_C(158588.22952607120),
    MAAC_FLT_C(158614.83773132920),MAAC_FLT_C(158641.44705253936),MAAC_FLT_C(158668.05748960807),MAAC_FLT_C(158694.66904244179),
    MAAC_FLT_C(158721.28171094693),MAAC_FLT_C(158747.89549502998),MAAC_FLT_C(158774.51039459740),MAAC_FLT_C(158801.12640955573),
    MAAC_FLT_C(158827.74353981143),MAAC_FLT_C(158854.36178527112),MAAC_FLT_C(158880.98114584130),MAAC_FLT_C(158907.60162142856),
    MAAC_FLT_C(158934.22321193956),MAAC_FLT_C(158960.84591728085),MAAC_FLT_C(158987.46973735909),MAAC_FLT_C(159014.09467208097),
    MAAC_FLT_C(159040.72072135314),MAAC_FLT_C(159067.34788508230),MAAC_FLT_C(159093.97616317519),MAAC_FLT_C(159120.60555553852),
    MAAC_FLT_C(159147.23606207906),MAAC_FLT_C(159173.86768270360),MAAC_FLT_C(159200.50041731889),MAAC_FLT_C(159227.13426583182),
    MAAC_FLT_C(159253.76922814918),MAAC_FLT_C(159280.40530417781),MAAC_FLT_C(159307.04249382461),MAAC_FLT_C(159333.68079699649),
    MAAC_FLT_C(159360.32021360032),MAAC_FLT_C(159386.96074354305),MAAC_FLT_C(159413.60238673165),MAAC_FLT_C(159440.24514307309),
    MAAC_FLT_C(159466.88901247433),MAAC_FLT_C(159493.53399484244),MAAC_FLT_C(159520.18009008438),MAAC_FLT_C(159546.82729810724),
    MAAC_FLT_C(159573.47561881805),MAAC_FLT_C(159600.12505212397),MAAC_FLT_C(159626.77559793202),MAAC_FLT_C(159653.42725614941),
    MAAC_FLT_C(159680.08002668325),MAAC_FLT_C(159706.73390944069),MAAC_FLT_C(159733.38890432892),MAAC_FLT_C(159760.04501125516),
    MAAC_FLT_C(159786.70223012666),MAAC_FLT_C(159813.36056085059),MAAC_FLT_C(159840.02000333427),MAAC_FLT_C(159866.68055748497),
    MAAC_FLT_C(159893.34222320997),MAAC_FLT_C(159920.00500041663),MAAC_FLT_C(159946.66888901225),MAAC_FLT_C(159973.33388890422),
    MAAC_FLT_C(159999.99999999988),MAAC_FLT_C(160026.66722220668),MAAC_FLT_C(160053.33555543202),MAAC_FLT_C(160080.00499958330),
    MAAC_FLT_C(160106.67555456801),MAAC_FLT_C(160133.34722029360),MAAC_FLT_C(160160.01999666760),MAAC_FLT_C(160186.69388359750),
    MAAC_FLT_C(160213.36888099083),MAAC_FLT_C(160240.04498875517),MAAC_FLT_C(160266.72220679806),MAAC_FLT_C(160293.40053502709),
    MAAC_FLT_C(160320.07997334987),MAAC_FLT_C(160346.76052167406),MAAC_FLT_C(160373.44217990729),MAAC_FLT_C(160400.12494795720),
    MAAC_FLT_C(160426.80882573151),MAAC_FLT_C(160453.49381313793),MAAC_FLT_C(160480.17991008417),MAAC_FLT_C(160506.86711647795),
    MAAC_FLT_C(160533.55543222709),MAAC_FLT_C(160560.24485723933),MAAC_FLT_C(160586.93539142248),MAAC_FLT_C(160613.62703468435),
    MAAC_FLT_C(160640.31978693281),MAAC_FLT_C(160667.01364807569),MAAC_FLT_C(160693.70861802087),MAAC_FLT_C(160720.40469667627),
    MAAC_FLT_C(160747.10188394980),MAAC_FLT_C(160773.80017974938),MAAC_FLT_C(160800.49958398298),MAAC_FLT_C(160827.20009655855),
    MAAC_FLT_C(160853.90171738411),MAAC_FLT_C(160880.60444636765),MAAC_FLT_C(160907.30828341722),MAAC_FLT_C(160934.01322844086),
    MAAC_FLT_C(160960.71928134665),MAAC_FLT_C(160987.42644204269),MAAC_FLT_C(161014.13471043704),MAAC_FLT_C(161040.84408643784),
    MAAC_FLT_C(161067.55456995327),MAAC_FLT_C(161094.26616089148),MAAC_FLT_C(161120.97885916062),MAAC_FLT_C(161147.69266466892),
    MAAC_FLT_C(161174.40757732463),MAAC_FLT_C(161201.12359703594),MAAC_FLT_C(161227.84072371112),MAAC_FLT_C(161254.55895725847),
    MAAC_FLT_C(161281.27829758628),MAAC_FLT_C(161307.99874460287),MAAC_FLT_C(161334.72029821656),MAAC_FLT_C(161361.44295833571),
    MAAC_FLT_C(161388.16672486870),MAAC_FLT_C(161414.89159772391),MAAC_FLT_C(161441.61757680977),MAAC_FLT_C(161468.34466203468),
    MAAC_FLT_C(161495.07285330712),MAAC_FLT_C(161521.80215053557),MAAC_FLT_C(161548.53255362847),MAAC_FLT_C(161575.26406249436),
    MAAC_FLT_C(161601.99667704175),MAAC_FLT_C(161628.73039717920),MAAC_FLT_C(161655.46522281526),MAAC_FLT_C(161682.20115385848),
    MAAC_FLT_C(161708.93819021754),MAAC_FLT_C(161735.67633180099),MAAC_FLT_C(161762.41557851751),MAAC_FLT_C(161789.15593027571),
    MAAC_FLT_C(161815.89738698432),MAAC_FLT_C(161842.63994855201),MAAC_FLT_C(161869.38361488748),MAAC_FLT_C(161896.12838589950),
    MAAC_FLT_C(161922.87426149679),MAAC_FLT_C(161949.62124158812),MAAC_FLT_C(161976.36932608229),MAAC_FLT_C(162003.11851488810),
    MAAC_FLT_C(162029.86880791440),MAAC_FLT_C(162056.62020507001),MAAC_FLT_C(162083.37270626382),MAAC_FLT_C(162110.12631140466),
    MAAC_FLT_C(162136.88102040152),MAAC_FLT_C(162163.63683316324),MAAC_FLT_C(162190.39374959879),MAAC_FLT_C(162217.15176961711),
    MAAC_FLT_C(162243.91089312723),MAAC_FLT_C(162270.67112003808),MAAC_FLT_C(162297.43245025873),MAAC_FLT_C(162324.19488369819),
    MAAC_FLT_C(162350.95842026550),MAAC_FLT_C(162377.72305986975),MAAC_FLT_C(162404.48880242003),MAAC_FLT_C(162431.25564782543),
    MAAC_FLT_C(162458.02359599507),MAAC_FLT_C(162484.79264683815),MAAC_FLT_C(162511.56280026378),MAAC_FLT_C(162538.33405618116),
    MAAC_FLT_C(162565.10641449949),MAAC_FLT_C(162591.87987512801),MAAC_FLT_C(162618.65443797593),MAAC_FLT_C(162645.43010295252),
    MAAC_FLT_C(162672.20686996708),MAAC_FLT_C(162698.98473892888),MAAC_FLT_C(162725.76370974723),MAAC_FLT_C(162752.54378233149),
    MAAC_FLT_C(162779.32495659095),MAAC_FLT_C(162806.10723243505),MAAC_FLT_C(162832.89060977317),MAAC_FLT_C(162859.67508851466),
    MAAC_FLT_C(162886.46066856902),MAAC_FLT_C(162913.24734984562),MAAC_FLT_C(162940.03513225401),MAAC_FLT_C(162966.82401570358),
    MAAC_FLT_C(162993.61400010390),MAAC_FLT_C(163020.40508536444),MAAC_FLT_C(163047.19727139481),MAAC_FLT_C(163073.99055810447),
    MAAC_FLT_C(163100.78494540305),MAAC_FLT_C(163127.58043320014),MAAC_FLT_C(163154.37702140535),MAAC_FLT_C(163181.17470992831),
    MAAC_FLT_C(163207.97349867865),MAAC_FLT_C(163234.77338756606),MAAC_FLT_C(163261.57437650024),MAAC_FLT_C(163288.37646539087),
    MAAC_FLT_C(163315.17965414765),MAAC_FLT_C(163341.98394268038),MAAC_FLT_C(163368.78933089875),MAAC_FLT_C(163395.59581871261),
    MAAC_FLT_C(163422.40340603172),MAAC_FLT_C(163449.21209276590),MAAC_FLT_C(163476.02187882498),MAAC_FLT_C(163502.83276411882),
    MAAC_FLT_C(163529.64474855730),MAAC_FLT_C(163556.45783205028),MAAC_FLT_C(163583.27201450770),MAAC_FLT_C(163610.08729583945),
    MAAC_FLT_C(163636.90367595552),MAAC_FLT_C(163663.72115476584),MAAC_FLT_C(163690.53973218042),MAAC_FLT_C(163717.35940810922),
    MAAC_FLT_C(163744.18018246230),MAAC_FLT_C(163771.00205514964),MAAC_FLT_C(163797.82502608138),MAAC_FLT_C(163824.64909516752),
    MAAC_FLT_C(163851.47426231820),MAAC_FLT_C(163878.30052744350),MAAC_FLT_C(163905.12789045356),MAAC_FLT_C(163931.95635125853),
    MAAC_FLT_C(163958.78590976857),MAAC_FLT_C(163985.61656589387),MAAC_FLT_C(164012.44831954464),MAAC_FLT_C(164039.28117063109),
    MAAC_FLT_C(164066.11511906344),MAAC_FLT_C(164092.95016475199),MAAC_FLT_C(164119.78630760699),MAAC_FLT_C(164146.62354753874),
    MAAC_FLT_C(164173.46188445756),MAAC_FLT_C(164200.30131827376),MAAC_FLT_C(164227.14184889771),MAAC_FLT_C(164253.98347623978),
    MAAC_FLT_C(164280.82620021031),MAAC_FLT_C(164307.67002071979),MAAC_FLT_C(164334.51493767856),MAAC_FLT_C(164361.36095099710),
    MAAC_FLT_C(164388.20806058586),MAAC_FLT_C(164415.05626635533),MAAC_FLT_C(164441.90556821600),MAAC_FLT_C(164468.75596607837),
    MAAC_FLT_C(164495.60745985300),MAAC_FLT_C(164522.46004945040),MAAC_FLT_C(164549.31373478117),MAAC_FLT_C(164576.16851575591),
    MAAC_FLT_C(164603.02439228518),MAAC_FLT_C(164629.88136427966),MAAC_FLT_C(164656.73943164991),MAAC_FLT_C(164683.59859430668),
    MAAC_FLT_C(164710.45885216061),MAAC_FLT_C(164737.32020512238),MAAC_FLT_C(164764.18265310270),MAAC_FLT_C(164791.04619601235),
    MAAC_FLT_C(164817.91083376206),MAAC_FLT_C(164844.77656626256),MAAC_FLT_C(164871.64339342469),MAAC_FLT_C(164898.51131515924),
    MAAC_FLT_C(164925.38033137703),MAAC_FLT_C(164952.25044198890),MAAC_FLT_C(164979.12164690570),MAAC_FLT_C(165005.99394603830),
    MAAC_FLT_C(165032.86733929763),MAAC_FLT_C(165059.74182659460),MAAC_FLT_C(165086.61740784015),MAAC_FLT_C(165113.49408294520)
};

#endif

static void bpp(void) {
    return;
}

MAAC_PUBLIC
void
maac_ics_init(maac_ics* ics) {
    /* TODO - remove memset with garbage data once we verify this works correctly with mostly garbage data */
    maac_memset(ics, 0xCD, sizeof *ics);
    ics->state = MAAC_ICS_STATE_GLOBAL_GAIN;
}

static maac_inline void
maac_ics_reset_iterators(maac_ics* ics) {
    ics->_g = 0;
    ics->_w = 0;
    ics->_i = 0;
    ics->_k = 0;
    ics->_p = 0;
    ics->_off = 0;
    ics->_group_off = 0;
}

static maac_inline void
maac_ics_reset_scalefactors(maac_ics* ics) {
    ics->_noise_flag = 1;
    ics->_scale_factor = ics->global_gain;
    ics->_dpcm_is_position = 0;
    ics->_noise_energy = ((maac_s32)ics->global_gain) - 90;
}

struct maac_ics_codebook_params {
    maac_u8 cb;
    maac_u8 group_len;
    maac_u16 sfb_start;
    maac_u16 sfb_end;
    maac_flt* samples;
};

typedef struct maac_ics_codebook_params maac_ics_codebook_params;

static void
maac_ics_zero_codebook(maac_ics* ics, const maac_ics_codebook_params* p) {
    maac_u8 group;
    maac_u16 sfb;
    maac_u16 off;

    for(group=0; group < p->group_len; group++) {

        off = ics->_group_off + (group * 128);

        for(sfb=p->sfb_start; sfb < p->sfb_end; sfb++) {
            p->samples[off + sfb] = 0;
        }
    }
}

static void brekp(void) {
    return;
}

static MAAC_RESULT
maac_ics_default_codebook(maac_ics* ics, maac_bitreader* maac_restrict br, const maac_ics_codebook_params* p) {
#if 0
    const maac_ics_info* info = &ics->info;
    const maac_u8 num_window_groups = maac_sfg_num_window_groups(info->scale_factor_grouping);
    const maac_scalefactor_bands b = maac_scalefactor_bandsf(info->window_sequence, p->sf_index);
#endif

    MAAC_RESULT res;
    maac_u8 n;
    maac_u8 num;
    maac_u8 pulse_offset;
    maac_s16 pulse_amp;
    maac_u16 off;
    maac_s16 sf;
    maac_u16 k;

    maac_flt scale;
    maac_flt quant;

    num = p->cb >= MAAC_FIRST_PAIR_HCB ? 2 : 4;

    sf = ics->scalefactors[ maac_section_idx(ics->_g, ics->_k) ];
    /* this one I'm more certain the range is 0 - 255 */
    maac_assert(sf >= 0);
    maac_assert(sf <= 255);
    sf = maac_clamp(sf, 0, 255);

    scale = maac_pow2_xdiv4(sf - 100);

    while(ics->_w < p->group_len) {
        off = ics->_group_off + (ics->_w * 128);

        while(p->sfb_start + ics->_p < p->sfb_end) {
            if( (res = maac_huffman_decode_spectral(&ics->_huffman, br, p->cb, ics->spectra_tmp)) != MAAC_OK) return res;

            if(p->samples != NULL) {

                if(ics->pulse_data_present) {
                    /* TODO add pulse data support */
                    if(ics->_k == ics->pulse.start_sfb) {
                        /* k = b.offsets[ics->pulse.start_sfb]; */
                        k = p->sfb_start;

                        for(n=0;n<ics->pulse.num_pulse;n++) {
                            pulse_offset = ics->pulse.pulses[n] & 0x1f;
                            pulse_amp = (ics->pulse.pulses[n] >> 5) & 0x0f;

                            k += pulse_offset;
                            if(k >= (p->sfb_start + ics->_p) && k < (p->sfb_start + ics->_p + num)) {
                    brekp();
                              if(ics->spectra_tmp[k - (p->sfb_start + ics->_p)] > 0) {
                                  ics->spectra_tmp[k - (p->sfb_start + ics->_p)] += pulse_amp;
                              } else {
                                  ics->spectra_tmp[k - (p->sfb_start + ics->_p)] -= pulse_amp;
                              }
                            }
                        }
                    }
                }

                for(n=0;n<num;n++) {
#ifdef MAAC_INVQUANT_TABLES
                    quant = (ics->spectra_tmp[n] < 0 ?
                       -MAAC_INV_QUANT[-ics->spectra_tmp[n]]
                       :
                       MAAC_INV_QUANT[ics->spectra_tmp[n]]);
#else
                    quant =
                        maac_flt_cast(ics->spectra_tmp[n]) *
                        maac_cbrt(maac_abs(ics->spectra_tmp[n]));
#endif
                    quant *= scale;
                    p->samples[off + p->sfb_start + ics->_p + n] = quant;
                }
            }
            ics->_p += num;
        }
        ics->_p = 0;
        ics->_w++;
    }
    ics->_w = 0;
    return MAAC_OK;
}


static MAAC_RESULT
maac_spectra_decode(maac_ics* ics, maac_bitreader* maac_restrict br, const maac_ics_decode_params* p) {
    const maac_ics_info* info = &ics->info;
    const maac_u8 num_window_groups = maac_sfg_num_window_groups(info->scale_factor_grouping);
    const maac_scalefactor_bands b = maac_scalefactor_bandsf(info->window_sequence, p->sf_index);

    maac_u32 group_lengths = maac_window_group_lengths(info->window_sequence, info->scale_factor_grouping) >> (ics->_g * 4);

    MAAC_RESULT res;

    maac_u32 idx;

    maac_ics_codebook_params cb_p;
    cb_p.samples = p->ch == NULL ? NULL : p->ch->samples;

    /* g tracks window groups, easy enough */
    /* i tracks the section within a group */
    /* k tracks the scale factor band *index* over the section (0-51 / 0-15) */
    /* p tracks the scale factor *band* over the section,
       relative to the start value, so it goes from
       0 -> (sfb_end - sfb_start) */

    while(ics->_g < num_window_groups) {
        cb_p.group_len = group_lengths & 0x0f;

        while(ics->_k < b.len) {

            idx = maac_section_idx(ics->_g, ics->_i);
            cb_p.cb = ics->section_data[idx].codebook;
            cb_p.sfb_start = b.offsets[ics->_k];
            cb_p.sfb_end   = b.offsets[ics->_k + 1];
            switch(cb_p.cb) {
                case MAAC_ZERO_HCB: /* fall-through */
                case MAAC_INTENSITY_HCB: /* fall-through */
                case MAAC_INTENSITY_HCB2: {
                    if(p->ch != NULL) maac_ics_zero_codebook(ics, &cb_p);
                    break;
                }
                case MAAC_NOISE_HCB: {
                    /* this is handled in a higher layer (sce/cpe) */
                    break;
                }
                default: {
                    if( (res = maac_ics_default_codebook(ics, br, &cb_p)) != MAAC_OK) return res;
                    break;
                }
            }
            ics->_k++;
            if(ics->_k == ics->section_data[idx].end) {
                ics->_i++;
            }
        }

        ics->_group_off += ((maac_u16)cb_p.group_len) * 128;
        group_lengths >>= 4;
        ics->_g++;
        ics->_i = 0;
        ics->_k = 0;
    }

    return MAAC_OK;
}

static MAAC_RESULT
maac_scale_factor_parse(maac_ics* ics,  maac_bitreader* maac_restrict br, const maac_ics_decode_params* p) {
    const maac_ics_info* info = &ics->info;
    const maac_u8 num_window_groups = maac_sfg_num_window_groups(info->scale_factor_grouping);
    const maac_scalefactor_bands b = maac_scalefactor_bandsf(info->window_sequence, p->sf_index);

    MAAC_RESULT res;
    maac_u8 cb;
    maac_u32 idx;
    maac_s16 s16tmp;
    maac_u16 off;

    while(ics->_g < num_window_groups) {
        off = maac_section_idx(ics->_g, 0);

        while(ics->_k < b.len) {
            idx = maac_section_idx(ics->_g, ics->_i);
            cb = ics->section_data[idx].codebook;

            while(ics->_k < ics->section_data[idx].end) {
                switch(cb) {
                    case MAAC_ZERO_HCB: {
                        ics->scalefactors[off + ics->_k] = 0;
                        break;
                    }
                    case MAAC_INTENSITY_HCB: /* fall-through */
                    case MAAC_INTENSITY_HCB2: {
                        if( (res = maac_huffman_decode(&ics->_huffman, br, 0)) != MAAC_OK) return res;

                        ics->_dpcm_is_position += ((maac_s16)ics->_huffman.index) - 60;
                        ics->scalefactors[off + ics->_k] = ics->_dpcm_is_position;
                        break;
                    }
                    case MAAC_NOISE_HCB: {
                        if(ics->_noise_flag) {
                            if( (res = maac_bitreader_fill(br, 9)) != MAAC_OK) return res;
                            s16tmp = (maac_s16)maac_bitreader_read(br, 9);
                            s16tmp -= ((maac_s16)(1 << 8));
                            ics->_noise_flag = 0;
                        } else {
                            if( (res = maac_huffman_decode(&ics->_huffman, br, 0)) != MAAC_OK) return res;
                            s16tmp = ((maac_s16)ics->_huffman.index) - 60;
                        }
                        ics->_noise_energy += s16tmp;
                        ics->scalefactors[off + ics->_k] = ics->_noise_energy;
                        break;
                    }
                    default: {
                        if( (res = maac_huffman_decode(&ics->_huffman, br, 0)) != MAAC_OK) return res;
                        ics->_scale_factor += ((maac_u8)ics->_huffman.index) - 60;
                        ics->scalefactors[off + ics->_k] = ics->_scale_factor;
                        break;
                    }
                }
                ics->_k++;
            }

            ics->_i++;
        }

        ics->_g++;
        ics->_k = 0;
        ics->_i = 0;
    }
    return MAAC_OK;
}


static MAAC_RESULT
maac_section_data_parse(maac_ics* ics, maac_bitreader* maac_restrict br, const maac_ics_decode_params* p) {
    const maac_ics_info* info = &ics->info;
    const maac_u8 esc_val =
      info->window_sequence == MAAC_WINDOW_SEQUENCE_EIGHT_SHORT ? 0x07 : 0x1f;
    const maac_u8 sect_bits =
      esc_val == 0x07 ? 3 : 5;
    const maac_u8 num_window_groups = maac_sfg_num_window_groups(info->scale_factor_grouping);
    const maac_scalefactor_bands b = maac_scalefactor_bandsf(info->window_sequence, p->sf_index);

    MAAC_RESULT res;
    maac_u8 sect_len_incr;
    maac_u32 idx;

    while(ics->_g < num_window_groups) {
        switch(ics->state) {
            case MAAC_ICS_STATE_SECTION_CODEBOOK: {
                maac_ics_state_section_codebook:
                /* this can happen when the entire section is just one big codebook 0, we have
                 * no codebooks or lengths to read */
                if(ics->_k == info->max_sfb) goto maac_ics_state_section_codebook_next;

                idx = maac_section_idx(ics->_g, ics->_i);

                if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
                ics->section_data[idx].codebook = maac_bitreader_read(br, 4);
                ics->section_data[idx].end = 0;
                ics->state = MAAC_ICS_STATE_SECTION_CODEBOOK_LENGTH;
                goto maac_ics_state_section_codebook_length;
            }

            case MAAC_ICS_STATE_SECTION_CODEBOOK_LENGTH: {
                maac_ics_state_section_codebook_length:
                idx = maac_section_idx(ics->_g, ics->_i);

                if( (res = maac_bitreader_fill(br, sect_bits)) != MAAC_OK) return res;
                sect_len_incr = maac_bitreader_read(br, sect_bits);
                ics->section_data[idx].end += sect_len_incr;
                if(sect_len_incr == esc_val) goto maac_ics_state_section_codebook_length;

                ics->state = MAAC_ICS_STATE_SECTION_CODEBOOK;

                /* previous our "end" value was a length, replace with _k to become an absolute position */
                ics->_k += ics->section_data[idx].end;

                ics->section_data[idx].end = ics->_k;
                ics->_i++;

                if(ics->_k < info->max_sfb) goto maac_ics_state_section_codebook;

                maac_ics_state_section_codebook_next:
                if(b.len > info->max_sfb) {
                    ics->section_data[maac_section_idx(ics->_g,ics->_i)].codebook = 0;
                    ics->section_data[maac_section_idx(ics->_g,ics->_i)].end =
                      b.len;
                }
                break;
            }
            default: {
                MAAC_UNREACHABLE_RETURN(MAAC_UNREACHABLE);
            }
        }

        ics->_k = 0;
        ics->_i = 0;
        ics->_g++;
    }

    return MAAC_OK;
}

MAAC_PUBLIC
MAAC_RESULT
maac_ics_decode(maac_ics* ics, maac_bitreader* maac_restrict br, const maac_ics_decode_params* p) {
    const maac_ics_info* info = &ics->info;
    MAAC_RESULT res;

    switch(ics->state) {
        case MAAC_ICS_STATE_GLOBAL_GAIN: {
            if( (res = maac_bitreader_fill(br,8)) != MAAC_OK) return res;
            ics->global_gain = maac_bitreader_read(br,8);
            if(p->common_window) {
                ics->state = MAAC_ICS_STATE_SECTION_CODEBOOK;
                maac_ics_reset_iterators(ics);
                goto maac_ics_state_section_codebook;
            }
            ics->state = MAAC_ICS_STATE_ICS_INFO;
            maac_ics_info_init(&ics->info);
            goto maac_ics_state_ics_info;
        }

        case MAAC_ICS_STATE_ICS_INFO: {
            maac_ics_state_ics_info:
            if( (res =  maac_ics_info_parse(&ics->info, br, p->sf_index)) != MAAC_OK) return res;
            ics->state = MAAC_ICS_STATE_SECTION_CODEBOOK;
            maac_ics_reset_iterators(ics);
            goto maac_ics_state_section_codebook;
        }

        case MAAC_ICS_STATE_SECTION_CODEBOOK: /* fall-through */
        case MAAC_ICS_STATE_SECTION_CODEBOOK_LENGTH: {
            maac_ics_state_section_codebook:
            if( (res = maac_section_data_parse(ics, br, p)) != MAAC_OK) return res;

            ics->state = MAAC_ICS_STATE_SCALE_FACTOR_DATA;
            maac_ics_reset_iterators(ics);
            maac_ics_reset_scalefactors(ics);
            maac_huffman_init(&ics->_huffman);

            goto maac_ics_state_scale_factor_data;
        }

        case MAAC_ICS_STATE_SCALE_FACTOR_DATA: {
            maac_ics_state_scale_factor_data:
            if( (res = maac_scale_factor_parse(ics, br, p)) != MAAC_OK) return res;
            ics->state = MAAC_ICS_STATE_PULSE_DATA_PRESENT;
            goto maac_ics_state_pulse_data_present;
        }

        case MAAC_ICS_STATE_PULSE_DATA_PRESENT: {
            maac_ics_state_pulse_data_present:
            if( (res = maac_bitreader_fill(br,1)) != MAAC_OK) return res;
            ics->pulse_data_present = (maac_u8)maac_bitreader_read(br, 1);
            if(ics->pulse_data_present) {
            bpp();
                ics->state = MAAC_ICS_STATE_PULSE_DATA;
                maac_pulse_init(&ics->pulse);
                goto maac_ics_state_pulse_data;
            }
            ics->state = MAAC_ICS_STATE_TNS_DATA_PRESENT;
            goto maac_ics_state_tns_data_present;
        }

        case MAAC_ICS_STATE_PULSE_DATA: {
            maac_ics_state_pulse_data:
            if( (res = maac_pulse_parse(&ics->pulse, br)) != MAAC_OK) return res;

            ics->state = MAAC_ICS_STATE_TNS_DATA_PRESENT;
            goto maac_ics_state_tns_data_present;
        }

        case MAAC_ICS_STATE_TNS_DATA_PRESENT: {
            maac_ics_state_tns_data_present:
            if( (res = maac_bitreader_fill(br,1)) != MAAC_OK) return res;
            ics->tns_data_present = (maac_u8)maac_bitreader_read(br, 1);
            if(ics->tns_data_present) {
                ics->tns._g = 0;
                ics->tns.state = MAAC_TNS_STATE_N_FILT;
                ics->state = MAAC_ICS_STATE_TNS_DATA;
                goto maac_ics_state_tns_data;
            }
            ics->state = MAAC_ICS_STATE_GAIN_CONTROL_DATA_PRESENT;
            goto maac_ics_state_gain_control_data_present;
        }

        case MAAC_ICS_STATE_TNS_DATA: {
            maac_ics_state_tns_data:
            if( (res = maac_tns_parse(&ics->tns, br, info->window_sequence)) != MAAC_OK) return res;

            ics->state = MAAC_ICS_STATE_GAIN_CONTROL_DATA_PRESENT;
            goto maac_ics_state_gain_control_data_present;
        }

        case MAAC_ICS_STATE_GAIN_CONTROL_DATA_PRESENT: {
            maac_ics_state_gain_control_data_present:
            if( (res = maac_bitreader_fill(br,1)) != MAAC_OK) return res;
            ics->gain_control_data_present = (maac_u8)maac_bitreader_read(br, 1);
            maac_assert(ics->gain_control_data_present == 0);
            if(ics->gain_control_data_present) {
                ics->state = MAAC_ICS_STATE_GAIN_CONTROL_DATA;
                return MAAC_GAIN_CONTROL_DATA_NOT_IMPLEMENTED;
            }

            ics->state = MAAC_ICS_STATE_SPECTRAL_DATA;
            maac_ics_reset_iterators(ics);
            maac_huffman_init(&ics->_huffman);
            goto maac_ics_state_spectral_data;
        }

        case MAAC_ICS_STATE_GAIN_CONTROL_DATA: {
            ics->state = MAAC_ICS_STATE_SPECTRAL_DATA;
            return MAAC_GAIN_CONTROL_DATA_NOT_IMPLEMENTED;
        }

        case MAAC_ICS_STATE_SPECTRAL_DATA: {
            maac_ics_state_spectral_data:
            if( (res = maac_spectra_decode(ics, br, p)) != MAAC_OK) return res;
            ics->state = MAAC_ICS_STATE_GLOBAL_GAIN;
            return MAAC_OK;
        }
    }

    MAAC_UNREACHABLE_RETURN(MAAC_UNREACHABLE);
}

#define maac_min(a,b) ( (a) < (b) ? (a) : (b) )

MAAC_PUBLIC
void
maac_ics_info_init(maac_ics_info* ics_info) {
    /* TODO - remove memset with garbage data once we verify this works correctly with mostly garbage data */
    maac_memset(ics_info, 0xa1, sizeof *ics_info);
    ics_info->state = MAAC_ICS_INFO_STATE_RESERVED_BIT;
}

#if MAAC_ENABLE_MAINPROFILE
/* Not sure if I'll ever go full main-profile tbh */

static const maac_u8 maac_pred_sfb_max_tbl[16] = {
    /* 0x00 (96000Hz)  */ 33,
    /* 0x01 (88200Hz)  */ 33,
    /* 0x02 (64000Hz)  */ 38,
    /* 0x03 (48000Hz)  */ 40,
    /* 0x04 (44100Hz)  */ 40,
    /* 0x05 (32000Hz)  */ 40,
    /* 0x06 (24000Hz)  */ 41,
    /* 0x07 (22050Hz)  */ 41,
    /* 0x08 (16000Hz)  */ 37,
    /* 0x09 (12000Hz)  */ 37,
    /* 0x0a (11025Hz)  */ 37,
    /* 0x0b  (8000Hz)  */ 34,
    /* 0x00 (reserved) */ 0,
    /* 0x00 (reserved) */ 0,
    /* 0x00 (reserved) */ 0,
    /* 0x00 (reserved) */ 0
};


maac_const
static
maac_u32
maac_pred_sfb_max(const maac_u32 sf_index) {
    return sf_index > 15 ? 0 : pred_sfb_max_tbl[sf_index];
}
#endif

MAAC_PUBLIC
MAAC_RESULT
maac_ics_info_parse(maac_ics_info* maac_restrict info, maac_bitreader* maac_restrict br, const maac_u32 sf_index) {
    MAAC_RESULT res;
    maac_u8 bits;
#if MAAC_ENABLE_MAINPROFILE
    maac_u8 pred_max;
#else
    (void)sf_index;
#endif

    switch(info->state) {
        case MAAC_ICS_INFO_STATE_RESERVED_BIT: {
            if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
            maac_bitreader_discard(br,1);
            info->state = MAAC_ICS_INFO_STATE_WINDOW_SEQUENCE;
        }
        /* fall-through */
        case MAAC_ICS_INFO_STATE_WINDOW_SEQUENCE: {
            if( (res = maac_bitreader_fill(br, 2)) != MAAC_OK) return res;
            info->window_sequence = (maac_u8)maac_bitreader_read(br,2);
            info->state = MAAC_ICS_INFO_STATE_WINDOW_SHAPE;
        }
        /* fall-through */
        case MAAC_ICS_INFO_STATE_WINDOW_SHAPE: {
            if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
            info->window_shape = (maac_u8)maac_bitreader_read(br,1);
            info->state = MAAC_ICS_INFO_STATE_MAX_SFB;
        }
        /* fall-through */
        case MAAC_ICS_INFO_STATE_MAX_SFB: {
            bits = info->window_sequence == MAAC_WINDOW_SEQUENCE_EIGHT_SHORT ? 4 : 6;
            if( (res = maac_bitreader_fill(br, bits)) != MAAC_OK) return res;
            info->max_sfb = (maac_u8)maac_bitreader_read(br,bits);

            if(info->window_sequence == MAAC_WINDOW_SEQUENCE_EIGHT_SHORT) {
                info->state = MAAC_ICS_INFO_STATE_SCALE_FACTOR_GROUPING;
                goto maac_ics_info_state_scale_factor_grouping;
            }
            info->scale_factor_grouping = 0x7f;
            info->state = MAAC_ICS_INFO_STATE_PREDICTOR_DATA_PRESENT;
            goto maac_ics_info_state_predictor_data_present;
        }

        case MAAC_ICS_INFO_STATE_SCALE_FACTOR_GROUPING: {
            maac_ics_info_state_scale_factor_grouping:
            if( (res = maac_bitreader_fill(br, 7)) != MAAC_OK) return res;
            info->scale_factor_grouping = maac_bitreader_read(br,7);
            break;
        }

        case MAAC_ICS_INFO_STATE_PREDICTOR_DATA_PRESENT: {
            maac_ics_info_state_predictor_data_present:
            if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
            info->predictor_data_present = maac_bitreader_read(br,1);
            if(info->predictor_data_present) {
#if MAAC_ENABLE_MAINPROFILE
                info->_sfb = 0;
                info->prediction_used[0] = 0;
                info->prediction_used[1] = 0;
                info->state = MAAC_ICS_INFO_STATE_PREDICTOR_RESET;
                goto maac_ics_info_state_predictor_reset;
#else
                return MAAC_PREDICTOR_DATA_NOT_IMPLEMENTED;
#endif
            }
            break;
        }

#if MAAC_ENABLE_MAINPROFILE
        case MAAC_ICS_INFO_STATE_PREDICTOR_RESET: {
            maac_ics_info_state_predictor_reset:
            if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
            info->predictor_reset = maac_bitreader_read(br,1);
            if(info->predictor_reset) {
                info->state = MAAC_ICS_INFO_STATE_PREDICTOR_RESET_GROUP_NUMBER;
                goto maac_ics_info_state_predictor_reset_group_number;
            }
            info->state = MAAC_ICS_INFO_STATE_PREDICTION_USED;
            goto maac_ics_info_state_prediction_used;
        }

        case MAAC_ICS_INFO_STATE_PREDICTOR_RESET_GROUP_NUMBER: {
            maac_ics_info_state_predictor_reset_group_number:
            if( (res = maac_bitreader_fill(br, 5)) != MAAC_OK) return res;
            info->predictor_reset_group_number = maac_bitreader_read(br,5);
            info->state = MAAC_ICS_INFO_STATE_PREDICTION_USED;
            goto maac_ics_info_state_prediction_used;
        }

        case MAAC_ICS_INFO_STATE_PREDICTION_USED: {
            maac_ics_info_state_prediction_used:
            pred_max = maac_min(info->max_sfb,maac_pred_sfb_max(sf_index));
            while(info->_sfb < pred_max) {
                if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
                info->prediction_used[info->_sfb / 32] |= ( (maac_bitreader_read(br, 1)) << (info->_sfb % 32) );
                info->_sfb++;
            }
            break;
        }
#endif
    }

    info->state = MAAC_ICS_INFO_STATE_RESERVED_BIT;
    return MAAC_OK;
}

static const maac_flt MAAC_IMDCT_A_2048[1024] = {
    MAAC_FLT_C(1.0000000000000000), MAAC_FLT_C(-0.0000000000000000),
    MAAC_FLT_C(0.99998116493225098), MAAC_FLT_C(-0.0061358846724033356),
    MAAC_FLT_C(0.99992471933364868), MAAC_FLT_C(-0.012271538376808167),
    MAAC_FLT_C(0.99983060359954834), MAAC_FLT_C(-0.018406730145215988),
    MAAC_FLT_C(0.99969881772994995), MAAC_FLT_C(-0.024541229009628296),
    MAAC_FLT_C(0.99952942132949829), MAAC_FLT_C(-0.030674804002046585),
    MAAC_FLT_C(0.99932235479354858), MAAC_FLT_C(-0.036807224154472351),
    MAAC_FLT_C(0.99907773733139038), MAAC_FLT_C(-0.042938258498907089),
    MAAC_FLT_C(0.99879544973373413), MAAC_FLT_C(-0.049067676067352295),
    MAAC_FLT_C(0.99847555160522461), MAAC_FLT_C(-0.055195245891809464),
    MAAC_FLT_C(0.99811810255050659), MAAC_FLT_C(-0.061320737004280090),
    MAAC_FLT_C(0.99772304296493530), MAAC_FLT_C(-0.067443922162055969),
    MAAC_FLT_C(0.99729043245315552), MAAC_FLT_C(-0.073564566671848297),
    MAAC_FLT_C(0.99682027101516724), MAAC_FLT_C(-0.079682439565658569),
    MAAC_FLT_C(0.99631261825561523), MAAC_FLT_C(-0.085797309875488281),
    MAAC_FLT_C(0.99576741456985474), MAAC_FLT_C(-0.091908954083919525),
    MAAC_FLT_C(0.99518471956253052), MAAC_FLT_C(-0.098017141222953796),
    MAAC_FLT_C(0.99456459283828735), MAAC_FLT_C(-0.10412163287401199),
    MAAC_FLT_C(0.99390697479248047), MAAC_FLT_C(-0.11022220551967621),
    MAAC_FLT_C(0.99321192502975464), MAAC_FLT_C(-0.11631862819194794),
    MAAC_FLT_C(0.99247956275939941), MAAC_FLT_C(-0.12241067737340927),
    MAAC_FLT_C(0.99170976877212524), MAAC_FLT_C(-0.12849810719490051),
    MAAC_FLT_C(0.99090266227722168), MAAC_FLT_C(-0.13458070158958435),
    MAAC_FLT_C(0.99005818367004395), MAAC_FLT_C(-0.14065824449062347),
    MAAC_FLT_C(0.98917651176452637), MAAC_FLT_C(-0.14673046767711639),
    MAAC_FLT_C(0.98825758695602417), MAAC_FLT_C(-0.15279719233512878),
    MAAC_FLT_C(0.98730140924453735), MAAC_FLT_C(-0.15885815024375916),
    MAAC_FLT_C(0.98630809783935547), MAAC_FLT_C(-0.16491311788558960),
    MAAC_FLT_C(0.98527765274047852), MAAC_FLT_C(-0.17096188664436340),
    MAAC_FLT_C(0.98421007394790649), MAAC_FLT_C(-0.17700421810150146),
    MAAC_FLT_C(0.98310548067092896), MAAC_FLT_C(-0.18303988873958588),
    MAAC_FLT_C(0.98196387290954590), MAAC_FLT_C(-0.18906866014003754),
    MAAC_FLT_C(0.98078525066375732), MAAC_FLT_C(-0.19509032368659973),
    MAAC_FLT_C(0.97956979274749756), MAAC_FLT_C(-0.20110464096069336),
    MAAC_FLT_C(0.97831737995147705), MAAC_FLT_C(-0.20711137354373932),
    MAAC_FLT_C(0.97702813148498535), MAAC_FLT_C(-0.21311031281948090),
    MAAC_FLT_C(0.97570210695266724), MAAC_FLT_C(-0.21910123527050018),
    MAAC_FLT_C(0.97433936595916748), MAAC_FLT_C(-0.22508391737937927),
    MAAC_FLT_C(0.97293996810913086), MAAC_FLT_C(-0.23105810582637787),
    MAAC_FLT_C(0.97150391340255737), MAAC_FLT_C(-0.23702360689640045),
    MAAC_FLT_C(0.97003126144409180), MAAC_FLT_C(-0.24298018217086792),
    MAAC_FLT_C(0.96852207183837891), MAAC_FLT_C(-0.24892760813236237),
    MAAC_FLT_C(0.96697646379470825), MAAC_FLT_C(-0.25486564636230469),
    MAAC_FLT_C(0.96539443731307983), MAAC_FLT_C(-0.26079410314559937),
    MAAC_FLT_C(0.96377605199813843), MAAC_FLT_C(-0.26671275496482849),
    MAAC_FLT_C(0.96212142705917358), MAAC_FLT_C(-0.27262136340141296),
    MAAC_FLT_C(0.96043050289154053), MAAC_FLT_C(-0.27851969003677368),
    MAAC_FLT_C(0.95870345830917358), MAAC_FLT_C(-0.28440752625465393),
    MAAC_FLT_C(0.95694035291671753), MAAC_FLT_C(-0.29028466343879700),
    MAAC_FLT_C(0.95514118671417236), MAAC_FLT_C(-0.29615089297294617),
    MAAC_FLT_C(0.95330601930618286), MAAC_FLT_C(-0.30200594663619995),
    MAAC_FLT_C(0.95143502950668335), MAAC_FLT_C(-0.30784964561462402),
    MAAC_FLT_C(0.94952815771102905), MAAC_FLT_C(-0.31368175148963928),
    MAAC_FLT_C(0.94758558273315430), MAAC_FLT_C(-0.31950202584266663),
    MAAC_FLT_C(0.94560730457305908), MAAC_FLT_C(-0.32531028985977173),
    MAAC_FLT_C(0.94359344244003296), MAAC_FLT_C(-0.33110630512237549),
    MAAC_FLT_C(0.94154405593872070), MAAC_FLT_C(-0.33688986301422119),
    MAAC_FLT_C(0.93945920467376709), MAAC_FLT_C(-0.34266072511672974),
    MAAC_FLT_C(0.93733900785446167), MAAC_FLT_C(-0.34841868281364441),
    MAAC_FLT_C(0.93518352508544922), MAAC_FLT_C(-0.35416352748870850),
    MAAC_FLT_C(0.93299281597137451), MAAC_FLT_C(-0.35989505052566528),
    MAAC_FLT_C(0.93076694011688232), MAAC_FLT_C(-0.36561298370361328),
    MAAC_FLT_C(0.92850607633590698), MAAC_FLT_C(-0.37131720781326294),
    MAAC_FLT_C(0.92621022462844849), MAAC_FLT_C(-0.37700742483139038),
    MAAC_FLT_C(0.92387950420379639), MAAC_FLT_C(-0.38268342614173889),
    MAAC_FLT_C(0.92151403427124023), MAAC_FLT_C(-0.38834503293037415),
    MAAC_FLT_C(0.91911387443542480), MAAC_FLT_C(-0.39399203658103943),
    MAAC_FLT_C(0.91667908430099487), MAAC_FLT_C(-0.39962419867515564),
    MAAC_FLT_C(0.91420978307723999), MAAC_FLT_C(-0.40524131059646606),
    MAAC_FLT_C(0.91170603036880493), MAAC_FLT_C(-0.41084316372871399),
    MAAC_FLT_C(0.90916800498962402), MAAC_FLT_C(-0.41642954945564270),
    MAAC_FLT_C(0.90659570693969727), MAAC_FLT_C(-0.42200025916099548),
    MAAC_FLT_C(0.90398931503295898), MAAC_FLT_C(-0.42755508422851562),
    MAAC_FLT_C(0.90134882926940918), MAAC_FLT_C(-0.43309381604194641),
    MAAC_FLT_C(0.89867448806762695), MAAC_FLT_C(-0.43861624598503113),
    MAAC_FLT_C(0.89596623182296753), MAAC_FLT_C(-0.44412213563919067),
    MAAC_FLT_C(0.89322429895401001), MAAC_FLT_C(-0.44961133599281311),
    MAAC_FLT_C(0.89044874906539917), MAAC_FLT_C(-0.45508357882499695),
    MAAC_FLT_C(0.88763964176177979), MAAC_FLT_C(-0.46053871512413025),
    MAAC_FLT_C(0.88479709625244141), MAAC_FLT_C(-0.46597650647163391),
    MAAC_FLT_C(0.88192129135131836), MAAC_FLT_C(-0.47139674425125122),
    MAAC_FLT_C(0.87901222705841064), MAAC_FLT_C(-0.47679921984672546),
    MAAC_FLT_C(0.87607008218765259), MAAC_FLT_C(-0.48218378424644470),
    MAAC_FLT_C(0.87309497594833374), MAAC_FLT_C(-0.48755016922950745),
    MAAC_FLT_C(0.87008696794509888), MAAC_FLT_C(-0.49289819598197937),
    MAAC_FLT_C(0.86704623699188232), MAAC_FLT_C(-0.49822765588760376),
    MAAC_FLT_C(0.86397284269332886), MAAC_FLT_C(-0.50353837013244629),
    MAAC_FLT_C(0.86086696386337280), MAAC_FLT_C(-0.50883013010025024),
    MAAC_FLT_C(0.85772860050201416), MAAC_FLT_C(-0.51410275697708130),
    MAAC_FLT_C(0.85455799102783203), MAAC_FLT_C(-0.51935601234436035),
    MAAC_FLT_C(0.85135519504547119), MAAC_FLT_C(-0.52458965778350830),
    MAAC_FLT_C(0.84812033176422119), MAAC_FLT_C(-0.52980363368988037),
    MAAC_FLT_C(0.84485357999801636), MAAC_FLT_C(-0.53499764204025269),
    MAAC_FLT_C(0.84155499935150146), MAAC_FLT_C(-0.54017144441604614),
    MAAC_FLT_C(0.83822470903396606), MAAC_FLT_C(-0.54532498121261597),
    MAAC_FLT_C(0.83486288785934448), MAAC_FLT_C(-0.55045795440673828),
    MAAC_FLT_C(0.83146959543228149), MAAC_FLT_C(-0.55557024478912354),
    MAAC_FLT_C(0.82804507017135620), MAAC_FLT_C(-0.56066155433654785),
    MAAC_FLT_C(0.82458931207656860), MAAC_FLT_C(-0.56573182344436646),
    MAAC_FLT_C(0.82110249996185303), MAAC_FLT_C(-0.57078075408935547),
    MAAC_FLT_C(0.81758481264114380), MAAC_FLT_C(-0.57580816745758057),
    MAAC_FLT_C(0.81403630971908569), MAAC_FLT_C(-0.58081394433975220),
    MAAC_FLT_C(0.81045717000961304), MAAC_FLT_C(-0.58579784631729126),
    MAAC_FLT_C(0.80684757232666016), MAAC_FLT_C(-0.59075969457626343),
    MAAC_FLT_C(0.80320751667022705), MAAC_FLT_C(-0.59569931030273438),
    MAAC_FLT_C(0.79953724145889282), MAAC_FLT_C(-0.60061645507812500),
    MAAC_FLT_C(0.79583692550659180), MAAC_FLT_C(-0.60551106929779053),
    MAAC_FLT_C(0.79210656881332397), MAAC_FLT_C(-0.61038279533386230),
    MAAC_FLT_C(0.78834640979766846), MAAC_FLT_C(-0.61523157358169556),
    MAAC_FLT_C(0.78455656766891479), MAAC_FLT_C(-0.62005722522735596),
    MAAC_FLT_C(0.78073722124099731), MAAC_FLT_C(-0.62485951185226440),
    MAAC_FLT_C(0.77688848972320557), MAAC_FLT_C(-0.62963825464248657),
    MAAC_FLT_C(0.77301043272018433), MAAC_FLT_C(-0.63439327478408813),
    MAAC_FLT_C(0.76910334825515747), MAAC_FLT_C(-0.63912445306777954),
    MAAC_FLT_C(0.76516723632812500), MAAC_FLT_C(-0.64383155107498169),
    MAAC_FLT_C(0.76120239496231079), MAAC_FLT_C(-0.64851438999176025),
    MAAC_FLT_C(0.75720882415771484), MAAC_FLT_C(-0.65317285060882568),
    MAAC_FLT_C(0.75318682193756104), MAAC_FLT_C(-0.65780669450759888),
    MAAC_FLT_C(0.74913638830184937), MAAC_FLT_C(-0.66241580247879028),
    MAAC_FLT_C(0.74505776166915894), MAAC_FLT_C(-0.66699993610382080),
    MAAC_FLT_C(0.74095112085342407), MAAC_FLT_C(-0.67155897617340088),
    MAAC_FLT_C(0.73681658506393433), MAAC_FLT_C(-0.67609268426895142),
    MAAC_FLT_C(0.73265427350997925), MAAC_FLT_C(-0.68060100078582764),
    MAAC_FLT_C(0.72846436500549316), MAAC_FLT_C(-0.68508368730545044),
    MAAC_FLT_C(0.72424709796905518), MAAC_FLT_C(-0.68954056501388550),
    MAAC_FLT_C(0.72000253200531006), MAAC_FLT_C(-0.69397145509719849),
    MAAC_FLT_C(0.71573084592819214), MAAC_FLT_C(-0.69837623834609985),
    MAAC_FLT_C(0.71143221855163574), MAAC_FLT_C(-0.70275473594665527),
    MAAC_FLT_C(0.70710676908493042), MAAC_FLT_C(-0.70710676908493042),
    MAAC_FLT_C(0.70275473594665527), MAAC_FLT_C(-0.71143221855163574),
    MAAC_FLT_C(0.69837623834609985), MAAC_FLT_C(-0.71573084592819214),
    MAAC_FLT_C(0.69397145509719849), MAAC_FLT_C(-0.72000253200531006),
    MAAC_FLT_C(0.68954056501388550), MAAC_FLT_C(-0.72424709796905518),
    MAAC_FLT_C(0.68508368730545044), MAAC_FLT_C(-0.72846436500549316),
    MAAC_FLT_C(0.68060100078582764), MAAC_FLT_C(-0.73265427350997925),
    MAAC_FLT_C(0.67609268426895142), MAAC_FLT_C(-0.73681658506393433),
    MAAC_FLT_C(0.67155897617340088), MAAC_FLT_C(-0.74095112085342407),
    MAAC_FLT_C(0.66699993610382080), MAAC_FLT_C(-0.74505776166915894),
    MAAC_FLT_C(0.66241580247879028), MAAC_FLT_C(-0.74913638830184937),
    MAAC_FLT_C(0.65780669450759888), MAAC_FLT_C(-0.75318682193756104),
    MAAC_FLT_C(0.65317285060882568), MAAC_FLT_C(-0.75720882415771484),
    MAAC_FLT_C(0.64851438999176025), MAAC_FLT_C(-0.76120239496231079),
    MAAC_FLT_C(0.64383155107498169), MAAC_FLT_C(-0.76516723632812500),
    MAAC_FLT_C(0.63912445306777954), MAAC_FLT_C(-0.76910334825515747),
    MAAC_FLT_C(0.63439327478408813), MAAC_FLT_C(-0.77301043272018433),
    MAAC_FLT_C(0.62963825464248657), MAAC_FLT_C(-0.77688848972320557),
    MAAC_FLT_C(0.62485951185226440), MAAC_FLT_C(-0.78073722124099731),
    MAAC_FLT_C(0.62005722522735596), MAAC_FLT_C(-0.78455656766891479),
    MAAC_FLT_C(0.61523157358169556), MAAC_FLT_C(-0.78834640979766846),
    MAAC_FLT_C(0.61038279533386230), MAAC_FLT_C(-0.79210656881332397),
    MAAC_FLT_C(0.60551106929779053), MAAC_FLT_C(-0.79583692550659180),
    MAAC_FLT_C(0.60061645507812500), MAAC_FLT_C(-0.79953724145889282),
    MAAC_FLT_C(0.59569931030273438), MAAC_FLT_C(-0.80320751667022705),
    MAAC_FLT_C(0.59075969457626343), MAAC_FLT_C(-0.80684757232666016),
    MAAC_FLT_C(0.58579784631729126), MAAC_FLT_C(-0.81045717000961304),
    MAAC_FLT_C(0.58081394433975220), MAAC_FLT_C(-0.81403630971908569),
    MAAC_FLT_C(0.57580816745758057), MAAC_FLT_C(-0.81758481264114380),
    MAAC_FLT_C(0.57078075408935547), MAAC_FLT_C(-0.82110249996185303),
    MAAC_FLT_C(0.56573182344436646), MAAC_FLT_C(-0.82458931207656860),
    MAAC_FLT_C(0.56066155433654785), MAAC_FLT_C(-0.82804507017135620),
    MAAC_FLT_C(0.55557024478912354), MAAC_FLT_C(-0.83146959543228149),
    MAAC_FLT_C(0.55045795440673828), MAAC_FLT_C(-0.83486288785934448),
    MAAC_FLT_C(0.54532498121261597), MAAC_FLT_C(-0.83822470903396606),
    MAAC_FLT_C(0.54017144441604614), MAAC_FLT_C(-0.84155499935150146),
    MAAC_FLT_C(0.53499764204025269), MAAC_FLT_C(-0.84485357999801636),
    MAAC_FLT_C(0.52980363368988037), MAAC_FLT_C(-0.84812033176422119),
    MAAC_FLT_C(0.52458965778350830), MAAC_FLT_C(-0.85135519504547119),
    MAAC_FLT_C(0.51935601234436035), MAAC_FLT_C(-0.85455799102783203),
    MAAC_FLT_C(0.51410275697708130), MAAC_FLT_C(-0.85772860050201416),
    MAAC_FLT_C(0.50883013010025024), MAAC_FLT_C(-0.86086696386337280),
    MAAC_FLT_C(0.50353837013244629), MAAC_FLT_C(-0.86397284269332886),
    MAAC_FLT_C(0.49822765588760376), MAAC_FLT_C(-0.86704623699188232),
    MAAC_FLT_C(0.49289819598197937), MAAC_FLT_C(-0.87008696794509888),
    MAAC_FLT_C(0.48755016922950745), MAAC_FLT_C(-0.87309497594833374),
    MAAC_FLT_C(0.48218378424644470), MAAC_FLT_C(-0.87607008218765259),
    MAAC_FLT_C(0.47679921984672546), MAAC_FLT_C(-0.87901222705841064),
    MAAC_FLT_C(0.47139674425125122), MAAC_FLT_C(-0.88192129135131836),
    MAAC_FLT_C(0.46597650647163391), MAAC_FLT_C(-0.88479709625244141),
    MAAC_FLT_C(0.46053871512413025), MAAC_FLT_C(-0.88763964176177979),
    MAAC_FLT_C(0.45508357882499695), MAAC_FLT_C(-0.89044874906539917),
    MAAC_FLT_C(0.44961133599281311), MAAC_FLT_C(-0.89322429895401001),
    MAAC_FLT_C(0.44412213563919067), MAAC_FLT_C(-0.89596623182296753),
    MAAC_FLT_C(0.43861624598503113), MAAC_FLT_C(-0.89867448806762695),
    MAAC_FLT_C(0.43309381604194641), MAAC_FLT_C(-0.90134882926940918),
    MAAC_FLT_C(0.42755508422851562), MAAC_FLT_C(-0.90398931503295898),
    MAAC_FLT_C(0.42200025916099548), MAAC_FLT_C(-0.90659570693969727),
    MAAC_FLT_C(0.41642954945564270), MAAC_FLT_C(-0.90916800498962402),
    MAAC_FLT_C(0.41084316372871399), MAAC_FLT_C(-0.91170603036880493),
    MAAC_FLT_C(0.40524131059646606), MAAC_FLT_C(-0.91420978307723999),
    MAAC_FLT_C(0.39962419867515564), MAAC_FLT_C(-0.91667908430099487),
    MAAC_FLT_C(0.39399203658103943), MAAC_FLT_C(-0.91911387443542480),
    MAAC_FLT_C(0.38834503293037415), MAAC_FLT_C(-0.92151403427124023),
    MAAC_FLT_C(0.38268342614173889), MAAC_FLT_C(-0.92387950420379639),
    MAAC_FLT_C(0.37700742483139038), MAAC_FLT_C(-0.92621022462844849),
    MAAC_FLT_C(0.37131720781326294), MAAC_FLT_C(-0.92850607633590698),
    MAAC_FLT_C(0.36561298370361328), MAAC_FLT_C(-0.93076694011688232),
    MAAC_FLT_C(0.35989505052566528), MAAC_FLT_C(-0.93299281597137451),
    MAAC_FLT_C(0.35416352748870850), MAAC_FLT_C(-0.93518352508544922),
    MAAC_FLT_C(0.34841868281364441), MAAC_FLT_C(-0.93733900785446167),
    MAAC_FLT_C(0.34266072511672974), MAAC_FLT_C(-0.93945920467376709),
    MAAC_FLT_C(0.33688986301422119), MAAC_FLT_C(-0.94154405593872070),
    MAAC_FLT_C(0.33110630512237549), MAAC_FLT_C(-0.94359344244003296),
    MAAC_FLT_C(0.32531028985977173), MAAC_FLT_C(-0.94560730457305908),
    MAAC_FLT_C(0.31950202584266663), MAAC_FLT_C(-0.94758558273315430),
    MAAC_FLT_C(0.31368175148963928), MAAC_FLT_C(-0.94952815771102905),
    MAAC_FLT_C(0.30784964561462402), MAAC_FLT_C(-0.95143502950668335),
    MAAC_FLT_C(0.30200594663619995), MAAC_FLT_C(-0.95330601930618286),
    MAAC_FLT_C(0.29615089297294617), MAAC_FLT_C(-0.95514118671417236),
    MAAC_FLT_C(0.29028466343879700), MAAC_FLT_C(-0.95694035291671753),
    MAAC_FLT_C(0.28440752625465393), MAAC_FLT_C(-0.95870345830917358),
    MAAC_FLT_C(0.27851969003677368), MAAC_FLT_C(-0.96043050289154053),
    MAAC_FLT_C(0.27262136340141296), MAAC_FLT_C(-0.96212142705917358),
    MAAC_FLT_C(0.26671275496482849), MAAC_FLT_C(-0.96377605199813843),
    MAAC_FLT_C(0.26079410314559937), MAAC_FLT_C(-0.96539443731307983),
    MAAC_FLT_C(0.25486564636230469), MAAC_FLT_C(-0.96697646379470825),
    MAAC_FLT_C(0.24892760813236237), MAAC_FLT_C(-0.96852207183837891),
    MAAC_FLT_C(0.24298018217086792), MAAC_FLT_C(-0.97003126144409180),
    MAAC_FLT_C(0.23702360689640045), MAAC_FLT_C(-0.97150391340255737),
    MAAC_FLT_C(0.23105810582637787), MAAC_FLT_C(-0.97293996810913086),
    MAAC_FLT_C(0.22508391737937927), MAAC_FLT_C(-0.97433936595916748),
    MAAC_FLT_C(0.21910123527050018), MAAC_FLT_C(-0.97570210695266724),
    MAAC_FLT_C(0.21311031281948090), MAAC_FLT_C(-0.97702813148498535),
    MAAC_FLT_C(0.20711137354373932), MAAC_FLT_C(-0.97831737995147705),
    MAAC_FLT_C(0.20110464096069336), MAAC_FLT_C(-0.97956979274749756),
    MAAC_FLT_C(0.19509032368659973), MAAC_FLT_C(-0.98078525066375732),
    MAAC_FLT_C(0.18906866014003754), MAAC_FLT_C(-0.98196387290954590),
    MAAC_FLT_C(0.18303988873958588), MAAC_FLT_C(-0.98310548067092896),
    MAAC_FLT_C(0.17700421810150146), MAAC_FLT_C(-0.98421007394790649),
    MAAC_FLT_C(0.17096188664436340), MAAC_FLT_C(-0.98527765274047852),
    MAAC_FLT_C(0.16491311788558960), MAAC_FLT_C(-0.98630809783935547),
    MAAC_FLT_C(0.15885815024375916), MAAC_FLT_C(-0.98730140924453735),
    MAAC_FLT_C(0.15279719233512878), MAAC_FLT_C(-0.98825758695602417),
    MAAC_FLT_C(0.14673046767711639), MAAC_FLT_C(-0.98917651176452637),
    MAAC_FLT_C(0.14065824449062347), MAAC_FLT_C(-0.99005818367004395),
    MAAC_FLT_C(0.13458070158958435), MAAC_FLT_C(-0.99090266227722168),
    MAAC_FLT_C(0.12849810719490051), MAAC_FLT_C(-0.99170976877212524),
    MAAC_FLT_C(0.12241067737340927), MAAC_FLT_C(-0.99247956275939941),
    MAAC_FLT_C(0.11631862819194794), MAAC_FLT_C(-0.99321192502975464),
    MAAC_FLT_C(0.11022220551967621), MAAC_FLT_C(-0.99390697479248047),
    MAAC_FLT_C(0.10412163287401199), MAAC_FLT_C(-0.99456459283828735),
    MAAC_FLT_C(0.098017141222953796), MAAC_FLT_C(-0.99518471956253052),
    MAAC_FLT_C(0.091908954083919525), MAAC_FLT_C(-0.99576741456985474),
    MAAC_FLT_C(0.085797309875488281), MAAC_FLT_C(-0.99631261825561523),
    MAAC_FLT_C(0.079682439565658569), MAAC_FLT_C(-0.99682027101516724),
    MAAC_FLT_C(0.073564566671848297), MAAC_FLT_C(-0.99729043245315552),
    MAAC_FLT_C(0.067443922162055969), MAAC_FLT_C(-0.99772304296493530),
    MAAC_FLT_C(0.061320737004280090), MAAC_FLT_C(-0.99811810255050659),
    MAAC_FLT_C(0.055195245891809464), MAAC_FLT_C(-0.99847555160522461),
    MAAC_FLT_C(0.049067676067352295), MAAC_FLT_C(-0.99879544973373413),
    MAAC_FLT_C(0.042938258498907089), MAAC_FLT_C(-0.99907773733139038),
    MAAC_FLT_C(0.036807224154472351), MAAC_FLT_C(-0.99932235479354858),
    MAAC_FLT_C(0.030674804002046585), MAAC_FLT_C(-0.99952942132949829),
    MAAC_FLT_C(0.024541229009628296), MAAC_FLT_C(-0.99969881772994995),
    MAAC_FLT_C(0.018406730145215988), MAAC_FLT_C(-0.99983060359954834),
    MAAC_FLT_C(0.012271538376808167), MAAC_FLT_C(-0.99992471933364868),
    MAAC_FLT_C(0.0061358846724033356), MAAC_FLT_C(-0.99998116493225098),
    MAAC_FLT_C(6.1232342629258393e-17), MAAC_FLT_C(-1.0000000000000000),
    MAAC_FLT_C(-0.0061358846724033356), MAAC_FLT_C(-0.99998116493225098),
    MAAC_FLT_C(-0.012271538376808167), MAAC_FLT_C(-0.99992471933364868),
    MAAC_FLT_C(-0.018406730145215988), MAAC_FLT_C(-0.99983060359954834),
    MAAC_FLT_C(-0.024541229009628296), MAAC_FLT_C(-0.99969881772994995),
    MAAC_FLT_C(-0.030674804002046585), MAAC_FLT_C(-0.99952942132949829),
    MAAC_FLT_C(-0.036807224154472351), MAAC_FLT_C(-0.99932235479354858),
    MAAC_FLT_C(-0.042938258498907089), MAAC_FLT_C(-0.99907773733139038),
    MAAC_FLT_C(-0.049067676067352295), MAAC_FLT_C(-0.99879544973373413),
    MAAC_FLT_C(-0.055195245891809464), MAAC_FLT_C(-0.99847555160522461),
    MAAC_FLT_C(-0.061320737004280090), MAAC_FLT_C(-0.99811810255050659),
    MAAC_FLT_C(-0.067443922162055969), MAAC_FLT_C(-0.99772304296493530),
    MAAC_FLT_C(-0.073564566671848297), MAAC_FLT_C(-0.99729043245315552),
    MAAC_FLT_C(-0.079682439565658569), MAAC_FLT_C(-0.99682027101516724),
    MAAC_FLT_C(-0.085797309875488281), MAAC_FLT_C(-0.99631261825561523),
    MAAC_FLT_C(-0.091908954083919525), MAAC_FLT_C(-0.99576741456985474),
    MAAC_FLT_C(-0.098017141222953796), MAAC_FLT_C(-0.99518471956253052),
    MAAC_FLT_C(-0.10412163287401199), MAAC_FLT_C(-0.99456459283828735),
    MAAC_FLT_C(-0.11022220551967621), MAAC_FLT_C(-0.99390697479248047),
    MAAC_FLT_C(-0.11631862819194794), MAAC_FLT_C(-0.99321192502975464),
    MAAC_FLT_C(-0.12241067737340927), MAAC_FLT_C(-0.99247956275939941),
    MAAC_FLT_C(-0.12849810719490051), MAAC_FLT_C(-0.99170976877212524),
    MAAC_FLT_C(-0.13458070158958435), MAAC_FLT_C(-0.99090266227722168),
    MAAC_FLT_C(-0.14065824449062347), MAAC_FLT_C(-0.99005818367004395),
    MAAC_FLT_C(-0.14673046767711639), MAAC_FLT_C(-0.98917651176452637),
    MAAC_FLT_C(-0.15279719233512878), MAAC_FLT_C(-0.98825758695602417),
    MAAC_FLT_C(-0.15885815024375916), MAAC_FLT_C(-0.98730140924453735),
    MAAC_FLT_C(-0.16491311788558960), MAAC_FLT_C(-0.98630809783935547),
    MAAC_FLT_C(-0.17096188664436340), MAAC_FLT_C(-0.98527765274047852),
    MAAC_FLT_C(-0.17700421810150146), MAAC_FLT_C(-0.98421007394790649),
    MAAC_FLT_C(-0.18303988873958588), MAAC_FLT_C(-0.98310548067092896),
    MAAC_FLT_C(-0.18906866014003754), MAAC_FLT_C(-0.98196387290954590),
    MAAC_FLT_C(-0.19509032368659973), MAAC_FLT_C(-0.98078525066375732),
    MAAC_FLT_C(-0.20110464096069336), MAAC_FLT_C(-0.97956979274749756),
    MAAC_FLT_C(-0.20711137354373932), MAAC_FLT_C(-0.97831737995147705),
    MAAC_FLT_C(-0.21311031281948090), MAAC_FLT_C(-0.97702813148498535),
    MAAC_FLT_C(-0.21910123527050018), MAAC_FLT_C(-0.97570210695266724),
    MAAC_FLT_C(-0.22508391737937927), MAAC_FLT_C(-0.97433936595916748),
    MAAC_FLT_C(-0.23105810582637787), MAAC_FLT_C(-0.97293996810913086),
    MAAC_FLT_C(-0.23702360689640045), MAAC_FLT_C(-0.97150391340255737),
    MAAC_FLT_C(-0.24298018217086792), MAAC_FLT_C(-0.97003126144409180),
    MAAC_FLT_C(-0.24892760813236237), MAAC_FLT_C(-0.96852207183837891),
    MAAC_FLT_C(-0.25486564636230469), MAAC_FLT_C(-0.96697646379470825),
    MAAC_FLT_C(-0.26079410314559937), MAAC_FLT_C(-0.96539443731307983),
    MAAC_FLT_C(-0.26671275496482849), MAAC_FLT_C(-0.96377605199813843),
    MAAC_FLT_C(-0.27262136340141296), MAAC_FLT_C(-0.96212142705917358),
    MAAC_FLT_C(-0.27851969003677368), MAAC_FLT_C(-0.96043050289154053),
    MAAC_FLT_C(-0.28440752625465393), MAAC_FLT_C(-0.95870345830917358),
    MAAC_FLT_C(-0.29028466343879700), MAAC_FLT_C(-0.95694035291671753),
    MAAC_FLT_C(-0.29615089297294617), MAAC_FLT_C(-0.95514118671417236),
    MAAC_FLT_C(-0.30200594663619995), MAAC_FLT_C(-0.95330601930618286),
    MAAC_FLT_C(-0.30784964561462402), MAAC_FLT_C(-0.95143502950668335),
    MAAC_FLT_C(-0.31368175148963928), MAAC_FLT_C(-0.94952815771102905),
    MAAC_FLT_C(-0.31950202584266663), MAAC_FLT_C(-0.94758558273315430),
    MAAC_FLT_C(-0.32531028985977173), MAAC_FLT_C(-0.94560730457305908),
    MAAC_FLT_C(-0.33110630512237549), MAAC_FLT_C(-0.94359344244003296),
    MAAC_FLT_C(-0.33688986301422119), MAAC_FLT_C(-0.94154405593872070),
    MAAC_FLT_C(-0.34266072511672974), MAAC_FLT_C(-0.93945920467376709),
    MAAC_FLT_C(-0.34841868281364441), MAAC_FLT_C(-0.93733900785446167),
    MAAC_FLT_C(-0.35416352748870850), MAAC_FLT_C(-0.93518352508544922),
    MAAC_FLT_C(-0.35989505052566528), MAAC_FLT_C(-0.93299281597137451),
    MAAC_FLT_C(-0.36561298370361328), MAAC_FLT_C(-0.93076694011688232),
    MAAC_FLT_C(-0.37131720781326294), MAAC_FLT_C(-0.92850607633590698),
    MAAC_FLT_C(-0.37700742483139038), MAAC_FLT_C(-0.92621022462844849),
    MAAC_FLT_C(-0.38268342614173889), MAAC_FLT_C(-0.92387950420379639),
    MAAC_FLT_C(-0.38834503293037415), MAAC_FLT_C(-0.92151403427124023),
    MAAC_FLT_C(-0.39399203658103943), MAAC_FLT_C(-0.91911387443542480),
    MAAC_FLT_C(-0.39962419867515564), MAAC_FLT_C(-0.91667908430099487),
    MAAC_FLT_C(-0.40524131059646606), MAAC_FLT_C(-0.91420978307723999),
    MAAC_FLT_C(-0.41084316372871399), MAAC_FLT_C(-0.91170603036880493),
    MAAC_FLT_C(-0.41642954945564270), MAAC_FLT_C(-0.90916800498962402),
    MAAC_FLT_C(-0.42200025916099548), MAAC_FLT_C(-0.90659570693969727),
    MAAC_FLT_C(-0.42755508422851562), MAAC_FLT_C(-0.90398931503295898),
    MAAC_FLT_C(-0.43309381604194641), MAAC_FLT_C(-0.90134882926940918),
    MAAC_FLT_C(-0.43861624598503113), MAAC_FLT_C(-0.89867448806762695),
    MAAC_FLT_C(-0.44412213563919067), MAAC_FLT_C(-0.89596623182296753),
    MAAC_FLT_C(-0.44961133599281311), MAAC_FLT_C(-0.89322429895401001),
    MAAC_FLT_C(-0.45508357882499695), MAAC_FLT_C(-0.89044874906539917),
    MAAC_FLT_C(-0.46053871512413025), MAAC_FLT_C(-0.88763964176177979),
    MAAC_FLT_C(-0.46597650647163391), MAAC_FLT_C(-0.88479709625244141),
    MAAC_FLT_C(-0.47139674425125122), MAAC_FLT_C(-0.88192129135131836),
    MAAC_FLT_C(-0.47679921984672546), MAAC_FLT_C(-0.87901222705841064),
    MAAC_FLT_C(-0.48218378424644470), MAAC_FLT_C(-0.87607008218765259),
    MAAC_FLT_C(-0.48755016922950745), MAAC_FLT_C(-0.87309497594833374),
    MAAC_FLT_C(-0.49289819598197937), MAAC_FLT_C(-0.87008696794509888),
    MAAC_FLT_C(-0.49822765588760376), MAAC_FLT_C(-0.86704623699188232),
    MAAC_FLT_C(-0.50353837013244629), MAAC_FLT_C(-0.86397284269332886),
    MAAC_FLT_C(-0.50883013010025024), MAAC_FLT_C(-0.86086696386337280),
    MAAC_FLT_C(-0.51410275697708130), MAAC_FLT_C(-0.85772860050201416),
    MAAC_FLT_C(-0.51935601234436035), MAAC_FLT_C(-0.85455799102783203),
    MAAC_FLT_C(-0.52458965778350830), MAAC_FLT_C(-0.85135519504547119),
    MAAC_FLT_C(-0.52980363368988037), MAAC_FLT_C(-0.84812033176422119),
    MAAC_FLT_C(-0.53499764204025269), MAAC_FLT_C(-0.84485357999801636),
    MAAC_FLT_C(-0.54017144441604614), MAAC_FLT_C(-0.84155499935150146),
    MAAC_FLT_C(-0.54532498121261597), MAAC_FLT_C(-0.83822470903396606),
    MAAC_FLT_C(-0.55045795440673828), MAAC_FLT_C(-0.83486288785934448),
    MAAC_FLT_C(-0.55557024478912354), MAAC_FLT_C(-0.83146959543228149),
    MAAC_FLT_C(-0.56066155433654785), MAAC_FLT_C(-0.82804507017135620),
    MAAC_FLT_C(-0.56573182344436646), MAAC_FLT_C(-0.82458931207656860),
    MAAC_FLT_C(-0.57078075408935547), MAAC_FLT_C(-0.82110249996185303),
    MAAC_FLT_C(-0.57580816745758057), MAAC_FLT_C(-0.81758481264114380),
    MAAC_FLT_C(-0.58081394433975220), MAAC_FLT_C(-0.81403630971908569),
    MAAC_FLT_C(-0.58579784631729126), MAAC_FLT_C(-0.81045717000961304),
    MAAC_FLT_C(-0.59075969457626343), MAAC_FLT_C(-0.80684757232666016),
    MAAC_FLT_C(-0.59569931030273438), MAAC_FLT_C(-0.80320751667022705),
    MAAC_FLT_C(-0.60061645507812500), MAAC_FLT_C(-0.79953724145889282),
    MAAC_FLT_C(-0.60551106929779053), MAAC_FLT_C(-0.79583692550659180),
    MAAC_FLT_C(-0.61038279533386230), MAAC_FLT_C(-0.79210656881332397),
    MAAC_FLT_C(-0.61523157358169556), MAAC_FLT_C(-0.78834640979766846),
    MAAC_FLT_C(-0.62005722522735596), MAAC_FLT_C(-0.78455656766891479),
    MAAC_FLT_C(-0.62485951185226440), MAAC_FLT_C(-0.78073722124099731),
    MAAC_FLT_C(-0.62963825464248657), MAAC_FLT_C(-0.77688848972320557),
    MAAC_FLT_C(-0.63439327478408813), MAAC_FLT_C(-0.77301043272018433),
    MAAC_FLT_C(-0.63912445306777954), MAAC_FLT_C(-0.76910334825515747),
    MAAC_FLT_C(-0.64383155107498169), MAAC_FLT_C(-0.76516723632812500),
    MAAC_FLT_C(-0.64851438999176025), MAAC_FLT_C(-0.76120239496231079),
    MAAC_FLT_C(-0.65317285060882568), MAAC_FLT_C(-0.75720882415771484),
    MAAC_FLT_C(-0.65780669450759888), MAAC_FLT_C(-0.75318682193756104),
    MAAC_FLT_C(-0.66241580247879028), MAAC_FLT_C(-0.74913638830184937),
    MAAC_FLT_C(-0.66699993610382080), MAAC_FLT_C(-0.74505776166915894),
    MAAC_FLT_C(-0.67155897617340088), MAAC_FLT_C(-0.74095112085342407),
    MAAC_FLT_C(-0.67609268426895142), MAAC_FLT_C(-0.73681658506393433),
    MAAC_FLT_C(-0.68060100078582764), MAAC_FLT_C(-0.73265427350997925),
    MAAC_FLT_C(-0.68508368730545044), MAAC_FLT_C(-0.72846436500549316),
    MAAC_FLT_C(-0.68954056501388550), MAAC_FLT_C(-0.72424709796905518),
    MAAC_FLT_C(-0.69397145509719849), MAAC_FLT_C(-0.72000253200531006),
    MAAC_FLT_C(-0.69837623834609985), MAAC_FLT_C(-0.71573084592819214),
    MAAC_FLT_C(-0.70275473594665527), MAAC_FLT_C(-0.71143221855163574),
    MAAC_FLT_C(-0.70710676908493042), MAAC_FLT_C(-0.70710676908493042),
    MAAC_FLT_C(-0.71143221855163574), MAAC_FLT_C(-0.70275473594665527),
    MAAC_FLT_C(-0.71573084592819214), MAAC_FLT_C(-0.69837623834609985),
    MAAC_FLT_C(-0.72000253200531006), MAAC_FLT_C(-0.69397145509719849),
    MAAC_FLT_C(-0.72424709796905518), MAAC_FLT_C(-0.68954056501388550),
    MAAC_FLT_C(-0.72846436500549316), MAAC_FLT_C(-0.68508368730545044),
    MAAC_FLT_C(-0.73265427350997925), MAAC_FLT_C(-0.68060100078582764),
    MAAC_FLT_C(-0.73681658506393433), MAAC_FLT_C(-0.67609268426895142),
    MAAC_FLT_C(-0.74095112085342407), MAAC_FLT_C(-0.67155897617340088),
    MAAC_FLT_C(-0.74505776166915894), MAAC_FLT_C(-0.66699993610382080),
    MAAC_FLT_C(-0.74913638830184937), MAAC_FLT_C(-0.66241580247879028),
    MAAC_FLT_C(-0.75318682193756104), MAAC_FLT_C(-0.65780669450759888),
    MAAC_FLT_C(-0.75720882415771484), MAAC_FLT_C(-0.65317285060882568),
    MAAC_FLT_C(-0.76120239496231079), MAAC_FLT_C(-0.64851438999176025),
    MAAC_FLT_C(-0.76516723632812500), MAAC_FLT_C(-0.64383155107498169),
    MAAC_FLT_C(-0.76910334825515747), MAAC_FLT_C(-0.63912445306777954),
    MAAC_FLT_C(-0.77301043272018433), MAAC_FLT_C(-0.63439327478408813),
    MAAC_FLT_C(-0.77688848972320557), MAAC_FLT_C(-0.62963825464248657),
    MAAC_FLT_C(-0.78073722124099731), MAAC_FLT_C(-0.62485951185226440),
    MAAC_FLT_C(-0.78455656766891479), MAAC_FLT_C(-0.62005722522735596),
    MAAC_FLT_C(-0.78834640979766846), MAAC_FLT_C(-0.61523157358169556),
    MAAC_FLT_C(-0.79210656881332397), MAAC_FLT_C(-0.61038279533386230),
    MAAC_FLT_C(-0.79583692550659180), MAAC_FLT_C(-0.60551106929779053),
    MAAC_FLT_C(-0.79953724145889282), MAAC_FLT_C(-0.60061645507812500),
    MAAC_FLT_C(-0.80320751667022705), MAAC_FLT_C(-0.59569931030273438),
    MAAC_FLT_C(-0.80684757232666016), MAAC_FLT_C(-0.59075969457626343),
    MAAC_FLT_C(-0.81045717000961304), MAAC_FLT_C(-0.58579784631729126),
    MAAC_FLT_C(-0.81403630971908569), MAAC_FLT_C(-0.58081394433975220),
    MAAC_FLT_C(-0.81758481264114380), MAAC_FLT_C(-0.57580816745758057),
    MAAC_FLT_C(-0.82110249996185303), MAAC_FLT_C(-0.57078075408935547),
    MAAC_FLT_C(-0.82458931207656860), MAAC_FLT_C(-0.56573182344436646),
    MAAC_FLT_C(-0.82804507017135620), MAAC_FLT_C(-0.56066155433654785),
    MAAC_FLT_C(-0.83146959543228149), MAAC_FLT_C(-0.55557024478912354),
    MAAC_FLT_C(-0.83486288785934448), MAAC_FLT_C(-0.55045795440673828),
    MAAC_FLT_C(-0.83822470903396606), MAAC_FLT_C(-0.54532498121261597),
    MAAC_FLT_C(-0.84155499935150146), MAAC_FLT_C(-0.54017144441604614),
    MAAC_FLT_C(-0.84485357999801636), MAAC_FLT_C(-0.53499764204025269),
    MAAC_FLT_C(-0.84812033176422119), MAAC_FLT_C(-0.52980363368988037),
    MAAC_FLT_C(-0.85135519504547119), MAAC_FLT_C(-0.52458965778350830),
    MAAC_FLT_C(-0.85455799102783203), MAAC_FLT_C(-0.51935601234436035),
    MAAC_FLT_C(-0.85772860050201416), MAAC_FLT_C(-0.51410275697708130),
    MAAC_FLT_C(-0.86086696386337280), MAAC_FLT_C(-0.50883013010025024),
    MAAC_FLT_C(-0.86397284269332886), MAAC_FLT_C(-0.50353837013244629),
    MAAC_FLT_C(-0.86704623699188232), MAAC_FLT_C(-0.49822765588760376),
    MAAC_FLT_C(-0.87008696794509888), MAAC_FLT_C(-0.49289819598197937),
    MAAC_FLT_C(-0.87309497594833374), MAAC_FLT_C(-0.48755016922950745),
    MAAC_FLT_C(-0.87607008218765259), MAAC_FLT_C(-0.48218378424644470),
    MAAC_FLT_C(-0.87901222705841064), MAAC_FLT_C(-0.47679921984672546),
    MAAC_FLT_C(-0.88192129135131836), MAAC_FLT_C(-0.47139674425125122),
    MAAC_FLT_C(-0.88479709625244141), MAAC_FLT_C(-0.46597650647163391),
    MAAC_FLT_C(-0.88763964176177979), MAAC_FLT_C(-0.46053871512413025),
    MAAC_FLT_C(-0.89044874906539917), MAAC_FLT_C(-0.45508357882499695),
    MAAC_FLT_C(-0.89322429895401001), MAAC_FLT_C(-0.44961133599281311),
    MAAC_FLT_C(-0.89596623182296753), MAAC_FLT_C(-0.44412213563919067),
    MAAC_FLT_C(-0.89867448806762695), MAAC_FLT_C(-0.43861624598503113),
    MAAC_FLT_C(-0.90134882926940918), MAAC_FLT_C(-0.43309381604194641),
    MAAC_FLT_C(-0.90398931503295898), MAAC_FLT_C(-0.42755508422851562),
    MAAC_FLT_C(-0.90659570693969727), MAAC_FLT_C(-0.42200025916099548),
    MAAC_FLT_C(-0.90916800498962402), MAAC_FLT_C(-0.41642954945564270),
    MAAC_FLT_C(-0.91170603036880493), MAAC_FLT_C(-0.41084316372871399),
    MAAC_FLT_C(-0.91420978307723999), MAAC_FLT_C(-0.40524131059646606),
    MAAC_FLT_C(-0.91667908430099487), MAAC_FLT_C(-0.39962419867515564),
    MAAC_FLT_C(-0.91911387443542480), MAAC_FLT_C(-0.39399203658103943),
    MAAC_FLT_C(-0.92151403427124023), MAAC_FLT_C(-0.38834503293037415),
    MAAC_FLT_C(-0.92387950420379639), MAAC_FLT_C(-0.38268342614173889),
    MAAC_FLT_C(-0.92621022462844849), MAAC_FLT_C(-0.37700742483139038),
    MAAC_FLT_C(-0.92850607633590698), MAAC_FLT_C(-0.37131720781326294),
    MAAC_FLT_C(-0.93076694011688232), MAAC_FLT_C(-0.36561298370361328),
    MAAC_FLT_C(-0.93299281597137451), MAAC_FLT_C(-0.35989505052566528),
    MAAC_FLT_C(-0.93518352508544922), MAAC_FLT_C(-0.35416352748870850),
    MAAC_FLT_C(-0.93733900785446167), MAAC_FLT_C(-0.34841868281364441),
    MAAC_FLT_C(-0.93945920467376709), MAAC_FLT_C(-0.34266072511672974),
    MAAC_FLT_C(-0.94154405593872070), MAAC_FLT_C(-0.33688986301422119),
    MAAC_FLT_C(-0.94359344244003296), MAAC_FLT_C(-0.33110630512237549),
    MAAC_FLT_C(-0.94560730457305908), MAAC_FLT_C(-0.32531028985977173),
    MAAC_FLT_C(-0.94758558273315430), MAAC_FLT_C(-0.31950202584266663),
    MAAC_FLT_C(-0.94952815771102905), MAAC_FLT_C(-0.31368175148963928),
    MAAC_FLT_C(-0.95143502950668335), MAAC_FLT_C(-0.30784964561462402),
    MAAC_FLT_C(-0.95330601930618286), MAAC_FLT_C(-0.30200594663619995),
    MAAC_FLT_C(-0.95514118671417236), MAAC_FLT_C(-0.29615089297294617),
    MAAC_FLT_C(-0.95694035291671753), MAAC_FLT_C(-0.29028466343879700),
    MAAC_FLT_C(-0.95870345830917358), MAAC_FLT_C(-0.28440752625465393),
    MAAC_FLT_C(-0.96043050289154053), MAAC_FLT_C(-0.27851969003677368),
    MAAC_FLT_C(-0.96212142705917358), MAAC_FLT_C(-0.27262136340141296),
    MAAC_FLT_C(-0.96377605199813843), MAAC_FLT_C(-0.26671275496482849),
    MAAC_FLT_C(-0.96539443731307983), MAAC_FLT_C(-0.26079410314559937),
    MAAC_FLT_C(-0.96697646379470825), MAAC_FLT_C(-0.25486564636230469),
    MAAC_FLT_C(-0.96852207183837891), MAAC_FLT_C(-0.24892760813236237),
    MAAC_FLT_C(-0.97003126144409180), MAAC_FLT_C(-0.24298018217086792),
    MAAC_FLT_C(-0.97150391340255737), MAAC_FLT_C(-0.23702360689640045),
    MAAC_FLT_C(-0.97293996810913086), MAAC_FLT_C(-0.23105810582637787),
    MAAC_FLT_C(-0.97433936595916748), MAAC_FLT_C(-0.22508391737937927),
    MAAC_FLT_C(-0.97570210695266724), MAAC_FLT_C(-0.21910123527050018),
    MAAC_FLT_C(-0.97702813148498535), MAAC_FLT_C(-0.21311031281948090),
    MAAC_FLT_C(-0.97831737995147705), MAAC_FLT_C(-0.20711137354373932),
    MAAC_FLT_C(-0.97956979274749756), MAAC_FLT_C(-0.20110464096069336),
    MAAC_FLT_C(-0.98078525066375732), MAAC_FLT_C(-0.19509032368659973),
    MAAC_FLT_C(-0.98196387290954590), MAAC_FLT_C(-0.18906866014003754),
    MAAC_FLT_C(-0.98310548067092896), MAAC_FLT_C(-0.18303988873958588),
    MAAC_FLT_C(-0.98421007394790649), MAAC_FLT_C(-0.17700421810150146),
    MAAC_FLT_C(-0.98527765274047852), MAAC_FLT_C(-0.17096188664436340),
    MAAC_FLT_C(-0.98630809783935547), MAAC_FLT_C(-0.16491311788558960),
    MAAC_FLT_C(-0.98730140924453735), MAAC_FLT_C(-0.15885815024375916),
    MAAC_FLT_C(-0.98825758695602417), MAAC_FLT_C(-0.15279719233512878),
    MAAC_FLT_C(-0.98917651176452637), MAAC_FLT_C(-0.14673046767711639),
    MAAC_FLT_C(-0.99005818367004395), MAAC_FLT_C(-0.14065824449062347),
    MAAC_FLT_C(-0.99090266227722168), MAAC_FLT_C(-0.13458070158958435),
    MAAC_FLT_C(-0.99170976877212524), MAAC_FLT_C(-0.12849810719490051),
    MAAC_FLT_C(-0.99247956275939941), MAAC_FLT_C(-0.12241067737340927),
    MAAC_FLT_C(-0.99321192502975464), MAAC_FLT_C(-0.11631862819194794),
    MAAC_FLT_C(-0.99390697479248047), MAAC_FLT_C(-0.11022220551967621),
    MAAC_FLT_C(-0.99456459283828735), MAAC_FLT_C(-0.10412163287401199),
    MAAC_FLT_C(-0.99518471956253052), MAAC_FLT_C(-0.098017141222953796),
    MAAC_FLT_C(-0.99576741456985474), MAAC_FLT_C(-0.091908954083919525),
    MAAC_FLT_C(-0.99631261825561523), MAAC_FLT_C(-0.085797309875488281),
    MAAC_FLT_C(-0.99682027101516724), MAAC_FLT_C(-0.079682439565658569),
    MAAC_FLT_C(-0.99729043245315552), MAAC_FLT_C(-0.073564566671848297),
    MAAC_FLT_C(-0.99772304296493530), MAAC_FLT_C(-0.067443922162055969),
    MAAC_FLT_C(-0.99811810255050659), MAAC_FLT_C(-0.061320737004280090),
    MAAC_FLT_C(-0.99847555160522461), MAAC_FLT_C(-0.055195245891809464),
    MAAC_FLT_C(-0.99879544973373413), MAAC_FLT_C(-0.049067676067352295),
    MAAC_FLT_C(-0.99907773733139038), MAAC_FLT_C(-0.042938258498907089),
    MAAC_FLT_C(-0.99932235479354858), MAAC_FLT_C(-0.036807224154472351),
    MAAC_FLT_C(-0.99952942132949829), MAAC_FLT_C(-0.030674804002046585),
    MAAC_FLT_C(-0.99969881772994995), MAAC_FLT_C(-0.024541229009628296),
    MAAC_FLT_C(-0.99983060359954834), MAAC_FLT_C(-0.018406730145215988),
    MAAC_FLT_C(-0.99992471933364868), MAAC_FLT_C(-0.012271538376808167),
    MAAC_FLT_C(-0.99998116493225098), MAAC_FLT_C(-0.0061358846724033356)
};

static const maac_flt MAAC_IMDCT_B_2048[1024] = {
    MAAC_FLT_C(0.99999970197677612), MAAC_FLT_C(0.00076699029887095094),
    MAAC_FLT_C(0.99999737739562988), MAAC_FLT_C(0.0023009690921753645),
    MAAC_FLT_C(0.99999266862869263), MAAC_FLT_C(0.0038349425885826349),
    MAAC_FLT_C(0.99998557567596436), MAAC_FLT_C(0.0053689070045948029),
    MAAC_FLT_C(0.99997615814208984), MAAC_FLT_C(0.0069028586149215698),
    MAAC_FLT_C(0.99996441602706909), MAAC_FLT_C(0.0084367943927645683),
    MAAC_FLT_C(0.99995028972625732), MAAC_FLT_C(0.0099707096815109253),
    MAAC_FLT_C(0.99993383884429932), MAAC_FLT_C(0.011504601687192917),
    MAAC_FLT_C(0.99991500377655029), MAAC_FLT_C(0.013038467615842819),
    MAAC_FLT_C(0.99989384412765503), MAAC_FLT_C(0.014572301879525185),
    MAAC_FLT_C(0.99987030029296875), MAAC_FLT_C(0.016106102615594864),
    MAAC_FLT_C(0.99984443187713623), MAAC_FLT_C(0.017639864236116409),
    MAAC_FLT_C(0.99981617927551270), MAAC_FLT_C(0.019173584878444672),
    MAAC_FLT_C(0.99978560209274292), MAAC_FLT_C(0.020707260817289352),
    MAAC_FLT_C(0.99975264072418213), MAAC_FLT_C(0.022240888327360153),
    MAAC_FLT_C(0.99971735477447510), MAAC_FLT_C(0.023774461820721626),
    MAAC_FLT_C(0.99967968463897705), MAAC_FLT_C(0.025307981297373772),
    MAAC_FLT_C(0.99963968992233276), MAAC_FLT_C(0.026841439306735992),
    MAAC_FLT_C(0.99959737062454224), MAAC_FLT_C(0.028374835848808289),
    MAAC_FLT_C(0.99955266714096069), MAAC_FLT_C(0.029908165335655212),
    MAAC_FLT_C(0.99950557947158813), MAAC_FLT_C(0.031441424041986465),
    MAAC_FLT_C(0.99945616722106934), MAAC_FLT_C(0.032974608242511749),
    MAAC_FLT_C(0.99940443038940430), MAAC_FLT_C(0.034507714211940765),
    MAAC_FLT_C(0.99935030937194824), MAAC_FLT_C(0.036040741950273514),
    MAAC_FLT_C(0.99929386377334595), MAAC_FLT_C(0.037573684006929398),
    MAAC_FLT_C(0.99923503398895264), MAAC_FLT_C(0.039106536656618118),
    MAAC_FLT_C(0.99917387962341309), MAAC_FLT_C(0.040639296174049377),
    MAAC_FLT_C(0.99911034107208252), MAAC_FLT_C(0.042171962559223175),
    MAAC_FLT_C(0.99904447793960571), MAAC_FLT_C(0.043704528361558914),
    MAAC_FLT_C(0.99897629022598267), MAAC_FLT_C(0.045236989855766296),
    MAAC_FLT_C(0.99890571832656860), MAAC_FLT_C(0.046769347041845322),
    MAAC_FLT_C(0.99883282184600830), MAAC_FLT_C(0.048301592469215393),
    MAAC_FLT_C(0.99875754117965698), MAAC_FLT_C(0.049833726137876511),
    MAAC_FLT_C(0.99867993593215942), MAAC_FLT_C(0.051365740597248077),
    MAAC_FLT_C(0.99859994649887085), MAAC_FLT_C(0.052897635847330093),
    MAAC_FLT_C(0.99851763248443604), MAAC_FLT_C(0.054429408162832260),
    MAAC_FLT_C(0.99843293428421021), MAAC_FLT_C(0.055961050093173981),
    MAAC_FLT_C(0.99834591150283813), MAAC_FLT_C(0.057492557913064957),
    MAAC_FLT_C(0.99825656414031982), MAAC_FLT_C(0.059023935347795486),
    MAAC_FLT_C(0.99816483259201050), MAAC_FLT_C(0.060555171221494675),
    MAAC_FLT_C(0.99807077646255493), MAAC_FLT_C(0.062086265534162521),
    MAAC_FLT_C(0.99797439575195312), MAAC_FLT_C(0.063617214560508728),
    MAAC_FLT_C(0.99787563085556030), MAAC_FLT_C(0.065148010849952698),
    MAAC_FLT_C(0.99777448177337646), MAAC_FLT_C(0.066678658127784729),
    MAAC_FLT_C(0.99767106771469116), MAAC_FLT_C(0.068209141492843628),
    MAAC_FLT_C(0.99756520986557007), MAAC_FLT_C(0.069739468395709991),
    MAAC_FLT_C(0.99745708703994751), MAAC_FLT_C(0.071269631385803223),
    MAAC_FLT_C(0.99734658002853394), MAAC_FLT_C(0.072799630463123322),
    MAAC_FLT_C(0.99723374843597412), MAAC_FLT_C(0.074329450726509094),
    MAAC_FLT_C(0.99711853265762329), MAAC_FLT_C(0.075859107077121735),
    MAAC_FLT_C(0.99700099229812622), MAAC_FLT_C(0.077388577163219452),
    MAAC_FLT_C(0.99688112735748291), MAAC_FLT_C(0.078917860984802246),
    MAAC_FLT_C(0.99675887823104858), MAAC_FLT_C(0.080446965992450714),
    MAAC_FLT_C(0.99663430452346802), MAAC_FLT_C(0.081975877285003662),
    MAAC_FLT_C(0.99650740623474121), MAAC_FLT_C(0.083504602313041687),
    MAAC_FLT_C(0.99637812376022339), MAAC_FLT_C(0.085033126175403595),
    MAAC_FLT_C(0.99624651670455933), MAAC_FLT_C(0.086561448872089386),
    MAAC_FLT_C(0.99611258506774902), MAAC_FLT_C(0.088089570403099060),
    MAAC_FLT_C(0.99597626924514771), MAAC_FLT_C(0.089617483317852020),
    MAAC_FLT_C(0.99583762884140015), MAAC_FLT_C(0.091145187616348267),
    MAAC_FLT_C(0.99569660425186157), MAAC_FLT_C(0.092672675848007202),
    MAAC_FLT_C(0.99555331468582153), MAAC_FLT_C(0.094199940562248230),
    MAAC_FLT_C(0.99540764093399048), MAAC_FLT_C(0.095726989209651947),
    MAAC_FLT_C(0.99525958299636841), MAAC_FLT_C(0.097253814339637756),
    MAAC_FLT_C(0.99510926008224487), MAAC_FLT_C(0.098780408501625061),
    MAAC_FLT_C(0.99495655298233032), MAAC_FLT_C(0.10030677169561386),
    MAAC_FLT_C(0.99480152130126953), MAAC_FLT_C(0.10183289647102356),
    MAAC_FLT_C(0.99464416503906250), MAAC_FLT_C(0.10335878282785416),
    MAAC_FLT_C(0.99448442459106445), MAAC_FLT_C(0.10488442331552505),
    MAAC_FLT_C(0.99432235956192017), MAAC_FLT_C(0.10640981793403625),
    MAAC_FLT_C(0.99415796995162964), MAAC_FLT_C(0.10793496668338776),
    MAAC_FLT_C(0.99399119615554810), MAAC_FLT_C(0.10945985466241837),
    MAAC_FLT_C(0.99382215738296509), MAAC_FLT_C(0.11098448932170868),
    MAAC_FLT_C(0.99365073442459106), MAAC_FLT_C(0.11250886321067810),
    MAAC_FLT_C(0.99347698688507080), MAAC_FLT_C(0.11403297632932663),
    MAAC_FLT_C(0.99330085515975952), MAAC_FLT_C(0.11555681377649307),
    MAAC_FLT_C(0.99312245845794678), MAAC_FLT_C(0.11708038300275803),
    MAAC_FLT_C(0.99294167757034302), MAAC_FLT_C(0.11860367655754089),
    MAAC_FLT_C(0.99275857210159302), MAAC_FLT_C(0.12012668699026108),
    MAAC_FLT_C(0.99257314205169678), MAAC_FLT_C(0.12164941430091858),
    MAAC_FLT_C(0.99238532781600952), MAAC_FLT_C(0.12317185848951340),
    MAAC_FLT_C(0.99219524860382080), MAAC_FLT_C(0.12469401955604553),
    MAAC_FLT_C(0.99200278520584106), MAAC_FLT_C(0.12621587514877319),
    MAAC_FLT_C(0.99180799722671509), MAAC_FLT_C(0.12773744761943817),
    MAAC_FLT_C(0.99161088466644287), MAAC_FLT_C(0.12925870716571808),
    MAAC_FLT_C(0.99141144752502441), MAAC_FLT_C(0.13077966868877411),
    MAAC_FLT_C(0.99120968580245972), MAAC_FLT_C(0.13230031728744507),
    MAAC_FLT_C(0.99100553989410400), MAAC_FLT_C(0.13382065296173096),
    MAAC_FLT_C(0.99079912900924683), MAAC_FLT_C(0.13534067571163177),
    MAAC_FLT_C(0.99059033393859863), MAAC_FLT_C(0.13686038553714752),
    MAAC_FLT_C(0.99037921428680420), MAAC_FLT_C(0.13837976753711700),
    MAAC_FLT_C(0.99016582965850830), MAAC_FLT_C(0.13989883661270142),
    MAAC_FLT_C(0.98995006084442139), MAAC_FLT_C(0.14141756296157837),
    MAAC_FLT_C(0.98973196744918823), MAAC_FLT_C(0.14293596148490906),
    MAAC_FLT_C(0.98951148986816406), MAAC_FLT_C(0.14445401728153229),
    MAAC_FLT_C(0.98928874731063843), MAAC_FLT_C(0.14597174525260925),
    MAAC_FLT_C(0.98906368017196655), MAAC_FLT_C(0.14748911559581757),
    MAAC_FLT_C(0.98883628845214844), MAAC_FLT_C(0.14900614321231842),
    MAAC_FLT_C(0.98860651254653931), MAAC_FLT_C(0.15052282810211182),
    MAAC_FLT_C(0.98837447166442871), MAAC_FLT_C(0.15203915536403656),
    MAAC_FLT_C(0.98814010620117188), MAAC_FLT_C(0.15355512499809265),
    MAAC_FLT_C(0.98790335655212402), MAAC_FLT_C(0.15507073700428009),
    MAAC_FLT_C(0.98766434192657471), MAAC_FLT_C(0.15658597648143768),
    MAAC_FLT_C(0.98742294311523438), MAAC_FLT_C(0.15810084342956543),
    MAAC_FLT_C(0.98717927932739258), MAAC_FLT_C(0.15961535274982452),
    MAAC_FLT_C(0.98693329095840454), MAAC_FLT_C(0.16112947463989258),
    MAAC_FLT_C(0.98668491840362549), MAAC_FLT_C(0.16264322400093079),
    MAAC_FLT_C(0.98643428087234497), MAAC_FLT_C(0.16415658593177795),
    MAAC_FLT_C(0.98618131875991821), MAAC_FLT_C(0.16566956043243408),
    MAAC_FLT_C(0.98592603206634521), MAAC_FLT_C(0.16718214750289917),
    MAAC_FLT_C(0.98566842079162598), MAAC_FLT_C(0.16869434714317322),
    MAAC_FLT_C(0.98540848493576050), MAAC_FLT_C(0.17020614445209503),
    MAAC_FLT_C(0.98514622449874878), MAAC_FLT_C(0.17171753942966461),
    MAAC_FLT_C(0.98488163948059082), MAAC_FLT_C(0.17322853207588196),
    MAAC_FLT_C(0.98461478948593140), MAAC_FLT_C(0.17473910748958588),
    MAAC_FLT_C(0.98434555530548096), MAAC_FLT_C(0.17624929547309875),
    MAAC_FLT_C(0.98407405614852905), MAAC_FLT_C(0.17775905132293701),
    MAAC_FLT_C(0.98380023241043091), MAAC_FLT_C(0.17926838994026184),
    MAAC_FLT_C(0.98352402448654175), MAAC_FLT_C(0.18077731132507324),
    MAAC_FLT_C(0.98324561119079590), MAAC_FLT_C(0.18228580057621002),
    MAAC_FLT_C(0.98296481370925903), MAAC_FLT_C(0.18379387259483337),
    MAAC_FLT_C(0.98268169164657593), MAAC_FLT_C(0.18530149757862091),
    MAAC_FLT_C(0.98239630460739136), MAAC_FLT_C(0.18680869042873383),
    MAAC_FLT_C(0.98210859298706055), MAAC_FLT_C(0.18831545114517212),
    MAAC_FLT_C(0.98181855678558350), MAAC_FLT_C(0.18982176482677460),
    MAAC_FLT_C(0.98152625560760498), MAAC_FLT_C(0.19132763147354126),
    MAAC_FLT_C(0.98123157024383545), MAAC_FLT_C(0.19283305108547211),
    MAAC_FLT_C(0.98093461990356445), MAAC_FLT_C(0.19433800876140594),
    MAAC_FLT_C(0.98063534498214722), MAAC_FLT_C(0.19584251940250397),
    MAAC_FLT_C(0.98033380508422852), MAAC_FLT_C(0.19734656810760498),
    MAAC_FLT_C(0.98002988100051880), MAAC_FLT_C(0.19885013997554779),
    MAAC_FLT_C(0.97972375154495239), MAAC_FLT_C(0.20035324990749359),
    MAAC_FLT_C(0.97941523790359497), MAAC_FLT_C(0.20185589790344238),
    MAAC_FLT_C(0.97910445928573608), MAAC_FLT_C(0.20335806906223297),
    MAAC_FLT_C(0.97879135608673096), MAAC_FLT_C(0.20485974848270416),
    MAAC_FLT_C(0.97847592830657959), MAAC_FLT_C(0.20636095106601715),
    MAAC_FLT_C(0.97815823554992676), MAAC_FLT_C(0.20786167681217194),
    MAAC_FLT_C(0.97783821821212769), MAAC_FLT_C(0.20936191082000732),
    MAAC_FLT_C(0.97751593589782715), MAAC_FLT_C(0.21086163818836212),
    MAAC_FLT_C(0.97719132900238037), MAAC_FLT_C(0.21236088871955872),
    MAAC_FLT_C(0.97686439752578735), MAAC_FLT_C(0.21385963261127472),
    MAAC_FLT_C(0.97653520107269287), MAAC_FLT_C(0.21535786986351013),
    MAAC_FLT_C(0.97620368003845215), MAAC_FLT_C(0.21685560047626495),
    MAAC_FLT_C(0.97586989402770996), MAAC_FLT_C(0.21835282444953918),
    MAAC_FLT_C(0.97553378343582153), MAAC_FLT_C(0.21984952688217163),
    MAAC_FLT_C(0.97519540786743164), MAAC_FLT_C(0.22134572267532349),
    MAAC_FLT_C(0.97485470771789551), MAAC_FLT_C(0.22284139692783356),
    MAAC_FLT_C(0.97451174259185791), MAAC_FLT_C(0.22433653473854065),
    MAAC_FLT_C(0.97416645288467407), MAAC_FLT_C(0.22583115100860596),
    MAAC_FLT_C(0.97381889820098877), MAAC_FLT_C(0.22732524573802948),
    MAAC_FLT_C(0.97346901893615723), MAAC_FLT_C(0.22881878912448883),
    MAAC_FLT_C(0.97311687469482422), MAAC_FLT_C(0.23031181097030640),
    MAAC_FLT_C(0.97276246547698975), MAAC_FLT_C(0.23180428147315979),
    MAAC_FLT_C(0.97240573167800903), MAAC_FLT_C(0.23329620063304901),
    MAAC_FLT_C(0.97204673290252686), MAAC_FLT_C(0.23478758335113525),
    MAAC_FLT_C(0.97168540954589844), MAAC_FLT_C(0.23627839982509613),
    MAAC_FLT_C(0.97132182121276855), MAAC_FLT_C(0.23776866495609283),
    MAAC_FLT_C(0.97095590829849243), MAAC_FLT_C(0.23925837874412537),
    MAAC_FLT_C(0.97058779001235962), MAAC_FLT_C(0.24074752628803253),
    MAAC_FLT_C(0.97021734714508057), MAAC_FLT_C(0.24223610758781433),
    MAAC_FLT_C(0.96984457969665527), MAAC_FLT_C(0.24372410774230957),
    MAAC_FLT_C(0.96946960687637329), MAAC_FLT_C(0.24521154165267944),
    MAAC_FLT_C(0.96909230947494507), MAAC_FLT_C(0.24669840931892395),
    MAAC_FLT_C(0.96871274709701538), MAAC_FLT_C(0.24818468093872070),
    MAAC_FLT_C(0.96833086013793945), MAAC_FLT_C(0.24967038631439209),
    MAAC_FLT_C(0.96794676780700684), MAAC_FLT_C(0.25115549564361572),
    MAAC_FLT_C(0.96756035089492798), MAAC_FLT_C(0.25264000892639160),
    MAAC_FLT_C(0.96717166900634766), MAAC_FLT_C(0.25412392616271973),
    MAAC_FLT_C(0.96678072214126587), MAAC_FLT_C(0.25560724735260010),
    MAAC_FLT_C(0.96638745069503784), MAAC_FLT_C(0.25708997249603271),
    MAAC_FLT_C(0.96599197387695312), MAAC_FLT_C(0.25857207179069519),
    MAAC_FLT_C(0.96559417247772217), MAAC_FLT_C(0.26005360484123230),
    MAAC_FLT_C(0.96519410610198975), MAAC_FLT_C(0.26153448224067688),
    MAAC_FLT_C(0.96479183435440063), MAAC_FLT_C(0.26301476359367371),
    MAAC_FLT_C(0.96438723802566528), MAAC_FLT_C(0.26449441909790039),
    MAAC_FLT_C(0.96398037672042847), MAAC_FLT_C(0.26597347855567932),
    MAAC_FLT_C(0.96357119083404541), MAAC_FLT_C(0.26745188236236572),
    MAAC_FLT_C(0.96315979957580566), MAAC_FLT_C(0.26892966032028198),
    MAAC_FLT_C(0.96274614334106445), MAAC_FLT_C(0.27040681242942810),
    MAAC_FLT_C(0.96233022212982178), MAAC_FLT_C(0.27188333868980408),
    MAAC_FLT_C(0.96191203594207764), MAAC_FLT_C(0.27335920929908752),
    MAAC_FLT_C(0.96149158477783203), MAAC_FLT_C(0.27483445405960083),
    MAAC_FLT_C(0.96106886863708496), MAAC_FLT_C(0.27630904316902161),
    MAAC_FLT_C(0.96064388751983643), MAAC_FLT_C(0.27778297662734985),
    MAAC_FLT_C(0.96021664142608643), MAAC_FLT_C(0.27925625443458557),
    MAAC_FLT_C(0.95978713035583496), MAAC_FLT_C(0.28072887659072876),
    MAAC_FLT_C(0.95935535430908203), MAAC_FLT_C(0.28220084309577942),
    MAAC_FLT_C(0.95892131328582764), MAAC_FLT_C(0.28367212414741516),
    MAAC_FLT_C(0.95848506689071655), MAAC_FLT_C(0.28514277935028076),
    MAAC_FLT_C(0.95804649591445923), MAAC_FLT_C(0.28661271929740906),
    MAAC_FLT_C(0.95760571956634521), MAAC_FLT_C(0.28808203339576721),
    MAAC_FLT_C(0.95716267824172974), MAAC_FLT_C(0.28955063223838806),
    MAAC_FLT_C(0.95671743154525757), MAAC_FLT_C(0.29101854562759399),
    MAAC_FLT_C(0.95626986026763916), MAAC_FLT_C(0.29248580336570740),
    MAAC_FLT_C(0.95582008361816406), MAAC_FLT_C(0.29395234584808350),
    MAAC_FLT_C(0.95536804199218750), MAAC_FLT_C(0.29541820287704468),
    MAAC_FLT_C(0.95491373538970947), MAAC_FLT_C(0.29688337445259094),
    MAAC_FLT_C(0.95445722341537476), MAAC_FLT_C(0.29834786057472229),
    MAAC_FLT_C(0.95399844646453857), MAAC_FLT_C(0.29981163144111633),
    MAAC_FLT_C(0.95353740453720093), MAAC_FLT_C(0.30127468705177307),
    MAAC_FLT_C(0.95307409763336182), MAAC_FLT_C(0.30273702740669250),
    MAAC_FLT_C(0.95260858535766602), MAAC_FLT_C(0.30419868230819702),
    MAAC_FLT_C(0.95214086771011353), MAAC_FLT_C(0.30565959215164185),
    MAAC_FLT_C(0.95167088508605957), MAAC_FLT_C(0.30711981654167175),
    MAAC_FLT_C(0.95119863748550415), MAAC_FLT_C(0.30857929587364197),
    MAAC_FLT_C(0.95072412490844727), MAAC_FLT_C(0.31003805994987488),
    MAAC_FLT_C(0.95024746656417847), MAAC_FLT_C(0.31149607896804810),
    MAAC_FLT_C(0.94976848363876343), MAAC_FLT_C(0.31295338273048401),
    MAAC_FLT_C(0.94928729534149170), MAAC_FLT_C(0.31440994143486023),
    MAAC_FLT_C(0.94880390167236328), MAAC_FLT_C(0.31586575508117676),
    MAAC_FLT_C(0.94831824302673340), MAAC_FLT_C(0.31732082366943359),
    MAAC_FLT_C(0.94783037900924683), MAAC_FLT_C(0.31877514719963074),
    MAAC_FLT_C(0.94734025001525879), MAAC_FLT_C(0.32022872567176819),
    MAAC_FLT_C(0.94684791564941406), MAAC_FLT_C(0.32168155908584595),
    MAAC_FLT_C(0.94635337591171265), MAAC_FLT_C(0.32313361763954163),
    MAAC_FLT_C(0.94585657119750977), MAAC_FLT_C(0.32458493113517761),
    MAAC_FLT_C(0.94535756111145020), MAAC_FLT_C(0.32603546977043152),
    MAAC_FLT_C(0.94485628604888916), MAAC_FLT_C(0.32748523354530334),
    MAAC_FLT_C(0.94435280561447144), MAAC_FLT_C(0.32893425226211548),
    MAAC_FLT_C(0.94384711980819702), MAAC_FLT_C(0.33038249611854553),
    MAAC_FLT_C(0.94333922863006592), MAAC_FLT_C(0.33182993531227112),
    MAAC_FLT_C(0.94282907247543335), MAAC_FLT_C(0.33327659964561462),
    MAAC_FLT_C(0.94231677055358887), MAAC_FLT_C(0.33472248911857605),
    MAAC_FLT_C(0.94180220365524292), MAAC_FLT_C(0.33616760373115540),
    MAAC_FLT_C(0.94128537178039551), MAAC_FLT_C(0.33761191368103027),
    MAAC_FLT_C(0.94076639413833618), MAAC_FLT_C(0.33905541896820068),
    MAAC_FLT_C(0.94024521112442017), MAAC_FLT_C(0.34049814939498901),
    MAAC_FLT_C(0.93972176313400269), MAAC_FLT_C(0.34194007515907288),
    MAAC_FLT_C(0.93919610977172852), MAAC_FLT_C(0.34338116645812988),
    MAAC_FLT_C(0.93866831064224243), MAAC_FLT_C(0.34482148289680481),
    MAAC_FLT_C(0.93813824653625488), MAAC_FLT_C(0.34626096487045288),
    MAAC_FLT_C(0.93760597705841064), MAAC_FLT_C(0.34769964218139648),
    MAAC_FLT_C(0.93707150220870972), MAAC_FLT_C(0.34913751482963562),
    MAAC_FLT_C(0.93653482198715210), MAAC_FLT_C(0.35057455301284790),
    MAAC_FLT_C(0.93599593639373779), MAAC_FLT_C(0.35201075673103333),
    MAAC_FLT_C(0.93545484542846680), MAAC_FLT_C(0.35344615578651428),
    MAAC_FLT_C(0.93491160869598389), MAAC_FLT_C(0.35488069057464600),
    MAAC_FLT_C(0.93436610698699951), MAAC_FLT_C(0.35631442070007324),
    MAAC_FLT_C(0.93381845951080322), MAAC_FLT_C(0.35774728655815125),
    MAAC_FLT_C(0.93326854705810547), MAAC_FLT_C(0.35917934775352478),
    MAAC_FLT_C(0.93271648883819580), MAAC_FLT_C(0.36061051487922668),
    MAAC_FLT_C(0.93216222524642944), MAAC_FLT_C(0.36204087734222412),
    MAAC_FLT_C(0.93160575628280640), MAAC_FLT_C(0.36347037553787231),
    MAAC_FLT_C(0.93104708194732666), MAAC_FLT_C(0.36489900946617126),
    MAAC_FLT_C(0.93048626184463501), MAAC_FLT_C(0.36632677912712097),
    MAAC_FLT_C(0.92992323637008667), MAAC_FLT_C(0.36775368452072144),
    MAAC_FLT_C(0.92935800552368164), MAAC_FLT_C(0.36917975544929504),
    MAAC_FLT_C(0.92879062891006470), MAAC_FLT_C(0.37060493230819702),
    MAAC_FLT_C(0.92822098731994629), MAAC_FLT_C(0.37202924489974976),
    MAAC_FLT_C(0.92764925956726074), MAAC_FLT_C(0.37345266342163086),
    MAAC_FLT_C(0.92707526683807373), MAAC_FLT_C(0.37487521767616272),
    MAAC_FLT_C(0.92649912834167480), MAAC_FLT_C(0.37629690766334534),
    MAAC_FLT_C(0.92592078447341919), MAAC_FLT_C(0.37771770358085632),
    MAAC_FLT_C(0.92534029483795166), MAAC_FLT_C(0.37913760542869568),
    MAAC_FLT_C(0.92475759983062744), MAAC_FLT_C(0.38055661320686340),
    MAAC_FLT_C(0.92417275905609131), MAAC_FLT_C(0.38197472691535950),
    MAAC_FLT_C(0.92358577251434326), MAAC_FLT_C(0.38339191675186157),
    MAAC_FLT_C(0.92299652099609375), MAAC_FLT_C(0.38480824232101440),
    MAAC_FLT_C(0.92240518331527710), MAAC_FLT_C(0.38622364401817322),
    MAAC_FLT_C(0.92181164026260376), MAAC_FLT_C(0.38763815164566040),
    MAAC_FLT_C(0.92121589183807373), MAAC_FLT_C(0.38905173540115356),
    MAAC_FLT_C(0.92061805725097656), MAAC_FLT_C(0.39046439528465271),
    MAAC_FLT_C(0.92001795768737793), MAAC_FLT_C(0.39187613129615784),
    MAAC_FLT_C(0.91941577196121216), MAAC_FLT_C(0.39328697323799133),
    MAAC_FLT_C(0.91881138086318970), MAAC_FLT_C(0.39469686150550842),
    MAAC_FLT_C(0.91820484399795532), MAAC_FLT_C(0.39610585570335388),
    MAAC_FLT_C(0.91759616136550903), MAAC_FLT_C(0.39751389622688293),
    MAAC_FLT_C(0.91698527336120605), MAAC_FLT_C(0.39892101287841797),
    MAAC_FLT_C(0.91637229919433594), MAAC_FLT_C(0.40032717585563660),
    MAAC_FLT_C(0.91575711965560913), MAAC_FLT_C(0.40173238515853882),
    MAAC_FLT_C(0.91513979434967041), MAAC_FLT_C(0.40313667058944702),
    MAAC_FLT_C(0.91452032327651978), MAAC_FLT_C(0.40454000234603882),
    MAAC_FLT_C(0.91389864683151245), MAAC_FLT_C(0.40594238042831421),
    MAAC_FLT_C(0.91327488422393799), MAAC_FLT_C(0.40734380483627319),
    MAAC_FLT_C(0.91264897584915161), MAAC_FLT_C(0.40874427556991577),
    MAAC_FLT_C(0.91202086210250854), MAAC_FLT_C(0.41014379262924194),
    MAAC_FLT_C(0.91139066219329834), MAAC_FLT_C(0.41154232621192932),
    MAAC_FLT_C(0.91075825691223145), MAAC_FLT_C(0.41293987631797791),
    MAAC_FLT_C(0.91012376546859741), MAAC_FLT_C(0.41433650255203247),
    MAAC_FLT_C(0.90948712825775146), MAAC_FLT_C(0.41573211550712585),
    MAAC_FLT_C(0.90884834527969360), MAAC_FLT_C(0.41712677478790283),
    MAAC_FLT_C(0.90820735692977905), MAAC_FLT_C(0.41852042078971863),
    MAAC_FLT_C(0.90756434202194214), MAAC_FLT_C(0.41991311311721802),
    MAAC_FLT_C(0.90691912174224854), MAAC_FLT_C(0.42130479216575623),
    MAAC_FLT_C(0.90627175569534302), MAAC_FLT_C(0.42269548773765564),
    MAAC_FLT_C(0.90562230348587036), MAAC_FLT_C(0.42408519983291626),
    MAAC_FLT_C(0.90497070550918579), MAAC_FLT_C(0.42547389864921570),
    MAAC_FLT_C(0.90431696176528931), MAAC_FLT_C(0.42686161398887634),
    MAAC_FLT_C(0.90366107225418091), MAAC_FLT_C(0.42824831604957581),
    MAAC_FLT_C(0.90300309658050537), MAAC_FLT_C(0.42963400483131409),
    MAAC_FLT_C(0.90234297513961792), MAAC_FLT_C(0.43101871013641357),
    MAAC_FLT_C(0.90168076753616333), MAAC_FLT_C(0.43240237236022949),
    MAAC_FLT_C(0.90101641416549683), MAAC_FLT_C(0.43378502130508423),
    MAAC_FLT_C(0.90034991502761841), MAAC_FLT_C(0.43516665697097778),
    MAAC_FLT_C(0.89968132972717285), MAAC_FLT_C(0.43654724955558777),
    MAAC_FLT_C(0.89901059865951538), MAAC_FLT_C(0.43792682886123657),
    MAAC_FLT_C(0.89833778142929077), MAAC_FLT_C(0.43930539488792419),
    MAAC_FLT_C(0.89766281843185425), MAAC_FLT_C(0.44068288803100586),
    MAAC_FLT_C(0.89698576927185059), MAAC_FLT_C(0.44205936789512634),
    MAAC_FLT_C(0.89630663394927979), MAAC_FLT_C(0.44343480467796326),
    MAAC_FLT_C(0.89562535285949707), MAAC_FLT_C(0.44480919837951660),
    MAAC_FLT_C(0.89494198560714722), MAAC_FLT_C(0.44618254899978638),
    MAAC_FLT_C(0.89425647258758545), MAAC_FLT_C(0.44755485653877258),
    MAAC_FLT_C(0.89356887340545654), MAAC_FLT_C(0.44892609119415283),
    MAAC_FLT_C(0.89287918806076050), MAAC_FLT_C(0.45029628276824951),
    MAAC_FLT_C(0.89218741655349731), MAAC_FLT_C(0.45166543126106262),
    MAAC_FLT_C(0.89149349927902222), MAAC_FLT_C(0.45303347706794739),
    MAAC_FLT_C(0.89079749584197998), MAAC_FLT_C(0.45440047979354858),
    MAAC_FLT_C(0.89009940624237061), MAAC_FLT_C(0.45576640963554382),
    MAAC_FLT_C(0.88939923048019409), MAAC_FLT_C(0.45713126659393311),
    MAAC_FLT_C(0.88869696855545044), MAAC_FLT_C(0.45849505066871643),
    MAAC_FLT_C(0.88799256086349487), MAAC_FLT_C(0.45985776185989380),
    MAAC_FLT_C(0.88728612661361694), MAAC_FLT_C(0.46121940016746521),
    MAAC_FLT_C(0.88657760620117188), MAAC_FLT_C(0.46257993578910828),
    MAAC_FLT_C(0.88586694002151489), MAAC_FLT_C(0.46393936872482300),
    MAAC_FLT_C(0.88515424728393555), MAAC_FLT_C(0.46529772877693176),
    MAAC_FLT_C(0.88443946838378906), MAAC_FLT_C(0.46665498614311218),
    MAAC_FLT_C(0.88372254371643066), MAAC_FLT_C(0.46801114082336426),
    MAAC_FLT_C(0.88300359249114990), MAAC_FLT_C(0.46936622262001038),
    MAAC_FLT_C(0.88228255510330200), MAAC_FLT_C(0.47072017192840576),
    MAAC_FLT_C(0.88155943155288696), MAAC_FLT_C(0.47207301855087280),
    MAAC_FLT_C(0.88083428144454956), MAAC_FLT_C(0.47342476248741150),
    MAAC_FLT_C(0.88010698556900024), MAAC_FLT_C(0.47477537393569946),
    MAAC_FLT_C(0.87937766313552856), MAAC_FLT_C(0.47612488269805908),
    MAAC_FLT_C(0.87864625453948975), MAAC_FLT_C(0.47747328877449036),
    MAAC_FLT_C(0.87791281938552856), MAAC_FLT_C(0.47882056236267090),
    MAAC_FLT_C(0.87717723846435547), MAAC_FLT_C(0.48016667366027832),
    MAAC_FLT_C(0.87643969058990479), MAAC_FLT_C(0.48151168227195740),
    MAAC_FLT_C(0.87569999694824219), MAAC_FLT_C(0.48285555839538574),
    MAAC_FLT_C(0.87495827674865723), MAAC_FLT_C(0.48419830203056335),
    MAAC_FLT_C(0.87421452999114990), MAAC_FLT_C(0.48553991317749023),
    MAAC_FLT_C(0.87346869707107544), MAAC_FLT_C(0.48688036203384399),
    MAAC_FLT_C(0.87272077798843384), MAAC_FLT_C(0.48821967840194702),
    MAAC_FLT_C(0.87197083234786987), MAAC_FLT_C(0.48955783247947693),
    MAAC_FLT_C(0.87121886014938354), MAAC_FLT_C(0.49089485406875610),
    MAAC_FLT_C(0.87046480178833008), MAAC_FLT_C(0.49223071336746216),
    MAAC_FLT_C(0.86970865726470947), MAAC_FLT_C(0.49356541037559509),
    MAAC_FLT_C(0.86895054578781128), MAAC_FLT_C(0.49489894509315491),
    MAAC_FLT_C(0.86819034814834595), MAAC_FLT_C(0.49623128771781921),
    MAAC_FLT_C(0.86742812395095825), MAAC_FLT_C(0.49756249785423279),
    MAAC_FLT_C(0.86666387319564819), MAAC_FLT_C(0.49889254570007324),
    MAAC_FLT_C(0.86589753627777100), MAAC_FLT_C(0.50022137165069580),
    MAAC_FLT_C(0.86512917280197144), MAAC_FLT_C(0.50154906511306763),
    MAAC_FLT_C(0.86435878276824951), MAAC_FLT_C(0.50287556648254395),
    MAAC_FLT_C(0.86358636617660522), MAAC_FLT_C(0.50420087575912476),
    MAAC_FLT_C(0.86281192302703857), MAAC_FLT_C(0.50552505254745483),
    MAAC_FLT_C(0.86203545331954956), MAAC_FLT_C(0.50684797763824463),
    MAAC_FLT_C(0.86125695705413818), MAAC_FLT_C(0.50816971063613892),
    MAAC_FLT_C(0.86047643423080444), MAAC_FLT_C(0.50949025154113770),
    MAAC_FLT_C(0.85969388484954834), MAAC_FLT_C(0.51080960035324097),
    MAAC_FLT_C(0.85890924930572510), MAAC_FLT_C(0.51212775707244873),
    MAAC_FLT_C(0.85812264680862427), MAAC_FLT_C(0.51344472169876099),
    MAAC_FLT_C(0.85733401775360107), MAAC_FLT_C(0.51476043462753296),
    MAAC_FLT_C(0.85654342174530029), MAAC_FLT_C(0.51607501506805420),
    MAAC_FLT_C(0.85575073957443237), MAAC_FLT_C(0.51738828420639038),
    MAAC_FLT_C(0.85495609045028687), MAAC_FLT_C(0.51870042085647583),
    MAAC_FLT_C(0.85415941476821899), MAAC_FLT_C(0.52001124620437622),
    MAAC_FLT_C(0.85336071252822876), MAAC_FLT_C(0.52132093906402588),
    MAAC_FLT_C(0.85255998373031616), MAAC_FLT_C(0.52262938022613525),
    MAAC_FLT_C(0.85175728797912598), MAAC_FLT_C(0.52393656969070435),
    MAAC_FLT_C(0.85095256567001343), MAAC_FLT_C(0.52524250745773315),
    MAAC_FLT_C(0.85014587640762329), MAAC_FLT_C(0.52654725313186646),
    MAAC_FLT_C(0.84933716058731079), MAAC_FLT_C(0.52785074710845947),
    MAAC_FLT_C(0.84852647781372070), MAAC_FLT_C(0.52915298938751221),
    MAAC_FLT_C(0.84771376848220825), MAAC_FLT_C(0.53045397996902466),
    MAAC_FLT_C(0.84689903259277344), MAAC_FLT_C(0.53175371885299683),
    MAAC_FLT_C(0.84608232975006104), MAAC_FLT_C(0.53305220603942871),
    MAAC_FLT_C(0.84526365995407104), MAAC_FLT_C(0.53434944152832031),
    MAAC_FLT_C(0.84444296360015869), MAAC_FLT_C(0.53564548492431641),
    MAAC_FLT_C(0.84362030029296875), MAAC_FLT_C(0.53694015741348267),
    MAAC_FLT_C(0.84279567003250122), MAAC_FLT_C(0.53823363780975342),
    MAAC_FLT_C(0.84196901321411133), MAAC_FLT_C(0.53952586650848389),
    MAAC_FLT_C(0.84114044904708862), MAAC_FLT_C(0.54081678390502930),
    MAAC_FLT_C(0.84030985832214355), MAAC_FLT_C(0.54210644960403442),
    MAAC_FLT_C(0.83947724103927612), MAAC_FLT_C(0.54339480400085449),
    MAAC_FLT_C(0.83864271640777588), MAAC_FLT_C(0.54468190670013428),
    MAAC_FLT_C(0.83780622482299805), MAAC_FLT_C(0.54596775770187378),
    MAAC_FLT_C(0.83696770668029785), MAAC_FLT_C(0.54725229740142822),
    MAAC_FLT_C(0.83612728118896484), MAAC_FLT_C(0.54853552579879761),
    MAAC_FLT_C(0.83528482913970947), MAAC_FLT_C(0.54981750249862671),
    MAAC_FLT_C(0.83444041013717651), MAAC_FLT_C(0.55109816789627075),
    MAAC_FLT_C(0.83359408378601074), MAAC_FLT_C(0.55237752199172974),
    MAAC_FLT_C(0.83274579048156738), MAAC_FLT_C(0.55365556478500366),
    MAAC_FLT_C(0.83189547061920166), MAAC_FLT_C(0.55493235588073730),
    MAAC_FLT_C(0.83104324340820312), MAAC_FLT_C(0.55620777606964111),
    MAAC_FLT_C(0.83018904924392700), MAAC_FLT_C(0.55748194456100464),
    MAAC_FLT_C(0.82933294773101807), MAAC_FLT_C(0.55875480175018311),
    MAAC_FLT_C(0.82847481966018677), MAAC_FLT_C(0.56002628803253174),
    MAAC_FLT_C(0.82761478424072266), MAAC_FLT_C(0.56129652261734009),
    MAAC_FLT_C(0.82675278186798096), MAAC_FLT_C(0.56256538629531860),
    MAAC_FLT_C(0.82588887214660645), MAAC_FLT_C(0.56383293867111206),
    MAAC_FLT_C(0.82502299547195435), MAAC_FLT_C(0.56509917974472046),
    MAAC_FLT_C(0.82415515184402466), MAAC_FLT_C(0.56636410951614380),
    MAAC_FLT_C(0.82328540086746216), MAAC_FLT_C(0.56762766838073730),
    MAAC_FLT_C(0.82241368293762207), MAAC_FLT_C(0.56888991594314575),
    MAAC_FLT_C(0.82154005765914917), MAAC_FLT_C(0.57015079259872437),
    MAAC_FLT_C(0.82066446542739868), MAAC_FLT_C(0.57141035795211792),
    MAAC_FLT_C(0.81978696584701538), MAAC_FLT_C(0.57266855239868164),
    MAAC_FLT_C(0.81890755891799927), MAAC_FLT_C(0.57392543554306030),
    MAAC_FLT_C(0.81802618503570557), MAAC_FLT_C(0.57518094778060913),
    MAAC_FLT_C(0.81714290380477905), MAAC_FLT_C(0.57643508911132812),
    MAAC_FLT_C(0.81625771522521973), MAAC_FLT_C(0.57768791913986206),
    MAAC_FLT_C(0.81537061929702759), MAAC_FLT_C(0.57893931865692139),
    MAAC_FLT_C(0.81448155641555786), MAAC_FLT_C(0.58018940687179565),
    MAAC_FLT_C(0.81359058618545532), MAAC_FLT_C(0.58143812417984009),
    MAAC_FLT_C(0.81269776821136475), MAAC_FLT_C(0.58268547058105469),
    MAAC_FLT_C(0.81180298328399658), MAAC_FLT_C(0.58393144607543945),
    MAAC_FLT_C(0.81090623140335083), MAAC_FLT_C(0.58517605066299438),
    MAAC_FLT_C(0.81000763177871704), MAAC_FLT_C(0.58641928434371948),
    MAAC_FLT_C(0.80910712480545044), MAAC_FLT_C(0.58766114711761475),
    MAAC_FLT_C(0.80820471048355103), MAAC_FLT_C(0.58890157938003540),
    MAAC_FLT_C(0.80730044841766357), MAAC_FLT_C(0.59014070034027100),
    MAAC_FLT_C(0.80639421939849854), MAAC_FLT_C(0.59137839078903198),
    MAAC_FLT_C(0.80548608303070068), MAAC_FLT_C(0.59261465072631836),
    MAAC_FLT_C(0.80457609891891479), MAAC_FLT_C(0.59384959936141968),
    MAAC_FLT_C(0.80366420745849609), MAAC_FLT_C(0.59508305788040161),
    MAAC_FLT_C(0.80275040864944458), MAAC_FLT_C(0.59631520509719849),
    MAAC_FLT_C(0.80183470249176025), MAAC_FLT_C(0.59754586219787598),
    MAAC_FLT_C(0.80091714859008789), MAAC_FLT_C(0.59877520799636841),
    MAAC_FLT_C(0.79999768733978271), MAAC_FLT_C(0.60000306367874146),
    MAAC_FLT_C(0.79907637834548950), MAAC_FLT_C(0.60122954845428467),
    MAAC_FLT_C(0.79815316200256348), MAAC_FLT_C(0.60245460271835327),
    MAAC_FLT_C(0.79722803831100464), MAAC_FLT_C(0.60367822647094727),
    MAAC_FLT_C(0.79630106687545776), MAAC_FLT_C(0.60490047931671143),
    MAAC_FLT_C(0.79537224769592285), MAAC_FLT_C(0.60612124204635620),
    MAAC_FLT_C(0.79444152116775513), MAAC_FLT_C(0.60734063386917114),
    MAAC_FLT_C(0.79350894689559937), MAAC_FLT_C(0.60855859518051147),
    MAAC_FLT_C(0.79257452487945557), MAAC_FLT_C(0.60977506637573242),
    MAAC_FLT_C(0.79163819551467896), MAAC_FLT_C(0.61099016666412354),
    MAAC_FLT_C(0.79070001840591431), MAAC_FLT_C(0.61220377683639526),
    MAAC_FLT_C(0.78975999355316162), MAAC_FLT_C(0.61341601610183716),
    MAAC_FLT_C(0.78881806135177612), MAAC_FLT_C(0.61462676525115967),
    MAAC_FLT_C(0.78787434101104736), MAAC_FLT_C(0.61583608388900757),
    MAAC_FLT_C(0.78692871332168579), MAAC_FLT_C(0.61704391241073608),
    MAAC_FLT_C(0.78598123788833618), MAAC_FLT_C(0.61825031042098999),
    MAAC_FLT_C(0.78503191471099854), MAAC_FLT_C(0.61945527791976929),
    MAAC_FLT_C(0.78408080339431763), MAAC_FLT_C(0.62065875530242920),
    MAAC_FLT_C(0.78312778472900391), MAAC_FLT_C(0.62186080217361450),
    MAAC_FLT_C(0.78217291831970215), MAAC_FLT_C(0.62306135892868042),
    MAAC_FLT_C(0.78121626377105713), MAAC_FLT_C(0.62426048517227173),
    MAAC_FLT_C(0.78025776147842407), MAAC_FLT_C(0.62545812129974365),
    MAAC_FLT_C(0.77929735183715820), MAAC_FLT_C(0.62665426731109619),
    MAAC_FLT_C(0.77833521366119385), MAAC_FLT_C(0.62784898281097412),
    MAAC_FLT_C(0.77737116813659668), MAAC_FLT_C(0.62904220819473267),
    MAAC_FLT_C(0.77640533447265625), MAAC_FLT_C(0.63023394346237183),
    MAAC_FLT_C(0.77543765306472778), MAAC_FLT_C(0.63142418861389160),
    MAAC_FLT_C(0.77446812391281128), MAAC_FLT_C(0.63261294364929199),
    MAAC_FLT_C(0.77349680662155151), MAAC_FLT_C(0.63380020856857300),
    MAAC_FLT_C(0.77252364158630371), MAAC_FLT_C(0.63498598337173462),
    MAAC_FLT_C(0.77154868841171265), MAAC_FLT_C(0.63617026805877686),
    MAAC_FLT_C(0.77057188749313354), MAAC_FLT_C(0.63735306262969971),
    MAAC_FLT_C(0.76959329843521118), MAAC_FLT_C(0.63853436708450317),
    MAAC_FLT_C(0.76861292123794556), MAAC_FLT_C(0.63971418142318726),
    MAAC_FLT_C(0.76763069629669189), MAAC_FLT_C(0.64089244604110718),
    MAAC_FLT_C(0.76664668321609497), MAAC_FLT_C(0.64206922054290771),
    MAAC_FLT_C(0.76566088199615479), MAAC_FLT_C(0.64324450492858887),
    MAAC_FLT_C(0.76467323303222656), MAAC_FLT_C(0.64441823959350586),
    MAAC_FLT_C(0.76368379592895508), MAAC_FLT_C(0.64559048414230347),
    MAAC_FLT_C(0.76269257068634033), MAAC_FLT_C(0.64676117897033691),
    MAAC_FLT_C(0.76169955730438232), MAAC_FLT_C(0.64793038368225098),
    MAAC_FLT_C(0.76070475578308105), MAAC_FLT_C(0.64909803867340088),
    MAAC_FLT_C(0.75970816612243652), MAAC_FLT_C(0.65026420354843140),
    MAAC_FLT_C(0.75870978832244873), MAAC_FLT_C(0.65142881870269775),
    MAAC_FLT_C(0.75770962238311768), MAAC_FLT_C(0.65259188413619995),
    MAAC_FLT_C(0.75670766830444336), MAAC_FLT_C(0.65375339984893799),
    MAAC_FLT_C(0.75570392608642578), MAAC_FLT_C(0.65491342544555664),
    MAAC_FLT_C(0.75469839572906494), MAAC_FLT_C(0.65607190132141113),
    MAAC_FLT_C(0.75369113683700562), MAAC_FLT_C(0.65722882747650146),
    MAAC_FLT_C(0.75268203020095825), MAAC_FLT_C(0.65838420391082764),
    MAAC_FLT_C(0.75167119503021240), MAAC_FLT_C(0.65953803062438965),
    MAAC_FLT_C(0.75065863132476807), MAAC_FLT_C(0.66069030761718750),
    MAAC_FLT_C(0.74964421987533569), MAAC_FLT_C(0.66184097528457642),
    MAAC_FLT_C(0.74862807989120483), MAAC_FLT_C(0.66299015283584595),
    MAAC_FLT_C(0.74761021137237549), MAAC_FLT_C(0.66413778066635132),
    MAAC_FLT_C(0.74659055471420288), MAAC_FLT_C(0.66528379917144775),
    MAAC_FLT_C(0.74556916952133179), MAAC_FLT_C(0.66642826795578003),
    MAAC_FLT_C(0.74454599618911743), MAAC_FLT_C(0.66757118701934814),
    MAAC_FLT_C(0.74352109432220459), MAAC_FLT_C(0.66871249675750732),
    MAAC_FLT_C(0.74249440431594849), MAAC_FLT_C(0.66985225677490234),
    MAAC_FLT_C(0.74146598577499390), MAAC_FLT_C(0.67099046707153320),
    MAAC_FLT_C(0.74043583869934082), MAAC_FLT_C(0.67212706804275513),
    MAAC_FLT_C(0.73940390348434448), MAAC_FLT_C(0.67326205968856812),
    MAAC_FLT_C(0.73837029933929443), MAAC_FLT_C(0.67439550161361694),
    MAAC_FLT_C(0.73733490705490112), MAAC_FLT_C(0.67552739381790161),
    MAAC_FLT_C(0.73629778623580933), MAAC_FLT_C(0.67665761709213257),
    MAAC_FLT_C(0.73525893688201904), MAAC_FLT_C(0.67778629064559937),
    MAAC_FLT_C(0.73421835899353027), MAAC_FLT_C(0.67891335487365723),
    MAAC_FLT_C(0.73317605257034302), MAAC_FLT_C(0.68003886938095093),
    MAAC_FLT_C(0.73213201761245728), MAAC_FLT_C(0.68116271495819092),
    MAAC_FLT_C(0.73108631372451782), MAAC_FLT_C(0.68228501081466675),
    MAAC_FLT_C(0.73003882169723511), MAAC_FLT_C(0.68340569734573364),
    MAAC_FLT_C(0.72898960113525391), MAAC_FLT_C(0.68452471494674683),
    MAAC_FLT_C(0.72793871164321899), MAAC_FLT_C(0.68564218282699585),
    MAAC_FLT_C(0.72688609361648560), MAAC_FLT_C(0.68675804138183594),
    MAAC_FLT_C(0.72583180665969849), MAAC_FLT_C(0.68787223100662231),
    MAAC_FLT_C(0.72477573156356812), MAAC_FLT_C(0.68898487091064453),
    MAAC_FLT_C(0.72371798753738403), MAAC_FLT_C(0.69009584188461304),
    MAAC_FLT_C(0.72265857458114624), MAAC_FLT_C(0.69120520353317261),
    MAAC_FLT_C(0.72159743309020996), MAAC_FLT_C(0.69231289625167847),
    MAAC_FLT_C(0.72053456306457520), MAAC_FLT_C(0.69341903924942017),
    MAAC_FLT_C(0.71947002410888672), MAAC_FLT_C(0.69452351331710815),
    MAAC_FLT_C(0.71840381622314453), MAAC_FLT_C(0.69562631845474243),
    MAAC_FLT_C(0.71733587980270386), MAAC_FLT_C(0.69672751426696777),
    MAAC_FLT_C(0.71626627445220947), MAAC_FLT_C(0.69782710075378418),
    MAAC_FLT_C(0.71519494056701660), MAAC_FLT_C(0.69892501831054688),
    MAAC_FLT_C(0.71412199735641479), MAAC_FLT_C(0.70002126693725586),
    MAAC_FLT_C(0.71304732561111450), MAAC_FLT_C(0.70111590623855591),
    MAAC_FLT_C(0.71197098493576050), MAAC_FLT_C(0.70220887660980225),
    MAAC_FLT_C(0.71089297533035278), MAAC_FLT_C(0.70330017805099487),
    MAAC_FLT_C(0.70981329679489136), MAAC_FLT_C(0.70438987016677856),
    MAAC_FLT_C(0.70873194932937622), MAAC_FLT_C(0.70547789335250854),
    MAAC_FLT_C(0.70764893293380737), MAAC_FLT_C(0.70656424760818481)
};

static const maac_flt MAAC_IMDCT_C_2048[512] = {
    MAAC_FLT_C(0.99999529123306274), MAAC_FLT_C(-0.0030679567717015743),
    MAAC_FLT_C(0.99995762109756470), MAAC_FLT_C(-0.0092037543654441833),
    MAAC_FLT_C(0.99988234043121338), MAAC_FLT_C(-0.015339205972850323),
    MAAC_FLT_C(0.99976938962936401), MAAC_FLT_C(-0.021474080160260201),
    MAAC_FLT_C(0.99961882829666138), MAAC_FLT_C(-0.027608145028352737),
    MAAC_FLT_C(0.99943059682846069), MAAC_FLT_C(-0.033741172403097153),
    MAAC_FLT_C(0.99920475482940674), MAAC_FLT_C(-0.039872925728559494),
    MAAC_FLT_C(0.99894130229949951), MAAC_FLT_C(-0.046003181487321854),
    MAAC_FLT_C(0.99864023923873901), MAAC_FLT_C(-0.052131704986095428),
    MAAC_FLT_C(0.99830156564712524), MAAC_FLT_C(-0.058258265256881714),
    MAAC_FLT_C(0.99792528152465820), MAAC_FLT_C(-0.064382627606391907),
    MAAC_FLT_C(0.99751144647598267), MAAC_FLT_C(-0.070504575967788696),
    MAAC_FLT_C(0.99706006050109863), MAAC_FLT_C(-0.076623864471912384),
    MAAC_FLT_C(0.99657112360000610), MAAC_FLT_C(-0.082740262150764465),
    MAAC_FLT_C(0.99604469537734985), MAAC_FLT_C(-0.088853552937507629),
    MAAC_FLT_C(0.99548077583312988), MAAC_FLT_C(-0.094963498413562775),
    MAAC_FLT_C(0.99487930536270142), MAAC_FLT_C(-0.10106986016035080),
    MAAC_FLT_C(0.99424046277999878), MAAC_FLT_C(-0.10717242211103439),
    MAAC_FLT_C(0.99356412887573242), MAAC_FLT_C(-0.11327095329761505),
    MAAC_FLT_C(0.99285042285919189), MAAC_FLT_C(-0.11936521530151367),
    MAAC_FLT_C(0.99209928512573242), MAAC_FLT_C(-0.12545497715473175),
    MAAC_FLT_C(0.99131083488464355), MAAC_FLT_C(-0.13154003024101257),
    MAAC_FLT_C(0.99048507213592529), MAAC_FLT_C(-0.13762012124061584),
    MAAC_FLT_C(0.98962199687957764), MAAC_FLT_C(-0.14369502663612366),
    MAAC_FLT_C(0.98872166872024536), MAAC_FLT_C(-0.14976453781127930),
    MAAC_FLT_C(0.98778414726257324), MAAC_FLT_C(-0.15582840144634247),
    MAAC_FLT_C(0.98680937290191650), MAAC_FLT_C(-0.16188639402389526),
    MAAC_FLT_C(0.98579752445220947), MAAC_FLT_C(-0.16793829202651978),
    MAAC_FLT_C(0.98474848270416260), MAAC_FLT_C(-0.17398387193679810),
    MAAC_FLT_C(0.98366242647171021), MAAC_FLT_C(-0.18002289533615112),
    MAAC_FLT_C(0.98253929615020752), MAAC_FLT_C(-0.18605515360832214),
    MAAC_FLT_C(0.98137921094894409), MAAC_FLT_C(-0.19208039343357086),
    MAAC_FLT_C(0.98018211126327515), MAAC_FLT_C(-0.19809840619564056),
    MAAC_FLT_C(0.97894817590713501), MAAC_FLT_C(-0.20410896837711334),
    MAAC_FLT_C(0.97767734527587891), MAAC_FLT_C(-0.21011184155941010),
    MAAC_FLT_C(0.97636973857879639), MAAC_FLT_C(-0.21610680222511292),
    MAAC_FLT_C(0.97502535581588745), MAAC_FLT_C(-0.22209362685680389),
    MAAC_FLT_C(0.97364425659179688), MAAC_FLT_C(-0.22807207703590393),
    MAAC_FLT_C(0.97222650051116943), MAAC_FLT_C(-0.23404195904731750),
    MAAC_FLT_C(0.97077214717864990), MAAC_FLT_C(-0.24000301957130432),
    MAAC_FLT_C(0.96928125619888306), MAAC_FLT_C(-0.24595504999160767),
    MAAC_FLT_C(0.96775382757186890), MAAC_FLT_C(-0.25189781188964844),
    MAAC_FLT_C(0.96618998050689697), MAAC_FLT_C(-0.25783109664916992),
    MAAC_FLT_C(0.96458977460861206), MAAC_FLT_C(-0.26375466585159302),
    MAAC_FLT_C(0.96295326948165894), MAAC_FLT_C(-0.26966831088066101),
    MAAC_FLT_C(0.96128046512603760), MAAC_FLT_C(-0.27557182312011719),
    MAAC_FLT_C(0.95957154035568237), MAAC_FLT_C(-0.28146493434906006),
    MAAC_FLT_C(0.95782643556594849), MAAC_FLT_C(-0.28734746575355530),
    MAAC_FLT_C(0.95604526996612549), MAAC_FLT_C(-0.29321914911270142),
    MAAC_FLT_C(0.95422810316085815), MAAC_FLT_C(-0.29907983541488647),
    MAAC_FLT_C(0.95237499475479126), MAAC_FLT_C(-0.30492922663688660),
    MAAC_FLT_C(0.95048606395721436), MAAC_FLT_C(-0.31076714396476746),
    MAAC_FLT_C(0.94856137037277222), MAAC_FLT_C(-0.31659337878227234),
    MAAC_FLT_C(0.94660091400146484), MAAC_FLT_C(-0.32240769267082214),
    MAAC_FLT_C(0.94460481405258179), MAAC_FLT_C(-0.32820984721183777),
    MAAC_FLT_C(0.94257318973541260), MAAC_FLT_C(-0.33399966359138489),
    MAAC_FLT_C(0.94050604104995728), MAAC_FLT_C(-0.33977687358856201),
    MAAC_FLT_C(0.93840354681015015), MAAC_FLT_C(-0.34554132819175720),
    MAAC_FLT_C(0.93626564741134644), MAAC_FLT_C(-0.35129275918006897),
    MAAC_FLT_C(0.93409252166748047), MAAC_FLT_C(-0.35703095793724060),
    MAAC_FLT_C(0.93188428878784180), MAAC_FLT_C(-0.36275571584701538),
    MAAC_FLT_C(0.92964088916778564), MAAC_FLT_C(-0.36846682429313660),
    MAAC_FLT_C(0.92736250162124634), MAAC_FLT_C(-0.37416407465934753),
    MAAC_FLT_C(0.92504924535751343), MAAC_FLT_C(-0.37984719872474670),
    MAAC_FLT_C(0.92270112037658691), MAAC_FLT_C(-0.38551604747772217),
    MAAC_FLT_C(0.92031830549240112), MAAC_FLT_C(-0.39117038249969482),
    MAAC_FLT_C(0.91790080070495605), MAAC_FLT_C(-0.39680999517440796),
    MAAC_FLT_C(0.91544872522354126), MAAC_FLT_C(-0.40243464708328247),
    MAAC_FLT_C(0.91296219825744629), MAAC_FLT_C(-0.40804415941238403),
    MAAC_FLT_C(0.91044127941131592), MAAC_FLT_C(-0.41363832354545593),
    MAAC_FLT_C(0.90788608789443970), MAAC_FLT_C(-0.41921690106391907),
    MAAC_FLT_C(0.90529674291610718), MAAC_FLT_C(-0.42477968335151672),
    MAAC_FLT_C(0.90267330408096313), MAAC_FLT_C(-0.43032649159431458),
    MAAC_FLT_C(0.90001589059829712), MAAC_FLT_C(-0.43585708737373352),
    MAAC_FLT_C(0.89732456207275391), MAAC_FLT_C(-0.44137126207351685),
    MAAC_FLT_C(0.89459949731826782), MAAC_FLT_C(-0.44686883687973022),
    MAAC_FLT_C(0.89184069633483887), MAAC_FLT_C(-0.45234957337379456),
    MAAC_FLT_C(0.88904833793640137), MAAC_FLT_C(-0.45781329274177551),
    MAAC_FLT_C(0.88622254133224487), MAAC_FLT_C(-0.46325978636741638),
    MAAC_FLT_C(0.88336336612701416), MAAC_FLT_C(-0.46868881583213806),
    MAAC_FLT_C(0.88047087192535400), MAAC_FLT_C(-0.47410020232200623),
    MAAC_FLT_C(0.87754529714584351), MAAC_FLT_C(-0.47949376702308655),
    MAAC_FLT_C(0.87458664178848267), MAAC_FLT_C(-0.48486924171447754),
    MAAC_FLT_C(0.87159508466720581), MAAC_FLT_C(-0.49022647738456726),
    MAAC_FLT_C(0.86857068538665771), MAAC_FLT_C(-0.49556526541709900),
    MAAC_FLT_C(0.86551362276077271), MAAC_FLT_C(-0.50088536739349365),
    MAAC_FLT_C(0.86242395639419556), MAAC_FLT_C(-0.50618666410446167),
    MAAC_FLT_C(0.85930180549621582), MAAC_FLT_C(-0.51146882772445679),
    MAAC_FLT_C(0.85614734888076782), MAAC_FLT_C(-0.51673179864883423),
    MAAC_FLT_C(0.85296058654785156), MAAC_FLT_C(-0.52197527885437012),
    MAAC_FLT_C(0.84974175691604614), MAAC_FLT_C(-0.52719914913177490),
    MAAC_FLT_C(0.84649091958999634), MAAC_FLT_C(-0.53240311145782471),
    MAAC_FLT_C(0.84320825338363647), MAAC_FLT_C(-0.53758704662322998),
    MAAC_FLT_C(0.83989381790161133), MAAC_FLT_C(-0.54275077581405640),
    MAAC_FLT_C(0.83654773235321045), MAAC_FLT_C(-0.54789406061172485),
    MAAC_FLT_C(0.83317017555236816), MAAC_FLT_C(-0.55301672220230103),
    MAAC_FLT_C(0.82976120710372925), MAAC_FLT_C(-0.55811852216720581),
    MAAC_FLT_C(0.82632106542587280), MAAC_FLT_C(-0.56319934129714966),
    MAAC_FLT_C(0.82284981012344360), MAAC_FLT_C(-0.56825894117355347),
    MAAC_FLT_C(0.81934750080108643), MAAC_FLT_C(-0.57329714298248291),
    MAAC_FLT_C(0.81581443548202515), MAAC_FLT_C(-0.57831376791000366),
    MAAC_FLT_C(0.81225061416625977), MAAC_FLT_C(-0.58330863714218140),
    MAAC_FLT_C(0.80865615606307983), MAAC_FLT_C(-0.58828157186508179),
    MAAC_FLT_C(0.80503135919570923), MAAC_FLT_C(-0.59323227405548096),
    MAAC_FLT_C(0.80137616395950317), MAAC_FLT_C(-0.59816068410873413),
    MAAC_FLT_C(0.79769086837768555), MAAC_FLT_C(-0.60306662321090698),
    MAAC_FLT_C(0.79397547245025635), MAAC_FLT_C(-0.60794979333877563),
    MAAC_FLT_C(0.79023021459579468), MAAC_FLT_C(-0.61281007528305054),
    MAAC_FLT_C(0.78645521402359009), MAAC_FLT_C(-0.61764729022979736),
    MAAC_FLT_C(0.78265058994293213), MAAC_FLT_C(-0.62246125936508179),
    MAAC_FLT_C(0.77881652116775513), MAAC_FLT_C(-0.62725180387496948),
    MAAC_FLT_C(0.77495312690734863), MAAC_FLT_C(-0.63201874494552612),
    MAAC_FLT_C(0.77106052637100220), MAAC_FLT_C(-0.63676184415817261),
    MAAC_FLT_C(0.76713889837265015), MAAC_FLT_C(-0.64148104190826416),
    MAAC_FLT_C(0.76318842172622681), MAAC_FLT_C(-0.64617604017257690),
    MAAC_FLT_C(0.75920921564102173), MAAC_FLT_C(-0.65084666013717651),
    MAAC_FLT_C(0.75520139932632446), MAAC_FLT_C(-0.65549284219741821),
    MAAC_FLT_C(0.75116515159606934), MAAC_FLT_C(-0.66011434793472290),
    MAAC_FLT_C(0.74710059165954590), MAAC_FLT_C(-0.66471099853515625),
    MAAC_FLT_C(0.74300795793533325), MAAC_FLT_C(-0.66928261518478394),
    MAAC_FLT_C(0.73888731002807617), MAAC_FLT_C(-0.67382901906967163),
    MAAC_FLT_C(0.73473888635635376), MAAC_FLT_C(-0.67835003137588501),
    MAAC_FLT_C(0.73056274652481079), MAAC_FLT_C(-0.68284553289413452),
    MAAC_FLT_C(0.72635912895202637), MAAC_FLT_C(-0.68731534481048584),
    MAAC_FLT_C(0.72212821245193481), MAAC_FLT_C(-0.69175922870635986),
    MAAC_FLT_C(0.71787005662918091), MAAC_FLT_C(-0.69617712497711182),
    MAAC_FLT_C(0.71358484029769897), MAAC_FLT_C(-0.70056879520416260),
    MAAC_FLT_C(0.70927280187606812), MAAC_FLT_C(-0.70493406057357788),
    MAAC_FLT_C(0.70493406057357788), MAAC_FLT_C(-0.70927280187606812),
    MAAC_FLT_C(0.70056879520416260), MAAC_FLT_C(-0.71358484029769897),
    MAAC_FLT_C(0.69617712497711182), MAAC_FLT_C(-0.71787005662918091),
    MAAC_FLT_C(0.69175922870635986), MAAC_FLT_C(-0.72212821245193481),
    MAAC_FLT_C(0.68731534481048584), MAAC_FLT_C(-0.72635912895202637),
    MAAC_FLT_C(0.68284553289413452), MAAC_FLT_C(-0.73056274652481079),
    MAAC_FLT_C(0.67835003137588501), MAAC_FLT_C(-0.73473888635635376),
    MAAC_FLT_C(0.67382901906967163), MAAC_FLT_C(-0.73888731002807617),
    MAAC_FLT_C(0.66928261518478394), MAAC_FLT_C(-0.74300795793533325),
    MAAC_FLT_C(0.66471099853515625), MAAC_FLT_C(-0.74710059165954590),
    MAAC_FLT_C(0.66011434793472290), MAAC_FLT_C(-0.75116515159606934),
    MAAC_FLT_C(0.65549284219741821), MAAC_FLT_C(-0.75520139932632446),
    MAAC_FLT_C(0.65084666013717651), MAAC_FLT_C(-0.75920921564102173),
    MAAC_FLT_C(0.64617604017257690), MAAC_FLT_C(-0.76318842172622681),
    MAAC_FLT_C(0.64148104190826416), MAAC_FLT_C(-0.76713889837265015),
    MAAC_FLT_C(0.63676184415817261), MAAC_FLT_C(-0.77106052637100220),
    MAAC_FLT_C(0.63201874494552612), MAAC_FLT_C(-0.77495312690734863),
    MAAC_FLT_C(0.62725180387496948), MAAC_FLT_C(-0.77881652116775513),
    MAAC_FLT_C(0.62246125936508179), MAAC_FLT_C(-0.78265058994293213),
    MAAC_FLT_C(0.61764729022979736), MAAC_FLT_C(-0.78645521402359009),
    MAAC_FLT_C(0.61281007528305054), MAAC_FLT_C(-0.79023021459579468),
    MAAC_FLT_C(0.60794979333877563), MAAC_FLT_C(-0.79397547245025635),
    MAAC_FLT_C(0.60306662321090698), MAAC_FLT_C(-0.79769086837768555),
    MAAC_FLT_C(0.59816068410873413), MAAC_FLT_C(-0.80137616395950317),
    MAAC_FLT_C(0.59323227405548096), MAAC_FLT_C(-0.80503135919570923),
    MAAC_FLT_C(0.58828157186508179), MAAC_FLT_C(-0.80865615606307983),
    MAAC_FLT_C(0.58330863714218140), MAAC_FLT_C(-0.81225061416625977),
    MAAC_FLT_C(0.57831376791000366), MAAC_FLT_C(-0.81581443548202515),
    MAAC_FLT_C(0.57329714298248291), MAAC_FLT_C(-0.81934750080108643),
    MAAC_FLT_C(0.56825894117355347), MAAC_FLT_C(-0.82284981012344360),
    MAAC_FLT_C(0.56319934129714966), MAAC_FLT_C(-0.82632106542587280),
    MAAC_FLT_C(0.55811852216720581), MAAC_FLT_C(-0.82976120710372925),
    MAAC_FLT_C(0.55301672220230103), MAAC_FLT_C(-0.83317017555236816),
    MAAC_FLT_C(0.54789406061172485), MAAC_FLT_C(-0.83654773235321045),
    MAAC_FLT_C(0.54275077581405640), MAAC_FLT_C(-0.83989381790161133),
    MAAC_FLT_C(0.53758704662322998), MAAC_FLT_C(-0.84320825338363647),
    MAAC_FLT_C(0.53240311145782471), MAAC_FLT_C(-0.84649091958999634),
    MAAC_FLT_C(0.52719914913177490), MAAC_FLT_C(-0.84974175691604614),
    MAAC_FLT_C(0.52197527885437012), MAAC_FLT_C(-0.85296058654785156),
    MAAC_FLT_C(0.51673179864883423), MAAC_FLT_C(-0.85614734888076782),
    MAAC_FLT_C(0.51146882772445679), MAAC_FLT_C(-0.85930180549621582),
    MAAC_FLT_C(0.50618666410446167), MAAC_FLT_C(-0.86242395639419556),
    MAAC_FLT_C(0.50088536739349365), MAAC_FLT_C(-0.86551362276077271),
    MAAC_FLT_C(0.49556526541709900), MAAC_FLT_C(-0.86857068538665771),
    MAAC_FLT_C(0.49022647738456726), MAAC_FLT_C(-0.87159508466720581),
    MAAC_FLT_C(0.48486924171447754), MAAC_FLT_C(-0.87458664178848267),
    MAAC_FLT_C(0.47949376702308655), MAAC_FLT_C(-0.87754529714584351),
    MAAC_FLT_C(0.47410020232200623), MAAC_FLT_C(-0.88047087192535400),
    MAAC_FLT_C(0.46868881583213806), MAAC_FLT_C(-0.88336336612701416),
    MAAC_FLT_C(0.46325978636741638), MAAC_FLT_C(-0.88622254133224487),
    MAAC_FLT_C(0.45781329274177551), MAAC_FLT_C(-0.88904833793640137),
    MAAC_FLT_C(0.45234957337379456), MAAC_FLT_C(-0.89184069633483887),
    MAAC_FLT_C(0.44686883687973022), MAAC_FLT_C(-0.89459949731826782),
    MAAC_FLT_C(0.44137126207351685), MAAC_FLT_C(-0.89732456207275391),
    MAAC_FLT_C(0.43585708737373352), MAAC_FLT_C(-0.90001589059829712),
    MAAC_FLT_C(0.43032649159431458), MAAC_FLT_C(-0.90267330408096313),
    MAAC_FLT_C(0.42477968335151672), MAAC_FLT_C(-0.90529674291610718),
    MAAC_FLT_C(0.41921690106391907), MAAC_FLT_C(-0.90788608789443970),
    MAAC_FLT_C(0.41363832354545593), MAAC_FLT_C(-0.91044127941131592),
    MAAC_FLT_C(0.40804415941238403), MAAC_FLT_C(-0.91296219825744629),
    MAAC_FLT_C(0.40243464708328247), MAAC_FLT_C(-0.91544872522354126),
    MAAC_FLT_C(0.39680999517440796), MAAC_FLT_C(-0.91790080070495605),
    MAAC_FLT_C(0.39117038249969482), MAAC_FLT_C(-0.92031830549240112),
    MAAC_FLT_C(0.38551604747772217), MAAC_FLT_C(-0.92270112037658691),
    MAAC_FLT_C(0.37984719872474670), MAAC_FLT_C(-0.92504924535751343),
    MAAC_FLT_C(0.37416407465934753), MAAC_FLT_C(-0.92736250162124634),
    MAAC_FLT_C(0.36846682429313660), MAAC_FLT_C(-0.92964088916778564),
    MAAC_FLT_C(0.36275571584701538), MAAC_FLT_C(-0.93188428878784180),
    MAAC_FLT_C(0.35703095793724060), MAAC_FLT_C(-0.93409252166748047),
    MAAC_FLT_C(0.35129275918006897), MAAC_FLT_C(-0.93626564741134644),
    MAAC_FLT_C(0.34554132819175720), MAAC_FLT_C(-0.93840354681015015),
    MAAC_FLT_C(0.33977687358856201), MAAC_FLT_C(-0.94050604104995728),
    MAAC_FLT_C(0.33399966359138489), MAAC_FLT_C(-0.94257318973541260),
    MAAC_FLT_C(0.32820984721183777), MAAC_FLT_C(-0.94460481405258179),
    MAAC_FLT_C(0.32240769267082214), MAAC_FLT_C(-0.94660091400146484),
    MAAC_FLT_C(0.31659337878227234), MAAC_FLT_C(-0.94856137037277222),
    MAAC_FLT_C(0.31076714396476746), MAAC_FLT_C(-0.95048606395721436),
    MAAC_FLT_C(0.30492922663688660), MAAC_FLT_C(-0.95237499475479126),
    MAAC_FLT_C(0.29907983541488647), MAAC_FLT_C(-0.95422810316085815),
    MAAC_FLT_C(0.29321914911270142), MAAC_FLT_C(-0.95604526996612549),
    MAAC_FLT_C(0.28734746575355530), MAAC_FLT_C(-0.95782643556594849),
    MAAC_FLT_C(0.28146493434906006), MAAC_FLT_C(-0.95957154035568237),
    MAAC_FLT_C(0.27557182312011719), MAAC_FLT_C(-0.96128046512603760),
    MAAC_FLT_C(0.26966831088066101), MAAC_FLT_C(-0.96295326948165894),
    MAAC_FLT_C(0.26375466585159302), MAAC_FLT_C(-0.96458977460861206),
    MAAC_FLT_C(0.25783109664916992), MAAC_FLT_C(-0.96618998050689697),
    MAAC_FLT_C(0.25189781188964844), MAAC_FLT_C(-0.96775382757186890),
    MAAC_FLT_C(0.24595504999160767), MAAC_FLT_C(-0.96928125619888306),
    MAAC_FLT_C(0.24000301957130432), MAAC_FLT_C(-0.97077214717864990),
    MAAC_FLT_C(0.23404195904731750), MAAC_FLT_C(-0.97222650051116943),
    MAAC_FLT_C(0.22807207703590393), MAAC_FLT_C(-0.97364425659179688),
    MAAC_FLT_C(0.22209362685680389), MAAC_FLT_C(-0.97502535581588745),
    MAAC_FLT_C(0.21610680222511292), MAAC_FLT_C(-0.97636973857879639),
    MAAC_FLT_C(0.21011184155941010), MAAC_FLT_C(-0.97767734527587891),
    MAAC_FLT_C(0.20410896837711334), MAAC_FLT_C(-0.97894817590713501),
    MAAC_FLT_C(0.19809840619564056), MAAC_FLT_C(-0.98018211126327515),
    MAAC_FLT_C(0.19208039343357086), MAAC_FLT_C(-0.98137921094894409),
    MAAC_FLT_C(0.18605515360832214), MAAC_FLT_C(-0.98253929615020752),
    MAAC_FLT_C(0.18002289533615112), MAAC_FLT_C(-0.98366242647171021),
    MAAC_FLT_C(0.17398387193679810), MAAC_FLT_C(-0.98474848270416260),
    MAAC_FLT_C(0.16793829202651978), MAAC_FLT_C(-0.98579752445220947),
    MAAC_FLT_C(0.16188639402389526), MAAC_FLT_C(-0.98680937290191650),
    MAAC_FLT_C(0.15582840144634247), MAAC_FLT_C(-0.98778414726257324),
    MAAC_FLT_C(0.14976453781127930), MAAC_FLT_C(-0.98872166872024536),
    MAAC_FLT_C(0.14369502663612366), MAAC_FLT_C(-0.98962199687957764),
    MAAC_FLT_C(0.13762012124061584), MAAC_FLT_C(-0.99048507213592529),
    MAAC_FLT_C(0.13154003024101257), MAAC_FLT_C(-0.99131083488464355),
    MAAC_FLT_C(0.12545497715473175), MAAC_FLT_C(-0.99209928512573242),
    MAAC_FLT_C(0.11936521530151367), MAAC_FLT_C(-0.99285042285919189),
    MAAC_FLT_C(0.11327095329761505), MAAC_FLT_C(-0.99356412887573242),
    MAAC_FLT_C(0.10717242211103439), MAAC_FLT_C(-0.99424046277999878),
    MAAC_FLT_C(0.10106986016035080), MAAC_FLT_C(-0.99487930536270142),
    MAAC_FLT_C(0.094963498413562775), MAAC_FLT_C(-0.99548077583312988),
    MAAC_FLT_C(0.088853552937507629), MAAC_FLT_C(-0.99604469537734985),
    MAAC_FLT_C(0.082740262150764465), MAAC_FLT_C(-0.99657112360000610),
    MAAC_FLT_C(0.076623864471912384), MAAC_FLT_C(-0.99706006050109863),
    MAAC_FLT_C(0.070504575967788696), MAAC_FLT_C(-0.99751144647598267),
    MAAC_FLT_C(0.064382627606391907), MAAC_FLT_C(-0.99792528152465820),
    MAAC_FLT_C(0.058258265256881714), MAAC_FLT_C(-0.99830156564712524),
    MAAC_FLT_C(0.052131704986095428), MAAC_FLT_C(-0.99864023923873901),
    MAAC_FLT_C(0.046003181487321854), MAAC_FLT_C(-0.99894130229949951),
    MAAC_FLT_C(0.039872925728559494), MAAC_FLT_C(-0.99920475482940674),
    MAAC_FLT_C(0.033741172403097153), MAAC_FLT_C(-0.99943059682846069),
    MAAC_FLT_C(0.027608145028352737), MAAC_FLT_C(-0.99961882829666138),
    MAAC_FLT_C(0.021474080160260201), MAAC_FLT_C(-0.99976938962936401),
    MAAC_FLT_C(0.015339205972850323), MAAC_FLT_C(-0.99988234043121338),
    MAAC_FLT_C(0.0092037543654441833), MAAC_FLT_C(-0.99995762109756470),
    MAAC_FLT_C(0.0030679567717015743), MAAC_FLT_C(-0.99999529123306274)
};

static const maac_flt MAAC_IMDCT_A_256[128] = {
    MAAC_FLT_C(1.0000000000000000), MAAC_FLT_C(-0.0000000000000000),
    MAAC_FLT_C(0.99879544973373413), MAAC_FLT_C(-0.049067676067352295),
    MAAC_FLT_C(0.99518471956253052), MAAC_FLT_C(-0.098017141222953796),
    MAAC_FLT_C(0.98917651176452637), MAAC_FLT_C(-0.14673046767711639),
    MAAC_FLT_C(0.98078525066375732), MAAC_FLT_C(-0.19509032368659973),
    MAAC_FLT_C(0.97003126144409180), MAAC_FLT_C(-0.24298018217086792),
    MAAC_FLT_C(0.95694035291671753), MAAC_FLT_C(-0.29028466343879700),
    MAAC_FLT_C(0.94154405593872070), MAAC_FLT_C(-0.33688986301422119),
    MAAC_FLT_C(0.92387950420379639), MAAC_FLT_C(-0.38268342614173889),
    MAAC_FLT_C(0.90398931503295898), MAAC_FLT_C(-0.42755508422851562),
    MAAC_FLT_C(0.88192129135131836), MAAC_FLT_C(-0.47139674425125122),
    MAAC_FLT_C(0.85772860050201416), MAAC_FLT_C(-0.51410275697708130),
    MAAC_FLT_C(0.83146959543228149), MAAC_FLT_C(-0.55557024478912354),
    MAAC_FLT_C(0.80320751667022705), MAAC_FLT_C(-0.59569931030273438),
    MAAC_FLT_C(0.77301043272018433), MAAC_FLT_C(-0.63439327478408813),
    MAAC_FLT_C(0.74095112085342407), MAAC_FLT_C(-0.67155897617340088),
    MAAC_FLT_C(0.70710676908493042), MAAC_FLT_C(-0.70710676908493042),
    MAAC_FLT_C(0.67155897617340088), MAAC_FLT_C(-0.74095112085342407),
    MAAC_FLT_C(0.63439327478408813), MAAC_FLT_C(-0.77301043272018433),
    MAAC_FLT_C(0.59569931030273438), MAAC_FLT_C(-0.80320751667022705),
    MAAC_FLT_C(0.55557024478912354), MAAC_FLT_C(-0.83146959543228149),
    MAAC_FLT_C(0.51410275697708130), MAAC_FLT_C(-0.85772860050201416),
    MAAC_FLT_C(0.47139674425125122), MAAC_FLT_C(-0.88192129135131836),
    MAAC_FLT_C(0.42755508422851562), MAAC_FLT_C(-0.90398931503295898),
    MAAC_FLT_C(0.38268342614173889), MAAC_FLT_C(-0.92387950420379639),
    MAAC_FLT_C(0.33688986301422119), MAAC_FLT_C(-0.94154405593872070),
    MAAC_FLT_C(0.29028466343879700), MAAC_FLT_C(-0.95694035291671753),
    MAAC_FLT_C(0.24298018217086792), MAAC_FLT_C(-0.97003126144409180),
    MAAC_FLT_C(0.19509032368659973), MAAC_FLT_C(-0.98078525066375732),
    MAAC_FLT_C(0.14673046767711639), MAAC_FLT_C(-0.98917651176452637),
    MAAC_FLT_C(0.098017141222953796), MAAC_FLT_C(-0.99518471956253052),
    MAAC_FLT_C(0.049067676067352295), MAAC_FLT_C(-0.99879544973373413),
    MAAC_FLT_C(6.1232342629258393e-17), MAAC_FLT_C(-1.0000000000000000),
    MAAC_FLT_C(-0.049067676067352295), MAAC_FLT_C(-0.99879544973373413),
    MAAC_FLT_C(-0.098017141222953796), MAAC_FLT_C(-0.99518471956253052),
    MAAC_FLT_C(-0.14673046767711639), MAAC_FLT_C(-0.98917651176452637),
    MAAC_FLT_C(-0.19509032368659973), MAAC_FLT_C(-0.98078525066375732),
    MAAC_FLT_C(-0.24298018217086792), MAAC_FLT_C(-0.97003126144409180),
    MAAC_FLT_C(-0.29028466343879700), MAAC_FLT_C(-0.95694035291671753),
    MAAC_FLT_C(-0.33688986301422119), MAAC_FLT_C(-0.94154405593872070),
    MAAC_FLT_C(-0.38268342614173889), MAAC_FLT_C(-0.92387950420379639),
    MAAC_FLT_C(-0.42755508422851562), MAAC_FLT_C(-0.90398931503295898),
    MAAC_FLT_C(-0.47139674425125122), MAAC_FLT_C(-0.88192129135131836),
    MAAC_FLT_C(-0.51410275697708130), MAAC_FLT_C(-0.85772860050201416),
    MAAC_FLT_C(-0.55557024478912354), MAAC_FLT_C(-0.83146959543228149),
    MAAC_FLT_C(-0.59569931030273438), MAAC_FLT_C(-0.80320751667022705),
    MAAC_FLT_C(-0.63439327478408813), MAAC_FLT_C(-0.77301043272018433),
    MAAC_FLT_C(-0.67155897617340088), MAAC_FLT_C(-0.74095112085342407),
    MAAC_FLT_C(-0.70710676908493042), MAAC_FLT_C(-0.70710676908493042),
    MAAC_FLT_C(-0.74095112085342407), MAAC_FLT_C(-0.67155897617340088),
    MAAC_FLT_C(-0.77301043272018433), MAAC_FLT_C(-0.63439327478408813),
    MAAC_FLT_C(-0.80320751667022705), MAAC_FLT_C(-0.59569931030273438),
    MAAC_FLT_C(-0.83146959543228149), MAAC_FLT_C(-0.55557024478912354),
    MAAC_FLT_C(-0.85772860050201416), MAAC_FLT_C(-0.51410275697708130),
    MAAC_FLT_C(-0.88192129135131836), MAAC_FLT_C(-0.47139674425125122),
    MAAC_FLT_C(-0.90398931503295898), MAAC_FLT_C(-0.42755508422851562),
    MAAC_FLT_C(-0.92387950420379639), MAAC_FLT_C(-0.38268342614173889),
    MAAC_FLT_C(-0.94154405593872070), MAAC_FLT_C(-0.33688986301422119),
    MAAC_FLT_C(-0.95694035291671753), MAAC_FLT_C(-0.29028466343879700),
    MAAC_FLT_C(-0.97003126144409180), MAAC_FLT_C(-0.24298018217086792),
    MAAC_FLT_C(-0.98078525066375732), MAAC_FLT_C(-0.19509032368659973),
    MAAC_FLT_C(-0.98917651176452637), MAAC_FLT_C(-0.14673046767711639),
    MAAC_FLT_C(-0.99518471956253052), MAAC_FLT_C(-0.098017141222953796),
    MAAC_FLT_C(-0.99879544973373413), MAAC_FLT_C(-0.049067676067352295)
};

static const maac_flt MAAC_IMDCT_B_256[128] = {
    MAAC_FLT_C(0.99998116493225098), MAAC_FLT_C(0.0061358846724033356),
    MAAC_FLT_C(0.99983060359954834), MAAC_FLT_C(0.018406730145215988),
    MAAC_FLT_C(0.99952942132949829), MAAC_FLT_C(0.030674804002046585),
    MAAC_FLT_C(0.99907773733139038), MAAC_FLT_C(0.042938258498907089),
    MAAC_FLT_C(0.99847555160522461), MAAC_FLT_C(0.055195245891809464),
    MAAC_FLT_C(0.99772304296493530), MAAC_FLT_C(0.067443922162055969),
    MAAC_FLT_C(0.99682027101516724), MAAC_FLT_C(0.079682439565658569),
    MAAC_FLT_C(0.99576741456985474), MAAC_FLT_C(0.091908954083919525),
    MAAC_FLT_C(0.99456459283828735), MAAC_FLT_C(0.10412163287401199),
    MAAC_FLT_C(0.99321192502975464), MAAC_FLT_C(0.11631862819194794),
    MAAC_FLT_C(0.99170976877212524), MAAC_FLT_C(0.12849810719490051),
    MAAC_FLT_C(0.99005818367004395), MAAC_FLT_C(0.14065824449062347),
    MAAC_FLT_C(0.98825758695602417), MAAC_FLT_C(0.15279719233512878),
    MAAC_FLT_C(0.98630809783935547), MAAC_FLT_C(0.16491311788558960),
    MAAC_FLT_C(0.98421007394790649), MAAC_FLT_C(0.17700421810150146),
    MAAC_FLT_C(0.98196387290954590), MAAC_FLT_C(0.18906866014003754),
    MAAC_FLT_C(0.97956979274749756), MAAC_FLT_C(0.20110464096069336),
    MAAC_FLT_C(0.97702813148498535), MAAC_FLT_C(0.21311031281948090),
    MAAC_FLT_C(0.97433936595916748), MAAC_FLT_C(0.22508391737937927),
    MAAC_FLT_C(0.97150391340255737), MAAC_FLT_C(0.23702360689640045),
    MAAC_FLT_C(0.96852207183837891), MAAC_FLT_C(0.24892760813236237),
    MAAC_FLT_C(0.96539443731307983), MAAC_FLT_C(0.26079410314559937),
    MAAC_FLT_C(0.96212142705917358), MAAC_FLT_C(0.27262136340141296),
    MAAC_FLT_C(0.95870345830917358), MAAC_FLT_C(0.28440752625465393),
    MAAC_FLT_C(0.95514118671417236), MAAC_FLT_C(0.29615089297294617),
    MAAC_FLT_C(0.95143502950668335), MAAC_FLT_C(0.30784964561462402),
    MAAC_FLT_C(0.94758558273315430), MAAC_FLT_C(0.31950202584266663),
    MAAC_FLT_C(0.94359344244003296), MAAC_FLT_C(0.33110630512237549),
    MAAC_FLT_C(0.93945920467376709), MAAC_FLT_C(0.34266072511672974),
    MAAC_FLT_C(0.93518352508544922), MAAC_FLT_C(0.35416352748870850),
    MAAC_FLT_C(0.93076694011688232), MAAC_FLT_C(0.36561298370361328),
    MAAC_FLT_C(0.92621022462844849), MAAC_FLT_C(0.37700742483139038),
    MAAC_FLT_C(0.92151403427124023), MAAC_FLT_C(0.38834503293037415),
    MAAC_FLT_C(0.91667908430099487), MAAC_FLT_C(0.39962419867515564),
    MAAC_FLT_C(0.91170603036880493), MAAC_FLT_C(0.41084316372871399),
    MAAC_FLT_C(0.90659570693969727), MAAC_FLT_C(0.42200025916099548),
    MAAC_FLT_C(0.90134882926940918), MAAC_FLT_C(0.43309381604194641),
    MAAC_FLT_C(0.89596623182296753), MAAC_FLT_C(0.44412213563919067),
    MAAC_FLT_C(0.89044874906539917), MAAC_FLT_C(0.45508357882499695),
    MAAC_FLT_C(0.88479709625244141), MAAC_FLT_C(0.46597650647163391),
    MAAC_FLT_C(0.87901222705841064), MAAC_FLT_C(0.47679921984672546),
    MAAC_FLT_C(0.87309497594833374), MAAC_FLT_C(0.48755016922950745),
    MAAC_FLT_C(0.86704623699188232), MAAC_FLT_C(0.49822765588760376),
    MAAC_FLT_C(0.86086696386337280), MAAC_FLT_C(0.50883013010025024),
    MAAC_FLT_C(0.85455799102783203), MAAC_FLT_C(0.51935601234436035),
    MAAC_FLT_C(0.84812033176422119), MAAC_FLT_C(0.52980363368988037),
    MAAC_FLT_C(0.84155499935150146), MAAC_FLT_C(0.54017144441604614),
    MAAC_FLT_C(0.83486288785934448), MAAC_FLT_C(0.55045795440673828),
    MAAC_FLT_C(0.82804507017135620), MAAC_FLT_C(0.56066155433654785),
    MAAC_FLT_C(0.82110249996185303), MAAC_FLT_C(0.57078075408935547),
    MAAC_FLT_C(0.81403630971908569), MAAC_FLT_C(0.58081394433975220),
    MAAC_FLT_C(0.80684757232666016), MAAC_FLT_C(0.59075969457626343),
    MAAC_FLT_C(0.79953724145889282), MAAC_FLT_C(0.60061645507812500),
    MAAC_FLT_C(0.79210656881332397), MAAC_FLT_C(0.61038279533386230),
    MAAC_FLT_C(0.78455656766891479), MAAC_FLT_C(0.62005722522735596),
    MAAC_FLT_C(0.77688848972320557), MAAC_FLT_C(0.62963825464248657),
    MAAC_FLT_C(0.76910334825515747), MAAC_FLT_C(0.63912445306777954),
    MAAC_FLT_C(0.76120239496231079), MAAC_FLT_C(0.64851438999176025),
    MAAC_FLT_C(0.75318682193756104), MAAC_FLT_C(0.65780669450759888),
    MAAC_FLT_C(0.74505776166915894), MAAC_FLT_C(0.66699993610382080),
    MAAC_FLT_C(0.73681658506393433), MAAC_FLT_C(0.67609268426895142),
    MAAC_FLT_C(0.72846436500549316), MAAC_FLT_C(0.68508368730545044),
    MAAC_FLT_C(0.72000253200531006), MAAC_FLT_C(0.69397145509719849),
    MAAC_FLT_C(0.71143221855163574), MAAC_FLT_C(0.70275473594665527)
};

static const maac_flt MAAC_IMDCT_C_256[64] = {
    MAAC_FLT_C(0.99969881772994995), MAAC_FLT_C(-0.024541229009628296),
    MAAC_FLT_C(0.99729043245315552), MAAC_FLT_C(-0.073564566671848297),
    MAAC_FLT_C(0.99247956275939941), MAAC_FLT_C(-0.12241067737340927),
    MAAC_FLT_C(0.98527765274047852), MAAC_FLT_C(-0.17096188664436340),
    MAAC_FLT_C(0.97570210695266724), MAAC_FLT_C(-0.21910123527050018),
    MAAC_FLT_C(0.96377605199813843), MAAC_FLT_C(-0.26671275496482849),
    MAAC_FLT_C(0.94952815771102905), MAAC_FLT_C(-0.31368175148963928),
    MAAC_FLT_C(0.93299281597137451), MAAC_FLT_C(-0.35989505052566528),
    MAAC_FLT_C(0.91420978307723999), MAAC_FLT_C(-0.40524131059646606),
    MAAC_FLT_C(0.89322429895401001), MAAC_FLT_C(-0.44961133599281311),
    MAAC_FLT_C(0.87008696794509888), MAAC_FLT_C(-0.49289819598197937),
    MAAC_FLT_C(0.84485357999801636), MAAC_FLT_C(-0.53499764204025269),
    MAAC_FLT_C(0.81758481264114380), MAAC_FLT_C(-0.57580816745758057),
    MAAC_FLT_C(0.78834640979766846), MAAC_FLT_C(-0.61523157358169556),
    MAAC_FLT_C(0.75720882415771484), MAAC_FLT_C(-0.65317285060882568),
    MAAC_FLT_C(0.72424709796905518), MAAC_FLT_C(-0.68954056501388550),
    MAAC_FLT_C(0.68954056501388550), MAAC_FLT_C(-0.72424709796905518),
    MAAC_FLT_C(0.65317285060882568), MAAC_FLT_C(-0.75720882415771484),
    MAAC_FLT_C(0.61523157358169556), MAAC_FLT_C(-0.78834640979766846),
    MAAC_FLT_C(0.57580816745758057), MAAC_FLT_C(-0.81758481264114380),
    MAAC_FLT_C(0.53499764204025269), MAAC_FLT_C(-0.84485357999801636),
    MAAC_FLT_C(0.49289819598197937), MAAC_FLT_C(-0.87008696794509888),
    MAAC_FLT_C(0.44961133599281311), MAAC_FLT_C(-0.89322429895401001),
    MAAC_FLT_C(0.40524131059646606), MAAC_FLT_C(-0.91420978307723999),
    MAAC_FLT_C(0.35989505052566528), MAAC_FLT_C(-0.93299281597137451),
    MAAC_FLT_C(0.31368175148963928), MAAC_FLT_C(-0.94952815771102905),
    MAAC_FLT_C(0.26671275496482849), MAAC_FLT_C(-0.96377605199813843),
    MAAC_FLT_C(0.21910123527050018), MAAC_FLT_C(-0.97570210695266724),
    MAAC_FLT_C(0.17096188664436340), MAAC_FLT_C(-0.98527765274047852),
    MAAC_FLT_C(0.12241067737340927), MAAC_FLT_C(-0.99247956275939941),
    MAAC_FLT_C(0.073564566671848297), MAAC_FLT_C(-0.99729043245315552),
    MAAC_FLT_C(0.024541229009628296), MAAC_FLT_C(-0.99969881772994995)
};


static const maac_u8 maac_bit_reverse_data[256] = {
    /* 00000000 => 00000000 */ 0x00,
    /* 00000001 => 10000000 */ 0x80,
    /* 00000010 => 01000000 */ 0x40,
    /* 00000011 => 11000000 */ 0xc0,
    /* 00000100 => 00100000 */ 0x20,
    /* 00000101 => 10100000 */ 0xa0,
    /* 00000110 => 01100000 */ 0x60,
    /* 00000111 => 11100000 */ 0xe0,
    /* 00001000 => 00010000 */ 0x10,
    /* 00001001 => 10010000 */ 0x90,
    /* 00001010 => 01010000 */ 0x50,
    /* 00001011 => 11010000 */ 0xd0,
    /* 00001100 => 00110000 */ 0x30,
    /* 00001101 => 10110000 */ 0xb0,
    /* 00001110 => 01110000 */ 0x70,
    /* 00001111 => 11110000 */ 0xf0,
    /* 00010000 => 00001000 */ 0x08,
    /* 00010001 => 10001000 */ 0x88,
    /* 00010010 => 01001000 */ 0x48,
    /* 00010011 => 11001000 */ 0xc8,
    /* 00010100 => 00101000 */ 0x28,
    /* 00010101 => 10101000 */ 0xa8,
    /* 00010110 => 01101000 */ 0x68,
    /* 00010111 => 11101000 */ 0xe8,
    /* 00011000 => 00011000 */ 0x18,
    /* 00011001 => 10011000 */ 0x98,
    /* 00011010 => 01011000 */ 0x58,
    /* 00011011 => 11011000 */ 0xd8,
    /* 00011100 => 00111000 */ 0x38,
    /* 00011101 => 10111000 */ 0xb8,
    /* 00011110 => 01111000 */ 0x78,
    /* 00011111 => 11111000 */ 0xf8,
    /* 00100000 => 00000100 */ 0x04,
    /* 00100001 => 10000100 */ 0x84,
    /* 00100010 => 01000100 */ 0x44,
    /* 00100011 => 11000100 */ 0xc4,
    /* 00100100 => 00100100 */ 0x24,
    /* 00100101 => 10100100 */ 0xa4,
    /* 00100110 => 01100100 */ 0x64,
    /* 00100111 => 11100100 */ 0xe4,
    /* 00101000 => 00010100 */ 0x14,
    /* 00101001 => 10010100 */ 0x94,
    /* 00101010 => 01010100 */ 0x54,
    /* 00101011 => 11010100 */ 0xd4,
    /* 00101100 => 00110100 */ 0x34,
    /* 00101101 => 10110100 */ 0xb4,
    /* 00101110 => 01110100 */ 0x74,
    /* 00101111 => 11110100 */ 0xf4,
    /* 00110000 => 00001100 */ 0x0c,
    /* 00110001 => 10001100 */ 0x8c,
    /* 00110010 => 01001100 */ 0x4c,
    /* 00110011 => 11001100 */ 0xcc,
    /* 00110100 => 00101100 */ 0x2c,
    /* 00110101 => 10101100 */ 0xac,
    /* 00110110 => 01101100 */ 0x6c,
    /* 00110111 => 11101100 */ 0xec,
    /* 00111000 => 00011100 */ 0x1c,
    /* 00111001 => 10011100 */ 0x9c,
    /* 00111010 => 01011100 */ 0x5c,
    /* 00111011 => 11011100 */ 0xdc,
    /* 00111100 => 00111100 */ 0x3c,
    /* 00111101 => 10111100 */ 0xbc,
    /* 00111110 => 01111100 */ 0x7c,
    /* 00111111 => 11111100 */ 0xfc,
    /* 01000000 => 00000010 */ 0x02,
    /* 01000001 => 10000010 */ 0x82,
    /* 01000010 => 01000010 */ 0x42,
    /* 01000011 => 11000010 */ 0xc2,
    /* 01000100 => 00100010 */ 0x22,
    /* 01000101 => 10100010 */ 0xa2,
    /* 01000110 => 01100010 */ 0x62,
    /* 01000111 => 11100010 */ 0xe2,
    /* 01001000 => 00010010 */ 0x12,
    /* 01001001 => 10010010 */ 0x92,
    /* 01001010 => 01010010 */ 0x52,
    /* 01001011 => 11010010 */ 0xd2,
    /* 01001100 => 00110010 */ 0x32,
    /* 01001101 => 10110010 */ 0xb2,
    /* 01001110 => 01110010 */ 0x72,
    /* 01001111 => 11110010 */ 0xf2,
    /* 01010000 => 00001010 */ 0x0a,
    /* 01010001 => 10001010 */ 0x8a,
    /* 01010010 => 01001010 */ 0x4a,
    /* 01010011 => 11001010 */ 0xca,
    /* 01010100 => 00101010 */ 0x2a,
    /* 01010101 => 10101010 */ 0xaa,
    /* 01010110 => 01101010 */ 0x6a,
    /* 01010111 => 11101010 */ 0xea,
    /* 01011000 => 00011010 */ 0x1a,
    /* 01011001 => 10011010 */ 0x9a,
    /* 01011010 => 01011010 */ 0x5a,
    /* 01011011 => 11011010 */ 0xda,
    /* 01011100 => 00111010 */ 0x3a,
    /* 01011101 => 10111010 */ 0xba,
    /* 01011110 => 01111010 */ 0x7a,
    /* 01011111 => 11111010 */ 0xfa,
    /* 01100000 => 00000110 */ 0x06,
    /* 01100001 => 10000110 */ 0x86,
    /* 01100010 => 01000110 */ 0x46,
    /* 01100011 => 11000110 */ 0xc6,
    /* 01100100 => 00100110 */ 0x26,
    /* 01100101 => 10100110 */ 0xa6,
    /* 01100110 => 01100110 */ 0x66,
    /* 01100111 => 11100110 */ 0xe6,
    /* 01101000 => 00010110 */ 0x16,
    /* 01101001 => 10010110 */ 0x96,
    /* 01101010 => 01010110 */ 0x56,
    /* 01101011 => 11010110 */ 0xd6,
    /* 01101100 => 00110110 */ 0x36,
    /* 01101101 => 10110110 */ 0xb6,
    /* 01101110 => 01110110 */ 0x76,
    /* 01101111 => 11110110 */ 0xf6,
    /* 01110000 => 00001110 */ 0x0e,
    /* 01110001 => 10001110 */ 0x8e,
    /* 01110010 => 01001110 */ 0x4e,
    /* 01110011 => 11001110 */ 0xce,
    /* 01110100 => 00101110 */ 0x2e,
    /* 01110101 => 10101110 */ 0xae,
    /* 01110110 => 01101110 */ 0x6e,
    /* 01110111 => 11101110 */ 0xee,
    /* 01111000 => 00011110 */ 0x1e,
    /* 01111001 => 10011110 */ 0x9e,
    /* 01111010 => 01011110 */ 0x5e,
    /* 01111011 => 11011110 */ 0xde,
    /* 01111100 => 00111110 */ 0x3e,
    /* 01111101 => 10111110 */ 0xbe,
    /* 01111110 => 01111110 */ 0x7e,
    /* 01111111 => 11111110 */ 0xfe,
    /* 10000000 => 00000001 */ 0x01,
    /* 10000001 => 10000001 */ 0x81,
    /* 10000010 => 01000001 */ 0x41,
    /* 10000011 => 11000001 */ 0xc1,
    /* 10000100 => 00100001 */ 0x21,
    /* 10000101 => 10100001 */ 0xa1,
    /* 10000110 => 01100001 */ 0x61,
    /* 10000111 => 11100001 */ 0xe1,
    /* 10001000 => 00010001 */ 0x11,
    /* 10001001 => 10010001 */ 0x91,
    /* 10001010 => 01010001 */ 0x51,
    /* 10001011 => 11010001 */ 0xd1,
    /* 10001100 => 00110001 */ 0x31,
    /* 10001101 => 10110001 */ 0xb1,
    /* 10001110 => 01110001 */ 0x71,
    /* 10001111 => 11110001 */ 0xf1,
    /* 10010000 => 00001001 */ 0x09,
    /* 10010001 => 10001001 */ 0x89,
    /* 10010010 => 01001001 */ 0x49,
    /* 10010011 => 11001001 */ 0xc9,
    /* 10010100 => 00101001 */ 0x29,
    /* 10010101 => 10101001 */ 0xa9,
    /* 10010110 => 01101001 */ 0x69,
    /* 10010111 => 11101001 */ 0xe9,
    /* 10011000 => 00011001 */ 0x19,
    /* 10011001 => 10011001 */ 0x99,
    /* 10011010 => 01011001 */ 0x59,
    /* 10011011 => 11011001 */ 0xd9,
    /* 10011100 => 00111001 */ 0x39,
    /* 10011101 => 10111001 */ 0xb9,
    /* 10011110 => 01111001 */ 0x79,
    /* 10011111 => 11111001 */ 0xf9,
    /* 10100000 => 00000101 */ 0x05,
    /* 10100001 => 10000101 */ 0x85,
    /* 10100010 => 01000101 */ 0x45,
    /* 10100011 => 11000101 */ 0xc5,
    /* 10100100 => 00100101 */ 0x25,
    /* 10100101 => 10100101 */ 0xa5,
    /* 10100110 => 01100101 */ 0x65,
    /* 10100111 => 11100101 */ 0xe5,
    /* 10101000 => 00010101 */ 0x15,
    /* 10101001 => 10010101 */ 0x95,
    /* 10101010 => 01010101 */ 0x55,
    /* 10101011 => 11010101 */ 0xd5,
    /* 10101100 => 00110101 */ 0x35,
    /* 10101101 => 10110101 */ 0xb5,
    /* 10101110 => 01110101 */ 0x75,
    /* 10101111 => 11110101 */ 0xf5,
    /* 10110000 => 00001101 */ 0x0d,
    /* 10110001 => 10001101 */ 0x8d,
    /* 10110010 => 01001101 */ 0x4d,
    /* 10110011 => 11001101 */ 0xcd,
    /* 10110100 => 00101101 */ 0x2d,
    /* 10110101 => 10101101 */ 0xad,
    /* 10110110 => 01101101 */ 0x6d,
    /* 10110111 => 11101101 */ 0xed,
    /* 10111000 => 00011101 */ 0x1d,
    /* 10111001 => 10011101 */ 0x9d,
    /* 10111010 => 01011101 */ 0x5d,
    /* 10111011 => 11011101 */ 0xdd,
    /* 10111100 => 00111101 */ 0x3d,
    /* 10111101 => 10111101 */ 0xbd,
    /* 10111110 => 01111101 */ 0x7d,
    /* 10111111 => 11111101 */ 0xfd,
    /* 11000000 => 00000011 */ 0x03,
    /* 11000001 => 10000011 */ 0x83,
    /* 11000010 => 01000011 */ 0x43,
    /* 11000011 => 11000011 */ 0xc3,
    /* 11000100 => 00100011 */ 0x23,
    /* 11000101 => 10100011 */ 0xa3,
    /* 11000110 => 01100011 */ 0x63,
    /* 11000111 => 11100011 */ 0xe3,
    /* 11001000 => 00010011 */ 0x13,
    /* 11001001 => 10010011 */ 0x93,
    /* 11001010 => 01010011 */ 0x53,
    /* 11001011 => 11010011 */ 0xd3,
    /* 11001100 => 00110011 */ 0x33,
    /* 11001101 => 10110011 */ 0xb3,
    /* 11001110 => 01110011 */ 0x73,
    /* 11001111 => 11110011 */ 0xf3,
    /* 11010000 => 00001011 */ 0x0b,
    /* 11010001 => 10001011 */ 0x8b,
    /* 11010010 => 01001011 */ 0x4b,
    /* 11010011 => 11001011 */ 0xcb,
    /* 11010100 => 00101011 */ 0x2b,
    /* 11010101 => 10101011 */ 0xab,
    /* 11010110 => 01101011 */ 0x6b,
    /* 11010111 => 11101011 */ 0xeb,
    /* 11011000 => 00011011 */ 0x1b,
    /* 11011001 => 10011011 */ 0x9b,
    /* 11011010 => 01011011 */ 0x5b,
    /* 11011011 => 11011011 */ 0xdb,
    /* 11011100 => 00111011 */ 0x3b,
    /* 11011101 => 10111011 */ 0xbb,
    /* 11011110 => 01111011 */ 0x7b,
    /* 11011111 => 11111011 */ 0xfb,
    /* 11100000 => 00000111 */ 0x07,
    /* 11100001 => 10000111 */ 0x87,
    /* 11100010 => 01000111 */ 0x47,
    /* 11100011 => 11000111 */ 0xc7,
    /* 11100100 => 00100111 */ 0x27,
    /* 11100101 => 10100111 */ 0xa7,
    /* 11100110 => 01100111 */ 0x67,
    /* 11100111 => 11100111 */ 0xe7,
    /* 11101000 => 00010111 */ 0x17,
    /* 11101001 => 10010111 */ 0x97,
    /* 11101010 => 01010111 */ 0x57,
    /* 11101011 => 11010111 */ 0xd7,
    /* 11101100 => 00110111 */ 0x37,
    /* 11101101 => 10110111 */ 0xb7,
    /* 11101110 => 01110111 */ 0x77,
    /* 11101111 => 11110111 */ 0xf7,
    /* 11110000 => 00001111 */ 0x0f,
    /* 11110001 => 10001111 */ 0x8f,
    /* 11110010 => 01001111 */ 0x4f,
    /* 11110011 => 11001111 */ 0xcf,
    /* 11110100 => 00101111 */ 0x2f,
    /* 11110101 => 10101111 */ 0xaf,
    /* 11110110 => 01101111 */ 0x6f,
    /* 11110111 => 11101111 */ 0xef,
    /* 11111000 => 00011111 */ 0x1f,
    /* 11111001 => 10011111 */ 0x9f,
    /* 11111010 => 01011111 */ 0x5f,
    /* 11111011 => 11011111 */ 0xdf,
    /* 11111100 => 00111111 */ 0x3f,
    /* 11111101 => 10111111 */ 0xbf,
    /* 11111110 => 01111111 */ 0x7f,
    /* 11111111 => 11111111 */ 0xff
};


/* take 1, taking the reference formula, hoisting a few
const values up. Pretty dang slow. */

#if 0
#include <math.h> /* for cos() */

    /* for reference formula is:
      n0 = (len / 2 + 1) / 2

      x[i] = 2/len *
        sum{k: 0 -> len/2} of:
          in[k] *
          cos(
            (2 * pi / len)
            *
            (i + n0)
            *
            (k + 0.5)
          )
    */

static maac_flt maac_imdct_value(const maac_flt* buf, const maac_u16 len, maac_u16 idx, const maac_flt n0, const maac_flt pi2n) {
   maac_u16 k  = 0;
   maac_flt sum = MAAC_FLT_C(0.0);

   for(k=0;k<len/2;k++) {
       sum += buf[k] * cos(pi2n * (maac_flt_cast(idx) + n0) * (maac_flt_cast(k) + MAAC_FLT_C(0.5)));
   }

   return sum;
}

MAAC_PRIVATE
void
maac_imdct(maac_flt* buf,  maac_u16 len) {
   const maac_flt n0 = (((maac_flt_cast(len)) / MAAC_FLT_C(2.0)) + MAAC_FLT_C(1.0)) / MAAC_FLT_C(2.0);
   const maac_flt pi2n = 2.0f * MAAC_M_PI / maac_flt_cast(len);
   const maac_flt scale = 2.0f / maac_flt_cast(len);
   maac_flt copy[1024];
   maac_u16 i = 0;

   maac_memcpy(copy, buf, sizeof(maac_flt) * ((maac_u32)len/2) );

   i = 0;
   while(i < len) {
       buf[i] = maac_imdct_value(copy, len, i, n0, pi2n) * scale;
       i++;
   }
}

#else

/* take 2 - let's try adapting
"The use of multirate filter banks for coding of high quality digital audio"
by Sporer, Brandenburg, and Edler (1992) */
static 
maac_u16
maac_ilog2(maac_u16 v) {
    maac_u16 r = 0;
    while(v >>= 1) {
        r++;
    }
    return r;
}

static void
maac_imdct_step0(maac_flt* buf, const maac_u16 n) {
    const maac_u16 n2 = n >> 1;
    /* TODO - should we scale input down? */
    const maac_flt scale = MAAC_FLT_C(1.0);

    maac_u16 k = 0;
    while(k < n2) {
        buf[k] *= scale;
        k++;
    }
    while(k < n) {
        buf[k] = -buf[n - k - 1];
        k++;
    }
}

static void
maac_imdct_step1(maac_flt* buf, const maac_flt* A, const maac_u16 n) {
    const maac_u16 n4 = n >> 2;
    maac_u16 k = 0;
    maac_u16 k2 = 0;
    maac_u16 k4 = 0;

    maac_flt a, b, c, d;

    /* I think the paper has a rendering error - it shows
    the formula as:
      buf[x]  (buf[q] - buf[t] * a) -
              -(buf[r] - buf[s]) * a
              but I think it's just supposed to be a regular minus -
              not subtracting a negation. */

    while(k < n4) {
        /* we're working backwards from end of buffer in increments of 4 */
        /* using some numbers of n=2048, first iteration we're performing:
          buf[2047] = (buf[0] - buf[2047] * A
                      -
                      (buf[2] - buf[2045]) * A

          buf[2045] = (buf[0] - buf[2047] * A
                      +
                      (buf[2] - buf[2045]) * A 

           last iteration we're performing:
          buf[[3]   = (buf[2044] - buf[3]) * A
                      -
                      (buf[2046] - buf[1]) * A

          buf[1]  = buf[2044] - buf[3] * A
                    +
                    buf[2046] - buf[1] * A

        We're only writing to the odd indexes so we don't
        have to worry about preserving even indexes, we just
        need to preserve the two odd indexes we read from
        since we overwrite mid-step. But we'll copy out all
        four values into variables since that's easier to read. */

        a = buf[n - k4 - 1];
        b = buf[n - k4 - 3];
        c = buf[k4];
        d = buf[k4+2];

        buf[n - k4 - 1] =
            (c - a) * A[k2]
            -
            (d - b) * A[k2+1];

        buf[n - k4 - 3] =
            (c - a) * A[k2 + 1]
            +
            (d - b) * A[k2];

        k++;
        k2 += 2;
        k4 += 4;
    }
}

static void
maac_imdct_step2(maac_flt* buf, const maac_flt* A, const maac_u16 n) {
    const maac_u16 n2 = n >> 1;
    const maac_u16 n8 = n >> 3;
    maac_u16 k = 0;
    maac_u16 k4 = 0;

    maac_flt a, b, c, d;

    while(k < n8) {
        /* this time I'm just going to write out the buffer indexes accessd.

        first iteration
          buf[1027] = stuff(buf[1027],buf[3])
          buf[1025] = stuff(buf[1025],buf[1])
          buf[3] = stuff(buf[1027],buf[3],buf[1025],buf[1])
          buf[1] = stuff(buf[1025],buf[1],buf[1027],buf[3])

        last iteration:
          buf[2047] = stuff(buf[2047],buf[1023])
          buf[2045] = stuff(buf[2045],buf[1021])
          buf[1023] = stuff(buf[2047],buf[1023],buf[2045],buf[1])
          buf[1021] = stuff(buf[2045],buf[1021],buf[2047],buf[3])
        so similar to last time - we only write to odd indexes,
        and we do overwrite stuff mid-update so we'll save to variables */
        a = buf[n2 + k4 + 3];
        b = buf[n2 + k4 + 1];
        c = buf[k4 + 3];
        d = buf[k4 + 1];

        buf[n2 + k4 + 3] =
          a + c;
        buf[n2 + k4 + 1] =
          b + d;
        buf[k4 + 3] =
          (a - c) * A[n2 - 4 - k4]
          -
          (b - d) * A[n2 - 3 - k4];
        buf[k4 + 1] =
          (b - d) * A[n2 - 4 - k4]
          +
          (a - c) * A[n2 - 3 - k4];

        k++;
        k4 += 4;
    }
}

static void
maac_imdct_step3(maac_flt* buf, const maac_flt* A, const maac_u16 n) {
    const maac_u16 ld = maac_ilog2(n);

    maac_u16 l = 0;
    maac_u16 r = 0;
    maac_u16 r4 = 0;
    maac_u16 s = 0;
    maac_u16 s2 = 0;

    maac_flt a, b, c, d;

    while(l < (ld - 3)) {
        const maac_u16 k0 = n >> (l + 2); /* equivalent to n / (2^(l+2)) */
        const maac_u16 k1 = 1 << (l + 3);
        const maac_u16 r_end = n >> (l + 4); /* equivalent to n / (2^(l+4)) */
        const maac_u16 s_end = 1 << (l + 1);

        while(r < r_end) {
            while(s < s_end) {
                /* like previous entries - this will only access odd indexes */
                a = buf[n - 1 - k0 * s2 - r4];
                b = buf[n - 3 - k0 * s2 - r4];
                c = buf[n - 1 - k0 * (s2 + 1) - r4];
                d = buf[n - 3 - k0 * (s2 + 1) - r4];

                buf[n - 1 - k0 * s2 - r4] =
                  a + c;

                buf[n - 3 - k0 * s2 - r4] =
                  b + d;

                buf[n - 1 - k0 * (s2 + 1) - r4] =
                  (a - c) * A[r * k1]
                  -
                  (b - d) * A[r * k1 + 1];

                buf[n - 3 - k0 * (s2 + 1) - r4] =
                  (b - d) * A[r * k1]
                  +
                  (a - c) * A[r * k1 + 1];
                s++;
                s2 += 2;
            }
            s=0;
            s2=0;
            r++;
            r4 += 4;
        }
        r=0;
        r4=0;
        l++;
    }
}

static maac_u16
maac_bit_reverse(maac_u16 n) {
    return
      (((maac_u16)maac_bit_reverse_data[n & 0xff]) << 8)
      |
      ((maac_u16)maac_bit_reverse_data[n >> 8]);
}

static void
maac_imdct_step4(maac_flt* buf, const maac_u16 n) {
    const maac_u16 n8 = n >> 3;
    /* needed to properly scale bits back into range */
    const maac_u16 ld = maac_ilog2(n);
    maac_u16 i;
    maac_u16 i8;
    maac_u16 j;
    maac_u16 j8;

    maac_flt f[8];

    /* we skip 0 and 255 */
    i=1;
    while(i < n8 - 1) {
        j = maac_bit_reverse(i) >> (16 - ld + 3);
        if(i < j) {
            i8 = 8 * i;
            j8 = 8 * j;

            f[0] = buf[i8+1];
            f[1] = buf[i8+3];
            f[2] = buf[i8+5];
            f[3] = buf[i8+7];
            f[4] = buf[j8+1];
            f[5] = buf[j8+3];
            f[6] = buf[j8+5];
            f[7] = buf[j8+7];

            buf[j8+1]=f[0];
            buf[j8+3]=f[1];
            buf[j8+5]=f[2];
            buf[j8+7]=f[3];
            buf[i8+1]=f[4];
            buf[i8+3]=f[5];
            buf[i8+5]=f[6];
            buf[i8+7]=f[7];
        }

        i++;
    }
}

static void
maac_imdct_step5(maac_flt* buf, const maac_u16 n) {
    const maac_u16 n2 = n >> 1;
    maac_u16 k = 0;
    maac_u16 k2 = 0;

    while(k < n2) {
        buf[k] = buf[k2+1];

        k++;
        k2 += 2;
    }
}

static void
maac_imdct_step6(maac_flt* buf, const maac_u16 n) {
    const maac_u16 n8 = n >> 3;
    const maac_u16 n3_4 = n - (n >> 2);

    maac_u16 k = 0;
    maac_u16 k2 = 0;
    maac_u16 k4 = 0;

    while(k < n8) {
        buf[n - 1 - k2] = buf[k4];
        buf[n - 2 - k2] = buf[k4+1];
        buf[n3_4 - 1 - k2] = buf[k4 + 2];
        buf[n3_4 - 2 - k2] = buf[k4 + 3];

        k++;
        k2 += 2;
        k4 += 4;
    }
}

static void
maac_imdct_step7(maac_flt* buf, const maac_flt* C, const maac_u16 n) {
    const maac_u16 n2 = n >> 1;
    const maac_u16 n8 = n >> 3;

    maac_u16 k = 0;
    maac_u16 k2 = 0;

    maac_flt a, b, c, d;

    while(k < n8) {
        a = buf[n2 + k2];
        b = buf[n2 + k2 + 1];
        c = buf[n - 2 - k2];
        d = buf[n - 1 - k2];

        buf[n2 + k2] =
          (a + c + C[k2+1] * (a - c) + C[k2] * (b + d)) / MAAC_FLT_C(2.0);

        buf[n - 2 - k2] =
          (a + c - C[k2+1] * (a - c) - C[k2] * (b + d)) / MAAC_FLT_C(2.0);

        buf[n2 + k2 + 1] =
          (b - d + C[k2+1] * (b + d) - C[k2] * (a - c)) / MAAC_FLT_C(2.0);

        buf[n - 1 - k2] =
          (-b + d + C[k2+1] * (b + d) - C[k2] * (a - c)) / MAAC_FLT_C(2.0);

        k++;
        k2 += 2;
    }
}

static void
maac_imdct_step8(maac_flt* buf, const maac_flt* B, const maac_u16 n) {
    const maac_u16 n2 = n >> 1;
    const maac_u16 n4 = n >> 2;

    maac_u16 k = 0;
    maac_u16 k2 = 0;

    maac_flt a, b;

    while(k < n4) {
        a = buf[k2 + n2];
        b = buf[k2 + 1 + n2];

        buf[k] = a * B[k2] + b * B[k2 + 1];
        buf[n2 - 1 - k] = a * B[k2 + 1] - b * B[k2];

        k++;
        k2 += 2;
    }
}

static void
maac_imdct_finalize(maac_flt* buf, maac_flt scale, const maac_u16 n) {
    const maac_u16 n2 = n >> 1;
    const maac_u16 n4 = n >> 2;
    const maac_u16 n3_4 = n - n4;

    /* original formula goes:
       while(k < n4) {
           buf[0] = buf[512]
           ..
           buf[511] = buf[1023]
       }

       while(k < n3_4) {
           buf[512]  = -buf[1023]
           buf[513]  = -buf[1022]
           buf[1023] = -buf[512]
           buf[1024] = -buf[511
           ..
           buf[1535] = -buf[0]
       }
       while(k < n) {
           buf[1536] = -buf[0]
           ..
           buf[2047] = -buf[511]
           k++;
       }
       which would have us overwrite values as we use them so instead:
           - update 1536 -> 2047 and 0 -> 511
           - update 512 -> 1023 and 1024 -> 1535
   */

   maac_u16 k = 0;
   while(k < n4) {
       /* copy -buf[0] -> buf[1546] */
       buf[n3_4 + k] = -buf[k] * scale;
       /* copy buf[512] -> buf[0] */
       buf[k] = buf[k + n4] * scale;
       k++;
   }
   /* so now:
     1546 -> 2047 contain what was in   0 -> 511 (negative)
     0    ->  511 contain what was in 512 -> 1023 (original)
     and everything we needed was in 0 -> 1023 originally, so
     we just have to mess with what indexes we read from
   */

   while(k < n2) {
       /* 512 needs to be -1023, which is now in 511 */
       buf[k] = -buf[n2 -k - 1];

       /* 1024 needs to be -511 - which is in 2047 and already negated */
       buf[k + n4] = buf[n + n4 - k - 1];

       k++;
   }

}

MAAC_PRIVATE
void
maac_imdct(maac_flt* samples, const maac_u16 n) {
    const maac_flt* A = n == 2048 ? MAAC_IMDCT_A_2048 : MAAC_IMDCT_A_256;
    const maac_flt* B = n == 2048 ? MAAC_IMDCT_B_2048 : MAAC_IMDCT_B_256;
    const maac_flt* C = n == 2048 ? MAAC_IMDCT_C_2048 : MAAC_IMDCT_C_256;

    maac_imdct_step0(samples, n);
    maac_imdct_step1(samples, A, n);
    maac_imdct_step2(samples, A, n);
    maac_imdct_step3(samples, A, n);
    maac_imdct_step4(samples, n);
    maac_imdct_step5(samples, n);
    maac_imdct_step6(samples, n);
    maac_imdct_step7(samples, C, n);
    maac_imdct_step8(samples, B, n);

    /* TODO the scalefactor in the paper's IMDCT is n/4,
    but MPEG's is 2/n - I would think that 2/n * n/4 would come
    out to 1/4 but that's definitely not it.
    Experimentation shows that 1/n works, but... why? */
    maac_imdct_finalize(samples, MAAC_FLT_C(1.0) / maac_flt_cast(n), n);
}
#endif

static const maac_u32
maac_frequency[16] = {
    /* 0x00 */ 96000,
    /* 0x01 */ 88200,
    /* 0x02 */ 64000,
    /* 0x03 */ 48000,
    /* 0x04 */ 44100,
    /* 0x05 */ 32000,
    /* 0x06 */ 24000,
    /* 0x07 */ 22050,
    /* 0x08 */ 16000,
    /* 0x09 */ 12000,
    /* 0x0a */ 11025,
    /* 0x0b */ 8000,
    /* 0x0c */ 7350,
    /* 0x0d */ 0,
    /* 0x0e */ 0,
    /* 0x0f */ 0
};

maac_const
MAAC_PUBLIC
maac_u32
maac_sampling_frequency(maac_u32 sample_frequency_index) {
    maac_assert(sample_frequency_index <= 0x0c);
    return maac_frequency[sample_frequency_index];
}

maac_const
MAAC_PUBLIC
maac_u32
maac_sampling_frequency_index(maac_u32 sample_rate) {
    if(sample_rate >= 92017) return 0x00;
    if(sample_rate >= 75132) return 0x01;
    if(sample_rate >= 55426) return 0x02;
    if(sample_rate >= 46009) return 0x03;
    if(sample_rate >= 37566) return 0x04;
    if(sample_rate >= 27713) return 0x05;
    if(sample_rate >= 23004) return 0x06;
    if(sample_rate >= 18783) return 0x07;
    if(sample_rate >= 13856) return 0x08;
    if(sample_rate >= 11502) return 0x09;
    if(sample_rate >=  9391) return 0x0a;
    return 0x0b;
}

#ifdef MAAC_NO_STDMATH

#define maac_math_memcpy(x,y) maac_assert( sizeof(*(x)) == sizeof(*(y)) ); maac_memcpy( (x), (y), sizeof *(y))

#else

#ifdef __cplusplus
#include <cmath>
#else
#include <math.h>
#endif

#define maac_sqrt(x)   maac_flt_cast(sqrt(x))
#define maac_pow(x,n)  maac_flt_cast(pow(x,n))

#endif

MAAC_PRIVATE
maac_flt maac_inv_sqrt(maac_flt x) {
#ifdef MAAC_NO_STDMATH
    maac_u32 i;
    /* specifically using 32-bit floats here to avoid using int64 */
    maac_f32 y;
    maac_f32 fx;

    fx = (maac_f32)x;

    maac_math_memcpy(&i, &fx);
    i = 0x5F1FFFF9 - (i >> 1);
    maac_math_memcpy(&y, &i);

    fx = y * 0.703952253f * ( 2.38924456f - fx * y * y );
    return maac_flt_cast(fx);
#else
    return MAAC_FLT_C(1.0) / maac_sqrt(x);
#endif
}

#ifdef MAAC_NO_STDMATH
/* pre-comuted pow(2,-3.0/4.0) through pow(2,3.0/4.0) */
static const maac_flt maac_pow2_fourths[7] = {
#ifdef MAAC_DOUBLE_PRECISION
    MAAC_F64_C(0.59460355750136051),
    MAAC_F64_C(0.70710678118654757),
    MAAC_F64_C(0.84089641525371450),
    MAAC_F64_C(1.0000000000000000),
    MAAC_F64_C(1.1892071150027210),
    MAAC_F64_C(1.4142135623730951),
    MAAC_F64_C(1.6817928305074290)
#else
    MAAC_F32_C(0.594603558),
    MAAC_F32_C(0.707106781),
    MAAC_F32_C(0.840896415),
    MAAC_F32_C(1.00000000),
    MAAC_F32_C(1.18920712),
    MAAC_F32_C(1.41421356),
    MAAC_F32_C(1.68179283)
#endif
};
#endif

MAAC_PRIVATE
maac_flt maac_pow2_xdiv4(maac_s16 x) {
#ifdef MAAC_NO_STDMATH
    /* so scalefactors in AAC follow the pattern of being
      2.0^(x/4)

      There's also a 0.5^(x/4) used but we'll get to that in a second.

      2.0^(x/4) can be broken down, say x is 9 -- 9/4 is the same as 8/4 + 1/4.
      And x^(y+z) == x^y * x^z.

      Again with x=9 we can break this down into:
      2.0^(9/4)
          == 2.0^((8/4) + (1/4))
          == 2.0^(8/4) * 2.0^(1/4)
          == 2^2 * 2^(1/4)
      Oh would you look at that, it's an integer power of two and a non-integer
      power of 2, and integer powers of 2 are easy to compute. It's just 1 << exponent.

      This means we can think of this formula as:
      (1 << int(x/4)) * (2.0^(x % 4))

      So one option would be something like:

      float f = (float)((1 << x/4)) * (2.0^(x % 4))

      We can have pre-computed 2^(x/4) values, we only need 4.

      Now for a negative power - say x was  -9, so our exponent is -2,
      you just compute the positive power then inverse, so:

      2^-2 == 1.0 / (2^2)

      So we *could* do something like:
      float f = ((float)(1 << abs(x)/4)) * (2.0^(abs(x) % 4))
      if(x < 0) f = 1.0/f;

      But what's great about power-of-two floats is they're all zero bits
      except the exponent section, so instead of branching on whether
      or not x is positive/negative, we can just take the base
      exponent value of 127 and add (x/4).

      If x is negative, we get a lower exponent and the equivalent of
      1.0 / (1 << exp). If x is positive, we get 1 << exp.

      So now we've got our power-of-two float, we just need to multiply
      with a pre-computed pow(2,(x%4)/4 and blammo, we have our value
      without resorting to a huge lookup table.

      Which is *great* because one thing about the AAC standards that drive
      me nuts is - I can't find limits. If somebody could tell me where
      it says "scale factors are in the range x to y" that would be great,
      because then I could make lookup tables with confidence.

      For handling 0.5^(x/4) we instead compute 2^(-x/4), since that
      computes 1/(2^(x/4)) - which is equivalent.
    */

    const maac_s32 x4 = x / 4;
    maac_s32 exp = 127;
    maac_u32 u;
    maac_f32 f;

    /*  ensure we don't overflow/underflow the exponent part of the float -
        exponent is 8 bits with 0 and 255 being reserved and 127 = 1.0,
        ensure we remain under 255 and over 0 */
    maac_assert(x4 < 128);
    maac_assert(x4 > -127);

    u = ((exp + x4) << 23) & MAAC_U32_C(0x7F800000);
    maac_math_memcpy(&f, &u);

    return maac_flt_cast(f) * maac_pow2_fourths[(x % 4)+3];
#else
    return maac_pow(MAAC_FLT_C(2.0), MAAC_FLT_C(0.25) * (maac_flt_cast(x)));
#endif
}

#ifdef MAAC_NO_STDMATH
static maac_f32
maac_frexpf(maac_f32 x, int* e) {
    /* extract the 8 exponent bits */
    maac_u32 u;
    maac_u32 t;

    maac_math_memcpy(&u, &x);
    t = u >> 23 & 0xff;

    /* if our exponent is zero, we're either dealing with 0 or a subnormal */
    if(t == 0) {
        if(x == 0.0f) { /* phew! */
            *e = 0;
            return x;
        }

        /* scale up by 2^64 (exponent = 191) */
        x = maac_frexpf(x * MAAC_F32_C(18446744073709551616.0), e);
        *e -= 64;
        return x;
    } else if(t == 0xff) {
        /* infinity or nan */
        return x;
    }

    /* we want to return a fraction and exponent such that
    frac * 2^exp = arg,
    with frac being in the range of 0.5,1.0 - with the 1.0 values excluded.

    So what we're *really* returning is something like:
    frac = ??
    exp = ceil(log2(x))

    the output range will be 1 to 128 */
    
    *e = t - 126; /* this is equivalent of ceil(log2(x)) - basically converting x
    into base2 without having to get into implementing log2 */

    /* clear out the original exponent bits, keep sign and mantissa*/
    u &= MAAC_U32_C(0x807fffff);
    /* set the exponent to 126 - so now we have a value in the range of 0.5 -> 1.0 */
    u |= MAAC_U32_C(0x3f000000);
    maac_math_memcpy(&x, &u);
    return x;
}

maac_const
static
maac_f32
maac_ldexpf(maac_f32 x, int n) {
    maac_f32 u;
    maac_f32 t1;
    maac_f32 t2;
    maac_f32 y;
    maac_u32 rem;

    y = x;

    /* similar to  above - we shouldn't ever have particularly large values of n
       since our input ranges will only be 0 - 8191 */
    if(n > 127) {
        rem = MAAC_U32_C(0x7f000000); /* equivalent to 0x1p127f */
        maac_math_memcpy(&t1, &rem);

        rem = 2;
        while(rem--) {
            y *= t1;
            n -= 127;
            if(n <= 127) break;
        }
        if(n > 127) n = 127;
    } else if(n < -126) {
        rem = MAAC_U32_C(0x00800000); /* equivalent to 0x1p-126f */
        maac_math_memcpy(&t1, &rem);
        rem = MAAC_U32_C(0x4b800000); /* equivalent to 0x1p24f */
        maac_math_memcpy(&t2, &rem);
        rem = 2;
        while(rem--) {
            y *= t1 - t2;
            n += 126 - 24;
            if(n >= -126) break;
        }
        if(n < -126) n = -126;
    }
    rem = (MAAC_U32_C(0x7f) + n) << 23;
    maac_math_memcpy(&u, &rem);
    return y * u;
}
#endif

MAAC_PRIVATE
maac_flt maac_cbrt(maac_u16 x) {
#ifdef MAAC_NO_STDMATH
/* https://people.freebsd.org/~lstewart/references/apple_tr_kt32_cuberoot.pdf */
    maac_s32 exp;
    maac_s32 shx;
    maac_f32 fx;
    maac_f32 fr;
    maac_f32 r;

    if(x == 0) return MAAC_FLT_C(0.0);

    fx = maac_f32_cast(x);

    fr = maac_frexpf(fx, &exp);
    shx = exp % 3;
    if(shx) {
        shx -= 3;
    }
    exp = (exp - shx) / 3;
    fr = maac_ldexpf(fr, shx);

    fr = (-0.46946116f * fr + 1.072302f) * fr + 0.3812513f;
    r = maac_ldexpf(fr, exp);

    r = (2.0f/3.0f) * r + (1.0f/3.0f) * fx / (r * r);
    r = (2.0f/3.0f) * r + (1.0f/3.0f) * fx / (r * r);

    return maac_flt_cast(r);
#else
    return maac_pow(maac_flt_cast(x), MAAC_FLT_C(1.0) / MAAC_FLT_C(3.0));
#endif
}

#ifdef MAAC_NO_STDSTRING

MAAC_PRIVATE
void* maac_memcpy(void* maac_restrict _dest, const void* maac_restrict _src, size_t len) {
    unsigned char* dest = (unsigned char *)_dest;
    const unsigned char* src = (const unsigned char *)_src;
    while(len--) {
        dest[len] = src[len];
    }
    return _dest;
}

#else

#ifndef MAAC_NO_STDSTRING
#ifdef __cplusplus
#include <cstring>
#else
#include <string.h>
#endif
#endif


MAAC_PRIVATE
void* maac_memcpy(void* maac_restrict _dest, const void* maac_restrict _src, size_t len) {
    return memcpy(_dest, _src, len);
}

#endif

#ifdef MAAC_NO_STDSTRING

MAAC_PRIVATE
void* maac_memset(void* _dest, int val, size_t len)
{
    unsigned char* dest = (unsigned char *)_dest;
    unsigned char c = (unsigned char)val;
    while(len--) {
        dest[len] = c;
    }
    return _dest;
}

#else


MAAC_PRIVATE
void* maac_memset(void* _dest, int val, size_t len) {
    return memset(_dest, val, len);
}

#endif

static maac_u32 maac_random_seed = MAAC_U32_C(0xbabab0ee);

MAAC_PUBLIC
void
maac_srand(maac_u32 val) {
    maac_random_seed = val;
}

MAAC_PRIVATE
maac_u32
maac_rand_seed(void) {
    return maac_random_seed;
}

static maac_inline void
maac_pce_reset(maac_pce* p) {
    maac_memset(p, 0, sizeof *p);
}

MAAC_PUBLIC
void
maac_pce_init(maac_pce* p) {
    maac_pce_reset(p);
}

MAAC_PUBLIC
MAAC_RESULT
maac_pce_decode(maac_pce* maac_restrict p, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;

    switch(p->state) {
        case MAAC_PCE_STATE_ELEMENT_INSTANCE_TAG: {
            if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
            maac_pce_reset(p);
        
            p->element_instance_tag = (maac_u8)maac_bitreader_read(br, 4);
            p->state = MAAC_PCE_STATE_PROFILE;
            goto maac_pce_profile;
        }

        case MAAC_PCE_STATE_PROFILE: {
            maac_pce_profile:
            if( (res = maac_bitreader_fill(br, 2)) != MAAC_OK) return res;
        
            p->profile = (maac_u8)maac_bitreader_read(br, 2);
            p->state = MAAC_PCE_STATE_SAMPLING_FREQUENCY_INDEX;
            goto maac_pce_sampling_frequency_index;
        }

        case MAAC_PCE_STATE_SAMPLING_FREQUENCY_INDEX: {
            maac_pce_sampling_frequency_index:
            if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
        
            p->sampling_frequency_index = (maac_u8)maac_bitreader_read(br, 4);
            p->state = MAAC_PCE_STATE_NUM_FRONT_CHANNEL_ELEMENTS;
            goto maac_pce_num_front_channel_elements;
        }

        case MAAC_PCE_STATE_NUM_FRONT_CHANNEL_ELEMENTS: {
            maac_pce_num_front_channel_elements:
            if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
        
            p->num_front_channel_elements = (maac_u8)maac_bitreader_read(br, 4);
            p->state = MAAC_PCE_STATE_NUM_SIDE_CHANNEL_ELEMENTS;
            goto maac_pce_num_side_channel_elements;
        }

        case MAAC_PCE_STATE_NUM_SIDE_CHANNEL_ELEMENTS: {
            maac_pce_num_side_channel_elements:
            if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
        
            p->num_side_channel_elements = (maac_u8)maac_bitreader_read(br, 4);
            p->state = MAAC_PCE_STATE_NUM_BACK_CHANNEL_ELEMENTS;
            goto maac_pce_num_back_channel_elements;
        }

        case MAAC_PCE_STATE_NUM_BACK_CHANNEL_ELEMENTS: {
            maac_pce_num_back_channel_elements:
            if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
        
            p->num_back_channel_elements = (maac_u8)maac_bitreader_read(br, 4);
            p->state = MAAC_PCE_STATE_NUM_LFE_CHANNEL_ELEMENTS;
            goto maac_pce_num_lfe_channel_elements;
        }

        case MAAC_PCE_STATE_NUM_LFE_CHANNEL_ELEMENTS: {
            maac_pce_num_lfe_channel_elements:
            if( (res = maac_bitreader_fill(br, 2)) != MAAC_OK) return res;
        
            p->num_lfe_channel_elements = (maac_u8)maac_bitreader_read(br, 2);
            p->state = MAAC_PCE_STATE_NUM_ASSOC_DATA_ELEMENTS;
            goto maac_pce_num_assoc_data_elements;
        }

        case MAAC_PCE_STATE_NUM_ASSOC_DATA_ELEMENTS: {
            maac_pce_num_assoc_data_elements:
            if( (res = maac_bitreader_fill(br, 3)) != MAAC_OK) return res;
        
            p->num_assoc_data_elements = (maac_u8)maac_bitreader_read(br, 3);
            p->state = MAAC_PCE_STATE_NUM_VALID_CC_ELEMENTS;
            goto maac_pce_num_valid_cc_elements;
        }

        case MAAC_PCE_STATE_NUM_VALID_CC_ELEMENTS: {
            maac_pce_num_valid_cc_elements:
            if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
        
            p->num_valid_cc_elements = (maac_u8)maac_bitreader_read(br, 4);
            p->state = MAAC_PCE_STATE_MONO_MIXDOWN_PRESENT;
            goto maac_pce_mono_mixdown_present;
        }

        case MAAC_PCE_STATE_MONO_MIXDOWN_PRESENT: {
            maac_pce_mono_mixdown_present:
            if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
        
            p->mono_mixdown_present = (maac_u8)maac_bitreader_read(br, 1);
            p->state = MAAC_PCE_STATE_MONO_MIXDOWN_ELEMENT_NUMBER;
            goto maac_pce_mono_mixdown_element_number;
        }

        case MAAC_PCE_STATE_MONO_MIXDOWN_ELEMENT_NUMBER: {
            maac_pce_mono_mixdown_element_number:
            if(p->mono_mixdown_present) {
                if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
        
                p->mono_mixdown_element_number = (maac_u8)maac_bitreader_read(br, 4);
            }
            p->state = MAAC_PCE_STATE_STEREO_MIXDOWN_PRESENT;
            goto maac_pce_stereo_mixdown_present;
        }

        case MAAC_PCE_STATE_STEREO_MIXDOWN_PRESENT: {
            maac_pce_stereo_mixdown_present:
            if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
        
            p->stereo_mixdown_present = (maac_u8)maac_bitreader_read(br, 1);
            p->state = MAAC_PCE_STATE_STEREO_MIXDOWN_ELEMENT_NUMBER;
            goto maac_pce_stereo_mixdown_element_number;
        }

        case MAAC_PCE_STATE_STEREO_MIXDOWN_ELEMENT_NUMBER: {
            maac_pce_stereo_mixdown_element_number:
            if(p->stereo_mixdown_present) {
                if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
        
                p->stereo_mixdown_element_number = (maac_u8)maac_bitreader_read(br, 4);
            }
            p->state = MAAC_PCE_STATE_MATRIX_MIXDOWN_IDX_PRESENT;
            goto maac_pce_matrix_mixdown_idx_present;
        }

        case MAAC_PCE_STATE_MATRIX_MIXDOWN_IDX_PRESENT: {
            maac_pce_matrix_mixdown_idx_present:
            if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
        
            p->matrix_mixdown_idx_present = (maac_u8)maac_bitreader_read(br, 1);
            p->state = MAAC_PCE_STATE_MATRIX_MIXDOWN_IDX;
            goto maac_pce_matrix_mixdown_idx;
        }

        case MAAC_PCE_STATE_MATRIX_MIXDOWN_IDX: {
            maac_pce_matrix_mixdown_idx:
            if(p->matrix_mixdown_idx_present) {
                if( (res = maac_bitreader_fill(br, 2)) != MAAC_OK) return res;
        
                p->matrix_mixdown_idx = (maac_u8)maac_bitreader_read(br, 2);
                p->state = MAAC_PCE_STATE_PSEUDO_SURROUND_ENABLE;
                goto maac_pce_pseudo_surround_enable;
            }

            p->state = MAAC_PCE_STATE_FRONT_ELEMENT_IS_CPE;
            goto maac_pce_front_element_is_cpe;
        }

        case MAAC_PCE_STATE_PSEUDO_SURROUND_ENABLE: {
            maac_pce_pseudo_surround_enable:
            if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
        
            p->pseudo_surround_enable = (maac_u8)maac_bitreader_read(br, 1);
            p->state = MAAC_PCE_STATE_FRONT_ELEMENT_IS_CPE;
            goto maac_pce_front_element_is_cpe;
        }

        case MAAC_PCE_STATE_FRONT_ELEMENT_IS_CPE: {
            maac_pce_front_element_is_cpe:
            while(p->_i < p->num_front_channel_elements) {
                if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
        
                p->front_element_is_cpe |= (maac_u16)(maac_bitreader_read(br, 1) << p->_i);
                p->state = MAAC_PCE_STATE_FRONT_ELEMENT_TAG_SELECT;
                goto maac_pce_front_element_tag_select;
            }
            p->_i = 0;
            p->state = MAAC_PCE_STATE_SIDE_ELEMENT_IS_CPE;
            goto maac_pce_side_element_is_cpe;
        }

        case MAAC_PCE_STATE_FRONT_ELEMENT_TAG_SELECT: {
            maac_pce_front_element_tag_select:
            if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
        
            p->front_element_tag_select[p->_i/2] &= (0xf0 >> (4 * (p->_i % 2)));
            p->front_element_tag_select[p->_i/2] |= maac_bitreader_read(br, 4) << (4 * (p->_i % 2));
            p->_i++;
            p->state = MAAC_PCE_STATE_FRONT_ELEMENT_IS_CPE;
            goto maac_pce_front_element_is_cpe;
        }

        case MAAC_PCE_STATE_SIDE_ELEMENT_IS_CPE: {
            maac_pce_side_element_is_cpe:
            while(p->_i < p->num_side_channel_elements) {
                if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
        
                p->side_element_is_cpe |= (maac_u16)(maac_bitreader_read(br, 1) << p->_i);
                p->state = MAAC_PCE_STATE_SIDE_ELEMENT_TAG_SELECT;
                goto maac_pce_side_element_tag_select;
            }
            p->_i = 0;
            p->state = MAAC_PCE_STATE_BACK_ELEMENT_IS_CPE;
            goto maac_pce_back_element_is_cpe;
        }

        case MAAC_PCE_STATE_SIDE_ELEMENT_TAG_SELECT: {
            maac_pce_side_element_tag_select:
            if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
        
            p->side_element_tag_select[p->_i/2] &= (0xf0 >> (4 * (p->_i % 2)));
            p->side_element_tag_select[p->_i/2] |= maac_bitreader_read(br, 4) << (4 * (p->_i % 2));
            p->_i++;
            p->state = MAAC_PCE_STATE_SIDE_ELEMENT_TAG_SELECT;
            goto maac_pce_side_element_tag_select;
        }

        case MAAC_PCE_STATE_BACK_ELEMENT_IS_CPE: {
            maac_pce_back_element_is_cpe:
            while(p->_i < p->num_back_channel_elements) {
                if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
        
                p->back_element_is_cpe |= (maac_u16)(maac_bitreader_read(br, 1) << p->_i);
                p->state = MAAC_PCE_STATE_BACK_ELEMENT_TAG_SELECT;
                goto maac_pce_back_element_tag_select;
            }
            p->_i = 0;
            p->state = MAAC_PCE_STATE_LFE_ELEMENT_TAG_SELECT;
            goto maac_pce_lfe_element_tag_select;
        }

        case MAAC_PCE_STATE_BACK_ELEMENT_TAG_SELECT: {
            maac_pce_back_element_tag_select:
            if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
        
            p->back_element_tag_select[p->_i/2] &= (0xf0 >> (4 * (p->_i % 2)));
            p->back_element_tag_select[p->_i/2] |= maac_bitreader_read(br, 4) << (4 * (p->_i % 2));
            p->_i++;
            p->state = MAAC_PCE_STATE_BACK_ELEMENT_TAG_SELECT;
            goto maac_pce_back_element_tag_select;
        }

        case MAAC_PCE_STATE_LFE_ELEMENT_TAG_SELECT: {
            maac_pce_lfe_element_tag_select:
            while(p->_i < p->num_lfe_channel_elements) {
                if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
                p->lfe_element_tag_select[p->_i/2] &= (0xf0 >> (4 * (p->_i % 2)));
                p->lfe_element_tag_select[p->_i/2] |= maac_bitreader_read(br, 4) << (4 * (p->_i % 2));
                p->_i++;
            }

            p->_i = 0;
            p->state = MAAC_PCE_STATE_ASSOC_DATA_ELEMENT_TAG_SELECT;
            goto maac_pce_assoc_data_element_tag_select;
        }

        case MAAC_PCE_STATE_ASSOC_DATA_ELEMENT_TAG_SELECT: {
            maac_pce_assoc_data_element_tag_select:
            while(p->_i < p->num_assoc_data_elements) {
                if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
                p->assoc_data_element_tag_select[p->_i/2] &= (0xf0 >> (4 * (p->_i % 2)));
                p->assoc_data_element_tag_select[p->_i/2] |= maac_bitreader_read(br, 4) << (4 * (p->_i % 2));
                p->_i++;
            }
        
            p->_i = 0;
            p->state = MAAC_PCE_STATE_CC_ELEMENT_IS_IND_SW;
            goto maac_pce_cc_element_is_ind_sw;
        }

        case MAAC_PCE_STATE_CC_ELEMENT_IS_IND_SW: {
            maac_pce_cc_element_is_ind_sw:
            while(p->_i < p->num_valid_cc_elements) {
                if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
        
                p->cc_element_is_ind_sw |= (maac_u16)(maac_bitreader_read(br, 1) << p->_i);
                p->state = MAAC_PCE_STATE_VALID_CC_ELEMENT_TAG_SELECT;
                goto maac_pce_valid_cc_element_tag_select;
            }
            maac_bitreader_byte_align(br);
            p->_i = 0;
            p->state = MAAC_PCE_STATE_COMMENT_FIELD_BYTES;
            goto maac_pce_comment_field_bytes;
        }

        case MAAC_PCE_STATE_VALID_CC_ELEMENT_TAG_SELECT: {
            maac_pce_valid_cc_element_tag_select:
            p->valid_cc_element_tag_select[p->_i/2] &= (0xf0 >> (4 * (p->_i % 2)));
            p->valid_cc_element_tag_select[p->_i/2] |= maac_bitreader_read(br, 4) << (4 * (p->_i % 2));
            p->_i++;
        
            p->state = MAAC_PCE_STATE_CC_ELEMENT_IS_IND_SW;
            goto maac_pce_cc_element_is_ind_sw;
        }

        case MAAC_PCE_STATE_COMMENT_FIELD_BYTES: {
            maac_pce_comment_field_bytes:
            if( (res = maac_bitreader_fill(br, 8)) != MAAC_OK) return res;
        
            p->comment_field_bytes = (maac_u8)maac_bitreader_read(br, 8);
            p->state = MAAC_PCE_STATE_COMMENT_FIELD_DATA;
            goto maac_pce_comment_field_data;
        }

        case MAAC_PCE_STATE_COMMENT_FIELD_DATA: {
            maac_pce_comment_field_data:
            while(p->_i < p->comment_field_bytes) {
                if( (res = maac_bitreader_fill(br, 8)) != MAAC_OK) return res;
        
                p->comment_field_data[p->_i++] = (maac_u8)maac_bitreader_read(br, 8);
            }
            p->state = MAAC_PCE_STATE_ELEMENT_INSTANCE_TAG;
            break;
        }
    }

    return MAAC_OK;
}

MAAC_PRIVATE
void
maac_pns_process(const maac_pns_params* p) {
    const maac_ics* ics = p->ics;
    const maac_ics_info* info = &ics->info;
    const maac_u8 num_window_groups = maac_sfg_num_window_groups(info->scale_factor_grouping);
    const maac_scalefactor_bands b = maac_scalefactor_bandsf(info->window_sequence, p->sf_index);
    maac_u32 group_lengths = maac_window_group_lengths(info->window_sequence, info->scale_factor_grouping);

    maac_u8 g;
    maac_u8 group_len;
    maac_u8 w;
    maac_u8 k;
    maac_u8 i;
    maac_u8 idx;
    maac_u16 off;
    maac_u16 group_off;
    maac_u16 sfb;
    maac_u16 sfb_start;
    maac_u16 sfb_end;
    maac_s32 r;
    maac_flt sample;
    maac_flt energy;
    maac_flt scale;

    group_off = 0;
    for(g=0; g<num_window_groups; g++) {
        group_len = group_lengths & 0x0f;

        for(w=0; w<group_len; w++) {
            i = 0;
            k = 0;

            off = group_off + ((maac_u16)w) * 128;

            while(k < b.len) {
                idx = maac_section_idx(g, i);

                if(ics->section_data[idx].codebook == MAAC_NOISE_HCB) {
                    scale = maac_pow2_xdiv4(ics->scalefactors[maac_section_idx(g,k)]);
                    energy = MAAC_FLT_C(0.0);
                    sfb_start = b.offsets[k];
                    sfb_end = b.offsets[k+1];

                    for(sfb=sfb_start;sfb<sfb_end;sfb++) {
                        r = (maac_s32)maac_rand(p->rand_state);
                        sample = maac_flt_cast(r);
                        p->spectra[off + sfb] = sample;
                        energy += sample * sample;
                    }
                    energy = scale * maac_inv_sqrt(energy);
                    for(sfb=sfb_start;sfb<sfb_end;sfb++) {
                        p->spectra[off + sfb] *= energy;
                    }
                    k++;
                } else {
                    /* skip to end of section */
                    k = ics->section_data[idx].end;
                }

                if(k == ics->section_data[idx].end) {
                    i++;
                }
            }
        }

        group_off += ((maac_u16)group_len) * 128;
        group_lengths >>=4;
    }
}

MAAC_PRIVATE
void
maac_pulse_init(maac_pulse* p) {
    p->state = MAAC_PULSE_STATE_NUMBER;
}

MAAC_PRIVATE
MAAC_RESULT
maac_pulse_parse(maac_pulse* maac_restrict p, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;

    switch(p->state) {
        case MAAC_PULSE_STATE_NUMBER: {
            if( (res = maac_bitreader_fill(br, 2)) != MAAC_OK) return res;
            p->num_pulse = maac_bitreader_read(br,2) + 1;
            p->state = MAAC_PULSE_STATE_START_SFB;
            goto maac_pulse_state_start_sfb;
        }

        case MAAC_PULSE_STATE_START_SFB: {
            maac_pulse_state_start_sfb:
            if( (res = maac_bitreader_fill(br, 6)) != MAAC_OK) return res;
            p->start_sfb = maac_bitreader_read(br, 6);
            p->_n = 0;
            p->state = MAAC_PULSE_STATE_OFFSET;
            goto maac_pulse_state_offset;
        }

        case MAAC_PULSE_STATE_OFFSET: {
            maac_pulse_state_offset:
            if( (res = maac_bitreader_fill(br, 5)) != MAAC_OK) return res;
            p->pulses[p->_n] = maac_bitreader_read(br,5);
            p->state = MAAC_PULSE_STATE_AMP;
            goto maac_pulse_state_amp;
        }

        case MAAC_PULSE_STATE_AMP: {
            maac_pulse_state_amp:
            if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
            p->pulses[p->_n++] |= maac_bitreader_read(br, 4) << 5;
            if(p->_n == p->num_pulse) break;

            p->state = MAAC_PULSE_STATE_OFFSET;
            goto maac_pulse_state_offset;
        }

        default: MAAC_UNREACHABLE_RETURN(MAAC_UNREACHABLE);
    }

    p->state = MAAC_PULSE_STATE_NUMBER;
    return MAAC_OK;
}

/* returns MAAC_OK just after parsing the element
 * instance tag and prepare for maac_sce_decode */

MAAC_PRIVATE
MAAC_RESULT
maac_sce_sync(maac_sce* maac_restrict s, maac_bitreader* maac_restrict br);


MAAC_PUBLIC
MAAC_RESULT
maac_raw_config(maac_raw* r, const maac_u8* data, maac_u32 len) {
    maac_bitreader br;
    maac_u32 t;

    maac_bitreader_init(&br);
    br.data = data;
    br.len = len;

    if( (maac_bitreader_fill(&br,5)) != MAAC_OK) return MAAC_ERROR;
    t = maac_bitreader_read(&br, 5);
    if(t != 2) {
        return MAAC_UNSUPPORTED_AOT;
    }
    if( (maac_bitreader_fill(&br,4)) != MAAC_OK) return MAAC_ERROR;
    t = maac_bitreader_read(&br, 4);
    if(t == 15) {
        if( (maac_bitreader_fill(&br,24)) != MAAC_OK) return MAAC_ERROR;
        r->sample_rate = maac_bitreader_read(&br, 24);
        r->sf_index = maac_sampling_frequency_index(r->sample_rate);
    } else {
        r->sf_index = t;
        r->sample_rate = maac_sampling_frequency(r->sf_index);
    }
    if( (maac_bitreader_fill(&br, 4)) != MAAC_OK) return MAAC_ERROR;
    r->channel_configuration = maac_bitreader_read(&br,4);

    return MAAC_OK;
}

MAAC_PUBLIC
void
maac_raw_init(maac_raw* r) {
    r->state = MAAC_RAW_STATE_BLOCK_ID;
    r->sample_rate = 0;
    r->sf_index = 16; /* invalid index */
    r->ele_id = 0x08; /* invalid element */
    r->out_channels = NULL;
    r->num_out_channels = 0;
    r->channel_configuration = 16; /* invalid config */
    r->rand_state = maac_rand_seed();
    r->_c = 0;
}

MAAC_PUBLIC
MAAC_RESULT
maac_raw_sync(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;

    switch(r->state) {
        case MAAC_RAW_STATE_BLOCK_ID: {
            maac_raw_state_block_id:
            if( (res = maac_bitreader_fill(br, 3)) != MAAC_OK) return res;
            r->ele_id = (maac_u8)maac_bitreader_read(br,3);

            maac_assert(r->ele_id != MAAC_RAW_DATA_BLOCK_ID_CCE);

            switch(r->ele_id) {
                case MAAC_RAW_DATA_BLOCK_ID_SCE: {
                    r->state = MAAC_RAW_STATE_SCE;
                    maac_sce_init(&r->ele.sce);
                    return maac_sce_sync(&r->ele.sce, br);
                }
                case MAAC_RAW_DATA_BLOCK_ID_CPE: {
                    r->state = MAAC_RAW_STATE_CPE;
                    maac_cpe_init(&r->ele.cpe);
                    return maac_cpe_sync(&r->ele.cpe, br);
                }
                case MAAC_RAW_DATA_BLOCK_ID_CCE: {
                    return MAAC_CCE_NOT_IMPLEMENTED;
                }
                case MAAC_RAW_DATA_BLOCK_ID_LFE: {
                    r->state = MAAC_RAW_STATE_LFE;
                    maac_sce_init(&r->ele.sce);
                    return maac_sce_sync(&r->ele.sce, br);
                }
                case MAAC_RAW_DATA_BLOCK_ID_DSE: {
                    r->state = MAAC_RAW_STATE_DSE;
                    maac_dse_init(&r->ele.dse);
                    return maac_dse_sync(&r->ele.dse, br);
                }
                case MAAC_RAW_DATA_BLOCK_ID_PCE: {
                    r->state = MAAC_RAW_STATE_PCE;
                    maac_pce_init(&r->ele.pce);
                    break;
                }
                case MAAC_RAW_DATA_BLOCK_ID_FIL: {
                    r->state = MAAC_RAW_STATE_FIL;
                    maac_fil_init(&r->ele.fil);
                    return maac_fil_sync(&r->ele.fil, br);
                }
                case MAAC_RAW_DATA_BLOCK_ID_END: {
                    maac_bitreader_byte_align(br);
                    break;
                }
            }
            return MAAC_OK;
        }

        case MAAC_RAW_STATE_SCE: {
            if(r->ele.sce.state == MAAC_SCE_STATE_TAG) return maac_sce_sync(&r->ele.sce, br);

            if( (res = maac_raw_decode_sce(r, br, NULL)) != MAAC_OK) return res;
            goto maac_raw_state_block_id;
        }

        case MAAC_RAW_STATE_CPE: {
            if(r->ele.cpe.state == MAAC_CPE_STATE_TAG) return maac_cpe_sync(&r->ele.cpe, br);

            if( (res = maac_raw_decode_cpe(r, br, NULL, NULL)) != MAAC_OK) return res;
            goto maac_raw_state_block_id;
        }

        case MAAC_RAW_STATE_LFE: {
            if(r->ele.sce.state == MAAC_SCE_STATE_TAG) return maac_sce_sync(&r->ele.sce, br);
            if( (res = maac_raw_decode_lfe(r, br, NULL)) != MAAC_OK) return res;
            goto maac_raw_state_block_id;
        }

        case MAAC_RAW_STATE_DSE: {
            if(r->ele.dse.state == MAAC_DSE_STATE_ELEMENT_INSTANCE_TAG) return maac_dse_sync(&r->ele.dse, br);

            if( (res = maac_raw_decode_dse(r, br)) != MAAC_OK) return res;
            goto maac_raw_state_block_id;
        }

        case MAAC_RAW_STATE_PCE: {
            if( (res = maac_raw_decode_pce(r, br)) != MAAC_OK) return res;
            goto maac_raw_state_block_id;
        }

        case MAAC_RAW_STATE_FIL: {
            if(r->ele.fil.state <= MAAC_FIL_STATE_PAYLOAD_TYPE) return maac_fil_sync(&r->ele.fil, br);
            if( (res = maac_raw_decode_fil(r, br)) != MAAC_OK) return res;
            goto maac_raw_state_block_id;
        }

        default: break;
    }

    /* if we're here it means maac_raw_data_block_sync was called out-of-sequence */
    return MAAC_OUT_OF_SEQUENCE;
}

MAAC_PUBLIC
MAAC_RESULT
maac_raw_decode_fil(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;

    if(r->state != MAAC_RAW_STATE_FIL) {
        return MAAC_OUT_OF_SEQUENCE;
    }
    if( (res = maac_fil_decode(&r->ele.fil, br)) != MAAC_OK) {
        return res;
    }
    r->state = MAAC_RAW_STATE_BLOCK_ID;
    return MAAC_OK;
}

MAAC_PUBLIC
MAAC_RESULT
maac_raw_decode_dse(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;

    if(r->state != MAAC_RAW_STATE_DSE) {
        return MAAC_OUT_OF_SEQUENCE;
    }
    if( (res = maac_dse_decode(&r->ele.dse, br)) != MAAC_OK) {
        return res;
    }

    r->state = MAAC_RAW_STATE_BLOCK_ID;
    return MAAC_OK;
}

MAAC_PUBLIC
MAAC_RESULT
maac_raw_decode_pce(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;

    if(r->state != MAAC_RAW_STATE_PCE) {
        return MAAC_OUT_OF_SEQUENCE;
    }
    if( (res = maac_pce_decode(&r->ele.pce, br)) != MAAC_OK) {
        return res;
    }

    r->state = MAAC_RAW_STATE_BLOCK_ID;
    return MAAC_OK;
}

MAAC_PUBLIC
MAAC_RESULT
maac_raw_decode_sce(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br, maac_channel* maac_restrict c) {
    MAAC_RESULT res;
    maac_sce_decode_params p;

    if(r->state != MAAC_RAW_STATE_SCE) {
        return MAAC_OUT_OF_SEQUENCE;
    }
    if(r->sf_index == 16) {
        return MAAC_SF_INDEX_NOT_SET;
    }

    p.sf_index = r->sf_index;
    p.ch = c;
    p.rand_state = &r->rand_state;

    if( (res = maac_sce_decode(&r->ele.sce, br, &p)) != MAAC_OK) {
        return res;
    }

    r->state = MAAC_RAW_STATE_BLOCK_ID;
    return MAAC_OK;
}

MAAC_PUBLIC
MAAC_RESULT
maac_raw_decode_cpe(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br, maac_channel* maac_restrict left, maac_channel* maac_restrict right) {
    MAAC_RESULT res;
    maac_cpe_decode_params p;

    if(r->state != MAAC_RAW_STATE_CPE) {
        return MAAC_OUT_OF_SEQUENCE;
    }
    if(r->sf_index == 16) {
        return MAAC_SF_INDEX_NOT_SET;
    }

    p.sf_index = r->sf_index;
    p.l = left;
    p.r = right;
    p.rand_state = &r->rand_state;

    if( (res = maac_cpe_decode(&r->ele.cpe, br, &p)) != MAAC_OK) {
        return res;
    }

    r->state = MAAC_RAW_STATE_BLOCK_ID;
    return MAAC_OK;
}

MAAC_PUBLIC
MAAC_RESULT
maac_raw_decode_lfe(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br, maac_channel* maac_restrict c) {
    MAAC_RESULT res;
    maac_sce_decode_params p;

    if(r->state != MAAC_RAW_STATE_LFE) {
        return MAAC_OUT_OF_SEQUENCE;
    }
    if(r->sf_index == 16) {
        return MAAC_SF_INDEX_NOT_SET;
    }

    p.sf_index = r->sf_index;
    p.ch = c;
    p.rand_state = &r->rand_state;

    if( (res = maac_sce_decode(&r->ele.sce, br, &p)) != MAAC_OK) {
        return res;
    }

    r->state = MAAC_RAW_STATE_BLOCK_ID;
    return MAAC_OK;
}

MAAC_PUBLIC
MAAC_RESULT
maac_raw_decode(maac_raw* maac_restrict r, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;
    maac_channel* ch1;
    maac_channel* ch2;

    switch(r->state) {
        case MAAC_RAW_STATE_BLOCK_ID: {
            maac_raw_state_block_id:
            if( (res = maac_raw_sync(r, br)) != MAAC_OK) return res;
            switch(r->state) {
                case MAAC_RAW_STATE_BLOCK_ID: break;
                case MAAC_RAW_STATE_SCE: goto maac_raw_state_sce;
                case MAAC_RAW_STATE_CPE: goto maac_raw_state_cpe;
                case MAAC_RAW_STATE_LFE: goto maac_raw_state_lfe;
                case MAAC_RAW_STATE_FIL: goto maac_raw_state_fil;
                case MAAC_RAW_STATE_PCE: goto maac_raw_state_pce;
                case MAAC_RAW_STATE_DSE: goto maac_raw_state_dse;
                default: MAAC_UNREACHABLE_RETURN(MAAC_UNREACHABLE);
            }
            /* we break when we read the END element, so reset our output */
            r->_c = 0;
            return MAAC_OK;
        }

        case MAAC_RAW_STATE_SCE: {
            maac_raw_state_sce:
            ch1 = r->_c < r->num_out_channels ? &r->out_channels[r->_c] : NULL;
            if( (res = maac_raw_decode_sce(r, br, ch1)) != MAAC_OK) return res;
            r->_c++;
            goto maac_raw_state_block_id;
        }

        case MAAC_RAW_STATE_CPE: {
            maac_raw_state_cpe:
            ch1 = r->_c < r->num_out_channels ? &r->out_channels[r->_c] : NULL;
            ch2 = r->_c+1 < r->num_out_channels ? &r->out_channels[r->_c+1] : NULL;
            if( (res = maac_raw_decode_cpe(r, br, ch1, ch2)) != MAAC_OK) return res;
            r->_c += 2;
            goto maac_raw_state_block_id;
        }

        case MAAC_RAW_STATE_LFE: {
            maac_raw_state_lfe:
            ch1 = r->_c < r->num_out_channels ? &r->out_channels[r->_c] : NULL;
            if( (res = maac_raw_decode_lfe(r, br, ch1)) != MAAC_OK) return res;
            r->_c++;
            goto maac_raw_state_block_id;
        }

        case MAAC_RAW_STATE_FIL: {
            maac_raw_state_fil:
            if( (res = maac_raw_decode_fil(r, br)) != MAAC_OK) return res;
            goto maac_raw_state_block_id;
        }

        case MAAC_RAW_STATE_PCE: {
            maac_raw_state_pce:
            if( (res = maac_raw_decode_pce(r, br)) != MAAC_OK) return res;
            goto maac_raw_state_block_id;
        }

        case MAAC_RAW_STATE_DSE: {
            maac_raw_state_dse:
            if( (res = maac_raw_decode_dse(r, br)) != MAAC_OK) return res;
            goto maac_raw_state_block_id;
        }

        default: break;
    }

    MAAC_UNREACHABLE_RETURN(MAAC_UNREACHABLE);
}

static const maac_u8
maac_num_swb_long_window[16] = {
    41 /* 96 kHz */,
    41 /* 88.2 kHz */,
    47 /* 64 kHz */,
    49 /* 48 kHz */,
    49 /* 44.1 kHz */,
    51 /* 32 kHz */,
    47 /* 24 kHz */,
    47 /* 22.05 kHz */,
    43 /* 16 kHz */,
    43 /* 12 kHz */,
    43 /* 11.025 kHz */,
    40 /* 8 kHz */,
    40 /* 7.035 kHz */,
    0 /* reserved */,
    0 /* reserved */,
    0 /* reserved */
};

static const maac_u16
maac_swb_offset_long_window_index[16] = {
    0 /* 96 kHz */,
    0 /* 88.2 kHz */,
    42 /* 64 kHz */,
    90 /* 48 kHz */,
    90 /* 44.1 kHz */,
    140 /* 32 kHz */,
    192 /* 24 kHz */,
    192 /* 22.05 kHz */,
    240 /* 16 kHz */,
    240 /* 12 kHz */,
    240 /* 11.025 kHz */,
    284 /* 8 kHz */,
    284 /* 7.035 kHz */,
    0 /* reserved */,
    0 /* reserved */,
    0 /* reserved */
};

static const maac_u16
maac_swb_offset_long_window[325] = {
    /* 96 kHz, 88.2 kHz */
    0, 4, 8, 12, 16, 20, 24, 28,
    32, 36, 40, 44, 48, 52, 56, 64,
    72, 80, 88, 96, 108, 120, 132, 144,
    156, 172, 188, 212, 240, 276, 320, 384,
    448, 512, 576, 640, 704, 768, 832, 896,
    960, 1024,

    /* 64 kHz */
    0, 4, 8, 12, 16, 20, 24, 28,
    32, 36, 40, 44, 48, 52, 56, 64,
    72, 80, 88, 100, 112, 124, 140, 156,
    172, 192, 216, 240, 268, 304, 344, 384,
    424, 464, 504, 544, 584, 624, 664, 704,
    744, 784, 824, 864, 904, 944, 984, 1024,

    /* 48 kHz, 44.1 kHz */
    0, 4, 8, 12, 16, 20, 24, 28,
    32, 36, 40, 48, 56, 64, 72, 80,
    88, 96, 108, 120, 132, 144, 160, 176,
    196, 216, 240, 264, 292, 320, 352, 384,
    416, 448, 480, 512, 544, 576, 608, 640,
    672, 704, 736, 768, 800, 832, 864, 896,
    928, 1024,

    /* 32 kHz */
    0, 4, 8, 12, 16, 20, 24, 28,
    32, 36, 40, 48, 56, 64, 72, 80,
    88, 96, 108, 120, 132, 144, 160, 176,
    196, 216, 240, 264, 292, 320, 352, 384,
    416, 448, 480, 512, 544, 576, 608, 640,
    672, 704, 736, 768, 800, 832, 864, 896,
    928, 960, 992, 1024,

    /* 24 kHz, 22.05 kHz */
    0, 4, 8, 12, 16, 20, 24, 28,
    32, 36, 40, 44, 52, 60, 68, 76,
    84, 92, 100, 108, 116, 124, 136, 148,
    160, 172, 188, 204, 220, 240, 260, 284,
    308, 336, 364, 396, 432, 468, 508, 552,
    600, 652, 704, 768, 832, 896, 960, 1024,

    /* 16 kHz, 12 kHz, 11.025 kHz */
    0, 8, 16, 24, 32, 40, 48, 56,
    64, 72, 80, 88, 100, 112, 124, 136,
    148, 160, 172, 184, 196, 212, 228, 244,
    260, 280, 300, 320, 344, 368, 396, 424,
    456, 492, 532, 572, 616, 664, 716, 772,
    832, 896, 960, 1024,

    /* 8 kHz, 7.035 kHz */
    0, 12, 24, 36, 48, 60, 72, 84,
    96, 108, 120, 132, 144, 156, 172, 188,
    204, 220, 236, 252, 268, 288, 308, 328,
    348, 372, 396, 420, 448, 476, 508, 544,
    580, 620, 664, 712, 764, 820, 880, 944,
    1024,
};

static const maac_u8
maac_num_swb_short_window[16] = {
    12 /* 96 kHz */,
    12 /* 88.2 kHz */,
    12 /* 64 kHz */,
    14 /* 48 kHz */,
    14 /* 44.1 kHz */,
    14 /* 32 kHz */,
    15 /* 24 kHz */,
    15 /* 22.05 kHz */,
    15 /* 16 kHz */,
    15 /* 12 kHz */,
    15 /* 11.025 kHz */,
    15 /* 8 kHz */,
    15 /* 7.035 kHz */,
    0 /* reserved */,
    0 /* reserved */,
    0 /* reserved */
};

static const maac_u16
maac_swb_offset_short_window_index[16] = {
    0 /* 96 kHz */,
    0 /* 88.2 kHz */,
    0 /* 64 kHz */,
    13 /* 48 kHz */,
    13 /* 44.1 kHz */,
    13 /* 32 kHz */,
    28 /* 24 kHz */,
    28 /* 22.05 kHz */,
    44 /* 16 kHz */,
    44 /* 12 kHz */,
    44 /* 11.025 kHz */,
    60 /* 8 kHz */,
    60 /* 7.035 kHz */,
    0 /* reserved */,
    0 /* reserved */,
    0 /* reserved */
};

static const maac_u16
maac_swb_offset_short_window[76] = {
    /* 96 kHz, 88.2 kHz, 64 kHz */
    0, 4, 8, 12, 16, 20, 24, 32,
    40, 48, 64, 92, 128,

    /* 48 kHz, 44.1 kHz, 32 kHz */
    0, 4, 8, 12, 16, 20, 28, 36,
    44, 56, 68, 80, 96, 112, 128,

    /* 24 kHz, 22.05 kHz */
    0, 4, 8, 12, 16, 20, 24, 28,
    36, 44, 52, 64, 76, 92, 108, 128,

    /* 16 kHz, 12 kHz, 11.025 kHz */
    0, 4, 8, 12, 16, 20, 24, 28,
    32, 40, 48, 60, 72, 88, 108, 128,

    /* 8 kHz, 7.035 kHz */
    0, 4, 8, 12, 16, 20, 24, 28,
    36, 44, 52, 60, 72, 88, 108, 128,
};

maac_pure
MAAC_PRIVATE
maac_scalefactor_bands maac_scalefactor_bands_long(maac_u8 sf_index) {
    maac_scalefactor_bands s = { NULL, 0 };

    s.offsets = &maac_swb_offset_long_window[maac_swb_offset_long_window_index[sf_index]];
    s.len = maac_num_swb_long_window[sf_index];

    return s;
}

maac_pure
MAAC_PRIVATE
maac_scalefactor_bands maac_scalefactor_bands_short(maac_u8 sf_index) {
    maac_scalefactor_bands s = { NULL, 0 };

    s.offsets = &maac_swb_offset_short_window[maac_swb_offset_short_window_index[sf_index]];
    s.len = maac_num_swb_short_window[sf_index];

    return s;
}

maac_pure
MAAC_PRIVATE
maac_scalefactor_bands maac_scalefactor_bandsf(maac_u8 window_sequence, maac_u8 sf_index) {
    return window_sequence == MAAC_WINDOW_SEQUENCE_EIGHT_SHORT ?
      maac_scalefactor_bands_short(sf_index)
      :
      maac_scalefactor_bands_long(sf_index);
}

MAAC_PUBLIC
void
maac_sce_init(maac_sce* s) {
    /* TODO - remove memset with garbage data once we verify this works correctly with mostly garbage data */
    maac_memset(s, 0xab, sizeof *s);
    s->state = MAAC_SCE_STATE_TAG;
}

MAAC_PRIVATE
MAAC_RESULT
maac_sce_sync(maac_sce* maac_restrict s, maac_bitreader* maac_restrict br) {
    MAAC_RESULT res;

    /* this function MUST be called with s->state == MAAC_SCE_STATE_TAG, this is
     * checked by sce_decode/raw_sync so we just assume that's true */
    maac_assert(s->state == MAAC_SCE_STATE_TAG);

    if( (res = maac_bitreader_fill(br, 4)) != MAAC_OK) return res;
    s->element_instance_tag = (maac_u8)maac_bitreader_read(br, 4);

    s->state = MAAC_SCE_STATE_ICS;
    maac_ics_init(&s->ics);

    return MAAC_OK;
}

MAAC_PUBLIC
MAAC_RESULT
maac_sce_decode(maac_sce* maac_restrict s, maac_bitreader* maac_restrict br, const maac_sce_decode_params* maac_restrict p) {
    MAAC_RESULT res;
    maac_ics_decode_params ics_p;
    maac_filterbank_params fb_p;
    maac_tns_params tns_p;
    maac_pns_params pns_p;

    ics_p.common_window = 0;
    ics_p.sf_index = p->sf_index;
    ics_p.ch = p->ch;

    switch(s->state) {

        case MAAC_SCE_STATE_TAG: {
            if( (res = maac_sce_sync(s, br)) != MAAC_OK) return res;
            goto maac_sce_state_ics;
        }

        case MAAC_SCE_STATE_ICS: {
            maac_sce_state_ics:
            if( (res = maac_ics_decode(&s->ics, br, &ics_p)) != MAAC_OK) return res;
            s->state = MAAC_SCE_STATE_TAG;
            break;
        }

        default: MAAC_UNREACHABLE_RETURN(MAAC_UNREACHABLE);
    }

    if(p->ch != NULL) {
        pns_p.rand_state = p->rand_state;
        pns_p.sf_index = p->sf_index;
        pns_p.spectra = p->ch->samples;
        pns_p.ics = &s->ics;

        maac_pns_process(&pns_p);

        if(s->ics.tns_data_present) {
            tns_p.sf_index = p->sf_index;
            tns_p.info = &s->ics.info;
            maac_tns_process(&s->ics.tns, &p->ch->samples[0], &tns_p);
        }

        fb_p.window_sequence   = s->ics.info.window_sequence;
        fb_p.window_shape      = s->ics.info.window_shape;
        fb_p.window_shape_prev = p->ch->window_shape_prev;
        maac_filterbank(&p->ch->samples[0], &p->ch->samples[2048], &fb_p);
        p->ch->window_shape_prev = fb_p.window_shape;

        p->ch->n_samples = 1024;
        p->ch->_n = 0;
    }

    return MAAC_OK;
}

/* coef_compress = 0, coef_len = 3 */
static const maac_flt MAAC_TNS_INVQUANT_0_3[] = {
    /* value = 0 (0) */ MAAC_FLT_C(0.0000000000000000),
    /* value = 1 (1) */ MAAC_FLT_C(0.43388373911755806),
    /* value = 2 (2) */ MAAC_FLT_C(0.78183148246802969),
    /* value = 3 (3) */ MAAC_FLT_C(0.97492791218182351),
    /* value = 4 (-4) */ MAAC_FLT_C(-0.98480775301220802),
    /* value = 5 (-3) */ MAAC_FLT_C(-0.86602540378443860),
    /* value = 6 (-2) */ MAAC_FLT_C(-0.64278760968653925),
    /* value = 7 (-1) */ MAAC_FLT_C(-0.34202014332566871)
};

/* coef_compress = 0, coef_len = 4 */
static const maac_flt MAAC_TNS_INVQUANT_0_4[] = {
    /* value = 0 (0) */ MAAC_FLT_C(0.0000000000000000),
    /* value = 1 (1) */ MAAC_FLT_C(0.20791169081775931),
    /* value = 2 (2) */ MAAC_FLT_C(0.40673664307580015),
    /* value = 3 (3) */ MAAC_FLT_C(0.58778525229247314),
    /* value = 4 (4) */ MAAC_FLT_C(0.74314482547739413),
    /* value = 5 (5) */ MAAC_FLT_C(0.86602540378443860),
    /* value = 6 (6) */ MAAC_FLT_C(0.95105651629515353),
    /* value = 7 (7) */ MAAC_FLT_C(0.99452189536827329),
    /* value = 8 (-8) */ MAAC_FLT_C(-0.99573417629503447),
    /* value = 9 (-7) */ MAAC_FLT_C(-0.96182564317281904),
    /* value = 10 (-6) */ MAAC_FLT_C(-0.89516329135506234),
    /* value = 11 (-5) */ MAAC_FLT_C(-0.79801722728023949),
    /* value = 12 (-4) */ MAAC_FLT_C(-0.67369564364655721),
    /* value = 13 (-3) */ MAAC_FLT_C(-0.52643216287735572),
    /* value = 14 (-2) */ MAAC_FLT_C(-0.36124166618715292),
    /* value = 15 (-1) */ MAAC_FLT_C(-0.18374951781657034)
};

/* coef_compress = 1, coef_len = 3 */
static const maac_flt MAAC_TNS_INVQUANT_1_3[] = {
    /* value = 0 (0) */ MAAC_FLT_C(0.0000000000000000),
    /* value = 1 (1) */ MAAC_FLT_C(0.43388373911755806),
    /* value = 2 (-2) */ MAAC_FLT_C(-0.64278760968653925),
    /* value = 3 (-1) */ MAAC_FLT_C(-0.34202014332566871)
};

/* coef_compress = 1, coef_len = 4 */
static const maac_flt MAAC_TNS_INVQUANT_1_4[] = {
    /* value = 0 (0) */ MAAC_FLT_C(0.0000000000000000),
    /* value = 1 (1) */ MAAC_FLT_C(0.20791169081775931),
    /* value = 2 (2) */ MAAC_FLT_C(0.40673664307580015),
    /* value = 3 (3) */ MAAC_FLT_C(0.58778525229247314),
    /* value = 4 (-4) */ MAAC_FLT_C(-0.67369564364655721),
    /* value = 5 (-3) */ MAAC_FLT_C(-0.52643216287735572),
    /* value = 6 (-2) */ MAAC_FLT_C(-0.36124166618715292),
    /* value = 7 (-1) */ MAAC_FLT_C(-0.18374951781657034)
};

static const maac_flt* const MAAC_TNS_INVQUANT[] = {
    MAAC_TNS_INVQUANT_0_3, MAAC_TNS_INVQUANT_0_4, MAAC_TNS_INVQUANT_1_3, MAAC_TNS_INVQUANT_1_4
};

static const maac_u8 TNS_MAX_BANDS_1024[16] = {
    31 /* 96 kHz */,
    31 /* 88.2 kHz */,
    34 /* 64 kHz */,
    40 /* 48 kHz */,
    42 /* 44.1 kHz */,
    51 /* 32 kHz */,
    46 /* 24 kHz */,
    46 /* 22.050 kHz */,
    42 /* 16 kHz */,
    42 /* 12 kHz */,
    42 /* 11.025 kHz */,
    39 /* 8 kHz */,
    0 /* reserved */,
    0 /* reserved */,
    0 /* reserved */,
    0 /* reserved */
};

static const maac_u8 TNS_MAX_BANDS_128[16] = {
    9 /* 96 kHz */,
    9 /* 88.2 kHz */,
    10 /* 64 kHz */,
    14 /* 48 kHz */,
    14 /* 44.1 kHz */,
    14 /* 32 kHz */,
    14 /* 24 kHz */,
    14 /* 22.050 kHz */,
    14 /* 16 kHz */,
    14 /* 12 kHz */,
    14 /* 11.025 kHz */,
    14 /* 8 kHz */,
    0 /* reserved */,
    0 /* reserved */,
    0 /* reserved */,
    0 /* reserved */
};

MAAC_PRIVATE
MAAC_RESULT
maac_tns_parse(maac_tns* maac_restrict tns, maac_bitreader* maac_restrict br, maac_u8 window_sequence) {
    const maac_u8 n_filt_bits = window_sequence == MAAC_WINDOW_SEQUENCE_EIGHT_SHORT ? 1 : 2;
    const maac_u8 length_bits = window_sequence == MAAC_WINDOW_SEQUENCE_EIGHT_SHORT ? 4 : 6;
    const maac_u8 order_bits  = window_sequence == MAAC_WINDOW_SEQUENCE_EIGHT_SHORT ? 3 : 5;
    const maac_u8 num_windows = window_sequence == MAAC_WINDOW_SEQUENCE_EIGHT_SHORT ? 8 : 1;

    maac_u8 coef_bits = 0;
    MAAC_RESULT res;

    switch(tns->state) {
        case MAAC_TNS_STATE_N_FILT: {
            maac_tns_data_state_n_filt:
            if( (res = maac_bitreader_fill(br, n_filt_bits)) != MAAC_OK) return res;
            tns->window[tns->_g].n_filt = (maac_u8)maac_bitreader_read(br, n_filt_bits);

            if(tns->window[tns->_g].n_filt) {
                tns->state = MAAC_TNS_STATE_COEF_RES;
                goto maac_ins_data_state_coef_res;
            }

            goto maac_tns_data_g_incr;
        }

        case MAAC_TNS_STATE_COEF_RES: {
            maac_ins_data_state_coef_res:
            if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
            tns->window[tns->_g].coef_res = (maac_u8) maac_bitreader_read(br, 1);
            tns->_k = 0;
            tns->state = MAAC_TNS_STATE_LENGTH;
            goto maac_tns_data_state_length;
        }

        case MAAC_TNS_STATE_LENGTH: {
            maac_tns_data_state_length:
            if( (res = maac_bitreader_fill(br, length_bits)) != MAAC_OK) return res;
            tns->window[tns->_g].filt[tns->_k].length = (maac_u8) maac_bitreader_read(br, length_bits);

            tns->state = MAAC_TNS_STATE_ORDER;
            goto maac_tns_data_state_order;
        }

        case MAAC_TNS_STATE_ORDER: {
            maac_tns_data_state_order:
            if( (res = maac_bitreader_fill(br, order_bits)) != MAAC_OK) return res;
            tns->window[tns->_g].filt[tns->_k].order = (maac_u8) maac_bitreader_read(br, order_bits);

            if(tns->window[tns->_g].filt[tns->_k].order) {
                tns->state = MAAC_TNS_STATE_DIRECTION;
                goto maac_tns_data_state_direction;
            }

            goto maac_tns_data_k_incr;
        }

        case MAAC_TNS_STATE_DIRECTION: {
            maac_tns_data_state_direction:
            if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
            tns->window[tns->_g].filt[tns->_k].direction = (maac_u8) maac_bitreader_read(br, 1);

            tns->state = MAAC_TNS_STATE_COEF_COMPRESS;
            goto maac_tns_data_state_coef_compress;
        }

        case MAAC_TNS_STATE_COEF_COMPRESS: {
            maac_tns_data_state_coef_compress:
            if( (res = maac_bitreader_fill(br, 1)) != MAAC_OK) return res;
            tns->window[tns->_g].filt[tns->_k].coef_compress = (maac_u8) maac_bitreader_read(br, 1);
            tns->_i = 0;
            tns->state = MAAC_TNS_STATE_COEF;
            goto maac_tns_data_state_coef;
        }

        case MAAC_TNS_STATE_COEF: {
            maac_tns_data_state_coef:
            coef_bits = 3 + tns->window[tns->_g].coef_res - tns->window[tns->_g].filt[tns->_k].coef_compress;
            if( (res = maac_bitreader_fill(br, coef_bits)) != MAAC_OK) return res;

            tns->window[tns->_g].filt[tns->_k].coef[tns->_i/2] &= 0xf0 >> (4 * (tns->_i % 2));
            tns->window[tns->_g].filt[tns->_k].coef[tns->_i/2] |=
              (maac_u8) maac_bitreader_read(br, coef_bits) << (4 * (tns->_i % 2));
            goto maac_tns_data_i_incr;
        }

        default: MAAC_UNREACHABLE_RETURN(MAAC_UNREACHABLE);
    }

    maac_tns_data_i_incr:
    tns->_i++;
    if(tns->_i < tns->window[tns->_g].filt[tns->_k].order) {
        goto maac_tns_data_state_coef;
    }

    maac_tns_data_k_incr:
    tns->_k++;
    if(tns->_k < tns->window[tns->_g].n_filt) {
        tns->state = MAAC_TNS_STATE_LENGTH;
        goto maac_tns_data_state_length;
    }
    tns->state = MAAC_TNS_STATE_N_FILT;

    maac_tns_data_g_incr:
    tns->_g++;
    if(tns->_g == num_windows) {
        tns->_g = 0;
        return MAAC_OK;
    }
    goto maac_tns_data_state_n_filt;

    MAAC_UNREACHABLE_RETURN(MAAC_UNREACHABLE);
}



static void maac_decode_coef(maac_u8 order, maac_u8 coef_res_bits, maac_u8 coef_compress, maac_u8* coef, maac_flt* a) {
    const maac_flt* tns_data = MAAC_TNS_INVQUANT[2 * coef_compress + coef_res_bits - 3];

    maac_u8 c;
    maac_u8 i;
    maac_u8 m;
    maac_u8 max;
    maac_flt tmp2[MAAC_TNS_TOTAL_ORDER];
    maac_flt b[MAAC_TNS_TOTAL_ORDER];

    for(i=0;i<order;i++) {
        c = (coef[i/2] >> (4 * (i % 2))) & 0x0f;
        tmp2[i] = tns_data[c];
    }

    a[0] = MAAC_FLT_C(1.0);
    for(m = 1; m <= order; m++) {
        for(i = 1; i < m; i++) {
            b[i] = a[i] + tmp2[m - 1] * a[m - i];
        }
        for(i = 1; i < m; i++) {
            a[i]  = b[i];
        }
        a[m] = tmp2[m - 1];
        if(m > max) max = m;
    }
}

static void
maac_tns_ar_filter(maac_flt* spec, maac_s16 size, maac_s16 inc, maac_flt* lpc, maac_u8 order) {
    maac_s16 i;
    maac_u8 j;
    maac_flt state[MAAC_TNS_MAX_ORDER];
    maac_flt val;

    for(j=0; j < order; j++) {
        state[j] = MAAC_FLT_C(0.0);
    }

    for(i=0; i < size; i++) {
        val = *spec;
        for(j=0;j<order;j++) {
            val -= lpc[j+1] * state[j];
        }
        for(j=order-1;j>0;j--) {
            state[j] = state[j-1];
        }
        state[0] = val;
        *spec = val;
        spec = &spec[inc];
    }
}

MAAC_PRIVATE
void
maac_tns_process(maac_tns* maac_restrict tns, maac_flt* maac_restrict samples, const maac_tns_params* maac_restrict p) {
    const maac_u8 num_windows = p->info->window_sequence == MAAC_WINDOW_SEQUENCE_EIGHT_SHORT ? 8 : 1;

    const maac_u8 TNS_MAX_BANDS = p->info->window_sequence == MAAC_WINDOW_SEQUENCE_EIGHT_SHORT ?
      TNS_MAX_BANDS_128[p->sf_index] : TNS_MAX_BANDS_1024[p->sf_index];

    const maac_u8 TNS_MAX_ORDER = p->info->window_sequence == MAAC_WINDOW_SEQUENCE_EIGHT_SHORT ?
      MAAC_TNS_MAX_ORDER_128 : MAAC_TNS_MAX_ORDER_1024;

    const maac_scalefactor_bands b = maac_scalefactor_bandsf(p->info->window_sequence, p->sf_index);

    maac_u8 w;
    maac_u8 f;
    maac_u8 bottom;
    maac_u8 top;
    maac_u8 tns_order;

    maac_u16 start;
    maac_u16 end;
    maac_s16 size;
    maac_s16 inc;

    maac_flt lpc[MAAC_TNS_TOTAL_ORDER];

    maac_memset(lpc, 0, sizeof(lpc));

    for(w=0; w<num_windows; w++) {
        bottom = b.len;

        for(f=0; f<tns->window[w].n_filt; f++) {
            top = bottom;
            if(tns->window[w].filt[f].length <= top) {
                bottom = top - tns->window[w].filt[f].length;
            } else {
                bottom = 0;
            }
            tns_order = maac_min(tns->window[w].filt[f].order, TNS_MAX_ORDER);
            if(tns_order == 0) continue;

            maac_decode_coef(tns_order, 3 + tns->window[w].coef_res, tns->window[w].filt[f].coef_compress, tns->window[w].filt[f].coef, lpc);

            bottom = maac_min(bottom, TNS_MAX_BANDS);
            bottom = maac_min(bottom, p->info->max_sfb);

            top = maac_min(top, TNS_MAX_BANDS);
            top = maac_min(top, p->info->max_sfb);

            start = b.offsets[bottom];
            end   = b.offsets[top];
            if(start >= end) continue;

            size = (maac_s16)( end - start );

            if(tns->window[w].filt[f].direction) {
                inc = -1;
                start = end - 1;
            } else {
                inc = 1;
            }
            maac_tns_ar_filter(&samples[(w * 128) + start], size, inc, lpc, tns_order);
        }
    }

    return;
}

maac_const
MAAC_PRIVATE
maac_u32
maac_window_group_lengths(maac_u8 window_sequence, maac_u8 scale_factor_grouping) {
    maac_u32 lengths = 0;
    maac_u8 shift = 0;
    maac_u8 len = 1;
    maac_u8 b = 7;

    if(window_sequence != MAAC_WINDOW_SEQUENCE_EIGHT_SHORT) return 1;

    while(b--) {
        if( (scale_factor_grouping >> b) & 0x01) {
            len++;
        } else {
            lengths |= (len & 0x0f) << shift;
            shift += 4;
            len = 1;
        }
    }
    lengths |= (len << shift);
    return lengths;
}

#endif /* IMPLEMENTATION_DEFINED */
#endif


/*

Copyright (c) 2026 John Regan

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

*/
