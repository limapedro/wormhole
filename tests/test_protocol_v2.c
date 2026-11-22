/* Wormhole v2 protocol unit tests. Copyright (C) 2026 Wormhole contributors. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "wormhole/protocol_v2.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_hello_roundtrip(void)
{
    uint8_t buf[512];
    size_t n = wh2_build_hello(buf, sizeof(buf), 7, 1234, WH2_CAP_MULTI_AUDIO,
                               "gaming-pc", 3, 2560, 1440, 60, 1);
    wh2_hello_t hello;
    assert(n > WH2_HEADER_LEN);
    assert(wh2_parse_hello(buf, n, &hello) == WH2_PARSE_OK);
    assert(hello.header.version == WH2_VERSION);
    assert(hello.header.type == WH2_MSG_HELLO);
    assert(hello.header.sequence == 7);
    assert(hello.header.timestamp_ns == 1234);
    assert(hello.capabilities == WH2_CAP_MULTI_AUDIO);
    assert(strcmp(hello.name, "gaming-pc") == 0);
    assert(hello.track_count == 3);
    assert(hello.width == 2560 && hello.height == 1440);
    assert(hello.fps_num == 60 && hello.fps_den == 1);
    printf("ok: v2 hello round-trip\n");
}

static void test_ack_roundtrip(void)
{
    uint8_t buf[128];
    wh2_hello_ack_t ack;
    size_t n = wh2_build_hello_ack(buf, sizeof(buf), 8, 99, WH2_CAP_MULTI_AUDIO, "gaming-pc");
    assert(n > WH2_HEADER_LEN);
    assert(wh2_parse_hello_ack(buf, n, &ack) == WH2_PARSE_OK);
    assert(ack.accepted_version == WH2_VERSION);
    assert(ack.capabilities == WH2_CAP_MULTI_AUDIO);
    assert(strcmp(ack.sender_name, "gaming-pc") == 0);
    printf("ok: v2 ack round-trip\n");
}

static void test_stream_desc_roundtrip(void)
{
    uint8_t buf[512];
    wh2_stream_desc_t desc;
    size_t n = wh2_build_stream_desc(buf, sizeof(buf), 9, 500, 17, "Mic One",
                                     48000, 2, 16, 1, 0);
    assert(n > WH2_HEADER_LEN);
    assert(wh2_parse_stream_desc(buf, n, &desc) == WH2_PARSE_OK);
    assert(desc.track_id == 17);
    assert(strcmp(desc.name, "Mic One") == 0);
    assert(desc.sample_rate == 48000);
    assert(desc.channels == 2 && desc.bits == 16);
    assert(desc.clock_id == 1 && desc.codec_fourcc == 0);
    printf("ok: v2 stream descriptor round-trip\n");
}

static void test_video_audio_roundtrip(void)
{
    uint8_t buf[4096];
    const uint8_t jpeg[5] = {0xff, 0xd8, 0x01, 0x02, 0xff};
    const uint8_t pcm[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    wh2_video_t video;
    wh2_audio_t audio;
    size_t n = wh2_build_video(buf, sizeof(buf), 10, 1000, 55, 1920, 1080, jpeg, sizeof(jpeg));
    assert(n == WH2_HEADER_LEN + 16 + sizeof(jpeg));
    assert(wh2_parse_video(buf, n, &video) == WH2_PARSE_OK);
    assert(video.sequence64 == 55 && video.width == 1920 && video.height == 1080);
    assert(video.payload_len == sizeof(jpeg));
    assert(memcmp(video.payload, jpeg, sizeof(jpeg)) == 0);

    n = wh2_build_audio(buf, sizeof(buf), 11, 1000, 17, 1024, 48000, 2, 16, pcm, sizeof(pcm));
    assert(n == WH2_HEADER_LEN + 16 + sizeof(pcm));
    assert(wh2_parse_audio(buf, n, &audio) == WH2_PARSE_OK);
    assert(audio.track_id == 17 && audio.frames == 1024);
    assert(audio.sample_rate == 48000 && audio.channels == 2 && audio.bits == 16);
    assert(audio.payload_len == sizeof(pcm));
    assert(memcmp(audio.payload, pcm, sizeof(pcm)) == 0);
    printf("ok: v2 video/audio round-trip\n");
}

static void test_malformed(void)
{
    uint8_t buf[256];
    wh2_header_t header;
    wh2_hello_t hello;
    size_t n = wh2_build_hello(buf, sizeof(buf), 1, 1, 0, "x", 0, 1, 1, 30, 1);
    assert(wh2_parse_hello(buf, n, &hello) == WH2_PARSE_OK);

    buf[0] = 'X';
    assert(wh2_header_decode(buf, n, &header) == WH2_PARSE_BAD_MAGIC);
    buf[0] = 'W';
    buf[4] = 9;
    assert(wh2_header_decode(buf, n, &header) == WH2_PARSE_BAD_VERSION);
    buf[4] = WH2_VERSION & 0xff;
    assert(wh2_header_decode(buf, 8, &header) == WH2_PARSE_TRUNCATED);
    assert(wh2_parse_hello(buf, n, &hello) == WH2_PARSE_OK);
    (void)header;
    printf("ok: v2 malformed input rejected\n");
}

int main(void)
{
    test_hello_roundtrip();
    test_ack_roundtrip();
    test_stream_desc_roundtrip();
    test_video_audio_roundtrip();
    test_malformed();
    printf("ALL V2 TESTS PASSED\n");
    return 0;
}
