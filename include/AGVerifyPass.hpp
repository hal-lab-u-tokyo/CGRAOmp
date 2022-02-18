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
*    File:          /include/AGVerifyPass.hpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  15-02-2022 13:23:43
*    Last Modified: 18-02-2022 15:50:18
*/
#ifndef AGVerifyPass_H
#define AGVerifyPass_H

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Support/Debug.h"

#include "VerifyPass.hpp"


#include <map>

using namespace llvm;


namespace CGRAOmp {
	
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
			AffineAGCompatibility() : AGCompatibility(), nested_level(0)
			{};

			/**
			 * @brief a configration of loop control for a dimention
			 */
			typedef struct {
				int64_t start;
				int64_t step;
				int64_t count;
			} DimEntry_t;

			typedef struct {
				bool valid;
				SmallVector<DimEntry_t> config;
			} ConfigTy;
			

			// for dyn_cast from VerifyResultBase pointer
			static bool classof(const VerifyResultBase* R) {
				if (auto ag_compat = dyn_cast<AGCompatibility<AddressGenerator::Kind::Affine>>(R)) {
					return ag_compat->geAGtKind() == AddressGenerator::Kind::Affine;
				}
				return false;
			}

			void print(raw_ostream &OS) const override;
			
			void add(Instruction* I, ConfigTy C) {
				if (!C.valid) {
					invalid_list.emplace_back(I);
					setVio();
				}
				config[I] = C;
			};

		private:
			DenseMap<Instruction*, ConfigTy> config;
			SmallVector<Instruction*> invalid_list;
			int nested_level;
	};

	/**
	 * @class VerifyAGCompatiblePass
	 * @brief A function pass to verify the memory access pattern
	 * 
	 * @tparam Kind A Kind of Address Generator
	 * 
	 * @remarks This template needs specilization of @em run method for each address generator type
	*/
	template <AddressGenerator::Kind Kind>
	class VerifyAGCompatiblePass :
		public AnalysisInfoMixin<VerifyAGCompatiblePass<Kind>> {
		public:
			using Result = AGCompatibility<Kind>;

			/**
			 * @brief actual implementation of running pass
			 * 
			 * @param L Loop
			 * @param AM LoopAnalysisManager
			 * @param AR LoopStandardAnalysisResults
			 * @return Result Derived class of the template AGCompatibility as an verificatio result
			 */
			Result run(Loop &L, LoopAnalysisManager &AM,
						LoopStandardAnalysisResults &AR);
		private:
			friend AnalysisInfoMixin<VerifyAGCompatiblePass<Kind>>;
			static AnalysisKey Key;

	};

	template <AddressGenerator::Kind Kind>
	AnalysisKey VerifyAGCompatiblePass<Kind>::Key;


	// /**
	//  * @brief parse the expression of addition between SCEVs
	//  * 
	//  * @param SAR parted expression
	//  * @param SE a result of ScalarEvolution for the function
	//  */
	// void parseSCEVAddRecExpr(const SCEVAddRecExpr *SAR,
	// 							ScalarEvolution &SE);
	// void parseSCEV(const SCEV *scev, ScalarEvolution &SE, int depth = 0);

	void verifySCEVAsAffineAG(const SCEV* S, ScalarEvolution &SE, AffineAGCompatibility::ConfigTy& C);
	

}

#endif //AGVerifyPass_H