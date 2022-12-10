#include "AsmWrapperX32.h"

using namespace asmjit;



void  AsmWrapperX86::GeneratePrologue() {
    _assm->push(x86::ebp);
    _assm->mov(x86::ebp, x86::esp);
}

void AsmWrapperX86::GenerateEpiloge() {
    _assm->mov(x86::esp, x86::ebp);
    _assm->pop(x86::ebp);
}


retTypeVariant AsmWrapperX86::GenerateCall(functionAddress faddr, std::vector<ArgType> args,
    asmwrapper::RetType retType, CallingConv callingConv) {
    
    //Do we need to make stack align? Hacker can make pattern and marks our obf. funcs

    /*Set args (only stack in x32*/
    for (const auto& arg : args) {
        PushArg(arg);
    }

    /*actual call*/
    if (std::holds_alternative<asmjit::Label>(faddr)) {
        Assembler()->call(std::get<asmjit::Label>(faddr));        
    }
    else if (std::holds_alternative<asmjit::Imm>(faddr)) {
        Assembler()->mov(asmjit::x86::ecx, std::get<asmjit::Imm>(faddr));
        Assembler()->call(asmjit::x86::ecx);
    }
    else if (std::holds_alternative<asmjit::RegGroup>(faddr)) {
        Assembler()->call(std::get<asmjit::RegGroup>(faddr));
    }

    // otherwise func cleanup stack by itself
    if (callingConv == IAssembler::CallingConv::__CDECL) {
        Assembler()->add(asmjit::x86::esp, args.size() * sizeof(DWORD));
    }
    
    
    //here we will process of ret val
    return GetRetValue(retType);
}

void AsmWrapperX86::GenerateJunk(asmwrapper::JunkType junkType)
{    
    auto a = GetRandomRegister(rand());
    auto b = GetRandomRegister(rand());

    switch (junkType)
    {    
    case asmwrapper::JunkType::REGMOVEMENT:
        while (a == b)
            b = GetRandomRegister(rand());

        Assembler()->push(a);
        Assembler()->mov(a, b);
        Assembler()->pop(b);
        break;
    case asmwrapper::JunkType::STACKABUSE:
        auto countLoops = utils::rand(6, 20);
        for (int i = 0; i < countLoops; ++i) {
            RandomCommand(utils::rand(0, 1));
        }

        if (pushCount > 0)
            Assembler()->add(x86::esp, sizeof(DWORD) * pushCount);

        
        
        Assembler()->mov(x86::bl, x86::ecx);
        Assembler()->inc(x86::ecx);
        break;    
    }
}

// TODO: we dont touch any local variables, need to make some one
functionAddress AsmWrapperX86::GenerateFunction(functionBlock funcBlock)
{
    // сейчас у нас нет обработки принимать арги....
    auto addr = Assembler()->newLabel();
    Assembler()->bind(addr);

    bool hasPrEpilog = funcBlock.fi.hasEpilogPrologue;
    if (hasPrEpilog)
        GenerateEpiloge();

    
    // if faile returns Imm(0)
    auto takeArgument = [&funcBlock, hasPrEpilog](size_t number) {
        auto argsCount = funcBlock.fi.parameters.size();
        if (number > argsCount)
            return asmjit::x86::Mem(0);
        auto offset = hasPrEpilog ? 4 : 0;
        auto reperPoint = hasPrEpilog ? x86::ebp : x86::esp;

        
        return x86::dword_ptr(reperPoint, offset + sizeof(number) * number);
    };

    /*local vars*/


    /*processing of need to call*/
    /*TODO: random insertion of this*/
    for (auto func : funcBlock.needToCall) {
        GenerateCall(func.addres, func.parameters, func.retType, func.cc);
    }
        
    if (funcBlock.randomizeBody == true) {
        for (int i = 0; i < utils::rand(4, 10); ++i)
            GenerateJunk(static_cast<asmwrapper::JunkType>(utils::rand(0x1, 0x2)));
    }

    switch (funcBlock.fi.retType)
    {
    case asmwrapper::RetType::_int: {
        //temp variant need randomize
        auto rg = GetRandomRegister(utils::rand(0, 1000000));
        Assembler()->mov(Assembler()->zax(), rg);
        break;
    }                    
    case asmwrapper::RetType::_double:
    case asmwrapper::RetType::_float:
        /*we need to know the ret location in processing of generate code*/
        /*test variant at the moment*/
        Assembler()->fld(x86::dword_ptr(x86::eax));
        break;

    default:
        break;
    }

    if (hasPrEpilog)
        GenerateEpiloge();
    
    if (funcBlock.fi.cc == IAssembler::CallingConv::__STDCALL) {
        Assembler()->add(x86::esp, funcBlock.fi.parameters.size() * sizeof(DWORD));
    }

    Assembler()->ret();
    return addr;
}

void AsmWrapperX86::AllocStackSpace(stackVariarity reg, std::size_t size)
{
    auto originalAddress = reg;
    Assembler()->sub(reg, size);
}

void AsmWrapperX86::PushArg(ArgType argType)
{
    //TODO: add a method that return a type...
    if (std::holds_alternative<asmjit::Imm>(argType)) {
        // mvsc uses push / pop
        // GCC uses directly manipulating w/ sp
        // TODO: add variantity?

        Assembler()->push(std::get<asmjit::Imm>(argType));
        //_assm->mov(x86::eax, std::get<asmjit::Imm>(argType));
        //_assm->mov(x86::dword_ptr(x86::esp, 4), x86::eax);
        //_assm->sub(x86::esp, 4);
    }
    if (std::holds_alternative<asmjit::x86::Mem>(argType)) {
        Assembler()->push(std::get<asmjit::x86::Mem>(argType));
    }
    if (std::holds_alternative<asmjit::x86::Gp>(argType)) {
        Assembler()->push(std::get<asmjit::x86::Gp>(argType));
    }
}

void AsmWrapperX86::RandomCommand(size_t command)
{
    auto commandIndex = utils::rand(0, 1);
    // add a switch with enum
    if (commandIndex == 0) {
        Assembler()->push(utils::rand(0x1, 0x100));
        pushCount += 1;
    }
    // pop with no args
    if (commandIndex == 1) {
        if (pushCount > 0) {
            Assembler()->add(x86::esp, 4);
            pushCount -= 1;
        }
    }

}
