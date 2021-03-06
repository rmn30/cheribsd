/*-
 * Copyright (c) 2012-2013 Robert N. M. Watson
 * Copyright (c) 2012-2013 Michael Roe
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

/* XXXRW: Can I drop some of these? */
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>
#include <machine/cpuregs.h>
#include <machine/pte.h>

#ifdef CPU_CHERI
#include <machine/cheriasm.h>
#include <machine/cherireg.h>
#endif

#include "assym.s"

/*
 * Software implementations of CCall, CReturn handlers for CHERI.
 *
 * The low-level CHERICCallVector exception handler, which has been relocated
 * to the MIPS exception vector table, jumps to either CHERICCall or
 * CHERICReturn running in the normal kernel address space.
 *
 * Notice that 'j' is used, implying that the kernel is in the 32-bit kernel
 * segment so that the target fits in the available immediate -- this is also
 * true of other FreeBSD exception handlers.
 */

	.set noreorder	/* Preserve nops, allow instructions in b-d slots. */

/*
 * CCall/CReturn low-level exception handler; this code must be position-
 * independent, as it will be relocated into the vector table.
 */
VECTOR(CHERICCallVector, unknown)
        .set push
        .set noat
        CHERI_EXCEPTION_ENTER(k0)

	/*
	 * Determine whether this is a CCall or CReturn instruction.
	 *
	 * XXXRW: Panic if CGetCause returns something other than CALL/RETURN.
	 *
	 * XXXRW: Panic if not entering from userspace.
	 *
	 * XXXRW: Should we be clearing $k0 and $k1 before returning to
	 * userspace?  Should other exception handlers be doing it?
	 */
	CGetCause	k1
	REG_SRL		k1, 8
	andi		k1, k1, 0x1	/* CALL is odd; RETURN is even. */
	beqz		k1, CReturn_label
	nop		/* Branch-delay slot. */

	j		CHERICCall
	nop		/* Branch-delay slot. */

CReturn_label:
	j		CHERICReturn
	nop		/* Branch-delay slot. */

        .set pop
VECTOR_END(CHERICCallVector)

/*
 * Software implementation of CCall; this code does not need to be position-
 * independent as it is not relocated to an exception vector.
 *
 * XXXRW: This software implementation of CCall uses an alternative calling
 * convention, in which the code capability to invoke is always placed in $c1,
 * and the corresponding data capability is always placed in $c2.  This
 * prevents software from having to copy in the faulting ccall instruction,
 * decoding it, and then switch()ing on the register numbers to use the right
 * instruction to extract the capabilities to fixed ones targetable in
 * assembly and by the compiler.
 *
 * NB: No attempt to make this pipeline well yet -- branch-delay slots not
 * well-utilised, some CP2 fields accessed multiple times.
 */
