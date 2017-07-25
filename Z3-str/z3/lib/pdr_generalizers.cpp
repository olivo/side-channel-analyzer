/*++
Copyright (c) 2011 Microsoft Corporation

Module Name:

    pdr_generalizers.cpp

Abstract:

    Generalizers of satisfiable states and unsat cores.

Author:

    Nikolaj Bjorner (nbjorner) 2011-11-20.

Revision History:

--*/


#include "pdr_context.h"
#include "pdr_farkas_learner.h"
#include "pdr_interpolant_provider.h"
#include "pdr_generalizers.h"
#include "expr_abstract.h"
#include "var_subst.h"


namespace pdr {

    static void solve_for_next_vars(expr_ref& F, model_node& n, expr_substitution& sub) {
        ast_manager& m = F.get_manager();
        manager& pm = n.pt().get_pdr_manager();
        const model_core & mdl = n.model();
        unsigned sz = mdl.get_num_constants();
        expr_ref_vector refs(m);

        for (unsigned i = 0; i < sz; i++) {
             func_decl * d = mdl.get_constant(i);    
             expr_ref interp(m);
             ptr_vector<app> cs;
             if (m.is_bool(d->get_range())) {
                get_value_from_model(mdl, d, interp);  
                app* c = m.mk_const(d);
                refs.push_back(c);
                refs.push_back(interp);
                sub.insert(c, interp);
             }
        }
        scoped_ptr<expr_replacer> rep = mk_default_expr_replacer(m);
        rep->set_substitution(&sub);
        (*rep)(F);
        th_rewriter rw(m);
        rw(F);
        ptr_vector<expr> todo;
        todo.push_back(F);
        expr* e1, *e2;
        while (!todo.empty()) {
            expr* e = todo.back();
            todo.pop_back();
            if (m.is_and(e)) {
                todo.append(to_app(e)->get_num_args(), to_app(e)->get_args());
            }
            else if ((m.is_eq(e, e1, e2) && pm.is_n(e1) && pm.is_o_formula(e2)) ||
                     (m.is_eq(e, e2, e1) && pm.is_n(e1) && pm.is_o_formula(e2))) {
                sub.insert(e1, e2);
                TRACE("pdr", tout << mk_pp(e1, m) << " |-> " << mk_pp(e2, m) << "\n";);
            }
        }
    }

    //
    // eliminate conjuncts from cube as long as state is satisfied.
    // 
    void model_evaluation_generalizer::operator()(model_node& n, expr_ref_vector& cube) {
        ptr_vector<expr> forms;
        forms.push_back(n.state());
        forms.push_back(n.pt().transition());
        m_model_evaluator.minimize_model(forms, cube);
    }

    //
    // eliminate conjuncts from cube as long as state is satisfied.
    // 
    void bool_model_evaluation_generalizer::operator()(model_node& n, expr_ref_vector& cube) {
        ptr_vector<expr> forms;
        forms.push_back(n.state());
        forms.push_back(n.pt().transition());
        m_model_evaluator.minimize_model(forms, cube);
    }

    //
    // main propositional induction generalizer.
    // drop literals one by one from the core and check if the core is still inductive.
    //    
    void core_bool_inductive_generalizer::operator()(model_node& n, expr_ref_vector& core, bool& uses_level) {
        if (core.size() <= 1) {
            return;
        }
        ast_manager& m = core.get_manager();
        TRACE("pdr", for (unsigned i = 0; i < core.size(); ++i) { tout << mk_pp(core[i].get(), m) << "\n"; } "\n";);
        unsigned num_failures = 0, i = 0, num_changes = 0;
        while (i < core.size() && (!m_failure_limit || num_failures <= m_failure_limit)) {
            expr_ref lit(m), state(m);
            lit = core[i].get();
            core[i] = m.mk_true();
            state = m.mk_not(n.pt().get_pdr_manager().mk_and(core));
            bool assumes_level = false;
            if (n.pt().is_invariant(n.level(), state, true, assumes_level, 0)) {
                num_failures = 0;
                core[i] = core.back();
                core.pop_back();
                TRACE("pdr", tout << "Remove: " << mk_pp(lit, m) << "\n";);
                ++num_changes;
                uses_level = assumes_level;
            }
            else {
                core[i] = lit;
                ++num_failures;
                ++i;
            }
        }
        TRACE("pdr", tout << "changes: " << num_changes << " index: " << i << " size: " << core.size() << "\n";);
    }

