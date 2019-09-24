def myfunc(s,t)
  f = open("func.cpp","w")
  f.puts <<EOS
#include <iostream>
extern "C" {
  double myfunc(double, double);
}
int main(){
  std::cout << myfunc(#{s},#{t}) <<std::endl;
  return 0;
}
EOS
  f.close()
  system("clang++ func.cpp output.o -o func ")
  system("./func")
end
s = ARGV[0].to_f
t = ARGV[1].to_f
puts "mine:" 
myfunc(s, t)
ans = s + t - 5.0
puts "ans:" 
puts ans
