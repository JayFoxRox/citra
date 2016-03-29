// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <unordered_map>

#include <boost/range/algorithm/fill.hpp>

#include "common/hash.h"
#include "common/make_unique.h"
#include "common/microprofile.h"
#include "common/profiler.h"

#include "video_core/debug_utils/debug_utils.h"
#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/video_core.h"

#include "shader.h"
#include "shader_interpreter.h"

#ifdef ARCHITECTURE_x86_64
#include "shader_jit_x64.h"
#endif // ARCHITECTURE_x86_64

namespace Pica {

namespace Shader {

#ifdef ARCHITECTURE_x86_64
static std::unordered_map<u64, std::unique_ptr<RunnableJitShader>> shader_map;
static const RunnableJitShader* jit_shader;
#endif // ARCHITECTURE_x86_64

void Shutdown() {
#ifdef ARCHITECTURE_x86_64
    shader_map.clear();
#endif // ARCHITECTURE_x86_64
}

//FIXME: Move somewhere else and pass to Run function
static Common::Profiling::TimingCategory shader_category("Vertex Shader");
MICROPROFILE_DEFINE(GPU_VertexShader, "GPU", "Vertex Shader", MP_RGB(50, 50, 240));

template <bool Debug>
RunnableShader<Debug>& Setup(UnitState& state, const Regs::ShaderConfig& config, const ShaderSetup& setup) {
    RunnableShader<Debug>* shader;
    shader = new RunnableInterpreterShader<Debug>();
    shader->program_code = setup.program_code;
    shader->swizzle_data = setup.swizzle_data;
    shader->Compile();
    return static_cast<RunnableShader<Debug>&>(*shader);
}

template RunnableShader<false>& Setup(UnitState& state, const Regs::ShaderConfig& config, const ShaderSetup& setup);
template RunnableShader<true>& Setup(UnitState& state, const Regs::ShaderConfig& config, const ShaderSetup& setup);

#if 0
RunnableShader<true>& Setup(UnitState& state, const Regs::ShaderConfig& config, const ShaderSetup& setup) {
    RunnableShader<true>* shader;
    shader = new RunnableInterpreterShader<true>();
    shader->program_code = setup.program_code;
    shader->swizzle_data = setup.swizzle_data;
    shader->Compile();
    return static_cast<RunnableShader<true>&>(*shader);
}

RunnableShader<false>& Setup(UnitState& state, const Regs::ShaderConfig& config, const ShaderSetup& setup) {
    RunnableShader<false>* shader;

#ifdef ARCHITECTURE_x86_64
    if (VideoCore::g_shader_jit_enabled) {
        shader = new RunnableJitShader();
    } else {
        shader = new RunnableInterpreterShader<false>();
    }
#else
    shader = new RunnableInterpreterShader<false>();
#endif

    shader->program_code = setup.program_code;
    shader->swizzle_data = setup.swizzle_data;
    shader->Compile();
    return static_cast<RunnableShader<false>&>(*shader);
}

#endif


#if 0
//FIXME: Make this work!
#ifdef ARCHITECTURE_x86_64
    if (VideoCore::g_shader_jit_enabled) {

        u64 cache_key = (Common::ComputeHash64(&g_state.vs.program_code, sizeof(g_state.vs.program_code)) ^
            Common::ComputeHash64(&g_state.vs.swizzle_data, sizeof(g_state.vs.swizzle_data)));

        auto iter = shader_map.find(cache_key);
        if (iter != shader_map.end()) {
            jit_shader = iter->second.get();
        } else {
            auto shader = Common::make_unique<RunnableJitShader>();
            shader->Compile();
            jit_shader = shader.get();
            shader_map[cache_key] = std::move(shader);
        }
    }
#endif // ARCHITECTURE_x86_64
}
#endif

//template RunnableShader<false>& Setup(UnitState& state, const Regs::ShaderConfig& config, const ShaderSetup& setup);
//template RunnableShader<true>& Setup(UnitState& state, const Regs::ShaderConfig& config, const ShaderSetup& setup);

//FIXME: Don't hardcode VS down there..
OutputVertex OutputRegistersToVertex(OutputRegisters& registers) {
    // Setup output data
    OutputVertex ret;

    // TODO(neobrain): Under some circumstances, up to 16 attributes may be output. We need to
    // figure out what those circumstances are and enable the remaining outputs then.
    unsigned index = 0;
    for (unsigned i = 0; i < 7; ++i) {

        if (index >= g_state.regs.vs_output_total)
            break;

        if ((g_state.regs.vs.output_mask & (1 << i)) == 0)
            continue;

        const auto& output_register_map = g_state.regs.vs_output_attributes[index]; // TODO: Don't hardcode VS here

        u32 semantics[4] = {
            output_register_map.map_x, output_register_map.map_y,
            output_register_map.map_z, output_register_map.map_w
        };

        for (unsigned comp = 0; comp < 4; ++comp) {
            float24* out = ((float24*)&ret) + semantics[comp];
            if (semantics[comp] != Regs::VSOutputAttributes::INVALID) {
                *out = registers.registers[i][comp];
            } else {
                // Zero output so that attributes which aren't output won't have denormals in them,
                // which would slow us down later.
                memset(out, 0, sizeof(*out));
            }
        }

        index++;
    }

    // The hardware takes the absolute and saturates vertex colors like this, *before* doing interpolation
    for (unsigned i = 0; i < 4; ++i) {
        ret.color[i] = float24::FromFloat32(
            std::fmin(std::fabs(ret.color[i].ToFloat32()), 1.0f));
    }

    LOG_TRACE(HW_GPU, "Output vertex: pos(%.2f, %.2f, %.2f, %.2f), quat(%.2f, %.2f, %.2f, %.2f), "
        "col(%.2f, %.2f, %.2f, %.2f), tc0(%.2f, %.2f), view(%.2f, %.2f, %.2f)",
        ret.pos.x.ToFloat32(), ret.pos.y.ToFloat32(), ret.pos.z.ToFloat32(), ret.pos.w.ToFloat32(),
        ret.quat.x.ToFloat32(), ret.quat.y.ToFloat32(), ret.quat.z.ToFloat32(), ret.quat.w.ToFloat32(),
        ret.color.x.ToFloat32(), ret.color.y.ToFloat32(), ret.color.z.ToFloat32(), ret.color.w.ToFloat32(),
        ret.tc0.u().ToFloat32(), ret.tc0.v().ToFloat32(),
        ret.view.x.ToFloat32(), ret.view.y.ToFloat32(), ret.view.z.ToFloat32());

    return ret;

}

#if 0
//FIXME: REMOVE!!!
// Run for performance
DebugData<debug> RunnableShader::Run(UnitState& state, const InputRegisters& input, const OutputRegisters& output) {

    Common::Profiling::ScopeTimer timer(shader_category);
    MICROPROFILE_SCOPE(GPU_VertexShader);

    state.program_counter = config.main_offset;
    state.debug.max_offset = 0;
    state.debug.max_opdesc_id = 0;

    state.conditional_code[0] = false;
    state.conditional_code[1] = false;

    Run();

    return ret;
}
#endif

} // namespace Shader

} // namespace Pica
