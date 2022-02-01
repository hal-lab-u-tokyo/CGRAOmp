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
*    File:          /include/CGRAOmpPass.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 14:19:42
*    Last Modified: 31-01-2022 13:45:48
*/
#ifndef CGRAOmpPass_H
#define CGRAOmpPass_H

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Support/Error.h"

#include "CGRAModel.hpp"

#define OMP_STATIC_INIT_SCHED		2
#define OMP_STATIC_INIT_PLASTITER	3
#define OMP_STATIC_INIT_PLOWER		4
#define OMP_STATIC_INIT_PUPPER		5
#define OMP_STATIC_INIT_PSTRIDE		6
#define OMP_STATIC_INIT_INCR		7
#define OMP_STATIC_INIT_CHUNK		8
#define OMP_STATIC_INIT_OPERAND_N	(OMP_STATIC_INIT_CHUNK + 1)

#include <system_error>

using namespace llvm;

#define KERNEL_INFO_PREFIX ".omp_offloading.entry"
#define ADD_FUNC_PASS(P) (createModuleToFunctionPassAdaptor(P))
#define ADD_LOOP_PASS(P) (createModuleToFunctionPassAdaptor(createFunctionToLoopPassAdaptor(P)))

namespace CGRAOmp {


	/**
	 * @class ModelManager
	 * @brief An interface for each LLVM pass to get the target CGRAModel.
	 * It can be obtained as an analysis result of ModelManagerPass.
	*/
	class ModelManager {
		public:
			ModelManager() = delete;
			/**
			 * @brief Construct a new ModelManager object
			 * 
			 * @param CM a generated CGRAModel
			 */
			explicit ModelManager(CGRAModel *CM) : model(CM) {};
			/// copy constructor
			ModelManager(const ModelManager&) = default;
			/// move constructor
			ModelManager(ModelManager&&) = default;
			/**
			 * @brief Get the CGRAModel object
			 * @return CGRAModel* this manger contains
			 */
			CGRAModel* getModel() const { return model; }

			/// implemented for enabling getCacheResult from inner modules
			template <typename IRUnitT, typename InvT>
			bool invalidate(IRUnitT& IR, const PreservedAnalyses &PA,
								InvT &Inv);
		private:
			CGRAModel* model;
	};

	/**
	 * @class ModelManagerPass
	 * @brief Module Pass to give the ModuleManager
	*/
	class ModelManagerPass : public AnalysisInfoMixin<ModelManagerPass> {
		public:
			using Result = ModelManager;
			Result run(Module &M, ModuleAnalysisManager &AM);
		private:
			friend AnalysisInfoMixin<ModelManagerPass>;
			static AnalysisKey Key;
			CGRAModel *model;
	};

	class ModelManagerFunctionProxy : public AnalysisInfoMixin<ModelManagerFunctionProxy> {
		public:
			using Result = ModelManager;
			Result run(Function &F, FunctionAnalysisManager &AM);
		private:
			friend AnalysisInfoMixin<ModelManagerFunctionProxy>;
			static AnalysisKey Key;
	};

	class ModelManagerLoopProxy : public AnalysisInfoMixin<ModelManagerLoopProxy> {
		public:
			using Result = ModelManager;
			Result run(Loop &L, LoopAnalysisManager &AM,
						LoopStandardAnalysisResults &AR);
		private:
			friend AnalysisInfoMixin<ModelManagerLoopProxy>;
			static AnalysisKey Key;
	};

	/**
	 * @class OmpKernelAnalysisPass
	 * @brief A module pass to find functions annotated as OpenMP kernels
	*/
	class OmpKernelAnalysisPass :
			public AnalysisInfoMixin<OmpKernelAnalysisPass> {
		public:
			using Result = StringMap<Function*>;
			Result run(Module &M, ModuleAnalysisManager &AM);
		private:
			friend AnalysisInfoMixin<OmpKernelAnalysisPass>;
			static AnalysisKey Key;
	};

	/**
	* @class OmpScheduleInfo
	* @brief Bundled values related to OpenMP statinc scheduling
	*/
	class OmpScheduleInfo {
		using Info_t = SetVector<Value*>;
		public:
			OmpScheduleInfo(Value *schedule_type,
							Value *last_iter_flag,
							Value *lower_bound,
							Value *upper_bound,
							Value *stride,
							Value *increment,
							Value *chunk) {
				info.insert(schedule_type);
				info.insert(last_iter_flag);
				info.insert(lower_bound);
				info.insert(upper_bound);
				info.insert(stride);
				info.insert(increment);
				info.insert(chunk);
				valid = true;
			};
			OmpScheduleInfo() {
				valid = false;
				for (int i = 0; i < OMP_STATIC_INIT_OPERAND_N; i++) {
					info.insert(nullptr);
				}
			};
			bool invalidate(Function &F, const PreservedAnalyses &PA,
						FunctionAnalysisManager::Invalidator &Inv);

			explicit operator bool() const noexcept {
				return valid;
			}
			Info_t::iterator begin() {
				return info.begin();
			}
			Info_t::iterator end() {
				return info.end();
			}
			bool contains(Value* V) {
				return info.contains(V);
			}

			Value* get_schedule_type() {
				return info[0];
			}
			Value* get_last_iter_flag() {
				return info[1];
			}
			Value* get_lower_bound() {
				return info[2];
			}
			Value* get_upper_bound() {
				return info[3];
			}
			Value* get_stride() {
				return info[4];
			}
			Value* get_increment() {
				return info[5];
			}
			Value* get_chunk() {
				return info[6];
			}
		private:
			Info_t info;
			bool valid;
	};

	/**
	 * @class OmpStaticShecudleAnalysis
	 * @brief A function pass to analyze OpenMP static scheduling
	 **/
	class OmpStaticShecudleAnalysis:
			public AnalysisInfoMixin<OmpStaticShecudleAnalysis> {
		public:
			// using Result = Expected<OmpScheduleInfo>;
			using Result = OmpScheduleInfo;
			Result run(Function &F, FunctionAnalysisManager &AM);

		private:
			friend AnalysisInfoMixin<OmpStaticShecudleAnalysis>;
			static AnalysisKey Key;
	};
}

#endif //CGRAOmpPass_H