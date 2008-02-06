#define POSIXLY_CORRECT
#include <unistd.h>
#include "../src/vgmstream.h"
#include "../src/util.h"

#define BUFSIZE 4000

extern char * optarg;
extern int optind, opterr, optopt;

void usage(const char * name) {
    fprintf(stderr,"Usage: %s [-o outfile.wav] [-l loop count] [-f fade time] [-i] [-p] [-c] infile\n\t-i: ignore loop\n\t-p: output to stdout\n\t-c: loop forever\n",name);
}

int main(int argc, char ** argv) {
    VGMSTREAM * s;
    sample buf[BUFSIZE*2];
    int32_t len;
    int32_t fade_samples;
    int i;
    FILE * outfile = NULL;
    int opt;
    int ignore_loop = 0;
    int play = 0;
    int forever = 0;
    double loop_count = 2.0;
    double fade_time = 10.0;
    
    while ((opt = getopt(argc, argv, "o:l:f:ipc")) != -1) {
        switch (opt) {
            case 'o':
                outfile = fopen(optarg,"wb");
                if (!outfile) {
                    fprintf(stderr,"failed to open %s for output\n",optarg);
                    return 1;
                }
                break;
            case 'l':
                loop_count = atoi(optarg);
                break;
            case 'f':
                fade_time = atoi(optarg);
                break;
            case 'i':
                ignore_loop = 1;
                break;
            case 'p':
                play = 1;
                break;
            case 'c':
                forever = 1;
                break;
            default:
                usage(argv[0]);
                return 1;
                break;
        }
    }

    if (optind!=argc-1) {
        usage(argv[0]);
        return 1;
    }

    if (forever && !play) {
        fprintf(stderr,"A file of infinite size? Not likely.\n");
        return 1;
    }
   
    s = init_vgmstream(argv[optind]);

    if (!s) {
        fprintf(stderr,"failed opening %s\n",argv[optind]);
        return 1;
    }

    if (ignore_loop) s->loop_flag=0;

    if (play) {
        if (outfile) {
            fprintf(stderr,"either -p or -o, make up your mind\n");
            return 1;
        }
        outfile = stdout;
    }
    else if (!outfile) {
        outfile = fopen("dump.wav","wb");
        if (!outfile) {
            fprintf(stderr,"failed to open dump.wav for output\n");
            return 1;
        }
    }

    if (forever && !s->loop_flag) {
        fprintf(stderr,"I could play a nonlooped track forever, but it wouldn't end well.");
        return 1;
    }

    if (!play) printf("decoding %s\n",argv[optind]);
    /*
    printf("sample rate %d Hz\n",s->sample_rate);
    printf("channels: %d\n",s->channels);
    if (s->loop_flag) {
        printf("loop start: %d samples (%.2lf seconds)\n",s->loop_start_sample,(double)s->loop_start_sample/s->sample_rate);
        printf("loop end: %d samples (%.2lf seconds)\n",s->loop_end_sample,(double)s->loop_end_sample/s->sample_rate);
    }
    printf("file total samples: %d (%.2lf seconds)\n",s->num_samples,(double)s->num_samples/s->sample_rate);
    */
    if (!play) describe_vgmstream(s);

    len = get_vgmstream_play_samples(loop_count,fade_time,s);
    if (!play) printf("samples to play: %d (%.2lf seconds)\n",len,(double)len/s->sample_rate);
    fade_samples = fade_time * s->sample_rate;

    /* slap on a .wav header */
    make_wav_header((uint8_t*)buf, len, s->sample_rate, s->channels);
    fwrite(buf,1,0x2c,outfile);

    while (forever) {
        render_vgmstream(buf,BUFSIZE,s);
        fwrite(buf,sizeof(sample)*s->channels,BUFSIZE,outfile);
    }

    for (i=0;i<len;i+=BUFSIZE) {
        int toget=BUFSIZE;
        if (i+BUFSIZE>len) toget=len-i;
        render_vgmstream(buf,toget,s);

        if (s->loop_flag && fade_samples > 0) {
            int samples_into_fade = i - (len - fade_samples);
            if (samples_into_fade + toget > 0) {
                int j,k;
                for (j=0;j<toget;j++,samples_into_fade++) {
                    if (samples_into_fade > 0) {
                        double fadedness = (double)(fade_samples-samples_into_fade)/fade_samples;
                        for (k=0;k<s->channels;k++) {
                            buf[j*s->channels+k] = buf[j*s->channels+k]*fadedness;
                        }
                    }
                }
            }
        }
        fwrite(buf,sizeof(sample)*s->channels,toget,outfile);
    }
    
    close_vgmstream(s);
}
