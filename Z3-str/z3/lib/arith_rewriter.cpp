/*++
Copyright (c) 2011 Microsoft Corporation

Module Name:

    arith_rewriter.cpp

Abstract:

    Basic rewriting rules for arithmetic

Author:

    Leonardo (leonardo) 2011-04-10

Notes:

--*/
#include"arith_rewriter.h"
#include"poly_rewriter_def.h"
#include"algebraic_numbers.h"
#include"ast_pp.h"

void arith_rewriter::updt_local_params(params_ref const & p) {
    m_arith_lhs       = p.get_bool(":arith-lhs", false);
    m_gcd_rounding    = p.get_bool(":gcd-rounding", false);
    m_eq2ineq         = p.get_bool(":eq2ineq", false);
    m_elim_to_real    = p.get_bool(":elim-to-real", false);
    m_push_to_real    = p.get_bool(":push-to-real", true);
    m_anum_simp       = p.get_bool(":algebraic-number-evaluator", true);
    m_max_degree      = p.get_uint(":max-degree", 64);
    m_expand_power    = p.get_bool(":expand-power", false);
    m_mul2power       = p.get_bool(":mul-to-power", false);
    m_elim_rem        = p.get_bool(":elim-rem", false);
    m_expand_tan      = p.get_bool(":expand-tan", false);
    set_sort_sums(p.get_bool(":sort-sums", false)); // set here to avoid collision with bvadd
}

void arith_rewriter::updt_params(params_ref const & p) {
    poly_rewriter<arith_rewriter_core>::updt_params(p);
    updt_local_params(p);
}

void arith_rewriter::get_param_descrs(param_descrs & r) {
    poly_rewriter<arith_rewriter_core>::get_param_descrs(r);
    r.insert(":algebraic-number-evaluator", CPK_BOOL, "(default: true) simplify/evaluate expressions containing (algebraic) irrational numbers.");
    r.insert(":mul-to-power", CPK_BOOL, "(default: false) collpase (* t ... t) into (^ t k), it is ignored if :expand-power is true.");
    r.insert(":expand-power", CPK_BOOL, "(default: false) expand (^ t k) into (* t ... t) if  1 < k <= :max-degree.");
    r.insert(":expand-tan", CPK_BOOL, "(default: false) replace (tan x) with (/ (sin x) (cos x)).");
    r.insert(":max-degree", CPK_UINT, "(default: 64) max degree of algebraic numbers (and power operators) processed by simplifier.");
    r.insert(":eq2ineq", CPK_BOOL, "(default: false) split arithmetic equalities into two inequalities.");
    r.insert(":sort-sums", CPK_BOOL, "(default: false) sort the arguments of + application.");
    r.insert(":gcd-rounding", CPK_BOOL, "(default: false) use gcd rounding on integer arithmetic atoms.");
    r.insert(":arith-lhs", CPK_BOOL, "(default: false) all monomials are moved to the left-hand-side, and the right-hand-side is just a constant.");
    r.insert(":elim-to-real", CPK_BOOL, "(default: false) eliminate to_real from arithmetic predicates that contain only integers.");
    r.insert(":push-to-real", CPK_BOOL, "(default: true) distribute to_real over * and +.");
    r.insert(":elim-rem", CPK_BOOL, "(default: false) replace (rem x y) with (ite (>= y 0) (mod x y) (- (mod x y))).");
}

br_status arith_rewriter::mk_app_core(func_decl * f, unsigned num_args, expr * const * args, expr_ref & result) {
    br_status st = BR_FAILED;
    SASSERT(f->get_family_id() == get_fid());
    switch (f->get_decl_kind()) {
    case OP_NUM: st = BR_FAILED; break;
    case OP_IRRATIONAL_ALGEBRAIC_NUM: st = BR_FAILED; break;
    case OP_LE:  SASSERT(num_args == 2); st = mk_le_core(args[0], args[1], result); break;
    case OP_GE:  SASSERT(num_args == 2); st = mk_ge_core(args[0], args[1], result); break;
    case OP_LT:  SASSERT(num_args == 2); st = mk_lt_core(args[0], args[1], result); break;
    case OP_GT:  SASSERT(num_args == 2); st = mk_gt_core(args[0], args[1], result); break;
    case OP_ADD: st = mk_add_core(num_args, args, result); break;
    case OP_MUL: st = mk_mul_core(num_args, args, result); break;
    case OP_SUB: st = mk_sub(num_args, args, result); break;
    case OP_DIV: SASSERT(num_args == 2); st = mk_div_core(args[0], args[1], result); break;
    case OP_IDIV: SASSERT(num_args == 2); st = mk_idiv_core(args[0], args[1], result); break;
    case OP_MOD: SASSERT(num_args == 2); st = mk_mod_core(args[0], args[1], result); break;
    case OP_REM: SASSERT(num_args == 2); st = mk_rem_core(args[0], args[1], result); break;
    case OP_UMINUS: SASSERT(num_args == 1);  st = mk_uminus(args[0], result); break;
    case OP_TO_REAL: SASSERT(num_args == 1); st = mk_to_real_core(args[0], result); break;
    case OP_TO_INT: SASSERT(num_args == 1);  st = mk_to_int_core(args[0], result); break;
    case OP_IS_INT: SASSERT(num_args == 1);  st = mk_is_int(args[0], result); break;
    case OP_POWER:  SASSERT(num_args == 2);  st = mk_power_core(args[0], args[1], result); break;
    case OP_SIN: SASSERT(num_args == 1); st = mk_sin_core(args[0], result); break;
    case OP_COS: SASSERT(num_args == 1); st = mk_cos_core(args[0], result); break;
    case OP_TAN: SASSERT(num_args == 1); st = mk_tan_core(args[0], result); break;
    case OP_ASIN: SASSERT(num_args == 1); st = mk_asin_core(args[0], result); break;
    case OP_ACOS: SASSERT(num_args == 1); st = mk_acos_core(args[0], result); break;
    case OP_ATAN: SASSERT(num_args == 1); st = mk_atan_core(args[0], result); break;
    case OP_SINH: SASSERT(num_args == 1); st = mk_sinh_core(args[0], result); break;
    case OP_COSH: SASSERT(num_args == 1); st = mk_cosh_core(args[0], result); break;
    case OP_TANH: SASSERT(num_args == 1); st = mk_tanh_core(args[0], result); break;
    default: st = BR_FAILED; break;
    }
    CTRACE("arith_rewriter", st != BR_FAILED, tout << mk_pp(f, m());
            for (unsigned i = 0; i < num_args; ++i) tout << mk_pp(args[i], m()) << " ";
            tout << "\n==>\n" << mk_pp(result.get(), m()) << "\n";);
    return st;
}

void arith_rewriter::get_coeffs_gcd(expr * t, numeral & g, bool & first, unsigned & num_consts) {
    unsigned sz;
    expr * const * ms = get_monomials(t, sz);
    SASSERT(sz >= 1);
    numeral a;
    for (unsigned i = 0; i < sz; i++) {
        expr * arg = ms[i];
        if (is_numeral(arg, a)) {
            if (!a.is_zero())
                num_consts++;
            continue;
        }
        if (first) {
            get_power_product(arg, g);
            SASSERT(g.is_int());
            first = false;
        }
        else {
            get_power_product(arg, a);
            SASSERT(a.is_int());
            g = gcd(abs(a), g);
        }
        if (g.is_one())
            return;
    }
}

