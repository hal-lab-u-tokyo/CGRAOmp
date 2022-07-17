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
*    Last Modified: 30-06-2022 14:09:33
*/
#include "common.hpp"
#include "CGRAOmpPass.hpp"
#include "VerifyPass.hpp"
#include "CGRAModel.hpp"
#include "OptionPlugin.hpp"
#include "CGRAOmpAnnotationPass.hpp"
#include "DFGPass.hpp"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Transforms/Utils/LoopSimplify.h"
#include "llvm/Transforms/Scalar/LoopInstSimplify.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"

#include "llvm/ADT/Statistic.h"

#include <system_error>
#include <type_traits>


using namespace llvm;
using namespace CGRAOmp;

#define DEBUG_TYPE "cgraomp"
static const char *VerboseDebug = DEBUG_TYPE "-verbose";

// # of successfully exracted DFGs
STATISTIC(num_dfg, "the number of extracted DFGs");


/* ===================== Implementation of ModelManagerPass ===================== */
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


/* ===================== Implementation of OmpKernelInfo ===================== */
template <typename IRUnitT, typename InvT>
bool OmpKernelInfo::invalidate(IRUnitT &IR, const PreservedAnalyses &PA,
								InvT &Inv)
{
	// always keeping this reuslt valid after creation
	auto PAC = PA.getChecker<OmpKernelAnalysisPass>();
	return !PAC.preservedWhenStateless();
}

void OmpKernelInfo::setOffloadMetadata(Module &M)
{
	ExitOnError Exit(ERR_MSG_PREFIX);
	error_code EC;
	std::string buf;
	raw_string_ostream OS(buf);

	md_list.clear();

	auto getMetadataInt = [&](Metadata* MD) -> int {
		if (auto CM = dyn_cast<llvm::ConstantAsMetadata>(MD)){
			if (auto *cint = dyn_cast<ConstantInt>(CM->getValue())) {
				return cint->getSExtValue();
			}
		}
		MD->print(OS);
		Exit(make_error<StringError>(formatv("Fails to parse offload info. {0} is not interger", buf), EC));
		return 0;
	};

	auto getMetadataStr = [&](Metadata* MD) -> StringRef {
		if (auto MS = dyn_cast<llvm::MDString>(MD)){
			return MS->getString();
		}
		MD->print(OS);
		Exit(make_error<StringError>(formatv("Fails to parse offload info. {0} is not string", buf), EC));
		return "";
	};


	if (auto offload_info = M.getNamedMetadata(OFFLOADINFO_METADATA_NAME)) {
		for (auto entry : offload_info->operands()) {
			if (entry->getNumOperands() == 6) {
				md_list.emplace_back(OffloadMetadata_t {
					getMetadataInt(entry->getOperand(0)),
					getMetadataInt(entry->getOperand(1)),
					getMetadataInt(entry->getOperand(2)),
					getMetadataStr(entry->getOperand(3)),
					getMetadataInt(entry->getOperand(4)),
					getMetadataInt(entry->getOperand(5)),
				});
			} else {
				entry->print(OS);
				Exit(make_error<StringError>(formatv("Invalid offload info entry {0}", buf), EC));
			}
		}
	} else {
		Exit(make_error<StringError>("omp_offload.info is not found", EC));
	}

}

OmpKernelInfo::md_iterator OmpKernelInfo::getMetadata(Function *offload)
{
	md_iterator it;
	for (it = md_begin(); it != md_end(); it++) {
		auto entry = *it;
		if (formatv(OUTLINED_FUNC_NAME_FMT, 
			entry.file_dev_ID, entry.file_ID, entry.func_name, entry.line).str()
			== offload->getName().str()) {
				return it;
		}
	}
	DEBUG_WITH_TYPE(VerboseDebug,
		dbgs() << formatv("{0}Metadata for {1} is not found\n",
		DBG_DEBUG_PREFIX, offload->getName()));

	return it;
}

int OmpKernelInfo::getKernelLine(Function *kernel) {
	auto md = getMetadata(getOffloadFunction(kernel));
	if (md != md_end()) {
		return md->line;
	} else {
		return -1;
	}
}

