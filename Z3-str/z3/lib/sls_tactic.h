/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    sls_tactic.h

Abstract:

    A Stochastic Local Search (SLS) tactic 

Author:

    Christoph (cwinter) 2012-02-29

Notes:

--*/
#ifndef _SLS_TACTIC_H_
#define _SLS_TACTIC_H_

#include"params.h"
class ast_manager;
class tactic;

tactic * mk_sls_tactic(ast_manager & m, params_ref const & p = params_ref());

tactic * mk_qfbv_sls_tactic(ast_manager & m, params_ref const & p = params_ref());

#endif
