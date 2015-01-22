#!/bin/sh
DIR="`dirname "$0"`"
VERSION="`git describe --always --tag | tr : _`"
echo "$VERSION"
