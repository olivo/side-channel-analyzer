/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    default_tactic.cpp

Abstract:

    General purpose tactic for the Z3 logic (when the logic is not specified).

Author:

    Leonardo (leonardo) 2012-02-22

Notes:

--*/
#include"default_tactic.h"
#include"simplify_tactic.h"
#include"qfbv_tactic.h"
#include"smt_tactic.h"
#include"qflia_tactic.h"
#include"qflra_tactic.h"
#include"qfnia_tactic.h"
#include"qfnra_tactic.h"
#include"nra_tactic.h"
#include"probe_arith.h"

tactic * mk_default_tactic(ast_manager & m, params_ref const & p) {
    tactic * st = using_params(and_then(mk_simplify_tactic(m),
                                        cond(mk_is_qfbv_probe(),
                                             mk_qfbv_tactic(m),
                                             cond(mk_is_qflia_probe(),
                                                  mk_qflia_tactic(m),
                                                  cond(mk_is_qflra_probe(),
                                                       mk_qflra_tactic(m),
                                                       cond(mk_is_qfnra_probe(),
                                                            mk_qfnra_tactic(m),
                                                            cond(mk_is_qfnia_probe(),
                                                                 mk_qfnia_tactic(m),
                                                                 cond(mk_is_nra_probe(),
                                                                      mk_nra_tactic(m),
                                                                      mk_smt_tactic()))))))),
                               p);
    return st;
}

