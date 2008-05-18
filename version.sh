DIR="`dirname "$0"`"
VERSION="`svnversion "$DIR" | tr : _`"
if [[ "$VERSION" = "exported" ]] ; then
echo "exported"
else
echo "r$VERSION"
fi
