#include <iostream>

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

extern "C" {
  double rui(double, double, double);
}
extern "C" {
  double func(double);
}
extern "C" {
  double dfunc(double);
}
extern "C" {
  double myfunc(double);
}
extern "C" {
  double newton(double, double, double);
}
extern "C" double x1() {
  return 6;
}
extern "C" double x2() {
  return -5;
}
extern "C" double x3() {
  return 1;
}
extern "C" double x4() {
  return 0;
}
extern "C" double x5() {
  return 0;
}
extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

int main(){
  myfunc(10);
  printf("\n");
  newton(1, 1, 100);
  printf("%e\n",rui(0.6,0.6,2));
  printf("%e\n",rui(0.6,0.6,3));
  printf("%e\n",func(1));
  printf("%e\n",func(1.5));
  printf("%e\n",dfunc(1));
  printf("%e\n",dfunc(1.5));
  return 0;
}
