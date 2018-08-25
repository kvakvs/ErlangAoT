.PHONY: run
run: compile
	target/debug/beam_aot 1> 1.ll 2> 1.ll && \
	cat 1.ll && \
	llc 1.ll && \
	llc 1.ll -filetype=obj -o 1.o

.PHONY: compile
compile:
	cargo build

