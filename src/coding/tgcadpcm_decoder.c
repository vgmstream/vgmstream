#include "coding.h"

/* Decodes SunPlus' ADPCM codec used on the Tiger Game.com.
 * Reverse engineered from the Game.com's BIOS. */

static uint16_t slopeTable[64] =
{
    0x0000, 0x0100, 0x0200, 0x0400, 0x0610, 0x0810, 0x0C18, 0x1020,
    0x0100, 0x0300, 0x0508, 0x0908, 0x0D18, 0x1118, 0x1920, 0x2128,
    0x0208, 0x0508, 0x0810, 0x0E10, 0x1420, 0x1A20, 0x2628, 0x3230,
    0x0310, 0x0710, 0x0B18, 0x1318, 0x1B28, 0x2328, 0x2930, 0x4338,
    0x0418, 0x0918, 0x0E20, 0x1820, 0x2230, 0x2C30, 0x4038, 0x5438,
    0x0520, 0x0B20, 0x1128, 0x1D28, 0x2938, 0x3538, 0x4D38, 0x6F38,
    0x0628, 0x0D28, 0x1430, 0x2230, 0x3038, 0x3E38, 0x5A38, 0x7638,
    0x0730, 0x0F30, 0x1738, 0x2738, 0x3738, 0x4738, 0x6738, 0x7D38
};

void decode_tgc(VGMSTREAMCHANNEL * stream, sample_t * outbuf, int32_t first_sample, int32_t samples_to_do)
{
    for (int i = first_sample, sample_count = 0; i < first_sample + samples_to_do; i++, sample_count++)
    {
        uint8_t samp = ((uint8_t)read_8bit(i/2, stream->streamfile) >>
            (i & 1 ? 4 : 0)) & 0xf;

        uint8_t slopeIndex = stream->adpcm_scale | (samp >> 1);

        stream->adpcm_step_index = slopeTable[slopeIndex] >> 8;
        stream->adpcm_scale      = slopeTable[slopeIndex] & 0xff;

        stream->adpcm_history1_16 += (samp & 1) ?
            -stream->adpcm_step_index:
             stream->adpcm_step_index;

        if (stream->adpcm_history1_16 < 0)
            stream->adpcm_history1_16 = 0;

        if (stream->adpcm_history1_16 > 0xff)
            stream->adpcm_history1_16 = 0xff;

        outbuf[sample_count] = stream->adpcm_history1_16 * 0x100 - 0x8000;
    }
}