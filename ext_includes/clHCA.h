#ifndef _clHCA_H
#define _clHCA_H

#ifdef __cplusplus
extern "C" {
#endif

enum { clHCA_samplesPerBlock = 0x80 * 8 };

/* Must pass at least 8 bytes of data to this function. Returns -1 on non-match, or
 * positive byte count on success. */
int clHCA_isOurFile0(const void *data);

/* Must pass a full header block for success. Returns 0 on success, -1 on failure. */
int clHCA_isOurFile1(const void *data, unsigned int size);

/* The opaque state structure. */
typedef struct clHCA clHCA;

/* In case you wish to allocate the structure on your own. */
int clHCA_sizeof();
void clHCA_clear(clHCA *, unsigned int ciphKey1, unsigned int ciphKey2);

/* Or you could let the library allocate it. */
clHCA * clHCA_new(unsigned int ciphKey1, unsigned int ciphKey2);
void clHCA_delete(clHCA *);

/* Requires a pre-allocated data structure.
 * Before any decoding may be performed, the header block must be passed in.
 * The recommended way of doing this is to detect the header length with
 * clHCA_isOurFile0, validate the header with clHCA_isOurFile1, then pass
 * it to this function, with the address of 0.
 * Subsequent decodes with non-zero address are assumed to be sample blocks,
 * and should be of the blockSize returned by the clHCA_getInfo function.
 * Returns 0 on success, -1 on failure. */
int clHCA_Decode(clHCA *, void *data, unsigned int size, unsigned int address);

/* This is the simplest decode function, for signed and clipped 16 bit samples.
 * May be called after clHCA_Decode, and will return the same data until the next
 * block of sample data is passed to clHCA_Decode. */
void clHCA_DecodeSamples16(clHCA *, signed short * outSamples);

typedef struct clHCA_stInfo {
	unsigned int version;
	unsigned int dataOffset;
	unsigned int samplingRate;
	unsigned int channelCount;
	unsigned int blockSize;
	unsigned int blockCount;
    unsigned int loopEnabled;
	unsigned int loopStart;
	unsigned int loopEnd;
} clHCA_stInfo;

/* Retrieve information relevant for decoding and playback with this function.
 * Must be called after successfully decoding a header block with clHCA_Decode,
 * or else it will fail.
 * Returns 0 on success, -1 on failure. */
int clHCA_getInfo(clHCA *, clHCA_stInfo *out);

#ifdef __cplusplus
}
#endif

#endif

