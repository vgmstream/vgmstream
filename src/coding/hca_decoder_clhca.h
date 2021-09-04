#ifndef _clHCA_H
#define _clHCA_H

#ifdef __cplusplus
extern "C" {
#endif


/* Must pass at least 8 bytes of data to this function.
 * Returns <0 on non-match, or header size on success. */
int clHCA_isOurFile(const void *data, unsigned int size);

/* The opaque state structure. */
typedef struct clHCA clHCA;

/* In case you wish to allocate and reset the structure on your own. */
int clHCA_sizeof(void);
void clHCA_clear(clHCA *);
void clHCA_done(clHCA *);

/* Or you could let the library allocate it. */
clHCA * clHCA_new(void);
void clHCA_delete(clHCA *);

/* Parses the HCA header. Must be called before any decoding may be performed,
 * and size must be at least headerSize long. The recommended way is to detect
 * the header length with clHCA_isOurFile, then read data and call this.
 * May be called multiple times to reset decoder state.
 * Returns 0 on success, <0 on failure. */
int clHCA_DecodeHeader(clHCA *, const void *data, unsigned int size);

typedef struct clHCA_stInfo {
    unsigned int version;
    unsigned int headerSize;
    unsigned int samplingRate;
    unsigned int channelCount;
    unsigned int blockSize;
    unsigned int blockCount;
    unsigned int encoderDelay;      /* samples appended to the beginning */
    unsigned int encoderPadding;    /* samples appended to the end */
    unsigned int loopEnabled;
    unsigned int loopStartBlock;
    unsigned int loopEndBlock;
    unsigned int loopStartDelay;    /* samples in block before loop starts */
    unsigned int loopEndPadding;    /* samples in block after loop ends */
    unsigned int samplesPerBlock;   /* should be 1024 */
    const char *comment;
    unsigned int encryptionEnabled; /* requires keycode */

    /* Derived sample formulas:
     * - sample count: blockCount*samplesPerBlock - encoderDelay - encoderPadding;
     * - loop start sample = loopStartBlock*samplesPerBlock - encoderDelay + loopStartDelay
     * - loop end sample = loopEndBlock*samplesPerBlock - encoderDelay + (samplesPerBlock - info.loopEndPadding)
     */
} clHCA_stInfo;

/* Retrieves header information for decoding and playback (it's the caller's responsability
 * to apply looping, encoder delay/skip samples, etc). May be called after clHCA_DecodeHeader.
 * Returns 0 on success, <0 on failure. */
int clHCA_getInfo(clHCA *, clHCA_stInfo *out);

/* Decodes a single frame, from data after headerSize. Should be called after
 * clHCA_DecodeHeader and size must be at least blockSize long.
 * Data may be modified if encrypted.
 * Returns 0 on success, <0 on failure. */
int clHCA_DecodeBlock(clHCA *, void *data, unsigned int size);

/* Extracts signed and clipped 16 bit samples into sample buffer.
 * May be called after clHCA_DecodeBlock, and will return the same data until
 * next decode. Buffer must be at least (samplesPerBlock*channels) long. */
void clHCA_ReadSamples16(clHCA *, signed short * outSamples);

/* Sets a 64 bit encryption key, to properly decode blocks. This may be called
 * multiple times to change the key, before or after clHCA_DecodeHeader.
 * Key is ignored if the file is not encrypted. */
void clHCA_SetKey(clHCA *, unsigned long long keycode);

/* Tests a single frame for validity, mainly to test if current key is correct.
 * Returns <0 on incorrect block (wrong key), 0 on silent block (not useful to determine)
 * and >0 if block is correct (the closer to 1 the more likely).
 * Incorrect keys may give a few valid frames, so it's best to test a number of them
 * and select the key with scores closer to 1. */
int clHCA_TestBlock(clHCA *hca, void *data, unsigned int size);

/* Resets the internal decode state, used when restarting to decode the file from the beginning.
 * Without it there are minor differences, mainly useful when testing a new key. */
void clHCA_DecodeReset(clHCA * hca);

#ifdef __cplusplus
}
#endif

#endif
