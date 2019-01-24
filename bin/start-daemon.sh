#!/bin/bash
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

# Runs the provided command line with the required environment variables for
# the various daemon processes - impalad, catalogd, statestored. Used to
# start up minicluster daemon processes.

set -euo pipefail
# If Kerberized, source appropriate vars.
if ${CLUSTER_DIR}/admin is_kerberized; then
  . ${MINIKDC_ENV}
fi

. ${IMPALA_HOME}/bin/set-classpath.sh
# LLVM must be on path to symbolise sanitiser stack traces.
export PATH="${IMPALA_TOOLCHAIN}/llvm-${IMPALA_LLVM_VERSION}/bin:${PATH}"
"$@"
