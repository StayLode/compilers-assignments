//===-- LocalOpts.cpp - Example Transformations --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstrTypes.h"


using namespace llvm;

bool runOnFunction(Function&);
bool runOnBasicBlock(BasicBlock&);

bool BasicSR(Instruction&);
bool AlgebraicId(Instruction&, Instruction::BinaryOps);
bool AdvancedSR(Instruction&, Instruction::BinaryOps);
bool MultiInstOpt(Instruction&, Instruction::BinaryOps);

bool isCloseToPow2(ConstantInt*);
bool isPow2MinusOne(ConstantInt*);
bool isPow2PlusOne(ConstantInt*);

// given the IR of the LLVM program...
// ...iterate over the functions...
PreservedAnalyses LocalOpts::run(Module &M, ModuleAnalysisManager &AM) {
  for (auto Fiter = M.begin(); Fiter != M.end(); ++Fiter)
    if (runOnFunction(*Fiter))
      return PreservedAnalyses::none();
  
  return PreservedAnalyses::all();
}

// ...then, for each function, scroll through the basic blocks...
bool runOnFunction(Function &F) {
  bool Transformed = false;

  for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
    if (runOnBasicBlock(*Iter)) {
      Transformed = true;
    }
  }

  return Transformed;
}

// ...and finally, iterate over the instructions of each basic block.
bool runOnBasicBlock(BasicBlock &B) {
  outs()<<"Optimizing Priority:\n"
          "\t1. ALGEBRAIC IDENTITY\n"
          "\t2. ADVANCED STRENGTH REDUCTION\n"
          "\t3. MULTI-INSTRUCTION OPTIMIZATION\n\n";

  for (auto &i: B){
    outs()<<i<<"\n";
    
    BinaryOperator *bOp = dyn_cast<BinaryOperator>(&i);
    if (not bOp)
      continue;

    auto opCode = bOp->getOpcode();
    
    // the priority of an optimization depends on the number of cycles it introduces
    switch(opCode){
      case Instruction::Add:
        AlgebraicId(i, opCode) || MultiInstOpt(i, opCode);
        break;

      case Instruction::Sub:
        MultiInstOpt(i, opCode);
        break;

      case Instruction::Mul:
        AlgebraicId(i, opCode) || AdvancedSR(i, opCode);
        break;
      
      case Instruction::UDiv:
      case Instruction::SDiv:
        AdvancedSR(i, opCode);
        break;

      default:
        break;
    }
  }

  // Dead Code Elimination
  // this operation is carried out last, in order to avoid problems due to the sequential scrolling of the instructions during the for
  for (auto iter = B.begin(); iter!=B.end();){
    BinaryOperator *bOp = dyn_cast<BinaryOperator>(iter);
    if(bOp and iter->hasNUses(0))
      iter = iter->eraseFromParent(); // any unused instruction is removed
    else
      ++iter;
  }

  return true;
}

/*bool BasicSR(Instruction &i){
  BinaryOperator *mul = dyn_cast<BinaryOperator>(&i);
  if (not mul or mul->getOpcode() != BinaryOperator::Mul){
    return false;
  }
  
  ConstantInt *C;
  Value *Factor;

  C = dyn_cast<ConstantInt>(mul->getOperand(0));
  Factor = mul->getOperand(1);

  if (not C or not C->getValue().isPowerOf2()){
    C = dyn_cast<ConstantInt>(mul->getOperand(1));
    if (not C or not C->getValue().isPowerOf2()){
      return false;
    }

    Factor = mul->getOperand(0);
  }
  
  Constant *shiftConst = ConstantInt::get(C->getType(), C->getValue().exactLogBase2());
  Instruction *new_shift = BinaryOperator::Create(BinaryOperator::Shl, Factor, shiftConst);
  new_shift->insertAfter(mul);
  outs()<<"\t✓ BasicSR Executed\n";
  mul->replaceAllUsesWith(new_shift);

  return true;
}*/

// Algebraic Identity:
//    𝑥 + 0 = 0 + 𝑥 -> 𝑥
//    𝑥 × 1 = 1 × 𝑥 -> 𝑥
bool AlgebraicId(Instruction &i, Instruction::BinaryOps opCode){
  Value *Factor = i.getOperand(0);
  ConstantInt *C = dyn_cast<ConstantInt>(i.getOperand(1));

  // at the end of the checks we'll have the constant operand in C, if it exists, and the remaining in Factor
  if (opCode == Instruction::Add){ // the numeric constant must exist and be 0 -> useless
    if(not C or not C->getValue().isZero()){
      C = dyn_cast<ConstantInt>(i.getOperand(0));
      if(not C or not C->getValue().isZero()){
        return false;
      }
      Factor = i.getOperand(1);
    } 
  }
  else if((opCode == Instruction::Mul)){ // the numeric constant must exist and be 1 -> useless
      if(not C or not C->getValue().isOne()){
      C = dyn_cast<ConstantInt>(i.getOperand(0));
      if(not C or not C->getValue().isOne()){
        return false;
      }
      Factor = i.getOperand(1);
      }
  }

  outs()<<"\t✓ AlgebraicId Executed\n";
  i.replaceAllUsesWith(Factor);
  return true;
}

