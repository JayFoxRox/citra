// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iomanip>
#include <sstream>

#include <QBoxLayout>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalMapper>
#include <QSpinBox>
#include <QTreeView>

#include "citra_qt/debugger/graphics_shader.h"
#include "citra_qt/util/util.h"

#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/shader/shader.h"

using nihstro::OpCode;
using nihstro::Instruction;
using nihstro::SourceRegister;
using nihstro::SwizzlePattern;

GraphicsVertexShaderModel::GraphicsVertexShaderModel(GraphicsVertexShaderWidget* parent): QAbstractTableModel(parent), par(parent) {

}

int GraphicsVertexShaderModel::columnCount(const QModelIndex& parent) const {
    return 3;
}

int GraphicsVertexShaderModel::rowCount(const QModelIndex& parent) const {
    return static_cast<int>(par->info.code.size());
}

QVariant GraphicsVertexShaderModel::headerData(int section, Qt::Orientation orientation, int role) const {
    switch(role) {
    case Qt::DisplayRole:
    {
        if (section == 0) {
            return tr("Offset");
        } else if (section == 1) {
            return tr("Raw");
        } else if (section == 2) {
            return tr("Disassembly");
        }

        break;
    }
    }

    return QVariant();
}

static std::string SelectorToString(u32 selector) {
    std::string ret;
    for (int i = 0; i < 4; ++i) {
        int component = (selector >> ((3 - i) * 2)) & 3;
        ret += "xyzw"[component];
    }
    return ret;
}

// e.g. "-c92[a0.x].xyzw"
static void print_input(std::ostringstream& output, const SourceRegister& input,
                        bool negate, const std::string& swizzle_mask, bool align = true,
                        const std::string& address_register_name = std::string()) {
    if (align)
        output << std::setw(4) << std::right;
    output << ((negate ? "-" : "") + input.GetName());

    if (!address_register_name.empty())
        output << '[' << address_register_name << ']';
    output << '.' << swizzle_mask;
};

