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
*    File:          /src/Passes/CGRAOmpVerifyPass/AGVerifyPass.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  15-02-2022 13:01:22
*    Last Modified: 15-02-2022 13:41:30
*/

#include "AGVerifyPass.hpp"

using namespace llvm;
using namespace CGRAOmp;

#define DEBUG_TYPE "cgraomp"

static const char *VerboseDebug = DEBUG_TYPE "-verbose";

/* ============= Implementation of VerifyAGCompatiblePass ============= */

// partial specilization for Affine AG
template <>
VerifyAGCompatiblePass<AddressGenerator::Kind::Affine>::Result
VerifyAGCompatiblePass<AddressGenerator::Kind::Affine>::run(Loop &L,
	LoopAnalysisManager &AM, LoopStandardAnalysisResults &AR)
{
	AffineAGCompatibility result;

	LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX 
					<< "Verifying Affine AG compatibility of a loop: "
					<< L.getName() << "\n");

	// get decoupled memory access insts
	auto DA = AM.getResult<DecoupledAnalysisPass>(L, AR);

	// // load pattern
	// check_all<0>(DA.get_loads(), AR.SE);
	// // store pattern
	// check_all<1>(DA.get_stores(), AR.SE);


	return result;
}

