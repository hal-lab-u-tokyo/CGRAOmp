#!/usr/bin/env python3
# -*- coding:utf-8 -*-

###
#   MIT License
#   
#   Copyright (c) 2022 Amano laboratory, Keio University & Processor Research Team, RIKEN Center for Computational Science
#   
#   Permission is hereby granted, free of charge, to any person obtaining a copy of
#   this software and associated documentation files (the "Software"), to deal in
#   the Software without restriction, including without limitation the rights to
#   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
#   of the Software, and to permit persons to whom the Software is furnished to do
#   so, subject to the following conditions:
#   
#   The above copyright notice and this permission notice shall be included in all
#   copies or substantial portions of the Software.
#   
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#   SOFTWARE.
#   
#   File:          /scripts/cc_config/Config.py
#   Project:       CGRAOmp
#   Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
#   Created Date:  09-02-2022 10:12:26
#   Last Modified: 07-07-2022 22:32:56
###

TARGET_FMT = "{target}-unknown-linux-gnu"
OMP_TARGET_FMT = "openmp-{target}"
RED_STR = "\x1b[31m{0}\x1b[39m"
MAGENTA_STR = "\x1b[35m{0}\x1b[39m"
GREEN_STR = "\x1b[32m{0}\x1b[39m"
BOLD_STR = "\x1b[1m{0}\x1b[0m"

WARNING_STR = MAGENTA_STR.format(BOLD_STR.format("Warning"))
ERROR_STR = RED_STR.format(BOLD_STR.format("Error"))

OPT_FLAGS = {"-O0", "-O1", "-O2", "-O3", "-Os", "-Oz"}

