#include "Crypto.h"

#include <cstring>
#include <random>
#include <vector>

namespace gs::server::crypto {

namespace {

// ---------------------------------------------------------------------------
// SHA-256 (FIPS 180-4)
//
// 표준 문서의 의사코드를 그대로 옮긴 것이다.
// 성능보다 "읽고 표준과 대조할 수 있는 것" 을 우선했다.
// ---------------------------------------------------------------------------

// 처음 64개 소수의 세제곱근 소수부 앞 32비트.
constexpr uint32_t kRoundConst[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

inline uint32_t Rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

void Sha256Compress(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];

    // 앞 16워드는 입력 블록을 빅엔디안으로 읽은 것이다.
    // 프로토콜 본문(리틀엔디안 그대로 memcpy)과 달리 해시는 표준이 빅엔디안으로 못박아서 직접 조립한다.
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4    ]) << 24)
             | (static_cast<uint32_t>(block[i * 4 + 1]) << 16)
             | (static_cast<uint32_t>(block[i * 4 + 2]) <<  8)
             | (static_cast<uint32_t>(block[i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = Rotr(w[i -  2], 17) ^ Rotr(w[i -  2], 19) ^ (w[i -  2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; ++i) {
        const uint32_t s1    = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
        const uint32_t ch    = (e & f) ^ (~e & g);
        const uint32_t temp1 = h + s1 + ch + kRoundConst[i] + w[i];
        const uint32_t s0    = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
        const uint32_t maj   = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = s0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

// HMAC-SHA256 (RFC 2104). 블록 크기는 SHA-256 기준 64바이트.
void HmacSha256(const uint8_t* key, std::size_t keyLen,
                const uint8_t* data, std::size_t dataLen, uint8_t* out) {
    constexpr std::size_t kBlock = 64;

    uint8_t keyBlock[kBlock] = {};
    if (keyLen > kBlock) {
        Sha256(key, keyLen, keyBlock);   // 키가 블록보다 길면 먼저 해시해서 줄인다
    } else {
        std::memcpy(keyBlock, key, keyLen);
    }

    uint8_t inner[kBlock], outer[kBlock];
    for (std::size_t i = 0; i < kBlock; ++i) {
        inner[i] = static_cast<uint8_t>(keyBlock[i] ^ 0x36);
        outer[i] = static_cast<uint8_t>(keyBlock[i] ^ 0x5c);
    }

    // H(outer || H(inner || data))
    // Sha256 이 스트리밍 API 가 아니라 이어붙인 버퍼를 만들어 넘긴다.
    std::vector<uint8_t> innerBuf(kBlock + dataLen);
    std::memcpy(innerBuf.data(), inner, kBlock);
    if (dataLen > 0) std::memcpy(innerBuf.data() + kBlock, data, dataLen);

    uint8_t innerHash[kSha256Size];
    Sha256(innerBuf.data(), innerBuf.size(), innerHash);

    uint8_t outerBuf[kBlock + kSha256Size];
    std::memcpy(outerBuf, outer, kBlock);
    std::memcpy(outerBuf + kBlock, innerHash, kSha256Size);

    Sha256(outerBuf, sizeof(outerBuf), out);
}

} // namespace

void Sha256(const uint8_t* data, std::size_t len, uint8_t* out) {
    // 처음 8개 소수의 제곱근 소수부 앞 32비트.
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    std::size_t offset = 0;
    for (; offset + 64 <= len; offset += 64) {
        Sha256Compress(state, data + offset);
    }

    // 패딩: 남은 바이트 + 0x80 + 0 채움 + 원본 비트길이(64비트 빅엔디안).
    uint8_t tail[128] = {};
    const std::size_t remain = len - offset;
    std::memcpy(tail, data + offset, remain);
    tail[remain] = 0x80;

    // 길이 필드(8바이트)가 들어갈 자리가 남는지에 따라 블록이 1개 또는 2개가 된다.
    const std::size_t tailBytes = (remain + 1 + 8 <= 64) ? 64 : 128;
    const uint64_t bitLen = static_cast<uint64_t>(len) * 8;
    for (int i = 0; i < 8; ++i) {
        tail[tailBytes - 1 - i] = static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF);
    }

    Sha256Compress(state, tail);
    if (tailBytes == 128) Sha256Compress(state, tail + 64);

    for (int i = 0; i < 8; ++i) {
        out[i * 4    ] = static_cast<uint8_t>((state[i] >> 24) & 0xFF);
        out[i * 4 + 1] = static_cast<uint8_t>((state[i] >> 16) & 0xFF);
        out[i * 4 + 2] = static_cast<uint8_t>((state[i] >>  8) & 0xFF);
        out[i * 4 + 3] = static_cast<uint8_t>((state[i]      ) & 0xFF);
    }
}

void Pbkdf2HmacSha256(const uint8_t* password, std::size_t passwordLen,
                      const uint8_t* salt, std::size_t saltLen,
                      uint32_t iterations, uint8_t* out) {
    // dkLen == hLen 이므로 블록 인덱스는 1 하나뿐이다.
    // U1 = HMAC(P, S || INT(1)), Ui = HMAC(P, Ui-1), 결과 = U1 xor U2 xor ...
    std::vector<uint8_t> first(saltLen + 4);
    if (saltLen > 0) std::memcpy(first.data(), salt, saltLen);
    first[saltLen + 0] = 0;
    first[saltLen + 1] = 0;
    first[saltLen + 2] = 0;
    first[saltLen + 3] = 1;

    uint8_t u[kSha256Size];
    HmacSha256(password, passwordLen, first.data(), first.size(), u);

    uint8_t result[kSha256Size];
    std::memcpy(result, u, kSha256Size);

    for (uint32_t iter = 1; iter < iterations; ++iter) {
        HmacSha256(password, passwordLen, u, kSha256Size, u);
        for (std::size_t i = 0; i < kSha256Size; ++i) {
            result[i] = static_cast<uint8_t>(result[i] ^ u[i]);
        }
    }

    std::memcpy(out, result, kSha256Size);
}

void RandomBytes(uint8_t* out, std::size_t len) {
    // MSVC 의 random_device 는 OS 의 암호학적 난수를 쓴다.
    // mt19937 같은 의사난수로 솔트를 만들면 예측 가능해져서 솔트의 의미가 없다.
    static thread_local std::random_device rd;
    for (std::size_t i = 0; i < len; ++i) {
        out[i] = static_cast<uint8_t>(rd() & 0xFF);
    }
}

std::string ToHex(const uint8_t* data, std::size_t len) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.resize(len * 2);
    for (std::size_t i = 0; i < len; ++i) {
        out[i * 2    ] = digits[(data[i] >> 4) & 0xF];
        out[i * 2 + 1] = digits[data[i] & 0xF];
    }
    return out;
}

bool FromHex(const std::string& hex, uint8_t* out, std::size_t outLen) {
    if (hex.size() != outLen * 2) return false;

    auto nibble = [](char c, uint8_t& value) -> bool {
        if (c >= '0' && c <= '9') { value = static_cast<uint8_t>(c - '0');      return true; }
        if (c >= 'a' && c <= 'f') { value = static_cast<uint8_t>(c - 'a' + 10); return true; }
        if (c >= 'A' && c <= 'F') { value = static_cast<uint8_t>(c - 'A' + 10); return true; }
        return false;
    };

    for (std::size_t i = 0; i < outLen; ++i) {
        uint8_t hi = 0, lo = 0;
        if (!nibble(hex[i * 2], hi) || !nibble(hex[i * 2 + 1], lo)) return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

bool ConstantTimeEquals(const uint8_t* a, const uint8_t* b, std::size_t len) {
    // 다른 바이트를 만나도 끝까지 돈다. 걸리는 시간이 내용과 무관해야 한다.
    uint8_t diff = 0;
    for (std::size_t i = 0; i < len; ++i) {
        diff = static_cast<uint8_t>(diff | (a[i] ^ b[i]));
    }
    return diff == 0;
}

} // namespace gs::server::crypto
