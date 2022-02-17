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
*    File:          /src/Passes/CGRAOmpDFGPass/DFGPass.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  15-12-2021 10:40:31
*    Last Modified: 18-02-2022 04:19:44
*/

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/User.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/CFG.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Debug.h"

#include "llvm/IR/InstrTypes.h"
#include "llvm/ADT/BreadthFirstIterator.h"

#include "common.hpp"
#include "DFGPass.hpp"
#include "CGRAOmpPass.hpp"
#include "VerifyPass.hpp"
#include "CGRADataFlowGraph.hpp"
#include "OptionPlugin.hpp"
#include "DecoupledAnalysis.hpp"

#include "BalanceTree.hpp"

#include <queue>
#include <system_error>


using namespace llvm;
using namespace CGRAOmp;

#define DEBUG_TYPE "cgraomp"

static const char *VerboseDebug = DEBUG_TYPE "-verbose";


DFGPassBuilder::DFGPassBuilder()
{
	// register built in DFGPasses
	callback_list.push_back(
		[](StringRef Name, DFGPassManager &PM) {
			#define DFG_PASS(NAME, CREATE_PASS) \
				if (Name ==  NAME) { \
					PM.addPass(CREATE_PASS); \
					return true; \
				}
			#include "BuiltInDFGPasses.def"
			return false;
		});

	// search for plugins
	Error E = search_callback();
	if (E) {
		ExitOnError Exit(ERR_MSG_PREFIX);
		Exit(std::move(E));
	}
}

void DFGPassBuilder::registerPipelineParsingCallback(const CallBackT &C) {
	callback_list.push_back(C);
}

/**
 * @details 
 * An exmaple of the call back function:
 * @code {.c}
extern "C" ::CGRAOmp::DFGPassPluginLibraryInfo getDFGPassPluginInfo() {
	return { "Plugin name", 
		[](DFGPassBuilder &PB) {
			PB.registerPipelineParsingCallback(
				[](StringRef Name, DFGPassManager &PM) {
					if (Name == "my-dfg-pass") {
						PM.addPass(MyDFGPass());
						return true;
					}
					return false;
				}
			);
		}
	};
}
 * @endcode
*/
Error DFGPassBuilder::search_callback()
{
	error_code EC;
	std::string ErrMsg;
	for (auto lib_path : OptDFGPassPlugin) {
		// load lib
		if (sys::DynamicLibrary::LoadLibraryPermanently(lib_path.c_str(), &ErrMsg)) {
			return make_error<StringError>(ErrMsg, EC);
		}
		// search for callback
		void *callback = nullptr;
		if (!(callback = sys::DynamicLibrary::SearchForAddressOfSymbol("getDFGPassPluginInfo"))) {
			return make_error<StringError>(formatv("getDFGPassPluginInfo function is not impelemnted in {0}", lib_path), EC);
		}
		// register the callback func for parsing pipeline
		auto info = reinterpret_cast<DFGPassPluginLibraryInfo(*)()>(callback)();
		info.RegisterPassBuilderCallbacks(*this);
		LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX << "A plugin of DFG Pass \"" <<
					info.PluginName << "\" is loaded\n");
	}

	return ErrorSuccess();
}

Error DFGPassBuilder::parsePassPipeline(DFGPassManager &DPM, ArrayRef<std::string> PipelineTexts)
{
	error_code EC;
	for (auto pass_name : PipelineTexts) {
		auto found = false;
		for (auto callback : callback_list) {
			if (callback(pass_name, DPM)) {
				found = true;
			}
		}
		if (!found) {
			return make_error<StringError>(formatv("{0} not found", pass_name), EC);
		}
	}
	return ErrorSuccess();
}

