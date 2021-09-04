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
*    File:          /Passes/CGRAOmpPlugins/CGRAModel.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 15:03:46
*    Last Modified: 03-09-2021 15:51:23
*/
#include "CGRAModel.hpp"

#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Casting.h"

#include <fstream>

using namespace llvm;
using namespace CGRAOmp;
using namespace std;

/* ---------- Implementation of ModelError ----------- */
char ModelError::ID = 0;

void ModelError::log(raw_ostream &OS) const
{
	OS << formatv("fail to parse \"{0}\"\n", filename);
	switch (errtype) {
		case ErrorType::MissingKey:
			OS << formatv("Missing key: {0}", error_key);
			break;
		case ErrorType::InvalidDataType:
			OS << formatv("{0} type data is expected for \"{1}\"", 
				exptected_type, error_key);
			if (json_val) {
				OS << " but "  << *json_val << " is specified";
			}
			break;
		case ErrorType::InvalidValue:
			OS << formatv("Invalid data \"{0}\" for {1}\n"
			"available values: {2}",
				 error_val, error_key,
				 make_range(valid_values.begin(), valid_values.end()));
			break;
	}
	if (_region != "") {
		OS << formatv(" in {0}", _region);
	}
}

/**
 * @brief Construct a new ModelError when a value is invalid for the specified key
 * 
 * @param filename filename of the parsed configration file 
 * @param key key string containing invalid value
 * @param val the invalid value string
 * @param list list of valid values
 */
ModelError::ModelError(StringRef filename, StringRef key, StringRef val,
						ArrayRef<StringRef> list) :
					filename(filename),
					errtype(ErrorType::InvalidValue),
					error_key(key),
					error_val(val)
{
	valid_values.clear();
	for (auto v : list) {
		valid_values.push_back(v);
	}
}

/* ---------- Utility functions for parsing the configration  ---------- */
/// Check if val is a valid setting for SettingT
template <typename SettingT>
bool containsValidData(StringMap<SettingT> settingMap, StringRef val)
{
	auto itr = settingMap.find(val);
	return (itr != settingMap.end());
}

/// Extract values from SettingMap
template <typename SettingT>
SmallVector<StringRef> get_setting_values(StringMap<SettingT> settingMap)
{
	SmallVector<StringRef> vec;
	for (auto itr : settingMap.keys()) {
		vec.push_back(itr);
	}
	return vec;
}

// 
Expected<CGRAModel::CGRACategory> getCategory(json::Object *json_obj,
												StringRef filename)
{
	CGRAModel::CGRACategory cgra_cat;
	if (auto *category = json_obj->get("category")) {
		auto cat_val = category->getAsString();
		if (cat_val.hasValue()) {
			if (containsValidData(CGRAModel::CategoryMap,
									cat_val.getValue())) {
				cgra_cat = CGRAModel::CategoryMap[cat_val.getValue()];
			} else {
				// invalid value
				return make_error<ModelError>(filename,
											"category", cat_val.getValue(),
						get_setting_values(CGRAModel::CategoryMap));
			}
		} else {
			//not string data
			return make_error<ModelError>(filename, "category", "string",
											category);
		}
	} else {
		//missing category
		return make_error<ModelError>(filename, "category");
	}
	return cgra_cat;
}


/* ---------- Implementation of CGRAModel class ---------- */

/// valid settings for CGRA category
StringMap<CGRAModel::CGRACategory> CGRAModel::CategoryMap({
	make_pair("generic", CGRAModel::CGRACategory::Generic),
	make_pair("decoupled", CGRAModel::CGRACategory::Decoupled)
});


Expected<CGRAModel> CGRAOmp::parseCGRASetting(StringRef filename)
{
	//open json file
	error_code fopen_ec;
	ifstream ifs(filename.str());

	//fail to open file
	if (ifs.fail()) {
		error_code EC;
		return make_error<StringError>(
			formatv("{0}: {1}", strerror(errno), filename), EC);
	}

	// load file contents and parse it 
	string json_str;
	json_str = string(istreambuf_iterator<char>(ifs),
							istreambuf_iterator<char>());
	auto parsed = json::parse(json_str);

	if (!parsed) {
		error_code parse_ec;
		Error E = parsed.takeError();
		string msg;
		raw_string_ostream str_os(msg);
		logAllUnhandledErrors(std::move(E), str_os);
		return make_error<StringError>(
			formatv("{0} is invalid JSON file\n{1}", filename, msg),
			parse_ec);
	}
	json::Object *top_obj = (*parsed).getAsObject();

	// Parse JSON file
	auto cgra_cat = getCategory(top_obj, filename);
	if (cgra_cat) {
		errs() << static_cast<std::underlying_type<CGRAModel::CGRACategory>::type>(*cgra_cat) << "\n";

	} else {
		return cgra_cat.takeError();
	}


	return CGRAModel();
}
