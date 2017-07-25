/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    qfnra_tactic.h

Abstract:

    Tactic for QF_NRA

Author:

    Leonardo (leonardo) 2012-02-28

Notes:

--*/
#ifndef _QFNRA_TACTIC_
#define _QFNRA_TACTIC_

#include"params.h"
class ast_manager;
class tactic;

tactic * mk_qfnra_tactic(ast_manager & m, params_ref const & p = params_ref());

#endif
