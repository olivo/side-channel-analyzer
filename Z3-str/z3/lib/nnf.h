/*++
Copyright (c) 2007 Microsoft Corporation

Module Name:

    nnf.h

Abstract:

    Negation Normal Form & Skolemization

Author:

    Leonardo (leonardo) 2008-01-11

Notes:
    Major revision on 2011-10-06

--*/
#ifndef _NNF_H_
#define _NNF_H_

#include"ast.h"
#include"nnf_params.h"
#include"params.h"
#include"defined_names.h"

class nnf {
    struct imp;
    imp *  m_imp;
public:
    nnf(ast_manager & m, defined_names & n, params_ref const & p = params_ref());
    nnf(ast_manager & m, defined_names & n, nnf_params & params); // for backward compatibility
    ~nnf();
    
    void operator()(expr * n,                          // [IN] expression that should be put into NNF
                    expr_ref_vector & new_defs,        // [OUT] new definitions
                    proof_ref_vector & new_def_proofs, // [OUT] proofs of the new definitions 
                    expr_ref & r,                      // [OUT] resultant expression
                    proof_ref & p                      // [OUT] proof for (~ n r)
                    );

    void updt_params(params_ref const & p);
    static void get_param_descrs(param_descrs & r);

    void cancel() { set_cancel(true); }
    void reset_cancel() { set_cancel(false); }
    void set_cancel(bool f);

    void reset();
    void reset_cache();
};

// Old strategy framework
class assertion_set_strategy;
// Skolem Normal Form
assertion_set_strategy * mk_snf(params_ref const & p = params_ref());
// Negation Normal Form
assertion_set_strategy * mk_nnf(params_ref const & p = params_ref());

// New strategy framework
class tactic;
// Skolem Normal Form
tactic * mk_snf_tactic(ast_manager & m, params_ref const & p = params_ref());
// Negation Normal Form
tactic * mk_nnf_tactic(ast_manager & m, params_ref const & p = params_ref());

#endif /* _NNF_H_ */
