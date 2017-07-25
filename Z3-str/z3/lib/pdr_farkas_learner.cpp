/*++
Copyright (c) 2011 Microsoft Corporation

Module Name:

    pdr_farkas_learner.cpp

Abstract:

    Proviced abstract interface and some inplementations of algorithms
    for strenghtning lemmas

Author:

    Krystof Hoder (t-khoder) 2011-11-1.

Revision History:

--*/

#include "ast_smt2_pp.h"
#include "array_decl_plugin.h"
#include "bool_rewriter.h"
#include "dl_decl_plugin.h"
#include "for_each_expr.h"
#include "dl_util.h"
#include "rewriter.h"
#include "rewriter_def.h"
#include "pdr_util.h"
#include "pdr_farkas_learner.h"
#include "th_rewriter.h"
#include "smtparser.h"
#include "pdr_interpolant_provider.h"
#include "ast_ll_pp.h"
#include "arith_bounds_tactic.h"

#define PROOF_MODE PGM_FINE
//#define PROOF_MODE PGM_COARSE

namespace pdr {

    class farkas_learner::constr {

        ast_manager&     m;
        arith_util       a;
        app_ref_vector   m_ineqs;
        vector<rational> m_coeffs;

        void mk_coerce(expr*& e1, expr*& e2) {
            if (a.is_int(e1) && a.is_real(e2)) {
                e1 = a.mk_to_real(e1);
            }
            else if (a.is_int(e2) && a.is_real(e1)) {
                e2 = a.mk_to_real(e2);
            }
        }

        app* mk_add(expr* e1, expr* e2) {
            mk_coerce(e1, e2);
            return a.mk_add(e1, e2);
        }

        app* mk_le(expr* e1, expr* e2) {
            mk_coerce(e1, e2);
            return a.mk_le(e1, e2);
        }

        app* mk_ge(expr* e1, expr* e2) {
            mk_coerce(e1, e2);
            return a.mk_ge(e1, e2);
        }

        app* mk_gt(expr* e1, expr* e2) {
            mk_coerce(e1, e2);
            return a.mk_gt(e1, e2);
        }

        app* mk_lt(expr* e1, expr* e2) {
            mk_coerce(e1, e2);
            return a.mk_lt(e1, e2);
        }

        void mul(rational const& c, expr* e, expr_ref& res) {
            expr_ref tmp(m);
            if (c.is_one()) {
                tmp = e;
            }
            else {
                tmp = a.mk_mul(a.mk_numeral(c, a.is_int(e)), e);
            }
            res = mk_add(res, tmp);
        }

        bool is_int_sort(app* c) {
            SASSERT(m.is_eq(c) || a.is_le(c) || a.is_lt(c) || a.is_gt(c) || a.is_ge(c));
            SASSERT(a.is_int(c->get_arg(0)) || a.is_real(c->get_arg(0)));
            return a.is_int(c->get_arg(0));
        }

        bool is_int_sort() {
            SASSERT(!m_ineqs.empty());
            return is_int_sort(m_ineqs[0].get());
        }

        void normalize_coeffs() {
            rational l(1);
            for (unsigned i = 0; i < m_coeffs.size(); ++i) {
                l = lcm(l, denominator(m_coeffs[i]));
            }
            if (!l.is_one()) {
                for (unsigned i = 0; i < m_coeffs.size(); ++i) {
                    m_coeffs[i] *= l;
                }
            }
        }

        app* mk_one() {
            return a.mk_numeral(rational(1), true);
        }

        app* fix_sign(bool is_pos, app* c) {
            expr* x, *y;
            SASSERT(m.is_eq(c) || a.is_le(c) || a.is_lt(c) || a.is_gt(c) || a.is_ge(c));
            bool is_int = is_int_sort(c);
            if (is_int && is_pos && (a.is_lt(c, x, y) || a.is_gt(c, y, x))) {
                return mk_le(mk_add(x, mk_one()), y);
            }
            if (is_int && !is_pos && (a.is_le(c, x, y) || a.is_ge(c, y, x))) {
                // !(x <= y) <=> x > y <=> x >= y + 1
                return mk_ge(x, mk_add(y, mk_one()));
            }
            if (is_pos) {
                return c;
            }
            if (a.is_le(c, x, y)) return mk_gt(x, y);
            if (a.is_lt(c, x, y)) return mk_ge(x, y);
            if (a.is_ge(c, x, y)) return mk_lt(x, y);
            if (a.is_gt(c, x, y)) return mk_le(x, y);
            UNREACHABLE();
            return c;
        }
        
