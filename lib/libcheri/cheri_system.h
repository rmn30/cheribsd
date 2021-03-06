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

#ifndef _CHERI_SYSTEM_H_
#define	_CHERI_SYSTEM_H_

/*
 * This header defines the interface for the CHERI system class.  Currently,
 * it is a bit catch-all, and provides a few key service that make it easy to
 * implement (and debug) sandboxed code.  In the future, we anticipate the
 * system class being an entry point to a number of other classes -- e.g.,
 * providing an open() method that returns file-descriptor objects.  We are
 * definitely not yet at that point.
 */

#define	CHERI_SYSTEM_METHOD_HELLOWORLD	1	/* printf("hello world\n"); */
#define	CHERI_SYSTEM_METHOD_PUTS	2	/* puts(). */
#define	CHERI_SYSTEM_METHOD_PUTCHAR	3	/* putchar(). */

/*
 * In the sandbox: notify the stub implementation of the object capability to
 * invoke.
 */
void	cheri_system_setup(struct cheri_object system_object);

/*
 * Methods themselves.
 */
int	cheri_system_helloworld(void);
int	cheri_system_puts(__capability const char *str);
int	cheri_system_putchar(int c);

/*
 * Method numbers, which can be modified to override the bottom layer of the
 * system class stub to invoke alternative methods.
 */
extern register_t cheri_system_methodnum_helloworld;
extern register_t cheri_system_methodnum_puts;
extern register_t cheri_system_methodnum_putchar;

/*
 * XXXRW: This API probably doesn't belong here.  But where does it belong?
 */
__capability struct sandbox	*cheri_getsandbox(void);

#endif /* !_CHERI_SYSTEM_H_ */