QVariant GraphicsVertexShaderModel::data(const QModelIndex& index, int role) const {
    switch (role) {
    case Qt::DisplayRole:
    {
        switch (index.column()) {
        case 0:
            if (par->info.HasLabel(index.row()))
                return QString::fromStdString(par->info.GetLabel(index.row()));

            return QString("%1").arg(4*index.row(), 4, 16, QLatin1Char('0'));

        case 1:
            return QString("%1").arg(par->info.code[index.row()].hex, 8, 16, QLatin1Char('0'));

        case 2:
        {
            std::ostringstream output;
            output.flags(std::ios::uppercase);

            // To make the code aligning columns of assembly easier to keep track of, this function
            // keeps track of the start of the start of the previous column, allowing alignment
            // based on desired field widths.
            int current_column = 0;
            auto AlignToColumn = [&](int col_width) {
                // Prints spaces to the output to pad previous column to size and advances the
                // column marker.
                current_column += col_width;
                int to_add = std::max(1, current_column - (int)output.tellp());
                for (int i = 0; i < to_add; ++i) {
                    output << ' ';
                }
            };

            const Instruction instr = par->info.code[index.row()];
            const OpCode opcode = instr.opcode;
            const OpCode::Info opcode_info = opcode.GetInfo();
            const u32 operand_desc_id = opcode_info.type == OpCode::Type::MultiplyAdd ?
                instr.mad.operand_desc_id.Value() : instr.common.operand_desc_id.Value();
            const SwizzlePattern swizzle = par->info.swizzle_info[operand_desc_id].pattern;

            // longest known instruction name: "setemit "
            int kOpcodeColumnWidth = 8;
            // "rXX.xyzw  "
            int kOutputColumnWidth = 10;
            // "-rXX.xyzw  ", no attempt is made to align indexed inputs
            int kInputOperandColumnWidth = 11;

            output << opcode_info.name;

            switch (opcode_info.type) {
            case OpCode::Type::Trivial:
                // Nothing to do here
                break;

            case OpCode::Type::Arithmetic:
            case OpCode::Type::MultiplyAdd:
            {
                // Use custom code for special instructions
                switch (opcode.EffectiveOpCode()) {
                case OpCode::Id::CMP:
                {
                    AlignToColumn(kOpcodeColumnWidth);

                    // NOTE: CMP always writes both cc components, so we do not consider the dest mask here.
                    output << " cc.xy";
                    AlignToColumn(kOutputColumnWidth);

                    SourceRegister src1 = instr.common.GetSrc1(false);
                    SourceRegister src2 = instr.common.GetSrc2(false);

                    output << ' ';
                    print_input(output, src1, swizzle.negate_src1, swizzle.SelectorToString(false).substr(0,1), false, instr.common.AddressRegisterName());
                    output << ' ' << instr.common.compare_op.ToString(instr.common.compare_op.x) << ' ';
                    print_input(output, src2, swizzle.negate_src2, swizzle.SelectorToString(true).substr(0,1), false);

                    output << ", ";

                    print_input(output, src1, swizzle.negate_src1, swizzle.SelectorToString(false).substr(1,1), false, instr.common.AddressRegisterName());
                    output << ' ' << instr.common.compare_op.ToString(instr.common.compare_op.y) << ' ';
                    print_input(output, src2, swizzle.negate_src2, swizzle.SelectorToString(true).substr(1,1), false);

                    break;
                }

                case OpCode::Id::MAD:
                case OpCode::Id::MADI:
                {
                    AlignToColumn(kOpcodeColumnWidth);

                    bool src_is_inverted = 0 != (opcode_info.subtype & OpCode::Info::SrcInversed);
                    SourceRegister src1 = instr.mad.GetSrc1(src_is_inverted);
                    SourceRegister src2 = instr.mad.GetSrc2(src_is_inverted);
                    SourceRegister src3 = instr.mad.GetSrc3(src_is_inverted);

                    output << std::setw(3) << std::right << instr.mad.dest.Value().GetName() << '.' << swizzle.DestMaskToString();
                    AlignToColumn(kOutputColumnWidth);
                    print_input(output, src1, swizzle.negate_src1, SelectorToString(swizzle.src1_selector));
                    AlignToColumn(kInputOperandColumnWidth);
                    if (src_is_inverted) {
                      print_input(output, src2, swizzle.negate_src2, SelectorToString(swizzle.src2_selector));
                    } else {
                      print_input(output, src2, swizzle.negate_src2, SelectorToString(swizzle.src2_selector), true, instr.mad.AddressRegisterName());
                    }
                    AlignToColumn(kInputOperandColumnWidth);
                    if (src_is_inverted) {
                      print_input(output, src3, swizzle.negate_src3, SelectorToString(swizzle.src3_selector), true, instr.mad.AddressRegisterName());
                    } else {
                      print_input(output, src3, swizzle.negate_src3, SelectorToString(swizzle.src3_selector));
                    }
                    AlignToColumn(kInputOperandColumnWidth);
                    break;
                }

                default:
                {
                    AlignToColumn(kOpcodeColumnWidth);

                    bool src_is_inverted = 0 != (opcode_info.subtype & OpCode::Info::SrcInversed);

                    if (opcode_info.subtype & OpCode::Info::Dest) {
                        // e.g. "r12.xy__"
                        output << std::setw(3) << std::right << instr.common.dest.Value().GetName() << '.' << swizzle.DestMaskToString();
                    } else if (opcode_info.subtype == OpCode::Info::MOVA) {
                        output << "  a0." << swizzle.DestMaskToString();
                    }
                    AlignToColumn(kOutputColumnWidth);

                    if (opcode_info.subtype & OpCode::Info::Src1) {
                        SourceRegister src1 = instr.common.GetSrc1(src_is_inverted);
                        print_input(output, src1, swizzle.negate_src1, swizzle.SelectorToString(false), true, instr.common.AddressRegisterName());
                        AlignToColumn(kInputOperandColumnWidth);
                    }

                    // TODO: In some cases, the Address Register is used as an index for SRC2 instead of SRC1
                    if (opcode_info.subtype & OpCode::Info::Src2) {
                        SourceRegister src2 = instr.common.GetSrc2(src_is_inverted);
                        print_input(output, src2, swizzle.negate_src2, swizzle.SelectorToString(true));
                        AlignToColumn(kInputOperandColumnWidth);
                    }
                    break;
                }
                }

                break;
            }

            case OpCode::Type::Conditional:
            case OpCode::Type::UniformFlowControl:
            {
                output << ' ';

                switch (opcode.EffectiveOpCode()) {
                case OpCode::Id::LOOP:
                    output << "(unknown instruction format)";
                    break;

                default:
                    if (opcode_info.subtype & OpCode::Info::HasCondition) {
                        output << '(';

                        if (instr.flow_control.op != instr.flow_control.JustY) {
                            if (instr.flow_control.refx) output << '!';
                            output << "cc.x";
                        }

                        if (instr.flow_control.op == instr.flow_control.Or) {
                            output << " || ";
                        } else if (instr.flow_control.op == instr.flow_control.And) {
                            output << " && ";
                        }

                        if (instr.flow_control.op != instr.flow_control.JustX) {
                            if (instr.flow_control.refy) output << '!';
                            output << "cc.y";
                        }

                        output << ") ";
                    } else if (opcode_info.subtype & OpCode::Info::HasUniformIndex) {
                        output << 'b' << instr.flow_control.bool_uniform_id << ' ';
                    }

                    u32 target_addr = instr.flow_control.dest_offset;
                    u32 target_addr_else = instr.flow_control.dest_offset;

                    if (opcode_info.subtype & OpCode::Info::HasAlternative) {
                        output << "else jump to 0x" << std::setw(4) << std::right << std::setfill('0') << std::hex << (4 * instr.flow_control.dest_offset);
                    } else if (opcode_info.subtype & OpCode::Info::HasExplicitDest) {
                        output << "jump to 0x" << std::setw(4) << std::right << std::setfill('0') << std::hex << (4 * instr.flow_control.dest_offset);
                    } else {
                        // TODO: Handle other cases
                        output << "(unknown destination)";
                    }

                    if (opcode_info.subtype & OpCode::Info::HasFinishPoint) {
                        output << " (return on 0x" << std::setw(4) << std::right << std::setfill('0') << std::hex
                               << (4 * instr.flow_control.dest_offset + 4 * instr.flow_control.num_instructions) << ')';
                    }

                    break;
                }
                break;
            }

            default:

                switch (opcode.EffectiveOpCode()) {
                case OpCode::Id::SETEMIT:
                    output << " ("
                           << "vertex_id: " << instr.setemit.vertex_id << "; "
                           << "prim_emit: " << (instr.setemit.prim_emit ? "yes" : "no") << "; "
                           << "winding: " << (instr.setemit.winding ? "ccw" : "cw") << ")"; //FIXME: Is this the correct way around?
                    break;

                default:
                    output << " (unknown instruction format)";
                    break;
                }
                break;
            }

            return QString::fromLatin1(output.str().c_str());
        }

        default:
            break;
        }
    }

    case Qt::FontRole:
        return GetMonospaceFont();

    case Qt::BackgroundRole:
    {
        // Highlight current instruction
        int current_record_index = par->cycle_index->value();
        if (current_record_index < static_cast<int>(par->debug_data.records.size())) {
            const auto& current_record = par->debug_data.records[current_record_index];
            if (index.row() == static_cast<int>(current_record.instruction_offset)) {
                return QColor(255, 255, 63);
            }
        }

        // Use a grey background for instructions which have no debug data associated to them
        for (const auto& record : par->debug_data.records)
            if (index.row() == static_cast<int>(record.instruction_offset))
                return QVariant();

        return QBrush(QColor(192, 192, 192));
    }


    // TODO: Draw arrows for each "reachable" instruction to visualize control flow


    default:
        break;
    }

    return QVariant();
}

