#include "../vgmstream.h"
#include "../util/reader_sf.h"

#include "libs/g72x_vgmstream.h"

extern void g72x_init_state(struct g72x_state *);

extern int g721_decoder( int code, /*int out_coding,*/ struct g72x_state *state_ptr);


void decode_g721(VGMSTREAMCHANNEL* stream, sample_t* outbuf, int channelspacing, int32_t first_sample, int32_t samples_to_do) {
    struct g72x_state* state_ptr = &(stream->g72x_state);

    int sample_count = 0;
    for (int i = first_sample; i < first_sample + samples_to_do; i++) {
        uint8_t code = read_u8(stream->offset+i/2,stream->streamfile);
        code = code >> (i & 1 ? 4 : 0);
        outbuf[sample_count]= g721_decoder(code, /*AUDIO_ENCODING_LINEAR,*/ state_ptr);
        sample_count += channelspacing;
    }
}

void setup_g721(VGMSTREAM* vgmstream) {
    for (int i = 0; i < vgmstream->channels; i++) {
        struct g72x_state* state_ptr = &(vgmstream->ch[i].g72x_state);
        g72x_init_state(state_ptr);
    }
}

/* G.721 has no zero nibbles, so we look at the first few bytes 
 * (known files start with 0xFFFFFFFF, but probably an oddity of the codec) */
bool g721_check_format(STREAMFILE* sf, int interleave, int max_size) {
    
    for (int i = 0; i < max_size; i++) {
        uint8_t code = read_u8(i,sf);
        if ((code & 0x0f) == 0 || (code & 0xf0) == 0)
            return false;
    }

    /* and also check start of second channel */
    for (int i = interleave; i < interleave + max_size; i++) {
        uint8_t code = read_u8(i,sf);
        if ((code & 0x0f) == 0 || (code & 0xf0) == 0)
            return false;
    }

    return true;
}
