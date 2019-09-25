#include <iostream>

extern "C" {
    double fib(double);
}

int main() {
    std::cout << "Call fib with 10: " << fib(10)  << std::endl;

    return 0;
}
