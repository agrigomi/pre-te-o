cc = g++-4.6

target = test

target.obj= \
      $(target).o \
      slab.o \
      slab_heap.o \
      mutex.o \
      event.o \
      str.o \
      ll_alg.o \
      llist.o \
      env.o \
      broadcast.o \
      syslog.o \
      console.o \
      b24_pma.o \
      karg_parser.o \
      driver_root.o \
      bdctrl_host.o \
      cache.o \
      ramd.o \
      sha1.o \
      btree.o \
      volume.o \
      vfs_host.o \
      vfs_root.o \
      vfs_buffer.o \
      vfs_handle.o \
      bitmap.o \
      buffer.o \
      dentry.o \
      inode.o \
      file.o \
      mgfs3.o \
      mgfs_vfs.o \
      vfs_context.o \
      tdisp.o \
      sdhost.o \
      timer.o \
      fdd.o \
      parser64.o \
      ext_ldr.o \
      repository3.o 

cc_flags = -m64 -c -g -O0 -pthread -fpic -I../../arch/x86/interface -I../../interface -I../../include -I../../arch/x86/include -D_GCC_ -D_CORE_ -D_DEBUG_ -Wall


%.o:%.cpp
	@$(cc) -o $@  $(cc_flags) $< 	
	@echo $<
	
%.o:%.c
	@$(cc) $(cc_flags) $< -o $@
	@echo $<

$(target): $(target.obj)
	@echo --- linking ---
	@$(cc) -o $@ -pthread -lpthread -lstdc++ -rdynamic -fpic  $^
	@echo $(target)

clean:
	@rm -v *.o
	@rm -v $(target)


