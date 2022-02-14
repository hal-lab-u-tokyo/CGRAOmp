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
*    Last Modified: 14-02-2022 15:29:55
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
#include "llvm/Support/Debug.h"

#include "CGRAOmpPass.hpp"
#include "DecoupledAnalysis.hpp"
#include "CGRAModel.hpp"

#include <map>

using namespace llvm;


namespace CGRAOmp {
	// Verification Kind
	/**
	 * @enum VerificationKind
	 * @brief Kind of verification
	 */
	enum class VerificationKind {
		///Summary of the verfication for a function including kernels
		FunctionSummary,
		/// Summary of the loop verification
		KernelSummary,
		// Loop structure
		/// Checking the instructions needed in the kernel are supported or not
		InstAvailability,
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
			explicit VerifyResultBase(VerificationKind kind) : kind(kind) {};

			/// default constructor
			VerifyResultBase() = delete;

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

			/**
			 * @brief get the kind of derived class
			 * @return VerificationKind 
			 */
			VerificationKind getKind() const {
				return kind;
			}

		protected:
			/**
			 * @brief An actual impelementation for casting to Boolean
			 */
			virtual bool bool_operator_impl() {
				return !isViolate;
			}
			bool isViolate = false;


		private:
			VerificationKind kind;
	};


	/**
	 * @class LoopVerifyResult
	 * @brief A derived class from VerifyResultBase bundling detailed results for each verification type
	*/
	class LoopVerifyResult : public VerifyResultBase {

		public:
			/**
			 * @brief Construct a new Verify Result object
			 */
			LoopVerifyResult() : VerifyResultBase(VerificationKind::KernelSummary) {}
			void print(raw_ostream &OS) const override;

			static bool classof(const VerifyResultBase* R) {
				return R->getKind() == VerificationKind::KernelSummary;
			}

			/**
			 * @brief Set the verification result for a kind of rule
			 * @param R the reuslt
			 */
			void setResult(VerifyResultBase *R) {
				each_result[static_cast<int>(R->getKind())] = R;
			}

		private:
			DenseMap<int, VerifyResultBase*> each_result;

			/**
			 * @brief bool_operator
			 * 
			 * @return true if there is no violation in bundled results
			 * @return false if there is some violation in a kind of rule
			 */
			bool bool_operator_impl() override;
	};

	/**
	 * @class VerifyResult
	 * @brief A derived class from VerifyResultBase bunding all kernel verification result
	*/
	class VerifyResult : public VerifyResultBase {
		private:
			SmallVector<Loop*> valid_kernels;

		public:
			using KernelList = SmallVector<Loop*>;
			using kernel_iterator = KernelList::iterator;

			/**
			 * @brief Construct a new Verify Result object
			 */
			VerifyResult() : VerifyResultBase(VerificationKind::FunctionSummary) {}
			void print(raw_ostream &OS) const override;

			/**
			 * @brief register a valid loop kernel for the target CGRA
			 * @param L Loop
			 */
			void registerKernel(Loop *L, LoopVerifyResult LVR) {
				valid_kernels.push_back(L);
				loop_verify_results[L] = LVR;
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
			static bool classof(const VerifyResultBase* R) {
				return R->getKind() == VerificationKind::FunctionSummary;
			}


		private:
			std::map<Loop*, LoopVerifyResult> loop_verify_results;
	};

	/**
	 * @brief Simple verification result only with a verification message
	 * 
	 * @tparam Kind Verification Kind
	 */
	template<VerificationKind Kind>
	class SimpleVerifyResult : public VerifyResultBase {
		public:
			SimpleVerifyResult() : SimpleVerifyResult(std::string()) {}
			SimpleVerifyResult(std::string msg) : VerifyResultBase(Kind),
				msg(msg) {};

			void setMessage(StringRef msg) {
				msg = msg.str();
			}
			void print(raw_ostream &OS) const {
				OS << msg;
			}
		private:
			std::string msg;
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
			// using InstList = SmallVector<Instruction*>;
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

	// class AGCompatibility;

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

			// template <typename AGVerifyPassT>
			// AGCompatibility& AG_verification(Loop &L, LoopAnalysisManager &AM,
			// 					LoopStandardAnalysisResults &AR);
	};

	/**
	 * @class  VerifyInstAvailabilityPass
	 * @brief A template class for verify the instruction availability
	 * It is possible to customize the routine with specilization
	 * 
	 * @tparam  VerifyPassTy Type of verification e.g., DecoupleVerifyPass
	 */
	template <typename VerifyPassTy>
	class VerifyInstAvailabilityPass : 
				public AnalysisInfoMixin<VerifyInstAvailabilityPass<VerifyPassTy>> {
		public:
			using Result = SimpleVerifyResult<VerificationKind::InstAvailability>;

			Result run(Loop &L, LoopAnalysisManager &AM,
						LoopStandardAnalysisResults &AR) {
				
				#define DEBUG_TYPE "cgraomp"
				LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX 
					<< "Verifying insturction compatibility: "
					<< L.getName() << "\n");
				#undef DEBUG_TYPE

				auto unsupported_insts = checkUnsupportedInst(L, AM, AR);
				if (unsupported_insts.hasValue()) {
					// Invalid
					SmallSet<StringRef, 32> opcodes;
					for (auto inst : *unsupported_insts) {
						opcodes.insert(inst->getOpcodeName());
					}
					std::string msg = "Unsupported instructions: ";
					for (auto opcode_name : opcodes) {
						msg += opcode_name.str() + " ";
					}
					auto R = SimpleVerifyResult<VerificationKind::InstAvailability>(msg);
					R.setVio();
					return R;
				} else {
					auto R = SimpleVerifyResult<VerificationKind::InstAvailability>();
					return R;
				}
			};

