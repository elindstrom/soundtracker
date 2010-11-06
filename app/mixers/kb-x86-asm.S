
/*
 * The Real SoundTracker - Cubically interpolating mixing routines
 *                         with IT style filter support
 *
 *                         Highly optimized i386 version.
 *
 * Copyright (C) 1999-2000 Tammo Hinrichs
 * Copyright (C) 2000 Michael Krause
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _C_LABEL
# ifdef __ELF__
#  define _C_LABEL(x) x
# else
#  define _C_LABEL(x) _ ## x
# endif
#endif
#undef GLOBAL
#define GLOBAL(x)   .globl _C_LABEL(x); _C_LABEL(x):

#if defined(__i386__)

.section	.data
		.align 4

_volrl:		.long 0
_volrr:		.long 0
_voll:		.long 0
_volr:		.long 0
magic1:		.long 0
ebpstore:	.long 0
store2:		.long 0
scopebuf:	.long 0
	
ffreq:		.float 0.0
freso:		.float 0.0
fl1:		.float 0.0
fb1:		.float 0.0
				
.section	.rodata
		.align 4
minampl:	.float 0.0001
cremoveconst:	.float 0.992
clampmax:	.float 32767.0
clampmin:	.float -32767.0
minuseins:	.float -1.0
			
.text

 GLOBAL(kbasm_post_mixing)
	pushl	%ebp
	movl	%esp, %ebp

	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%esi
	pushl	%edi
	
	finit

	movl	8(%ebp),%esi
	movl	12(%ebp),%edi
	movl	16(%ebp),%ecx
	addl	%ecx,%ecx	// stereo
	flds	20(%ebp)	// (amp)
	
	call	clipstereo

	popl	%edi
	popl	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx

	leave
	ret
	
clipstereo:	// convert/clip samples, 16bit signed
	flds	clampmin	// (min) (amp)
	flds	clampmax	// (max) (min) (amp)
	movw	$32767,%bx
	movw	$-32767,%dx
	movl	%ebp,ebpstore
	xorl	%ebp,%ebp
		
.lp:
	flds    (%esi)		// (ls) (max) (min) (amp)
	fmul	%st(3)
	fcom	%st(1)
	fnstsw	%ax
	sahf
	ja	.max
	fcom	%st(2)
	fstsw	%ax
	sahf
	jb	.min
	fistp	(%edi)		// (max) (min) (amp)
.next:
	addl	$4,%esi
	addl	$2,%edi
	decl	%ecx
	jnz	.lp
	jmp	.ende

.max:
	fstp	%st		// (max) (min) (amp)
	movw    %bx,(%edi)
	incl	%ebp
	jmp	.next
.min:
	fstp	%st		// (max) (min) (amp)
	movw	%dx,(%edi)
	incl	%ebp
	jmp	.next

.ende:
	movl	%ebp,%eax
	movl	ebpstore,%ebp
	fstp	%st		// (min) (amp)
	fstp	%st		// (amp)
	fstp	%st		// -
	ret

 GLOBAL(kbasm_mix)
	pushl	%ebp
	movl	%esp, %ebp

	pushl	%eax
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%esi
	pushl	%edi
	
	finit

	movl	%ebp,ebpstore
	movl	8(%ebp),%ebp

	// Put data into appropriate places for the real mixer subroutine
	movl	(%ebp),%eax
	movl	4(%ebp),%ebx
	movl	%eax,_voll
	movl	%ebx,_volr
	movl	8(%ebp),%eax
	movl	12(%ebp),%ebx
	movl	%eax,_volrl
	movl	%ebx,_volrr

	movl	44(%ebp),%eax
	movl	%eax,ffreq
	movl	48(%ebp),%eax
	movl	%eax,freso
	movl	52(%ebp),%eax
	movl	%eax,fl1
	movl	56(%ebp),%eax
	movl	%eax,fb1
	movl	60(%ebp),%ecx
	movl	%ecx,scopebuf
	movl	24(%ebp),%ebx // freqi
	movl	28(%ebp),%esi // freqf
	movl	16(%ebp),%eax // pointer to sample data
	movl	20(%ebp),%edx // sample position (fractional part)
	movl	32(%ebp),%edi // destination float buffer
	movl	36(%ebp),%ecx // number of samples to mix

	movl	64(%ebp),%ebp
	addl	$kbasm_mixers,%ebp
	movl	(%ebp),%ebp
	call	*%ebp
	
	// Update changed values
	movl	ebpstore,%ebp
	movl	8(%ebp),%ebp

	movl	%eax,16(%ebp) // pointer to sample data
	movl	%edx,20(%ebp) // sample position (fractional part)
	movl	%edi,32(%ebp) // destination float buffer
	movl	fl1,%eax
	movl	%eax,52(%ebp)
	movl	fb1,%eax
	movl	%eax,56(%ebp)
	
	movl	_voll,%eax
	movl	_volr,%ebx
	movl	%eax,(%ebp)
	movl	%ebx,4(%ebp)

	movl	ebpstore,%ebp

	popl	%edi
	popl	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%eax

	leave
	ret

	.MACRO	CUBICMIXER SCOPES=0, FILTERED=0, BACKWARDS=0, VOLRAMP
	flds	_voll			/* (vl) */
	flds	_volr			/* (vr) (vl) */
	movl	%eax,%ebp
	shrl	$1,%ebp
	movl	%edx,%eax
	.if	\BACKWARDS 
	negl	%eax
	.endif
	shrl	$24,%eax

	.if	\SCOPES 
	movl	%esp,store2
	movl	scopebuf,%esp
	.endif

	.p2align 4,,7
