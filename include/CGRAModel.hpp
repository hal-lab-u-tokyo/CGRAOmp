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
*    File:          /include/CGRAModel.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 15:00:30
*    Last Modified: 17-09-2021 16:49:48
*/
#ifndef CGRAModel_H
#define CGRAModel_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"

#include "CGRAInstMap.hpp"

#include <string>
#include <climits>

// Key setting to parse the JSON object
#define CATEGORY_KEY 	"category"
#define AG_CONF_KEY		"address_generator"
#define AG_STYLE_KEY	"control"
#define AG_MAX_NEST_KEY	"max_nested_level"
#define COND_STYLE_KEY	"conditional"
#define IDP_STYLE_KEY	"inter-loop-dependency"
#define OPT_ENABLE_KEY	"allowed"
#define OPT_TYPE_KEY	"type"
#define CUSTOM_INST_KEY	"custom_instructions"
#define GEN_INST_KEY	"generic_instructions"
#define INST_MAP_KEY	"instruction_map"



using namespace llvm;

namespace CGRAOmp
{

	/**
	 * @class CGRAModel
	 * @brief A base class of CGRA model for DFG extraction
	 */
	class CGRAModel {
		public:
			/**
			 * @enum CGRACategory
			 * @brief a category of the CGRA model
			 */
			enum class CGRACategory {
				/// Generic CGRAs
				Generic,
				/// A CGRA decoupling memory access & computation
				Decoupled,
			};

			/**
			 * @enum ConditionalStyle
			 * @brief Ability to handle conditional execution (i.e., if-else part)
			 */
			enum class ConditionalStyle {
				/// conditional is not allowd
				No,
				/// conditional is handled by MUX operation
				MuxInst,
				/// conditional is handled by TriState ALU
				TriState
			};

			/**
			 * @enum InterLoopDep
			 * @brief How to describe the inter-loop dependecy
			 */
			enum class InterLoopDep {
				/// loop with inter-loop dependency is not allowed
				No,
				/// Generating a DFG with back-edge as inter-loop dependency
				Generic,
				/// Replacing the inter-loop dependency with backward operation node
				BackwardInst,
			};
			/// Map category string to CGRACategory
			static StringMap<CGRACategory> CategoryMap;
			/// Map category string to ConditionalStyle
			static StringMap<ConditionalStyle> CondStyleMap;
			/// Map category string to InterLoopDep
			static StringMap<InterLoopDep> InterLoopDepMap;

			// constructors
			/**
			 * @brief Construct a new CGRAModel object
			 * 
			 * @param filename filename of the configration file
			 * @param category category of the CGRA
			 * @param cond conditional style of the CGRA
			 * @param inter_loop_dep ability to handle inter loop dependency
			 */
			explicit CGRAModel(StringRef filename, CGRACategory category,
								ConditionalStyle cond,
								InterLoopDep inter_loop_dep) :
								filename(filename), category(category),
								cond(cond), inter_loop_dep(inter_loop_dep) {};
			CGRAModel() = delete;
			/// Copy constructor
			CGRAModel(const CGRAModel &) = default;
			/// Move constructor
			CGRAModel(CGRAModel &&) = default;

			/**
			 * @brief add a supported generic insturction for the CGRA
			 * 
			 * @param opcode opcode of the instruction
			 * 	- if it is already added instruction, this function does nothing
			 * @return Error is returned if unknown opcode is specified
			 */
			Error addSupportedInst(StringRef opcode);


			/**
			 * @brief add a custom insturction for the CGRA
			 * 
			 * @param opcode opcode of the instruction
			 * 	- if it is already added instruction, this function does nothing
			 * @param MAM ModuleAnalysisManager
			 */
			void addCustomInst(StringRef opcode, ModuleAnalysisManager &MAM);

			/**
			 * @brief add a instruction mapping entry to the CGRA model
			 * 
			 * @param opcode opcode of the instruction to be replaced
			 * @param map_cond a mapping condition
			 * @return Error is returned if the opcode is for unsupported instruction
			 */
			Error add_map_entry(StringRef opcode, MapCondition *map_cond);

