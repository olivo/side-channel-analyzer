/*++
Copyright (c) 2006 Microsoft Corporation

Module Name:

    dl_compiler.cpp

Abstract:

    <abstract>

Author:

    Krystof Hoder (t-khoder) 2010-09-14.

Revision History:

--*/


#include <sstream>
#include"ref_vector.h"
#include"dl_context.h"
#include"dl_rule.h"
#include"dl_util.h"
#include"dl_compiler.h"
#include"ast_pp.h"
#include"ast_smt2_pp.h"


namespace datalog {

    void compiler::reset() {
        m_pred_regs.reset();
        m_new_reg = 0;
    }

    void compiler::ensure_predicate_loaded(func_decl * pred, instruction_block & acc) {
        pred2idx::entry * e = m_pred_regs.insert_if_not_there2(pred, UINT_MAX);
        if(e->get_data().m_value!=UINT_MAX) {
            //predicate is already loaded
            return;
        }
        relation_signature sig;
        m_context.get_rmanager().from_predicate(pred, sig);
        reg_idx reg = get_fresh_register(sig);
        e->get_data().m_value=reg;

        acc.push_back(instruction::mk_load(m_context.get_manager(), pred, reg));
    }

    void compiler::make_join(reg_idx t1, reg_idx t2, const variable_intersection & vars, reg_idx & result, 
            instruction_block & acc) {
        relation_signature res_sig;
        relation_signature::from_join(m_reg_signatures[t1], m_reg_signatures[t2], vars.size(), 
            vars.get_cols1(), vars.get_cols2(), res_sig);
        result = get_fresh_register(res_sig);
        acc.push_back(instruction::mk_join(t1, t2, vars.size(), vars.get_cols1(), vars.get_cols2(), result));
    }

    void compiler::make_join_project(reg_idx t1, reg_idx t2, const variable_intersection & vars, 
            const unsigned_vector & removed_cols, reg_idx & result, instruction_block & acc) {
        relation_signature aux_sig;
        relation_signature::from_join(m_reg_signatures[t1], m_reg_signatures[t2], vars.size(), 
            vars.get_cols1(), vars.get_cols2(), aux_sig);
        relation_signature res_sig;
        relation_signature::from_project(aux_sig, removed_cols.size(), removed_cols.c_ptr(), 
            res_sig);

        result = get_fresh_register(res_sig);
        acc.push_back(instruction::mk_join_project(t1, t2, vars.size(), vars.get_cols1(), 
            vars.get_cols2(), removed_cols.size(), removed_cols.c_ptr(), result));
    }

    void compiler::make_select_equal_and_project(reg_idx src, const relation_element & val, unsigned col,
            reg_idx & result, instruction_block & acc) {
        relation_signature res_sig;
        relation_signature::from_project(m_reg_signatures[src], 1, &col, res_sig);
        result = get_fresh_register(res_sig);
        acc.push_back(instruction::mk_select_equal_and_project(m_context.get_manager(),
            src, val, col, result));
    }

    void compiler::make_clone(reg_idx src, reg_idx & result, instruction_block & acc) {
        relation_signature sig = m_reg_signatures[src];
        result = get_fresh_register(sig);
        acc.push_back(instruction::mk_clone(src, result));
    }

    void compiler::make_union(reg_idx src, reg_idx tgt, reg_idx delta, bool widening, 
            instruction_block & acc) {
        SASSERT(m_reg_signatures[src]==m_reg_signatures[tgt]);
        SASSERT(delta==execution_context::void_register || m_reg_signatures[src]==m_reg_signatures[delta]);

        if(widening) {
            acc.push_back(instruction::mk_widen(src, tgt, delta));
        }
        else {
            acc.push_back(instruction::mk_union(src, tgt, delta));
        }
    }

    void compiler::make_projection(reg_idx src, unsigned col_cnt, const unsigned * removed_cols, 
            reg_idx & result, instruction_block & acc) {
        SASSERT(col_cnt>0);

        relation_signature res_sig;
        relation_signature::from_project(m_reg_signatures[src], col_cnt, removed_cols, res_sig);
        result = get_fresh_register(res_sig);
        acc.push_back(instruction::mk_projection(src, col_cnt, removed_cols, result));
    }

    compiler::reg_idx compiler::get_fresh_register(const relation_signature & sig) {
        //since we might be resizing the m_reg_signatures vector, the argument must not point inside it
        SASSERT((&sig>=m_reg_signatures.end()) || (&sig<m_reg_signatures.begin()));
        reg_idx result = m_reg_signatures.size();
        m_reg_signatures.push_back(sig);
        return result;
    }

    compiler::reg_idx compiler::get_single_column_register(const relation_sort & s) {
        relation_signature singl_sig;
        singl_sig.push_back(s);
        return get_fresh_register(singl_sig);
    }

    void compiler::get_fresh_registers(const func_decl_set & preds,  pred2idx & regs) {
        func_decl_set::iterator pit = preds.begin();
        func_decl_set::iterator pend = preds.end();
        for(; pit!=pend; ++pit) {
            func_decl * pred = *pit;
            reg_idx reg;
            TRUSTME( m_pred_regs.find(pred, reg) );

            SASSERT(!regs.contains(pred));
            relation_signature sig = m_reg_signatures[reg];
            reg_idx delta_reg = get_fresh_register(sig);
            regs.insert(pred, delta_reg);
        }
    }

    void compiler::make_dealloc_non_void(reg_idx r, instruction_block & acc) {
        if(r!=execution_context::void_register) {
            acc.push_back(instruction::mk_dealloc(r));
        }
    }