bool arith_rewriter::div_polynomial(expr * t, numeral const & g, const_treatment ct, expr_ref & result) {
    SASSERT(m_util.is_int(t));
    SASSERT(!g.is_one());
    unsigned sz;
    expr * const * ms = get_monomials(t, sz);
    expr_ref_buffer new_args(m()); 
    numeral a;
    for (unsigned i = 0; i < sz; i++) {
        expr * arg = ms[i];
        if (is_numeral(arg, a)) {
            a /= g;
            if (!a.is_int()) {
                switch (ct) {
                case CT_FLOOR:
                    a = floor(a);
                    break;
                case CT_CEIL:
                    a = ceil(a);
                    break;
                case CT_FALSE:
                    return false;
                }
            }
            if (!a.is_zero())
                new_args.push_back(m_util.mk_numeral(a, true));
            continue;
        }
        expr * pp = get_power_product(arg, a);
        a /= g;
        SASSERT(a.is_int());
        if (!a.is_zero()) {
            if (a.is_one())
                new_args.push_back(pp);
            else
                new_args.push_back(m_util.mk_mul(m_util.mk_numeral(a, true), pp));
        }
    }
    switch (new_args.size()) {
    case 0: result = m_util.mk_numeral(numeral(0), true); return true;
    case 1: result = new_args[0]; return true;
    default: result = m_util.mk_add(new_args.size(), new_args.c_ptr()); return true;
    }
}

bool arith_rewriter::is_bound(expr * arg1, expr * arg2, op_kind kind, expr_ref & result) {
    numeral c;
    if (!is_add(arg1) && is_numeral(arg2, c)) {
        numeral a;
        bool r = false;
        expr * pp = get_power_product(arg1, a);
        if (a.is_neg()) {
            a.neg();
            c.neg();
            kind = inv(kind);
            r = true;
        }
        if (!a.is_one())
            r = true;
        if (!r)
            return false;
        c /= a;
        bool is_int = m_util.is_int(arg1);
        if (is_int && !c.is_int()) {
            switch (kind) {
            case LE: c = floor(c); break;
            case GE: c = ceil(c); break;
            case EQ: result = m().mk_false(); return true;
            }
        }
        expr * k = m_util.mk_numeral(c, is_int);
        switch (kind) {
        case LE: result = m_util.mk_le(pp, k); return true;
        case GE: result = m_util.mk_ge(pp, k); return true;
        case EQ: result = m_util.mk_eq(pp, k); return true;
        }
    }
    return false;
}

bool arith_rewriter::elim_to_real_var(expr * var, expr_ref & new_var) {
    numeral val;
    if (m_util.is_numeral(var, val)) {
        if (!val.is_int())
            return false;
        new_var = m_util.mk_numeral(val, true);
        return true;
    }
    else if (m_util.is_to_real(var)) {
        new_var = to_app(var)->get_arg(0);
        return true;
    }
    return false;
}

bool arith_rewriter::elim_to_real_mon(expr * monomial, expr_ref & new_monomial) {
    if (m_util.is_mul(monomial)) {
        expr_ref_buffer new_vars(m());
        expr_ref new_var(m());
        unsigned num = to_app(monomial)->get_num_args();
        for (unsigned i = 0; i < num; i++) {
            if (!elim_to_real_var(to_app(monomial)->get_arg(i), new_var))
                return false;
            new_vars.push_back(new_var);
        }
        new_monomial = m_util.mk_mul(new_vars.size(), new_vars.c_ptr());
        return true;
    }
    else {
        return elim_to_real_var(monomial, new_monomial);
    }
}

bool arith_rewriter::elim_to_real_pol(expr * p, expr_ref & new_p) {
    if (m_util.is_add(p)) {
        expr_ref_buffer new_monomials(m());
        expr_ref new_monomial(m());
        unsigned num = to_app(p)->get_num_args();
        for (unsigned i = 0; i < num; i++) {
            if (!elim_to_real_mon(to_app(p)->get_arg(i), new_monomial))
                return false;
            new_monomials.push_back(new_monomial);
        }
        new_p = m_util.mk_add(new_monomials.size(), new_monomials.c_ptr());
        return true;
    }
    else {
        return elim_to_real_mon(p, new_p);
    }
}

bool arith_rewriter::elim_to_real(expr * arg1, expr * arg2, expr_ref & new_arg1, expr_ref & new_arg2) {
    if (!m_util.is_real(arg1))
        return false;
    return elim_to_real_pol(arg1, new_arg1) && elim_to_real_pol(arg2, new_arg2);
}

bool arith_rewriter::is_reduce_power_target(expr * arg, bool is_eq) {
    unsigned sz;
    expr * const * args;
    if (m_util.is_mul(arg)) {
        sz = to_app(arg)->get_num_args();
        args = to_app(arg)->get_args();
    }
    else {
        sz = 1;
        args = &arg;
    }
    for (unsigned i = 0; i < sz; i++) {
        expr * arg = args[i];
        if (m_util.is_power(arg)) {
            rational k;
            if (m_util.is_numeral(to_app(arg)->get_arg(1), k) && k.is_int() && ((is_eq && k > rational(1)) || (!is_eq && k > rational(2))))
                return true;
        }
    }
    return false;
}

expr * arith_rewriter::reduce_power(expr * arg, bool is_eq) {
    if (is_zero(arg))
        return arg;
    unsigned sz;
    expr * const * args;
    if (m_util.is_mul(arg)) {
        sz = to_app(arg)->get_num_args();
        args = to_app(arg)->get_args();
    }
    else {
        sz = 1;
        args = &arg;
    }
    ptr_buffer<expr> new_args;
    rational k;
    for (unsigned i = 0; i < sz; i++) {
        expr * arg = args[i];
        if (m_util.is_power(arg) && m_util.is_numeral(to_app(arg)->get_arg(1), k) && k.is_int() && ((is_eq && k > rational(1)) || (!is_eq && k > rational(2)))) {
            if (is_eq || !k.is_even())
                new_args.push_back(to_app(arg)->get_arg(0));
            else
                new_args.push_back(m_util.mk_power(to_app(arg)->get_arg(0), m_util.mk_numeral(rational(2), m_util.is_int(arg))));
        }
        else {
            new_args.push_back(arg);
        }
    }
    SASSERT(new_args.size() >= 1);
    if (new_args.size() == 1)
        return new_args[0];
    else
        return m_util.mk_mul(new_args.size(), new_args.c_ptr());
}

br_status arith_rewriter::reduce_power(expr * arg1, expr * arg2, op_kind kind, expr_ref & result) {
    expr * new_arg1 = reduce_power(arg1, kind == EQ);
    expr * new_arg2 = reduce_power(arg2, kind == EQ);
    switch (kind) {
    case LE: result = m_util.mk_le(new_arg1, new_arg2); return BR_REWRITE1;
    case GE: result = m_util.mk_ge(new_arg1, new_arg2); return BR_REWRITE1;
    default: result = m().mk_eq(new_arg1, new_arg2); return BR_REWRITE1;
    }
}

