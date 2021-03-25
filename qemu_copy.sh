#!/bin/sh
mv ~/Android/Sdk/system-images/android-24/google_apis/x86_64/kernel-qemu ~/Android/Sdk/system-images/android-24/google_apis/x86_64/kernel-qemu_$1".bak"
cp arch/x86/boot/bzImage ~/Android/Sdk/system-images/android-24/google_apis/x86_64/kernel-qemu
