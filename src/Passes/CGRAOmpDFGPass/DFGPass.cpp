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
*    Last Modified: 11-02-2022 00:51:06
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
	auto &kernels = AM.getResult<OmpKernelAnalysisPass>(M);

	// verify each OpenMP kernel
	for (auto &it : kernels) {
		//verify OpenMP target function
		auto *F = it.second;
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
	for (auto L : verify_result.kernels()) {
		// errs() << L->getName() << "\n";
		createDataFlowGraph(F, *L, AM, LAM, AR);
	}
	return false;
}

bool DFGPassHandler::createDataFlowGraph(Function &F, Loop &L, FunctionAnalysisManager &FAM,
									LoopAnalysisManager &LAM,
									LoopStandardAnalysisResults &AR)
{
	//get decoupled memory access for the kernel
	auto &DA = LAM.getResult<DecoupledAnalysisPass>(L, AR);
	//get the CGRA model
	auto &MM = FAM.getResult<ModelManagerFunctionProxy>(F);
	auto *model = MM.getModel();

	// ---------- test ----------
	// CGRADFG G;
	// G.setName(L.getName().str());

	// SmallPtrSet<Instruction*, 32> list;
	// DenseMap<User*,DFGNode*> user_to_node;

	// for (auto &BB : L.getBlocks()) {
	// 	for (auto &I : *BB) {
	// 		list.insert(&I);
	// 		I.dump();
	// 	}
	// }

	// errs() << "\n ---------- report ---------- \n";

	// int count = 0;
	// int edge_count = 0;
	// int lc_edge = 0;
	// int gep_edge = 0;
	// int glb_count = 0;
	// for (auto *I : list) {
	// 	if (!isa<PHINode>(*I) && !isa<GetElementPtrInst>(*I) && 
	// 		!isa<CmpInst>(*I) && !isa<BranchInst>(*I)) {
	// 		for (int i = 0; i < I->getNumOperands(); i++) {
	// 			auto operand = I->getOperand(i);
	// 			if (!isa<Constant>(*operand)) {
	// 				if (auto oi = dyn_cast<Instruction>(operand)) {
	// 					if (!list.contains(oi)) {
	// 						glb_count++;
	// 					}
	// 				}
	// 				if (isa<PHINode>(*operand)) {
	// 					lc_edge++;
	// 				} else if (isa<GetElementPtrInst>(*operand)) {
	// 					gep_edge++;
	// 				} else {
	// 					edge_count++;
	// 				}
	// 			}
	// 		}
	// 		I->print(errs());
	// 		errs() << formatv(" --- {0} {1} {2} {3}\n", edge_count, lc_edge, gep_edge, glb_count);
	// 		count++;
	// 	}
	// }
	// errs() << "count " << count << formatv(" --- {0} {1} {2} {3}\n", edge_count, lc_edge, gep_edge, glb_count);
	// errs() << "total node " << count + gep_edge << " " << edge_count + gep_edge * 2 << " " << lc_edge << "\n";
	// for (auto *I : list) {
	// 	if (isa<GetElementPtrInst>(*I)) {
	// 		I->dump();
	// 	}
	// }

	// ---------- end test ----------

	// for (auto access : DA.loads()) {
	// 	access->dump();
	// }
	// for (auto access : DA.stores()) {
	// 	access->dump();
	// }

	SmallVector<Value*> NotReached(DA.stores());
	SmallPtrSet<User*, 32> stores(DA.store_begin(), DA.store_end());
	SmallPtrSet<User*, 32> traversed;
	DenseMap<User*,DFGNode*> user_to_node;
	SmallPtrSet<User*, 32> custom_op;

	std::queue<User*> fifo;
	CGRADFG G;
	G.setName(L.getName().str());

	// push memory loads to fifo
	// errs() << "Loads\n";
	for (User *v : DA.loads()) {
		fifo.push(v);
		traversed.insert(v);
		// v->dump();
	}
	// errs() << "\n";
	// traverse
	// in this traversal, only instructions can appear
	while (!fifo.empty()) {
		User *v = fifo.front();
		fifo.pop();

		if (auto *inst = dyn_cast<Instruction>(v)) {
			if (auto *imap = model->isSupported(inst)) {
				if (auto binop = dyn_cast<BinaryOpMapEntry>(imap)) {
					// Computational node
					auto NewNode = make_comp_node(inst, imap->getMapName());
					NewNode = G.addNode(*NewNode);
					user_to_node[v] = NewNode;
					// inst->print(errs());
					// // errs() << " " << inst->hasAllowReassoc() << " "<< "\n";
					// errs() << "\n";
				} else if (auto customop = dyn_cast<CustomInstMapEntry>(imap)) {
					// errs() << "custom\n";
					auto NewNode = make_comp_node(inst, customop->getMapName());
					NewNode = G.addNode(*NewNode);
					user_to_node[v] = NewNode;
					custom_op.insert(v);
				}
			} else if (isMemAccess(*inst)) {
				// Memory access node
				auto NewNode = make_mem_node(*inst);
				NewNode = G.addNode(*NewNode);
				user_to_node[v] = NewNode;
			} else {
				inst->print(errs() << "LLVM instruction ");
				errs() << " is not supported in the target CGRA\n";
			}
		} else {
			LLVM_DEBUG(dbgs() << WARN_DEBUG_PREFIX << "unexpected IR ";
						v->print(dbgs()));
		}
		// store means the end of DFG so not traverse any more
		if (!stores.contains(v)) {
			for (auto *suc : v->users()) {
				if (!traversed.contains(suc)) {
					fifo.push(suc);
					traversed.insert(suc);
				}
			}
		}
	}
	// search for constant value while analyzing edges
	for (auto entry : make_range(user_to_node.begin(), user_to_node.end())) {
		auto user = entry.first;
		DFGNode *dst = entry.second;
		int last_operand = user->getNumOperands();
		if (custom_op.contains(user) || stores.contains(user)) {
			last_operand--;
		}
		if (isa<LoadInst>(*user)) {
			continue;
		}
		for (int i = 0; i < last_operand; i++) {
			DFGNode* src = nullptr;
			if (auto operand = dyn_cast<User>(user->getOperand(i))) {
				if (user_to_node.find(operand) != user_to_node.end()) {
					// DFG contains it
					src = user_to_node[operand];
				} else if (auto *c = dyn_cast<Constant>(operand)) {
					// Constant value
					src = make_const_node(c);
					src = G.addNode(*src);
				} else {
					// data defined outside the loop
					Value *last = operand;
					while (isa<TruncInst>(*last) || isa<BitCastInst>(*last)) {
						if (auto next = dyn_cast<User>(last)->getOperand(0)) {
							last = next;
						}
					}
					src = make_const_node(last);
					src = G.addNode(*src);
				}
				if (src) {
					auto NewEdge = new DFGEdge(*dst, i);
					assert(G.connect(*src, *dst, *NewEdge) && "Trying to connect non-exist nodes");
				}
			} else {
				LLVM_DEBUG(dbgs() << WARN_DEBUG_PREFIX;
							user->getOperand(i)->print(dbgs() << " Non User type ");
							user->print(dbgs() << " is an operand of "); dbgs() << "\n");
			}
		}
	}

	if (OptDFGPlainNodeName) {
		G.makeSequentialNodeID();
	}


	// apply tree height reduction if needed
	DPM->run(G, L, FAM, LAM, AR);

	std::string fname;
	if (OptDFGFilePrefix != "") {
		fname = formatv("{0}_{1}_{2}.dot", OptDFGFilePrefix, F.getName(), L.getName());
	} else {
		auto module_name = llvm::sys::path::stem(F.getParent()->getSourceFileName());
		auto parent = llvm::sys::path::parent_path(F.getParent()->getSourceFileName());
		if (parent == "") {
			parent = ".";
		}
		fname = formatv("{0}/{1}_{2}_{3}.dot", parent, module_name,
						 F.getName(), L.getName());
	}
	LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX << "Saving DFG: " << fname << "\n");
	Error E = G.saveAsDotGraph(fname);
	if (E) {
		ExitOnError Exit(ERR_MSG_PREFIX);
		Exit(std::move(E));
	}

	// for (auto *v : post_order(&G)) {
	// 	errs() << v->getUniqueName() << "\n";
	// }


	return true;
}



#undef DEBUG_TYPE