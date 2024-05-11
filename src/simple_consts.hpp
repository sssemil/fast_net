#define PAGE_SIZE 1024
#define PORT 12345
#define RING_SIZE 128
#define NUM_REQUESTS (1024 * 1024)
#define SERVER_ADDR "127.0.0.1"
#define VERIFY 1
#define VERBOSE 0
#define CLIENT_THREADS 1

struct RequestData {
  int event_type;
  int32_t* buffer;
};

enum EventType { READ_EVENT, WRITE_EVENT, SEND_EVENT, RECV_EVENT };
