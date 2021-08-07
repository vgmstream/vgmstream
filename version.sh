#!/bin/sh

# echo current git version (doesn't make version.h)

# test if git exists
if ! command -v git > /dev/null
then
    VERSION="unknown"
else
    VERSION=$(git describe --always 2>&1 | tr : _ )
fi

# ignore git stderr "fatal: 
if case $VERSION in fatal*) ;; *) false;; esac; then
    echo ""
else
    echo "$VERSION"
fi;
