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
*    File:          /include/CGRAOmpAnnotationPass.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  15-09-2021 09:53:19
*    Last Modified: 15-09-2021 11:31:23
*/
#ifndef CGRAOmpAnnotationPass_H
#define CGRAOmpAnnotationPass_H

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/StringRef.h"

#define LLVM_ANNOT_NAME "llvm.global.annotations"

using namespace llvm;

namespace CGRAOmp {

	/**
	 * @class ModuleAnnotationAnalysisPass
	 * @brief Module Pass to analyze which annotations each function has
	*/
	class ModuleAnnotationAnalysisPass :
			public AnalysisInfoMixin<ModuleAnnotationAnalysisPass> {
		public:
			using ResultBase = DenseMap<Function*, SetVector<StringRef>>;
			/**
			 * @class Result
			 * @brief Inner class for analysis result for ModuleAnnotationAnalysisPass
			*/
			class Result {
				public:
					bool invalidate(Module& M, const PreservedAnalyses &PA,
								ModuleAnalysisManager::Invalidator &Inv) {
					auto PAC = PA.getChecker<ModuleAnnotationAnalysisPass>();
					return !PAC.preservedWhenStateless();
				}
				/// interface for DenseMap.find
				ResultBase::iterator find(Function *F) {
					auto hoge = result[F];
					return result.find(F);
				}
				/// interface for DenseMap.end
				ResultBase::iterator end() {
					return result.end();
				}
				/// interface for DenseMap[]
				SetVector<StringRef>& operator [](Function *F) {
					return result[F];
				}

				private:
					ResultBase result;
			};
			Result run(Module &F, ModuleAnalysisManager &AM);

		private:
			friend AnalysisInfoMixin<ModuleAnnotationAnalysisPass>;
			static AnalysisKey Key;
	};

	/**
	 * @class AnnotationAnalysisPass
	 * @brief Function Pass to analyze which annotations the function has
	*/
	class AnnotationAnalysisPass :
			public AnalysisInfoMixin<AnnotationAnalysisPass> {
		public:
			using Result = SetVector<StringRef>;
			Result run(Function &F, FunctionAnalysisManager &AM);

		private:
			friend AnalysisInfoMixin<AnnotationAnalysisPass>;
			static AnalysisKey Key;
	};

}

#endif //CGRAOmpAnnotationPass_H