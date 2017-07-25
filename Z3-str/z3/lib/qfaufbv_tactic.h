/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    qfaufbv_tactic.h

Abstract:

    Tactic for QF_AUFBV

Author:

    Leonardo (leonardo) 2012-02-23

Notes:

--*/
#ifndef _QFAUFBV_TACTIC_H_
#define _QFAUFBV_TACTIC_H_

#include"params.h"
class ast_manager;
class tactic;

tactic * mk_qfaufbv_tactic(ast_manager & m, params_ref const & p = params_ref());

#endif
