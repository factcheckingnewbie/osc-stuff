#pragma once


// Network endpoints
#define LISTEN_ADDRESS "127.0.0.1"
#define LISTEN_PORT 9000
 
#define SEND_ADDRESS "127.0.0.1"
#define SEND_PORT 57120

// Inbound OSC: exactly 3 floats at /accel per message
#define INPUT_OSC_ADDRESS "/accel"
#define FLOATS_PER_READ 3

// Outbound OSC: forward same address (compile-time pass-through)
#define OUTPUT_OSC_ADDRESS INPUT_OSC_ADDRESS

// Accumulate N inbound messages before sending one blob
#define READS_PER_BLOB 4

// MTU guard for total OSC packet bytes (address + typetags + blob header + blob data)
#define MAX_OSC_PACKET_BYTES 1400
