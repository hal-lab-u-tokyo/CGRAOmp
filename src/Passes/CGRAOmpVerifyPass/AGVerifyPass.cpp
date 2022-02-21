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
*    Last Modified: 22-02-2022 06:24:11
*/

#include "AGVerifyPass.hpp"

#include "llvm/Analysis/IVDescriptors.h"
#include "llvm/IR/Constants.h"

#include <deque>

using namespace llvm;
using namespace CGRAOmp;

#define DEBUG_TYPE "cgraomp"

static const char *VerboseDebug = DEBUG_TYPE "-verbose";

/* ============= Implementation of VerifyAGCompatiblePass ============= */

// partial specilization for Affine AG
template <>
VerifyAGCompatiblePass<AffineAGCompatibility>::Result
VerifyAGCompatiblePass<AffineAGCompatibility>::run(Loop &L,
	LoopAnalysisManager &AM, LoopStandardAnalysisResults &AR)
{
	using llvm::SCEVTypes;

	AffineAGCompatibility result;

	LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX 
					<< "Verifying Affine AG compatibility of a loop: "
					<< L.getName() << "\n");

	// get decoupled memory access insts
	auto DA = AM.getResult<DecoupledAnalysisPass>(L, AR);
	auto &SE = AR.SE;

	auto &MM = AM.getResult<ModelManagerLoopProxy>(L, AR);
	auto *model = MM.getModel();
	auto *dec_model = dyn_cast<DecoupledCGRA>(model);
	auto *AG = dyn_cast<AffineAG>(dec_model->getAG());

	// verify the kernel only if decoupling succeeds
	if (DA) {
		for (auto load : DA.loads()) {
			auto addr = load->getOperand(0);
			if (SE.isSCEVable(addr->getType())) {
				const auto *s = SE.getSCEV(addr);
				AffineAGCompatibility::ConfigTy C;
				verifySCEVAsAffineAG(s, AR, C);
				result.add(load, C);

			} else {
				// SCEV not available
				result.setVio();
			}
		}
		for (auto store : DA.stores()) {
			auto addr = store->getOperand(1);
			if (SE.isSCEVable(addr->getType())) {
				const auto *s = SE.getSCEV(addr);
				AffineAGCompatibility::ConfigTy C;
				verifySCEVAsAffineAG(s, AR, C);
				result.add(store, C);
			} else {
				// SCEV not available
				result.setVio();
			}
		}
	}

	if (!result) {
		LLVM_DEBUG(dbgs() << ERR_DEBUG_PREFIX
			<< "Loop Kernel " << L.getName()
			<< " has memory access incompatible with the Affine AG\n";
		);
	}
	DEBUG_WITH_TYPE(VerboseDebug,
		dbgs() << DBG_DEBUG_PREFIX << "analyzed result of affine access\n";
		result.print(dbgs()));

	return result;
}


/* ============= Implementation of VerifyAGCompatiblePass ============= */
void AffineAGCompatibility::print(raw_ostream &OS) const
{
	for (auto item : config) {
		SmallVector<int> tmp;
		auto *I = item.first;
		auto C = item.second;
		for (auto entry : C.config) {
			tmp.emplace_back(entry.step);
		}
		I->print(OS << "MemAccess ");
		OS << formatv(" Valid? {0:T}: {1}\n", C.valid, make_range(tmp.begin(), tmp.end()));
	}

}

llvm::json::Value AffineAGCompatibility::getConfigAsJson(Instruction *I) const
{

	auto config_value = config.find(I);
	json::Object top;
	if (config_value != config.end()) {
		if (config_value->second.base != nullptr) {
			top["base"] = std::move(json::Value(
				config_value->second.base->getNameOrAsOperand())
			);
		} else {
			top["base"] = std::move(json::Value("unknown"));
		}
		json::Array arr;
		for (auto C : config_value->second.config) {

			arr.push_back(json::Object(
				{{"start", json::Value(C.start)},
				 {"step", json::Value(C.step)},
				 {"count", json::Value(C.count)}}
			));
		}
		top["offset"] = std::move(arr);
	}

	return std::move(top);
}

