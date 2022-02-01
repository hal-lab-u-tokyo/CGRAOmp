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
*    File:          /src/Passes/CGRAModel/CGRAInstMap.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  05-09-2021 18:38:43
*    Last Modified: 31-01-2022 13:49:06
*/


#include "llvm/ADT/Optional.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "common.hpp"
#include "CGRAInstMap.hpp"
#include "OptionPlugin.hpp"
#include "CGRAModel.hpp"

using namespace CGRAOmp;
using namespace llvm;
using namespace std;


/* =================== Implementation of MapCondition =================== */
bool MapCondition::match(Instruction *I)
{
	if (flag_list.size() > 0) {
		for (auto flag : flag_list) {
			if (!(FlagGetter[flag])(I)) {
				return false;
			}
		}
	}
	if (!anyLHS) {
		bool match = match_use(I);
		if (!match) return false;
	}
	if (!anyRHS) {
		bool match = match_use(I);
		if (!match) return false;
	}
	if (!anyPred) {
		if (auto cmp_inst = dyn_cast<CmpInst>(I)) {
			if (cmp_inst->getPredicate() != cmp_pred) return false;
		} else {
			return false;
		}
	}
	return true;
}

StringMap<std::function<bool(Instruction*)>> MapCondition::FlagGetter({
	FLAG_GETTER("nuw", OverflowingBinaryOperator, hasNoUnsignedWrap),
	FLAG_GETTER("nsw", OverflowingBinaryOperator, hasNoSignedWrap),
	FLAG_GETTER("exact", PossiblyExactOperator, isExact),
	FLAG_GETTER("fast", FPMathOperator, isFast),
	FLAG_GETTER("nnan", FPMathOperator, hasNoNaNs),
	FLAG_GETTER("ninf", FPMathOperator, hasNoInfs),
	FLAG_GETTER("nsz", FPMathOperator, hasNoSignedZeros),
	FLAG_GETTER("arcp", FPMathOperator, hasAllowReciprocal),
	FLAG_GETTER("contract", FPMathOperator, hasAllowContract),
	FLAG_GETTER("afn", FPMathOperator, hasApproxFunc),
	FLAG_GETTER("reassoc", FPMathOperator, hasAllowReassoc),
});

StringMap<CmpInst::Predicate> MapCondition::PredMap({
	// icmp
	CMP_PRED_PAIR("eq", ICMP_EQ), CMP_PRED_PAIR("ne", ICMP_NE),
	CMP_PRED_PAIR("ugt", ICMP_UGT), CMP_PRED_PAIR("uge", ICMP_UGE),
	CMP_PRED_PAIR("ult", ICMP_ULT), CMP_PRED_PAIR("ule", ICMP_ULE),
	CMP_PRED_PAIR("sgt", ICMP_SGT), CMP_PRED_PAIR("sge", ICMP_SGE),
	CMP_PRED_PAIR("slt", ICMP_SLT), CMP_PRED_PAIR("sle", ICMP_SLE),
	// fcmp
	CMP_PRED_PAIR("false", FCMP_FALSE), CMP_PRED_PAIR("oeq", FCMP_OEQ),
	CMP_PRED_PAIR("ogt", FCMP_OGT), CMP_PRED_PAIR("oge", FCMP_OGE),
	CMP_PRED_PAIR("olt", FCMP_OLT), CMP_PRED_PAIR("ole", FCMP_OLE),
	CMP_PRED_PAIR("one", FCMP_ONE), CMP_PRED_PAIR("ord", FCMP_ORD),
	CMP_PRED_PAIR("ueq", FCMP_UEQ), CMP_PRED_PAIR("ugt", FCMP_UGT),
	CMP_PRED_PAIR("uge", FCMP_UGE), CMP_PRED_PAIR("ult", FCMP_ULT),
	CMP_PRED_PAIR("ule", FCMP_ULE), CMP_PRED_PAIR("une", FCMP_UNE),
	CMP_PRED_PAIR("uno", FCMP_UNO), CMP_PRED_PAIR("true", FCMP_TRUE)
});


Error MapCondition::setFlags(ArrayRef<string> flags){
	for (auto f : flags) {
		auto it = FlagGetter.find(f);
		if (it != FlagGetter.end()) {
			flag_list.push_back(it->first());
		} else {
			// invalid flag string
			error_code EC;
			return make_error<StringError>("Unknown flag \"" + f +
					"\" for instruction mapping condition", EC);
		}
	}
	return ErrorSuccess();
}


Error MapCondition::setPred(StringRef pred) {
	auto it = PredMap.find(pred);
	if (it != PredMap.end()) {
		cmp_pred = it->second;
		anyPred = false;
		pred_str = pred;
	} else {
		// invalid pred string
		error_code EC;
		return make_error<StringError>("Unknown pred type \"" + pred +
					"\" for instruction mapping condition", EC);
	}
	return ErrorSuccess();
}


