#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <string_view>
#include <filesystem>
#include <cassert>

namespace MAP {

    constexpr auto FUNC_INDEX = 3;
    constexpr auto FUNC_ADDR = 2;
    constexpr auto FUNC_NAME = 1;

    struct FunctionData {
        DWORD64 RVA;
        std::string name;
    };

    // It contains only what I need
    struct MapFileData {
        std::vector<FunctionData> functions;        
    };

    class Parser {
    public:
        /// <summary>
        /// Initiate Parser by given file path
        /// </summary>
        /// <param name="pathToFile">path to file</param>
        Parser(std::string_view pathToFile);
        
        /// <summary>
        /// Parse the given .map file
        /// </summary>
        /// <returns>Parsed .map file represented by MapFileData</returns>
        MapFileData Parse();

        ~Parser() { delete _fileData; }
    private:
        size_t _fileSize{};
        HANDLE _hFile{};
        LPVOID _fileData = nullptr;

        // internal function takes current reading line
        void processRead(std::string& line, MapFileData& res);            
    };
}