			/**
			 * @brief checking whether the instruction is supported by the CGRA or not.
			 * 
			 * @param I an instruction
			 * @return a pointer of InstMapEntry is return if it is supported.
			 * Otherwise, it return nullptr.
			 */
			InstMapEntry* isSupported(Instruction *I);

			/**
			 * @brief an interface to down cast to derived classes
			 * 
			 * @tparam DerivedT derived class named
			 * @return DerivedT* casted pointer to the derived class
			 */
			template <typename DerivedT>
			DerivedT* asDerived() {
				return dyn_cast<DerivedT>(this);
			}

			/**
			 * @brief Get the category of the CGRA
			 * 
			 * @return CGRACategory 
			 */
			CGRACategory getKind() const {
				return category;
			}

		protected:
			StringRef filename;
			ConditionalStyle cond;
			InterLoopDep inter_loop_dep;
			CGRACategory category;
			InstMap inst_map;

	};

	/**
	 * @class AddressGenerator
	 * @brief An abstract class representing a type of address generator
	 */
	class AddressGenerator {
		public:
			enum class Kind {
				Affine,
				FullState,
			};

			AddressGenerator() = delete;
			explicit AddressGenerator(Kind kind) : kind(kind) {};
			AddressGenerator(const AddressGenerator &AG) = default;
			AddressGenerator(AddressGenerator &&) = default;


			Kind getKind() const {
				return kind;
			}

			template <typename DerivedT>
			DerivedT* asDerived() {
				return dyn_cast<DerivedT>(this);
			}

		private:
			Kind kind;
	};

	/**
	 * @class DecoupledCGRA
	 * @brief CGRA model for category "decoupled"
	*/
	class DecoupledCGRA : public CGRAModel {
		public:
			/**
			 * @brief Construct a new DecoupledCGRA
			 *
			 * @param filename filename of the configration file
			 * @param cond conditional style of the CGRA
			 * @param inter_loop_dep ability to handle inter loop dependency
			 */
			DecoupledCGRA(StringRef filename, AddressGenerator *foo,
							ConditionalStyle cond, InterLoopDep inter_loop_dep) :
				CGRAModel(filename, CGRACategory::Decoupled,
							cond, inter_loop_dep), AG(foo) {};

			/// copy constructor
			DecoupledCGRA(const DecoupledCGRA&);

			/// destructor
			~DecoupledCGRA() {
				delete AG;
			}

			/**
			 * @brief get the address generator
			 * 
			 * @return a pointer to the AddressGenerator instance
			 */
			AddressGenerator* getAG() const {
				return AG;
			}

			/// for downcasting from CGRAModel by llvm::dyn_cast
			static bool classof(const CGRAModel *M) {
				return M->getKind() == CGRACategory::Decoupled;
			}
		private:
			AddressGenerator *AG;

	};

	/**
	 * @class AffineAG
	 * @brief A model of address generator compatible to memory access with affine expression
	 * the accessed memory address must be expressed in 
	 *  @f[
	 * C_0 + C_1x + C_2y + C_3z...
	 * @f]
	 * where @f$ x, y, z... @f$ are loop variable and @f$ C_0, C_1, ... @f$ are constants.
	 */
	class AffineAG : public AddressGenerator {
		using AG = AddressGenerator;
		public:
			/**
			 * @brief Default constructor without any limitation about the nested level
			 */
			AffineAG() : AG(AG::Kind::Affine) {
				max_nests = INT_MAX;
			}
			/**
			 * @brief Construct a new AffineAG object
			 * 
			 * @param max_nests maximum nested level to be allowed
			 */
			explicit AffineAG(int max_nests) : AG(AG::Kind::Affine),
				max_nests(max_nests) {};

			/// copy constructor
			AffineAG(const AffineAG&);

