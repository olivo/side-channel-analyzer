/*++
Copyright (c) 2011 Microsoft Corporation

Module Name:

    degree_shift_tactic.h

Abstract:

    Simple degree shift procedure. 
    Basic idea: if goal G contains a real variable x, x occurs with degrees
    d_1, ..., d_k in G, and n = gcd(d_1, ..., d_k) > 1. 
    Then, replace x^n with a new fresh variable y.

Author:

    Leonardo de Moura (leonardo) 2011-12-30.

Revision History:

--*/
#ifndef _DEGREE_SHIFT_TACTIC_H_
#define _DEGREE_SHIFT_TACTIC_H_

#include"params.h"
class ast_manager;
class tactic;

tactic * mk_degree_shift_tactic(ast_manager & m, params_ref const & p = params_ref());

#endif
