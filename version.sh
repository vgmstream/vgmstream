#!/bin/sh
DIR="`dirname "$0"`"
VERSION="`git describe --always | tr : _`"
echo "$VERSION"
