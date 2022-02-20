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
*    Last Modified: 20-02-2022 18:29:22
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
#include "LoopDependencyAnalysis.hpp"

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
	auto module_name = llvm::sys::path::stem(M.getSourceFileName());
	// get parent directory path
	auto parent = llvm::sys::path::parent_path(M.getSourceFileName());
	if (parent == "") {
		// under current dir
		parent = ".";
	}

	// obtain OpenMP kernels
	auto &kernel_info = AM.getResult<OmpKernelAnalysisPass>(M);
	auto &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

	// verify each OpenMP kernel
	for (auto F : kernel_info.kernels()) {
		//verify OpenMP target function
		switch(model->getKind()) {
			case CGRAModel::CGRACategory::Decoupled:
				createDataFlowGraphsForAllKernels<DecoupledVerifyPass>(*F, FAM);
				break;
			case CGRAModel::CGRACategory::TimeMultiplexed:
				createDataFlowGraphsForAllKernels<TimeMultiplexedVerifyPass>(*F, FAM);
				break;
		}
	}
	
	// Optimize and export each generated DFG
	for (auto G : graphs()) {
		auto F = G->getFunction();
		auto L = G->getLoop();
		auto AR = getLSAR(*F, FAM);
		auto &LAM = FAM.getResult<LoopAnalysisManagerFunctionProxy>(*F).getManager();

		// apply DFG Passes
		DPM->run(*G, *L, FAM, LAM, AR);

		// use plain node name istread of pointer values
		if (OptDFGPlainNodeName) {
			G->makeSequentialNodeID();
		}

		// determine export name
		std::string fname, label;
		auto offload_func = kernel_info.getOffloadFunction(F);
		auto md = kernel_info.getMetadata(offload_func);
		
		if (OptUseSimpleDFGName && md != kernel_info.md_end()) {
			// use original function name instead of offloading function name
			label = formatv("{0}_{1}", module_name, md->func_name);
		} else {
			label = formatv("{0}_{1}", module_name, offload_func->getName());
		}

		if (OptDFGFilePrefix != "") {
			fname = formatv("{0}_{1}_{2}.dot", OptDFGFilePrefix, label, L->getName());
		} else {

			fname = formatv("{0}/{1}_{2}.dot", parent, label, L->getName());
		}
		G->setName(label);

		LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX << "Saving DFG: " << fname << "\n");
		// save
		Error E = G->saveAsDotGraph(fname);
		if (E) {
			ExitOnError Exit(ERR_MSG_PREFIX);
			Exit(std::move(E));
		}
	}
	
	return PreservedAnalyses::all();
}


template<typename VerifyPassT>
void DFGPassHandler::createDataFlowGraphsForAllKernels(Function &F, FunctionAnalysisManager &AM)
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
}



