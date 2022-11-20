
#include "..\Config.h"
#include "..\Include\WindowsHeaders.h"
#include <filesystem>

namespace obfuscator {

namespace pe {


    enum class Arch {
        UNKNOWN_ARCH = 0x0,
        x32 = 0x1,
        x64,        
    };

    enum class Subsystem {
        UNKNOWN = 0x0,
        native = 0x1, // driver can has !native 
        gui,
        cui //terminal
    };

    enum class FileType {
        UNKNOWN = 0x0,
        exe, // exe == sys
        dll
    };

class PEImage {
public:

    OBFUSCATOR_API PEImage() = default;
    OBFUSCATOR_API ~PEImage() = default;
    OBFUSCATOR_API PEImage(PEImage&&) = default;

    OBFUSCATOR_API NTSTATUS Load(const std::string_view path);

private:
    OBFUSCATOR_API NTSTATUS ParsePE();

private:
    bool _loadAsImage = false;
    LPVOID _imageView = nullptr;
    DWORD _numberOfSection = 0;
    
    Arch _architecture = Arch::UNKNOWN_ARCH;
    std::uint64_t _entryPoint = 0;
    std::uint64_t _imgBase = 0;
    DWORD _sectionAlignment = 0;
    std::int64_t _sizeOfImage = 0;
    std::int64_t _sizeOfHeaders = 0;
    Subsystem _subsystem = Subsystem::UNKNOWN;
    FileType _fileType = FileType::UNKNOWN;
    std::vector<IMAGE_SECTION_HEADER> _sections;
};


}
}