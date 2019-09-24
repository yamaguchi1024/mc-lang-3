#include <iostream>
extern "C" {
  double myfunc(double, double);
}
int main(){
  std::cout << myfunc(10.1,1.1) <<std::endl;
  return 0;
}
