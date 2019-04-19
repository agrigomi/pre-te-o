$(CONFIG):
	@echo --- build xmake ...
	cd xmake && make
	@echo --- build mkfs ...
	cd mkfs && make

clean:
	cd mkfs && make $@ 
	cd xmake && make $@
	
