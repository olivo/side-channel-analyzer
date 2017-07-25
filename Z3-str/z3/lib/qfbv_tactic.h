/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    qfbv_tactic.h

Abstract:

    Tactic for QF_BV based on bit-blasting

Author:

    Leonardo (leonardo) 2012-02-22

Notes:

--*/
#ifndef _QFBV_TACTIC_
#define _QFBV_TACTIC_

#include"params.h"
class ast_manager;
class tactic;

tactic * mk_qfbv_tactic(ast_manager & m, params_ref const & p = params_ref());

#endif
