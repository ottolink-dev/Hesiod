#include "highmap/range.hpp"
#include <fstream>
#include <cmath>
#include <iostream>
int main(int argc,char**argv){
 using namespace hmap;std::ofstream f(argv[1],std::ios::binary);
 Array input(glm::ivec2(32,24)),bounds(input.shape);
 for(size_t i=0;i<input.vector.size();++i){input.vector[i]=float(int(i%101)-50)/20.f;bounds.vector[i]=float(int(i%17)-8)/20.f;}
 for(float k:{.01f,.1f,.2f,.5f,1.f})for(int mode=0;mode<6;++mode){
  Array a=input;
  if(mode==0)clamp_min_smooth(a,.1f,k);
  if(mode==1)clamp_max_smooth(a,.7f,k);
  if(mode==2)clamp_min_smooth(a,bounds,k);
  if(mode==3)clamp_max_smooth(a,bounds,k);
  if(mode==4)clamp_smooth(a,.1f,.7f,k);
  if(mode==5)for(auto&v:a.vector)v=clamp_min_smooth(v,.1f,k);
  for(float v:a.vector)if(!std::isfinite(v))return 3;
  f.write(reinterpret_cast<const char*>(a.vector.data()),a.vector.size()*4);
 }
 std::cout<<"30 positive-width overload cases,23040 finite cells\n";
}