    void compiler::make_add_constant_column(func_decl* head_pred, reg_idx src, const relation_sort & s, const relation_element & val,
            reg_idx & result, instruction_block & acc) {
        reg_idx singleton_table;
        if(!m_constant_registers.find(s, val, singleton_table)) {
            singleton_table = get_single_column_register(s);
            m_top_level_code.push_back(
                instruction::mk_unary_singleton(m_context.get_manager(), head_pred, s, val, singleton_table));
            m_constant_registers.insert(s, val, singleton_table);
        }
        if(src==execution_context::void_register) {
            make_clone(singleton_table, result, acc);
        }
        else {
            variable_intersection empty_vars(m_context.get_manager());
            make_join(src, singleton_table, empty_vars, result, acc);
        }
    }

    void compiler::make_add_unbound_column(rule* compiled_rule, unsigned col_idx, func_decl* pred, reg_idx src, const relation_sort & s, reg_idx & result, 
            instruction_block & acc) {
        
        TRACE("dl", tout << "Adding unbound column " << mk_pp(pred, m_context.get_manager()) << "\n";);
            IF_VERBOSE(3, { 
                    relation_manager& rm = m_context.get_rmanager(); 
                    expr_ref e(m_context.get_manager()); 
                    compiled_rule->to_formula(e); 
                    verbose_stream() << "Compiling unsafe rule column " << col_idx << "\n" 
                                     << mk_ismt2_pp(e, m_context.get_manager()) << "\n"; 
                });
        reg_idx total_table = get_single_column_register(s);
        relation_signature sig;
        sig.push_back(s);
        acc.push_back(instruction::mk_total(sig, pred, total_table));        
        if(src == execution_context::void_register) {
            result = total_table;
        }
        else {
            variable_intersection empty_vars(m_context.get_manager());
            make_join(src, total_table, empty_vars, result, acc);
            make_dealloc_non_void(total_table, acc);
        }
    }

    void compiler::make_full_relation(func_decl* pred, const relation_signature & sig, reg_idx & result, 
            instruction_block & acc) {
        TRACE("dl", tout << "Adding unbound column " << mk_pp(pred, m_context.get_manager()) << "\n";);
        result = get_fresh_register(sig);
        acc.push_back(instruction::mk_total(sig, pred, result));
    }


    void compiler::make_duplicate_column(reg_idx src, unsigned col, reg_idx & result, 
            instruction_block & acc) {

        relation_signature & src_sig = m_reg_signatures[src];
        reg_idx single_col_reg;
        unsigned src_col_cnt = src_sig.size();
        if(src_col_cnt==1) {
            single_col_reg = src;
        }
        else {
            unsigned_vector removed_cols;
            for(unsigned i=0; i<src_col_cnt; i++) {
                if(i!=col) {
                    removed_cols.push_back(i);
                }
            }
            make_projection(src, removed_cols.size(), removed_cols.c_ptr(), single_col_reg, acc);
        }
        variable_intersection vi(m_context.get_manager());
        vi.add_pair(col, 0);
        make_join(src, single_col_reg, vi, result, acc);
        make_dealloc_non_void(single_col_reg, acc);
    }

    void compiler::make_rename(reg_idx src, unsigned cycle_len, const unsigned * permutation_cycle, 
            reg_idx & result, instruction_block & acc) {
        relation_signature res_sig;
        relation_signature::from_rename(m_reg_signatures[src], cycle_len, permutation_cycle, res_sig);
        result = get_fresh_register(res_sig);
        acc.push_back(instruction::mk_rename(src, cycle_len, permutation_cycle, result));
    }

