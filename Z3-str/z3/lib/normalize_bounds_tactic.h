/*++
Copyright (c) 2011 Microsoft Corporation

Module Name:

    normalize_bounds_tactic.h

Abstract:

    Replace x with x' + l, when l <= x
    where x' is a fresh variable.
    Note that, after the transformation 0 <= x'.

Author:

    Leonardo de Moura (leonardo) 2011-10-21.

Revision History:

--*/
#ifndef _NORMALIZE_BOUNDS_TACTIC_H_
#define _NORMALIZE_BOUNDS_TACTIC_H_

#include"params.h"
class ast_manager;
class tactic;

tactic * mk_normalize_bounds_tactic(ast_manager & m, params_ref const & p = params_ref());

#endif
