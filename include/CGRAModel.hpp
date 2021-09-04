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
*    Last Modified: 03-09-2021 15:51:21
*/
#ifndef CGRAModel_H
#define CGRAModel_H

#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"


using namespace llvm;

namespace CGRAOmp
{
	class InstMapEntry {
		public:
			InstMapEntry() {};
	};

	class CGRAModel {
		public:
			CGRAModel() {
				errs() << "Constructor\n";
			}
			// CGRA style settings
			enum class CGRACategory {
				Generic,
				Decoupled
			};

			enum class ConditionalStyle {
				No,
				MuxInst,
				TriState
			};

			enum class InterLoopDep {
				No,
				Generic,
				BackwardInst,
			};
			// Available set of each value
			static StringMap<CGRACategory> CategoryMap;

		protected:
			ConditionalStyle cond;
			InterLoopDep inter_loop_dep;
			StringSet<> support_instr;
			SmallVector<InstMapEntry> inst_map;
	};

	class DecoupledCGRA : public CGRAModel {
		public:
			DecoupledCGRA();

		private:
	};

	class GenericCGRA : public CGRAModel {

	};

	Expected<CGRAModel> parseCGRASetting(StringRef filename);

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
					exptected_type(exptected_type),
					json_val(v) {}

			ModelError(StringRef filename, StringRef key, 
						StringRef val, ArrayRef<StringRef> list);

			void setRegion(StringRef region) {
				_region = region;
			}

			static char ID;
			std::error_code convertToErrorCode() const override {
				// Unknown error
				return std::error_code(41, std::generic_category());
			}
			void log(raw_ostream &OS) const override;

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
			};

			ErrorType errtype;
			json::Value *json_val = nullptr;
			StringRef filename;
			StringRef error_key;
			StringRef error_val;
			StringRef exptected_type;
			StringRef _region = "";
			SmallVector<StringRef> valid_values;

	};
} // namespace CGRAOmp


#endif //CGRAModel_H