CHERICCall:
        .set push
        .set noat

	/* First, test argument registers for tag validity. */
	cbtu		CHERI_REG_CCALLCODE, CCall_c1_invalid
	nop
	cbtu		CHERI_REG_CCALLDATA, CCall_c2_invalid
	nop

	/* Second, check for the sealed bit on both arguments. */
	cgetunsealed	k0, CHERI_REG_CCALLCODE
	bnez		k0, CCall_c1_unsealed
	nop

	cgetunsealed	k0, CHERI_REG_CCALLDATA
	bnez		k0, CCall_c2_unsealed
	nop

	/* Third, check for type equality. */
	cgettype	k0, CHERI_REG_CCALLCODE
	cgettype	k1, CHERI_REG_CCALLDATA
	bne		k0, k1, CCall_c1_c2_type_mismatch
	nop

	/* Fourth, check permissions. */
	cgetperm	k0, CHERI_REG_CCALLCODE
	REG_LI		k1, CHERI_PERM_SEAL
	and		k0, k0, k1
	bne		k0, k1, CCall_c1_perm_seal
	nop

	cgetperm	k0, CHERI_REG_CCALLCODE
	REG_LI		k1, CHERI_PERM_EXECUTE
	and		k0, k0, k1
	bne		k0, k1, CCall_c1_perm_execute
	nop

	/* Fifth, check proposed PC is not lower than base. */
	cgetbase	k0, CHERI_REG_CCALLCODE
	cgettype	k1, CHERI_REG_CCALLCODE
	sltu		k1, k1, k0
	bnez		k1, CCall_c1_range
	nop

	/*
	 * Sixth, check proposed PC is not greater than base + length - 4.
	 *
	 * XXXRW: CHERI ISA spec calls for '-1'; we use '4' as it is the
	 * length of an instruction.
	 *
	 * XXXRW: Check this logic.
	 */
	cgetbase	k0, CHERI_REG_CCALLCODE
	cgetlen		k1, CHERI_REG_CCALLCODE
	PTR_ADDU	k0, k0, k1
	PTR_SUBIU	k0, 4
	cgettype	k1, CHERI_REG_CCALLCODE
	sltu		k1, k0, k1
	bnez		k1, CCall_c1_range
	nop

	/*
	 * Now prepare to push IDC, PCC, PC+4 onto the trusted stack.  Begin
	 * by retrieving the current PCB pointer to reach the trusted stack.
	 */
	GET_CPU_PCPU(k1)   
	PTR_L		k1, PC_CURPCB(k1)

	/* Retrieve current trusted stack pointer. */
	PTR_L		k0, U_PCB_CHERISTACK_TSP(k1)

	/* If at bottom (byte offset 0), then overflow. */
	beqz		k0, CCall_stack_overflow
	nop

	/* Decrement trusted stack pointer. */
	PTR_SUBIU	k0, k0, CHERI_FRAME_SIZE

	/* Write back trusted stack pointer. */
	PTR_S		k0, U_PCB_CHERISTACK_TSP(k1)

	/* Convert trusted stack-relative offset to global pointer. */
	PTR_ADDU	k0, k1, k0			/* Add PCB pointer. */
	PTR_ADDIU	k0, k0, U_PCB_CHERISTACK_FRAMES	/* Add PCB offset. */

	/* Push IDC. */
	csc		CHERI_REG_IDC, k0, CHERI_STACKFRAME_IDC(CHERI_REG_KDC)

	/* Push PCC. */
	csc		CHERI_REG_EPCC, k0, CHERI_STACKFRAME_PCC(CHERI_REG_KDC)

	/*
	 * Push SP; zero register afterwards since the caller is unable to do
	 * that before CCall.
	 *
	 * XXXRW: Not called for by CHERI spec.
	 */
	csd		sp, k0, CHERI_STACKFRAME_SP(CHERI_REG_KDC)
	move		sp, zero

	/* Push PC + 4; k1 is overwritten, so no longer PCB pointer. */
	MFC0		k1, MIPS_COP_0_EXC_PC
	PTR_ADDU	k1, k1, 4
	csd		k1, k0, CHERI_STACKFRAME_PC(CHERI_REG_KDC)

	/*
	 * Temporarily set KDC type to allow unsealing.
	 *
	 * XXXRW: This is inefficient and (should be) unnecessary.
	 */
	cgettype	k0, CHERI_REG_CCALLCODE
	csettype	CHERI_REG_KDC, CHERI_REG_KDC, k0

	/* Unseal cs; install in PCC. */
	cunseal		CHERI_REG_EPCC, CHERI_REG_CCALLCODE, CHERI_REG_KDC

	/* Unseal cb; install in IDC. */
	cunseal		CHERI_REG_IDC, CHERI_REG_CCALLDATA, CHERI_REG_KDC

	/* Installe cs.otype. */
	cgettype	k0, CHERI_REG_CCALLCODE
	cgetbase	k1, CHERI_REG_CCALLCODE
	dsub		k0, k0, k1
	MTC0		k0, MIPS_COP_0_EXC_PC
	COP0_SYNC

	/*
	 * Restore KDC type.
	 *
	 * XXXRW: This should also be unnecessary.
	 */
	PTR_LI		k0, 0
	csettype	CHERI_REG_KDC, CHERI_REG_KDC, k0

	CHERI_EXCEPTION_RETURN(k0)
	eret

