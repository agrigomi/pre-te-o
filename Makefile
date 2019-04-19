$(CONFIG):
	cd host && make CONFIG=$@ BRANCH=$(BRANCH)
	cd boot2 && make CONFIG=$@
	cd core && make CONFIG=$@ BRANCH=$(BRANCH)
	cd ext && make CONFIG=$@ BRANCH=$(BRANCH)
	host/mkfs/mgfs3/mgfs3 create -d:pre-te-o.img -s:2 -u:1 -f:pre-te-o.fmap -V -n:TEST-BOOT
clean:
	rm -f pre-te-o.img
	cd host && make  $@
	cd ./core && make $@ CONFIG=$(CONFIG)
	cd ./boot2 && make $@ CONFIG=$(CONFIG)
	cd ext && make $@ CONFIG=$(CONFIG)
