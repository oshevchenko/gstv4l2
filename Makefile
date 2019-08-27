all: process-vmalloc

# $@ matches the target; $< matches the first dependent
process-vmalloc: process-vmalloc.o
	gcc -o $@ $<

process-vmalloc.o: process-vmalloc.c
	gcc -c $<

clean:
	rm process-vmalloc.o process-vmalloc