// actual implementation for decoupled CGRA
template<>
void DFGPassHandler::createDataFlowGraph<DecoupledVerifyPass>(Function &F, Loop &L, FunctionAnalysisManager &FAM,
									LoopAnalysisManager &LAM,
									LoopStandardAnalysisResults &AR)
{
	//get decoupled memory access for the kernel
	auto &DA = LAM.getResult<DecoupledAnalysisPass>(L, AR);
	//get the CGRA model
	auto &MM = FAM.getResult<ModelManagerFunctionProxy>(F);
	auto *model = MM.getModel();


	DenseMap<Value*,DFGNode*> value_to_node;
	SmallPtrSet<User*, 32> custom_op;
	ValueMap<Value*, Value*> invars_src;

	auto G = new CGRADFG(&F, &L);

	// add memory load
	for (auto inst : DA.get_loads()) {
		auto NewNode = make_mem_node(*inst);
		NewNode = G->addNode(*NewNode);
		value_to_node[inst] = NewNode;
	}
	// add memory store
	for (auto inst : DA.get_stores()) {
		auto NewNode = make_mem_node(*inst);
		NewNode = G->addNode(*NewNode);
		value_to_node[inst] = NewNode;
	}

	// add comp node
	for (auto user : DA.get_comps()) {
		if (auto *inst = dyn_cast<Instruction>(user)) {
			if (auto *imap = model->isSupported(inst)) {
				// if (auto binop = dyn_cast<BinaryOpMapEntry>(imap)) {
				auto NewNode = make_comp_node(inst, imap->getMapName());
				NewNode = G->addNode(*NewNode);
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
		NewNode = G->addNode(*NewNode);
		value_to_node[val] = NewNode;
	}

	auto connect = [&](User *I, int num_operand) {
		DFGNode *dst = value_to_node[I];
		for (int i = 0; i < num_operand; i++) {
			auto operand = I->getOperand(i);
			DFGNode* src = value_to_node[operand];

			if (src) {
				auto NewEdge = new DFGEdge(*dst, i);
				assert(G->connect(*src, *dst, *NewEdge) && "Trying to connect non-exist nodes");
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

	addGraph(G);
}


// actual implementation for time-multiplexed CGRA
template<typename VerifyPassT>
void DFGPassHandler::createDataFlowGraph(Function &F, Loop &L, FunctionAnalysisManager &FAM,
									LoopAnalysisManager &LAM,
									LoopStandardAnalysisResults &AR)
{
	//get the CGRA model
	auto &MM = FAM.getResult<ModelManagerFunctionProxy>(F);
	auto *model = MM.getModel();

	// get verification result
	VerifyResult& verify_result = FAM.getResult<TimeMultiplexedVerifyPass>(F);
	LoopVerifyResult* LVR = verify_result.getLoopVerifyResult(&L);
	assert(LVR && "Failed to get loop verify result");

	// collections
	DenseMap<Value*,DFGNode*> value_to_node; // map to Value -> DFGNode
	SmallPtrSet<User*, 32> custom_op;
	DenseMap<Value*, MemoryLoopDependency*> memdep_map;
	SmallPtrSet<Instruction*, 32> kernel_inst;
	DenseMap<PHINode*, LoopDependency*> idv_phis, lc_dep_phis;
	SmallPtrSet<BasicBlock*, 32> all_blocks(L.block_begin(), L.block_end());
	SmallPtrSet<GetElementPtrInst*, 32> gep_set;

	// to check whether the node exists or not
	auto is_node_exist = [&](Value *V) {
		return value_to_node.find(V) != value_to_node.end();
	};

	// instance of the graph
	auto G = new CGRADFG(&F, &L);

	// get loop dependency info
	auto LD = LAM.getResult<LoopDependencyAnalysisPass>(L, AR);
	

	// get all induction variable
	for (auto idv_dep : LD.idv_deps()) {
		idv_phis[idv_dep->getPhi()] = idv_dep;
	}
	// get all loop carried delepdency
	for (auto lc_dep : LD.lc_deps()) {
		lc_dep_phis[lc_dep->getPhi()] = lc_dep;
	}

	// lambdas
	// to check the phinode is associated with loop dependency
	auto phi_contained = [](DenseMap<PHINode*, LoopDependency*> &M, PHINode* phi) -> LoopDependency* {
		if (M.find(phi) != M.end()) {
			return M[phi];
		} else {
			return nullptr;
		}
	};

	// to check the loaded value depends on the stored value in a previous iteration
	auto is_memdep = [&](Value *load) {
		return memdep_map.find(load) != memdep_map.end();
	};
	
	// get instructions for loop control
	Instruction* BackBranch = LVR->getBackBranch(&L);
	Instruction* LoopCond = LVR->getBackCondition(&L);


	// make a node for each instruction in the kernel
	for (auto &BB : all_blocks) {
		for (auto &I : *BB) {
			Instruction* inst = &I;
			// check if it is special instruction like phi, branch of loop control
			if (auto phi = dyn_cast<PHINode>(inst)) {
				if (phi_contained(idv_phis, phi) || phi_contained(lc_dep_phis, phi)) {
					continue;
				}
			} else if (auto gep = dyn_cast<GetElementPtrInst>(inst)) {
				gep_set.insert(gep);
				continue;
			} else if (inst == BackBranch || inst == LoopCond) {
				continue;
			}

			if (auto *imap = model->isSupported(inst)) {
				auto NewNode = make_comp_node(inst, imap->getMapName());
				NewNode = G->addNode(*NewNode);
				value_to_node[inst] = NewNode;
				if (auto customop = dyn_cast<CustomInstMapEntry>(imap)) {
					custom_op.insert(inst);
				}
				kernel_inst.insert(inst);
			} else {
				LLVM_DEBUG(dbgs() << ERR_DEBUG_PREFIX 
					<< "Unsupported instructions are included");
				DEBUG_WITH_TYPE(VerboseDebug, 
					inst->print(dbgs() << "\t");
					dbgs() << "\n";
				);
			}
		}
	}

	// find edges coming from outside of the loop or contants
	for (auto sink : kernel_inst) {
		for (auto *src : sink->operand_values()) {
			if (auto src_inst = dyn_cast<Instruction>(src)) {
				if (!all_blocks.contains(src_inst->getParent())) {
					// global data
					auto NewNode = make_global_node(src_inst);
					NewNode = G->addNode(*NewNode);
					value_to_node[src_inst] = NewNode;
				}
			} else if (auto src_const = dyn_cast<Constant>(src)) {
				// constant data
				auto NewNode = make_const_node(src_const);
				NewNode = G->addNode(*NewNode);
				value_to_node[src_const] = NewNode;
			} else if (auto src_arg = dyn_cast<Argument>(src)) {
				// argument is also global
				auto NewNode = make_global_node(src_arg);
				NewNode = G->addNode(*NewNode);
				value_to_node[src_arg] = NewNode;
			} else {
				LLVM_DEBUG(dbgs() << ERR_DEBUG_PREFIX 
					<< "Incoming edge from unexpected element");
				DEBUG_WITH_TYPE(VerboseDebug, 
					src->print(dbgs() << "\t");
					dbgs() << "\n";
				);
			}
		}
	}

	// common routing to make connection for inter-loop dependency
	// Args:
	//    dep: LoopDependency (which contains def and use instruction, init data, etc)
	//    phi: PhiNode to select either init data or data from previous iteration
	auto connect_to_loop_dep_node = [&,this](LoopDependency *dep, PHINode* phi) {
		Instruction* I = dep->getDefInst();
		DFGNode* self = value_to_node[I];
		int last_operand = I->getNumOperands();

		if (custom_op.contains(I)) last_operand--; // the last is function to be called
		for (int i = 0; i < last_operand; i++) {
			auto operand = I->getOperand(i);
			if (operand == phi) {
				// if it depends on itself, connects to def instruction
				auto NewEdge = new LoopDependencyEdge(*self, i, dep->getDistance());
				assert(G->connect(*self, *self, *NewEdge) && "Trying to connect non-exist nodes");
				// making other instructions refer this instruction instead of the phi node
				value_to_node[phi] = self;
				// also making init edge
				auto init_data = dep->getInit();
				DFGNode* InitNode;
				if (!is_node_exist(init_data)) {
					if (isa<Constant>(*init_data)) {
						InitNode = make_const_node(init_data);
						value_to_node[init_data] = InitNode;
						InitNode = G->addNode(*InitNode);
					} else {
						InitNode = make_global_node(init_data);
						value_to_node[init_data] = InitNode;
						InitNode = G->addNode(*InitNode);
					}
				} else {
					InitNode = value_to_node[init_data];
				}
				auto InitEdge = new InitDataEdge(*self, i);
				assert(G->connect(*InitNode, *self, *InitEdge) && "Trying to connect non-exist nodes");
			} else {
				// the operand is intra-loop dependency, so create normal edges
				DFGNode* src = value_to_node[operand];
				auto NewEdge = new DFGEdge(*self, i);
				assert(G->connect(*src, *self, *NewEdge) && "Trying to connect non-exist nodes");
			}
		}
	};

	// get memory dependency
	for (auto dep : LD.mem_deps()) {
		auto mem_dep = static_cast<MemoryLoopDependency*>(dep);
		auto load = mem_dep->getLoad();
		memdep_map[load] = mem_dep;
	}

	// make connection for induction variables
	for (auto item : idv_phis) {
		auto phi = item.first;
		auto dep = item.second;
		connect_to_loop_dep_node(dep, phi);
		kernel_inst.erase(dep->getDefInst());
	}

	// make connection for inter-loop dependecies
	for (auto item : lc_dep_phis) {
		auto phi = item.first;
		auto dep = item.second;
		connect_to_loop_dep_node(dep, phi);
		kernel_inst.erase(dep->getDefInst());
	}

	// make data-flow for GEP instructions
	// Note: it assumes continuous memory allocation for multidimensional array 
	for (auto gep : gep_set) {
		auto ptr = gep->getPointerOperand();
		auto sizes = getArrayElementSizes(gep->getSourceElementType());
		SmallVector<int> inc;
		for (auto i = sizes.rbegin(); i != sizes.rend(); i++) {
			int total = 1;
			for (auto j = sizes.rbegin(); j <= i; j++) {
				total *= *j;
			}
			inc.insert(inc.begin(), total);
		}
		inc.emplace_back(1);

		DFGNode *base_addr;
		if (!is_node_exist(ptr)) {
			base_addr = new GlobalDataNode(ptr);
			base_addr = G->addNode(*base_addr);
			value_to_node[ptr] = base_addr;
		} else {
			base_addr = value_to_node[ptr];
		}

		int i = 0;
		DFGNode* last = nullptr;
		for (auto idx = gep->idx_begin(); idx != gep->idx_end(); idx++) {
			if (auto inst_indice = dyn_cast<Instruction>(idx)) {
				if (all_blocks.contains(inst_indice->getParent())) {
					auto indice_node = value_to_node[inst_indice];
					DFGNode* add = new GEPAddNode(gep);
					add = G->addNode(*add);
					DFGEdge* NewEdge = new DFGEdge(*add);
					assert(G->connect(*indice_node, *add, *NewEdge) && "Trying to connect non-exist nodes");
					NewEdge = new DFGEdge(*add);
					assert(G->connect(*base_addr, *add, *NewEdge) && "Trying to connect non-exist nodes");
					last = add;
				}
			}
		}
		if (last) {
			value_to_node[gep] = last;
		}
	} 

	// make connections for remaining nodes
	for (auto inst : kernel_inst) {
		DFGNode* dst = value_to_node[inst];
		int last_operand = inst->getNumOperands();
		if (custom_op.contains(inst)) last_operand--;
		for (int i = 0; i < last_operand; i++) {
			auto operand = inst->getOperand(i);
			DFGEdge *NewEdge;
			if (is_memdep(operand)) {
				// connect mem load for init edges
				auto InitEdge = new InitDataEdge(*dst, i);
				G->connect(*(value_to_node[operand]), *dst, *InitEdge);

				// connect to def node instead of memory load
				auto memdep = memdep_map[operand];
				operand = memdep->getDef();
				NewEdge = new LoopDependencyEdge(*dst, i, memdep->getDistance());
				

			} else {
				NewEdge = new DFGEdge(*dst, i);
			}
			if (is_node_exist(operand)) {
				DFGNode* src = value_to_node[operand];
				assert(G->connect(*src, *dst, *NewEdge) && "Trying to connect non-exist nodes");
			} else {
				operand->print(errs() << "not exist ");
				errs() << "\n";
			}
		}
	}



	addGraph(G);
}



#undef DEBUG_TYPE