    public:
        constr(ast_manager& m) : m(m), a(m), m_ineqs(m) {}

        /** add a multiple of constraint c to the current constr */
        void add(rational const & coef, app * c) {
            bool is_pos = true;
            expr* e;
            while (m.is_not(c, e)) {
                is_pos = !is_pos;
                c = to_app(e);
            }

            if (!coef.is_zero() && !m.is_true(c)) {
                m_coeffs.push_back(coef);                
                m_ineqs.push_back(fix_sign(is_pos, c));                
            }
        }

        //
        // Extract the complement of premises multiplied by Farkas coefficients.
        //
        void get(expr_ref& res) {
            if (m_coeffs.empty()) {
                res = m.mk_false();
                return;
            }
            bool is_int = is_int_sort();
            if (is_int) {                
                normalize_coeffs();
            }
            TRACE("pdr", 
                  for (unsigned i = 0; i < m_coeffs.size(); ++i) {
                      tout << m_coeffs[i] << ": " << mk_pp(m_ineqs[i].get(), m) << "\n";
                  }
                  );
            app_ref zero(a.mk_numeral(rational::zero(), is_int), m);
            res = zero;
            bool is_strict = false;
            bool is_eq     = true;
            expr* x, *y;
            for (unsigned i = 0; i < m_coeffs.size(); ++i) {
                app* c = m_ineqs[i].get();
                if (m.is_eq(c, x, y)) {
                    mul(m_coeffs[i],  x, res);
                    mul(-m_coeffs[i], y, res);
                }
                if (a.is_lt(c, x, y) || a.is_gt(c, y, x)) {
                    mul(m_coeffs[i],  x, res);
                    mul(-m_coeffs[i], y, res);
                    is_strict = true;
                    is_eq = false;
                }
                if (a.is_le(c, x, y) || a.is_ge(c, y, x)) {
                    mul(m_coeffs[i],  x, res);
                    mul(-m_coeffs[i], y, res);
                    is_eq = false;
                }
            }
            zero = a.mk_numeral(rational::zero(), a.is_int(res));
            if (is_eq) {
                res = m.mk_eq(res, zero);
            }
            else if (is_strict) {
                res = mk_lt(res, zero);
            }
            else {
                res = mk_le(res, zero);
            }            
            res = m.mk_not(res);
            th_rewriter rw(m);
            proof_ref pr(m);
            expr_ref tmp(m);
            rw(res, tmp, pr);
            res = tmp;
        }
    };
    
    farkas_learner::farkas_learner(front_end_params& params, ast_manager& outer_mgr) 
        : m_proof_params(get_proof_params(params)), 
          m(PROOF_MODE),
          m_brwr(m),
          p2o(m, outer_mgr),
          o2p(outer_mgr, m),
          m_simplifier(mk_arith_bounds_tactic(outer_mgr))
    {
        m.register_decl_plugins();
        m_ctx = alloc(smt::solver, m, m_proof_params);
    }

    front_end_params farkas_learner::get_proof_params(front_end_params& orig_params) {
        front_end_params res(orig_params);
        res.m_proof_mode = PROOF_MODE;
        res.m_arith_bound_prop = BP_NONE;
        // temp hack to fix the build
        // res.m_conflict_resolution_strategy = CR_ALL_DECIDED;
        res.m_arith_auto_config_simplex = true;
        res.m_arith_propagate_eqs = false;
        res.m_arith_eager_eq_axioms = false;
        res.m_arith_eq_bounds = false;
        return res;
    }

    class farkas_learner::equality_expander_cfg : public default_rewriter_cfg
    {
        ast_manager& m;
        arith_util   m_ar;
    public:
        equality_expander_cfg(ast_manager& m) : m(m), m_ar(m) {}

