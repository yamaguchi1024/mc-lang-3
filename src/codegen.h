//===----------------------------------------------------------------------===//
// Code Generation
// このファイルでは、parser.hによって出来たASTからLLVM IRを生成しています。
// といっても、難しいことをしているわけではなく、IRBuilder(https://llvm.org/doxygen/IRBuilder_8h_source.html)
// のインターフェースを利用して、parser.hで定義した「ソースコードの意味」をIRに落としています。
// 各ファイルの中で一番LLVMの機能を多用しているファイルです。
//===----------------------------------------------------------------------===//

// https://llvm.org/doxygen/LLVMContext_8h_source.html
static LLVMContext Context;
//static LLVMDoubleTypeInContext Context;
// https://llvm.org/doxygen/classllvm_1_1IRBuilder.html
// LLVM IRを生成するためのインターフェース
static IRBuilder<> Builder(Context);
// https://llvm.org/doxygen/classllvm_1_1Module.html
// このModuleはC++ Moduleとは何の関係もなく、LLVM IRを格納するトップレベルオブジェクトです。
static std::unique_ptr<Module> myModule;
// 変数名とllvm::Valueのマップを保持する
//static std::map<std::string, Value *> NamedValues;
static std::map<std::string, AllocaInst*> NamedValues;
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;



// https://llvm.org/doxygen/classllvm_1_1Value.html
// llvm::Valueという、LLVM IRのオブジェクトでありFunctionやModuleなどを構成するクラスを使います
Value *NumberAST::codegen() {
    // 64bit整数型のValueを返す
    if (isd == 1){
        return ConstantFP::get(Context, llvm::APFloat(Val));
    }else{
        return ConstantInt::get(Context, APInt(64, Val_i, true));
    }
}

Value *LogErrorV(const char *str) {
    LogError(str);
    return nullptr;
}

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
                                          const std::string &VarName) {
  IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                   TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(Type::getDoubleTy(Context), nullptr, VarName);
}

// TODO 2.4: 引数のcodegenを実装してみよう
Value *VariableExprAST::codegen() {
    // NamedValuesの中にVariableExprAST::NameとマッチするValueがあるかチェックし、
    // あったらそのValueを返す。
    Value *V = NamedValues[variableName];
    if (!V)
        return LogErrorV("Unknown variable name");
    return Builder.CreateLoad(V, variableName.c_str());
}

// TODO 2.5: 関数呼び出しのcodegenを実装してみよう
Value *CallExprAST::codegen() {
    // 1. myModule->getFunctionを用いてcalleeがdefineされているかを
    // チェックし、されていればそのポインタを得る。
    Function *CalleeF = myModule->getFunction(callee);
    if (!CalleeF)
        return LogErrorV("Unknown function referenced");

    // 2. llvm::Function::arg_sizeと実際に渡されたargsのサイズを比べ、
    // サイズが間違っていたらエラーを出力。
    if (CalleeF->arg_size() != args.size())
        return LogErrorV("Incorrect # arguments passed");

    std::vector<Value *> argsV;
    // 3. argsをそれぞれcodegenしllvm::Valueにし、argsVにpush_backする。
    for (unsigned i = 0, e = args.size(); i != e; ++i) {
        argsV.push_back(args[i]->codegen());
        if (!argsV.back())
            return nullptr;
    }

    // 4. IRBuilderのCreateCallを呼び出し、Valueをreturnする。
    return Builder.CreateCall(CalleeF, argsV, "calltmp");
}

