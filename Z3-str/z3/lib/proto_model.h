/*++
Copyright (c) 2006 Microsoft Corporation

Module Name:

    proto_model.h

Abstract:

    This is the old model object.
    smt::context used it during model construction and for
    reporting the model for external consumers. 
    The whole value_factory "business" was due to model construction
    and unnecessary for external consumers.
    Future solvers will not use value_factory objects for 
    helping during model construction. 
    
    After smt::context finishes building the model, it is converted
    into a new (light) model object.

Author:

    Leonardo de Moura (leonardo) 2007-03-08.

Revision History:

--*/
#ifndef _PROTO_MODEL_H_
#define _PROTO_MODEL_H_

#include"model_core.h"
#include"model_params.h"
#include"value_factory.h"
#include"plugin_manager.h"
#include"simplifier.h"
#include"arith_decl_plugin.h"
#include"func_decl_dependencies.h"
#include"model.h"

class proto_model : public model_core {
    model_params const &          m_params; 
    ast_ref_vector                m_asts;
    plugin_manager<value_factory> m_factories;
    user_sort_factory *           m_user_sort_factory;
    simplifier &                  m_simplifier;
    family_id                     m_afid;        //!< array family id: hack for displaying models in V1.x style
    func_decl_set                 m_aux_decls;
    ptr_vector<expr>              m_tmp;

    void reset_finterp();

    expr * mk_some_interp_for(func_decl * d);

    // Invariant: m_decls subset m_func_decls union m_const_decls union union m_value_decls
    // Invariant: m_func_decls  subset m_decls
    // Invariant: m_const_decls subset m_decls
    
    void remove_aux_decls_not_in_set(ptr_vector<func_decl> & decls, func_decl_set const & s);
    void cleanup_func_interp(func_interp * fi, func_decl_set & found_aux_fs);


public:
    proto_model(ast_manager & m, simplifier & s, model_params const & p);
    virtual ~proto_model(); 

    model_params const & get_model_params() const { return m_params; }

    void register_factory(value_factory * f) { m_factories.register_plugin(f); }

    bool eval(expr * e, expr_ref & result, bool model_completion = false);

    bool is_array_value(expr * v) const;
    
    value_factory * get_factory(family_id fid);

    expr * get_some_value(sort * s);

    bool get_some_values(sort * s, expr_ref & v1, expr_ref & v2);

    expr * get_fresh_value(sort * s);

    void register_value(expr * n);

    //
    // Primitives for building models
    //
    void register_decl(func_decl * d, expr * v);
    void register_decl(func_decl * f, func_interp * fi, bool aux = false);
    void reregister_decl(func_decl * f, func_interp * new_fi, func_decl * f_aux);
    void compress();
    void cleanup();

    //
    // Primitives for building finite interpretations for uninterpreted sorts,
    // and retrieving the known universe.
    //
    void freeze_universe(sort * s);
    bool is_finite(sort * s) const;
    obj_hashtable<expr> const & get_known_universe(sort * s) const;
    virtual ptr_vector<expr> const & get_universe(sort * s) const;
    virtual unsigned get_num_uninterpreted_sorts() const;
    virtual sort * get_uninterpreted_sort(unsigned idx) const;

    //
    // Complete partial function interps
    //
    void complete_partial_func(func_decl * f);
    void complete_partial_funcs();

    //
    // Create final model object. 
    // proto_model is corrupted after that.
    model * mk_model();
};

typedef ref<proto_model> proto_model_ref;

#endif /* _PROTO_MODEL_H_ */

