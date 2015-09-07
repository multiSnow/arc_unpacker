// PAK archive
//
// Company:   Cronus
// Engine:    -
// Extension: .pak
//
// Known games:
// - Doki Doki Princess
// - Sweet Pleasure

#include "fmt/cronus/common.h"
#include "fmt/cronus/pak_archive.h"
#include "fmt/cronus/grp_converter.h"
#include "io/buffered_io.h"
#include "util/pack/lzss.h"
#include "util/plugin_mgr.hh"
#include "util/range.h"

using namespace au;
using namespace au::fmt::cronus;

static const bstr magic = "CHERRY PACK 2.0\x00"_b;

namespace
{
    struct Plugin
    {
        u32 key1;
        u32 key2;
    };

    struct TableEntry
    {
        std::string name;
        size_t offset;
        size_t size;
    };

    using Table = std::vector<std::unique_ptr<TableEntry>>;
}

static Table read_table(io::IO &arc_io, const Plugin &plugin, bool encrypted)
{
    auto file_count = arc_io.read_u32_le() ^ plugin.key1;
    auto file_data_start = arc_io.read_u32_le() ^ plugin.key2;

    auto table_size_orig = file_count * 24;
    auto table_size_comp = file_data_start - arc_io.tell();
    auto table_data = arc_io.read(table_size_comp);
    if (encrypted)
    {
        auto key = get_delta_key("CHERRYSOFT"_b);
        delta_decrypt(table_data, key);
        table_data = util::pack::lzss_decompress_bytewise(
            table_data, table_size_orig);
    }
    io::BufferedIO table_io(table_data);

    Table table;
    for (auto i : util::range(file_count))
    {
        std::unique_ptr<TableEntry> entry(new TableEntry);
        entry->name = table_io.read_to_zero(16).str();
        entry->offset = table_io.read_u32_le() + file_data_start;
        entry->size = table_io.read_u32_le();
        table.push_back(std::move(entry));
    }
    return table;
}

static Table guess_plugin_and_read_table(
    io::IO &arc_io, const std::vector<Plugin> &plugins, bool encrypted)
{
    auto pos = arc_io.tell();
    for (auto &plugin : plugins)
    {
        arc_io.seek(pos);
        try
        {
            auto table = read_table(arc_io, plugin, encrypted);
            if (!table.size())
                continue;
            auto &last_entry = table.at(table.size() - 1);
            if (last_entry->offset + last_entry->size == arc_io.size())
                return table;
        }
        catch (std::exception &e)
        {
            continue;
        }
    }
    throw std::runtime_error("Unknown encryption scheme");
}

static std::unique_ptr<File> read_file(io::IO &arc_io, const TableEntry &entry)
{
    std::unique_ptr<File> file(new File);
    arc_io.seek(entry.offset);
    file->io.write_from_io(arc_io, entry.size);
    file->name = entry.name;
    return file;
}

struct PakArchive::Priv
{
    GrpConverter grp_converter;
    util::PluginManager<Plugin> plugin_mgr;
};

PakArchive::PakArchive() : p(new Priv)
{
    p->plugin_mgr.add(
        "dokidoki", "Doki Doki Princess",
        {0x00000000, 0x00000000});

    p->plugin_mgr.add(
        "sweet", "Sweet Pleasure",
        {0xBC138744, 0x64E0BA23});

    add_transformer(&p->grp_converter);
}

PakArchive::~PakArchive()
{
}

bool PakArchive::is_recognized_internal(File &arc_file) const
{
    return arc_file.io.read(magic.size()) == magic;
}

void PakArchive::unpack_internal(File &arc_file, FileSaver &file_saver) const
{
    arc_file.io.skip(magic.size());
    bool encrypted = arc_file.io.read_u32_le() > 0;
    Table table = guess_plugin_and_read_table(
        arc_file.io, p->plugin_mgr.get_all(), encrypted);
    for (auto &entry : table)
    {
        auto file = read_file(arc_file.io, *entry);
        file->guess_extension();
        file_saver.save(std::move(file));
    }
}

static auto dummy = fmt::Registry::add<PakArchive>("cronus/pak");
