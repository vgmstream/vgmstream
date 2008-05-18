#!/bin/sh
DIR="`dirname "$0"`"
VERSION="`svnversion "$DIR" | tr : _`"
echo "r$VERSION"
