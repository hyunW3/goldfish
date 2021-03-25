#!/bin/sh
make clean
export ARCH=x86_64
export CROSS_COMPILE=~/x86_64-linux-android-4.9/bin/x86_64-linux-android-
make x86_64_ranchu_defconfig
make -j4
