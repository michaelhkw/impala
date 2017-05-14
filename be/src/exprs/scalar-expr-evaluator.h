// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef IMPALA_EXPRS_SCALAR_EXPR_EVALUATOR_H
#define IMPALA_EXPRS_SCALAR_EXPR_EVALUATOR_H

#include <boost/scoped_ptr.hpp>

#include "common/object-pool.h"
#include "common/status.h"
#include "exprs/expr-value.h"
#include "udf/udf-internal.h" // for CollectionVal
#include "udf/udf.h"

using namespace impala_udf;

namespace impala {

class MemPool;
class RuntimeState;
class ScalarExpr;
class Status;
class TupleRow;

/// ScalarExprEvaluator is the interface for evaluating a scalar expression. It holds a
/// reference to the root of a ScalarExpr tree, runtime state (e.g. FunctionContexts)
/// needed during evaluation and also a buffer for the expression evaluation result.
/// A single evaluator is not thread-safe. It implements Get*Val() interfaces for every
/// possible return types and drives the execution by calling the Get*Val() function of
/// the root ScalarExpr with the input tuple row.
///
/// A ScalarExprEvaluator is created using the Create() interface. It must be initialized
/// by calling Open() before use and Close() must also be called to free up resources
/// owned by the evaluator.
///
/// FunctionContext is the interface for Impala to communicate with built-in functions,
/// UDF and UDAF. It is passed to UDF/UDAF to store its thread-private states, propagate
/// errors and allocate memory. An evaluator contains a vector of FunctionContext for
/// the ScalarExpr nodes in the Expr tree. The index of each node's entry is defined in
/// the its 'fn_ctx_idx_' field. The range in the vector for the sub-expression tree
/// rooted at a node is defined by [fn_ctx_idx_start_, fn_ctx_idx_end_).
///
class ScalarExprEvaluator {
 public:
  ~ScalarExprEvaluator();

  /// Creates an evaluator for the scalar expression tree rooted at 'expr' and all
  /// FunctionContexts needed during evaluation. Allocations from this evaluator will
  /// be from 'mem_pool'. The newly created evaluator will be stored in 'pool' and
  /// returned in 'evaluator'. Returns error status on failure.
  static Status Create(const ScalarExpr& expr, RuntimeState* state, ObjectPool* pool,
      MemPool* mem_pool, ScalarExprEvaluator** evaluator) WARN_UNUSED_RESULT;

  /// Convenience function for creating multiple ScalarExprEvaluators. The evaluators
  /// are returned in 'evaluators'.
  static Status Create(const std::vector<ScalarExpr*>& exprs, RuntimeState* state,
      ObjectPool* pool, MemPool* mem_pool, std::vector<ScalarExprEvaluator*>* evaluators)
      WARN_UNUSED_RESULT;

  /// Initializes the ScalarExprEvaluator on all nodes in the ScalarExpr tree. This is
  /// also the location in which constant arguments to functions are computed. Does not
  /// need to be called on clones. Idempotent (this allows exprs to be opened multiple
  /// times in subplans without reinitializing function states).
  Status Open(RuntimeState* state) WARN_UNUSED_RESULT;

  /// Convenience function for opening multiple ScalarExprEvaluators.
  static Status Open(const std::vector<ScalarExprEvaluator*>& evaluators,
      RuntimeState* state) WARN_UNUSED_RESULT;

  /// Free resources held by this evaluator. Must be called on every ScalarExprEvaluator,
  /// including clones. Has no effect if already closed.
  void Close(RuntimeState* state);

  /// Convenience function for closing multiple ScalarExprEvaluators.
  static void Close(const std::vector<ScalarExprEvaluator*>& evaluators,
      RuntimeState* state);

  /// Creates a copy of this ScalarExprEvaluator. Open() must be called first. The copy
  /// contains clones of each FunctionContext, which share the fragment-local state of the
  /// original one but have their own FreePool and thread-local state. This should be used
  /// to create an ScalarExprEvaluator for each execution thread that needs to evaluate
  /// 'root_'. All allocations will be from 'mem_pool' so callers should use different
  /// MemPool for evaluators in different threads. Note that clones are considered opened.
  /// The cloned ScalarExprEvaluator cannot be used after the original ScalarExprEvaluator
  /// is destroyed because it may reference fragment-local state from the original.
  /// TODO: IMPALA-4743: Evaluate input arguments in ScalarExpr::Init() and store them
  /// in ScalarExpr.
  Status Clone(ObjectPool* pool, RuntimeState* state, MemPool* mem_pool,
      ScalarExprEvaluator** new_evaluator) const WARN_UNUSED_RESULT;

  /// Convenience functions for cloning multiple ScalarExprEvaluators. The newly
  /// created evaluators are appended to 'new_evaluators.
  static Status Clone(ObjectPool* pool, RuntimeState* state, MemPool* mem_pool,
      const std::vector<ScalarExprEvaluator*>& evaluators,
      std::vector<ScalarExprEvaluator*>* new_evaluators) WARN_UNUSED_RESULT;

  /// If 'expr' is constant, evaluates it with no input row argument and returns the
  /// result in 'const_val'. Sets 'const_val' to NULL if the argument is not constant.
  /// The returned AnyVal and associated varlen data is owned by this evaluator. This
  /// should only be called after Open() has been called on this expr. Returns an error
  /// if there was an error evaluating the expression or if memory could not be allocated
  /// for the expression result.
  Status GetConstValue(
      RuntimeState* state, const ScalarExpr& expr, AnyVal** const_val) WARN_UNUSED_RESULT;

