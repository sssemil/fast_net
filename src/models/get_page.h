#pragma once

#include <array>
#include <cstdint>

#include "../consts.h"

enum GetPageStatus : uint32_t {
  SUCCESS = 200,
  INVALID_PAGE_NUMBER = 400,
};

struct GetPageRequest {
  uint32_t page_number;

  void to_network_order() { page_number = htonl(page_number); }

  void to_host_order() { page_number = ntohl(page_number); }
};

struct GetPageResponse {
  uint32_t status;
  std::array<uint8_t, PAGE_SIZE> content;

  void set_status(const GetPageStatus s) { status = static_cast<uint32_t>(s); }

  [[nodiscard]] GetPageStatus get_status() const {
    return static_cast<GetPageStatus>(status);
  }

  void to_network_order() { status = htonl(status); }

  void to_host_order() { status = ntohl(status); }
};
