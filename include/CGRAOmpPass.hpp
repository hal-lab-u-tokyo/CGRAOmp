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
*    Last Modified: 22-02-2022 03:38:11
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
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/FormatVariadic.h"

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
#define OFFLOADINFO_METADATA_NAME "omp_offload.info"
#define OUTLINED_FUNC_NAME_FMT "__omp_offloading_{0:x-}_{1:x-}_{2}_l{3:d}"
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

	/**
	 * @class ModelManagerFunctionProxy
	 * @brief A proxy to access model manager from function passes
	 * @remark In advance of access, ModelManagerPass must be executed because of the cache meachanism of the AnalysisManager
	 */
	class ModelManagerFunctionProxy : public AnalysisInfoMixin<ModelManagerFunctionProxy> {
		public:
			using Result = ModelManager;
			Result run(Function &F, FunctionAnalysisManager &AM);
		private:
			friend AnalysisInfoMixin<ModelManagerFunctionProxy>;
			static AnalysisKey Key;
	};

	/**
	 * @class ModelManagerLoopProxy
	 * @brief A proxy to access model manager from loop passes
	 * @remark In advance of access, ModelManagerPass must be executed because of the cache meachanism of the AnalysisManager
	 */
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
	 * @class OmpKernelInfo
	 * @brief A set of information regarding OpenMP target kernel
	*/
	class OmpKernelInfo {
		public:
			struct OffloadMetadata_t {
				int metadata_kind;
				int file_dev_ID;
				int file_ID;
				StringRef func_name;
				int line;
				int order;
			};

			/// implemented for enabling getCacheResult from inner modules
			template <typename IRUnitT, typename InvT>
			bool invalidate(IRUnitT& IR, const PreservedAnalyses &PA,
								InvT &Inv);

			using MetadataList = SmallVector<OffloadMetadata_t>;
			using md_iterator = MetadataList::iterator;

			using FunctionList = SmallVector<Function*>;
			using func_iterator = FunctionList::iterator;


			// metadata itarators
			inline md_iterator md_begin() {
				return md_list.begin();
			}

			inline md_iterator md_end() {
				return md_list.end();
			}

			inline iterator_range<md_iterator> metadata() {
				return make_range(md_begin(), md_end());
			}

			// kernel iterators
			inline func_iterator kernel_begin() {
				return kernel_list.begin();
			}

			inline func_iterator kernel_end() {
				return kernel_list.end();
			}

			inline iterator_range<func_iterator> kernels() {
				return make_range(kernel_begin(), kernel_end());
			}

			/**
			 * @brief add kernel function as an entry
			 * 
			 * @param offload Offloading function corresponding to the target region
			 * @param kernel Outlined function for the kernel
			 */
			void add_kernel(Function *offload, Function *kernel) {
				kernel_list.emplace_back(kernel);
				offload_func_list.emplace_back(offload);
			}

			/**
			 * @brief Get the Offload Function pointer from the kernel function
			 * 
			 * @param kernel kernel function
			 * @return Function* the offloading function
			 */
			Function* getOffloadFunction(Function *kernel) {
				auto kernel_it = kernel_begin();
				auto offload_it = offload_func_list.begin(); 
				for (;kernel_it != kernel_end(); kernel_it++, offload_it++) {
					if (kernel == *kernel_it) return *offload_it;
				}
				return nullptr;
			}

			/**
			 * @brief get metadata about the kernel function 
			 * 
			 * @param offload offloading function
			 * @return md_iterator an iterator for the metadata set
			 */
			md_iterator getMetadata(Function *offload);

			/**
			 * @brief parse metadata from Module and save it
			 * 
			 * @param M Module
			 * 
			 * @remarks If there is no metadata "omp_offload.info", it poses an error and finish the process
			 */
			void setOffloadMetadata(Module &M);

			/**
			 * @brief get the line number where the kernel starts
			 * 
			 * @param kernel kernel function
			 * @return int line number
			 */
			int getKernelLine(Function *kernel);
		private:
			MetadataList md_list;
			FunctionList kernel_list;
			FunctionList offload_func_list;

	};

	/**
	 * @class OmpKernelAnalysisPass
	 * @brief A module pass to find functions annotated as OpenMP kernels
	*/
	class OmpKernelAnalysisPass :
			public AnalysisInfoMixin<OmpKernelAnalysisPass> {
		public:
			using Result = OmpKernelInfo;
			Result run(Module &M, ModuleAnalysisManager &AM);
		private:
			friend AnalysisInfoMixin<OmpKernelAnalysisPass>;
			friend class RemoveScheduleRuntimePass;
			static AnalysisKey Key;
			
			
	};

	/**
	* @class OmpScheduleInfo
	* @brief Bundled values related to OpenMP statinc scheduling
	*/
	class OmpScheduleInfo {
		using Info_t = SetVector<Value*>;
		public:
			OmpScheduleInfo(CallBase *caller,
							Value *schedule_type,
							Value *last_iter_flag,
							Value *lower_bound,
							Value *upper_bound,
							Value *stride,
							Value *increment,
							Value *chunk) : 
				caller(caller) {
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

			CallBase *get_caller() {
				return caller;
			}

			
		private:
			Info_t info;
			CallBase *caller;
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
			friend class RemoveScheduleRuntimePass;
			static AnalysisKey Key;
	};

	class RemoveScheduleRuntimePass:
		public PassInfoMixin<RemoveScheduleRuntimePass> {
		public:
			PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
	};

}

#endif //CGRAOmpPass_H