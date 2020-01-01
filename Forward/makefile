all:	errhand bitio main-c Forward build
errhand:
	gcc errhand.c -c -o errhand.o
bitio:
	gcc bitio.c -c -o bitio.o
main-c:
	gcc main-c.c -c -o main.o
main-e:
	gcc main-e.c -c -o main.o
Forward:
	gcc Forward.c -c -o Forward.o
build:
	gcc bitio.o errhand.o main.o Forward.o -o run_forward_c.o -lm
run:
	./run.o
clean:
	rm -f *.o
