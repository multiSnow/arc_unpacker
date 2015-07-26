#ifndef AU_FMT_KIRIKIRI_TLG_TLG5_DECODER_H
#define AU_FMT_KIRIKIRI_TLG_TLG5_DECODER_H
#include "file.h"

namespace au {
namespace fmt {
namespace kirikiri {
namespace tlg {

    class Tlg5Decoder final
    {
    public:
        std::unique_ptr<File> decode(File &file);
    };

} } } }

#endif
