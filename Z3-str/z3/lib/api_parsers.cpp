/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    api_parsers.cpp

Abstract:
    API for parsing different formats

Author:

    Leonardo de Moura (leonardo) 2012-02-29.

Revision History:

--*/
#include<iostream>
#include"z3.h"
#include"api_log_macros.h"
#include"api_context.h"
#include"api_util.h"
#include"cmd_context.h"
#include"smt2parser.h"
#include"smtparser.h"
#include"z3_solver.h"

extern "C" {

    void init_smtlib_parser(Z3_context c, 
                            unsigned num_sorts,
                            Z3_symbol const sort_names[],
                            Z3_sort const types[],
                            unsigned num_decls,
                            Z3_symbol const decl_names[],
                            Z3_func_decl const decls[]) {
        mk_c(c)->reset_parser();
        mk_c(c)->m_smtlib_parser = smtlib::parser::create(mk_c(c)->m());
        mk_c(c)->m_smtlib_parser->initialize_smtlib();
        smtlib::symtable * table = mk_c(c)->m_smtlib_parser->get_benchmark()->get_symtable();
        for (unsigned i = 0; i < num_sorts; i++) {
            table->insert(to_symbol(sort_names[i]), to_sort(types[i]));
        }
        for (unsigned i = 0; i < num_decls; i++) {
            table->insert(to_symbol(decl_names[i]), to_func_decl(decls[i]));
        }
    }
    
    void Z3_API Z3_parse_smtlib_string(Z3_context c, 
                                       const char * str,
                                       unsigned  num_sorts,
                                       Z3_symbol const sort_names[],
                                       Z3_sort   const sorts[],
                                       unsigned  num_decls,
                                       Z3_symbol const decl_names[],
                                       Z3_func_decl const decls[]) {
        Z3_TRY;
        LOG_Z3_parse_smtlib_string(c, str, num_sorts, sort_names, sorts, num_decls, decl_names, decls);
        std::ostringstream outs;
        bool ok = false;

        RESET_ERROR_CODE();
        init_smtlib_parser(c, num_sorts, sort_names, sorts, num_decls, decl_names, decls);
        mk_c(c)->m_smtlib_parser->set_error_stream(outs);
        try {
            ok = mk_c(c)->m_smtlib_parser->parse_string(str);        
        }
        catch (...) {
            ok = false;
        }
        mk_c(c)->m_smtlib_error_buffer = outs.str();
        if (!ok) {
            mk_c(c)->reset_parser();
            SET_ERROR_CODE(Z3_PARSER_ERROR);
        }
        Z3_CATCH;
    }

    void Z3_API Z3_parse_smtlib_file(Z3_context c, 
                                     const char * file_name,
                                     unsigned num_sorts,
                                     Z3_symbol const sort_names[],
                                     Z3_sort const types[],
                                     unsigned num_decls,
                                     Z3_symbol const decl_names[],
                                     Z3_func_decl const decls[]) {
        Z3_TRY;
        LOG_Z3_parse_smtlib_file(c, file_name, num_sorts, sort_names, types, num_decls, decl_names, decls);
        bool ok = false;
        RESET_ERROR_CODE();
        std::ostringstream outs;
        init_smtlib_parser(c, num_sorts, sort_names, types, num_decls, decl_names, decls);
        mk_c(c)->m_smtlib_parser->set_error_stream(outs);
        try {
            ok = mk_c(c)->m_smtlib_parser->parse_file(file_name);
        }
        catch(...) {
            ok = false;
        }
        mk_c(c)->m_smtlib_error_buffer = outs.str();
        if (!ok) {
            mk_c(c)->reset_parser();
            SET_ERROR_CODE(Z3_PARSER_ERROR);
        }
        Z3_CATCH;
    }

    unsigned Z3_API Z3_get_smtlib_num_formulas(Z3_context c) {
        Z3_TRY;
        LOG_Z3_get_smtlib_num_formulas(c);
        RESET_ERROR_CODE();
        if (mk_c(c)->m_smtlib_parser) {
            return mk_c(c)->m_smtlib_parser->get_benchmark()->get_num_formulas();
        }
        SET_ERROR_CODE(Z3_NO_PARSER);
        return 0;
        Z3_CATCH_RETURN(0);
    }

    Z3_ast Z3_API Z3_get_smtlib_formula(Z3_context c, unsigned i) {
        Z3_TRY;
        LOG_Z3_get_smtlib_formula(c, i);
        RESET_ERROR_CODE();
        if (mk_c(c)->m_smtlib_parser) {
            if (i < mk_c(c)->m_smtlib_parser->get_benchmark()->get_num_formulas()) {
                ast * f = mk_c(c)->m_smtlib_parser->get_benchmark()->begin_formulas()[i];
                mk_c(c)->save_ast_trail(f);
                RETURN_Z3(of_ast(f));
            }
            else {
                SET_ERROR_CODE(Z3_IOB);
            }
        }
        else {
            SET_ERROR_CODE(Z3_NO_PARSER);
        }
        RETURN_Z3(0);
        Z3_CATCH_RETURN(0);
    }

    unsigned Z3_API Z3_get_smtlib_num_assumptions(Z3_context c) {
        Z3_TRY;
        LOG_Z3_get_smtlib_num_assumptions(c);
        RESET_ERROR_CODE();
        if (mk_c(c)->m_smtlib_parser) {
            return mk_c(c)->m_smtlib_parser->get_benchmark()->get_num_axioms();
        }
        SET_ERROR_CODE(Z3_NO_PARSER);
        return 0;
        Z3_CATCH_RETURN(0);
    }

    Z3_ast Z3_API Z3_get_smtlib_assumption(Z3_context c, unsigned i) {
        Z3_TRY;
        LOG_Z3_get_smtlib_assumption(c, i);
        RESET_ERROR_CODE();
        if (mk_c(c)->m_smtlib_parser) {
            if (i < mk_c(c)->m_smtlib_parser->get_benchmark()->get_num_axioms()) {
                ast * a = mk_c(c)->m_smtlib_parser->get_benchmark()->begin_axioms()[i];
                mk_c(c)->save_ast_trail(a);
                RETURN_Z3(of_ast(a));
            }
            else {
                SET_ERROR_CODE(Z3_IOB);
            }
        }
        else {
            SET_ERROR_CODE(Z3_NO_PARSER);
        }
        RETURN_Z3(0);
        Z3_CATCH_RETURN(0);
    }

    unsigned Z3_API Z3_get_smtlib_num_decls(Z3_context c) {
        Z3_TRY;
        LOG_Z3_get_smtlib_num_decls(c);
        RESET_ERROR_CODE();
        if (mk_c(c)->m_smtlib_parser) {
            mk_c(c)->extract_smtlib_parser_decls();
            return mk_c(c)->m_smtlib_parser_decls.size();
        }
        SET_ERROR_CODE(Z3_NO_PARSER);
        return 0;
        Z3_CATCH_RETURN(0);
    }

    Z3_func_decl Z3_API Z3_get_smtlib_decl(Z3_context c, unsigned i) {
        Z3_TRY;
        LOG_Z3_get_smtlib_decl(c, i);
        RESET_ERROR_CODE(); 
        mk_c(c)->extract_smtlib_parser_decls();
        if (mk_c(c)->m_smtlib_parser) {
            if (i < mk_c(c)->m_smtlib_parser_decls.size()) {
                func_decl * d = mk_c(c)->m_smtlib_parser_decls[i];
                mk_c(c)->save_ast_trail(d);
                RETURN_Z3(of_func_decl(d));
            }
            else {
                SET_ERROR_CODE(Z3_IOB);
            }
        }
        else {
            SET_ERROR_CODE(Z3_NO_PARSER);
        }
        RETURN_Z3(0);
        Z3_CATCH_RETURN(0);
    }

    unsigned Z3_API Z3_get_smtlib_num_sorts(Z3_context c) {
        Z3_TRY;
        LOG_Z3_get_smtlib_num_sorts(c);
        RESET_ERROR_CODE();
        if (mk_c(c)->m_smtlib_parser) {
            mk_c(c)->extract_smtlib_parser_decls();
            return mk_c(c)->m_smtlib_parser_sorts.size();
        }
        SET_ERROR_CODE(Z3_NO_PARSER);
        return 0;
        Z3_CATCH_RETURN(0);
    }

    Z3_sort Z3_API Z3_get_smtlib_sort(Z3_context c, unsigned i) {
        Z3_TRY;
        LOG_Z3_get_smtlib_sort(c, i);
        RESET_ERROR_CODE(); 
        if (mk_c(c)->m_smtlib_parser) {
            mk_c(c)->extract_smtlib_parser_decls();
            if (i < mk_c(c)->m_smtlib_parser_sorts.size()) {
                sort* s = mk_c(c)->m_smtlib_parser_sorts[i];
                mk_c(c)->save_ast_trail(s);
                RETURN_Z3(of_sort(s));
            }
            else {
                SET_ERROR_CODE(Z3_IOB);
            }
        }
        else {
            SET_ERROR_CODE(Z3_NO_PARSER);
        }
        RETURN_Z3(0);
        Z3_CATCH_RETURN(0);
    }

    Z3_string Z3_API Z3_get_smtlib_error(Z3_context c) {        
        Z3_TRY;
        LOG_Z3_get_smtlib_error(c);
        RESET_ERROR_CODE(); 
        return mk_c(c)->m_smtlib_error_buffer.c_str();
        Z3_CATCH_RETURN("");
    }
    
    Z3_ast parse_z3_stream(Z3_context c, std::istream& is) {
        z3_solver parser(c, is, verbose_stream(), mk_c(c)->fparams(), false);
        if (!parser.parse()) {
            SET_ERROR_CODE(Z3_PARSER_ERROR);
            return of_ast(mk_c(c)->m().mk_true());
        }
        expr_ref_vector assumptions(mk_c(c)->m());
        parser.get_assumptions(assumptions);
        return of_ast(mk_c(c)->mk_and(assumptions.size(), assumptions.c_ptr()));
    }

    Z3_ast Z3_API Z3_parse_z3_string(Z3_context c, Z3_string str) { 
        Z3_TRY;
        LOG_Z3_parse_z3_string(c, str);
        std::string s(str);
        std::istringstream is(s);
        Z3_ast r = parse_z3_stream(c, is);
        RETURN_Z3(r);
        Z3_CATCH_RETURN(0);
    }

    Z3_ast Z3_API Z3_parse_z3_file(Z3_context c, Z3_string file_name) {
        Z3_TRY;
        LOG_Z3_parse_z3_file(c, file_name);
        std::ifstream is(file_name);
        if (!is) {
            SET_ERROR_CODE(Z3_PARSER_ERROR);
            return 0;
        }
        Z3_ast r = parse_z3_stream(c, is);
        RETURN_Z3(r);
        Z3_CATCH_RETURN(0);
    }

    // ---------------
    // Support for SMTLIB2

    class z3_context_solver : public solver {
        api::context & m_ctx;
        smt::solver & ctx() const { return m_ctx.get_solver(); }
    public:
        virtual ~z3_context_solver() {}
        z3_context_solver(api::context& c) : m_ctx(c) {}
        virtual void init(ast_manager & m, symbol const & logic) {}
        virtual void collect_statistics(statistics & st) const {}
        virtual void reset() { ctx().reset(); }
        virtual void assert_expr(expr * t) { ctx().assert_expr(t); }
        virtual void push() { ctx().push(); }
        virtual void pop(unsigned n) { ctx().pop(n); }
        virtual unsigned get_scope_level() const { return ctx().get_scope_level(); }
        virtual lbool check_sat(unsigned num_assumptions, expr * const * assumptions) {
            return ctx().check(num_assumptions, assumptions);
        }
        virtual void get_unsat_core(ptr_vector<expr> & r) {
            unsigned sz = ctx().get_unsat_core_size();
            for (unsigned i = 0; i < sz; i++)
                r.push_back(ctx().get_unsat_core_expr(i));            
        }
        virtual void get_model(model_ref & m) { ctx().get_model(m); }
        virtual proof * get_proof() { return ctx().get_proof(); }
        virtual std::string reason_unknown() const { return ctx().last_failure_as_string(); }
        virtual void get_labels(svector<symbol> & r) {
            buffer<symbol> tmp;
            ctx().get_relevant_labels(0, tmp);
            r.append(tmp.size(), tmp.c_ptr());            
        }
        
        // These are controlled by the main API
        virtual void set_cancel(bool f) { }
        void cancel() { set_cancel(true); }
        void reset_cancel() { set_cancel(false); }        
        virtual void set_progress_callback(progress_callback * callback) {}        
    };

    Z3_ast parse_smtlib2_stream(bool exec, Z3_context c, std::istream& is,
                                unsigned num_sorts,
                                Z3_symbol const sort_names[],
                                Z3_sort const sorts[],
                                unsigned num_decls,
                                Z3_symbol const decl_names[],
                                Z3_func_decl const decls[]) {
        Z3_TRY;
        cmd_context ctx(mk_c(c)->fparams(), false, &(mk_c(c)->m()));
        ctx.set_ignore_check(true);
        if (exec) {
            ctx.set_solver(alloc(z3_context_solver, *mk_c(c)));
        }
        for (unsigned i = 0; i < num_decls; ++i) {
           ctx.insert(to_symbol(decl_names[i]), to_func_decl(decls[i]));
        }
        for (unsigned i = 0; i < num_sorts; ++i) {
            psort* ps = ctx.pm().mk_psort_cnst(to_sort(sorts[i]));
            ctx.insert(ctx.pm().mk_psort_user_decl(0, to_symbol(sort_names[i]), ps));
        }
        if (!parse_smt2_commands(ctx, is)) {
            SET_ERROR_CODE(Z3_PARSER_ERROR);
            return of_ast(mk_c(c)->m().mk_true());
        }
        ptr_vector<expr>::const_iterator it  = ctx.begin_assertions();
        ptr_vector<expr>::const_iterator end = ctx.end_assertions();
        unsigned size = static_cast<unsigned>(end - it);
        return of_ast(mk_c(c)->mk_and(size, it));
        Z3_CATCH_RETURN(0);
    }

    Z3_ast Z3_API Z3_parse_smtlib2_string(Z3_context c, Z3_string str,
                                          unsigned num_sorts,
                                          Z3_symbol const sort_names[],
                                          Z3_sort const sorts[],
                                          unsigned num_decls,
                                          Z3_symbol const decl_names[],
                                          Z3_func_decl const decls[]) {
        Z3_TRY;
        LOG_Z3_parse_smtlib2_string(c, str, num_sorts, sort_names, sorts, num_decls, decl_names, decls);
        std::string s(str);
        std::istringstream is(s);
        Z3_ast r = parse_smtlib2_stream(false, c, is, num_sorts, sort_names, sorts, num_decls, decl_names, decls);
        RETURN_Z3(r);
        Z3_CATCH_RETURN(0);
    }

    Z3_ast Z3_API Z3_parse_smtlib2_file(Z3_context c, Z3_string file_name,
                                        unsigned num_sorts,
                                        Z3_symbol const sort_names[],
                                        Z3_sort const sorts[],
                                        unsigned num_decls,
                                        Z3_symbol const decl_names[],
                                        Z3_func_decl const decls[]) {
        Z3_TRY;
        LOG_Z3_parse_smtlib2_string(c, file_name, num_sorts, sort_names, sorts, num_decls, decl_names, decls);
        std::ifstream is(file_name);
        if (!is) {
            SET_ERROR_CODE(Z3_PARSER_ERROR);
            return 0;
        }
        Z3_ast r = parse_smtlib2_stream(false, c, is, num_sorts, sort_names, sorts, num_decls, decl_names, decls);
        RETURN_Z3(r);
        Z3_CATCH_RETURN(0);
    }

    Z3_ast Z3_API Z3_exec_smtlib2_string(Z3_context c, Z3_string str,
                                         unsigned num_sorts,
                                         Z3_symbol sort_names[],
                                         Z3_sort sorts[],
                                         unsigned num_decls,
                                         Z3_symbol decl_names[],
                                         Z3_func_decl decls[]) { 
        Z3_TRY;
        cmd_context ctx(mk_c(c)->fparams(), false, &(mk_c(c)->m()));
        std::string s(str);
        std::istringstream is(s);
        // No logging for this one, since it private.
        return parse_smtlib2_stream(true, c, is, num_sorts, sort_names, sorts, num_decls, decl_names, decls);
        Z3_CATCH_RETURN(0);
    }

    Z3_ast Z3_API Z3_exec_smtlib2_file(Z3_context c, Z3_string file_name,
                                       unsigned num_sorts,
                                       Z3_symbol sort_names[],
                                       Z3_sort sorts[],
                                       unsigned num_decls,
                                       Z3_symbol decl_names[],
                                       Z3_func_decl decls[]) { 
        Z3_TRY;
        std::ifstream is(file_name);
        if (!is) {
            SET_ERROR_CODE(Z3_PARSER_ERROR);
            return 0;
        }
        // No logging for this one, since it private.
        return parse_smtlib2_stream(true, c, is, num_sorts, sort_names, sorts, num_decls, decl_names, decls);
        Z3_CATCH_RETURN(0);
    }

};