br_status arith_rewriter::mk_le_ge_eq_core(expr * arg1, expr * arg2, op_kind kind, expr_ref & result) {
    expr_ref new_arg1(m());
    expr_ref new_arg2(m());
    if ((is_zero(arg1) && is_reduce_power_target(arg2, kind == EQ)) ||
        (is_zero(arg2) && is_reduce_power_target(arg1, kind == EQ)))
        return reduce_power(arg1, arg2, kind, result);
    CTRACE("elim_to_real", m_elim_to_real, tout << "after_elim_to_real\n" << mk_ismt2_pp(arg1, m()) << "\n" << mk_ismt2_pp(arg2, m()) << "\n";);
    br_status st = cancel_monomials(arg1, arg2, m_arith_lhs, new_arg1, new_arg2);
    TRACE("mk_le_bug", tout << "st: " << st << "\n";);
    if (st != BR_FAILED) {
        arg1 = new_arg1;
        arg2 = new_arg2;
    }
    expr_ref new_new_arg1(m());
    expr_ref new_new_arg2(m());
    if (m_elim_to_real && elim_to_real(arg1, arg2, new_new_arg1, new_new_arg2)) {
        arg1 = new_new_arg1;
        arg2 = new_new_arg2;
        if (st == BR_FAILED)
            st = BR_DONE;
    }
    numeral a1, a2;
    if (is_numeral(arg1, a1) && is_numeral(arg2, a2)) {
        switch (kind) {
        case LE: result = a1 <= a2 ? m().mk_true() : m().mk_false(); return BR_DONE;
        case GE: result = a1 >= a2 ? m().mk_true() : m().mk_false(); return BR_DONE;
        default: result = a1 == a2 ? m().mk_true() : m().mk_false(); return BR_DONE;
        }
    }

#define ANUM_LE_GE_EQ() {                                                               \
    switch (kind) {                                                                     \
    case LE: result = am.le(v1, v2) ? m().mk_true() : m().mk_false(); return BR_DONE;   \
    case GE: result = am.ge(v1, v2) ? m().mk_true() : m().mk_false(); return BR_DONE;   \
    default: result = am.eq(v1, v2) ? m().mk_true() : m().mk_false(); return BR_DONE;   \
    }                                                                                   \
}

    if (m_anum_simp) {
        if (is_numeral(arg1, a1) && m_util.is_irrational_algebraic_numeral(arg2)) {
            anum_manager & am = m_util.am();                              
            scoped_anum v1(am);
            am.set(v1, a1.to_mpq());
            anum const & v2 = m_util.to_irrational_algebraic_numeral(arg2);
            ANUM_LE_GE_EQ();
        }
        if (m_util.is_irrational_algebraic_numeral(arg1) && is_numeral(arg2, a2)) {
            anum_manager & am = m_util.am();                              
            anum const & v1 = m_util.to_irrational_algebraic_numeral(arg1);
            scoped_anum v2(am);
            am.set(v2, a2.to_mpq());
            ANUM_LE_GE_EQ();
        }
        if (m_util.is_irrational_algebraic_numeral(arg1) && m_util.is_irrational_algebraic_numeral(arg2)) {
            anum_manager & am = m_util.am();                              
            anum const & v1 = m_util.to_irrational_algebraic_numeral(arg1);
            anum const & v2 = m_util.to_irrational_algebraic_numeral(arg2);
            ANUM_LE_GE_EQ();
        }
    }
    if (is_bound(arg1, arg2, kind, result))
        return BR_DONE;
    if (is_bound(arg2, arg1, inv(kind), result))
        return BR_DONE;
    bool is_int = m_util.is_int(arg1);
    if (is_int && m_gcd_rounding) {
        bool first = true;
        numeral g; 
        unsigned num_consts = 0;
        get_coeffs_gcd(arg1, g, first, num_consts);
        TRACE("arith_rewriter_gcd", tout << "[step1] g: " << g << ", num_consts: " << num_consts << "\n";);
        if ((first || !g.is_one()) && num_consts <= 1)
            get_coeffs_gcd(arg2, g, first, num_consts);
        TRACE("arith_rewriter_gcd", tout << "[step2] g: " << g << ", num_consts: " << num_consts << "\n";);
        if (!first && !g.is_one() && num_consts <= 1) {
            bool is_sat = div_polynomial(arg1, g, (kind == LE ? CT_CEIL : (kind == GE ? CT_FLOOR : CT_FALSE)), new_arg1);
            if (!is_sat) {
                result = m().mk_false();
                return BR_DONE;
            }
            is_sat = div_polynomial(arg2, g, (kind == LE ? CT_FLOOR : (kind == GE ? CT_CEIL : CT_FALSE)), new_arg2);
            if (!is_sat) {
                result = m().mk_false();
                return BR_DONE;
            }
            arg1 = new_arg1.get();
            arg2 = new_arg2.get();
            st = BR_DONE;
        }
    }
    if (st != BR_FAILED) {
        switch (kind) {
        case LE: result = m_util.mk_le(arg1, arg2); return BR_DONE;
        case GE: result = m_util.mk_ge(arg1, arg2); return BR_DONE;
        default: result = m().mk_eq(arg1, arg2); return BR_DONE;
        }
    }
    return BR_FAILED;
}

br_status arith_rewriter::mk_le_core(expr * arg1, expr * arg2, expr_ref & result) {
    return mk_le_ge_eq_core(arg1, arg2, LE, result);
}

br_status arith_rewriter::mk_lt_core(expr * arg1, expr * arg2, expr_ref & result) {
    result = m().mk_not(m_util.mk_le(arg2, arg1));
    return BR_REWRITE2;
}

br_status arith_rewriter::mk_ge_core(expr * arg1, expr * arg2, expr_ref & result) {
    return mk_le_ge_eq_core(arg1, arg2, GE, result);
}

br_status arith_rewriter::mk_gt_core(expr * arg1, expr * arg2, expr_ref & result) {
    result = m().mk_not(m_util.mk_le(arg1, arg2));
    return BR_REWRITE2;
}

br_status arith_rewriter::mk_eq_core(expr * arg1, expr * arg2, expr_ref & result) {
    if (m_eq2ineq) {
        result = m().mk_and(m_util.mk_le(arg1, arg2), m_util.mk_ge(arg1, arg2));
        return BR_REWRITE2;
    }
    return mk_le_ge_eq_core(arg1, arg2, EQ, result);
}

bool arith_rewriter::is_anum_simp_target(unsigned num_args, expr * const * args) {
    if (!m_anum_simp)
        return false;
    unsigned num_irrat = 0;
    unsigned num_rat   = 0;
    for (unsigned i = 0; i < num_args; i++) {
        if (m_util.is_numeral(args[i])) {
            num_rat++;
            if (num_irrat > 0)
                return true;
        }
        if (m_util.is_irrational_algebraic_numeral(args[i]) && 
            m_util.am().degree(m_util.to_irrational_algebraic_numeral(args[i])) <= m_max_degree) {
            num_irrat++;
            if (num_irrat > 1 || num_rat > 0)
                return true;
        }
    }
    return false;
}

br_status arith_rewriter::mk_add_core(unsigned num_args, expr * const * args, expr_ref & result) {
    if (is_anum_simp_target(num_args, args)) {
        expr_ref_buffer new_args(m()); 
        anum_manager & am = m_util.am();                              
        scoped_anum r(am);     
        scoped_anum arg(am);
        rational rarg;
        am.set(r, 0);
        for (unsigned i = 0; i < num_args; i ++) {
            unsigned d = am.degree(r);
            if (d > 1 && d > m_max_degree) {
                new_args.push_back(m_util.mk_numeral(r, false));
                am.set(r, 0);
            }
            
            if (m_util.is_numeral(args[i], rarg)) {
                am.set(arg, rarg.to_mpq());
                am.add(r, arg, r);
                continue;
            }
            
            if (m_util.is_irrational_algebraic_numeral(args[i])) {
                anum const & irarg = m_util.to_irrational_algebraic_numeral(args[i]);
                if (am.degree(irarg) <= m_max_degree) {
                    am.add(r, irarg, r);
                    continue;
                }
            }

            new_args.push_back(args[i]);
        }

        if (new_args.empty()) {
            result = m_util.mk_numeral(r, false);
            return BR_DONE;
        }

        new_args.push_back(m_util.mk_numeral(r, false));
        br_status st = poly_rewriter<arith_rewriter_core>::mk_add_core(new_args.size(), new_args.c_ptr(), result);
        if (st == BR_FAILED) {
            result = m().mk_app(get_fid(), OP_ADD, new_args.size(), new_args.c_ptr());
            return BR_DONE;
        }
        return st;
    }
    else {
        return poly_rewriter<arith_rewriter_core>::mk_add_core(num_args, args, result);
    }
}

