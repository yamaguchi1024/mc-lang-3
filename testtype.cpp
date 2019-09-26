#include <iostream>
using namespace std;

class Foo{
public:
	Foo(){};
};

class Bar{
public:
	Bar(){}
};

int main(void){
	double var = 10;
  int va = 10;
	cout << typeid(var).name() << endl;
	cout << typeid(va).name() << endl;
	Foo foo;
	Foo foo2;
	Bar bar;
	cout << typeid(foo).name() << endl;
	cout << typeid(bar).name() << endl;
	if(typeid(foo) == typeid(bar)){
		cout << "クラスが一致しています" << endl;
	}else{
		cout << "クラスが一致していません" << endl;
	}

	if(typeid(foo) == typeid(foo2)){
		cout << "クラスが一致しています" << endl;
	}else{
		cout << "クラスが一致していません" << endl;
	}
}