cubicmixer\@:
	fild	(,%ebp,2)		/* (w0) (vl) */
	fmuls	kb_x86_ct0(,%eax,4)	/* (w0') (vl) */
	.ifeq	\BACKWARDS 
	fild	2(,%ebp,2)		/* (w1) (w0') (vl) */
	fmuls	kb_x86_ct1(,%eax,4)	/* (w1') (w0') (vl) */
	fild	4(,%ebp,2)		/* (w2) (w1') (w0') (vl) */
	fmuls	kb_x86_ct2(,%eax,4)	/* (w2') (w1') (w0') (vl) */
	fild	6(,%ebp,2)		/* (w3) (w2') (w1') (w0') (vl) */
	.else
	fild	-2(,%ebp,2)		/* (w1) (w0') (vl) */
	fmuls	kb_x86_ct1(,%eax,4)	/* (w1') (w0') (vl) */
	fild	-4(,%ebp,2)		/* (w2) (w1') (w0') (vl) */
	fmuls	kb_x86_ct2(,%eax,4)	/* (w2') (w1') (w0') (vl) */
	fild	-6(,%ebp,2)		/* (w3) (w2') (w1') (w0') (vl) */
	.endif
	fmuls	kb_x86_ct3(,%eax,4)	/* (w3') (w2') (w1') (w0') (vl) */
	fxch	%st(2)			/* (w1') (w2') (w3') (w0') (vr) (vl) */
	faddp	%st,%st(3)		/* (w2') (w3') (w0+w1) (vr) (vl) */
	addl	%esi,%edx
	lea	8(%edi),%edi
	faddp	%st,%st(2)		/* (w2+w3) (w0+w1) (vr) (vl) */
	adcl	%ebx,%ebp
	movl	%edx,%eax
	faddp	%st,%st(1)		/* (val) (vr) (vl) */

	.if	\BACKWARDS 
	negl	%eax
	.endif

	.if	\FILTERED 
	/* IT Filter Code */
	/* b=reso*b+freq*(in-l); */
	/* l+=freq*b; */
	fsubs	fl1			/* (in-l) .. */
	fmuls	ffreq			/* (f*(in-l)) .. */
	flds	fb1			/* (b) (f*(in-l)) .. */
	fmuls	freso			/* (r*b) (f*(in-l)) .. */
	faddp	%st,%st(1)		/* (b') .. */
	fsts	fb1
	fmuls	ffreq			/* (f*b') .. */
	fadds	fl1			/* (l') .. */
	fsts	fl1			/* (val) (vr) (vl) */
	.endif

	shrl	$24,%eax

	.if	\SCOPES 
	/* Scopes */
	fld	%st(1)                  /* (vr) (val) (vr) (vl) */
	fadd	%st(3)			/* (vr + vl) (val) (vr) (vl) */
	fmul	%st(1),%st		/* (val * (vr + vl)) (val) (vr) (vl) */
	fistp	(%esp)			/* (val) (vr) (vl) */
	lea	2(%esp),%esp
	.endif

	.if	\VOLRAMP 
	fld	%st(1)			/* (vr) (val) (vr) (vl) */
	fld	%st(3)			/* (vl) (vr) (val) (vr) (vl) */
	fmul	%st(2),%st		/* (left) (vr) (val) (vr) (vl) */
	fxch	%st(4)			/* (vl)  (vr) (val) (vr) (left) */
	fadds	_volrl			/* (vl') (vr) (val) (vr) (left) */
	fxch	%st(2)			/* (val) (vr) (vl') (vr) (left) */
	fmulp	%st(1)			/* (right) (vl') (vr) (left) */
	fxch	%st(2)			/* (vr) (vl') (right) (left) */
	fadds	_volrr			/* (vr') (vl') (right) (left) */
	fxch	%st(3)			/* (left)  (vl') (right) (vr') */
	fadds	-8(%edi)		/* (lfinal) (vl') (right) (vr') */
	fxch	%st(2)			/* (right) (vl') (lfinal) (vr') */
	fadds	-4(%edi)		/* (rfinal) (vl') (lfinal) (vr') */
	fstps	-4(%edi)		/* (vl') (lfinal) (vr') */
	fxch	%st(1)			/* (lfinal) (vl) (vr) */
	fstps	-8(%edi)		/* (vl) (vr) */
	fxch	%st(1)			/* (vr) (vl) */
	.else
	fld	%st(2)			/* (vl) (val) (vr) (vl) */
	fmul	%st(1),%st		/* (left) (val) (vr) (vl) */
	fadds	-8(%edi)		/* (lfinal) (val) (vr) (vl) */
	fxch	%st(1)			/* (val) (lfinal) (vr) (vl) */
	fmul	%st(2),%st		/* (right) (lfinal) (vr) (vl) */
	fadds	-4(%edi)		/* (rfinal) (lfinal) (vr) (vl) */
	fstps	-4(%edi)		/* (lfinal) (vr) (vl) */
	fstps	-8(%edi)		/* (vr) (vl) */
	.endif
		
	decl	%ecx
	jnz	cubicmixer\@

	.if	\VOLRAMP 
	fstps	_volr			/* (vl) */
	fstps	_voll			/* - */
	.endif
	shll	$1,%ebp
	movl	%ebp,%eax

	.if	\SCOPES 
	movl	store2,%esp
	.endif

	ret
	.ENDM

