/*++
Copyright (c) 2006 Microsoft Corporation

Module Name:

    array_decl_plugin.h

Abstract:

    <abstract>

Author:

    Leonardo de Moura (leonardo) 2008-01-09.

Revision History:

--*/
#ifndef _ARRAY_DECL_PLUGIN_H_
#define _ARRAY_DECL_PLUGIN_H_

#include"ast.h"


inline sort* get_array_range(sort const * s) { 
    return to_sort(s->get_parameter(s->get_num_parameters() - 1).get_ast());
}

inline unsigned get_array_arity(sort const * s) {
    return s->get_num_parameters() -1;
}

inline sort* get_array_domain(sort const * s, unsigned idx) {
    return to_sort(s->get_parameter(idx).get_ast());
}

enum array_sort_kind {
    ARRAY_SORT
};

enum array_op_kind {
    OP_STORE,
    OP_SELECT,
    OP_CONST_ARRAY,
    OP_ARRAY_EXT_SKOLEM,
    OP_ARRAY_DEFAULT,
    OP_ARRAY_MAP,
    OP_SET_UNION,
    OP_SET_INTERSECT,
    OP_SET_DIFFERENCE,
    OP_SET_COMPLEMENT,
    OP_SET_SUBSET,
    OP_AS_ARRAY, // used for model construction
    LAST_ARRAY_OP
};

class array_decl_plugin : public decl_plugin {
    symbol m_store_sym;
    symbol m_select_sym;
    symbol m_const_sym;
    symbol m_default_sym;
    symbol m_map_sym;
    symbol m_set_union_sym;
    symbol m_set_intersect_sym;
    symbol m_set_difference_sym;
    symbol m_set_complement_sym;
    symbol m_set_subset_sym;
    symbol m_array_ext_sym;
    symbol m_as_array_sym;

    bool check_set_arguments(unsigned arity, sort * const * domain);

    func_decl * mk_const(sort* ty, unsigned arity, sort * const * domain);

    func_decl * mk_map(func_decl* f, unsigned arity, sort* const* domain);

    func_decl * mk_default(unsigned arity, sort* const* domain);

    func_decl * mk_select(unsigned arity, sort * const * domain);

    func_decl * mk_store(unsigned arity, sort * const * domain);

    func_decl * mk_array_ext_skolem(unsigned arity, sort * const * domain, unsigned i);

    func_decl * mk_set_union(unsigned arity, sort * const * domain);

    func_decl * mk_set_intersect(unsigned arity, sort * const * domain);

    func_decl * mk_set_difference(unsigned arity, sort * const * domain);

    func_decl * mk_set_complement(unsigned arity, sort * const * domain);

    func_decl * mk_set_subset(unsigned arity, sort * const * domain);

    func_decl * mk_as_array(func_decl * f);

    bool is_array_sort(sort* s) const;
 public:
    array_decl_plugin();
    virtual ~array_decl_plugin() {}

    virtual decl_plugin * mk_fresh() {
        return alloc(array_decl_plugin);
    }

    //
    // Contract for sort: 
    //   parameters[0]     - 1st dimension 
    //   ...
    //   parameters[n-1]   - nth dimension
    //   parameters[n]     - range
    //
    virtual sort * mk_sort(decl_kind k, unsigned num_parameters, parameter const * parameters);
    
    //
    // Contract for func_decl:
    //   parameters[0]     - array sort
    // Contract for others:
    //   no parameters
    virtual func_decl * mk_func_decl(decl_kind k, unsigned num_parameters, parameter const * parameters, 
                                     unsigned arity, sort * const * domain, sort * range);

    virtual void get_op_names(svector<builtin_name> & op_names, symbol const & logic);

    virtual void get_sort_names(svector<builtin_name> & sort_names, symbol const & logic);

    virtual expr * get_some_value(sort * s);

    virtual bool is_fully_interp(sort const * s) const;
};

class array_util {
    ast_manager & m_manager;
    family_id    m_fid;
public:
    array_util(ast_manager& m): m_manager(m), m_fid(m.get_family_id("array")) {}
    ast_manager & get_manager() const { return m_manager; }
    family_id get_family_id() const { return m_fid; }
    bool is_array(sort* s) const { return is_sort_of(s, m_fid, ARRAY_SORT);}
    bool is_array(expr* n) const { return is_array(m_manager.get_sort(n)); }
    bool is_select(expr* n) const { return is_app_of(n, m_fid, OP_SELECT); }
    bool is_store(expr* n) const { return is_app_of(n, m_fid, OP_STORE); }
    bool is_const(expr* n) const { return is_app_of(n, m_fid, OP_CONST_ARRAY); }
    bool is_map(expr* n) const { return is_app_of(n, m_fid, OP_ARRAY_MAP); }
    bool is_as_array(expr * n) const { return is_app_of(n, m_fid, OP_AS_ARRAY); }
    bool is_as_array_tree(expr * n);
    func_decl * get_as_array_func_decl(app * n) const { SASSERT(is_as_array(n)); return to_func_decl(n->get_decl()->get_parameter(0).get_ast()); }
    app * mk_map(func_decl * f, unsigned num_args, expr * const * args) {
        parameter p(f);
        return m_manager.mk_app(m_fid, OP_ARRAY_MAP, 1, &p, num_args, args);
    }
    app * mk_const_array(sort * s, expr * v) {
        parameter param(s);
        return m_manager.mk_app(m_fid, OP_CONST_ARRAY, 1, &param, 1, &v);
    }
    app * mk_empty_set(sort * s) {
        return mk_const_array(s, m_manager.mk_false());
    }
    app * mk_full_set(sort * s) {
        return mk_const_array(s, m_manager.mk_true());
    }

    sort * mk_array_sort(sort* dom, sort* range) { return mk_array_sort(1, &dom, range); }

    sort * mk_array_sort(unsigned arity, sort* const* domain, sort* range);
};


#endif /* _ARRAY_DECL_PLUGIN_H_ */

