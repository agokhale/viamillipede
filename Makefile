VMPDTRACE= DTRACE #conditional compilation for dtrace
BINDIR= /usr/local/bin
MANDIR= /usr/local/man/man
MK_DEBUG_FILES= no
SHAREDIR= ""
PROG= viamillipede
SRCS= plumbing.c tx.c rx.c viamillipede.c terminate.c prbs.c
.ifdef VMPDTRACE
  SRCS+= dtrace_viamillipede.d
  CFLAGS += -DVMPDTRACE
.endif
CFLAGS+= -g
# This LDADD knob can't possibly be intended to use this way
LDADD= -lpthread
.include <bsd.prog.mk>
.include <bsd.clang-analyze.mk>