void MapCondition::print(raw_ostream &OS)
{
	OS << "Conditions\n";
	if (flag_list.size() > 0) {
		OS << formatv("\tflags: {0}\n", VEC_MAKE_RANGE(flag_list));
	}
	if (!anyLHS) {
		OS << "\tLHS operand: ";
	}
	if (!anyRHS) {
		OS << "\tRHS operand: ";
	}
	if (!anyLHS || !anyRHS) {
		if (isUseInt) {
			OS << formatv("\tConst Int {0}\n", use_int);
		} else {
			OS << formatv("\tConst double {0}\n ", use_double);
		}
	}

	if (!anyPred) {
		OS << formatv("\tCmpInst Predicate: {0}\n", pred_str);
	}

	OS << formatv("mapping to {0}", map_name);
}

void MapCondition::setConst(int use, bool isLeft) {
	assert(anyLHS && anyRHS && "Only once either setConst or setRHS can be used");
	const_operand = 0 ? isLeft : 1;
	use_int = use;
	if (isLeft) anyLHS = false;
	else anyRHS = false;
	isUseInt = true;
	match_use = [=](Instruction *I) {
					if (const_operand < I->getNumOperands()) {
						auto V = I->getOperand(const_operand);
						if (auto cint = dyn_cast<ConstantInt>(V)) {
							return use_int == cint->getSExtValue();
						}
					}
					return false;
				};
};

void MapCondition::setConst(double use, bool isLeft) {
	assert(anyLHS && anyRHS && "Only once either setLHS or setRHS can be used");
	const_operand = 0 ? isLeft : 1;
	use_double = use;
	if (isLeft) anyLHS = false;
	else anyRHS = false;
	match_use = [=](Instruction *I) {
					if (const_operand < I->getNumOperands()) {
						auto V = I->getOperand(const_operand);
						if (auto cfp = dyn_cast<ConstantFP>(V)) {
							return equal_double(use_double,
										cfp->getValue().convertToDouble());
						}
					}
					return false;
				};
}

/* =================== Implementation of InstMap =================== */

Error InstMap::add_generic_inst(StringRef opcode)
{
	if (defaultEntries.find(opcode) != defaultEntries.end()) {
		// already added
		if (OptVerbose) {
			errs() << formatv(WARN_MSG_PREFIX
				"instruction \"{0}\" is already added\n", opcode);
		}
		return ErrorSuccess();
	}
	auto generator = entry_gen.find(opcode);
	if (generator != entry_gen.end()) {
		entry_ptr x = (generator->second)(nullptr);
		entries.push_back(x);
		defaultEntries[opcode] = x;
	} else {
		// not supported
		error_code EC;
		return make_error<StringError>("Unknown opcode \""
					+ opcode + "\" for the supported instructions", EC);
	}
	return ErrorSuccess();
}

void InstMap::add_custom_inst(StringRef opcode, ModuleAnalysisManager &MAM)
{
	if (defaultEntries.find(opcode) != defaultEntries.end()) {
		// already added
		if (OptVerbose) {
			errs() << formatv(WARN_MSG_PREFIX
				"instruction \"{0}\" is already added\n", opcode);
		}
	} else {
		auto opcode_str = opcode.str();
		entry_gen[opcode] = [&,opcode_str](MapCondition *c){
			if (c) {
				return make_shared<CustomInstMapEntry>(opcode_str, MAM, c);
			} else {
				return make_shared<CustomInstMapEntry>(opcode_str, MAM);
			}
		};
		entry_ptr x = entry_gen[opcode](nullptr);
		entries.push_back(x);
		defaultEntries[opcode] = x;
	}
}

/**
 * @details When a generic instruction is added to InstMap, a default entry, which matches any instruction with the same opcode, is added.
 * This method is called, then the default entry is erased.
 *
*/
Error InstMap::add_map_entry(StringRef opcode, MapCondition* map_cond)
{
	if (defaultEntries.find(opcode) == defaultEntries.end()) {
		//not added
		error_code EC;
		return make_error<StringError>("A mapping condition for not supported insruction \"" + opcode + "\" is specified", EC);
	} else {
		if (defaultEntries[opcode] != nullptr) {
			// search for and erase default entry
			entry_iterator it = entries.begin();
			while (it != entries.end()) {
				if (*it == defaultEntries[opcode]) {
					it = entries.erase(it);
				} else {
					it++;
				}
			}
			//add new entry
			entries.push_back((entry_gen[opcode])(map_cond));
			defaultEntries[opcode] = nullptr;
		} else {
			// just add new entry
			entries.push_back((entry_gen[opcode])(map_cond));
		}
	}

	return ErrorSuccess();

}


