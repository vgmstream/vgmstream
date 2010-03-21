#include "../vgmstream.h"

#ifdef VGM_USE_G7221
#include <stdio.h>
#include "coding.h"
#include "../util.h"

/* just dump channels to files for now */
void decode_g7221(VGMSTREAM * vgmstream, 
        sample * outbuf, int channelspacing, int32_t samples_to_do, int channel) {

    static FILE *dumpfiles[2] = {NULL,NULL};

    if (0 == vgmstream->samples_into_block)
    {
        uint8_t buffer[960/8];
        if (NULL == dumpfiles[channel])
        {
            char filename[] = "dump0.bin";
            snprintf(filename,sizeof(filename),"dump%d.bin",channel);
            dumpfiles[channel] = fopen(filename, "wb");
        }
        vgmstream->ch[channel].streamfile->read(vgmstream->ch[channel].streamfile, buffer, vgmstream->ch[channel].offset, vgmstream->interleave_block_size);
        fwrite(buffer, 1, vgmstream->interleave_block_size, dumpfiles[channel]);
    }
}

#endif
