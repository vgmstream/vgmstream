#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "g7221_decoder_lib.h"
#include "g7221_decoder_aes.h"


/* Decodes Siren14 from Namco's BNSF, a mono MLT/DCT-based codec for speech/sound (low bandwidth).
 * Reverse engineered for various exes with info from Polycom's reference int decoder.
 * - Reference decoder and codec info: https://www.itu.int/rec/T-REC-G.722.1/en
 *
 * Technically the name is "ITU-T G.722.1 Annex C" (official ITU-T spec), while "Siren14"
 * was Polycom's original format with slightly different frames, though Namco calls it
 * "Siren14 Version 3.02 For Products" while using G.722.1's frame format.
 * Siren7 (7000hz bandwidth) isn't supported, only Siren14 (14000hz).
 *
 * Very roughly the encoder works like this:
 * - Apply a Modulated Lapped Transform (MLT) function over 640*2 samples to get spectrum
 *   coefficients (can be decomposed into a window, overlap and add with a DCT-IV, of samples
 *   from a current frame and samples from a prev frame).
 * - resulting coefs are divided into 28 bands called "regions" of 500hz.
 * - Each region contains 20 MLT spectrum coefs, total 28 regions * 500hz = 14000hz.
 * - Bands above 14khz are ignored (overall output quality isn't high).
 * - Pack amplitude envelope bits, defined as the RMS (Root-Mean-Square) of the coefs in
 *   the region. First region sets all bits, rest is differentially and huffman coded. 
 *   Remaning bits are left to quantize coefs.
 * - Regions are given a "category" to quantize, that define the number of quantization bits
 *   and other coding parameters. Results are combined into vector indices,
 *   and Huffman-coded (frequent vectors require less bits).
 * Decoding thus unpacks amplitudes, region coefs and does inverse MLT.
 *
 * Namco roughly follows the reference decoder ('refdec') with some differences:
 * - uses mostly int32, while refdec has int16 with exact rounding/overflow handling (no output diffs)
 * - modified random number generator (minor output diffs)
 * - very minor change in bit unpacking (minor output diffs)
 * - modified DCT-IV optimizations, scaling and window functions (minor output diffs)
 * - internally PCM16 bufs, but converts to float (sample/32768.0) afterwards if the platform needs it
 * - less error control (on error decoder is supposed to repeat last coefs)
 * - can't decode Siren7, and given output diffs it's not actually ITU-compliant
 * - minor optimizations here and there but otherwise very similar
 * This decoder generally uses Polycom's terminology, and while some parts like the bitreader could be
 * reimplemented they are mostly untouched for documentation purposes.
 *
 * TODO: missing some validations (may segfault on bad data), 
 *       access indexes with (idx & max) and clamp buffer reads
 */

#include "g7221_decoder_lib_data.h"

/*****************************************************************************
 * IMLT
 *****************************************************************************/

static int imlt_window(int16_t* new_samples, int16_t* old_samples, int16_t* out_samples) {
    int i;
    int sample_lo, sample_hi;
    int16_t win_val_lo, win_val_hi, new_val, old_val;
    const int16_t *win_ptr_lo, *win_ptr_hi;
    int16_t *new_ptr, *old_ptr, *out_ptr_lo, *out_ptr_hi;


    /* overlap 2nd half of prev frame's samples and 1st half of current frame's samples with
     * a window function to smooth out between frames */
    win_ptr_lo = imlt_samples_window + 0;
    win_ptr_hi = imlt_samples_window + 640;
    new_ptr = new_samples + 320;
    old_ptr = old_samples + 0;
    out_ptr_lo = out_samples + 0;
    out_ptr_hi = out_samples + 640;

    while (out_ptr_lo != out_ptr_hi) {
        win_val_lo = *win_ptr_lo++;
        win_val_hi = *--win_ptr_hi;
        new_val = *--new_ptr;
        old_val = *old_ptr++;

        sample_lo = (new_val * win_val_lo + old_val * *win_ptr_hi + 32768) >> 13;
        if (sample_lo > 32767)
            sample_lo = 32767;
        else if (sample_lo < -32768)
            sample_lo = -32768;
        *out_ptr_lo++ = sample_lo;

        sample_hi = (new_val * win_val_hi - old_val * win_val_lo + 32768) >> 13;
        if (sample_hi > 32767)
            sample_hi = 32767;
        else if (sample_hi < -32768)
            sample_hi = -32768;
        *--out_ptr_hi = sample_hi;
    }

    /* save the 2nd half of the new samples to use above in next frame */
    old_ptr = old_samples + 0;
    new_ptr = new_samples + 320;

    for (i = 0; i < 320; i++) {
        old_ptr[i] = new_ptr[i];
    }

    return 0;
}

