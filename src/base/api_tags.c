#include "api_internal.h"

typedef struct {
    VGMSTREAM_TAGS* vtags;
    libstreamfile_t* libsf;
    STREAMFILE* sf_tags;
} libvgmstream_tags_priv_t;

LIBVGMSTREAM_API libvgmstream_tags_t* libvgmstream_tags_init(libstreamfile_t* libsf) {
    if (!libsf)
        return NULL;

    libvgmstream_tags_t* tags = NULL;
    libvgmstream_tags_priv_t* priv = NULL;

    tags = calloc(1, sizeof(libvgmstream_tags_t));
    if (!tags) goto fail;

    tags->priv = calloc(1, sizeof(libvgmstream_tags_priv_t));
    if (!tags->priv) goto fail;

    priv = tags->priv;
    priv->vtags = vgmstream_tags_init(&tags->key, &tags->val);
    if (!priv->vtags) goto fail;

    priv->sf_tags = open_api_streamfile(libsf);
    if (!priv->sf_tags) goto fail;

    return tags;
fail:
    libvgmstream_tags_free(tags);
    return NULL;
}


LIBVGMSTREAM_API void libvgmstream_tags_find(libvgmstream_tags_t* tags, const char* target_filename) {
    if (!tags || !tags->priv || !target_filename)
        return;
    //TODO: handle NULL filename?
    libvgmstream_tags_priv_t* priv = tags->priv;
    vgmstream_tags_reset(priv->vtags, target_filename);
}


LIBVGMSTREAM_API bool libvgmstream_tags_next_tag(libvgmstream_tags_t* tags) {
    if (!tags)
        return false;

    libvgmstream_tags_priv_t* priv = tags->priv;

    return vgmstream_tags_next_tag(priv->vtags, priv->sf_tags);
}


LIBVGMSTREAM_API void libvgmstream_tags_free(libvgmstream_tags_t* tags) {
    if (!tags)
        return;

    libvgmstream_tags_priv_t* priv = tags->priv;
    if (priv) {
        vgmstream_tags_close(priv->vtags);
        close_streamfile(priv->sf_tags);
    }
    free(tags->priv);
    free(tags);
}
