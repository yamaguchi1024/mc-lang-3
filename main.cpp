#include <iostream>

extern "C" {
    int fib(int);
    int myfunc(int, int);
}

int main() {
    std::cout << "Call fib with 10: " << fib(10) << std::endl;
    //std::cout << "Call myfunc with 3 2: " << myfunc(3,2) << std::endl;

    return 0;
}
