#include "coding.h"
#include "../util.h"

#ifdef VGM_USE_VORBIS
#include <vorbis/vorbisfile.h>

void decode_ogg_vorbis(ogg_vorbis_codec_data * data, sample * outbuf, int32_t samples_to_do, int channels) {
    int samples_done = 0;
    OggVorbis_File *ogg_vorbis_file = &data->ogg_vorbis_file;

    do {
        long rc = ov_read(ogg_vorbis_file, (char *)(outbuf + samples_done*channels),
                (samples_to_do - samples_done)*sizeof(sample)*channels, 0,
                sizeof(sample), 1, &data->bitstream);

        if (rc > 0) samples_done += rc/sizeof(sample)/channels;
        else return;
    } while (samples_done < samples_to_do);

    swap_samples_le(outbuf, samples_to_do*channels);
}


void reset_ogg_vorbis(VGMSTREAM *vgmstream) {
    ogg_vorbis_codec_data *data = vgmstream->codec_data;
    OggVorbis_File *ogg_vorbis_file = &(data->ogg_vorbis_file);

    ov_pcm_seek(ogg_vorbis_file, 0);
}

void seek_ogg_vorbis(VGMSTREAM *vgmstream, int32_t num_sample) {
    ogg_vorbis_codec_data *data = (ogg_vorbis_codec_data *)(vgmstream->codec_data);
    OggVorbis_File *ogg_vorbis_file = &(data->ogg_vorbis_file);

    ov_pcm_seek_lap(ogg_vorbis_file, num_sample);
}

void free_ogg_vorbis(ogg_vorbis_codec_data *data) {
    if (!data) {
        OggVorbis_File *ogg_vorbis_file = &(data->ogg_vorbis_file);

        ov_clear(ogg_vorbis_file);

        close_streamfile(data->ov_streamfile.streamfile);
        free(data);
    }
}

#endif
