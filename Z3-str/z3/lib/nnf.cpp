/*++
Copyright (c) 2007 Microsoft Corporation

Module Name:

    nnf.cpp

Abstract:

    Negation Normal Form & Skolemization

Author:

    Leonardo (leonardo) 2008-01-11

Notes:
    Major revision on 2011-10-06

--*/
#include"nnf.h"
#include"warning.h"
#include"used_vars.h"
#include"well_sorted.h"
#include"var_subst.h"

#include"name_exprs.h"
#include"act_cache.h"
#include"cooperate.h"

#include"ast_smt2_pp.h"

// Old strategy framework
#include"assertion_set_strategy.h"

// New framework
#include"tactical.h"

class skolemizer {
    typedef act_cache cache;

    ast_manager & m_manager;
    symbol        m_sk_hack;
    bool          m_sk_hack_enabled;
    cache         m_cache;
    cache         m_cache_pr;

    void process(quantifier * q, expr_ref & r, proof_ref & p) {
        used_vars uv;
        uv(q);
        SASSERT(is_well_sorted(m(), q));
        unsigned sz = uv.get_max_found_var_idx_plus_1();
        ptr_buffer<sort> sorts;
        expr_ref_vector args(m());
        for (unsigned i = 0; i < sz; i++) {
            sort * s = uv.get(i);
            if (s != 0) {
                sorts.push_back(s);
                args.push_back(m().mk_var(i, s));
            }
        }

        TRACE("skolemizer", tout << "skid: " << q->get_skid() << "\n";);
        
        expr_ref_vector substitution(m());
        unsigned num_decls = q->get_num_decls();
        for (unsigned i = num_decls; i > 0; ) {
            --i;
            sort * r            = q->get_decl_sort(i);
            func_decl * sk_decl = m().mk_fresh_func_decl(q->get_decl_name(i), q->get_skid(), sorts.size(), sorts.c_ptr(), r);
            app * sk            = m().mk_app(sk_decl, args.size(), args.c_ptr());
            substitution.push_back(sk);
        }
        //
        // (VAR 0) is in the first position of substitution.
        // (VAR num_decls-1) is in the last position.
        //
        for (unsigned i = 0; i < sz; i++) {
            sort * s = uv.get(i);
            if (s != 0)
                substitution.push_back(m().mk_var(i, s));
            else
                substitution.push_back(0);
        }
        //
        // (VAR num_decls) ... (VAR num_decls+sz-1) 
        // are in positions num_decls .. num_decls+sz-1
        //
        std::reverse(substitution.c_ptr(), substitution.c_ptr() + substitution.size());
        //
        // (VAR 0) should be in the last position of substitution.
        //
        var_subst s(m());
        SASSERT(is_well_sorted(m(), q->get_expr()));
        expr_ref tmp(m());
        expr * body = q->get_expr();
        if (m_sk_hack_enabled) {
            unsigned num_patterns = q->get_num_patterns();
            for (unsigned i = 0; i < num_patterns; ++i) {
                expr * p = q->get_pattern(i);
                if (is_sk_hack(p)) {
                    expr * sk_hack = to_app(p)->get_arg(0);
                    if (q->is_forall()) // check whether is in negative/positive context.
                        tmp  = m().mk_or(body, m().mk_not(sk_hack)); // negative context
                    else
                        tmp  = m().mk_and(body, sk_hack); // positive context
                    body = tmp;
                }
            }
        }
        s(body, substitution.size(), substitution.c_ptr(), r);
        SASSERT(is_well_sorted(m(), r));
        p = 0;
        if (m().proofs_enabled()) {
            if (q->is_forall()) 
                p = m().mk_skolemization(m().mk_not(q), m().mk_not(r));
            else
                p = m().mk_skolemization(q, r);
        }
    }

public:
    skolemizer(ast_manager & m, params_ref const & p):
        m_manager(m),
        m_sk_hack("sk_hack"),
        m_cache(m),
        m_cache_pr(m) {
        updt_params(p);
    }

    void updt_params(params_ref const & p) {    
        m_sk_hack_enabled  = p.get_bool(":nnf-sk-hack", false);
    }

    static void get_param_descrs(param_descrs & r) {
        r.insert(":nnf-sk-hack", CPK_BOOL, "(default: false) hack for VCC");
    }

    ast_manager & m() const { return m_manager; }

