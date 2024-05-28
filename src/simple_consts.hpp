#define PAGE_SIZE 4096
#define PORT 12348
#define RING_SIZE 384
#define NUM_REQUESTS (1024 * 1024)
#define SERVER_ADDR "127.0.0.1"
#define VERIFY 0
#define VERBOSE 0
#define CLIENT_THREADS 16

struct RequestData {
  size_t seq[2];
  int event_type;
  int32_t* buffer;
  ssize_t buffer_offset;
};

enum EventType { READ_EVENT, WRITE_EVENT, SEND_EVENT, RECV_EVENT };
