// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "citra_qt/debugger/graphics/graphics_breakpoint_observer.h"

class GraphicsCombinerWidget : public BreakPointObserverDock {
    Q_OBJECT

    using Event = Pica::DebugContext::Event;

public:
    GraphicsCombinerWidget(std::shared_ptr<Pica::DebugContext> debug_context,
                           QWidget* parent = nullptr);

private slots:
    void OnBreakPointHit(Pica::DebugContext::Event event, void* data) override;
    void OnResumed() override;

    void Reload();

private:
    QLabel* depth_label;
    QLabel* blend_label;
    QLabel* tev_stages_label;
    QLabel* fog_label;
    QLabel* fog_lut_label;
};
