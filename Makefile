#all:
#	cc -g  -o viamillipede -lpthread  plumbing.c rx.c tx.c viamillipede.c
#clean: 
#	rm -f viamillipede viamillipede.core
#
#test: clean all
#	sh  test.sh

#.include <src.opts.mk>
PROG= viamillipede
SRCS= plumbing.c tx.c rx.c viamillipede.c
# This LDADD knob can't possibly be intended to use this way
LDADD= -lpthread
.include <bsd.prog.mk>