    void operator()(quantifier * q, expr_ref & r, proof_ref & p) {
        r = m_cache.find(q);
        if (r.get() != 0) {
            p = 0;
            if (m().proofs_enabled())
                p = static_cast<proof*>(m_cache_pr.find(q));
        }
        else {
            process(q, r, p);
            m_cache.insert(q, r);
            if (m().proofs_enabled())
                m_cache_pr.insert(q, p);
        }
    }
    
    bool is_sk_hack(expr * p) const {
        SASSERT(m().is_pattern(p));
        if (to_app(p)->get_num_args() != 1)
            return false;
        expr * body = to_app(p)->get_arg(0);
        if (!is_app(body))
            return false;
        func_decl * f = to_app(body)->get_decl();
        if (!(f->get_name() == m_sk_hack && f->get_arity() == 1))
            return false;
        if (!m().is_bool(body)) {
            warning_msg("sk_hack constant must return a Boolean");
            return false;
        }
        return true;
    }
};

typedef default_exception nnf_params_exception;
typedef default_exception nnf_exception;

struct nnf::imp {

    struct frame {
        expr *    m_curr;
        unsigned  m_i:28;
        unsigned  m_pol:1;  // pos/neg polarity
        unsigned  m_in_q:1; // true if m_curr is nested in a quantifier
        unsigned  m_new_child:1; 
        unsigned  m_cache_result:1;
        unsigned  m_spos;   // top of the result stack, when the frame was created.
        frame(expr * n, bool pol, bool in_q, bool cache_res, unsigned spos):
            m_curr(n),
            m_i(0),
            m_pol(pol),
            m_in_q(in_q),
            m_new_child(false),
            m_cache_result(cache_res),
            m_spos(spos) {
        }
    };

    // There are four caches:
#define NEG_NQ_CIDX 0   // negative polarity and not nested in a quantifier
#define POS_NQ_CIDX 1   // positive polarity and not nested in a quantifier
#define NEG_Q_CIDX  2   // negative polarity and nested in a quantifier
#define POS_Q_CIDX  3   // positive polarity and nested in a quantifier
    
    ast_manager &          m_manager;
    svector<frame>         m_frame_stack;
    expr_ref_vector        m_result_stack;
    
    typedef act_cache      cache;
    cache *                m_cache[4];

    expr_ref_vector        m_todo_defs;
    proof_ref_vector       m_todo_proofs;
    
    // proof generation goodness ----
    proof_ref_vector       m_result_pr_stack;
    cache *                m_cache_pr[4];
    // ------------------------------
    
    skolemizer             m_skolemizer;

    // configuration ----------------
    nnf_mode               m_mode;
    bool                   m_ignore_labels;
    bool                   m_skolemize;
    // ------------------------------

    name_exprs *           m_name_nested_formulas;
    name_exprs *           m_name_quant;
    
    symbol                 m_skolem;

    volatile bool          m_cancel;
    unsigned long long     m_max_memory; // in bytes

    imp(ast_manager & m, defined_names & n, params_ref const & p):
        m_manager(m),
        m_result_stack(m),
        m_todo_defs(m),
        m_todo_proofs(m),
        m_result_pr_stack(m),
        m_skolemizer(m, p),
        m_skolem("skolem"),
        m_cancel(false) {
        updt_local_params(p);
        for (unsigned i = 0; i < 4; i++) {
            m_cache[i] = alloc(act_cache, m);
            if (m.proofs_enabled())
                m_cache_pr[i] = alloc(act_cache, m);
        }
        m_name_nested_formulas = mk_nested_formula_namer(m, n);
        m_name_quant           = mk_quantifier_label_namer(m, n);
    }

    ast_manager & m() const { return m_manager; }

    bool proofs_enabled() const { return m().proofs_enabled(); }

    ~imp() {
        for (unsigned i = 0; i < 4; i++) {
            dealloc(m_cache[i]);
            if (proofs_enabled())
                dealloc(m_cache_pr[i]);
        }
        del_name_exprs(m_name_nested_formulas);
        del_name_exprs(m_name_quant);
    }

    void updt_params(params_ref const & p) {
        updt_local_params(p);
        m_skolemizer.updt_params(p);
    }

