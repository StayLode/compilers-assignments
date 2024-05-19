#include "llvm/Transforms/Utils/LoopICM.h"
#include <set>

using namespace llvm;

bool isOperandLI(Value *Val, Loop &L);

// function to evaluate if an instruction is loop invariant
bool isInstructionLI(Instruction &Inst, Loop &L) {
	// a PHI node merges multiple reaching definitions
    if (isa<PHINode>(Inst))
        return false;

	// an instruction is loop invariant if its operands are loop invariant
    for (auto op_iter = Inst.op_begin(); op_iter != Inst.op_end(); op_iter++) {
        Value *Val = *op_iter;
		if (!isOperandLI(Val, L))
			return false;
    }

    return true;
}

// function to evaluate if an operand is loop invariant
bool isOperandLI(Value *Val, Loop &L) {
	// costant values and function arguments are by definition loop invariant
    if (isa<ConstantInt>(Val) or isa<Argument>(Val))
        return true;

	// find the reaching definition for Val
    Instruction *I = dyn_cast<Instruction>(Val); 
    // check if there is exactly one reaching definition
	if (!I)
        return false;

	// a PHI node merges multiple reaching definitions
	if(isa<PHINode>(Val))
		return false;

	// the reaching definition is outside the loop
	if (!L.contains(I))
        return true;
	
	// check if the reaching definition, inside the loop, is loop invariant
    return isInstructionLI(*I, L);
}


bool dominatesAllExits(Instruction &Inst, std::set<BasicBlock *> LoopExitBB, DominatorTree &DomTree) {
	
	for (BasicBlock *BB : LoopExitBB) 
		if (!DomTree.dominates(Inst.getParent(), BB))
			return false;

	return true;
}

bool isLoopDead(Instruction &Inst, Loop &L) {
	for (auto user_iter = Inst.user_begin(); user_iter != Inst.user_end(); user_iter++) {
		Instruction *I = dyn_cast<Instruction>(*user_iter);
		if (!L.contains(I))
			return false;
	}
	return true;
}

PreservedAnalyses LoopICM::run(Loop &L, LoopAnalysisManager &AM, LoopStandardAnalysisResults &AR, LPMUpdater &LU) {

	if (!L.isLoopSimplifyForm()) {
		outs() << "\nThe loop is not in Simplify Form.\n";
		return PreservedAnalyses::all();
	}
	outs() << "\nThe loop is in Simplify Form, let's go!\n";
	
	// set of basic blocks outside the loop
	std::set<BasicBlock *> LoopExitBB;
	// set of instructions that will be moved in the preheader
	std::set<Instruction *> preHeaderInstr;

	// last preheader block instruction
	Instruction *FinalInst = L.getLoopPreheader()->getTerminator();


	outs() << "********** LOOP **********\n";
	for (auto block_iter = L.block_begin(); block_iter != L.block_end(); ++block_iter) {
		BasicBlock *BB = *block_iter;

		// find the Exit Basic Blocks
		for (BasicBlock *Succ : successors(BB))
			if (!L.contains(Succ))
				LoopExitBB.insert(BB);

		// find the Loop Invariant instructions
		for (auto instr_iter = BB->begin(); instr_iter != BB->end(); instr_iter++) {
			Instruction &Inst = *instr_iter;

			// check if the instruction is loop invariant
			if (isInstructionLI(Inst, L)){
				outs() << "Loop Invariant Instruction: ";
				Inst.print(outs());

				// check if the Instruction Inst is eligible for the Code Motion:
				// check the dominance on all exits or the absence of uses after the loop
				if (dominatesAllExits(Inst, LoopExitBB, AR.DT) || isLoopDead(Inst, L)){
					preHeaderInstr.insert(&Inst);
					outs() << "\t-> to be moved";
				}

        		outs() << "\n";
      		}	 
    	}
	}

	// move instructions in the preheader, outside the loop
	for (Instruction *Inst : preHeaderInstr) {
		Inst->removeFromParent(); 				// unlink Inst from its basic block, but does not delete it, in order to move it elsewhere
		Inst->insertBefore(FinalInst); 
	}

	return PreservedAnalyses::all();
}