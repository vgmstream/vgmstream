#include "layout.h"
#include "../vgmstream.h"

/* TODO: currently only properly handles mono substreams */
/* TODO: there must be a reasonable way to respect the loop settings, as is
   the substreams are in their own little world */

#define INTERLEAVE_BUF_SIZE 512
void render_vgmstream_scd_int(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    sample interleave_buf[INTERLEAVE_BUF_SIZE];
    int32_t samples_done = 0;
    scd_int_codec_data *data = vgmstream->codec_data;

    while (samples_done < sample_count)
    {
        int32_t samples_to_do = INTERLEAVE_BUF_SIZE;
        int c;
        if (samples_to_do > sample_count - samples_done)
            samples_to_do = sample_count - samples_done;

        for (c=0; c < data->substream_count; c++)
        {
            int32_t i;

            render_vgmstream(interleave_buf,
                    samples_to_do, data->substreams[c]);

            for (i=0; i < samples_to_do; i++)
            {
                buffer[(samples_done+i)*data->substream_count + c] = interleave_buf[i];
            }
        }

        samples_done += samples_to_do;

    }
}