StringMap<InstMap::entry_generator> InstMap::entry_gen({
	BINOP_ENTRY("add", Add), BINOP_ENTRY("fadd", FAdd),
	BINOP_ENTRY("sub", Sub), BINOP_ENTRY("fsub", FSub),
	BINOP_ENTRY("mul", Mul), BINOP_ENTRY("fmul", FMul),
	BINOP_ENTRY("udiv", UDiv), BINOP_ENTRY("sdiv", SDiv),
	BINOP_ENTRY("fdiv", FDiv), BINOP_ENTRY("urem", URem),
	BINOP_ENTRY("frem", FRem), BINOP_ENTRY("srem", SRem),
	BINOP_ENTRY("shl", Shl), BINOP_ENTRY("lshr", LShr),
	BINOP_ENTRY("ashr", AShr), BINOP_ENTRY("and", And),
	BINOP_ENTRY("or", Or), BINOP_ENTRY("xor", Xor),
	COMPOP_ENTRY("icmp", true), COMPOP_ENTRY("fcmp", false),
	MEMOP_ENTRY("load", Load), MEMOP_ENTRY("store", Store)
});


InstMapEntry* InstMap::find(StringRef opcode)
{
	for (auto it : entries) {
		if (it->match(opcode)) {
			return it.get();
		}
	}
	return nullptr;
}

InstMapEntry* InstMap::find(Instruction *I)
{
	for (auto it : entries) {
		if (it->match(I)) {
			return it.get();
		}
	}
	return nullptr;
}

/* ================== Implementation of InstMapEntry ================== */
void InstMapEntry::print(raw_ostream &OS)
{
	OS << formatv("Entry for the instruction: {0}\n", opcode_str);
	map_cond->print(OS);
}

/* ================== Implementation of BinaryOpMapEntry ================== */
bool BinaryOpMapEntry::match(Instruction *I)
{
	if (auto binop = dyn_cast<BinaryOperator>(I)) {
		return (binop->getOpcode() == this->ops) && map_cond->match(I);
	} else {
		return false;
	}
}

/* ================== Implementation of MemoryOpMapEntry ================== */
/**
 * @details Currently, only @a load and @a store instructions are considered
 *
*/
bool MemoryOpMapEntry::match(Instruction *I)
{
	if (auto binop = dyn_cast<LoadInst>(I)) {
		return kind == Instruction::MemoryOps::Load;
	} else if (auto binop = dyn_cast<StoreInst>(I)) {
		return kind == Instruction::MemoryOps::Store;
	} else {
		return false;
	}
}

/* ================== Implementation of CompOpMapEntry ================== */
bool CompOpMapEntry::match(Instruction *I)
{
	if (auto binop = dyn_cast<CmpInst>(I)) {
		if (I->getOpcode() == Instruction::OtherOps::ICmp && isInteger) {
			return map_cond->match(I);
		} else if (I->getOpcode() == Instruction::OtherOps::ICmp 
						&& !isInteger) {
			return map_cond->match(I);
		}
	}
	return false;
}

/* ================= Implementation of CustomInstMapEntry ================= */
/**
 * @details the function corresponding to the custom instruction must satisfy the following conditions
 * -# the function name is the same as the opcode of the custom instruction
 * -# the function is annotated with "cgra_custom_inst"
 * 
 * The following declaration is an example to annotate a function:
 * @code
 * __attribute__((annotate("cgra_custom_inst"))) int func(int x, int y);
 * @endcode 
 *
*/
bool CustomInstMapEntry::match(Instruction *I)
{
	// when it is call inst
	if (auto callop = dyn_cast<CallBase>(I)) {
		auto F = callop->getCalledFunction();
		// check the func name
		if (F->getName() == opcode_str) {
			// check the called function has the attributes
			return isCustomOpFunc(F) && map_cond->match(I);
		}
	}
	return false;
}

bool CustomInstMapEntry::isCustomOpFunc(Function *F)
{
	auto *M = F->getParent();
	auto &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(*M).getManager();
	auto annot = FAM.getResult<AnnotationAnalysisPass>(*F);
	return annot.contains(CGRAOMP_CUSTOM_INST_ATTR);
}

/* ============ Utility functions for parsing the configration  ============ */
Expected<SmallVector<string>> CGRAOmp::getStringArray(json::Object *json_obj,
									StringRef key, StringRef filename)
{
	SmallVector<string> list;
	if (json_obj->get(key)) {
		auto array = json_obj->get(key)->getAsArray();
		for (auto it : *array) {
			auto str = it.getAsString();
			if (str.hasValue()) {
				list.push_back(str.getValue().str());
			} else {
				return make_error<ModelError>(filename, key,
										"an array of string", &it);
			}
		}
	} else {
		return make_error<ModelError>(filename, key);
	}

	return list;
}

