/*++
Copyright (c) 2008 Microsoft Corporation

Module Name:

    expr_context_simplifier.h

Abstract:

    <abstract>

Author:

    Nikolaj Bjorner (nbjorner) 2008-06-03

Revision History:

--*/
#ifndef _EXPR_CONTEXT_SIMPLIFIER_H_
#define _EXPR_CONTEXT_SIMPLIFIER_H_

#include "ast.h"
#include "obj_hashtable.h"
#include "basic_simplifier_plugin.h"
#include "front_end_params.h"
#include "smt_solver.h"
#include "arith_decl_plugin.h"

class expr_context_simplifier {
    typedef obj_map<expr, bool> context_map;

    ast_manager& m_manager;
    arith_util   m_arith;
    context_map  m_context;
    expr_ref_vector m_trail;
    basic_simplifier_plugin m_simp;
    expr_mark m_mark;
    bool m_forward;
public:
    expr_context_simplifier(ast_manager & m);
    void reduce_fix(expr * m, expr_ref & result);
    void operator()(expr * m, expr_ref & result) { reduce(m, result); }
    void insert_context(expr* e, bool polarity);
private:
    void reduce(expr * m, expr_ref & result);
    void reduce_rec(expr * m, expr_ref & result);
    void reduce_rec(quantifier* q, expr_ref & result);
    void reduce_rec(app * a, expr_ref & result);
    void clean_trail(unsigned old_lim);
    bool insert_arg(bool is_and, expr* arg, expr_ref_vector& args);
    void reduce_and_or(bool is_and, unsigned num_args, expr * const* args, expr_ref & result);
    void reduce_and(unsigned num_args, expr * const* args, expr_ref & result);
    void reduce_or(unsigned num_args, expr * const* args, expr_ref & result);
    bool is_true(expr* e) const;
    bool is_false(expr* e) const;
};

class expr_strong_context_simplifier {
    ast_manager& m_manager;
    front_end_params & m_params;
    arith_util    m_arith;
    unsigned      m_id;
    func_decl_ref m_fn;
    smt::solver   m_solver;
    
    void simplify(expr* e, expr_ref& result) { simplify_model_based(e, result); }
    void simplify_basic(expr* fml, expr_ref& result);
    void simplify_model_based(expr* fml, expr_ref& result);

    bool is_forced(expr* e, expr* v);

public:
    expr_strong_context_simplifier(front_end_params& p, ast_manager& m);
    void operator()(expr* e, expr_ref& result) { simplify(e, result); }
    void operator()(expr_ref& result) { simplify(result.get(), result); }
    void push() { m_solver.push(); }
    void pop() { m_solver.pop(1); }
    void assert(expr* e) { m_solver.assert_expr(e); }
    
    void collect_statistics(statistics & st) const { m_solver.collect_statistics(st); }
    void reset_statistics() { m_solver.reset_statistics(); }
    void set_cancel(bool f) { m_solver.set_cancel(f); }
};

#endif /* _EXPR_CONTEXT_SIMPLIFIER_H__ */

