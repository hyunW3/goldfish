#!/bin/sh
make clean
make x86_64_emu_defconfig
make menuconfig
make -j4 