br_status arith_rewriter::mk_mul_core(unsigned num_args, expr * const * args, expr_ref & result) {
    if (is_anum_simp_target(num_args, args)) {
        expr_ref_buffer new_args(m()); 
        anum_manager & am = m_util.am();                              
        scoped_anum r(am);     
        scoped_anum arg(am);
        rational rarg;
        am.set(r, 1);
        for (unsigned i = 0; i < num_args; i ++) {
            unsigned d = am.degree(r);
            if (d > 1 && d > m_max_degree) {
                new_args.push_back(m_util.mk_numeral(r, false));
                am.set(r, 1);
            }

            if (m_util.is_numeral(args[i], rarg)) {
                am.set(arg, rarg.to_mpq());
                am.mul(r, arg, r);
                continue;
            }
            if (m_util.is_irrational_algebraic_numeral(args[i])) {
                anum const & irarg = m_util.to_irrational_algebraic_numeral(args[i]);
                if (am.degree(irarg) <= m_max_degree) {
                    am.mul(r, irarg, r);                    
                    continue;
                }
            }

            new_args.push_back(args[i]);
        }

        if (new_args.empty()) {
            result = m_util.mk_numeral(r, false);
            return BR_DONE;
        }
        new_args.push_back(m_util.mk_numeral(r, false));
        
        br_status st = poly_rewriter<arith_rewriter_core>::mk_mul_core(new_args.size(), new_args.c_ptr(), result);
        if (st == BR_FAILED) {
            result = m().mk_app(get_fid(), OP_MUL, new_args.size(), new_args.c_ptr());
            return BR_DONE;
        }
        return st;
    }
    else {
        return poly_rewriter<arith_rewriter_core>::mk_mul_core(num_args, args, result);
    }
}

br_status arith_rewriter::mk_div_irrat_rat(expr * arg1, expr * arg2, expr_ref & result) { 
    SASSERT(m_util.is_real(arg1));                                              
    SASSERT(m_util.is_irrational_algebraic_numeral(arg1));                      
    SASSERT(m_util.is_numeral(arg2));                                           
    anum_manager & am = m_util.am();                                            
    anum const & val1  = m_util.to_irrational_algebraic_numeral(arg1);          
    rational rval2;                                                             
    VERIFY(m_util.is_numeral(arg2, rval2));
    if (rval2.is_zero())
        return BR_FAILED;
    scoped_anum val2(am);                                                       
    am.set(val2, rval2.to_mpq());                                                        
    scoped_anum r(am);                                                          
    am.div(val1, val2, r);                                                       
    result = m_util.mk_numeral(r, false);                                       
    return BR_DONE;                                                             
}

br_status arith_rewriter::mk_div_rat_irrat(expr * arg1, expr * arg2, expr_ref & result) { 
    SASSERT(m_util.is_real(arg1));                                              
    SASSERT(m_util.is_numeral(arg1));                                           
    SASSERT(m_util.is_irrational_algebraic_numeral(arg2));                      
    anum_manager & am = m_util.am();                                            
    rational rval1;                                                             
    VERIFY(m_util.is_numeral(arg1, rval1));
    scoped_anum val1(am);                                                       
    am.set(val1, rval1.to_mpq());
    anum const & val2  = m_util.to_irrational_algebraic_numeral(arg2);          
    scoped_anum r(am);                                                          
    am.div(val1, val2, r);                                                       
    result = m_util.mk_numeral(r, false);                                       
    return BR_DONE;                                                             
}

br_status arith_rewriter::mk_div_irrat_irrat(expr * arg1, expr * arg2, expr_ref & result) {  
    SASSERT(m_util.is_real(arg1));                                              
    SASSERT(m_util.is_irrational_algebraic_numeral(arg1));                      
    SASSERT(m_util.is_irrational_algebraic_numeral(arg2));                      
    anum_manager & am = m_util.am();                                            
    anum const & val1  = m_util.to_irrational_algebraic_numeral(arg1);          
    if (am.degree(val1) > m_max_degree)                                    
        return BR_FAILED;                                                       
    anum const & val2  = m_util.to_irrational_algebraic_numeral(arg2);          
    if (am.degree(val2) > m_max_degree)                                    
        return BR_FAILED;                                                       
    scoped_anum r(am);
    am.div(val1, val2, r);
    result = m_util.mk_numeral(r, false);
    return BR_DONE;                                       
}

br_status arith_rewriter::mk_div_core(expr * arg1, expr * arg2, expr_ref & result) {
    if (m_anum_simp) {
        if (m_util.is_irrational_algebraic_numeral(arg1) && m_util.is_numeral(arg2))
            return mk_div_irrat_rat(arg1, arg2, result);
        if (m_util.is_irrational_algebraic_numeral(arg1) && m_util.is_irrational_algebraic_numeral(arg2))
            return mk_div_irrat_irrat(arg1, arg2, result);
        if (m_util.is_irrational_algebraic_numeral(arg2) && m_util.is_numeral(arg1))
            return mk_div_rat_irrat(arg1, arg2, result);
    }
    set_curr_sort(m().get_sort(arg1));
    numeral v1, v2;
    bool is_int;
    if (m_util.is_numeral(arg2, v2, is_int) && !v2.is_zero()) {
        SASSERT(!is_int);
        if (m_util.is_numeral(arg1, v1, is_int)) {
            result = m_util.mk_numeral(v1/v2, false);
            return BR_DONE;
        }
        else {
            numeral k(1);
            k /= v2;
            result = m().mk_app(get_fid(), OP_MUL,
                                m_util.mk_numeral(k, false),
                                arg1);
            return BR_REWRITE1;
        }
    }

    if (!m_util.is_int(arg1)) {
        // (/ (* v1 b) (* v2 d)) --> (* v1/v2 (/ b d))
        expr * a, * b, * c, * d;
        if (m_util.is_mul(arg1, a, b) && m_util.is_numeral(a, v1)) {
            // do nothing arg1 is of the form v1 * b
        }
        else {
            v1 = rational(1);
            b  = arg1;
        }
        if (m_util.is_mul(arg2, c, d) && m_util.is_numeral(c, v2)) {
            // do nothing arg2 is of the form v2 * d
        }
        else {
            v2 = rational(1);
            d  = arg2;
        }
        TRACE("div_bug", tout << "v1: " << v1 << ", v2: " << v2 << "\n";);
        if (!v1.is_one() || !v2.is_one()) {
            v1 /= v2;
            result = m_util.mk_mul(m_util.mk_numeral(v1, false),
                                   m_util.mk_div(b, d));
            return BR_REWRITE2;
        }
    }

    return BR_FAILED;
}

br_status arith_rewriter::mk_idiv_core(expr * arg1, expr * arg2, expr_ref & result) {
    set_curr_sort(m().get_sort(arg1));
    numeral v1, v2;
    bool is_int;
    if (m_util.is_numeral(arg1, v1, is_int) && m_util.is_numeral(arg2, v2, is_int) && !v2.is_zero()) {
        result = m_util.mk_numeral(div(v1, v2), is_int);
        return BR_DONE;
    }
    return BR_FAILED;
}
 
