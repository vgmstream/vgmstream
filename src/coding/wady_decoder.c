#include "coding.h"


/* originally only positives are stored (pre-init by copying negatives) */
static const int wady_table[64+64] = {
    0,   2,   4,   6,   8,   10,  12,  15,
    18,  21,  24,  28,  32,  36,  40,  44,
    49,  54,  59,  64,  70,  76,  82,  88,
    95,  102, 109, 116, 124, 132, 140, 148,
    160, 170, 180, 190, 200, 210, 220, 230,
    240, 255, 270, 285, 300, 320, 340, 360,
    380, 400, 425, 450, 475, 500, 525, 550,
    580, 610, 650, 700, 750, 800, 900, 1000,
   -0,  -2,  -4,  -6,  -8,  -10, -12, -15,
   -18, -21, -24, -28, -32, -36, -40, -44,
   -49, -54, -59, -64, -70, -76, -82, -88,
   -95, -102,-109,-116,-124,-132,-140,-148,
   -160,-170,-180,-190,-200,-210,-220,-230,
   -240,-255,-270,-285,-300,-320,-340,-360,
   -380,-400,-425,-450,-475,-500,-525,-550,
   -580,-610,-650,-700,-750,-800,-900,-1000,
};

/* There is another decoding mode mainly for SFX. Uses headered frames/blocks (big),
 * L-frame then R-frame, DPCM uses another table plus a RLE/LZ-like mode */

/* Marble engine WADY decoder, decompiled from the exe
 * (some info from: https://github.com/morkt/GARbro/blob/master/ArcFormats/Marble/AudioWADY.cs) */
void decode_wady(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    int i, sample_pos = 0;
    off_t frame_offset = stream->offset; /* frame size is 1 */
    int32_t hist = stream->adpcm_history1_32;
    int scale = stream->adpcm_scale;

    for (i = first_sample; i < first_sample + samples_to_do; i++) {
        int8_t code = read_s8(frame_offset + i, stream->streamfile);

        if (code & 0x80)
            hist = (code << 9); /* PCM */
        else
            hist += scale * wady_table[code]; /* DPCM */

        outbuf[sample_pos] = hist; /* no clamp */
        sample_pos += channelspacing;
    }

    stream->adpcm_history1_32 = hist;
}
