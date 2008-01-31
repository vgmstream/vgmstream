#include "../src/vgmstream.h"
#include "../src/util.h"

#define BUFSIZE 4000

int main(int argc, char ** argv) {
    VGMSTREAM * s;
    sample buf[BUFSIZE*2];
    int32_t len;
    int i;
    FILE * outfile = fopen("dump.bin","wb");

    if (argc!=2) {printf("1 arg\n"); return 1;}

    s = init_vgmstream(argv[1]);

    if (!s) {
        printf("open failed\n");
        return 1;
    }


    printf("samplerate %d Hz\n",s->sample_rate);
    printf("channels: %d\n",s->channels);
    if (s->loop_flag) {
        printf("loop start: %d samples (%lf seconds)\n",s->loop_start_sample,(double)s->loop_start_sample/s->sample_rate);
        printf("loop end: %d samples (%lf seconds)\n",s->loop_end_sample,(double)s->loop_end_sample/s->sample_rate);
    }
    printf("file total samples %d (%lf seconds)\n",s->num_samples);

    len = get_vgmstream_play_samples(2.0,10.0,s);
    printf("samples to play %d (%lf seconds)\n",len,(double)len/s->sample_rate);

    for (i=0;i<len;i+=BUFSIZE) {
        int toget=BUFSIZE;
        if (i+BUFSIZE>len) toget=len-i;
        render_vgmstream(buf,toget,s);
        fwrite(buf,sizeof(sample)*2,toget,outfile);
    }
    
    close_vgmstream(s);
}
