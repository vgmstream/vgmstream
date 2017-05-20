#define POSIXLY_CORRECT
#include <getopt.h>
#include "../src/vgmstream.h"
#include "../src/util.h"
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef VERSION
#include "../version.h"
#endif
#ifndef VERSION
#define VERSION "(unknown version)"
#endif

#define BUFSIZE 0x8000

extern char * optarg;
extern int optind, opterr, optopt;


static void make_wav_header(uint8_t * buf, int32_t sample_count, int32_t sample_rate, int channels);
static void make_smpl_chunk(uint8_t * buf, int32_t loop_start, int32_t loop_end);

void usage(const char * name) {
    fprintf(stderr,"vgmstream test decoder " VERSION " " __DATE__ "\n"
          "Usage: %s [-o outfile.wav] [-l loop count]\n"
          "    [-f fade time] [-d fade delay] [-ipcmxeE] infile\n"
          "Options:\n"
          "    -o outfile.wav: name of output .wav file, default is dump.wav\n"
          "    -l loop count: loop count, default 2.0\n"
          "    -f fade time: fade time (seconds) after N loops, default 10.0\n"
          "    -d fade delay: fade delay (seconds, default 0.0\n"
          "    -i: ignore looping information and play the whole stream once\n"
          "    -p: output to stdout (for piping into another program)\n"
          "    -P: output to stdout even if stdout is a terminal\n"
          "    -c: loop forever (continuously)\n"
          "    -m: print metadata only, don't decode\n"
          "    -x: decode and print adxencd command line to encode as ADX\n"
          "    -g: decode and print oggenc command line to encode as OGG\n"
          "    -b: decode and print batch variable commands\n"
          "    -L: append a smpl chunk and create a looping wav\n"
          "    -e: force end-to-end looping\n"
          "    -E: force end-to-end looping even if file has real loop points\n"
          "    -r outfile2.wav: output a second time after resetting\n"
          "    -2 N: only output the Nth (first is 0) set of stereo channels\n"
          "    -F: don't fade after N loops and play the rest of the stream\n"
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
    int lwav = 0;
    int batchvar = 0;
    int only_stereo = -1;
    double loop_count = 2.0;
    double fade_seconds = 10.0;
    double fade_delay_seconds = 0.0;
    int fade_ignore = 0;
    int32_t bytecount;

    while ((opt = getopt(argc, argv, "o:l:f:d:ipPcmxeLEFr:gb2:")) != -1) {
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
            case 'b':
                batchvar = 1;
                break;
            case 'e':
                force_loop = 1;
                break;
            case 'E':
                really_force_loop = 1;
                break;
            case 'L':
                lwav = 1;
                break;
            case 'r':
                reset_outfilename = optarg;
                break;
            case '2':
                only_stereo = atoi(optarg);
                break;
            case 'F':
                fade_ignore = 1;
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
        } else if (batchvar) {
            if (!metaonly) printf("set fname=\"%s\"\n",outfilename);
            printf("set tsamp=%d\nset chan=%d\n", s->num_samples, s->channels);
            if (s->loop_flag) printf("set lstart=%d\nset lend=%d\nset loop=1\n", s->loop_start_sample, s->loop_end_sample);
            else printf("set loop=0\n");
        }
        else if (metaonly) printf("metadata for %s\n",argv[optind]);
        else printf("decoding %s\n",argv[optind]);
    }
    if (!play && !adxencd && !oggenc && !batchvar) {
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
    if (!buf) {
        fprintf(stderr,"failed allocating output buffer\n");
        close_vgmstream(s);
        return 1;
    }

    /* signal ignore fade for get_vgmstream_play_samples */
    if (loop_count && fade_ignore) {
        fade_seconds = -1.0;
    }

    len = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,s);
    if (!play && !adxencd && !oggenc && !batchvar) printf("samples to play: %d (%.4lf seconds)\n",len,(double)len/s->sample_rate);
    fade_samples = fade_seconds * s->sample_rate;

    /* slap on a .wav header */
    if (only_stereo != -1) {
        make_wav_header((uint8_t*)buf, len, s->sample_rate, 2);
    } else {
        make_wav_header((uint8_t*)buf, len, s->sample_rate, s->channels);
    }
    if (lwav && s->loop_flag) { // Adding space for smpl chunk at end
        bytecount = get_32bitLE((uint8_t*)buf + 4);
        put_32bitLE((uint8_t*)buf + 4, bytecount + 0x44);
    }
    fwrite(buf,1,0x2c,outfile);

    /* decode forever */
    while (forever) {
        render_vgmstream(buf,BUFSIZE,s);
        swap_samples_le(buf,s->channels*BUFSIZE);
        if (only_stereo != -1) {
            int j;
            for (j=0;j<BUFSIZE;j++) {
                fwrite(buf+j*s->channels+(only_stereo*2),sizeof(sample),2,outfile);
            }
        } else {
            fwrite(buf,sizeof(sample)*s->channels,BUFSIZE,outfile);
        }
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
        swap_samples_le(buf,s->channels*toget);

        if (only_stereo != -1) {
            int j;
            for (j=0;j<toget;j++) {
                fwrite(buf+j*s->channels+(only_stereo*2),sizeof(sample),2,outfile);
            }
        } else {
            fwrite(buf,sizeof(sample)*s->channels,toget,outfile);
        }
    }

    if (lwav && s->loop_flag) { // Writing smpl chuck
        make_smpl_chunk((uint8_t*)buf, s->loop_start_sample, s->loop_end_sample);
        fwrite(buf,1,0x44,outfile);
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

            /* do proper little endian samples */
            swap_samples_le(buf,s->channels*toget);
            if (only_stereo != -1) {
                int j;
                for (j=0;j<toget;j++) {
                    fwrite(buf+j*s->channels+(only_stereo*2),sizeof(sample),2,outfile);
                }
            } else {
                fwrite(buf,sizeof(sample)*s->channels,toget,outfile);
            }
        }
        fclose(outfile); outfile = NULL;
    }

    close_vgmstream(s);
    free(buf);

    return 0;
}



