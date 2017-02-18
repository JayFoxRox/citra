// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
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

    auto main_scrollarea = new QScrollArea;
    main_scrollarea->setFrameShape(QFrame::NoFrame);

    auto depth_group = new QGroupBox(tr("Depth"));
    {
        QVBoxLayout* vbox = new QVBoxLayout;

        depth_label = new QLabel;
        depth_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        vbox->addWidget(depth_label);

        depth_group->setLayout(vbox);
    }
    main_layout->addWidget(depth_group);

    auto blend_group = new QGroupBox(tr("Blending"));
    {
        QVBoxLayout* vbox = new QVBoxLayout;

        blend_label = new QLabel;
        blend_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        vbox->addWidget(blend_label);

        blend_group->setLayout(vbox);
    }
    main_layout->addWidget(blend_group);

    auto tev_stages_group = new QGroupBox(tr("Tev stages"));
    {
        QVBoxLayout* vbox = new QVBoxLayout;

        tev_stages_label = new QLabel;
        tev_stages_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        vbox->addWidget(tev_stages_label);

        tev_stages_group->setLayout(vbox);
    }
    main_layout->addWidget(tev_stages_group);

    auto fog_group = new QGroupBox(tr("Fog"));
    {
        QVBoxLayout* vbox = new QVBoxLayout;

        fog_label = new QLabel;
        fog_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        vbox->addWidget(fog_label);

        fog_lut_label = new QLabel;
        vbox->addWidget(fog_lut_label);

        fog_group->setLayout(vbox);
    }
    main_layout->addWidget(fog_group);

    main_layout->setSizeConstraint(QLayout::SetMinimumSize);
    main_widget->setLayout(main_layout);

    main_scrollarea->setWidget(main_widget);

    setWidget(main_scrollarea);

    widget()->setEnabled(false);
}

void GraphicsCombinerWidget::Reload() {
    const auto& regs = Pica::g_state.regs;

    depth_label->setText(
        QString("w-buffer: %0\n")
            .arg((regs.depthmap_enable == Pica::Regs::DepthBuffering::WBuffering) ? "yes" : "no") +
        QString("Depth offset: %0\n")
            .arg(Pica::float24::FromRaw(regs.viewport_depth_near_plane).ToFloat32(), 0, 'e', 3) +
        QString("Depth scale: %0")
            .arg(Pica::float24::FromRaw(regs.viewport_depth_range).ToFloat32(), 0, 'e', 3));

    auto getBlendEquation = []() -> std::string {
        /*
                Add = 0,
                Subtract = 1,
                %1 - %0
                Min(%0, %1)
                Max(%0, %1)
        */
        return "";
    };

    auto getBlendFactor = []() -> std::string {
        /*
                Zero = 0,
                One = 1,
                SourceColor = 2,
                OneMinusSourceColor = 3,
                DestColor = 4,
                OneMinusDestColor = 5,
                SourceAlpha = 6,
                OneMinusSourceAlpha = 7,
                DestAlpha = 8,
                OneMinusDestAlpha = 9,
                ConstantColor = 10,
                OneMinusConstantColor = 11,
                ConstantAlpha = 12,
                OneMinusConstantAlpha = 13,
                SourceAlphaSaturate = 14,
        */
        return "";
    };

    if (regs.output_merger.alphablend_enable) {
        blend_label->setText(
            QString("Mode: Alpha blending\n") +
            QString("Blend equation: %0, %1\n")
                .arg(static_cast<int>(regs.output_merger.alpha_blending.blend_equation_rgb.Value()))
                .arg(static_cast<int>(regs.output_merger.alpha_blending.blend_equation_a.Value())) +
            QString("Blend source factor: %0, %1\n")
                .arg(static_cast<int>(regs.output_merger.alpha_blending.factor_source_rgb.Value()))
                .arg(static_cast<int>(regs.output_merger.alpha_blending.factor_source_a.Value())) +
            QString("Blend destination factor: %0, %1\n")
                .arg(static_cast<int>(regs.output_merger.alpha_blending.factor_dest_rgb.Value()))
                .arg(static_cast<int>(regs.output_merger.alpha_blending.factor_dest_a.Value())) +
            QString("Alpha testing: %0, func: %1, ref: %2")
                .arg(static_cast<int>(regs.output_merger.alpha_test.enable))
                .arg(static_cast<int>(regs.output_merger.alpha_test.func.Value()))
                .arg(static_cast<int>(regs.output_merger.alpha_test.ref)));

        // blend_const
        /*
                union {
                    BitField<0, 8, BlendEquation> ;
                    BitField<8, 8, BlendEquation> blend_equation_a;

                    BitField<16, 4, BlendFactor> factor_source_rgb;
                    BitField<20, 4, BlendFactor> factor_dest_rgb;

                    BitField<24, 4, BlendFactor> factor_source_a;
                    BitField<28, 4, BlendFactor> factor_dest_a;
                } blend_const;
                union {
                    BitField<0, 1, u32> enable;
                    BitField<4, 3, CompareFunc> func;
                    BitField<8, 8, u32> ref;
                } alpha_test;
        */
    } else {
        blend_label->setText(QString("Mode: Logic blending\n"));
    }

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

    std::string fog_modes[]{"Disabled", "Unknown", "Unknown", "Unknown",
                            "Unknown",  "Fog",     "Unknown", "Gas"};

    const auto& color = regs.fog_color;
    fog_label->setText(
        QString("mode: %0 (%1)\n")
            .arg(static_cast<int>(regs.fog_mode.Value()))
            .arg(QString::fromStdString(fog_modes[static_cast<int>(regs.fog_mode.Value())])) +
        QString("z-flip: %0\n").arg(regs.fog_flip ? "yes" : "no") +
        QString("color: %0, %1, %2 (0x%3)\n")
            .arg(color.r)
            .arg(color.g)
            .arg(color.b)
            .arg(color.raw, 8, 16, QLatin1Char('0')) +
        QString("Lookup Table:"));

    // TODO: Move this into a seperate LUT viewer
    QPixmap pixmap;
    constexpr unsigned int SUBPIXELS = 3;
    QImage fog_lut_image(128 * SUBPIXELS, 256, QImage::Format_ARGB32);
    for (unsigned int y = 0; y < 256; ++y) {
        for (unsigned int x = 0; x < (128 * SUBPIXELS); ++x) {
            auto fog_lut_entry = Pica::g_state.fog.lut[x / SUBPIXELS];
            int fog_value = fog_lut_entry.value;
            unsigned int subpixel = x % SUBPIXELS;
            uint color;
            if (subpixel == 0) {
                color = qRgba(255, 0, 0, 255);
            } else {
                fog_value += fog_lut_entry.difference.Value() * static_cast<int>(subpixel) /
                             static_cast<int>(SUBPIXELS);
                color = qRgba(255, 128, 128, 255);
            }

            // fog_value is 11 bit, we can only show 8 bit, only keep msb
            if (y > (fog_value >> (11 - 8))) {
                fog_lut_image.setPixel(x, y, color);
            } else {
                fog_lut_image.setPixel(x, y, qRgba(255, 255, 255, 255));
            }
        }
    }
    pixmap = QPixmap::fromImage(fog_lut_image);
    fog_lut_label->setPixmap(pixmap);
}

void GraphicsCombinerWidget::OnBreakPointHit(Pica::DebugContext::Event event, void* data) {
    Reload();
    widget()->setEnabled(true);
}

void GraphicsCombinerWidget::OnResumed() {
    widget()->setEnabled(false);
}