        bool get_subst(expr * s, expr * & t, proof * & t_pr) {
            expr * x, *y;
            if (m.is_eq(s, x, y) && (m_ar.is_int(x) || m_ar.is_real(x))) {
                t = m.mk_and(m_ar.mk_ge(x, y), m_ar.mk_le(x, y));
                return true;
            }
            else {
                return false;
            }
        }
    };

    class collect_pure_proc {
        func_decl_set& m_symbs;
    public:
        collect_pure_proc(func_decl_set& s):m_symbs(s) {}

        void operator()(app* a) {
            if (a->get_family_id() == null_family_id) {
                m_symbs.insert(a->get_decl());
            }
        }
        void operator()(var*) {}
        void operator()(quantifier*) {}
    };


    bool farkas_learner::get_lemma_guesses(expr * A_ext, expr * B_ext, expr_ref_vector& lemmas)
    {
        expr_ref A(o2p(A_ext), m);
        expr_ref B(o2p(B_ext), m);
        proof_ref pr(m);
        expr_ref tmp(m);
        expr_ref_vector ilemmas(m);
        equality_expander_cfg ee_rwr_cfg(m);
        rewriter_tpl<equality_expander_cfg> ee_rwr(m, false, ee_rwr_cfg);

        lemmas.reset();

        ee_rwr(A, A);
        ee_rwr(B, B);

        expr_set bs;
        func_decl_set Bsymbs;
        expr_ref_vector blist(m);
        datalog::flatten_and(B, blist);
        for (unsigned i = 0; i < blist.size(); ++i) {
            bs.insert(blist[i].get());
        }
        collect_pure_proc collect_proc(Bsymbs);
        for_each_expr(collect_proc, B);

        if (!m_ctx) {
            m_ctx = alloc(smt::solver, m, m_proof_params);
        }

        m_ctx->push();
        m_ctx->assert_expr(A);
        expr_set::iterator it = bs.begin(), end = bs.end();
        for (; it != end; ++it) {
            m_ctx->assert_expr(*it);
        }
        lbool res = m_ctx->check();
        bool is_unsat = res == l_false;
        if (is_unsat) {
            pr = m_ctx->get_proof();
            get_lemmas(m_ctx->get_proof(), bs, Bsymbs, A, ilemmas);
            for (unsigned i = 0; i < ilemmas.size(); ++i) {
                lemmas.push_back(p2o(ilemmas[i].get()));
            }
            simplify_lemmas(lemmas);
        }
        m_ctx->pop(1);

        IF_VERBOSE(3, {
                for (unsigned i = 0; i < lemmas.size(); ++i) {
                    verbose_stream() << "B': " << mk_pp(ilemmas[i].get(), m) << "\n";
                }
            });

        TRACE("farkas_learner",
              tout << (is_unsat?"unsat":"sat") << "\n";
              tout << "A: " << mk_pp(A_ext, m_ctx->m()) << "\n";
              tout << "B: " << mk_pp(B_ext, m_ctx->m()) << "\n";
              for (unsigned i = 0; i < lemmas.size(); ++i) {
                  tout << "B': " << mk_pp(ilemmas[i].get(), m) << "\n";
              });
        DEBUG_CODE(
            if (is_unsat) {
                m_ctx->push();
                m_ctx->assert_expr(A);
                for (unsigned i = 0; i < ilemmas.size(); ++i) {
                    m_ctx->assert_expr(ilemmas[i].get());
                }
                lbool res2 = m_ctx->check();
                SASSERT(l_false == res2);
                m_ctx->pop(1);
            }
            );
        return is_unsat;
    }

    //
    // Perform simple subsumption check of lemmas.
    //
    void farkas_learner::simplify_lemmas(expr_ref_vector& lemmas) {
        goal_ref g(alloc(goal, lemmas.get_manager(), false, false, false));
        TRACE("farkas_simplify_lemmas",            
              for (unsigned i = 0; i < lemmas.size(); ++i) {
                  tout << mk_pp(lemmas[i].get(), lemmas.get_manager()) << "\n";
              });

        for (unsigned i = 0; i < lemmas.size(); ++i) {
            g->assert_expr(lemmas[i].get()); 
        }
        model_converter_ref mc;
        proof_converter_ref pc;
        expr_dependency_ref core(m);
        goal_ref_buffer result;
        (*m_simplifier)(g, result, mc, pc, core);
        lemmas.reset();
        SASSERT(result.size() == 1);
        goal* r = result[0];
        for (unsigned i = 0; i < r->size(); ++i) {
            lemmas.push_back(r->form(i));
        }
    }


