// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/configure_general.h"
#include "citra_qt/ui_settings.h"
#include "core/settings.h"
#include "core/system.h"
#include "ui_configure_general.h"

#include <QDir>

ConfigureGeneral::ConfigureGeneral(QWidget* parent)
    : QWidget(parent), ui(new Ui::ConfigureGeneral) {

    ui->setupUi(this);
    this->setConfiguration();

    ui->toggle_cpu_jit->setEnabled(!System::IsPoweredOn());

    // format systems language
    QString defaultLocale = QLocale::system().name(); // e.g. "de_DE"
    defaultLocale.truncate(defaultLocale.lastIndexOf('_')); // e.g. "de"

    QString m_langPath = QApplication::applicationDirPath();
    m_langPath.append("/languages");
    QDir dir(m_langPath);
    QStringList fileNames = dir.entryList(QStringList("*.qm"));

    for (int i = 0; i < fileNames.size(); ++i) {
        // get locale extracted by filename
        QString locale;
        locale = fileNames[i]; // "de.qm"
        locale.truncate(locale.lastIndexOf('.')); // "de"

        QString lang = QLocale::languageToString(QLocale(locale).language());

        ui->language_combobox->addItem(lang);
/*
        QAction *action = new QAction(ico, lang, this);
        action->setCheckable(true);
        action->setData(locale);

        ui.menuLanguage->addAction(action);
        langGroup->addAction(action);

        // set default translators and language checked
        if (defaultLocale == locale) {
            action->setChecked(true);
        }
*/
    }
}

ConfigureGeneral::~ConfigureGeneral() {}

void ConfigureGeneral::setConfiguration() {
    ui->toggle_deepscan->setChecked(UISettings::values.gamedir_deepscan);
    ui->toggle_check_exit->setChecked(UISettings::values.confirm_before_closing);
    ui->toggle_cpu_jit->setChecked(Settings::values.use_cpu_jit);
    ui->region_combobox->setCurrentIndex(Settings::values.region_value);
}

void ConfigureGeneral::applyConfiguration() {
    UISettings::values.gamedir_deepscan = ui->toggle_deepscan->isChecked();
    UISettings::values.confirm_before_closing = ui->toggle_check_exit->isChecked();
    Settings::values.region_value = ui->region_combobox->currentIndex();
    Settings::values.use_cpu_jit = ui->toggle_cpu_jit->isChecked();
    Settings::Apply();
}
