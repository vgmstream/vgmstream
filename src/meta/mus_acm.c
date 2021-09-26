#include "meta.h"
#include "../layout/layout.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef WIN32
#define DIRSEP '\\'
#else
#define DIRSEP '/'
#endif


static char** parse_mus(STREAMFILE* sf, int *out_file_count, int *out_loop_flag, int *out_loop_start_index, int *out_loop_end_index);
static void clean_mus(char** mus_filenames, int file_count);

/* .MUS - playlist for InterPlay games [Planescape: Torment (PC), Baldur's Gate Enhanced Edition (PC)] */
VGMSTREAM* init_vgmstream_mus_acm(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    segmented_layout_data *data = NULL;

    int channel_count, loop_flag = 0, loop_start_index = -1, loop_end_index = -1;
    int32_t num_samples = 0, loop_start_samples = 0, loop_end_samples = 0;

    char** mus_filenames = NULL;
    int i, segment_count = 0;


    /* checks */
    if (!check_extensions(sf, "mus"))
        goto fail;

    /* get file paths from the .MUS text file */
    mus_filenames = parse_mus(sf, &segment_count, &loop_flag, &loop_start_index, &loop_end_index);
    if (!mus_filenames) goto fail;

    /* init layout */
    data = init_layout_segmented(segment_count);
    if (!data) goto fail;

    /* open each segment subfile */
    for (i = 0; i < segment_count; i++) {
        STREAMFILE* temp_sf = sf->open(sf, mus_filenames[i], STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!temp_sf) goto fail;

        /* find .ACM type */
        switch(read_32bitBE(0x00,temp_sf)) {
            case 0x97280301: /* ACM header id [Planescape: Torment (PC)]  */
                data->segments[i] = init_vgmstream_acm(temp_sf);
                break;
            case 0x4F676753: /* "OggS" [Planescape: Torment Enhanced Edition (PC)] */
                data->segments[i] = init_vgmstream_ogg_vorbis(temp_sf);
                break;
            default:
                data->segments[i] = NULL;
                break;
        }
        close_streamfile(temp_sf);

        if (!data->segments[i]) goto fail;


        if (i==loop_start_index)
            loop_start_samples = num_samples;
        if (i==loop_end_index)
            loop_end_samples   = num_samples;

        num_samples += data->segments[i]->num_samples;
    }

    if (i==loop_end_index)
        loop_end_samples = num_samples;

    /* setup segmented VGMSTREAMs */
    if (!setup_layout_segmented(data))
        goto fail;


    channel_count = data->output_channels;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = data->segments[0]->sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_samples;
    vgmstream->loop_end_sample = loop_end_samples;

    vgmstream->meta_type = meta_MUS_ACM;
    vgmstream->coding_type = data->segments[0]->coding_type;
    vgmstream->layout_type = layout_segmented;
    vgmstream->layout_data = data;

    clean_mus(mus_filenames, segment_count);
    return vgmstream;

fail:
    clean_mus(mus_filenames, segment_count);
    free_layout_segmented(data);
    close_vgmstream(vgmstream);
    return NULL;
}

/* .mus text file parsing */

#define NAME_LENGTH PATH_LIMIT

static int exists(char *filename, STREAMFILE *streamfile) {
    STREAMFILE * temp = streamfile->open(streamfile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!temp) return 0;

    close_streamfile(temp);
    return 1;
}