    void farkas_learner::combine_constraints(unsigned n, app * const * lits, rational const * coeffs, expr_ref& res)
    {
        constr res_c(m);
        for(unsigned i = 0; i < n; ++i) {
            res_c.add(coeffs[i], lits[i]);
        }
        res_c.get(res);
    }

    class farkas_learner::constant_replacer_cfg : public default_rewriter_cfg
    {
        ast_manager& m;
        const obj_map<expr, expr *>& m_translation;
    public:
        constant_replacer_cfg(ast_manager& m, const obj_map<expr, expr *>& translation)
            : m(m), m_translation(translation)
        { }

        bool get_subst(expr * s, expr * & t, proof * & t_pr) {
            return m_translation.find(s, t);
        }
    };

    // every uninterpreted symbol is in symbs
    class is_pure_expr_proc {
        func_decl_set const& m_symbs;
    public:
        struct non_pure {};

        is_pure_expr_proc(func_decl_set const& s):m_symbs(s) {}

        void operator()(app* a) {
            if (a->get_family_id() == null_family_id) {
                if (!m_symbs.contains(a->get_decl())) {
                    throw non_pure();
                }
            }
        }
        void operator()(var*) {}
        void operator()(quantifier*) {}
    };

    bool farkas_learner::is_pure_expr(func_decl_set const& symbs, expr* e) const {
        is_pure_expr_proc proc(symbs);
        try {
            for_each_expr(proc, e);
        }
        catch (is_pure_expr_proc::non_pure) {
            return false;
        }
        return true;
    };