/* "dct4_x640_int" */
static int imlt_dct4(int16_t* mlt_coefs, int16_t* new_samples, int mag_shift) {
    int i, j, k, n;
    const uint8_t *set1_ptr;
    int mod_shift, sub_shift;


    /* vs refdec: very optimized, output is slightly different (louder) but it's massively
     * faster (around 20% vs float refdec, int refdec was very slow to begin with).
     * Can't quite clean this due to the complex math simplifications.
     * Should correspond to: cos(PI*(t+0.5)*(k+0.5)/block_length) */

    /* rotation butterflies? (cos/sin 640 groups) */
    {
        int cos_val, sin_val;
        const uint16_t *cos_ptr, *sin_ptr;
        int16_t mlt_val_lo, mlt_val_hi;
        int16_t *mlt_ptr_lo, *mlt_ptr_hi;

        mlt_ptr_lo = mlt_coefs + 0;
        mlt_ptr_hi = mlt_coefs + 640;
        cos_ptr = &imlt_cos_tables[0]; /* cos_table_64 */
        sin_ptr = &imlt_sin_tables[0]; /* sin_table_64 */

        for (i = 40; i > 0; --i) {
            cos_val = *cos_ptr++;
            sin_val = *sin_ptr++;
            mlt_val_lo = *mlt_ptr_lo >> 1;
            *mlt_ptr_lo++ = (cos_val * mlt_val_lo + 32768) >> 16;
            *--mlt_ptr_hi = (sin_val * -mlt_val_lo + 32768) >> 16;

            cos_val = *cos_ptr++;
            sin_val = *sin_ptr++;
            mlt_val_lo = *mlt_ptr_lo >> 1;
            *mlt_ptr_lo++ = (cos_val * mlt_val_lo + 32768) >> 16;
            *--mlt_ptr_hi = (sin_val * mlt_val_lo + 32768) >> 16;
        }

        for (i = 120; i > 0; --i) {
            cos_val = *cos_ptr++;
            sin_val = *sin_ptr++;
            mlt_val_lo = *mlt_ptr_lo >> 1;
            mlt_val_hi = *--mlt_ptr_hi >> 1;
            *mlt_ptr_lo++ = (cos_val * mlt_val_lo + sin_val * mlt_val_hi + 32768) >> 16;
            *mlt_ptr_hi   = (sin_val * -mlt_val_lo + cos_val * mlt_val_hi + 32768) >> 16;

            cos_val = *cos_ptr++;
            sin_val = *sin_ptr++;
            mlt_val_lo = *mlt_ptr_lo >> 1;
            mlt_val_hi = *--mlt_ptr_hi >> 1;
            *mlt_ptr_lo++ = (cos_val * mlt_val_lo + sin_val * mlt_val_hi + 32768) >> 16;
            *mlt_ptr_hi   = (sin_val * mlt_val_lo - cos_val * mlt_val_hi + 32768) >> 16;
        }
    }

    /* sum/diff butterflies? */
    {
        int16_t mlt_val_lo, mlt_val_mlo, mlt_val_mhi, mlt_val_hi;
        int16_t *mlt_ptr, *mlt_ptr_lo, *mlt_ptr_mlo, *mlt_ptr_mhi, *mlt_ptr_hi;

        mlt_ptr = mlt_coefs + 0;
        for (i = 2; i > 0; --i) {
            mlt_ptr_lo = mlt_ptr + 0;
            mlt_ptr_hi = mlt_ptr + 320;
            mlt_ptr_mlo = mlt_ptr + 160;
            mlt_ptr_mhi = mlt_ptr + 160;
            for (j = 80; j > 0; --j) {
                mlt_val_lo = *mlt_ptr_lo;
                mlt_val_hi = *--mlt_ptr_hi;
                mlt_val_mhi = *--mlt_ptr_mhi;
                mlt_val_mlo = *mlt_ptr_mlo;
                *mlt_ptr_lo++  = (mlt_val_hi + mlt_val_lo) >> 1;
                *mlt_ptr_mlo++ = (mlt_val_lo - mlt_val_hi) >> 1;
                *mlt_ptr_mhi   = (mlt_val_mlo + mlt_val_mhi) >> 1;
                *mlt_ptr_hi    = (mlt_val_mhi - mlt_val_mlo) >> 1;
            }
            mlt_ptr += 320;
        }
    }

    /* helper table used in next 3 sections */
    set1_ptr = imlt_set1_table;

    /* rotation butterflies? (cos/sin 160/80/40/20/10 groups) */
    {
        int cos_val, sin_val;
        const uint16_t *cos_ptr, *sin_ptr, *cos_ptr_lo, *sin_ptr_lo;
        int16_t mlt_val_lo, mlt_val_hi, mlt_val_mlo, mlt_val_mhi;
        int16_t *mlt_ptr, *mlt_ptr_lo, *mlt_ptr_hi, *mlt_ptr_mlo, *mlt_ptr_mhi;

        cos_ptr = &imlt_cos_tables[320+160]; /* cos_table_16 > 8 > 4 > 2 */
        sin_ptr = &imlt_sin_tables[320+160]; /* sin_table_16 > 8 > 4 > 2 */

        for (n = 160; n >= 20; n /= 2) {
            mlt_ptr = mlt_coefs + 0;
            while (mlt_ptr < mlt_coefs + 640) {
                for (j = *set1_ptr; j > 0; --j) {
                    mlt_ptr_lo = mlt_ptr + 0;
                    mlt_ptr_hi = mlt_ptr + n;
                    mlt_ptr_mlo = mlt_ptr + (n / 2);
                    mlt_ptr_mhi = mlt_ptr + (n / 2);
                    for (k = n / 4; k > 0; --k) {
                        mlt_val_lo = *mlt_ptr_lo;
                        mlt_val_hi = *--mlt_ptr_hi;
                        mlt_val_mhi = *--mlt_ptr_mhi;
                        mlt_val_mlo = *mlt_ptr_mlo;
                        *mlt_ptr_lo++ = mlt_val_lo + mlt_val_hi;
                        *mlt_ptr_mlo++ = mlt_val_lo - mlt_val_hi;
                        *mlt_ptr_mhi = mlt_val_mlo + mlt_val_mhi;
                        *mlt_ptr_hi = mlt_val_mhi - mlt_val_mlo;
                    }
                    mlt_ptr += n;
                }
                set1_ptr++;

                for (j = *set1_ptr; j > 0; --j) {
                    mlt_ptr_lo = mlt_ptr + 0;
                    mlt_ptr_hi = mlt_ptr + n;
                    cos_ptr_lo = cos_ptr + 0;
                    sin_ptr_lo = sin_ptr + 0;
                    for (k = n / 4; k > 0; --k) {
                        cos_val = *cos_ptr_lo++;
                        sin_val = *sin_ptr_lo++;
                        mlt_val_lo = *mlt_ptr_lo;
                        mlt_val_hi = *--mlt_ptr_hi;
                        *mlt_ptr_lo++ = (cos_val * mlt_val_lo + sin_val * mlt_val_hi + 32768) >> 16;
                        *mlt_ptr_hi   = (sin_val * -mlt_val_lo + cos_val * mlt_val_hi + 32768) >> 16;

                        cos_val = *cos_ptr_lo++;
                        sin_val = *sin_ptr_lo++;
                        mlt_val_lo = *mlt_ptr_lo;
                        mlt_val_hi = *--mlt_ptr_hi;
                        *mlt_ptr_lo++ = (cos_val * mlt_val_lo + sin_val * mlt_val_hi + 32768) >> 16;
                        *mlt_ptr_hi   = (sin_val * mlt_val_lo - cos_val * mlt_val_hi + 32768) >> 16;
                    }
                    mlt_ptr += n;
                }
                set1_ptr++;
            }

            /* next sub-tables */
            cos_ptr += n / 2;
            sin_ptr += n / 2;
        }
    }

    /* rotation butterflies? (cos/sin 5 groups) */
    {
        int cos_val, sin_val;
        const uint16_t *cos_ptr, *sin_ptr;
        int16_t mlt_val_lo, mlt_val_hi, mlt_val_mlo, mlt_val_mhi;
        int16_t *mlt_ptr;

        /* n/cos-sin would continue from above but for clarity: */
        cos_ptr = &imlt_cos_tables[320+160+80+40+20+10]; /* cos_table_1 */
        sin_ptr = &imlt_sin_tables[320+160+80+40+20+10]; /* sin_table_1 */

        {
            n = 10;
            mlt_ptr = mlt_coefs + 0;
            while (mlt_ptr < mlt_coefs + 640) {
                for (j = *set1_ptr; j > 0; --j) {
                    mlt_val_lo = mlt_ptr[0];
                    mlt_val_hi = mlt_ptr[n - 1];
                    mlt_val_mlo = mlt_ptr[n / 2 - 1];
                    mlt_val_mhi = mlt_ptr[n / 2];
                    mlt_ptr[0] = mlt_val_lo + mlt_val_hi;
                    mlt_ptr[n / 2] = mlt_val_lo - mlt_val_hi;
                    mlt_ptr[n / 2 - 1] = mlt_val_mhi + mlt_val_mlo;
                    mlt_ptr[n - 1] = mlt_val_mlo - mlt_val_mhi;

                    mlt_val_lo = mlt_ptr[1];
                    mlt_val_hi = mlt_ptr[n - 2];
                    mlt_val_mlo = mlt_ptr[n / 2 - 2];
                    mlt_val_mhi = mlt_ptr[n / 2 + 1];
                    mlt_ptr[1] = mlt_val_hi + mlt_val_lo;
                    mlt_ptr[n / 2 + 1] = mlt_val_lo - mlt_val_hi;
                    mlt_ptr[n / 2 - 2] = mlt_val_mhi + mlt_val_mlo;
                    mlt_ptr[n - 2] = mlt_val_mlo - mlt_val_mhi;

                    mlt_val_lo = mlt_ptr[2];
                    mlt_val_hi = mlt_ptr[n - 3];
                    mlt_ptr[2] = mlt_val_hi + mlt_val_lo;
                    mlt_ptr[n / 2 + 2] = mlt_val_lo - mlt_val_hi;

                    mlt_ptr += n;
                }

                cos_val = cos_ptr[0];
                sin_val = sin_ptr[0];
                mlt_val_lo = mlt_ptr[0];
                mlt_val_hi = mlt_ptr[n - 1];
                mlt_ptr[0] = (cos_val * mlt_val_lo + sin_val * mlt_val_hi + 32768) >> 16;
                mlt_ptr[n - 1] = (sin_val * mlt_val_lo - cos_val * mlt_val_hi + 32768) >> 16;

                cos_val = cos_ptr[1];
                sin_val = sin_ptr[1];
                mlt_val_lo = mlt_ptr[1];
                mlt_val_hi = mlt_ptr[n - 2];
                mlt_ptr[1] = (cos_val * mlt_val_lo + sin_val * mlt_val_hi + 32768) >> 16;
                mlt_ptr[n - 2] = (sin_val * -mlt_val_lo + cos_val * mlt_val_hi + 32768) >> 16;

                cos_val = cos_ptr[2];
                sin_val = sin_ptr[2];
                mlt_val_lo = mlt_ptr[2];
                mlt_val_hi = mlt_ptr[n - 3];
                mlt_ptr[2] = (cos_val * mlt_val_lo + sin_val * mlt_val_hi + 32768) >> 16;
                mlt_ptr[n - 3]= (sin_val * mlt_val_lo - cos_val * mlt_val_hi + 32768) >> 16;

                cos_val = cos_ptr[3];
                sin_val = sin_ptr[3];
                mlt_val_lo = mlt_ptr[3];
                mlt_val_hi = mlt_ptr[n - 4];
                mlt_ptr[3] = (cos_val * mlt_val_lo + sin_val * mlt_val_hi + 32768) >> 16;
                mlt_ptr[n - 4] = (sin_val * -mlt_val_lo + cos_val * mlt_val_hi + 32768) >> 16;

                cos_val = cos_ptr[4];
                sin_val = sin_ptr[4];
                mlt_val_lo = mlt_ptr[4];
                mlt_val_hi = mlt_ptr[n - 5];
                mlt_ptr[4] = (cos_val * mlt_val_lo + sin_val * mlt_val_hi + 32768) >> 16;
                mlt_ptr[n - 5] = (sin_val * mlt_val_lo - cos_val * mlt_val_hi + 32768) >> 16;

                mlt_ptr += n;
                set1_ptr += 2;
            }
        }
    }


    mod_shift = mag_shift - 1;
    sub_shift = 1;
    if (mod_shift >= 8)
        sub_shift = 2;
    mod_shift -= sub_shift;


    /* dct core? */
    {
        const int16_t *mlt_ptr;
        int16_t *new_ptr;

        mlt_ptr = mlt_coefs + 0;
        new_ptr = new_samples + 0;
        while (1) {
            for (i = *set1_ptr; i; --i) {
                new_ptr[0] = (mlt_ptr[4] + mlt_ptr[3] + mlt_ptr[2] + mlt_ptr[1] + mlt_ptr[0]) >> sub_shift;
                new_ptr[1] = (19261 * mlt_ptr[1] + 31164 * mlt_ptr[0] - 19261 * mlt_ptr[3] - 31164 * mlt_ptr[4]) >> (sub_shift + 15);
                new_ptr[2] = (26510 * mlt_ptr[4] + 26510 * mlt_ptr[0] - 10126 * mlt_ptr[1] - 32768 * mlt_ptr[2] - 10126 * mlt_ptr[3]) >> (sub_shift + 15);
                new_ptr[3] = (31164 * mlt_ptr[3] + 19261 * mlt_ptr[0] - 31164 * mlt_ptr[1] - 19261 * mlt_ptr[4]) >> (sub_shift + 15);
                new_ptr[4] = (10126 * mlt_ptr[4] + 32768 * mlt_ptr[2] + 10126 * mlt_ptr[0] - 26510 * mlt_ptr[1] - 26510 * mlt_ptr[3]) >> (sub_shift + 15);
                mlt_ptr += 5;
                new_ptr += 5;
            }
            set1_ptr += 2;

            if (mlt_ptr >= mlt_coefs + 640)
                break;

            new_ptr[0] = (  5126 * mlt_ptr[4] +  14876 * mlt_ptr[3] +  23170 * mlt_ptr[2] + 32365 * mlt_ptr[0] + 29197 * mlt_ptr[1]) >> (sub_shift + 15);
            new_ptr[1] = (-14876 * mlt_ptr[4] + -32365 * mlt_ptr[3] +   5126 * mlt_ptr[1] + 29197 * mlt_ptr[0] - 23170 * mlt_ptr[2]) >> (sub_shift + 15);
            new_ptr[2] = ( 23170 * mlt_ptr[4] +  23170 * mlt_ptr[3] + -23170 * mlt_ptr[1] + 23170 * mlt_ptr[0] - 23170 * mlt_ptr[2]) >> (sub_shift + 15);
            new_ptr[3] = (-29197 * mlt_ptr[4] +   5126 * mlt_ptr[3] +  23170 * mlt_ptr[2] + 14876 * mlt_ptr[0] - 32365 * mlt_ptr[1]) >> (sub_shift + 15);
            new_ptr[4] = ( 32365 * mlt_ptr[4] + -29197 * mlt_ptr[3] +  23170 * mlt_ptr[2] +  5126 * mlt_ptr[0] - 14876 * mlt_ptr[1]) >> (sub_shift + 15);
            mlt_ptr += 5;
            new_ptr += 5;
        }
    }

    /* swapping and sum/diffs? */
    {
        const uint8_t *set2_ptr;
        int16_t *mlt_ptr, *new_ptr;
        int16_t tmp1_val_a, tmp1_val_b;
        int16_t *tmp0_ptr, *tmp1_ptr, *tmp1_ptr_lo, *tmp1_ptr_mlo, *tmp1_ptr_mhi, *tmp2_ptr;

        set2_ptr = imlt_set2_table;

        mlt_ptr = mlt_coefs + 0;
        new_ptr = new_samples + 0;
        while (new_ptr < new_samples + 640) {
            for (i = *set2_ptr; i; --i) {
                *mlt_ptr++ = new_ptr[0];
                *mlt_ptr++ = new_ptr[5];
                *mlt_ptr++ = new_ptr[1];
                *mlt_ptr++ = new_ptr[6];
                *mlt_ptr++ = new_ptr[2];
                *mlt_ptr++ = new_ptr[7];
                *mlt_ptr++ = new_ptr[3];
                *mlt_ptr++ = new_ptr[8];
                *mlt_ptr++ = new_ptr[4];
                *mlt_ptr++ = new_ptr[9];
                new_ptr += 10;
            }
            set2_ptr++;

            *mlt_ptr++ = new_ptr[0];
            *mlt_ptr++ = new_ptr[9] + new_ptr[1];
            *mlt_ptr++ = new_ptr[1] - new_ptr[9];
            *mlt_ptr++ = new_ptr[2] - new_ptr[8];
            *mlt_ptr++ = new_ptr[8] + new_ptr[2];
            *mlt_ptr++ = new_ptr[7] + new_ptr[3];
            *mlt_ptr++ = new_ptr[3] - new_ptr[7];
            *mlt_ptr++ = new_ptr[4] - new_ptr[6];
            *mlt_ptr++ = new_ptr[6] + new_ptr[4];
            *mlt_ptr++ = new_ptr[5];
            new_ptr += 10;
        }

        /* below is some three way swapping, tmp ptrs change between mlt<>new */
        tmp0_ptr = mlt_coefs + 640;
        tmp1_ptr = new_samples + 640;
        for (n = 20; n <= 160; n *= 2) {
            tmp2_ptr = tmp0_ptr + 0;
            tmp0_ptr = tmp1_ptr - 640;
            tmp1_ptr = tmp2_ptr - 640;
            do {
                for (j = *set2_ptr; j > 0; --j) {
                    tmp1_ptr_mhi = tmp1_ptr + (n / 2);
                    for (k = n / 4; k > 0; --k) {
                        *tmp0_ptr++ = *tmp1_ptr++;
                        *tmp0_ptr++ = *tmp1_ptr_mhi++;
                        *tmp0_ptr++ = *tmp1_ptr++;
                        *tmp0_ptr++ = *tmp1_ptr_mhi++;
                    }
                    tmp1_ptr += n / 2;
                }
                set2_ptr++;

                if (tmp1_ptr >= tmp2_ptr)
                    break;

                tmp1_ptr_lo = tmp1_ptr + 0;
                tmp1_ptr_mlo = tmp1_ptr + (n - 1);

                *tmp0_ptr++ = *tmp1_ptr_lo++;

                tmp1_val_a = *tmp1_ptr_lo++;
                tmp1_val_b = *tmp1_ptr_mlo;
                *tmp0_ptr++ = tmp1_val_b + tmp1_val_a;
                *tmp0_ptr++ = tmp1_val_a - tmp1_val_b;
                for (j = (n / 2 - 2) / 2; j > 0; --j) {
                    tmp1_val_a = *tmp1_ptr_lo++;
                    tmp1_val_b = *--tmp1_ptr_mlo;
                    *tmp0_ptr++ = tmp1_val_a - tmp1_val_b;
                    *tmp0_ptr++ = tmp1_val_b + tmp1_val_a;

                    tmp1_val_a = *tmp1_ptr_lo++;
                    tmp1_val_b = *--tmp1_ptr_mlo;
                    *tmp0_ptr++ = tmp1_val_b + tmp1_val_a;
                    *tmp0_ptr++ = tmp1_val_a - tmp1_val_b;
                }
                *tmp0_ptr++ = -*tmp1_ptr_lo;
                tmp1_ptr += n;
            }
            while (tmp1_ptr < tmp2_ptr);
        }
    }

    /* final modifications and post scaling? */
    {
        int16_t mlt_val_lo, mlt_val_mhi, mlt_val_mlo, mlt_val_hi;
        const int16_t *mlt_ptr_lo, *mlt_ptr_hi, *mlt_ptr_mlo, *mlt_ptr_mhi;
        int16_t *new_ptr;

        if (mod_shift <= 0) {
            /* negative scale (right shift) */ 
            mod_shift = -mod_shift;

            mlt_ptr_lo = mlt_coefs + 0;
            mlt_ptr_mlo = mlt_coefs + 160;
            mlt_ptr_mhi = mlt_coefs + 480;
            mlt_ptr_hi = mlt_coefs + 640;
            new_ptr = new_samples + 0;

            mlt_val_lo = *mlt_ptr_lo++;
            *new_ptr++ = mlt_val_lo << mod_shift;

            mlt_val_mlo = *mlt_ptr_mlo++;
            mlt_val_hi = *--mlt_ptr_hi;
            *new_ptr++ = (mlt_val_hi + mlt_val_mlo) << mod_shift;
            *new_ptr++ = (mlt_val_mlo - mlt_val_hi) << mod_shift;

            for (i = 159; i > 0; --i) {
                mlt_val_lo = *mlt_ptr_lo++;
                mlt_val_mhi = *--mlt_ptr_mhi;
                *new_ptr++ = (mlt_val_lo - mlt_val_mhi) << mod_shift;
                *new_ptr++ = (mlt_val_mhi + mlt_val_lo) << mod_shift;

                mlt_val_mlo = *mlt_ptr_mlo++;
                mlt_val_hi = *--mlt_ptr_hi;
                *new_ptr++ = (mlt_val_hi + mlt_val_mlo) << mod_shift;
                *new_ptr++ = (mlt_val_mlo - mlt_val_hi) << mod_shift;
            }

            *new_ptr = -*mlt_ptr_mlo << mod_shift;
        }
        else {
            /* same but positive (left shift) */

            mlt_ptr_lo = mlt_coefs + 0;
            mlt_ptr_mlo = mlt_coefs + 160;
            mlt_ptr_mhi = mlt_coefs + 480;
            mlt_ptr_hi = mlt_coefs + 640;
            new_ptr = new_samples + 0;

            mlt_val_lo = *mlt_ptr_lo++;
            *new_ptr++ = mlt_val_lo >> mod_shift;

            mlt_val_mlo = *mlt_ptr_mlo++;
            mlt_val_hi = *--mlt_ptr_hi;
            *new_ptr++ = (mlt_val_hi + mlt_val_mlo) >> mod_shift;
            *new_ptr++ = (mlt_val_mlo - mlt_val_hi) >> mod_shift;

            for (i = 159; i > 0; --i) {
                mlt_val_lo = *mlt_ptr_lo++;
                mlt_val_mhi = *--mlt_ptr_mhi;
                *new_ptr++ = (mlt_val_lo - mlt_val_mhi) >> mod_shift;
                *new_ptr++ = (mlt_val_mhi + mlt_val_lo) >> mod_shift;

                mlt_val_mlo = *mlt_ptr_mlo++;
                mlt_val_hi = *--mlt_ptr_hi;
                *new_ptr++ = (mlt_val_hi + mlt_val_mlo) >> mod_shift;
                *new_ptr++ = (mlt_val_mlo - mlt_val_hi) >> mod_shift;
            }

            *new_ptr = -*mlt_ptr_mlo >> mod_shift;
        }
    }

    return 0;
}

