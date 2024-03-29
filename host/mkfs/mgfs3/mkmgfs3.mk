# Generated by xmake tool

project.name=mkmgfs3
project.location=.
gpp=g++
ld=ld
incl=-I../../../include \
		    		-I../../../core/driver/fs/mgfs3 \
				-I../../../interface \
				-I../../xmake
interface=../../../interface
cpp_flags=-c -pthread -Wall $(incl) -D_GCC_ -D_CORE_
lib=-lstdc++ -lpthread
cpp.ext=.cpp
cpp.out=.o
cpp.tool=$(gpp)
cpp.flags=
rel.ext=
rel.out=.o
rel.tool=$(ld)
rel.flags=-r
elf.ext=
elf.out=
elf.tool=$(gpp)
elf.flags=
target=mkmgfs3
target.flags=-o $@ $(lib)

### branch 'x86' ###

branch=x86
branch.out=$(project.location)/out
branch.flags=$(cpp_flags) -o $@
x86.out=$(project.location)/out
x86.flags=$(cpp_flags) -o $@

### group: 'mgfs3' ###
mgfs3.location=../../../core/driver/fs/mgfs3
mgfs3.flags=
mgfs3.dep=$(mgfs3.location)/mgfs3.h \
			     		$(mgfs3.location)/inode.h \
					$(interface)/iFSIO.h \
					$(mgfs3.location)/dentry.h \
					$(mgfs3.location)/file.h \
					$(mgfs3.location)/bitmap.h \
					$(mgfs3.location)/buffer.h
mgfs3.out= \
	$(x86.out)/mgfs3-inode$(cpp.out) \
	$(x86.out)/mgfs3-dentry$(cpp.out) \
	$(x86.out)/mgfs3-mgfs3$(cpp.out) \
	$(x86.out)/mgfs3-bitmap$(cpp.out) \
	$(x86.out)/mgfs3-buffer$(cpp.out) \
	$(x86.out)/mgfs3-file$(cpp.out) 

mgfs3.target=$(x86.out)/rel_mgfs3$(rel.out)

$(x86.out)/rel_mgfs3$(rel.out): $(mgfs3.out)
	@echo [$(rel.tool)] $@
	$(rel.tool) -o $@ $(rel.flags) $(mgfs3.out)


$(x86.out)/mgfs3-inode$(cpp.out): $(mgfs3.location)/inode$(cpp.ext) $(mgfs3.dep)
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os $(cpp.flags) $(x86.flags) $(mgfs3.flags) $(mgfs3.location)/inode$(cpp.ext)


$(x86.out)/mgfs3-dentry$(cpp.out): $(mgfs3.location)/dentry$(cpp.ext) $(mgfs3.dep)
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os $(cpp.flags) $(x86.flags) $(mgfs3.flags) $(mgfs3.location)/dentry$(cpp.ext)


$(x86.out)/mgfs3-mgfs3$(cpp.out): $(mgfs3.location)/mgfs3$(cpp.ext) $(mgfs3.dep)
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os $(cpp.flags) $(x86.flags) $(mgfs3.flags) $(mgfs3.location)/mgfs3$(cpp.ext)


$(x86.out)/mgfs3-bitmap$(cpp.out): $(mgfs3.location)/bitmap$(cpp.ext) $(mgfs3.dep)
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os $(cpp.flags) $(x86.flags) $(mgfs3.flags) $(mgfs3.location)/bitmap$(cpp.ext)


$(x86.out)/mgfs3-buffer$(cpp.out): $(mgfs3.location)/buffer$(cpp.ext) $(mgfs3.dep)
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os $(cpp.flags) $(x86.flags) $(mgfs3.flags) $(mgfs3.location)/buffer$(cpp.ext)


$(x86.out)/mgfs3-file$(cpp.out): $(mgfs3.location)/file$(cpp.ext) $(mgfs3.dep)
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os $(cpp.flags) $(x86.flags) $(mgfs3.flags) $(mgfs3.location)/file$(cpp.ext)


### group: 'sync' ###
sync.location=../../../core/sync
sync.flags=
sync.dep=
sync.out= \
	$(x86.out)/sync-mutex$(cpp.out) \
	$(x86.out)/sync-mutex_api$(cpp.out) 

sync.target=$(sync.out)

$(x86.out)/sync-mutex$(cpp.out): $(sync.location)/mutex$(cpp.ext) $(sync.dep)
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os -I$(sync.location) $(cpp.flags) $(x86.flags) $(sync.flags) $(sync.location)/mutex$(cpp.ext)


$(x86.out)/sync-mutex_api$(cpp.out): $(sync.location)/mutex_api$(cpp.ext) $(sync.dep)
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os -I$(sync.location) $(cpp.flags) $(x86.flags) $(sync.flags) $(sync.location)/mutex_api$(cpp.ext)


### group: 'str' ###
str.location=../../../core/common/lib
str.flags=
str.dep=$(str.location)/str.h
str.out= \
	$(x86.out)/str-str$(cpp.out) 

str.target=$(str.out)

$(x86.out)/str-str$(cpp.out): $(str.location)/str$(cpp.ext) $(str.dep)
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os -I$(str.location) $(cpp.flags) $(x86.flags) $(str.flags) $(str.location)/str$(cpp.ext)


