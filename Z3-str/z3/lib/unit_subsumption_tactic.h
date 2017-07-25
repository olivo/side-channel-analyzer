/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    unit_subsumption_tactic.h

Abstract:

    Simplify goal using subsumption based on unit propagation.

Author:

    Nikolaj Bjorner (nbjorner) 2012-9-6

Notes:

    Background: PDR generates several clauses that subsume each-other.
    Simplify a goal assuming it is a conjunction of clauses.
    Subsumed clauses are simplified by using unit-propagation 
    It uses the smt_context for the solver.

--*/
#ifndef _UNIT_SUBSUMPTION_TACTIC_H_
#define _UNIT_SUBSUMPTION_TACTIC_H_
#include "tactic.h"

tactic * mk_unit_subsumption_tactic(ast_manager & m, params_ref const & p = params_ref());

#endif