		private:
			friend AnalysisInfoMixin<VerifyInstAvailabilityPass<VerifyPassTy>>;
			static AnalysisKey Key;
			using InstList = SmallVector<Instruction*>;

			/**
			 * @brief default routine to check whether the kernel contains unspported instructions
			 * 
			 * @param L Loop
			 * @param AM LoopAnalysisManager
			 * @param AR LoopStandardAnalysisResults
			 * @return Optional<InstList> if any unsupported instructions are included, it returns a list of them, otherwise None.
			 * 
			 * @remark Please specilize this method depending on the  target CGRA type
			 */
			Optional<InstList>
			checkUnsupportedInst(Loop& L, LoopAnalysisManager &AM,
								LoopStandardAnalysisResults &AR)
			{
				auto LN = LoopNest::getLoopNest(L, AR.SE);
				auto innermost = LN->getInnermostLoop();

				auto &MM = AM.getResult<ModelManagerLoopProxy>(L, AR);
				auto *model = MM.getModel();

				InstList unsupported;

				for (auto &BB : innermost->getBlocks()) {
					for (auto &I : *BB) {
						auto *imap = model->isSupported(&I);
						if (!imap) {
							unsupported.emplace_back(&I);
						}
					}
				}
				if (unsupported.size() == 0) {
					return None;
				} else {
					return Optional<InstList>(std::move(unsupported));
				}
			}	

	};

	template <typename VerifyPassTy>
	AnalysisKey VerifyInstAvailabilityPass<VerifyPassTy>::Key;


	/**
	 * @class AGCompatibility
	 * @brief A derived class from VerifyResultBase describing whether the memory access pattern is compatible with the AGs or not
	 * 
	 * @tparam kind Kind of AddressGenerator
	*/
	template<AddressGenerator::Kind AGKind>
	class AGCompatibility : public VerifyResultBase {
		public:
			/**
			 * @brief Construct a new AGCompatibility object
			 */
			AGCompatibility() : ag_kind(AGKind),
				VerifyResultBase(VerificationKind::MemoryAccess) {};
			

			static bool classof(const VerifyResultBase* R) {
				return R->getKind() == VerificationKind::MemoryAccess;
			}

			void print(raw_ostream &OS) const override {}

			/**
			 * @brief Get the Kind object
			 * @return AddressGenerator::Kind 
			 */
			AddressGenerator::Kind geAGtKind() const {
				return ag_kind;
			}
		private:
			AddressGenerator::Kind ag_kind;

	};

	/**
	 * @class AffineAGCompatibility
	 * @brief A derived class from AGCompatibility for affine AGs
	*/
	class AffineAGCompatibility : public AGCompatibility<AddressGenerator::Kind::Affine> {
		public:
			AffineAGCompatibility() : AGCompatibility()
			{};

			/**
			 * @brief a configration of loop control for a dimention
			 */
			typedef struct {
				bool valid;
				int64_t start;
				int64_t inc;
				int64_t end;
			} config_t;

			// for dyn_cast from VerifyResultBase pointer
			static bool classof(const VerifyResultBase* R) {
				if (auto ag_compat = dyn_cast<AGCompatibility<AddressGenerator::Kind::Affine>>(R)) {
					return ag_compat->geAGtKind() == AddressGenerator::Kind::Affine;
				}
				return false;
			}
		private:

	};

	/**
	 * @class VerifyAGCompatiblePass
	 * @brief A function pass to verify the memory access pattern
	 * 
	 * @tparam Kind A Kind of Address Generator
	 * 
	 * @remarks This template needs specilization of @em run_impl method for each address generator type
	*/
	template <AddressGenerator::Kind Kind>
	class VerifyAGCompatiblePass :
		public AnalysisInfoMixin<VerifyAGCompatiblePass<Kind>> {
		public:
			using Result = AGCompatibility<Kind>;

			Result run(Loop &L, LoopAnalysisManager &AM,
						LoopStandardAnalysisResults &AR) {
				switch (Kind) {
					case AddressGenerator::Kind::Affine:
						return run_impl(L, AM, AR);
					default:
						llvm_unreachable("This type of AG is not implemented\n");
				}
			};
		private:
			friend AnalysisInfoMixin<VerifyAGCompatiblePass<Kind>>;
			friend DecoupledVerifyPass;
			static AnalysisKey Key;
			DecoupledCGRA *dec_model;


			/**
			 * @brief actual implementation of running pass
			 * 
			 * @param L Loop
			 * @param AM LoopAnalysisManager
			 * @param AR LoopStandardAnalysisResults
			 * @return Result Derived class of the template AGCompatibility as an verificatio result
			 */
			Result run_impl(Loop &L, LoopAnalysisManager &AM,
						LoopStandardAnalysisResults &AR);

			/**
			 * @brief parse the expression of addition between SCEVs
			 * 
			 * @param SAR parted expression
			 * @param SE a result of ScalarEvolution for the function
			 */
			void parseSCEVAddRecExpr(const SCEVAddRecExpr *SAR,
										ScalarEvolution &SE);
			void parseSCEV(const SCEV *scev, ScalarEvolution &SE, int depth = 0);

			// template<int N, typename T>
			// bool check_all(SmallVector<T*> &list, ScalarEvolution &SE);

	};

	template <AddressGenerator::Kind Kind>
	AnalysisKey VerifyAGCompatiblePass<Kind>::Key;

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