//===----------------------------------------------------------------------===//
// AST
// ここのセクションでは、AST（構文解析木）の葉を定義している。
// MCコンパイラの根幹であるクラスで、ParserだけではなくCodeGenでも使われているので
// 非常に重要。イメージとしては、Lexerによって次のトークンを取ってきて、それが例えば
// 数値リテラルであったらNumberASTに値を格納し、そのポインタを親ノードが保持する。
// 全てのコードを無事にASTとして表現できたら、後述するcodegenを再帰的に呼び出す事に
// よりオブジェクトファイルを生成する。
//===----------------------------------------------------------------------===//

namespace {
    // ExprAST - `5+2`や`2*10-2`等のexpressionを表すクラス
    class ExprAST {
        public:
            virtual ~ExprAST() = default;
            virtual Value *codegen() = 0;
    };

    // NumberAST - `5`や`2`等の数値リテラルを表すクラス
    class NumberAST : public ExprAST {
        // 実際に数値の値を保持する変数
        double Val;
        int Val_i;
        bool isd;
//ここに型の概念を追加する？
        public:
        NumberAST(double Val,bool isd) : Val(Val), isd(isd) {}
        NumberAST(int Val_i,bool isd) : Val_i(Val_i), isd(isd) {}
        Value *codegen() override;
    };

    // BinaryAST - `+`や`*`等の二項演算子を表すクラス
    class BinaryAST : public ExprAST {
        char Op;
        std::unique_ptr<ExprAST> LHS, RHS;

        public:
        BinaryAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
            : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

        Value *codegen() override;
    };

    // VariableExprAST - 変数の名前を表すクラス
    class VariableExprAST : public ExprAST {
        std::string variableName;
        //std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> variableName;std::unique_ptr<ExprAST> Body;
//ここに型の概念を追加する？
        public:
        //VariableExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> variableName,std::unique_ptr<ExprAST> Body) : variableName(std::move(variableName)), Body(std::move(Body)) {}
        VariableExprAST(const std::string &variableName) : variableName(variableName) {}
        Value *codegen() override;
        const std::string &getName() const { return variableName; }
    };

    // CallExprAST - 関数呼び出しを表すクラス
    class CallExprAST : public ExprAST {
        std::string callee;
        std::vector<std::unique_ptr<ExprAST>> args;

        public:
        CallExprAST(const std::string &callee,
                std::vector<std::unique_ptr<ExprAST>> args)
            : callee(callee), args(std::move(args)) {}

        Value *codegen() override;
    };

    // PrototypeAST - 関数シグネチャーのクラスで、関数の名前と引数の名前を表すクラス
    class PrototypeAST {
        std::string Name;
        std::vector<std::string> args;

        public:
        PrototypeAST(const std::string &Name, std::vector<std::string> args)
            : Name(Name), args(std::move(args)) {}

        Function *codegen();
        const std::string &getFunctionName() const { return Name; }
    };

    // FunctionAST - 関数シグネチャー(PrototypeAST)に加えて関数のbody(C++で言うint foo) {...}の中身)を
    // 表すクラスです。
    class FunctionAST {
        std::unique_ptr<PrototypeAST> proto;
        std::unique_ptr<ExprAST> body;

        public:
        FunctionAST(std::unique_ptr<PrototypeAST> proto,
                std::unique_ptr<ExprAST> body)
            : proto(std::move(proto)), body(std::move(body)) {}

        Function *codegen();
    };

    class IfExprAST : public ExprAST {
        std::unique_ptr<ExprAST> Cond, Then, Else;

        public:
        IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then,
                std::unique_ptr<ExprAST> Else)
            : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

        Value *codegen() override;
    };

    /// ForExprAST - for/inのための式クラス。
    class ForExprAST : public ExprAST {
        std::string VarName;
        std::unique_ptr<ExprAST> Start, End, Step, Body;

        public:
        ForExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Start, std::unique_ptr<ExprAST> End,std::unique_ptr<ExprAST> Step, std::unique_ptr<ExprAST> Body)
        : VarName(VarName), Start(std::move(Start)), End(std::move(End)), Step(std::move(Step)), Body(std::move(Body)) {}

        Value *codegen() override;
        //virtual Value *codegen();
    };

    class VarExprAST : public ExprAST {
        std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
        std::unique_ptr<ExprAST> Body;

        public:
        VarExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames,std::unique_ptr<ExprAST> Body)
        : VarNames(std::move(VarNames)), Body(std::move(Body)) {}
        Value *codegen() override;
};

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Parser
// Parserでは、ParseTopLevelExprから始まり、お互いを再帰的に呼び出すことでAST Tree(構文解析木)
// を構成していく。
//===----------------------------------------------------------------------===//

