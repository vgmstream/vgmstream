#!/bin/sh

# patch script for media-autobuild_suite (or apply manually)

# allows Windows XP builds of FFmpeg (may be outdated)
git apply ffmpeg-revert-bcrypt-random.patch
