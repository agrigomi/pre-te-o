out=./out
target=mkmgfs3
xmake=../../xmake/xmake
config=config.xml
branch=x86

$(target): $(target).mk
	@make -f  $^ $@
	
$(target).mk: $(config)
	@$(xmake) -V -t:$(target) -b:$(branch) -o:$@ $^
	
clean:
	@rm -f $(out)/$(target)
	@rm -f $(out)/*.o
