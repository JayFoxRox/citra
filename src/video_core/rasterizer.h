// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

//FIXME: Might not need all of these - just added to get going
#include "common/color.h"
#include "common/common_types.h"
#include "common/math_util.h"

#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/rasterizer.h"
#include "video_core/utils.h"

namespace Pica {

namespace Shader {
    struct OutputVertex;
}

namespace Rasterizer {

void ProcessTriangle(const Shader::OutputVertex& v0,
                     const Shader::OutputVertex& v1,
                     const Shader::OutputVertex& v2);

struct RasterizerState {
    Math::Vec4<u8> primary_color; // Only if 'needs' set!
    Math::Vec2<float24> uv[3]; // Only if 'needs' set!
    Math::Vec4<u8> texture_color[3];

    Math::Vec4<u8> combiner_output;

    bool alpha_pass;
    bool z_pass;

    u32 z; // Result of the depth calculation
    Math::Vec4<u8> result;

    // These are the inputs:
    Math::Vec2<fixedS28P4> p;
    Math::Vec4<u8> dest; // Previous color of framebuffer (should be masked by 'needs' flag?)

    // And this is a flow control thing.. FIXME: Remove
    bool discard;
};

struct RasterizerSetup {
    bool needs_primary_color;
    bool needs_uv[3];
    std::vector<std::function<void(RasterizerState& state)>> functions;

    void Setup();

    void ProcessPixel(const Math::Vec2<fixedS28P4>& p,
                      int w0, int w1, int w2,
                      const Shader::OutputVertex& v0,
                      const Shader::OutputVertex& v1,
                      const Shader::OutputVertex& v2);

};

extern RasterizerSetup g_setup;

} // namespace Rasterizer

} // namespace Pica
