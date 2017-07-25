/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    ctx_solver_simplify_tactic.h

Abstract:

    Context simplifier for propagating solver assignments.

Author:

    Nikolaj (nbjorner) 2012-3-6

Notes:

--*/
#ifndef _CTX_SOLVER_SIMPLIFY_TACTIC_H_
#define _CTX_SOLVER_SIMPLIFY_TACTIC_H_

#include"tactical.h"

tactic * mk_ctx_solver_simplify_tactic(ast_manager & m, params_ref const & p = params_ref());

#endif