    void updt_local_params(params_ref const & p) {    
        symbol mode_sym    = p.get_sym(":nnf-mode", m_skolem);
        if (mode_sym == m_skolem)
            m_mode = NNF_SKOLEM;
        else if (mode_sym == "full")
            m_mode = NNF_FULL;
        else if (mode_sym == "quantifiers")
            m_mode = NNF_QUANT;
        else 
            throw nnf_params_exception("invalid NNF mode");

        TRACE("nnf", tout << "nnf-mode: " << m_mode << " " << mode_sym << "\n" << p << "\n";);

        m_ignore_labels    = p.get_bool(":nnf-ignore-labels", false);
        m_skolemize        = p.get_bool(":skolemize", true);
        m_max_memory       = megabytes_to_bytes(p.get_uint(":max-memory", UINT_MAX));
    }

    static void get_param_descrs(param_descrs & r) {
        insert_max_memory(r);
        r.insert(":nnf-mode", CPK_SYMBOL, 
                 "(default: skolem) NNF translation mode: skolem (skolem normal form), quantifiers (skolem normal form + quantifiers in NNF), full");
        r.insert(":nnf-ignore-labels", CPK_BOOL, 
                 "(default: false) remove/ignore labels in the input formula, this option is ignored if proofs are enabled");
        r.insert(":skolemize", CPK_BOOL,
                 "(default: true) skolemize (existential force) quantifiers");
        skolemizer::get_param_descrs(r);
    }

    void reset() {
        m_frame_stack.reset();
        m_result_stack.reset();
        m_result_pr_stack.reset();
        m_todo_defs.reset();
        m_todo_proofs.reset();
    }

    void reset_cache() {
        for (unsigned i = 0; i < 4; i++) {
            m_cache[i]->reset();
            if (proofs_enabled())
                m_cache_pr[i]->reset();
        }
    }

    void push_frame(expr * t, bool pol, bool in_q, bool cache_res) {
        m_frame_stack.push_back(frame(t, pol, in_q, cache_res, m_result_stack.size()));
    }

    static unsigned get_cache_idx(bool pol, bool in_q) { 
        return static_cast<unsigned>(in_q) * 2 + static_cast<unsigned>(pol); 
    }

    void cache_result(expr * t, bool pol, bool in_q, expr * v, proof * pr) {
        unsigned idx = get_cache_idx(pol, in_q);
        m_cache[idx]->insert(t, v);
        if (proofs_enabled())
            m_cache_pr[idx]->insert(t, pr);
    }

    expr * get_cached(expr * t, bool pol, bool in_q) const { 
        return m_cache[get_cache_idx(pol, in_q)]->find(t); 
    }

    proof * get_cached_pr(expr * t, bool pol, bool in_q) const {
        SASSERT(proofs_enabled());
        return static_cast<proof*>(m_cache_pr[get_cache_idx(pol, in_q)]->find(t));
    }

    /**
       \brief Return true if the result for (t, pol, in_q) is already cached,
       and store the result on the stack.
    */
    bool process_cached(expr * t, bool pol, bool in_q) {
        expr * r = get_cached(t, pol, in_q);
        if (r) {
            m_result_stack.push_back(r);
            if (proofs_enabled()) {
                proof * pr = get_cached_pr(t, pol, in_q);
                m_result_pr_stack.push_back(pr);
                SASSERT(m_result_stack.size() == m_result_pr_stack.size());
            }
            m_frame_stack.pop_back();
            set_new_child_flag(t, r);
            return true;
        }
        return false;
    }

    void set_cancel(bool f) {
        m_cancel = f;
    }
    
    void checkpoint() {
        cooperate("nnf");
        if (memory::get_allocation_size() > m_max_memory)
            throw nnf_exception(STE_MAX_MEMORY_MSG);
        if (m_cancel)
            throw nnf_exception(STE_CANCELED_MSG);
    }

    void set_new_child_flag() {
        if (!m_frame_stack.empty())
            m_frame_stack.back().m_new_child = true;
    }

    void set_new_child_flag(expr * old_t, expr * new_t) { 
        if (old_t != new_t) 
            set_new_child_flag(); 
    }
    
    void skip(expr * t, bool pol) {
        expr * r = pol ? t : m().mk_not(t);
        m_result_stack.push_back(r);
        if (proofs_enabled()) {
            m_result_pr_stack.push_back(m().mk_oeq_reflexivity(r));
            SASSERT(m_result_stack.size() == m_result_pr_stack.size());
        }
    }

