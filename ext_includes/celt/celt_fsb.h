/* Copyright (c) 2007-2008 CSIRO
   Copyright (c) 2007-2009 Xiph.Org Foundation
   Copyright (c) 2008 Gregory Maxwell 
   Written by Jean-Marc Valin and Gregory Maxwell */
/**
  @file celt.h
  @brief Contains all the functions for encoding and decoding audio
 */

/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   
   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   
   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


/* this is a custom celt.h based on libcelt's, only importing and 
 * renaming certain functions, allowing multiple libcelts to coexist.
 * See the original celt.h for extra info. */

#ifndef _CELT_FSB_H
#define _CELT_FSB_H

#include "celt_types.h"

#define EXPORT


/** No error */
#define CELT_OK                0
/** GET the bit-stream version for compatibility check */
#define CELT_GET_BITSTREAM_VERSION 2000

/** State of the decoder. One decoder state is needed for each stream.
    It is initialised once at the beginning of the stream. Do *not*
    re-initialise the state for every frame */
typedef struct CELT0061Decoder CELT0061Decoder;
typedef struct CELT0110Decoder CELT0110Decoder;

/** The mode contains all the information necessary to create an
    encoder. Both the encoder and decoder need to be initialised
    with exactly the same mode, otherwise the quality will be very 
    bad */
typedef struct CELT0061Mode CELT0061Mode;
typedef struct CELT0110Mode CELT0110Mode;


/* Mode calls */

/** Creates a new mode struct. This will be passed to an encoder or 
    decoder. The mode MUST NOT BE DESTROYED until the encoders and 
    decoders that use it are destroyed as well.
 @param Fs Sampling rate (32000 to 96000 Hz)
 @param channels Number of channels
 @param frame_size Number of samples (per channel) to encode in each 
                   packet (even values; 64 - 512)
 @param error Returned error code (if NULL, no error will be returned)
 @return A newly created mode
*/
EXPORT CELT0061Mode *celt_0061_mode_create(celt_int32 Fs, int channels, int frame_size, int *error);
EXPORT CELT0110Mode *celt_0110_mode_create(celt_int32 Fs, int frame_size, int *error);

/** Destroys a mode struct. Only call this after all encoders and 
    decoders using this mode are destroyed as well.
 @param mode Mode to be destroyed
*/
EXPORT void celt_0061_mode_destroy(CELT0061Mode *mode);
EXPORT void celt_0110_mode_destroy(CELT0110Mode *mode);

/** Query information from a mode */
EXPORT int celt_0061_mode_info(const CELT0061Mode *mode, int request, celt_int32 *value);
EXPORT int celt_0110_mode_info(const CELT0110Mode *mode, int request, celt_int32 *value);


/* Decoder stuff */

/** Creates a new decoder state. Each stream needs its own decoder state (can't
    be shared across simultaneous streams).
 @param mode Contains all the information about the characteristics of the
             stream (must be the same characteristics as used for the encoder)
 @param channels Number of channels
 @param error Returns an error code
 @return Newly created decoder state.
 */
EXPORT CELT0061Decoder *celt_0061_decoder_create(const CELT0061Mode *mode);
EXPORT CELT0110Decoder *celt_0110_decoder_create_custom(const CELT0110Mode *mode, int channels, int *error);

/** Destroys a a decoder state.
 @param st Decoder state to be destroyed
 */
EXPORT void celt_0061_decoder_destroy(CELT0061Decoder *st);
EXPORT void celt_0110_decoder_destroy(CELT0110Decoder *st);

/** Decodes a frame of audio.
 @param st Decoder state
 @param data Compressed data produced by an encoder
 @param len Number of bytes to read from "data". This MUST be exactly the number
            of bytes returned by the encoder. Using a larger value WILL NOT WORK.
 @param pcm One frame (frame_size samples per channel) of decoded PCM will be
            returned here in 16-bit PCM format (native endian). 
 @return Error code.
 */
EXPORT int celt_0061_decode(CELT0061Decoder *st, const unsigned char *data, int len, celt_int16 *pcm);
EXPORT int celt_0110_decode(CELT0110Decoder *st, const unsigned char *data, int len, celt_int16 *pcm, int frame_size);



#endif /*_CELT_FSB_H */
