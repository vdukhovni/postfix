#ifndef _CHECK_ARG_INCLUDED_
#define _CHECK_ARG_INCLUDED_

/*++
/* NAME
/*	check_arg 3h
/* SUMMARY
/*	type checking/narrowing/widening for unprototyped arguments
/* SYNOPSIS
/*	#include <check_arg.h>
/*
/*	extern int CHECK_VAL_DUMMY(int);
/*	extern int CHECK_PTR_DUMMY(int);
/*	extern int CHECK_CONST_PTR_DUMMY(int);
/*
/*	int val;
/*	int *p1;
/*	const int *p2;
/*
/*	func(CHECK_VAL(int, val), CHECK_PTR(int, p1), CHECK_CONST_PTR(int, p2));
/* DESCRIPTION
/*      This module implements wrappers for unprototyped function
/*      arguments, to enable the same type checking, type narrowing,
/*      and type widening as for prototyped function arguments. The
/*	wrappers may also be useful in other contexts.
/*
/*      Each non-pointer argument type is handled by a corresponding
/*      CHECK_VAL(type, value) wrapper (type = int, long, etc.), and
/*      each pointer argument type is handled by a corresponding
/*      CHECK_CONST_PTR(type, ptr) or CHECK_PTR(type, ptr) wrapper.
/*
/*      A good compiler will report the following problems:
/* .IP \(bu
/*      Const pointer argument where a non-const pointer is expected.
/* .IP \(bu
/*      Pointer argument where a non-pointer is expected.
/* .IP \(bu
/*      Pointer/pointer type mismatches except void/non-void pointers.
/*      The latter is why all check_arg_xxx_ptr() macros cast their
/*      result to the desired type.
/* .IP \(bu
/*      Non-constant non-pointer argument where a pointer is expected.
/*. PP
/*      Just like function prototypes, the CHECK_(CONST_)PTR() wrappers
/*      handle "bare" numeric constants by casting their argument to
/*      the desired pointer type.
/*
/*      Just like function prototypes, the CHECK_VAL() wrapper cannot
/*      force the caller to specify a particular non-pointer type and
/*      casts its argument to the desired type.
/* IMPLEMENTATION
/* .ad
/* .fi
/*      This implementation uses unreachable assignments to dummy
/*      variables. Even a basic optimizer will eliminate these
/*      assignments along with any reference to the dummy assignment
/*      targets. It should be possible to declare these variables as
/*      extern only, without any actual definition for storage
/*      allocation.
/* .na
/* .nf

 /*
  * Templates for parameter value checks.
  */
#define CHECK_VAL(type, v) ((type) (1 ? (v) : (CHECK_VAL_DUMMY(type) = (v))))
#define CHECK_PTR(type, p) ((type *) (1 ? (p) : (CHECK_PTR_DUMMY(type) = (p))))
#define CHECK_CONST_PTR(type, p) \
	((const type *) (1 ? (p) : (CHECK_CONST_PTR_DUMMY(type) = (p))))

 /*
  * Templates for dummy assignment targets. These will never be referenced,
  */
#define CHECK_VAL_DUMMY(type) check_val_dummy_##type
#define CHECK_PTR_DUMMY(type) check_ptr_dummy_##type
#define CHECK_CONST_PTR_DUMMY(type) check_const_ptr_dummy_##type

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

#endif
