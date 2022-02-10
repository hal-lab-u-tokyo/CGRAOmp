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
*    File:          /include/VerifyPass.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 15:00:17
*    Last Modified: 11-02-2022 00:49:03
*/
#ifndef VerifyPass_H
#define VerifyPass_H

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/ADT/iterator_range.h"


#include "CGRAOmpPass.hpp"
#include "DecoupledAnalysis.hpp"
#include "CGRAModel.hpp"

using namespace llvm;


namespace CGRAOmp {
	// Verification Kind
	/**
	 * @enum VerificationKind
	 * @brief Kind of verification
	 */
	enum class VerificationKind {
		// Loop structure
		/// Checking the kernel exceeds the maximumn nested level
		MaxNestedLevel,
		/// Checking each memory access meets the allowed access pattern
		MemoryAccess,
		/// Checking the loop has inter-loop-dependency
		InterLoopDep,
		/// Checking if the loop nested structure is perfectly nested or not
		NestedPerfectly,
		/// 
		IterationSize,
		// Inside loop
		/// Checking if the loop contains condional part or not
		Conditional,
		/// Checking if the loop contains function call
		FunctionCall,
	};

	class VerifyPass;

	/**
	 * @class VerifyResultBase
	 * @brief An abstract class for the verification information
	*/
	class VerifyResultBase {
		public:
			/**
			 * @brief Construct a new Verify Result Base object
			 */
			VerifyResultBase() {};

			/**
			 * @brief explicit cast operator for Boolean
			 * Derived classes can custom the violation condition by overriding bool_operator_impl
			 * 
			 * @return true if there is no violation
			 * @return false if the kernel has some violation
			 */
			explicit operator bool() {
				return this->bool_operator_impl();
			}


		// interface to print messages
		/**
		 * @brief An abstract method to print the verification result
		 * 
		 * @param OS an output stream
		 */
		virtual void print(raw_ostream &OS) const = 0;
		/**
		 * @brief dump the verification result like as debugging in LLVM
		 */
		void dump() { this->print(dbgs()); }
		/**
		 * @brief support << operator to show the verification result
		 */
		friend raw_ostream& operator<<(raw_ostream& OS, 
										const VerifyResultBase &v) {
			v.print(OS);
			return OS;
		}

		/**
		 * @brief mark this result as violated
		 */
		void setVio() { isViolate = true; }
		protected:
			/**
			 * @brief An actual impelementation for casting to Boolean
			 */
			virtual bool bool_operator_impl() {
				return !isViolate;
			}
			bool isViolate = false;

	};

	/**
	 * @class VerifyResult
	 * @brief A derived class from VerifyResultBase bundling detailed results for each verification type
	*/
	class VerifyResult : public VerifyResultBase {
		private:
			SmallVector<Loop*> valid_kernels;

		public:
			using KernelList = SmallVector<Loop*>;
			using kernel_iterator = KernelList::iterator;

			VerifyResult() : VerifyResultBase() {}
			void print(raw_ostream &OS) const override;

			/**
			 * @brief register a valid loop kernel for the target CGRA
			 * @param L Loop
			 */
			void registerKernel(Loop *L) {
				valid_kernels.push_back(L);
			}

			inline kernel_iterator kernel_begin() {
				return valid_kernels.begin();
			};

			inline kernel_iterator kernel_end() {
				return valid_kernels.end();
			}
			inline iterator_range<kernel_iterator> kernels() {
				return make_range(kernel_begin(), kernel_end());
			}


		private:
			friend VerifyPass;
			DenseMap<int, VerifyResultBase*> each_result;

			/**
			 * @brief Set the verification result for a kind of rule
			 * @param kind verified kind
			 * @param R the reuslt
			 */
			void setResult(VerificationKind kind, VerifyResultBase *R) {
				each_result[static_cast<int>(kind)] = R;
			}
			/**
			 * @brief bool_operator
			 * 
			 * @return true if there is no violation in bundled results
			 * @return false if there is some violation in a kind of rule
			 */
			bool bool_operator_impl() override;
	};

	/**
	 * @class VerifyPassBase
	 * @brief a template for verification pass
	 * @tparam DerivedT an actual class of pass
	 */
	template <typename DerivedT>
	class VerifyPassBase : public AnalysisInfoMixin<DerivedT> {
		public:
			using LoopList = SmallVector<Loop*>;
		protected:

