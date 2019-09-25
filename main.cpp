#include <iostream>

extern "C" {
    double fib(double);
}

int main() {
    std::cout << "Call fib with 10: " << fib(2)  << std::endl;

    return 0;
}