    //
    // extract multiple cores from unreachable state.
    //
    void core_multi_generalizer::operator()(model_node& n, expr_ref_vector& core, bool& uses_level) {
        ast_manager& m = core.get_manager();
        manager& pm = n.pt().get_pdr_manager();
        expr_ref_vector states(m), cores(m);
        datalog::flatten_and(n.state(), states);
        obj_hashtable<expr> core_exprs;
        for (unsigned i = 0; i < core.size(); ++i) {
            core_exprs.insert(core[i].get());
        }
        TRACE("pdr", 
            tout << "Old core:\n"; for (unsigned i = 0; i < core.size(); ++i) tout << mk_pp(core[i].get(), m) << "\n";
            tout << "State:\n"; for (unsigned i = 0; i < states.size(); ++i) tout << mk_pp(states[i].get(), m) << "\n";
        );
        cores.push_back(pm.mk_and(core));
        bool change = false;
        for (unsigned i = 0; i < states.size(); ++i) {
            expr_ref tmp(states[i].get(), m), new_state(m);
            expr_ref_vector new_core(m);
            if (core_exprs.contains(tmp.get())) {
                states[i] = m.mk_true();
                new_state = m.mk_not(pm.mk_and(states));
                bool assumes_level;
                if (n.pt().is_invariant(n.level(), new_state, false, assumes_level, &new_core)) {
#if 0
                    for (unsigned j = 0; j < new_core.size(); ++j) {
                        core_exprs.insert(new_core[j].get());
                    }
#endif
                    if (assumes_level) {
                        uses_level = true;
                    }
                    tmp = pm.mk_and(new_core);
                    TRACE("pdr", tout << "New core:\n" << mk_pp(tmp, m) << "\n";);
                    cores.push_back(tmp);
                    change = true;
                }
                else {
                    states[i] = tmp;
                }
            }
        }
        if (change) {
            core.reset();
            core.push_back(pm.mk_or(cores));
            TRACE("pdr", tout << "New Cores:\n" << mk_pp(core[0].get(), m) << "\n";);
        }
    }

    // 
    // for each disjunct of core:
    //     weaken predecessor.
    //    

    core_farkas_generalizer::core_farkas_generalizer(context& ctx, ast_manager& m, front_end_params& p):
        core_generalizer(ctx), 
        m_farkas_learner(p, m) 
    {}
    
    void core_farkas_generalizer::operator()(model_node& n, expr_ref_vector& core, bool& uses_level) {
        front_end_params& p = m_ctx.get_fparams();
        ast_manager& m  = n.pt().get_manager();
        manager& pm = n.pt().get_pdr_manager();
        if (core.empty()) return;
        expr_ref A(m), B(pm.mk_and(core)), C(m);
        expr_ref_vector Bs(m);
        pm.get_or(B, Bs);
        A = n.pt().get_propagation_formula(m_ctx.get_pred_transformers(), n.level());

        bool change = false;
        for (unsigned i = 0; i < Bs.size(); ++i) {
            expr_ref_vector lemmas(m);
            C = Bs[i].get();
            if (m_farkas_learner.get_lemma_guesses(A, B, lemmas)) {
                TRACE("pdr", 
                      tout << "Old core:\n" << mk_pp(B, m) << "\n";
                      tout << "New core:\n" << mk_pp(pm.mk_and(lemmas), m) << "\n";);            
                Bs[i] = pm.mk_and(lemmas);
                change = true;
            }
        }
        if (change) {
            C = pm.mk_or(Bs);
            TRACE("pdr", tout << "prop:\n" << mk_pp(A,m) << "\ngen:" << mk_pp(B, m) << "\nto: " << mk_pp(C, m) << "\n";);
            core.reset();
            datalog::flatten_and(C, core);    
            uses_level = true;
        }    
    }

    void core_farkas_generalizer::collect_statistics(statistics& st) const {
        m_farkas_learner.collect_statistics(st);
    }

