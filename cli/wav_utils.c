#include "wav_utils.h"
#include "../src/util/reader_put.h"

static int make_riff_chunk(uint8_t* buf, wav_header_t* wav, uint32_t header_size, uint32_t data_size) {

    put_data (buf+0x00, "RIFF", 0x04);
    put_u32le(buf+0x04, header_size - 0x08 + data_size);

    put_data (buf+0x08, "WAVE", 0x04);

    return 0x08 + 0x04;
}

static int make_fmt_chunk(uint8_t* buf, wav_header_t* wav) {
    //TO-DO: improve chunk generation
    // Per spec, any non-PCM (0x0001) codec must add 'fact' and use WAVEFORMATEX (cbSize = 0 if needed).
    // In practice all tools will handle IEEE_FLOAT regardless (ex. Audacity just creates WAVEFORMAT for FLOAT anyway).

    int codec = wav->is_float ? 0x0003 : 0x0001; // WAVE_FORMAT_IEEE_FLOAT or WAVE_FORMAT_PCM
    int bytes_per_second = wav->sample_rate * wav->channels * wav->sample_size;
    int16_t block_align = wav->channels * wav->sample_size;
    int bits_per_sample = wav->sample_size * 8;

    // make WAVEFORMAT chunk (see mmreg.h)
    put_data (buf + 0x00, "fmt ", 0x04);
    put_u32le(buf + 0x04, 0x10);

    put_s16le(buf + 0x08, codec);
    put_s16le(buf + 0x0a, wav->channels);
    put_s32le(buf + 0x0c, wav->sample_rate);
    put_s32le(buf + 0x10, bytes_per_second);
    put_s16le(buf + 0x14, block_align);
    put_s16le(buf + 0x16, bits_per_sample);

    return 0x08 + 0x10;
}

/* see riff.c */
static int make_smpl_chunk(uint8_t* buf, wav_header_t* wav) {

    put_data (buf + 0x00, "smpl", 0x04);
    put_s32le(buf + 0x04, 0x3c);

    for (int i = 0; i < 7; i++) {
        put_s32le(buf + 0x08 + i * 0x04, 0);
    }

    put_s32le(buf + 0x24, 1);

    for (int i = 0; i < 3; i++) {
        put_s32le(buf + 0x28 + i * 0x04, 0);
    }

    put_s32le(buf + 0x34, wav->loop_start);
    put_s32le(buf + 0x38, wav->loop_end);
    put_s32le(buf + 0x3C, 0);
    put_s32le(buf + 0x40, 0);

    return 0x08 + 0x3c;
}

static int make_data_chunk(uint8_t* buf, wav_header_t* wav, uint32_t data_size) {
    
    put_data (buf + 0x00, "data", 0x04);
    put_u32le(buf + 0x04, data_size);

    return 0x08;
}

/* make a RIFF header for .wav */
size_t wav_make_header(uint8_t* buf, size_t buf_size, wav_header_t* wav) {
    size_t header_size;

    /* RIFF + fmt + smpl + data */    
    header_size  = 0x08 + 0x04;
    header_size += 0x08 + 0x10;
    if (wav->write_smpl_chunk && wav->loop_end)
        header_size += 0x08 + 0x3c;
    header_size += 0x08;

    if (header_size > buf_size)
        return 0;

    if (!wav->sample_size)
        wav->sample_size = sizeof(short);
    if (wav->sample_size <= 0x00 || wav->sample_size > 0x04)
        return 0;

    size_t data_size = wav->sample_count * wav->channels * wav->sample_size;
    int size;

    size = make_riff_chunk(buf, wav, header_size, data_size);
    buf += size;

    size = make_fmt_chunk(buf, wav);
    buf += size;

    if (wav->write_smpl_chunk && wav->loop_end) {
        size = make_smpl_chunk(buf, wav);
        buf += size;
    }

    size = make_data_chunk(buf, wav, data_size);
    buf += size;

    /* could try to add channel_layout but would need WAVEFORMATEXTENSIBLE */

    return header_size;
}

static inline void swap_value(uint8_t* buf, int sample_size) {
    for (int i = 0; i < sample_size / 2; i++) {
        char temp = buf[i];
        buf[i] = buf[sample_size - i - 1];
        buf[sample_size - i - 1] = temp;
    }
}

/* when endianness is LE buffer is correct already and this function can be skipped */
void wav_swap_samples_le(void* samples, int samples_len, int sample_size) {
    /* Windows can't be BE... I think */
#if !defined(_WIN32)
#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    if (!sample_size)
        sample_size = sizeof(short);

    /* 16b sample in memory is AABB where AA=MSB, BB=LSB, swap to BBAA */

    uint8_t* buf = samples;
    for (int i = 0; i < samples_len; i += sample_size) {
        swap_value(buf + i, sample_size);
    }
#endif
#endif
}
