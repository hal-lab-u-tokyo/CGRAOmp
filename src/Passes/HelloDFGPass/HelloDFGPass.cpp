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
*    File:          /src/Passes/TestDFGPass/test.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  30-01-2022 20:48:59
*    Last Modified: 31-01-2022 15:18:47
*/
#include "DFGPass.hpp"
#include "llvm/ADT/StringRef.h"

#include "HelloDFGPass.hpp"

using namespace llvm;
using namespace CGRAOmp;

bool HelloDFGPass::run(CGRADFG &G, Loop &L, FunctionAnalysisManager &FAM,
									LoopAnalysisManager &LAM,
									LoopStandardAnalysisResults &AR)
{
	llvm::errs() << "My DFG Pass is called: Hello World\n";
	return false;
}


extern "C" ::CGRAOmp::DFGPassPluginLibraryInfo getDFGPassPluginInfo() {
	return { "A sample of DFG Pass", 
		[](DFGPassBuilder &PB) {
			PB.registerPipelineParsingCallback(
				[](StringRef Name, DFGPassManager &PM) {
					if (Name == "hello") {
						PM.addPass(HelloDFGPass());
						return true;
					}
					return false;
				}
			);
		}
	};
}