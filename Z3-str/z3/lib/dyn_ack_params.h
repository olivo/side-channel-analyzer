/*++
Copyright (c) 2006 Microsoft Corporation

Module Name:

    dyn_ack_params.h

Abstract:

    <abstract>

Author:

    Leonardo de Moura (leonardo) 2007-05-18.

Revision History:

--*/
#ifndef _DYN_ACK_PARAMS_H_
#define _DYN_ACK_PARAMS_H_

#include"ini_file.h"

enum dyn_ack_strategy {
    DACK_DISABLED,
    DACK_ROOT, // congruence is the root of the conflict
    DACK_CR    // congruence used during conflict resolution
};

struct dyn_ack_params {
    dyn_ack_strategy m_dack;
    bool             m_dack_eq;
    double           m_dack_factor;
    unsigned         m_dack_threshold;
    unsigned         m_dack_gc;
    double           m_dack_gc_inv_decay;

public:
    dyn_ack_params():
        m_dack(DACK_ROOT),
        m_dack_eq(false),
        m_dack_factor(0.1),
        m_dack_threshold(10),
        m_dack_gc(2000), 
        m_dack_gc_inv_decay(0.8) {
    }

    void register_params(ini_params & p);
};


#endif /* _DYN_ACK_PARAMS_H_ */

