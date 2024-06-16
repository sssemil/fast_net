#include <iostream>
#include <mutex>
#include <stack>

class BufferPool {
 public:
  BufferPool(size_t buffer_size, size_t initial_pool_size);
  ~BufferPool();

  char* allocate();
  void deallocate(char* buffer);

 private:
  std::stack<char*> pool;
  size_t buffer_size;

  void expand_pool(size_t count);
};

BufferPool::BufferPool(size_t buffer_size, size_t initial_pool_size)
    : buffer_size(buffer_size) {
  expand_pool(initial_pool_size);
}

BufferPool::~BufferPool() {
  while (!pool.empty()) {
    delete[] pool.top();
    pool.pop();
  }
}

void BufferPool::expand_pool(size_t count) {
  for (size_t i = 0; i < count; ++i) {
    pool.push(new char[buffer_size]);
  }
}

char* BufferPool::allocate() {
//    return new char[buffer_size];

  if (pool.empty()) {
    return new char[buffer_size];
  } else {
    char* buffer = pool.top();
    pool.pop();
    return buffer;
  }
}

void BufferPool::deallocate(char* buffer) {
//    delete[] buffer;
//    return;
  pool.push(buffer);
}
