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
*    File:          /include/CGRAInstMap.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  05-09-2021 18:35:11
*    Last Modified: 14-09-2021 23:13:13
*/

#ifndef CGRAInstMap_H
#define CGRAInstMap_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/JSON.h"


#include <float.h>
#include <math.h>

#define WARN_MSG_PREFIX "\x1B[35m\033[1mWarning\033[0m: "

#define CGRAOMP_CUSTOM_INST_ATTR "cgra_custom_inst"


#define FLAG_GETTER(FLAG, ISA, GETTER) make_pair(FLAG, [](Instruction *I) { \
	if (isa<ISA>(I)) { \
		return I->GETTER(); \
	} else { \
		return false;\
	} \
})

#define CMP_PRED_PAIR(STR, ENUM) make_pair(STR, CmpInst::Predicate::ENUM)


#define BINOP_ENTRY(OPCODE, OPENUM) make_pair(OPCODE, [](MapCondition *c){ \
	if (c) { \
		return make_shared<BinaryOpMapEntry>(OPCODE, BinaryOperator::OPENUM, c); \
	} else { \
		return make_shared<BinaryOpMapEntry>(OPCODE, BinaryOperator::OPENUM); \
	}\
})
#define COMPOP_ENTRY(OPCODE, IS_INTEGER) make_pair(OPCODE, [](MapCondition *c){ \
	if (c) { \
		return make_shared<CompOpMapEntry>(OPCODE, IS_INTEGER, c); \
	} else { \
		return make_shared<CompOpMapEntry>(OPCODE, IS_INTEGER); \
	}\
})
#define MEMOP_ENTRY(OPCODE, OPENUM) make_pair(OPCODE, [](MapCondition *c){ \
	if (c) { \
		return make_shared<MemoryOpMapEntry>(OPCODE, Instruction::MemoryOps::OPENUM, c); \
	} else { \
		return make_shared<MemoryOpMapEntry>(OPCODE, Instruction::MemoryOps::OPENUM); \
	}\
})

// Key setting to parse the JSON object
#define INST_KEY		"inst"
#define MAP_KEY			"map"
#define CONST_RHS_KEY	"rhs"
#define CONST_LHS_KEY	"lhs"
#define FLAGS_KEY		"flags"
#define PRED_KEY		"pred"
#define CONST_INT_KEY	"ConstantInt"
#define CONST_DBL_KEY	"ConstantDouble"

#define VEC_MAKE_RANGE(VEC) (make_range(VEC.begin(), VEC.end()))

using namespace llvm;


namespace CGRAOmp
{
	/**
	 * @class MapCondition
	 * @brief a representation of instruction mapping condition
	*/
	class MapCondition {
		public:
			/**
			 * @brief Construct a new MapCondition object
			 * 
			 * @param map a name which the instruction to be mapped to
			 */
			MapCondition(StringRef map) :
				map_name(map.str()), anyLHS(true),
				anyRHS(true), anyPred(true) {};
			/// Copy constructor
			MapCondition(const MapCondition &) = default;
			/// Move Constructor
			MapCondition(MapCondition &&) = default;

			/// Destructor
			~MapCondition(){}

			/**
			 * @brief Set a condition for a constant operand
			 * 
			 * @param use an integer value of the contant operant
			 * @param isLeft true if it is for the left hand side operand
			 */
			void setConst(int use, bool isLeft);

			/**
			 * @brief Set a condition for a constant operand
			 * the condition is met if an instruction has the same constant operand
			 * 
			 * @param use a double value of the contant operant
			 * @param isLeft true if it is for the left hand side operand
			 */
			void setConst(double use, bool isLeft);

			/**
			 * @brief Set a condition for instruction flags
			 * the condition is met if an instruction has all the specified flags
			 * @param flags a list of the flag
			 * @return Error is returned if unknown flag is included in the flags
			 */
			Error setFlags(ArrayRef<std::string>  flags);

			/**
			 * @brief Set a condition for the predicate type of icmp/fcmp
			 * 
			 * @param pred a predicate type
			 * @return Error is returned if unknown predicate is specified
			 */
			Error setPred(StringRef pred);

			/**
			 * @brief test if the instruction satisfies the condition
			 * 
			 * @param I instruction to be tested
			 * @return true in the case of satisfying the condition
			 * @return otherwise false
			 */
			bool match(Instruction *I);

			/**
			 * @brief get map name associated with this condition
			 * 
			 * @return string of map name
			 */
			std::string getMapName() {
				return map_name;
			}

			void print(raw_ostream &OS);

			void dump() {
				print(errs());
				errs() << "\n";
			}
		private:
			/// wrapper for const operand comparison
			std::function<bool(Instruction*I)> match_use;

			std::string map_name;
			int use_int;
			double use_double;
			SmallVector<StringRef> flag_list;
			CmpInst::Predicate cmp_pred;
			bool isUseInt = false;
			bool anyLHS;
			bool anyRHS;
			bool anyPred;
			int const_operand;

