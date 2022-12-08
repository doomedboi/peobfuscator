#pragma once


#include "AsmWrapperX32.h"
#include <memory>
#include <assert.h>

struct AsmFactory {

    static std::unique_ptr<IAssembler>
        Assembler(IAssembler::Arch arch) {
        switch (arch)
        {
        case IAssembler::Arch::x86:
            return std::make_unique<AsmWrapperX86>();
            break;
        case IAssembler::Arch::x64:
            assert("Not implement yet" && 0);
            break;
        default:
            return nullptr;
        }
    }
};