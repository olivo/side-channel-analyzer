/*++
Copyright (c) 2011 Microsoft Corporation

Module Name:

    nnf_tactic.h

Abstract:

    NNF tactic

Author:

    Leonardo de Moura (leonardo) 2011-12-28.

Revision History:

--*/
#ifndef _NNF_TACTIC_H_
#define _NNF_TACTIC_H_

#include"params.h"
class ast_manager;
class tactic;

tactic * mk_snf_tactic(ast_manager & m, params_ref const & p = params_ref());
tactic * mk_nnf_tactic(ast_manager & m, params_ref const & p = params_ref());

#endif