    bool visit(expr * t, bool pol, bool in_q) {
        SASSERT(m().is_bool(t));

        if (m_mode == NNF_SKOLEM || (m_mode == NNF_QUANT && !in_q)) {
            if (!has_quantifiers(t) && !has_labels(t)) {
                skip(t, pol);
                return true; // t does not need to be processed
            }
        }

        bool cache_res = t->get_ref_count() > 1;

        if (cache_res) {
            expr * r = get_cached(t, pol, in_q);
            if (r) {
                m_result_stack.push_back(r);
                set_new_child_flag(t, r);
                if (proofs_enabled()) {
                    proof * pr = get_cached_pr(t, pol, in_q);
                    m_result_pr_stack.push_back(pr);
                    SASSERT(m_result_stack.size() == m_result_pr_stack.size());
                }
                return true; // t was already processed
            }
        }

        switch (t->get_kind()) {
        case AST_APP:
            if (to_app(t)->get_num_args() == 0) {
                skip(t, pol);
                return true;
            }
            else {
                push_frame(t, pol, in_q, cache_res);
                return false;
            }
        case AST_QUANTIFIER:
            push_frame(t, pol, in_q, cache_res);
            return false;
        case AST_VAR:
            skip(t, pol);
            return true;
        default:
            UNREACHABLE();
            return true;
        }
    }

    proof * mk_proof(bool pol, unsigned num_parents, proof * const * parents, app * old_e, app * new_e) {
        if (pol) {
            if (old_e->get_decl() == new_e->get_decl())
                return m().mk_oeq_congruence(old_e, new_e, num_parents, parents);
            else 
                return m().mk_nnf_pos(old_e, new_e, num_parents, parents);
        }
        else 
            return m().mk_nnf_neg(old_e, new_e, num_parents, parents);
    }

    bool process_and_or(app * t, frame & fr) {
        unsigned num_args = t->get_num_args();
        while (fr.m_i < num_args) {
            expr * arg = t->get_arg(fr.m_i);
            fr.m_i++;
            if (!visit(arg, fr.m_pol, fr.m_in_q))
                return false;
        }
        app * r;
        if (m().is_and(t) == fr.m_pol)
            r = m().mk_and(t->get_num_args(), m_result_stack.c_ptr() + fr.m_spos);
        else
            r = m().mk_or(t->get_num_args(), m_result_stack.c_ptr() + fr.m_spos);
        
        m_result_stack.shrink(fr.m_spos);
        m_result_stack.push_back(r);
        if (proofs_enabled()) {
            proof * pr = mk_proof(fr.m_pol, t->get_num_args(), m_result_pr_stack.c_ptr() + fr.m_spos, t, r);
            m_result_pr_stack.shrink(fr.m_spos);
            m_result_pr_stack.push_back(pr);
            SASSERT(m_result_stack.size() == m_result_pr_stack.size());
        }
        return true;
    }

    bool process_not(app * t, frame & fr) {
        if (fr.m_i == 0) {
            fr.m_i = 1;
            if (!visit(t->get_arg(0), !fr.m_pol, fr.m_in_q))
                return false;
        }
        expr  * r  = m_result_stack.back();
        proof * pr = 0;
        if (proofs_enabled()) {
            pr = m_result_pr_stack.back();
            if (!fr.m_pol) {
                pr = m().mk_nnf_neg(t, r, 1, &pr);
                m_result_pr_stack.pop_back();
                m_result_pr_stack.push_back(pr);
                SASSERT(m_result_stack.size() == m_result_pr_stack.size());
            }
        }
        return true;
    }

    bool process_implies(app * t, frame & fr) {
        SASSERT(t->get_num_args() == 2);
        switch (fr.m_i) {
        case 0:
            fr.m_i = 1;
            if (!visit(t->get_arg(0), !fr.m_pol, fr.m_in_q))
                return false;
        case 1:
            fr.m_i = 2;
            if (!visit(t->get_arg(1), fr.m_pol, fr.m_in_q))
                return false;
        default:
            break;
        }

        app * r;
        if (fr.m_pol)
            r = m().mk_or(2, m_result_stack.c_ptr() + fr.m_spos);
        else
            r = m().mk_and(2, m_result_stack.c_ptr() + fr.m_spos);
        
        m_result_stack.shrink(fr.m_spos);
        m_result_stack.push_back(r);
        if (proofs_enabled()) {
            proof * pr = mk_proof(fr.m_pol, 2, m_result_pr_stack.c_ptr() + fr.m_spos, t, r);
            m_result_pr_stack.shrink(fr.m_spos);
            m_result_pr_stack.push_back(pr);
            SASSERT(m_result_stack.size() == m_result_pr_stack.size());
        }
        return true;
    }

