all:
	cc -g  -o viamillipede -lpthread  plumbing.c rx.c tx.c viamillipede.c
clean: 
	rm -f viamillipede viamillipede.core

test: clean all
	sh  test.sh
