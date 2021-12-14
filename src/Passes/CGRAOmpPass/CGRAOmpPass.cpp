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
*    File:          /src/Passes/CGRAOmpPass/CGRAOmpPass.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 14:19:22
*    Last Modified: 14-12-2021 10:35:55
*/
#include "CGRAOmpPass.hpp"
#include "VerifyPass.hpp"
#include "CGRAModel.hpp"
#include "OptionPlugin.hpp"
#include "CGRAOmpAnnotationPass.hpp"

#include "llvm/Transforms/Scalar/LoopRotation.h"
#include "llvm/Transforms/Utils/LCSSA.h"
#include "llvm/Passes/PassBuilder.h"

#include "llvm/ADT/Statistic.h"

#include <system_error>


using namespace llvm;
using namespace CGRAOmp;

#define DEBUG_TYPE "cgraomp"

// # of successfully exracted DFGs
STATISTIC(num_dfg, "the number of extracted DFGs");

AnalysisKey ModelManagerPass::Key;

ModelManagerPass::Result
ModelManagerPass::run(Module &M, ModuleAnalysisManager &AM)
{
	// for cache result
	AM.getResult<ModuleAnnotationAnalysisPass>(M);

	LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX << "Instantiating CGRAModel\n");
	auto ErrorOrModel = parseCGRASetting(PathToCGRAConfig, AM);
	if (!ErrorOrModel) {
		ExitOnError Exit(ERR_MSG_PREFIX);
		Exit(std::move(ErrorOrModel.takeError()));
	}
	auto model = *ErrorOrModel;

	return ModelManager(model);
}

bool ModelManager::invalidate(Module& M, const PreservedAnalyses &PA,
								ModuleAnalysisManager::Invalidator &Inv)
{
	// always keeping this reuslt valid after creation
	auto PAC = PA.getChecker<ModelManagerPass>();
	return !PAC.preservedWhenStateless();
}

AnalysisKey OmpKernelAnalysisPass::Key;

/**
 * @details 
**/

OmpKernelAnalysisPass::Result OmpKernelAnalysisPass::run(Module &M,
										ModuleAnalysisManager &AM)
{
	using OpBitCast = ConcreteOperator<Operator, Instruction::BitCast>;
	Result result;
	LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX << "Searching for OpenMP kernels\n");
	for (auto &G : M.globals()) {
		// find offloading.entry
		if (G.getName().startswith(KERNEL_INFO_PREFIX)) {
			// found
			auto *info = dyn_cast<ConstantStruct>(G.getOperand(0));
			if (!info) continue;
			auto *kernel_func = dyn_cast<Function>(
				info->getOperand(0)->getOperand(0));
			if (!kernel_func) continue;
			// offloading function is obtained
			LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX <<
				 "Found offloading function: " + kernel_func->getName() << "\n");
			for (auto & BB : *kernel_func) {
				for (auto &I : BB) {
					if (auto ci = dyn_cast<CallBase>(&I)) {
						auto called_func = ci->getCalledFunction();
						// found function call to loop kernel
						if (called_func->getName() == "__kmpc_fork_call") {
							assert(ci->getNumOperands() >= 3 && "Failed to find micro task");
							if (auto bitcast_inst = dyn_cast<OpBitCast>(
								ci->getOperand(2)
							)) {
								if (auto micro_task = dyn_cast<Function>(
									bitcast_inst->getOperand(0)
								)) {
									result[kernel_func->getName()] = micro_task;
								}
							} else {
								LLVM_DEBUG(dbgs() << WARN_DEBUG_PREFIX 
							<< "__kmpc_fork_call found but the 3rd operand is not bitcast\n");
							}
						}
					}
				}
			}
		}
	}
	return result;
}

AnalysisKey OmpStaticShecudleAnalysis::Key;

OmpStaticShecudleAnalysis::Result
OmpStaticShecudleAnalysis::run(Loop &L, LoopAnalysisManager &AM,
						LoopStandardAnalysisResults &AR)
{
	// find __kmpc_for_static_init
	CallBase *init_call = nullptr;
	for (auto &BB : *(L.getHeader()->getFirstNonPHI()->getFunction())) {
		for (auto &I : BB) {
			if (auto call_inst = dyn_cast<CallBase>(&I)) {
				auto F = call_inst->getCalledFunction();
				if (F->getName().startswith("__kmpc_for_static_init")) {
					init_call = call_inst;
					break;
				}
			}
		}
		if (init_call) {
			break;
		}
	}

	if (init_call &&
			init_call->getNumOperands() >= OMP_STATIC_INIT_OPERAND_N) {
		errs() << init_call->getNumOperands() << "\n";
		ScheduleInfo info(
			init_call->getOperand(OMP_STATIC_INIT_SCHED),
			init_call->getOperand(OMP_STATIC_INIT_PLASTITER),
			init_call->getOperand(OMP_STATIC_INIT_PLOWER),
			init_call->getOperand(OMP_STATIC_INIT_PUPPER),
			init_call->getOperand(OMP_STATIC_INIT_PSTRIDE),
			init_call->getOperand(OMP_STATIC_INIT_INCR),
			init_call->getOperand(OMP_STATIC_INIT_CHUNK)
		);
		return info;
	} else {
		ScheduleInfo invalid_info;
		return invalid_info;
	}

}

#undef DEBUG_TYPE

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
	return {
		LLVM_PLUGIN_API_VERSION, "CGRAOmp", LLVM_VERSION_STRING,
		[](PassBuilder &PB) {
			PB.registerPipelineParsingCallback(
				[](StringRef Name, ModulePassManager &PM,
					ArrayRef<PassBuilder::PipelineElement>){
						if (Name == "cgraomp") {
							// make a pipeline
							//PM.addPass(ADD_LOOP_PASS(LoopRotatePass()));
							// PM.addPass(ADD_FUNC_PASS(LCSSAPass()));
							// Verify->DFGExraction->Runtime Insertion
							PM.addPass(VerifyModulePass());
							return true;
						}
						return false;
				}
			);

			PB.registerAnalysisRegistrationCallback(
				[](ModuleAnalysisManager &MAM) {
					MAM.registerPass([&] {
						return ModelManagerPass();
					});
			});

			PB.registerAnalysisRegistrationCallback(
				[](ModuleAnalysisManager &MAM) {
					MAM.registerPass([&] {
						return OmpKernelAnalysisPass();
					});
			});

			PB.registerAnalysisRegistrationCallback(
				[](LoopAnalysisManager &LAM) {
					LAM.registerPass([&] {
						return OmpStaticShecudleAnalysis();
					});
			});
		}
	};
}
