
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

    char sound_is_adpcm; /* 0 = no (see "additional notes" above) , 1 = yes */
    char sample_startpoint_present; /* -1 = failed to make startpoint, 1 = startpoint present */
    char sample_mode; /* 0 = creates decoding setup, 1 = continue decoding data with setup in place */
    char sample_has_base_setup_from_the_start; /* 0 = no, 1 = yes */
    char sample_pair_is_decoded; /* 0 = no, 1 = yes */

    unsigned char base_pair; /* represents a read byte from ADPCM data, consisting of two 4-bit nibbles each.*/
    long int base_scale; /* how loud should this sample be. */
    void* sample_hist; /* two pairs of signed 16-bit data, representing samples. yes, it is void. */
};

/* filter table consisting of 16 numbers each. */

const short int ongakukan_adpcm_filter[16] = { 233, 549, 453, 375, 310, 233, 233, 233, 233, 233, 233, 233, 310, 375, 453, 549 };

/* streamfile read function declararion, more may be added in the future. */

uint8_t read_u8_wrapper(ongakukan_adp_t* handle);

/* function declarations for the inner workings of codec data. */

char set_up_sample_startpoint(ongakukan_adp_t* handle);
void decode_ongakukan_adpcm_sample(ongakukan_adp_t* handle, short int* sample_hist);

/* codec management functions, meant to oversee and supervise ADP data from the top-down.
 * in layman terms, they control how ADP data should be handled and when. */

ongakukan_adp_t* boot_ongakukan_adpcm(STREAMFILE* sf, long int data_offset, long int data_size,
    char sound_is_adpcm, char sample_has_base_setup_from_the_start)
{
    ongakukan_adp_t* handle = NULL;

    /* allocate handle using malloc. */
    handle = malloc(sizeof(ongakukan_adp_t));
    if (!handle) goto fail;

    /* now, to set up the rest of the handle with the data we have... */
    handle->sf = sf;
    handle->data_offset = data_offset;
    handle->start_offset = data_offset;
    handle->data_size = data_size;
    handle->samples_filled = 0;
    handle->samples_consumed = 0;
    handle->sample_mode = 0;
    handle->sound_is_adpcm = sound_is_adpcm;
    handle->sample_has_base_setup_from_the_start = sample_has_base_setup_from_the_start;
    handle->sample_startpoint_present = set_up_sample_startpoint(handle);
    /* if we failed in planting up the seeds for an ADPCM decoder, we simply throw in the towel and take a walk in the park. */
    if (handle->sample_startpoint_present == -1) { goto fail; }

    return handle;
fail:
    free_all_ongakukan_adpcm(handle);
    return NULL;
}

void free_all_ongakukan_adpcm(ongakukan_adp_t* handle)
{
    if (!handle) return;
    close_streamfile(handle->sf);
    free(handle->sample_hist);
    free(handle);
}

void reset_all_ongakukan_adpcm(ongakukan_adp_t* handle)
{
    if (!handle) return;

    /* wipe out certain values from handle so we can start over. */
    handle->data_offset = handle->start_offset;
    handle->sample_pair_is_decoded = 0;
    handle->sample_mode = 0;
    handle->sample_has_base_setup_from_the_start = 1;
    handle->alt_sample_work1 = 0;
    handle->alt_sample_work2 = handle->sample_work;
}

void seek_ongakukan_adpcm_pos(ongakukan_adp_t* handle, long int target_sample)
{
    if (!handle) return;

    char ts_modulus = 0; /* ts_modulus is here to ensure target_sample gets rounded to a multiple of 2. */
    long int ts_data_offset = 0; /* ts_data_offset is basically data_offset but with (left(if PCM)/right(if ADPCM))-shifted target_sample calc by 1. */
    if (handle->sound_is_adpcm == 0) { ts_data_offset = target_sample << 1; }
    else { ts_data_offset = target_sample >> 1; ts_modulus = target_sample % 2; target_sample = target_sample - ts_modulus; }
    /* if ADPCM, right-shift the former first then have ts_modulus calc remainder of target_sample by 2 so we can subtract it with ts_modulus.
     * this is needed for the two counters that the decoder has that can both add and subtract with 2, respectively
     * (and in order, too; meaning one counter does "plus 2" while the other does "minus 2",
     * and though they're fairly useless ATM, you pretty much want to leave them alone). */

    /* anyway, we'll have to tell decoder that target_sample is calling and wants to go somewhere right now,
     * so we'll have data_offset reposition itself to where sound data for that sample ought to be 
     * and (as of now) reset basically all decode state up to this point so we can continue to decode all sample pairs without issue. */
    handle->data_offset = handle->start_offset + ts_data_offset;
    handle->sample_pair_is_decoded = 0;
    handle->sample_mode = 0;
    handle->sample_has_base_setup_from_the_start = 1;
    handle->alt_sample_work1 = target_sample;
    handle->alt_sample_work2 = handle->sample_work - target_sample;

    /* for now, just do what reset_all_ongakukan_adpcm does but for the current sample instead of literally everything.
     * seek_ongakukan_adpcm_pos in its current state is a bit more involved than the above, but works. */
}

long int grab_num_samples_from_ongakukan_adp(ongakukan_adp_t* handle)
{
    if (!handle) return 0;
    return handle->sample_work;
}

long int grab_samples_filled_from_ongakukan_adp(ongakukan_adp_t* handle)
{
    if (!handle) return 0;
    return handle->samples_filled;
}

