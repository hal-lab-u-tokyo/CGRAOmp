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
*    File:          /include/OptionPlugin.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 14:20:02
*    Last Modified: 14-09-2021 01:55:22
*/
#ifndef OptionPlugin_H
#define OptionPlugin_H

#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"

#include <regex>
#include <string>

using namespace llvm;
using namespace std;

namespace CGRAOmp
{
	/**
	 * @class OptKeyValue
	 * @brief A pair of key and value for command line option
	*/
	class OptKeyValue {
		public:
			/**
			 * @brief Default constructor
			 * Instances made by this constructor are always invalidated
			 */
			OptKeyValue() : key(""), value(""), valid(false) {}
			/**
			 * @brief Construct a new Key-Value object
			 * 
			 * @param key key of string
			 * @param value value of string
			 */
			OptKeyValue(string key, string value) :
				key(key), value(value), valid(true) {};
			/**
			 * @brief　converting constructor from char*
			 * 
			 * @param s string
			 * 	- It must be "key=value".
			 * 	- Otherwise, the instance will be invalidated
			 */
			OptKeyValue(const char* s) : OptKeyValue(string(s)) {}
			/**
			 * @brief　converting constructor from std::string
			 * 
			 * @param s string
			 * 	- It must be "key=value".
			 * 	- Otherwise, the instance will be invalidated
			 */
			OptKeyValue(string keyvalue);

			/// copy constructor
			OptKeyValue(const OptKeyValue &) = default;
			/// move constructor
			OptKeyValue(OptKeyValue &&) = default;

			/**
			 * @brief Get both key and value
			 * 
			 * @return pair<string, string>, the first is key, and the second is value.
			 */
			pair<string, string> get() {
				return make_pair(key, value);
			}

			/**
			 * @brief Get the key
			 * 
			 * @return string of key
			 */
			string get_key() const {
				return key;
			}

			/**
			 * @brief Get the value
			 * 
			 * @return string of value
			 */
			string get_value() const {
				return value;
			}

			/**
			 * @brief explicit operator for bool
			 * 
			 * @return true in the case of valid key-value
			 * @return Otherwise, false
			 * 
			 * The following code can check wherether the option is valid or not.
			 * @code
			 * cl::opt<OptKeyValue> MyOption("optname");
			 * ...
			 * if (MyOption) {
			 * // have a valid key-value
			 * } else {
			 * // the option is not specified or invalid
			 * }
			 * @endcode
			 * 
			 */
			explicit operator bool() const noexcept {
				return valid;
			}

			/**
			 * @brief equality comparetor
			 */
			bool operator!=(const OptKeyValue &rhs) {
				return !(*this == rhs);
			}

			/**
			 * @brief inequality comparator
			 */
			bool operator==(const OptKeyValue &rhs) {
				return (key == rhs.key) && (value == rhs.value);
			}

			/// move assignment operator
			OptKeyValue& operator=(OptKeyValue &&r) = default;
			/// copy assignement operator
			OptKeyValue& operator=(const OptKeyValue &r) = default;

		private:
			bool valid;
			string key;
			string value;
	};

	/// path to model config
	extern cl::opt<string> PathToCGRAConfig;
	/// alias of config file path
	extern cl::alias PathToCGRAConfigAlias;

	/// a verbose option
	extern cl::opt<bool> OptVerbose;
	/// key string for opcode in DOT graph
	extern cl::opt<string> OptDFGOpKey;
	/// to save human readable DOT
	extern cl::opt<bool> OptDFGPlainNodeName;

	/// to set common preference for graph
	extern cl::list<OptKeyValue> OptDFGGraphProp;
	/// to set common preference for node
	extern cl::list<OptKeyValue> OptDFGNodeProp;
	/// to set common preference for edge
	extern cl::list<OptKeyValue> OptDFGEdgeProp;
}

namespace llvm {

/// stream insertion operator for llvm::raw_ostream
raw_ostream& operator<<(raw_ostream &OS, const CGRAOmp::OptKeyValue &D);

namespace cl {

using namespace CGRAOmp;

/// non-member operator overloading for OptionValue impelentation
bool operator!=(const OptKeyValue &lhs, const OptKeyValue &rhs);
/// non-member operator overloading for OptionValue impelentation
bool operator==(const OptKeyValue &lhs, const OptKeyValue &rhs);

/// template specialization of OptionValue for OptKeyValue
template <>
struct OptionValue<OptKeyValue> final : OptionValueCopy<OptKeyValue> {
  using WrapperType = OptKeyValue;

  OptionValue() = default;

  OptionValue(const OptKeyValue &V) { this->setValue(V); }

  OptionValue<OptKeyValue> &operator=(const OptKeyValue &V) {
    setValue(V);
    return *this;
  }

private:
  void anchor() override;
};

/// template specialization of parser for OptKeyValue
template <> class parser<OptKeyValue> : public basic_parser<OptKeyValue> {
	public:
		parser(Option &O) : basic_parser(O) {}

		// parse - Return true on error.
		bool parse(Option &O, StringRef, StringRef Arg, OptKeyValue &Value) {
			Value = OptKeyValue(Arg.str());
			if (!Value) {
				return O.error("'" + Arg +
							"' value invalid for key-value argument!");
			}
			return false;
		}

		// getValueName - Overload in subclass to provide a better default value.
		StringRef getValueName() const override { return "key=value"; }

		void printOptionDiff(const Option &O, const OptVal &V, const OptVal &Default, size_t GlobalWidth) const;

		// An out-of-line virtual method to provide a 'home' for this class.
		void anchor() override;
};

extern template class basic_parser<OptKeyValue>;

} // end namespace cl

} // end namespace llvm

#endif //OptionPlugin_H