/* "inverse_MLT" */
static int rmlt_coefs_to_samples(int mag_shift, int16_t* mlt_coefs, int16_t* old_samples, int16_t* out_samples /*, int p_samples_done*/) {
    int res;
    int16_t new_samples[640];

    /* block transform MLT spectrum coefs to time domain PCM samples using DCT-IV (inverse) */
    res = imlt_dct4(mlt_coefs, new_samples, mag_shift);
    if (res < 0) return res;

    /* apply IMLT overlapped window filter function (640 samples) */
    res = imlt_window(new_samples, old_samples, out_samples);
    if (res < 0) return res;

    //*p_samples_done = 640; /* in Namco's code but actually ignored */

    return 0;
}

/*****************************************************************************
 * UNPACKING
 *****************************************************************************/

static inline int calc_offset(const int* absolute_region_power_index, int available_bits) {
    int region, cat_index;
    int offset, delta;

    offset = -32;
    delta = 32;
    do {
        int test_offset = offset + delta;
        int bits = 0;

        /* obtain a category for each region using the test offset */
        for (region = 0; region < NUMBER_OF_REGIONS; region++)  {
            cat_index = (test_offset - absolute_region_power_index[region]) / 2;
            if (cat_index < 0)
                cat_index = 0;
            else if (cat_index > NUM_CATEGORIES - 1)
                cat_index = NUM_CATEGORIES - 1;

            /* compute the number of bits that will be used given the cat assignments */
            bits += expected_bits_table[cat_index];
        }

        /* if (bits > available_bits - 32) then divide the offset region for the bin search */
        if (bits >= available_bits - 32) {
            offset = test_offset;
        }
        delta /= 2;
    }
    while (delta > 0);

    return offset;
}

