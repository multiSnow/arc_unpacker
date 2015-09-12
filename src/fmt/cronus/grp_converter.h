#pragma once

#include "fmt/converter.h"

namespace au {
namespace fmt {
namespace cronus {

    class GrpConverter final : public Converter
    {
    public:
        GrpConverter();
        ~GrpConverter();
    protected:
        bool is_recognized_internal(File &) const override;
        std::unique_ptr<File> decode_internal(File &) const override;
    private:
        struct Priv;
        std::unique_ptr<Priv> p;
    };

} } }