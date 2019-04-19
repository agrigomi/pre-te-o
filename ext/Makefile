$(CONFIG):
	cd lib && make CONFIG=$@ BRANCH=$(BRANCH)
	cd init && make CONFIG=$@ BRANCH=$(BRANCH)
	cd test && make CONFIG=$@ BRANCH=$(BRANCH)

clean:
	cd init && make $@ CONFIG=$(CONFIG)
	cd test && make $@ CONFIG=$(CONFIG)
	cd lib && make $@ CONFIG=$(CONFIG)