/**
 * @details Here is an explation regarding a valid JSON object
 * - Required fileds
 * 	- An instruction to be replaced
 * 		- key: "inst"
 * 		- value: string of the opcode
 * 	- A name to be mapped
 * 		- key: "map"
 * 		- value: string of the name to be mapped
 * - Optional fileds
 * 	- A list of instruction flags
 * 		- key: "flags"
 * 		- value: an array of string
 * 	- A predicate for comparison instructions
 * 		- key: "pred"
 * 		- value: string of the predicate type
 * 	- A constant operand
 * 		- key:
 * 			- "lhs" for left hand side of operand
 * 			- "rhs" for right hand side of operand
 *		- value: an object as following
 *			- For interger: {"ConstantInt": @a int_value }
 *			- For double: {"ConstantDouble": @a double_value }
 *
*/
Expected<pair<StringRef,MapCondition*>>  CGRAOmp::createMapCondition(json::Object *json_obj,
													StringRef filename)
{
	auto make_model_error = [&](auto... args) {
		auto EI = std::make_unique<ModelError>(filename, args...);
		EI->setRegion("an entry of \"instruction_map\"");
		return Error(std::move(EI));
	};
	//get necessary items
	StringRef target_inst;
	if (json_obj->get(INST_KEY)) {
		auto inst_str = json_obj->get(INST_KEY)->getAsString();
		if (inst_str.hasValue()) {
			target_inst = inst_str.getValue();
		} else {
			return make_model_error(INST_KEY, "string",
									 json_obj->get(INST_KEY));
		}
	} else {
		return make_model_error(INST_KEY);
	}
	StringRef map_name;
	if (json_obj->get(MAP_KEY)) {
		auto map_str = json_obj->get(MAP_KEY)->getAsString();
		if (map_str.hasValue()) {
			map_name = map_str.getValue();
		} else {
			return make_model_error(MAP_KEY, "string", json_obj->get(MAP_KEY));
		}
	} else {
		return make_model_error(MAP_KEY);
	}


	auto map_cond = new MapCondition(map_name);

	//get optional conditions
	// disable missing key error
	auto handler = [&](ModelError &E) -> Error {
		if (E.isMissingKey()) {
			return ErrorSuccess();
		} else {
			return make_error<ModelError>(std::move(E));
		}
	};
	// get flag condition
	auto flags = getStringArray(json_obj, FLAGS_KEY, filename);
	if (!flags) {
		auto unhandledErr = handleErrors(
			std::move(flags.takeError()),
			handler);
		if (unhandledErr) return unhandledErr;
	} else {
		if (auto E = map_cond->setFlags(flags.get())) {
			return std::move(E);
		}
	}
	// get pred condition
	if (json_obj->get(PRED_KEY)) {
		auto pred_str = json_obj->get(PRED_KEY)->getAsString();
		if (pred_str.hasValue()) {
			if (auto E = map_cond->setPred(pred_str.getValue())) {
				return std::move(E);
			}
		} else {
			return make_model_error(INST_KEY, "string",
									json_obj->get(PRED_KEY));
		}
	}

	// get constant operand condition
	auto setConst = [&,ptr = move(map_cond)](json::Object *obj, bool isLeft) mutable -> Error {
		if (obj->get(CONST_INT_KEY)) {
			auto ci = obj->get(CONST_INT_KEY)->getAsInteger();
			if (ci.hasValue()) {
				ptr->setConst(int(ci.getValue()), isLeft);
			} else {
				return make_model_error(CONST_INT_KEY, "Integer",
									obj->get(CONST_INT_KEY));
			}
		} else if (obj->get(CONST_DBL_KEY)) {
			auto cd = obj->get(CONST_DBL_KEY)->getAsNumber();
			if (cd.hasValue()) {
				ptr->setConst(cd.getValue(), isLeft);
			} else {
				return make_model_error(CONST_DBL_KEY, "Integer",
									obj->get(CONST_DBL_KEY));
			}
		}
		return ErrorSuccess();
	};

	bool lhs_en = false;
	if (json_obj->get(CONST_LHS_KEY)) {
		if (auto E = setConst(json_obj->get(CONST_LHS_KEY)->getAsObject(),
								true)) {
			return std::move(E);
		}
		lhs_en = true;
	}

	if (json_obj->get(CONST_RHS_KEY)) {
		if (lhs_en) {
			// both left and right hand side are specified, so ignore right
			if (OptVerbose) {
				errs() << formatv(WARN_MSG_PREFIX
				"both left and right hand side condition is specified for an instruction mapping for {0}. So, one for the right hand side is ignored\n", target_inst);
			}
		} else {
			if (auto E = setConst(json_obj->get(CONST_RHS_KEY)->getAsObject(),
									 false)) {
				return std::move(E);
			}
		}
	}

	return make_pair(target_inst, map_cond);
}