#include "MapParser.h"

MAP::Parser::Parser(std::string_view pathToFile) {
    if (auto status = std::filesystem::exists(pathToFile);
        status != TRUE) {
        assert("File doesn't exist" && false);
    }
    _hFile = ::CreateFileA(
        pathToFile.data(), FILE_READ_ACCESS, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    );
    _fileSize = std::filesystem::file_size(pathToFile);
    _fileData = new BYTE[_fileSize];
    DWORD read{};
    if (::ReadFile(_hFile, _fileData, _fileSize, &read, nullptr) != TRUE) {
        delete _fileData;
        ::CloseHandle(_hFile);
        assert("failed read file" && false);
    }
}

MAP::MapFileData MAP::Parser::Parse() {
    MapFileData res;
    std::istringstream ss;
    ss.str(static_cast<std::string>((char*)_fileData));
    /*read cycle*/
    for (std::string input; std::getline(ss, input);) {
        processRead(input, res);
    }

    return res;
}

void MAP::Parser::processRead(std::string& line, MapFileData& res)
{
    auto spliceBySpaces = [&line]() {
        std::vector<std::string> result;
        result.reserve(4);

        std::istringstream ss(line);
        for (std::string str; ss >> str;) {
            result.emplace_back(str);
        }

        return result;
    };

    auto strToHex = [](std::string& str, DWORD64* res) {
        std::stringstream ss;
        ss << std::hex << str;
        ss >> *res;
    };

    auto splicedLn = spliceBySpaces();
    if (splicedLn.size() < 4)
        return;

    // default names are not mangled
    // example: main eg libraries funcs
    auto unmanglingFName = [&splicedLn]() {
        auto manglingName = splicedLn[1];
        std::string result; result.reserve(10);
        bool readingName = false;        

        auto symbol = manglingName[0];
        //is It mangled by compiler? Exit immeadetly
        if (symbol != '?') {
            return manglingName;
        }

        for (int i = 0; i < manglingName.size(); ++i) {
            if (manglingName[i] == '?') {
                readingName = true;
                continue;
            }
            if (readingName != FALSE && manglingName[i] == '@') {
                result = (manglingName.substr(1, i - 1));
                readingName = false;
            }
        }
        return result;
    };


    // we found a function
    if (splicedLn[FUNC_INDEX].compare("f") == 0) {
        DWORD64 dummy;
        strToHex(splicedLn[FUNC_ADDR], &dummy);
        res.functions.emplace_back(dummy, unmanglingFName());
    }
}



