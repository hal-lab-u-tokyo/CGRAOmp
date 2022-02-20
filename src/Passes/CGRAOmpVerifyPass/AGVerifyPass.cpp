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
*    Last Modified: 21-02-2022 03:45:50
*/

#include "AGVerifyPass.hpp"

#include "llvm/Analysis/IVDescriptors.h"

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
				verifySCEVAsAffineAG(s, SE, C);
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
				verifySCEVAsAffineAG(s, SE, C);
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
	json::Array top;
	if (config_value != config.end()) {
		for (auto C : config_value->second.config) {
			top.push_back(json::Object(
				{{"start", json::Value(C.start)},
				 {"step", json::Value(C.step)},
				 {"count", json::Value(C.count)}}
			));
		}
	}

	return std::move(top);
}

/* ================= Utility functions ================= */
void CGRAOmp::verifySCEVAsAffineAG(const SCEV* S, ScalarEvolution &SE, AffineAGCompatibility::ConfigTy& C)
{
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
					InductionDescriptor IDV;
					// TODO: below code cannot determine outer most loop count
					int count = SE.getSmallConstantMaxTripCount(SAR->getLoop());
					if (step->getSCEVType() == scConstant) {
						if (auto step_val = dyn_cast<SCEVConstant>(step)->getAPInt().getRawData()) {
							// errs() << "Step: " << *step_val << "\n";
							AffineAGCompatibility::DimEntry_t entry = {0, (int64_t)(*step_val), count};
							C.config.emplace_back(entry);
							scev_stack.emplace_back(start);
						} else {
							// unknown
							invalidate();
						}
					} else {
						// not constant
						// errs() << "not constant\n";
						invalidate();
					}
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

#undef DEBUG_TYPE