    void compiler::make_assembling_code(
        rule* compiled_rule, 
        func_decl* head_pred, 
        reg_idx    src, 
        const svector<assembling_column_info> & acis0,
        reg_idx &           result, 
        instruction_block & acc) {

        TRACE("dl", tout << mk_pp(head_pred, m_context.get_manager()) << "\n";);

        unsigned col_cnt = acis0.size();
        reg_idx curr = src;

        relation_signature empty_signature;

        relation_signature * curr_sig;
        if(curr!=execution_context::void_register) {
            curr_sig = & m_reg_signatures[curr];
        }
        else {
            curr_sig = & empty_signature;
        }
        unsigned src_col_cnt=curr_sig->size();

        svector<assembling_column_info> acis(acis0);
        int2int handled_unbound;

        //first remove unused source columns
        int_set referenced_src_cols;
        for(unsigned i=0; i<col_cnt; i++) {
            if(acis[i].kind==ACK_BOUND_VAR) {
                SASSERT(acis[i].source_column<src_col_cnt); //we refer only to existing columns
                referenced_src_cols.insert(acis[i].source_column);
            }
        }

        //if an ACK_BOUND_VAR pointed to column i, after projection it will point to
        //i-new_src_col_offset[i] due to removal of some of earlier columns
        unsigned_vector new_src_col_offset;

        unsigned_vector src_cols_to_remove;
        for(unsigned i=0; i<src_col_cnt; i++) {
            if(!referenced_src_cols.contains(i)) {
                src_cols_to_remove.push_back(i);
            }
            new_src_col_offset.push_back(src_cols_to_remove.size());
        }
        if(!src_cols_to_remove.empty()) {
            reg_idx new_curr;
            make_projection(curr, src_cols_to_remove.size(), src_cols_to_remove.c_ptr(), new_curr, acc);
            make_dealloc_non_void(curr, acc);
            curr=new_curr;
            curr_sig = & m_reg_signatures[curr];

            //update ACK_BOUND_VAR references
            for(unsigned i=0; i<col_cnt; i++) {
                if(acis[i].kind==ACK_BOUND_VAR) {
                    unsigned col = acis[i].source_column;
                    acis[i].source_column = col-new_src_col_offset[col];
                }
            }
        }

        //convert all result columns into bound variables by extending the source table
        for(unsigned i=0; i<col_cnt; i++) {
            if(acis[i].kind==ACK_BOUND_VAR) {
                continue;
            }
            unsigned bound_column_index;
            if(acis[i].kind!=ACK_UNBOUND_VAR || !handled_unbound.find(acis[i].var_index,bound_column_index)) {
                reg_idx new_curr;
                bound_column_index=curr_sig->size();
                if(acis[i].kind==ACK_CONSTANT) {
                    make_add_constant_column(head_pred, curr, acis[i].domain, acis[i].constant, new_curr, acc);
                }
                else {
                    SASSERT(acis[i].kind==ACK_UNBOUND_VAR);
                    make_add_unbound_column(compiled_rule, i, head_pred, curr, acis[i].domain, new_curr, acc);
                    handled_unbound.insert(acis[i].var_index,bound_column_index);
                }
                make_dealloc_non_void(curr, acc);
                curr=new_curr;
                curr_sig = & m_reg_signatures[curr];
                SASSERT(bound_column_index==curr_sig->size()-1);
            }
            SASSERT((*curr_sig)[bound_column_index]==acis[i].domain);
            acis[i].kind=ACK_BOUND_VAR;
            acis[i].source_column=bound_column_index;
        }

        //duplicate needed source columns
        int_set used_cols;
        for(unsigned i=0; i<col_cnt; i++) {
            SASSERT(acis[i].kind==ACK_BOUND_VAR);
            unsigned col=acis[i].source_column;
            if(!used_cols.contains(col)) {
                used_cols.insert(col);
                continue;
            }
            reg_idx new_curr;
            make_duplicate_column(curr, col, new_curr, acc);
            make_dealloc_non_void(curr, acc);
            curr=new_curr;
            curr_sig = & m_reg_signatures[curr];
            unsigned bound_column_index=curr_sig->size()-1;
            SASSERT((*curr_sig)[bound_column_index]==acis[i].domain);
            acis[i].source_column=bound_column_index;
        }

        //reorder source columns to match target
        SASSERT(curr_sig->size()==col_cnt); //now the intermediate table is a permutation
        for(unsigned i=0; i<col_cnt; i++) {
            if(acis[i].source_column==i) {
                continue;
            }
            unsigned_vector permutation;
            unsigned next=i;
            do {
                permutation.push_back(next);
                unsigned prev=next;
                next=acis[prev].source_column;
                SASSERT(next>=i); //columns below i are already reordered
                SASSERT(next<col_cnt);
                acis[prev].source_column=prev;
                SASSERT(permutation.size()<=col_cnt); //this should not be an infinite loop
            } while(next!=i);

            reg_idx new_curr;
            make_rename(curr, permutation.size(), permutation.c_ptr(), new_curr, acc);
            make_dealloc_non_void(curr, acc);
            curr=new_curr;
            curr_sig = & m_reg_signatures[curr];
        }

        if(curr==execution_context::void_register) {
            SASSERT(src==execution_context::void_register);
            SASSERT(acis0.size()==0);
            make_full_relation(head_pred, empty_signature, curr, acc);
        }

        result=curr;
    }

    void compiler::get_local_indexes_for_projection(app * t, var_counter & globals, unsigned ofs, 
            unsigned_vector & res) {
        unsigned n = t->get_num_args();
        for(unsigned i = 0; i<n; i++) {
            expr * e = t->get_arg(i);
            if(!is_var(e) || globals.get(to_var(e)->get_idx())!=0) {
                continue;
            }
            res.push_back(i+ofs);
        }
    }

    void compiler::get_local_indexes_for_projection(rule * r, unsigned_vector & res) {
        SASSERT(r->get_positive_tail_size()==2);
        ast_manager & m = m_context.get_manager();
        var_counter counter;
        counter.count_vars(m, r);
        app * t1 = r->get_tail(0);
        app * t2 = r->get_tail(1);
        counter.count_vars(m, t1, -1);
        counter.count_vars(m, t2, -1);
        get_local_indexes_for_projection(t1, counter, 0, res);
        get_local_indexes_for_projection(t2, counter, t1->get_num_args(), res);
    }