/* =================== Implementation of OmpKernelAnalysisPass =================== */
AnalysisKey OmpKernelAnalysisPass::Key;
/**
 * @details 
**/
OmpKernelAnalysisPass::Result OmpKernelAnalysisPass::run(Module &M,
										ModuleAnalysisManager &AM)
{
	using OpBitCast = ConcreteOperator<Operator, Instruction::BitCast>;
	Result result;

	result.setOffloadMetadata(M);

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
									result.add_kernel(kernel_func, micro_task);
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
	// for (auto &F : M) {
	// 	if (F.getName().startswith("__nv_MAIN__F")) {
	// 		result[F.getName()] = &F;
	// 	}
	// }
	return result;
}

/* ===================== Implementation of OmpScheduleInfo ===================== */
bool OmpScheduleInfo::invalidate(Function &F, const PreservedAnalyses &PA,
								FunctionAnalysisManager::Invalidator &Inv)
{
	auto PAC = PA.getChecker<OmpStaticShecudleAnalysis>();
	return !PAC.preservedWhenStateless();
}


/* ================ Implementation of OmpStaticShecudleAnalysis ================= */
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
			init_call,
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

/* ================ Implementation of RemoveScheduleRuntimePass ================= */

PreservedAnalyses 
RemoveScheduleRuntimePass::run(Module &M, ModuleAnalysisManager &AM)
{

	auto &kernel_info = AM.getResult<OmpKernelAnalysisPass>(M);

	for (auto F : kernel_info.kernels()) {
		auto &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
		auto R = FAM.getResult<OmpStaticShecudleAnalysis>(*F);
		if (R) {
			R.get_caller()->eraseFromParent();
		}
	}
	
	return PreservedAnalyses::all();

}

#undef DEBUG_TYPE

static void registerModuleAnalyses(ModuleAnalysisManager &MAM)
{
#define MODULE_ANALYSIS(NAME, CREATE_PASS) \
	MAM.registerPass([&] { return CREATE_PASS; });

#include "OmpPasses.def"

}

static void registerFunctionAnalyses(FunctionAnalysisManager &FAM)
{
#define FUNCTION_ANALYSIS(NAME, CREATE_PASS) \
	FAM.registerPass([&] { return CREATE_PASS; });

#include "OmpPasses.def"

}

static void registerLoopAnalyses(LoopAnalysisManager &LAM)
{
#define LOOP_ANALYSIS(NAME, CREATE_PASS) \
	LAM.registerPass([&] { return CREATE_PASS; });

#include "OmpPasses.def"

}

static void registerPostOptimizationPasses(ModulePassManager &PM)
{
	PM.addPass(createModuleToFunctionPassAdaptor(PromotePass()));
	PM.addPass(createModuleToFunctionPassAdaptor(LoopSimplifyPass()));
	PM.addPass(createModuleToFunctionPassAdaptor(
				createFunctionToLoopPassAdaptor(LoopInstSimplifyPass())));
	PM.addPass(createModuleToFunctionPassAdaptor(InstCombinePass()));
	PM.addPass(createModuleToFunctionPassAdaptor(SimplifyCFGPass()));
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
	return {
		LLVM_PLUGIN_API_VERSION, "CGRAOmp", LLVM_VERSION_STRING,
		[](PassBuilder &PB) {
			PB.registerPipelineParsingCallback(
				[](StringRef Name, ModulePassManager &PM,
					ArrayRef<PassBuilder::PipelineElement>){
						if (Name == CGRAOMP_PASS_NAME) {
							// make a pipeline
							//PM.addPass(ADD_LOOP_PASS(LoopRotatePass()));
							// PM.addPass(ADD_FUNC_PASS(LCSSAPass()));
							PM.addPass(RemoveScheduleRuntimePass());
							registerPostOptimizationPasses(PM);

							// Verify->DFGExraction->Runtime Insertion
							PM.addPass(VerifyModulePass());
							PM.addPass(DFGPassHandler());
							return true;
						}
						// Expand analysis passes
						#define MODULE_ANALYSIS(NAME, CREATE_PASS) \
						if (Name == "require<" NAME ">") { \
							PM.addPass( \
								RequireAnalysisPass<std::remove_reference<decltype(CREATE_PASS)>::type, Module>()); \
							return true; \
						}

						#include "OmpPasses.def"
						return false;
				}
			);

			PB.registerAnalysisRegistrationCallback(registerModuleAnalyses);
			PB.registerAnalysisRegistrationCallback(registerFunctionAnalyses);
			PB.registerAnalysisRegistrationCallback(registerLoopAnalyses);

		}
	};
}