    bool process_ite(app * t, frame & fr) {
        SASSERT(t->get_num_args() == 3);
        switch (fr.m_i) {
        case 0:
            fr.m_i = 1;
            if (!visit(t->get_arg(0), true, fr.m_in_q))
                return false;
        case 1:
            fr.m_i = 2;
            if (!visit(t->get_arg(0), false, fr.m_in_q))
                return false;
        case 2:
            fr.m_i = 3;
            if (!visit(t->get_arg(1), fr.m_pol, fr.m_in_q))
                return false;
        case 3:
            fr.m_i = 4;
            if (!visit(t->get_arg(2), fr.m_pol, fr.m_in_q))
                return false;
        default:
            break;
        }
        
        expr * const * rs = m_result_stack.c_ptr() + fr.m_spos;
        expr * _cond      = rs[0];
        expr * _not_cond  = rs[1];
        expr * _then      = rs[2];
        expr * _else      = rs[3];

        app * r = m().mk_and(m().mk_or(_not_cond, _then), m().mk_or(_cond, _else));
        m_result_stack.shrink(fr.m_spos);
        m_result_stack.push_back(r);
        if (proofs_enabled()) {
            proof * pr = mk_proof(fr.m_pol, 4, m_result_pr_stack.c_ptr() + fr.m_spos, t, r);
            m_result_pr_stack.shrink(fr.m_spos);
            m_result_pr_stack.push_back(pr);
            SASSERT(m_result_stack.size() == m_result_pr_stack.size());
        }
        return true;
    }

    bool is_eq(app * t) const { return m().is_eq(t) || m().is_iff(t); }
    
    bool process_iff_xor(app * t, frame & fr) {
        SASSERT(t->get_num_args() == 2);
        switch (fr.m_i) {
        case 0:
            fr.m_i = 1;
            if (!visit(t->get_arg(0), true, fr.m_in_q))
                return false;
        case 1:
            fr.m_i = 2;
            if (!visit(t->get_arg(0), false, fr.m_in_q))
                return false;
        case 2:
            fr.m_i = 3;
            if (!visit(t->get_arg(1), true, fr.m_in_q))
                return false;
        case 3:
            fr.m_i = 4;
            if (!visit(t->get_arg(1), false, fr.m_in_q))
                return false;
        default:
            break;
        }

        expr * const * rs = m_result_stack.c_ptr() + fr.m_spos;
        expr * lhs      = rs[0];
        expr * not_lhs  = rs[1];
        expr * rhs      = rs[2];
        expr * not_rhs  = rs[3];

        app * r;
        if (is_eq(t) == fr.m_pol) 
            r = m().mk_and(m().mk_or(not_lhs, rhs), m().mk_or(lhs, not_rhs));
        else
            r = m().mk_and(m().mk_or(lhs, rhs), m().mk_or(not_lhs, not_rhs));
        m_result_stack.shrink(fr.m_spos);
        m_result_stack.push_back(r);
        if (proofs_enabled()) {
            proof * pr = mk_proof(fr.m_pol, 4, m_result_pr_stack.c_ptr() + fr.m_spos, t, r);
            m_result_pr_stack.shrink(fr.m_spos);
            m_result_pr_stack.push_back(pr);
            SASSERT(m_result_stack.size() == m_result_pr_stack.size());
        }
        return true;
    }

    bool process_eq(app * t, frame & fr) {
        if (m().is_bool(t->get_arg(0)))
            return process_iff_xor(t, fr);
        else
            return process_default(t, fr);
    }
    
    bool process_default(app * t, frame & fr) {
        SASSERT(fr.m_i == 0);
        if (m_mode == NNF_FULL || t->has_quantifiers() || t->has_labels()) {
            expr_ref  n2(m());
            proof_ref pr2(m());
            if (m_mode == NNF_FULL || (m_mode != NNF_SKOLEM && fr.m_in_q))
                m_name_nested_formulas->operator()(t, m_todo_defs, m_todo_proofs, n2, pr2);
            else
                m_name_quant->operator()(t, m_todo_defs, m_todo_proofs, n2, pr2);
        
            if (!fr.m_pol)
                n2 = m().mk_not(n2);
            
            m_result_stack.push_back(n2);
            if (proofs_enabled()) {
                if (!fr.m_pol) {
                    proof * prs[1] = { pr2 };
                     pr2 = m().mk_oeq_congruence(m().mk_not(t), static_cast<app*>(n2.get()), 1, prs);
                }
                m_result_pr_stack.push_back(pr2);
                SASSERT(m_result_stack.size() == m_result_pr_stack.size());
            }
        }
        else {
            skip(t, fr.m_pol);
        }
        return true;
    }

