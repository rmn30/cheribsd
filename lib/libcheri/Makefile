# $FreeBSD$

LIB=	cheri
SRCS=	cheri_invoke.S			\
	cheri_enter.c			\
	cheri_fd.c			\
	cheri_system.c			\
	libcheri_classes.S		\
	libcheri_stat.c			\
	sandbox.c			\
	sandbox_loader.c		\

# Temporary stuff that should be in libc or similar
SRCS+=	memcpy_c.s			\
	memcpy_c_tofrom.c		\
	strnlen_c.c			\
	strncpy_c.c

INCS=	cheri_class.h			\
	cheri_enter.h			\
	cheri_fd.h			\
	cheri_invoke.h			\
	cheri_memcpy.h			\
	cheri_system.h			\
	sandbox.h			\
	sandboxasm.h

CFLAGS+=	-I${.CURDIR}/${MACHINE_ARCH}

INCSDIR=	${INCLUDEDIR}/cheri

CFLAGS+=	-I${.CURDIR}/../libc/include \
		-I${.CURDIR}/../libc/locale

SHLIB_MAJOR=	1

MAN=	cheri_invoke.3			\
	libcheri.3

USE_CHERI=	yes

CLEANFILES=	cheri_classes.o cheri_invoke.o

NO_WERROR=

.PATH: ${.CURDIR}/${MACHINE_ARCH}
.PATH: ${.CURDIR}/../libc/string
.PATH: ${.CURDIR}/../libc/${MACHINE}/string

.include <bsd.lib.mk>