Value *BinaryAST::codegen() {
    // 二項演算子の両方の引数をllvm::Valueにする。
    if(Op == '='){
        VariableExprAST *LHSE = static_cast<VariableExprAST*>(LHS.get());
        if (!LHSE)
          return nullptr;
        Value *Val = RHS->codegen();
        if (!Val)
          return nullptr;
        Value *Variable = NamedValues[LHSE->getName()];
        if (!Variable)
          return LogErrorV("Unknown variable name");
        Builder.CreateStore(Val, Variable);
        return Val;
     }
    Value *L = LHS->codegen();
    Value *R = RHS->codegen();
    if (!L || !R)
        return nullptr;
    if(L->getType() != R->getType()){
        return LogErrorV("operation type is not matched");
    }
    if(L->getType()->isFloatingPointTy()==0){
        switch (Op) {
            case '+':
                // LLVM IR Builerを使い、この二項演算のIRを作る
                return Builder.CreateAdd(L, R, "addtmp");
                // TODO 1.7: '-'と'*'に対してIRを作ってみよう
                // 上の行とhttps://llvm.org/doxygen/classllvm_1_1IRBuilder.htmlを参考のこと
            case '-':
                return Builder.CreateSub(L, R, "subtmp");
            case '*':
                return Builder.CreateMul(L, R, "multmp");
            case '<':
                return Builder.CreateIntCast(Builder.CreateICmp(llvm::CmpInst::ICMP_SLT, L, R, "slttmp"), llvm::Type::getInt64Ty(Context),true);
            default:
                return LogErrorV("invalid binary operator");
    }
    }else{
        switch (Op) {
            case '+':
                // LLVM IR Builerを使い、この二項演算のIRを作る
                return Builder.CreateFAdd(L, R, "addtmp");
                // TODO 1.7: '-'と'*'に対してIRを作ってみよう
                // 上の行とhttps://llvm.org/doxygen/classllvm_1_1IRBuilder.htmlを参考のこと
            case '-':
                return Builder.CreateFSub(L, R, "subtmp");
            case '*':
                return Builder.CreateFMul(L, R, "multmp");
            case '/':
                return Builder.CreateFDiv(L, R, "divtmp");
            case '<':
                L = Builder.CreateFCmpULT(L, R, "cmptmp");
                return Builder.CreateUIToFP(L, Type::getDoubleTy(Context),"booltmp");
                //return Builder.CreateFPCast(Builder.CreateFCmpULT(L, R, "slttmp"),
                //                            Type::getDoubleTy(Context),
                //                            "booltmp");
    //return Builder.CreateIntCast(Builder.CreateICmp(llvm::CmpInst::ICMP_SLT, L, R, "slttmp"), llvm::Type::getInt64Ty(Context),true);
                
           // case '.':
             //   return
            // TODO 3.1: '<'を実装してみよう
            // '<'のcodegenを実装して下さい。その際、以下のIRBuilderのメソッドが使えます。
            // CreateICmp: https://llvm.org/doxygen/classllvm_1_1IRBuilder.html#a103d309fa238e186311cbeb961b5bcf4
            // llvm::CmpInst::ICMP_SLT: https://llvm.org/doxygen/classllvm_1_1CmpInst.html#a283f9a5d4d843d20c40bb4d3e364bb05
            // CreateIntCast: https://llvm.org/doxygen/classllvm_1_1IRBuilder.html#a5bb25de40672dedc0d65e608e4b78e2f
            // CreateICmpの返り値がi1(1bit)なので、CreateIntCastはそれをint64にcastするのに用います。
            default:
                return LogErrorV("invalid binary operator");
        }
    }
}

Function *PrototypeAST::codegen() {
    // MC言語では変数の型も関数の返り値もintの為、関数の返り値をInt64にする。
    //std::vector<Type *> prototype(args.size(), Type::getInt64Ty(Context));
    std::vector<Type *> prototype(args.size(), Type::getDoubleTy(Context));
   // FunctionType *FT = FunctionType::get(Type::getInt64Ty(Context), prototype, false);
    FunctionType *FT = FunctionType::get(Type::getDoubleTy(Context), prototype, false);
    // https://llvm.org/doxygen/classllvm_1_1Function.html
    // llvm::Functionは関数のIRを表現するクラス
    Function *F =
        Function::Create(FT, Function::ExternalLinkage, Name, myModule.get());

    // 引数の名前を付ける
    unsigned i = 0;
    for (auto &Arg : F->args())
        Arg.setName(args[i++]);

    return F;
}


