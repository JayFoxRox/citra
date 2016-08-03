// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QAbstractListModel>
#include <QDockWidget>

#include "citra_qt/debugger/graphics_breakpoint_observer.h"

#include "video_core/gpu_debugger.h"
#include "video_core/debug_utils/debug_utils.h"

class QPushButton;
class QTreeView;

class GPUCommandListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum {
        CommandIdRole = Qt::UserRole,
    };

    GPUCommandListModel(QObject* parent);

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    friend class GPUCommandListWidget;
};

class GPUCommandListWidget : public BreakPointObserverDock {
    Q_OBJECT

    using Event = Pica::DebugContext::Event;

public:
    GPUCommandListWidget(std::shared_ptr<Pica::DebugContext> debug_context,
                         QWidget* parent = nullptr);

public slots:
    void OnCommandDoubleClicked(const QModelIndex&);

    void SetCommandInfo(const QModelIndex&);

    void CopyAllToClipboard();

private slots:
    void OnBreakPointHit(Pica::DebugContext::Event event, void* data) override;
    void OnResumed() override;

    /**
     * Reload widget based on the current PICA200 state
     */
    void Reload();

private:
    QTreeView* list_widget;
    QWidget* command_info_widget;
    GPUCommandListModel* model;

    friend class GPUCommandListModel;
};
