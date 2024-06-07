#include "llvm/Transforms/Utils/LoopFusion.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"

#include <set>

using namespace llvm;

/*
The entry block will be the guard if exists, otherwise the preheader
*/
BasicBlock* getEntryBlock(Loop* L){
    if(L->isGuarded())
        //! Get the Loop guard, the block before the preheader
        return L->getLoopPreheader()->getUniquePredecessor();
    else
        return L->getLoopPreheader();
}

/*
Two loops are control flow equivalent (CFE) if L0 dominates L1 and L1 postdominates L0
*/
bool areControlFlowEquivalent(DominatorTree& DT, PostDominatorTree& PT, Loop* L1, Loop* L2) {
    BasicBlock* B1 = getEntryBlock(L1);
    BasicBlock* B2 = getEntryBlock(L2);

    return DT.dominates(B1, B2) && PT.dominates(B2, B1);
}

/*
Two loops are adjacent if there are no BB between the exit of L1 and the beginning of L2

- If loops are guarded, the non-loop guard-branch successor of L1 must be the entry block of L2
- If loops are not guarded, the L1 exit block must be the entry block of L2
*/
bool areAdjacentBlocks(Loop* L1, Loop* L2){

    //Get the entry block of L2
    BasicBlock* BB2 = getEntryBlock(L2);
    
    if(L1->isGuarded()){
        BasicBlock* BB1Guard = L1->getLoopPreheader()->getUniquePredecessor();
        BranchInst* BB1Branch = dyn_cast<BranchInst>(BB1Guard->getTerminator());

        return BB1Branch->getSuccessor(1) == BB2 || BB1Branch->getSuccessor(0) == BB2;
    }
    else
        return L1->getExitBlock() == BB2;
}

/*
Return true if L1 and L2 have the same trip count:
 -  "trip count" is the number of times the header of the loop will execute 
    if an exit is taken after the specified number of backedges have been taken.
*/
bool haveSameTripCount(ScalarEvolution& SE, Loop* L1, Loop* L2){
    //Get trip count from the number of times the backedge executes before the given exit would be taken
    auto trip1 = SE.getTripCountFromExitCount(SE.getExitCount(L1, L1->getExitingBlock()));
    auto trip2 = SE.getTripCountFromExitCount(SE.getExitCount(L2, L2->getExitingBlock()));

    return trip1 == trip2;
}

/*
Function used to get the (entry) body of Loop L
*/
BasicBlock* getBody(Loop* L) {
    return (dyn_cast<BranchInst>(L->getHeader()->getTerminator()))->getSuccessor(0); 
}

bool haveNegDistanceDependence( DependenceInfo &DI,ScalarEvolution& SE, Loop* L1, Loop* L2){
    /*BasicBlock* Body1 = getBody(L1);
    BasicBlock* Body2 = getBody(L2);
    for (auto instr_iter = Body1->begin(); instr_iter != Body1->end(); instr_iter++) {
		Instruction &Inst1 = *instr_iter;
        
        for (auto instr_iter2 = Body2->begin(); instr_iter2 != Body2->end(); instr_iter2++) {
		    Instruction &Inst2 = *instr_iter2;
                    
            Value *Ptr1 = getLoadStorePointerOperand(&Inst1);
            Value *Ptr2 = getLoadStorePointerOperand(&Inst2);
            if (!Ptr1 || !Ptr2)
                continue;
                    
            const SCEV *SCEVPtr1 = SE.getSCEVAtScope(Ptr1, L1);
            const SCEV *SCEVPtr2 = SE.getSCEVAtScope(Ptr2, L2);

            bool IsAlwaysGE = SE.isKnownPredicate(ICmpInst::ICMP_SGE,SCEVPtr1,SCEVPtr2);
            //outs()<<"L1 -> " <<Ptr0<<"\nL2 -> " <<Ptr1<<"\n"<<IsAlwaysGE<<"\n";
            if (!IsAlwaysGE){
                return true;
            }
        }
    }
    return false;*/
    return false;
}

/*
Replace the uses of the L2 induction variable with the L1 induction variable
*/
void replaceUsesInductionVariable(Loop* L1, Loop* L2) {
    PHINode* ivL1 = L1->getCanonicalInductionVariable();
    PHINode* ivL2 = L2->getCanonicalInductionVariable();
    ivL2->replaceAllUsesWith(ivL1);
    ivL2->eraseFromParent();
  }

