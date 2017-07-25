﻿/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    Solver.cs

Abstract:

    Z3 Managed API: Solvers

Author:

    Christoph Wintersteiger (cwinter) 2012-03-22

Notes:
    
--*/

using System;
using System.Diagnostics.Contracts;

namespace Microsoft.Z3
{
    /// <summary>
    /// Solvers.
    /// </summary>
    [ContractVerification(true)]
    public class Solver : Z3Object
    {
        /// <summary>
        /// A string that describes all available solver parameters.
        /// </summary>
        public string Help
        {
            get {
                Contract.Ensures(Contract.Result<string>() != null);

                return Native.Z3_solver_get_help(Context.nCtx, NativeObject); }
        }

        /// <summary>
        /// Sets the solver parameters.
        /// </summary>
        public Params Parameters
        {
            set
            {
                Contract.Requires(value != null);

                Context.CheckContextMatch(value);
                Native.Z3_solver_set_params(Context.nCtx, NativeObject, value.NativeObject);
            }
        }

        /// <summary>
        /// Retrieves parameter descriptions for solver.
        /// </summary>
	ParamDescrs ParameterDescriptions 
	{
	   get
	   {
               return new ParamDescrs(Context, Native.Z3_solver_get_param_descrs(Context.nCtx, NativeObject));
	   }	
	}


        /// <summary>
        /// The current number of backtracking points (scopes).
        /// </summary>
        /// <seealso cref="Pop"/>
        /// <seealso cref="Push"/>
        public uint NumScopes
        {
            get { return Native.Z3_solver_get_num_scopes(Context.nCtx, NativeObject); }
        }

        /// <summary>
        /// Creates a backtracking point.
        /// </summary>
        /// <seealso cref="Pop"/>
        public void Push()
        {
            Native.Z3_solver_push(Context.nCtx, NativeObject);
        }

        /// <summary>
        /// Backtracks <paramref name="n"/> backtracking points.
        /// </summary>
        /// <remarks>Note that an exception is thrown if <paramref name="n"/> is not smaller than <c>NumScopes</c></remarks>
        /// <seealso cref="Push"/>
        public void Pop(uint n = 1)
        {
            Native.Z3_solver_pop(Context.nCtx, NativeObject, n);
        }

        /// <summary>
        /// Resets the Solver.
        /// </summary>
        /// <remarks>This removes all assertions from the solver.</remarks>
        public void Reset()
        {
            Native.Z3_solver_reset(Context.nCtx, NativeObject);
        }

        /// <summary>
        /// Assert a constraint (or multiple) into the solver.
        /// </summary>        
        public void Assert(params BoolExpr[] constraints)
        {
            Contract.Requires(constraints != null);
            Contract.Requires(Contract.ForAll(constraints, c => c != null));

            Context.CheckContextMatch(constraints);
            foreach (BoolExpr a in constraints)
            {
                Native.Z3_solver_assert(Context.nCtx, NativeObject, a.NativeObject);
            }
        }

        /// <summary>
        /// The number of assertions in the solver.
        /// </summary>
        public uint NumAssertions
        {
            get
            {
                ASTVector ass = new ASTVector(Context, Native.Z3_solver_get_assertions(Context.nCtx, NativeObject));
                return ass.Size;
            }
        }

        /// <summary>
        /// The set of asserted formulas.
        /// </summary>
        public BoolExpr[] Assertions
        {
            get
            {
                Contract.Ensures(Contract.Result<BoolExpr[]>() != null);

                ASTVector ass = new ASTVector(Context, Native.Z3_solver_get_assertions(Context.nCtx, NativeObject));
                uint n = ass.Size;
                BoolExpr[] res = new BoolExpr[n];
                for (uint i = 0; i < n; i++)
                    res[i] = new BoolExpr(Context, ass[i].NativeObject);
                return res;
            }
        }

