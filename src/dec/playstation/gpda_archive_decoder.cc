#include "dec/playstation/gpda_archive_decoder.h"
#include "algo/range.h"
#include "err.h"

using namespace au;
using namespace au::dec::playstation;

static const bstr magic = "GPDA"_b;

namespace
{
    struct ArchiveEntryImpl final : dec::ArchiveEntry
    {
        size_t offset;
        size_t size;
    };
}

bool GpdaArchiveDecoder::is_recognized_impl(io::File &input_file) const
{
    input_file.stream.seek(0);
    return input_file.stream.read(magic.size()) == magic
        && input_file.stream.read_u32_le() == input_file.stream.size();
}

std::unique_ptr<dec::ArchiveMeta> GpdaArchiveDecoder::read_meta_impl(
    const Logger &logger, io::File &input_file) const
{
    input_file.stream.seek(12);
    const auto file_count = input_file.stream.read_u32_le();
    auto meta = std::make_unique<ArchiveMeta>();
    for (const auto i : algo::range(file_count))
    {
        auto entry = std::make_unique<ArchiveEntryImpl>();

        entry->offset = input_file.stream.read_u32_le();
        if (input_file.stream.read_u32_le() != 0)
            throw err::CorruptDataError("Expected '0'");
        entry->size = input_file.stream.read_u32_le();

        const auto name_offset = input_file.stream.read_u32_le();
        if (!name_offset)
            entry->path = "";
        else
            input_file.stream.peek(name_offset, [&]()
                {
                    const auto name_size = input_file.stream.read_u32_le();
                    entry->path = input_file.stream.read(name_size).str();
                });

        if (!entry->size)
            continue;
        meta->entries.push_back(std::move(entry));
    }
    return meta;
}

std::unique_ptr<io::File> GpdaArchiveDecoder::read_file_impl(
    const Logger &logger,
    io::File &input_file,
    const dec::ArchiveMeta &m,
    const dec::ArchiveEntry &e) const
{
    const auto entry = static_cast<const ArchiveEntryImpl*>(&e);
    const auto data = input_file.stream.seek(entry->offset).read(entry->size);
    return std::make_unique<io::File>(entry->path, data);
}

std::vector<std::string> GpdaArchiveDecoder::get_linked_formats() const
{
    return {"playstation/gpda", "playstation/gim", "cri/hca", "gnu/gzip"};
}

static auto _ = dec::register_decoder<GpdaArchiveDecoder>("playstation/gpda");