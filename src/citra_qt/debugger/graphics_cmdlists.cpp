// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QListView>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeView>
#include <QVBoxLayout>

#include "citra_qt/debugger/graphics_cmdlists.h"
#include "citra_qt/util/spinbox.h"
#include "citra_qt/util/util.h"

#include "common/vector_math.h"

#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/debug_utils/debug_utils.h"

QImage LoadTexture(u8* src, const Pica::DebugUtils::TextureInfo& info) {
    QImage decoded_image(info.width, info.height, QImage::Format_ARGB32);
    for (int y = 0; y < info.height; ++y) {
        for (int x = 0; x < info.width; ++x) {
            Math::Vec4<u8> color = Pica::DebugUtils::LookupTexture(src, x, y, info, true);
            decoded_image.setPixel(x, y, qRgba(color.r(), color.g(), color.b(), color.a()));
        }
    }

    return decoded_image;
}

class TextureInfoWidget : public QWidget {
public:
    TextureInfoWidget(u8* src, const Pica::DebugUtils::TextureInfo& info, QWidget* parent = nullptr) : QWidget(parent) {
        QLabel* image_widget = new QLabel;
        QPixmap image_pixmap = QPixmap::fromImage(LoadTexture(src, info));
        image_pixmap = image_pixmap.scaled(200, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        image_widget->setPixmap(image_pixmap);

        QVBoxLayout* layout = new QVBoxLayout;
        layout->addWidget(image_widget);
        setLayout(layout);
    }
};

GPUCommandListModel::GPUCommandListModel(QObject* parent) : QAbstractListModel(parent) {

}

int GPUCommandListModel::rowCount(const QModelIndex& parent) const {
    return Pica::g_state.regs.NumIds();
}

int GPUCommandListModel::columnCount(const QModelIndex& parent) const {
    return 4;
}

QVariant GPUCommandListModel::data(const QModelIndex& index, int role) const {
#if 1
    if (!index.isValid())
        return QVariant();

    unsigned int reg_index = index.row();
    u32 reg_value = Pica::g_state.regs[reg_index];

    if (role == Qt::DisplayRole) {
        QString content;
        switch ( index.column() ) {
        case 0:
            return QString::fromLatin1(Pica::Regs::GetCommandName(reg_index).c_str());
        case 1:
            return QString("%1").arg(reg_index, 3, 16, QLatin1Char('0'));
        case 2:
            return QString("%1").arg(reg_value, 8, 16, QLatin1Char('0'));
        }
    } else if (role == CommandIdRole) {
        return QVariant::fromValue<int>(reg_index);
    }
#endif
    return QVariant();
}

QVariant GPUCommandListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    switch(role) {
    case Qt::DisplayRole:
    {
        switch (section) {
        case 0:
            return tr("Register Name");
        case 1:
            return tr("Register");
        case 2:
            return tr("Value");
        }

        break;
    }
    }

    return QVariant();
}

#define COMMAND_IN_RANGE(cmd_id, reg_name)   \
    (cmd_id >= PICA_REG_INDEX(reg_name) &&   \
     cmd_id < PICA_REG_INDEX(reg_name) + sizeof(decltype(Pica::g_state.regs.reg_name)) / 4)

void GPUCommandListWidget::OnCommandDoubleClicked(const QModelIndex& index) {
    const unsigned int command_id = list_widget->model()->data(index, GPUCommandListModel::CommandIdRole).toUInt();
    if (COMMAND_IN_RANGE(command_id, texture0) ||
        COMMAND_IN_RANGE(command_id, texture1) ||
        COMMAND_IN_RANGE(command_id, texture2)) {

        unsigned index;
        if (COMMAND_IN_RANGE(command_id, texture0)) {
            index = 0;
        } else if (COMMAND_IN_RANGE(command_id, texture1)) {
            index = 1;
        } else if (COMMAND_IN_RANGE(command_id, texture2)) {
            index = 2;
        } else {
            UNREACHABLE_MSG("Unknown texture command");
        }
        auto config = Pica::g_state.regs.GetTextures()[index].config;
        auto format = Pica::g_state.regs.GetTextures()[index].format;
        auto info = Pica::DebugUtils::TextureInfo::FromPicaRegister(config, format);

        // TODO: Open a surface debugger
    }
}