br_status arith_rewriter::mk_mod_core(expr * arg1, expr * arg2, expr_ref & result) {
    set_curr_sort(m().get_sort(arg1));
    numeral v1, v2;
    bool is_int;
    if (m_util.is_numeral(arg1, v1, is_int) && m_util.is_numeral(arg2, v2, is_int) && !v2.is_zero()) {
        result = m_util.mk_numeral(mod(v1, v2), is_int);
        return BR_DONE;
    }
    
    if (m_util.is_numeral(arg2, v2, is_int) && is_int && v2.is_one()) {
        result = m_util.mk_numeral(numeral(0), true);
        return BR_DONE;
    }

    // mod is idempotent on non-zero modulus.
    expr* t1, *t2;
    if (m_util.is_mod(arg1, t1, t2) && t2 == arg2 && m_util.is_numeral(arg2, v2, is_int) && is_int && !v2.is_zero()) {
        result = arg1;
        return BR_DONE;
    }

    // propagate mod inside only if not all arguments are not already mod.
    if (m_util.is_numeral(arg2, v2, is_int) && is_int && v2.is_pos() && (is_add(arg1) || is_mul(arg1))) {
        TRACE("mod_bug", tout << "mk_mod:\n" << mk_ismt2_pp(arg1, m()) << "\n" << mk_ismt2_pp(arg2, m()) << "\n";);
        unsigned num_args = to_app(arg1)->get_num_args();
        unsigned i;
        rational arg_v;
        for (i = 0; i < num_args; i++) {
            expr * arg = to_app(arg1)->get_arg(i);
            if (m_util.is_mod(arg))
                continue;
            if (m_util.is_numeral(arg, arg_v) && mod(arg_v, v2) == arg_v)
                continue;
            // found target for rewriting
            break;
        }
        TRACE("mod_bug", tout << "mk_mod target: " << i << "\n";);
        if (i == num_args)
            return BR_FAILED; // did not find any target for applying simplification
        ptr_buffer<expr> new_args;
        for (unsigned i = 0; i < num_args; i++)
            new_args.push_back(m_util.mk_mod(to_app(arg1)->get_arg(i), arg2));
        result = m_util.mk_mod(m().mk_app(to_app(arg1)->get_decl(), new_args.size(), new_args.c_ptr()), arg2);
        TRACE("mod_bug", tout << "mk_mod result: " << mk_ismt2_pp(result, m()) << "\n";);
        return BR_REWRITE3;
    }
  
    return BR_FAILED;
}

br_status arith_rewriter::mk_rem_core(expr * arg1, expr * arg2, expr_ref & result) {
    set_curr_sort(m().get_sort(arg1));
    numeral v1, v2;
    bool is_int;
    if (m_util.is_numeral(arg1, v1, is_int) && m_util.is_numeral(arg2, v2, is_int) && !v2.is_zero()) {
        numeral m = mod(v1, v2);
        //
        // rem(v1,v2) = if v2 >= 0 then mod(v1,v2) else -mod(v1,v2)
        //
        if (v2.is_neg()) {
            m.neg();
        }
        result = m_util.mk_numeral(m, is_int);
        return BR_DONE;
    }
    else if (m_util.is_numeral(arg2, v2, is_int) && is_int && v2.is_one()) {
        result = m_util.mk_numeral(numeral(0), true);
        return BR_DONE;
    }
    else if (m_util.is_numeral(arg2, v2, is_int) && is_int && !v2.is_zero()) {
        if (is_add(arg1) || is_mul(arg1)) {
            ptr_buffer<expr> new_args;
            unsigned num_args = to_app(arg1)->get_num_args();
            for (unsigned i = 0; i < num_args; i++)
                new_args.push_back(m_util.mk_rem(to_app(arg1)->get_arg(i), arg2));
            result = m().mk_app(to_app(arg1)->get_decl(), new_args.size(), new_args.c_ptr());
            return BR_REWRITE2;
        }
        else {
            if (v2.is_neg()) {
                result = m_util.mk_uminus(m_util.mk_mod(arg1, arg2));
                return BR_REWRITE2;
            }
            else {
                result = m_util.mk_mod(arg1, arg2);
                return BR_REWRITE1;
            }
        }
    }
    else if (m_elim_rem) {
        expr * mod = m_util.mk_mod(arg1, arg2);
        result = m().mk_ite(m_util.mk_ge(arg2, m_util.mk_numeral(rational(0), true)),
                            mod,
                            m_util.mk_uminus(mod));
        TRACE("elim_rem", tout << "result: " << mk_ismt2_pp(result, m()) << "\n";);
        return BR_REWRITE3;
    }
    return BR_FAILED;
}

br_status arith_rewriter::mk_power_core(expr * arg1, expr * arg2, expr_ref & result) {
    numeral x, y;
    bool is_num_x    = m_util.is_numeral(arg1, x);
    bool is_num_y    = m_util.is_numeral(arg2, y);
    bool is_int_sort = m_util.is_int(arg1);
    
    if ((is_num_x && x.is_one()) || 
        (is_num_y && y.is_one())) {
        result = arg1;
        return BR_DONE;
    }

    if (is_num_x && is_num_y) {
        if (x.is_zero() && y.is_zero())
            return BR_FAILED;
        
        if (y.is_zero()) { 
            result = m_util.mk_numeral(rational(1), m().get_sort(arg1));
            return BR_DONE;
        }

        if (x.is_zero()) {
            result = arg1;
            return BR_DONE;
        }

        if (y.is_unsigned() && y.get_unsigned() <= m_max_degree) { 
            x = power(x, y.get_unsigned());
            result = m_util.mk_numeral(x, m().get_sort(arg1));
            return BR_DONE;
        }
        
        if (!is_int_sort && (-y).is_unsigned() && (-y).get_unsigned() <= m_max_degree) {
            x = power(rational(1)/x, (-y).get_unsigned());
            result = m_util.mk_numeral(x, m().get_sort(arg1));
            return BR_DONE;
        }
    }

    if (m_util.is_power(arg1) && is_num_y && y.is_int() && !y.is_zero()) {
        // (^ (^ t y2) y) --> (^ t (* y2 y))  If y2 > 0 && y != 0 && y and y2 are integers
        rational y2;
        if (m_util.is_numeral(to_app(arg1)->get_arg(1), y2) && y2.is_int() && y2.is_pos()) {
            result = m_util.mk_power(to_app(arg1)->get_arg(0), m_util.mk_numeral(y*y2, is_int_sort));
            return BR_REWRITE1;
        }
    }

    if (!is_int_sort && is_num_y && y.is_neg()) {
        // (^ t -k) --> (^ (/ 1 t) k)
        result = m_util.mk_power(m_util.mk_div(m_util.mk_numeral(rational(1), false), arg1),
                                 m_util.mk_numeral(-y, false));
        return BR_REWRITE2;
    }

    if (!is_int_sort && is_num_y && !y.is_int() && !numerator(y).is_one()) {
        // (^ t (/ p q)) --> (^ (^ t (/ 1 q)) p)
        result = m_util.mk_power(m_util.mk_power(arg1, m_util.mk_numeral(rational(1)/denominator(y), false)),
                                 m_util.mk_numeral(numerator(y), false));
        return BR_REWRITE2;
    }

    if ((m_expand_power || (m_som && is_app(arg1) && to_app(arg1)->get_family_id() == get_fid())) &&
        is_num_y && y.is_unsigned() && 1 < y.get_unsigned() && y.get_unsigned() <= m_max_degree) {
        ptr_buffer<expr> args;
        unsigned k = y.get_unsigned();
        for (unsigned i = 0; i < k; i++) {
            args.push_back(arg1);
        }
        result = m_util.mk_mul(args.size(), args.c_ptr());
        return BR_REWRITE1;
    }

    if (!is_num_y)
        return BR_FAILED;

    bool is_irrat_x = m_util.is_irrational_algebraic_numeral(arg1);
    
    if (!is_num_x && !is_irrat_x) 
        return BR_FAILED;
    
    rational num_y = numerator(y);
    rational den_y = denominator(y);
    bool is_neg_y  = false;
    if (num_y.is_neg()) {
        num_y.neg();
        is_neg_y = true;
    }
    SASSERT(num_y.is_pos());
    SASSERT(den_y.is_pos());
    
    if (is_neg_y && is_int_sort)
        return BR_FAILED;

    if (!num_y.is_unsigned() || !den_y.is_unsigned())
        return BR_FAILED;

    unsigned u_num_y = num_y.get_unsigned();
    unsigned u_den_y = den_y.get_unsigned();
    
    if (u_num_y > m_max_degree || u_den_y > m_max_degree)
        return BR_FAILED;
    
    if (is_num_x) {
        rational xk, r;
        xk = power(x, u_num_y);
        if (xk.root(u_den_y, r)) {
            if (is_neg_y)
                r = rational(1)/r;
            result = m_util.mk_numeral(r, m().get_sort(arg1));
            return BR_DONE;
        }
        if (m_anum_simp) {
            anum_manager & am = m_util.am();
            scoped_anum r(am);
            am.set(r, xk.to_mpq());
            am.root(r, u_den_y, r);
            if (is_neg_y)
                am.inv(r);
            result = m_util.mk_numeral(r, false);
            return BR_DONE;
        }
        return BR_FAILED;
    }

    SASSERT(is_irrat_x);
    if (!m_anum_simp)
        return BR_FAILED;

    anum const & val  = m_util.to_irrational_algebraic_numeral(arg1);
    anum_manager & am = m_util.am();
    if (am.degree(val) > m_max_degree)
        return BR_FAILED;
    scoped_anum r(am);
    am.power(val, u_num_y, r);
    am.root(r, u_den_y, r);
    if (is_neg_y)
        am.inv(r);
    result = m_util.mk_numeral(r, false);
    return BR_DONE;
}

