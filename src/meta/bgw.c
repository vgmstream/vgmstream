#include "meta.h"
#include "../coding/coding.h"


static STREAMFILE* setup_bgw_atrac3_streamfile(STREAMFILE *streamFile, off_t subfile_offset, size_t subfile_size, size_t frame_size, int channels);


/* BGW - from Final Fantasy XI (PC) music files */
VGMSTREAM * init_vgmstream_bgw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    uint32_t codec, file_size, block_size, sample_rate, block_align;
    int32_t loop_start;
    off_t start_offset;

    int channel_count, loop_flag = 0;

    /* check extensions */
    if ( !check_extensions(streamFile, "bgw") )
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x42474d53 || /* "BGMS" */
        read_32bitBE(0x04,streamFile) != 0x74726561 || /* "trea" */
        read_32bitBE(0x08,streamFile) != 0x6d000000 )  /* "m\0\0\0" */
        goto fail;

    codec = read_32bitLE(0x0c,streamFile);
    file_size = read_32bitLE(0x10,streamFile);
    /*file_id = read_32bitLE(0x14,streamFile);*/
    block_size = read_32bitLE(0x18,streamFile);
    loop_start = read_32bitLE(0x1c,streamFile);
    sample_rate = (read_32bitLE(0x20,streamFile) + read_32bitLE(0x24,streamFile)) & 0x7FFFFFFF; /* bizarrely obfuscated sample rate */
    start_offset = read_32bitLE(0x28,streamFile);
    /*0x2c: unk (vol?) */
    /*0x2d: unk (0x10?) */
    channel_count = read_8bit(0x2e,streamFile);
    block_align = (uint8_t)read_8bit(0x2f,streamFile);

    if (file_size != get_streamfile_size(streamFile))
        goto fail;

    loop_flag = (loop_start > 0);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_FFXI_BGW;
    vgmstream->sample_rate = sample_rate;

    switch (codec) {
        case 0: /* PS ADPCM */
            vgmstream->coding_type = coding_PSX_cfg;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (block_align / 2) + 1; /* half, even if channels = 1 */

            vgmstream->num_samples = block_size * block_align;
            if (loop_flag) {
                vgmstream->loop_start_sample = (loop_start-1) * block_align;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }
            
            break;

#ifdef VGM_USE_FFMPEG
        case 3: { /* ATRAC3 (encrypted) */
            uint8_t buf[0x100];
            int bytes, joint_stereo, skip_samples;
            size_t data_size = file_size - start_offset;

            vgmstream->num_samples = block_size; /* atrac3_bytes_to_samples gives the same value */
            if (loop_flag) {
                vgmstream->loop_start_sample = loop_start;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }

            block_align  = 0xC0 * vgmstream->channels; /* 0x00 in header */
            joint_stereo = 0;
            skip_samples = 0;

            bytes = ffmpeg_make_riff_atrac3(buf, 0x100, vgmstream->num_samples, data_size, vgmstream->channels, vgmstream->sample_rate, block_align, joint_stereo, skip_samples);
            if (bytes <= 0) goto fail;

            temp_streamFile = setup_bgw_atrac3_streamfile(streamFile, start_offset,data_size, 0xC0,channel_count);
            if (!temp_streamFile) goto fail;

            vgmstream->codec_data = init_ffmpeg_header_offset(temp_streamFile, buf,bytes, 0,data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            close_streamfile(temp_streamFile);
            break;
        }
#endif

        default:
            goto fail;
    }


    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}


/* SPW (SEWave) - from  PlayOnline viewer for Final Fantasy XI (PC) */
VGMSTREAM * init_vgmstream_spw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    uint32_t codec, file_size, block_size, sample_rate, block_align;
    int32_t loop_start;
    off_t start_offset;

    int channel_count, loop_flag = 0;

    /* check extensions */
    if ( !check_extensions(streamFile, "spw") )
        goto fail;

    /* check header */
    if (read_32bitBE(0,streamFile) != 0x53655761 || /* "SeWa" */
        read_32bitBE(4,streamFile) != 0x76650000)   /* "ve\0\0" */
        goto fail;

    file_size = read_32bitLE(0x08,streamFile);
    codec = read_32bitLE(0x0c,streamFile);
    /*file_id = read_32bitLE(0x10,streamFile);*/
    block_size = read_32bitLE(0x14,streamFile);
    loop_start = read_32bitLE(0x18,streamFile);
    sample_rate = (read_32bitLE(0x1c,streamFile) + read_32bitLE(0x20,streamFile)) & 0x7FFFFFFF; /* bizarrely obfuscated sample rate */
    start_offset = read_32bitLE(0x24,streamFile);
    /*0x2c: unk (0x00?) */
    /*0x2d: unk (0x00/01?) */
    channel_count = read_8bit(0x2a,streamFile);
    block_align = read_8bit(0x2b,streamFile);
    /*0x2c: unk (0x01 when PCM, 0x10 when VAG?) */

    if (file_size != get_streamfile_size(streamFile))
        goto fail;

    loop_flag = (loop_start > 0);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_FFXI_SPW;
    vgmstream->sample_rate = sample_rate;

    switch (codec) {
        case 0: /* PS ADPCM */
            vgmstream->coding_type = coding_PSX_cfg;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = (block_align / 2) + 1; /* half, even if channels = 1 */
            
            vgmstream->num_samples = block_size * block_align;
            if (loop_flag) {
                vgmstream->loop_start_sample = (loop_start-1) * block_align;;
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }
            
            break;

        case 1: /* PCM */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            
            vgmstream->num_samples = block_size;
            if (loop_flag) {
                vgmstream->loop_start_sample = (loop_start-1);
                vgmstream->loop_end_sample = vgmstream->num_samples;
            }

            break;

        default:
            goto fail;
    }


    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


#define BGW_KEY_MAX (0xC0*2)

typedef struct {
    uint8_t key[BGW_KEY_MAX];
    size_t key_size;
} bgw_decryption_data;

/* Encrypted ATRAC3 info from Moogle Toolbox (https://sourceforge.net/projects/mogbox/) */
static size_t bgw_decryption_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, bgw_decryption_data* data) {
    size_t bytes_read;
    int i;

    bytes_read = streamfile->read(streamfile, dest, offset, length);

    /* decrypt data (xor) */
    for (i = 0; i < bytes_read; i++) {
        dest[i] ^= data->key[(offset + i) % data->key_size];
    }

    return bytes_read;
}

static STREAMFILE* setup_bgw_atrac3_streamfile(STREAMFILE *streamFile, off_t subfile_offset, size_t subfile_size, size_t frame_size, int channels) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    bgw_decryption_data io_data = {0};
    size_t io_data_size = sizeof(bgw_decryption_data);
    int ch;

    /* setup decryption with key (first frame + modified channel header) */
    if (frame_size*channels == 0 || frame_size*channels > BGW_KEY_MAX) goto fail;

    io_data.key_size = read_streamfile(io_data.key, subfile_offset, frame_size*channels, streamFile);
    for (ch = 0; ch < channels; ch++) {
        uint32_t xor = get_32bitBE(io_data.key + frame_size*ch);
        put_32bitBE(io_data.key + frame_size*ch, xor ^ 0xA0024E9F);
    }

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_clamp_streamfile(temp_streamFile, subfile_offset,subfile_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, bgw_decryption_read,NULL);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}
