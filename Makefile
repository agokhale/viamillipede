
BINDIR= /usr/local/bin
MANDIR= /usr/local/man/man
MK_DEBUG_FILES= no
SHAREDIR= ""

PROG= viamillipede
SRCS= plumbing.c tx.c rx.c viamillipede.c dtrace_viamillipede.d terminate.c
# This LDADD knob can't possibly be intended to use this way
LDADD= -lpthread
.include <bsd.prog.mk>