static inline void compute_raw_power_categories(int* power_categories, const int* absolute_region_power_index, int offset) {
    int region, cat_index;

    for (region = 0; region < NUMBER_OF_REGIONS; region++) {
        cat_index = (offset - absolute_region_power_index[region]) / 2;
        if (cat_index < 0) 
            cat_index = 0;
        else if (cat_index > NUM_CATEGORIES - 1)
            cat_index = NUM_CATEGORIES - 1;

        power_categories[region] = cat_index;
    }
}

static inline void comp_powercat_and_catbalance(int* power_categories, int* category_balances, const int* absolute_region_power_index, int available_bits, int offset) {
    int region, ccp;
    int max_rate_categories[NUMBER_OF_REGIONS];
    int min_rate_categories[NUMBER_OF_REGIONS];
    int temp_category_balances[2*NUM_CATEGORIZATION_CONTROL_POSSIBILITIES];
    int expected_number_of_code_bits, max, min, max_rate_pointer, min_rate_pointer;


    /* Namco uses power_categories directly instead of max_rate_categories, but we'll separate for clarity.
     * It also loads min_rate_categories and expected_number_of_code_bits in the previous region loop */
    expected_number_of_code_bits = 0;
    for (region = 0; region < NUMBER_OF_REGIONS; region++) {
        int power_category = power_categories[region];
        max_rate_categories[region] = power_category;
        min_rate_categories[region] = power_category;
        expected_number_of_code_bits += expected_bits_table[power_category];
    }

    max = expected_number_of_code_bits;
    min = expected_number_of_code_bits;
    max_rate_pointer = NUM_CATEGORIZATION_CONTROL_POSSIBILITIES;
    min_rate_pointer = NUM_CATEGORIZATION_CONTROL_POSSIBILITIES;

    for (ccp = 0; ccp < NUM_CATEGORIZATION_CONTROL_POSSIBILITIES - 1; ccp++) {

        if (max + min <= available_bits * 2) {
            int raw_min = 10000;
            int raw_min_index = 0;

            /* Search from lowest freq regions to highest for best */
            /* region to reassign to a higher bit rate category.   */
            for (region = 0; region < NUMBER_OF_REGIONS; region++)  {
                if (max_rate_categories[region] > 0) {
                    int tmp = (offset - absolute_region_power_index[region]) - (max_rate_categories[region] * 2);
                    if (tmp < raw_min) {
                        raw_min = tmp;
                        raw_min_index = region;
                    }
                }
            }

            max_rate_pointer--;
            temp_category_balances[max_rate_pointer] = raw_min_index;

            max -= expected_bits_table[max_rate_categories[raw_min_index]];
            max_rate_categories[raw_min_index]--;
            max += expected_bits_table[max_rate_categories[raw_min_index]];
        }
        else {
            int raw_max = -10000;
            int raw_max_index = NUMBER_OF_REGIONS - 1;

            /* Search from highest freq regions to lowest for best region to reassign to a lower bit rate category. */
            for (region = NUMBER_OF_REGIONS - 1; region >= 0; region--)  {
                if (min_rate_categories[region] < NUM_CATEGORIES - 1) {
                    int tmp = (offset - absolute_region_power_index[region]) - (min_rate_categories[region] * 2);
                    if (tmp > raw_max) {
                        raw_max = tmp;
                        raw_max_index = region;
                    }
                }
            }

            temp_category_balances[min_rate_pointer] = raw_max_index;
            min_rate_pointer++;

            min -= expected_bits_table[min_rate_categories[raw_max_index]];
            min_rate_categories[raw_max_index]++;
            min += expected_bits_table[min_rate_categories[raw_max_index]];
        }
    }

    for (region = 0; region < NUMBER_OF_REGIONS; region++) {
        power_categories[region] = max_rate_categories[region];
    }

    for (ccp = 0; ccp < NUM_CATEGORIZATION_CONTROL_POSSIBILITIES - 1; ccp++) {
        category_balances[ccp] = temp_category_balances[max_rate_pointer + ccp];
    }
}