br_status arith_rewriter::mk_to_int_core(expr * arg, expr_ref & result) {
    numeral a;
    if (m_util.is_numeral(arg, a)) {
        result = m_util.mk_numeral(floor(a), true);
        return BR_DONE;
    }
    else if (m_util.is_to_real(arg)) {
        result = to_app(arg)->get_arg(0);
        return BR_DONE;
    }
    else {
        return BR_FAILED;
    }
}

br_status arith_rewriter::mk_to_real_core(expr * arg, expr_ref & result) {
    numeral a;
    if (m_util.is_numeral(arg, a)) {
        result = m_util.mk_numeral(a, false);
        return BR_DONE;
    }
#if 0
    // The following rewriting rule is not correct.
    // It is used only for making experiments.
    if (m_util.is_to_int(arg)) {
        result = to_app(arg)->get_arg(0);
        return BR_DONE;
    }
#endif
    // push to_real over OP_ADD, OP_MUL
    if (m_push_to_real) {
        if (m_util.is_add(arg) || m_util.is_mul(arg)) {
            ptr_buffer<expr> new_args;
            unsigned num = to_app(arg)->get_num_args();
            for (unsigned i = 0; i < num; i++) {
                new_args.push_back(m_util.mk_to_real(to_app(arg)->get_arg(i)));
            }
            if (m_util.is_add(arg)) 
                result = m().mk_app(get_fid(), OP_ADD, new_args.size(), new_args.c_ptr());
            else
                result = m().mk_app(get_fid(), OP_MUL, new_args.size(), new_args.c_ptr());
            return BR_REWRITE2;
        }
    }
    return BR_FAILED;
}

br_status arith_rewriter::mk_is_int(expr * arg, expr_ref & result) {
    numeral a;
    if (m_util.is_numeral(arg, a)) {
        result = a.is_int() ? m().mk_true() : m().mk_false();
        return BR_DONE;
    }
    else if (m_util.is_to_real(arg)) {
        result = m().mk_true();
        return BR_DONE;
    }
    else {
        result = m().mk_eq(m().mk_app(get_fid(), OP_TO_REAL,
                                      m().mk_app(get_fid(), OP_TO_INT, arg)),
                           arg);
        return BR_REWRITE3;
    }
}

void arith_rewriter::set_cancel(bool f) {
    m_util.set_cancel(f);
}

// Return true if t is of the form  c*Pi where c is a numeral.
// Store c into k
bool arith_rewriter::is_pi_multiple(expr * t, rational & k) {
    if (m_util.is_pi(t)) {
        k = rational(1);
        return true;
    }
    expr * a, * b;
    return m_util.is_mul(t, a, b) && m_util.is_pi(b) && m_util.is_numeral(a, k);
}

// Return true if t is of the form  (+ s c*Pi) where c is a numeral.
// Store c into k, and c*Pi into m.
bool arith_rewriter::is_pi_offset(expr * t, rational & k, expr * & m) {
    if (m_util.is_add(t)) {
        unsigned num = to_app(t)->get_num_args();
        for (unsigned i = 0; i < num; i++) {
            expr * arg = to_app(t)->get_arg(i);
            if (is_pi_multiple(arg, k)) {
                m = arg;
                return true;
            }
        }
    }
    return false;
}

// Return true if t is of the form 2*pi*to_real(s).
bool arith_rewriter::is_2_pi_integer(expr * t) {
    expr * a, * m, * b, * c;
    rational k;
    return 
        m_util.is_mul(t, a, m) && 
        m_util.is_numeral(a, k) && 
        k.is_int() &&
        mod(k, rational(2)).is_zero() &&
        m_util.is_mul(m, b, c) &&
        ((m_util.is_pi(b) && m_util.is_to_real(c)) || (m_util.is_to_real(b) && m_util.is_pi(c)));
}

// Return true if t is of the form s + 2*pi*to_real(s).
// Store 2*pi*to_real(s) into m.
bool arith_rewriter::is_2_pi_integer_offset(expr * t, expr * & m) {
    if (m_util.is_add(t)) {
        unsigned num = to_app(t)->get_num_args();
        for (unsigned i = 0; i < num; i++) {
            expr * arg = to_app(t)->get_arg(i);
            if (is_2_pi_integer(arg)) {
                m = arg;
                return true;
            }
        }
    }
    return false;
}

// Return true if t is of the form pi*to_real(s).
bool arith_rewriter::is_pi_integer(expr * t) {
    expr * a, * b;
    if (m_util.is_mul(t, a, b)) {
        rational k;
        if (m_util.is_numeral(a, k)) {
            if (!k.is_int())
                return false;
            expr * c, * d;
            if (!m_util.is_mul(b, c, d))
                return false;
            a = c;
            b = d;
        }
        TRACE("tan", tout << "is_pi_integer " << mk_ismt2_pp(t, m()) << "\n";
              tout << "a: " << mk_ismt2_pp(a, m()) << "\n";
              tout << "b: " << mk_ismt2_pp(b, m()) << "\n";);
        return 
            (m_util.is_pi(a) && m_util.is_to_real(b)) ||
            (m_util.is_to_real(a) && m_util.is_pi(b));
    }
    return false;
}

// Return true if t is of the form s + pi*to_real(s).
// Store 2*pi*to_real(s) into m.
bool arith_rewriter::is_pi_integer_offset(expr * t, expr * & m) {
    if (m_util.is_add(t)) {
        unsigned num = to_app(t)->get_num_args();
        for (unsigned i = 0; i < num; i++) {
            expr * arg = to_app(t)->get_arg(i);
            if (is_pi_integer(arg)) {
                m = arg;
                return true;
            }
        }
    }
    return false;
}

