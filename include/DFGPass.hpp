/*
*    MIT License
*    
*    Copyright (c) 2021 Amano laboratory, Keio University & Processor Research Team, RIKEN Center for Computational Science
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
*    File:          /include/DFGPass.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  15-12-2021 09:59:52
*    Last Modified: 15-12-2021 17:39:58
*/
#ifndef DFGPASS_H
#define DFGPASS_H

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "CGRAModel.hpp"
#include "CGRADataFlowGraph.hpp"

using namespace llvm;


namespace CGRAOmp {
	class DFGPass : public PassInfoMixin<DFGPass> {
		public:
			PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
		private:
			template<typename VerifyPassT>
			bool createDataFlowGraphsForAllKernels(Function &F, FunctionAnalysisManager &AM);
			bool createDataFlowGraph(Function &F, Loop &L, FunctionAnalysisManager &FAM,
										LoopAnalysisManager &LAM, 
										LoopStandardAnalysisResults &AR);
			inline bool isMemAccess(Instruction &I) {
				return isa<LoadInst>(I) || isa<StoreInst>(I);
			}
			inline DFGNode* make_mem_node(Instruction &I) {
				if (isa<LoadInst>(I)) {
					return new MemAccessNode<DFGNode::NodeKind::MemLoad>(node_count++);
				} else {
					return new MemAccessNode<DFGNode::NodeKind::MemStore>(node_count++);
				}
			}
			inline DFGNode* make_comp_node(InstMapEntry *imap) {
				return new ComputeNode(node_count++, imap);
			}
			inline DFGNode* make_const_node(Constant *C) {
				return new ConstantNode(node_count++, C);
			}

			int node_count = 0;
	};

}

#endif //DFGPASS_H