static int categorize(int available_bits, const int* absolute_region_power_index, int* power_categories, int* category_balances) {
    int offset;

    /* compensate increased bit usage for higher bitrates (used?) */
    if (available_bits > MAX_DCT_LENGTH) {
        available_bits = 5 * (available_bits - MAX_DCT_LENGTH) / 8 + MAX_DCT_LENGTH;
    }

    /* calculate category stuff (originally inline'd) */

    offset = calc_offset(absolute_region_power_index, available_bits);

    compute_raw_power_categories(power_categories, absolute_region_power_index, offset);

    comp_powercat_and_catbalance(power_categories, category_balances, absolute_region_power_index, available_bits, offset);

    return 0;
}

static inline void index_to_array(int index, int* array_cv, int category) {
    int q, p;
    int max_bin_plus_one = max_bin_plus1[category];
    int inverse_of_max_bin_plus_one_scaled = max_bin_plus_one_inverse_scaled[category];

    /* vs refdec: unrolled, inline'd version of the inverted loop, with some ops simplified
     * (depending on pre-scaled tables), since this is called many times.
     * From tests it's not too noticeable though. */

    p = index;
    /* fills array (vector_dimension[category] - 1) times inversely */
    switch (category) {
        case 0:
        case 1:
        case 2:
            q = (p * inverse_of_max_bin_plus_one_scaled) >> 16;
            array_cv[1] = p - (q * max_bin_plus_one);
            p = q;

            q = (q * inverse_of_max_bin_plus_one_scaled) >> 16;
            array_cv[0] = p - (q * max_bin_plus_one);
            //p = q;
            break;

        case 3:
            q = (p * inverse_of_max_bin_plus_one_scaled) >> 16;
            array_cv[3] = p - (q * 5); //max_bin_plus_one = 5
            p = q;

            q = (q * inverse_of_max_bin_plus_one_scaled) >> 16;
            array_cv[2] = p - (q * 5); //max_bin_plus_one = 5
            p = q;

            q = (q * inverse_of_max_bin_plus_one_scaled) >> 16;
            array_cv[1] = p - (q * 5); //max_bin_plus_one = 5
            p = q;

            q = (q * inverse_of_max_bin_plus_one_scaled) >> 16;
            array_cv[0] = p - (q * 5); //max_bin_plus_one = 5
            //p = q;
            break;

        case 4:
            array_cv[3] = p & 3;
            p >>= 2;

            array_cv[2] = p & 3;
            p >>= 2;

            array_cv[1] = p & 3;
            p >>= 2;

            array_cv[0] = p & 3;
            /* not sure how this case is optimized */
            break;

        case 5:
            q = (p * inverse_of_max_bin_plus_one_scaled) >> 16;
            array_cv[4] = p - (q * 3); //max_bin_plus_one = 3
            p = q;

            q = (q * inverse_of_max_bin_plus_one_scaled) >> 16;
            array_cv[3] = p - (q * 3); //max_bin_plus_one = 3
            p = q;

            q = (q * inverse_of_max_bin_plus_one_scaled) >> 16;
            array_cv[2] = p - (q * 3); //max_bin_plus_one = 3
            p = q;

            q = (q * inverse_of_max_bin_plus_one_scaled) >> 16;
            array_cv[1] = p - (q * 3); //max_bin_plus_one = 3
            p = q;

            q = (q * inverse_of_max_bin_plus_one_scaled) >> 16;
            array_cv[0] = p - (q * 3); //max_bin_plus_one = 3
            //p = q;
            break;

        case 6:
            array_cv[4] = p & 1;
            p >>= 1;

            array_cv[3] = p & 1;
            p >>= 1;

            array_cv[2] = p & 1;
            p >>= 1;

            array_cv[1] = p & 1;
            p >>= 1;

            array_cv[0] = p & 1;
            //p >>= 1;
            /* not sure how this case is optimized */
            break;

        default:
            break;
    }
}