Function *FunctionAST::codegen() {
    // この関数が既にModuleに登録されているか確認
    Function *function = myModule->getFunction(proto->getFunctionName());
    // 関数名が見つからなかったら、新しくこの関数のIRクラスを作る。
    if (!function)
        function = proto->codegen();
    if (!function)
        return nullptr;

    // エントリーポイントを作る
    BasicBlock *BB = BasicBlock::Create(Context, "entry", function);
    Builder.SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.
    NamedValues.clear();
    for (auto &Arg : function->args()) {
    // Create an alloca for this variable.
        AllocaInst *Alloca = CreateEntryBlockAlloca(function, Arg.getName());

    // Store the initial value into the alloca.
        Builder.CreateStore(&Arg, Alloca);

    // Add arguments to variable symbol table.
        NamedValues[Arg.getName()] = Alloca;
    }
    // 関数のbody(ExprASTから継承されたNumberASTかBinaryAST)をcodegenする
    if (Value *RetVal = body->codegen()) {
        // returnのIRを作る
        Builder.CreateRet(RetVal);

        // https://llvm.org/doxygen/Verifier_8h.html
        // 関数の検証
        verifyFunction(*function);

        return function;
    }

    // もし関数のbodyがnullptrなら、この関数をModuleから消す。
    function->eraseFromParent();
    return nullptr;
}

Value *ForExprAST::codegen() {
  Function *TheFunction = Builder.GetInsertBlock()->getParent();

  // Create an alloca for the variable in the entry block.
  AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);

  // Emit the start code first, without 'variable' in scope.
  Value *StartVal = Start->codegen();
  if (!StartVal)
    return nullptr;

  // Store the value into the alloca.
  Builder.CreateStore(StartVal, Alloca);

  // Make the new basic block for the loop header, inserting after current
  // block.
  BasicBlock *LoopBB = BasicBlock::Create(Context, "loop", TheFunction);

  // Insert an explicit fall through from the current block to the LoopBB.
  Builder.CreateBr(LoopBB);

  // Start insertion in LoopBB.
  Builder.SetInsertPoint(LoopBB);

  // Within the loop, the variable is defined equal to the PHI node.  If it
  // shadows an existing variable, we have to restore it, so save it now.
  AllocaInst *OldVal = NamedValues[VarName];
  NamedValues[VarName] = Alloca;

  // Emit the body of the loop.  This, like any other expr, can change the
  // current BB.  Note that we ignore the value computed by the body, but don't
  // allow an error.
  if (!Body->codegen())
    return nullptr;

  // Emit the step value.
  Value *StepVal = nullptr;
  if (Step) {
    StepVal = Step->codegen();
    if (!StepVal)
      return nullptr;
  } else {
    // If not specified, use 1.0.
    StepVal = ConstantFP::get(Context, APFloat(1.0));
  }

  // Compute the end condition.
  Value *EndCond = End->codegen();
  if (!EndCond)
    return nullptr;

  // Reload, increment, and restore the alloca.  This handles the case where
  // the body of the loop mutates the variable.
  Value *CurVar = Builder.CreateLoad(Alloca, VarName.c_str());
  Value *NextVar = Builder.CreateFAdd(CurVar, StepVal, "nextvar");
  Builder.CreateStore(NextVar, Alloca);

  // Convert condition to a bool by comparing non-equal to 0.0.
  EndCond = Builder.CreateFCmpONE(
      EndCond, ConstantFP::get(Context, APFloat(0.0)), "loopcond");

  // Create the "after loop" block and insert it.
  BasicBlock *AfterBB =
      BasicBlock::Create(Context, "afterloop", TheFunction);

  // Insert the conditional branch into the end of LoopEndBB.
  Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

  // Any new code will be inserted in AfterBB.
  Builder.SetInsertPoint(AfterBB);
//  Value *StartVal = Start->codegen();
//  if (!StartVal) return 0;
//  Function *TheFunction = Builder.GetInsertBlock()->getParent();
//  BasicBlock *PreheaderBB = Builder.GetInsertBlock();
//  BasicBlock *LoopBB = BasicBlock::Create(Context, "loop", TheFunction);
//  Builder.CreateBr(LoopBB);
//  Builder.SetInsertPoint(LoopBB);

//  PHINode *Variable = Builder.CreatePHI(Type::getDoubleTy(Context), 2, VarName);

