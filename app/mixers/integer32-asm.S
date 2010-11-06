
/*
 * The Real SoundTracker - Assembly routines for the mixer (header)
 *
 * Copyright (C) 1999 Michael Krause
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

 --------------------------------------------------------------------------
	
 The first code ever that I've written in i386 Assembly. It's not been
 optimized for pairing / pipelining, and it's only marginally faster than
 the C version on my K6-2 @400 (12-13% instead of 14% CPU usage with
 astaris.xm -- stereo_16_scopes, that is).

 --------------------------------------------------------------------------
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
	
#define ACCURACY 12
	
.text

 GLOBAL(mixerasm_stereo_16_scopes)
	pushl	%ebp
	movl	%esp,%ebp

	pushl	%edi
	pushl	%esi
	pushl	%ebx
	pushl	%ecx
	pushl	%edx

	movl	8(%ebp),%eax	# eax = fixed-point sample position
	movl	16(%ebp),%esi	# esi = source sample data
	movl	20(%ebp),%ecx	# ecx = destination mixed data (32bit)
	movl	24(%ebp),%edi	# edi = destination scope data (8bit)
	movl	40(%ebp),%esp	# esp = loop counter
				# ebx, edx = working registers
.0:
	movl	%eax,%ebx	# Get sample into edx
	sarl	$ACCURACY,%ebx
	movswl	(%esi,%ebx,2),%edx

	imull	28(%ebp),%edx	# Apply volume

	movl	%edx,%ebx	# left channel
	imull	32(%ebp),%ebx		
	sarl	$6,%ebx
	addl	%ebx,(%ecx)

	movl	%edx, %ebx	# right channel
	imull	36(%ebp),%ebx
	sarl	$6,%ebx
	addl	%ebx,4(%ecx)
	sarl	$6,%edx

	addl	$8,%ecx		# (increment mixed data pointer)
	movw	%dx,(%edi)	# Store scope data
	addl	12(%ebp),%eax	# (increment sample position pointer)

	lea	2(%edi),%edi	# (increment scope data pointer)
	
	decl	%esp		# loop
	jnz	.0

	lea	-20(%ebp),%esp
		
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%esi
	popl	%edi
		
	leave
	ret

 GLOBAL(mixerasm_mono_16_scopes)
	pushl	%ebp
	movl	%esp,%ebp

	pushl	%edi
	pushl	%esi
	pushl	%ebx
	pushl	%ecx
	pushl	%edx

	movl	8(%ebp),%eax	# eax = fixed-point sample position
	movl	16(%ebp),%esi	# esi = source sample data
	movl	20(%ebp),%ecx	# ecx = destination mixed data (32bit)
	movl	24(%ebp),%edi	# edi = destination scope data (8bit)
	movl	32(%ebp),%esp	# esp = loop counter
	movl	28(%ebp),%ebx	# ebx = volume
				# edx = working register
.1:
	movl	%eax,%edx	# Get sample into edx
	sarl	$ACCURACY,%edx
	movswl	(%esi,%edx,2),%edx

	imull	%ebx,%edx	# Apply volume
	addl	%edx,(%ecx)

	sarl	$6,%edx
	movw	%dx,(%edi)	# Store scope data
	lea	2(%edi),%edi	# (increment scope data pointer)

	addl	$4,%ecx
	addl	12(%ebp),%eax	# (increment sample position pointer)

	decl	%esp		# loop
	jnz	.1

	lea	-20(%ebp),%esp
		
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%esi
	popl	%edi
		
	leave
	ret

 GLOBAL(mixerasm_stereo_16)
	pushl	%ebp
	movl	%esp,%ebp

	pushl	%edi
	pushl	%esi
	pushl	%ebx
	pushl	%ecx
	pushl	%edx

	movl	8(%ebp),%eax	# eax = fixed-point sample position
	movl	12(%ebp),%ecx	# ecx = increment
	movl	16(%ebp),%esi	# esi = source sample data
	movl	20(%ebp),%edi	# edi = destination mixed data (32bit)
	movl	32(%ebp),%esp	# esp = loop counter
				# ebx, edx = working registers
.2:
	movl	%eax,%ebx	# Get sample into edx
	sarl	$ACCURACY,%ebx
	movswl	(%esi,%ebx,2),%edx
	movl	%edx,%ebx

	imull	24(%ebp),%ebx	# left channel
	imull	28(%ebp),%edx	# right channel
	sarl	$6,%ebx
	sarl	$6,%edx
	addl	%ebx,(%edi)
	addl	%edx,4(%edi)

	addl	$8,%edi		# (increment mixed data pointer)
	addl	%ecx,%eax	# (increment sample position pointer)

	decl	%esp		# loop
	jnz	.2

	lea	-20(%ebp),%esp
		
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%esi
	popl	%edi
		
	leave
	ret

 GLOBAL(mixerasm_mono_16)
	pushl	%ebp
	movl	%esp,%ebp

	pushl	%edi
	pushl	%esi
	pushl	%ebx
	pushl	%ecx
	pushl	%edx

	movl	8(%ebp),%eax	# eax = fixed-point sample position
	movl	12(%ebp),%ecx	# ecx = increment
	movl	16(%ebp),%esi	# esi = source sample data
	movl	20(%ebp),%edi	# edi = destination mixed data (32bit)
	movl	28(%ebp),%esp	# esp = loop counter
	movl	24(%ebp),%ebx	# ebx = volume
				# edx = working register
.3:
	movl	%eax,%edx	# Get sample into edx
	sarl	$ACCURACY,%edx
	movswl	(%esi,%edx,2),%edx

	imull	%ebx,%edx	# Apply volume
	addl	%ecx,%eax	# (increment sample position pointer)
	addl	%edx,(%edi)
	addl	$4,%edi

	decl	%esp		# loop
	jnz	.3

	lea	-20(%ebp),%esp
		
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%esi
	popl	%edi
		
	leave
	ret

#endif /* defined(__i386__) */
