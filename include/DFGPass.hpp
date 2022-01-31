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
*    Last Modified: 31-01-2022 13:37:46
*/
#ifndef DFGPASS_H
#define DFGPASS_H

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/ADT/SmallVector.h"

#include "CGRAModel.hpp"
#include "CGRADataFlowGraph.hpp"

using namespace llvm;


namespace CGRAOmp {
	/**
	 * @class DFGPassHandler
	 * @brief Module Pass to create data flow graph for all kernels while aplying DFG Optimization Passes
	*/
	class DFGPassHandler : public PassInfoMixin<DFGPassHandler> {
		public:
			PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
		private:
			/**
			 * @brief Create a Data Flow Graphs For All Kernels object
			 * @tparam VerifyPassT VerifyPass type for the target CGRA category
			 * @param F Function including offloading kernels
			 * @param AM AnalysisManager for the function
			 * @return true if generating all graphs without errors
			 * @return otherwise: false
			 */
			template<typename VerifyPassT>
			bool createDataFlowGraphsForAllKernels(Function &F, FunctionAnalysisManager &AM);

			/**
			 * @brief Create a Data Flow Graph object
			 * 
			 * @param F Function including offloading kernels
			 * @param L Kernel looop
			 * @param FAM AnalysisManager for the function
			 * @param LAM AnalysisManager for the loop
			 * @param AR AnalysisResults for the loop
			 * @return true if generating a graph without errors
			 * @return otherwise: false
			 */
			bool createDataFlowGraph(Function &F, Loop &L, FunctionAnalysisManager &FAM,
										LoopAnalysisManager &LAM, 
										LoopStandardAnalysisResults &AR);

			/**
			 * @brief check if the instruction is memory access or not
			 * 
			 * @param I Instruction to be checked
			 * @return true if it is a memory access
			 * @return otherwise: false
			 */
			inline bool isMemAccess(Instruction &I) {
				return isa<LoadInst>(I) || isa<StoreInst>(I);
			}

			/**
			 * @brief create memory access node
			 * 
			 * @param I Instruction for the memory access
			 * @return DFGNode* a pointer to the node
			 */
			inline DFGNode* make_mem_node(Instruction &I) {
				if (isa<LoadInst>(I)) {
					return new MemAccessNode<DFGNode::NodeKind::MemLoad>(node_count++);
				} else {
					return new MemAccessNode<DFGNode::NodeKind::MemStore>(node_count++);
				}
			}
			/**
			 * @brief create computational node
			 * 
			 * @param I Instruction for the computational
			 * @return DFGNode* a pointer to the node
			 */
			inline DFGNode* make_comp_node(InstMapEntry *imap) {
				return new ComputeNode(node_count++, imap);
			}

			/**
			 * @brief create constant node
			 * 
			 * @param I The constant value
			 * @return DFGNode* a pointer to the node
			 */
			inline DFGNode* make_const_node(Constant *C) {
				return new ConstantNode(node_count++, C);
			}

			// /**
			//  * @brief reduce tree height
			//  * 
			//  * @param G a data flow graph
			//  * @return CGRADFG height reduced graph
			//  */
			// CGRADFG reduce_tree_height(CGRADFG G);
			int node_count = 0;
	};

	struct DFGPassConcept {
		virtual ~DFGPassConcept() = default;

		virtual StringRef name() const = 0;

		virtual void run(CGRADFG &G, Loop &L, FunctionAnalysisManager &FAM,
									LoopAnalysisManager &LAM,
									LoopStandardAnalysisResults &AR) = 0;

	};

	template <typename PassT>
	struct DFGPassModel : public DFGPassConcept {
		explicit DFGPassModel(PassT Pass) : Pass(std::move(Pass)) {}
		DFGPassModel(const DFGPassModel &Arg) : Pass(Arg.Pass) {}
		DFGPassModel(DFGPassModel &&Arg) : Pass(std::move(Arg.Pass)) {}

		void run(CGRADFG &G, Loop &L, FunctionAnalysisManager &FAM,
									LoopAnalysisManager &LAM,
									LoopStandardAnalysisResults &AR) override {
			Pass.run(G, L, FAM, LAM, AR);
		};

		StringRef name() const override { return PassT::name(); }

		private:
			PassT Pass;
	};

	class DFGPassManager {
		public:
			template <typename PassT>
			void addPass(PassT Pass) {
				pipeline.emplace_back(new DFGPassModel(std::move(Pass)));
				errs() << "Pushed back " << pipeline.size() << "\n";
			}
			void run(CGRADFG &G, Loop &L, FunctionAnalysisManager &FAM,
									LoopAnalysisManager &LAM,
									LoopStandardAnalysisResults &AR);
		private:
			SmallVector<DFGPassConcept*> pipeline;
	};

	class DFGPassBuilder {
		public:
			using CallBackT = std::function<bool(StringRef, DFGPassManager &)>;
			DFGPassBuilder();
			void registerPipelineParsingCallback(CallBackT C);
			Error parsePassPipeline(DFGPassManager &DPM, ArrayRef<std::string> PipelineText);

		private:
			SmallVector<CallBackT> callback_list;
			Error search_callback();
	};

	struct DFGPassPluginLibraryInfo {
		/// A meaningful name of the plugin.
		const char *PluginName;
		/// The version of the plugin.
		void (*RegisterPassBuilderCallbacks)(DFGPassBuilder &);
	};

}

#endif //DFGPASS_H