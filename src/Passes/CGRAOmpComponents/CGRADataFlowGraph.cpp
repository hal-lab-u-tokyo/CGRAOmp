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
*    File:          /src/Passes/CGRAOmpComponents/CGRADataFlowGraph.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 15:03:59
*    Last Modified: 01-02-2022 15:27:20
*/

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"

#include "CGRADataFlowGraph.hpp"
#include "OptionPlugin.hpp"
#include "common.hpp"

#include <system_error>
#include <regex>
#include <string>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "cgraomp"

string ConstantNode::getConstStr() const
 {
	Constant* const_value = dyn_cast<Constant>(val);
	if (auto *cint = dyn_cast<ConstantInt>(const_value)) {
		return formatv("int{0}={1}", cint->getBitWidth(), cint->getSExtValue());
	} else if (auto *cfloat = dyn_cast<ConstantFP>(const_value)) {
		auto apf = cfloat->getValueAPF();
		float f = apf.convertToFloat();
		return formatv("{0}={1}", getFloatType(apf), f);
	} else {
		LLVM_DEBUG(dbgs() << ERR_DEBUG_PREFIX << " Unexpected constant type: ";
					const_value->print(dbgs());
					dbgs() << "\n"
		);
	}
	return "";
}

/* ================== Implementation of CGRADFG ================== */
bool CGRADFG::addNode(NodeType &N)
{
	auto E = new DFGEdge(N);
	auto result = CGRADFGBase::addNode(N);
	return result && CGRADFGBase::connect(getRoot(), N, *E);
}

bool CGRADFG::connect(NodeType &Src, NodeType &Dst, EdgeType &E)
{
	auto result = CGRADFGBase::connect(Src, Dst, E);
	// if there exists only an edge: vroot -> Dst, remove it.
	EdgeListTy vedges;
	if (getRoot().findEdgesTo(Dst, vedges)) {
		assert(vedges.size() == 1 && "more than one edges from virtual root to a node");
		getRoot().removeEdge(**(vedges.begin()));
	}
	return result;
}

/**
 * @details If OptDFGPlainNodeName option is enabled,
 * this method calls convertToReadableNodeName.
 * Then, the converted contents is saved as a file.
 *
*/
Error CGRADFG::saveAsDotGraph(StringRef filepath)
{
	bool human_readable = CGRAOmp::OptDFGPlainNodeName;
	// open file
	error_code EC;
	raw_fd_ostream File(filepath, EC, sys::fs::OpenFlags::F_Text);
	string buf;
	raw_string_ostream StrOS(buf);
	raw_ostream *GF = (human_readable) ? dyn_cast<raw_ostream>(&StrOS) :
										dyn_cast<raw_ostream>(&File);

	if (!EC) {
		WriteGraph(*GF, (const CGRADFG*)(this));
		if (human_readable) {
			for (auto *N : *this) {
				if (*N == getRoot()) continue;
				string beforeName = formatv("Node{0:x1}", (const void*)N);
				string afterName = N->getUniqueName();
				buf = regex_replace(buf, regex(beforeName), afterName);
			}
			File << buf;
		}
	} else {
		return errorCodeToError(EC);
	}
	return ErrorSuccess();
}


/* ============= Specilization of DotGraphTraits for CGRADFG ============= */

/// currently empty
StringMap<StringRef> CGRADFGDotGraphTraits::default_graph_prop = {

};

/// currently empty
StringMap<StringRef> CGRADFGDotGraphTraits::default_node_prop = {

};

/// currently empty
StringMap<StringRef> CGRADFGDotGraphTraits::default_edge_prop = {

};


string CGRADFGDotGraphTraits::getEdgeAttributes(const DFGNode *Node, 
					GraphTraits<DFGNode *>::ChildIteratorType I,
					  const CGRADFG *G)
{
	const DFGEdge *E = static_cast<const DFGEdge*>(*I.getCurrent());
	return E->getEdgeAttr();
}

string CGRADFGDotGraphTraits::getGraphProperties(const CGRADFG *G) {
	string buf;
	raw_string_ostream OS(buf);
	OS << "\t//Graph Properties\n";
	auto printer = [&](string attr_type, auto &opt_prop,
						StringMap<StringRef> &def)
	{
		if (opt_prop.size() > 0) {
			OS << formatv("\t{0}[\n", attr_type);
			for (auto attr : opt_prop) {
				OS << "\t\t" << attr << ";\n";
			}
			OS << "\t]\n";
		} else {
			if (def.size() > 0) {
				OS << formatv("\t{0}[\n", attr_type);
				for (auto &attr : def) {
					OS << formatv("\t\t{0}={1};\n", attr.first(), attr.second);
				}
				OS << "\t]\n";
			}
		}
	};

	printer("graph", OptDFGGraphProp, default_graph_prop);
	printer("node", OptDFGNodeProp, default_node_prop);
	printer("edge", OptDFGEdgeProp, default_edge_prop);

	return buf;
}