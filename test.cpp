#include <iostream>

extern "C" {
  double newton(double, double);
}

int main(){
  std::cout << newton(3, 10) << std::endl;
  return 0;
}
