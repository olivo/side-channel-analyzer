/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    cofactor_term_ite_tactic.h

Abstract:

    Wrap cofactor_elim_term_ite as a tactic.
    Eliminate (ground) term if-then-else's using cofactors.

Author:

    Leonardo de Moura (leonardo) 2012-02-20.

Revision History:

--*/
#ifndef _COFACTOR_TERM_ITE_TACTIC_H_
#define _COFACTOR_TERM_ITE_TACTIC_H_

#include"params.h"
class ast_manager;
class tactic;

tactic * mk_cofactor_term_ite_tactic(ast_manager & m, params_ref const & p = params_ref());

#endif