CCall_c1_invalid:
	dli	k0, ((CHERI_EXCCODE_TAG << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_c2_invalid:
	dli	k0, ((CHERI_EXCCODE_TAG << 8) | 2)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_c1_unsealed:
	dli	k0, ((CHERI_EXCCODE_SEAL << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_c2_unsealed:
	dli	k0, ((CHERI_EXCCODE_SEAL << 8) | 2)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_c1_c2_type_mismatch:
	dli	k0, ((CHERI_EXCCODE_TYPE << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_c1_perm_seal:
	dli	k0, ((CHERI_EXCCODE_PERM_SEAL << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_c1_perm_execute:
	dli	k0, ((CHERI_EXCCODE_PERM_EXECUTE << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_c1_range:
	dli	k0, ((CHERI_EXCCODE_LENGTH << 8) | 1)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_stack_overflow:
	dli	k0, ((CHERI_EXCCODE_CALL << 8) | 0)
	b	CCall_throw_exception
	csetcause	k0		/* Branch-delay slot. */

CCall_throw_exception:
	j	_C_LABEL(MipsUserGenException)
	nop

        .set pop

/*
 * Software implementation of CReturn; this code does not need to be position-
 * independent as it is not relocated to an exception vector.
 *
 * Possible failure modes:
 *
 * 1. Trusted stack underflow.
 */
CHERICReturn:
        .set push
        .set noat

	/* Retrieve current PCB pointer. */
	GET_CPU_PCPU(k1)   
	PTR_L		k1, PC_CURPCB(k1)

	/*
	 * The only currently defined check in CReturn is stack underflow;
	 * perform that check.
	 */
	PTR_L		k0, U_PCB_CHERISTACK_TSP(k1)
	sltiu		k0, CHERI_STACK_SIZE
	beqz		k0, CReturn_stack_underflow
	nop

	/* Reload trusted stack pointer. */
	PTR_L		k0, U_PCB_CHERISTACK_TSP(k1)
	
	/* Convert trusted stack-relative offset to global pointer. */
	PTR_ADDU	k0, k1, k0			/* Add PCB pointer. */
	PTR_ADDIU	k0, k0, U_PCB_CHERISTACK_FRAMES	/* Add PCB offset. */

	/* Pop IDC. */
	clc		CHERI_REG_IDC, k0, CHERI_STACKFRAME_IDC(CHERI_REG_KDC)

	/* Pop PCC. */
	clc		CHERI_REG_EPCC, k0, CHERI_STACKFRAME_PCC(CHERI_REG_KDC)

	/*
	 * Pop SP.
	 *
	 * XXXRW: Not called for by CHERI spec.
	 */
	cld		sp, k0, CHERI_STACKFRAME_SP(CHERI_REG_KDC)

	/* Pop PC + padding; +4 already done; toasts k0; k1 still PCB. */
	cld		k0, k0, CHERI_STACKFRAME_PC(CHERI_REG_KDC)
	MTC0		k0, MIPS_COP_0_EXC_PC
	COP0_SYNC

	/* Update trusted stack pointer. */
	PTR_L		k0, U_PCB_CHERISTACK_TSP(k1)
	PTR_ADDIU	k0, CHERI_FRAME_SIZE
	PTR_S		k0, U_PCB_CHERISTACK_TSP(k1)

	CHERI_EXCEPTION_RETURN(k0)
	eret

CReturn_stack_underflow:
	dli	k0, ((CHERI_EXCCODE_RETURN << 8) | 0)
	csetcause	k0
	j	_C_LABEL(MipsUserGenException)
	nop

	.set pop