    void compiler::compile_rule_evaluation_run(rule * r, reg_idx head_reg, const reg_idx * tail_regs, 
            reg_idx delta_reg, bool use_widening, instruction_block & acc) {
        
        m_instruction_observer.start_rule(r);

        const app * h = r->get_head();
        unsigned head_len = h->get_num_args();
        func_decl * head_pred = h->get_decl();

        TRACE("dl", r->display(m_context, tout); );

        unsigned pt_len = r->get_positive_tail_size();
        SASSERT(pt_len<=2); //we require rules to be processed by the mk_simple_joins rule transformer plugin

        reg_idx single_res;
        ptr_vector<expr> single_res_expr;

        //used to save on filter_identical instructions where the check is already done 
        //by the join operation
        unsigned second_tail_arg_ofs;

        if(pt_len == 2) {
            reg_idx t1_reg=tail_regs[0];
            reg_idx t2_reg=tail_regs[1];
            app * a1 = r->get_tail(0);
            app * a2 = r->get_tail(1);
            SASSERT(m_reg_signatures[t1_reg].size()==a1->get_num_args());
            SASSERT(m_reg_signatures[t2_reg].size()==a2->get_num_args());

            variable_intersection a1a2(m_context.get_manager());
            a1a2.populate(a1,a2);

            unsigned_vector removed_cols;
            get_local_indexes_for_projection(r, removed_cols);

            if(removed_cols.empty()) {
                make_join(t1_reg, t2_reg, a1a2, single_res, acc);
            }
            else {
                make_join_project(t1_reg, t2_reg, a1a2, removed_cols, single_res, acc);
            }

            unsigned rem_index = 0;
            unsigned rem_sz = removed_cols.size();
            unsigned a1len=a1->get_num_args();
            for(unsigned i=0; i<a1len; i++) {
                SASSERT(rem_index==rem_sz || removed_cols[rem_index]>=i);
                if(rem_index<rem_sz && removed_cols[rem_index]==i) {
                    rem_index++;
                    continue;
                }
                single_res_expr.push_back(a1->get_arg(i));
            }
            second_tail_arg_ofs = single_res_expr.size();
            unsigned a2len=a2->get_num_args();
            for(unsigned i=0; i<a2len; i++) {
                SASSERT(rem_index==rem_sz || removed_cols[rem_index]>=i+a1len);
                if(rem_index<rem_sz && removed_cols[rem_index]==i+a1len) {
                    rem_index++;
                    continue;
                }
                single_res_expr.push_back(a2->get_arg(i));
            }
            SASSERT(rem_index==rem_sz);
        }
        else if(pt_len==1) {
            reg_idx t_reg=tail_regs[0];
            app * a = r->get_tail(0);
            SASSERT(m_reg_signatures[t_reg].size()==a->get_num_args());

            single_res = t_reg;

            unsigned n=a->get_num_args();
            for(unsigned i=0; i<n; i++) {
                expr * arg = a->get_arg(i);
                if(is_app(arg)) {
                    app * c = to_app(arg); //argument is a constant
                    SASSERT(c->get_num_args()==0);
                    SASSERT(m_context.get_decl_util().is_numeral_ext(arg));
                    reg_idx new_reg;
                    make_select_equal_and_project(single_res, c, single_res_expr.size(), new_reg, acc);
                    if(single_res!=t_reg) {
                        //since single_res is a local register, we deallocate it
                        make_dealloc_non_void(single_res, acc);
                    }
                    single_res = new_reg;
                }
                else {
                    SASSERT(is_var(arg));
                    single_res_expr.push_back(arg);
                }
            }
            if(single_res==t_reg) {
                //we may be modifying the register later, so we need a local copy
                make_clone(t_reg, single_res, acc);
            }

        }
        else {
            SASSERT(pt_len==0);

            //single_res register should never be used in this case
            single_res=execution_context::void_register;
        }

        add_unbound_columns_for_negation(r, head_pred, single_res, single_res_expr, acc);

        int2ints var_indexes;

        reg_idx filtered_res = single_res;

        {
            //enforce equality to constants
            unsigned srlen=single_res_expr.size();
            SASSERT((single_res==execution_context::void_register) ? (srlen==0) : (srlen==m_reg_signatures[single_res].size()));
            for(unsigned i=0; i<srlen; i++) {
                expr * exp = single_res_expr[i];
                if(is_app(exp)) {
                    SASSERT(m_context.get_decl_util().is_numeral_ext(exp));
                    relation_element value = to_app(exp);
                    acc.push_back(instruction::mk_filter_equal(m_context.get_manager(), filtered_res, value, i));
                }
                else {
                    SASSERT(is_var(exp));
                    unsigned var_num=to_var(exp)->get_idx();
                    int2ints::entry * e = var_indexes.insert_if_not_there2(var_num, unsigned_vector());
                    e->get_data().m_value.push_back(i);
                }
            }
        }

        //enforce equality of columns
        int2ints::iterator vit=var_indexes.begin();
        int2ints::iterator vend=var_indexes.end();
        for(; vit!=vend; ++vit) {
            int2ints::key_data & k = *vit;
            unsigned_vector & indexes = k.m_value;
            if(indexes.size()==1) {
                continue;
            }
            SASSERT(indexes.size()>1);
            if(pt_len==2 && indexes[0]<second_tail_arg_ofs && indexes.back()>=second_tail_arg_ofs) {
                //If variable appears in multiple tails, the identicity will already be enforced by join.
                //(If behavior the join changes so that it is not enforced anymore, remove this
                //condition!)
                continue;
            }
            acc.push_back(instruction::mk_filter_identical(filtered_res, indexes.size(), indexes.c_ptr()));
        }

        //enforce negative predicates
        unsigned ut_len=r->get_uninterpreted_tail_size();
        for(unsigned i=pt_len; i<ut_len; i++) {
            app * neg_tail = r->get_tail(i);
            func_decl * neg_pred = neg_tail->get_decl();
            variable_intersection neg_intersection(m_context.get_manager());
            neg_intersection.populate(single_res_expr, neg_tail);
            unsigned_vector t_cols(neg_intersection.size(), neg_intersection.get_cols1());
            unsigned_vector neg_cols(neg_intersection.size(), neg_intersection.get_cols2());

            unsigned neg_len = neg_tail->get_num_args();
            for(unsigned i=0; i<neg_len; i++) {
                expr * e = neg_tail->get_arg(i);
                if(is_var(e)) {
                    continue;
                }
                SASSERT(is_app(e));
                relation_sort arg_sort;
                m_context.get_rmanager().from_predicate(neg_pred, i, arg_sort);
                reg_idx new_reg;
                make_add_constant_column(head_pred, filtered_res, arg_sort, to_app(e), new_reg, acc);

                make_dealloc_non_void(filtered_res, acc);
                filtered_res = new_reg;                // here filtered_res value gets changed !!

                t_cols.push_back(single_res_expr.size());
                neg_cols.push_back(i);
                single_res_expr.push_back(e);
            }
            SASSERT(t_cols.size()==neg_cols.size());

            reg_idx neg_reg;
            TRUSTME( m_pred_regs.find(neg_pred, neg_reg) );
            acc.push_back(instruction::mk_filter_by_negation(filtered_res, neg_reg, t_cols.size(),
                t_cols.c_ptr(), neg_cols.c_ptr()));
        }

        //enforce interpreted tail predicates
        unsigned ft_len=r->get_tail_size(); //full tail
        for(unsigned tail_index=ut_len; tail_index<ft_len; tail_index++) {
            app * t = r->get_tail(tail_index);
            var_idx_set t_vars;
            ast_manager & m = m_context.get_manager();
            collect_vars(m, t, t_vars);

            if(t_vars.empty()) {
                expr_ref simplified(m);
                m_context.get_rewriter()(t, simplified);
                if(m.is_true(simplified)) {
                    //this tail element is always true
                    continue;
                }
                //the tail of this rule is never satisfied
                SASSERT(m.is_false(simplified));
                goto finish;
            }

            //determine binding size
            unsigned max_var=0;
            var_idx_set::iterator vit = t_vars.begin();
            var_idx_set::iterator vend = t_vars.end();
            for(; vit!=vend; ++vit) {
                unsigned v = *vit;
                if(v>max_var) { max_var = v; }
            }

            //create binding
            expr_ref_vector binding(m);
            binding.resize(max_var+1);
            vit = t_vars.begin();
            for(; vit!=vend; ++vit) {
                unsigned v = *vit;
                int2ints::entry * e = var_indexes.find_core(v);
                if(!e) {
                    //we have an unbound variable, so we add an unbound column for it
                    relation_sort unbound_sort = 0;

                    for(unsigned hindex = 0; hindex<head_len; hindex++) {
                        expr * harg = h->get_arg(hindex);
                        if(!is_var(harg) || to_var(harg)->get_idx()!=v) {
                            continue;
                        }
                        unbound_sort = to_var(harg)->get_sort();
                    }
                    if(!unbound_sort) {
                        // the variable in the interpreted tail is neither bound in the 
                        // uninterpreted tail nor present in the head
                        std::stringstream sstm;
                        sstm << "rule with unbound variable #" << v << " in interpreted tail: ";                       
                        r->display(m_context, sstm);
                        throw default_exception(sstm.str());
                    }

                    reg_idx new_reg;
                    TRACE("dl", tout << mk_pp(head_pred, m_context.get_manager()) << "\n";);
                    make_add_unbound_column(r, 0, head_pred, filtered_res, unbound_sort, new_reg, acc);

                    make_dealloc_non_void(filtered_res, acc);
                    filtered_res = new_reg;                // here filtered_res value gets changed !!

                    unsigned unbound_column_index = single_res_expr.size();
                    single_res_expr.push_back(m.mk_var(v, unbound_sort));

                    e = var_indexes.insert_if_not_there2(v, unsigned_vector());
                    e->get_data().m_value.push_back(unbound_column_index);
                }
                unsigned src_col=e->get_data().m_value.back();
                relation_sort var_sort = m_reg_signatures[filtered_res][src_col];
                binding[max_var-v]=m.mk_var(src_col, var_sort);
            }


            expr_ref renamed(m);
            m_context.get_var_subst()(t, binding.size(), binding.c_ptr(), renamed);
            app_ref app_renamed(to_app(renamed), m);
            acc.push_back(instruction::mk_filter_interpreted(filtered_res, app_renamed));
        }

        {
            //put together the columns of head relation
            relation_signature & head_sig = m_reg_signatures[head_reg];
            svector<assembling_column_info> head_acis;
            unsigned_vector head_src_cols;
            for(unsigned i=0; i<head_len; i++) {
                assembling_column_info aci;
                aci.domain=head_sig[i];

                expr * exp = h->get_arg(i);
                if(is_var(exp)) {
                    unsigned var_num=to_var(exp)->get_idx();
                    int2ints::entry * e = var_indexes.find_core(var_num);
                    if(e) {
                        unsigned_vector & binding_indexes = e->get_data().m_value;
                        aci.kind=ACK_BOUND_VAR;
                        aci.source_column=binding_indexes.back();
                        SASSERT(aci.source_column<single_res_expr.size()); //we bind only to existing columns
                        if(binding_indexes.size()>1) {
                            //if possible, we do not want multiple head columns
                            //point to a single column in the intermediate table,
                            //since then we would have to duplicate the column
                            //(and remove columns we did not point to at all)
                            binding_indexes.pop_back();
                        }
                    }
                    else {
                        aci.kind=ACK_UNBOUND_VAR;
                        aci.var_index=var_num;
                    }
                }
                else {
                    SASSERT(is_app(exp));
                    SASSERT(m_context.get_decl_util().is_numeral_ext(exp));
                    aci.kind=ACK_CONSTANT;
                    aci.constant=to_app(exp);
                }
                head_acis.push_back(aci);
            }
            SASSERT(head_acis.size()==head_len);

            reg_idx new_head_reg;
            make_assembling_code(r, head_pred, filtered_res, head_acis, new_head_reg, acc);

            //update the head relation
            make_union(new_head_reg, head_reg, delta_reg, use_widening, acc);
        }

    finish:
        m_instruction_observer.finish_rule();
    }

