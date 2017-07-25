/*++
Copyright (c) 2006 Microsoft Corporation

Module Name:

    asserted_formulas.h

Abstract:

    <abstract>

Author:

    Leonardo de Moura (leonardo) 2008-06-11.

Revision History:

--*/
#ifndef _ASSERTED_FORMULAS_H_
#define _ASSERTED_FORMULAS_H_

#include"front_end_params.h"
#include"simplifier.h"
#include"basic_simplifier_plugin.h"
#include"static_features.h" 
#include"macro_manager.h"
#include"macro_finder.h"
#include"defined_names.h"
#include"solver_plugin.h"
#include"maximise_ac_sharing.h"
#include"bit2int.h"
#include"qe.h"
#include"statistics.h"
#include"user_rewriter.h"

class arith_simplifier_plugin;
class bv_simplifier_plugin;

class asserted_formulas {
    ast_manager &               m_manager;
    front_end_params &          m_params;
    simplifier                  m_pre_simplifier;
    subst_simplifier            m_simplifier;
    basic_simplifier_plugin *   m_bsimp;
    bv_simplifier_plugin *      m_bvsimp;
    defined_names               m_defined_names;
    static_features             m_static_features;
    expr_ref_vector             m_asserted_formulas;     // formulas asserted by user
    proof_ref_vector            m_asserted_formula_prs;  // proofs for the asserted formulas.
    unsigned                    m_asserted_qhead;

    expr_map                    m_subst;
    ptr_vector<app>             m_vars;  // domain of m_subst
    unsigned                    m_vars_qhead;

    expr_mark                   m_forbidden;
    ptr_vector<app>             m_forbidden_vars;

    macro_manager               m_macro_manager;
    scoped_ptr<macro_finder>    m_macro_finder;
    
    typedef plugin_manager<solver_plugin> solver_plugins;
    solver_plugins              m_solver_plugins;

    bit2int                     m_bit2int;

    maximise_bv_sharing         m_bv_sharing;

    user_rewriter               m_user_rewriter;

    bool                        m_inconsistent;
    qe::expr_quant_elim_star1   m_quant_elim;

    struct scope {
        unsigned                m_asserted_formulas_lim;
        unsigned                m_vars_lim;
        unsigned                m_forbidden_vars_lim;
        bool                    m_inconsistent_old;
    };
    svector<scope>              m_scopes;
    volatile bool               m_cancel_flag;

    void setup_simplifier_plugins(simplifier & s, basic_simplifier_plugin * & bsimp, arith_simplifier_plugin * & asimp, bv_simplifier_plugin * & bvsimp);
    bool trivial_solve(expr * lhs, expr * rhs, app_ref & var, expr_ref & subst, proof_ref& pr);
    bool is_pos_literal(expr * n);
    bool is_neg_literal(expr * n);
    bool solve_ite_definition_core(expr * lhs1, expr * rhs1, expr * lhs2, expr * rhs2, expr * cond, app_ref & var, expr_ref & subst);
    bool solve_ite_definition(expr * arg1, expr * arg2, expr * arg3, app_ref & var, expr_ref & subst);
    bool solve_core(expr * n, app_ref & var, expr_ref & subst, proof_ref& pr);
    bool solve_core();
    void solve();
    void reduce_asserted_formulas();
    void swap_asserted_formulas(expr_ref_vector & new_exprs, proof_ref_vector & new_prs);
    void find_macros_core();
    void find_macros();
    void expand_macros();
    void apply_demodulators();
    void apply_quasi_macros();
    void nnf_cnf();
    bool apply_eager_bit_blaster();
    bool apply_user_rewriter();
    void infer_patterns();
    void eliminate_term_ite();
    void reduce_and_solve();
    void flush_cache() { m_pre_simplifier.reset(); m_simplifier.reset(); }
    void restore_subst(unsigned old_size);
    void restore_forbidden_vars(unsigned old_size);
    void set_eliminate_and(bool flag);
    void propagate_values();
    void propagate_booleans();
    bool pull_cheap_ite_trees();
    bool pull_nested_quantifiers();
    void push_assertion(expr * e, proof * pr, expr_ref_vector & result, proof_ref_vector & result_prs);
    void context_simplifier();
    void strong_context_simplifier();
    void eliminate_and();
    void refine_inj_axiom();
    bool cheap_quant_fourier_motzkin();
    bool quant_elim();
    bool apply_der_core();
    void apply_der();
    void apply_distribute_forall();
    bool apply_bit2int();
    void lift_ite();
    bool elim_bvs_from_quantifiers();
    void ng_lift_ite(); 
#ifdef Z3DEBUG
    bool check_well_sorted() const;
#endif
    unsigned get_total_size() const;
    bool has_bv() const;
    void max_bv_sharing();
    bool canceled() { return m_cancel_flag; }

public:
    asserted_formulas(ast_manager & m, front_end_params & p);
    ~asserted_formulas();