/* needs the name of a file in the directory to test, as all we can do reliably is attempt to open a file */
static int find_directory_name(char *name_base, char *dir_name, int subdir_name_size, char *subdir_name, char *name, char *file_name, STREAMFILE *streamfile) {
    /* find directory name */
    {
        char temp_dir_name[NAME_LENGTH];

        subdir_name[0]='\0';
        concatn(subdir_name_size,subdir_name,name_base);

        if (strlen(subdir_name) >= subdir_name_size-2) goto fail;
        subdir_name[strlen(subdir_name)+1]='\0';
        subdir_name[strlen(subdir_name)]=DIRSEP;

        temp_dir_name[0]='\0';
        concatn(sizeof(temp_dir_name),temp_dir_name,dir_name);
        concatn(sizeof(temp_dir_name),temp_dir_name,subdir_name);
        concatn(sizeof(temp_dir_name),temp_dir_name,name_base);
        concatn(sizeof(temp_dir_name),temp_dir_name,name);
        concatn(sizeof(temp_dir_name),temp_dir_name,".ACM");

        if (!exists(temp_dir_name,streamfile)) {
            int i;
            /* try all lowercase */
            for (i=strlen(subdir_name)-1;i>=0;i--) {
                subdir_name[i]=tolower(subdir_name[i]);
            }
            temp_dir_name[0]='\0';
            concatn(sizeof(temp_dir_name),temp_dir_name,dir_name);
            concatn(sizeof(temp_dir_name),temp_dir_name,subdir_name);
            concatn(sizeof(temp_dir_name),temp_dir_name,name_base);
            concatn(sizeof(temp_dir_name),temp_dir_name,name);
            concatn(sizeof(temp_dir_name),temp_dir_name,".ACM");

            if (!exists(temp_dir_name,streamfile)) {
                /* try first uppercase */
                subdir_name[0]=toupper(subdir_name[0]);
                temp_dir_name[0]='\0';
                concatn(sizeof(temp_dir_name),temp_dir_name,dir_name);
                concatn(sizeof(temp_dir_name),temp_dir_name,subdir_name);
                concatn(sizeof(temp_dir_name),temp_dir_name,name_base);
                concatn(sizeof(temp_dir_name),temp_dir_name,name);
                concatn(sizeof(temp_dir_name),temp_dir_name,".ACM");
                if (!exists(temp_dir_name,streamfile)) {
                    /* try also 3rd uppercase */
                    subdir_name[2]=toupper(subdir_name[2]);
                    temp_dir_name[0]='\0';
                    concatn(sizeof(temp_dir_name),temp_dir_name,dir_name);
                    concatn(sizeof(temp_dir_name),temp_dir_name,subdir_name);
                    concatn(sizeof(temp_dir_name),temp_dir_name,name_base);
                    concatn(sizeof(temp_dir_name),temp_dir_name,name);
                    concatn(sizeof(temp_dir_name),temp_dir_name,".ACM");
                    
                    if (!exists(temp_dir_name,streamfile)) {
                        /* ah well, disaster has befallen your party */
                        goto fail;
                    }
                }
            }
        }
    }

    return 0;

fail:
    return 1;
}