void GraphicsVertexShaderWidget::DumpShader() {
    QString filename = QFileDialog::getSaveFileName(this, tr("Save Shader Dump"), "shader_dump.shbin",
                                                    tr("Shader Binary (*.shbin)"));

    if (filename.isEmpty()) {
        // If the user canceled the dialog, don't dump anything.
        return;
    }

    auto& setup  = Pica::g_state.vs;
    auto& config = Pica::g_state.regs.vs;

    Pica::DebugUtils::DumpShader(filename.toStdString(), config, setup, Pica::g_state.regs.vs_output_attributes);
}

GraphicsVertexShaderWidget::GraphicsVertexShaderWidget(std::shared_ptr< Pica::DebugContext > debug_context,
                                                       QWidget* parent)
        : BreakPointObserverDock(debug_context, "Pica Shader Units", parent) {
    setObjectName("PicaShaderUnits");

    // Clear input vertex data so that it contains valid float values in case a debug shader
    // execution happens before the first Vertex Loaded breakpoint.
    // TODO: This makes a crash in the interpreter much less likely, but not impossible. The
    //       interpreter should guard against out-of-bounds accesses to ensure crashes in it aren't
    //       possible.
    std::memset(&input_vertex, 0, sizeof(input_vertex));

    auto input_data_mapper = new QSignalMapper(this);

    // TODO: Support inputting data in hexadecimal raw format
    for (unsigned i = 0; i < ARRAY_SIZE(input_data); ++i) {
        input_data[i] = new QLineEdit;
        input_data[i]->setValidator(new QDoubleValidator(input_data[i]));
    }

    breakpoint_warning = new QLabel(tr("(data only available at shader invocation breakpoints)"));

    // TODO: Add some button for jumping to the shader entry point

    model = new GraphicsVertexShaderModel(this);
    binary_list = new QTreeView;
    binary_list->setModel(model);
    binary_list->setRootIsDecorated(false);
    binary_list->setAlternatingRowColors(true);

    auto dump_shader = new QPushButton(QIcon::fromTheme("document-save"), tr("Dump"));

    instruction_description = new QLabel;

    cycle_index = new QSpinBox;

    connect(dump_shader, SIGNAL(clicked()), this, SLOT(DumpShader()));

    connect(cycle_index, SIGNAL(valueChanged(int)), this, SLOT(OnCycleIndexChanged(int)));

    for (unsigned i = 0; i < ARRAY_SIZE(input_data); ++i) {
        connect(input_data[i], SIGNAL(textEdited(const QString&)), input_data_mapper, SLOT(map()));
        input_data_mapper->setMapping(input_data[i], i);
    }
    connect(input_data_mapper, SIGNAL(mapped(int)), this, SLOT(OnInputAttributeChanged(int)));

    auto main_widget = new QWidget;
    auto main_layout = new QVBoxLayout;
    {
        auto input_data_group = new QGroupBox(tr("Input Data"));

        // For each vertex attribute, add a QHBoxLayout consisting of:
        // - A QLabel denoting the source attribute index
        // - Four QLineEdits for showing and manipulating attribute data
        // - A QLabel denoting the shader input attribute index
        auto sub_layout = new QVBoxLayout;
        for (unsigned i = 0; i < 16; ++i) {
            // Create an HBoxLayout to store the widgets used to specify a particular attribute
            // and store it in a QWidget to allow for easy hiding and unhiding.
            auto row_layout = new QHBoxLayout;
            // Remove unecessary padding between rows
            row_layout->setContentsMargins(0, 0, 0, 0);

            row_layout->addWidget(new QLabel(tr("Attribute %1").arg(i, 2)));
            for (unsigned comp = 0; comp < 4; ++comp)
                row_layout->addWidget(input_data[4 * i + comp]);

            row_layout->addWidget(input_data_mapping[i] = new QLabel);

            input_data_container[i] = new QWidget;
            input_data_container[i]->setLayout(row_layout);
            input_data_container[i]->hide();

            sub_layout->addWidget(input_data_container[i]);
        }

        auto shader_unit_group = new QGroupBox(tr("Shader Unit"));
        {
            auto row_layout = new QHBoxLayout;
            for (unsigned i = 0; i < 4; ++i) {
                // Create an HBoxLayout to store the widgets used to specify a particular attribute
                // and store it in a QWidget to allow for easy hiding and unhiding.
                row_layout->setContentsMargins(0, 0, 0, 0);
                row_layout->addWidget(new QRadioButton(tr("Unit %1").arg(i, 1)));
            }
            row_layout->addWidget(new QRadioButton(tr("Active (None)")));
            shader_unit_group->setLayout(row_layout);
        }

        main_layout->addWidget(shader_unit_group);

        sub_layout->addWidget(breakpoint_warning);
        breakpoint_warning->hide();

        input_data_group->setLayout(sub_layout);
        main_layout->addWidget(input_data_group);
    }

    // Make program listing expand to fill available space in the dialog
    binary_list->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    main_layout->addWidget(binary_list);

    main_layout->addWidget(dump_shader);
    {
        auto sub_layout = new QFormLayout;
        sub_layout->addRow(tr("Cycle Index:"), cycle_index);

        main_layout->addLayout(sub_layout);
    }

    // Set a minimum height so that the size of this label doesn't cause the rest of the bottom
    // part of the UI to keep jumping up and down when cycling through instructions.
    instruction_description->setMinimumHeight(instruction_description->fontMetrics().lineSpacing() * 6);
    instruction_description->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    main_layout->addWidget(instruction_description);

    main_widget->setLayout(main_layout);
    setWidget(main_widget);

    widget()->setEnabled(false);
}