// CurTokは現在のトークン(tok_number, tok_eof, または')'や'+'などの場合そのascii)が
// 格納されている。
// getNextTokenにより次のトークンを読み、Curtokを更新する。
static int CurTok;
static int getNextToken() { return CurTok = lexer.gettok(); }

// 二項演算子の結合子をmc.cppで定義している。
static std::map<char, int> BinopPrecedence;

// GetTokPrecedence - 二項演算子の結合度を取得
// もし現在のトークンが二項演算子ならその結合度を返し、そうでないなら-1を返す。
static int GetTokPrecedence() {
    if (!isascii(CurTok))
        return -1;

    int tokprec = BinopPrecedence[CurTok];
    if (tokprec <= 0)
        return -1;
    return tokprec;
}

// LogError - エラーを表示しnullptrを返してくれるエラーハンドリング関数
std::unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

// Forward declaration
static std::unique_ptr<ExprAST> ParseExpression();

// 数値リテラルをパースする関数。
static std::unique_ptr<ExprAST> ParseNumberExpr() {
    // NumberASTのValにlexerからnumValを読んできて、セットする。
    bool isd = 1;
    auto Result = llvm::make_unique<NumberAST>(lexer.getNumVal(),isd);
    if(std::isnan(lexer.getNumVal())){
        isd = 0;
        Result = llvm::make_unique<NumberAST>(lexer.getNumVal_i(),isd);
    }
    getNextToken(); // トークンを一個進めて、returnする。
    return std::move(Result);
}

//static std::unique_ptr<ExprAST> ParseNumberDouble(){  
//}
static std::unique_ptr<ExprAST> ParseNumberNeg(){
    getNextToken();
    if(CurTok != tok_number){
        return LogError("expected 'number' after the '-'");
    }else{
        bool isd = 1;
        auto Result = llvm::make_unique<NumberAST>(-lexer.getNumVal(), isd);
        if(std::isnan(lexer.getNumVal()))
            isd = 0;
            Result = llvm::make_unique<NumberAST>(-lexer.getNumVal_i(), isd);
        getNextToken();
        return Result;
    }
}

// TODO 1.5: 括弧を実装してみよう
// 括弧は`'(' ExprAST ')'`の形で表されます。最初の'('を読んだ後、次のトークンは
// ExprAST(NumberAST or BinaryAST)のはずなのでそれをパースし、最後に')'で有ることを
// 確認します。
static std::unique_ptr<ExprAST> ParseParenExpr() {
    // 1. ParseParenExprが呼ばれた時、CurTokは'('のはずなので、括弧の中身を得るために
    //    トークンを進めます。e.g. getNextToken()
    // 2. 現在のトークンはExprASTのはずなので、ParseExpression()を呼んでパースします。
    //    もし返り値がnullptrならエラーなのでParseParenExprでもnullptrを返します。
    // 3. 2で呼んだParseExpressionではトークンが一つ進められて帰ってきているはずなので、
    // 　 CurTokが')'かどうかチェックします。もし')'でなければ、LogErrorを用いてエラーを出して下さい。
    // 4. getNextToken()を呼んでトークンを一つ進め、2で呼んだParseExpressionの返り値を返します。
    //
    // 課題を解く時はこの行を消してここに実装して下さい。
    getNextToken(); // eat (.
    auto V = ParseExpression();
    if (!V)
        return nullptr;

    if (CurTok != ')')
        return LogError("expected ')'");
    getNextToken(); // eat ).
    return V;
}

