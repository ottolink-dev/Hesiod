#include "highmap/synthesis.hpp"
#include <iostream>
int main(){
 hmap::Array input(glm::ivec2(37,29),1.f);
 for(float overlap:{0.f,0.01f}) {
  std::cout<<"overlap="<<overlap<<'\n';
  auto core=hmap::quilting({&input},{12,8},{3,3},overlap,123);
  std::cout<<"core: "<<core.shape.x<<'x'<<core.shape.y<<' '<<core.vector.size()<<" cells\n";
  auto shuffle=hmap::quilting_shuffle(input,{12,8},overlap,123);
  auto blend=hmap::quilting_blend({&input},{12,8},overlap,123);
  auto expand=hmap::quilting_expand(input,2.f,{12,8},overlap,123,{},false);
  for(auto p:{&shuffle,&blend,&expand})std::cout<<p->shape.x<<'x'<<p->shape.y<<' '<<p->vector.size()<<" cells\n";
 }
}
