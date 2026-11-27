#include "highmap/primitives.hpp"
#include "highmap/internal/assert.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"
#include <cmath>
#include <iostream>
void report(const char*name,const hmap::Array&a,const hmap::Array&b){
 size_t bad_a=0,bad_b=0;double error=0;
 for(size_t i=0;i<a.vector.size();++i){bad_a+=!std::isfinite(a.vector[i]);bad_b+=!std::isfinite(b.vector[i]);error+=std::abs(double(a.vector[i])-b.vector[i]);}
 std::cout<<name<<": CPU nonfinite="<<bad_a<<" GPU nonfinite="<<bad_b<<" cells="<<a.vector.size()<<" old_assert="<<hmap::assert_almost_equal(a,b,1e-5f)<<" strict_L1="<<error<<'\n';
}
int main(){
 using namespace hmap;
 if(!gpu::init_opencl())return 2;
 glm::ivec2 shape(64,64);glm::vec2 kw(4.f,4.f);unsigned seed=42;
 Array nx=.25f*noise(PERLIN,shape,kw,seed+10),ny=.25f*noise(PERLIN,shape,kw,seed+20);
 report("noise",noise(PERLIN_HALF,shape,kw,seed,&nx,&ny),gpu::noise(PERLIN_HALF,shape,kw,seed,&nx,&ny));
 report("noise_fbm",noise_fbm(PERLIN_HALF,shape,kw,seed,6,.7f,.5f,2.f,nullptr,&nx,&ny),gpu::noise_fbm(PERLIN_HALF,shape,kw,seed,6,.7f,.5f,2.f,nullptr,&nx,&ny));
 for(float x:{-1.f,0.f,1.f})std::cout<<"clamp_min_smooth("<<x<<",0,0)="<<clamp_min_smooth(x,0.f,0.f)<<'\n';
}
