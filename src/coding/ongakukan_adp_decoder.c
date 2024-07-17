#include <stdlib.h>
#include "coding.h"
#include "coding_utils_samples.h"
#include "libs/ongakukan_adp_lib.h"

struct ongakukan_adp_data
{
	void* handle;
	//int32_t num_samples;
	int16_t* samples;
	int32_t samples_done;
	int32_t samples_filled;
	int32_t samples_consumed;
	int32_t getting_samples;
	STREAMFILE* sf;
};

ongakukan_adp_data* init_ongakukan_adp(STREAMFILE* sf, int32_t data_offset, int32_t data_size,
	char sample_needs_setup, char sample_has_base_setup_from_the_start)
{
	ongakukan_adp_data* data = NULL;

	data = calloc(1, sizeof(ongakukan_adp_data));
	if (!data) goto fail;

	data->sf = reopen_streamfile(sf, 0);
	data->handle = boot_ongakukan_adpcm(data->sf, (long int)(data_offset), (long int)(data_size),
		sample_needs_setup, sample_has_base_setup_from_the_start);
	if (!data->handle) goto fail;

	return data;
fail:
	free_ongakukan_adp(data);
	return NULL;
}

void decode_ongakukan_adp(VGMSTREAM* vgmstream, sample_t* outbuf, int32_t samples_to_do)
{
	ongakukan_adp_data* data = vgmstream->codec_data;

	data->samples_filled = (int32_t)grab_samples_filled_from_ongakukan_adp(data->handle);
	data->samples_consumed = (int32_t)grab_samples_consumed_from_ongakukan_adp(data->handle);
	data->samples = (int16_t*)grab_sample_hist_from_ongakukan_adp(data->handle);
	while (data->samples_done < samples_to_do)
	{
		if (data->samples_filled)
		{
			data->getting_samples = data->samples_filled;
			if (data->getting_samples > samples_to_do - data->samples_done)
				data->getting_samples = samples_to_do - data->samples_done;

			memcpy(outbuf + data->samples_done,
				(void**)(data->samples) + data->samples_consumed,
				data->getting_samples * sizeof(int16_t));
			data->samples_done += data->getting_samples;

			/* mark consumed samples. */
			data->samples_consumed += data->getting_samples;
			data->samples_filled -= data->getting_samples;

			/* and keep the lib updated while doing so. */
			send_samples_consumed_to_ongakukan_adp((long int)(data->samples_consumed), data->handle);
			send_samples_filled_to_ongakukan_adp((long int)(data->samples_filled), data->handle);
		}
		else { decode_ongakukan_adp_data(data->handle);
		data->samples_filled = (int32_t)grab_samples_filled_from_ongakukan_adp(data->handle);
		data->samples_consumed = (int32_t)grab_samples_consumed_from_ongakukan_adp(data->handle);
		data->samples = (int16_t*)grab_sample_hist_from_ongakukan_adp(data->handle); }
	}
}

void reset_ongakukan_adp(ongakukan_adp_data* data)
{
	if (!data) return;
	reset_all_ongakukan_adpcm(data->handle);
}

void seek_ongakukan_adp(ongakukan_adp_data* data, int32_t current_sample)
{
	if (!data) return;
	seek_ongakukan_adpcm_pos(data->handle, current_sample);
}

void free_ongakukan_adp(ongakukan_adp_data* data)
{
	if (!data) return;
	close_streamfile(data->sf);
	free_all_ongakukan_adpcm(data->handle);
	free(data->samples);
	free(data);
}

int32_t ongakukan_adp_get_samples(ongakukan_adp_data* data)
{
	if (!data) return 0;
	return (int32_t)(grab_num_samples_from_ongakukan_adp(data->handle));
}
