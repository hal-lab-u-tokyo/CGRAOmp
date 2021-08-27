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
*    File:          /include/VerifyPass.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 15:00:17
*    Last Modified: 27-08-2021 16:42:54
*/
#ifndef VerifyPass_H
#define VerifyPass_H

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace CGRAOmp {
	// Verification Kind
	enum VerificationKind {
		// Loop structure
		MaxNestedLevel,
		MemoryAccess,
		InterLoopDep,
		NestedPerfectly,
		IterationSize,
		// Inside loop
		Conditional,
		FunctionCall,
	};

	class VerifyResultBase {
		public:
			VerifyResultBase() {};
			explicit operator bool() const {
				return !isViolate;
			}


		// interface to print messages
		virtual void print(raw_ostream &OS) const = 0;
		void dump() { this->print(errs()); }
		friend raw_ostream& operator<<(raw_ostream& OS, 
										const VerifyResultBase &v) {
			v.print(OS);
			return OS;
		}

		protected:
			bool isViolate = false;

	};

	class VerifyResult : public VerifyResultBase {
		public:
			VerifyResult() : VerifyResultBase() {}
			void print(raw_ostream &OS) const override;
	};

	class VerifyPass : public AnalysisInfoMixin<VerifyPass> {
		public:
			using Result = VerifyResult;
			Result run(Function &F, FunctionAnalysisManager &AM);
		private:
			friend AnalysisInfoMixin<VerifyPass>;
			static AnalysisKey Key;
	};
}

#endif //VerifyPass_H