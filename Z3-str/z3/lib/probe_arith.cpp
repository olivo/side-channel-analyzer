/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    probe_arith.cpp

Abstract:

    Some probes for arithmetic problems.

Author:

    Leonardo de Moura (leonardo) 2012-03-01.

Revision History:

--*/
#include"probe.h"
#include"expr2polynomial.h"
#include"for_each_expr.h"
#include"arith_decl_plugin.h"
#include"goal_util.h"

class arith_degree_probe : public probe {
    struct proc {
        ast_manager &            m;
        unsynch_mpq_manager      m_qm;
        polynomial::manager      m_pm;
        default_expr2polynomial  m_expr2poly;
        arith_util               m_util;
        unsigned                 m_max_degree;
        unsigned long long       m_acc_degree;
        unsigned                 m_counter;

        proc(ast_manager & _m):m(_m), m_pm(m_qm), m_expr2poly(m, m_pm), m_util(m) {
            m_max_degree = 0;
            m_acc_degree = 0;
            m_counter    = 0;
        }

        void updt_degree(polynomial_ref const & p) {
            unsigned deg = m_pm.total_degree(p);
            if (deg > m_max_degree)
                m_max_degree = deg;
            m_acc_degree += deg;
            m_counter++;
        }
        
        void process(app * n) {
            expr * lhs = n->get_arg(0);
            expr * rhs = n->get_arg(1);
            polynomial_ref p1(m_pm);
            polynomial_ref p2(m_pm);
            scoped_mpz d1(m_qm);
            scoped_mpz d2(m_qm);
            m_expr2poly.to_polynomial(lhs, p1, d1);
            m_expr2poly.to_polynomial(rhs, p2, d2);
            updt_degree(p1);
            updt_degree(p2);
        }

        void operator()(var * n) {}
        void operator()(quantifier * n) {}
        void operator()(app * n) {
            if (m_util.is_le(n) || m_util.is_lt(n) || m_util.is_gt(n) || m_util.is_ge(n))
                process(n);
            if (m.is_eq(n) && m_util.is_int_real(n->get_arg(0)))
                process(n);
        }
    };

    bool m_avg;
public:
    arith_degree_probe(bool avg):m_avg(avg) {}

    virtual result operator()(goal const & g) {
        proc p(g.m());
        for_each_expr_at(p, g);
        if (m_avg)
            return p.m_counter == 0 ? 0.0 : static_cast<double>(p.m_acc_degree)/static_cast<double>(p.m_counter);
        else
            return p.m_max_degree;
    }
};

class arith_bw_probe : public probe {
    struct proc {
        ast_manager &            m;
        arith_util               m_util;
        unsigned                 m_max_bw;
        unsigned long long       m_acc_bw;
        unsigned                 m_counter;

        proc(ast_manager & _m):m(_m), m_util(m) {
            m_max_bw  = 0;
            m_acc_bw  = 0;
            m_counter = 0;
        }
        
        void operator()(var * n) {}
        void operator()(quantifier * n) {}
        void operator()(app * n) {
            rational val;
            if (m_util.is_numeral(n, val)) {
                unsigned bw = val.bitsize();
                if (bw > m_max_bw)
                    m_max_bw = bw;
                m_acc_bw += bw;
                m_counter++;
            }
        }

    };

    bool m_avg;
public:
    arith_bw_probe(bool avg):m_avg(avg) {}
        
    virtual result operator()(goal const & g) {
        proc p(g.m());
        for_each_expr_at(p, g);
        if (m_avg)
            return p.m_counter == 0 ? 0.0 : static_cast<double>(p.m_acc_bw)/static_cast<double>(p.m_counter);
        else
            return p.m_max_bw;
    }
};

probe * mk_arith_avg_degree_probe() {
    return alloc(arith_degree_probe, true);
}

probe * mk_arith_max_degree_probe() {
    return alloc(arith_degree_probe, false);
}

probe * mk_arith_avg_bw_probe() {
    return alloc(arith_bw_probe, true);
}

probe * mk_arith_max_bw_probe() {
    return alloc(arith_bw_probe, false);
}

struct is_non_qflira_functor {
    struct found {};
    ast_manager & m;
    arith_util    u;
    bool          m_int;
    bool          m_real;

    is_non_qflira_functor(ast_manager & _m, bool _int, bool _real):m(_m), u(m), m_int(_int), m_real(_real) {}

    void operator()(var *) { throw found();  }
    
    void operator()(quantifier *) { throw found(); }
    
    bool compatible_sort(app * n) const {
        if (m.is_bool(n))
            return true;
        if (m_int && u.is_int(n))
            return true;
        if (m_real && u.is_real(n))
            return true;
        return false;
    }

    void operator()(app * n) {
        if (!compatible_sort(n))
            throw found();
        family_id fid = n->get_family_id();
        if (fid == m.get_basic_family_id())
            return; 
        if (fid == u.get_family_id()) {
            switch (n->get_decl_kind()) {
            case OP_LE:  case OP_GE: case OP_LT: case OP_GT:
            case OP_ADD: case OP_NUM:
                return;
            case OP_MUL:
                if (n->get_num_args() != 2)
                    throw found();
                if (!u.is_numeral(n->get_arg(0)))
                    throw found();
                return;
            case OP_TO_REAL:
                if (!m_real)
                    throw found();
                break;
            default:
                throw found();
            }
            return;
        }
        if (is_uninterp_const(n))
            return;
        throw found();
    }
};

