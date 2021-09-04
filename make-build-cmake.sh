#!/bin/sh

# example script that builds vgmstream with most libs enabled using CMake + make

sudo apt-get update
sudo apt-get install gcc g++ make build-essential git cmake
sudo apt-get install libao-dev audacious-dev libjansson-dev
sudo apt-get install libvorbis-dev libmpg123-dev libspeex-dev libavformat-dev libavcodec-dev libavutil-dev libswresample-dev

mkdir -p build
cd build 
cmake -S .. -B .
make