kbasm_mix_cubic_noscopes_unfiltered_forward_noramp:
	CUBICMIXER 0, 0, 0, 0
kbasm_mix_cubic_noscopes_unfiltered_backward_noramp:
	CUBICMIXER 0, 0, 1, 0
kbasm_mix_cubic_noscopes_filtered_forward_noramp:
	CUBICMIXER 0, 1, 0, 0
kbasm_mix_cubic_noscopes_filtered_backward_noramp:
	CUBICMIXER 0, 1, 1, 0
kbasm_mix_cubic_scopes_unfiltered_forward_noramp:
	CUBICMIXER 1, 0, 0, 0
kbasm_mix_cubic_scopes_unfiltered_backward_noramp:
	CUBICMIXER 1, 0, 1, 0
kbasm_mix_cubic_scopes_filtered_forward_noramp:
	CUBICMIXER 1, 1, 0, 0
kbasm_mix_cubic_scopes_filtered_backward_noramp:
	CUBICMIXER 1, 1, 1, 0
kbasm_mix_cubic_noscopes_unfiltered_forward:
	CUBICMIXER 0, 0, 0, 1
kbasm_mix_cubic_noscopes_unfiltered_backward:
	CUBICMIXER 0, 0, 1, 1
kbasm_mix_cubic_noscopes_filtered_forward:
	CUBICMIXER 0, 1, 0, 1
kbasm_mix_cubic_noscopes_filtered_backward:
	CUBICMIXER 0, 1, 1, 1
kbasm_mix_cubic_scopes_unfiltered_forward:
	CUBICMIXER 1, 0, 0, 1
kbasm_mix_cubic_scopes_unfiltered_backward:
	CUBICMIXER 1, 0, 1, 1
kbasm_mix_cubic_scopes_filtered_forward:
	CUBICMIXER 1, 1, 0, 1
kbasm_mix_cubic_scopes_filtered_backward:
	CUBICMIXER 1, 1, 1, 1

.section	.data
 GLOBAL(kbasm_mixers)
	.long kbasm_mix_cubic_noscopes_unfiltered_forward_noramp
	.long kbasm_mix_cubic_noscopes_unfiltered_backward_noramp
	.long kbasm_mix_cubic_noscopes_filtered_forward_noramp
	.long kbasm_mix_cubic_noscopes_filtered_backward_noramp
	.long kbasm_mix_cubic_scopes_unfiltered_forward_noramp
	.long kbasm_mix_cubic_scopes_unfiltered_backward_noramp
	.long kbasm_mix_cubic_scopes_filtered_forward_noramp
	.long kbasm_mix_cubic_scopes_filtered_backward_noramp
	.long kbasm_mix_cubic_noscopes_unfiltered_forward
	.long kbasm_mix_cubic_noscopes_unfiltered_backward
	.long kbasm_mix_cubic_noscopes_filtered_forward
	.long kbasm_mix_cubic_noscopes_filtered_backward
	.long kbasm_mix_cubic_scopes_unfiltered_forward
	.long kbasm_mix_cubic_scopes_unfiltered_backward
	.long kbasm_mix_cubic_scopes_filtered_forward
	.long kbasm_mix_cubic_scopes_filtered_backward

#endif /* defined(__i386__) */

	.END