/**
 * make a header for PCM .wav
 * buffer must be 0x2c bytes
 */
static void make_wav_header(uint8_t * buf, int32_t sample_count, int32_t sample_rate, int channels) {
    size_t bytecount;

    bytecount = sample_count*channels*sizeof(sample);

    memcpy(buf+0, "RIFF", 4); /* RIFF header */
    put_32bitLE(buf+4, (int32_t)(bytecount+0x2c-8)); /* size of RIFF */

    memcpy(buf+8, "WAVE", 4); /* WAVE header */

    memcpy(buf+0xc, "fmt ", 4); /* WAVE fmt chunk */
    put_32bitLE(buf+0x10, 0x10); /* size of WAVE fmt chunk */
    put_16bitLE(buf+0x14, 1); /* compression code 1=PCM */
    put_16bitLE(buf+0x16, channels); /* channel count */
    put_32bitLE(buf+0x18, sample_rate); /* sample rate */
    put_32bitLE(buf+0x1c, sample_rate*channels*sizeof(sample)); /* bytes per second */
    put_16bitLE(buf+0x20, (int16_t)(channels*sizeof(sample))); /* block align */
    put_16bitLE(buf+0x22, sizeof(sample)*8); /* significant bits per sample */

    /* PCM has no extra format bytes, so we don't even need to specify a count */

    memcpy(buf+0x24, "data", 4); /* WAVE data chunk */
    put_32bitLE(buf+0x28, (int32_t)bytecount); /* size of WAVE data chunk */
}

static void make_smpl_chunk(uint8_t * buf, int32_t loop_start, int32_t loop_end) {
   int i;

    memcpy(buf+0, "smpl", 4);/* header */
    put_32bitLE(buf+4, 0x3c);/* size */

    for (i = 0; i < 7; i++)
        put_32bitLE(buf+8 + i * 4, 0);

    put_32bitLE(buf+36, 1);

    for (i = 0; i < 3; i++)
        put_32bitLE(buf+40 + i * 4, 0);

    put_32bitLE(buf+52, loop_start);
    put_32bitLE(buf+56, loop_end);
    put_32bitLE(buf+60, 0);
    put_32bitLE(buf+64, 0);
}
