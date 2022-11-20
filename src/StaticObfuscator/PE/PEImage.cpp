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

    auto* peHeader32 = reinterpret_cast<IMAGE_NT_HEADERS32*>(
        reinterpret_cast<BYTE*>(dosHeader) + dosHeader->e_lfanew
        );
    auto* peHeader64 = reinterpret_cast<IMAGE_NT_HEADERS64*>(
        reinterpret_cast<BYTE*>(dosHeader) + dosHeader->e_lfanew
        );

    // check whether valid PE or not
    if (peHeader32->Signature != IMAGE_NT_SIGNATURE)
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

    if (auto arch = checkArch(peHeader32); arch == Arch::x64)
        parseHeaders(peHeader64);
    else
        parseHeaders(peHeader32);

    
    auto parseSections = [this, peHeader32]() {
        auto firstSection = reinterpret_cast<IMAGE_SECTION_HEADER*>(
            peHeader32 + 1
            );
        for (int i = 0; i < _numberOfSection; ++i, ++firstSection)
            _sections.push_back(*firstSection);
    };

    parseSections();

    return STATUS_SUCCESS;
}

}
}