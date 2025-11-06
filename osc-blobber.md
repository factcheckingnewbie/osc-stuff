# tinyosc Binary Blob Implementation Instructions

## Overview
Modify tinyosc to receive float data via UDP, accumulate into buffers, pack as binary OSC blobs, and send to SuperCollider.

## Configuration Parameters

Add to tinyosc.h:
```c
#define LISTEN_ADDRESS "127.0.0.1"
#define LISTEN_PORT 9000
#define SEND_ADDRESS "127.0.0.1"
#define SEND_PORT 57120
#define FLOATS_PER_READ 3
#define READS_PER_BLOB 4
#define MAX_BLOB_SIZE 1024
```

## Step 1: Add Global Variables

In main.c, add before main():
```c
static char blob_buffer[MAX_BLOB_SIZE];
static float accumulator[FLOATS_PER_READ * READS_PER_BLOB];
static int current_read = 0;
static int send_sock;
static struct sockaddr_in sc_addr;
```

## Step 2: Modify Socket Initialization

Replace existing socket setup in main():
```c
// Receiving socket
sin.sin_family = AF_INET;
sin.sin_port = htons(LISTEN_PORT);
sin.sin_addr.s_addr = inet_addr(LISTEN_ADDRESS);
bind(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in));

// Sending socket
send_sock = socket(AF_INET, SOCK_DGRAM, 0);
sc_addr.sin_family = AF_INET;
sc_addr.sin_port = htons(SEND_PORT);
sc_addr.sin_addr.s_addr = inet_addr(SEND_ADDRESS);
```

## Step 3: Add Blob Sending Function

Add to tinyosc.c:
```c
void send_accumulated_blob(const char* osc_address) {
    tosc_message msg;
    tosc_writeMessage(&msg, blob_buffer, MAX_BLOB_SIZE);
    
    tosc_writeAddress(&msg, osc_address);
    tosc_writeFormat(&msg, "b");
    
    int blob_size = FLOATS_PER_READ * READS_PER_BLOB * 4;
    tosc_writeNextBlob(&msg, blob_size);
    
    uint32_t* ptr = (uint32_t*)(msg.buffer + msg.marker);
    for(int i = 0; i < FLOATS_PER_READ * READS_PER_BLOB; i++) {
        uint32_t val = *(uint32_t*)&accumulator[i];
        ptr[i] = htonl(val);
    }
    msg.marker += blob_size;
    
    sendto(send_sock, msg.buffer, tosc_getMessageLength(&msg), 
           0, (struct sockaddr*)&sc_addr, sizeof(sc_addr));
    
    current_read = 0;
}
```

## Step 4: Modify Message Processing

In main.c, replace the message handling section:
```c
while (1) {
    int len = recvfrom(fd, buffer, sizeof(buffer), 0, 
                       (struct sockaddr *) &sin, &sin_len);
    
    if (len > 0) {
        tosc_message osc;
        tosc_parseMessage(&osc, buffer, len);
        
        // Extract floats from incoming message
        if (osc.format[0] == 'f' && osc.format[1] == 'f' && osc.format[2] == 'f') {
            int base_index = current_read * FLOATS_PER_READ;
            
            for (int i = 0; i < FLOATS_PER_READ; i++) {
                accumulator[base_index + i] = tosc_getNextFloat(&osc);
            }
            
            current_read++;
            
            if (current_read >= READS_PER_BLOB) {
                send_accumulated_blob("/address");
            }
        }
    }
}
```

## Step 5: Update ws-bridge.js Configuration

Change UDP destination port from 41235 to 9000:
```javascript
const udpConfig = {
    host: 'localhost',
    port: 9000  // Changed from 41235
};
```

## Step 6: Compilation

```bash
gcc -o tinyosc main.c tinyosc.c -lm
```

## Data Flow Summary

1. ws-bridge.js sends float triplets to 127.0.0.1:9000
2. tinyosc receives and accumulates floats
3. After 4 reads (12 floats), pack into blob
4. Send blob to SuperCollider at 127.0.0.1:57120
5. Reset counter and continue

## Memory Management

- Pre-allocated buffers (no malloc in loop)
- Fixed-size accumulator array
- Reusable blob buffer
- Stack variables avoided in hot path

## Error Handling

Minimal as requested:
- No bounds checking
- No validation of float values
- Assumes consistent message format
- No retry on send failure