void send_samples_filled_to_ongakukan_adp(long int samples_filled, ongakukan_adp_t* handle)
{
    if (!handle) return;
    handle->samples_filled = samples_filled;
}

long int grab_samples_consumed_from_ongakukan_adp(ongakukan_adp_t* handle)
{
    if (!handle) return 0;
    return handle->samples_consumed;
}

void send_samples_consumed_to_ongakukan_adp(long int samples_consumed, ongakukan_adp_t* handle)
{
    if (!handle) return;
    handle->samples_consumed = samples_consumed;
}

void* grab_sample_hist_from_ongakukan_adp(ongakukan_adp_t* handle)
{
    if (!handle) return 0;
    return handle->sample_hist;
}

/* function definitions for the inner workings of codec data. */

char set_up_sample_startpoint(ongakukan_adp_t* handle)
{
    /* malloc "sample hist" to 2 short ints */
    handle->sample_hist = malloc(2 * sizeof(short int));

    /* and make decoder fail hard if they don't have "sample hist" and opened streamfile object. */
    if (!handle->sample_hist) return -1;
    if (!handle->sf) return -1;

    if (handle->sound_is_adpcm == 0) {
        /* num_samples but for PCM16 sound data. */
        handle->sample_work = handle->data_size >> 1;
    }
    else {
        /* num_samples but for Ongakukan ADPCM sound data. */
        handle->sample_work = handle->data_size << 1;
    }
    /* set "beginning" and "end" sample vars and send a "message" that we went through no sample yet.*/
    handle->alt_sample_work1 = 0;
    handle->alt_sample_work2 = handle->sample_work;
    handle->sample_pair_is_decoded = 0;

    return 1;
}

void decode_ongakukan_adp_data(ongakukan_adp_t* handle)
{
    /* set samples_filled to 0 and have our decoder go through every sample that exists in the sound data.*/
    handle->samples_filled = 0;
    decode_ongakukan_adpcm_sample(handle, handle->sample_hist);
    /* if setup is established for further decoding, switch gears and have the decoder use that setup for as long as possible. */
    if (handle->sample_has_base_setup_from_the_start == 1)
    {
        handle->sample_has_base_setup_from_the_start = 0;
        handle->sample_mode = 1;
    }
    /* if sample pair is decoded, advance to next byte, tell our handle that we went through 2 samples and make decoder go through next available data again. */
    if (handle->sample_pair_is_decoded == 1)
    {
        handle->data_offset++;
        handle->alt_sample_work1 += 2;
        handle->alt_sample_work2 -= 2;
        handle->samples_consumed = 0;
        handle->samples_filled = 2;
        handle->sample_pair_is_decoded = 0;
    }
}

void decode_ongakukan_adpcm_sample(ongakukan_adp_t* handle, short int* sample_hist)
{
    unsigned char nibble1 = 0, nibble2 = 0; /* two chars representing a 4-bit nibble. */
    long int nibble1_1 = 0, nibble2_1 = 0; /* two long ints representing pure sample data. */

    if (handle->sample_pair_is_decoded == 0)
    {
        /* sample_mode being 0 means we can just do a setup for future sample decoding so we have nothing to worry about in the future. */
        if (handle->sample_mode == 0)
        {
            /* set "base scale", two "sample hist"s, and "base pair", respectively. */
            handle->base_scale = 0x10; /* yes, this is how "base scale" is set; to 16. */
            *(sample_hist+0) = 0; /* dereference first sample hist pos to something we can use. */
            *(sample_hist+1) = 0; /* dereference second sample hist pos to something we can use. */
            handle->base_pair = 0; /* set representing byte to 0 while we're at it. */
        }
        /* "pinch off" of a single-byte read. remember that we need two nibbles. */
        handle->base_pair = (uint8_t)read_u8_wrapper(handle);

        /* now pick two nibbles, subtract them, reverse the order of said nibbles so we can calc them one-by-one,
         * and finally tell handle that we're done with two samples and would like to take a break, please. */
        nibble1 = handle->base_pair & 0xf;
        nibble1_1 = nibble1 + -8;
        nibble2 = (handle->base_pair >> 4) & 0xf;
        nibble2_1 = nibble2 + -8;
        nibble2_1 = nibble2_1 * handle->base_scale;
        *(sample_hist+0) = *(sample_hist+1) + nibble2_1;
        handle->base_scale = (handle->base_scale * (ongakukan_adpcm_filter[nibble2])) >> 8;
        nibble1_1 = nibble1_1 * handle->base_scale;
        *(sample_hist+1) = *(sample_hist+0) + nibble1_1;
        handle->base_scale = (handle->base_scale * (ongakukan_adpcm_filter[nibble1])) >> 8;
        handle->sample_pair_is_decoded = 1;
    }
}

/* streamfile read function definitions at the very bottom. */

uint8_t read_u8_wrapper(ongakukan_adp_t* handle)
{
    /* if data_offset minus start_offset goes beyond data_size, return 0. */
    if ((handle->data_offset - handle->start_offset) > handle->data_size) return 0;
    /* if data_offset minus start_offset goes below 0, same. */
    if ((handle->data_offset - handle->start_offset) < 0) return 0;
    /* otherwise, read the byte from data_offset and see history get made. */
    return read_u8((off_t)(handle->data_offset), handle->sf);
}
