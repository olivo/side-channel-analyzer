/*++
Copyright (c) 2011 Microsoft Corporation

Module Name:

    pdr_dl.cpp

Abstract:

    SMT2 interface for the datalog PDR

Author:

    Krystof Hoder (t-khoder) 2011-9-22.

Revision History:

--*/

#include "dl_cmds.h"
#include "dl_context.h"
#include "dl_mk_coi_filter.h"
#include "dl_mk_interp_tail_simplifier.h"
#include "dl_mk_subsumption_checker.h"
#include "dl_mk_rule_inliner.h"
#include "dl_rule.h"
#include "dl_rule_transformer.h"
#include "smt2parser.h"
#include "pdr_context.h"
#include "pdr_dl_interface.h"
#include "dl_rule_set.h"
#include "dl_mk_slice.h"

using namespace pdr;

dl_interface::dl_interface(datalog::context& ctx) : 
    m_ctx(ctx), 
    m_pdr_rules(ctx), 
    m_old_rules(ctx),
    m_context(0) {
    m_context = alloc(pdr::context, ctx.get_fparams(), ctx.get_params(), ctx.get_manager());
}


dl_interface::~dl_interface() {
    dealloc(m_context);
}


//
// Check if the new rules are weaker so that we can 
// re-use existing context.
// 
void dl_interface::check_reset() {
    datalog::rule_ref_vector const& new_rules = m_ctx.get_rules().get_rules();
    datalog::rule_ref_vector const& old_rules = m_old_rules.get_rules();    
    for (unsigned i = 0; i < new_rules.size(); ++i) {
        bool found = false;
        for (unsigned j = 0; !found && j < old_rules.size(); ++j) {
            if (m_ctx.check_subsumes(*old_rules[j], *new_rules[i])) {
                found = true;
            }
        }
        if (!found) {
            CTRACE("pdr", (old_rules.size() > 0), new_rules[i]->display(m_ctx, tout << "Fresh rule "););
            m_context->reset();
            break;
        }
    }
    m_old_rules.reset();
    m_old_rules.add_rules(new_rules.size(), new_rules.c_ptr());
}


lbool dl_interface::query(expr * query) {
    //we restore the initial state in the datalog context
    m_ctx.ensure_opened();
    m_pdr_rules.reset();
    m_ctx.get_rmanager().reset_relations();
    ast_manager& m =                      m_ctx.get_manager();
    datalog::relation_manager& rm =       m_ctx.get_rmanager();
    datalog::rule_manager& rule_manager = m_ctx.get_rule_manager();
    datalog::rule_set        old_rules(m_ctx.get_rules());
    func_decl_ref            query_pred(m);
    datalog::rule_ref_vector query_rules(rule_manager);
    datalog::rule_ref        query_rule(rule_manager);
    rule_manager.mk_query(query, query_pred, query_rules, query_rule);
    m_ctx.add_rules(query_rules);
    expr_ref bg_assertion = m_ctx.get_background_assertion();

    check_reset();
   
    TRACE("pdr",
          if (!m.is_true(bg_assertion)) {
              tout << "axioms:\n";
              tout << mk_pp(bg_assertion, m) << "\n";
          }
          tout << "query: " << mk_pp(query, m) << "\n";
          tout << "rules:\n";
          m_ctx.display_rules(tout);
          );

    model_converter_ref mc = datalog::mk_skip_model_converter();
    proof_converter_ref pc;
    if (m_ctx.get_params().get_bool(":generate-proof-trace", false)) {
        pc = datalog::mk_skip_proof_converter();
    }
    m_ctx.set_output_predicate(query_pred);
    m_ctx.apply_default_transformation(mc, pc);

    if (m_ctx.get_params().get_bool(":slice", true)) {
        datalog::rule_transformer transformer(m_ctx);
        datalog::mk_slice* slice = alloc(datalog::mk_slice, m_ctx);
        transformer.register_plugin(slice);
        m_ctx.transform_rules(transformer, mc, pc);        
        query_pred = slice->get_predicate(query_pred.get());
        m_ctx.set_output_predicate(query_pred);
    }

    IF_VERBOSE(2, m_ctx.display_rules(verbose_stream()););
    m_pdr_rules.add_rules(m_ctx.get_rules());
    m_pdr_rules.close();
    m_ctx.reopen();
    m_ctx.replace_rules(old_rules);

    if (m_pdr_rules.get_rules().empty()) {
        return l_false;
    }

    m_context->set_proof_converter(pc);
    m_context->set_model_converter(mc);
    m_context->set_query(query_pred);
    m_context->set_axioms(bg_assertion);
    m_context->update_rules(m_pdr_rules);
    
        
    while (true) {
        try {
            return m_context->solve();
        }
        catch (pdr::qi& q) {
            m_context->refine(q, m_pdr_rules);
        }
    }
}

