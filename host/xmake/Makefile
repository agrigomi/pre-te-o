cc = g++

target = xmake

target.obj= \
      $(target).o \
      heap.o \
      mutex.o \
      mutex_api.o \
      str.o \
      ll_alg.o \
      llist.o \
      xml.o \
      repository2.o \
      clargs.o \
      process.o

cc_flags = -m64 -Os -c -pthread -I../../interface -I../../include -D_GCC_ -D_CORE_


%.o:%.cpp
	@$(cc) -o $@  $(cc_flags) $< 	
	@echo $<
	
%.o:%.c
	@$(cc) $(cc_flags) $< -o $@
	@echo $<

$(target): $(target.obj)
	@echo --- linking ---
	@$(cc) -o $(target) -pthread -lpthread -lstdc++ $(target.obj)
	@echo $(target)

clean:
	@rm  *.o
	@rm  $(target)