//  Variable->addIncoming(StartVal, PreheaderBB);
//  Value *OldVal = NamedValues[VarName];

//  NamedValues[VarName] = Variable;
//  if (Body->codegen() == 0)
//    return 0;

//  Value *StepVal;
//  if (Step) {
//    StepVal = Step->codegen();
//    if (!StepVal) return nullptr;
//  } else {
//  StepVal = ConstantFP::get(Context, APFloat(1.0));
//  }
//  Value *NextVar = Builder.CreateFAdd(Variable, StepVal, "nextvar");
//  Value *EndCond = End->codegen();
//  if (!EndCond) return nullptr;

//  EndCond = Builder.CreateFCmpONE(EndCond,ConstantFP::get(Context, APFloat(0.0)),"loopcond");
//  BasicBlock *LoopEndBB = Builder.GetInsertBlock();
//  BasicBlock *AfterBB = BasicBlock::Create(Context, "afterloop", TheFunction);

//  Builder.CreateCondBr(EndCond, LoopBB, AfterBB);
//  Builder.SetInsertPoint(AfterBB);
//  Variable->addIncoming(NextVar, LoopEndBB);

  if (OldVal)
    NamedValues[VarName] = OldVal;
  else
    NamedValues.erase(VarName);

  return Constant::getNullValue(Type::getDoubleTy(Context));
}


