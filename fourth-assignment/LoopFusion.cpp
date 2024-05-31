#include "llvm/Transforms/Utils/LoopFusion.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"

#include <set>

using namespace llvm;

BasicBlock* getEntryBlock(Loop* L){
    if(L->isGuarded())
        //Get the Loop guard
        return L->getLoopPreheader()->getUniquePredecessor();
    else
        return L->getLoopPreheader();
}

bool areControlFlowEquivalent(DominatorTree& DT, PostDominatorTree& PT, Loop* L1, Loop* L2) {
    BasicBlock* B1 = getEntryBlock(L1);
    BasicBlock* B2 = getEntryBlock(L2);

    return DT.dominates(B1, B2) && PT.dominates(B2, B1);
}


bool areAdjacentBlocks(Loop* L1, Loop* L2){
    //Get the entry block of L2
    BasicBlock* BB2 = getEntryBlock(L2);
    
    //If loops are guarder the non-loop guard-branch successor of L1 must be the L2 entry block
    if(L1->isGuarded()){
        BasicBlock* BB1Guard = L1->getLoopPreheader()->getUniquePredecessor();
        BranchInst* BB1Branch = dyn_cast<BranchInst>(BB1Guard->getTerminator());

        return BB1Branch->getSuccessor(1) == BB2 || BB1Branch->getSuccessor(0) == BB2;
    }
    else
        return L1->getExitBlock() == BB2;
}


bool haveSameTripCount(ScalarEvolution& SE, Loop* L1, Loop* L2){
    //Get trip count from the number of times the backedge executes before the given exit would be taken
    auto trip1 = SE.getTripCountFromExitCount(SE.getExitCount(L1, L1->getExitingBlock()));
    auto trip2 = SE.getTripCountFromExitCount(SE.getExitCount(L2, L2->getExitingBlock()));

    return trip1 == trip2;
}

PreservedAnalyses LoopFusion::run(Function &F,FunctionAnalysisManager &AM) {

    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
    Loop* firstLoop = nullptr; 

    std::deque <Loop *> loops;

    for(auto *l: LI)
        loops.push_front(l);

    for (auto *TopLevelLoop: loops){
        /* for (Loop *L : depth_first(TopLevelLoop))
        // We only handle inner-most loops.
            if (L->isInnermost())
            Worklist.push_back(L); */

        if (firstLoop != nullptr){
            outs()<<"L1:\n";
            firstLoop->print(outs());
            outs()<<"L2:\n";
            TopLevelLoop->print(outs());

            if(areAdjacentBlocks(firstLoop, TopLevelLoop)) 
                outs()<<"Are adiacent\n";
            else 
                outs()<<"Are NOT adiacent\n";

            if (haveSameTripCount(SE, firstLoop, TopLevelLoop))
                outs()<<"Have same trip count\n";
            else
                outs()<<"Have NOT same trip count\n";

            if(areControlFlowEquivalent(DT, PDT, firstLoop, TopLevelLoop)) 
                outs()<<"Are control flow equivalent\n";
            else 
                outs()<<"Are NOT control flow equivalent\n";
        }
        outs()<<"\n";
        firstLoop = TopLevelLoop;
    }
    
	return PreservedAnalyses::all();
}