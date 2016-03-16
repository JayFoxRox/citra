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
static std::unordered_map<u64, CompiledShader*> shader_map;
static JitCompiler jit;
static CompiledShader* jit_shader;

static void ClearCache() {
    shader_map.clear();
    jit.Clear();
    LOG_INFO(HW_GPU, "Shader JIT cache cleared");
}
#endif // ARCHITECTURE_x86_64

//FIXME: This should return a runnable shader object (= code and swizzle mask + entry point)
void Setup(const Regs::ShaderConfig& config, const ShaderSetup& setup) {
#ifdef ARCHITECTURE_x86_64
    if (VideoCore::g_shader_jit_enabled) {
        u64 cache_key = (Common::ComputeHash64(&setup.program_code, sizeof(setup.program_code)) ^
            Common::ComputeHash64(&setup.swizzle_data, sizeof(setup.swizzle_data)) ^
            config.main_offset);

        auto iter = shader_map.find(cache_key);
        if (iter != shader_map.end()) {
            jit_shader = iter->second;
        } else {
            // Check if remaining JIT code space is enough for at least one more (massive) shader
            if (jit.GetSpaceLeft() < jit_shader_size) {
                // If not, clear the cache of all previously compiled shaders
                ClearCache();
            }
            jit_shader = jit.Compile(setup, config);
            shader_map.emplace(cache_key, jit_shader);
        }
    }
#endif // ARCHITECTURE_x86_64
}

void Shutdown() {
#ifdef ARCHITECTURE_x86_64
    ClearCache();
#endif // ARCHITECTURE_x86_64
}

template<bool Debug>
void HandleEMIT(UnitState<Debug>& state) {

    //FIXME: doesn't work as expected, no idea why?!
    if (g_debug_context && !g_debug_context->at_breakpoint)
        g_debug_context->OnEvent(DebugContext::Event::ShaderEmitInstruction, nullptr);

    auto &emit_params = state.registers.emit_params;
    auto &emit_buffer = state.registers.emit_buffer;

    assert(emit_params.vertex_id < 3);

    auto& buffer_data = emit_buffer[emit_params.vertex_id];
    for(unsigned int i = 0; i < 16; i++) {
        buffer_data[i] = state.registers.output[i];
    }

    if (emit_params.primitive_emit) {
        ASSERT_MSG(state.emit_triangle_callback, "EMIT invoked but no handler set!");
        OutputVertex v0 = GenerateOutputVertex(emit_buffer[0]);
        OutputVertex v1 = GenerateOutputVertex(emit_buffer[1]);
        OutputVertex v2 = GenerateOutputVertex(emit_buffer[2]);
        //FIXME: What about v3?!
        if (emit_params.winding) {
          state.emit_triangle_callback(v2, v1, v0);
        } else {
          state.emit_triangle_callback(v0, v1, v2);
        }
    }

}

// Explicit instantiation
template void HandleEMIT(UnitState<false>& state);
template void HandleEMIT(UnitState<true>& state);

static Common::Profiling::TimingCategory shader_category("Vertex Shader");
MICROPROFILE_DEFINE(GPU_VertexShader, "GPU", "Vertex Shader", MP_RGB(50, 50, 240));

void Run(const Regs::ShaderConfig& config, const ShaderSetup& setup, UnitState<false>& state, const InputVertex& input, int num_attributes) {

    Common::Profiling::ScopeTimer timer(shader_category);
    MICROPROFILE_SCOPE(GPU_VertexShader); //FIXME!

    //FIXME: Should be part of the interpreter
    state.program_counter = config.main_offset;

    state.debug.max_offset = 0;
    state.debug.max_opdesc_id = 0;

    // Setup input register table
    const auto& attribute_register_map = config.input_register_map;

    // TODO: Instead of this cumbersome logic, just load the input data directly like
    // for (int attr = 0; attr < num_attributes; ++attr) { input_attr[0] = state.registers.input[attribute_register_map.attribute0_register]; }
    if (num_attributes > 0) state.registers.input[attribute_register_map.attribute0_register] = input.attr[0];
    if (num_attributes > 1) state.registers.input[attribute_register_map.attribute1_register] = input.attr[1];
    if (num_attributes > 2) state.registers.input[attribute_register_map.attribute2_register] = input.attr[2];
    if (num_attributes > 3) state.registers.input[attribute_register_map.attribute3_register] = input.attr[3];
    if (num_attributes > 4) state.registers.input[attribute_register_map.attribute4_register] = input.attr[4];
    if (num_attributes > 5) state.registers.input[attribute_register_map.attribute5_register] = input.attr[5];
    if (num_attributes > 6) state.registers.input[attribute_register_map.attribute6_register] = input.attr[6];
    if (num_attributes > 7) state.registers.input[attribute_register_map.attribute7_register] = input.attr[7];
    if (num_attributes > 8) state.registers.input[attribute_register_map.attribute8_register] = input.attr[8];
    if (num_attributes > 9) state.registers.input[attribute_register_map.attribute9_register] = input.attr[9];
    if (num_attributes > 10) state.registers.input[attribute_register_map.attribute10_register] = input.attr[10];
    if (num_attributes > 11) state.registers.input[attribute_register_map.attribute11_register] = input.attr[11];
    if (num_attributes > 12) state.registers.input[attribute_register_map.attribute12_register] = input.attr[12];
    if (num_attributes > 13) state.registers.input[attribute_register_map.attribute13_register] = input.attr[13];
    if (num_attributes > 14) state.registers.input[attribute_register_map.attribute14_register] = input.attr[14];
    if (num_attributes > 15) state.registers.input[attribute_register_map.attribute15_register] = input.attr[15];

bool jit = &state == &g_state.shader_unit[3];

#ifdef ARCHITECTURE_x86_64
    if (VideoCore::g_shader_jit_enabled && jit)
        jit_shader(&setup.uniforms, &state);
    else
        RunInterpreter(setup, state);
#else
    RunInterpreter(setup, state);
#endif // ARCHITECTURE_x86_64

    return;

}


