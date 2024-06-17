#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include <vector>

#include "simple_consts.hpp"

class BufferPool {
 public:
  explicit BufferPool(const std::vector<size_t>& buffer_sizes,
                      size_t initial_capacity = 10) {
#if !ALLOCATE_MALLOC
    for (size_t size : buffer_sizes) {
      pools[size].reserve(initial_capacity);
      for (size_t j = 0; j < initial_capacity; ++j) {
        pools[size].push_back(static_cast<char*>(std::malloc(size)));
      }
    }
#endif
  }

  ~BufferPool() {
#if !ALLOCATE_MALLOC
    for (auto& [size, pool] : pools) {
      for (auto buffer : pool) {
        std::free(buffer);
      }
    }
#endif
  }

  char* allocate(size_t size) {
#if !ALLOCATE_MALLOC
    auto it = pools.find(size);
    if (it != pools.end()) {
      auto& pool = it->second;
      if (!pool.empty()) {
        char* buffer = pool.back();
        pool.pop_back();
        //        printf("Reusing buffer of size %lu\n", size);
        return buffer;
      }
    }
#endif
    //    printf("Allocating new buffer of size %lu\n", size);
    return static_cast<char*>(std::malloc(size));
  }

  void deallocate(char* buffer, size_t size) {
#if !ALLOCATE_MALLOC
    //    printf("Deallocating buffer of size %lu\n", size);
    auto it = pools.find(size);
    if (it != pools.end()) {
      auto& pool = it->second;
      if (pool.size() < pool.capacity()) {
        pool.push_back(buffer);
        return;
      }
    }
#endif
    std::free(buffer);
  }

 private:
  std::unordered_map<size_t, std::vector<char*>> pools;
};