    /**
       Revised version of Farkas strengthener.
       1. Mark B-pure nodes as derivations that depend only on B.
       2. Collect B-influenced nodes
       3. (optional) Permute B-pure units over resolution steps to narrow dependencies on B.
       4. Weaken B-pure units for resolution with Farkas Clauses.
       5. Add B-pure units elsewhere.

       Rules:
       - hypothesis h |- h

                    H |- false
       - lemma      ----------
                     |- not H

                    Th |- L \/ C   H |- not L
       - th-lemma   -------------------------
                           H  |- C

         Note: C is false for theory axioms, C is unit literal for propagation.

       - rewrite        |- t = s

                        H |- t = s
       - monotonicity   ----------------
                       H |- f(t) = f(s)

                        H |- t = s H' |- s = u
       - trans          ----------------------
                            H, H' |- t = u

                        H |- C \/ L  H' |- not L
       - unit_resolve   ------------------------
                                H, H' |- C

                        H |- a ~ b   H' |- a
       - mp             --------------------
                             H, H' |- b

       - def-axiom       |- C
       
       - asserted        |- f

       Mark nodes by:
          - Hypotheses
          - Dependency on bs
          - Dependency on A

       A node is unit derivable from bs if:
          - It has no hypotheses.
          - It depends on bs.
          - It does not depend on A.

       NB: currently unit derivable is not symmetric: A clause can be 
       unit derivable, but a unit literal with hypotheses is not.
       This is clearly wrong, because hypotheses are just additional literals
       in a clausal version.

       NB: the routine is not interpolating, though an interpolating variant would 
       be preferrable because then we can also use it for model propagation.

       We collect the unit derivable nodes from bs.
       These are the weakenings of bs, besides the 
       units under Farkas.
                    
    */
    void farkas_learner::get_lemmas(proof* root, expr_set const& bs, func_decl_set const& Bsymbs, expr* A, expr_ref_vector& lemmas) {

        proof_ref pr(root, m);
        permute_unit_resolution(pr);
        
        ptr_vector<expr_set> hyprefs;
        obj_map<expr, expr_set*> hypmap;
        ast_mark b_depend, a_depend, visited, b_closed;
        expr_set* empty_set = alloc(expr_set);
        hyprefs.push_back(empty_set); 
        ptr_vector<proof> todo;
        TRACE("pdr_verbose", tout << mk_pp(pr, m) << "\n";);
        todo.push_back(pr);
        while (!todo.empty()) {
            proof* p = todo.back();
            SASSERT(m.is_proof(p));
            if (visited.is_marked(p)) {
                todo.pop_back();
                continue;
            }
            bool all_visit = true;
            for (unsigned i = 0; i < m.get_num_parents(p); ++i) {
                expr* arg = p->get_arg(i);
                SASSERT(m.is_proof(arg));
                if (!visited.is_marked(arg)) {
                    all_visit = false;
                    todo.push_back(to_app(arg));
                }
            }
            if (!all_visit) {
                continue;
            }
            visited.mark(p, true);
            todo.pop_back();

            // retrieve hypotheses and dependencies on A, bs.
            bool b_dep = false, a_dep = false;
            expr_set* hyps = empty_set;
            for (unsigned i = 0; i < m.get_num_parents(p); ++i) {
                expr* arg = p->get_arg(i);
                a_dep = a_dep || a_depend.is_marked(arg);
                b_dep = b_dep || b_depend.is_marked(arg);
                expr_set* hyps2 = hypmap.find(arg);
                if (hyps != hyps2 && !hyps2->empty()) {
                    if (hyps->empty()) {
                        hyps = hyps2;
                    }
                    else {
                        expr_set* hyps3 = alloc(expr_set);
                        datalog::set_union(*hyps3, *hyps);
                        datalog::set_union(*hyps3, *hyps2);
                        hyps = hyps3;
                        hyprefs.push_back(hyps);
                    }
                }
            }
            hypmap.insert(p, hyps);
            a_depend.mark(p, a_dep);
            b_depend.mark(p, b_dep);

#define IS_B_PURE(_p) (b_depend.is_marked(_p) && !a_depend.is_marked(_p) && hypmap.find(_p)->empty())

            // Add lemmas that depend on bs, have no hypotheses, don't depend on A.
            if ((!hyps->empty() || a_depend.is_marked(p)) && 
                b_depend.is_marked(p) && !is_farkas_lemma(p)) {
                for (unsigned i = 0; i < m.get_num_parents(p); ++i) {                
                    app* arg = to_app(p->get_arg(i));
                    if (IS_B_PURE(arg)) {
                        if (is_pure_expr(Bsymbs, m.get_fact(arg))) {
                            TRACE("farkas_learner", 
                                  tout << "Add: " << mk_pp(m.get_fact(arg), m) << "\n";
                                  tout << mk_pp(arg, m) << "\n";
                                  );
                            lemmas.push_back(m.get_fact(arg));
                        }
                        else {
                            get_asserted(p, bs, b_closed, lemmas);
                            b_closed.mark(p, true);
                        }
                    }
                }
            }
            
            switch(p->get_decl_kind()) {
            case PR_ASSERTED:
                if (bs.contains(m.get_fact(p))) {
                    b_depend.mark(p, true);
                }
                else {
                    SASSERT(m.get_fact(p) == A);
                    a_depend.mark(p, true);
                }
                break;
            case PR_HYPOTHESIS: {
                SASSERT(hyps == empty_set);
                hyps = alloc(expr_set);
                hyps->insert(m.get_fact(p));
                hyprefs.push_back(hyps);
                hypmap.insert(p, hyps);
                break;
            }
            case PR_DEF_AXIOM: {
                if (!is_pure_expr(Bsymbs, m.get_fact(p))) {
                    a_depend.mark(p, true);
                }
                break;
            }
            case PR_LEMMA: {
                expr_set* hyps2 = alloc(expr_set);
                hyprefs.push_back(hyps2);
                datalog::set_union(*hyps2, *hyps); 
                hyps = hyps2;
                expr* fml = m.get_fact(p);
                hyps->remove(fml);
                if (m.is_or(fml)) {
                    for (unsigned i = 0; i < to_app(fml)->get_num_args(); ++i) {
                        expr* f = to_app(fml)->get_arg(i);
                        expr_ref hyp(m);
                        m_brwr.mk_not(f, hyp);
                        hyps->remove(hyp);
                    }
                }
                hypmap.insert(p, hyps);
                break;
            }
            case PR_TH_LEMMA: {
                if (!is_farkas_lemma(p)) break;
               
                SASSERT(m.has_fact(p));
                unsigned prem_cnt = m.get_num_parents(p);
                func_decl * d = p->get_decl();
                SASSERT(d->get_num_parameters() >= prem_cnt+2);
                SASSERT(d->get_parameter(0).get_symbol() == "arith");
                SASSERT(d->get_parameter(1).get_symbol() == "farkas");
                parameter const* params = d->get_parameters() + 2;

                app_ref_vector lits(m);
                expr_ref tmp(m);
                unsigned num_b_pures = 0;
                rational coef;
                vector<rational> coeffs;

                TRACE("farkas_learner", 
                        for (unsigned i = 0; i < prem_cnt; ++i) {
                            VERIFY(params[i].is_rational(coef));
                            proof* prem = to_app(p->get_arg(i));
                            bool b_pure = IS_B_PURE(prem);
                            tout << (b_pure?"B":"A") << " " << coef << " " << mk_pp(m.get_fact(prem), m) << "\n";
                        }
                        tout << mk_pp(m.get_fact(p), m) << "\n";
                        );

                // NB. Taking 'abs' of coefficients is a workaround.
                // The Farkas coefficient extraction in arith_core must be wrong.
                // The coefficients would be always positive relative to the theory lemma.

                for(unsigned i = 0; i < prem_cnt; ++i) {                    
                    expr * prem_e = p->get_arg(i);
                    SASSERT(is_app(prem_e));
                    proof * prem = to_app(prem_e);
                   
                    if(IS_B_PURE(prem)) {
                        ++num_b_pures;
                    }
                    else {
                        VERIFY(params[i].is_rational(coef));
                        lits.push_back(to_app(m.get_fact(prem)));
                        coeffs.push_back(abs(coef));
                    }
                }
                params += prem_cnt;
                if (prem_cnt + 2 < d->get_num_parameters()) {
                    unsigned num_args = 1;
                    expr* fact = m.get_fact(p);
                    expr* const* args = &fact;
                    if (m.is_or(fact)) {
                        app* _or = to_app(fact);
                        num_args = _or->get_num_args();
                        args = _or->get_args();                        
                    }
                    SASSERT(prem_cnt + 2 + num_args == d->get_num_parameters());
                    for (unsigned i = 0; i < num_args; ++i) {
                        expr* prem_e = args[i];
                        m_brwr.mk_not(prem_e, tmp);
                        VERIFY(params[i].is_rational(coef));
                        SASSERT(is_app(tmp));
                        lits.push_back(to_app(tmp));
                        coeffs.push_back(abs(coef));
                    }

                }
                SASSERT(coeffs.size() == lits.size());
                if (num_b_pures > 0) {
                    expr_ref res(m);
                    combine_constraints(coeffs.size(), lits.c_ptr(), coeffs.c_ptr(), res);
                    TRACE("farkas_learner", tout << "Add: " << mk_pp(res, m) << "\n";);
                    lemmas.push_back(res);    
                    b_closed.mark(p, true);
                }
            }
            default:
                break;
            }
        }

        std::for_each(hyprefs.begin(), hyprefs.end(), delete_proc<expr_set>());
    }