  /// Calls the appropriate Get*Val() function on 'e' and stores the result in result_.
  /// This is used by ScalarExpr to call GetValue() on sub-expression, rather than root_.
  void* GetValue(const ScalarExpr& e, const TupleRow* row);

  /// Calls the appropriate Get*Val() function on this evaluator's root_ expr tree, stores
  /// the result in 'result_' and returns a pointer to it.
  void* GetValue(const TupleRow* row);

  /// Evaluates the expression of this evaluator on tuple row 'row' and returns
  /// the results. One function for each data type implemented.
  BooleanVal GetBooleanVal(TupleRow* row);
  TinyIntVal GetTinyIntVal(TupleRow* row);
  SmallIntVal GetSmallIntVal(TupleRow* row);
  IntVal GetIntVal(TupleRow* row);
  BigIntVal GetBigIntVal(TupleRow* row);
  FloatVal GetFloatVal(TupleRow* row);
  DoubleVal GetDoubleVal(TupleRow* row);
  StringVal GetStringVal(TupleRow* row);
  CollectionVal GetCollectionVal(TupleRow* row);
  TimestampVal GetTimestampVal(TupleRow* row);
  DecimalVal GetDecimalVal(TupleRow* row);

  /// Returns an error status if there was any error in evaluating the expression
  /// or its sub-expressions.
  Status GetError(int start_idx = 0, int end_idx = -1) const WARN_UNUSED_RESULT;

  /// Convenience functions: print value into 'str' or 'stream'.  NULL turns into "NULL".
  void PrintValue(const TupleRow* row, std::string* str);
  void PrintValue(void* value, std::string* str);
  void PrintValue(void* value, std::stringstream* stream);
  void PrintValue(const TupleRow* row, std::stringstream* stream);

  /// Returns true if any of the expression contexts in the array has local allocations.
  /// The last two are helper functions.
  static bool HasLocalAllocations(const std::vector<ScalarExprEvaluator*>& evaluators);
  bool HasLocalAllocations() const;

  /// Frees all local allocations made by fn_ctxs_. This can be called when result
  /// data from this context is no longer needed. The last two are helper functions.
  void FreeLocalAllocations();
  static void FreeLocalAllocations(const std::vector<ScalarExprEvaluator*>& evaluators);

  /// Get the number of digits after the decimal that should be displayed for this value.
  /// Returns -1 if no scale has been specified (currently the scale is only set for
  /// doubles set by RoundUpTo). GetValue() must have already been called.
  /// TODO: remove this (IMPALA-4720).
  int output_scale() const { return output_scale_; }
  const ScalarExpr& root() const { return root_; }
  bool opened() const { return opened_; }
  bool closed() const { return closed_; }
  bool is_clone() const { return is_clone_; }
  MemPool* mem_pool() const { return mem_pool_; }

  /// The builtin functions are not called from anywhere in the code and the
  /// symbols are therefore not included in the binary. We call these functions
  /// by using dlsym. The compiler must think this function is callable to
  /// not strip these symbols.
  static void InitBuiltinsDummy();

  static const char* LLVM_CLASS_NAME;

 private:
  friend class ScalarExpr;
  /// Users of private GetValue() or 'pool_'.
  friend class CaseExpr;
  friend class HiveUdfCall;
  friend class ScalarFnCall;

  /// FunctionContexts for nodes in this Expr tree. Created by this ScalarExprEvaluator
  /// and live in the same object pool as this evaluator (i.e. same life span as the
  /// evaluator).
  std::vector<FunctionContext*> fn_ctxs_;

  /// Array access to fn_ctxs_. Used by ScalarFnCall's codegend compute function
  /// to access the correct FunctionContext.
  FunctionContext** fn_ctxs_ptr_ = nullptr;

  /// Pointer to the MemPool which all allocations (including fn_ctxs_') come from.
  /// Owned by the exec node which owns this evaluator.
  MemPool* mem_pool_;

  /// The expr tree which this evaluator is for.
  const ScalarExpr& root_;

  /// Stores the evaluation for this expr tree. This is used in interpreted path when
  /// we need to return a void*.
  ExprValue result_;

  /// True if this evaluator came from a Clone() call. Used to manage FunctionStateScope.
  bool is_clone_ = false;

  /// Variables keeping track of current state.
  bool initialized_ = false;
  bool opened_ = false;
  bool closed_ = false;

  /// The number of digits after the decimal that should be displayed for this value.
  /// -1 if no scale has been specified (currently the scale is only set for doubles
  /// set by RoundUpTo). This value relies on FunctionContext to be allocated first
  /// before it's derived so it lives in the evaluator instead of Expr.
  /// TODO: move this to Expr initialization after IMPALA-4743 is fixed.
  int output_scale_ = -1;

  ScalarExprEvaluator(const ScalarExpr& root, MemPool* mem_pool);

  /// Retrieves a registered FunctionContext. 'i' is the 'fn_context_index_' of the
  /// corresponding sub-expression in the Expr tree.
  FunctionContext* fn_context(int i) {
    DCHECK_GE(i, 0);
    DCHECK_LT(i, fn_ctxs_.size());
    return fn_ctxs_[i];
  }

  /// Walks the expression tree 'expr' and fills in 'fn_ctxs_' for all Expr nodes
  /// which need FunctionContext.
  void CreateFnCtxs(RuntimeState* state, const ScalarExpr& expr);
};

}

#endif