static bool is_qflia(goal const & g) {
    is_non_qflira_functor p(g.m(), true, false);
    return !test(g, p);
}

class is_qflia_probe : public probe {
public:
    virtual result operator()(goal const & g) {
        return is_qflia(g);
    }
};

static bool is_qflra(goal const & g) {
    is_non_qflira_functor p(g.m(), false, true);
    return !test(g, p);
}

class is_qflra_probe : public probe {
public:
    virtual result operator()(goal const & g) {
        return is_qflra(g);
    }
};

static bool is_qflira(goal const & g) {
    is_non_qflira_functor p(g.m(), true, true);
    return !test(g, p);
}

class is_qflira_probe : public probe {
public:
    virtual result operator()(goal const & g) {
        return is_qflira(g);
    }
};

static bool is_lp(goal const & g) {
    ast_manager & m = g.m();
    arith_util u(m);
    unsigned sz = g.size();
    for (unsigned i = 0; i < sz; i++) {
        expr * f  = g.form(i);
        bool sign = false;
        while (m.is_not(f, f))
            sign = !sign;
        if (m.is_eq(f) && !sign) {
            if (m.get_sort(to_app(f)->get_arg(0))->get_family_id() != u.get_family_id())
                return false;
            continue;
        }
        if (u.is_le(f) || u.is_ge(f) || u.is_lt(f) || u.is_gt(f))
            continue;
        return false;
    }
    return true;
}

static bool is_ilp(goal const & g) {
    if (!is_qflia(g))
        return false;
    if (has_term_ite(g))
        return false;
    return is_lp(g);
}

static bool is_mip(goal const & g) {
    if (!is_qflira(g))
        return false;
    if (has_term_ite(g))
        return false;
    return is_lp(g);
}

class is_ilp_probe : public probe {
public:
    virtual result operator()(goal const & g) {
        return is_ilp(g);
    }
};

class is_mip_probe : public probe {
public:
    virtual result operator()(goal const & g) {
        return is_mip(g);
    }
};

probe * mk_is_qflia_probe() {
    return alloc(is_qflia_probe);
}

probe * mk_is_qflra_probe() {
    return alloc(is_qflra_probe);
}

probe * mk_is_qflira_probe() {
    return alloc(is_qflira_probe);
}

probe * mk_is_ilp_probe() {
    return alloc(is_ilp_probe);
}

probe * mk_is_mip_probe() {
    return alloc(is_mip_probe);
}


struct is_non_nira_functor {
    struct found {};
    ast_manager & m;
    arith_util    u;
    bool          m_int;
    bool          m_real;
    bool          m_quant;

    is_non_nira_functor(ast_manager & _m, bool _int, bool _real, bool _quant):m(_m), u(m), m_int(_int), m_real(_real), m_quant(_quant) {}

    void operator()(var * x) {
        if (!m_quant)
            throw found();
        sort * s = x->get_sort();
        if (m_int && u.is_int(s))
            return;
        if (m_real && u.is_real(s))
            return;
        throw found();
    }
    
    void operator()(quantifier *) { 
        if (!m_quant)
            throw found(); 
    }
    
    bool compatible_sort(app * n) const {
        if (m.is_bool(n))
            return true;
        if (m_int && u.is_int(n))
            return true;
        if (m_real && u.is_real(n))
            return true;
        return false;
    }

    void operator()(app * n) {
        if (!compatible_sort(n))
            throw found();
        family_id fid = n->get_family_id();
        if (fid == m.get_basic_family_id())
            return; 
        if (fid == u.get_family_id())
            return;
        if (is_uninterp_const(n))
            return;
        throw found();
    }
};

static bool is_qfnia(goal const & g) {
    is_non_nira_functor p(g.m(), true, false, false);
    return !test(g, p);
}

static bool is_qfnra(goal const & g) {
    is_non_nira_functor p(g.m(), false, true, false);
    return !test(g, p);
}

static bool is_nia(goal const & g) {
    is_non_nira_functor p(g.m(), true, false, true);
    return !test(g, p);
}

static bool is_nra(goal const & g) {
    is_non_nira_functor p(g.m(), false, true, true);
    return !test(g, p);
}

class is_qfnia_probe : public probe {
public:
    virtual result operator()(goal const & g) {
        return is_qfnia(g);
    }
};

class is_qfnra_probe : public probe {
public:
    virtual result operator()(goal const & g) {
        return is_qfnra(g);
    }
};

class is_nia_probe : public probe {
public:
    virtual result operator()(goal const & g) {
        return is_nia(g);
    }
};

class is_nra_probe : public probe {
public:
    virtual result operator()(goal const & g) {
        return is_nra(g);
    }
};

probe * mk_is_qfnia_probe() {
    return alloc(is_qfnia_probe);
}

probe * mk_is_qfnra_probe() {
    return alloc(is_qfnra_probe);
}

probe * mk_is_nia_probe() {
    return alloc(is_nia_probe);
}

probe * mk_is_nra_probe() {
    return alloc(is_nra_probe);
}
