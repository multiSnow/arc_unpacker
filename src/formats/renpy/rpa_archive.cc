// RPA archive
//
// Company:   -
// Engine:    Ren'Py
// Extension: .rpa
//
// Known games:
// - Everlasting Summer
// - Katawa Shoujo
// - Long Live The Queen

#include "formats/renpy/rpa_archive.h"
#include "io/buffered_io.h"
#include "util/zlib.h"

using namespace au;
using namespace au::fmt::renpy;

#define PICKLE_MARK            '('
#define PICKLE_STOP            '.'
#define PICKLE_POP             '0'
#define PICKLE_POP_MARK        '1'
#define PICKLE_DUP             '2'
#define PICKLE_FLOAT           'F'
#define PICKLE_INT             'I'
#define PICKLE_BININT1         'K'
#define PICKLE_BININT2         'M'
#define PICKLE_BININT4         'J'
#define PICKLE_LONG            'L'
#define PICKLE_NONE            'N'
#define PICKLE_PERSID          'P'
#define PICKLE_BINPERSID       'Q'
#define PICKLE_REDUCE          'R'
#define PICKLE_STRING          'S'
#define PICKLE_BINSTRING       'T'
#define PICKLE_SHORT_BINSTRING 'U'
#define PICKLE_UNICODE         'V'
#define PICKLE_BINUNICODE      'X'
#define PICKLE_APPEND          'a'
#define PICKLE_BUILD           'b'
#define PICKLE_GLOBAL          'c'
#define PICKLE_DICT            'd'
#define PICKLE_EMPTY_DICT      '}'
#define PICKLE_APPENDS         'e'
#define PICKLE_GET             'g'
#define PICKLE_BINGET          'h'
#define PICKLE_LONG_BINGET     'j'
#define PICKLE_INST            'i'
#define PICKLE_LIST            'l'
#define PICKLE_EMPTY_LIST      ']'
#define PICKLE_OBJ             'o'
#define PICKLE_PUT             'p'
#define PICKLE_BINPUT          'q'
#define PICKLE_LONG_BINPUT     'r'
#define PICKLE_SETITEM         's'
#define PICKLE_TUPLE           't'
#define PICKLE_EMPTY_TUPLE     ')'
#define PICKLE_SETITEMS        'u'
#define PICKLE_BINFLOAT        'G'

// Pickle protocol 2
#ifdef VISUAL_STUDIO_2015_CAME_OUT
    #define PICKLE_PROTO    '\x80'_u8
    #define PICKLE_NEWOBJ   '\x81'_u8
    #define PICKLE_EXT1     '\x82'_u8
    #define PICKLE_EXT2     '\x83'_u8
    #define PICKLE_EXT4     '\x84'_u8
    #define PICKLE_TUPLE1   '\x85'_u8
    #define PICKLE_TUPLE2   '\x86'_u8
    #define PICKLE_TUPLE3   '\x87'_u8
    #define PICKLE_NEWTRUE  '\x88'_u8
    #define PICKLE_NEWFALSE '\x89'_u8
    #define PICKLE_LONG1    '\x8a'_u8
    #define PICKLE_LONG4    '\x8b'_u8
#else
    #define PICKLE_PROTO    (u8)'\x80'
    #define PICKLE_NEWOBJ   (u8)'\x81'
    #define PICKLE_EXT1     (u8)'\x82'
    #define PICKLE_EXT2     (u8)'\x83'
    #define PICKLE_EXT4     (u8)'\x84'
    #define PICKLE_TUPLE1   (u8)'\x85'
    #define PICKLE_TUPLE2   (u8)'\x86'
    #define PICKLE_TUPLE3   (u8)'\x87'
    #define PICKLE_NEWTRUE  (u8)'\x88'
    #define PICKLE_NEWFALSE (u8)'\x89'
    #define PICKLE_LONG1    (u8)'\x8a'
    #define PICKLE_LONG4    (u8)'\x8b'
#endif

namespace
{
    typedef struct
    {
        std::vector<std::string> strings;
        std::vector<int> numbers;
    } UnpickleContext;

    typedef struct
    {
        std::string name;
        u32 offset;
        u32 size;
        std::string prefix;
        size_t prefix_size;
    } TableEntry;

    typedef std::vector<std::unique_ptr<TableEntry>> Table;
}

static void unpickle_handle_string(std::string str, UnpickleContext *context)
{
    context->strings.push_back(str);
}

static void unpickle_handle_number(size_t number, UnpickleContext *context)
{
    context->numbers.push_back(number);
}

