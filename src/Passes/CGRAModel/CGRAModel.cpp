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
*    File:          /src/Passes/CGRAModel/CGRAModel.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 15:03:46
*    Last Modified: 14-02-2022 15:33:58
*/
#include "CGRAModel.hpp"

#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Casting.h"
#include "llvm/IR/InstrTypes.h"

#include <fstream>

using namespace llvm;
using namespace CGRAOmp;
using namespace std;

// generator table for each AG type
static StringMap<AGGen_t> AG_gen({
	std::make_pair("affine", createAffineAG),
});

/* ===================== Implementation of ModelError ===================== */
char ModelError::ID = 0;

void ModelError::log(raw_ostream &OS) const
{
	OS << formatv("fail to parse \"{0}\"\n", filename);
	string after_str = "";
	switch (errtype) {
		case ErrorType::MissingKey:
			OS << formatv("Missing key: \"{0}\"", error_key);
			break;
		case ErrorType::InvalidDataType:
			OS << formatv("{0} type data is expected for \"{1}\"", 
				exptected_type, error_key);
			if (json_val != "") {
				OS << " but "  << json_val << " is specified";
			}
			break;
		case ErrorType::InvalidValue:
			OS << formatv("Invalid data \"{0}\" for {1}",
				 error_val, error_key);
			if (valid_values.size() > 0) {
				after_str = formatv("available values: {0}",
					 make_range(valid_values.begin(), valid_values.end()));
			}
			break;
		case ErrorType::NoImplemented:
			OS << formatv("Currently, configuring {0} for {1} is not implemented", error_val, error_key);
	}
	if (region != "") {
		OS << formatv(" in {0}", region);
	}
	if (after_str != "") {
		OS << "\n" << after_str;
	}
}

ModelError::ModelError(StringRef filename, StringRef key, StringRef val,
						ArrayRef<StringRef> list) :
					filename(filename),
					errtype(ErrorType::InvalidValue),
					error_key(key),
					error_val(val.str())
{
	valid_values.clear();
	for (auto v : list) {
		valid_values.push_back(v);
	}
}

/* ============ Utility functions for parsing the configration  ============ */

template <typename SettingT>
bool CGRAOmp::containsValidData(StringMap<SettingT> settingMap, StringRef val)
{
	auto itr = settingMap.find(val);
	return (itr != settingMap.end());
}

template <typename SettingT>
SmallVector<StringRef> CGRAOmp::get_setting_values(StringMap<SettingT> settingMap)
{
	SmallVector<StringRef> vec;
	for (auto itr : settingMap.keys()) {
		vec.push_back(itr);
	}
	return vec;
}

Expected<CGRAModel::CGRACategory> CGRAOmp::getCategory(json::Object *json_obj,
												StringRef filename)
{
	CGRAModel::CGRACategory cgra_cat;
	if (auto *category = json_obj->get(CATEGORY_KEY)) {
		auto cat_val = category->getAsString();
		if (cat_val.hasValue()) {
			if (containsValidData(CGRAModel::CategoryMap,
									cat_val.getValue())) {
				cgra_cat = CGRAModel::CategoryMap[cat_val.getValue()];
			} else {
				// invalid value
				return make_error<ModelError>(filename,
											CATEGORY_KEY, cat_val.getValue(),
						get_setting_values(CGRAModel::CategoryMap));
			}
		} else {
			//not string data
			return make_error<ModelError>(filename, CATEGORY_KEY, "string",
											category);
		}
	} else {
		//missing category
		return make_error<ModelError>(filename, CATEGORY_KEY);
	}
	return cgra_cat;
}


char conditional_key[] = COND_STYLE_KEY;
char interloopdep_key[] = IDP_STYLE_KEY;


/**
 * @details checking steps
 * -# an item whose key = key_str exists?
 * -# its value is an object?
 * -# the object contains an item whose key = "allowed"?
 * -# its value is boolean type?
 * 	- false: return SettingT::No
 * 	- true: continue
 * -# the object contains an item whose key = "type"?
 * -# its value is string type?
 * -# settingMap contains its value as key?
 * 	- yes: return it value
 * 	- no: return ModelError
 */