    bool process_label(app * t, frame & fr) {
        if (fr.m_i == 0) {
            fr.m_i = 1;
            if (!visit(t->get_arg(0), fr.m_pol, fr.m_in_q))
                return false;
        }

        expr * arg = m_result_stack.back();
        proof * arg_pr = proofs_enabled() ? m_result_pr_stack.back() : 0;

        if (m_ignore_labels && !proofs_enabled()) 
            return true; // the result is already on the stack

        
        buffer<symbol> names;
        bool pos;
        m().is_label(t, pos, names);
        expr_ref r(m());
        proof_ref pr(m());
        if (fr.m_pol == pos) {
            expr * lbl_lit = m().mk_label_lit(names.size(), names.c_ptr());
            r              = m().mk_and(arg, lbl_lit);
            if (proofs_enabled()) {
                expr_ref aux(m_manager);
                aux = m().mk_label(true, names.size(), names.c_ptr(), arg);
                pr = m().mk_transitivity(mk_proof(fr.m_pol, 1, &arg_pr, t, to_app(aux)),
                                         m().mk_iff_oeq(m().mk_rewrite(aux, r)));
            }
        }    
        else {
            r = arg;
            if (proofs_enabled()) {
                proof * p1 = m().mk_iff_oeq(m().mk_rewrite(t, t->get_arg(0)));
                pr = m().mk_transitivity(p1, arg_pr);
            }
        }
        
        m_result_stack.pop_back();
        m_result_stack.push_back(r);
        if (proofs_enabled()) {
            m_result_pr_stack.pop_back();
            m_result_pr_stack.push_back(pr);
            SASSERT(m_result_stack.size() == m_result_pr_stack.size());
        }
        return true;
    }

    bool process_app(app * t, frame & fr) {
        SASSERT(m().is_bool(t));
        if (t->get_family_id() == m().get_basic_family_id()) {
            switch (static_cast<basic_op_kind>(t->get_decl_kind())) {
            case OP_AND: case OP_OR:
                return process_and_or(t, fr);
            case OP_NOT:
                return process_not(t, fr);
            case OP_IMPLIES:
                return process_implies(t, fr);
            case OP_ITE:
                return process_ite(t, fr);
            case OP_IFF:
            case OP_XOR:
                return process_iff_xor(t, fr);
            case OP_EQ:
                return process_eq(t, fr);
            default:
                break;
            }
        }

        if (m().is_label(t)) {
            return process_label(t, fr);
        }
        
        return process_default(t, fr);
    }

    bool process_var(var * v, frame & fr) {
        skip(v, fr.m_pol);
        return true;
    }
    
    bool process_quantifier(quantifier * q, frame & fr) {
        expr_ref  r(m());
        proof_ref pr(m());
        if (fr.m_i == 0) {
            fr.m_i = 1;
            if (q->is_forall() == fr.m_pol || !m_skolemize) {
                if (!visit(q->get_expr(), fr.m_pol, true))
                    return false;
            }
            else {
                m_skolemizer(q, r, pr);
                if (!visit(r, !q->is_forall(), fr.m_in_q))
                    return false;
            }
        }

        if (q->is_forall() == fr.m_pol || !m_skolemize) {
            expr * new_expr     = m_result_stack.back();
            proof * new_expr_pr = proofs_enabled() ? m_result_pr_stack.back() : 0;
            
            ptr_buffer<expr> new_patterns;

            if (q->is_forall() == fr.m_pol) {
                // collect non sk_hack patterns
                unsigned num_patterns = q->get_num_patterns();
                for (unsigned i = 0; i < num_patterns; i++) {
                    expr * pat = q->get_pattern(i);
                    if (!m_skolemizer.is_sk_hack(pat))
                        new_patterns.push_back(pat);
                }
            }
            else {
                // New quantifier has existential force.
                // So, ignore patterns
            }
            
            quantifier * new_q = 0;
            proof * new_q_pr   = 0;
            if (fr.m_pol) {
                new_q = m().update_quantifier(q, new_patterns.size(), new_patterns.c_ptr(), new_expr);
                if (proofs_enabled())
                    new_q_pr = m().mk_nnf_pos(q, new_q, 1, &new_expr_pr);
            }
            else {
                new_q = m().update_quantifier(q, !q->is_forall(), new_patterns.size(), new_patterns.c_ptr(), new_expr);
                if (proofs_enabled())
                    new_q_pr = m().mk_nnf_neg(q, new_q, 1, &new_expr_pr);
            }
            
            m_result_stack.pop_back();
            m_result_stack.push_back(new_q);
            if (proofs_enabled()) {
                m_result_pr_stack.pop_back();
                m_result_pr_stack.push_back(new_q_pr);
                SASSERT(m_result_stack.size() == m_result_pr_stack.size());
            }
        }
        else {
            // Quantifier was skolemized.
            // The result is already on the stack.
            // However, the proof must be updated
            if (proofs_enabled()) {
                m_skolemizer(q, r, pr); // retrieve the proof
                pr = m().mk_transitivity(pr, m_result_pr_stack.back());
                m_result_pr_stack.pop_back();
                m_result_pr_stack.push_back(pr);
                SASSERT(m_result_stack.size() == m_result_pr_stack.size());
            }
        }
        return true;
    }
    