static void unpickle(io::IO &table_io, UnpickleContext *context)
{
    // Stupid unpickle "implementation" ahead: instead of twiddling with stack,
    // arrays, dictionaries and all that crap, just remember all pushed strings
    // and integers for later interpretation.  We also take advantage of RenPy
    // using Pickle's HIGHEST_PROTOCOL, which means there's no need to parse
    // 90% of the opcodes (such as "S" with escape stuff).
    size_t table_size = table_io.size();
    while (table_io.tell() < table_size)
    {
        u8 c = table_io.read_u8();
        switch (c)
        {
            case PICKLE_SHORT_BINSTRING:
            {
                char size = table_io.read_u8();
                unpickle_handle_string(table_io.read(size), context);
                break;
            }

            case PICKLE_BINUNICODE:
            {
                u32 size = table_io.read_u32_le();
                unpickle_handle_string(table_io.read(size), context);
                break;
            }

            case PICKLE_BININT1:
            {
                unpickle_handle_number(table_io.read_u8(), context);
                break;
            }

            case PICKLE_BININT2:
            {
                unpickle_handle_number(table_io.read_u16_le(), context);
                break;
            }

            case PICKLE_BININT4:
            {
                unpickle_handle_number(table_io.read_u32_le(), context);
                break;
            }

            case PICKLE_LONG1:
            {
                size_t length = table_io.read_u8();
                u32 number = 0;
                size_t i;
                size_t pos = table_io.tell();
                for (i = 0; i < length; i++)
                {
                    table_io.seek(pos + length - 1 - i);
                    number *= 256;
                    number += table_io.read_u8();
                }
                unpickle_handle_number(number, context);
                table_io.seek(pos + length);
                break;
            }

            case PICKLE_PROTO:
                table_io.skip(1);
                break;

            case PICKLE_BINPUT:
                table_io.skip(1);
                break;

            case PICKLE_LONG_BINPUT:
                table_io.skip(4);
                break;

            case PICKLE_APPEND:
            case PICKLE_SETITEMS:
            case PICKLE_MARK:
            case PICKLE_EMPTY_LIST:
            case PICKLE_EMPTY_DICT:
            case PICKLE_TUPLE1:
            case PICKLE_TUPLE2:
            case PICKLE_TUPLE3:
                break;

            case PICKLE_STOP:
                return;

            default:
            {
                std::string msg = "Unsupported pickle operator ";
                msg += c;
                throw std::runtime_error(msg);
            }
        }
    }
}

static Table decode_table(io::IO &table_io, u32 key)
{
    UnpickleContext context;
    unpickle(table_io, &context);

    // Suspicion: reading renpy sources leaves me under impression that
    // older games might not embed prefixes at all. This means that there
    // are twice as many numbers as strings, and all prefixes should be set
    // to empty.  Since I haven't seen such games, I leave this remark only
    // as a comment.
    if (context.strings.size() % 2 != 0)
        throw std::runtime_error("Unsupported table format");
    if (context.numbers.size() != context.strings.size())
        throw std::runtime_error("Unsupported table format");

    size_t file_count = context.strings.size() / 2;
    Table entries;
    entries.reserve(file_count);

    for (size_t i = 0; i < file_count; i++)
    {
        std::unique_ptr<TableEntry> entry(new TableEntry);
        entry->name = context.strings[i * 2 ];
        entry->prefix = context.strings[i * 2 + 1];
        entry->offset = context.numbers[i * 2] ^ key;
        entry->size = context.numbers[i * 2 + 1] ^ key;
        entries.push_back(std::move(entry));
    }
    return entries;
}

static int guess_version(io::IO &arc_io)
{
    const std::string magic_3("RPA-3.0 ", 8);
    const std::string magic_2("RPA-2.0 ", 8);
    if (arc_io.read(magic_3.size()) == magic_3)
        return 3;
    arc_io.seek(0);
    if (arc_io.read(magic_2.size()) == magic_2)
        return 2;
    return -1;
}

static u32 read_hex_number(io::IO &arc_io, size_t length)
{
    size_t i;
    u32 result = 0;
    for (i = 0; i < length; i++)
    {
        char c = arc_io.read_u8();
        result *= 16;
        if (c >= 'A' && c <= 'F')
            result += c + 10 - 'A';

        else if (c >= 'a' && c <= 'f')
            result += c + 10 - 'a';

        else if (c >= '0' && c <= '9')
            result += c - '0';
    }
    return result;
}

static std::string read_raw_table(io::IO &arc_io)
{
    size_t compressed_size = arc_io.size() - arc_io.tell();
    std::string compressed = arc_io.read(compressed_size);
    std::string uncompressed = util::zlib_inflate(compressed);
    return uncompressed;
}

static std::unique_ptr<File> read_file(io::IO &arc_io, const TableEntry &entry)
{
    std::unique_ptr<File> file(new File);

    arc_io.seek(entry.offset);

    file->io.write(entry.prefix);
    file->io.write_from_io(arc_io, entry.size);

    file->name = entry.name;
    return file;
}

bool RpaArchive::is_recognized_internal(File &arc_file) const
{
    return guess_version(arc_file.io) >= 0;
}

void RpaArchive::unpack_internal(File &arc_file, FileSaver &file_saver) const
{
    int version = guess_version(arc_file.io);
    size_t table_offset = read_hex_number(arc_file.io, 16);

    u32 key;
    if (version == 3)
    {
        arc_file.io.skip(1);
        key = read_hex_number(arc_file.io, 8);
    }
    else if (version == 2)
    {
        key = 0;
    }
    else
    {
        throw std::runtime_error("Unknown RPA version");
    }

    arc_file.io.seek(table_offset);
    io::BufferedIO table_io(read_raw_table(arc_file.io));
    auto table = decode_table(table_io, key);

    for (auto &entry : table)
        file_saver.save(read_file(arc_file.io, *entry));
}
