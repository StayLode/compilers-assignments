#ifndef LLVM_TRANSFORMS_LoopICM_H
#define LLVM_TRANSFORMS_LoopICM_H

#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/IR/Dominators.h"

namespace llvm{
    class LoopICM: public PassInfoMixin<LoopICM> {
        public: 
            PreservedAnalyses run(Loop &L, LoopAnalysisManager &AM, 
                                  LoopStandardAnalysisResults &AR, LPMUpdater &U);
        };
    } // namespacellvm

#endif // LLVM_TRANSFORMS_LoopICM_H