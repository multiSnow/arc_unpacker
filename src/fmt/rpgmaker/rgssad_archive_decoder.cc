#include "fmt/rpgmaker/rgssad_archive_decoder.h"
#include "fmt/rpgmaker/rgs/common.h"
#include "util/range.h"

using namespace au;
using namespace au::fmt::rpgmaker;

static const bstr magic = "RGSSAD\x00\x01"_b;
static const u32 initial_key = 0xDEADCAFE;

static rgs::Table read_table(io::IO &arc_io, u32 key)
{
    rgs::Table table;

    while (arc_io.tell() < arc_io.size())
    {
        std::unique_ptr<rgs::TableEntry> entry(new rgs::TableEntry);

        size_t name_size = arc_io.read_u32_le() ^ key;
        key = rgs::advance_key(key);
        entry->name = arc_io.read(name_size).str();
        for (auto i : util::range(name_size))
        {
            entry->name[i] ^= key;
            key = rgs::advance_key(key);
        }

        entry->size = arc_io.read_u32_le() ^ key;
        key = rgs::advance_key(key);

        entry->key = key;
        entry->offset = arc_io.tell();

        arc_io.skip(entry->size);
        table.push_back(std::move(entry));
    }
    return table;
}

bool RgssadArchiveDecoder::is_recognized_internal(File &arc_file) const
{
    return arc_file.io.read(magic.size()) == magic;
}

void RgssadArchiveDecoder::unpack_internal(
    File &arc_file, FileSaver &saver) const
{
    arc_file.io.skip(magic.size());
    auto table = read_table(arc_file.io, initial_key);
    for (auto &entry : table)
        saver.save(rgs::read_file(arc_file.io, *entry));
}

static auto dummy = fmt::Registry::add<RgssadArchiveDecoder>("rm/rgssad");