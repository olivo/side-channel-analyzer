/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    qfnia_tactic.h

Abstract:

    Tactic for QF_NIA

Author:

    Leonardo (leonardo) 2012-02-28

Notes:

--*/
#ifndef _QFNIA_TACTIC_
#define _QFNIA_TACTIC_

#include"params.h"
class ast_manager;
class tactic;

tactic * mk_qfnia_tactic(ast_manager & m, params_ref const & p = params_ref());

#endif
