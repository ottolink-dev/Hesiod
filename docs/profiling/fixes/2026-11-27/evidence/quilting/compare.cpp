#include "highmap/synthesis.hpp"
#include <fstream>
#include <iostream>
#include <cmath>
#include <cstdint>
using namespace hmap;
Array input(glm::ivec2 shape, int salt) {
 Array a(shape);
 for(int j=0;j<shape.y;++j) for(int i=0;i<shape.x;++i)
  a(i,j)=float(((i*37+j*71+i*j*13+salt*101)%257)-128)/128.f;
 return a;
}
void save(std::ofstream& f, const Array& a) {
 f.write(reinterpret_cast<const char*>(&a.shape.x),sizeof(int));
 f.write(reinterpret_cast<const char*>(&a.shape.y),sizeof(int));
 f.write(reinterpret_cast<const char*>(a.vector.data()),a.vector.size()*sizeof(float));
 for(float x:a.vector) if(!std::isfinite(x)) throw std::runtime_error("nonfinite");
}
int main(int argc,char**argv) {
 std::ofstream f(argv[1],std::ios::binary); int cases=0;
 int large=argc>2?std::stoi(argv[2]):0;
 for(auto shape: {glm::ivec2(48,48),glm::ivec2(48,32),glm::ivec2(37,29)})
 for(int channels: {0,4,20}) for(int flags=0;flags<8;++flags)
 for(float overlap: {0.f,0.5f,0.9f}) for(int method=0;method<5;++method)
 for(int aliases=0;aliases<3;++aliases) {
  if(large && (shape!=glm::ivec2(48,48)||channels!=20||flags!=7||overlap!=0.9f||method!=large||aliases!=0))continue;
  if(large)shape={1024,1024};
  Array a=input(shape,1), b=input(shape,2);
  std::vector<Array> storage;std::vector<Array*> ptrs;
  for(int k=0;k<channels;++k)storage.push_back(input(shape,k+3));
  for(int k=0;k<channels;++k)ptrs.push_back(aliases==1?&storage[0]:aliases==2&&k==0?&a:&storage[k]);
  glm::ivec2 patch=large?glm::ivec2(307,307):glm::ivec2(12,8);
  if(shape.x==shape.y&&!large)patch={12,12};
  std::cerr<<shape.x<<","<<shape.y<<" channels="<<channels<<" flags="<<flags<<" overlap="<<overlap<<" method="<<method<<" aliases="<<aliases<<"\n";
  Array out;
  if(method==0)out=quilting({&a,&b},patch,{3,4},overlap,123,ptrs,flags&1,flags&2,flags&4,0.5f);
  if(method==1)out=quilting_shuffle(a,patch,overlap,123,ptrs,flags&1,flags&2,flags&4,0.5f);
  if(method>=2)out=quilting_expand(a,method==4?1.5f:2.f,patch,overlap,123,ptrs,method!=3,flags&1,flags&2,flags&4,0.5f);
  save(f,out);save(f,a);for(auto*p:ptrs)save(f,*p);++cases;
 }
 std::cout<<cases<<" cases\n";
}