app * arith_rewriter::mk_sqrt(rational const & k) {
    return m_util.mk_power(m_util.mk_numeral(k, false), m_util.mk_numeral(rational(1, 2), false));
}

// Return a constant representing sin(k * pi).
// Return 0 if failed.
expr * arith_rewriter::mk_sin_value(rational const & k) {
    rational k_prime = mod(floor(k), rational(2)) + k - floor(k);
    TRACE("sine", tout << "k: " << k << ", k_prime: " << k_prime << "\n";);
    SASSERT(k_prime >= rational(0) && k_prime < rational(2));
    bool     neg = false;
    if (k_prime >= rational(1)) {
        neg     = true;
        k_prime = k_prime - rational(1); 
    }
    SASSERT(k_prime >= rational(0) && k_prime < rational(1));
    if (k_prime.is_zero() || k_prime.is_one()) {
        // sin(0) == sin(pi) == 0
        return m_util.mk_numeral(rational(0), false);
    }
    if (k_prime == rational(1, 2)) {
        // sin(pi/2) == 1,  sin(3/2 pi) == -1
        return m_util.mk_numeral(rational(neg ? -1 : 1), false);
    }
    if (k_prime == rational(1, 6) || k_prime == rational(5, 6)) {
        // sin(pi/6)   == sin(5/6 pi)  == 1/2
        // sin(7 pi/6) == sin(11/6 pi) == -1/2
        return m_util.mk_numeral(rational(neg ? -1 : 1, 2), false);
    }
    if (k_prime == rational(1, 4) || k_prime == rational(3, 4)) {
        // sin(pi/4)   == sin(3/4 pi) ==   Sqrt(1/2)
        // sin(5/4 pi) == sin(7/4 pi) == - Sqrt(1/2)
        expr * result = mk_sqrt(rational(1, 2));
        return neg ? m_util.mk_uminus(result) : result;
    }
    if (k_prime == rational(1, 3) || k_prime == rational(2, 3)) {
        // sin(pi/3)   == sin(2/3 pi) ==   Sqrt(3)/2
        // sin(4/3 pi) == sin(5/3 pi) == - Sqrt(3)/2
        expr * result = m_util.mk_div(mk_sqrt(rational(3)), m_util.mk_numeral(rational(2), false));
        return neg ? m_util.mk_uminus(result) : result;
    }
    if (k_prime == rational(1, 12) || k_prime == rational(11, 12)) {
        // sin(1/12 pi)  == sin(11/12 pi)  ==  [sqrt(6) - sqrt(2)]/4
        // sin(13/12 pi) == sin(23/12 pi)  == -[sqrt(6) - sqrt(2)]/4
        expr * result = m_util.mk_div(m_util.mk_sub(mk_sqrt(rational(6)), mk_sqrt(rational(2))), m_util.mk_numeral(rational(4), false));
        return neg ? m_util.mk_uminus(result) : result;
    }
    if (k_prime == rational(5, 12) || k_prime == rational(7, 12)) {
        // sin(5/12 pi)  == sin(7/12 pi)   == [sqrt(6) + sqrt(2)]/4
        // sin(17/12 pi) == sin(19/12 pi)  == -[sqrt(6) + sqrt(2)]/4
        expr * result = m_util.mk_div(m_util.mk_add(mk_sqrt(rational(6)), mk_sqrt(rational(2))), m_util.mk_numeral(rational(4), false));
        return neg ? m_util.mk_uminus(result) : result;
    }
    return 0;
}

br_status arith_rewriter::mk_sin_core(expr * arg, expr_ref & result) {
    if (is_app_of(arg, get_fid(), OP_ASIN)) {
        // sin(asin(x)) == x
        result = to_app(arg)->get_arg(0);
        return BR_DONE;
    }
    rational k;
    if (is_numeral(arg, k) && k.is_zero()) {
        // sin(0) == 0
        result = arg;
        return BR_DONE;
    }

    if (is_pi_multiple(arg, k)) {
        result = mk_sin_value(k);
        if (result.get() != 0)
            return BR_REWRITE_FULL;
    }

    expr * m;
    if (is_pi_offset(arg, k, m)) {
        rational k_prime = mod(floor(k), rational(2)) + k - floor(k);
        SASSERT(k_prime >= rational(0) && k_prime < rational(2));
        if (k_prime.is_zero()) {
            // sin(x + 2*n*pi) == sin(x)
            result = m_util.mk_sin(m_util.mk_sub(arg, m));
            return BR_REWRITE2;
        }
        if (k_prime == rational(1, 2)) {
            // sin(x + pi/2 + 2*n*pi) == cos(x)
            result = m_util.mk_cos(m_util.mk_sub(arg, m));
            return BR_REWRITE2;
        }
        if (k_prime.is_one()) {
            // sin(x + pi + 2*n*pi) == -sin(x)
            result = m_util.mk_uminus(m_util.mk_sin(m_util.mk_sub(arg, m)));
            return BR_REWRITE3;
        }
        if (k_prime == rational(3, 2)) {
            // sin(x + 3/2*pi + 2*n*pi) == -cos(x)
            result = m_util.mk_uminus(m_util.mk_cos(m_util.mk_sub(arg, m)));
            return BR_REWRITE3;
        }
    }

    if (is_2_pi_integer_offset(arg, m)) {
        // sin(x + 2*pi*to_real(a)) == sin(x)
        result = m_util.mk_sin(m_util.mk_sub(arg, m));
        return BR_REWRITE2;
    }

    return BR_FAILED;
}

br_status arith_rewriter::mk_cos_core(expr * arg, expr_ref & result) {
    if (is_app_of(arg, get_fid(), OP_ACOS)) {
        // cos(acos(x)) == x
        result = to_app(arg)->get_arg(0);
        return BR_DONE;
    }

    rational k;
    if (is_numeral(arg, k) && k.is_zero()) {
        // cos(0) == 1
        result = m_util.mk_numeral(rational(1), false);
        return BR_DONE;
    }

    if (is_pi_multiple(arg, k)) {
        k = k + rational(1, 2);
        result = mk_sin_value(k);
        if (result.get() != 0)
            return BR_REWRITE_FULL;
    }

    expr * m;
    if (is_pi_offset(arg, k, m)) {
        rational k_prime = mod(floor(k), rational(2)) + k - floor(k);
        SASSERT(k_prime >= rational(0) && k_prime < rational(2));
        if (k_prime.is_zero()) {
            // cos(x + 2*n*pi) == cos(x)
            result = m_util.mk_cos(m_util.mk_sub(arg, m));
            return BR_REWRITE2;
        }
        if (k_prime == rational(1, 2)) {
            // cos(x + pi/2 + 2*n*pi) == -sin(x)
            result = m_util.mk_uminus(m_util.mk_sin(m_util.mk_sub(arg, m)));
            return BR_REWRITE3;
        }
        if (k_prime.is_one()) {
            // cos(x + pi + 2*n*pi) == -cos(x)
            result = m_util.mk_uminus(m_util.mk_cos(m_util.mk_sub(arg, m)));
            return BR_REWRITE3;
        }
        if (k_prime == rational(3, 2)) {
            // cos(x + 3/2*pi + 2*n*pi) == sin(x)
            result = m_util.mk_sin(m_util.mk_sub(arg, m));
            return BR_REWRITE2;
        }
    }

    if (is_2_pi_integer_offset(arg, m)) {
        // cos(x + 2*pi*to_real(a)) == cos(x)
        result = m_util.mk_cos(m_util.mk_sub(arg, m));
        return BR_REWRITE2;
    }

    return BR_FAILED;
}

