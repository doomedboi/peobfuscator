#include "PEImage.h"

namespace obfuscator {

namespace pe {
OBFUSCATOR_API NTSTATUS PEImage::Load(std::string_view path)
{
    if (std::filesystem::exists(std::filesystem::path(
        std::move(path))) != TRUE)
        return STATUS_NO_SUCH_FILE;

    auto handle = CreateFileA(path.data(), FILE_GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, 0, NULL
    );

    if (handle == INVALID_HANDLE_VALUE)
        return STATUS_OPEN_FAILED;

    // try to create map file as a image 
    // in the mem like it's happening in real life
    auto hMapView = CreateFileMappingA(handle, NULL,
        SEC_IMAGE | PAGE_READONLY, 0, 0, NULL);

    if (hMapView != INVALID_HANDLE_VALUE)
        _loadAsImage = true;
    else {
        hMapView = CreateFileMappingA(handle, NULL,
            PAGE_READONLY, 0, 0, NULL);
        _loadAsImage = false;
    }

    auto imageView = MapViewOfFile(hMapView, FILE_MAP_READ, 0, 0, NULL);

    if (imageView == nullptr) {
        CloseHandle(hMapView);
        CloseHandle(handle);
        
        return GetLastError();
    }

    _imageView = imageView;

    /*now it's time to parse the image*/
    auto status = ParsePE();
    if (!NT_SUCCESS(status))
        return status;
    return STATUS_SUCCESS;
}

OBFUSCATOR_API NTSTATUS PEImage::ParsePE()
{
    auto* dosHeader = reinterpret_cast
        <IMAGE_DOS_HEADER*>(_imageView);

    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
        return STATUS_INVALID_IMAGE_FORMAT;

    _ntHeaders32 = reinterpret_cast<IMAGE_NT_HEADERS32*>(
        reinterpret_cast<BYTE*>(dosHeader) + dosHeader->e_lfanew
        );
    _ntHeaders64 = reinterpret_cast<IMAGE_NT_HEADERS64*>(
        reinterpret_cast<BYTE*>(dosHeader) + dosHeader->e_lfanew
        );

    // check whether valid PE or not
    if (_ntHeaders32->Signature != IMAGE_NT_SIGNATURE)
        return STATUS_INVALID_SIGNATURE;

    

    auto parseHeaders = [this] (auto peHeader) {
        _numberOfSection = peHeader->FileHeader.NumberOfSections;

        if (peHeader->OptionalHeader.Magic == 0x20B)
            _architecture = Arch::x64;
        else if (peHeader->OptionalHeader.Magic == 0x10B)
            _architecture = Arch::x32;
        else
            _architecture = Arch::UNKNOWN_ARCH;

        _entryPoint = peHeader->OptionalHeader.AddressOfEntryPoint;
        _imgBase = peHeader->OptionalHeader.ImageBase;
        _sectionAlignment = peHeader->OptionalHeader.SectionAlignment;
        _sizeOfImage = peHeader->OptionalHeader.SizeOfImage;
        _sizeOfHeaders = peHeader->OptionalHeader.SizeOfHeaders;
        _subsystem = Subsystem(peHeader->OptionalHeader.Subsystem);

        if (peHeader->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE)
            _fileType = FileType::exe;
        else if (peHeader->FileHeader.Characteristics & IMAGE_FILE_DLL)
            _fileType = FileType::dll;
    };
    
    auto checkArch = [](IMAGE_NT_HEADERS32* imgHeaders) {
        return Arch(imgHeaders->OptionalHeader.Magic);
    };

    if (auto arch = checkArch(_ntHeaders32); arch == Arch::x64)
        parseHeaders(_ntHeaders64);
    else
        parseHeaders(_ntHeaders32);

    
    auto parseSections = [this]() {
        auto firstSection = reinterpret_cast<IMAGE_SECTION_HEADER*>(
            _ntHeaders32 + 1
            );
        for (int i = 0; i < _numberOfSection; ++i, ++firstSection)
            _sections.push_back(*firstSection);
    };

    parseSections();

    return STATUS_SUCCESS;
}

OBFUSCATOR_API getDirectoryResult
    PEImage::GetDataDirectoryEntry(std::size_t index, AddressingType mode)
{
    if (index < 0 || index >= 16)
        return { 0LL, 0, AddressingType(mode) };

    auto dataDirectories = _architecture == Arch::x64 ?
        _ntHeaders64->OptionalHeader.DataDirectory :
        _ntHeaders32->OptionalHeader.DataDirectory;

    auto resultAddress = dataDirectories[index].VirtualAddress;
    if (mode == AddressingType::VA)
        resultAddress += DWORD(_imageView);
    
    return { resultAddress, dataDirectories[index].Size, mode };    
}


OBFUSCATOR_API void PEImage::ParseExport()
{
    /*get DATA_DIRECTORY_TABLE w/ 0 index(export)*/
    auto [addrDataDirectory, exportTableSize, mode]
        = (GetDataDirectoryEntry(0, AddressingType::VA));
        
    if (addrDataDirectory == 0)
        return;

    auto exportTable = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(
        addrDataDirectory
    );
    
    auto imageView = (uintptr_t)_imageView;
    auto* vaFunctions = (DWORD*)(exportTable->AddressOfFunctions + imageView);
    auto* vaFuncsNames = (DWORD*)(exportTable->AddressOfNames + imageView);
    auto* vaOrdinals = (WORD*)(exportTable->AddressOfNameOrdinals + imageView);
    
    auto numOfFuncs = exportTable->NumberOfFunctions;
    auto numOfNames = exportTable->NumberOfNames;

    for (int i = 0; i < std::max<>(numOfFuncs, numOfNames); ++i) {
            
        DWORD funcAddr = NULL;
        std::string name;
        if (i < numOfNames) {
            name = (char*)(vaFuncsNames[i] + imageView);
            funcAddr = vaFunctions[vaOrdinals[i]];
        }
        else
            name = "";

        if (funcAddr == 0)
            continue;
        
        const char* forward = nullptr;
        /*check for forwording functions*/
        // hint: forwarding funcs always do not
        // beyond of the export table
        if (funcAddr > addrDataDirectory && funcAddr < (addrDataDirectory + exportTableSize)) {
            forward = (char*)funcAddr;
        }
        else
            forward = nullptr;

        _exports.emplace_back(ExportEntry
            {name, funcAddr + imageView/*va*/, std::string(forward == nullptr? "" : forward)}
        );
    }
}


OBFUSCATOR_API exports PEImage::GetExports()
{
    if (_exports.empty())
        ParseExport();
    return _exports;
}


}
}