#include "highmap/internal/assert.hpp"
#include <limits>
#include <iostream>
int main(){
 hmap::Array zero(glm::ivec2(2,2)), one(glm::ivec2(2,2),1.f);
 hmap::Array nan(glm::ivec2(2,2),std::numeric_limits<float>::quiet_NaN());
 hmap::Array inf(glm::ivec2(2,2),std::numeric_limits<float>::infinity());
 for(auto [a,b]: {std::pair{&nan,&one},{&inf,&inf},{&zero,&one},{&one,&zero}})
  std::cout<<hmap::assert_almost_equal(*a,*b)<<'\n';
}
