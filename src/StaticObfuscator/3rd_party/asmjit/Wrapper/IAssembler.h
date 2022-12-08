#pragma once


#include "../x86.h"
#include <memory>
#include <vector>
#include <variant>
#include <intsafe.h>
#include "../../../Utilities/Utils.h"

struct functionInfo;
struct functionBlock;

using functionAddress = std::variant<asmjit::Label, asmjit::Imm, asmjit::RegGroup>;
using ArgType         = std::variant<asmjit::Imm>;
using stackVariarity = asmjit::x86::Gp;

enum class JunkType
{
    REGMOVEMENT = 0x1,
    STACKABUSE = 0x2
};

class IAssembler {
public:

    enum class Arch {
        x86 = 0x1,
        x64 = 0x2
    };

    // determines method of function's invocation
    enum class CallType {
        IMM = 0x1, // call via immideatly 
        REG = 0x2, // call via register
        LABEL = 0x3
    };

    enum class CallingConv {
        __CDECL = 0x1,    // caller care about stack
        __STDCALL = 0x2,  // calle care about stack
        __FASTCALL = 0x3, // first 4 args(in WIN) passed by regs
        __THISSCALL = 0x4 // first arg = this, used in class methods 
    };

    IAssembler(IAssembler&&) = delete;

    IAssembler(IAssembler&)  = delete;
    
    IAssembler(IAssembler::Arch arch = IAssembler::Arch::x86)
    {
        _runtime._environment._arch = (asmjit::Arch)arch;
        _codeHolder.init(_runtime.environment());
        _assm = new asmjit::x86::Assembler(&_codeHolder);
    }

    virtual void GeneratePrologue() = 0;
    virtual void GenerateEpiloge() = 0;
    virtual void GenerateCall(functionAddress, std::vector<ArgType> args,
         CallingConv callingConv) = 0;

    virtual functionAddress GenerateFunction(functionBlock) = 0; //TODO: think about args & etc

    virtual void GenerateJunk(JunkType) = 0;

    // no sp & bp
    asmjit::x86::Gp GetRandomRegister(size_t seed) {
        int garbage;
        ::srand((seed << 4) + (seed ^ 0x1110001010101));
#define REGS_COUNT 5
        auto idx = utils::rand(0, 5);
        switch (idx)
        {
        case 0:
            return Assembler()->zax(); // eax or rax
            break;
        case 1:
            return Assembler()->zbx(); // ebx or rbx
            break;
        case 2:
            return Assembler()->zcx();
            break;
        case 3:
            return Assembler()->zdx();
            break;
        case 4:
            return Assembler()->zsi();
        case 5:
            return Assembler()->zdi();
        default:
            break;
        }  
    }

    inline asmjit::CodeHolder* CodeHolder() { return &_codeHolder; }
    inline asmjit::x86::Assembler* Assembler() { return _assm; }
    inline asmjit::JitRuntime* Runtime() { return &_runtime; }

    virtual ~IAssembler() { delete _assm; }
protected:
    asmjit::x86::Assembler* _assm = nullptr;
    asmjit::JitRuntime     _runtime{};
    asmjit::CodeHolder     _codeHolder;
};

struct functionInfo {
    functionAddress addres; // Address of function
    IAssembler::CallingConv cc; // Calling Convention
    std::vector<ArgType> parameters;
    bool hasEpilogPrologue = true;
};

struct functionBlock {
    functionInfo fi; // Calling function info
    std::vector<functionInfo> needToCall; // Will be called inside fi
    bool randomizeBody = false;                   
};
