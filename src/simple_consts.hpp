#pragma once

#ifndef PAGE_SIZE
#define PAGE_SIZE 8
#endif

#ifndef PORT
#define PORT 12347
#endif

#ifndef RING_SIZE
#define RING_SIZE 1024
#endif

#ifndef NUM_REQUESTS
#define NUM_REQUESTS (1024 * 1024)
#endif

#ifndef SERVER_ADDR
#define SERVER_ADDR "127.0.0.1"
#endif

#ifndef VERIFY
#define VERIFY 0
#endif

#ifndef VERBOSE
#define VERBOSE 0
#endif

#ifndef CLIENT_THREADS
#define CLIENT_THREADS 8
#endif

#ifndef ALLOCATE_MALLOC
#define ALLOCATE_MALLOC 0
#endif

#define BUFFER_POOL_INITIAL_POOL_SIZE 128

struct RequestData {
  size_t seq[2];
  int event_type;
  ssize_t buffer_offset;
  size_t buffer_size;
  int32_t buffer[];
};

enum EventType { READ_EVENT, WRITE_EVENT, SEND_EVENT, RECV_EVENT };
