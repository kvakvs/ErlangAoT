x: mochijson.kernel.term mochijson2.kernel.term

%.kernel.term: %.erl
	./to_kern.escript $< | iconv -f ISO8859-1 -t UTF8 > $@