			/**
			 * @brief search for perfectly nested loops in the function
			 * If a nested loop structure contains more than one innermost loops,
			 * this regards there is no perfectly nested loop.
			 * 
			 * @param F function
			 * @param AR LoopStandardAnalysisResults
			 * @return LoopList: a list of loop
			 */
			LoopList findPerfectlyNestedLoop(Function &F,
				LoopStandardAnalysisResults &AR);
	};

	/**
	 * @class TimeMultiplexedVerifyPass
	 * @brief A function pass to verify the kernel for TimeMultiplexed CGRA
	*/
	class TimeMultiplexedVerifyPass : public AnalysisInfoMixin<TimeMultiplexedVerifyPass> {
		public:
			using Result = VerifyResult;
			Result run(Function &F, FunctionAnalysisManager &AM);
		private:
			friend AnalysisInfoMixin<TimeMultiplexedVerifyPass>;
			static AnalysisKey Key;

	};

	class AGCompatibility;

	/**
	 * @class DecoupledVerifyPass
	 * @brief A function pass to verify the kernel for Decoupled CGRA
	*/
	class DecoupledVerifyPass :
			public VerifyPassBase<DecoupledVerifyPass> {
		public:
			using Result = VerifyResult;
			Result run(Function &F, FunctionAnalysisManager &AM);
		private:
			friend AnalysisInfoMixin<DecoupledVerifyPass>;
			static AnalysisKey Key;

			template <typename AGVerifyPassT>
			AGCompatibility& AG_verification(Loop &L, LoopAnalysisManager &AM,
								LoopStandardAnalysisResults &AR);
	};

	/**
	 * @class AGCompatibility
	 * @brief A derived class from VerifyResultBase describing whether the memory access pattern is compatible with the AGs or not
	*/
	class AGCompatibility : public VerifyResultBase {
		public:

			void print(raw_ostream &OS) const override {}
	};

	/**
	 * @class AffineAGCompatibility
	 * @brief A derived class from AGCompatibility for affine AGs
	*/
	class AffineAGCompatibility : public AGCompatibility {
		public:
			/**
			 * @brief a configration of loop control for a dimention
			 */
			typedef struct {
				bool valid;
				int64_t start;
				int64_t inc;
				int64_t end;
			} config_t;

		private:

	};

	/**
	 * @class VerifyAffineAGCompatiblePass
	 * @brief A function pass to verify the memory access pattern
	*/
	class VerifyAffineAGCompatiblePass :
		public AnalysisInfoMixin<VerifyAffineAGCompatiblePass> {
		public:
			using Result = AffineAGCompatibility;

			Result run(Loop &L, LoopAnalysisManager &AM,
						LoopStandardAnalysisResults &AR);
		private:
			friend AnalysisInfoMixin<VerifyAffineAGCompatiblePass>;
			static AnalysisKey Key;
			DecoupledCGRA *dec_model;

			/**
			 * @brief parse the expression of addition between SCEVs
			 * 
			 * @param SAR parted expression
			 * @param SE a result of ScalarEvolution for the function
			 */
			void parseSCEVAddRecExpr(const SCEVAddRecExpr *SAR,
										ScalarEvolution &SE);
			void parseSCEV(const SCEV *scev, ScalarEvolution &SE, int depth = 0);

			template<int N, typename T>
			bool check_all(SmallVector<T*> &list, ScalarEvolution &SE);

	};

	/**
	 * @class VerifyModulePass
	 * @brief A module pass to verify all annotated functions
	*/
	class VerifyModulePass : public PassInfoMixin<VerifyModulePass> {
		public:
			PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
		private:

	};

	/**
	 * @brief a utility function to obtain analysis results in LoopStandardAnalysisResults
	 * 
	 * @param F Function
	 * @param AM FunctionAnalysisManager
	 * @return LoopStandardAnalysisResults 
	 */
	LoopStandardAnalysisResults getLSAR(Function &F,
								FunctionAnalysisManager &AM);

	CGRAModel* getModelFromLoop(Loop &L, LoopAnalysisManager &AM, LoopStandardAnalysisResults &AR);

}

#endif //VerifyPass_H