    void compiler::add_unbound_columns_for_negation(rule* r, func_decl* pred, reg_idx& single_res, ptr_vector<expr>& single_res_expr, 
                                                    instruction_block & acc) {
        uint_set pos_vars;
        u_map<expr*> neg_vars;
        ast_manager& m = m_context.get_manager();
        unsigned pt_len = r->get_positive_tail_size();
        unsigned ut_len = r->get_uninterpreted_tail_size();
        if (pt_len == ut_len || pt_len == 0) {
            return;
        }
        // populate negative variables:
        for (unsigned i = pt_len; i < ut_len; ++i) {
            app * neg_tail = r->get_tail(i);
            unsigned neg_len = neg_tail->get_num_args();
            for (unsigned j = 0; j < neg_len; ++j) {
                expr * e = neg_tail->get_arg(j);
                if (is_var(e)) {
                    neg_vars.insert(to_var(e)->get_idx(), e);
                }
            }
        }
        // populate positive variables:
        for (unsigned i = 0; i < single_res_expr.size(); ++i) {
            expr* e = single_res_expr[i];
            if (is_var(e)) {
                pos_vars.insert(to_var(e)->get_idx());
            }
        }
        // add negative variables that are not in positive:
        u_map<expr*>::iterator it = neg_vars.begin(), end = neg_vars.end();
        for (; it != end; ++it) {
            unsigned v = it->m_key;
            expr* e = it->m_value;
            if (!pos_vars.contains(v)) {
                single_res_expr.push_back(e);
                make_add_unbound_column(r, v, pred, single_res, m.get_sort(e), single_res, acc);
                TRACE("dl", tout << "Adding unbound column: " << mk_pp(e, m) << "\n";);
            }
        }
    }

