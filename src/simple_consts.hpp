#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifndef PORT
#define PORT 12348
#endif

#ifndef RING_SIZE
#define RING_SIZE 384
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
#define CLIENT_THREADS 16
#endif


struct RequestData {
  size_t seq[2];
  int event_type;
  int32_t* buffer;
  ssize_t buffer_offset;
};

enum EventType { READ_EVENT, WRITE_EVENT, SEND_EVENT, RECV_EVENT };