    void model_precond_generalizer::operator()(model_node& n, expr_ref_vector& cube) {
        ast_manager& m  = n.pt().get_manager();
        manager& pm = n.pt().get_pdr_manager();
        expr_ref A(m), state(m);
        expr_ref_vector states(m);
        A = n.pt().get_formulas(n.level(), true);   

        // extract substitution for next-state variables.
        expr_substitution sub(m);        
        solve_for_next_vars(A, n, sub);
        scoped_ptr<expr_replacer> rep = mk_default_expr_replacer(m);
        rep->set_substitution(&sub);
        A = m.mk_and(A, n.state());
        (*rep)(A);                

        datalog::flatten_and(A, states);

        for (unsigned i = 0; i < states.size(); ++i) {
            expr* s = states[i].get();
            if (pm.is_o_formula(s) && pm.is_homogenous_formula(s)) {
                cube.push_back(s);
            }
        }
        TRACE("pdr", for (unsigned i = 0; i < cube.size(); ++i) tout << mk_pp(cube[i].get(), m) << "\n";);
    }

    /**
              < F, phi, i + 1 >
             /                 \
       < G, psi, i >       < H, theta, i >
          core

      Given:
        1. psi => core
        2. Gi => not core
        3. phi & psi & theta => F_{i+1}

      Then, by weakening 2:
        Gi => (F_{i+1} => not (phi & core & theta))
            
      Find interpolant I, such that

        Gi => I, I => (F_{i+1} => not (phi & core' & theta'))
      
      where core => core', theta => theta'

      This implementation checks if

        Gi => (F_{i+1} => not (phi & theta))

    */
    void core_interpolant_generalizer::operator()(model_node& n, expr_ref_vector& core, bool& uses_level) {
        if (!n.parent()) {
            return;
        }
        manager& pm = n.pt().get_pdr_manager();
        ast_manager& m = n.pt().get_manager();
        model_node&  p = *n.parent();

        // find index of node into parent.
        unsigned index = 0;
        for (; index < p.children().size() && (&n != p.children()[index]); ++index);
        SASSERT(index < p.children().size());

        expr_ref G(m), F(m), r(m), B(m), I(m), cube(m);
        expr_ref_vector fmls(m);

        F = p.pt().get_formulas(p.level(), true);
        G = n.pt().get_formulas(n.level(), true);
        pm.formula_n2o(index, false, G);

        // get formulas from siblings.
        for (unsigned i = 0; i < p.children().size(); ++i) {
            if (i != index) {
                pm.formula_n2o(p.children()[i]->state(), r, i, true);
                fmls.push_back(r);
            }
        }
        fmls.push_back(F);
        fmls.push_back(p.state());
        B = pm.mk_and(fmls);        
        
        // when G & B is unsat, find I such that G => I, I => not B
        lbool res = pm.get_interpolator().get_interpolant(G, B, I);

        TRACE("pdr", 
              tout << "Interpolating:\n" << mk_pp(G, m) << "\n" << mk_pp(B, m) << "\n";
              if (res == l_true) tout << mk_pp(I, m) << "\n"; else tout << "failed\n";);

        if(res == l_true) {
            pm.formula_o2n(I, cube, index, true);
            TRACE("pdr", tout << "After renaming: " << mk_pp(cube, m) << "\n";);
            core.reset();
            datalog::flatten_and(cube, core);
            uses_level = true;
        }
    }


    // 
    //     < F, phi, i + 1> 
    //             |
    //      < G, psi, i >
    // 
    // where:
    //
    //  p(x) <- F(x,y,p,q)
    //  q(x) <- G(x,y)
    // 
    // Hyp:
    //  Q_k(x) => phi(x)           j <= k <= i
    //  Q_k(x) => R_k(x)           j <= k <= i + 1
    //  Q_k(x) <=> Trans(Q_{k-1})  j <  k <= i + 1
    // Conclusion:
    //  Q_{i+1}(x) => phi(x)
    //
    class core_induction_generalizer::imp {
        context&     m_ctx;
        manager& pm;
        ast_manager& m;

        //
        //  Create predicate Q_level
        // 
        func_decl_ref mk_pred(unsigned level, func_decl* f) {
            func_decl_ref result(m);
            std::ostringstream name;
            name << f->get_name() << "_" << level;
            symbol sname(name.str().c_str());
            result = m.mk_func_decl(sname, f->get_arity(), f->get_domain(), f->get_range());
            return result;
        }

