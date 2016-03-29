// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/shader/shader.h"

namespace Pica {

namespace Shader {

template<bool Debug>
class RunnableInterpreterShader : public RunnableShader<Debug> {

public:

    void Compile();
    DebugData<Debug> Run(unsigned int offset, UnitState& state, const InputRegisters& input, OutputRegisters& output);

};

} // namespace

} // namespace
