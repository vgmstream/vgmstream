#ifndef _AT3PLUS_DECODER_H
#define _AT3PLUS_DECODER_H

struct maiatrac3plus_codec_data {
    sample_t* buffer;
    int channels;
    int samples_discard;
    void* handle;
};

#endif