    void farkas_learner::get_asserted(proof* p, expr_set const& bs, ast_mark& b_closed, expr_ref_vector& lemmas) {
        ast_mark visited;
        proof* p0 = p;
        ptr_vector<proof> todo;        
        todo.push_back(p);
                      
        while (!todo.empty()) {
            p = todo.back();
            todo.pop_back();
            if (visited.is_marked(p) || b_closed.is_marked(p)) {
                continue;
            }
            visited.mark(p, true);
            for (unsigned i = 0; i < m.get_num_parents(p); ++i) {
                expr* arg = p->get_arg(i);
                SASSERT(m.is_proof(arg));
                todo.push_back(to_app(arg));
            }
            if (p->get_decl_kind() == PR_ASSERTED &&
                bs.contains(m.get_fact(p))) {
                TRACE("farkas_learner", 
                      tout << mk_ll_pp(p0,m) << "\n";
                      tout << "Add: " << mk_pp(p,m) << "\n";);
                lemmas.push_back(m.get_fact(p));
                b_closed.mark(p, true);
            }
        }
    }

    // permute unit resolution over Theory lemmas to track premises.
    void farkas_learner::permute_unit_resolution(proof_ref& pr) {
        expr_ref_vector refs(m);
        obj_map<proof,proof*> cache;
        permute_unit_resolution(refs, cache, pr);
    }
    void farkas_learner::permute_unit_resolution(expr_ref_vector& refs, obj_map<proof,proof*>& cache, proof_ref& pr) {
        proof* pr2 = 0;
        proof_ref_vector parents(m);
        proof_ref prNew(pr); 
        if (cache.find(pr, pr2)) {
            pr = pr2;
            return;
        }

        for (unsigned i = 0; i < m.get_num_parents(pr); ++i) {
            prNew = m.get_parent(pr, i);
            permute_unit_resolution(refs, cache, prNew);
            parents.push_back(prNew);
        }

        prNew = pr;
        if (pr->get_decl_kind() == PR_UNIT_RESOLUTION &&
            parents[0]->get_decl_kind() == PR_TH_LEMMA) { 
                /*
                Unit resolution:
                    T1:      (or l_1 ... l_n l_1' ... l_m')
                    T2:      (not l_1)
                    ...
                    T(n+1):  (not l_n)
                    [unit-resolution T1 ... T(n+1)]: (or l_1' ... l_m')
                Th lemma:
                    T1:      (not l_1)
                    ...
                    Tn:      (not l_n)
                    [th-lemma T1 ... Tn]: (or l_{n+1} ... l_m)

                    Such that (or l_1 .. l_n l_{n+1} .. l_m) is a theory axiom.

                Implement conversion:

                    T1 |- not l_1 ... Tn |- not l_n  
                    -------------------------------  TH_LEMMA
                           (or k_1 .. k_m j_1 ... j_m)    S1 |- not k_1 ... Sm |- not k_m
                           -------------------------------------------------------------- UNIT_RESOLUTION
                                         (or j_1 .. j_m)


                    |-> 

                    T1 |- not l_1 ... Tn |- not l_n S1 |- not k_1 ... Sm |- not k_m
                    ---------------------------------------------------------------- TH_LEMMA
                                        (or j_1 .. j_m)

                */
                proof_ref_vector premises(m);
                proof* thLemma = parents[0].get();
                for (unsigned i = 0; i < m.get_num_parents(thLemma); ++i) {
                    premises.push_back(m.get_parent(thLemma, i));
                }
                for (unsigned i = 1; i < parents.size(); ++i) {
                    premises.push_back(parents[i].get());
                }
                parameter const* params = thLemma->get_decl()->get_parameters();
                unsigned num_params = thLemma->get_decl()->get_num_parameters();
                SASSERT(params[0].is_symbol());
                family_id tid = m.get_family_id(params[0].get_symbol());
                SASSERT(tid != null_family_id);
                prNew = m.mk_th_lemma(tid, m.get_fact(pr), 
                                      premises.size(), premises.c_ptr(), num_params-1, params+1);
        }
        else {
            ptr_vector<expr> args;
            for (unsigned i = 0; i < parents.size(); ++i) {
                args.push_back(parents[i].get());
            }
            if (m.has_fact(pr)) {
                args.push_back(m.get_fact(pr));
            }
            prNew = m.mk_app(pr->get_decl(), args.size(), args.c_ptr());
        }    
      
        cache.insert(pr, prNew);
        refs.push_back(prNew);
        pr = prNew;
    }
	

