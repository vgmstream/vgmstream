#include "meta.h"
#include "../util.h"

/* Apple Core Audio Format */

VGMSTREAM * init_vgmstream_apple_caff(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];

    off_t start_offset = 0;
    off_t data_size = 0;
    off_t sample_count = 0;
    off_t interleave = 0;
    int sample_rate = -1, unused_frames = 0;
    int channel_count = 0;
    
    off_t file_length;
    off_t chunk_offset = 8;
    int found_desc = 0, found_pakt = 0, found_data = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("caf",filename_extension(filename))) goto fail;

    /* check "caff" id */
    if (read_32bitBE(0,streamFile)!=0x63616666) goto fail;
    /* check version, flags */
    if (read_32bitBE(4,streamFile)!=0x00010000) goto fail;

    file_length = (off_t)get_streamfile_size(streamFile);

    while (chunk_offset < file_length)
    {
        /* high half of size (expect 0s) */
        if (read_32bitBE(chunk_offset+4,streamFile) != 0) goto fail;

        /* handle chunk type */
        switch (read_32bitBE(chunk_offset,streamFile))
        {
            case 0x64657363:    /* desc */
                found_desc = 1;
                {
                    /* rather than put a convoluted conversion here for
                       portability, just look it up */
                    uint32_t sratefloat = read_32bitBE(chunk_offset+0x0c, streamFile);
                    if (read_32bitBE(chunk_offset+0x10, streamFile) != 0) goto fail;
                    switch (sratefloat)
                    {
					    case 0x40D19400:
							sample_rate = 18000;
							break;
                        case 0x40D58880:
                            sample_rate = 22050;
                            break;
                        case 0x40DF4000:
                            sample_rate = 32000;
                            break;
                        case 0x40E58880:
                            sample_rate = 44100;
                            break;
                        case 0x40E77000:
                            sample_rate = 48000;
                            break;
                        default:
                            goto fail;
                    }
                }

                {
                    uint32_t bytes_per_packet, frames_per_packet, channels_per_frame, bits_per_channel;
                    uint32_t codec_4cc = read_32bitBE(chunk_offset+0x14, streamFile);
                    /* only supporting ima4 for now */
                    if (codec_4cc != 0x696d6134) goto fail;

                    /* format flags */
                    if (read_32bitBE(chunk_offset+0x18, streamFile) != 0) goto fail;
                    bytes_per_packet = read_32bitBE(chunk_offset+0x1c, streamFile);
                    frames_per_packet = read_32bitBE(chunk_offset+0x20, streamFile);
                    channels_per_frame = read_32bitBE(chunk_offset+0x24, streamFile);
                    bits_per_channel = read_32bitBE(chunk_offset+0x28, streamFile);

                    interleave = bytes_per_packet / channels_per_frame;
                    channel_count = channels_per_frame;
                    if (channels_per_frame != 1 && channels_per_frame != 2)
                        goto fail;
                    /* ima4-specific */
                    if (frames_per_packet != 64) goto fail;
                    if ((frames_per_packet / 2 + 2) * channels_per_frame !=
                        bytes_per_packet) goto fail;
                    if (bits_per_channel != 0) goto fail;
                }
                break;
            case 0x70616b74:    /* pakt */
                found_pakt = 1;
                /* 64-bit packet table size, 0 for constant bitrate */
                if (
                    read_32bitBE(chunk_offset+0x0c,streamFile) != 0 ||
                    read_32bitBE(chunk_offset+0x10,streamFile) != 0) goto fail;
                /* high half of valid frames count */
                if (read_32bitBE(chunk_offset+0x14,streamFile) != 0) goto fail;
                /* frame count */
                sample_count = read_32bitBE(chunk_offset+0x18,streamFile);
                /* priming frames */
                if (read_32bitBE(chunk_offset+0x1c,streamFile) != 0) goto fail;
                /* remainder (unused) frames */
                unused_frames = read_32bitBE(chunk_offset+0x20,streamFile);
                break;
            case 0x66726565:    /* free */
                /* padding, ignore */
                break;
            case 0x64617461:    /* data */
                if (read_32bitBE(chunk_offset+12,streamFile) != 1) goto fail;
                found_data = 1;
                start_offset = chunk_offset + 16;
                data_size = read_32bitBE(chunk_offset+8,streamFile) - 4;
                break;
            default:
                goto fail;
        }

        /* done with chunk */
        chunk_offset += 12 + read_32bitBE(chunk_offset+8,streamFile);
    }

    if (!found_pakt || !found_desc || !found_data) goto fail;
    if (start_offset == 0 || data_size == 0 || sample_count == 0 ||
        sample_rate == -1 || channel_count == 0) goto fail;

    /* ima4-specific */
    /* check for full packets */
    if (data_size % (interleave*channel_count) != 0) goto fail;
    if ((sample_count+unused_frames)%((interleave-2)*2) != 0) goto fail;
    /* check that all packets are accounted for */
    if (data_size/interleave/channel_count !=
        (sample_count+unused_frames)/((interleave-2)*2)) goto fail;

    vgmstream = allocate_vgmstream(channel_count,0);
    if (!vgmstream) goto fail;

    vgmstream->channels = channel_count;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = sample_count;
    /* ima4-specific */
    vgmstream->coding_type = coding_APPLE_IMA4;
    if (channel_count == 2)
        vgmstream->layout_type = layout_interleave;
    else
        vgmstream->layout_type = layout_none;
    vgmstream->interleave_block_size = interleave;
    vgmstream->meta_type = meta_CAFF;

    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<channel_count;i++)
        {
            vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].offset =
                vgmstream->ch[i].channel_start_offset =
                start_offset + interleave * i;
        }
    }

    return vgmstream;
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