/* ================= Utility functions ================= */
void CGRAOmp::verifySCEVAsAffineAG(const SCEV* S, LoopStandardAnalysisResults &AR, AffineAGCompatibility::ConfigTy& C)
{
	auto &SE = AR.SE;
	// stack for depth first seach
	std::vector<const SCEV*> scev_stack;
	scev_stack.emplace_back(S);
	// trun into true when the seach first reaches a non-addrec
	auto end_addrec = false;
	C.valid = true;

	auto invalidate = [&]() {
		scev_stack.clear();
		C.valid = false;
	};

	// depth first search
	while (!scev_stack.empty()) {
		bool end_addrec_prev = end_addrec;
		// pop
		const auto *scev = scev_stack.back();
		scev_stack.pop_back();
		// switch depending on the type of SCEV
		switch (scev->getSCEVType()) {
			case scAddRecExpr:
			{
				// errs() << "add rec\n";
				if (end_addrec) {
					// unexpected addrec
					invalidate();
				} else {
					// get step and start
					auto *SAR = dyn_cast<SCEVAddRecExpr>(scev);
					auto *step = SAR->getStepRecurrence(SE);
					const auto *start = SAR->getStart();
					int count = SE.getSmallConstantTripCount(SAR->getLoop());
					int step_val = 0;
					int start_val = 0;
					if (step->getSCEVType() == scConstant) {
						if (auto step_val_ptr = dyn_cast<SCEVConstant>(step)->getAPInt().getRawData()) {
							step_val = *step_val_ptr;
						} else {
							invalidate();
						}
					} else {
						invalidate();
					}
					Value *base;
					if (parseStartSCEV(start, &start_val, &base)) {
						C.base = base;
					}
					AffineAGCompatibility::DimEntry_t entry = {start_val, step_val, count};
					C.config.insert(C.config.begin(), entry);
					scev_stack.emplace_back(start);

				}
			} break;// end case scAddRecExpr
			case scAddExpr:
			case scMulExpr:
			{
				end_addrec = true;
				const auto SA = dyn_cast<SCEVCommutativeExpr>(scev);
				// errs() << "scev add or mul operand  " << SA->getNumOperands() << " ";
				// scev->dump();
				for (const auto operand : SA->operands()) {
					// operand->dump();
					scev_stack.emplace_back(operand);
				}

			} break; // end scAddExpr, scMulExpr
			case scSignExtend:
			case scTruncate:
			case scZeroExtend:
			case scPtrToInt:
			{
				end_addrec = true;
				const auto SC = dyn_cast<SCEVCastExpr>(scev);
				// errs() << "cast\n";
				// errs() << "operand " << SC->getNumOperands();
				// scev->dump();
				for (const auto operand : SC->operands()) {
					// operand->dump();
					scev_stack.emplace_back(operand);
				}
			} break; // end scSignExtend,scTruncate,scZeroExtend,scPtrToInt
			case scConstant:
				end_addrec = true;
				break;
			case scUnknown:
			{
				end_addrec = true;
				// errs() << "unknown\n";
				const auto* SU = dyn_cast<SCEVUnknown>(scev);
				auto *val = SU->getValue();
			} break; // end scUnknown
			default:
				// errs() << "scev type " << scev->getSCEVType() << "\n";
				invalidate();
		} // end switch
	} // end depth first search

}

bool CGRAOmp::parseStartSCEV(const SCEV* S, int *offset, Value **base)
{
	*base = nullptr;
	*offset = 0;
	if (auto SA = dyn_cast<SCEVAddExpr>(S)) {
		if (SA->getNumOperands() <= 2) {
			for (auto operand : SA->operands()) {
				if (auto const_scev = dyn_cast<SCEVConstant>(operand)) {
					*offset += *(const_scev->getAPInt().getRawData());
				} else if (auto scev_value = dyn_cast<SCEVUnknown>(operand)) {
					// already set
					if (*base) return false;
					*base = scev_value->getValue();
				}
			}
		} else {
			return false;
		}
	} else if (auto SU = dyn_cast<SCEVUnknown>(S)) {
		*base = SU->getValue();
	}
	return true;
}

// not fully implemented bellow
unsigned CGRAOmp::computeLoopTripCount(const Loop *L, LoopStandardAnalysisResults &AR)
{
	InductionDescriptor IDV;
	if (!L->getInductionDescriptor(AR.SE, IDV)) {
		return 0;
	}
	BasicBlock *Latch = L->getLoopLatch();
	auto BackBranch = dyn_cast<BranchInst>(Latch->getTerminator());
	if (!BackBranch || !BackBranch->isConditional()) {
		return 0;
	}
	ICmpInst *Compare = dyn_cast<ICmpInst>(BackBranch->getCondition());
	if (!Compare || Compare->hasNUsesOrMore(2)) {
		return 0;
	}

	PHINode* IdvPhi = L->getInductionVariable(AR.SE);
	IdvPhi->dump();
	auto opcode = IDV.getInductionOpcode();
	auto *step = IDV.getConstIntStepValue();
	if (!step) {
		return 0;
	}
	int step_int = step->getSExtValue();

	// initial value of induction variable
	auto init_value = IdvPhi->getIncomingValueForBlock(L->getLoopPreheader());

	Value* Bound = (Compare->getOperand(0) == IdvPhi) ? Compare->getOperand(1) :
					Compare->getOperand(0);


	if (L->isGuarded()) {
		L->getLoopGuardBranch()->dump();
	}
	return 0;
}

#undef DEBUG_TYPE