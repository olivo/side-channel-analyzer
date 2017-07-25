/*++
Copyright (c) 2011 Microsoft Corporation

Module Name:

    sat_types.h

Abstract:

    Basic types used in the SAT solver

Author:

    Leonardo de Moura (leonardo) 2011-05-21.

Revision History:

--*/
#ifndef _SAT_TYPES_H_
#define _SAT_TYPES_H_

#include"debug.h"
#include"approx_set.h"
#include"lbool.h"
#include"tactic_exception.h"
#include"vector.h"
#include<iomanip>

namespace sat {
#define SAT_VB_LVL 10

    // TODO: there is some duplication in the sat and smt namespaces.
    // The sat namespace should be the base.
    // I should cleanup the smt namespace later.

    /**
       \brief A boolean variable is just an integer.
    */
    typedef unsigned bool_var;

    typedef svector<bool_var> bool_var_vector;
    
    const bool_var null_bool_var  = UINT_MAX >> 1;
    
    /**
       \brief The literal b is represented by the value 2*b, and
       the literal (not b) by the value 2*b + 1
    */
    class literal {
        unsigned  m_val;
        explicit literal(unsigned v):m_val(v) {}
    public:
        literal():m_val(null_bool_var << 1) {
            SASSERT(var() == null_bool_var && !sign());
        }
        
        literal(bool_var v, bool _sign):
            m_val((v << 1) + static_cast<unsigned>(_sign)) {
            SASSERT(var() == v);
            SASSERT(sign() == _sign);
        }
        
        bool_var var() const { 
            return m_val >> 1; 
        }
        
        bool sign() const {
            return m_val & 1; 
        }

        literal unsign() const {
            return literal(m_val & ~1);
        }
         
        unsigned index() const {
            return m_val;
        }
        
        void neg() {
            m_val = m_val ^ 1;
        }
        
        friend literal operator~(literal l) {
            return literal(l.m_val ^ 1);
        }

        unsigned to_uint() const { return m_val; }

        unsigned hash() const { return to_uint(); }

        friend literal to_literal(unsigned x);
        friend bool operator<(literal const & l1, literal const & l2);
        friend bool operator==(literal const & l1, literal const & l2);
        friend bool operator!=(literal const & l1, literal const & l2);
    };

    const literal null_literal;

    inline literal to_literal(unsigned x) { return literal(x); }
    inline bool operator<(literal const & l1, literal const & l2) { return l1.m_val < l2.m_val;  }
    inline bool operator==(literal const & l1, literal const & l2) { return l1.m_val == l2.m_val; }
    inline bool operator!=(literal const & l1, literal const & l2) { return l1.m_val != l2.m_val; }

    inline std::ostream & operator<<(std::ostream & out, literal l) { out << (l.sign() ? "-" : "") << l.var(); return out; }

    typedef svector<literal> literal_vector;

    typedef unsigned clause_offset;
    typedef unsigned ext_constraint_idx;
    typedef unsigned ext_justification_idx;

    struct literal2unsigned { unsigned operator()(literal l) const { return l.to_uint(); } };

    typedef approx_set_tpl<literal, literal2unsigned, unsigned> literal_approx_set;

    typedef approx_set_tpl<bool_var, u2u, unsigned> var_approx_set;
   
    enum phase {
        POS_PHASE, NEG_PHASE, PHASE_NOT_AVAILABLE
    };

    class solver;
    class clause;
    class clause_wrapper;
    class integrity_checker;
    typedef ptr_vector<clause> clause_vector;

    class solver_exception : public tactic_exception {
    public:                                                
        solver_exception(char const * msg):tactic_exception(msg) {}
    };

    typedef default_exception sat_param_exception;

    typedef svector<lbool> model;

    inline lbool value_at(bool_var v, model const & m) { return m[v]; }
    inline lbool value_at(literal l, model const & m) { lbool r = value_at(l.var(), m); return l.sign() ? ~r : r; }
    
    inline std::ostream & operator<<(std::ostream & out, model const & m) {
        bool first = true;
        for (bool_var v = 0; v < m.size(); v++) {
            if (m[v] == l_undef) continue;
            if (first) first = false; else out << " ";
            if (m[v] == l_true) out << v; else out << "-" << v;
        }
        return out;
    }

    class uint_set {
        svector<char>        m_in_set;
        svector<unsigned>    m_set;
    public:
        typedef svector<unsigned>::const_iterator iterator;
        void insert(unsigned v) { 
            m_in_set.reserve(v+1, false);
            if (m_in_set[v]) 
                return; 
            m_in_set[v] = true; 
            m_set.push_back(v); 
        }
        
        bool contains(unsigned v) const { 
            return v < m_in_set.size() && m_in_set[v] != 0; 
        }
        
        bool empty() const { 
            return m_set.empty(); 
        }

        // erase some variable from the set
        unsigned erase() { 
            SASSERT(!empty()); 
            unsigned v = m_set.back(); 
            m_set.pop_back(); 
            m_in_set[v] = false; 
            return v; 
        }
        iterator begin() const { return m_set.begin(); }
        iterator end() const { return m_set.end(); }
        void reset() { m_set.reset(); m_in_set.reset(); }
        void cleanup() { m_set.finalize(); m_in_set.finalize(); }
    };

    typedef uint_set bool_var_set;

    class literal_set {
        uint_set m_set;
    public:
        void insert(literal l) { m_set.insert(l.index()); }
        bool contains(literal l) const { return m_set.contains(l.index()); }
        bool empty() const { return m_set.empty(); }
        void reset() { m_set.reset(); }
        void cleanup() { m_set.cleanup(); }
    };
    
    struct mem_stat {
    };
    
    inline std::ostream & operator<<(std::ostream & out, mem_stat const & m) {
        double mem = static_cast<double>(memory::get_allocation_size())/static_cast<double>(1024*1024);
        out << " :memory " << std::fixed << std::setprecision(2) << mem;
        return out;
    }

    struct dimacs_lit {
        literal m_lit;
        dimacs_lit(literal l):m_lit(l) {}
    };

    inline std::ostream & operator<<(std::ostream & out, dimacs_lit const & dl) {
        literal l = dl.m_lit;
        if (l.sign()) out << "-" << (l.var() + 1);
        else out << (l.var() + 1);
        return out;
    }

    struct mk_lits_pp {
        unsigned        m_num;
        literal const * m_lits;
        mk_lits_pp(unsigned num, literal const * ls):m_num(num), m_lits(ls) {}
    };

    inline std::ostream & operator<<(std::ostream & out, mk_lits_pp const & ls) {
        for (unsigned i = 0; i < ls.m_num; i++) {
            if (i > 0) out << " ";
            out << ls.m_lits[i];
        }
        return out;
    }

    inline std::ostream & operator<<(std::ostream & out, literal_vector const & ls) {
        return out << mk_lits_pp(ls.size(), ls.c_ptr());
    }
};

#endif
