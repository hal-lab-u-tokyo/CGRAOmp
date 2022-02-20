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
*    File:          /include/LoopDependencyAnalysis.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  18-02-2022 18:10:42
*    Last Modified: 20-02-2022 22:42:54
*/
#ifndef LoopDependencyAnalysis_H
#define LoopDependencyAnalysis_H

#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/ADT/SmallVector.h"

#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/ADT/Optional.h"

using namespace llvm;


namespace CGRAOmp {

	/**
	 * @class LoopDependency
	 * @brief A base class for loop dependency relationship
	*/
	class LoopDependency {
		public:
			enum class DepKind {
				Simple,
				Memory,
				InductionVar,
			};

			/**
			 * @brief Construct a new Loop Dependency object for normal loop carried dependency
			 * 
			 * @param def 
			 * @param use 
			 * @param phi 
			 */
			LoopDependency(DepKind kind, Value* def, Value *init,
					PHINode *phi, int distance) : 
				def(def), distance(distance), kind(kind), phi(phi),
				init(init) {};
			
			DepKind getKind() const {
				return kind;
			}

			bool correspond(PHINode *P) {
				return P == phi;
			}

			// bool correspond(Instruction *U) {
			// 	return U == use;
			// }

			Value* getDef() {
				return def;
			}

			Instruction* getDefInst() {
				return dyn_cast<Instruction>(def);
			}

			Value* getInit() {
				return init;
			}

			PHINode* getPhi() {
				return phi;
			}

			int getDistance() {
				return distance;
			}
		
		protected:
			PHINode* phi;
			DepKind kind;
			Value *def;
			Value *init;

			int distance;

	};

	/**
	 * @class SimpleLoopDependency
	 * @brief A derived class from LoopDependency for loop carried dependency via register
	*/
	class SimpleLoopDependency : public LoopDependency {
		public:
			SimpleLoopDependency(Instruction *def, Value* init, PHINode* phi) :
				LoopDependency(DepKind::Simple, def, init, phi, 1) {};

			static bool classof(const LoopDependency* LD) {
				return LD->getKind() == LoopDependency::DepKind::Simple;
			}
		private:

	};

	/**
	 * @class MemoryLoopDependency
	 * @brief A derived class from LoopDependency for loop carried dependency via memory access
	*/
	class MemoryLoopDependency : public LoopDependency {
		public:
			MemoryLoopDependency(StoreInst *store, LoadInst *load, int distance) :
				LoopDependency(DepKind::Memory, store->getOperand(0),
								 nullptr, nullptr, distance),
				store(store), load(load) {};

			
			LoadInst *getLoad() {
				return load;
			}

			StoreInst *getStore() {
				return store;
			}

			static bool classof(const LoopDependency* LD) {
				return LD->getKind() == LoopDependency::DepKind::Memory;
			}
		private:
			LoadInst *load;
			StoreInst *store;

	};

	/**
	 * @class InductionVariableDependency
	 * @brief A derived class from LoopDependency for loop carried dependency associated with loop induction variables
	*/
	class InductionVariableDependency : public LoopDependency {
		public:
			InductionVariableDependency(PHINode *indvar, Instruction *bin_op,
				Value* start, Value *step) :
				LoopDependency(DepKind::InductionVar, bin_op, start, indvar, 1),
				 step(step) {};


			static bool classof(const LoopDependency* LD) {
				return LD->getKind() == LoopDependency::DepKind::InductionVar;
			}
		private:
			Value *step, *start;
			Instruction* bin_op;
	};

	/**
	 * @class LoopDependencyInfo
	 * @brief LoopDependency analysis result
	*/
	class LoopDependencyInfo {
		public:
			using DepList = SmallVector<LoopDependency*>;

			using dep_iterator = DepList::iterator;

			// induction variable dependency
			inline dep_iterator idv_dep_begin() {
				return indvar_dep_list.begin();
			}
			inline dep_iterator idv_dep_end() {
				return indvar_dep_list.end();
			}
			inline iterator_range<dep_iterator> idv_deps() {
				return make_range(idv_dep_begin(), idv_dep_end());
			}

			void add_idv_dep(InductionVariableDependency *D) {
				indvar_dep_list.emplace_back(D);
			};

			int getNumIdvDep() {
				return indvar_dep_list.size();
			}

			// memory dependency
			inline dep_iterator mem_dep_begin() {
				return mem_dep_list.begin();
			}
			inline dep_iterator mem_dep_end() {
				return mem_dep_list.end();
			}
			inline iterator_range<dep_iterator> mem_deps() {
				return make_range(mem_dep_begin(), mem_dep_end());
			}

			void add_mem_dep(MemoryLoopDependency *D) {
				mem_dep_list.emplace_back(D);
			}

			int getNumMemDep() {
				return mem_dep_list.size();
			}

			// loop-carried dependency
			inline dep_iterator lc_dep_begin() {
				return lc_dep_list.begin();
			}
			inline dep_iterator lc_dep_end() {
				return lc_dep_list.end();
			}
			inline iterator_range<dep_iterator> lc_deps() {
				return make_range(lc_dep_begin(), lc_dep_end());
			}

			void add_dep(SimpleLoopDependency *D) {
				lc_dep_list.emplace_back(D);
			}

			int getNumDep() {
				return lc_dep_list.size();
			}

		private:
			DepList indvar_dep_list;
			DepList mem_dep_list;
			DepList lc_dep_list;
	};


	/**
	 * @class LoopDependencyAnalysisPass
	 * @brief A loop pass to analyze memory access for decoupling
	*/
	class LoopDependencyAnalysisPass :
			public AnalysisInfoMixin<LoopDependencyAnalysisPass> {
		public:
			using Result = LoopDependencyInfo;
			Result run(Loop &L, LoopAnalysisManager &AM,
								LoopStandardAnalysisResults &AR);
		private:
			friend AnalysisInfoMixin<LoopDependencyAnalysisPass>;
			static AnalysisKey Key;


			Optional<int> getDistance(Instruction* A, Instruction *B, ScalarEvolution &SE);

	};
}


#endif //LoopDependencyAnalysis_H