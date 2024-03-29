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

bool BasicSR(Instruction&);
bool AlgebraicId(Instruction&);
bool AdvancedSR(Instruction&);
bool MultiInstOpt(Instruction&);
bool isCloseToPow2(ConstantInt*);
bool isPow2MinusOne(ConstantInt*);

bool runOnBasicBlock(BasicBlock &B) {
    for (auto &i: B){
      // Algebraic Identity takes priority over Basic Strength Reduction
      AlgebraicId(i) || AdvancedSR(i) || BasicSR(i);

      // Advanced Strength Reduction
      
      
      // Multi-Instruction Optimization
      MultiInstOpt(i);
    }
    return true;
  }


bool runOnFunction(Function &F) {
  bool Transformed = false;

  for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
    if (runOnBasicBlock(*Iter)) {
      Transformed = true;
    }
  }

  return Transformed;
}

bool BasicSR(Instruction &i){
  BinaryOperator *mul = dyn_cast<BinaryOperator>(&i);
  if (not mul or mul->getOpcode() != BinaryOperator::Mul)
    return false;
  
  ConstantInt *C;
  Value *Factor;

  C = dyn_cast<ConstantInt>(mul->getOperand(0));
  Factor = mul->getOperand(1);

  if (not C or not C->getValue().isPowerOf2()){
    C = dyn_cast<ConstantInt>(mul->getOperand(1));
    if (not C or not C->getValue().isPowerOf2()){
      outs()<< *mul << ": no operand is a constant integer or power of 2\n";
      return false;
    }

    Factor = mul->getOperand(0);
  }
  
  outs()<<"Converting "<< *mul << " to left shift\n";
  Constant *shiftConst = ConstantInt::get(C->getType(), C->getValue().exactLogBase2());
  Instruction *new_shift = BinaryOperator::Create(BinaryOperator::Shl, Factor, shiftConst);
  new_shift->insertAfter(mul);
  mul->replaceAllUsesWith(new_shift);

  return true;
}

bool AlgebraicId(Instruction &i){
  auto opCode = i.getOpcode();
  if (opCode != Instruction::Add and opCode != Instruction::Mul)
    return false;
  
  Value *Factor = i.getOperand(0);
  ConstantInt *C = dyn_cast<ConstantInt>(i.getOperand(1));

  if (opCode == Instruction::Add){
    if(not C or not C->getValue().isZero()){
      C = dyn_cast<ConstantInt>(i.getOperand(0));
      if(not C or not C->getValue().isZero()){
        outs()<< i << ": no operand is 0\n";
        return false;
      }
      Factor = i.getOperand(1);
    } 
  }else if((opCode == Instruction::Mul)){
      if(not C or not C->getValue().isOne()){
      C = dyn_cast<ConstantInt>(i.getOperand(0));
      if(not C or not C->getValue().isOne()){
        outs()<< i << ": no operand is 1\n";
        return false;
      }
      Factor = i.getOperand(1);
      }
    }
  i.replaceAllUsesWith(Factor);
  return true;
}


bool isCloseToPow2(ConstantInt *C){
	return (C->getValue()-1).isPowerOf2() or (C->getValue()+1).isPowerOf2();
}
bool isPow2MinusOne(ConstantInt *C){
	return (C->getValue()-1).isPowerOf2();
}


bool AdvancedSR(Instruction &i){
  
  auto opCode = i.getOpcode();
  if (opCode != Instruction::Mul and opCode != Instruction::SDiv){
    return false;
  }
  Value *Factor = i.getOperand(0);
  ConstantInt *C = dyn_cast<ConstantInt>(i.getOperand(1));

  if (opCode == Instruction::Mul){
    if(not C or not(C->getValue().isPowerOf2() or isCloseToPow2(C))){
        C = dyn_cast<ConstantInt>(i.getOperand(0));
        
        if(not C or not(C->getValue().isPowerOf2() or isCloseToPow2(C))){
          outs()<< i << ": no operand is a constant integer nor power of 2, or close to it\n";
          return false;
        }
        Factor = i.getOperand(1);
    }
  }else{
    if(not C or not C->getValue().isPowerOf2()){
      outs()<< i << ": no right operand is a constant integer nor power of 2\n";
      return false;
    }
  }
  Instruction::BinaryOps shiftType = (opCode == Instruction::Mul) ? BinaryOperator::Shl : BinaryOperator::LShr;
  Instruction::BinaryOps opType = isPow2MinusOne(C) ? BinaryOperator::Add : BinaryOperator::Sub;
  APInt adaptedValue = isPow2MinusOne(C) ? (C->getValue()-1) : (C->getValue()+1);

  if (isCloseToPow2(C)){
      Constant *shiftConst = ConstantInt::get(C->getType(), (adaptedValue).exactLogBase2());
      Instruction *new_shift = BinaryOperator::Create(shiftType, Factor, shiftConst);
      Instruction *new_adapt = BinaryOperator::Create(opType, new_shift, Factor);

      new_shift->insertAfter(&i);
      new_adapt->insertAfter(new_shift);
      i.replaceAllUsesWith(new_adapt);
  }
  else{
    Constant *shiftConst = ConstantInt::get(C->getType(), C->getValue().exactLogBase2());
    Instruction *new_shift = BinaryOperator::Create(shiftType, Factor, shiftConst);
    new_shift->insertAfter(&i);
    i.replaceAllUsesWith(new_shift);
  }

  return true;
}

bool MultiInstOpt(Instruction &i){
  return true;
}


PreservedAnalyses LocalOpts::run(Module &M, ModuleAnalysisManager &AM) {
  for (auto Fiter = M.begin(); Fiter != M.end(); ++Fiter)
    if (runOnFunction(*Fiter))
      return PreservedAnalyses::none();
  
  return PreservedAnalyses::all();
}
