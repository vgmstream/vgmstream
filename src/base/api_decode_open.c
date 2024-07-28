#include "api_internal.h"
#if LIBVGMSTREAM_ENABLE

#define INTERNAL_BUF_SAMPLES  1024


static void load_vgmstream(libvgmstream_priv_t* priv, libvgmstream_options_t* opt) {
    STREAMFILE* sf_api = open_api_streamfile(opt->libsf);
    if (!sf_api)
        return;

    //TODO: handle internal format_id
    
    sf_api->stream_index = opt->subsong_index;
    priv->vgmstream = init_vgmstream_from_STREAMFILE(sf_api);
    close_streamfile(sf_api);
}

static void apply_config(libvgmstream_priv_t* priv) {
    libvgmstream_config_t* cfg = &priv->cfg;

    vgmstream_cfg_t vcfg = {
        .disable_config_override = cfg->disable_config_override,
        .allow_play_forever = cfg->allow_play_forever,

        .play_forever = cfg->play_forever,
        .ignore_loop  = cfg->ignore_loop,
        .force_loop = cfg->force_loop,
        .really_force_loop = cfg->really_force_loop,
        .ignore_fade = cfg->ignore_fade,

        .loop_count = cfg->loop_count,
        .fade_time = cfg->fade_time,
        .fade_delay = cfg->fade_delay,
    };

    if (!vcfg.allow_play_forever)
        vcfg.play_forever = 0;

    vgmstream_apply_config(priv->vgmstream, &vcfg);
}

static void prepare_mixing(libvgmstream_priv_t* priv, libvgmstream_options_t* opt) {
    /* enable after config but before outbuf */
    if (priv->cfg.auto_downmix_channels) {
        vgmstream_mixing_autodownmix(priv->vgmstream, priv->cfg.auto_downmix_channels);
    }
    else if (opt && opt->stereo_track >= 1) {
        vgmstream_mixing_stereo_only(priv->vgmstream, opt->stereo_track - 1);
    }

    vgmstream_mixing_enable(priv->vgmstream, INTERNAL_BUF_SAMPLES, NULL /*&input_channels*/, NULL /*&output_channels*/);
}

static void update_position(libvgmstream_priv_t* priv) {
    libvgmstream_priv_position_t* pos = &priv->pos;
    VGMSTREAM* v = priv->vgmstream;

    pos->play_forever = vgmstream_get_play_forever(v);
    pos->play_samples = vgmstream_get_samples(v);
    pos->current = 0;
}

static void update_format_info(libvgmstream_priv_t* priv) {
    libvgmstream_format_t* fmt = &priv->fmt;
    VGMSTREAM* v = priv->vgmstream;

    fmt->sample_size = 0x02;
    fmt->sample_type = LIBVGMSTREAM_SAMPLE_PCM16; 
    fmt->sample_rate = v->sample_rate;

    fmt->subsong_index = v->stream_index;
    fmt->subsong_count = v->num_streams;

    fmt->channels = v->channels;
    fmt->input_channels = 0;
    vgmstream_mixing_enable(v, 0, &fmt->input_channels, &fmt->channels);
    fmt->channel_layout = v->channel_layout;

    fmt->stream_samples = v->num_samples;
    fmt->loop_start = v->loop_start_sample;
    fmt->loop_end = v->loop_end_sample;
    fmt->loop_flag = v->loop_flag;

    fmt->play_forever = priv->pos.play_forever;
    fmt->play_samples = priv->pos.play_samples;

    fmt->format_id = v->format_id;

    fmt->stream_bitrate = get_vgmstream_average_bitrate(v);

    get_vgmstream_coding_description(v, fmt->codec_name, sizeof(fmt->codec_name));
    get_vgmstream_layout_description(v, fmt->layout_name, sizeof(fmt->layout_name));
    get_vgmstream_meta_description(v, fmt->meta_name, sizeof(fmt->meta_name));

    if (v->stream_name[0] != '\0') { //snprintf UB for NULL args
        snprintf(fmt->stream_name, sizeof(fmt->stream_name), "%s", v->stream_name);
    }
}

LIBVGMSTREAM_API int libvgmstream_open_song(libvgmstream_t* lib, libvgmstream_options_t* opt) {
    if (!lib ||!lib->priv)
        return LIBVGMSTREAM_ERROR_GENERIC;
    if (!opt || !opt->libsf || opt->subsong_index < 0)
        return LIBVGMSTREAM_ERROR_GENERIC;

    // close loaded song if any + reset
    libvgmstream_close_song(lib);

    libvgmstream_priv_t* priv = lib->priv;

    load_vgmstream(priv, opt);
    if (!priv->vgmstream)
        return LIBVGMSTREAM_ERROR_GENERIC;
    apply_config(priv);
    prepare_mixing(priv, opt);
    update_position(priv);

    update_format_info(priv);


    return LIBVGMSTREAM_OK;
}


LIBVGMSTREAM_API void libvgmstream_close_song(libvgmstream_t* lib) {
    if (!lib || !lib->priv)
        return;

    libvgmstream_priv_t* priv = lib->priv;

    close_vgmstream(priv->vgmstream);
    priv->vgmstream = NULL;

    libvgmstream_priv_reset(priv, true);
}

#endif