    bool farkas_learner::is_farkas_lemma(expr* e) {
        app * a;
        func_decl* d;
        symbol sym;
        return 
            is_app(e) && 
            (a = to_app(e), d = a->get_decl(), true) &&
            PR_TH_LEMMA == a->get_decl_kind() &&
            d->get_num_parameters() >= 2 &&
            d->get_parameter(0).is_symbol(sym) && sym == "arith" &&
            d->get_parameter(1).is_symbol(sym) && sym == "farkas" &&
            d->get_num_parameters() >= m.get_num_parents(to_app(e)) + 2;
    };


    void farkas_learner::test()  {
        front_end_params params;
        enable_trace("farkas_learner");
               
        bool res;
        ast_manager m;
        m.register_decl_plugins();        
        arith_util a(m);
        pdr::farkas_learner fl(params, m);
        expr_ref_vector lemmas(m);
        
        sort_ref int_s(a.mk_int(), m);
        expr_ref x(m.mk_const(symbol("x"), int_s), m);
        expr_ref y(m.mk_const(symbol("y"), int_s), m);
        expr_ref z(m.mk_const(symbol("z"), int_s), m);    
        expr_ref u(m.mk_const(symbol("u"), int_s), m);  
        expr_ref v(m.mk_const(symbol("v"), int_s), m);

        // A: x > y >= z
        // B: x < z
        // Farkas: x <= z
        expr_ref A(m.mk_and(a.mk_gt(x,y), a.mk_ge(y,z)),m);        
        expr_ref B(a.mk_gt(z,x),m);        
        res = fl.get_lemma_guesses(A, B, lemmas);        
        std::cout << "\nres: " << res << "\nlemmas: " << pp_cube(lemmas, m) << "\n";

        // A: x > y >= z + 2
        // B: x = 1, z = 8
        // Farkas: x <= z + 2
        expr_ref one(a.mk_numeral(rational(1), true), m);
        expr_ref two(a.mk_numeral(rational(2), true), m);
        expr_ref eight(a.mk_numeral(rational(8), true), m);
        A = m.mk_and(a.mk_gt(x,y),a.mk_ge(y,a.mk_add(z,two)));
        B = m.mk_and(m.mk_eq(x,one), m.mk_eq(z, eight));
        res = fl.get_lemma_guesses(A, B, lemmas);        
        std::cout << "\nres: " << res << "\nlemmas: " << pp_cube(lemmas, m) << "\n";

        // A: x > y >= z \/ x >= u > z
        // B: z > x + 1
        // Farkas: z >= x
        A = m.mk_or(m.mk_and(a.mk_gt(x,y),a.mk_ge(y,z)),m.mk_and(a.mk_ge(x,u),a.mk_gt(u,z)));
        B = a.mk_gt(z, a.mk_add(x,one));
        res = fl.get_lemma_guesses(A, B, lemmas);        
        std::cout << "\nres: " << res << "\nlemmas: " << pp_cube(lemmas, m) << "\n";

        // A: (x > y >= z \/ x >= u > z \/ u > v)
        // B: z > x + 1 & not (u > v)
        // Farkas: z >= x & not (u > v)
        A = m.mk_or(m.mk_and(a.mk_gt(x,y),a.mk_ge(y,z)),m.mk_and(a.mk_ge(x,u),a.mk_gt(u,z)), a.mk_gt(u, v));
        B = m.mk_and(a.mk_gt(z, a.mk_add(x,one)), m.mk_not(a.mk_gt(u, v)));
        res = fl.get_lemma_guesses(A, B, lemmas);        
        std::cout << "\nres: " << res << "\nlemmas: " << pp_cube(lemmas, m) << "\n";
        
    }

    void farkas_learner::collect_statistics(statistics& st) const {
        if (m_ctx) {
            m_ctx->collect_statistics(st);
        }
    }


    void farkas_learner::test(char const* filename)  {
        if (!filename) {
            test();
            return;
        }
        ast_manager m;
        m.register_decl_plugins();  
        scoped_ptr<smtlib::parser> p = smtlib::parser::create(m);
        p->initialize_smtlib();

        if (!p->parse_file(filename)) {
            warning_msg("Failed to parse file %s\n", filename);
            return;
        }
        expr_ref A(m), B(m);

        smtlib::theory::expr_iterator it = p->get_benchmark()->begin_axioms();
        smtlib::theory::expr_iterator end = p->get_benchmark()->end_axioms();
        A = m.mk_and(static_cast<unsigned>(end-it), it);

        it = p->get_benchmark()->begin_formulas();
        end = p->get_benchmark()->end_formulas();
        B = m.mk_and(static_cast<unsigned>(end-it), it);

        front_end_params params;
        pdr::farkas_learner fl(params, m);
        expr_ref_vector lemmas(m);
        bool res = fl.get_lemma_guesses(A, B, lemmas);        
        std::cout << "lemmas: " << pp_cube(lemmas, m) << "\n";
    }
};

