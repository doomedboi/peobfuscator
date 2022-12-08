#pragma once
#include "IAssembler.h"


class AsmWrapperX86 : public IAssembler {
public:

    AsmWrapperX86() {};

    virtual void GeneratePrologue() override;

    virtual void GenerateEpiloge() override;

    virtual void GenerateCall(functionAddress label, std::vector<ArgType> args,
        CallingConv callingConv) override;

    virtual void GenerateJunk(JunkType) override;

    

    virtual functionAddress GenerateFunction(functionBlock) override;


private:
    // TODO: Consider the way of calculating lenght by passing local var pack
    void AllocStackSpace(stackVariarity reg, std::size_t size);
    void PushArg(ArgType);
    void RandomCommand(size_t command);
    inline static size_t pushCount = 0;

};