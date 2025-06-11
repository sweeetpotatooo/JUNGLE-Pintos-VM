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
	pintos -v -k -T 600 -m 20   --fs-disk=10 -p tests/vm/page-merge-seq:page-merge-seq -p tests/vm/child-sort:child-sort --swap-disk=4 -- -q   -f run page-merge-seq

.PHONY: zeros
zeros:
	make clean && \
	make -j && \
	cd ./build && \
	dd if=/dev/zero of=tests/vm/zeros bs=1024 count=6 && \
	pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/vm/mmap-overlap:mmap-overlap -p tests/vm/zeros:zeros --swap-disk=4 -- -q   -f run mmap-overlap