			StringRef pred_str;

			/**
			 * @brief equality comparetor for double values
			 * 
			 * @param a one double value
			 * @param b the other double value
			 * @return true if the two value seems to be the same.
			 * @return otherwise false
			 */
			static bool equal_double(double a, double b) {
				return (fabs(a - b) <= DBL_EPSILON
					* fmax(1, fmax(fabs(a), fabs(b))));
			}

			/// look-up table of functions to get flag from an inputed instruction
			/// key: string of flag
			static StringMap<std::function<bool(Instruction*)>> FlagGetter;
			/// look-up table of an enumeration of predicate
			/// key: string of predicate
			static StringMap<CmpInst::Predicate> PredMap;

	};

	/**
	 * @brief create a MapCondition instance from json object
	 * 
	 * @param json_obj a json object containing information for the mapping condition
	 * @param filename filename of JSON config (just for error message)
	 * @return Expected<std::pair<StringRef,MapCondition*>> a pair of opcode string of an instruction and the conrresponding mapping condition if there is no error.
	 * Otherwise, it will return ModelError.
	 */
	Expected<std::pair<StringRef,MapCondition*>> createMapCondition(
												json::Object *json_obj,
												StringRef filename);

	/**
	 * @brief Get the String Array from json object
	 * 
	 * @param json_obj a json object containing an item of array
	 * @param key a key of the array
	 * @param filename ilename of JSON config (just for error message)
	 * @return Expected<SmallVector<std::string>> obtained array of strings if there is no error.
	 * Otherwise, it will return ModelError.
	 */
	Expected<SmallVector<std::string>> getStringArray(json::Object *json_obj,
									StringRef key, StringRef filename);

	/**
	 * @class InstMapEntry
	 * @brief An abstract class for an entry to replace IR instruction to targeting CGRA instruction
	 */
	class InstMapEntry {
		public:
			/**
			 * @brief Construct a new InstMapEntry object
			 * 
			 * @param opcode string of the instruction opcode
			 */
			InstMapEntry(StringRef opcode) :
				opcode_str(opcode.str()) {
					map_cond = new MapCondition(opcode);
				};

			/**
			 * @brief  Construct a new InstMapEntry object with an initial MapCondition instance
			 * 
			 * @param opcode string of the instruction opcode
			 * @param cond mapping condition
			 */
			InstMapEntry(StringRef opcode, MapCondition* cond) :
				opcode_str(opcode.str()), map_cond(cond) {
				};

			~InstMapEntry() {
				delete map_cond;
			}

			/**
			 * @brief set a mapping condition
			 * 
			 * @param cond mapping condition
			 */
			void addMapping(MapCondition* cond) {
				map_cond = cond;
			}

			/**
			 * @brief an abstract method to check the condition is met
			 * 
			 * @param I an instruction
			 * @return true in the case of satisfying the condition
			 * @return otherwise false
			 */
			virtual bool match(Instruction *I) = 0;
			/**
			 * @brief check this entry is for the specified instruction
			 * This will not check the mapping condition.
			 * @param I an instruction
			 * @return true when this entry's opcode is the same as the instruction
			 * @return otherwise false
			 */
			virtual bool match(StringRef opcode) {
				return opcode_str == opcode;
			};

			/**
			 * @brief get the map name of this entry
			 */
			virtual std::string getMapName() {
				return map_cond->getMapName();
			}


			void print(raw_ostream &OS);
			void dump() {
				print(errs());
				errs() << "\n";
			}
		protected:
			MapCondition* map_cond;
			std::string opcode_str;
	};

	/**
	 * @class BinaryOpMapEntry
	 *  @brief A derived class from InstMapEntry for BinaryOperation
	 */
	class BinaryOpMapEntry : public InstMapEntry {
		public:
			/**
			 * @brief Construct a new BinaryOpMapEntry object
			 * 
			 * @param opcode string of the instruction opcode
			 * @param ops a type of binary operations
			 */
			BinaryOpMapEntry(StringRef opcode, Instruction::BinaryOps ops) :
				InstMapEntry(opcode), ops(ops) {};

			/**
			 * @brief Construct a new BinaryOpMapEntry object with an initial MapCondition instance
			 * 
			 * @param opcode string of the instruction opcode
			 * @param ops a type of binary operations
			 * @param cond mapping condition
			 */
			BinaryOpMapEntry(StringRef opcode, Instruction::BinaryOps ops,
								MapCondition* cond) :
				InstMapEntry(opcode, cond), ops(ops) {};

			/**
			 * @brief Derived function from InstMapEntry::match specilized for binary operation
			 */
			bool match(Instruction *I);
		private:
			Instruction::BinaryOps ops;
	};

	/**
	 * @class CompOpMapEntry
	 *  @brief A derived class from InstMapEntry for comparison instructions
	 */
	class CompOpMapEntry : public InstMapEntry {
		public:
			/**
			 * @brief Construct a new CompOpMapEntry object
			 * 
			 * @param opcode string of the instruction opcode
			 * @param isInteger value type to be compared
			 * 	- true: integer type
			 * 	- false: double type
			 */
			CompOpMapEntry(StringRef opcode, bool isInteger) :
				InstMapEntry(opcode),
				isInteger(isInteger) {};

