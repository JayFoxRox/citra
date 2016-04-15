// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSignalMapper>

#include "citra_qt/debugger/graphics/graphics_combiner.h"
#include "citra_qt/util/util.h"

#include "video_core/pica.h"
#include "video_core/pica_state.h"

GraphicsCombinerWidget::GraphicsCombinerWidget(std::shared_ptr<Pica::DebugContext> debug_context,
                                               QWidget* parent)
    : BreakPointObserverDock(debug_context, "Pica Combiner", parent) {
    setObjectName("PicaCombiner");

    auto main_widget = new QWidget;
    auto main_layout = new QVBoxLayout;

    auto tev_stages_group = new QGroupBox(tr("Tev stages"));
    {
        QVBoxLayout* vbox = new QVBoxLayout;

        tev_stages_label = new QLabel;
        tev_stages_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        vbox->addWidget(tev_stages_label);

        tev_stages_group->setLayout(vbox);
    }
    main_layout->addWidget(tev_stages_group);

    main_widget->setLayout(main_layout);
    setWidget(main_widget);

    widget()->setEnabled(false);
}

void GraphicsCombinerWidget::Reload() {
    const auto& regs = Pica::g_state.regs;

    auto tev_stages = regs.GetTevStages();
    auto tev_stages_str = QString();
    for (unsigned int i = 0; i < tev_stages.size(); i++) {
        const auto& tev_stage = tev_stages[i];

        auto color_str = Pica::DebugUtils::GetTevStageConfigColorCombinerString(tev_stage);
        auto alpha_str = Pica::DebugUtils::GetTevStageConfigAlphaCombinerString(tev_stage);

        if (i > 0)
            tev_stages_str += "\n";
        tev_stages_str += QString("Stage %1: ").arg(i) + QString::fromStdString(color_str) + "; " +
                          QString::fromStdString(alpha_str);
    }
    tev_stages_label->setText(tev_stages_str);
}

void GraphicsCombinerWidget::OnBreakPointHit(Pica::DebugContext::Event event, void* data) {
    Reload();
    widget()->setEnabled(true);
}

void GraphicsCombinerWidget::OnResumed() {
    widget()->setEnabled(false);
}
