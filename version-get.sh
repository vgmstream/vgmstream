#!/bin/sh

# echo current git version (doesn't make version_auto.h)

# try get version from Git (dynamic), including lightweight tags
if ! command -v git > /dev/null
then
    VERSION="unknown"
else
    VERSION=$(git describe --tags --always 2>&1 | tr : _ )
fi

# ignore git stderr "fatal: 
if case $VERSION in fatal*) ;; *) false;; esac; then
    echo "unknown"
else
    echo "$VERSION"
fi;