        /// <summary>
        /// Checks whether the assertions in the solver are consistent or not.
        /// </summary>
        /// <remarks>
        /// <seealso cref="Model"/>
        /// <seealso cref="UnsatCore"/>
        /// <seealso cref="Proof"/>    
        /// </remarks>    
        public Status Check(params Expr[] assumptions)
        {
            Z3_lbool r;
            if (assumptions == null)
                r = (Z3_lbool)Native.Z3_solver_check(Context.nCtx, NativeObject);
            else
                r = (Z3_lbool)Native.Z3_solver_check_assumptions(Context.nCtx, NativeObject, (uint)assumptions.Length, AST.ArrayToNative(assumptions));
            switch (r)
            {
                case Z3_lbool.Z3_L_TRUE: return Status.SATISFIABLE;
                case Z3_lbool.Z3_L_FALSE: return Status.UNSATISFIABLE;
                default: return Status.UNKNOWN;
            }
        }

        /// <summary>
        /// The model of the last <c>Check</c>.
        /// </summary>
        /// <remarks>
        /// The result is <c>null</c> if <c>Check</c> was not invoked before,
        /// if its results was not <c>SATISFIABLE</c>, or if model production is not enabled.
        /// </remarks>
        public Model Model
        {
            get
            {
                IntPtr x = Native.Z3_solver_get_model(Context.nCtx, NativeObject);
                if (x == IntPtr.Zero)
                    return null;
                else
                    return new Model(Context, x);
            }
        }

        /// <summary>
        /// The proof of the last <c>Check</c>.
        /// </summary>
        /// <remarks>    
        /// The result is <c>null</c> if <c>Check</c> was not invoked before,
        /// if its results was not <c>UNSATISFIABLE</c>, or if proof production is disabled.
        /// </remarks>
        public Expr Proof
        {
            get
            {
                IntPtr x = Native.Z3_solver_get_proof(Context.nCtx, NativeObject);
                if (x == IntPtr.Zero)
                    return null;
                else
                    return Expr.Create(Context, x);
            }
        }

        /// <summary>
        /// The unsat core of the last <c>Check</c>.
        /// </summary>
        /// <remarks>
        /// The unsat core is a subset of <c>Assertions</c>
        /// The result is empty if <c>Check</c> was not invoked before,
        /// if its results was not <c>UNSATISFIABLE</c>, or if core production is disabled.
        /// </remarks>
        public Expr[] UnsatCore
        {
            get
            {
                Contract.Ensures(Contract.Result<Expr[]>() != null);

                ASTVector core = new ASTVector(Context, Native.Z3_solver_get_unsat_core(Context.nCtx, NativeObject));
                uint n = core.Size;
                Expr[] res = new Expr[n];
                for (uint i = 0; i < n; i++)
                    res[i] = Expr.Create(Context, core[i].NativeObject);
                return res;
            }
        }

        /// <summary>
        /// A brief justification of why the last call to <c>Check</c> returned <c>UNKNOWN</c>.
        /// </summary>
        public string ReasonUnknown
        {
            get
            {
                Contract.Ensures(Contract.Result<string>() != null);

                return Native.Z3_solver_get_reason_unknown(Context.nCtx, NativeObject);
            }
        }

        /// <summary>
        /// Solver statistics.
        /// </summary>
        public Statistics Statistics
        {
            get {
                Contract.Ensures(Contract.Result<Statistics>() != null);

                return new Statistics(Context, Native.Z3_solver_get_statistics(Context.nCtx, NativeObject)); }
        }

        /// <summary>
        /// A string representation of the solver.
        /// </summary>
        public override string ToString()
        {
            return Native.Z3_solver_to_string(Context.nCtx, NativeObject);
        }

        #region Internal
        internal Solver(Context ctx, IntPtr obj)
            : base(ctx, obj)
        {
            Contract.Requires(ctx != null);
        }

        internal class DecRefQueue : Z3.DecRefQueue
        {
            public override void IncRef(Context ctx, IntPtr obj)
            {
                Native.Z3_solver_inc_ref(ctx.nCtx, obj);
            }

            public override void DecRef(Context ctx, IntPtr obj)
            {
                Native.Z3_solver_dec_ref(ctx.nCtx, obj);
            }
        };        

        internal override void IncRef(IntPtr o)
        {
            Context.Solver_DRQ.IncAndClear(Context, o);
            base.IncRef(o);
        }

        internal override void DecRef(IntPtr o)
        {
            Context.Solver_DRQ.Add(o);
            base.DecRef(o);
        }
        #endregion
    }
}
