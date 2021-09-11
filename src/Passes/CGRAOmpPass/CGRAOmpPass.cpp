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
*    Last Modified: 11-09-2021 18:12:13
*/
#include "CGRAOmpPass.hpp"
#include "VerifyPass.hpp"
#include "CGRAModel.hpp"
#include "CGRADataFlowGraph.hpp"

#include "llvm/IR/Function.h"
#include "llvm/ADT/Statistic.h"

#include "llvm/IR/Instruction.h"

using namespace llvm;
using namespace CGRAOmp;

#define DEBUG_TYPE "cgraomp"

// # of successfully exracted DFGs
STATISTIC(num_dfg, "the number of extracted DFGs");

PreservedAnalyses CGRAOmpPass::run(Module &M, ModuleAnalysisManager &AM)
{
	errs() << "CGRAOmpPass is called\n";

	auto model = parseCGRASetting("share/presets/decoupled_affine_AG.json");
	if (model) {

	} else {
		ExitOnError Exit(ERR_MSG_PREFIX);
		Exit(std::move(model.takeError()));
	}

	for (auto &F : M) {
		errs() << F.getName() << "\n";


		// //memo. -Wno-unknown-assumption
		// auto attrs = F.getAttributes();
		// for (auto attr_set : make_range(attrs.begin(), attrs.end())) {
		// 	auto attr = attr_set.getAttribute("llvm.assume");
		// 	errs() << F.getName() << " " << attr.isValid() << " "
		// 	<< attr.getAsString() << "\n";
		// }
		// Skip declaration
		if (F.isDeclaration()) continue;

		auto &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
		errs() << "get FAM\n";
		auto verify_res = FAM.getResult<VerifyPass>(F);
		errs() << "get Rest\n";
		errs() << verify_res << "\n";
		if (verify_res) num_dfg++;


		for (auto &BB : F) {
			for (auto &I : BB) {
				I.print(errs());
				if (auto entry = model->isSupported(&I)) {
					errs() << "\tSupported" << "\n";
					entry->dump();
				} else {
					errs() << "\tUnsupported\n";
				}
			}
		}
		errs() << "fin\n";
	}

	auto dfg = CGRADFG();
	SmallVector<DFGNode*> IDtoNode;
	InstMap inst_map;
	cantFail(inst_map.add_generic_inst("add"));
	for (int i = 0; i < 5; i++) {
		auto *NewNode = new ComputeNode(i, inst_map.find("add"));
		IDtoNode.push_back(NewNode);
		dfg.addNode(*NewNode);
	}
	SmallVector<pair<int,int>> edge_list = {{0, 3}, {1, 3}, {2, 4}, {3, 4}};
	for (auto e : edge_list) {
		auto *NewEdge = new DFGEdge(*IDtoNode[e.second]);
		dfg.connect(*IDtoNode[e.first], *IDtoNode[e.second], *NewEdge);
	}

	errs() << "DFS\n";
	for (auto &v : depth_first(&dfg)) {
		//skip virtual root
		if (*v == dfg.getRoot()) continue;
			errs() << formatv("\tID {0}\n", v->getID());
	}

	if (auto E = dfg.saveAsDotGraph("graph_test.dot")) {
		ExitOnError Exit(ERR_MSG_PREFIX);
		Exit(std::move(E));
	}


//	CGRAModel hoge;
	return PreservedAnalyses::all();
}


#undef DEBUG_TYPE

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
	return {
		LLVM_PLUGIN_API_VERSION, "CGRAOmp", "v0.1",
		[](PassBuilder &PB) {
			PB.registerPipelineParsingCallback(
				[](StringRef Name, ModulePassManager &PM,
					ArrayRef<PassBuilder::PipelineElement>){
						if(Name == "cgraomp"){
							PM.addPass(CGRAOmpPass());
							return true;
						}
						return false;
				}
			);
		}
	};
}
