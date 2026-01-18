#define MINIMP3_IMPLEMENTATION
#include "minimp3_vgmstream.h"


static int init_frame(mp3dec_t *dec, const uint8_t *mp3, int mp3_bytes, mp3dec_frame_info_t *info, bs_t *bs_frame, const uint8_t **p_hdr) {
    const uint8_t *hdr;
    int i = 0;
    
    int frame_size = 0;
    if (mp3_bytes > 4 && dec->header[0] == 0xff && hdr_compare(dec->header, mp3))
    {
        frame_size = hdr_frame_bytes(mp3, dec->free_format_bytes) + hdr_padding(mp3);
        if (frame_size != mp3_bytes && (frame_size + HDR_SIZE > mp3_bytes || !hdr_compare(mp3, mp3 + frame_size)))
        {
            frame_size = 0;
        }
    }
    if (!frame_size)
    {
        memset(dec, 0, sizeof(mp3dec_t));
        i = mp3d_find_frame(mp3, mp3_bytes, &dec->free_format_bytes, &frame_size);
        if (!frame_size || i + frame_size > mp3_bytes)
        {
            info->frame_bytes = i;
            return 0;
        }
    }

    hdr = mp3 + i;
    memcpy(dec->header, hdr, HDR_SIZE);
    info->frame_bytes = i + frame_size;
    info->frame_offset = i;
    info->channels = HDR_IS_MONO(hdr) ? 1 : 2;
    info->hz = hdr_sample_rate_hz(hdr);
    info->layer = 4 - HDR_GET_LAYER(hdr);
    info->bitrate_kbps = hdr_bitrate_kbps(hdr);

    bs_init(bs_frame, hdr + HDR_SIZE, frame_size - HDR_SIZE);
    if (HDR_IS_CRC(hdr))
    {
        get_bits(bs_frame, 16);
    }

    *p_hdr = hdr;
    return 1;
}


int mp3dec_decode_frame_ubimpeg(
        mp3dec_t *dec_main, const uint8_t *mp3_main, int mp3_main_bytes, mp3dec_frame_info_t *info_main,
        mp3dec_t *dec_surr, const uint8_t *mp3_surr, int mp3_surr_bytes, mp3dec_frame_info_t *info_surr,
        int surr_mode,
        mp3d_sample_t *pcm)
{
    if (!pcm) {
        return 0;
    }

    const uint8_t *hdr_main;
    const uint8_t *hdr_surr;
    bs_t bs_frame_main[1];
    bs_t bs_frame_surr[1];

    if (!init_frame(dec_main, mp3_main, mp3_main_bytes, info_main, bs_frame_main, &hdr_main))
        return 0;
    if (info_main->layer != 2)
        return 0;

    if (surr_mode == UBIMPEG_SURR_FULL) {
        if (!init_frame(dec_surr, mp3_surr, mp3_surr_bytes, info_surr, bs_frame_surr, &hdr_surr))
            return 0;
        if (info_surr->layer != 2 || info_surr->channels != 1)
            return 0;
    }

    // originally read Layer III too

    mp3dec_scratch_t scratch_main;
    mp3dec_scratch_t scratch_surr;
    {
        L12_scale_info sci_main[1];
        L12_scale_info sci_surr[1];

        L12_read_scale_info(hdr_main, bs_frame_main, sci_main);
        memset(scratch_main.grbuf[0], 0, 576*2*sizeof(float));

        if (surr_mode == UBIMPEG_SURR_FULL) {
            L12_read_scale_info(hdr_surr, bs_frame_surr, sci_surr);
            memset(scratch_surr.grbuf[0], 0, 576*2*sizeof(float));
        }

        for (int igr = 0; igr < 3; igr++) {
            // in Layer II this returns always 12 per call and 384 samples (half in layer1)
            L12_dequantize_granule(scratch_main.grbuf[0], bs_frame_main, sci_main, info_main->layer | 1);
            L12_apply_scf_384(sci_main, sci_main->scf + igr, scratch_main.grbuf[0]);

            // ubi-mpeg extra: apply surround coefs before synth
            if (surr_mode == UBIMPEG_SURR_FAKE) {
                for (int i = 0; i < 384; i++) {
                    scratch_main.grbuf[1][i] -= scratch_main.grbuf[0][i]; // R - L
                }
                info_main->channels = 2; // probably mono
            }

            if (surr_mode == UBIMPEG_SURR_FULL) {
                L12_dequantize_granule(scratch_surr.grbuf[0], bs_frame_surr, sci_surr, info_surr->layer | 1);
                L12_apply_scf_384(sci_surr, sci_surr->scf + igr, scratch_surr.grbuf[0]);

                for (int i = 0; i < 384; i++) {
                    scratch_main.grbuf[0][i] += scratch_surr.grbuf[0][i]; // L + surr
                    if (info_main->channels > 1)
                        scratch_main.grbuf[1][i] -= scratch_surr.grbuf[0][i]; // R - surr
                }
            }

            mp3d_synth_granule(dec_main->qmf_state, scratch_main.grbuf[0], 12, info_main->channels, pcm, scratch_main.syn[0]);
            memset(scratch_main.grbuf[0], 0, 576*2*sizeof(float));
            pcm += 384 * info_main->channels;

            if (bs_frame_main->pos > bs_frame_main->limit) {
                mp3dec_init(dec_main);
                return 0;
            }
        }
    }

    return hdr_frame_samples(dec_main->header);
}
