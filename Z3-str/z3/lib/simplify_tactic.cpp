/*++
Copyright (c) 2011 Microsoft Corporation

Module Name:

    simplify_tactic.cpp

Abstract:

    Apply simplification and rewriting rules.

Author:

    Leonardo (leonardo) 2011-11-20

Notes:

--*/
#include"simplify_tactic.h"
#include"th_rewriter.h"
#include"ast_smt2_pp.h"

struct simplify_tactic::imp {
    ast_manager &   m_manager;
    th_rewriter     m_r;
    unsigned        m_num_steps;

    imp(ast_manager & m, params_ref const & p):
        m_manager(m),
        m_r(m, p),
        m_num_steps(0) {
    }

    ast_manager & m() const { return m_manager; }

    void set_cancel(bool f) {
        m_r.set_cancel(f);
    }

    void reset() {
        m_r.reset();
        m_num_steps = 0;
    }

    void operator()(goal & g) {
        SASSERT(g.is_well_sorted());
        tactic_report report("simplifier", g);
        TRACE("before_simplifier", g.display(tout););
        m_num_steps = 0;
        if (g.inconsistent())
            return;
        expr_ref   new_curr(m());
        proof_ref  new_pr(m());
        unsigned size = g.size();
        for (unsigned idx = 0; idx < size; idx++) {
            if (g.inconsistent())
                break;
            expr * curr = g.form(idx);
            m_r(curr, new_curr, new_pr);
            m_num_steps += m_r.get_num_steps();
            if (g.proofs_enabled()) {
                proof * pr = g.pr(idx);
                new_pr     = m().mk_modus_ponens(pr, new_pr);
            }
            g.update(idx, new_curr, new_pr, g.dep(idx));
        }
        TRACE("after_simplifier_bug", g.display(tout););
        g.elim_redundancies();
        TRACE("after_simplifier", g.display(tout););
        SASSERT(g.is_well_sorted());
    }

    unsigned get_num_steps() const { return m_num_steps; }
};

simplify_tactic::simplify_tactic(ast_manager & m, params_ref const & p):
    m_params(p) {
    m_imp = alloc(imp, m, p);
}

simplify_tactic::~simplify_tactic() {
    dealloc(m_imp);
}

void simplify_tactic::updt_params(params_ref const & p) {
    m_params = p;
    m_imp->m_r.updt_params(p);
}

void simplify_tactic::get_param_descrs(param_descrs & r) {
    th_rewriter::get_param_descrs(r);
}

void simplify_tactic::operator()(goal_ref const & in, 
                                 goal_ref_buffer & result, 
                                 model_converter_ref & mc, 
                                 proof_converter_ref & pc,
                                 expr_dependency_ref & core) {
    (*m_imp)(*(in.get()));
    in->inc_depth();
    result.push_back(in.get());
    mc = 0; pc = 0; core = 0;
}

void simplify_tactic::set_cancel(bool f) {
    if (m_imp)
        m_imp->set_cancel(f);
}

void simplify_tactic::cleanup() {
    ast_manager & m = m_imp->m();
    imp * d = m_imp;
    #pragma omp critical (tactic_cancel)
    {
        m_imp = 0;
    }
    dealloc(d);
    d = alloc(imp, m, m_params);
    #pragma omp critical (tactic_cancel)
    {
        m_imp = d;
    }
}

unsigned simplify_tactic::get_num_steps() const {
    return m_imp->get_num_steps();
}

tactic * mk_simplify_tactic(ast_manager & m, params_ref const & p) {
    return clean(alloc(simplify_tactic, m, p));
}

tactic * mk_elim_and_tactic(ast_manager & m, params_ref const & p) {
    params_ref xp = p;
    xp.set_bool(":elim-and", true);
    return using_params(mk_simplify_tactic(m, xp), xp);
}

