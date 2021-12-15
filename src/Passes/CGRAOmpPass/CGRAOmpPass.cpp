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
*    Last Modified: 15-12-2021 11:42:38
*/
#include "CGRAOmpPass.hpp"
#include "VerifyPass.hpp"
#include "CGRAModel.hpp"
#include "OptionPlugin.hpp"
#include "CGRAOmpAnnotationPass.hpp"
#include "DFGPass.hpp"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Scalar/LoopRotation.h"
#include "llvm/Transforms/Utils/LCSSA.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/FormatVariadic.h"

#include "llvm/ADT/Statistic.h"

#include <system_error>


using namespace llvm;
using namespace CGRAOmp;

#define DEBUG_TYPE "cgraomp"
static const char *VerboseDebug = DEBUG_TYPE "-verbose";

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

template <typename IRUnitT, typename InvT>
bool ModelManager::invalidate(IRUnitT &IR, const PreservedAnalyses &PA,
								InvT &Inv)
{
	// always keeping this reuslt valid after creation
	auto PAC = PA.getChecker<ModelManagerPass>();
	return !PAC.preservedWhenStateless();
}


AnalysisKey ModelManagerFunctionProxy::Key;

ModelManagerFunctionProxy::Result
ModelManagerFunctionProxy::run(Function &F, FunctionAnalysisManager &AM)
{
	auto &MAMProxy = AM.getResult<ModuleAnalysisManagerFunctionProxy>(F);
	auto &M = *(F.getParent());
	auto *MM = MAMProxy.getCachedResult<ModelManagerPass>(M);
	assert(MM && "ModuleManagerPass must be executed at the beginning");
	return *MM;
}

AnalysisKey ModelManagerLoopProxy::Key;

ModelManagerLoopProxy::Result
ModelManagerLoopProxy::run(Loop &L, LoopAnalysisManager &AM,
							LoopStandardAnalysisResults &AR)
{
	auto &FAMProxy = AM.getResult<FunctionAnalysisManagerLoopProxy>(L, AR);
	auto *F = (*(L.block_begin()))->getParent();
	auto *MM = FAMProxy.getCachedResult<ModelManagerFunctionProxy>(*F);
	assert(MM && "ModelManagerFunctionProxy must be executed before this pass");
	return *MM;
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

bool OmpScheduleInfo::invalidate(Function &F, const PreservedAnalyses &PA,
								FunctionAnalysisManager::Invalidator &Inv)
{
	auto PAC = PA.getChecker<OmpStaticShecudleAnalysis>();
	return !PAC.preservedWhenStateless();
}


AnalysisKey OmpStaticShecudleAnalysis::Key;

OmpStaticShecudleAnalysis::Result
OmpStaticShecudleAnalysis::run(Function &F, FunctionAnalysisManager &AM)
{
	// find calling __kmpc_for_static_init*
	CallBase *init_call = nullptr;
	for (auto &BB : F) {
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
		DEBUG_WITH_TYPE(VerboseDebug,
						dbgs() << formatv("{0}Number of arguments of {1} is {2}\n",
						DBG_DEBUG_PREFIX,
						init_call->getCalledFunction()->getName(),
						init_call->getNumArgOperands()));
		OmpScheduleInfo info(
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
		OmpScheduleInfo invalid_info;
		LLVM_DEBUG(dbgs() << ERR_DEBUG_PREFIX << "call of \"__kmpc_for_static_init\" is not found\n");
		return invalid_info;
	}

}

#undef DEBUG_TYPE

static void registerModuleAnalyses(ModuleAnalysisManager &MAM)
{
#define MODULE_ANALYSIS(CREATE_PASS) \
	MAM.registerPass([&] { return CREATE_PASS; });

#include "OmpPasses.def"

}

static void registerFunctionAnalyses(FunctionAnalysisManager &FAM)
{
#define FUNCTION_ANALYSIS(CREATE_PASS) \
	FAM.registerPass([&] { return CREATE_PASS; });

#include "OmpPasses.def"

}

static void registerLoopAnalyses(LoopAnalysisManager &LAM)
{
#define LOOP_ANALYSIS(CREATE_PASS) \
	LAM.registerPass([&] { return CREATE_PASS; });

#include "OmpPasses.def"

}
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
							PM.addPass(DFGPass());
							return true;
						}
						return false;
				}
			);

			PB.registerAnalysisRegistrationCallback(registerModuleAnalyses);
			PB.registerAnalysisRegistrationCallback(registerFunctionAnalyses);
			PB.registerAnalysisRegistrationCallback(registerLoopAnalyses);

		}
	};
}