    void recover_result(expr * t, expr_ref & result, proof_ref & result_pr) {
        // recover result from the top of the stack.
        result = m_result_stack.back();
        m_result_stack.pop_back();
        SASSERT(m_result_stack.empty());
        if (proofs_enabled()) {
            result_pr = m_result_pr_stack.back();
            m_result_pr_stack.pop_back();
            if (result_pr.get() == 0)
                result_pr = m().mk_reflexivity(t);
            SASSERT(m_result_pr_stack.empty());
        }
    }

    void process(expr * t, expr_ref & result, proof_ref & result_pr) {
        TRACE("nnf", tout << "processing:\n" << mk_ismt2_pp(t, m()) << "\n";);
        SASSERT(m().is_bool(t));

        if (visit(t, true /* positive polarity */, false /* not nested in quantifier */)) {
            recover_result(t, result, result_pr);
            return;
        }
        SASSERT(!m_frame_stack.empty());
        while (!m_frame_stack.empty()) {
            checkpoint();
            frame & fr = m_frame_stack.back();
            expr * t   = fr.m_curr;

            if (fr.m_i == 0 && t->get_ref_count() > 1 && process_cached(t, fr.m_pol, fr.m_in_q))
                continue;

            bool status;

            switch (t->get_kind()) {
            case AST_APP:
                status = process_app(to_app(t), fr);
                break;
            case AST_QUANTIFIER:
                status = process_quantifier(to_quantifier(t), fr);
                break;
            case AST_VAR:
                status = process_var(to_var(t), fr);
                break;
            default:
                UNREACHABLE();
                status = true;
                break;
            }

            if (status) {
                if (fr.m_cache_result)
                    cache_result(fr.m_curr, fr.m_pol, fr.m_in_q, m_result_stack.back(), proofs_enabled() ? m_result_pr_stack.back() : 0);
                m_frame_stack.pop_back();
            }
        }
        recover_result(t, result, result_pr);
    }

    void operator()(expr * n, expr_ref_vector & new_defs, proof_ref_vector & new_def_proofs, expr_ref & r, proof_ref & pr) {
        reset();
        process(n, r, pr);
        unsigned old_sz1 = new_defs.size();
        unsigned old_sz2 = new_def_proofs.size();
        
        for (unsigned i = 0; i < m_todo_defs.size(); i++) {
            expr_ref  dr(m());
            proof_ref dpr(m());
            process(m_todo_defs.get(i), dr, dpr);
            new_defs.push_back(dr);
            if (proofs_enabled()) {
                proof * new_pr = m().mk_modus_ponens(m_todo_proofs.get(i), dpr);
                new_def_proofs.push_back(new_pr); 
            }
        }
        std::reverse(new_defs.c_ptr() + old_sz1, new_defs.c_ptr() + new_defs.size());
        std::reverse(new_def_proofs.c_ptr() + old_sz2, new_def_proofs.c_ptr() + new_def_proofs.size());
    }
};


nnf::nnf(ast_manager & m, defined_names & n, params_ref const & p) {
    TRACE("nnf", tout << "nnf constructor: " << p << "\n";);
    m_imp = alloc(imp, m, n, p);
}

