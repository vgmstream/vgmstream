#ifndef _RESAMPLER_H_
#define _RESAMPLER_H_

#include "sbuf.h"

/* Resamples input samples to output samples depending on ratio (src_rate / dst_rate),
 * were lower tario = returns more, higher = returns less (so 0.5 = double samples).
 * Ratio 1.0 is allowed (some resamplers may act as filters).
 * Returns float samples.
 */


/* types varying points+coefs, from worst/fastest to best/slowest */
typedef enum {
    RESAMPLER_TYPE_DEFAULT,
    RESAMPLER_TYPE_LINEAR,
    #if 0
    RESAMPLER_TYPE_SPU,
    RESAMPLER_TYPE_XA,
    #endif
    RESAMPLER_TYPE_HERMITE4,
    RESAMPLER_TYPE_LAGRANGE4,
    RESAMPLER_TYPE_LAGRANGE6,
    RESAMPLER_TYPE_SINC,
} resampler_type_t;

typedef struct {
    resampler_type_t type;
    int channels;
    double ratio;
    //int quality; //for sinc
} resampler_cfg_t;

typedef struct resampler_ctx_t resampler_ctx_t;

/* new resampler */
void* resampler_init(resampler_cfg_t* cfg);

void resampler_free(resampler_ctx_t* ctx);

void resampler_reset(resampler_ctx_t* ctx);

//void resampler_set_ratio(void* ctx, double ratio);

/* error codes returned below */
#define RESAMPLER_RES_OK     0
#define RESAMPLER_RES_ERROR  -1
//#define RESAMPLER_RES_FEED   -2 //not enough samples
//#define RESAMPLER_RES_FULL   -3 //too many samples

/* add src samples; use when resampler_get_samples returns RESAMPLER_FEED */
int resampler_push_samples(resampler_ctx_t* ctx, sbuf_t* src);

/* set callback to read src samples as needed */
//void resampler_pull_samples(void* ctx, callback_t* cb);

/* get current resampled samples; valid until next push/pull */
int resampler_get_samples(resampler_ctx_t* ctx, sbuf_t* dst);

/* consume pending samples at EOF */
int resampler_drain_samples(resampler_ctx_t* ctx, sbuf_t* dst);

#endif