static int decode_vector_quantized_mlt_indices(uint32_t* data_u32, int* p_bitpos, int bit_count, uint32_t* p_random_value, int* decoder_region_standard_deviation, int* power_categories, int16_t* mlt_coefs) {
    int16_t standard_deviation;
    int array_cv[MAX_VECTOR_DIMENSION];
    int i, v, region, category, index;
    uint32_t cur_u32, bitmask;
    uint32_t* ptr_u32;

    /* bitreading setup */
    ptr_u32 = &data_u32[(*p_bitpos >> 5)];
    bitmask = 1 << (31 - (*p_bitpos & 0x1F));
    cur_u32 = *ptr_u32;
    ptr_u32++;


    /* read MLT coefs per region, differently depending on the category config */
    for (region = 0; region < NUMBER_OF_REGIONS; region++)  {
        standard_deviation = decoder_region_standard_deviation[region];
        category = power_categories[region];

        /* lower categories encode MLT coefs based on vectors incides + huffman (?) */
        if (category < 7) {
            const int16_t* decoder_tree_ptr = table_of_decoder_tables[category];
            int16_t* decoder_mlt_ptr = &mlt_coefs[region * REGION_SIZE];

            for (v = 0; v < number_of_vectors[category]; v++)  {
                index = 0;

                do {
                    int bit = (bitmask & cur_u32) != 0;
                    bitmask >>= 1;
                    (*p_bitpos)++;
                    if (bitmask == 0) {
                        bitmask = 0x80000000;
                        cur_u32 = *ptr_u32;
                        ptr_u32++;
                    }

                    index = *(decoder_tree_ptr + (index*2) + bit);
                }
                while (index > 0);

                /* ran out of bits */
                if (ptr_u32 > &data_u32[bit_count >> 5]) {
                    category = 7; /* this category doesn't bitread and only noise fills */

                    /* Namco doesn't set remaining regions to category 7 like the spec, nor checks
                     * when reading sign bits below, but doesn't seem to cause any problems */
                    //for (i = region + 1; i < NUMBER_OF_REGIONS; i++) {
                    //    power_categories[i] = 7;
                    //}
                    break;
                }

                index = -index;

                /* convert index into array of sign bits used to access the centroid table */
                index_to_array(index, array_cv, category);

                /* vs refdec: sign reading slightly simplified */

                for (i = 0; i < vector_dimension[category]; i++) {
                    int decoder_mlt_value = 0;
                    int negative;

                    /* non-zero array  = results in non-zero coef and encodes bit sign */
                    if (array_cv[i] != 0) {
                        decoder_mlt_value = standard_deviation * mlt_quant_centroid[category][array_cv[i]];
                        decoder_mlt_value = decoder_mlt_value >> 12;

                        negative = (bitmask & cur_u32) != 0;
                        bitmask >>= 1;
                        (*p_bitpos)++;
                        if (bitmask == 0) {
                            bitmask = 0x80000000;
                            cur_u32 = *ptr_u32;
                            ptr_u32++;
                        }

                        if (negative == 0)
                            decoder_mlt_value = -decoder_mlt_value;
                    }

                    *decoder_mlt_ptr = decoder_mlt_value;
                    decoder_mlt_ptr++;
                }
            }
        }

        /* higher categories don't encode all 20 MLT coefs, so rest are filled with
         * noise to pretend we have something */
        if (category >= 5) {
            static const int noise_fill_factor[3] = {5793, 8192, 23170}; /* 0.176777, 0.25, 0.707107 */
            uint32_t random_value;

            int16_t* decoder_mlt_ptr = &mlt_coefs[region * REGION_SIZE];
            int16_t noise_fill_pos = (standard_deviation * noise_fill_factor[category - 5]) >> 15; /* should be int16 */
            int16_t noise_fill_neg = -noise_fill_pos;

            /* vs refdec: updated differently (with hist state), and reupdated after 10 coefs */
            *p_random_value *= 69069;
            random_value = *p_random_value;

            /* in some versions of Namco's decoder this is unrolled too */

            if (category >= 7) {
                /* all coefs are noise-filled */
                for (i = 0; i < REGION_SIZE; i++) {
                    {
                        if (random_value & 1) 
                            *decoder_mlt_ptr = noise_fill_pos;
                        else
                            *decoder_mlt_ptr = noise_fill_neg;
                        random_value = (random_value >> 1);
                    }
                    decoder_mlt_ptr++;
                }
            }
            else {
                /* some coefs are noise-filled */
                for (i = 0; i < REGION_SIZE; i++)  {
                    if (*decoder_mlt_ptr == 0) {
                        if (random_value & 1) 
                            *decoder_mlt_ptr = noise_fill_pos;
                        else
                            *decoder_mlt_ptr = noise_fill_neg;
                        random_value = (random_value >> 1);
                    }
                    decoder_mlt_ptr++;
                }
            }
        }
    }

    return 0;
}