template <char const *key_str, typename SettingT>
Expected<SettingT> CGRAOmp::getOption(json::Object *json_obj,
										StringRef filename,
										StringMap<SettingT> settingMap)
{

	auto make_model_error = [&](auto... args) {
		auto EI = std::make_unique<ModelError>(filename, args...);
		EI->setRegion(key_str);
		return Error(std::move(EI));
	};

	SettingT style;
	if (auto *cond = json_obj->get(key_str)) {
		auto cond_obj = cond->getAsObject();
		// get allowed or not allowed
		if (cond_obj->get(OPT_ENABLE_KEY)) {
			auto isAllowed = cond_obj->get(OPT_ENABLE_KEY)->getAsBoolean();
			if (isAllowed.hasValue()) {
				if (!isAllowed.getValue()) {
					return SettingT::No;
				}
			} else {
				// not bool type
				return make_model_error(OPT_ENABLE_KEY, "bool", 
								cond_obj->get(OPT_ENABLE_KEY));
			}
		} else {
			// missing allowed
			return make_model_error(OPT_ENABLE_KEY);
		}

		// in the case of conditional allowed
		if (cond_obj->get(OPT_TYPE_KEY)) {
			auto t_str = cond_obj->get(OPT_TYPE_KEY)->getAsString();
			if (t_str.hasValue()) {
				if (containsValidData(settingMap,
										t_str.getValue())) {
					style = settingMap[t_str.getValue()];
				} else {
					// invalid data
					return make_model_error(OPT_TYPE_KEY, t_str.getValue(),
								get_setting_values(settingMap));
				}
			} else {
				// not string type
				return make_model_error(OPT_TYPE_KEY, "string", 
								cond_obj->get(OPT_TYPE_KEY));
			}
		} else {
			// missing style
			return make_model_error(OPT_TYPE_KEY);
		}
	} else {
		//missing conditional
		return make_error<ModelError>(filename, key_str);
	}

	return style;
}

Expected<AddressGenerator*> CGRAOmp::parseAGConfig(json::Object *json_obj,
												StringRef filename)
{
	auto make_model_error = [&](auto... args) {
		auto EI = std::make_unique<ModelError>(filename, args...);
		EI->setRegion(AG_CONF_KEY);
		return Error(std::move(EI));
	};

	if (json_obj->get(AG_CONF_KEY)) {
		auto *conf_obj = json_obj->get(AG_CONF_KEY)->getAsObject();
		// get AG style
		StringRef style_str;
		if (conf_obj->get(AG_STYLE_KEY)) {
			auto style = conf_obj->get(AG_STYLE_KEY)->getAsString();
			if (style.hasValue()) {
				style_str = *style;
			} else {
				return make_model_error(AG_STYLE_KEY, "string",
										conf_obj->get(AG_STYLE_KEY));
			}
		} else {
			// missing AG style setting
			return make_model_error(AG_STYLE_KEY);
		}
		if (AG_gen.find(style_str) != AG_gen.end()) {
			if (auto AG = AG_gen[style_str](conf_obj, filename)) {
				return *AG;
			} else {
				return AG.takeError();
			}

		} else {
			// unknown type for AG style
			return make_model_error(AG_STYLE_KEY, style_str,
						get_setting_values(AG_gen));
		}

	} else {
		//missing address generator
		return make_error<ModelError>(filename, AG_CONF_KEY);
	}
}
Expected<AddressGenerator*> CGRAOmp::createAffineAG(json::Object *json_obj,
												StringRef filename)
{
	auto make_model_error = [&](auto... args) {
		auto EI = std::make_unique<ModelError>(filename, args...);
		EI->setRegion(AG_CONF_KEY);
		return Error(std::move(EI));
	};

	// get max nested level
	if (json_obj->get(AG_MAX_NEST_KEY)) {
		auto max_nests = json_obj->get(AG_MAX_NEST_KEY)->getAsInteger();
		if (max_nests.hasValue()) {
			if (*max_nests > 0) {
				return new AffineAG(*max_nests);
			} else {
				// negative integer
				return make_model_error(AG_MAX_NEST_KEY, to_string(*max_nests),
										ArrayRef<StringRef>({}));
			}
		} else {
			// not integer
			return make_model_error(AG_MAX_NEST_KEY, "integer",
								json_obj->get(AG_MAX_NEST_KEY));
		}
	} else {
		// missing max nested level setting
		// consider there is no limitation regarding the nested level
		return new AffineAG();
	}
}

