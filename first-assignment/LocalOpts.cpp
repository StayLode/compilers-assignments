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


PreservedAnalyses LocalOpts::run(Module &M, ModuleAnalysisManager &AM) {
  for (auto Fiter = M.begin(); Fiter != M.end(); ++Fiter)
    if (runOnFunction(*Fiter))
      return PreservedAnalyses::none();
  
  return PreservedAnalyses::all();
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
        
        case Instruction::SDiv:
          AdvancedSR(i, opCode);
          break;

        default:
          break;
      }

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

bool AlgebraicId(Instruction &i, Instruction::BinaryOps opCode){
  
  Value *Factor = i.getOperand(0);
  ConstantInt *C = dyn_cast<ConstantInt>(i.getOperand(1));

  if (opCode == Instruction::Add){
    if(not C or not C->getValue().isZero()){
      C = dyn_cast<ConstantInt>(i.getOperand(0));
      if(not C or not C->getValue().isZero()){
        return false;
      }
      Factor = i.getOperand(1);
    } 
  }else if((opCode == Instruction::Mul)){
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


bool isCloseToPow2(ConstantInt *C){
	return (C->getValue()-1).isPowerOf2() or (C->getValue()+1).isPowerOf2();
}
bool isPow2MinusOne(ConstantInt *C){
	return (C->getValue()-1).isPowerOf2();
}


bool AdvancedSR(Instruction &i, Instruction::BinaryOps opCode){
  

  Value *Factor = i.getOperand(0);
  ConstantInt *C = dyn_cast<ConstantInt>(i.getOperand(1));

  if (opCode == Instruction::Mul){
    if(not C or not(C->getValue().isPowerOf2() or isCloseToPow2(C))){
        C = dyn_cast<ConstantInt>(i.getOperand(0));
        
        if(not C or not(C->getValue().isPowerOf2() or isCloseToPow2(C)))
          return false;
        Factor = i.getOperand(1);
    }
  }else{
    if(not C or not C->getValue().isPowerOf2())
      return false;
    
  }
  Instruction::BinaryOps shiftType = (opCode == Instruction::Mul) ? BinaryOperator::Shl : BinaryOperator::LShr;
  Instruction::BinaryOps opType = isPow2MinusOne(C) ? BinaryOperator::Add : BinaryOperator::Sub;
  APInt adaptedValue = isPow2MinusOne(C) ? (C->getValue()-1) : (C->getValue()+1);

  if (C->getValue().isPowerOf2()){
    Constant *shiftConst = ConstantInt::get(C->getType(), C->getValue().exactLogBase2());
    Instruction *new_shift = BinaryOperator::Create(shiftType, Factor, shiftConst);
    new_shift->insertAfter(&i);
    outs()<<"\t✓ AdvancedSR Executed\n";
    i.replaceAllUsesWith(new_shift);
  }
  else{
      Constant *shiftConst = ConstantInt::get(C->getType(), (adaptedValue).exactLogBase2());
      Instruction *new_shift = BinaryOperator::Create(shiftType, Factor, shiftConst);
      Instruction *new_adapt = BinaryOperator::Create(opType, new_shift, Factor);

      new_shift->insertAfter(&i);
      new_adapt->insertAfter(new_shift);
      outs()<<"\t✓ AdvancedSR Executed\n";
      i.replaceAllUsesWith(new_adapt);
  }

  return true;
}

bool MultiInstOpt(Instruction &i, Instruction::BinaryOps opCode){

  //verificare che uno dei due operandi sia una costante e salvarla in C
  Value *Factor = i.getOperand(0);
  ConstantInt *C = dyn_cast<ConstantInt>(i.getOperand(1));

  if(not C){
      C = dyn_cast<ConstantInt>(i.getOperand(0));
      if(not C)
        return false;

      Factor = i.getOperand(1);
  }
  //scorrere gli utilizzi dell'istruzione
  for (auto userIter = i.user_begin(); userIter != i.user_end(); ++userIter) {
    Instruction *user = dyn_cast<Instruction>(*userIter);
    //tra quelli trovati, verificare che siano add o sub, rispettivamente se l'istruzione iniziale è sub o add
    Instruction::BinaryOps oppositeType = (opCode==Instruction::Add) ? BinaryOperator::Sub : BinaryOperator::Add;
    if (user->getOpcode() != oppositeType) continue;

    ConstantInt *COpp = dyn_cast<ConstantInt>(user->getOperand(1));
    if(not COpp){
        COpp = dyn_cast<ConstantInt>(user->getOperand(0));
        if(not COpp)
          return false;     
    }
    //se esiste una costante tra gli operandi, verificare che sia uguale a C
    if(COpp->getValue()!=C->getValue()) continue;
    
    outs()<<"\t✓ MultiInstOpt Executed\n" ;
    user->replaceAllUsesWith(Factor);
  }
  return true;
}