/* unpacks input buffer into MLT coefs */
static int unpack_frame(int bit_rate, const uint8_t* data, int frame_size, /*int* p_frame_size, */ int* p_mag_shift, int16_t* mlt_coefs, uint32_t* p_random_value, int test_errors) {
    uint32_t data_u32[0x78/4 + 2];
    int bitpos, expected_frame_size;
    int power_categories[NUMBER_OF_REGIONS];
    int category_balances[NUM_CATEGORIZATION_CONTROL_POSSIBILITIES-1];
    int absolute_region_power_index[NUMBER_OF_REGIONS]; /* a.k.a. RMS_index */
    int decoder_region_standard_deviation[NUMBER_OF_REGIONS];
    uint16_t categorization_control;
    int i;
    int res;


    /* setup bitreading */
    {
        expected_frame_size = bit_rate / 8 / 50;
        if (frame_size < expected_frame_size)
            return 1;
        //p_frame_size = expected_frame_size; /* Namco returns this, for some reason */

        /* Siren14 data is packed into U16 LE, but Namco reads and stores them in a U32 LE temp array for their bitreading */
        for (i = 0; i < (expected_frame_size >> 2); i++) {
            data_u32[i] = (data[0x04*i + 2] << 0) | (data[0x04*i + 3] << 8) | (data[0x04*i + 0] << 16) | (data[0x04*i + 1] << 24);
        }
        /* data32 also has extra ints probably against outside reads, which wasn't originally
         * memset'ed but we'll do just in case (doesn't seem to matter) */
        for (i = (expected_frame_size >> 2); i < 0x78/4 + 2; i++) {
            data_u32[i] = 0;
        }

        bitpos = 0;
    }

    /* decode amplitude envelope scales */
    {
        int rms_index = 0; /* amplitudes are root-mean-square */
        int region;

        /* get amplitude envelope (5b) for region 0 */
        for (i = 0; i < 5; i++)  {
            int bit = (data_u32[bitpos >> 5] >> (31 - (bitpos & 0x1F))) & 1;
            bitpos++;

            rms_index = (rms_index << 1) | bit;
        }
        absolute_region_power_index[0] = rms_index - ESF_ADJUSTMENT_TO_RMS_INDEX;

        /* get amplitudes for other regions, coded differentially based on region 0 (done with a temp array in refdec) */
        for (region = 1; region < NUMBER_OF_REGIONS; region++) {
            int diff_index = 0;
            int region_index = region > 13 ? 13 - 1 : region - 1;

            do {
                int bit = (data_u32[bitpos >> 5] >> (31 - (bitpos & 0x1F))) & 1;
                bitpos++;

                diff_index = differential_region_power_decoder_tree[region_index][diff_index][bit];
            }
            while (diff_index > 0);

            absolute_region_power_index[region] = absolute_region_power_index[region-1] - diff_index - DRP_DIFF_MIN;
        }
    }

    /* read categorization info bits */
    {
        categorization_control = 0;
        for (i = 0; i < NUM_CATEGORIZATION_CONTROL_BITS; i++) {
            int bit = (data_u32[bitpos >> 5] >> (31 - (bitpos & 0x1F))) & 1;
            bitpos++;

            categorization_control = (categorization_control << 1) | bit;
        }
    }

    /* determine categorization config per region */
    res = categorize(
       8 * expected_frame_size - bitpos,
       absolute_region_power_index, power_categories, category_balances);
    if (res < 0) return res;

    /* adjust power categories (rate_adjust_categories) */
    {
        for (i = 0; i < categorization_control; i++) {
            int region = category_balances[i];
            power_categories[region]++;
        }
    }

    /* recover amplitude envelope deviation (done in decode_envelope in refdec) */
    {
        int region, region_index, max_index /*, test_index*/;

        /* vs refdec: Namco *doesn't* calc test_index here, so resulting region_index
          * can be +-1 vs refdec, and final samples around +-10 (usually quieter).
         * Also reuses and mods absolute_region_power_index but we have decoder_region_standard_deviation for clarity */

        //test_index = 0;
        max_index = 0;
        for (region = 0; region < NUMBER_OF_REGIONS; region++) {
            region_index = absolute_region_power_index[region];
            if (max_index < region_index)
                max_index = region_index;
            //test_index += region_standard_deviation_table[region_index + REGION_POWER_TABLE_NUM_NEGATIVES];
        }

        max_index += REGION_POWER_TABLE_NUM_NEGATIVES;
        region_index = 9;
        while ((region_index >= 0) && (/*test_index >= 8 ||*/ max_index > 28)) {
            max_index -= 2;
            region_index--;
            //test_index /= 2;
        }

        for (region = 0; region < NUMBER_OF_REGIONS; region++) {
            int rsd_index = absolute_region_power_index[region] + REGION_POWER_TABLE_NUM_NEGATIVES + region_index * 2;
            decoder_region_standard_deviation[region] = region_standard_deviation_table[rsd_index];
        }

        *p_mag_shift = region_index;
    }

    /* decode the quantized bits into MLT coefs */
    res = decode_vector_quantized_mlt_indices(
        data_u32, &bitpos, 8 * expected_frame_size,
        p_random_value,
        decoder_region_standard_deviation, power_categories, mlt_coefs);
    if (res < 0) return res;


    /* test for errors (in refdec but not Namco's, useful to detect decryption) */
    if (test_errors) {
        int max_pad_bytes = 0x8; /* usually 0x04 and rarely ~0x08 */
        int bits_left = 8 * expected_frame_size - bitpos;
        int i, endpos, test_bits;

        if (bits_left > 0) {

            /* frame must be padded with 1s after regular data */
            endpos = bitpos;
            for (i = 0; i < bits_left; i++) {
                int bit = (data_u32[endpos >> 5] >> (31 - (endpos & 0x1F))) & 1;
                endpos++;

                if (bit == 0)
                    return -1;
            }

            /* extra: test we aren't in the middle of padding (happens with bad keys, this test catches most)
             * After reading the whole frame, last bit position should land near last useful
             * data, a few bytes into padding, so check there aren't too many padding bits. */
            endpos = bitpos;
            test_bits = 8 * max_pad_bytes;
            if (test_bits > bitpos)
                test_bits = bitpos;
            for (i = 0; i < test_bits; i++) {
                int bit = (data_u32[endpos >> 5] >> (31 - (endpos & 0x1F))) & 1;
                endpos--; /* from last position towards valid data */

                if (bit != 1)
                    break;
            }

            if (i == test_bits)
                return -8;

        }
        else {
            /* ? */
            if (categorization_control < NUM_CATEGORIZATION_CONTROL_BITS - 1 && bits_left < 0)
                return -2;
        }

        for (i = 0; i < NUMBER_OF_REGIONS; i++) {
            if ((absolute_region_power_index[i] + ESF_ADJUSTMENT_TO_RMS_INDEX > 31) ||
                (absolute_region_power_index[i] + ESF_ADJUSTMENT_TO_RMS_INDEX < -8))
              return -4;
        }
    }

    return 0;
}


