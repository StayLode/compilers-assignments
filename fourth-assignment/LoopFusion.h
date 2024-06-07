#ifndef LLVM_TRANSFORMS_LOOPFUSION_H
#define LLVM_TRANSFORMS_LOOPFUSION_H

#include "llvm/IR/PassManager.h"
#include <llvm/IR/Constants.h>
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

namespace llvm{
    class LoopFusion: public PassInfoMixin<LoopFusion> {
        public: 
            PreservedAnalyses run(Function &F,FunctionAnalysisManager &AM);
        };
    } // namespace llvm

#endif // LLVM_TRANSFORMS_LOOPFUSION_H