Value *VarExprAST::codegen() {
  std::vector<AllocaInst *> OldBindings;
  Function *TheFunction = Builder.GetInsertBlock()->getParent();
  // Register all variables and emit their initializer.
  for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
      const std::string &VarName = VarNames[i].first;
      ExprAST *Init = VarNames[i].second.get();
       // Emit the initializer before adding the variable to scope, this prevents
     // the initializer from referencing the variable itself, and permits stuff
     // like this:
     //  var a = 1 in
     //    var a = a in ...   # refers to outer 'a'.
      Value *InitVal;
      if (Init) {
         InitVal = Init->codegen();
         if (!InitVal)
            return nullptr;
      } else { // If not specified, use 0.0.
         InitVal = ConstantFP::get(Context, APFloat(0.0));
      }
      AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
      Builder.CreateStore(InitVal, Alloca);
      // Remember the old variable binding so that we can restore the binding when
      // we unrecurse.
      OldBindings.push_back(NamedValues[VarName]);
      // Remember this binding.
      NamedValues[VarName] = Alloca;
    }
     // Codegen the body, now that all vars are in scope.
    Value *BodyVal = Body->codegen();
    if (!BodyVal)
        return nullptr;
    // Pop all our variables from scope.
    for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
        NamedValues[VarNames[i].first] = OldBindings[i];
  // Return the body computation.
    return BodyVal;
}
Value *IfExprAST::codegen() {
    // if x < 5 then x + 3 else x - 5;
    // というコードが入力だと考える。
    // Cond->codegen()によって"x < 5"のcondition部分がcodegenされ、その返り値(int)が
    // CondVに格納される。
    Value *CondV = Cond->codegen();
    if (!CondV)
        return nullptr;

    // CondVはint64でtrueなら0以外、falseなら0が入っているため、CreateICmpNEを用いて
    // CondVが0(false)とnot-equalかどうか判定し、CondVをbool型にする。
    CondV = Builder.CreateFCmpONE(CondV,ConstantFP::get(Context, APFloat(0.0)),"ifcond");
    //CondV = Builder.CreateICmpNE(
            //CondV, ConstantInt::get(Context, APInt(64,0)), "ifcond");
    // if文を呼んでいる関数の名前
    Function *ParentFunc = Builder.GetInsertBlock()->getParent();

    // "thenだった場合"と"elseだった場合"のブロックを作り、ラベルを付ける。
    // "ifcont"はif文が"then"と"else"の処理の後、二つのコントロールフローを
    // マージするブロック。
    BasicBlock *ThenBB =
        BasicBlock::Create(Context, "then", ParentFunc);
    BasicBlock *ElseBB = BasicBlock::Create(Context, "else");
    BasicBlock *MergeBB = BasicBlock::Create(Context, "ifcont");
    // condition, trueだった場合のブロック、falseだった場合のブロックを登録する。
    // https://llvm.org/doxygen/classllvm_1_1IRBuilder.html#a3393497feaca1880ab3168ee3db1d7a4
    Builder.CreateCondBr(CondV, ThenBB, ElseBB);

    // "then"のブロックを作り、その内容(expression)をcodegenする。
    Builder.SetInsertPoint(ThenBB);
    Value *ThenV = Then->codegen();
    if (!ThenV)
        return nullptr;
    // "then"のブロックから出る時は"ifcont"ブロックに飛ぶ。
    Builder.CreateBr(MergeBB);
    // ThenBBをアップデートする。
    ThenBB = Builder.GetInsertBlock();

    // TODO 3.4: "else"ブロックのcodegenを実装しよう
    // "then"ブロックを参考に、"else"ブロックのcodegenを実装して下さい。
    // 注意: 20行下のコメントアウトを外して下さい。
    ParentFunc->getBasicBlockList().push_back(ElseBB);
    
    Builder.SetInsertPoint(ElseBB);
    Value *ElseV = Else->codegen();
    if (!ElseV)
        return nullptr;
    Builder.CreateBr(MergeBB);
    ElseBB = Builder.GetInsertBlock();
    
    // "ifcont"ブロックのcodegen
    ParentFunc->getBasicBlockList().push_back(MergeBB);
    Builder.SetInsertPoint(MergeBB);
    // https://llvm.org/docs/LangRef.html#phi-instruction
    // PHINodeは、"then"ブロックのValueか"else"ブロックのValue
    // どちらをifブロック全体の返り値にするかを実行時に選択します。
    // もちろん、"then"ブロックに入るconditionなら前者が選ばれ、そうでなければ後者な訳です。
    // LLVM IRはSSAという"全ての変数が一度だけassignされる"規約があるため、
    // 値を上書きすることが出来ません。従って、このように実行時にコントロールフローの
    // 値を選択する機能が必要です。
    PHINode *PN =
        Builder.CreatePHI(Type::getDoubleTy(Context), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    // TODO 3.4:を実装したらコメントアウトを外して下さい。
     PN->addIncoming(ElseV, ElseBB);
    return PN;
}

//===----------------------------------------------------------------------===//
// MC コンパイラエントリーポイント
// mc.cppでMainLoop()が呼ばれます。MainLoopは各top level expressionに対して
// HandleTopLevelExpressionを呼び、その中でASTを作り再帰的にcodegenをしています。
//===----------------------------------------------------------------------===//

static std::string streamstr;
static llvm::raw_string_ostream stream(streamstr);
static void HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        if (auto *FnIR = FnAST->codegen()) {
            FnIR->print(stream);
        }
    } else {
        getNextToken();
    }
}

static void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      fprintf(stderr, "Read extern: ");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      FunctionProtos[ProtoAST->getFunctionName()] = std::move(ProtoAST);
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

// その名の通りtop level expressionをcodegenします。例えば、「2+1;3+3;」というファイルが
// 入力だった場合、この関数は最初の「2+1」をcodegenして返ります。(そしてMainLoopからまだ呼び出されます)
static void HandleTopLevelExpression() {
    // ここでテキストファイルを全てASTにします。
    if (auto FnAST = ParseTopLevelExpr()) {
        // できたASTをcodegenします。
        if (auto *FnIR = FnAST->codegen()) {
            streamstr = "";
            FnIR->print(stream);
        }
    } else {
        // エラー
        getNextToken();
    }
}

static void MainLoop() {
    myModule = llvm::make_unique<Module>("my cool jit", Context);
    while (true) {
        switch (CurTok) {
            case tok_eof:
                // ここで最終的なLLVM IRをプリントしています。
                fprintf(stderr, "%s", stream.str().c_str());
                return;
            case tok_def:
                HandleDefinition();
                break;
            case ';': // ';'で始まった場合、無視します
                getNextToken();
                break;
            case tok_extern:
                HandleExtern();
            default:
                HandleTopLevelExpression();
                break;
        }
    }
}
