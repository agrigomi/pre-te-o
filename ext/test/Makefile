config_dir = .
out_dir = ./out/$(CONFIG)
xmake = ../../host/xmake/xmake
target = test

$(CONFIG): $(out_dir) $(CONFIG).mk
	@make -f $(CONFIG).mk $(target) CONFIG=$(CONFIG) BRANCH=$(BRANCH)

$(CONFIG).mk: $(config_dir)/$(CONFIG).xml
	@rm -Rfv $(out_dir)/*
	@$(xmake) -V -t:$(target) -b:$(BRANCH) -o:$@ $^

$(out_dir):
	mkdir $(out_dir)

clean:
	@rm -Rf $(out_dir)/*
	@rm -f *.mk
