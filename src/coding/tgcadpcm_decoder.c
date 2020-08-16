#include "coding.h"

/* Decodes SunPlus' ADPCM codec used on the Tiger Game.com.
 * Highly improved, optimised signed 16-bit version of the algorithm. */
static const int16_t slope_table[8][16] =
{
    {    0,     0,  256,  -256,  512,  -512, 1024, -1024,  1536,  -1536,  2048,  -2048,  3072,  -3072,  4096,  -4096 }, 
    {  256,  -256,  768,  -768, 1280, -1280, 2304, -2304,  3328,  -3328,  4352,  -4352,  6400,  -6400,  8448,  -8448 }, 
    {  512,  -512, 1280, -1280, 2048, -2048, 3584, -3584,  5120,  -5120,  6656,  -6656,  9728,  -9728, 12800, -12800 }, 
    {  768,  -768, 1792, -1792, 2816, -2816, 4864, -4864,  6912,  -6912,  8960,  -8960, 10496, -10496, 17152, -17152 }, 
    { 1024, -1024, 2304, -2304, 3584, -3584, 6144, -6144,  8704,  -8704, 11264, -11264, 16384, -16384, 21504, -21504 }, 
    { 1280, -1280, 2816, -2816, 4352, -4352, 7424, -7424, 10496, -10496, 13568, -13568, 19712, -19712, 28416, -28416 }, 
    { 1536, -1536, 3328, -3328, 5120, -5120, 8704, -8704, 12288, -12288, 15872, -15872, 23040, -23040, 30208, -30208 }, 
    { 1792, -1792, 3840, -3840, 5888, -5888, 9984, -9984, 14080, -14080, 18176, -18176, 26368, -26368, 32000, -32000 },
};

static const uint8_t next_step[8][16] =
{
    { 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 4 },
    { 0, 0, 0, 0, 1, 1, 1, 1, 3, 3, 3, 3, 4, 4, 5, 5 },
    { 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 5, 5, 6, 6 },
    { 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 5, 5, 6, 6, 7, 7 },
    { 3, 3, 3, 3, 4, 4, 4, 4, 6, 6, 6, 6, 7, 7, 7, 7 },
    { 4, 4, 4, 4, 5, 5, 5, 5, 7, 7, 7, 7, 7, 7, 7, 7 },
    { 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7 },
    { 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 }
};

void decode_tgc(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int32_t first_sample, int32_t samples_to_do)
{
    for (int i = first_sample, sample_count = 0; i < first_sample + samples_to_do; i++, sample_count++)
    {
        uint8_t nibble = ((uint8_t)read_8bit(stream->offset + i/2, stream->streamfile) >>
            (i & 1 ? 4 : 0)) & 0xf;

        stream->adpcm_history1_32 += slope_table[stream->adpcm_step_index][nibble];
        stream->adpcm_step_index   = next_step  [stream->adpcm_step_index][nibble];

        if (stream->adpcm_history1_32 < -32768)
            stream->adpcm_history1_32 = -32768;

        if (stream->adpcm_history1_32 > 32767)
            stream->adpcm_history1_32 = 32767;

        outbuf[sample_count] = (sample_t)stream->adpcm_history1_32;
    }
}
