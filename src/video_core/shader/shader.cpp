// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <cstring>
#include "common/bit_set.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "video_core/pica_state.h"
#include "video_core/regs_rasterizer.h"
#include "video_core/regs_shader.h"
#include "video_core/shader/shader.h"
#include "video_core/shader/shader_interpreter.h"
#ifdef ARCHITECTURE_x86_64
#include "video_core/shader/shader_jit_x64.h"
#endif // ARCHITECTURE_x86_64
#include "video_core/video_core.h"

namespace Pica {

namespace Shader {

OutputVertex OutputVertex::FromAttributeBuffer(const RasterizerRegs& regs, AttributeBuffer& input) {
    // Setup output data
    union {
        OutputVertex ret{};
        std::array<float24, 24> vertex_slots;
    };
    static_assert(sizeof(vertex_slots) == sizeof(ret), "Struct and array have different sizes.");

    unsigned int num_attributes = regs.vs_output_total;
    ASSERT(num_attributes <= 7);
    for (unsigned int i = 0; i < num_attributes; ++i) {
        const auto& output_register_map = regs.vs_output_attributes[i];

        RasterizerRegs::VSOutputAttributes::Semantic semantics[4] = {
            output_register_map.map_x, output_register_map.map_y, output_register_map.map_z,
            output_register_map.map_w};

        for (unsigned comp = 0; comp < 4; ++comp) {
            RasterizerRegs::VSOutputAttributes::Semantic semantic = semantics[comp];
            if (semantic < vertex_slots.size()) {
                vertex_slots[semantic] = input.attr[i][comp];
            } else if (semantic != RasterizerRegs::VSOutputAttributes::INVALID) {
                LOG_ERROR(HW_GPU, "Invalid/unknown semantic id: %u", (unsigned int)semantic);
            }
        }
    }

    // The hardware takes the absolute and saturates vertex colors like this, *before* doing
    // interpolation
    for (unsigned i = 0; i < 4; ++i) {
        ret.color[i] = float24::FromFloat32(std::fmin(std::fabs(ret.color[i].ToFloat32()), 1.0f));
    }

    LOG_TRACE(HW_GPU, "Output vertex: pos(%.2f, %.2f, %.2f, %.2f), quat(%.2f, %.2f, %.2f, %.2f), "
                      "col(%.2f, %.2f, %.2f, %.2f), tc0(%.2f, %.2f), view(%.2f, %.2f, %.2f)",
              ret.pos.x.ToFloat32(), ret.pos.y.ToFloat32(), ret.pos.z.ToFloat32(),
              ret.pos.w.ToFloat32(), ret.quat.x.ToFloat32(), ret.quat.y.ToFloat32(),
              ret.quat.z.ToFloat32(), ret.quat.w.ToFloat32(), ret.color.x.ToFloat32(),
              ret.color.y.ToFloat32(), ret.color.z.ToFloat32(), ret.color.w.ToFloat32(),
              ret.tc0.u().ToFloat32(), ret.tc0.v().ToFloat32(), ret.view.x.ToFloat32(),
              ret.view.y.ToFloat32(), ret.view.z.ToFloat32());

    return ret;
}

void UnitState::LoadInput(const ShaderRegs& config, const AttributeBuffer& input) {
    const unsigned max_attribute = config.max_input_attribute_index;

    for (unsigned attr = 0; attr <= max_attribute; ++attr) {
        unsigned reg = config.GetRegisterForAttribute(attr);
        registers.input[reg] = input.attr[attr];
    }
}

void UnitState::WriteOutput(const ShaderRegs& config, AttributeBuffer& output) {
    unsigned int output_i = 0;
    for (unsigned int reg : Common::BitSet<u32>(config.output_mask)) {
        output.attr[output_i++] = registers.output[reg];
    }
}

MICROPROFILE_DEFINE(GPU_Shader, "GPU", "Shader", MP_RGB(50, 50, 240));

#ifdef ARCHITECTURE_x86_64
static std::unique_ptr<JitX64Engine> jit_engine;
#endif // ARCHITECTURE_x86_64
static InterpreterEngine interpreter_engine;

ShaderEngine* GetEngine() {
#ifdef ARCHITECTURE_x86_64
    // TODO(yuriks): Re-initialize on each change rather than being persistent
    if (VideoCore::g_shader_jit_enabled) {
        if (jit_engine == nullptr) {
            jit_engine = std::make_unique<JitX64Engine>();
        }
        return jit_engine.get();
    }
#endif // ARCHITECTURE_x86_64

    return &interpreter_engine;
}

void Shutdown() {
#ifdef ARCHITECTURE_x86_64
    jit_engine = nullptr;
#endif // ARCHITECTURE_x86_64
}

bool SharedGS() {
    return g_state.regs.pipeline.vs_com_mode == Pica::PipelineRegs::VSComMode::Shared;
}

void WriteUniformBoolReg(bool gs, u32 value) {
    auto& setup = gs ? g_state.gs : g_state.vs;

    ASSERT(setup.uniforms.b.size() == 16);
    for (unsigned i = 0; i < 16; ++i)
        setup.uniforms.b[i] = (value & (1 << i)) != 0;
}

void WriteUniformIntReg(bool gs, unsigned index, const Math::Vec4<u8>& values) {
    const char* shader_type = gs ? "GS" : "VS";
    auto& setup = gs ? g_state.gs : g_state.vs;

    ASSERT(index < setup.uniforms.i.size());
    setup.uniforms.i[index] = values;
    LOG_TRACE(HW_GPU, "Set %s integer uniform %d to %02x %02x %02x %02x", shader_type, index,
              values.x.Value(), values.y.Value(), values.z.Value(), values.w.Value());
}

void WriteUniformFloatSetupReg(bool gs, u32 value) {
    auto& config = gs ? g_state.regs.gs : g_state.regs.vs;

    config.uniform_setup.setup = value;
}

void WriteUniformFloatReg(bool gs, u32 value) {
    const char* shader_type = gs ? "GS" : "VS";
    auto& config = gs ? g_state.regs.gs : g_state.regs.vs;
    auto& setup = gs ? g_state.gs : g_state.vs;

    auto& uniform_setup = config.uniform_setup;
    auto& uniform_write_buffer = setup.uniform_write_buffer;
    auto& float_regs_counter = setup.float_regs_counter;

    // TODO: Does actual hardware indeed keep an intermediate buffer or does
    //       it directly write the values?
    uniform_write_buffer[float_regs_counter++] = value;

    // Uniforms are written in a packed format such that four float24 values are
    // encoded in
    // three 32-bit numbers. We write to internal memory once a full such vector
    // is
    // written.
    if ((float_regs_counter >= 4 && uniform_setup.IsFloat32()) ||
        (float_regs_counter >= 3 && !uniform_setup.IsFloat32())) {
        float_regs_counter = 0;

        auto& uniform = setup.uniforms.f[uniform_setup.index];

        if (uniform_setup.index >= 96) {
            LOG_ERROR(HW_GPU, "Invalid %s float uniform index %d", shader_type,
                      (int)uniform_setup.index);
        } else {

            // NOTE: The destination component order indeed is "backwards"
            if (uniform_setup.IsFloat32()) {
                for (auto i : {0, 1, 2, 3})
                    uniform[3 - i] = float24::FromFloat32(*(float*)(&uniform_write_buffer[i]));
            } else {
                // TODO: Untested
                uniform.w = float24::FromRaw(uniform_write_buffer[0] >> 8);
                uniform.z = float24::FromRaw(((uniform_write_buffer[0] & 0xFF) << 16) |
                                             ((uniform_write_buffer[1] >> 16) & 0xFFFF));
                uniform.y = float24::FromRaw(((uniform_write_buffer[1] & 0xFFFF) << 8) |
                                             ((uniform_write_buffer[2] >> 24) & 0xFF));
                uniform.x = float24::FromRaw(uniform_write_buffer[2] & 0xFFFFFF);
            }

            LOG_TRACE(HW_GPU, "Set %s float uniform %x to (%f %f %f %f)", shader_type,
                      (int)uniform_setup.index, uniform.x.ToFloat32(), uniform.y.ToFloat32(),
                      uniform.z.ToFloat32(), uniform.w.ToFloat32());

            // TODO: Verify that this actually modifies the register!
            uniform_setup.index.Assign(uniform_setup.index + 1);
        }
    }
}

void WriteProgramCodeOffset(bool gs, u32 value) {
    auto& config = gs ? g_state.regs.gs : g_state.regs.vs;
    config.program.offset = value;
}

void WriteProgramCode(bool gs, u32 value) {
    const char* shader_type = gs ? "GS" : "VS";
    auto& config = gs ? g_state.regs.gs : g_state.regs.vs;
    auto& setup = gs ? g_state.gs : g_state.vs;

    if (config.program.offset >= setup.program_code.size()) {
        LOG_ERROR(HW_GPU, "Invalid %s program offset %d", shader_type, (int)config.program.offset);
    } else {
        setup.program_code[config.program.offset] = value;
        config.program.offset++;
    }
}

void WriteSwizzlePatternsOffset(bool gs, u32 value) {
    auto& config = gs ? g_state.regs.gs : g_state.regs.vs;
    config.swizzle_patterns.offset = value;
}

void WriteSwizzlePatterns(bool gs, u32 value) {
    const char* shader_type = gs ? "GS" : "VS";
    auto& config = gs ? g_state.regs.gs : g_state.regs.vs;
    auto& setup = gs ? g_state.gs : g_state.vs;

    if (config.swizzle_patterns.offset >= setup.swizzle_data.size()) {
        LOG_ERROR(HW_GPU, "Invalid %s swizzle pattern offset %d", shader_type,
                  (int)config.swizzle_patterns.offset);
    } else {
        setup.swizzle_data[config.swizzle_patterns.offset] = value;
        config.swizzle_patterns.offset++;
    }
}

} // namespace Shader

} // namespace Pica