        // 
        // Create formula exists y . z . F[Q_{level-1}, x, y, z]
        //
        expr_ref mk_transition_rule(
            expr_ref_vector const& reps, 
            unsigned level, 
            datalog::rule const& rule) 
        {
            expr_ref_vector conj(m), sub(m);
            expr_ref result(m);
            ptr_vector<sort> sorts;
            svector<symbol> names;
            unsigned ut_size = rule.get_uninterpreted_tail_size();
            unsigned t_size = rule.get_tail_size();              
            if (0 == level && 0 < ut_size) {
                result = m.mk_false();
                return result;
            }
            app* atom = rule.get_head();
            SASSERT(atom->get_num_args() == reps.size());
            
            for (unsigned i = 0; i < reps.size(); ++i) {
                expr* arg = atom->get_arg(i);
                if (is_var(arg)) {
                    unsigned idx = to_var(arg)->get_idx();
                    if (idx >= sub.size()) sub.resize(idx+1);
                    if (sub[idx].get()) {
                        conj.push_back(m.mk_eq(sub[idx].get(), reps[i]));
                    }
                    else {
                        sub[idx] = reps[i];
                    }
                }
                else {
                    conj.push_back(m.mk_eq(arg, reps[i]));
                }
            }
            for (unsigned i = 0; 0 < level && i < ut_size; i++) {
                app* atom = rule.get_tail(i);
                func_decl* head = atom->get_decl();
                func_decl_ref fn = mk_pred(level-1, head);
                conj.push_back(m.mk_app(fn, atom->get_num_args(), atom->get_args()));
            }                        
            for (unsigned i = ut_size; i < t_size; i++) {
                conj.push_back(rule.get_tail(i));
            }         
            result = pm.mk_and(conj);
            if (!sub.empty()) {
                expr_ref tmp = result;
                var_subst(m, false)(tmp, sub.size(), sub.c_ptr(), result);
            }
            get_free_vars(result, sorts);         
            for (unsigned i = 0; i < sorts.size(); ++i) {
                if (!sorts[i]) {
                    sorts[i] = m.mk_bool_sort();
                }
                names.push_back(symbol(sorts.size() - i - 1));
            }
            if (!sorts.empty()) {
                sorts.reverse();
                result = m.mk_exists(sorts.size(), sorts.c_ptr(), names.c_ptr(), result); 
            }            
            return result;
        }

        expr_ref bind_head(expr_ref_vector const& reps, expr* fml) {
            expr_ref result(m);
            expr_abstract(m, 0, reps.size(), reps.c_ptr(), fml, result);
            ptr_vector<sort> sorts;
            svector<symbol>  names;
            unsigned sz = reps.size();
            for (unsigned i = 0; i < sz; ++i) {
                sorts.push_back(m.get_sort(reps[sz-i-1]));
                names.push_back(symbol(sz-i-1));
            }
            if (sz > 0) {
                result = m.mk_forall(sorts.size(), sorts.c_ptr(), names.c_ptr(), result);
            }
            return result;
        }

        expr_ref_vector mk_reps(pred_transformer& pt) {
            expr_ref_vector reps(m);
            expr_ref rep(m);
            for (unsigned i = 0; i < pt.head()->get_arity(); ++i) {
                rep = m.mk_const(pm.o2n(pt.sig(i), 0));
                reps.push_back(rep);
            }
            return reps;
        }

        //
        // extract transition axiom:
        // 
        //  forall x . p_lvl(x) <=> exists y z . F[p_{lvl-1}(y), q_{lvl-1}(z), x]
        //
        expr_ref mk_transition_axiom(pred_transformer& pt, unsigned level) {
            expr_ref fml(m.mk_false(), m), tr(m);
            expr_ref_vector reps = mk_reps(pt);
            ptr_vector<datalog::rule> const& rules = pt.rules();
            for (unsigned i = 0; i < rules.size(); ++i) {
                tr = mk_transition_rule(reps, level, *rules[i]);
                fml = (i == 0)?tr.get():m.mk_or(fml, tr);
            }
            func_decl_ref fn = mk_pred(level, pt.head());
            fml = m.mk_iff(m.mk_app(fn, reps.size(), reps.c_ptr()), fml);
            fml = bind_head(reps, fml);
            return fml;
        }

        // 
        // Create implication:
        //  Q_level(x) => phi(x)    
        //
        expr_ref mk_predicate_property(unsigned level, pred_transformer& pt, expr* phi) {
            expr_ref_vector reps = mk_reps(pt);
            func_decl_ref fn = mk_pred(level, pt.head());
            expr_ref fml(m);
            fml = m.mk_implies(m.mk_app(fn, reps.size(), reps.c_ptr()), phi);
            fml = bind_head(reps, fml);
            return fml;            
        }