/*
Function used to fuse two loops
*/
void fuseLoops(Loop* L1, Loop* L2, LoopInfo& LI){
    replaceUsesInductionVariable(L1,L2);

    //Retrieve Loop2 Body
    BasicBlock* Body2 = getBody(L2);
    
    //L1 Blocks
    BasicBlock* Exit1 = getEntryBlock(L2);
    BasicBlock* Latch1 = L1->getLoopLatch();
    BasicBlock* Header1 = L1->getHeader();

    //L2 Blocks
    BasicBlock* Exit2 = L2->getExitBlock();
    BasicBlock* Header2 = L2->getHeader();
    BasicBlock* Latch2 = L2->getLoopLatch();

    // L1 Header links to L2 Exit
    Header1->getTerminator()->replaceSuccessorWith(Exit1, Exit2);

    // All the predecessors of L1 latch must be linked to L2 body
    // -> L1 Body links to L2 Body  
    for (BasicBlock* pred : predecessors(Latch1))
        pred->getTerminator()->replaceSuccessorWith(Latch1, Body2);  
    
    
    // All the predecessors of L2 latch must be linked to L1 latch 
    // -> L2 Body links to L1 Latch
    for (BasicBlock* pred : predecessors(Latch2))
        pred->getTerminator()->replaceSuccessorWith(Latch2, Latch1);
    

    // L2 Header links to L2 Latch
    Header2->getTerminator()->replaceSuccessorWith(Body2, Latch2);

    for (auto& BB : L2->blocks())
        if(BB != Header2 && BB != Latch2)
            L1->addBasicBlockToLoop(BB,LI);

    LI.erase(L2); 
}

void printDetails(ScalarEvolution &SE, DominatorTree &DT, PostDominatorTree &PDT, DependenceInfo &DI, Loop* firstLoop, Loop* secondLoop){
    if(areAdjacentBlocks(firstLoop, secondLoop)) 
        outs()<<"Are adiacent\n";
    else
        outs()<<"Are NOT adiacent\n";

    if (haveSameTripCount(SE, firstLoop, secondLoop))
        outs()<<"Have same trip count\n";
    else
        outs()<<"Have NOT same trip count\n";

    if(areControlFlowEquivalent(DT, PDT, firstLoop, secondLoop)) 
        outs()<<"Are control flow equivalent\n";
    else 
        outs()<<"Are NOT control flow equivalent\n";

    if(!haveNegDistanceDependence(DI, SE, firstLoop, secondLoop)) 
        outs()<<"Have NOT negative distance depencedencies\n";
    else 
        outs()<<"Have negative distance depencedencies\n";


}

/*
Function used to check if a loop can be fused, controlling its fundamental blocks and if it's in simplified form
*/
bool isEligibleForFusion(Loop* L){
    if (!L->getLoopPreheader() or !L->getHeader() or !L->getLoopLatch() or !L->getExitingBlock() or !L->getExitBlock()){
        outs()<<"Loop does NOT have necessary information!";
        return false;
    }   
    if (!L->isLoopSimplifyForm()) {
        outs()<<"Loop is NOT in simplified form!";
        return false;
    }

    return true;
}

PreservedAnalyses LoopFusion::run(Function &F,FunctionAnalysisManager &AM) {

    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
    DependenceInfo &DI = AM.getResult<DependenceAnalysis>(F);

    Loop* firstLoop = nullptr; 

    std::deque <Loop *> loops;

    //Invert loops order in LoopInfo
    for(auto *l: LI)
        loops.push_front(l);
    
    for (auto *TopLevelLoop: loops){
        /* for (Loop *L : depth_first(TopLevelLoop))
        // We only handle inner-most loops.
            if (L->isInnermost())
            Worklist.push_back(L); */
        if (!isEligibleForFusion(TopLevelLoop) or !firstLoop){
            firstLoop = TopLevelLoop;
            continue;
        }

        outs()<<"L1:\n";
        firstLoop->print(outs());
        outs()<<"L2:\n";
        TopLevelLoop->print(outs());
        printDetails(SE, DT, PDT, DI, firstLoop, TopLevelLoop);
        if (areAdjacentBlocks(firstLoop, TopLevelLoop)
            && haveSameTripCount(SE, firstLoop, TopLevelLoop)
            && areControlFlowEquivalent(DT, PDT, firstLoop, TopLevelLoop)
            && !haveNegDistanceDependence(DI, SE, firstLoop, TopLevelLoop)){
            
            fuseLoops(firstLoop, TopLevelLoop, LI);
            EliminateUnreachableBlocks(F);
        }
        else{
            firstLoop = TopLevelLoop;
        }
        
        outs()<<"_______________________\n";

    }
    
	return PreservedAnalyses::all();
}