expr_ref dl_interface::get_cover_delta(int level, func_decl* pred) {
    return m_context->get_cover_delta(level, pred);
}

void dl_interface::add_cover(int level, func_decl* pred, expr* property) {
    m_context->add_cover(level, pred, property);
}

unsigned dl_interface::get_num_levels(func_decl* pred) {
    return m_context->get_num_levels(pred);
}

void dl_interface::collect_statistics(statistics& st) const {
    m_context->collect_statistics(st);
}

void dl_interface::display_certificate(std::ostream& out) const {
    m_context->display_certificate(out);
}

expr_ref dl_interface::get_answer() {
    return m_context->get_answer();
}

void dl_interface::cancel() {
    m_context->cancel();
}

void dl_interface::cleanup() {
    m_context->cleanup();
}

void dl_interface::updt_params() {
    dealloc(m_context);
    m_context = alloc(pdr::context, m_ctx.get_fparams(), m_ctx.get_params(), m_ctx.get_manager());
}

void dl_interface::collect_params(param_descrs& p) {
    p.insert(":bfs-model-search", CPK_BOOL, "PDR: (default true) use BFS strategy for expanding model search");
    p.insert(":use-farkas", CPK_BOOL, "PDR: (default true) use lemma generator based on Farkas (for linear real arithmetic)");
    PRIVATE_PARAMS(p.insert(":use-farkas-model", CPK_BOOL, "PDR: (default false) enable using Farkas generalization through model propagation"););
    PRIVATE_PARAMS(p.insert(":use-precondition-generalizer", CPK_BOOL, "PDR: (default false) enable generalizations from weakest pre-conditions"););
    PRIVATE_PARAMS(p.insert(":use-multicore-generalizer", CPK_BOOL, 
                            "PDR: (default false) extract multiple cores for blocking states"););
    PRIVATE_PARAMS(p.insert(":use-farkas-properties", CPK_BOOL, 
                            "PDR: (default false) experimental Farkas lemma generation method"););
    PRIVATE_PARAMS(p.insert(":use-inductive-generalizer", CPK_BOOL, 
                            "PDR: (default true) generalize lemmas using induction strengthening"););
    PRIVATE_PARAMS(p.insert(":use-interpolants", CPK_BOOL, 
                            "PDR: (default false) use iZ3 interpolation for lemma generation"););
    PRIVATE_PARAMS(p.insert(":dump-interpolants", CPK_BOOL, 
                            "PDR: (default false) display interpolants"););
    p.insert(":cache-mode", CPK_UINT, "PDR: use no (0 - default) symbolic (1) or explicit cache (2) for model search");
    PRIVATE_PARAMS(p.insert(":inductive-reachability-check", CPK_BOOL, 
                            "PDR: (default false) assume negation of the cube on the previous level when "
                            "checking for reachability (not only during cube weakening)"););
    PRIVATE_PARAMS(p.insert(":max-num-contexts", CPK_UINT,
                            "PDR: (default 500) maximal number of contexts to create"););
    PRIVATE_PARAMS(p.insert(":try-minimize-core", CPK_BOOL,
                            "PDR: (default false) try to reduce core size (before inductive minimization)");); 
    PRIVATE_PARAMS(p.insert(":simplify-formulas-pre", CPK_BOOL,
                            "PDR: (default false) simplify derived formulas before inductive propagation"););
    PRIVATE_PARAMS(p.insert(":simplify-formulas-post", CPK_BOOL,
                            "PDR: (default false) simplify derived formulas after inductive propagation"););
    PRIVATE_PARAMS(p.insert(":slice", CPK_BOOL,
                            "PDR: (default true) simplify clause set using slicing"););
    p.insert(":generate-proof-trace", CPK_BOOL,
             "PDR: (default false) trace for 'sat' answer as proof object");


}
