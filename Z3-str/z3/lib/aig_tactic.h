/*++
Copyright (c) 2011 Microsoft Corporation

Module Name:

    aig_tactic.h

Abstract:

    Tactic for minimizing circuits using AIGs.

Author:

    Leonardo (leonardo) 2011-10-24

Notes:

--*/
#ifndef _AIG_TACTIC_H_
#define _AIG_TACTIC_H_

#include"params.h"
class tactic;

tactic * mk_aig_tactic(params_ref const & p = params_ref());

#endif
