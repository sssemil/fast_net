#ifndef PAGE_SIZE
#define PAGE_SIZE 4*4096
#endif

#ifndef PORT
#define PORT 12348
#endif

#ifndef RING_SIZE
#define RING_SIZE 128
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


struct RequestData {
  size_t seq[2];
  int event_type;
  ssize_t buffer_offset;
  size_t buffer_size;
  int32_t buffer[];
};

enum EventType { READ_EVENT, WRITE_EVENT, SEND_EVENT, RECV_EVENT };

RequestData* create_request_data(size_t buffer_size) {
    size_t total_size = sizeof(RequestData) + buffer_size * sizeof(int32_t);
    RequestData* request = static_cast<RequestData*>(std::malloc(total_size));
    if (!request) {
        std::cerr << "Failed to allocate memory\n";
        std::exit(EXIT_FAILURE);
    }
    request->buffer_size = buffer_size;
    return request;
}