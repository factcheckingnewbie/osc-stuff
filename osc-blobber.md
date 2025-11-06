# osc-blobber Implementation Instructions
Assume this is ment to be an effective production grade on-shot app.

## Overview
Receive float data via listen-UDP-port, accumulate into buffers, pack as binary OSC blobs, and send to send-UDP-port.
Only floats mentv to be used are on listn-port.

## Problem Statement
- Multiple bus.control in SuperCollider handling 400Hz data each
- Need to reduce sclang overhead by sending binary OSC blobs
- Heap-only methods required (no malloc/free at 400Hz)

## Configuration Parameters

Add to osc-blobber-custom.h:
```
#define LISTEN_ADDRESS "127.0.0.1"
#define LISTEN_PORT 9000
#define SEND_ADDRESS "127.0.0.1"
#define SEND_PORT 57120
#define FLOATS_PER_READ 3  // informative, how many floats there are expected to be per read on listen-port 
#define READS_PER_BLOB 4   // iterations before sending (configurable)
#define MAX_BLOB_SIZE 1024
#define OSC_ADRESS "/address"
```


### Data Flow
1. WebSocket server (separate) → ws-bridge.js (separate) → UDP port 9000
2. listens on 127.0.0.1:9000
3. accumulates float data
4. sends binary blobs to SuperCollider at 127.0.0.1:57120

### Variables
- Total floats per blob = FLOATS_PER_READ × READS_PER_BLOB
- Example: 3 floats × 4 reads = 12 floats per blob

### Processing Logic
1. Receive OSC from LISTEN_ADRESS:LISTEN__PORT messages containing floats 
2. Adds FLOATS_PER_READ (
3. Accumulate in pre-allocated buffer
4. After READS_PER_BLOB iterations:
   - Pack accumulated floats into binary blob
   - Send blob to SuperCollider at /address
   - Reset counter
5. Continue loop

### Binary Blob Format
- OSC message with address "/address"
- Type tag "b" (blob)
- Contains sequential 32-bit floats in big-endian format
- SuperCollider unpacks as Int8Array, converts 4-byte chunks to floats


## SuperCollider Receiver
- Listening on port 57120 (configurable)
- OSC address: /address
- Expects binary blob containing big-endian floats
- Unpacks using blob.size/4 to determine float count

## Data Format
Input stream example (from WebSocket server):
```
0.01305 0.162 9.880951
0.003 0.16395001 9.897
-0.0010500001 0.16905001 9.864
```


## Key Constraints
- 32-bit floats only at sending end
- 400Hz data rate
- Heap-only methods (pre-allocated buffers)
- C only (not C++)
