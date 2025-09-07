#include "../vgmstream.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "decode.h"
#include "mixing.h"
#include "plugins.h"
#include "sbuf.h"
#include "codec_info.h"

#include "../util/log.h"
#include "decode_state.h"


static void* decode_state_init() {
    return calloc(1, sizeof(decode_state_t));
}

static void decode_state_reset(VGMSTREAM* vgmstream) {
    if (!vgmstream->decode_state)
        return;
    memset(vgmstream->decode_state, 0, sizeof(decode_state_t));
}

static void decode_state_free(VGMSTREAM* vgmstream) {
    free(vgmstream->decode_state);
}

// this could be part of the VGMSTREAM but for now keep separate as it simplifies 
// some loop-related stuff
void* decode_init() {
    return decode_state_init();
}


/* custom codec handling, not exactly "decode" stuff but here to simplify adding new codecs */

void decode_free(VGMSTREAM* vgmstream) {
    decode_state_free(vgmstream);

    if (!vgmstream->codec_data)
        return;
    
    const codec_info_t* codec_info = codec_get_info(vgmstream);
    if (codec_info) {
        codec_info->free(vgmstream->codec_data);
        return;
    }

    if (vgmstream->coding_type == coding_CIRCUS_VQ) {
        free_circus_vq(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_ICE_RANGE ||
        vgmstream->coding_type == coding_ICE_DCT) {
        free_ice(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_UBI_ADPCM) {
        free_ubi_adpcm(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_ONGAKUKAN_ADPCM) {
        free_ongakukan_adp(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_EA_MT) {
        free_ea_mt(vgmstream->codec_data, vgmstream->channels);
    }

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type == coding_MP4_AAC) {
        free_mp4_aac(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_G7221
    if (vgmstream->coding_type == coding_G7221C) {
        free_g7221(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_G719
    if (vgmstream->coding_type == coding_G719) {
        free_g719(vgmstream->codec_data, vgmstream->channels);
    }
#endif

    if (vgmstream->coding_type == coding_ACM) {
        free_acm(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_NWA) {
        free_nwa(vgmstream->codec_data);
    }
}


void decode_seek(VGMSTREAM* vgmstream, int32_t sample) {
    decode_state_reset(vgmstream);

    if (!vgmstream->codec_data)
        return;

    const codec_info_t* codec_info = codec_get_info(vgmstream);
    if (codec_info) {
        codec_info->seek(vgmstream, sample);
        return;
    }

    if (vgmstream->coding_type == coding_CIRCUS_VQ) {
        seek_circus_vq(vgmstream->codec_data, sample);
    }

    if (vgmstream->coding_type == coding_ICE_RANGE ||
        vgmstream->coding_type == coding_ICE_DCT) {
        seek_ice(vgmstream->codec_data, sample);
    }

    if (vgmstream->coding_type == coding_UBI_ADPCM) {
        seek_ubi_adpcm(vgmstream->codec_data, sample);
    }

    if (vgmstream->coding_type == coding_ONGAKUKAN_ADPCM) {
        seek_ongakukan_adp(vgmstream->codec_data, sample);
    }

    if (vgmstream->coding_type == coding_EA_MT) {
        seek_ea_mt(vgmstream, sample);
    }

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type == coding_MP4_AAC) {
        seek_mp4_aac(vgmstream, sample);
    }
#endif

    if (vgmstream->coding_type == coding_NWA) {
        seek_nwa(vgmstream->codec_data, sample);
    }
}

void decode_loop(VGMSTREAM* vgmstream) {
    decode_seek(vgmstream, vgmstream->loop_current_sample);
}

void decode_reset(VGMSTREAM* vgmstream) {
    decode_state_reset(vgmstream);

    if (!vgmstream->codec_data)
        return;

    const codec_info_t* codec_info = codec_get_info(vgmstream);
    if (codec_info) {
        codec_info->reset(vgmstream->codec_data);
        return;
    }

    if (vgmstream->coding_type == coding_CIRCUS_VQ) {
        reset_circus_vq(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_ICE_RANGE ||
        vgmstream->coding_type == coding_ICE_DCT) {
        reset_ice(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_UBI_ADPCM) {
        reset_ubi_adpcm(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_ONGAKUKAN_ADPCM) {
        reset_ongakukan_adp(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_EA_MT) {
        reset_ea_mt(vgmstream);
    }

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type == coding_MP4_AAC) {
        reset_mp4_aac(vgmstream);
    }
#endif

#ifdef VGM_USE_G7221
    if (vgmstream->coding_type == coding_G7221C) {
        reset_g7221(vgmstream->codec_data);
    }
#endif

#ifdef VGM_USE_G719
    if (vgmstream->coding_type == coding_G719) {
        reset_g719(vgmstream->codec_data, vgmstream->channels);
    }
#endif

    if (vgmstream->coding_type == coding_ACM) {
        reset_acm(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_NWA) {
        reset_nwa(vgmstream->codec_data);
    }
}


/* Get the number of samples of a single frame (smallest self-contained sample group, 1/N channels) */
int decode_get_samples_per_frame(VGMSTREAM* vgmstream) {
    /* Value returned here is the max (or less) that vgmstream will ask a decoder per
     * "decode_x" call. Decoders with variable samples per frame or internal discard
     * may return 0 here and handle arbitrary samples_to_do values internally
     * (or some internal sample buffer max too). */

    switch (vgmstream->coding_type) {
        case coding_SILENCE:
            return 0;

        case coding_CRI_ADX:
        case coding_CRI_ADX_fixed:
        case coding_CRI_ADX_exp:
        case coding_CRI_ADX_enc_8:
        case coding_CRI_ADX_enc_9:
            return (vgmstream->interleave_block_size - 2) * 2;

        case coding_NGC_DSP:
        case coding_NGC_DSP_subint:
            return 14;
        case coding_AFC:
        case coding_AFC_2bit:
        case coding_VADPCM:
            return 16;
        case coding_NGC_DTK:
            return 28;
        case coding_G721:
            return 1;

        case coding_PCM16LE:
        case coding_PCM16BE:
        case coding_PCM16_int:
        case coding_PCM8:
        case coding_PCM8_int:
        case coding_PCM8_U:
        case coding_PCM8_U_int:
        case coding_PCM8_SB:
        case coding_ULAW:
        case coding_ULAW_int:
        case coding_ALAW:
        case coding_PCMFLOAT:
        case coding_PCM24LE:
        case coding_PCM24BE:
        case coding_PCM32LE:
            return 1;
        case coding_SDX2:
        case coding_SDX2_int:
        case coding_CBD2:
        case coding_CBD2_int:
        case coding_ACM:
        case coding_DERF:
        case coding_WADY:
        case coding_DPCM_KCEJ:
        case coding_NWA:
        case coding_SASSC:
        case coding_CIRCUS_ADPCM:
            return 1;

        case coding_IMA:
        case coding_DVI_IMA:
        case coding_SNDS_IMA:
        case coding_QD_IMA:
        case coding_UBI_IMA:
        case coding_UBI_SCE_IMA:
        case coding_OKI16:
        case coding_OKI4S:
        case coding_MTF_IMA:
            return 1;
        case coding_PCM4:
        case coding_PCM4_U:
        case coding_IMA_mono:
        case coding_DVI_IMA_mono:
        case coding_CAMELOT_IMA:
        case coding_WV6_IMA:
        case coding_HV_IMA:
        case coding_SQEX_IMA:
        case coding_BLITZ_IMA:
        case coding_PCFX:
            return 2;
        case coding_XBOX_IMA:
        case coding_XBOX_IMA_mch:
        case coding_XBOX_IMA_mono:
        case coding_FSB_IMA:
        case coding_WWISE_IMA:
        case coding_CD_IMA: /* (0x24 - 0x04) * 2 */
        case coding_CRANKCASE_IMA: /* (0x23 - 0x3) * 2 */
            return 64;
        case coding_APPLE_IMA4:
            return 64;
        case coding_MS_IMA:
            return ((vgmstream->frame_size - 0x04*vgmstream->channels) * 2 / vgmstream->channels) + 1; /* +1 from header sample */
        case coding_MS_IMA_mono:
            return ((vgmstream->frame_size - 0x04) * 2) + 1; /* +1 from header sample */
        case coding_REF_IMA:
            return ((vgmstream->interleave_block_size - 0x04 * vgmstream->channels) * 2 / vgmstream->channels) + 1; /* +1 from header sample */
        case coding_RAD_IMA:
            return (vgmstream->interleave_block_size - 0x04*vgmstream->channels) * 2 / vgmstream->channels;
        case coding_NDS_IMA:
        case coding_DAT4_IMA:
            return (vgmstream->interleave_block_size - 0x04) * 2;
        case coding_AWC_IMA:
            return (0x800 - 0x04) * 2;
        case coding_RAD_IMA_mono:
            return 32;
        case coding_H4M_IMA:
            return 0; /* variable (block-controlled) */

        case coding_XA:
        case coding_XA_EA:
            return 28*8 / vgmstream->channels; /* 8 subframes per frame, mono/stereo */
        case coding_XA8:
            return 28*4 / vgmstream->channels; /* 4 subframes per frame, mono/stereo */
        case coding_PSX:
        case coding_PSX_badflags:
        case coding_HEVAG:
            return 28;
        case coding_PSX_cfg:
        case coding_PSX_pivotal:
            return (vgmstream->frame_size - 0x01) * 2; /* size 0x01 header */

        case coding_EA_XA:
        case coding_EA_XA_int:
        case coding_EA_XA_V2:
        case coding_MAXIS_XA:
            return 28;
        case coding_EA_XAS_V0:
            return 32;
        case coding_EA_XAS_V1:
            return 128;

        case coding_MSADPCM:
            return (vgmstream->frame_size - 0x07*vgmstream->channels)*2 / vgmstream->channels + 2;
        case coding_MSADPCM_mono:
        case coding_MSADPCM_ck:
            return (vgmstream->frame_size - 0x07)*2 + 2;
        case coding_WS: /* only works if output sample size is 8 bit, which always is for WS ADPCM */
            return vgmstream->ws_output_size;
        case coding_AICA:
        case coding_CP_YM:
            return 1;
        case coding_AICA_int:
            return 2;
        case coding_ASKA:
            return (vgmstream->frame_size - 0x04*vgmstream->channels) * 2 / vgmstream->channels;
        case coding_NXAP:
            return (0x40-0x04) * 2;
        case coding_NDS_PROCYON:
            return 30;
        case coding_LEVEL5:
            return 32;
        case coding_LSF:
            return 54;

#ifdef VGM_USE_G7221
        case coding_G7221C:
            return 32000/50; /* Siren7: 16000/50 */
#endif
#ifdef VGM_USE_G719
        case coding_G719:
            return 48000/50;
#endif
        case coding_MTAF:
            return 128*2;
        case coding_MTA2:
            return 128*2;
        case coding_MPC3:
            return 10;
        case coding_FADPCM:
            return 256; /* (0x8c - 0xc) * 2 */
        case coding_ASF:
            return 32;  /* (0x11 - 0x1) * 2 */
        case coding_TANTALUS:
            return 30; /* (0x10 - 0x01) * 2 */
        case coding_DSA:
            return 14;  /* (0x08 - 0x1) * 2 */
        case coding_XMD:
            return (vgmstream->interleave_block_size - 0x06)*2 + 2;
        case coding_PTADPCM:
            return (vgmstream->interleave_block_size - 0x05)*2 + 2;
        case coding_UBI_ADPCM:
            return 0; /* varies per mode */
        case coding_IMUSE:
            return 0; /* varies per frame */
        case coding_ONGAKUKAN_ADPCM:
            return 0; /* actually 1. */
        case coding_EA_MT:
            return 0; /* 432, but variable in looped files */
        case coding_CIRCUS_VQ:
            return 0;
        case coding_ICE_RANGE:
        case coding_ICE_DCT:
            return 0; /* ~100 (range), ~16 (DCT) */
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
        case coding_MP4_AAC:
            return mp4_get_samples_per_frame(vgmstream->codec_data);
#endif
        default:
            return 0;
    }
}

/* Get the number of bytes of a single frame (smallest self-contained byte group, 1/N channels) */
int decode_get_frame_size(VGMSTREAM* vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_SILENCE:
            return 0;

        case coding_CRI_ADX:
        case coding_CRI_ADX_fixed:
        case coding_CRI_ADX_exp:
        case coding_CRI_ADX_enc_8:
        case coding_CRI_ADX_enc_9:
            return vgmstream->interleave_block_size;

        case coding_NGC_DSP:
            return 0x08;
        case coding_NGC_DSP_subint:
            return 0x08 * vgmstream->channels;
        case coding_AFC:
        case coding_VADPCM:
            return 0x09;
        case coding_AFC_2bit:
            return 0x05;
        case coding_NGC_DTK:
            return 0x20;
        case coding_G721:
            return 0;

        case coding_PCM16LE:
        case coding_PCM16BE:
        case coding_PCM16_int:
            return 0x02;
        case coding_PCM8:
        case coding_PCM8_int:
        case coding_PCM8_U:
        case coding_PCM8_U_int:
        case coding_PCM8_SB:
        case coding_ULAW:
        case coding_ULAW_int:
        case coding_ALAW:
            return 0x01;
        case coding_PCMFLOAT:
        case coding_PCM32LE:
            return 0x04;
        case coding_PCM24LE:
        case coding_PCM24BE:
            return 0x03;

        case coding_SDX2:
        case coding_SDX2_int:
        case coding_CBD2:
        case coding_CBD2_int:
        case coding_DERF:
        case coding_WADY:
        case coding_DPCM_KCEJ:
        case coding_NWA:
        case coding_SASSC:
        case coding_CIRCUS_ADPCM:
            return 0x01;

        case coding_PCM4:
        case coding_PCM4_U:
        case coding_IMA:
        case coding_IMA_mono:
        case coding_DVI_IMA:
        case coding_DVI_IMA_mono:
        case coding_CAMELOT_IMA:
        case coding_WV6_IMA:
        case coding_HV_IMA:
        case coding_SQEX_IMA:
        case coding_BLITZ_IMA:
        case coding_PCFX:
        case coding_OKI16:
        case coding_OKI4S:
        case coding_MTF_IMA:
        case coding_SNDS_IMA:
            return 0x01;
        case coding_RAD_IMA:
        case coding_NDS_IMA:
        case coding_DAT4_IMA:
        case coding_REF_IMA:
            return vgmstream->interleave_block_size;
        case coding_MS_IMA:
        case coding_MS_IMA_mono:
            return vgmstream->frame_size;
        case coding_AWC_IMA:
            return 0x800;
        case coding_RAD_IMA_mono:
            return 0x14;
        case coding_QD_IMA:
            return 0; //todo: 0x01?
        case coding_UBI_IMA: /* variable (PCM then IMA) */
            return 0;
        case coding_UBI_SCE_IMA:
            return 0;
        case coding_XBOX_IMA:
            //todo should be  0x48 when stereo, but blocked/interleave layout don't understand stereo codecs
            return 0x24; //vgmstream->channels==1 ? 0x24 : 0x48;
        case coding_XBOX_IMA_mono:
        case coding_WWISE_IMA:
        case coding_CD_IMA:
            return 0x24;
        case coding_CRANKCASE_IMA:
            return 0x23;
        case coding_XBOX_IMA_mch:
        case coding_FSB_IMA:
            return 0x24 * vgmstream->channels;
        case coding_APPLE_IMA4:
            return 0x22;
        case coding_H4M_IMA:
            return 0x00; /* variable (block-controlled) */

        case coding_XA:
        case coding_XA_EA:
        case coding_XA8:
            return 0x80;
        case coding_PSX:
        case coding_PSX_badflags:
        case coding_HEVAG:
            return 0x10;
        case coding_PSX_cfg:
        case coding_PSX_pivotal:
            return vgmstream->frame_size;

        case coding_EA_XA:
            return 0x1E;
        case coding_EA_XA_int:
            return 0x0F;
        case coding_MAXIS_XA:
            return 0x0F*vgmstream->channels;
        case coding_EA_XA_V2:
            return 0; /* variable (ADPCM frames of 0x0f or PCM frames of 0x3d) */
        case coding_EA_XAS_V0:
            return 0xF+0x02+0x02;
        case coding_EA_XAS_V1:
            return 0x4c*vgmstream->channels;

        case coding_MSADPCM:
        case coding_MSADPCM_mono:
        case coding_MSADPCM_ck:
            return vgmstream->frame_size;
        case coding_WS:
            return vgmstream->current_block_size;
        case coding_AICA:
        case coding_AICA_int:
        case coding_CP_YM:
            return 0x01;
        case coding_ASKA:
            return vgmstream->frame_size;
        case coding_NXAP:
            return 0x40;
        case coding_NDS_PROCYON:
            return 0x10;
        case coding_LEVEL5:
            return 0x12;
        case coding_LSF:
            return 0x1C;

#ifdef VGM_USE_G7221
        case coding_G7221C:
#endif
#ifdef VGM_USE_G719
        case coding_G719:
#endif
        case coding_MTAF:
            return vgmstream->interleave_block_size;
        case coding_MTA2:
            return 0x90;
        case coding_MPC3:
            return 0x04;
        case coding_FADPCM:
            return 0x8c;
        case coding_ASF:
            return 0x11;
        case coding_TANTALUS:
            return 0x10;
        case coding_DSA:
            return 0x08;
        case coding_XMD:
            return vgmstream->interleave_block_size;
        case coding_PTADPCM:
            return vgmstream->interleave_block_size;
        /* UBI_ADPCM: varies per mode? */
        /* IMUSE: VBR */
        /* EA_MT: VBR, frames of bit counts or PCM frames */
        /* CELT FSB: varies, usually 0x80-100 */
        /* TAC: VBR around ~0x200-300 */
        default: /* (VBR or managed by decoder) */
            return 0;
    }
}

/* In NDS IMA the frame size is the block size, so the last one is short */
int decode_get_samples_per_shortframe(VGMSTREAM* vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_NDS_IMA:
            return (vgmstream->interleave_last_block_size-4)*2;
        default:
            return decode_get_samples_per_frame(vgmstream);
    }
}

int decode_get_shortframe_size(VGMSTREAM* vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_NDS_IMA:
            return vgmstream->interleave_last_block_size;
        default:
            return decode_get_frame_size(vgmstream);
    }
}

/* ugly kludge due to vgmstream's finicky internals, to be improved some day:
 * - some codecs have frame sizes AND may also have interleave
 * - meaning, ch0 could read 0x100 (frame_size) N times until 0x1000 (interleave)
 *   then skip 0x1000 per other channels and keep reading 0x100
 *   (basically: ch0=0x0000..0x1000, ch1=0x1000..0x2000, ch0=0x2000..0x3000, etc)
 * - interleave layout assumes by default codecs DON'T update offsets and only interleave does
 *   - interleave calculates how many frames/samples will read before moving offsets,
 *     then once 1 channel is done skips original channel data + other channel's data
 *   - decoders need to calculate current frame offset on every frame since
 *     offsets only move when interleave moves offsets (ugly)
 * - other codecs move offsets internally instead (also ugly)
 *   - but interleave doesn't know this and will skip too much data
 * 
 * To handle the last case, return a flag here that interleave layout can use to
 * separate between both cases when the interleave data is done 
 * - codec doesn't advance offsets: will skip interleave for all channels including current
 *   - ex. 2ch, 0x100, 0x1000: after reading 0x100*10 frames offset is still 0x0000 > skips 0x1000*2 (ch0+ch1)
 * - codec does advance offsets: will skip interleave for all channels except current
 *   - ex. 2ch, 0x100, 0x1000: after reading 0x100*10 frames offset is at 0x1000 >  skips 0x1000*1 (ch1)
 * 
 * Ideally frame reading + skipping would be moved to some kind of consumer functions
 * separate from frame decoding which would simplify all this but meanwhile...
 * 
 * Instead of this flag, codecs could be converted to avoid moving offsets (like most codecs) but it's
 * getting hard to understand the root issue so have some wall of text as a reminder.
 */
bool decode_uses_internal_offset_updates(VGMSTREAM* vgmstream) {
    return vgmstream->coding_type == coding_MS_IMA || vgmstream->coding_type == coding_MS_IMA_mono;
}


// decode frames for decoders which decode frame by frame and have their own sample buffer
static void decode_frames(sbuf_t* sdst, VGMSTREAM* vgmstream, int samples_to_do) {
    const int max_empty = 1000;
    int num_empty = 0;
    decode_state_t* ds = vgmstream->decode_state;
    sbuf_t* ssrc = &ds->sbuf;

    const codec_info_t* codec_info = codec_get_info(vgmstream);
    ds->samples_left = samples_to_do; //sdst->samples; // TODO this can be slow for interleaved decoders

    // old-style decoding
    if (codec_info && codec_info->decode_buf) {
        //TODO improve: interleaved layout moves offsets while flat doesn't, can't handle properly without samples_into_block
        // (probably should make a new interleave layout that behaves like a block layout and only moves offsets on a new block, 
        //  while decoder always moves offsets)
        ds->samples_into = vgmstream->samples_into_block;

        bool ok = codec_info->decode_buf(vgmstream, sdst);
        if (!ok) goto decode_fail;

        sdst->filled += ds->samples_left;
        return;
    }

    // fill the external buf by decoding N times; may read partially that buf
    while (sdst->filled < sdst->samples) {

        // decode new frame if prev one was consumed
        if (ssrc->filled == 0) {
            bool ok = false;

            if (codec_info) {
                ok = codec_info->decode_frame(vgmstream);
            }
            else {
                goto decode_fail;
            }

            if (!ok)
                goto decode_fail;
        }

        // decoder may not fill the buffer in a few calls in some codecs, but more it's probably a bug
        if (ssrc->filled == 0) {
            num_empty++;
            if (num_empty > max_empty) {
                VGM_LOG("VGMSTREAM: deadlock?\n");
                goto decode_fail;
            }
        }
        else {
            num_empty = 0; //reset for discard loops
        }
    
        if (ds->discard) {
            // decoder may signal that samples need to be discarded (ex. encoder delay or during loops)
            int samples_discard = ds->discard;
            if (samples_discard > ssrc->filled)
                samples_discard = ssrc->filled;

            sbuf_consume(ssrc, samples_discard);
            ds->discard -= samples_discard;
            // there may be more discard in next loop
        }
        else {
            // copy + consume
            int samples_copy = sbuf_get_copy_max(sdst, ssrc);

            sbuf_copy_segments(sdst, ssrc, samples_copy);
            sbuf_consume(ssrc, samples_copy);

            ds->samples_left -= samples_copy;
        }
    }

    return;
decode_fail:
    //TODO clean ssrc?
    //* on error just put some 0 samples
    VGM_LOG("VGMSTREAM: decode fail, missing %i samples\n", sdst->samples - sdst->filled);
    sbuf_silence_rest(sdst);
}


/* Decode samples into the buffer. Assume that we have written samples_filled into the
 * buffer already, and we have samples_to_do consecutive samples ahead of us (won't call
 * more than one frame if configured above to do so).
 * Called by layouts since they handle samples written/to_do */
void decode_vgmstream(sbuf_t* sdst, VGMSTREAM* vgmstream, int samples_to_do) {
    int ch;

    //TODO: this cast isn't correct for float sbuf-decoders but shouldn't be used/matter (for buffer+ch below)
    int16_t* buffer = sdst->buf;
    buffer += sdst->filled * vgmstream->channels; // passed externally to decoders to simplify I guess
    //samples_to_do -= samples_filled; /* pre-adjusted */

    switch (vgmstream->coding_type) {
        case coding_SILENCE:
            sbuf_silence_rest(sdst);
            break;

        case coding_CRI_ADX:
        case coding_CRI_ADX_exp:
        case coding_CRI_ADX_fixed:
        case coding_CRI_ADX_enc_8:
        case coding_CRI_ADX_enc_9:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_adx(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->interleave_block_size, vgmstream->coding_type, vgmstream->codec_config);
            }
            break;
        case coding_NGC_DSP:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ngc_dsp(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_NGC_DSP_subint:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ngc_dsp_subint(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch,
                        vgmstream->interleave_block_size);
            }
            break;

        case coding_PCM16LE:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm16le(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCM16BE:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm16be(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCM16_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm16_int(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->codec_endian);
            }
            break;
        case coding_PCM8:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCM8_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8_int(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCM8_U:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8_unsigned(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCM8_U_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8_unsigned_int(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCM8_SB:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm8_sb(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_PCM4:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm4(vgmstream,&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_PCM4_U:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcm4_unsigned(vgmstream, &vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;

        case coding_ULAW:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ulaw(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_ULAW_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ulaw_int(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_ALAW:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_alaw(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_NDS_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_nds_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_DAT4_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_dat4_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_XBOX_IMA:
        case coding_XBOX_IMA_mono: {
            int is_stereo = (vgmstream->channels > 1 && vgmstream->coding_type == coding_XBOX_IMA);
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_xbox_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch, is_stereo);
            }
            break;
        }
        case coding_XBOX_IMA_mch:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_xbox_ima_mch(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_MS_IMA:
        case coding_MS_IMA_mono:
            //TODO: improve
            vgmstream->codec_config = (vgmstream->coding_type == coding_MS_IMA_mono) || vgmstream->channels == 1; /* mono mode */
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ms_ima(vgmstream,&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_RAD_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_rad_ima(vgmstream,&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_RAD_IMA_mono:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_rad_ima_mono(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_NGC_DTK:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ngc_dtk(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_G721:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_g721(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_AFC:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_afc(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_AFC_2bit:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_afc_2bit(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_VADPCM: {
            int order = vgmstream->codec_config;
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_vadpcm(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, order);
            }
            break;
        }
        case coding_PSX:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_psx(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        0, vgmstream->codec_config);
            }
            break;
        case coding_PSX_badflags:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_psx(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        1, vgmstream->codec_config);
            }
            break;
        case coding_PSX_cfg:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_psx_configurable(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->frame_size, vgmstream->codec_config);
            }
            break;
        case coding_PSX_pivotal:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_psx_pivotal(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->frame_size);
            }
            break;
        case coding_HEVAG:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_hevag(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_XA:
        case coding_XA_EA:
        case coding_XA8: {
            decode_xa(vgmstream, buffer, samples_to_do);
            break;
        }
        case coding_EA_XA:
        case coding_EA_XA_int: {
            int is_stereo = (vgmstream->channels > 1 && vgmstream->coding_type == coding_EA_XA);
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_xa(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch, is_stereo);
            }
            break;
        }
        case coding_EA_XA_V2:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_xa_v2(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_MAXIS_XA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_maxis_xa(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_EA_XAS_V0:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_xas_v0(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_EA_XAS_V1:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_xas_v1(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_CIRCUS_VQ:
            decode_circus_vq(vgmstream->codec_data, buffer, samples_to_do, vgmstream->channels);
            break;
        case coding_ICE_RANGE:
        case coding_ICE_DCT:
            decode_ice(vgmstream->codec_data, buffer, samples_to_do);
            break;
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
        case coding_MP4_AAC:
            decode_mp4_aac(vgmstream->codec_data, buffer, samples_to_do, vgmstream->channels);
            break;
#endif
        case coding_SDX2:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_sdx2(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_SDX2_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_sdx2_int(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_CBD2:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_cbd2(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_CBD2_int:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_cbd2_int(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_DERF:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_derf(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_WADY:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_wady(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_DPCM_KCEJ:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_dpcm_kcej(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_CIRCUS_ADPCM:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_circus_adpcm(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;

        case coding_IMA:
        case coding_IMA_mono:
        case coding_DVI_IMA:
        case coding_DVI_IMA_mono: {
            int is_stereo = (vgmstream->channels > 1 && vgmstream->coding_type == coding_IMA)
                    || (vgmstream->channels > 1 && vgmstream->coding_type == coding_DVI_IMA);
            int is_high_first = vgmstream->coding_type == coding_DVI_IMA
                    || vgmstream->coding_type == coding_DVI_IMA_mono;
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_standard_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch,
                        is_stereo, is_high_first);
            }
            break;
        }
        case coding_MTF_IMA: {
            int is_stereo = (vgmstream->channels > 1);
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_mtf_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch, is_stereo);
            }
            break;
        }
        case coding_CAMELOT_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_camelot_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_WV6_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_wv6_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_HV_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_hv_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_SQEX_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_sqex_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_BLITZ_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_blitz_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;

        case coding_APPLE_IMA4:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_apple_ima4(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_SNDS_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_snds_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_QD_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_otns_ima(vgmstream, &vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_FSB_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_fsb_ima(vgmstream, &vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_WWISE_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_wwise_ima(vgmstream,&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_REF_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ref_ima(vgmstream,&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_AWC_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_awc_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_UBI_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ubi_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_UBI_SCE_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ubi_sce_ima(&vgmstream->ch[ch], buffer+ch,
                    vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_H4M_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                uint16_t frame_format = (uint16_t)((vgmstream->codec_config >> 8) & 0xFFFF);

                decode_h4m_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch,
                        frame_format);
            }
            break;
        case coding_CD_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_cd_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_CRANKCASE_IMA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_crankcase_ima(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;

        case coding_WS:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ws(vgmstream, ch, buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;

#ifdef VGM_USE_G7221
        case coding_G7221C:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_g7221(vgmstream, buffer+ch, vgmstream->channels, samples_to_do, ch);
            }
            break;
#endif
#ifdef VGM_USE_G719
        case coding_G719:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_g719(vgmstream, buffer+ch, vgmstream->channels, samples_to_do, ch);
            }
            break;
#endif
        case coding_ACM:
            decode_acm(vgmstream->codec_data, buffer, samples_to_do, vgmstream->channels);
            break;
        case coding_NWA:
            decode_nwa(vgmstream->codec_data, buffer, samples_to_do);
            break;
        case coding_MSADPCM:
        case coding_MSADPCM_mono:
            if (vgmstream->channels == 1 || vgmstream->coding_type == coding_MSADPCM_mono) {
                for (ch = 0; ch < vgmstream->channels; ch++) {
                    decode_msadpcm_mono(vgmstream,buffer+ch,
                            vgmstream->channels,vgmstream->samples_into_block, samples_to_do, ch,
                            vgmstream->codec_config);
                }
            }
            else if (vgmstream->channels == 2) {
                decode_msadpcm_stereo(vgmstream, buffer, vgmstream->samples_into_block,samples_to_do);
            }
            break;
        case coding_MSADPCM_ck:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_msadpcm_ck(vgmstream, buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_AICA:
        case coding_AICA_int: {
            int is_stereo = (vgmstream->channels > 1 && vgmstream->coding_type == coding_AICA);
            int is_high_first = vgmstream->codec_config == 1;
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_aica(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch,
                        is_stereo, is_high_first);
            }
            break;
        }
        case coding_CP_YM: {
            int is_stereo = (vgmstream->channels > 1);
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_cp_ym(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch,
                        is_stereo);
            }
            break;
        }
        case coding_ASKA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_aska(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch, vgmstream->frame_size);
            }
            break;
        case coding_NXAP:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_nxap(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_TGC:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_tgc(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_NDS_PROCYON:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_nds_procyon(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_LEVEL5:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_l5_555(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_SASSC:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_sassc(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_LSF:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_lsf(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_MTAF:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_mtaf(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_MTA2:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_mta2(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch, vgmstream->codec_config);
            }
            break;
        case coding_MPC3:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_mpc3(vgmstream, &vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;
        case coding_FADPCM:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_fadpcm(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels,vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_ASF:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_asf(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_TANTALUS:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_tantalus(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_DSA:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_dsa(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do);
            }
            break;
        case coding_XMD:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_xmd(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->interleave_block_size);
            }
            break;
        case coding_PTADPCM:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ptadpcm(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->interleave_block_size);
            }
            break;
        case coding_PCFX:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_pcfx(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do,
                        vgmstream->codec_config);
            }
            break;
        case coding_OKI16:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_oki16(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;

        case coding_OKI4S:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_oki4s(&vgmstream->ch[ch], buffer+ch,
                        vgmstream->channels, vgmstream->samples_into_block, samples_to_do, ch);
            }
            break;

        case coding_UBI_ADPCM:
            decode_ubi_adpcm(vgmstream, buffer, samples_to_do);
            break;

        case coding_ONGAKUKAN_ADPCM:
            decode_ongakukan_adp(vgmstream, buffer, samples_to_do);
            break;

        case coding_EA_MT:
            for (ch = 0; ch < vgmstream->channels; ch++) {
                decode_ea_mt(vgmstream, buffer+ch, vgmstream->channels, samples_to_do, ch);
            }
            break;

        default: {
            sbuf_t stmp = *sdst;
            stmp.samples = stmp.filled + samples_to_do; //TODO improve 

            decode_frames(&stmp, vgmstream, samples_to_do);
            break;
        }
    }
}

/* Calculate number of consecutive samples we can decode. Takes into account hitting
 * a loop start or end, or going past a single frame. */
int decode_get_samples_to_do(int samples_this_block, int samples_per_frame, VGMSTREAM* vgmstream) {
    int samples_to_do;
    int samples_left_this_block;

    samples_left_this_block = samples_this_block - vgmstream->samples_into_block;
    samples_to_do = samples_left_this_block; /* by default decodes all samples left */

    /* fun loopy crap, why did I think this would be any simpler? */
    if (vgmstream->loop_flag) {
        int samples_after_decode = vgmstream->current_sample + samples_left_this_block;

        /* are we going to hit the loop end during this block? */
        if (samples_after_decode > vgmstream->loop_end_sample) {
            /* only do samples up to loop end */
            samples_to_do = vgmstream->loop_end_sample - vgmstream->current_sample;
        }

        /* are we going to hit the loop start during this block? (first time only) */
        if (samples_after_decode > vgmstream->loop_start_sample && !vgmstream->hit_loop) {
            /* only do samples up to loop start */
            samples_to_do = vgmstream->loop_start_sample - vgmstream->current_sample;
        }
    }

    /* if it's a framed encoding don't do more than one frame */
    if (samples_per_frame > 1 && (vgmstream->samples_into_block % samples_per_frame) + samples_to_do > samples_per_frame)
        samples_to_do = samples_per_frame - (vgmstream->samples_into_block % samples_per_frame);

    return samples_to_do;
}


/* Detect loop start and save values, or detect loop end and restore (loop back). Returns true if loop was done. */
bool decode_do_loop(VGMSTREAM* vgmstream) {
    //if (!vgmstream->loop_flag) return false;

    /* is this the loop end? = new loop, continue from loop_start_sample */
    if (vgmstream->current_sample == vgmstream->loop_end_sample) {

        /* disable looping if target count reached and continue normally
         * (only needed with the "play stream end after looping N times" option enabled) */
        vgmstream->loop_count++;
        if (vgmstream->loop_target && vgmstream->loop_target == vgmstream->loop_count) {
            vgmstream->loop_flag = false; /* could be improved but works ok, will be restored on resets */
            return false;
        }

        /* against everything I hold sacred, preserve adpcm history before looping for certain types */
        if (vgmstream->meta_type == meta_DSP_STD ||
            vgmstream->meta_type == meta_DSP_RS03 ||
            vgmstream->meta_type == meta_DSP_CSTR ||
            vgmstream->coding_type == coding_PSX ||
            vgmstream->coding_type == coding_PSX_badflags) {
            for (int ch = 0; ch < vgmstream->channels; ch++) {
                vgmstream->loop_ch[ch].adpcm_history1_16 = vgmstream->ch[ch].adpcm_history1_16;
                vgmstream->loop_ch[ch].adpcm_history2_16 = vgmstream->ch[ch].adpcm_history2_16;
                vgmstream->loop_ch[ch].adpcm_history1_32 = vgmstream->ch[ch].adpcm_history1_32;
                vgmstream->loop_ch[ch].adpcm_history2_32 = vgmstream->ch[ch].adpcm_history2_32;
            }
        }

        //TODO: improve
        /* codecs with codec_data that decode_loop need special handling, usually:
         * - during decode, codec uses vgmstream->ch[].offset to handle current offset
         * - on hit_loop, current offset is auto-copied to vgmstream->loop_ch[].offset
         * - decode_seek codecs may overwrite vgmstream->loop_ch[].offset with a custom value (such as start_offset)
         * - vgmstream->loop_ch[] is copied below to vgmstream->ch[] (with the newly assigned custom value)
         * - then codec will use vgmstream->ch[].offset during decode
         * regular codecs will use copied vgmstream->loop_ch[].offset without issue */
        decode_loop(vgmstream);

        /* restore! */
        memcpy(vgmstream->ch, vgmstream->loop_ch, sizeof(VGMSTREAMCHANNEL) * vgmstream->channels);
        vgmstream->current_sample = vgmstream->loop_current_sample;
        vgmstream->samples_into_block = vgmstream->loop_samples_into_block;
        vgmstream->current_block_size = vgmstream->loop_block_size;
        vgmstream->current_block_samples = vgmstream->loop_block_samples;
        vgmstream->current_block_offset = vgmstream->loop_block_offset;
        vgmstream->next_block_offset = vgmstream->loop_next_block_offset;
        vgmstream->full_block_size = vgmstream->loop_full_block_size;

        /* loop layouts (after restore, in case layout needs state manipulations) */
        switch(vgmstream->layout_type) {
            case layout_segmented:
                loop_layout_segmented(vgmstream, vgmstream->loop_current_sample);
                break;
            case layout_layered:
                loop_layout_layered(vgmstream, vgmstream->loop_current_sample);
                break;
            default:
                break;
        }

        /* play state is applied over loops and stream decoding, so it's not restored on loops */
        //vgmstream->pstate = vgmstream->lstate;

        return true; /* has looped */
    }


    /* is this the loop start? save if we haven't saved yet (right when first loop starts) */
    if (!vgmstream->hit_loop && vgmstream->current_sample == vgmstream->loop_start_sample) {
        /* save! */
        memcpy(vgmstream->loop_ch, vgmstream->ch, sizeof(VGMSTREAMCHANNEL) * vgmstream->channels);
        vgmstream->loop_current_sample = vgmstream->current_sample;
        vgmstream->loop_samples_into_block = vgmstream->samples_into_block;
        vgmstream->loop_block_size = vgmstream->current_block_size;
        vgmstream->loop_block_samples = vgmstream->current_block_samples;
        vgmstream->loop_block_offset = vgmstream->current_block_offset;
        vgmstream->loop_next_block_offset = vgmstream->next_block_offset;
        vgmstream->loop_full_block_size = vgmstream->full_block_size;

        /* play state is applied over loops and stream decoding, so it's not saved on loops */
        //vgmstream->lstate = vgmstream->pstate;

        vgmstream->hit_loop = true; /* info that loop is now ready to use */
    }

    return false; /* has not looped */
}
