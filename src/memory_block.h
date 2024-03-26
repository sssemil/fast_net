#pragma once

#include <cstdlib>
#include <functional>
#include <utility>

class MemoryBlock {
 public:
  static constexpr int size = 1024 * 1024;
  char data[size]{};

  MemoryBlock() {
    setFillingStrategy(fillAlphabetically);
    fill();
  }
  void setFillingStrategy(std::function<void(char *, int)> strategy) {
    fillingStrategy = std::move(strategy);
  }

  void fill() {
    if (fillingStrategy) {
      fillingStrategy(data, size);
    } else {
      // TODO: raise an exception
    }
  }
  static bool verify(const char *buffer, int startPosition, int bufferSize) {
    for (int i = 0; i < bufferSize; ++i) {
      if (char expectedChar = 'a' + (startPosition + i) % 26;
          buffer[i] != expectedChar) {
        return false;
      }
    }
    return true;
  }

 private:
  std::function<void(char *, int)> fillingStrategy;

  static void fillAlphabetically(char *data, int size) {
    for (int i = 0; i < size; ++i) {
      data[i] = i % 26 + 'a';
    }
  }

  static void fillWithPseudoRandom(char *data, int size, unsigned int seed) {
    srand(seed);
    for (int i = 0; i < size; ++i) {
      data[i] = rand() % 256;
    }
  }
};
