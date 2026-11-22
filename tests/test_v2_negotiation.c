/* End-to-end TCP test of Wormhole v2 negotiation and multi-audio framing. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "wormhole/protocol_v2.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define INVALID_S INVALID_SOCKET
#define close_s closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef int sock_t;
#define INVALID_S (-1)
#define close_s close
#endif

static int send_all(sock_t s, const uint8_t *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int n = send(s, (const char *)data + sent, (int)(len - sent), 0);
        if (n <= 0) return 0;
        sent += (size_t)n;
    }
    return 1;
}

static int recv_frame(sock_t s, uint8_t *buf, size_t cap, size_t *out_len)
{
    wh2_header_t header;
    uint32_t total;
    size_t got = 0;
    while (got < WH2_HEADER_LEN) {
        int n = recv(s, (char *)buf + got, (int)(WH2_HEADER_LEN - got), 0);
        if (n <= 0) return 0;
        got += (size_t)n;
    }
    total = (uint32_t)buf[8] | ((uint32_t)buf[9] << 8) |
            ((uint32_t)buf[10] << 16) | ((uint32_t)buf[11] << 24);
    if (total < WH2_HEADER_LEN || total > cap)
        return 0;
    while (got < total) {
        int n = recv(s, (char *)buf + got, (int)(total - got), 0);
        if (n <= 0) return 0;
        got += (size_t)n;
    }
    if (wh2_header_decode(buf, got, &header) != WH2_PARSE_OK)
        return 0;
    *out_len = got;
    return 1;
}

int main(void)
{
    sock_t listener, server = INVALID_S, client = INVALID_S;
    struct sockaddr_in addr;
    uint8_t out[8192], in[8192];
    size_t n, got;
    wh2_hello_t hello;
    wh2_hello_ack_t ack;
    wh2_stream_desc_t desc;
    wh2_video_t video;
    wh2_audio_t audio;
    const uint8_t jpeg[4] = {0xff, 0xd8, 0xff, 0xd9};
    const uint8_t pcm[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    int descriptors = 0;
    int audio_frames = 0;
    int video_frames = 0;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
#endif

    listener = socket(AF_INET, SOCK_STREAM, 0);
    assert(listener != INVALID_S);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(18881);
    assert(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);
    assert(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    assert(listen(listener, 1) == 0);

    client = socket(AF_INET, SOCK_STREAM, 0);
    assert(client != INVALID_S);
    assert(connect(client, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    server = accept(listener, NULL, NULL);
    assert(server != INVALID_S);

    /* Sender announces v2 with three audio tracks. */
    n = wh2_build_hello(out, sizeof(out), 1, 100, WH2_CAP_MULTI_AUDIO | WH2_CAP_CLOCK_SYNC,
                        "gaming-pc", 3, 2560, 1440, 60, 1);
    assert(n && send_all(server, out, n));

    /* Receiver accepts and negotiates down to the shared capabilities. */
    assert(recv_frame(client, in, sizeof(in), &got));
    assert(wh2_parse_hello(in, got, &hello) == WH2_PARSE_OK);
    assert(hello.track_count == 3);
    printf("negotiation: HELLO from '%s' with %u tracks\n", hello.name, hello.track_count);
    n = wh2_build_hello_ack(out, sizeof(out), 2, 101,
                            hello.capabilities & (WH2_CAP_MULTI_AUDIO | WH2_CAP_CLOCK_SYNC));
    assert(n && send_all(client, out, n));

    assert(recv_frame(server, in, sizeof(in), &got));
    assert(wh2_parse_hello_ack(in, got, &ack) == WH2_PARSE_OK);
    assert(ack.accepted_version == WH2_VERSION);
    assert(ack.capabilities == (WH2_CAP_MULTI_AUDIO | WH2_CAP_CLOCK_SYNC));
    printf("negotiation: ACK accepted v%u, capabilities=0x%x\n",
           ack.accepted_version, ack.capabilities);

    /* Three stream descriptors, then one video frame and three audio frames. */
    {
        const char *names[3] = {"DesktopAudio", "MicOne", "MicTwo"};
        int i;
        for (i = 0; i < 3; ++i) {
            n = wh2_build_stream_desc(out, sizeof(out), (uint32_t)(3 + i), 200,
                                      (uint32_t)(10 + i), names[i], 48000, 2, 16,
                                      1, 0);
            assert(n && send_all(server, out, n));
        }
    }
    n = wh2_build_video(out, sizeof(out), 6, 300, 1, 1920, 1080, jpeg, sizeof(jpeg));
    assert(n && send_all(server, out, n));
    {
        int i;
        for (i = 0; i < 3; ++i) {
            n = wh2_build_audio(out, sizeof(out), (uint32_t)(7 + i), 301,
                                (uint32_t)(10 + i), 1024, 48000, 2, 16, pcm, sizeof(pcm));
            assert(n && send_all(server, out, n));
        }
    }

    for (;;) {
        if (!recv_frame(client, in, sizeof(in), &got))
            break;
        {
            wh2_header_t h;
            assert(wh2_header_decode(in, got, &h) == WH2_PARSE_OK);
            if (h.type == WH2_MSG_STREAM_DESC) {
                assert(wh2_parse_stream_desc(in, got, &desc) == WH2_PARSE_OK);
                ++descriptors;
                printf("stream %u: '%s' %u Hz %u ch %u-bit clock=%u\n",
                       desc.track_id, desc.name, desc.sample_rate,
                       desc.channels, desc.bits, desc.clock_id);
            } else if (h.type == WH2_MSG_VIDEO) {
                assert(wh2_parse_video(in, got, &video) == WH2_PARSE_OK);
                assert(video.width == 1920 && video.height == 1080);
                assert(video.payload_len == sizeof(jpeg));
                ++video_frames;
                printf("video: %ux%u, %zu bytes\n", video.width, video.height, video.payload_len);
            } else if (h.type == WH2_MSG_AUDIO) {
                assert(wh2_parse_audio(in, got, &audio) == WH2_PARSE_OK);
                ++audio_frames;
                assert(audio.payload_len == sizeof(pcm));
            }
        }
        if (descriptors == 3 && audio_frames == 3 && video_frames == 1)
            break;
    }
    assert(descriptors == 3);
    assert(audio_frames == 3);
    assert(video_frames == 1);
    printf("V2 NEGOTIATION TEST PASSED: %d descriptors, %d audio frames\n",
           descriptors, audio_frames);

    close_s(client);
    close_s(server);
    close_s(listener);
    return 0;
}