Expected<CGRAModel*> CGRAOmp::parseCGRASetting(StringRef filename,
						ModuleAnalysisManager &MAM)
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

	// fail to parse
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
	CGRAModel *model;
	// check common settings
	auto cgra_cat = getCategory(top_obj, filename);
	if (!cgra_cat) {
		// in the case of invalid category
		return cgra_cat.takeError();
	}
	auto cond_type = getOption<conditional_key, CGRAModel::ConditionalStyle>(top_obj,filename, CGRAModel::CondStyleMap);
	if (!cond_type) {
		return cond_type.takeError();
	}
	auto ild_type = getOption<interloopdep_key, CGRAModel::InterLoopDep>(top_obj,filename, CGRAModel::InterLoopDepMap);
	if (!ild_type) {
		return ild_type.takeError();
	}

	// instantiate an actual class of model
	switch (*cgra_cat) {
		case CGRAModel::CGRACategory::Decoupled:
			{
				auto AG = parseAGConfig(top_obj, filename);
				if (AG) {
					model = new DecoupledCGRA(filename, *AG,
												*cond_type, *ild_type);
					model->asDerived<DecoupledCGRA>()->getAG()->getKind();
				} else {
					return AG.takeError();
				}
			}
			break;
		case CGRAModel::CGRACategory::TimeMultiplexed:
			model = new TMCGRA(filename);
			break;
		default:
			auto config = make_pair<StringRef, StringRef>(CATEGORY_KEY,
					top_obj->get(CATEGORY_KEY)->getAsString().getValue());
			return make_error<ModelError>(filename,	config);
	}


	// add supported instructions
	auto inst_list = getStringArray(top_obj, GEN_INST_KEY, filename);
	if (!inst_list) {
		return inst_list.takeError();
	} else {
		for (auto inst : *inst_list) {
			Error E = model->addSupportedInst(inst);
			if (E) {
				return Error(std::move(E));
			}
		}
	}

	// add custom instructions
	auto cust_list = getStringArray(top_obj, CUSTOM_INST_KEY, filename);
	if (!cust_list) {
		return cust_list.takeError();
	} else {
		for (auto inst : *cust_list) {
			model->addCustomInst(inst, MAM);
		}
	}

	// add instruction mapping (optional)
	if (top_obj->get(INST_MAP_KEY)) {
		auto inst_map_obj = top_obj->get(INST_MAP_KEY)->getAsArray();
		for (auto entry : *inst_map_obj) {
			auto map_cond = createMapCondition(entry.getAsObject(), filename);
			if (!map_cond) {
				return map_cond.takeError();
			} else {
				if (auto E = model->add_map_entry(map_cond->first,
								map_cond->second)) {
					return std::move(E);
				}
			}
		}
	}

	return model;
}


/* =================== Implementation of CGRAModel class =================== */
// valid settings for CGRA category
StringMap<CGRAModel::CGRACategory> CGRAModel::CategoryMap({
	make_pair("time-multiplexed", CGRAModel::CGRACategory::TimeMultiplexed),
	make_pair("decoupled", CGRAModel::CGRACategory::Decoupled),
});
// valid settings for Conditional Style
StringMap<CGRAModel::ConditionalStyle> CGRAModel::CondStyleMap({
	make_pair("MuxInst", CGRAModel::ConditionalStyle::MuxInst),
	make_pair("TriState", CGRAModel::ConditionalStyle::TriState),
});
// valid settings for inter-loop dependecy
StringMap<CGRAModel::InterLoopDep> CGRAModel::InterLoopDepMap({
	make_pair("generic", CGRAModel::InterLoopDep::Generic),
	make_pair("BackwardInst", CGRAModel::InterLoopDep::BackwardInst),
});


Error CGRAModel::addSupportedInst(StringRef opcode)
{
	return std::move(inst_map.add_generic_inst(opcode));
}

void CGRAModel::addCustomInst(StringRef opcode, ModuleAnalysisManager &MAM)
{
	inst_map.add_custom_inst(opcode, MAM);
}



Error CGRAModel::add_map_entry(StringRef opcode, MapCondition *map_cond)
{
	if (auto E = inst_map.add_map_entry(opcode, map_cond)) {
		return std::move(E);
	} else {
		return ErrorSuccess();
	}
}

InstMapEntry* CGRAModel::isSupported(Instruction *I)
{
	return inst_map.find(I);
}

/* ======= Implementation of DecoupleCGRA and replated classes ======= */
DecoupledCGRA::DecoupledCGRA(const DecoupledCGRA &rhs) : 
	CGRAModel(rhs)
{
	using AGKind = AddressGenerator::Kind;
	// copy as an actual derived class
	switch(rhs.getAG()->getKind()) {
		case AGKind::Affine:
			AG = new AffineAG(*(rhs.AG->asDerived<AffineAG>()));
			break;
		case AGKind::FullState:
			AG = new FullStateAG(*(rhs.AG->asDerived<FullStateAG>()));
			break;
	}
}

TMCGRA::TMCGRA(const TMCGRA &rhs) : CGRAModel(rhs)
{
	assert(!"Copy constructor for TMCGRA is not implemented");
}

AffineAG::AffineAG(const AffineAG& rhs) : AddressGenerator(rhs)
{
	max_nests = rhs.max_nests;
}

FullStateAG::FullStateAG(const FullStateAG &rhs) : AddressGenerator(rhs)
{
	assert(!"Copy constructor for FullStateAG is not implemented");
}