/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    tactic2solver.h

Abstract:

    Wrapper for implementing the external solver interface
    using a tactic.

    This is a light version of the strategic solver.

Author:

    Leonardo (leonardo) 2012-01-23

Notes:

--*/
#ifndef _TACTIC2SOLVER_H_
#define _TACTIC2SOLVER_H_

#include"solver.h"
#include"tactic.h"

class tactic2solver : public solver {
    struct ctx {
        symbol                       m_logic;
        expr_ref_vector              m_assertions;
        unsigned_vector              m_scopes;
        ref<simple_check_sat_result> m_result;
        tactic_ref                   m_tactic;
        ctx(ast_manager & m, symbol const & logic);
        ast_manager & m() const { return m_assertions.m(); }
    };
    scoped_ptr<ctx>            m_ctx;
    front_end_params *         m_fparams;
    params_ref                 m_params;
    bool                       m_produce_models;
    bool                       m_produce_proofs;
    bool                       m_produce_unsat_cores;
public:
    tactic2solver():m_ctx(0), m_fparams(0), m_produce_models(false), m_produce_proofs(false), m_produce_unsat_cores(false) {}
    virtual ~tactic2solver();

    virtual tactic * get_tactic(ast_manager & m, params_ref const & p) = 0;
    
    virtual void set_front_end_params(front_end_params & p) { m_fparams = &p; } 

    virtual void updt_params(params_ref const & p);
    virtual void collect_param_descrs(param_descrs & r);

    virtual void set_produce_proofs(bool f) { m_produce_proofs = f; }
    virtual void set_produce_models(bool f) { m_produce_models = f; }
    virtual void set_produce_unsat_cores(bool f) { m_produce_unsat_cores = f; }

    virtual void init(ast_manager & m, symbol const & logic);
    virtual void reset();
    virtual void assert_expr(expr * t);
    virtual void push();
    virtual void pop(unsigned n);
    virtual unsigned get_scope_level() const;
    virtual lbool check_sat(unsigned num_assumptions, expr * const * assumptions);

    virtual void set_cancel(bool f);

    virtual void collect_statistics(statistics & st) const;
    virtual void get_unsat_core(ptr_vector<expr> & r);
    virtual void get_model(model_ref & m);
    virtual proof * get_proof();
    virtual std::string reason_unknown() const;
    virtual void get_labels(svector<symbol> & r) {}

    virtual void set_progress_callback(progress_callback * callback) {}

    virtual unsigned get_num_assertions() const;
    virtual expr * get_assertion(unsigned idx) const;

    virtual void display(std::ostream & out) const;
};

/**
   \brief Specialization for cmd_context
*/
class tactic2solver_cmd : public tactic2solver {
    scoped_ptr<tactic_factory> m_tactic_factory;
public:
    virtual ~tactic2solver_cmd() {}
    /**
       \brief Set tactic that will be used to process the satisfiability queries.
    */
    void set_tactic(tactic_factory * f); 
    virtual tactic * get_tactic(ast_manager & m, params_ref const & p);
};

/**
   \brief Specialization for API
*/
class tactic2solver_api : public tactic2solver {
    tactic_ref m_tactic;
public:
    tactic2solver_api(tactic * t):m_tactic(t) {}
    virtual ~tactic2solver_api() {}
    virtual tactic * get_tactic(ast_manager & m, params_ref const & p);
};


#endif
