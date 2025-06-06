include ../Makefile.kernel
.PHONY: vm
vm:
	make clean && \
	make -j && \
	cd ./build && \
	make check 2>&1 | tee ../log.txt


.PHONY: debug
debug:
	make clean && \
	make -j && \
	cd ./build && \
	pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/vm/mmap-unmap:mmap-unmap -p ../../tests/vm/sample.txt:sample.txt --swap-disk=4 -- -q   -f run mmap-unmap
	