    void compiler::compile_rule_evaluation(rule * r, const pred2idx * input_deltas,
            reg_idx output_delta, bool use_widening, instruction_block & acc) {
        typedef std::pair<reg_idx, unsigned> tail_delta_info; //(delta register, tail index)
        typedef svector<tail_delta_info> tail_delta_infos;

        unsigned rule_len = r->get_uninterpreted_tail_size();
        reg_idx head_reg;
        TRUSTME( m_pred_regs.find(r->get_head()->get_decl(), head_reg) );

        svector<reg_idx> tail_regs;
        tail_delta_infos tail_deltas;
        for(unsigned j=0;j<rule_len;j++) {
            func_decl * tail_pred = r->get_tail(j)->get_decl();
            reg_idx tail_reg;
            TRUSTME( m_pred_regs.find(tail_pred, tail_reg) );
            tail_regs.push_back(tail_reg);

            if(input_deltas && !all_or_nothing_deltas()) {
                reg_idx tail_delta_idx;
                if(input_deltas->find(tail_pred, tail_delta_idx)) {
                    tail_deltas.push_back(tail_delta_info(tail_delta_idx, j));
                }
            }
        }

        if(!input_deltas || all_or_nothing_deltas()) {
            compile_rule_evaluation_run(r, head_reg, tail_regs.c_ptr(), output_delta, use_widening, acc);
        }
        else {
            tail_delta_infos::iterator tdit = tail_deltas.begin();
            tail_delta_infos::iterator tdend = tail_deltas.end();
            for(; tdit!=tdend; ++tdit) {
                tail_delta_info tdinfo = *tdit;
                flet<reg_idx> flet_tail_reg(tail_regs[tdinfo.second], tdinfo.first);
                compile_rule_evaluation_run(r, head_reg, tail_regs.c_ptr(), output_delta, use_widening, acc);
            }
        }
    }

    class cycle_breaker
    {
        typedef func_decl * T;
        typedef rule_dependencies::item_set item_set; //set of T

        rule_dependencies & m_deps;
        item_set & m_removed;
        svector<T> m_stack;
        ast_mark m_stack_content;
        ast_mark m_visited;

        void traverse(T v) {
            SASSERT(!m_stack_content.is_marked(v));
            if(m_visited.is_marked(v) || m_removed.contains(v)) {
                return;
            }

            m_stack.push_back(v);
            m_stack_content.mark(v, true);
            m_visited.mark(v, true);

            const item_set & deps = m_deps.get_deps(v);
            item_set::iterator it = deps.begin();
            item_set::iterator end = deps.end();
            for(; it!=end; ++it) {
                T d = *it;
                if(m_stack_content.is_marked(d)) {
                    //TODO: find the best vertex to remove in the cycle
                    m_removed.insert(v);
                    break;
                }
                traverse(d);
            }
            SASSERT(m_stack.back()==v);

            m_stack.pop_back();
            m_stack_content.mark(v, false);
        }
    public:
        cycle_breaker(rule_dependencies & deps, item_set & removed) 
            : m_deps(deps), m_removed(removed) { SASSERT(removed.empty()); }

        void operator()() {
            rule_dependencies::iterator it = m_deps.begin();
            rule_dependencies::iterator end = m_deps.end();
            for(; it!=end; ++it) {
                T v = it->m_key;
                traverse(v);
            }
            m_deps.remove(m_removed);
        }
    };

    void compiler::detect_chains(const func_decl_set & preds, func_decl_vector & ordered_preds, 
            func_decl_set & global_deltas) {
        typedef obj_map<func_decl, func_decl *> pred2pred;

        SASSERT(ordered_preds.empty());
        SASSERT(global_deltas.empty());

        rule_dependencies deps(m_rule_set.get_dependencies());
        deps.restrict(preds);
        cycle_breaker(deps, global_deltas)();
        TRUSTME( deps.sort_deps(ordered_preds) );

        //the predicates that were removed to get acyclic induced subgraph are put last
        //so that all their local input deltas are already populated
        func_decl_set::iterator gdit = global_deltas.begin();
        func_decl_set::iterator gend = global_deltas.end();
        for(; gdit!=gend; ++gdit) {
            ordered_preds.push_back(*gdit);
        }
    }

    void compiler::compile_preds(const func_decl_vector & head_preds, const func_decl_set & widened_preds,
            const pred2idx * input_deltas, const pred2idx & output_deltas, instruction_block & acc) {
        func_decl_vector::const_iterator hpit = head_preds.begin();
        func_decl_vector::const_iterator hpend = head_preds.end();
        for(; hpit!=hpend; ++hpit) {
            func_decl * head_pred = *hpit;

            bool widen_predicate_in_loop = widened_preds.contains(head_pred);

            reg_idx d_head_reg; //output delta for the initial rule execution
            if(!output_deltas.find(head_pred, d_head_reg)) {
                d_head_reg = execution_context::void_register;
            }

            const rule_vector & pred_rules = m_rule_set.get_predicate_rules(head_pred);
            rule_vector::const_iterator rit = pred_rules.begin();
            rule_vector::const_iterator rend = pred_rules.end();
            for(; rit!=rend; ++rit) {
                rule * r = *rit;
                SASSERT(head_pred==r->get_head()->get_decl());

                compile_rule_evaluation(r, input_deltas, d_head_reg, widen_predicate_in_loop, acc);
            }
        }
    }