bool DFGPassManager::run(CGRADFG &G, Loop &L, FunctionAnalysisManager &FAM,
									LoopAnalysisManager &LAM,
									LoopStandardAnalysisResults &AR)
{
	bool changed = false;
	// iterate for all the passes
	for (auto pass : pipeline) {
		LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX << "applying " << pass->name() << "\n");
		changed |= pass->run(G, L, FAM, LAM, AR);
	}
	return changed;
}

DFGPassHandler::DFGPassHandler() : DPB(new DFGPassBuilder()), DPM(new DFGPassManager())
{
	Error E = DPB->parsePassPipeline(*DPM, OptDFGPassPipeline);
	if (E) {
		ExitOnError Exit(ERR_MSG_PREFIX);
		Exit(std::move(E));
	}
}

PreservedAnalyses DFGPassHandler::run(Module &M, ModuleAnalysisManager &AM)
{
	auto &MM = AM.getResult<ModelManagerPass>(M);
	auto model = MM.getModel();

	// obtain OpenMP kernels
	auto &kernel_info = AM.getResult<OmpKernelAnalysisPass>(M);

	// verify each OpenMP kernel
	for (auto F : kernel_info.kernels()) {
		//verify OpenMP target function
		auto &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
		switch(model->getKind()) {
			case CGRAModel::CGRACategory::Decoupled:
				createDataFlowGraphsForAllKernels<DecoupledVerifyPass>(*F, FAM);
				break;
			case CGRAModel::CGRACategory::TimeMultiplexed:
				assert("not implemented now");
				break;
		}
	}

	return PreservedAnalyses::all();
}


template<typename VerifyPassT>
bool DFGPassHandler::createDataFlowGraphsForAllKernels(Function &F, FunctionAnalysisManager &AM)
{
	VerifyResult& verify_result = AM.getResult<VerifyPassT>(F);
	auto AR = getLSAR(F, AM);
	auto &LAM = AM.getResult<LoopAnalysisManagerFunctionProxy>(F).getManager();

	LLVM_DEBUG(
		if (verify_result.kernels().empty()) {
			dbgs() << WARN_DEBUG_PREFIX 
				<< formatv("{0} does not have any valid kernels\n", 
					F.getName());
		}
	);

	for (auto L : verify_result.kernels()) {
		createDataFlowGraph<VerifyPassT>(F, *L, AM, LAM, AR);
	}
	return false;
}

