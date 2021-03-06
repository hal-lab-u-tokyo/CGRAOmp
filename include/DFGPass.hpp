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
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  15-12-2021 09:59:52
*    Last Modified: 12-07-2022 18:22:30
*/
#ifndef DFGPASS_H
#define DFGPASS_H

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/iterator_range.h"

#include "CGRAModel.hpp"
#include "CGRADataFlowGraph.hpp"

using namespace llvm;


namespace CGRAOmp {

	/**
	 * @class DFGPassConcept
	 * @brief The abstract class of the DFG pass manager interfaces
	*/
	struct DFGPassConcept {
		virtual ~DFGPassConcept() = default;

		/**
		 * @brief  Polymorphic method to access the name of a pass
		 * @return StringRef: the name of the actual pass
		 */
		virtual StringRef name() const = 0;

		/**
		 * @brief The polymorphic API which runs the pass over a given DFG
		 * 
		 * @param G Data flow graph (DFG)
		 * @param L Loop associated with the DFGs
		 * @param FAM FunctionAnalysisManager to access analysis results
		 * @param LAM LoopAnalysisManager to access analysis results
		 * @param AR LoopStandardAnalysisResults
		 * @return It returns true if DFG G is changed
		 * @return Otherwise, it returns false
		 */
		virtual bool run(CGRADFG &G, Loop &L, FunctionAnalysisManager &FAM,
									LoopAnalysisManager &LAM,
									LoopStandardAnalysisResults &AR) = 0;

	};

	/**
	 * @class DFGPassModel
	 * @brief A template wrapper used to implement the polymorphic API
	 * @tparam PassT user defined DFG Pass type
	 */
	template <typename PassT>
	struct DFGPassModel : public DFGPassConcept {
		/// default constructor
		explicit DFGPassModel(PassT Pass) : Pass(std::move(Pass)) {}
		/// copy constructor
		DFGPassModel(const DFGPassModel &Arg) : Pass(Arg.Pass) {}
		/// move constructor
		DFGPassModel(DFGPassModel &&Arg) : Pass(std::move(Arg.Pass)) {}

		/// Wrapper to run the implemented optimization function
		bool run(CGRADFG &G, Loop &L, FunctionAnalysisManager &FAM,
									LoopAnalysisManager &LAM,
									LoopStandardAnalysisResults &AR) override {
			return Pass.run(G, L, FAM, LAM, AR);
		};

		/// Wrapper to get the name
		StringRef name() const override { return PassT::name(); }

		private:
			PassT Pass;
	};

	/**
	 * @class DFGPassManager
	 * @brief Manages a sequence of passes over a DFG
	*/
	class DFGPassManager {
		public:
			/**
			 * @brief Adding a pass to the DFG pass manager
			 * @tparam PassT user defined DFG Pass type
			 * @param Pass pass to be added
			 */
			template <typename PassT>
			void addPass(PassT Pass) {
				pipeline.emplace_back(new DFGPassModel(std::move(Pass)));
			}

			/**
			 * @brief Running all DFG Passes in order of registration to the pipeline
			 * 
			 * @param G Data flow graph (DFG)
			 * @param L Loop associated with the DFGs
			 * @param FAM FunctionAnalysisManager to access analysis results
			 * @param LAM LoopAnalysisManager to access analysis results
			 * @param AR LoopStandardAnalysisResults
			 * @return It returns true if DFG G is changed
			 * @return Otherwise, it returns false
			 */
			bool run(CGRADFG &G, Loop &L, FunctionAnalysisManager &FAM,
									LoopAnalysisManager &LAM,
									LoopStandardAnalysisResults &AR);
		private:
			SmallVector<DFGPassConcept*> pipeline;
	};

	/**
	 * @class DFGPassBuilder
	 * @brief A class to access the DFG Passes
	*/
	class DFGPassBuilder {
		public:
			using CallBackT = std::function<bool(StringRef, DFGPassManager &)>;
			DFGPassBuilder();
			/**
			 * @brief Register a call back function to parse the pass pipeline
			 * @param C Call back function
			 */
			void registerPipelineParsingCallback(const CallBackT &C);

			/**
			 * @brief Parsing the pipeline setting for DFG optimization
			 * @param DPM DFGPassManager
			 * @param PipelineText a list of name of passes to be applied
			 * @return Error is retured if a name of pass does not match any registered passes
			 */
			Error parsePassPipeline(DFGPassManager &DPM, ArrayRef<std::string> PipelineText);

		private:
			SmallVector<CallBackT> callback_list;

			/**
			 * @brief Looking up call back function in the dynamic libraries 
			 * @return Error in either case, a failure in loading the dynmic lib or no call back function named @em getDFGPassPluginInfo
			 */
			Error search_callback();
	};

