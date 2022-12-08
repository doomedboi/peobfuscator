#include "..\PE\PEImage.h"
#include "..\3rd_party\asmjit\x86.h"

#include <iostream>
#include <string>
#include "../3rd_party/asmjit/Wrapper/AsmWrapperFactory.h"
#include <fstream>

void __stdcall ffz(int a) {
    ::printf("%d <-it's ur int a\n", a);
}

void kek() { std::cout << "Pizda\n"; }

// TODO: no args - run window - mode 
int main(int argc, char* argv[])
{
    obfuscator::pe::PEImage pe;
    
    std::string fName = "D:\\SteamLibrary\\steamapps\\common\\Warface\\0_1177\\Bin64Release\\Game.exe";
    auto fPath = std::filesystem::path(fName);
    if (std::filesystem::exists(fPath)) {
        pe.Load(fPath.generic_string());
        auto exports = pe.GetExports();
        for (auto exportEntry : exports)
            std::cout << std::hex << exportEntry.name << ' ' 
                << exportEntry.AddressOfFunction << std::endl;
        
        pe.GetImports();
        pe.GetRelocs();
    }
    else
        std::cout << "Failed to find the specified file!\n";



    auto assembler = AsmFactory::Assembler(IAssembler::Arch::x86);
    auto* rt = assembler->Runtime();
        
    
    functionInfo fi =
        { .addres = {}, .cc = IAssembler::CallingConv::__STDCALL,
        .parameters = {asmjit::imm(777)}, .hasEpilogPrologue = true };
    functionBlock fb = { .fi = fi, .needToCall = {}, .randomizeBody = false };    

    auto f = assembler->GenerateFunction(fb);

    assembler->GenerateCall(f, {asmjit::imm(777)}, IAssembler::CallingConv::__STDCALL);

    //assembler->Assembler()->add(asmjit::x86::esp, 4);
    assembler->Assembler()->ret();
    

    
    functionInfo kekFi = {
        .addres = asmjit::Imm(kek), .cc = IAssembler::CallingConv::__CDECL,
        .parameters = {}, .hasEpilogPrologue = false};

    std::vector<decltype(fi)> aa; aa.push_back(fi); aa.push_back(kekFi);
    
    
    
    std::ofstream fout("C:\\Users\\Avgust\\Desktop\\sample.exe");

    for (auto el : assembler->CodeHolder()->sectionById(0)->buffer())
        fout << el;


    using pfun = int(*)();
    pfun myFun;
    auto err = rt->add(&myFun, assembler->CodeHolder());
    
           
    
    //std::cout << myFun();

    int a = 4;
    return 0;
}