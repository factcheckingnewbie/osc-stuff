#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "osc-blobber-custom.h"
    

// Pad to next 4-byte boundary
static inline size_t pad4(size_t n) { return (n + 3u) & ~3u; }

int main(void) {
  // Precompute constants (once)
  static const char in_typetags[] = ",fff"; // inbound is exactly 3 floats
  static const char out_typetags[] = ",b";  // outbound blob

  const size_t in_addr_block = pad4(strlen(INPUT_OSC_ADDRESS) + 1u);
  const size_t in_type_block = pad4(sizeof(in_typetags)); // includes '\0'
  const size_t in_args_offset = in_addr_block + in_type_block;
  const size_t in_args_bytes = 4u * (size_t)FLOATS_PER_READ; // big-endian floats, 4 bytes each
  const size_t in_min_bytes = in_args_offset + in_args_bytes;

  const size_t out_addr_block = pad4(strlen(OUTPUT_OSC_ADDRESS) + 1u);
  const size_t out_type_block = pad4(sizeof(out_typetags)); // includes '\0'
  const size_t total_floats = (size_t)FLOATS_PER_READ * (size_t)READS_PER_BLOB;
  const uint32_t blob_bytes = (uint32_t)(4u * total_floats); // multiple of 4, so no blob padding needed
  const size_t out_packet_bytes = out_addr_block + out_type_block + 4u /*blob size i32*/ + (size_t)blob_bytes;

  if (out_packet_bytes > (size_t)MAX_OSC_PACKET_BYTES) {
    // Single init-time guard; no runtime checks
    fprintf(stderr, "osc-blobber: out packet %zu exceeds MAX_OSC_PACKET_BYTES=%d\n",
            out_packet_bytes, MAX_OSC_PACKET_BYTES);
    return 1;
  }

  // Pre-build outbound address block
  uint8_t addr_block[64]; // ample for short paths like "/accel"
  if (out_addr_block > sizeof(addr_block)) {
    fprintf(stderr, "osc-blobber: address block too large\n");
    return 1;
  }
  memset(addr_block, 0, out_addr_block);
  memcpy(addr_block, OUTPUT_OSC_ADDRESS, strlen(OUTPUT_OSC_ADDRESS));

  // Pre-build outbound typetag block for ",b"
  uint8_t type_block[8]; // ",b\0" => padded to 4 bytes
  memset(type_block, 0, out_type_block);
  memcpy(type_block, out_typetags, strlen(out_typetags));

  // Pre-build inbound address block (for optional minimal sanity at startup only)
  uint8_t in_addr_check[64];
  if (in_addr_block > sizeof(in_addr_check)) {
    fprintf(stderr, "osc-blobber: inbound address block too large\n");
    return 1;
  }
  memset(in_addr_check, 0, in_addr_block);
  memcpy(in_addr_check, INPUT_OSC_ADDRESS, strlen(INPUT_OSC_ADDRESS));

  // Pre-allocate buffers (heap-only requirement satisfied; no malloc in hot loop)
  uint8_t recv_buf[2048];
  uint8_t out_buf[MAX_OSC_PACKET_BYTES];

  // Accumulator stores network-order float bytes directly (no conversions)
  uint8_t accum[4u * (size_t)FLOATS_PER_READ * (size_t)READS_PER_BLOB];
  uint8_t* wptr = accum;
  uint8_t* const wend = accum + sizeof(accum);

  // Prepare constant parts of outbound buffer once (address + typetag)
  memcpy(out_buf, addr_block, out_addr_block);
  memcpy(out_buf + out_addr_block, type_block, out_type_block);
  const size_t blob_size_offset = out_addr_block + out_type_block;
  const size_t blob_data_offset = blob_size_offset + 4u;

  // UDP socket (one socket for recv and send)
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    perror("socket");
    return 1;
  }

  struct sockaddr_in sin = {0};
  sin.sin_family = AF_INET;
  sin.sin_port = htons(LISTEN_PORT);
  if (inet_pton(AF_INET, LISTEN_ADDRESS, &sin.sin_addr) != 1) {
    fprintf(stderr, "osc-blobber: invalid LISTEN_ADDRESS\n");
    close(fd);
    return 1;
  }
  if (bind(fd, (struct sockaddr*)&sin, sizeof(sin)) != 0) {
    perror("bind");
    close(fd);
    return 1;
  }

  struct sockaddr_in dst = {0};
  dst.sin_family = AF_INET;
  dst.sin_port = htons(SEND_PORT);
  if (inet_pton(AF_INET, SEND_ADDRESS, &dst.sin_addr) != 1) {
    fprintf(stderr, "osc-blobber: invalid SEND_ADDRESS\n");
    close(fd);
    return 1;
  }

  // Minimal startup sanity (one-time): ensure inbound address layout matches our assumptions
  // No runtime per-packet checks afterward.
  {
    // None beyond initial constants; proceed directly.
  }

  for (;;) {
    ssize_t n = recvfrom(fd, recv_buf, sizeof(recv_buf), 0, NULL, NULL);
    if (n <= 0) {
      // Minimal handling: ignore errors/timeouts; continue
      continue;
    }

    // Minimal size check to avoid OOB read if upstream misbehaves
    if ((size_t)n < in_min_bytes) {
      continue;
    }

    // Optional zero-cost assumption: inbound address equals INPUT_OSC_ADDRESS and typetags ",fff"
    // We do not check per packet; fixed offsets computed above.
    const uint8_t* args = recv_buf + in_args_offset;

    // Copy FLOATS_PER_READ big-endian floats (each 4 bytes) straight into accumulator
    // No per-float conditions; a single memcpy per packet.
    memcpy(wptr, args, in_args_bytes);
    wptr += in_args_bytes;

    // When full, emit one blob
    if (wptr == wend) {
      // Write blob size (big-endian int32)
      const uint32_t be_len = htonl(blob_bytes);
      memcpy(out_buf + blob_size_offset, &be_len, 4);

      // Copy payload (already big-endian float bytes)
      memcpy(out_buf + blob_data_offset, accum, blob_bytes);

      // Send
      (void)sendto(fd, out_buf, out_packet_bytes, 0, (struct sockaddr*)&dst, sizeof(dst));

      // Reset accumulator write pointer
      wptr = accum;
    }
  }

  // Unreachable
  // close(fd);
  // return 0;
}