nnf::nnf(ast_manager & m, defined_names & n, nnf_params & np) {
    params_ref p;
    if (np.m_nnf_mode == NNF_FULL)
        p.set_sym(":nnf-mode", symbol("full"));
    else if (np.m_nnf_mode == NNF_QUANT)
        p.set_sym(":nnf-mode", symbol("quantifiers"));
    
    if (np.m_nnf_ignore_labels)
        p.set_bool(":nnf-ignore-labels", true);
    
    if (np.m_nnf_sk_hack)
        p.set_bool(":nnf-sk-hack", true);
    m_imp = alloc(imp, m, n, p);
}

nnf::~nnf() {
    dealloc(m_imp);
}
    
void nnf::operator()(expr * n, expr_ref_vector & new_defs, proof_ref_vector & new_def_proofs, expr_ref & r,  proof_ref & p) {
    m_imp->operator()(n, new_defs, new_def_proofs, r, p);
    TRACE("nnf_result", tout << mk_ismt2_pp(n, m_imp->m()) << "\nNNF result:\n" << mk_ismt2_pp(r, m_imp->m()) << "\n";);
}

void nnf::updt_params(params_ref const & p) {
    m_imp->updt_params(p);
}

void nnf::get_param_descrs(param_descrs & r) {
    imp::get_param_descrs(r);
}

void nnf::set_cancel(bool f) {
    m_imp->set_cancel(f);
}

void nnf::reset() {
    m_imp->reset();
}

void nnf::reset_cache() {
    m_imp->reset_cache();
}

// TODO: delete after conversion to new tactic framework is done.
class nnf_strategy : public assertion_set_strategy {
    params_ref    m_params;
    nnf *         m_nnf;

    struct set_nnf {
        nnf_strategy & m_owner;
        
        set_nnf(nnf_strategy & owner, nnf & n):
            m_owner(owner) {
            #pragma omp critical (nnf_strategy)
            {
                m_owner.m_nnf = &n;
            }
        }
        
        ~set_nnf() {
            #pragma omp critical (nnf_strategy)
            {
                m_owner.m_nnf = 0;
            }
        }
    };
public:
    nnf_strategy(params_ref const & p):
        m_params(p),
        m_nnf(0) {
        TRACE("nnf", tout << "nnf_strategy constructor: " << p << "\n";);
    }

    virtual ~nnf_strategy() {}

    virtual void updt_params(params_ref const & p) { m_params = p; }
    static void get_param_descrs(param_descrs & r) { nnf::get_param_descrs(r); }
    virtual void collect_param_descrs(param_descrs & r) { get_param_descrs(r); }

    virtual void operator()(assertion_set & s, model_converter_ref & mc) {
        TRACE("nnf", tout << "params: " << m_params << "\n"; s.display(tout););
        SASSERT(is_well_sorted(s));
        as_st_report report("nnf", s);
        mc = 0;
        if (s.inconsistent())
            return;
        ast_manager & m = s.m();
        defined_names dnames(m);
        nnf local_nnf(m, dnames, m_params);
        set_nnf setter(*this, local_nnf);
        
        expr_ref_vector defs(m);
        proof_ref_vector def_prs(m);
        
        expr_ref   new_curr(m);
        proof_ref  new_pr(m);
        
        unsigned sz = s.size();
        for (unsigned i = 0; i < sz; i++) {
            expr * curr = s.form(i);
            local_nnf(curr, defs, def_prs, new_curr, new_pr);
            if (m.proofs_enabled()) {
                proof * pr = s.pr(i);
                new_pr     = m.mk_modus_ponens(pr, new_pr);
            }
            s.update(i, new_curr, new_pr);
        }
        
        sz = defs.size();
        for (unsigned i = 0; i < sz; i++) {
            if (m.proofs_enabled())
                s.assert_expr(defs.get(i), def_prs.get(i));
            else
                s.assert_expr(defs.get(i), 0);
        }
        TRACE("nnf", s.display(tout););
        SASSERT(is_well_sorted(s));
    }

    virtual void cleanup() {}
    virtual void set_cancel(bool f) {
        #pragma omp critical (nnf_strategy)
        {
            if (m_nnf)
                m_nnf->set_cancel(f);
        }
    }
};

as_st * mk_snf(params_ref const & p) {
    return alloc(nnf_strategy, p);
}

as_st * mk_nnf(params_ref const & p) {
    params_ref new_p(p);
    new_p.set_sym(":nnf-mode", symbol("full"));
    TRACE("nnf", tout << "mk_nnf: " << new_p << "\n";);
    return using_params(mk_snf(), new_p);
}

