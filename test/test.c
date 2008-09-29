#define POSIXLY_CORRECT
#include <unistd.h>
#include "../src/vgmstream.h"
#include "../src/util.h"
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

#define BUFSIZE 4000

extern char * optarg;
extern int optind, opterr, optopt;

void usage(const char * name) {
    fprintf(stderr,"vgmstream test decoder " VERSION " " __DATE__ "\n"
          "Usage: %s [-o outfile.wav] [-l loop count]\n"
          "    [-f fade time] [-d fade delay] [-ipcmxeE] infile\n"
          "Options:\n"
          "    -o outfile.wav: name of output .wav file, default is dump.wav\n"
          "    -l loop count: loop count, default 2.0\n"
          "    -f fade time: fade time (seconds), default 10.0\n"
          "    -d fade delay: fade delay (seconds, default 0.0\n"
          "    -i: ignore looping information and play the whole stream once\n"
          "    -p: output to stdout (for piping into another program)\n"
          "    -P: output to stdout even if stdout is a terminal\n"
          "    -c: loop forever (continuously)\n"
          "    -m: print metadata only, don't decode\n"
          "    -x: decode and print adxencd command line to encode as ADX\n"
          "    -g: decode and print oggenc command line to encode as OGG\n"
          "    -e: force end-to-end looping\n"
          "    -E: force end-to-end looping even if file has real loop points\n"
          "    -r outfile2.wav: output a second time after resetting\n"
            ,name);
    
}