    void setup();
    void assert_expr(expr * e, proof * in_pr);
    void assert_expr(expr * e);
    void reset();
    void set_cancel_flag(bool f);
    void push_scope();
    void pop_scope(unsigned num_scopes);
    bool inconsistent() const { return m_inconsistent; }
    proof * get_inconsistency_proof() const;
    void reduce();
    unsigned get_num_formulas() const { return m_asserted_formulas.size(); }
    unsigned get_formulas_last_level() const;
    unsigned get_qhead() const { return m_asserted_qhead; }
    void commit(); 
    expr * get_formula(unsigned idx) const { return m_asserted_formulas.get(idx); }
    proof * get_formula_proof(unsigned idx) const { return m_manager.proofs_enabled() ? m_asserted_formula_prs.get(idx) : 0; }
    expr * const * get_formulas() const { return m_asserted_formulas.c_ptr(); }
    proof * const * get_formula_proofs() const { return m_asserted_formula_prs.c_ptr(); }
    void init(unsigned num_formulas, expr * const * formulas, proof * const * prs);
    void register_simplifier_plugin(simplifier_plugin * p) { m_simplifier.register_plugin(p); }
    simplifier & get_simplifier() { return m_simplifier; }
    void set_user_rewriter(void* ctx, user_rewriter::fn* rw) { m_user_rewriter.set_rewriter(ctx, rw); }
    void get_assertions(ptr_vector<expr> & result);
    bool empty() const { return m_asserted_formulas.empty(); }
    void collect_static_features();
    void display(std::ostream & out) const;
    void display_ll(std::ostream & out, ast_mark & pp_visited) const;
    void collect_statistics(statistics & st) const;
    // TODO: improve precision of the following method.
    bool has_quantifiers() const { return m_simplifier.visited_quantifier(); /* approximation */ }

    // -----------------------------------
    //
    // Macros
    //
    // -----------------------------------
    unsigned get_num_macros() const { return m_macro_manager.get_num_macros(); }
    unsigned get_first_macro_last_level() const { return m_macro_manager.get_first_macro_last_level(); }
    func_decl * get_macro_func_decl(unsigned i) const { return m_macro_manager.get_macro_func_decl(i); }
    func_decl * get_macro_interpretation(unsigned i, expr_ref & interp) const { return m_macro_manager.get_macro_interpretation(i, interp); }
    quantifier * get_macro_quantifier(func_decl * f) const { return m_macro_manager.get_macro_quantifier(f); }
    // auxiliary function used to create a logic context based on a model.
    void insert_macro(func_decl * f, quantifier * m, proof * pr) { m_macro_manager.insert(f, m, pr); }

    // -----------------------------------
    //
    // Eliminated vars
    //
    // -----------------------------------
    ptr_vector<app>::const_iterator begin_subst_vars() const  { return m_vars.begin(); }
    ptr_vector<app>::const_iterator end_subst_vars() const    { return m_vars.end(); }
    ptr_vector<app>::const_iterator begin_subst_vars_last_level() const  { 
        unsigned sidx = m_scopes.empty() ? 0 : m_scopes.back().m_vars_lim;
        return m_vars.begin() + sidx;
    }
    expr * get_subst(app * var) { expr * def = 0; proof * pr; m_subst.get(var, def, pr); return def; }
    bool is_subst(app * var) const { return m_subst.contains(var); }
    void get_ordered_subst_vars(ptr_vector<app> & ordered_vars);  
};

#endif /* _ASSERTED_FORMULAS_H_ */

