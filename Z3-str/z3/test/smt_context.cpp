#include "smt_context.h"

void tst_smt_context()
{
    front_end_params params;

    ast_manager m;
    m.register_decl_plugins();

    smt::context ctx(m, params);

    app_ref a1(m.mk_const(symbol("a"), m.mk_bool_sort()), m);
    app_ref b1(m.mk_const(symbol("b"), m.mk_bool_sort()), m);
    app_ref c1(m.mk_const(symbol("c"), m.mk_bool_sort()), m);
    app_ref na1(m.mk_not(a1), m);
    ctx.assert_expr(na1);
    ctx.assert_expr(m.mk_or(c1.get(), b1.get()));

    {
        app_ref nc(m.mk_not(c1), m);
        ptr_vector<expr> assumptions;
        assumptions.push_back(nc.get());

        ctx.check(assumptions.size(), assumptions.c_ptr());
    }

    ctx.check();
}