void GraphicsVertexShaderWidget::OnBreakPointHit(Pica::DebugContext::Event event, void* data) {
    auto input = static_cast<Pica::Shader::InputVertex*>(data);
    if (event == Pica::DebugContext::Event::VertexShaderInvocation || event == Pica::DebugContext::Event::GeometryShaderInvocation) {
        Reload(true, data);
    } else {
        // No vertex data is retrievable => invalidate currently stored vertex data
        Reload(true, nullptr);
    }
    widget()->setEnabled(true);
}

void GraphicsVertexShaderWidget::Reload(bool replace_vertex_data, void* vertex_data) {
    model->beginResetModel();

    if (replace_vertex_data) {
        if (vertex_data) {
            memcpy(&input_vertex, vertex_data, sizeof(input_vertex));
            for (unsigned attr = 0; attr < 16; ++attr) {
                for (unsigned comp = 0; comp < 4; ++comp) {
                    input_data[4 * attr + comp]->setText(QString("%1").arg(input_vertex.attr[attr][comp].ToFloat32()));
                }
            }
            breakpoint_warning->hide();
        } else {
            for (unsigned attr = 0; attr < 16; ++attr) {
                for (unsigned comp = 0; comp < 4; ++comp) {
                    input_data[4 * attr + comp]->setText(QString("???"));
                }
            }
            breakpoint_warning->show();
        }
    }

    // Reload shader code
    info.Clear();

    bool show_gs = true;

    auto& shader_setup = show_gs ? Pica::g_state.gs : Pica::g_state.vs;
    auto& shader_config = show_gs ? Pica::g_state.regs.gs : Pica::g_state.regs.vs;
    for (auto instr : shader_setup.program_code)
        info.code.push_back({instr});
    int num_attributes = Pica::g_state.regs.vertex_attributes.GetNumTotalAttributes();

    for (auto pattern : shader_setup.swizzle_data)
        info.swizzle_info.push_back({pattern});

    u32 entry_point = shader_config.main_offset;
    info.labels.insert({ entry_point, "main" });

    // Generate debug information using a virtual shader unit
    Pica::Shader::UnitState<true> shader_unit;
    for (unsigned i = 0; i < ARRAY_SIZE(shader_unit.registers.temporary); i++) {
        shader_unit.registers.temporary[i] = Pica::g_state.shader_units[3].registers.temporary[i];
    }
    if (show_gs) {
        using Pica::Shader::OutputVertex;
        auto AddTriangle = [](
            const OutputVertex& v0, const OutputVertex& v1, const OutputVertex& v2) {
            //TODO: Do cool stuff
        };
        shader_unit.emit_triangle_callback = AddTriangle;
    }
    debug_data = shader_setup.ProduceDebugInfo(shader_unit, input_vertex, num_attributes, shader_config);

    // Reload widget state
    for (int attr = 0; attr < num_attributes; ++attr) {
        unsigned source_attr = shader_config.input_register_map.GetRegisterForAttribute(attr);
        input_data_mapping[attr]->setText(QString("-> v%1").arg(source_attr));
        input_data_container[attr]->setVisible(true);
    }
    // Only show input attributes which are used as input to the shader
    for (unsigned int attr = num_attributes; attr < 16; ++attr) {
        input_data_container[attr]->setVisible(false);
    }

    // Initialize debug info text for current cycle count
    cycle_index->setMaximum(static_cast<int>(debug_data.records.size() - 1));
    OnCycleIndexChanged(cycle_index->value());

    model->endResetModel();
}

