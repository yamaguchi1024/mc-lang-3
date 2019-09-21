#include <iostream>

extern "C" {
    int fib(int);
}

int main() {
  std::cout << "Call fib with 10: " << fib(10) << std::endl;
  return 0;
}