// TODO 2.2: 識別子をパースしよう
// トークンが識別子の場合は、引数(変数)の参照か関数の呼び出しの為、
// 引数の参照である場合はVariableExprASTを返し、関数呼び出しの場合は
// CallExprASTを返す。
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    // 1. getIdentifierを用いて識別子を取得する。
    std::string IdName = lexer.getIdentifier();

    // 2. トークンを次に進める。
    getNextToken();

    // 3. 次のトークンが'('の場合は関数呼び出し。そうでない場合は、
    // VariableExprASTを識別子を入れてインスタンス化し返す。
    if (CurTok != '(')
        return llvm::make_unique<VariableExprAST>(IdName);

    // 4. '('を読んでトークンを次に進める。
    getNextToken();

    // 5. 関数fooの呼び出しfoo(3,4,5)の()内をパースする。
    // 引数は数値、二項演算子、(親関数で定義された)引数である可能性があるので、
    // ParseExpressionを用いる。
    // 呼び出しが終わるまで(CurTok == ')'になるまで)引数をパースしていき、都度argsにpush_backする。
    // 呼び出しの終わりと引数同士の区切りはCurTokが')'であるか','であるかで判別できることに注意。
    std::vector<std::unique_ptr<ExprAST>> args;
    if (CurTok != ')') {
        while (true) {
            if (auto Arg = ParseExpression())
                args.push_back(std::move(Arg));
            else
                return nullptr;

            if (CurTok == ')')
                break;

            if (CurTok != ',')
                return LogError("Expected ')' or ',' in argument list");
            getNextToken();
        }
    }

    // 6. トークンを次に進める。
    getNextToken();

    // 7. CallExprASTを構成し、返す。
    return llvm::make_unique<CallExprAST>(IdName, std::move(args));
}

  static std::unique_ptr<ExprAST> ParseForExpr() {
    getNextToken();  // eat for
    if (CurTok != tok_identifier)
      return nullptr;
 
      std::string IdName = lexer.getIdentifier();
      getNextToken();  // eat i
 
      if (CurTok != '=')
        return nullptr;
      getNextToken();  // eat =
 
      // for i = 1,10
      auto StartV = ParseExpression();
      if (StartV == 0) return 0;
      if (CurTok != ',')
        return nullptr;
      getNextToken();
 
      auto EndV = ParseExpression();
      if (EndV == 0) return 0;
 
      // for i = 1,10,2 in
      std::unique_ptr<ExprAST> StepV = 0;
      if (CurTok == ',') {
        getNextToken();
        StepV = ParseExpression();
        if (StepV == 0) return 0;
       }
 
      if (CurTok != tok_in)
        return nullptr;
      getNextToken();  // eat in

      auto BodyV = ParseExpression();
      if (BodyV == 0) return 0;
 
      
      //return llvm::make_unique<ForExprAST>(IdName, std::move(StartV),std::move(EndV),std::move(StepV), std::move(BodyV));
      std::unique_ptr<ExprAST> ForExpAST (new ForExprAST(IdName, std::move(StartV), std::move(EndV), std::move(StepV), std::move(BodyV)));
      return ForExpAST;
}


static std::unique_ptr<ExprAST> ParseVarExpr() {
    getNextToken();  // eat the type.

    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

    // At least one variable name is required.
    if (CurTok != tok_identifier)
      return LogError("expected identifier after double");
    while (1) {
      std::string Name = lexer.getIdentifier();
      getNextToken();  // eat identifier.
      // Read the optional initializer.
      std::unique_ptr<ExprAST> Init;

      if (CurTok == '=') {
          getNextToken(); // eat the '='.
          Init = ParseExpression();
          if (!Init) return nullptr;
        }

      VarNames.push_back(std::make_pair(Name, std::move(Init)));

      // End of var list, exit loop.
      if (CurTok != ',') break;
      getNextToken(); // eat the ','.

      if (CurTok != tok_identifier)
         return LogError("expected identifier list after var");
     }

        // At this point, we have to have 'in'.
     if (CurTok != tok_in)
         return LogError("expected 'in' keyword after 'var'");
     getNextToken();  // eat 'in'.

     auto Body = ParseExpression();
     if (!Body)
       return nullptr;

     std::unique_ptr<ExprAST> VarExpAST(new VarExprAST(std::move(VarNames),std::move(Body)));
     return VarExpAST;
}

//doubleの型の変数を定義する
static std::unique_ptr<ExprAST> ParseDoubleVariable(){
   getNextToken();//eat double
   return nullptr;
}
static std::unique_ptr<ExprAST> ParseIfExpr() {
    // TODO 3.3: If文のパーシングを実装してみよう。
    // 1. ParseIfExprに来るということは現在のトークンが"if"なので、
    // トークンを次に進めます。
    getNextToken();
    // 2. ifの次はbranching conditionを表すexpressionがある筈なので、
    // ParseExpressionを呼んでconditionをパースします。
    auto Vcond = ParseExpression();
    // 3. "if x < 4 then .."のような文の場合、今のトークンは"then"である筈なので
    // それをチェックし、トークンを次に進めます。
    if (CurTok != tok_then){
      return nullptr;
    }else{
      getNextToken();
    }
    // 4. "then"ブロックのexpressionをParseExpressionを呼んでパースします。
    auto Vthen = ParseExpression();
    // 5. 3と同様、今のトークンは"else"である筈なのでチェックし、トークンを次に進めます。
    if (CurTok != tok_else){
      return nullptr;
    }else{//elseないとダメなの？
      getNextToken();
    }
    // 6. "else"ブロックのexpressionをParseExpressionを呼んでパースします。
    auto Velse = ParseExpression();
    // 7. IfExprASTを作り、returnします。
    std::unique_ptr<ExprAST> IfExpAST (new IfExprAST(std::move(Vcond),std::move(Vthen),std::move(Velse)));
    return IfExpAST;
    //return llvm::make_unique<IfExprAST>(std::move(Vcond),std::move(Vthen),std::move(Velse));
}