int main(int argc, char ** argv) {
    VGMSTREAM * s;
    sample * buf = NULL;
    int32_t len;
    int32_t fade_samples;
    int i;
    FILE * outfile = NULL;
    char * outfilename = NULL;
    char * reset_outfilename = NULL;
    int opt;
    int ignore_loop = 0;
    int force_loop = 0;
    int really_force_loop = 0;
    int play = 0;
    int play_wreckless = 0;
    int forever = 0;
    int metaonly = 0;
    int adxencd = 0;
    int oggenc = 0;
    double loop_count = 2.0;
    double fade_seconds = 10.0;
    double fade_delay_seconds = 0.0;
    
    while ((opt = getopt(argc, argv, "o:l:f:d:ipPcmxeEr:g")) != -1) {
        switch (opt) {
            case 'o':
                outfilename = optarg;
                break;
            case 'l':
                loop_count = atof(optarg);
                break;
            case 'f':
                fade_seconds = atof(optarg);
                break;
            case 'd':
                fade_delay_seconds = atof(optarg);
                break;
            case 'i':
                ignore_loop = 1;
                break;
            case 'p':
                play = 1;
                break;
            case 'P':
                play_wreckless = 1;
                play = 1;
                break;
            case 'c':
                forever = 1;
                break;
            case 'm':
                metaonly = 1;
                break;
            case 'x':
                adxencd = 1;
                break;
            case 'g':
                oggenc = 1;
                break;
            case 'e':
                force_loop = 1;
                break;
            case 'E':
                really_force_loop = 1;
                break;
            case 'r':
                reset_outfilename = optarg;
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

    if (play && (!play_wreckless &&isatty(STDOUT_FILENO))) {
        fprintf(stderr,"Are you sure you want to output wave data to the terminal?\nIf so use -P instead of -p.\n");
        return 1;
    }

#ifdef WIN32
    /* make stdout output work with windows */
    if (play) {
        _setmode(fileno(stdout),_O_BINARY);
    }
#endif

    if (ignore_loop && force_loop) {
        fprintf(stderr,"-e and -i are incompatible\n");
        return 1;
    }
    if (ignore_loop && really_force_loop) {
        fprintf(stderr,"-E and -i are incompatible\n");
        return 1;
    }
    if (force_loop && really_force_loop) {
        fprintf(stderr,"-E and -e are somewhat redundant, are you confused?\n");
        return 1;
    }

    s = init_vgmstream(argv[optind]);

    if (!s) {
        fprintf(stderr,"failed opening %s\n",argv[optind]);
        return 1;
    }

    /* force only if there aren't already loop points */
    if (force_loop && !s->loop_flag) {
        /* this requires a bit more messing with the VGMSTREAM than I'm
         * comfortable with... */
        s->loop_flag=1;
        s->loop_start_sample=0;
        s->loop_end_sample=s->num_samples;
        s->loop_ch=calloc(s->channels,sizeof(VGMSTREAMCHANNEL));
    }

    /* force even if there are loop points */
    if (really_force_loop) {
        if (!s->loop_flag) s->loop_ch=calloc(s->channels,sizeof(VGMSTREAMCHANNEL));
        s->loop_flag=1;
        s->loop_start_sample=0;
        s->loop_end_sample=s->num_samples;
    }

    if (ignore_loop) s->loop_flag=0;

    if (play) {
        if (outfilename) {
            fprintf(stderr,"either -p or -o, make up your mind\n");
            return 1;
        }
        outfile = stdout;
    } else if (!metaonly) {
        if (!outfilename) outfilename = "dump.wav";
        outfile = fopen(outfilename,"wb");
        if (!outfile) {
            fprintf(stderr,"failed to open %s for output\n",outfilename);
            return 1;
        }
    }

    if (forever && !s->loop_flag) {
        fprintf(stderr,"I could play a nonlooped track forever, but it wouldn't end well.");
        return 1;
    }

    if (!play) {
        if (adxencd) {
            printf("adxencd");
            if (!metaonly) printf(" \"%s\"",outfilename);
            if (s->loop_flag) printf(" -lps%d -lpe%d",s->loop_start_sample,s->loop_end_sample);
            printf("\n");
        } else if (oggenc) {
            printf("oggenc");
            if (!metaonly) printf(" \"%s\"",outfilename);
            if (s->loop_flag) printf(" -c LOOPSTART=%d -c LOOPLENGTH=%d",s->loop_start_sample,
                    s->loop_end_sample-s->loop_start_sample);
            printf("\n");
        }
        else if (metaonly) printf("metadata for %s\n",argv[optind]);
        else printf("decoding %s\n",argv[optind]);
    }
    if (!play && !adxencd && !oggenc) {
        char description[1024];
        description[0]='\0';
        describe_vgmstream(s,description,1024);
        printf("%s\n",description);
    }
    if (metaonly) {
        close_vgmstream(s);
        return 0;
    }

    buf = malloc(BUFSIZE*sizeof(sample)*s->channels);

    len = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,s);
    if (!play && !adxencd && !oggenc) printf("samples to play: %d (%.2lf seconds)\n",len,(double)len/s->sample_rate);
    fade_samples = fade_seconds * s->sample_rate;

    /* slap on a .wav header */
    make_wav_header((uint8_t*)buf, len, s->sample_rate, s->channels);
    fwrite(buf,1,0x2c,outfile);

    /* decode forever */
    while (forever) {
        render_vgmstream(buf,BUFSIZE,s);
        fwrite(buf,sizeof(sample)*s->channels,BUFSIZE,outfile);
    }

    /* decode */
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

    fclose(outfile); outfile = NULL;

#ifdef PROFILE_STREAMFILE
    {
        int i,j;
        size_t total_bytes_read = 0;
        for (i=0;i<s->channels;i++) {
            size_t bytes_read = get_streamfile_bytes_read(s->ch[i].streamfile);
            size_t file_size = get_streamfile_size(s->ch[i].streamfile);
            int error_count = get_streamfile_error_count(s->ch[i].streamfile);
            int already_reported = 0;

            /* see if we've reported this STREAMFILE already */
            for (j=i-1;!already_reported && j>=0;j--) {
                if (s->ch[j].streamfile == s->ch[i].streamfile) {
                    already_reported=1;
                }
            }

            if (already_reported) continue;

            total_bytes_read += bytes_read;
            fprintf(stderr,"ch%d: %lf%% (%d bytes read, file is %d bytes) %d errors\n",i,
                    bytes_read*100.0/file_size,bytes_read,file_size,error_count);
        }
        fprintf(stderr,"total bytes read: %d\n",total_bytes_read);
    }
#endif

    if (reset_outfilename) {
        outfile = fopen(reset_outfilename,"wb");
        if (!outfile) {
            fprintf(stderr,"failed to open %s for output\n",reset_outfilename);
            return 1;
        }
        /* slap on a .wav header */
        make_wav_header((uint8_t*)buf, len, s->sample_rate, s->channels);
        fwrite(buf,1,0x2c,outfile);

        reset_vgmstream(s);

        /* these manipulations are undone by reset */

        /* force only if there aren't already loop points */
        if (force_loop && !s->loop_flag) {
            /* this requires a bit more messing with the VGMSTREAM than I'm
             * comfortable with... */
            s->loop_flag=1;
            s->loop_start_sample=0;
            s->loop_end_sample=s->num_samples;
            s->loop_ch=calloc(s->channels,sizeof(VGMSTREAMCHANNEL));
        }

        /* force even if there are loop points */
        if (really_force_loop) {
            if (!s->loop_flag) s->loop_ch=calloc(s->channels,sizeof(VGMSTREAMCHANNEL));
            s->loop_flag=1;
            s->loop_start_sample=0;
            s->loop_end_sample=s->num_samples;
        }

        if (ignore_loop) s->loop_flag=0;

        /* decode */
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
        fclose(outfile); outfile = NULL;
    }

    close_vgmstream(s);

    return 0;
}
