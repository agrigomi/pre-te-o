#!/bin/sh
../host/mkfs/mgfs2/mkmgfs -s1 -c2 -n./out/x86_64/mgfs2.img -a./out/x86_64/boot_s1.bin -b./out/x86_64/boot_s2.bin -f ./config/x86_64/file_map_x86_64
#../host/mkfs/mgfs2/mkmgfs -s1 -c2 -n./out/x86_64/mgfs2.img -a../boot2/x86/out/vbr.bin -b../boot2/x86/out/stage2.bin -f ./config/x86_64/file_map_x86_64
