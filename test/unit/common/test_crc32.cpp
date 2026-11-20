#include "doctest.h"
#include "zugodb/common/crc32.hpp"
#include <cstring>

using namespace zugodb;

static const uint8_t *Bytes(const char *s) {
    return reinterpret_cast<const uint8_t *>(s);
}

TEST_CASE("CRC-32 gives the standard check value over \"123456789\"") {
    // The check value of CRC-32/ISO-HDLC (zlib, PNG, gzip), from the
    // catalogue of parametrised CRC algorithms.
    CHECK(Crc32(Bytes("123456789"), 9) == 0xCBF43926u);
}

TEST_CASE("CRC-32 of nothing is 0 and a running value chains") {
    CHECK(Crc32(nullptr, 0) == 0u);
    // Known zlib values.
    CHECK(Crc32(Bytes("a"), 1) == 0xE8B7BE43u);
    const char *text = "The quick brown fox jumps over the lazy dog";
    auto n = std::strlen(text);
    CHECK(Crc32(Bytes(text), n) == 0x414FA339u);
    for (size_t split = 0; split <= n; split++) {
        CHECK(Crc32(Bytes(text) + split, n - split, Crc32(Bytes(text), split)) == 0x414FA339u);
    }
}
