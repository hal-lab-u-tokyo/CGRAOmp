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
*    File:          /include/DecoupledAnalysis.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  14-12-2021 11:36:50
*    Last Modified: 18-02-2022 03:08:00
*/
#ifndef DecoupledAnalysis_H
#define DecoupledAnalysis_H

#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"

using namespace llvm;


namespace CGRAOmp {

	/**
	 * @class DecoupledAnalysis
	 * @brief An analysis result of memory access decoupling for loop kernel
	*/
	class DecoupledAnalysis {
		public:
			DecoupledAnalysis() {};
			~DecoupledAnalysis() {};


			using MemLoadList = SmallVector<LoadInst*>;
			using MemStoreList = SmallVector<StoreInst*>;
			using CompList = SmallVector<User*>;
			using InvarList = SmallVector<Value*>;

			using load_iterator = MemLoadList::iterator;
			using store_iterator = MemStoreList::iterator;
			using comp_iterator = CompList::iterator;
			using invar_iterator = InvarList::iterator;


			void setMemLoad(MemLoadList &&l) {
				mem_load = l;
			}

			void setMemStore(MemStoreList &&l) {
				mem_store = l;
			}
			void setComp(CompList &&l) {
				comp = l;
			}
			void setInvars(InvarList &&l) {
				loop_invariant = l;
			}

			void setError(StringRef cause) {
				err_cause = cause;
				err = true;
			}

			void print(raw_ostream &OS) const {
				if (!err) {
					OS << "Success";
				} else {
					OS << "Error " << err_cause;
				}
			}

			/**
			 * @brief explicit cast operator for Boolean
			 * 
			 * @return true if decoupling success
			 * @return false in the case of any error
			 */
			explicit operator bool() {
				return !err;
			}
			
			// load instrcution
			inline load_iterator load_begin() {
				return mem_load.begin();
			}
			inline load_iterator load_end() {
				return mem_load.end();
			}
			inline iterator_range<load_iterator> loads() {
				return make_range(load_begin(), load_end());
			}
			// store instruction
			inline store_iterator store_begin() {
				return mem_store.begin();
			}
			inline store_iterator store_end() {
				return mem_store.end();
			}
			inline iterator_range<store_iterator> stores() {
				return make_range(store_begin(), store_end());
			}
			// computation
			inline comp_iterator comp_begin() {
				return comp.begin();
			}
			inline comp_iterator comp_end() {
				return comp.end();
			}
			inline iterator_range<comp_iterator> comps() {
				return make_range(comp_begin(), comp_end());
			}
			// loop invariant
			inline invar_iterator invars_begin() {
				return loop_invariant.begin();
			}
			inline invar_iterator invars_end() {
				return loop_invariant.end();
			}
			inline iterator_range<invar_iterator> invars() {
				return make_range(invars_begin(), invars_end());
			}

			MemLoadList& get_loads() {
				return mem_load;
			}

			MemStoreList& get_stores() {
				return mem_store;
			}

			CompList& get_comps() {
				return comp;
			}
			
			InvarList& get_invars() {
				return loop_invariant;
			}

			/**
			 * @brief Set the node traversal skip sequence for loop invariant nodes
			 * 
			 * @param node the loop invariant node
			 * @param hist sequence of skipped nodes
			 */
			void setInvarSkipSequence(Value* node, SmallVector<Value*> seq) {
				invar_skip_seq[node] = seq;
			}

			/**
			 * @brief get the node traversal skip sequence
			 * 
			 * @param node the loop invariant node
			 * @return SmallVector<Value*>* a sequence of skipped node
			 * if there is any skipped node. Otherwise it returns nullptr
			 */
			SmallVector<Value*>* getSkipSequence(Value* node) {
				if (invar_skip_seq.find(node) != invar_skip_seq.end()) {
					return &(invar_skip_seq[node]);
				} else {
					return nullptr;
				}
			}


		private:
			MemLoadList mem_load;
			MemStoreList mem_store;
			CompList comp;
			InvarList loop_invariant;
			StringRef err_cause;
			// key = , value = 
			DenseMap<Value*, SmallVector<Value*>> invar_skip_seq;

			bool err = false;

	};

	/**
	 * @class DecoupledAnalysisPass
	 * @brief A loop pass to analyze memory access for decoupling
	*/
	class DecoupledAnalysisPass :
			public AnalysisInfoMixin<DecoupledAnalysisPass> {
		public:
			using Result = DecoupledAnalysis;
			Result run(Loop &L, LoopAnalysisManager &AM,
								LoopStandardAnalysisResults &AR);
		private:
			friend AnalysisInfoMixin<DecoupledAnalysisPass>;
			static AnalysisKey Key;

			void traversal(SmallVector<LoadInst*> &LL, SmallVector<StoreInst*> &SL);

			/**
			 * @brief check whether the instruction load the value of pointer instead of data
			 * 
			 * @param I the load instruction to be checked
			 * @return true if it loads the value of some pointer
			 * @return return: otherwise
			 */
			bool isPointerValue(LoadInst *I);

			// /**
			//  * @brief check if the instruction is memory access or not
			//  * 
			//  * @param I Instruction to be checked
			//  * @return true if it is a memory access
			//  * @return otherwise: false
			//  */
			// inline bool isMemAccess(Instruction &I) {
			// 	return isa<LoadInst>(I) || isa<StoreInst>(I);
			// }
	};
}


#endif //DecoupledAnalysis_H