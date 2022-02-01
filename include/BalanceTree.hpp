/*
*    MIT License
*    
*    Copyright (c) 2022 Amano laboratory, Keio University & Processor Research Team, RIKEN Center for Computational Science
*    
*    Permission is hereby granted, free of charge, to any person obtaining a copy of
*    this software and associated documentation files (the "Software"), to deal in
*    the Software without restriction, including without limitation the rights to
*    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
*    of the Software, and to permit persons to whom the Software is furnished to do
*    so, subject to the following conditions:
*    
*    The above copyright notice and this permission notice shall be included in all
*    copies or substantial portions of the Software.
*    
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*    SOFTWARE.
*    
*    File:          /include/BalanceTree.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  01-02-2022 11:45:50
*    Last Modified: 01-02-2022 19:55:52
*/

#ifndef BALANCETREE_H
#define BALANCETREE_H

#include "DFGPass.hpp"
#include "CGRADataFlowGraph.hpp"

#include "llvm/IR/PassManager.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/IndexedMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Instruction.h"

#define PREC_PAIR(OP, LEVEL) std::make_pair(Instruction::OP, LEVEL)

using namespace llvm;

namespace CGRAOmp
{

	/**
	 * @class BalanceTree
	 * @brief A DFGPass to balance the tree structure
	 * @details 
	 * This optimization is based on algorithm proposed in [1].
	 * @see [1] Coons, Katherine Elizabeth, et al. Optimal huffman tree-height reduction for instruction-level parallelism. Computer Science Department, University of Texas at Austin, 2008.
	 */
	class BalanceTree : public PassInfoMixin<BalanceTree> {
		public:
			void run(CGRADFG &G, Loop &L, FunctionAnalysisManager &FAM,
										LoopAnalysisManager &LAM,
										LoopStandardAnalysisResults &AR);
		private:
			using EdgeListTy = SmallVector<DFGEdge *, 10U>;
			void initWeight(CGRADFG &G);
			SmallVector<ComputeNode*> findRootCandidates(CGRADFG &G);
			void toBalanced(CGRADFG &G, ComputeNode* Root);

			DenseMap<DFGNode*,int> weight;
			IndexedMap<bool> visited;
			SmallPtrSet<DFGNode*, 10> candidate_set;

			static std::map<int,int> OperatorPrecedence;
			static int getOperatorPrecedence(ComputeNode* N) {
				return OperatorPrecedence[N->getInst()->getOpcode()];
			}
	};
}

#endif //BALANCETREE_H