
$(CONFIG): vbr_$(CONFIG).mk fsbr_mgfs3_$(CONFIG).mk
	@echo --- build boot loader --
	mkdir -p out
	mkdir -p out/$(CONFIG)
	make -f fsbr_mgfs3_$(CONFIG).mk fsbr_mgfs3 CONFIG=$(CONFIG)
	make -f vbr_$(CONFIG).mk vbr CONFIG=$(CONFIG)

vbr_$(CONFIG).mk: $(CONFIG).xml
	../host/xmake/xmake -V -t:vbr -o:$@ $^

fsbr_mgfs3_$(CONFIG).mk: $(CONFIG).xml
	../host/xmake/xmake -V -t:fsbr_mgfs3 -o:$@ $^

clean:
	@rm -f vbr_$(CONFIG).mk
	@rm -f fsbr_mgfs3_$(CONFIG).mk
	@rm -rf out/$(CONFIG)/*
