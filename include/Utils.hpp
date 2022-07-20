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
*    File:          /include/Utils.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  17-07-2022 19:02:30
*    Last Modified: 20-07-2022 11:32:04
*/
#ifndef CGRAOMP_UTILS_H
#define CGRAOMP_UTILS_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/APFloat.h"

#include <string>


using namespace llvm;

namespace CGRAOmp {
	namespace Utils {
		/**
		 * @brief a utility function to obtain analysis results in LoopStandardAnalysisResults
		 * 
		 * @param F Function
		 * @param AM FunctionAnalysisManager
		 * @return LoopStandardAnalysisResults 
		 */
		LoopStandardAnalysisResults getLSAR(Function &F,
									FunctionAnalysisManager &AM);


		BranchInst* findBackBranch(Loop *L);

		void getAllGEP(Loop* L, SmallVector<Instruction*> &List);

		void getArrayElementSizes(Type *Ty, SmallVector<int> &sizes, Type* &element_type);

		int getFloatDataWidth(const APFloat f);

		int getDataWidth(const Type* T);

		std::string getFloatType(const APFloat f);

		double getFloatValueAsDouble(const APFloat f);
		
	}

}
#endif //CGRAOMP_UTILS_H