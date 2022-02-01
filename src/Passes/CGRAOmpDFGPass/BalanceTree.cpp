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
*    File:          /src/Passes/CGRAOmpDFGPass/BalanceTree.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  01-02-2022 11:44:11
*    Last Modified: 01-02-2022 19:51:45
*/
#include "DFGPass.hpp"
#include "BalanceTree.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/BreadthFirstIterator.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/IR/Instruction.h"
#include "llvm/ADT/PriorityQueue.h"

#include <algorithm>
#include <queue>

using namespace llvm;
using namespace CGRAOmp;
using namespace std;

/**
 * @details It is a similar setting to precedence table of @em clang
 * But the lower level means the higher priority
 * @see clang/Basic/OperatorPrecedence.h
 */
map<int, int> BalanceTree::OperatorPrecedence({
	PREC_PAIR(FMul, 0), PREC_PAIR(Mul, 0), // *
	PREC_PAIR(FAdd, 1), PREC_PAIR(Add, 1), // +
	PREC_PAIR(And, 2), // &
	PREC_PAIR(Xor, 3), // ^
	PREC_PAIR(Or, 4)   // |
});


void BalanceTree::run(CGRADFG &G, Loop &L, FunctionAnalysisManager &FAM,
									LoopAnalysisManager &LAM,
									LoopStandardAnalysisResults &AR)
{
	// reset status
	visited.clear();
	visited.grow(G.size() - 1);
	weight.clear();
	candidate_set.clear();

	initWeight(G);
	for (auto root : findRootCandidates(G)) {
		toBalanced(G, root);
	}
}


void BalanceTree::initWeight(CGRADFG &G)
{
	for (auto *N : G) {
		if (*N == G.getRoot()) continue;
		weight[N] = 0;
		visited[N->getID()] = false;
	}
	// visit topological order
	for (auto *N : breadth_first(&G)) {
		// skip if it is vroot or constant node
		if (*N == G.getRoot()) continue;
		if (isa<ConstantNode>(*N)) continue;
		// sum up weight of sub-tree
		SmallVector<CGRADFG::EdgeInfoType> in_edges;
		if (G.findIncomingEdgesToNode(*N, in_edges, true)) {
			int sum = 0;
			for (auto EI : in_edges) {
				auto Src = EI.first;
				sum += weight[Src];
			}
			weight[N] = sum;
		} else {
			weight[N] = 1;
		}
	}
}

SmallVector<ComputeNode*> BalanceTree::findRootCandidates(CGRADFG &G)
{
	SmallVector<ComputeNode*> candidates;
	for (auto *N : make_range(G.begin(), G.end())) {
		// only computational node can be a candidate
		if (auto *comp_node = dyn_cast<ComputeNode>(N)) {
			auto inst = comp_node->getInst();
			auto isAppend = false;
			// check its associativity and commutativity
			// thus, float instructions are targeted when fast-math (or options like that) is specified
			if (inst->isAssociative() && inst->isCommutative()) {
				int use_count = comp_node->getEdges().size();
				if (use_count > 1) {
					// it is a candicate of root
					isAppend = true;
				} else if (use_count == 1) {
					auto use = &(comp_node->getEdges().front()->getTargetNode());
					if (auto use_comp_node =  dyn_cast<ComputeNode>(use)) {
						// the use is different type of instruction
						auto use_inst = use_comp_node->getInst();
						if (inst->getOpcode() != use_inst->getOpcode()) {
							isAppend = true;
						}
					} else {
						isAppend = true;
					}
				}
			}
			if (isAppend) {
				candidates.push_back(comp_node);
				candidate_set.insert(comp_node);
			}
		}
	}

	// sort in order of operator precedence
	std::sort(candidates.begin(), candidates.end(),
				[&](const auto &lhs, const auto &rhs){ 
					return getOperatorPrecedence(lhs) < getOperatorPrecedence(rhs);
				});
	return candidates;
}

void BalanceTree::toBalanced(CGRADFG &G, ComputeNode* Root)
{
	queue<DFGNode*> worklist;
	SmallVector<DFGNode*> replaced;
	auto compare = [&](DFGNode* lhs, DFGNode *rhs) {
		return weight[lhs] > weight[rhs];
	};
	PriorityQueue<DFGNode*, vector<DFGNode*>, decltype(compare)> leaves {compare};

	// for temporary storage
	SmallVector<CGRADFG::EdgeInfoType> in_edges;

	// mark as visited
	visited[Root->getID()] = true;

	errs() << "Balancing at " << Root->getUniqueName() << "\n";

	// Add predecessors of root to worklist
	in_edges.clear();
	if (G.findIncomingEdgesToNode(*Root, in_edges, true)) {
		for (auto EI : in_edges) {
			worklist.push(EI.first);
		}
		in_edges.clear();
	}

	while (!worklist.empty()) {
		auto T = worklist.front();
		worklist.pop();
		if (auto comp_node = dyn_cast<ComputeNode>(T)) {
			if (candidate_set.contains(comp_node)) {
				// balancing the subexpressions
				if (!visited[T->getID()]) {
					toBalanced(G, comp_node);
				}
				leaves.push(T);
			} else if (comp_node->getInst()->getOpcode() == Root->getInst()->getOpcode()) {
				replaced.push_back(comp_node);
				if (G.findIncomingEdgesToNode(*T, in_edges, true)) {
					for (auto EI : in_edges) {
						auto Src = EI.first;
						worklist.push(Src);
					}
					in_edges.clear();
				}
			}
		}
	}

	// nothing to do
	errs() << "leaves " << leaves.size() << "\n";
	if (leaves.size() == 0 ) return;

	errs() << "leaves for balancing at " << Root->getUniqueName() << "\n";
	errs() << "remove ";
	for (auto N : replaced) {
		errs() << N->getUniqueName() << " ";
		G.removeNode(*N);
		N->clear();
	}
	errs() << "tempolrary\n";
	int pos = 0;
	while (leaves.size() > 2) {
		auto Ra1 = leaves.top(); leaves.pop();
		auto Rb1 = leaves.top(); leaves.pop();
		auto T = replaced[pos++];
		weight[T] = weight[Ra1] + weight[Rb1];
		errs() << formatv("connect {0}, {1} to {2}\n",
							Ra1->getUniqueName(),
							Rb1->getUniqueName(),
							T->getUniqueName());
		errs() << "Weight of " << T->getUniqueName() << " " << weight[T] << "\n";
		G.addNode(*T);
		G.connect(*Ra1, *T, *(new DFGEdge(*T, 0)));
		G.connect(*Rb1, *T, *(new DFGEdge(*T, 1)));
		leaves.push(T);
	}
	// remove in-coming edges of Root
	if (G.findIncomingEdgesToNode(*Root, in_edges, true)) {
		for (auto EI : in_edges) {
			for (auto E: EI.second) {
				EI.first->removeEdge(*E);
			}
		}
		in_edges.clear();
	}
	// connect remaining leaves to root
	int count = 0;
	while (!leaves.empty()) {
		auto v = leaves.top(); leaves.pop();
		G.connect(*v, *Root, *(new DFGEdge(*Root, count++)));
	}

}