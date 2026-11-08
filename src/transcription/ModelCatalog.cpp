#include "transcription/ModelCatalog.h"

#include <array>
#include <cstdio>
#include <fstream>

namespace stilleto
{
namespace
{
constexpr std::uint64_t MiB = 1024ull * 1024ull;

constexpr std::array catalog {
    CatalogModel { "tiny.en",  "Tiny (English)",  "Fastest. Good for short notes on modest hardware.",       75 * MiB,   "c78c86eb1a8faa21b369bcd33207cc90d64ae9df", true },
    CatalogModel { "base.en",  "Base (English)",  "Recommended. Accurate everyday dictation, fast on CPU.",  142 * MiB,  "137c40403d78fd54d454da0f9bd998f78703390c", true },
    CatalogModel { "small.en", "Small (English)", "Higher accuracy for names and jargon. Slower on CPU.",    466 * MiB,  "db8a495a91d927739e50b3fc1cc4c6b8f6c2d022", true },
    CatalogModel { "tiny",     "Tiny (Multilingual)",  "Fastest multilingual model.",                       75 * MiB,   "bd577a113a864445d4c299885e0cb97d4ba92b5f", false },
    CatalogModel { "base",     "Base (Multilingual)",  "Balanced multilingual dictation.",                  142 * MiB,  "465707469ff3a37a2b9b8d8f89f2f99de7299dac", false },
    CatalogModel { "small",    "Small (Multilingual)", "Better multilingual accuracy. Slower on CPU.",      466 * MiB,  "55356645c2b361a969dfd0ef2c5a50d530afd8d5", false },
    CatalogModel { "large-v3-turbo-q5_0", "Large v3 Turbo (Quantised)", "Best accuracy. Needs a fast CPU or GPU.", 547 * MiB, "e050f7970618a659205450ad97eb95a18d69c9ee", false },
};
} // namespace

std::string CatalogModel::downloadUrl() const
{
    return "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/" + fileName();
}

std::span<const CatalogModel> ModelCatalog::all() noexcept { return catalog; }

const CatalogModel* ModelCatalog::findById (std::string_view id) noexcept
{
    for (const auto& m : catalog)
        if (m.id == id)
            return &m;
    return nullptr;
}

const CatalogModel* ModelCatalog::findByFileName (std::string_view fileName) noexcept
{
    for (const auto& m : catalog)
        if (m.fileName() == fileName)
            return &m;
    return nullptr;
}

ModelFileStatus inspectModelFile (const std::filesystem::path& file)
{
    std::error_code ec;

    if (! std::filesystem::is_regular_file (file, ec))
        return ModelFileStatus::Missing;

    const auto size = std::filesystem::file_size (file, ec);
    if (ec) return ModelFileStatus::Unreadable;
    if (size < 1024 * 1024) return ModelFileStatus::TooSmall;   // real models are tens of MB

    std::ifstream in (file, std::ios::binary);
    if (! in) return ModelFileStatus::Unreadable;

    std::uint32_t magic = 0;
    in.read (reinterpret_cast<char*> (&magic), sizeof (magic));
    if (! in) return ModelFileStatus::Unreadable;

    // whisper.cpp's loader checks GGML_FILE_MAGIC ('ggml' as a little-endian uint32).
    constexpr std::uint32_t ggmlMagic = 0x67676d6c;
    return magic == ggmlMagic ? ModelFileStatus::Valid : ModelFileStatus::BadMagic;
}

std::string_view describe (ModelFileStatus status) noexcept
{
    switch (status)
    {
        case ModelFileStatus::Valid:      return "Valid model file";
        case ModelFileStatus::Missing:    return "Model file not found";
        case ModelFileStatus::Unreadable: return "Model file could not be read";
        case ModelFileStatus::TooSmall:   return "Model file is incomplete or empty";
        case ModelFileStatus::BadMagic:   return "Not a whisper.cpp (ggml) model file";
    }
    return "Unknown";
}

std::string formatBytes (std::uint64_t bytes)
{
    char buffer[32];
    const double mb = static_cast<double> (bytes) / (1024.0 * 1024.0);

    if (mb >= 1024.0)
        std::snprintf (buffer, sizeof (buffer), "%.1f GB", mb / 1024.0);
    else if (mb >= 1.0)
        std::snprintf (buffer, sizeof (buffer), "%.0f MB", mb);
    else
        std::snprintf (buffer, sizeof (buffer), "%.0f KB", static_cast<double> (bytes) / 1024.0);

    return buffer;
}

} // namespace stilleto
