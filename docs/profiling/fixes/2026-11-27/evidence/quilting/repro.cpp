#include "highmap/synthesis.hpp"
#include "highmap/filters.hpp"
#include <cmath>
#include <iostream>
#include <string>
int main(int argc,char**argv) {
 hmap::Array a(glm::ivec2(48,48));
 for(int j=0;j<48;++j)for(int i=0;i<48;++i)a(i,j)=float(((i*37+j*71+i*j*13+101)%257)-128)/128.f;
 if(argc>1&&std::string(argv[1])=="shape") {
  hmap::Array small(glm::ivec2(32,32));
  auto out=hmap::quilting({&a},{12,12},{3,4},0.5f,123,{&small});
  std::cout<<out.vector.size()<<" result cells\n";return 0;
 }
 auto out=hmap::quilting_expand(a,2.f,{12,12},0.5f,123,{},true,false,false,false,0.2f);
 size_t bad=0;for(float v:out.vector)bad+=!std::isfinite(v);
 std::cout<<bad<<" of "<<out.vector.size()<<" nonfinite quilting cells\n";
 hmap::smooth_cpulse(a,0);bad=0;for(float v:a.vector)bad+=!std::isfinite(v);
 std::cout<<bad<<" of "<<a.vector.size()<<" nonfinite direct smooth_cpulse cells\n";
}
