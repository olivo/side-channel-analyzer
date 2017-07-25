/*++
Copyright (c) 2006 Microsoft Corporation

Module Name:

    dl_mk_simple_joins.h

Abstract:

    <abstract>

Author:

    Leonardo de Moura (leonardo) 2010-05-20.

Revision History:

--*/
#ifndef _DL_MK_SIMPLE_JOINS_H_
#define _DL_MK_SIMPLE_JOINS_H_

#include"map.h"
#include"obj_pair_hashtable.h"

#include"dl_context.h"
#include"dl_rule_set.h"
#include"dl_rule_transformer.h"

namespace datalog {

    /**
       \brief Functor for creating rules that contain simple joins.
       A simple join is the join of two tables.

       After applying this transformation, every rule has at most one join.
       So, the rules will have the form

          HEAD :- TAIL.
          HEAD :- TAIL_1, TAIL_2.
         
       We also assume a rule may contain interpreted expressions that work as filtering conditions.
       So, we may also have:

          HEAD :- TAIL, C_1, ..., C_n.
          HEAD :- TAIL_1, TAIL_2, C_1, ..., C_n.
          
       Where the C_i's are interpreted expressions.

       We say that a rule containing C_i's is a rule with a "big tail".
    */
    class mk_simple_joins : public rule_transformer::plugin {
        context &							m_context;
    public:
        mk_simple_joins(context & ctx);
        
        rule_set * operator()(rule_set const & source, model_converter_ref& mc, proof_converter_ref& pc);
    };

};

#endif /* _DL_MK_SIMPLE_JOINS_H_ */

