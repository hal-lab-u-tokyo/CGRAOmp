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
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  14-12-2021 11:36:50
*    Last Modified: 15-12-2021 11:16:16
*/
#ifndef DecoupledAnalysis_H
#define DecoupledAnalysis_H

#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/ADT/iterator_range.h"

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

			using load_iterator = MemLoadList::iterator;
			using store_iterator = MemStoreList::iterator;

			void setMemLoad(MemLoadList &&l) {
				mem_load = l;
			}

			void setMemStore(MemStoreList &&l) {
				mem_store = l;
			}
			inline load_iterator load_begin() {
				return mem_load.begin();
			}
			inline load_iterator load_end() {
				return mem_load.end();
			}
			inline iterator_range<load_iterator> loads() {
				return make_range(load_begin(), load_end());
			}
			inline store_iterator store_begin() {
				return mem_store.begin();
			}
			inline store_iterator store_end() {
				return mem_store.end();
			}
			inline iterator_range<store_iterator> stores() {
				return make_range(store_begin(), store_end());
			}

			MemLoadList& get_loads() {
				return mem_load;
			}

			MemStoreList& get_stores() {
				return mem_store;
			}

		private:
			MemLoadList mem_load;
			MemStoreList mem_store;

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

			/**
			 * @brief check whether the instruction load the value of pointer instead of data
			 * 
			 * @param I the load instruction to be checked
			 * @return true if it loads the value of some pointer
			 * @return return: otherwise
			 */
			bool isPointerValue(LoadInst *I);
	};
}


#endif //DecoupledAnalysis_H