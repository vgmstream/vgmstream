#include "paths.h"

void fix_dir_separators(char* filename) {
    char c;
    int i = 0;
    while ((c = filename[i]) != '\0') {
        if ((c == '\\' && DIR_SEPARATOR == '/') || (c == '/' && DIR_SEPARATOR == '\\'))
            filename[i] = DIR_SEPARATOR;
        i++;
    }
}