// functions used to test whether a number+/-1 is a power of two ("similar" power of two)
bool isCloseToPow2(ConstantInt *C){
  if (C->getValue().ugt(2))
	  return isPow2MinusOne(C) or isPow2PlusOne(C);
  return false;
}
bool isPow2MinusOne(ConstantInt *C){
	return (C->getValue()-1).isPowerOf2();
}
bool isPow2PlusOne(ConstantInt *C){
	return (C->getValue()+1).isPowerOf2();
}

// Advanced Strength Reduction:
//    15 × 𝑥 = 𝑥 × 15 -> (𝑥 ≪ 4) – x
//    y = x / 8       -> y = x >> 3
bool AdvancedSR(Instruction &i, Instruction::BinaryOps opCode){
  Value *Factor = i.getOperand(0);
  ConstantInt *C = dyn_cast<ConstantInt>(i.getOperand(1));

  // at the end of the checks we'll have the constant operand in C, if it exists, and the remaining in Factor
  if (opCode == Instruction::Mul){ // the numeric constant must exist and be a power of two or "similar"
    if(not C or not(C->getValue().isPowerOf2() or isCloseToPow2(C))){
        C = dyn_cast<ConstantInt>(i.getOperand(0));
        
        if(not C or not(C->getValue().isPowerOf2() or isCloseToPow2(C)))
          return false;
        Factor = i.getOperand(1);
    }
  }else{
    // in the case of division, the numeric constant must exist and be a power of two
    if(not C or not C->getValue().isPowerOf2())
      return false;
  }
  
  // the new instructions are defined based on the opcode of the current instruction and its constant operand C
  Instruction::BinaryOps shiftType = (opCode == Instruction::Mul) ? BinaryOperator::Shl : BinaryOperator::LShr;
  Instruction::BinaryOps opType = isPow2MinusOne(C) ? BinaryOperator::Add : BinaryOperator::Sub;

  if (C->getValue().isPowerOf2()){
    // if C is an exact power of two, the SR changes the mul with a simple shift
    Constant *shiftConst = ConstantInt::get(C->getType(), C->getValue().exactLogBase2());
    Instruction *new_shift = BinaryOperator::Create(shiftType, Factor, shiftConst);
    new_shift->insertAfter(&i);
    outs()<<"\t✓ AdvancedSR Executed\n";
    i.replaceAllUsesWith(new_shift);
  }
  else{
    // if C is "similar" to an exact power of two, the SR changes the mul with a shift, after adapting C in order to be a power of 2
    APInt adaptedValue = isPow2MinusOne(C) ? (C->getValue()-1) : (C->getValue()+1);

    Constant *shiftConst = ConstantInt::get(C->getType(), adaptedValue.exactLogBase2());
    Instruction *new_shift = BinaryOperator::Create(shiftType, Factor, shiftConst);
    Instruction *new_adapt = BinaryOperator::Create(opType, new_shift, Factor);

    new_shift->insertAfter(&i);
    new_adapt->insertAfter(new_shift);
    outs()<<"\t✓ AdvancedSR Executed\n";
    i.replaceAllUsesWith(new_adapt);
  }

  return true;
}

// Multi-Instruction Optimization:
//    𝑎 = 𝑏 + 1, 𝑐 = 𝑎 − 1 -> 𝑎 = 𝑏 + 1, 𝑐 = 𝑏
bool MultiInstOpt(Instruction &i, Instruction::BinaryOps opCode){
  Value *Factor = i.getOperand(0);
  ConstantInt *C = dyn_cast<ConstantInt>(i.getOperand(1));
  
  // at the end of the checks we'll have the constant operand in C, if it exists, and the remaining in Factor
  if(not C){
      C = dyn_cast<ConstantInt>(i.getOperand(0));
      if(not C)
        return false;

      Factor = i.getOperand(1);
  }
  
  // scroll through all uses of the current statement
  for (auto userIter = i.user_begin(); userIter != i.user_end(); ++userIter) {
    Instruction *user = dyn_cast<Instruction>(*userIter);
    
    // among those found, verify that they are add or sub instructions, respectively if the initial instruction (i) is sub or add
    Instruction::BinaryOps oppositeType = (opCode==Instruction::Add) ? Instruction::Sub : Instruction::Add;
    if (user->getOpcode() != oppositeType) continue;

    ConstantInt *COpp = dyn_cast<ConstantInt>(user->getOperand(1));
    if(not COpp){
        COpp = dyn_cast<ConstantInt>(user->getOperand(0));
        if(not COpp)
          return false;     
    }
    // if there is a constant between the operands, check that it's equal to C
    if(COpp->getValue()!=C->getValue()) continue;
    
    outs()<<"\t✓ MultiInstOpt Executed\n" ;
    user->replaceAllUsesWith(Factor);
  }
  return true;
}