    public:
        imp(context& ctx): m_ctx(ctx), pm(ctx.get_pdr_manager()), m(ctx.get_manager()) {}

        // 
        // not exists y . F(x,y)
        // 
        expr_ref mk_blocked_transition(pred_transformer& pt, unsigned level) {
            SASSERT(level > 0);
            expr_ref fml(m.mk_true(), m);
            expr_ref_vector reps = mk_reps(pt), fmls(m);
            ptr_vector<datalog::rule> const& rules = pt.rules();
            for (unsigned i = 0; i < rules.size(); ++i) {
                fmls.push_back(m.mk_not(mk_transition_rule(reps, level, *rules[i])));           
            }
            fml = pm.mk_and(fmls);
            TRACE("pdr", tout << mk_pp(fml, m) << "\n";);
            return fml;
        }

        expr_ref mk_induction_goal(pred_transformer& pt, unsigned level, unsigned depth) {
            SASSERT(level >= depth);
            expr_ref_vector conjs(m);
            ptr_vector<pred_transformer> pts;
            unsigned_vector levels;
            // negated goal
            expr_ref phi = mk_blocked_transition(pt, level);
            conjs.push_back(m.mk_not(mk_predicate_property(level, pt, phi)));
            pts.push_back(&pt);
            levels.push_back(level);
            // Add I.H. 
            for (unsigned lvl = level-depth; lvl < level; ++lvl) {
                if (lvl > 0) {
                    expr_ref psi = mk_blocked_transition(pt, lvl);
                    conjs.push_back(mk_predicate_property(lvl, pt, psi));
                    pts.push_back(&pt);
                    levels.push_back(lvl);
                }
            }
            // Transitions:
            for (unsigned qhead = 0; qhead < pts.size(); ++qhead) {
                pred_transformer& qt = *pts[qhead];
                unsigned lvl = levels[qhead];

                // Add transition definition and properties at level.
                conjs.push_back(mk_transition_axiom(qt, lvl));
                conjs.push_back(mk_predicate_property(lvl, qt, qt.get_formulas(lvl, true)));
                
                // Enqueue additional hypotheses
                ptr_vector<datalog::rule> const& rules = qt.rules();
                if (lvl + depth < level || lvl == 0) {
                    continue;
                }
                for (unsigned i = 0; i < rules.size(); ++i) {
                    datalog::rule& r = *rules[i];
                    unsigned ut_size = r.get_uninterpreted_tail_size();
                    for (unsigned j = 0; j < ut_size; ++j) {
                        func_decl* f = r.get_tail(j)->get_decl();
                        pred_transformer* rt = m_ctx.get_pred_transformers().find(f);
                        bool found = false;
                        for (unsigned k = 0; !found && k < levels.size(); ++k) {
                            found = (rt == pts[k] && levels[k] + 1 == lvl);
                        }
                        if (!found) {
                            levels.push_back(lvl-1);
                            pts.push_back(rt);
                        }
                    }
                }                
            }

            expr_ref result = pm.mk_and(conjs);
            TRACE("pdr", tout << mk_pp(result, m) << "\n";);
            return result;
        }
    };

    //
    // Instantiate Peano induction schema.
    // 
    void core_induction_generalizer::operator()(model_node& n, expr_ref_vector& core, bool& uses_level) {
        model_node* p = n.parent();
        if (p == 0) {
            return;
        }
        unsigned depth = 2;
        imp imp(m_ctx);
        ast_manager& m = core.get_manager();
        expr_ref goal = imp.mk_induction_goal(p->pt(), p->level(), depth);
        smt::solver ctx(m, m_ctx.get_fparams(), m_ctx.get_params());
        ctx.assert_expr(goal);
        lbool r = ctx.check();
        TRACE("pdr", tout << r << "\n";
                     for (unsigned i = 0; i < core.size(); ++i) {
                         tout << mk_pp(core[i].get(), m) << "\n";
                     });
        if (r == l_false) {
            core.reset();
            expr_ref phi = imp.mk_blocked_transition(p->pt(), p->level());
            core.push_back(m.mk_not(phi));
            uses_level = true;
        }
    }