    void compiler::make_inloop_delta_transition(const pred2idx & global_head_deltas, 
            const pred2idx & global_tail_deltas, const pred2idx & local_deltas, instruction_block & acc) {
        //move global head deltas into tail ones
        pred2idx::iterator gdit = global_head_deltas.begin();
        pred2idx::iterator gend = global_head_deltas.end();
        for(; gdit!=gend; ++gdit) {
            func_decl * pred = gdit->m_key;
            reg_idx head_reg = gdit->m_value;
            reg_idx tail_reg;
            TRUSTME( global_tail_deltas.find(pred, tail_reg) );
            acc.push_back(instruction::mk_move(head_reg, tail_reg));
        }
        //empty local deltas
        pred2idx::iterator lit = local_deltas.begin();
        pred2idx::iterator lend = local_deltas.end();
        for(; lit!=lend; ++lit) {
            acc.push_back(instruction::mk_dealloc(lit->m_value));
        }
    }

    void compiler::compile_loop(const func_decl_vector & head_preds, const func_decl_set & widened_preds,
            const pred2idx & global_head_deltas, const pred2idx & global_tail_deltas, 
            const pred2idx & local_deltas, instruction_block & acc) {
        instruction_block * loop_body = alloc(instruction_block);
        loop_body->set_observer(&m_instruction_observer);

        pred2idx all_head_deltas(global_head_deltas);
        unite_disjoint_maps(all_head_deltas, local_deltas);
        pred2idx all_tail_deltas(global_tail_deltas);
        unite_disjoint_maps(all_tail_deltas, local_deltas);

        //generate code for the iterative fixpoint search
        //The order in which we iterate the preds_vector matters, since rules can depend on
        //deltas generated earlier in the same iteration.
        compile_preds(head_preds, widened_preds, &all_tail_deltas, all_head_deltas, *loop_body);

        svector<reg_idx> loop_control_regs; //loop is controlled by global src regs
        collect_map_range(loop_control_regs, global_tail_deltas);
        //move target deltas into source deltas at the end of the loop
        //and clear local deltas
        make_inloop_delta_transition(global_head_deltas, global_tail_deltas, local_deltas, *loop_body);

        loop_body->set_observer(0);
        acc.push_back(instruction::mk_while_loop(loop_control_regs.size(),
            loop_control_regs.c_ptr(),loop_body));
    }

    void compiler::compile_dependent_rules(const func_decl_set & head_preds,
            const pred2idx * input_deltas, const pred2idx & output_deltas, 
            bool add_saturation_marks, instruction_block & acc) {
        
        if(!output_deltas.empty()) {
            func_decl_set::iterator hpit = head_preds.begin();
            func_decl_set::iterator hpend = head_preds.end();
            for(; hpit!=hpend; ++hpit) {
                if(output_deltas.contains(*hpit)) {
                    //we do not support retrieving deltas for rules that are inside a recursive 
                    //stratum, since we would have to maintain this 'global' delta through the loop
                    //iterations
                    NOT_IMPLEMENTED_YET();
                }
            }
        }

        func_decl_vector preds_vector;
        func_decl_set global_deltas;

        detect_chains(head_preds, preds_vector, global_deltas);

        func_decl_set local_deltas(head_preds);
        set_difference(local_deltas, global_deltas);

        pred2idx d_global_src;  //these deltas serve as sources of tuples for rule evaluation inside the loop
        get_fresh_registers(global_deltas, d_global_src);
        pred2idx d_global_tgt;  //these deltas are targets for new tuples in rule evaluation inside the loop
        get_fresh_registers(global_deltas, d_global_tgt);
        pred2idx d_local;
        get_fresh_registers(local_deltas, d_local);

        pred2idx d_all_src(d_global_src); //src together with local deltas
        unite_disjoint_maps(d_all_src, d_local);
        pred2idx d_all_tgt(d_global_tgt); //tgt together with local deltas
        unite_disjoint_maps(d_all_tgt, d_local);


        func_decl_set empty_func_decl_set;

        //generate code for the initial run
        compile_preds(preds_vector, empty_func_decl_set, input_deltas, d_global_src, acc);

        if(!compile_with_widening()) {
            compile_loop(preds_vector, empty_func_decl_set, d_global_tgt, d_global_src,
                d_local, acc);
        }
        else {
            //do the part where we zero the global predicates and run the loop saturation loop again
            if(global_deltas.size()<head_preds.size()) {

                pred2idx globals_backup;
                get_fresh_registers(global_deltas, globals_backup); //these actually are not deltas

                {
                    //make backup copy of relations that will be widened
                    func_decl_set::iterator it = global_deltas.begin();
                    func_decl_set::iterator end = global_deltas.end();
                    for(; it!=end; ++it) {
                        reg_idx rel_idx;
                        TRUSTME( m_pred_regs.find(*it, rel_idx) );
                        reg_idx backup_idx;
                        TRUSTME( globals_backup.find(*it, backup_idx) );

                        make_clone(rel_idx, backup_idx, acc);
                    }
                }

                compile_loop(preds_vector, global_deltas, d_global_tgt, d_global_src,
                    d_local, acc);

                {
                    //restore the original values of widened relations before we rerun the loop
                    func_decl_set::iterator it = global_deltas.begin();
                    func_decl_set::iterator end = global_deltas.end();
                    for(; it!=end; ++it) {
                        reg_idx rel_idx;
                        TRUSTME( m_pred_regs.find(*it, rel_idx) );
                        reg_idx backup_idx;
                        TRUSTME( globals_backup.find(*it, backup_idx) );

                        acc.push_back(instruction::mk_move(backup_idx, rel_idx));
                    }
                }
            }
            compile_loop(preds_vector, global_deltas, d_global_tgt, d_global_src,
                d_local, acc);
        }


        if(add_saturation_marks) {
            //after the loop finishes, all predicates in the group are saturated, 
            //so we may mark them as such
            func_decl_set::iterator fdit = head_preds.begin();
            func_decl_set::iterator fdend = head_preds.end();
            for(; fdit!=fdend; ++fdit) {
                acc.push_back(instruction::mk_mark_saturated(m_context.get_manager(), *fdit));
            }
        }
    }