// ParsePrimary - NumberASTか括弧をパースする関数
static std::unique_ptr<ExprAST> ParsePrimary() {
    switch (CurTok) {
        default:
            return LogError("unknown token when expecting an expression");
        case tok_identifier:
            return ParseIdentifierExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
        case tok_if:
            return ParseIfExpr();
        case '-':
            return ParseNumberNeg();
        case tok_for:
            return ParseForExpr();
//        case tok_int:
//            return ParseIntVariable();
//        case tok_double:
//            return ParseDoubleVariable();
        case tok_var:
            return ParseVarExpr();
        //case '.':
          //  return ParseNumberDouble();
    }
}

// TODO 1.6: 二項演算のパーシングを実装してみよう
// このパーサーの中で一番重要と言っても良い、二項演算子のパーシングを実装します。
// LHSに二項演算子の左側が入った状態で呼び出され、LHSとRHSと二項演算子がペアになった
// 状態で返ります。
static std::unique_ptr<ExprAST> ParseBinOpRHS(int CallerPrec,
        std::unique_ptr<ExprAST> LHS) {
    while (true) {
        // 1. 現在の二項演算子の結合度を取得する。 e.g. int tokprec = GetTokPrecedence();
        int tokprec = GetTokPrecedence();

        // 2. もし呼び出し元の演算子(CallerPrec)よりも結合度が低ければ、ここでは演算をせずにLHSを返す。
        // 例えば、「4*2+3」ではここで'2'が返るはずで、「4+2*3」ではここでは返らずに処理が進み、
        // '2*3'がパースされた後に返る。
        if (tokprec < CallerPrec)
            return LHS;

        // 3. 二項演算子をセットする。e.g. int BinOp = CurTok;
        int BinOp = CurTok;

        // 4. 次のトークン(二項演算子の右のexpression)に進む。
        getNextToken();

        // 5. 二項演算子の右のexpressionをパースする。 e.g. auto RHS = ParsePrimary();
        auto RHS = ParsePrimary();
        if (!RHS)
            return nullptr;

        // GetTokPrecedence()を呼んで、もし次のトークンも二項演算子だった場合を考える。
        // もし次の二項演算子の結合度が今の演算子の結合度よりも強かった場合、ParseBinOpRHSを再帰的に
        // 呼んで先に次の二項演算子をパースする。
        int NextPrec = GetTokPrecedence();
        if (tokprec < NextPrec) {
            RHS = ParseBinOpRHS(tokprec + 1, std::move(RHS));
            if (!RHS)
                return nullptr;
        }
        // LHS, RHSをBinaryASTにしてLHSに代入する。
        LHS = llvm::make_unique<BinaryAST>(BinOp, std::move(LHS), std::move(RHS));
    }
}

// TODO 2.3: 関数のシグネチャをパースしよう
static std::unique_ptr<PrototypeAST> ParsePrototype() {
    // 2.2とほぼ同じ。CallExprASTではなくPrototypeASTを返し、
    // 引数同士の区切りが','ではなくgetNextToken()を呼ぶと直ぐに
    // CurTokに次の引数(もしくは')')が入るという違いのみ。
    if (CurTok != tok_identifier)
        return LogErrorP("Expected function name in prototype");

    std::string FnName = lexer.getIdentifier();;
    getNextToken();// eat function name

    if (CurTok != '(')
        return LogErrorP("Expected '(' in prototype");
    getNextToken();//eat (
    std::vector<std::string> ArgNames;
    while (CurTok == tok_identifier) {
        std::string curArg = lexer.getIdentifier();
        ArgNames.push_back(curArg);
        getNextToken();
    }
    if (CurTok != ')')
        return LogErrorP("Expected ')' in prototype");

    getNextToken();

    return llvm::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

static std::unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken();
    auto proto = ParsePrototype();
    if (!proto)
        return nullptr;

    if (auto E = ParseExpression())
        return llvm::make_unique<FunctionAST>(std::move(proto), std::move(E));
    return nullptr;
}

// ExprASTは1. 数値リテラル 2. '('から始まる演算 3. 二項演算子の三通りが考えられる為、
// 最初に1,2を判定して、そうでなければ二項演算子だと思う。
static std::unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParsePrimary();
    if (!LHS)
        return nullptr;

    return ParseBinOpRHS(0, std::move(LHS));
}

// パーサーのトップレベル関数。まだ関数定義は実装しないので、今のmc言語では
// __anon_exprという関数がトップレベルに作られ、その中に全てのASTが入る。
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if (auto E = ParseExpression()) {
        auto Proto = llvm::make_unique<PrototypeAST>("__anon_expr",
                std::vector<std::string>());
        return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

  static std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken(); // eat extern.
    return ParsePrototype();
  }

