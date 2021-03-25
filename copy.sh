#!/bin/sh

mv ~/Android/Sdk/system-images/android-24/google_apis/x86_64/kernel-ranchu ~/Android/Sdk/system-images/android-24/google_apis/x86_64/kernel-ranchu-week-$1

 
cp ~/goldfish/arch/x86/boot/bzImage ~/Android/Sdk/system-images/android-24/google_apis/x86_64/kernel-ranchu

