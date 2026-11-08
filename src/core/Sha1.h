#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace stilleto
{

/** SHA-1, used only to verify downloaded model files against the checksums published by
    whisper.cpp (not for any security decision). */
class Sha1
{
public:
    Sha1() { reset(); }

    void reset();
    void update (const void* data, std::size_t size);
    [[nodiscard]] std::array<std::uint8_t, 20> finish();

    [[nodiscard]] static std::string toHex (const std::array<std::uint8_t, 20>& digest);
    [[nodiscard]] static std::string ofString (std::string_view text);

    /** Hashes a file in chunks. shouldContinue (optional) is polled between chunks; returning false
        aborts and yields nullopt. */
    [[nodiscard]] static std::optional<std::string> ofFile (const std::filesystem::path& file,
                                                            const std::function<bool()>& shouldContinue = {});

private:
    void processBlock (const std::uint8_t* block);

    std::array<std::uint32_t, 5> state {};
    std::array<std::uint8_t, 64> buffer {};
    std::uint64_t totalBytes = 0;
    std::size_t bufferLength = 0;
};

} // namespace stilleto
