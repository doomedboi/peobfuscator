
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

    /// <summary>
    /// Load PE by specified path
    /// </summary>
    /// <returns>Status code true on succes</returns>
    OBFUSCATOR_API NTSTATUS Load(const std::string_view path);
    
    /// <summary>
    /// Obtain pe's export table
    /// </summary>
    /// <returns>Export table</returns>
    OBFUSCATOR_API exports GetExports();

    /// <summary>
    /// Obtain pe's imports
    /// </summary>
    /// <returns>Import table</returns>
    OBFUSCATOR_API Imports GetImports();

    /// <summary>
    /// Obtain pe's relocs
    /// </summary>
    /// <returns> Relocs table</returns>
    OBFUSCATOR_API Relocs  GetRelocs();

private:
    /// <summary>
    /// Parse internal pe's headers
    /// <returns> NT_STATUS SUCCES if OK<\returns>
    /// </summary>
    OBFUSCATOR_API NTSTATUS ParsePE();
    
    /// <summary>
    /// Get Data_Directory by index
    /// <returns>Directory result info otherwise empty tuple<\returns>
    /// </summary>
    OBFUSCATOR_API getDirectoryResult
        GetDataDirectoryEntry(std::size_t, AddressingType);
    
    /// <summary>
    /// Parse export sections
    /// <result>_exports internal field<\result>
    /// </summary>
    OBFUSCATOR_API void ParseExport();
    
    /// <summary>
    /// Parse pe's Imports
    /// <result>_imports internal field<\result>
    /// </summary>
    OBFUSCATOR_API void ParseImport();

    /// <summary>
    /// Obtain pe's relocs if has
    /// <result>_relocs internal field if hasn't - empty container<\result>
    /// </summary>
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