template<typename VerifyPassT>
bool DFGPassHandler::createDataFlowGraph(Function &F, Loop &L, FunctionAnalysisManager &FAM,
									LoopAnalysisManager &LAM,
									LoopStandardAnalysisResults &AR)
{
	//get decoupled memory access for the kernel
	auto &DA = LAM.getResult<DecoupledAnalysisPass>(L, AR);
	//get the CGRA model
	auto &MM = FAM.getResult<ModelManagerFunctionProxy>(F);
	auto *model = MM.getModel();

	auto &MAMProxy = FAM.getResult<ModuleAnalysisManagerFunctionProxy>(F);
	auto &M = *(F.getParent());
	auto *kernel_info = MAMProxy.getCachedResult<OmpKernelAnalysisPass>(M);
	assert(kernel_info && "OmpKernelAnalysisiPass must be executed before DFGPass");

	DenseMap<Value*,DFGNode*> value_to_node;
	SmallPtrSet<User*, 32> custom_op;
	ValueMap<Value*, Value*> invars_src;

	CGRADFG G;

	// add memory load
	for (auto inst : DA.get_loads()) {
		auto NewNode = make_mem_node(*inst);
		NewNode = G.addNode(*NewNode);
		value_to_node[inst] = NewNode;
	}
	// add memory store
	for (auto inst : DA.get_stores()) {
		auto NewNode = make_mem_node(*inst);
		NewNode = G.addNode(*NewNode);
		value_to_node[inst] = NewNode;
	}

	// add comp node
	for (auto user : DA.get_comps()) {
		if (auto *inst = dyn_cast<Instruction>(user)) {
			if (auto *imap = model->isSupported(inst)) {
				// if (auto binop = dyn_cast<BinaryOpMapEntry>(imap)) {
				auto NewNode = make_comp_node(inst, imap->getMapName());
				NewNode = G.addNode(*NewNode);
				value_to_node[inst] = NewNode;

				if (auto customop = dyn_cast<CustomInstMapEntry>(imap)) {

					custom_op.insert(inst);
				}
			} else {
				LLVM_DEBUG(dbgs() << ERR_DEBUG_PREFIX 
					<< "Unsupported instructions are included");
				DEBUG_WITH_TYPE(VerboseDebug, 
					inst->print(dbgs() << "\t");
					dbgs() << "\n";
				);
			}
		} else {
			LLVM_DEBUG(dbgs() << ERR_DEBUG_PREFIX 
				<< "computational part of decoupling result invalid");
			DEBUG_WITH_TYPE(VerboseDebug, 
				inst->print(dbgs() << "\t");
				dbgs() << " is not instruction\n";
			);
		}
	}

	// add loop invariants as const nodes
	for (auto val : DA.get_invars()) {
		DFGNode* NewNode;
		// get node actually connected to comp or store node
		if (auto skip_seq = DA.getSkipSequence(val)) {
			invars_src[val] = skip_seq->back();
			NewNode = make_const_node(val, skip_seq);
		} else {
			invars_src[val] = val;
			NewNode = make_const_node(val);
		}
		NewNode = G.addNode(*NewNode);
		value_to_node[val] = NewNode;
	}

	auto connect = [&](User *I, int num_operand) {
		DFGNode *dst = value_to_node[I];
		for (int i = 0; i < num_operand; i++) {
			auto operand = I->getOperand(i);
			DFGNode* src = value_to_node[operand];

			if (src) {
				auto NewEdge = new DFGEdge(*dst, i);
				assert(G.connect(*src, *dst, *NewEdge) && "Trying to connect non-exist nodes");
			} else {
				LLVM_DEBUG(
					operand->print(dbgs() << ERR_DEBUG_PREFIX 
					<< "graph node for ");
					dbgs() << " is not created\n";
				);
			}
		}
	};
	// add edges to comp node
	for (auto inst : DA.get_comps()) {
		int last_operand = inst->getNumOperands();
		if (custom_op.contains(inst)) last_operand--;
		connect(inst, last_operand);
	}

	// add edges to mem store node
	for (auto inst : DA.get_stores()) {
		connect(inst, inst->getNumOperands() - 1);
	}


	if (OptDFGPlainNodeName) {
		G.makeSequentialNodeID();
	}


	// apply tree height reduction if needed
	DPM->run(G, L, FAM, LAM, AR);

	// determine export name
	std::string fname, label;
	auto offload_func = kernel_info->getOffloadFunction(&F);
	auto md = kernel_info->getMetadata(offload_func);
	auto module_name = llvm::sys::path::stem(F.getParent()->getSourceFileName());

	if (OptUseSimpleDFGName && md != kernel_info->md_end()) {
		// use original function name instead of offloading function name
		label = formatv("{0}_{1}", module_name, md->func_name);
	} else {
		label = formatv("{0}_{1}", module_name, offload_func->getName());
	}

	if (OptDFGFilePrefix != "") {
		fname = formatv("{0}_{1}_{2}.dot", OptDFGFilePrefix, label, L.getName());
	} else {
		auto parent = llvm::sys::path::parent_path(F.getParent()->getSourceFileName());
		if (parent == "") {
			parent = ".";
		}
		fname = formatv("{0}/{1}_{2}.dot", parent, label, L.getName());
	}
	G.setName(label);

	LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX << "Saving DFG: " << fname << "\n");
	Error E = G.saveAsDotGraph(fname);
	if (E) {
		ExitOnError Exit(ERR_MSG_PREFIX);
		Exit(std::move(E));
	}

	return true;
}



#undef DEBUG_TYPE