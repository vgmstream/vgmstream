
/* Decodes Ongakukan ADPCM, found in their PS2 and PSP games.
 * Basically their take on ADPCM with some companding and quantization involved.
 *
 * Original decoder is a mix of COP0 and VU1 code, however PS2 floats aren't actually used (if at all)
 * when it comes to converting encoded sample data (consisting of a single byte with two 4-bit nibbles, respectively) to PCM16.
 *
 * The decoder you see here is a hand-crafted, faithful C adaptation of original MIPS R5900 (PS2) and R4000 (PSP) code, from various executables of their games.
 * As a consequence of all this, a new, entirely custom decoder had to be designed from the ground-up into vgmstream. No info surrounding this codec was available. */

/* Additional notes:
 * - This code does not support PCM16 sound data, in any way, shape, or form.
 * -- Ongakukan's internal sound engine from their PS2 and PSP games allow for only two codecs: signed PCM16, and their own take on ADPCM, respectively.
 * -- However, much of that support is reliant on a flag that's set to either one of the two codecs depending on the opened file extension.
 *    Basically, how it works is: if sound data is "PCM16" (available to "wav" and "ads" files), set flag to 0.
 *    If sound data is "ADPCM" (available to "adp" files), set it to 1.
 *    Code handles this flag as a boolean var; 0 is "false" and 1 is "true".
 * -- As vgmstream has built-in support for the former codec (and the many metas that use it) however, despite being fairly easy to add here,
 *    re-implementing one from scratch would be a wasted effort regardless; it is consequentially not included. */

#include <stdlib.h>
#include "../../util/reader_sf.h"
#include "ongakukan_adp_lib.h"

/* the struct that oversees everything. */

struct ongakukan_adp_t
{
    STREAMFILE* sf; /* streamfile var. */

    long int data_offset; /* current offset of data that's being read. */
    long int start_offset; /* base offset of encoded sound data. */
    long int data_size; /* sound data size, basically ADP size if it didn't have 44 bytes more. */
    long int sample_work; /* total number of samples, calc'd using data_size as a base. */
    long int alt_sample_work1; /* represents current number of samples as they're decoded. */
    long int alt_sample_work2; /* represents the many samples left to go through. */
    long int samples_filled; /* how many samples were filled to vgmstream buffer. */
    long int samples_consumed; /* how many samples vgmstream buffer had to consume. */

    bool sound_is_adpcm; /* false = no (see "additional notes" above) , true = yes */
    bool sample_startpoint_present; /* false = failed to make startpoint, true = startpoint present */
    char sample_mode; /* 0 = creates decoding setup, 1 = continue decoding data with setup in place */
    bool sample_pair_is_decoded; /* false = no, true = yes */

    unsigned char base_pair; /* represents a read byte from ADPCM data, consisting of two 4-bit nibbles each.*/
    long int base_scale; /* how loud should this sample be. */
    short int sample_hist[2]; /* two pairs of signed 16-bit data, representing samples. yes, it is void. */
};

/* filter table consisting of 16 numbers each. */

static const short int ongakukan_adpcm_filter[16] = { 233, 549, 453, 375, 310, 233, 233, 233, 233, 233, 233, 233, 310, 375, 453, 549 };

/* streamfile read function declararion, more may be added in the future. */

static uint8_t read_u8_wrapper(ongakukan_adp_t* handle);

/* function declarations for the inner workings of codec data. */

static bool set_up_sample_startpoint(ongakukan_adp_t* handle);
static void decode_ongakukan_adpcm_samples(ongakukan_adp_t* handle);

/* codec management functions, meant to oversee and supervise ADP data from the top-down.
 * in layman terms, they control how ADP data should be handled and when. */

ongakukan_adp_t* ongakukan_adpcm_init(STREAMFILE* sf, long int data_offset, long int data_size, bool sound_is_adpcm)
{
    ongakukan_adp_t* handle = NULL;

    if (!sound_is_adpcm)
        return NULL;

    /* allocate handle. */
    handle = calloc(1, sizeof(ongakukan_adp_t));
    if (!handle) goto fail;

    /* now, to set up the rest of the handle with the data we have... */
    handle->sf = sf;
    handle->data_offset = data_offset;
    handle->start_offset = data_offset;
    handle->data_size = data_size;
    handle->sample_mode = 0;
    handle->sound_is_adpcm = sound_is_adpcm;
    handle->sample_startpoint_present = set_up_sample_startpoint(handle);
    /* if we failed in planting up the seeds for an ADPCM decoder, we simply throw in the towel and take a walk in the park. */
    if (handle->sample_startpoint_present == false) { goto fail; }

    return handle;
fail:
    ongakukan_adpcm_free(handle);
    return NULL;
}

void ongakukan_adpcm_free(ongakukan_adp_t* handle)
{
    if (!handle) return;
    free(handle);
}

void ongakukan_adpcm_reset(ongakukan_adp_t* handle)
{
    if (!handle) return;

    /* wipe out certain values from handle so we can start over. */
    handle->data_offset = handle->start_offset;
    handle->sample_pair_is_decoded = false;
    handle->sample_mode = 0;
    handle->alt_sample_work1 = 0;
    handle->alt_sample_work2 = handle->sample_work;
}