br_status arith_rewriter::mk_tan_core(expr * arg, expr_ref & result) {
    if (is_app_of(arg, get_fid(), OP_ATAN)) {
        // tan(atan(x)) == x
        result = to_app(arg)->get_arg(0);
        return BR_DONE;
    }

    rational k;
    if (is_numeral(arg, k) && k.is_zero()) {
        // tan(0) == 0
        result = arg;
        return BR_DONE;
    }

    if (is_pi_multiple(arg, k)) {
        expr_ref n(m()), d(m());
        n = mk_sin_value(k);
        if (n.get() == 0)
            goto end;
        if (is_zero(n)) {
            result = n;
            return BR_DONE;
        }
        k = k + rational(1, 2);
        d = mk_sin_value(k);
        SASSERT(d.get() != 0);
        if (is_zero(d)) {
            goto end;
        }
        result = m_util.mk_div(n, d);
        return BR_REWRITE_FULL;
    }

    expr * m;
    if (is_pi_offset(arg, k, m)) {
        rational k_prime = k - floor(k);
        SASSERT(k_prime >= rational(0) && k_prime < rational(1));
        if (k_prime.is_zero()) {
            // tan(x + n*pi) == tan(x)
            result = m_util.mk_tan(m_util.mk_sub(arg, m));
            return BR_REWRITE2;
        }
    }

    if (is_pi_integer_offset(arg, m)) {
        // tan(x + pi*to_real(a)) == tan(x)
        result = m_util.mk_tan(m_util.mk_sub(arg, m));
        return BR_REWRITE2;
    }

 end:
    if (m_expand_tan) {
        result = m_util.mk_div(m_util.mk_sin(arg), m_util.mk_cos(arg));
        return BR_REWRITE2;
    }
    return BR_FAILED;
}

br_status arith_rewriter::mk_asin_core(expr * arg, expr_ref & result) {
    // Remark: we assume that ForAll x : asin(-x) == asin(x). 
    // Mathematica uses this as an axiom. Although asin is an underspecified function for x < -1 or x > 1.
    // Actually, in Mathematica, asin(x) is a total function that returns a complex number fo x < -1 or x > 1.
    rational k;
    if (is_numeral(arg, k)) {
        if (k.is_zero()) {
            result = arg;
            return BR_DONE;
        }
        if (k < rational(-1)) {
            // asin(-2) == -asin(2)
            // asin(-3) == -asin(3)
            k.neg();
            result = m_util.mk_uminus(m_util.mk_asin(m_util.mk_numeral(k, false)));            
            return BR_REWRITE2;
        }

        if (k > rational(1))
            return BR_FAILED;
        
        bool neg = false;
        if (k.is_neg()) {
            neg = true;
            k.neg();
        }

        if (k.is_one()) {
            // asin(1)  == pi/2
            // asin(-1) == -pi/2
            result = m_util.mk_mul(m_util.mk_numeral(rational(neg ? -1 : 1, 2), false), m_util.mk_pi());
            return BR_REWRITE2;
        }

        if (k == rational(1, 2)) {
            // asin(1/2)  == pi/6
            // asin(-1/2) == -pi/6
            result = m_util.mk_mul(m_util.mk_numeral(rational(neg ? -1 : 1, 6), false), m_util.mk_pi());
            return BR_REWRITE2;
        }
    }

    expr * t;
    if (m_util.is_times_minus_one(arg, t)) {
        // See comment above
        // asin(-x) ==> -asin(x)
        result = m_util.mk_uminus(m_util.mk_asin(t));
        return BR_REWRITE2;
    }

    return BR_FAILED;
}

br_status arith_rewriter::mk_acos_core(expr * arg, expr_ref & result) {
    rational k;
    if (is_numeral(arg, k)) {
        if (k.is_zero()) {
            // acos(0) = pi/2
            result = m_util.mk_mul(m_util.mk_numeral(rational(1, 2), false), m_util.mk_pi());
            return BR_REWRITE2;
        }
        if (k.is_one()) {
            // acos(1) = 0
            result = m_util.mk_numeral(rational(0), false);
            return BR_DONE;
        }
        if (k.is_minus_one()) {
            // acos(-1) = pi
            result = m_util.mk_pi();
            return BR_DONE;
        }
        if (k == rational(1, 2)) {
            // acos(1/2) = pi/3
            result = m_util.mk_mul(m_util.mk_numeral(rational(1, 3), false), m_util.mk_pi());
            return BR_REWRITE2;
        }
        if (k == rational(-1, 2)) {
            // acos(-1/2) = 2/3 pi
            result = m_util.mk_mul(m_util.mk_numeral(rational(2, 3), false), m_util.mk_pi());
            return BR_REWRITE2;
        }
    }
    return BR_FAILED;
}

br_status arith_rewriter::mk_atan_core(expr * arg, expr_ref & result) {
    rational k;
    if (is_numeral(arg, k)) {
        if (k.is_zero()) {
            result = arg;
            return BR_DONE;
        }

        if (k.is_one()) {
            // atan(1)  == pi/4
            result = m_util.mk_mul(m_util.mk_numeral(rational(1, 4), false), m_util.mk_pi());
            return BR_REWRITE2;
        }

        if (k.is_minus_one()) {
            // atan(-1) == -pi/4
            result = m_util.mk_mul(m_util.mk_numeral(rational(-1, 4), false), m_util.mk_pi());
            return BR_REWRITE2;
        }

        if (k < rational(-1)) {
            // atan(-2) == -tan(2)
            // atan(-3) == -tan(3)
            k.neg();
            result = m_util.mk_uminus(m_util.mk_atan(m_util.mk_numeral(k, false)));            
            return BR_REWRITE2;
        }
        return BR_FAILED;
    }

    expr * t;
    if (m_util.is_times_minus_one(arg, t)) {
        // atan(-x) ==> -atan(x)
        result = m_util.mk_uminus(m_util.mk_atan(t));
        return BR_REWRITE2;
    }
    return BR_FAILED;
}

br_status arith_rewriter::mk_sinh_core(expr * arg, expr_ref & result) {
    if (is_app_of(arg, get_fid(), OP_ASINH)) {
        // sinh(asinh(x)) == x
        result = to_app(arg)->get_arg(0);
        return BR_DONE;
    }
    expr * t;
    if (m_util.is_times_minus_one(arg, t)) {
        // sinh(-t) == -sinh(t)
        result = m_util.mk_uminus(m_util.mk_sinh(t));
        return BR_REWRITE2;
    }
    return BR_FAILED;
}

br_status arith_rewriter::mk_cosh_core(expr * arg, expr_ref & result) {
    if (is_app_of(arg, get_fid(), OP_ACOSH)) {
        // cosh(acosh(x)) == x
        result = to_app(arg)->get_arg(0);
        return BR_DONE;
    }
    expr * t;
    if (m_util.is_times_minus_one(arg, t)) {
        // cosh(-t) == cosh
        result = m_util.mk_cosh(t);
        return BR_DONE;
    }
    return BR_FAILED;
}

br_status arith_rewriter::mk_tanh_core(expr * arg, expr_ref & result) {
    if (is_app_of(arg, get_fid(), OP_ATANH)) {
        // tanh(atanh(x)) == x
        result = to_app(arg)->get_arg(0);
        return BR_DONE;
    }
    expr * t;
    if (m_util.is_times_minus_one(arg, t)) {
        // tanh(-t) == -tanh(t)
        result = m_util.mk_uminus(m_util.mk_tanh(t));
        return BR_REWRITE2;
    }
    return BR_FAILED;
}

template class poly_rewriter<arith_rewriter_core>;
