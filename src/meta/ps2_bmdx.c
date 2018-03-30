#include "meta.h"
#include "../coding/coding.h"

static STREAMFILE* setup_bmdx_streamfile(STREAMFILE *streamFile, off_t start_offset, size_t data_size);


/* .bmdx - from Beatmania IIDX (PS2) games */
VGMSTREAM * init_vgmstream_ps2_bmdx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t start_offset;
    size_t data_size;
    int loop_flag, channel_count, encryption;


    /* checks */
    if (!check_extensions(streamFile, "bmdx"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x01006408 ||
        read_32bitBE(0x04,streamFile) != 0x00)
        goto fail;

    start_offset = read_32bitLE(0x08,streamFile);
    data_size = read_32bitLE(0x0c,streamFile);
    loop_flag = (read_32bitLE(0x10,streamFile) != 0x00);
    channel_count = read_32bitLE(0x1C,streamFile);
    encryption = (read_32bitLE(0x20,streamFile) == 1);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x18,streamFile);

    vgmstream->num_samples = ps_bytes_to_samples(data_size, channel_count);
    vgmstream->loop_start_sample = ps_bytes_to_samples(read_32bitLE(0x10,streamFile), channel_count);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_PS2_BMDX;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = (channel_count == 1) ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x24,streamFile);

    /* later games are encrypted [beatmaniaIIDX 14 GOLD (PS2)] */
    if (encryption) {
        temp_streamFile = setup_bmdx_streamfile(streamFile, start_offset, data_size);
        if (!temp_streamFile) goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,encryption ? temp_streamFile : streamFile,start_offset))
        goto fail;

    close_streamfile(temp_streamFile);

    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}


typedef struct {
    uint8_t xor;
    uint8_t add;
    off_t start_offset;
    size_t data_size;
} bmdx_decryption_data;

static size_t bmdx_decryption_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, bmdx_decryption_data* data) {
    size_t bytes_read;
    int i;

    bytes_read = streamfile->read(streamfile, dest, offset, length);

    /* decrypt data (xor) */
    for (i = 0; i < bytes_read; i++) {
        if (offset+i >= data->start_offset && offset+i < data->start_offset + data->data_size) {
            if (((offset+i) % 0x10) == 0) /* XOR header byte per frame */
                dest[i] = dest[i] ^ data->xor;
            else if (((offset+i) % 0x10) == 2) /* ADD first data byte per frame */
                dest[i] = (uint8_t)(dest[i] + data->add);
        }
    }

    return bytes_read;
}

static STREAMFILE* setup_bmdx_streamfile(STREAMFILE *streamFile, off_t start_offset, size_t data_size) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;
    bmdx_decryption_data io_data = {0};
    size_t io_data_size = sizeof(bmdx_decryption_data);

    /* setup decryption (usually xor=0xFF and add=0x02) */
    io_data.xor = read_8bit(start_offset,streamFile);
    io_data.add = (~(uint8_t)read_8bit(start_offset+2,streamFile)) + 0x01;
    io_data.start_offset = start_offset;
    io_data.data_size = data_size;


    /* setup custom streamfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, bmdx_decryption_read,NULL);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}