void ongakukan_adpcm_seek(ongakukan_adp_t* handle, long int target_sample)
{
    if (!handle) return;

    char ts_modulus = 0; /* ts_modulus is here to ensure target_sample gets rounded to a multiple of 2. */
    long int ts_data_offset = 0; /* ts_data_offset is basically data_offset but with (left(if PCM)/right(if ADPCM))-shifted target_sample calc by 1. */
    ts_data_offset = target_sample >> 1;
    ts_modulus = target_sample % 2;
    target_sample = target_sample - ts_modulus;
    /* if ADPCM, right-shift the former first then have ts_modulus calc remainder of target_sample by 2 so we can subtract it with ts_modulus.
     * this is needed for the two counters that the decoder has that can both add and subtract with 2, respectively
     * (and in order, too; meaning one counter does "plus 2" while the other does "minus 2",
     * and though they're fairly useless ATM, you pretty much want to leave them alone). */

    /* anyway, we'll have to tell decoder that target_sample is calling and wants to go somewhere right now,
     * so we'll have data_offset reposition itself to where sound data for that sample ought to be
     * and (as of now) reset basically all decode state up to this point so we can continue to decode all sample pairs without issue. */
    handle->data_offset = handle->start_offset + ts_data_offset;
    handle->sample_pair_is_decoded = false;
    handle->sample_mode = 0;
    handle->alt_sample_work1 = target_sample;
    handle->alt_sample_work2 = handle->sample_work - target_sample;

    /* for now, just do what reset_all_ongakukan_adpcm does but for the current sample instead of literally everything.
     * seek_ongakukan_adpcm_pos in its current state is a bit more involved than the above, but works. */
}

long int ongakukan_adpcm_get_num_samples(ongakukan_adp_t* handle)
{
    if (!handle) return 0;
    return handle->sample_work;
}

short* ongakukan_adpcm_get_sample_hist(ongakukan_adp_t* handle)
{
    if (!handle) return 0;
    return handle->sample_hist;
}

/* function definitions for the inner workings of codec data. */

static bool set_up_sample_startpoint(ongakukan_adp_t* handle)
{
    /* make decoder fail hard if streamfile object isn't opened or downright useless. */
    if (!handle->sf) return false;

    if (handle->sound_is_adpcm == 0) { return false; }
    else { /* num_samples but for Ongakukan ADPCM sound data. */ handle->sample_work = handle->data_size << 1; }
    /* set "beginning" and "end" sample vars and send a "message" that we went through no sample yet.*/
    handle->alt_sample_work1 = 0;
    handle->alt_sample_work2 = handle->sample_work;
    handle->sample_pair_is_decoded = false;

    return true;
}

void ongakukan_adpcm_decode_data(ongakukan_adp_t* handle)
{
    /* set samples_filled to 0 and have our decoder go through every sample that exists in the sound data.*/
    decode_ongakukan_adpcm_samples(handle);
    /* if setup is established for further decoding, switch gears and have the decoder use that setup for as long as possible. */
    /* if sample pair is decoded, advance to next byte, tell our handle that we went through 2 samples and make decoder go through next available data again. */
    if (handle->sample_pair_is_decoded == true)
    {
        handle->data_offset++;
        handle->alt_sample_work1 += 2;
        handle->alt_sample_work2 -= 2;
        handle->sample_pair_is_decoded = false;
    }
}

static void decode_ongakukan_adpcm_samples(ongakukan_adp_t* handle)
{
    unsigned char nibble1 = 0, nibble2 = 0; /* two chars representing a 4-bit nibble. */
    long int nibble1_1 = 0, nibble2_1 = 0; /* two long ints representing pure sample data. */

    if (handle->sample_pair_is_decoded == false)
    {
        /* sample_mode being 0 means we can just do a setup for future sample decoding so we have nothing to worry about in the future. */
        if (handle->sample_mode == 0)
        {
            /* set "base scale", two "sample hist"s, and "base pair", respectively. */
            handle->base_scale = 0x10;
            handle->sample_hist[0] = 0;
            handle->sample_hist[1] = 0;
            handle->base_pair = 0;
            handle->sample_mode = 1; /* indicates we have the setup we need to decode samples. */
        }
        handle->base_pair = (uint8_t)read_u8_wrapper(handle);

        nibble1 = handle->base_pair & 0xf;
        nibble1_1 = nibble1 + -8;
        nibble2 = (handle->base_pair >> 4) & 0xf;
        nibble2_1 = nibble2 + -8;
        nibble2_1 = nibble2_1 * handle->base_scale;
        handle->sample_hist[0] = handle->sample_hist[1] + nibble2_1;
        handle->base_scale = (handle->base_scale * (ongakukan_adpcm_filter[nibble2])) >> 8;
        nibble1_1 = nibble1_1 * handle->base_scale;
        handle->sample_hist[1] = handle->sample_hist[0] + nibble1_1;
        handle->base_scale = (handle->base_scale * (ongakukan_adpcm_filter[nibble1])) >> 8;
        handle->sample_pair_is_decoded = true;
    }
}

/* streamfile read function definitions at the very bottom. */

static uint8_t read_u8_wrapper(ongakukan_adp_t* handle)
{
    if ((handle->data_offset - handle->start_offset) > handle->data_size) return 0;
    if ((handle->data_offset - handle->start_offset) < 0) return 0;
    return read_u8((off_t)(handle->data_offset), handle->sf);
}
