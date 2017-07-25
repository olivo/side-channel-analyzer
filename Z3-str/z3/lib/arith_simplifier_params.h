/*++
Copyright (c) 2006 Microsoft Corporation

Module Name:

    arith_simplifier_params.h

Abstract:

    <abstract>

Author:

    Leonardo de Moura (leonardo) 2008-05-09.

Revision History:

--*/
#ifndef _ARITH_SIMPLIFIER_PARAMS_H_
#define _ARITH_SIMPLIFIER_PARAMS_H_

#include"ini_file.h"

struct arith_simplifier_params {
    bool    m_arith_expand_eqs;
    bool    m_arith_process_all_eqs;

    arith_simplifier_params():
        m_arith_expand_eqs(false),
        m_arith_process_all_eqs(false) {
    }
    
    void register_params(ini_params & p);
};
    
#endif /* _ARITH_SIMPLIFIER_PARAMS_H_ */

