/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    nlsat_tactic.h

Abstract:

    Tactic for using nonlinear procedure.

Author:

    Leonardo (leonardo) 2012-01-02

Notes:

--*/
#ifndef _NLSAT_TACTIC_H_
#define _NLSAT_TACTIC_H_

#include"params.h"
class ast_manager;
class tactic;

tactic * mk_nlsat_tactic(ast_manager & m, params_ref const & p = params_ref());

#endif
