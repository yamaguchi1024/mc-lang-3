#include <iostream>

extern "C" {
    int myfunc(int, int);
}

int main() {
    std::cout << "Call myfunc with 3 and 10: " << myfunc(3, 10) << std::endl;

    return 0;
}
