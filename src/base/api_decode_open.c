#include "api_internal.h"
#include "sbuf.h"
#include "mixing.h"
#include "info.h"


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

    // Traditionally in CLI loop_count = 0 removes loops but this is pretty odd for a lib
    // (calling _setup with nothing set would remove most audio).
    // For now loop_count 0 is set to 1, and loop_count <0 is assumed to be same 0
    if (vcfg.loop_count == 0) {
        vcfg.loop_count = 1;
    } else if (vcfg.loop_count < 0)
        vcfg.loop_count = 0;

    vgmstream_apply_config(priv->vgmstream, &vcfg);
}

static void prepare_mixing(libvgmstream_priv_t* priv) {
    libvgmstream_config_t* cfg = &priv->cfg;

    /* enable after config but before outbuf */
    if (cfg->auto_downmix_channels) {
        vgmstream_mixing_autodownmix(priv->vgmstream, cfg->auto_downmix_channels);
    }
    else if (cfg->stereo_track >= 1) {
        vgmstream_mixing_stereo_only(priv->vgmstream, cfg->stereo_track - 1);
    }

    if (cfg->force_sfmt) {
        // external force
        sfmt_t force_sfmt = SFMT_NONE;
        switch(cfg->force_sfmt) {
            case LIBVGMSTREAM_SFMT_PCM16: force_sfmt = SFMT_S16; break;
            case LIBVGMSTREAM_SFMT_FLOAT: force_sfmt = SFMT_FLT; break;
            case LIBVGMSTREAM_SFMT_PCM24: force_sfmt = SFMT_O24; break;
            case LIBVGMSTREAM_SFMT_PCM32: force_sfmt = SFMT_S32; break;
            default: break;
        }

        mixing_macro_output_sample_format(priv->vgmstream, force_sfmt);
    }
    else {
        // internal force, swap certain internal bufs into standard output
        sfmt_t force_sfmt = SFMT_NONE;
        sfmt_t input_sfmt = mixing_get_input_sample_type(priv->vgmstream);
        switch(input_sfmt) {
            case SFMT_F16: force_sfmt = SFMT_FLT; break;
            case SFMT_S24: force_sfmt = SFMT_O24; break;
            default: break;
        }

        mixing_macro_output_sample_format(priv->vgmstream, force_sfmt);
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

    fmt->subsong_index = v->stream_index;
    fmt->subsong_count = v->num_streams;

    fmt->channels = v->channels;
    fmt->input_channels = 0;
    vgmstream_mixing_enable(v, 0, &fmt->input_channels, &fmt->channels); //query
    fmt->channel_layout = v->channel_layout;

    fmt->sample_format = api_get_output_sample_type(priv);
    fmt->sample_size = api_get_sample_size(fmt->sample_format);

    fmt->sample_rate = v->sample_rate;

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

// apply config if data + config is loaded and not already loaded
void api_apply_config(libvgmstream_priv_t* priv) {
    if (priv->setup_done)
        return;
    if (!priv->vgmstream)
        return;

    apply_config(priv);
    prepare_mixing(priv);

    update_position(priv);
    update_format_info(priv);

    priv->setup_done = true;
}

static void load_vgmstream(libvgmstream_priv_t* priv, libstreamfile_t* libsf, int subsong_index) {
    STREAMFILE* sf_api = open_api_streamfile(libsf);
    if (!sf_api)
        return;

    //TODO: handle format_id

    sf_api->stream_index = subsong_index;
    priv->vgmstream = init_vgmstream_from_STREAMFILE(sf_api);
    close_streamfile(sf_api);
}

LIBVGMSTREAM_API int libvgmstream_open_stream(libvgmstream_t* lib, libstreamfile_t* libsf, int subsong_index) {
    if (!lib ||!lib->priv || !libsf)
        return LIBVGMSTREAM_ERROR_GENERIC;

    // close loaded song if any + reset
    libvgmstream_close_stream(lib);

    libvgmstream_priv_t* priv = lib->priv;
    if (subsong_index < 0)
        return LIBVGMSTREAM_ERROR_GENERIC;

    load_vgmstream(priv, libsf, subsong_index);
    if (!priv->vgmstream)
        return LIBVGMSTREAM_ERROR_GENERIC;

    // apply now if possible to update format info
    if (priv->config_loaded) {
        api_apply_config(priv);
    }
    else {
        // no config: just update info (apply_config will be called later)
        update_position(priv);
        update_format_info(priv);
    }

    return LIBVGMSTREAM_OK;
}


LIBVGMSTREAM_API void libvgmstream_close_stream(libvgmstream_t* lib) {
    if (!lib || !lib->priv)
        return;

    libvgmstream_priv_t* priv = lib->priv;

    close_vgmstream(priv->vgmstream);
    priv->vgmstream = NULL;
    priv->setup_done = false;
    //priv->config_loaded = false; // loaded config still applies (_close is also called on _open)

    libvgmstream_priv_reset(priv, true);
}