			/**
			 * @brief Get the maximum nested level supported by this AddressGenerator
			 */
			int getMaxNests() const {
				return max_nests;
			}

			/// for downcasting from AddressGenerator by llvm::dyn_cast
			static bool classof(const AG *ag) {
				return ag->getKind() == Kind::Affine;
			}

		private:
			int max_nests;
	};

	/**
	 * @class FullStateAG
	 * @brief a model of address generator without any constraints
	 */
	class FullStateAG : public AddressGenerator {
		using AG = AddressGenerator;
		public:
			/// copy constructor
			FullStateAG(const FullStateAG&);
			/// for downcasting from AddressGenerator by llvm::dyn_cast
			static bool classof(const AG *ag) {
				return ag->getKind() == Kind::FullState;
			}
	};

	/**
	 * @class GenericCGRA
	 * @brief CGRA model for category "generic"
	*/
	class GenericCGRA : public CGRAModel {
		public:
			/**
			 * @brief Construct a new Generic CGRA object
			 * 
			 * @param filename filename of the config file
			 */
			explicit GenericCGRA(StringRef filename) :
				CGRAModel(filename, CGRACategory::Generic,
							ConditionalStyle::No,
							InterLoopDep::No) {}
			GenericCGRA(const GenericCGRA &);

			/// for downcasting from CGRAModel by llvm::dyn_cast
			static bool classof(const CGRAModel *M) {
				return M->getKind() == CGRAModel::CGRACategory::Generic;
			}

		private:
	};

	/**
	 * @class ModelError
	 * 
	 * @brief A custom error used to notice the specified configuration file is invalid
	 */
	class ModelError : public ErrorInfo<ModelError> {
		public:
			/**
			 * @brief Construct a new ModelError when missing a required item
			 *
			 * @param filename filename of the parsed configration file
			 * @param missed_key key string of missing item
			 */
			ModelError(StringRef filename, StringRef missed_key) :
				filename(filename),
				errtype(ErrorType::MissingKey),
				error_key(missed_key) {}

			/**
			 * @brief Construct a new ModelError when unexpected value type is specified 
			 * 
			 * @param filename filename of the parsed configration file 
			 * @param key key string containing invalid type of value
			 * @param exptected_type the expected type of string
			 * @param v (optional) the invalid json::Value instance
			 */
			ModelError(StringRef filename, StringRef key,
						StringRef exptected_type, 
						json::Value *v = nullptr) :
					filename(filename),
					errtype(ErrorType::InvalidDataType),
					error_key(key),
					exptected_type(exptected_type) {
						if (v != nullptr) {
							raw_string_ostream OS(json_val);
							OS << *v;
						}
					}

			/**
			 * @brief Construct a new ModelError when not implemented configuration is specified
			 * 
			 * @param filename filename of the parsed configration file 
			 * @param config a pair of key and value string for the configuration
			 */
			ModelError(StringRef filename, std::pair<StringRef,StringRef> config) :
				filename(filename), errtype(ErrorType::NoImplemented),
				error_key(config.first), error_val(config.second.str()) {};

			/**
			 * @brief Construct a new ModelError when a value is invalid for the specified key
			 * 
			 * @param filename filename of the parsed configration file 
			 * @param key key string containing invalid value
			 * @param val the invalid value string
			 * @param list list of valid values
			 */
			ModelError(StringRef filename, StringRef key, 
						StringRef val, ArrayRef<StringRef> list);

			/**
			 * @brief set a region where parse error ocurrs
			 * 
			 * @param region_str a string of region info for error message
			 */
			void setRegion(StringRef region_str) {
				region = region_str;
			}

			static char ID;
			std::error_code convertToErrorCode() const override {
				// Unknown error
				return std::error_code(41, std::generic_category());
			}
			void log(raw_ostream &OS) const override;


			/**
			 * @brief check if the error cause if due to missing key
			 * 
			 * @return true in the case of missing key
			 * @return false otherwise
			 */
			bool isMissingKey() {
				return errtype == ErrorType::MissingKey;
			}