	/**
	 * @struct DFGPassPluginLibraryInfo
	 * @brief A bundled information about the DFG Pass
	 */
	struct DFGPassPluginLibraryInfo {
		/// A meaningful name of the plugin.
		const char *PluginName;
		/// The version of the plugin.
		void (*RegisterPassBuilderCallbacks)(DFGPassBuilder &);
	};

	/**
	 * @class DFGPassHandler
	 * @brief Module Pass to create data flow graph for all kernels while aplying DFG Optimization Passes
	*/
	class DFGPassHandler : public PassInfoMixin<DFGPassHandler> {
		public:
			using GraphList = SmallVector<CGRADFG*>;
			using graph_iterator = GraphList::iterator;

			/// Default constructor
			DFGPassHandler();
			~DFGPassHandler() {
				delete DPB;
				delete DPM;
			}
			/// Move constractor
			DFGPassHandler(DFGPassHandler &&P) : PassInfoMixin<DFGPassHandler>(std::move(P)),
				DPB(P.DPB), DPM(P.DPM) {
				P.DPB = nullptr;
				P.DPM = nullptr;
			};

			PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
			
			void addGraph(CGRADFG *G) {
				graph_list.emplace_back(G);
			}

			graph_iterator removeGraph(CGRADFG *G) {
				graph_iterator it;
				//find graph
				for (it = graph_begin(); it != graph_end(); it++) {
					if (*it == G) {
						break;
					}
				}
				if (it != graph_end()) {
					return graph_list.erase(it);
				}
				return it;
			}

			graph_iterator graph_begin() {
				return graph_list.begin();
			}

			graph_iterator graph_end() {
				return graph_list.end();
			}


			iterator_range<graph_iterator> graphs() {
				return make_range(graph_begin(), graph_end());
			}

		private:
			/**
			 * @brief Create a Data Flow Graphs For All Kernels object
			 * @tparam VerifyPassT VerifyPass type for the target CGRA category
			 * @param F Function including offloading kernels
			 * @param AM AnalysisManager for the function
			 */
			template<typename VerifyPassT>
			void createDataFlowGraphsForAllKernels(Function &F, FunctionAnalysisManager &AM);

			/**
			 * @brief Create a Data Flow Graph object
			 * 
			 * @param F Function including offloading kernels
			 * @param L Kernel looop
			 * @param FAM AnalysisManager for the function
			 * @param LAM AnalysisManager for the loop
			 * @param AR AnalysisResults for the loop
			 */
			template<typename VerifyPassT>
			void createDataFlowGraph(Function &F, Loop &L, FunctionAnalysisManager &FAM,
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
				if (auto load = dyn_cast<LoadInst>(&I)) {
					return new MemAccessNode(load);
				} else if (auto store = dyn_cast<StoreInst>(&I)) {
					return new MemAccessNode(store);
				} else {
					assert(false && "Non-memory access instruction");
				}
			}
			/**
			 * @brief create computational node
			 * 
			 * @param I Instruction for the computational
			 * @return DFGNode* a pointer to the node
			 */
			inline DFGNode* make_comp_node(Instruction *inst, std::string opcode) {
				return new ComputeNode(inst, opcode);
			}

			/**
			 * @brief create constant node
			 * 
			 * @param V Value corresponding the constant
			 * @return DFGNode* a pointer to the node
			 */
			inline DFGNode* make_const_node(Value *V) {
				return new ConstantNode(V);
			}

			/**
			 * @brief create constant node with different data source
			 * 
			 * @param V Value corresponding the constant
			 * @param seq A sequence of skipped node
			 * @return DFGNode* a pointer to the node
			 */
			inline DFGNode* make_const_node(Value *V, SmallVector<Value*>* seq) {
				return new ConstantNode(V, seq);
			}

			/**
			 * @brief create global data node
			 * 
			 * @param V Value corresponding the global value
			 * @return DFGNode* a pointer to the node
			 */
			inline DFGNode* make_global_node(Value *V) {
				return new GlobalDataNode(V);
			}

			/**
			 * @brief create global data node with different data source
			 * 
			 * @param V Value corresponding the global value
			 * @param seq A sequence of skipped node
			 * @return DFGNode* a pointer to the node
			 */
			inline DFGNode* make_global_node(Value *V, SmallVector<Value*>* seq) {
				return new GlobalDataNode(V, seq);
			}

			DFGPassBuilder *DPB;
			DFGPassManager *DPM;
			SmallVector<CGRADFG*> graph_list;


	};

}

#endif //DFGPASS_H