//FIXME: Move to Pica or something? Not really related to command_processor but not really shader related either.
//       Also it should be callable by the shader debugger
OutputVertex GenerateOutputVertex(Math::Vec4<float24> (&output_registers)[16]) {

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
            if (semantics[comp] < Regs::VSOutputAttributes::INVALID) {
                *out = output_registers[i][comp];
            } else {
                ASSERT_MSG(semantics[comp] == Regs::VSOutputAttributes::INVALID, "Unknown semantics");
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


DebugData<true> ProduceDebugInfo(const Regs::ShaderConfig& config, const ShaderSetup& setup, const UnitState<false>& state, const InputVertex& input, int num_attributes) {
    UnitState<true> debug_state;

    auto DoNothing = [](const Pica::Shader::OutputVertex& v0, const Pica::Shader::OutputVertex& v1, const Pica::Shader::OutputVertex& v2) {
        // FIXME: This should probably set some label so it's indicated that new data was drawn?
    };
    debug_state.emit_triangle_callback = DoNothing;

    debug_state.program_counter = config.main_offset;

    debug_state.debug.max_offset = 0;
    debug_state.debug.max_opdesc_id = 0;

    // Load temporary registers
    for (unsigned i = 0; i < 16; i++)
        debug_state.registers.temporary[i] = state.registers.temporary[i];

    // Load conditional state
    debug_state.conditional_code[0] = state.conditional_code[0];
    debug_state.conditional_code[1] = state.conditional_code[1];

    // Setup input register table
    const auto& attribute_register_map = config.input_register_map;
    float24 dummy_register;
    boost::fill(debug_state.registers.input, &dummy_register);

    if (num_attributes > 0) debug_state.registers.input[attribute_register_map.attribute0_register] = &input.attr[0].x;
    if (num_attributes > 1) debug_state.registers.input[attribute_register_map.attribute1_register] = &input.attr[1].x;
    if (num_attributes > 2) debug_state.registers.input[attribute_register_map.attribute2_register] = &input.attr[2].x;
    if (num_attributes > 3) debug_state.registers.input[attribute_register_map.attribute3_register] = &input.attr[3].x;
    if (num_attributes > 4) debug_state.registers.input[attribute_register_map.attribute4_register] = &input.attr[4].x;
    if (num_attributes > 5) debug_state.registers.input[attribute_register_map.attribute5_register] = &input.attr[5].x;
    if (num_attributes > 6) debug_state.registers.input[attribute_register_map.attribute6_register] = &input.attr[6].x;
    if (num_attributes > 7) debug_state.registers.input[attribute_register_map.attribute7_register] = &input.attr[7].x;
    if (num_attributes > 8) debug_state.registers.input[attribute_register_map.attribute8_register] = &input.attr[8].x;
    if (num_attributes > 9) debug_state.registers.input[attribute_register_map.attribute9_register] = &input.attr[9].x;
    if (num_attributes > 10) debug_state.registers.input[attribute_register_map.attribute10_register] = &input.attr[10].x;
    if (num_attributes > 11) debug_state.registers.input[attribute_register_map.attribute11_register] = &input.attr[11].x;
    if (num_attributes > 12) debug_state.registers.input[attribute_register_map.attribute12_register] = &input.attr[12].x;
    if (num_attributes > 13) debug_state.registers.input[attribute_register_map.attribute13_register] = &input.attr[13].x;
    if (num_attributes > 14) debug_state.registers.input[attribute_register_map.attribute14_register] = &input.attr[14].x;
    if (num_attributes > 15) debug_state.registers.input[attribute_register_map.attribute15_register] = &input.attr[15].x;

    RunInterpreter(setup, debug_state);
    return debug_state.debug;
}

} // namespace Shader

} // namespace Pica
