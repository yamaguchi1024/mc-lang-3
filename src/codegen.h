//===----------------------------------------------------------------------===//
// Code Generation
// このファイルでは、parser.hによって出来たASTからLLVM IRを生成しています。
// といっても、難しいことをしているわけではなく、IRBuilder(https://llvm.org/doxygen/IRBuilder_8h_source.html)
// のインターフェースを利用して、parser.hで定義した「ソースコードの意味」をIRに落としています。
// 各ファイルの中で一番LLVMの機能を多用しているファイルです。
//===----------------------------------------------------------------------===//

// https://llvm.org/doxygen/LLVMContext_8h_source.html
static LLVMContext Context;
// https://llvm.org/doxygen/classllvm_1_1IRBuilder.html
// LLVM IRを生成するためのインターフェース
static IRBuilder<> Builder(Context);
// https://llvm.org/doxygen/classllvm_1_1Module.html
// このModuleはC++ Moduleとは何の関係もなく、LLVM IRを格納するトップレベルオブジェクトです。
static std::unique_ptr<Module> myModule;
// 変数名とllvm::Valueのマップを保持する
static std::map<std::string, Value *> NamedValues;

// https://llvm.org/doxygen/classllvm_1_1Value.html
// llvm::Valueという、LLVM IRのオブジェクトでありFunctionやModuleなどを構成するクラスを使います
Value *NumberAST::codegen() {
    // 64bit整数型のValueを返す
    return ConstantInt::get(Context, APInt(64, Val, true));
}

Value *LogErrorV(const char *str) {
    LogError(str);
    return nullptr;
}

// TODO 2.4: 引数のcodegenを実装してみよう
Value *VariableExprAST::codegen() {
    // NamedValuesの中にVariableExprAST::NameとマッチするValueがあるかチェックし、
    // あったらそのValueを返す。
  Value *V = NamedValues[variableName];
    if (!V)
      return LogErrorV(("Unknown variable name: " + variableName).c_str());
    return V;
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
    Value *L = LHS->codegen();
    Value *R = RHS->codegen();
    if (!L || !R)
        return nullptr;

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
    case '/':
      // ref
      // https://llvm.org/doxygen/Value_8h_source.html#l00245
      return Builder.CreateUDiv(L, R, "divtmp");
    case ge:
      return Builder.CreateIntCast(
				   Builder.CreateICmpUGE(L, R, "ugetmp"),
				   Type::getInt64Ty(Context),
				   true,
				   "cast_i1_to_i64");
    case le:
      return Builder.CreateIntCast(
				   Builder.CreateICmpULE(L, R, "uletmp"),
				   Type::getInt64Ty(Context),
				   true,
				   "cast_i1_to_i64");
      
    case '>':
      return Builder.CreateIntCast(
				   Builder.CreateICmpUGT(L, R, "ugttmp"),
				   Type::getInt64Ty(Context),
				   true,
				   "cast_i1_to_i64");
      // TODO 3.1: '<'を実装してみよう
      // '<'のcodegenを実装して下さい。その際、以下のIRBuilderのメソッドが使えます。
      // CreateICmp: https://llvm.org/doxygen/classllvm_1_1IRBuilder.html#a103d309fa238e186311cbeb961b5bcf4
      // CreateIntCast: https://llvm.org/doxygen/classllvm_1_1IRBuilder.html#a5bb25de40672dedc0d65e608e4b78e2f
      // CreateICmpの返り値がi1(1bit)なので、CreateIntCastはそれをint64にcastするのに用います。
    case '<':
      return Builder.CreateIntCast(
				   Builder.CreateICmpULT(L, R, "ulttmp"),
				   Type::getInt64Ty(Context),
				   true,
				   "cast_i1_to_i64");
    default:
      return LogErrorV("invalid binary operator");
    }
}

Function *PrototypeAST::codegen() {
    // MC言語では変数の型も関数の返り値もintの為、関数の返り値をInt64にする。
    std::vector<Type *> prototype(args.size(), Type::getInt64Ty(Context));
    FunctionType *FT =
        FunctionType::get(Type::getInt64Ty(Context), prototype, false);
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
    for (auto &Arg : function->args())
        NamedValues[Arg.getName()] = &Arg;

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

Value *IfExprAST::codegen() {
    // if x < 5 then x + 3 else x - 5;
    // というコードが入力だと考える。
    // Cond->codegen()によって"x < 5"のcondition部分がcodegenされ、その返り値(int)が
    // CondVに格納される
    Value *CondV = Cond->codegen();
    if (!CondV)
        return nullptr;

    // CondVはint64なので、int64の0とequalかどうか判定することでCondVをbool型にする。
    CondV = Builder.CreateICmpNE(
            CondV, ConstantInt::get(Context, APInt(64, 0)), "ifcond");
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

    // done then block

    // TODO 3.4: "else"ブロックのcodegenを実装しよう
    // "then"ブロックを参考に、"else"ブロックのcodegenを実装して下さい。

    // beginning of else block
    ParentFunc->getBasicBlockList().push_back(ElseBB);

    Builder.SetInsertPoint(ElseBB);
    Value* ElseV = Else->codegen();
    if (!ElseV) return nullptr;

    // Create Branch to MergeBB
    Builder.CreateBr(MergeBB);
    // update
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
        Builder.CreatePHI(Type::getInt64Ty(Context), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    // TODO 3.4を実装したらコメントアウトを外して下さい。
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
            default:
                HandleTopLevelExpression();
                break;
        }
    }
}