void GraphicsVertexShaderWidget::OnResumed() {
    widget()->setEnabled(false);
}

void GraphicsVertexShaderWidget::OnInputAttributeChanged(int index) {
    float value = input_data[index]->text().toFloat();
    input_vertex.attr[index / 4][index % 4] = Pica::float24::FromFloat32(value);
    // Re-execute shader with updated value
    Reload();
}

void GraphicsVertexShaderWidget::OnCycleIndexChanged(int index) {
    QString text;

    auto& record = debug_data.records[index];
    if (record.mask & Pica::Shader::DebugDataRecord::SRC1)
        text += tr("SRC1: %1, %2, %3, %4\n").arg(record.src1.x.ToFloat32()).arg(record.src1.y.ToFloat32()).arg(record.src1.z.ToFloat32()).arg(record.src1.w.ToFloat32());
    if (record.mask & Pica::Shader::DebugDataRecord::SRC2)
        text += tr("SRC2: %1, %2, %3, %4\n").arg(record.src2.x.ToFloat32()).arg(record.src2.y.ToFloat32()).arg(record.src2.z.ToFloat32()).arg(record.src2.w.ToFloat32());
    if (record.mask & Pica::Shader::DebugDataRecord::SRC3)
        text += tr("SRC3: %1, %2, %3, %4\n").arg(record.src3.x.ToFloat32()).arg(record.src3.y.ToFloat32()).arg(record.src3.z.ToFloat32()).arg(record.src3.w.ToFloat32());
    if (record.mask & Pica::Shader::DebugDataRecord::DEST_IN)
        text += tr("DEST_IN: %1, %2, %3, %4\n").arg(record.dest_in.x.ToFloat32()).arg(record.dest_in.y.ToFloat32()).arg(record.dest_in.z.ToFloat32()).arg(record.dest_in.w.ToFloat32());
    if (record.mask & Pica::Shader::DebugDataRecord::DEST_OUT)
        text += tr("DEST_OUT: %1, %2, %3, %4\n").arg(record.dest_out.x.ToFloat32()).arg(record.dest_out.y.ToFloat32()).arg(record.dest_out.z.ToFloat32()).arg(record.dest_out.w.ToFloat32());

    if (record.mask & Pica::Shader::DebugDataRecord::ADDR_REG_OUT)
        text += tr("Addres Registers: %1, %2\n").arg(record.address_registers[0]).arg(record.address_registers[1]);
    if (record.mask & Pica::Shader::DebugDataRecord::CMP_RESULT)
        text += tr("Compare Result: %1, %2\n").arg(record.conditional_code[0] ? "true" : "false").arg(record.conditional_code[1] ? "true" : "false");

    if (record.mask & Pica::Shader::DebugDataRecord::COND_BOOL_IN)
        text += tr("Static Condition: %1\n").arg(record.cond_bool ? "true" : "false");
    if (record.mask & Pica::Shader::DebugDataRecord::COND_CMP_IN)
        text += tr("Dynamic Conditions: %1, %2\n").arg(record.cond_cmp[0] ? "true" : "false").arg(record.cond_cmp[1] ? "true" : "false");
    if (record.mask & Pica::Shader::DebugDataRecord::LOOP_INT_IN)
        text += tr("Loop Parameters: %1 (repeats), %2 (initializer), %3 (increment), %4\n").arg(record.loop_int.x).arg(record.loop_int.y).arg(record.loop_int.z).arg(record.loop_int.w);

    text += tr("Instruction offset: 0x%1").arg(4 * record.instruction_offset, 4, 16, QLatin1Char('0'));
    if (record.mask & Pica::Shader::DebugDataRecord::NEXT_INSTR) {
        text += tr(" -> 0x%2").arg(4 * record.next_instruction, 4, 16, QLatin1Char('0'));
    } else {
        text += tr(" (last instruction)");
    }

    instruction_description->setText(text);

    // Emit model update notification and scroll to current instruction
    QModelIndex instr_index = model->index(record.instruction_offset, 0);
    emit model->dataChanged(instr_index, model->index(record.instruction_offset, model->columnCount()));
    binary_list->scrollTo(instr_index, QAbstractItemView::EnsureVisible);
}
