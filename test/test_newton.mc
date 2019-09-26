extern printd(x);
extern x1();
extern x2();
extern x3();
extern x4();
extern x5();

def rui(x0 x iter)
    if (iter < 2) then
      x
    else
      rui(x0, x0*x, iter-1);

def func(x)
  x5() * rui(x, x, 5) + x4() * rui(x, x, 4) + x3() * rui(x, x, 3) + x2() * rui(x, x, 2) + x1() * x;
  
def dfunc(x)
  5 * x5() * rui(x, x, 4) + 4 * x4() * rui(x, x, 3) + 3 * x3() * rui(x, x, 2) + 2 * x2() * x;

def myfunc(x)
  if (x < 3) then
    printd(x)
  else
    var a=3,b=2 in
      for i=1,i<x in
        printd(x-a*b); 

def newton(x iter max)
  if (iter < max) then
    newton(x-func(x)/dfunc(x), iter+1, max)
  else
    printd(x);
