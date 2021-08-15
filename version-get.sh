#!/bin/sh

# echo current git version (doesn't make version_auto.h)
VERSION_DEFAULT=unknown
VERSION_NAME=VGMSTREAM_VERSION
#VERSION_FILE=version_auto.h


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
    LINE="$VERSION_DEFAULT"

    while IFS= read -r -u3 item; do
        COMP="#define $VERSION_NAME*"
        if [[ $item == $COMP ]] ; then
            STR_REM1="*$VERSION_NAME \""
            STR_REM2="\"*"
            LINE=$item
            LINE=${LINE/$STR_REM1/}
            LINE=${LINE/$STR_REM2/}
        fi
    done 3< "version.h"
fi


# final print
echo "$LINE"
