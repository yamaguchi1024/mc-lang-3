def x0()
  6;

def x1()
  -5;

def x2()
  1;

def rui(x0 x iter)
    if (iter < 2) then
      x
    else
      rui(x0, x0*x, iter-1);

def func(x)
  x2() * rui(x, x, 2) + x1() * x + x0();
  
def dfunc(x)
  2 * x2() * x + x1();

def newton(x iter)
  if (0 < iter) then
    newton(x-func(x)/dfunc(x), iter-1)
  else
    x;
