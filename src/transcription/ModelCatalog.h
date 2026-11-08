#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace stilleto
{

/** A whisper.cpp ggml model published in the upstream repository
    (https://huggingface.co/ggerganov/whisper.cpp), with the SHA-1 listed in
    external/whisper.cpp/models/README.md. */
struct CatalogModel
{
    std::string_view id;            // e.g. "base.en"
    std::string_view displayName;   // e.g. "Base (English)"
    std::string_view description;
    std::uint64_t approximateBytes;
    std::string_view sha1;
    bool englishOnly;

    [[nodiscard]] std::string fileName() const { return "ggml-" + std::string (id) + ".bin"; }
    [[nodiscard]] std::string downloadUrl() const;
};

namespace ModelCatalog
{
    [[nodiscard]] std::span<const CatalogModel> all() noexcept;
    [[nodiscard]] const CatalogModel* findById (std::string_view id) noexcept;
    [[nodiscard]] const CatalogModel* findByFileName (std::string_view fileName) noexcept;

    /** Models larger than this ask for an explicit confirmation before downloading. */
    constexpr std::uint64_t largeModelThresholdBytes = 400ull * 1024 * 1024;

    constexpr std::string_view defaultModelId = "base.en";
}

/** Result of a cheap structural check of a model file (no inference). */
enum class ModelFileStatus
{
    Valid,
    Missing,
    Unreadable,
    TooSmall,
    BadMagic
};

/** Checks that the file exists, is readable and starts with the ggml magic used by whisper.cpp. */
[[nodiscard]] ModelFileStatus inspectModelFile (const std::filesystem::path& file);

[[nodiscard]] std::string_view describe (ModelFileStatus status) noexcept;

/** "142 MB" style formatting. */
[[nodiscard]] std::string formatBytes (std::uint64_t bytes);

} // namespace stilleto
