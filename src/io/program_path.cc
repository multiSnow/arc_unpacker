#include "io/program_path.h"
#include "err.h"
#include "io/file_system.h"

using namespace au;

namespace
{
    static io::path program_path;
}

void io::set_program_path_from_arg(const std::string &arg)
{
    program_path = io::absolute(io::path(arg)).str();
}

io::path io::get_program_path()
{
    return program_path;
}

io::path io::get_etc_dir_path()
{
    const io::path path(program_path.parent());
    const io::path path1 = path / "etc";
    if (io::is_directory(path1))
        return path1.str();
    const io::path path2 = path.parent() / "etc";
    if (io::is_directory(path2))
        return path2;
    throw err::FileNotFoundError("Can't locate 'etc/' directory!");
}