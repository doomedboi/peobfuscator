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

OBFUSCATOR_API PEImage::~PEImage()
{
    if (_imageView)
        UnmapViewOfFile(_imageView);
}

OBFUSCATOR_API Imports PEImage::GetImports()
{
    if (_imports.size() == 0)
        ParseImport();
    return _imports;

}

OBFUSCATOR_API Relocs PEImage::GetRelocs()
{
    if (_relocs.size() == 0)
        ParseRelocs();
    return _relocs;
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
        
    if (addrDataDirectory == 0 || exportTableSize == 0)
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

OBFUSCATOR_API void PEImage::ParseImport()
{
    _imports.clear();

    auto [importSectionAddy, size, addrType] = 
        GetDataDirectoryEntry(IMAGE_DIRECTORY_ENTRY_IMPORT, AddressingType::VA);
    auto* IAT = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(importSectionAddy);
    if (!IAT)
        return;
        
    for (auto currentDLL = IAT; ; ++currentDLL) {
        if (currentDLL->Characteristics == 0)
            break;
        DWORD iatIndex = 0;

        auto table = currentDLL->OriginalFirstThunk ?
            currentDLL->OriginalFirstThunk : currentDLL->FirstThunk;
        //to va
        table += (uintptr_t)_imageView;
        
        auto AddressOfData = [this](auto addr) {
            return _architecture == Arch::x64 ?
                ((IMAGE_THUNK_DATA64*)addr)->u1.AddressOfData : ((IMAGE_THUNK_DATA32*)addr)->u1.AddressOfData;
        };

        ImportModuleEntry importModule;
        importModule.moduleName = std::string(
            currentDLL->Name == 0 ? "" : (char*)(currentDLL->Name + (uintptr_t)_imageView
        ));

        for (; AddressOfData(table);) {
            auto AddyOfData = AddressOfData(table);
            auto* importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(AddyOfData);
            if (importByName == nullptr)
                return;

            ImportFunction funcData;
            // check import type
            if (AddyOfData < (_architecture == Arch::x64 ?
                IMAGE_ORDINAL_FLAG64 : IMAGE_ORDINAL_FLAG32) && importByName->Name) {
                // import by name
                funcData.functionName = std::string((char*)importByName->Name + (uintptr_t)_imageView);
                funcData.ordinalNum = 0;
            } 
            else {
                funcData.functionName = "";
                funcData.ordinalNum = static_cast<WORD>(AddyOfData & 0xFFFF);
                funcData.iat = 0;
            }
            if (currentDLL->FirstThunk)
                funcData.iat = iatIndex + currentDLL->FirstThunk;
            else
                // cacl by myself
                funcData.iat = (AddyOfData - (uintptr_t)_imageView);

            table += _architecture == Arch::x64 ? 
                sizeof(IMAGE_THUNK_DATA64) : sizeof(IMAGE_THUNK_DATA32);
            iatIndex += _architecture == Arch::x64 ?
                sizeof(uint64_t) : sizeof(uint32_t);
            importModule.funcs.push_back(funcData);
        }
        _imports.push_back(importModule);
    }
}

OBFUSCATOR_API void PEImage::ParseRelocs()
{
    auto [relocsDirAddy, dirSz, type] =
        GetDataDirectoryEntry(IMAGE_DIRECTORY_ENTRY_BASERELOC, AddressingType::VA);
    auto* relocsDir = reinterpret_cast<PIMAGE_BASE_RELOCATION>(
        relocsDirAddy
        );
    if (dirSz == NULL)
        return;

    _relocs.clear();
    _relocs.reserve(20);

    while (relocsDir->VirtualAddress) {
        std::vector<RelocData> block;
        block.reserve(20);
        // it can't be smaller
        if (relocsDir->SizeOfBlock >= sizeof(IMAGE_BASE_RELOCATION)) {
            /*winnt hasn't member TypeOffset but actually it has*/ 
            auto relocItemsCount =
                (relocsDir->SizeOfBlock - sizeof(relocsDir->VirtualAddress)
                    - sizeof(relocsDir->SizeOfBlock)) / sizeof(WORD);
            WORD* relocsArray = (WORD*)(relocsDir + 1); // again: see winnt.h
            
            
            // process relocation entries
            for (int i = 0; i < relocItemsCount; ++i) { 
                auto type = (relocsArray[i] >> 12);
                auto offset = (relocsArray[i] & 0xFFF);
                
                auto itemRVA = relocsDir->VirtualAddress + offset;
                

                block.emplace_back(
                    itemRVA,
                    type
                );
                
            }
        }

        _relocs.push_back(block);
        relocsDir = reinterpret_cast<IMAGE_BASE_RELOCATION*>
            ((BYTE*)relocsDir + (relocsDir->SizeOfBlock));
    }
}

auto ImportModuleEntry::NumberOfFuncs()
{
    return funcs.size();
}

OBFUSCATOR_API uint64_t PEImage::GetImageBase() {
    return _imgBase;
}

OBFUSCATOR_API std::vector<IMAGE_SECTION_HEADER> PEImage::GetSections() {
    return _sections;
}

}
}