		private:
			/**
			 * @enum ErrorType
			 * @brief Type of error cause
			 */
			enum class ErrorType {
				/// missing a required item
				MissingKey,
				/// specified value is an invalid data type
				InvalidDataType,
				/// specified value is not valid configuration
				InvalidValue,
				/// currently not implemented
				NoImplemented,
			};

			ErrorType errtype;
			std::string json_val = "";
			StringRef filename;
			StringRef error_key;
			std::string error_val;
			StringRef exptected_type;
			StringRef region = "";
			SmallVector<StringRef> valid_values;

	};


	/* ---------- Utility functions  ---------- */
	/**
	 * @brief a helper function to instantiate CGRAModel based on JSON configfile
	 * 
	 * @param filepath filepath to the JSON config file
	 * @param MAM ModuleAnalysisManager
	 * @return Expected<CGRAModel> CGRAModel if there is no error. Otherwise, it contains ModelError
	 */
	Expected<CGRAModel*> parseCGRASetting(StringRef filepath,
											ModuleAnalysisManager &MAM);

	/**
	 * @brief a template to extract only values from SettingMap
	 * 
	 * @tparam SettingT type of setting value (e.g., CGRACategory)
	 * @param settingMap map setting string to SettingT value
	 * @return SmallVector<StringRef> the extracted values
	 */
	template <typename SettingT>
	SmallVector<StringRef> get_setting_values(StringMap<SettingT> settingMap);

	/**
	 * @brief Check if val is a valid setting for SettingT
	 * 
	 * @tparam SettingT type of setting value (e.g., CGRACategory)
	 * @param settingMap map setting string to SettingT value
	 * @param val a value of string to be checked
	 * @return true the value is valid
	 * @return false the value is unknown
	 */
	template <typename SettingT>
	bool containsValidData(StringMap<SettingT> settingMap, StringRef val);

	/**
	 * @brief Get CGRACategory from JSON config
	 * 
	 * @param json_obj parsed JSON config
	 * @param filename filename of JSON config (just for error message)
	 * @return Expected<CGRAModel::CGRACategory> CGRACategory if there is no error. Otherwise, it contains ModelError.
	 */
	Expected<CGRAModel::CGRACategory> getCategory(json::Object *json_obj,
													StringRef filename);

	/**
	 * @brief Get common option from JSON config
	 * 
	 * @tparam *key_str key string
	 * @tparam SettingT type of setting value (e.g., CGRACategory)
	 * @param json_obj parsed JSON config
	 * @param filename filename of JSON config (just for error message)
	 * @param settingMap  map setting string to SettingT value
	 * @return Expected<SettingT> SettingT value if there is no error. Otherwise, it contains ModelError.
	 */
	template <char const *key_str, typename SettingT>
	Expected<SettingT> getOption(json::Object *json_obj,
											StringRef filename,
											StringMap<SettingT> settingMap);

	/**
	 * @brief parse a JSON config and instantiate AddressGenerator
	 * 
	 * @param json_obj parsed JSON config
	 * @param filename filename of JSON config (just for error message)
	 * @return Expected<AddressGenerator*> a pointer to the generated AddressGenerator instance if there is no error. Otherwise, it contains ModelError
	 */
	Expected<AddressGenerator*> parseAGConfig(json::Object *json_obj,
												StringRef filename);

	/**
	 * @brief Create a AffineAG object according to JSON config
	 * 
	 * @param json_obj parsed JSON config
	 * @param filename filename of JSON config (just for error message)
	 * @return Expected<AddressGenerator*> a pointer to the generated AddressGenerator instance if there is no error. Otherwise, it contains ModelError
	 */
	Expected<AddressGenerator*>  createAffineAG(json::Object *json_obj,
												StringRef filename);

	using AGGen_t = std::function<Expected<AddressGenerator*>(json::Object*,StringRef)>;

} // namespace CGRAOmp


#endif //CGRAModel_H