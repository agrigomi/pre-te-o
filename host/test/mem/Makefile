cc = g++-4.6

target = mem_test

target.obj= \
      main.o \
      slab.o \
      slab_heap.o \
      mutex.o \
      mutex_api.o \
      str.o \
      ll_alg.o \
      llist.o \
      env.o \
      syslog.o \
      driver_root.o \
      console.o \
      b24_pma.o \
      tdisp.o \
      sdhost.o \
      repository3.o 

cc_flags = -m64 -c -g -O0 -pthread -fpic -I../../../arch/x86/interface -I../../../interface -I../../../include -I../../../arch/x86/include -D_GCC_ -D_CORE_ -D_DEBUG_ -Wall


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


