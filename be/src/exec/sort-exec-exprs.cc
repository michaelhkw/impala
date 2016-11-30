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

#include "exec/sort-exec-exprs.h"
#include "exprs/expr-context.h"

#include "common/names.h"

namespace impala {

Status SortExecExprs::Init(const TSortInfo& sort_info, ObjectPool* pool) {
  return Init(sort_info.ordering_exprs,
      sort_info.__isset.sort_tuple_slot_exprs ? &sort_info.sort_tuple_slot_exprs : NULL,
      pool);
}

Status SortExecExprs::Init(const vector<TExpr>& thrift_ordering_exprs,
    const vector<TExpr>* thrift_sort_tuple_slot_exprs, ObjectPool* pool) {
  vector<Expr*> lhs_ordering_exprs;
  RETURN_IF_ERROR(
      Expr::CreateExprTrees(pool, thrift_ordering_exprs, &lhs_ordering_exprs));
  ExprContext::Create(pool, lhs_ordering_exprs, &lhs_ordering_expr_ctxs_);

  if (thrift_sort_tuple_slot_exprs != NULL) {
    materialize_tuple_ = true;
    vector<Expr*> sort_tuple_slot_exprs;
    RETURN_IF_ERROR(Expr::CreateExprTrees(pool, *thrift_sort_tuple_slot_exprs,
        &sort_tuple_slot_exprs));
    ExprContext::Create(pool, sort_tuple_slot_exprs, &sort_tuple_slot_expr_ctxs_);
  } else {
    materialize_tuple_ = false;
  }
  return Status::OK();
}

Status SortExecExprs::Init(const vector<ExprContext*>& lhs_ordering_expr_ctxs,
                           const vector<ExprContext*>& rhs_ordering_expr_ctxs) {
  lhs_ordering_expr_ctxs_ = lhs_ordering_expr_ctxs;
  rhs_ordering_expr_ctxs_ = rhs_ordering_expr_ctxs;
  return Status::OK();
}

Status SortExecExprs::Prepare(RuntimeState* state, const RowDescriptor& child_row_desc,
    const RowDescriptor& output_row_desc, MemTracker* expr_mem_tracker) {
  if (materialize_tuple_) {
    RETURN_IF_ERROR(ExprContext::Prepare(sort_tuple_slot_expr_ctxs_, state,
        child_row_desc, expr_mem_tracker));
  }
  RETURN_IF_ERROR(ExprContext::Prepare(lhs_ordering_expr_ctxs_, state,
      output_row_desc, expr_mem_tracker));
  return Status::OK();
}

Status SortExecExprs::Open(RuntimeState* state) {
  if (materialize_tuple_) {
    RETURN_IF_ERROR(ExprContext::Open(sort_tuple_slot_expr_ctxs_, state));
  }
  RETURN_IF_ERROR(ExprContext::Open(lhs_ordering_expr_ctxs_, state));
  RETURN_IF_ERROR(ExprContext::CloneIfNotExists(
      lhs_ordering_expr_ctxs_, state, &rhs_ordering_expr_ctxs_));
  return Status::OK();
}

void SortExecExprs::Close(RuntimeState* state) {
  if (materialize_tuple_) {
    ExprContext::Close(sort_tuple_slot_expr_ctxs_, state);
  }
  ExprContext::Close(rhs_ordering_expr_ctxs_, state);
  ExprContext::Close(lhs_ordering_expr_ctxs_, state);
}

} //namespace impala
