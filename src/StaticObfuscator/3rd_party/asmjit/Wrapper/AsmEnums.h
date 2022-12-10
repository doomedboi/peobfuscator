#pragma once

namespace asmwrapper {
    enum class JunkType
    {
        REGMOVEMENT = 0x1,
        STACKABUSE = 0x2
    };

    enum class RetType {
        _void = 0x0,
        _int,
        _float, // x32 = st(0); in case of win64 __xmm0
        _double,      // like above
    };
}