### group: 'repo' ###
repo.location=../../../core/sys
repo.flags=
repo.dep=$(repo.location)/repository.h
repo.out= \
	$(x86.out)/repo-repository2$(cpp.out) 

repo.target=$(repo.out)

$(x86.out)/repo-repository2$(cpp.out): $(repo.location)/repository2$(cpp.ext) $(repo.dep)
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os -I$(repo.location) $(cpp.flags) $(x86.flags) $(repo.flags) $(repo.location)/repository2$(cpp.ext)


### group: 'main' ###
main.location=.
main.flags=
main.out= \
	$(x86.out)/main-main$(cpp.out) \
	$(x86.out)/main-create$(cpp.out) \
	$(x86.out)/main-rwdm$(cpp.out) \
	$(x86.out)/main-dir$(cpp.out) \
	$(x86.out)/main-link$(cpp.out) \
	$(x86.out)/main-state$(cpp.out) \
	$(x86.out)/main-fmap$(cpp.out) 

main.target=$(x86.out)/rel_main$(rel.out)

$(x86.out)/rel_main$(rel.out): $(main.out)
	@echo [$(rel.tool)] $@
	$(rel.tool) -o $@ $(rel.flags) $(main.out)


$(x86.out)/main-main$(cpp.out): $(main.location)/main$(cpp.ext) $(main.location)/mkfs.h
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os -I$(main.location) $(cpp.flags) $(x86.flags) $(main.flags) $(main.location)/main$(cpp.ext)


$(x86.out)/main-create$(cpp.out): $(main.location)/create$(cpp.ext) $(main.location)/mkfs.h
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os -I$(main.location) $(cpp.flags) $(x86.flags) $(main.flags) $(main.location)/create$(cpp.ext)


$(x86.out)/main-rwdm$(cpp.out): $(main.location)/rwdm$(cpp.ext) $(main.location)/mkfs.h
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os -I$(main.location) $(cpp.flags) $(x86.flags) $(main.flags) $(main.location)/rwdm$(cpp.ext)


$(x86.out)/main-dir$(cpp.out): $(main.location)/dir$(cpp.ext) $(main.location)/mkfs.h
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os -I$(main.location) $(cpp.flags) $(x86.flags) $(main.flags) $(main.location)/dir$(cpp.ext)


$(x86.out)/main-link$(cpp.out): $(main.location)/link$(cpp.ext) $(main.location)/mkfs.h
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os -I$(main.location) $(cpp.flags) $(x86.flags) $(main.flags) $(main.location)/link$(cpp.ext)


$(x86.out)/main-state$(cpp.out): $(main.location)/state$(cpp.ext) $(main.location)/mkfs.h
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os -I$(main.location) $(cpp.flags) $(x86.flags) $(main.flags) $(main.location)/state$(cpp.ext)


$(x86.out)/main-fmap$(cpp.out): $(main.location)/fmap$(cpp.ext) $(main.location)/mkfs.h
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os -I$(main.location) $(cpp.flags) $(x86.flags) $(main.flags) $(main.location)/fmap$(cpp.ext)


### group: 'args' ###
args.location=../../xmake
args.flags=
args.out= \
	$(x86.out)/args-clargs$(cpp.out) 

args.target=$(args.out)

$(x86.out)/args-clargs$(cpp.out): $(args.location)/clargs$(cpp.ext) $(args.location)/clargs.h
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os -I$(args.location) $(cpp.flags) $(x86.flags) $(args.flags) $(args.location)/clargs$(cpp.ext)


### group: 'heap' ###
heap.location=../../../core/mem
heap.flags=
heap.out= \
	$(x86.out)/heap-ring_heap$(cpp.out) 

heap.target=$(heap.out)

$(x86.out)/heap-ring_heap$(cpp.out): $(heap.location)/ring_heap$(cpp.ext) 
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os -I$(heap.location) $(cpp.flags) $(x86.flags) $(heap.flags) $(heap.location)/ring_heap$(cpp.ext)


### group: 'vfs' ###
vfs.location=../../../core/driver/fs
vfs.flags=
vfs.dep=$(interface)/iFSIO.h
vfs.out= \
	$(x86.out)/vfs-vfs_buffer$(cpp.out) 

vfs.target=$(vfs.out)

$(x86.out)/vfs-vfs_buffer$(cpp.out): $(vfs.location)/vfs_buffer$(cpp.ext) $(vfs.dep)
	@echo [$(cpp.tool)] $@
	$(cpp.tool)  -Os -I$(incl) $(cpp.flags) $(x86.flags) $(vfs.flags) $(vfs.location)/vfs_buffer$(cpp.ext)

x86.all= \
	$(mgfs3.target) \
	$(sync.target) \
	$(str.target) \
	$(repo.target) \
	$(main.target) \
	$(args.target) \
	$(heap.target) \
	$(vfs.target) 

x86.target=$(x86.all)
$(branch.out)/mkmgfs3$(elf.out): $(x86.target)
	@echo [$(elf.tool)] $@
	$(elf.tool) $(x86.target) -o $@ $(lib) $(elf.flags)

$(target): $(branch.out)/mkmgfs3$(elf.out)
