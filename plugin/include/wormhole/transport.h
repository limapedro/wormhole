/* Wormhole network transport: portable sockets and mixed v2/Teleport framing. */
#ifndef WORMHOLE_TRANSPORT_H
#define WORMHOLE_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET wh_socket_t;
#define WH_INVALID_SOCKET INVALID_SOCKET
#else
typedef int wh_socket_t;
#define WH_INVALID_SOCKET (-1)
#endif

int wh_net_listen(uint16_t port, wh_socket_t *out);
int wh_net_accept(wh_socket_t listener, wh_socket_t *out);
int wh_net_connect(const char *host, uint16_t port, wh_socket_t *out);
void wh_net_close(wh_socket_t sock);
int wh_net_send(wh_socket_t sock, const uint8_t *data, size_t len);

/* Reads one complete frame, either a v2 frame or a Teleport-compatible
 * packet. The full frame is returned in buf, including its header. */
int wh_net_recv_frame(wh_socket_t sock, uint8_t *buf, size_t cap, size_t *len);

#endif
