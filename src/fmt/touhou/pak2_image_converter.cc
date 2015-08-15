// PAK2 image file
//
// Company:   Team Shanghai Alice
// Engine:    -
// Extension: .cv2
//
// Known games:
// - Touhou 10.5 - Scarlet Weather Rhapsody
// - Touhou 12.3 - Unthinkable Natural Law

#include <boost/filesystem.hpp>
#include "fmt/touhou/pak2_image_converter.h"
#include "io/buffered_io.h"
#include "util/colors.h"
#include "util/format.h"
#include "util/image.h"
#include "util/range.h"

using namespace au;
using namespace au::fmt::touhou;

struct Pak2ImageConverter::Priv
{
    PaletteMap palette_map;
};

Pak2ImageConverter::Pak2ImageConverter() : p(new Priv)
{
}

Pak2ImageConverter::~Pak2ImageConverter()
{
}

void Pak2ImageConverter::set_palette_map(const PaletteMap &palette_map)
{
    p->palette_map.clear();
    for (auto &it : palette_map)
        p->palette_map[it.first] = it.second;
}

bool Pak2ImageConverter::is_recognized_internal(File &file) const
{
    return file.has_extension("cv2");
}

std::unique_ptr<File> Pak2ImageConverter::decode_internal(File &file) const
{
    auto bit_depth = file.io.read_u8();
    auto width = file.io.read_u32_le();
    auto height = file.io.read_u32_le();
    auto stride = file.io.read_u32_le();
    auto palette_number = file.io.read_u32_le();

    io::BufferedIO source_io(file.io);

    Palette palette;
    if (bit_depth == 8)
    {
        auto path = boost::filesystem::path(file.name);
        path.remove_filename();
        path /= util::format("palette%03d.pal", palette_number);

        auto it = p->palette_map.find(path.generic_string());
        palette = it != p->palette_map.end()
            ? it->second
            : create_default_palette();
    }

    std::vector<util::Color> pixels(width * height);
    auto *pixels_ptr = &pixels[0];
    for (size_t y : util::range(height))
    {
        for (size_t x : util::range(stride))
        {
            util::Color color;

            switch (bit_depth)
            {
                case 32:
                case 24:
                    color = util::color::bgra8888(source_io);
                    break;

                case 8:
                    color = palette[source_io.read_u8()];
                    break;

                default:
                    throw std::runtime_error("Unsupported channel count");
            }

            if (x < width)
                *pixels_ptr++ = color;
        }
    }

    auto image = util::Image::from_pixels(width, height, pixels);
    return image->create_file(file.name);
}