			/**
			 * @brief Construct a new CompOpMapEntry object with an initial MapCondition instance
			 * 
			 * @param opcode string of the instruction opcode
			 * @param isInteger value type to be compared
			 * 	- true: integer type
			 * 	- false: double type
			 * @param cond mapping condition
			 */
			CompOpMapEntry(StringRef opcode, bool isInteger,
							MapCondition *cond) :
				InstMapEntry(opcode, cond),
				isInteger(isInteger) {};

			/**
			 * @brief Derived function from InstMapEntry::match specilized for comparison instructions
			 */
			bool match(Instruction *I);
		private:
			bool isInteger;
	};

	/**
	 * @class MemoryOpMapEntry
	 *  @brief A derived class from InstMapEntry for memory operation
	 */
	class MemoryOpMapEntry : public InstMapEntry {
		public:
			/**
			 * @brief Construct a new MemoryOpMapEntry object
			 * 
			 * @param opcode string of the instruction opcode
			 * @param kind a type of memory instruction
			 */
			MemoryOpMapEntry(StringRef opcode, Instruction::MemoryOps kind) :
				InstMapEntry(opcode),
				kind(kind) {};
			/**
			 * @brief Construct a new MemoryOpMapEntry object
			 *
			 * @param opcode string of the instruction opcode
			 * @param kind a type of memory instruction
			 * @param cond mapping condition
			 */
			MemoryOpMapEntry(StringRef opcode, Instruction::MemoryOps kind,
								MapCondition *cond) :
				InstMapEntry(opcode, cond),
				kind(kind) {};

			/**
			 * @brief Derived function from InstMapEntry::match specilized for memory operation
			 */
			bool match(Instruction *I);
		private:
			Instruction::MemoryOps kind;
	};

	/**
	 * @class CustomInstMapEntry
	 *  @brief A derived class from InstMapEntry for custom instruction
	 */
	class CustomInstMapEntry : public InstMapEntry {
		public:
			/**
			 * @brief Construct a new CustomIns MapEntry object
			 * 
			 * @param func_name a function name correspoinding to the custom instruction
			 */
			CustomInstMapEntry(StringRef func_name) :
				InstMapEntry(func_name) {};
			/**
			 * @brief Construct a new CustomInstMapEntry object with an initial MapCondition instance
			 * 
			 * @param func_name a function name correspoinding to the custom instruction
			 * @param cond mapping condition
			 */
			CustomInstMapEntry(StringRef func_name, MapCondition* map_cond) :
				InstMapEntry(func_name, map_cond) {};

			/**
			 * @brief Derived function from InstMapEntry::match specilized for custom instruction
			 */
			bool match(Instruction *I);

		private:
			bool isCustomOpFunc(Function *F);
			static DenseSet<Function*> cached;
	};

	/**
	 * @class InstMap
	 * @brief collective of all the instruction mapping
	*/
	class InstMap {
		public:
			/// Constructor
			InstMap() {};

			/**
			 * @brief find an entry for the instruction
			 * 
			 * @param opcode opcode of the instruction
			 * @return a pointer of InstMapEntry is return if an entry is found.
			 * Otherwise, it return nullptr.
			 */

			InstMapEntry* find(StringRef opcode);
			/**
			 * @brief find an entry for the instruction
			 * 
			 * @param I an instruction
			 * @return a pointer of InstMapEntry is return if an entry is found.
			 * Otherwise, it return nullptr.
			 */
			InstMapEntry* find(Instruction *I);

			/**
			 * @brief add a generic instruction without mapping condition
			 * 
			 * @param opcode opcode string of the instruction
			 * @return Error is returned if unknown opcode is specified
			 */
			Error add_generic_inst(StringRef opcode);

			/**
			 * @brief add a custom instruction
			 * 
			 * @param opcode opcode string of the instruction (= the corresponding function name)
			 */
			void add_custom_inst(StringRef opcode);

			/**
			 * @brief add an entry with a mapping condition
			 * 
			 * @param opcode opcode string of the instruction
			 * @param map_cond mapping condition instance
			 * @return Error is returned if the opcode of an instruction is not added yet.
			 */
			Error add_map_entry(StringRef opcode, MapCondition* map_cond);

		private:
			using entry_ptr = std::shared_ptr<InstMapEntry>;
			using entry_iterator = SmallVector<entry_ptr>::iterator;
			using entry_generator = std::function<entry_ptr(MapCondition*)>;

			/// look-up table of functions to instantiate an entry from an inputed opcode
			/// key: opcode string
			static StringMap<entry_generator> entry_gen;

			SmallVector<entry_ptr> entries;
			StringMap<entry_ptr> defaultEntries;

	};

} // namespace CGRAOmp

#endif //CGRAInstMap_H