config_dir = ./config/$(CONFIG)
out_dir = ./out/$(CONFIG)

cc = gcc
as = as
nas=nasm
ld = ld

s1_bin = $(out_dir)/boot_s1.bin
s2_bin = $(out_dir)/boot_s2.bin
s3_bin = $(out_dir)/boot_s3.bin


s1_addr = 0x7c00
s2_addr = 0x7e00
s3_addr = 0xb000

arch=./arch/x86
fs_dir = ./fs/$(FILESYSTEM)
lib_dir = ./lib

incl=-I./include -I$(arch) -I$(fs_dir) -I$(lib_dir)

cc_flags = -c -march=i386 -m32 \
-ffreestanding \
-fomit-frame-pointer \
-mno-red-zone \
-fno-toplevel-reorder \
-fno-unit-at-a-time \
-fno-stack-protector \
-mpreferred-stack-boundary=2 \
-W -Wall -Wstrict-prototypes \
-fno-ident \
$(incl) \
-include code16gcc.h -D$(CONFIG)

ld_flags =  --oformat=binary -melf_i386 --architecture=i386 -nodefaultlibs -nostartfiles -nostdlib

entrys1.loc=$(arch)/entry
entrys1.src=$(entrys1.loc)/boot_s1_16_32.asm
entrys1.obj=$(out_dir)/boot_s1_16_32.o

entrys2.loc=$(arch)/entry
entrys2.src=$(entrys2.loc)/boot_s2_16_32.S
entrys2.obj=$(out_dir)/boot_s2_16_32.o

entrys3.loc=$(arch)/entry
entrys3.src=$(entrys3.loc)/boot_s3_16_32.S
entrys3.lst=$(out_dir)/boot_s3_16_32.lst
entrys3.obj=$(out_dir)/boot_s3_16_32.o

a20.loc=$(arch)
a20.src=$(a20.loc)/a20.c
a20.obj=$(out_dir)/a20.o

bios.loc=$(arch)
bios.src=$(arch)/bios.c
bios.obj=$(out_dir)/bios.o

cpuid.loc=$(arch)
cpuid.src=$(cpuid.loc)/cpuid.c
cpuid.obj=$(out_dir)/cpuid.o

globals.loc=$(arch)
globals.src=$(globals.loc)/globals.c
globals.obj=$(out_dir)/globals.o

pm.loc=$(arch)
pm.src=$(pm.loc)/pm.c
pm.obj=$(out_dir)/pm.o

pml4.loc=$(arch)
pml4.src=$(pml4.loc)/pml4.c
pml4.obj=$(out_dir)/pml4.o

stubs2.loc=$(arch)
stubs2.src=$(stubs2.loc)/stub.c
stubs2.obj=$(out_dir)/stub_s2.o

stubs3.loc=$(arch)
stubs3.src=$(stubs3.loc)/stub.c
stubs3.obj=$(out_dir)/stub_s3.o

lib.loc=$(lib_dir)
lib.src=$(lib.loc)/lib.c
lib.obj=$(out_dir)/lib.o

fs.loc=$(fs_dir)
fs.src=$(fs.loc)/$(FILESYSTEM).c
fs.obj=$(out_dir)/$(FILESYSTEM).o

mains2.loc=./s2
mains2.src=$(mains2.loc)/main_s2.c
mains2.obj=$(out_dir)/main_s2.o

kstartup.loc=$(arch)
kstartup.src=$(kstartup.loc)/kstartup.c
kstartup.obj=$(out_dir)/kstartup.o

menu.loc=./s3
menu.src=$(menu.loc)/menu.c
menu.obj=$(out_dir)/menu.o

mains3.loc=./s3
mains3.src=$(mains3.loc)/main_s3.c
mains3.obj=$(out_dir)/main_s3.o

obj_s1=$(entrys1.obj)

obj_s2=$(entrys2.obj) \
	$(bios.obj) \
	$(stubs2.obj) \
	$(fs.obj) \
	$(lib.obj) \
	$(mains2.obj) 

obj_s3=$(entrys3.obj)\
	$(stubs3.obj) \
	$(a20.obj) \
	$(lib.obj) \
	$(mains3.obj)\
	$(menu.obj) \
	$(pm.obj) \
	$(pml4.obj) \
	$(kstartup.obj) \
	$(cpuid.obj) \
	$(globals.obj)

clean:
	@rm -fv $(out_dir)/*.o
	@rm -fv $(out_dir)/*.bin
	@rm -fv $(out_dir)/*.img

all: $(s1_bin) $(s2_bin) $(s3_bin)

$(s1_bin):$(obj_s1)
	@echo ------- linking stage 1 ---------
	@$(ld) -e $(s1_addr) $(ld_flags) $(entrys1.obj) -o $(s1_bin)


$(s2_bin):$(obj_s2)
	@echo  ------- linking stage 2 ---------
	@$(ld) -e $(s2_addr) -T./s2/setup_s2.ld $(ld_flags) $^  -o$@

$(s3_bin):$(obj_s3)
	@echo  ------- linking stage 3 ---------
	@$(ld) -e $(s3_addr) -T./s3/setup_s3.ld $(ld_flags) $^  -o$@


$(entrys1.obj):$(entrys1.src)
	@$(nas) -felf64 $^ -o $@
	@objcopy $@ -Oelf32-i386
	@echo $^

$(entrys2.obj):$(entrys2.src)
	@$(as) --32 $(call cc-option, -m32) $^ -o $@
	@echo $^
	
$(mains2.obj):$(mains2.src)
	@$(cc) $(cc_flags) -O2 $^ -o $@ 
	@echo $^

$(bios.obj):$(bios.src)
	@$(cc) $(cc_flags) -O0 $^ -o $@ 
	@echo $^

$(stubs2.obj):$(stubs2.src)
	@$(cc) $(cc_flags) -O2 -D_STAGE_2_ $^ -o $@ 
	@echo $^

$(fs.obj):$(fs.src)
	@$(cc) $(cc_flags) -O2 $^ -o $@ 
	@echo $^

$(lib.obj):$(lib.src)
	@$(cc) $(cc_flags) -O2 $^ -o $@ 
	@echo $^

$(entrys3.obj):$(entrys3.src)
	@$(as) --32 $(call cc-option, -m32) $^ -o $@
	@echo $^

$(stubs3.obj):$(stubs3.src)
	@$(cc) $(cc_flags) -O0 -D_STAGE_3_ $^ -o $@ 
	@echo $^

$(mains3.obj):$(mains3.src)
	@$(cc) $(cc_flags)  -O0 $^ -o $@ 
	@echo $^

$(menu.obj):$(menu.src)
	@$(cc) $(cc_flags)  -O0 $^ -o $@ 
	@echo $^

$(pm.obj):$(pm.src)
	@$(cc) $(cc_flags) -O2 $^ -o $@ 
	@echo $^

$(pml4.obj):$(pml4.src)
	@$(cc) $(cc_flags) -O1 $^ -o $@ 
	@echo $^

$(kstartup.obj):$(kstartup.src)
	@$(cc) $(cc_flags) -O0 $^ -o $@ 
	@echo $^

$(cpuid.obj):$(cpuid.src)
	@$(cc) $(cc_flags) -O2 $^ -o $@ 
	@echo $^

$(globals.obj):$(globals.src)
	@$(cc) $(cc_flags) -O0 $^ -o $@ 
	@echo $^

$(a20.obj):$(a20.src)
	@$(cc) $(cc_flags) -O2 $^ -o $@ 
	@echo $^