    bool compiler::is_nonrecursive_stratum(const func_decl_set & preds) const {
        SASSERT(preds.size()>0);
        if(preds.size()>1) {
            return false;
        }
        func_decl * head_pred = *preds.begin();
        const rule_vector & rules = m_rule_set.get_predicate_rules(head_pred);
        rule_vector::const_iterator it = rules.begin();
        rule_vector::const_iterator end = rules.end();
        for(; it!=end; ++it) {
            //it is sufficient to check just for presence of the first head predicate,
            //since if the rules are recursive and their heads are strongly connected by dependence,
            //this predicate must appear in some tail
            if((*it)->is_in_tail(head_pred)) {
                return false;
            }
        }
        return true;
    }

    void compiler::compile_nonrecursive_stratum(const func_decl_set & preds, 
            const pred2idx * input_deltas, const pred2idx & output_deltas, 
            bool add_saturation_marks, instruction_block & acc) {
        //non-recursive stratum always has just one head predicate
        SASSERT(preds.size()==1);
        SASSERT(is_nonrecursive_stratum(preds));
        func_decl * head_pred = *preds.begin();
        const rule_vector & rules = m_rule_set.get_predicate_rules(head_pred);

        reg_idx output_delta;
        if(!output_deltas.find(head_pred, output_delta)) {
            output_delta = execution_context::void_register;
        }

        rule_vector::const_iterator it = rules.begin();
        rule_vector::const_iterator end = rules.end();
        for(; it!=end; ++it) {
            rule * r = *it;
            SASSERT(r->get_head()->get_decl()==head_pred);

            compile_rule_evaluation(r, input_deltas, output_delta, false, acc);
        }

        if(add_saturation_marks) {
            //now the predicate is saturated, so we may mark it as such
            acc.push_back(instruction::mk_mark_saturated(m_context.get_manager(), head_pred));
        }
    }

    bool compiler::all_saturated(const func_decl_set & preds) const {
        func_decl_set::iterator fdit = preds.begin();
        func_decl_set::iterator fdend = preds.end();
        for(; fdit!=fdend; ++fdit) {
            if(!m_context.get_rmanager().is_saturated(*fdit)) {
                return false;
            }
        }
        return true;
    }

    void compiler::compile_strats(const rule_stratifier & stratifier, 
            const pred2idx * input_deltas, const pred2idx & output_deltas, 
            bool add_saturation_marks, instruction_block & acc) {
        rule_set::pred_set_vector strats = stratifier.get_strats();
        rule_set::pred_set_vector::const_iterator sit = strats.begin();
        rule_set::pred_set_vector::const_iterator send = strats.end();
        for(; sit!=send; ++sit) {
            func_decl_set & strat_preds = **sit;

            if(all_saturated(strat_preds)) {
                //all predicates in stratum are saturated, so no need to compile rules for them
                continue;
            }

            TRACE("dl",
                tout << "Stratum: ";
                func_decl_set::iterator pit = strat_preds.begin();
                func_decl_set::iterator pend = strat_preds.end();
                for(; pit!=pend; ++pit) {
                    func_decl * pred = *pit;
                    tout << pred->get_name() << " ";
                }
                tout << "\n";
                );

            if(is_nonrecursive_stratum(strat_preds)) {
                //this stratum contains just a single non-recursive rule
                compile_nonrecursive_stratum(strat_preds, input_deltas, output_deltas, add_saturation_marks, acc);
            }
            else {
                compile_dependent_rules(strat_preds, input_deltas, output_deltas, 
                    add_saturation_marks, acc);
            }
        }
    }

    void compiler::do_compilation(instruction_block & execution_code, 
            instruction_block & termination_code) {

        unsigned rule_cnt=m_rule_set.get_num_rules();
        if(rule_cnt==0) {
            return;
        }

        instruction_block & acc = execution_code;
        acc.set_observer(&m_instruction_observer);


        //load predicate data
        for(unsigned i=0;i<rule_cnt;i++) {
            const rule * r = m_rule_set.get_rule(i);
            ensure_predicate_loaded(r->get_head()->get_decl(), acc);

            unsigned rule_len = r->get_uninterpreted_tail_size();
            for(unsigned j=0;j<rule_len;j++) {
                ensure_predicate_loaded(r->get_tail(j)->get_decl(), acc);
            }
        }
        
        pred2idx empty_pred2idx_map;

        compile_strats(m_rule_set.get_stratifier(), static_cast<pred2idx *>(0), 
            empty_pred2idx_map, true, execution_code);



        //store predicate data
        pred2idx::iterator pit = m_pred_regs.begin();
        pred2idx::iterator pend = m_pred_regs.end();
        for(; pit!=pend; ++pit) {
            pred2idx::key_data & e = *pit;
            func_decl * pred = e.m_key;
            reg_idx reg = e.m_value;
            termination_code.push_back(instruction::mk_store(m_context.get_manager(), pred, reg));
        }

        acc.set_observer(0);

        TRACE("dl", execution_code.display(m_context, tout););
    }


}

