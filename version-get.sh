#!/bin/sh

# echo current git version (doesn't make version_auto.h)
VERSION_EMPTY=$1
#VERSION_FILE=--
VERSION_NAME=$2
if [ -z "$VERSION_EMPTY" ]; then VERSION_EMPTY=false; fi
#if [ -z "$VERSION_FILE" ]; then VERSION_FILE=version_auto.h; fi
if [ -z "$VERSION_NAME" ]; then VERSION_NAME=VGMSTREAM_VERSION; fi
VERSION_DEFAULT=unknown

# try get version from Git (dynamic), including lightweight tags
if ! command -v git > /dev/null ; then
    VERSION=""
else
    VERSION=$(git describe --tags --always 2>&1 | tr : _ )
fi

# ignore git stderr "fatal:*" or blank
if  [[ $VERSION != fatal* ]] && [ ! -z "$VERSION" ] ; then
    LINE="$VERSION"
else
    # try to get version from version.h (static)
    #echo "Git version not found, can't autogenerate version (using default)"

    # option to output empty line instead of default version, so plugins can detect git-less builds
    if [ "$VERSION_EMPTY" == "true" ]; then 
        LINE="/* ignored */"
    else
        LINE="$VERSION_DEFAULT"
        while IFS= read -r -u3 item; do
            COMP="#define $VERSION_NAME*"
            if [[ $item == $COMP ]] ; then
                # clean "#define ..." leaving rXXXX only
                STR_REMOVE1="*$VERSION_NAME \""
                STR_REMOVE2="\"*"
                LINE=$item
                LINE=${LINE/$STR_REMOVE1/}
                LINE=${LINE/$STR_REMOVE2/}
            fi
        done 3< "version.h"
    fi
fi


# final print
echo "$LINE"
