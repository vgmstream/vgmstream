#!/bin/sh

# get current git version, redirect stderr to stdin, change : to _
VERSION=$(git describe --always 2>&1 | tr : _ )

# ignore git stderr "fatal: 
if case $VERSION in fatal*) ;; *) false;; esac; then
    echo ""
else
    echo "$VERSION"
fi;
