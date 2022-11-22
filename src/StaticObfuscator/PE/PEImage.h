
#include "..\Config.h"
#include "..\Include\WindowsHeaders.h"
#include <map>
#include <algorithm>
#include <utility>
#include <vector>
#include <string>
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

    enum class AddressingType {
        RVA = 0x0,
        VA = 0x1
    };

    struct ExportEntry {        
        std::string   name;
        DWORD   AddressOfFunction; // VA
        std::string forward; // empty if no
    };

    struct ImportFunction {
        std::string functionName;      
        DWORD iat;
        DWORD ordinalNum; //no zero? = imp by ord
    };

    struct ImportModuleEntry {
        auto NumberOfFuncs();
        std::string moduleName = "";       
        std::vector<ImportFunction> funcs;
    };

    struct RelocData {
        DWORD rva = 0;
        DWORD type = 0;
    };


using exports = std::vector<ExportEntry>;
using getDirectoryResult = std::tuple<std::uint64_t, std::size_t, AddressingType>;
using Imports = std::vector<ImportModuleEntry>;
using Relocs = std::vector<std::vector<RelocData>>;

class PEImage {
public:

    OBFUSCATOR_API PEImage() = default;

    OBFUSCATOR_API ~PEImage();
    
    OBFUSCATOR_API PEImage(PEImage&&) = default;

    OBFUSCATOR_API NTSTATUS Load(const std::string_view path);

    OBFUSCATOR_API exports GetExports();

    OBFUSCATOR_API Imports GetImports();

    OBFUSCATOR_API Relocs  GetRelocs();

private:
    OBFUSCATOR_API NTSTATUS ParsePE();
    OBFUSCATOR_API getDirectoryResult
        GetDataDirectoryEntry(std::size_t, AddressingType);
    OBFUSCATOR_API void ParseExport();
    OBFUSCATOR_API void ParseImport();
    OBFUSCATOR_API void ParseRelocs();

private:
    bool _loadAsImage = false;
    IMAGE_NT_HEADERS32* _ntHeaders32 = nullptr;
    IMAGE_NT_HEADERS64* _ntHeaders64 = nullptr;
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
    exports _exports;
    Imports _imports;
    Relocs _relocs;
};


}
}