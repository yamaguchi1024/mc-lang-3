### ミニキャン言語(MC)講義　第三回事前課題

[第一回事前課題](https://github.com/yamaguchi1024/mc-lang-1/)では、数字と二項演算子のみからなるexpressionをコンパイルし、オブジェクトファイルを得ました。[第二回事前課題](https://github.com/yamaguchi1024/mc-lang-2/)では、関数定義と関数呼び出しを実装し、C++のmain関数とMC言語で生成したオブジェクトファイルをリンクし、ELFファイルを作り実行しました。

最終回である第三回事前課題ではif, then, elseを用いたコントロールフローを実装し、フィボナッチ数列の計算が再帰的に行えるようにします。

#### 3.1 '<'を実装してみよう
コントロールフローを実装するには何かしらの比較演算子を実装する必要があります。今回は'<'を実装してみますが、'>'や'=='等も是非実装してみて下さい。

`codegen.h`の`TODO 3.1`と`mc.cpp`のTODO 3.1に詳細が書いてあり、これを終えると`test1.mc`が正常にコンパイルできるようになります。

#### 3.2 "if", "then", "else"をトークナイズしてみよう
`lexer.h`の`TODO 3.2`に詳細が書いてあります。

#### 3.3 If文のパーシングを実装してみよう
`parser.h`の`TODO 3.3`に詳細が書いてあります。

#### 3.4 "else"ブロックのcodegenを実装してみよう
`codegen.h`の`TODO 3.4`に詳細が書いてあります。3.2から3.4を終えると`test4.mc`が正常にコンパイルできるようになります。

#### 3.5 n番目のフィボナッチ数列を返す関数をMC言語で書いてみよう
`test/test5.mc`に「整数nを引数にとり、n番目のフィボナッチ数列を返す関数fib」を実装して下さい。
main.cppがfib(10)を呼んで標準出力に表示していいるので、実装が終わったら第二回課題と同様にC++とoutput.oをリンクし、ELFファイルを作って実行して下さい。
`test/test5_expected_output.txt`に期待されるLLVM IRの出力があります。
```
$ make
$ ./mc test/test5.mc
$ clang++ main.cpp output.o -o main
$ ./main                     
Call fib with 10: 55
```
余力がある方は、fibよりも複雑な例をMC言語で実装してみて下さい。

課題は以上になります。三週間お疲れ様でした！  
  

### Continuous build status  
  
Build Type | Status  
:-: | :-:  
**Ubuntu 18.04 LTS Debug x64** | [![Ubuntu 18.04 LTS Debug x64](https://github.com/YuqiaoZhang/mc-lang-3/workflows/Ubuntu%2018.04%20LTS%20Debug%20x64/badge.svg)](https://github.com/YuqiaoZhang/mc-lang-3/actions?query=workflow%3A%22Ubuntu+18.04+LTS+Debug+x64%22)  
    