    //
    // cube => n.state() & formula
    // so n.state() & cube & ~formula is unsat
    // so weaken cube while result is still unsat.
    //
    void model_farkas_generalizer::operator()(model_node& n, expr_ref_vector& cube) {
        ast_manager& m  = n.pt().get_manager();
        manager& pm = n.pt().get_pdr_manager();
        front_end_params& p = m_ctx.get_fparams();
        farkas_learner learner(p, m);
        expr_ref A0(m), A(m), B(m), state(m);
        expr_ref_vector states(m);

        A0 = n.pt().get_formulas(n.level(), true);   

        // extract substitution for next-state variables.
        expr_substitution sub(m);        
        solve_for_next_vars(A0, n, sub);
        scoped_ptr<expr_replacer> rep = mk_default_expr_replacer(m);
        rep->set_substitution(&sub);
        (*rep)(A0);        
        A0 = m.mk_not(A0);
        
        state = n.state();
        (*rep)(state);

        datalog::flatten_and(state, states);

        ptr_vector<func_decl> preds;
        n.pt().find_predecessors(n.model(), preds);

        TRACE("pdr", for (unsigned i = 0; i < cube.size(); ++i) tout << mk_pp(cube[i].get(), m) << "\n";);

        for (unsigned i = 0; i < preds.size(); ++i) {            
            pred_transformer& pt = m_ctx.get_pred_transformer(preds[i]);
            SASSERT(pt.head() == preds[i]);              
            expr_ref_vector lemmas(m), o_cube(m), other(m), o_state(m), other_state(m);
            pm.partition_o_atoms(cube,   o_cube,  other, i);
            pm.partition_o_atoms(states, o_state, other_state, i);
            TRACE("pdr", 
                  tout << "cube:\n";
                  for (unsigned i = 0; i < cube.size(); ++i)    tout << mk_pp(cube[i].get(), m) << "\n";
                  tout << "o_cube:\n";
                  for (unsigned i = 0; i < o_cube.size(); ++i)  tout << mk_pp(o_cube[i].get(), m) << "\n";
                  tout << "other:\n";
                  for (unsigned i = 0; i < other.size(); ++i)   tout << mk_pp(other[i].get(), m) << "\n";
                  tout << "o_state:\n";
                  for (unsigned i = 0; i < o_state.size(); ++i) tout << mk_pp(o_state[i].get(), m) << "\n";
                  tout << "other_state:\n";
                  for (unsigned i = 0; i < other_state.size(); ++i) tout << mk_pp(other_state[i].get(), m) << "\n";
            );
            A = m.mk_and(A0, pm.mk_and(other), pm.mk_and(other_state));
            B = m.mk_and(pm.mk_and(o_cube), pm.mk_and(o_state));

            TRACE("pdr", 
                  tout << "A: " << mk_pp(A, m) << "\n";
                  tout << "B: " << mk_pp(B, m) << "\n";);
                     
            if (learner.get_lemma_guesses(A, B, lemmas)) {
                cube.append(lemmas);
                cube.append(o_state);
                TRACE("pdr", 
                      tout << "New lemmas:\n";
                      for (unsigned i = 0; i < lemmas.size(); ++i) {
                          tout << mk_pp(lemmas[i].get(), m) << "\n";
                      }
                      );                
            }          
        }
        TRACE("pdr", for (unsigned i = 0; i < cube.size(); ++i) tout << mk_pp(cube[i].get(), m) << "\n";);
    }

    //
    // use properties from predicate transformer to strengthen state.
    //
    void core_farkas_properties_generalizer::operator()(model_node& n, expr_ref_vector& core, bool& uses_level) {
        if (core.empty()) return;

        front_end_params& p = m_ctx.get_fparams();
        ast_manager& m = n.pt().get_manager();
        manager& pm = n.pt().get_pdr_manager();
        expr_ref_vector lemmas(m), properties(m);
        expr_ref A(m), B(m);
        farkas_learner learner(p, m);
        n.get_properties(properties);
        A = n.pt().get_propagation_formula(m_ctx.get_pred_transformers(), n.level());
        B = m.mk_and(properties.size(), properties.c_ptr());
        if (learner.get_lemma_guesses(A, B, lemmas)) {
            TRACE("pdr", 
                  tout << "Properties:\n";
                  for (unsigned i = 0; i < properties.size(); ++i) {
                      tout << mk_pp(properties[i].get(), m) << "\n";
                  }
                  tout << mk_pp(B, m) << "\n";
                  tout << "New core:\n";
                  for (unsigned i = 0; i < lemmas.size(); ++i) {
                      tout << mk_pp(lemmas[i].get(), m) << "\n";
                  });
            core.append(lemmas); // disjunction?
            uses_level = true;
        }
    }

};