/*****************************************************************************
 * API
 *****************************************************************************/

struct g7221_handle {
    /* control */
    int bit_rate;
    int frame_size;
    int test_errors;
    /* AES setup/state */
    s14aes_handle* aes;
    /* state */
    int16_t mlt_coefs[MAX_DCT_LENGTH];
    int16_t old_samples[MAX_DCT_LENGTH >> 1];
    uint32_t random_value;
};

g7221_handle* g7221_init(int bytes_per_frame) {
    g7221_handle* handle = NULL;
    int bit_rate;

    /* valid only: 0x78, 0x50 or 0x3c */
    bit_rate = bytes_per_frame * 8 * 50;
    if (bit_rate != 24000 && bit_rate != 32000 && bit_rate != 48000)
        goto fail;

    handle = calloc(1, sizeof(g7221_handle));
    if (!handle) goto fail;

    handle->bit_rate = bit_rate;
    handle->frame_size = bytes_per_frame;

    g7221_reset(handle);

    return handle;
fail:
    free(handle);
    return NULL;
}


int g7221_decode_frame(g7221_handle* handle, uint8_t* data, int16_t* out_samples) {
    int res;
    int mag_shift;
    int encrypted = handle->aes != NULL;

    /* first 0x10 bytes may be encrypted with AES. Original code also saves encrypted bytes,
     * then re-crypts after unpacking, presumably to guard against memdumps. */
    if (encrypted) {
        s14aes_decrypt(handle->aes, data);
    }

    /* Namco's decoder is designed so that out_samples can be set in place of mlt_coefs,
     * so we could avoid one extra buffer, but for clarity we'll leave as is */

    /* unpack data into MLT spectrum coefs */
    res = unpack_frame(handle->bit_rate, data, handle->frame_size, &mag_shift, handle->mlt_coefs, &handle->random_value, handle->test_errors);
    if (res < 0) goto fail;

    /* convert coefs to samples using reverse (inverse) MLT */
    res = rmlt_coefs_to_samples(mag_shift, handle->mlt_coefs, handle->old_samples, out_samples);
    if (res < 0) goto fail;

    /* Namco also sets number of codes/samples done from unpack_frame/rmlt (ptr arg),
     * but they seem unused */

    return 0;
fail:
    return res;
}

#if 0
int g7221_decode_empty(g7221_handle* handle, int16_t* out_samples) {
    static const uint8_t empty_frame[0x3c] = {
         0x1E,0x0B,0x89,0x40,0x02,0x4F,0x51,0x35, 0x10,0xA1,0xFE,0xDF,0x52,0x51,0x10,0x0B,
         0xF0,0x69,0x7B,0xAE,0x18,0x17,0x00,0x52, 0x07,0x74,0xF4,0x65,0xA2,0x58,0xD8,0x3F,
         0xD9,0xAA,0x65,0x35,0x2A,0x14,0xE3,0x58, 0xD7,0xC0,0xD2,0x02,0x5B,0x0E,0x2A,0x98,
         0xA3,0x04,0x5E,0x51,0xE5,0xC5,0xB2,0x14, 0xBF,0x58,0xFF,0xFF
    };
    int res;
    int mag_shift;

    /* This only seems to exist in older exes. Namco's samples don't reach EOF, so this
     * wouldn't need to be called. Doesn't seem to use encoder delay either. */

    res = unpack_frame(24000, empty_frame, 0x3c, &mag_shift, handle->mlt_coefs, &handle->random_value);
    if (res) goto fail;

    /* convert coefs to samples using reverse (inverse) MLT */
    res = rmlt_coefs_to_samples(mag_shift, handle->mlt_coefs, handle->old_samples, out_samples);
    if (res) goto fail;

    return 1;
fail:
    return 0;
}
#endif

void g7221_reset(g7221_handle* handle) {

    /* initialize old values (others get overwritten) */
    memset(&handle->old_samples, 0, sizeof(handle->old_samples));

    /* initialize the random number generator */
    handle->random_value = 0x10001;

    /* vs refdec: different default random. Namco used a global, so maybe multiple
     * bnsf playing at the same time would get slightly different results */
}

void g7221_free(g7221_handle* handle) {
    if (!handle)
        return;

    s14aes_close(handle->aes);
    free(handle);
}

int g7221_set_key(g7221_handle* handle, const uint8_t* key) {
    const int key_size = 192 / 8; /* only 192 bit mode */
    uint8_t temp_key[192 / 8];
    const char* mod_key = "Ua#oK3P94vdxX,ft*k-mnjoO"; /* constant for all platform/games */
    int i;

    if (!handle)
        goto fail;

    /* disable, useful for testing? */
    if (key == NULL) {
        s14aes_close(handle->aes);
        handle->aes = NULL;
        handle->test_errors = 1; /* force? */
        return 1;
    }

    /* init AES state (tables) or reuse if already exists */
    if (handle->aes == NULL) {
        handle->aes = s14aes_init();
        if (!handle->aes) goto fail;
    }

    handle->test_errors = 1;

    /* Base key is XORed probably against memdumps, as plain key would be part of the final AES key. However
     * roundkey is still in memdumps near AES state (~0x1310 from sbox table, that starts with 0x63,0x7c,0x77,0x7b...)
     * so it isn't too effective. XORing was originally done inside aes_expand_key during S14/S22 init. */
    for (i = 0; i < key_size; i++) {
        temp_key[i] = key[i] ^ mod_key[i];
    }

    /* reset new key */
    s14aes_set_key(handle->aes, temp_key);

    return 0;
fail:
    return -1;
}