void GPUCommandListWidget::SetCommandInfo(const QModelIndex& index) {
    QWidget* new_info_widget = nullptr;

    const unsigned int command_id = list_widget->model()->data(index, GPUCommandListModel::CommandIdRole).toUInt();
    if (COMMAND_IN_RANGE(command_id, texture0) ||
        COMMAND_IN_RANGE(command_id, texture1) ||
        COMMAND_IN_RANGE(command_id, texture2)) {

        unsigned index;
        if (COMMAND_IN_RANGE(command_id, texture0)) {
            index = 0;
        } else if (COMMAND_IN_RANGE(command_id, texture1)) {
            index = 1;
        } else {
            index = 2;
        }
        auto config = Pica::g_state.regs.GetTextures()[index].config;
        auto format = Pica::g_state.regs.GetTextures()[index].format;

        auto info = Pica::DebugUtils::TextureInfo::FromPicaRegister(config, format);
        u8* src = Memory::GetPhysicalPointer(config.GetPhysicalAddress());
        new_info_widget = new TextureInfoWidget(src, info);
    }
    if (command_info_widget) {
        delete command_info_widget;
        command_info_widget = nullptr;
    }
    if (new_info_widget) {
        widget()->layout()->addWidget(new_info_widget);
        command_info_widget = new_info_widget;
    }
}
#undef COMMAND_IN_RANGE

GPUCommandListWidget::GPUCommandListWidget(std::shared_ptr< Pica::DebugContext > debug_context,
                                                 QWidget* parent)
        : BreakPointObserverDock(debug_context, "Pica Registers", parent) {
    setObjectName("PicaRegisters");
    model = new GPUCommandListModel(this);

    QWidget* main_widget = new QWidget;

    list_widget = new QTreeView;
    list_widget->setModel(model);
    list_widget->setFont(GetMonospaceFont());
    list_widget->setRootIsDecorated(false);
    list_widget->setUniformRowHeights(true);

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    list_widget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
    list_widget->header()->setResizeMode(QHeaderView::ResizeToContents);
#endif

    connect(list_widget->selectionModel(), SIGNAL(currentChanged(const QModelIndex&,const QModelIndex&)),
            this, SLOT(SetCommandInfo(const QModelIndex&)));
    connect(list_widget, SIGNAL(doubleClicked(const QModelIndex&)),
            this, SLOT(OnCommandDoubleClicked(const QModelIndex&)));

    QPushButton* copy_all = new QPushButton(tr("Copy All"));
    connect(copy_all, SIGNAL(clicked()), this, SLOT(CopyAllToClipboard()));

    command_info_widget = nullptr;

    QVBoxLayout* main_layout = new QVBoxLayout;
    main_layout->addWidget(list_widget);
    {
        QHBoxLayout* sub_layout = new QHBoxLayout;
        sub_layout->addWidget(copy_all);
        main_layout->addLayout(sub_layout);
    }
    main_widget->setLayout(main_layout);

    setWidget(main_widget);

    widget()->setEnabled(false);
}


//FIXME: do in breakpoints..
void GPUCommandListWidget::OnBreakPointHit(Pica::DebugContext::Event event, void* data) {
// FIXME: Can do stuff like..   if (event == Pica::DebugContext::Event::VertexLoaded)
    Reload();
    widget()->setEnabled(true);
}

void GPUCommandListWidget::Reload() {
    model->beginResetModel();

#if 0
    // Reload shader code
    info.Clear();
#endif

#if 0
    auto& shader_setup = Pica::g_state.vs;
    auto& shader_config = Pica::g_state.regs.vs;
    for (auto instr : shader_setup.program_code)
        info.code.push_back({instr});
    int num_attributes = Pica::g_state.regs.vertex_attributes.GetNumTotalAttributes();

    for (auto pattern : shader_setup.swizzle_data)
        info.swizzle_info.push_back({pattern});

    u32 entry_point = Pica::g_state.regs.vs.main_offset;
    info.labels.insert({ entry_point, "main" });

    // Generate debug information
    debug_data = Pica::Shader::ProduceDebugInfo(input_vertex, num_attributes, shader_config, shader_setup);

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
    cycle_index->setMaximum(debug_data.records.size() - 1);
    OnCycleIndexChanged(cycle_index->value());
#endif

    model->endResetModel();
}

void GPUCommandListWidget::OnResumed() {
    widget()->setEnabled(false);
}

void GPUCommandListWidget::CopyAllToClipboard() {
    QClipboard* clipboard = QApplication::clipboard();
    QString text;

    QAbstractItemModel* model = static_cast<QAbstractItemModel*>(list_widget->model());

    for (int row = 0; row < model->rowCount({}); ++row) {
        for (int col = 0; col < model->columnCount({}); ++col) {
            QModelIndex index = model->index(row, col);
            text += model->data(index).value<QString>();
            text += '\t';
        }
        text += '\n';
    }

    clipboard->setText(text);
}
