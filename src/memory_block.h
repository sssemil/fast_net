#pragma once

#include <array>
#include <vector>

#include "consts.h"

class IFillingStrategy {
 public:
  virtual ~IFillingStrategy() = default;
  [[nodiscard]] virtual uint8_t get_value_at(size_t position) const = 0;
};

class AlphabeticalFillingStrategy final : public IFillingStrategy {
 public:
  [[nodiscard]] uint8_t get_value_at(const size_t position) const override {
    return 'a' + position % 26;
  }
};

class PseudoRandomFillingStrategy final : public IFillingStrategy {
  unsigned int seed;

 public:
  explicit PseudoRandomFillingStrategy(
      const unsigned int seed = PSEUDO_RANDOM_SEED)
      : seed(seed) {}

  [[nodiscard]] uint8_t get_value_at(const size_t position) const override {
    return static_cast<uint8_t>((position * 2654435761u + seed) % 256);
  }
};

class MemoryBlock {
 public:
  std::vector<uint8_t> data;
  size_t page_size;
  size_t page_count;
  const IFillingStrategy* strategy;

  MemoryBlock(const size_t page_size, const size_t page_count,
              const IFillingStrategy* strategy)
      : data(page_size * page_count, 0),
        page_size(page_size),
        page_count(page_count),
        strategy(strategy) {
    if (strategy == nullptr) {
      throw std::runtime_error("Filling strategy is not set");
    }
    fill();
  }

  void fill() {
    for (size_t i = 0; i < data.size(); ++i) {
      data[i] = strategy->get_value_at(i);
    }
  }

  [[nodiscard]] bool verify(
      const std::array<uint8_t, PAGE_SIZE>& page_content,
      const size_t page_number) const {
    for (size_t i = 0; i < PAGE_SIZE; ++i) {
      if (page_content[i] !=
          strategy->get_value_at(page_number * PAGE_SIZE + i)) {
        return false;
      }
    }
    return true;
  }
};
