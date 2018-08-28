OUTPUT=erlang_aot

.PHONY: run
run: compile compile-runtime
	RUST_BACKTRACE=1 target/debug/$(OUTPUT)

#	target/debug/beam_aot 1> 1.ll 2> 1.ll && \
#	cat 1.ll && \
#	llc 1.ll && \
#	llc 1.ll -filetype=obj -o 1.o

.PHONY: compile compile-runtime
compile:
	cargo build

compile-runtime:
	cd erl_runtime && cargo build

.PHONY: test
test:
	cd erl_aotc_parser && cargo test