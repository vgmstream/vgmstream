#include "layout.h"
#include "../vgmstream.h"
#include "../coding/acm_decoder.h"
#include "../coding/coding.h"

void render_vgmstream_mus_acm(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    int samples_written=0;
    mus_acm_codec_data *data = vgmstream->codec_data;

    while (samples_written<sample_count) {
        ACMStream *acm = data->files[data->current_file];
        int samples_to_do;
        int samples_this_block = acm->total_values / acm->info.channels;

#if 0
        if (vgmstream->loop_flag && vgmstream_do_loop(vgmstream)) {
            continue;
        }
#endif

        samples_to_do = vgmstream_samples_to_do(samples_this_block, 1, vgmstream);

        if (samples_written+samples_to_do > sample_count)
            samples_to_do=sample_count-samples_written;

        if (samples_to_do == 0)
        {
            data->current_file++;
            /* check for loop */
            if (vgmstream->loop_flag) {
                if (data->current_file == data->loop_end_file)
                    data->current_file = data->loop_start_file;
            } else {
                /* */
            }
            acm_reset(data->files[data->current_file]);
            continue;
        }

        decode_acm(acm,
                buffer+samples_written*vgmstream->channels,
                samples_to_do, vgmstream->channels);

        samples_written += samples_to_do;
        vgmstream->current_sample += samples_to_do;
        vgmstream->samples_into_block+=samples_to_do;
    }
}
