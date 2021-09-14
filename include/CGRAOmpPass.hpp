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
*    File:          /include/CGRAOmpPass.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 14:19:42
*    Last Modified: 14-09-2021 01:44:19
*/
#ifndef CGRAOmpPass_H
#define CGRAOmpPass_H

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "CGRAModel.hpp"


#include <system_error>

using namespace llvm;

#define ERR_MSG_PREFIX "CGRAOmpPass \x1B[31m\033[1mError\033[0m: "

namespace CGRAOmp {

	class CGRAOmpPass : public PassInfoMixin<CGRAOmpPass> {
		public:
			explicit CGRAOmpPass(CGRAModel* model) : model(model) {};
			PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
		private:
			CGRAModel *model;
	};

	
}

#endif //CGRAOmpPass_H