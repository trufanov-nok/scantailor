/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C)  Joseph Artsimovich <joseph.artsimovich@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
//begin of modified by monday2000
//Export_Subscans
// This file was added by monday2000

#include "ExportDialog.h"

#include "OpenGLSupport.h"
#include "config.h"
#include "settings/ini_keys.h"
#include <QVariant>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>

namespace exporting {

ExportDialog::ExportDialog(QWidget* parent, const QString& defaultOutDir)
    :   QDialog(parent), m_defaultOutDir(defaultOutDir)
{
    ui.setupUi(this);

    connect(ui.DefaultOutputFolder, SIGNAL(toggled(bool)), this, SLOT(OnCheckDefaultOutputFolder(bool)));
    connect(ui.ExportButton, SIGNAL(clicked()), this, SLOT(OnClickExport()));

    connect(ui.outExportDirBrowseBtn, SIGNAL(clicked()), this, SLOT(outExportDirBrowse()));

    connect(ui.outExportDirLine, SIGNAL(textEdited(QString)),
            this, SLOT(outExportDirEdited(QString))
           );

    ExportModes mode = (ExportModes) m_settings.value(_key_export_split_mixed_settings, _key_export_split_mixed_settings_def).toInt();
    displayExportMode(mode);

    ui.DefaultOutputFolder->setChecked(m_settings.value(_key_export_default_output_folder, true).toBool());
    ui.labelFilesProcessed->clear();
    ui.btnResetToDefault->show();
    ui.ExportButton->setText(tr("Export"));
    ui.OkButton->setText(tr("Close"));
    ui.tabWidget->setCurrentIndex(0); // as I often forget to set this right in ui designer
    //connect(ui.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));

    connect(ui.GenerateBlankBackSubscans, SIGNAL(toggled(bool)), this, SLOT(OnCheckGenerateBlankBackSubscans(bool)));
    connect(ui.UseSepSuffixForPics, SIGNAL(toggled(bool)), this, SLOT(OnCheckUseSepSuffixForPics(bool)));
    connect(ui.KeepOriginalColorIllumForeSubscans, SIGNAL(toggled(bool)), this, SLOT(OnCheckKeepOriginalColorIllumForeSubscans(bool)));

    ui.GenerateBlankBackSubscans->setChecked(m_settings.value(_key_export_generate_blank_subscans, _key_export_generate_blank_subscans_def).toBool());
    ui.UseSepSuffixForPics->setChecked(m_settings.value(_key_export_use_sep_suffix, _key_export_use_sep_suffix_def).toBool());
    ui.KeepOriginalColorIllumForeSubscans->setChecked(m_settings.value(_key_export_keep_original_color, _key_export_keep_original_color_def).toBool());
    ui.cbMultipageOutput->setChecked(m_settings.value(_key_export_to_multipage, _key_export_to_multipage_def).toBool());
}

ExportDialog::~ExportDialog()
{
}

void
ExportDialog::displayExportMode(ExportModes mode)
{
    ui.cbExportImage->setChecked(mode.testFlag(ExportMode::WholeImage));
    ui.cbExportWithoutOutputStage->setChecked(mode.testFlag(ExportMode::ImageWithoutOutputStage));
    ui.cbExportForeground->setChecked(mode.testFlag(ExportMode::Foreground));
    ui.cbExportBackground->setChecked(mode.testFlag(ExportMode::Background));
    ui.cbExportAutomask->setChecked(mode.testFlag(ExportMode::AutoMask));
    ui.cbExportMask->setChecked(mode.testFlag(ExportMode::Mask));
    ui.cbExportZones->setChecked(mode.testFlag(ExportMode::Zones));
}

void
ExportDialog::OnCheckDefaultOutputFolder(bool state)
{

    m_settings.setValue(_key_export_default_output_folder, state);
    ui.groupBoxExport->setEnabled(!state);
    if (state) {
        m_prevOutDir = ui.outExportDirLine->text();
        ui.outExportDirLine->setText(m_defaultOutDir);
    } else {
        ui.outExportDirLine->setText(m_prevOutDir);
    }
}

void
ExportDialog::OnClickExport()
{
    if (ui.outExportDirLine->text().isEmpty() &&
            !ui.DefaultOutputFolder->isChecked()) {
        QMessageBox::warning(
            this, tr("Error"),
            tr("The export output directory is empty.")
        );
        return;
    }

    QDir const out_dir(ui.outExportDirLine->text());

    if (out_dir.isAbsolute() && !out_dir.exists()) {
        // Maybe create it.
        bool create = m_autoOutDir;
        if (!m_autoOutDir) {
            create = QMessageBox::question(
                         this, tr("Create Directory?"),
                         tr("The export output directory doesn't exist. Create it?"),
                         QMessageBox::Yes | QMessageBox::No
                     ) == QMessageBox::Yes;
            if (!create) {
                return;
            }
        }
        if (create) {
            if (!out_dir.mkpath(out_dir.path())) {
                QMessageBox::warning(
                    this, tr("Error"),
                    tr("Unable to create the export output directory.")
                );
                return;
            }
        }
    }
    if ((!out_dir.isAbsolute() || !out_dir.exists()) && !ui.DefaultOutputFolder->isChecked()) {

        QMessageBox::warning(
            this, tr("Error"),
            tr("The export output directory is not set or doesn't exist.")
        );
        return;
    }

    if (ui.ExportButton->text() != tr("Stop")) {
        emit SetStartExportSignal();
    } else {
        emit ExportStopSignal();
    }
}

void
ExportDialog::outExportDirBrowse()
{
    QString initial_dir(ui.outExportDirLine->text());
    if (initial_dir.isEmpty() || !QDir(initial_dir).exists()) {
        initial_dir = QDir::home().absolutePath();
    }

    QString const dir(
        QFileDialog::getExistingDirectory(
            this, tr("Export output directory"), initial_dir
        )
    );

    if (!dir.isEmpty()) {
        setExportOutputDir(dir);
    }
}

void
ExportDialog::setExportOutputDir(QString const& dir)
{
    ui.outExportDirLine->setText(QDir::toNativeSeparators(dir));
}

void
ExportDialog::outExportDirEdited(QString const& /*text*/)
{
    m_autoOutDir = false;
}

void
ExportDialog::setCount(int count)
{
    m_count = count;
    ui.progressBar->setMaximum(m_count);
}

void
ExportDialog::stepProgress()
{
    if (ui.btnResetToDefault->isVisible()) {
        ui.btnResetToDefault->hide();
    }
    const QString txt = tr("Processed pages %1 of %2");
    ui.labelFilesProcessed->setText(txt.arg(ui.progressBar->value() + 1).arg(m_count));
    ui.progressBar->setValue(ui.progressBar->value() + 1);
}

void
ExportDialog::exportCanceled()
{
    QMessageBox::information(this, tr("Information"), tr("The files export is stopped by the user."));
    reset();
}

void
ExportDialog::exportCompleted()
{
    setExportLabel();
    QMessageBox::information(this, tr("Information"), tr("The files export is finished."));
    reset();
}

void
ExportDialog::exportError(QString const& errorStr)
{
    QMessageBox::critical(this, tr("Error"), errorStr);
    reset();
}

void
ExportDialog::startExport(void)
{
    ExportModes mode = ExportMode::None;

    if (ui.cbExportImage->isChecked()) {
        mode |= ExportMode::WholeImage;
    }
    if (ui.cbExportZones->isChecked()) {
        mode |= ExportMode::ImageWithoutOutputStage;
    }
    if (ui.cbExportForeground->isChecked()) {
        mode |= ExportMode::Foreground;
    }
    if (ui.cbExportBackground->isChecked()) {
        mode |= ExportMode::Background;
    }
    if (ui.cbExportAutomask->isChecked()) {
        mode |= ExportMode::AutoMask;
    }
    if (ui.cbExportMask->isChecked()) {
        mode |= ExportMode::Mask;
    }
    if (ui.cbExportZones->isChecked()) {
        mode |= ExportMode::Zones;
    }

    if (mode == ExportMode::None) {
        QMessageBox::warning(this,  tr("Error"), tr("Nothing to export. Please select some data to export."));
        reset();
        return;
    }

    ExportSettings settings;
    settings.mode = mode;
    settings.default_out_dir = ui.DefaultOutputFolder->isChecked();
    settings.export_dir_path = ui.outExportDirLine->text();
    settings.export_to_multipage = ui.cbMultipageOutput->isChecked();
    settings.save_blank_background = ui.GenerateBlankBackSubscans->isChecked();
    settings.use_sep_suffix_for_pics = ui.UseSepSuffixForPics->isChecked();
    settings.page_gen_tweaks = PageGenTweak::NoTweaks;
    // forced as export is intended to be used with DjVu Imager. Still this option added for usage in Publish processing stage.
    settings.save_blank_foreground = true;
#ifdef TARGET_OS_MAC
    // for compatibility with Qt 5.5
    if (ui.KeepOriginalColorIllumForeSubscans->isChecked()) {
        settings.page_gen_tweaks |= PageGenTweak::KeepOriginalColorIllumForeSubscans;
    } else {
        settings.page_gen_tweaks &= !PageGenTweak::KeepOriginalColorIllumForeSubscans;
    }

    if (mode.testFlag(ExportMode::ImageWithoutOutputStage)) {
        settings.page_gen_tweaks |= PageGenTweak::IgnoreOutputProcessingStage;
    } else {
        settings.page_gen_tweaks &= !PageGenTweak::IgnoreOutputProcessingStage;
    }
#else
    settings.page_gen_tweaks.setFlag(PageGenTweak::KeepOriginalColorIllumForeSubscans, ui.KeepOriginalColorIllumForeSubscans->isChecked());
    settings.page_gen_tweaks.setFlag(PageGenTweak::IgnoreOutputProcessingStage, mode.testFlag(ExportMode::ImageWithoutOutputStage));
#endif
    settings.export_selected_pages_only = ui.cbExportSelected->isChecked();

    emit ExportOutputSignal(settings);
}

void
ExportDialog::reset(void)
{
    ui.btnResetToDefault->show();
    ui.labelFilesProcessed->clear();
    ui.progressBar->setValue(0);
    ui.ExportButton->setText(tr("Export"));
}

void
ExportDialog::setExportLabel(void)
{
    ui.ExportButton->setText(tr("Export"));
}

void
ExportDialog::setStartExport(void)
{
    m_count = 0;

    ui.btnResetToDefault->hide();
    ui.progressBar->setValue(0);
    ui.labelFilesProcessed->setText(tr("Starting the export..."));
    ui.ExportButton->setText(tr("Stop"));

    QTimer::singleShot(1, this, SLOT(startExport()));
}

void
ExportDialog::OnCheckGenerateBlankBackSubscans(bool state)
{
    m_settings.setValue(_key_export_generate_blank_subscans, state);
}

void
ExportDialog::OnCheckUseSepSuffixForPics(bool state)
{
    m_settings.setValue(_key_export_use_sep_suffix, state);
}

void
ExportDialog::OnCheckKeepOriginalColorIllumForeSubscans(bool state)
{
    m_settings.setValue(_key_export_keep_original_color, state);
}

void
ExportDialog::saveExportMode(ExportMode val, bool on)
{
    ExportModes mode = (ExportModes) m_settings.value(_key_export_split_mixed_settings, ExportMode::Foreground | ExportMode::Background).toInt();
#ifdef TARGET_OS_MAC
    // for compatibility with Qt 5.5
    if (on) {
        mode |= val;
    } else {
        mode &= !val;
    }
#else
    mode.setFlag(val, on);
#endif
    m_settings.setValue(_key_export_split_mixed_settings, (int) mode);
}

void ExportDialog::on_cbExportForeground_stateChanged(int arg1)
{
    saveExportMode(ExportMode::Foreground, arg1);
}

void ExportDialog::on_cbExportBackground_stateChanged(int arg1)
{
    saveExportMode(ExportMode::Background, arg1);
}

void ExportDialog::on_cbExportMask_stateChanged(int arg1)
{
    saveExportMode(ExportMode::Mask, arg1);
}

void ExportDialog::on_cbExportZones_stateChanged(int arg1)
{
    saveExportMode(ExportMode::Zones, arg1);
}

void ExportDialog::on_cbMultipageOutput_toggled(bool checked)
{
    m_settings.setValue(_key_export_to_multipage, checked);
}

void ExportDialog::on_cbExportImage_stateChanged(int arg1)
{
    saveExportMode(ExportMode::WholeImage, arg1);
}

void ExportDialog::on_cbExportAutomask_stateChanged(int arg1)
{
    saveExportMode(ExportMode::AutoMask, arg1);
}

void ExportDialog::on_cbExportWithoutOutputStage_stateChanged(int arg1)
{
    saveExportMode(ExportMode::ImageWithoutOutputStage, arg1);
}

void ExportDialog::on_btnResetToDefault_clicked()
{
    displayExportMode(ExportModes(_key_export_split_mixed_settings_def));
    ui.GenerateBlankBackSubscans->setChecked(_key_export_generate_blank_subscans_def);
    ui.UseSepSuffixForPics->setChecked(_key_export_use_sep_suffix_def);
    ui.KeepOriginalColorIllumForeSubscans->setChecked(_key_export_keep_original_color_def);
    ui.cbMultipageOutput->setChecked(_key_export_to_multipage_def);
}

}
