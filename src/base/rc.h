#ifndef _RC_H_
#define _RC_H_

// return codes
typedef enum {
    // standard
    RC_RENDER_OK        = 0,
    // End Of Render, after reaching target samples (w/ N loops, mixing, etc).
    RC_RENDER_EOR       = 1, 
    // Generic decode errors
    RC_LAYOUT_ERROR     = -1,
    RC_RENDER_ERROR     = -2,
} rc_t;

#endif
