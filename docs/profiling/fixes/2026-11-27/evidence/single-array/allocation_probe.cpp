#include "highmap/virtual_array/virtual_array.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>

static std::atomic<std::size_t> tracked_bytes{0};
static std::atomic<std::size_t> matching_allocations{0};
void *operator new(std::size_t size)
{
  if (size == tracked_bytes.load()) ++matching_allocations;
  if (void *result = std::malloc(size ? size : 1)) return result;
  throw std::bad_alloc();
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }
int main(int argc, char **argv)
{
  using namespace hmap;
  int n = argc > 1 ? std::atoi(argv[1]) : 1024;
  int stride = argc > 2 ? std::atoi(argv[2]) : 1;
  StorageMode storage = argc > 3 && std::atoi(argv[3]) ? VA_DISK_LRU_STREAM : VA_RAM;
  ComputeMode seq{.mode = ForEachMode::VA_SEQUENTIAL};
  VirtualArray a({n,n}, {256,256}, 0, storage);
  VirtualArray b({n,n}, {256,256}, 0, storage);
  VirtualArray c({n,n}, {256,256}, 0, storage);
  a.fill(2.f,seq); b.fill(3.f,seq); c.fill(5.f,seq);
  tracked_bytes = std::size_t(n) * n * sizeof(float);
  matching_allocations = 0;
  std::size_t count = 0;
  for_each_tile(TileAccess{{&a,&b},{&c,&a}},
    [&](const std::vector<const Array*> &in, std::vector<Array*> &out, const TileRegion&)
    {
      count = matching_allocations.load();
      for (int k=0;k<out[0]->size();++k) out[0]->vector[k] += in[0]->vector[k]+in[1]->vector[k];
      for (float &value : out[1]->vector) value += 1.f;
    }, {.mode=ForEachMode::VA_SINGLE_ARRAY,.stride=stride});
  tracked_bytes=0;
  float sum_a=a.sum(seq),sum_c=c.sum(seq);
  std::printf("{\"size\":%d,\"stride\":%d,\"disk\":%d,\"gather_source_shape_allocations\":%zu,\"sum_a\":%.0f,\"sum_c\":%.0f}\n", n,stride,int(storage),count,sum_a,sum_c);
  return sum_a == 3.f*n*n && sum_c == 10.f*n*n ? 0 : 2;
}