#if 0

    void model_farkas_generalizer::extract_eqs(expr_ref_vector const& lits, eqs& eqs) {
        expr* e;
        rational vl;
        bool is_int;
        for (unsigned i = 0; i < lits.size(); ++i) {
            if (is_eq(lits[i], e, vl, is_int)) {                
                eqs.push_back(std::make_pair(e, vl));
            }
        }        
    }

    void model_farkas_generalizer::solve_eqs(expr_ref& A, expr_ref_vector& other, eqs const& o_eqs) {
        if (o_eqs.empty()) {
            return;
        }
        
        expr_substitution sub(m);        
        scoped_ptr<expr_replacer> rep = mk_default_expr_replacer(m);
        rep->set_substitution(&sub);

        expr* e1;
        rational vl1;
        bool is_int;
        for (unsigned i = 0; i < other.size(); ++i) {
            if (is_eq(other[i].get(), e1, vl1, is_int)) {
                unsigned k = m_random(o_eqs.size());
                for (unsigned j = 0; j < o_eqs.size(); ++j) {
                    unsigned l = (j+k)%o_eqs.size();
                    expr* e2 = o_eqs[l].first;
                    if (m.get_sort(e1) == m.get_sort(e2)) {
                        sub.insert(e1, mk_offset(e2, vl1-o_eqs[l].second));
                        other[i] = m.mk_true();
                        break;
                    }
                }                
            }
        }
        (*rep)(A);
        other.push_back(A);
        A = m.mk_and(other.size(), other.c_ptr());
    }

    expr* model_farkas_generalizer::mk_offset(expr* e, rational const& r) {
        if (r.is_zero()) {
            return e;
        }
        else {
            return a.mk_add(e, a.mk_numeral(r, a.is_int(e)));
        }
    }

    void model_farkas_generalizer::connect_vars(ptr_vector<expr>& vars, vector<rational>& vals, expr_ref_vector& lits) {
        switch(vars.size()) {
        case 0: 
            break;
        case 1: 
            lits.push_back(m.mk_eq(vars[0], a.mk_numeral(vals[0], a.is_int(vars[0])))); 
            break;
        case 2:
            lits.push_back(m.mk_eq(vars[0], a.mk_numeral(vals[0], a.is_int(vars[0])))); 
            lits.push_back(m.mk_eq(vars[0], mk_offset(vars[1], vals[0]-vals[1])));
            break;
        default: {
            ptr_vector<expr> new_vars;
            vector<rational> new_vals;
            unsigned j, i = m_random(vars.size());
            new_vars.push_back(vars[i]);
            new_vals.push_back(vals[i]);
            lits.push_back(m.mk_eq(vars[i], a.mk_numeral(vals[i], a.is_int(vars[i])))); 
            vars.erase(vars.begin() + i);
            vals.erase(vals.begin() + i);
            while (!vars.empty()) {
                i = m_random(vars.size());
                j = m_random(new_vars.size());
                lits.push_back(m.mk_eq(vars[i], mk_offset(new_vars[j], vals[i]-new_vals[j])));
                new_vars.push_back(vars[i]);
                new_vals.push_back(vals[i]);
                vars.erase(vars.begin() + i);
                vals.erase(vals.begin() + i);
            }
            break;
        }
        }
    }

    bool model_farkas_generalizer::is_eq(expr* e, expr*& r, rational& vl, bool& is_int) {
        expr* r2 = 0;
        return 
            (m.is_eq(e, r, r2) && a.is_numeral(r2, vl, is_int)) ||
            (m.is_eq(e, r2, r) && a.is_numeral(r2, vl, is_int));
    }

    void model_farkas_generalizer::relativize(expr_ref_vector& literals) {
        ast_manager& m = literals.get_manager();
        ptr_vector<expr> ints, reals;
        vector<rational> int_vals, real_vals;
        expr* e;
        rational vl;
        bool is_int;
        for (unsigned i = 0; i < literals.size(); ) {
            if (is_eq(literals[i].get(), e, vl, is_int)) {                
                (is_int?ints:reals).push_back(e); 
                (is_int?int_vals:real_vals).push_back(vl);
                literals[i] = literals.back();
                literals.pop_back();
            }
            else {
                ++i;
            }
        }
        connect_vars(ints, int_vals, literals);
        connect_vars(reals, real_vals, literals);
    }

#endif
