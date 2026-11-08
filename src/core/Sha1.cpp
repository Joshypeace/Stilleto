#include "core/Sha1.h"

#include <cstring>
#include <fstream>
#include <vector>

namespace stilleto
{
namespace
{
constexpr std::uint32_t rotl (std::uint32_t v, int bits) noexcept { return (v << bits) | (v >> (32 - bits)); }
}

void Sha1::reset()
{
    state = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u };
    totalBytes = 0;
    bufferLength = 0;
}

void Sha1::processBlock (const std::uint8_t* block)
{
    std::uint32_t w[80];
    for (int i = 0; i < 16; ++i)
        w[i] = (std::uint32_t (block[i * 4]) << 24) | (std::uint32_t (block[i * 4 + 1]) << 16)
             | (std::uint32_t (block[i * 4 + 2]) << 8) | std::uint32_t (block[i * 4 + 3]);
    for (int i = 16; i < 80; ++i)
        w[i] = rotl (w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    auto [a, b, c, d, e] = state;

    for (int i = 0; i < 80; ++i)
    {
        std::uint32_t f, k;
        if (i < 20)      { f = (b & c) | (~b & d);           k = 0x5A827999u; }
        else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d);  k = 0x8F1BBCDCu; }
        else             { f = b ^ c ^ d;                    k = 0xCA62C1D6u; }

        const std::uint32_t t = rotl (a, 5) + f + e + k + w[i];
        e = d; d = c; c = rotl (b, 30); b = a; a = t;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

void Sha1::update (const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*> (data);
    totalBytes += size;

    while (size > 0)
    {
        const std::size_t take = std::min (size, buffer.size() - bufferLength);
        std::memcpy (buffer.data() + bufferLength, bytes, take);
        bufferLength += take;
        bytes += take;
        size -= take;

        if (bufferLength == buffer.size())
        {
            processBlock (buffer.data());
            bufferLength = 0;
        }
    }
}

std::array<std::uint8_t, 20> Sha1::finish()
{
    const std::uint64_t bitLength = totalBytes * 8;
    const std::uint8_t pad = 0x80;
    update (&pad, 1);

    const std::uint8_t zero = 0;
    while (bufferLength != 56)
        update (&zero, 1);

    std::uint8_t lengthBytes[8];
    for (int i = 0; i < 8; ++i)
        lengthBytes[i] = static_cast<std::uint8_t> (bitLength >> (56 - 8 * i));
    update (lengthBytes, 8);

    std::array<std::uint8_t, 20> digest {};
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 4; ++j)
            digest[static_cast<std::size_t> (i * 4 + j)] = static_cast<std::uint8_t> (state[static_cast<std::size_t> (i)] >> (24 - 8 * j));

    reset();
    return digest;
}

std::string Sha1::toHex (const std::array<std::uint8_t, 20>& digest)
{
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve (40);
    for (const auto b : digest)
    {
        out += hex[b >> 4];
        out += hex[b & 0x0F];
    }
    return out;
}

std::string Sha1::ofString (std::string_view text)
{
    Sha1 sha;
    sha.update (text.data(), text.size());
    return toHex (sha.finish());
}

std::optional<std::string> Sha1::ofFile (const std::filesystem::path& file, const std::function<bool()>& shouldContinue)
{
    std::ifstream in (file, std::ios::binary);
    if (! in) return std::nullopt;

    Sha1 sha;
    std::vector<char> chunk (1 << 20);

    while (in)
    {
        in.read (chunk.data(), static_cast<std::streamsize> (chunk.size()));
        const auto got = in.gcount();
        if (got > 0) sha.update (chunk.data(), static_cast<std::size_t> (got));
        if (shouldContinue && ! shouldContinue()) return std::nullopt;
    }

    return toHex (sha.finish());
}

} // namespace stilleto
