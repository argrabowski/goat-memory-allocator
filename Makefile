all: test_goatmalloc

test_goatmalloc: test_goatmalloc.c
	gcc test_goatmalloc.c goatmalloc.c -o test_goatmalloc

clean:
	rm -f test_goatmalloc