static char** parse_mus(STREAMFILE *sf, int *out_file_count, int *out_loop_flag, int *out_loop_start_index, int *out_loop_end_index) {
    char** names = NULL;

    char filename[NAME_LENGTH];
    char line[NAME_LENGTH];
    char * end_ptr;
    char name_base[NAME_LENGTH];
    char dir_name[NAME_LENGTH];
    char subdir_name[NAME_LENGTH];

    int file_count = 0;
    size_t bytes_read;
    int line_ok = 0;
    off_t mus_offset = 0;

    int i;
    int loop_flag = 0, loop_start_index = -1, loop_end_index = -1;


    /* read file name base */
    bytes_read = read_line(line, sizeof(line), mus_offset, sf, &line_ok);
    if (!line_ok) goto fail;
    mus_offset += bytes_read;
    memcpy(name_base,line,sizeof(name_base));

    /* uppercase name_base */
    {
        int j;
        for (j=0;name_base[j];j++)
            name_base[j] = toupper(name_base[j]);
    }

    /* read track entry count */
    bytes_read = read_line(line, sizeof(line), mus_offset, sf, &line_ok);
    if (!line_ok) goto fail;
    if (line[0] == '\0') goto fail;
    mus_offset += bytes_read;
    file_count = strtol(line,&end_ptr,10);
    /* didn't parse whole line as an integer (optional opening whitespace) */
    if (*end_ptr != '\0') goto fail;

    /* set names */
    names = calloc(file_count,sizeof(char*)); /* array of strings (size NAME_LENGTH) */
    if (!names) goto fail;

    for (i = 0; i < file_count; i++) {
        names[i] = calloc(1,sizeof(char)*NAME_LENGTH);
        if (!names[i]) goto fail;
    }

    dir_name[0]='\0';
    sf->get_name(sf,filename,sizeof(filename));
    concatn(sizeof(dir_name),dir_name,filename);

    /* find directory name for the directory contianing the MUS */
    {
        char * last_slash = strrchr(dir_name,DIRSEP);
        if (last_slash != NULL) {
            last_slash[1]='\0'; /* trim off the file name */
        } else {
            dir_name[0] = '\0'; /* no dir name? annihilate! */
        }
    }

    /* can't do this until we have a file name */
    subdir_name[0]='\0';

    /* parse each entry */
    {
        char name[NAME_LENGTH];
        char loop_name_base_temp[NAME_LENGTH];
        char loop_name_temp[NAME_LENGTH];
        char loop_name_base[NAME_LENGTH];
        char loop_name[NAME_LENGTH];

        for (i = 0; i < file_count; i++)
        {
            int fields_matched;
            bytes_read = read_line(line,sizeof(line), mus_offset, sf, &line_ok);
            if (!line_ok) goto fail;
            mus_offset += bytes_read;

            fields_matched = sscanf(line,"%s %s %s",name,
                    loop_name_base_temp,loop_name_temp);

            if (fields_matched < 1) goto fail;
            if (fields_matched == 3 && loop_name_base_temp[0] != '@' && loop_name_temp[0] != '@')
            {
                int j;
                memcpy(loop_name,loop_name_temp,sizeof(loop_name));
                memcpy(loop_name_base,loop_name_base_temp,sizeof(loop_name_base));
                for (j=0;loop_name[j];j++) loop_name[j]=toupper(loop_name[j]);
                for (j=0;loop_name_base[j];j++) loop_name_base[j]=toupper(loop_name_base[j]);
                /* loop back entry */
                loop_end_index = i;
            }
            else if (fields_matched >= 2 && loop_name_base_temp[0] != '@')
            {
                int j;
                memcpy(loop_name,loop_name_base_temp,sizeof(loop_name));
                memcpy(loop_name_base,name_base,sizeof(loop_name_base));
                for (j=0;loop_name[j];j++) loop_name[j]=toupper(loop_name[j]);
                for (j=0;loop_name_base[j];j++) loop_name_base[j]=toupper(loop_name_base[j]);
                /* loop back entry */
                loop_end_index = i;
            }
            else
            {
                /* normal entry, ignoring the @TAG for now */
            }

            {
                /* uppercase */
                int j;
                for (j=0;j<strlen(name);j++)
                    name[j]=toupper(name[j]);
            }

            /* try looking in the common directory */
            names[i][0] = '\0';
            concatn(NAME_LENGTH,names[i],dir_name);
            concatn(NAME_LENGTH,names[i],name);
            concatn(NAME_LENGTH,names[i],".ACM");

            if (!exists(names[i],sf)) {

                /* We can't test for the directory until we have a file name
                 * to look for, so we do it here with the first file that seems to
                 * be in a subdirectory */
                if (subdir_name[0]=='\0') {
                    if (find_directory_name(name_base, dir_name, sizeof(subdir_name), subdir_name, name, filename, sf))
                        goto fail;
                }

                names[i][0] = '\0';
                concatn(NAME_LENGTH,names[i],dir_name);
                concatn(NAME_LENGTH,names[i],subdir_name);
                concatn(NAME_LENGTH,names[i],name_base);
                concatn(NAME_LENGTH,names[i],name);
                concatn(NAME_LENGTH,names[i],".ACM");

                if (!exists(names[i],sf)) goto fail;
            }
        }

        if (loop_end_index != -1) {
            /* find the file to loop back to */
            char target_name[NAME_LENGTH];
            target_name[0]='\0';
            concatn(sizeof(target_name),target_name,dir_name);
            concatn(sizeof(target_name),target_name,subdir_name);
            concatn(sizeof(target_name),target_name,loop_name_base);
            concatn(sizeof(target_name),target_name,loop_name);
            concatn(sizeof(target_name),target_name,".ACM");

            for (i=0;i<file_count;i++) {
                if (!strcmp(target_name,names[i])) {
                    loop_start_index = i;
                    break;
                }
            }

            if (loop_start_index != -1) {
                /*if (loop_start_index < file_count-1) loop_start_index++;*/
                loop_end_index++;
                loop_flag = 1;
            }

        }
    }

    *out_loop_start_index = loop_start_index;
    *out_loop_end_index = loop_end_index;
    *out_loop_flag = loop_flag;
    *out_file_count = file_count;


    return names;
fail:
    clean_mus(names, file_count);
    return NULL;
}

static void clean_mus(char** mus_filenames, int file_count) {
    int i;

    if (!mus_filenames) return;

    for (i = 0; i < file_count; i++) {
        free(mus_filenames[i]);
    }
    free(mus_filenames);
}
