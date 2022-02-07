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
*    File:          /src/Passes/CGRAOmpComponents/OptionPlugin.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 14:18:09
*    Last Modified: 03-02-2022 16:58:24
*/

#include "llvm/Support/CommandLine.h"

#include "OptionPlugin.hpp"

using namespace llvm;
using namespace cl;
using namespace std;

cl::opt<string> CGRAOmp::PathToCGRAConfig("cgra-model",
			cl::init("model.json"), cl::desc("Path to CGRA config file"),
			cl::value_desc("<filepath>"));

cl::alias CGRAOmp::PathToCGRAConfigAlias("cm",
			cl::aliasopt(CGRAOmp::PathToCGRAConfig));

cl::opt<bool> CGRAOmp::OptVerbose("cgraomp-verbose", cl::init(false), 
			cl::desc("Enables verbose output for CGRAOmp"));

cl::opt<string> CGRAOmp::OptDFGOpKey("cgra-dfg-op-key", cl::init("opcode"),
			cl::desc("opcode key for DOT generation"), cl::value_desc("key"));

cl::opt<bool> CGRAOmp::OptDFGPlainNodeName("cgra-dfg-plain", cl::init(false), 
			cl::desc("Use plain node names instead of pointer values for DOT generation"));

cl::list<CGRAOmp::OptKeyValue> CGRAOmp::OptDFGGraphProp("cgra-dfg-graph-prop", 
			cl::desc("Set a common perefenrece of DOT for graph"),
			cl::value_desc("attr1=value1,attr2=value2,..."),
			cl::CommaSeparated);

cl::list<CGRAOmp::OptKeyValue> CGRAOmp::OptDFGNodeProp("cgra-dfg-node-prop", 
			cl::desc("Set a common perefenrece of DOT for node"),
			cl::value_desc("attr1=value1,attr2=value2,..."),
			cl::CommaSeparated);

cl::list<CGRAOmp::OptKeyValue> CGRAOmp::OptDFGEdgeProp("cgra-dfg-edge-prop", 
			cl::desc("Set a common perefenrece of DOT for edge"),
			cl::value_desc("attr1=value1,attr2=value2,..."),
			cl::CommaSeparated);

cl::list<string> CGRAOmp::OptDFGPassPipeline("dfg-pass-pipeline", 
			cl::value_desc("<DFG Pass name>"),
			cl::desc("A textual description of the pass pipeline for DFG optimization"),
			cl::CommaSeparated);

cl::list<string> CGRAOmp::OptDFGPassPlugin("load-dfg-pass-plugin", 
			cl::value_desc("<Path string>"),
			cl::desc("Load DFG pass plugin"));

cl::opt<string> CGRAOmp::OptDFGFilePrefix("dfg-file-prefix",
			cl::value_desc("<string>"), cl::desc("The prefix used for the data flow graph name (default)"));

/**
 * @details It parses a string based on a regular expression
*/
CGRAOmp::OptKeyValue::OptKeyValue(string keyvalue)
{
	SmallVector<string, 2> vec;
	regex pattern("=");
	auto it = sregex_token_iterator(keyvalue.begin(),
									keyvalue.end(),
									pattern, -1);
	auto end = sregex_token_iterator();
	while (it != end) {
		vec.push_back(*it++);
	}
	if (vec.size() != 2) {
		valid = false;
	} else {
		key = vec[0];
		value = vec[1];
		valid = true;
	}
}

namespace llvm {
namespace cl {
	template class basic_parser<CGRAOmp::OptKeyValue>;
}
}

bool operator!=(const OptKeyValue &lhs, const OptKeyValue &rhs)
{
	return !(lhs == rhs);
}
bool operator==(const OptKeyValue &lhs, const OptKeyValue &rhs)
{
	return ((lhs.get_key() == rhs.get_key()) &&
				(lhs.get_value() == rhs.get_value()));
}

raw_ostream& llvm::operator<<(raw_ostream &OS, const OptKeyValue &D)
{
	OS << formatv("{0}={1}", D.get_key(), D.get_value());
	return OS;
}

void OptionValue<CGRAOmp::OptKeyValue>::anchor() {}
void parser<CGRAOmp::OptKeyValue>::anchor() {}

static const size_t MaxOptWidth = 8;
void parser<CGRAOmp::OptKeyValue>::printOptionDiff(const Option &O,
					const OptVal &V, const OptVal &D,
					size_t GlobalWidth) const
{
	printOptionName(O, GlobalWidth);
	std::string Str;
	{
		raw_string_ostream SS(Str);
		SS << V.getValue();
	}
	outs() << "= " << Str;
	size_t NumSpaces = MaxOptWidth > Str.size() ?
							MaxOptWidth - Str.size() : 0;
	outs().indent(NumSpaces) << " (default: ";

	if (D.hasValue()) outs() << D.getValue();
	else outs() << "*no default*";

	outs() << ")\n";
}

