#pragma once

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace eu_digital::digest {

inline std::uint32_t rotate_left(std::uint32_t value, std::uint32_t bits) {
    return (value << bits) | (value >> (32U - bits));
}

inline std::array<std::uint8_t, 20> sha1(std::string_view input) {
    std::vector<std::uint8_t> bytes(input.begin(), input.end());
    bytes.push_back(0x80);
    while ((bytes.size() % 64U) != 56U) bytes.push_back(0);
    const std::uint64_t bit_length = static_cast<std::uint64_t>(input.size()) * 8U;
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<std::uint8_t>((bit_length >> shift) & 0xffU));
    }

    std::uint32_t h0 = 0x67452301U;
    std::uint32_t h1 = 0xefcdab89U;
    std::uint32_t h2 = 0x98badcfeU;
    std::uint32_t h3 = 0x10325476U;
    std::uint32_t h4 = 0xc3d2e1f0U;
    for (std::size_t chunk = 0; chunk < bytes.size(); chunk += 64U) {
        std::array<std::uint32_t, 80> words{};
        for (std::size_t index = 0; index < 16U; ++index) {
            const auto offset = chunk + index * 4U;
            words[index] = (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
                (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
                (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
                static_cast<std::uint32_t>(bytes[offset + 3U]);
        }
        for (std::size_t index = 16U; index < 80U; ++index) {
            words[index] = rotate_left(words[index - 3U] ^ words[index - 8U] ^
                                       words[index - 14U] ^ words[index - 16U], 1U);
        }
        std::uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (std::size_t index = 0; index < 80U; ++index) {
            std::uint32_t function = 0;
            std::uint32_t constant = 0;
            if (index < 20U) {
                function = (b & c) | ((~b) & d);
                constant = 0x5a827999U;
            } else if (index < 40U) {
                function = b ^ c ^ d;
                constant = 0x6ed9eba1U;
            } else if (index < 60U) {
                function = (b & c) | (b & d) | (c & d);
                constant = 0x8f1bbcdcU;
            } else {
                function = b ^ c ^ d;
                constant = 0xca62c1d6U;
            }
            const auto temporary = rotate_left(a, 5U) + function + e + constant + words[index];
            e = d;
            d = c;
            c = rotate_left(b, 30U);
            b = a;
            a = temporary;
        }
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }
    return {
        static_cast<std::uint8_t>(h0 >> 24U), static_cast<std::uint8_t>(h0 >> 16U),
        static_cast<std::uint8_t>(h0 >> 8U), static_cast<std::uint8_t>(h0),
        static_cast<std::uint8_t>(h1 >> 24U), static_cast<std::uint8_t>(h1 >> 16U),
        static_cast<std::uint8_t>(h1 >> 8U), static_cast<std::uint8_t>(h1),
        static_cast<std::uint8_t>(h2 >> 24U), static_cast<std::uint8_t>(h2 >> 16U),
        static_cast<std::uint8_t>(h2 >> 8U), static_cast<std::uint8_t>(h2),
        static_cast<std::uint8_t>(h3 >> 24U), static_cast<std::uint8_t>(h3 >> 16U),
        static_cast<std::uint8_t>(h3 >> 8U), static_cast<std::uint8_t>(h3),
        static_cast<std::uint8_t>(h4 >> 24U), static_cast<std::uint8_t>(h4 >> 16U),
        static_cast<std::uint8_t>(h4 >> 8U), static_cast<std::uint8_t>(h4)};
}

inline std::array<std::uint8_t, 32> sha256(std::string_view input) {
    static constexpr std::array<std::uint32_t, 64> constants{
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
        0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
        0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
        0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
        0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
        0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
        0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
        0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};
    const auto rotr = [](std::uint32_t value, std::uint32_t bits) {
        return (value >> bits) | (value << (32U - bits));
    };
    std::vector<std::uint8_t> bytes(input.begin(), input.end());
    bytes.push_back(0x80);
    while ((bytes.size() % 64U) != 56U) bytes.push_back(0);
    const std::uint64_t bit_length = static_cast<std::uint64_t>(input.size()) * 8U;
    for (int shift = 56; shift >= 0; shift -= 8) bytes.push_back(static_cast<std::uint8_t>((bit_length >> shift) & 0xffU));
    std::array<std::uint32_t, 8> state{
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    for (std::size_t chunk = 0; chunk < bytes.size(); chunk += 64U) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16U; ++index) {
            const auto offset = chunk + index * 4U;
            words[index] = (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
                (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
                (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
                static_cast<std::uint32_t>(bytes[offset + 3U]);
        }
        for (std::size_t index = 16U; index < 64U; ++index) {
            const auto small_sigma0 = rotr(words[index - 15U], 7U) ^ rotr(words[index - 15U], 18U) ^ (words[index - 15U] >> 3U);
            const auto small_sigma1 = rotr(words[index - 2U], 17U) ^ rotr(words[index - 2U], 19U) ^ (words[index - 2U] >> 10U);
            words[index] = words[index - 16U] + small_sigma0 + words[index - 7U] + small_sigma1;
        }
        auto working = state;
        for (std::size_t index = 0; index < 64U; ++index) {
            const auto big_sigma1 = rotr(working[4], 6U) ^ rotr(working[4], 11U) ^ rotr(working[4], 25U);
            const auto choice = (working[4] & working[5]) ^ ((~working[4]) & working[6]);
            const auto temporary1 = working[7] + big_sigma1 + choice + constants[index] + words[index];
            const auto big_sigma0 = rotr(working[0], 2U) ^ rotr(working[0], 13U) ^ rotr(working[0], 22U);
            const auto majority = (working[0] & working[1]) ^ (working[0] & working[2]) ^ (working[1] & working[2]);
            const auto temporary2 = big_sigma0 + majority;
            working[7] = working[6]; working[6] = working[5]; working[5] = working[4];
            working[4] = working[3] + temporary1;
            working[3] = working[2]; working[2] = working[1]; working[1] = working[0];
            working[0] = temporary1 + temporary2;
        }
        for (std::size_t index = 0; index < 8U; ++index) state[index] += working[index];
    }
    std::array<std::uint8_t, 32> result{};
    for (std::size_t index = 0; index < 8U; ++index) {
        result[index * 4U] = static_cast<std::uint8_t>(state[index] >> 24U);
        result[index * 4U + 1U] = static_cast<std::uint8_t>(state[index] >> 16U);
        result[index * 4U + 2U] = static_cast<std::uint8_t>(state[index] >> 8U);
        result[index * 4U + 3U] = static_cast<std::uint8_t>(state[index]);
    }
    return result;
}

template <std::size_t N>
inline std::string hex(const std::array<std::uint8_t, N>& digest) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : digest) output << std::setw(2) << static_cast<unsigned int>(byte);
    return output.str();
}

inline std::array<std::uint8_t, 16> parse_uuid(std::string_view value) {
    std::array<std::uint8_t, 16> result{};
    std::size_t output = 0;
    for (const char character : value) {
        if (character == '-') continue;
        if (output >= result.size() * 2U) throw std::invalid_argument("invalid UUID");
        const auto nibble = [](char value) -> std::uint8_t {
            if (value >= '0' && value <= '9') return static_cast<std::uint8_t>(value - '0');
            if (value >= 'a' && value <= 'f') return static_cast<std::uint8_t>(value - 'a' + 10);
            if (value >= 'A' && value <= 'F') return static_cast<std::uint8_t>(value - 'A' + 10);
            throw std::invalid_argument("invalid UUID hex");
        };
        if ((output % 2U) == 0) result[output / 2U] = static_cast<std::uint8_t>(nibble(character) << 4U);
        else result[output / 2U] |= nibble(character);
        ++output;
    }
    if (output != 32U) throw std::invalid_argument("invalid UUID length");
    return result;
}

inline std::string uuid5(std::string_view namespace_uuid, std::string_view name) {
    const auto namespace_bytes = parse_uuid(namespace_uuid);
    std::string input(reinterpret_cast<const char*>(namespace_bytes.data()), namespace_bytes.size());
    input.append(name);
    auto digest = sha1(input);
    digest[6] = static_cast<std::uint8_t>((digest[6] & 0x0fU) | 0x50U);
    digest[8] = static_cast<std::uint8_t>((digest[8] & 0x3fU) | 0x80U);
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < 16U; ++index) {
        if (index == 4U || index == 6U || index == 8U || index == 10U) output << '-';
        output << std::setw(2) << static_cast<unsigned int>(digest[index]);
    }
    return output.str();
}

}  // namespace eu_digital::digest
