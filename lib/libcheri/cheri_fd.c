/*-
 * Copyright (c) 2014 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "cheri_class.h"
#include "cheri_fd.h"

/*
 * This file implements the CHERI 'file descriptor' (fd) class.  Pretty
 * minimalist.
 *
 * XXXRW: This is a slightly risky business, as we're taking capabilities as
 * arguments and casting them back to global pointers that can be passed to
 * the conventional MIPS system-call ABI.  We need to check that the access we
 * then perform on the pointer is authorised by the capability it was derived
 * from (e.g., length, permissions).
 *
 * XXXRW: Right now, no implementation of permission checking narrowing
 * file-descriptor rights, but we will add that once user permissions are
 * supported.  There's some argument we would like to have a larger permission
 * mask than supported by CHERI -- how should we handle that?
 *
 * XXXRW: This raises lots of questions about reference/memory management.
 * For now, define a 'revoke' interface that clears the fd (-1) to prevent new
 * operations from started.  However, this doesn't block waiting on old ones
 * to complete.  Differentiate 'revoke' from 'destroy', the latter of which is
 * safe only once all references have drained.  We rely on the ambient code
 * knowing when it is safe (or perhaps never calling it).
 */

CHERI_CLASS_DECL(cheri_fd);

/*
 * Data segment for a cheri_fd.  As this is a system class, we don't need to
 * explicitly store $c0.
 */
struct cheri_fd {
	int	cf_fd;		/* Underlying file descriptor. */
};

/*
 * Allocate a new cheri_fd object for an already-open file descriptor.
 */