# pipeline settings for O2 in clang
CLANG_O2_FLAGS = []
# 1st stage
CLANG_O2_FLAGS.append(["-tti", "-tbaa", "-scoped-noalias-aa", "-assumption-cache-tracker", "-targetlibinfo", "-verify", "-ee-instrument", "-simplifycfg", "-domtree", "-sroa", "-early-cse", "-lower-expect"])
# 2nd stage
CLANG_O2_FLAGS.append(["-targetlibinfo", "-tti", "-tbaa", "-scoped-noalias-aa", "-assumption-cache-tracker", "-profile-summary-info", "-annotation2metadata", "-forceattrs", "-inferattrs", "-ipsccp", "-called-value-propagation", "-globalopt", "-domtree", "-mem2reg", "-deadargelim", "-domtree", "-basic-aa", "-aa", "-loops", "-lazy-branch-prob", "-lazy-block-freq", "-opt-remark-emitter", "-instcombine", "-simplifycfg", "-basiccg", "-globals-aa", "-prune-eh", "-inline", "-openmpopt", "-function-attrs", "-domtree", "-sroa", "-basic-aa", "-aa", "-memoryssa", "-early-cse-memssa", "-speculative-execution", "-aa", "-lazy-value-info", "-jump-threading", "-correlated-propagation", "-simplifycfg", "-domtree", "-basic-aa", "-aa", "-loops", "-lazy-branch-prob", "-lazy-block-freq", "-opt-remark-emitter", "-instcombine", "-libcalls-shrinkwrap", "-loops", "-postdomtree", "-branch-prob", "-block-freq", "-lazy-branch-prob", "-lazy-block-freq", "-opt-remark-emitter", "-pgo-memop-opt", "-basic-aa", "-aa", "-loops", "-lazy-branch-prob", "-lazy-block-freq", "-opt-remark-emitter", "-tailcallelim", "-simplifycfg", "-reassociate", "-domtree", "-loops", "-loop-simplify", "-lcssa-verification", "-lcssa", "-basic-aa", "-aa", "-scalar-evolution", "-loop-rotate", "-memoryssa", "-lazy-branch-prob", "-lazy-block-freq", "-licm", "-loop-unswitch", "-simplifycfg", "-domtree", "-basic-aa", "-aa", "-loops", "-lazy-branch-prob", "-lazy-block-freq", "-opt-remark-emitter", "-instcombine", "-loop-simplify", "-lcssa-verification", "-lcssa", "-scalar-evolution", "-loop-idiom", "-indvars", "-loop-deletion", "-loop-unroll", "-sroa", "-aa", "-mldst-motion", "-phi-values", "-aa", "-memdep", "-lazy-branch-prob", "-lazy-block-freq", "-opt-remark-emitter", "-gvn", "-phi-values", "-basic-aa", "-aa", "-memdep", "-memcpyopt", "-sccp", "-demanded-bits", "-bdce", "-aa", "-lazy-branch-prob", "-lazy-block-freq", "-opt-remark-emitter", "-instcombine", "-lazy-value-info", "-jump-threading", "-correlated-propagation", "-postdomtree", "-adce", "-basic-aa", "-aa", "-memoryssa", "-dse", "-loops", "-loop-simplify", "-lcssa-verification", "-lcssa", "-aa", "-scalar-evolution", "-lazy-branch-prob", "-lazy-block-freq", "-licm", "-simplifycfg", "-domtree", "-basic-aa", "-aa", "-loops", "-lazy-branch-prob", "-lazy-block-freq", "-opt-remark-emitter", "-instcombine", "-barrier", "-elim-avail-extern", "-basiccg", "-rpo-function-attrs", "-globalopt", "-globaldce", "-basiccg", "-globals-aa", "-domtree", "-float2int", "-lower-constant-intrinsics", "-domtree", "-loops", "-loop-simplify", "-lcssa-verification", "-lcssa", "-basic-aa", "-aa", "-scalar-evolution", "-loop-rotate", "-loop-accesses", "-lazy-branch-prob", "-lazy-block-freq", "-opt-remark-emitter", "-loop-distribute", "-postdomtree", "-branch-prob", "-block-freq", "-scalar-evolution", "-basic-aa", "-aa", "-loop-accesses", "-demanded-bits", "-lazy-branch-prob", "-lazy-block-freq", "-opt-remark-emitter", "-inject-tli-mappings", "-loop-vectorize", "-loop-simplify", "-scalar-evolution", "-aa", "-loop-accesses", "-lazy-branch-prob", "-lazy-block-freq", "-loop-load-elim", "-basic-aa", "-aa", "-lazy-branch-prob", "-lazy-block-freq", "-opt-remark-emitter", "-instcombine", "-simplifycfg", "-domtree", "-loops", "-scalar-evolution", "-basic-aa", "-aa", "-demanded-bits", "-lazy-branch-prob", "-lazy-block-freq", "-opt-remark-emitter", "-inject-tli-mappings", "-slp-vectorizer", "-vector-combine", "-opt-remark-emitter", "-instcombine", "-loop-simplify", "-lcssa-verification", "-lcssa", "-scalar-evolution", "-loop-unroll", "-lazy-branch-prob", "-lazy-block-freq", "-opt-remark-emitter", "-instcombine", "-memoryssa", "-loop-simplify", "-lcssa-verification", "-lcssa", "-scalar-evolution", "-lazy-branch-prob", "-lazy-block-freq", "-licm", "-opt-remark-emitter", "-transform-warning", "-alignment-from-assumptions", "-strip-dead-prototypes", "-globaldce", "-constmerge", "-cg-profile", "-domtree", "-loops", "-postdomtree", "-branch-prob", "-block-freq", "-loop-simplify", "-lcssa-verification", "-lcssa", "-basic-aa", "-aa", "-scalar-evolution", "-block-freq", "-loop-sink", "-lazy-branch-prob", "-lazy-block-freq", "-opt-remark-emitter", "-instsimplify", "-div-rem-pairs", "-simplifycfg", "-annotation-remarks", "-verify"])

# default setting for pre optimization when -O0 is specified
# in this case, only loop canonicalization will be applied
PRE_OPTS_O0 = [["-polly-canonicalize"]]

# default setting for pre optimization when -O1 or higher level is specified
PRE_OPTS = []
PRE_OPTS.append(["--inferattrs", "--indvars", "--indvars-widen-indvars", "--aa-pipeline=\"basic-aa,scoped-noalias-aa,tbaa,globals-aa,scev-aa\"", "-loop-unroll", "--unroll-allow-partial", "-simplifycfg", "-loop-simplify", "-loop-idiom", "-loop-instsimplify", "-loop-rotate", "-mem2reg", "-instcombine", "-loop-load-elim", "-instsimplify", "--early-cse", "--early-cse-memssa", "-dce",  "--scalar-evolution", "-memoryssa", "-gvn", "-constmerge", "-simplifycfg", "-reassociate", "-instcombine", "-mldst-motion", "-polly-canonicalize"])

