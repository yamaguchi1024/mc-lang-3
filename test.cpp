#include <iostream>

extern "C" {
    int test(int);
}

int main() {
  std::cout << "Call test with 10: " << test(10) << std::endl;
  return 0;
}