int
cheri_fd_new(int fd, struct cheri_object *cop)
{
	__capability void *basecap;
	struct cheri_fd *cfp;

	cfp = calloc(1, sizeof(*cfp));
	if (cfp == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	cfp->cf_fd = fd;
	basecap = cheri_settype(cheri_getdefault(),
	    (register_t)CHERI_CLASS_ENTRY(cheri_fd));
	cop->co_codecap = cheri_sealcode(basecap);
	cop->co_datacap = cheri_sealdata(cheri_andperm(
	    cheri_ptr(cfp, sizeof(*cfp)), CHERI_PERM_LOAD | CHERI_PERM_STORE),
	     basecap);
	return (0);
}

/*
 * Revoke further accesses via the object -- although in-flight accesses
 * continue.  Note: does not close the fd or free memory.  The latter must
 */
void
cheri_fd_revoke(struct cheri_object co)
{
	__capability struct cheri_fd *cfp;
	__capability void *basecap;

	basecap = cheri_settype(cheri_getdefault(),
	    (register_t)CHERI_CLASS_ENTRY(cheri_fd));
	cfp = cheri_unseal(co.co_datacap, basecap);
	cfp->cf_fd = -1;
}

/*
 * Actually free a cheri_fd.  This can only be done if there are no
 * outstanding references in any sandboxes (etc).
 */
void
cheri_fd_destroy(struct cheri_object co)
{
	__capability struct cheri_fd *cfp;
	__capability void *basecap;

	basecap = cheri_settype(cheri_getdefault(),
	    (register_t)CHERI_CLASS_ENTRY(cheri_fd));
	cfp = cheri_unseal(co.co_datacap, basecap);
	assert(cfp->cf_fd == -1);
	free((void *)cfp);
}

/*
 * Forward fstat() on a cheri_fd to the underlying file descriptor.
 */
static struct cheri_fd_ret
_cheri_fd_fstat_c(__capability struct stat *sb_c)
{
	struct cheri_fd_ret ret;
	__capability struct cheri_fd *cfp;
	struct stat *sb;

	/* XXXRW: Object-capability user permission check on idc. */

	/* XXXRW: Change to check permissions directly and throw exception. */
	if (!(cheri_getperm(sb_c) & CHERI_PERM_STORE) ||
	    !(cheri_getlen(sb_c) >= sizeof(*sb))) {
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = EPROT;
		return (ret);
	}
	sb = (void *)sb_c;

	/* Check that the cheri_fd hasn't been revoked. */
	cfp = cheri_getidc();
	if (cfp->cf_fd == -1) {
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = EBADF;
		return (ret);
	}

	/* Forward to operating system. */
	ret.cfr_retval0 = fstat(cfp->cf_fd, sb);
	ret.cfr_retval1 = (ret.cfr_retval0 < 0 ? errno : 0);
	return (ret);
}

/*
 * Forward lseek() on a cheri_fd to the underlying file descriptor.
 */
static struct cheri_fd_ret
_cheri_fd_lseek_c(off_t offset, int whence)
{
	struct cheri_fd_ret ret;
	__capability struct cheri_fd *cfp;

	/* XXXRW: Object-capability user permission check on idc. */

	/* Check that the cheri_fd hasn't been revoked. */
	cfp = cheri_getidc();
	if (cfp->cf_fd == -1) {
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = EBADF;
		return (ret);
	}

	/* Forward to operating system. */
	ret.cfr_retval0 = lseek(cfp->cf_fd, offset, whence);
	ret.cfr_retval1 = (ret.cfr_retval0 < 0 ? errno : 0);
	return (ret);
}

/*
 * Forward read_c() on a cheri_fd to the underlying file descriptor.
 */
static struct cheri_fd_ret
_cheri_fd_read_c(__capability void *buf_c)
{
	struct cheri_fd_ret ret;
	__capability struct cheri_fd *cfp;
	void *buf;

	/* XXXRW: Object-capability user permission check on idc. */

	/* XXXRW: Change to check permissions directly and throw exception. */
	if (!(cheri_getperm(buf_c) & CHERI_PERM_STORE)) {
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = EPROT;
		return (ret);
	}
	buf = (void *)buf_c;

	/* Check that the cheri_fd hasn't been revoked. */
	cfp = cheri_getidc();
	if (cfp->cf_fd == -1) {
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = EBADF;
		return (ret);
	}

	/* Forward to operating system. */
	ret.cfr_retval0 = read(cfp->cf_fd, buf, cheri_getlen(buf_c));
	ret.cfr_retval1 = (ret.cfr_retval0 < 0 ? errno : 0);
	return (ret);
}

/*
 * Forward write_c() on a cheri_fd to the underlying file descriptor.
 */
static struct cheri_fd_ret
_cheri_fd_write_c(__capability void *buf_c)
{
	struct cheri_fd_ret ret;
	__capability struct cheri_fd *cfp;
	void *buf;

	/* XXXRW: Object-capability user permission check on idc. */

	/* XXXRW: Change to check permissions directly and throw exception. */
	if (!(cheri_getperm(buf_c) & CHERI_PERM_LOAD)) {
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = EPROT;
		return (ret);
	}
	buf = (void *)buf_c;

	/* Check that cheri_fd hasn't been revoked. */
	cfp = cheri_getidc();
	if (cfp->cf_fd == -1) {
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = EBADF;
		return (ret);
	}

	/* Forward to operating system. */
	ret.cfr_retval0 = write(cfp->cf_fd, buf, cheri_getlen(buf_c));
	ret.cfr_retval1 = (ret.cfr_retval0 < 0 ? errno : 0);
	return (ret);
}

/*
 * XXXRW: It would be nice if CHERI_CLASS_ASM() could actually be C and we
 * could keep this symbol local to the current object rather than making it
 * global.
 *
 * XXXRW: temporarily replaced __unused struct cheri_object co with capability
 * pointers to try to avoid a compiler bug.
 */
struct cheri_fd_ret	cheri_fd_enter(register_t methodnum, register_t a1,
			    register_t a2, struct cheri_object co,
			    __capability void *c3)
			    __attribute__((cheri_ccall));

struct cheri_fd_ret
cheri_fd_enter(register_t methodnum, register_t a1, register_t a2,
    struct cheri_object co __unused, __capability void *c3)
{
	struct cheri_fd_ret ret;

	switch (methodnum) {
	case CHERI_FD_METHOD_FSTAT_C:
		return (_cheri_fd_fstat_c(c3));

	case CHERI_FD_METHOD_LSEEK_C:
		return (_cheri_fd_lseek_c(a1, a2));

	case CHERI_FD_METHOD_READ_C:
		return (_cheri_fd_read_c(c3));

	case CHERI_FD_METHOD_WRITE_C:
		return (_cheri_fd_write_c(c3));

	default:
		ret.cfr_retval0 = -1;
		ret.cfr_retval1 = ENOMETHOD;
		return (ret);
	}
}
