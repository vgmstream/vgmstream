#include "coding.h"
#include "../base/decode_state.h"
#include "../base/codec_info.h"
#include "libs/mio_xerisa.h"
#include "../util/io_callback_sf.h"

/* Decodes MIO ("Music Interleaved and Orthogonal transformed") audio files.
 * Adapted to C from original C++ lib source by Leshade Entis: 
 * - http://www.entis.jp/eridev/download/
 * - http://www.amatsukami.jp/entis/bin/erinalib.lzh
 * (licensed under a custom license somewhat equivalent to GPL).
 *
 * Full lib is called "ERINA-Library" (2000~2022) / "ERISA-Library" (2004~2005), and handles various
 * Entis's formats, while this code just handles "MIO" (audio) decoding. "ERISA" uses a new coding
 * type but also handles older files.
 *
 * MIO has a RIFF-like chunk header, and then is divided into big-ish VBR blocks. "Lead" blocks
 * (first one, but also others that act as "keyframes") setup code model, which can be huffman (ERINA)
 * or arithmetic (ERISA) coding depending on header. Lib reads per-block config then codes, then
 * dequantizes per sub-band with iLOT (lapped orthogonal transform) and iDCT after some
 * pre/post-processing. There is a lossless mode with huffman codes + PCM16/8 as well.
 *
 * Original C++ lib audio parts has roughly 4 modules, adapted and simplified to C like this:
 * - erisafile (MIOFile): file ops like parsing header and reading blocks
 * - erisacontext (MIOContext): bitreading from blocks and code unpacking (huffman/arithmetical decoding)
 * - erisasound (MIODecoder): decodes audio context data into samples
 * - erisamatrix (EMT_eri*): iDTC/iLOT/etc helper functions
 * 
 * (this conversion removes encoder/non-MIO parts, hides non-public methods, unifies dupes, tweaks exceptions, improves errors, etc)
 */


/* opaque struct */
typedef struct {
    MIOFile* miofile;
    MIODecoder* miodec;
    MIOContext* mioctx;

    /* current buf */
    void* sbuf;
    int samples;
    int channels;
    int fmtsize;

    io_callback_t cb;
    io_priv_t io_priv;

    int start_offset;
} mio_codec_data;


static void free_mio(void* priv_data) {
    mio_codec_data* data = priv_data;
    if (!data) return;

    MIOFile_Close(data->miofile);
    MIODecoder_Close(data->miodec);
    MIOContext_Close(data->mioctx);
    ERI_eriCloseLibrary();
    free(data);
}

void* init_mio(STREAMFILE* sf, int* p_loop_start) {
    ERI_eriInitializeLibrary();

    mio_codec_data* data = calloc(1, sizeof(mio_codec_data));
    if (!data) return NULL;

    io_callbacks_set_sf(&data->cb, &data->io_priv);
    data->io_priv.offset = 0;
    data->io_priv.sf = sf; //temp, will be updated later

    data->miofile = MIOFile_Open();
    if (!data->miofile) goto fail;

    if (MIOFile_Initialize(data->miofile, &data->cb))
        goto fail;

    data->miodec = MIODecoder_Open();
    if (!data->miodec) goto fail;

    if (MIODecoder_Initialize(data->miodec, &data->miofile->mioih))
        goto fail;

    data->mioctx = MIOContext_Open();
    if (!data->mioctx) goto fail;

    data->start_offset = data->io_priv.offset;

    //TODO: lowest quality files allow 8-bit PCM
    if (data->miofile->mioih.dwBitsPerSample == 8) {
        goto fail;
    }

    //TODO: improve get info from libs
    if (p_loop_start) {
        *p_loop_start = data->miofile->mioih.rewindPoint;
    }

    return data;
fail:
    free_mio(data);
    return NULL;
}

static bool read_frame(mio_codec_data* data, STREAMFILE* sf) {

    data->io_priv.sf = sf;
    //data->io_priv.offset = ...; // persists between calls

    // read new packet into buffer
    int err = MIOFile_NextPacket(data->miofile, &data->cb);
    if (err == eslErrEof)
        return false;
    if (err) {
        VGM_LOG("MIO: decode packet error\n");
        return false;
    }

    // current block is keyframe and can be used to seek/decode start
    // number depends on coding mode but usually 1 keyframe per 3-7 non-keyframe blocks (~32768 samples per block)
    //if (data->miofile->miodh.bytFlags & MIO_LEAD_BLOCK) {
    //    ...
    //}

    return true;
}

static bool decode_frame(mio_codec_data* data, sbuf_t* sbuf) {

    // calculate current buffer size
    void* ptrWaveBuf = MIOFile_GetCurrentWaveBuffer(data->miofile);
    if (!ptrWaveBuf) {
        VGM_LOG("MIO: buffer error\n");
        return false;
    }

    // pass buffer to context (bitreader)
    MIOContext_AttachInputFile(data->mioctx, data->miofile->packet, data->miofile->packet_size);

    // convert data into samples
    if (MIODecoder_DecodeSound(data->miodec, data->mioctx, &data->miofile->miodh, ptrWaveBuf)) {
        VGM_LOG("MIO: decode error\n");
        return false;
    }

    //int bufferCount = MIOFile_GetCurrentWaveBufferCount(hnd->miofile);
    int samples = data->miofile->miodh.dwSampleCount;
    int channels = data->miofile->mioih.dwChannelCount;
    sfmt_t format = SFMT_S16;
    //int fmtsize = data->miofile->mioih.dwBitsPerSample / 8;
    //if (fmtsize == 8)
    //    format = SFMT_S8;

    sbuf_init(sbuf, format, ptrWaveBuf, samples, channels);
    sbuf->filled = samples;

    return true;
}


static bool decode_frame_mio(VGMSTREAM* v) {
    VGMSTREAMCHANNEL* vs = &v->ch[0];
    mio_codec_data* data = v->codec_data;
    decode_state_t* ds = v->decode_state;

    bool ok = read_frame(data, vs->streamfile);
    if (!ok)
        return false;

    ok = decode_frame(data, &ds->sbuf);
    if (!ok)
        return false;

    return true;
}

static void reset_mio(void* priv_data) {
    mio_codec_data* data = priv_data;
    if (!data) return;

    data->io_priv.offset = data->start_offset;

    MIOContext_FlushBuffer(data->mioctx);
    MIOFile_Reset(data->miofile, &data->cb);
}

static void seek_mio(VGMSTREAM* v, int32_t num_sample) {
    mio_codec_data* data = v->codec_data;
    decode_state_t* ds = v->decode_state;
    if (!data) return;

    reset_mio(data);

    // (due to implicit encode delay the above is byte-exact equivalent vs a discard loop)
    ds->discard = num_sample;
}

const codec_info_t mio_decoder = {
    .sample_type = SFMT_S16,
    .decode_frame = decode_frame_mio,
    .free = free_mio,
    .reset = reset_mio,
    .seek = seek_mio,
};
