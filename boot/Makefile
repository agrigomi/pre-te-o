config_dir = ./config/$(CONFIG)
out_dir = ./out/$(CONFIG)

$(CONFIG):$(out_dir)
	make -f $(config_dir)/$(CONFIG).mk OUT_DIR=$(out_dir) FILESYSTEM=$(FILESYSTEM) all

$(out_dir):
	mkdir $(out_dir)

clean:
#	cd $(mkfs) && make -i $@
	@make -sf $(config_dir)/$(CONFIG).mk OUT_DIR=$(out_dir) $@
	@rmdir $(out_dir)
