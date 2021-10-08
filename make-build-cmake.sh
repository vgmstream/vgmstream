#!/bin/sh

# example script that builds vgmstream with most libs enabled using CMake + make

sudo apt-get -y update
# base deps
sudo apt-get install gcc g++ make build-essential git cmake
# optional: for extra formats (can be ommited to build with static libs)
sudo apt-get install libmpg123-dev libvorbis-dev libspeex-dev
sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev libswresample-dev
sudo apt-get install yasm libopus-dev
# optional: for vgmstream 123 and audacious
sudo apt-get install -y libao-dev audacious-dev
#sudo apt-get install libjansson-dev

mkdir -p build
cd build 
cmake -S .. -B .
make
