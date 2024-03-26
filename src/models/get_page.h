#pragma once

#include <array>
#include <cstdint>

#include "../consts.h"

// TODO: Consider network byte order/endianness during serialization/deserialization

enum GetPageStatus {
  SUCCESS = 200,
  INVALID_PAGE_NUMBER = 400,
};

struct GetPageRequest {
  uint32_t page_number;
  GetPageRequest() = default;
};

struct GetPageResponse {
  GetPageStatus status;
  const std::array<uint8_t, PAGE_SIZE> content = {};
};