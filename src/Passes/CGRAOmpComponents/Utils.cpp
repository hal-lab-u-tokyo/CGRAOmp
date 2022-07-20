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
*    File:          /src/Passes/CGRAOmpComponents/Utils.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  17-07-2022 19:01:36
*    Last Modified: 20-07-2022 11:33:19
*/

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopNestAnalysis.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Support/FormatVariadic.h"

#include "Utils.hpp"

using namespace llvm;
using namespace CGRAOmp;
using namespace CGRAOmp;
using namespace std;

/* ================= Utility functions ================= */
LoopStandardAnalysisResults Utils::getLSAR(Function &F,
								FunctionAnalysisManager &AM)
{
	auto &AA = AM.getResult<AAManager>(F);
	auto &AC = AM.getResult<AssumptionAnalysis>(F);
	auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
	auto &LI = AM.getResult<LoopAnalysis>(F);
	auto &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
	auto &TLI = AM.getResult<TargetLibraryAnalysis>(F);
	auto &TTI = AM.getResult<TargetIRAnalysis>(F);
	BlockFrequencyInfo *BFI = &AM.getResult<BlockFrequencyAnalysis>(F);
	MemorySSA *MSSA = &AM.getResult<MemorySSAAnalysis>(F).getMSSA();

	return LoopStandardAnalysisResults(
		{AA, AC, DT, LI, SE, TLI, TTI, BFI, MSSA}
	);
}

BranchInst* Utils::findBackBranch(Loop *L)
 {
	BasicBlock *Latch = L->getLoopLatch();
	auto BackBranch = dyn_cast<BranchInst>(Latch->getTerminator());
	if (!BackBranch || !BackBranch->isConditional()) {
		return nullptr;
	} else {
		return BackBranch;
	}
 }

void Utils::getAllGEP(Loop* L, SmallVector<Instruction*> &List)
{
	for (auto &BB : L->getBlocks()) {
		for (auto &I : *BB) {
			if (auto gep = dyn_cast<GetElementPtrInst>(&I)) {
				List.emplace_back(gep);
			}
		}
	}

}

void Utils::getArrayElementSizes(Type *Ty, SmallVector<int> &sizes, Type* &element_type)
{
	element_type = Ty;
	while (auto arr_type = dyn_cast<ArrayType>(element_type)) {
		sizes.emplace_back(arr_type->getNumElements());
		element_type = arr_type->getElementType();
	}
}

int Utils::getFloatDataWidth(const APFloat f) 
{
	switch (APFloatBase::SemanticsToEnum(f.getSemantics())) {
		case APFloat::Semantics::S_IEEEhalf:
			return 16;
		case APFloat::Semantics::S_IEEEsingle:
			return 32;
		case APFloat::Semantics::S_IEEEdouble:
			return 64;
		case APFloat::Semantics::S_IEEEquad:
			return 128;
		default:
			return 0;
	}
}

string Utils::getFloatType(const APFloat f) 
{
	int width = Utils::getFloatDataWidth(f);
	if (width > 0) {
		return formatv("float{0}", width);
	} else {
		return "unknown";
	}
}

double Utils::getFloatValueAsDouble(const APFloat f)
{
	int width = Utils::getFloatDataWidth(f);
	if (width < 64)	{
		return (double)(f.convertToFloat());
	} else {
		// it might causes an error for float128
		return f.convertToDouble();
	}
}

int Utils::getDataWidth(const Type* T) 
{
	switch (T->getTypeID()) {
		case Type::BFloatTyID:
			return 16;
		case Type::FloatTyID:
			return 32;
		case Type::DoubleTyID:
			return 64;
		case Type::FP128TyID:
			return 128;
		case Type::IntegerTyID:
		{
			auto intty = dyn_cast<IntegerType>(T);
			return intty->getBitWidth();
		} break;
		default:
			return 0;
	}
}