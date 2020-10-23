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

#include "SettingsDialog.h"

#include "OpenGLSupport.h"
#include "config.h"
#include "imageproc/Constants.h"
#include <QVariant>
#include <QDir>
#include <QDebug>
#include <QResource>
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QStyleFactory>
#include <QProcess>
#include <QStandardPaths>
#include "MainWindow.h"
#include "filters/output/DespeckleLevel.h"
#include "filters/output/Params.h"
#include "settings/TiffCompressionInfo.h"
#include "settings/globalstaticsettings.h"

SettingsDialog::SettingsDialog(QWidget* parent)
    :   QDialog(parent), m_accepted(false)
{
    backupSettings();

    ui.setupUi(this);
    ui.lineEdit->setFiltering(true);
    connect(ui.lineEdit, SIGNAL(filterChanged(QString)), this, SLOT(filterChanged(QString)));

    ui.treeWidget->addAction(ui.actionExpand_all);
    ui.treeWidget->addAction(ui.actionCollapse_all);
    populateTreeWidget(ui.treeWidget);
    restoreSettingsTreeState(ui.treeWidget);

    connect(ui.buttonBox, SIGNAL(accepted()), SLOT(commitChanges()));
    connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &SettingsDialog::on_dialogButtonClicked);
    if (QPushButton* btn = ui.buttonBox->button(QDialogButtonBox::RestoreDefaults)) {
        btn->setToolTip(tr("Reset settings in file: %1").arg(m_settings.fileName()));
    }
    // pageGeneral is displayed by default
    initLanguageList(((MainWindow*)parent)->getLanguage());

    m_alignment = Alignment::load(&m_settings);
    connect(ui.widgetAlignment, &AlignmentWidget::alignmentChanged, this, [this]() {
        QString txt = Alignment::getVerboseDescription(*ui.widgetAlignment->alignment());
        if (txt.isEmpty()) {
            txt = tr("Not applicable. Page size is equal to content zone with margins." \
                     " So content position is defined by margins.");
        }
        ui.lblSelectedAlignment->setText(txt);
    });

    on_stackedWidget_currentChanged(0);
}

void SettingsDialog::initLanguageList(QString cur_lang)
{
    ui.language->clear();
    ui.language->addItem("English", "en");

    const QStringList language_file_filter("scantailor-universal_*.qm");
    QStringList fileNames = QDir().entryList(language_file_filter);

    if (fileNames.isEmpty()) {
        fileNames = QDir(QApplication::applicationDirPath()).entryList(language_file_filter);
        if (fileNames.isEmpty()) {
            fileNames = QDir(QString::fromUtf8(TRANSLATIONS_DIR_ABS)).entryList(language_file_filter);
            if (fileNames.isEmpty()) {
                fileNames = QDir(QString::fromUtf8(TRANSLATIONS_DIR_REL)).entryList(language_file_filter);
                if (fileNames.isEmpty()) {
                    fileNames = QDir(QApplication::applicationDirPath() + "/" +
                                     QString::fromUtf8(TRANSLATIONS_DIR_REL)).entryList(language_file_filter);
                }
            }
        }
    }

    fileNames.sort();

    for (int i = 0; i < fileNames.size(); ++i) {
        // get locale extracted by filename
        QString locale;
        locale = fileNames[i];
        locale.truncate(locale.lastIndexOf('.'));
        locale.remove(0, locale.indexOf('_') + 1);

        QString lang = QLocale::languageToString(QLocale(locale).language());
        ui.language->addItem(lang, locale);
        if (locale == cur_lang) {
            ui.language->setCurrentIndex(i + 1);
        }
    }

    ui.language->setEnabled(ui.language->count() > 0);
}

void SettingsDialog::backupSettings()
{
    m_oldSettings.clear();
    const QStringList sl = m_settings.allKeys();

    for (QString const& key : sl) {
        m_oldSettings[key] = m_settings.value(key);
    }
}

void SettingsDialog::restoreSettings()
{
    m_settings.clear();

    for (QSettings::SettingsMap::const_iterator it = m_oldSettings.constBegin(); it != m_oldSettings.constEnd(); ++it) {
        m_settings.setValue(it.key(), *it);
    }
    m_alignment = Alignment::load(&m_settings);
}

SettingsDialog::~SettingsDialog()
{
    if (!m_accepted) {
        restoreSettings();
        storeSettingsTreeState(ui.treeWidget);
    } else {
        m_alignment.save(&m_settings);
        storeSettingsTreeState(ui.treeWidget);
        emit settingsChanged();
    }

    GlobalStaticSettings::applyAppStyle(m_settings);
}

void
SettingsDialog::on_dialogButtonClicked(QAbstractButton* btn)
{
    if (ui.buttonBox->buttonRole(btn) == QDialogButtonBox::ResetRole) {
        if (QMessageBox::question(this, tr("Restore defaults"),
                                  tr("All settings will be set to their defaults. Continue?"), QMessageBox::Yes | QMessageBox::Cancel,
                                  QMessageBox::Cancel) != QMessageBox::Cancel) {
            const QStringList keys = m_settings.allKeys();
            for (const QString& key : keys) {
                if (!key.startsWith(_key_recent_projects)) {
                    m_settings.remove(key);
                }
            }
            GlobalStaticSettings::updateSettings();
            GlobalStaticSettings::updateHotkeys();
            populateTreeWidget(ui.treeWidget);
            restoreSettingsTreeState(ui.treeWidget);
            on_stackedWidget_currentChanged(0);
        }
    }
}

void
SettingsDialog::commitChanges()
{
    m_accepted = true;
    accept();
}

void
SettingsDialog::on_treeWidget_itemActivated(QTreeWidgetItem* item, int column)
{

}

void
SettingsDialog::on_treeWidget_itemSelectionChanged()
{
    QList<QTreeWidgetItem*> sItems = ui.treeWidget->selectedItems();
    if (sItems.isEmpty()) {
        ui.stackedWidget->setCurrentWidget(ui.pageEmpty);
    } else {
        ui.stackedWidget->setCurrentWidget(sItems[0]->data(0, Qt::UserRole).value<QWidget*>());
    }
}

void
SettingsDialog::setupItem(QTreeWidgetItem* item, QWidget* w, QString s, bool default_val)
{
    item->setData(0, Qt::UserRole, QVariant::fromValue(w));
    item->setData(0, Qt::UserRole + 1, s);
    if (!s.isEmpty()) {
        const bool b = m_settings.value(s, default_val).toBool();
        item->setCheckState(1, b ? Qt::Checked : Qt::Unchecked);
        w->setEnabled(b);
    }
}

void
disable_subtree(QTreeWidgetItem* item)
{
    item->setDisabled(true);
    for (int i = 0; i < item->childCount(); i++) {
        disable_subtree(item->child(i));
    }
}

void check_nested_disabled(QTreeWidgetItem* item)
{
    QString key = item->data(0, Qt::UserRole + 1).toString();
    bool disable_children = (!key.isEmpty() && item->checkState(1) != Qt::Checked);

    for (int i = 0; i < item->childCount(); i++)     {
        if (disable_children) {
            disable_subtree(item->child(i));
        } else {
            check_nested_disabled(item->child(i));
        }
    }
}

void
SettingsDialog::populateTreeWidget(QTreeWidget* treeWidget)
{
    const QStringList settingsTreeTitles = (QStringList()
                                            << tr("General")
                                            <<        tr("Hotkey management")
                                            <<        tr("Docking")
                                            <<        tr("Thumbnails panel")
                                            <<        tr("Auto-save project")
                                            <<        tr("Tiff compression")
                                            <<        tr("Debug mode"))
                                           << tr("Fix Orientation")
                                           << tr("Split pages")
                                           <<        tr("Apply cut")
                                           << tr("Deskew")
                                           <<        tr("Mark deviant pages")
                                           << tr("Select Content")
                                           <<        tr("Page detection")
                                           <<        tr("Mark deviant pages")
                                           << tr("Page layout")
                                           <<        tr("Margins")
                                           <<        tr("Alignment")
                                           <<        tr("Mark deviant pages")
                                           << tr("Output")
                                           <<        tr("Black & White mode")
                                           <<        tr("Color/Grayscale mode")
                                           <<        tr("Mixed mode")
                                           <<               tr("Auto layer")
                                           <<               tr("Picture zones layer")
                                           <<               tr("Foreground layer")
                                           <<        tr("Fill zones")
                                           <<        tr("Dewarping")
                                           <<        tr("Despeckling")
                                           << tr("Publish");
    const QResource tree_metadata(":/SettingsTreeData.tsv");
    QStringList tree_data = QString::fromUtf8((char const*)tree_metadata.data(), tree_metadata.size()).split('\n');

    QTreeWidgetItem* last_top_item = nullptr;
    QTreeWidgetItem* parent_item = nullptr;
    int idx = 0;

    treeWidget->blockSignals(true);
    treeWidget->clear();
    const int item_indentation = treeWidget->indentation();
    const QFontMetrics fm(treeWidget->font());
    int max_text_width = 0;

    for (QString const& name : settingsTreeTitles) {

        QStringList metadata = tree_data[idx++].split('\t', QString::KeepEmptyParts);
        Q_ASSERT(!metadata.isEmpty());

        int level = 0;
        int item_text_width = item_indentation + fm.width(name);

        while (metadata[level++].isEmpty()) {
            item_text_width += item_indentation;
        }

        max_text_width = std::max(max_text_width, item_text_width);

        parent_item = last_top_item; // level == 1
        for (int i = 0; i < level - 2; i++) {
            Q_ASSERT(parent_item != nullptr);
            parent_item = parent_item->child(parent_item->childCount() - 1);
        }

        if (level == 1) {
            last_top_item = new QTreeWidgetItem(treeWidget, QStringList(name));
            parent_item = last_top_item;
        } else {
            parent_item = new QTreeWidgetItem(parent_item, QStringList(name));
        }

        QString id = metadata[level - 1];
        Q_ASSERT(!id.isEmpty());

        QWidget* page = ui.stackedWidget->findChild<QWidget*>(id);
        if (page == 0) {
            page = ui.pageEmpty;
        }

        QString reg_key;
        if (metadata.size() > level) {
            reg_key = metadata[level];
        }

        bool state = true;
        if (metadata.size() > level + 1) {
            state = metadata[level + 1].toInt() > 0;
        }

        setupItem(parent_item, page, reg_key, state);

    }

    max_text_width = std::max(max_text_width + 20, 200);
    treeWidget->setColumnWidth(0, max_text_width);
    treeWidget->setColumnWidth(1, fm.width(treeWidget->headerItem()->text(1)) + 20);

    treeWidget->blockSignals(false);

    treeWidget->setCurrentItem(treeWidget->topLevelItem(0));

    for (int i = 0; i < treeWidget->topLevelItemCount(); i++) {
        check_nested_disabled(treeWidget->topLevelItem(i));
    }
}

bool filterItem(QTreeWidgetItem* i, const QString& filter)
{
    bool visible = false;
    for (int idx = 0; idx < i->childCount(); idx++) {
        if (filterItem(i->child(idx), filter)) {
            visible = true;
        }
    }

    if (!visible) {
        visible = i->text(0).contains(filter, Qt::CaseInsensitive);
    }

    i->setHidden(!visible);
    return visible;
}

void SettingsDialog::filterChanged(const QString& filter)
{
    ui.treeWidget->blockSignals(true);
    for (int i = 0; i < ui.treeWidget->topLevelItemCount(); i++) {
        filterItem(ui.treeWidget->topLevelItem(i), filter);
    }
    ui.treeWidget->expandAll();
    ui.treeWidget->blockSignals(false);
}

void conditionalExpand(QTreeWidgetItem* item, int& idx, const QStringList& states)
{
    item->setExpanded((idx < states.count()) ? !states[idx++].isEmpty() : false);
    for (int i = 0; i < item->childCount(); i++) {
        conditionalExpand(item->child(i), idx, states);
    }
}

void SettingsDialog::restoreSettingsTreeState(QTreeWidget* treeWidget)
{
    QStringList tree_expand_state = m_settings.value(_key_app_settings_tree_state).toString().split(',', QString::KeepEmptyParts);
    int idx = 0;
    for (int i = 0; i < treeWidget->topLevelItemCount(); i++) {
        conditionalExpand(treeWidget->topLevelItem(i), idx, tree_expand_state);
    }
}

void saveExpandState(QTreeWidgetItem* item, QStringList& states)
{
    states << (item->isExpanded() ? "1" : "");
    for (int i = 0; i < item->childCount(); i++) {
        saveExpandState(item->child(i), states);
    }
}

void SettingsDialog::storeSettingsTreeState(QTreeWidget* treeWidget)
{
    QStringList tree_expand_state;
    for (int i = 0; i < treeWidget->topLevelItemCount(); i++) {
        saveExpandState(treeWidget->topLevelItem(i), tree_expand_state);
    }
    m_settings.setValue(_key_app_settings_tree_state, tree_expand_state.join(','));
}

void SettingsDialog::on_actionExpand_all_triggered()
{
    ui.treeWidget->expandAll();
}

void SettingsDialog::on_actionCollapse_all_triggered()
{
    ui.treeWidget->collapseAll();
}

void SettingsDialog::on_treeWidget_itemChanged(QTreeWidgetItem* item, int column)
{
    if (column == 1) {
        QString key = item->data(0, Qt::UserRole + 1).toString();
        bool b = item->checkState(1) == Qt::Checked;
        m_settings.setValue(key, b);
        item->data(0, Qt::UserRole).value<QWidget*>()->setEnabled(b);
        for (int i = 0; i < item->childCount(); i++) {
            QTreeWidgetItem* c = item->child(i);

            if (c->data(1, Qt::CheckStateRole).isValid()) {
                if (!b && c->checkState(1) != Qt::Unchecked) {
                    c->setCheckState(1, Qt::Unchecked);
                    ui.treeWidget->expandItem(c);
                }
            }

            c->setDisabled(!b);
        }
        if (item->childCount() > 0) {
            ui.treeWidget->expandItem(item);
        }
    }

}

void SettingsDialog::on_language_currentIndexChanged(int index)
{
    m_settings.setValue(_key_app_language, ui.language->itemData(index).toString());
}

void SettingsDialog::on_cbApplyCutDefault_clicked(bool checked)
{
    m_settings.setValue(_key_page_split_apply_cut_default, checked);
}

void SettingsDialog::loadTiffList()
{
    const bool filtered_only = !m_settings.value(_key_tiff_compr_show_all, _key_tiff_compr_show_all_def).toBool();

    ui.cbTiffFilter->blockSignals(true);
    ui.cbTiffFilter->setChecked(filtered_only);
    ui.cbTiffFilter->blockSignals(false);

    const QString old_val_bw = m_settings.value(_key_tiff_compr_method_bw, _key_tiff_compr_method_bw_def).toString();
    const QString old_val_color = m_settings.value(_key_tiff_compr_method_color, _key_tiff_compr_method_color_def).toString();

    ui.cbTiffCompressionBW->clear();
    ui.cbTiffCompressionColor->clear();

    ui.cbTiffCompressionBW->blockSignals(true);
    ui.cbTiffCompressionColor->blockSignals(true);

    for (auto it = TiffCompressions::constBegin(); it != TiffCompressions::constEnd(); it++) {
        if (!it.key().isEmpty()) {
            const TiffCompressionInfo & info = it.value();
            if (!filtered_only || info.always_shown) {
                ui.cbTiffCompressionBW->addItem(info.name, info.description);
                if (!info.for_bw_only) {
                    ui.cbTiffCompressionColor->addItem(info.name, info.description);
                }
            }
        }

    }


    ui.cbTiffCompressionBW->blockSignals(false);
    ui.cbTiffCompressionColor->blockSignals(false);

    if (ui.cbTiffCompressionBW->count()) {
        int idx = ui.cbTiffCompressionBW->findText(old_val_bw);
        if (idx < 0) {
            idx = ui.cbTiffCompressionBW->findText("LZW");
            if (idx < 0) {
                idx = 0;
            }
        }
        ui.cbTiffCompressionBW->setCurrentIndex(idx);
        if (idx == 0) {
            on_cbTiffCompressionBW_currentIndexChanged(0);
        }
    }

    if (ui.cbTiffCompressionColor->count()) {
        int idx = ui.cbTiffCompressionColor->findText(old_val_color);
        if (idx < 0) {
            idx = ui.cbTiffCompressionColor->findText("LZW");
            if (idx < 0) {
                idx = 0;
            }
        }
        ui.cbTiffCompressionColor->setCurrentIndex(idx);
        if (idx == 0) {
            on_cbTiffCompressionColor_currentIndexChanged(0);
        }
    }
}

void SettingsDialog::on_stackedWidget_currentChanged(int /*arg1*/)
{
    const QWidget* currentPage = ui.stackedWidget->currentWidget();
    if (currentPage == ui.pageApplyCut) {
        ui.cbApplyCutDefault->setChecked(m_settings.value(_key_page_split_apply_cut_default, _key_page_split_apply_cut_default_def).toBool());
    } else if (currentPage == ui.pageTiffCompression) {
        loadTiffList();
        ui.useHorizontalPredictor->setChecked(m_settings.value(_key_tiff_compr_horiz_pred, _key_tiff_compr_horiz_pred_def).toBool());
    } else if (currentPage == ui.pageAutoSaveProject) {
        ui.sbSavePeriod->setValue(abs(m_settings.value(_key_autosave_time_period_min, _key_autosave_time_period_min_def).toInt()));
    } else if (currentPage == ui.pageOutput) {
        ui.dpiDefaultXValue->setValue(m_settings.value(_key_output_default_dpi_x, _key_output_default_dpi_x_def).toUInt());
        ui.dpiDefaultYValue->setValue(m_settings.value(_key_output_default_dpi_y, _key_output_default_dpi_y_def).toUInt());

        ui.ThresholdMinValue->setValue(m_settings.value(_key_output_bin_threshold_min, _key_output_bin_threshold_min_def).toInt());
        ui.ThresholdMaxValue->setValue(m_settings.value(_key_output_bin_threshold_max, _key_output_bin_threshold_max_def).toInt());
        ui.ThresholdDefaultsValue->setMinimum(ui.ThresholdMinValue->value());
        ui.ThresholdDefaultsValue->setMaximum(ui.ThresholdMaxValue->value());
        ui.ThresholdDefaultsValue->setValue(m_settings.value(_key_output_bin_threshold_default, _key_output_bin_threshold_default_def).toInt());
        connect(ui.ThresholdMinValue, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &SettingsDialog::onThresholdValueChanged);
        connect(ui.ThresholdMaxValue, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &SettingsDialog::onThresholdValueChanged);

        ui.originalPageDisplayOnKeyHold->setChecked(m_settings.value(_key_output_show_orig_on_space, _key_output_show_orig_on_space_def).toBool());
    } else if (currentPage == ui.pageDespeckling) {
        QComboBox* cb = ui.despecklingDefaultsValue;
        cb->blockSignals(true);
        cb->clear();
        cb->addItem(tr("None"),  output::DESPECKLE_OFF);
        cb->addItem(tr("Cautious"),  output::DESPECKLE_CAUTIOUS);
        cb->addItem(tr("Normal"),  output::DESPECKLE_NORMAL);
        cb->addItem(tr("Aggressive"),  output::DESPECKLE_AGGRESSIVE);
        int def = m_settings.value(_key_output_despeckling_default_lvl, output::DESPECKLE_CAUTIOUS).toInt();

        int idx = cb->findData(def);
        cb->setCurrentIndex(idx != -1 ? idx : 0);

        cb->blockSignals(false);
    } else if (currentPage == ui.pagePictureZonesLayer) {
        ui.rectangularAreasSensitivityValue->setValue(m_settings.value(_key_picture_zones_layer_sensitivity, _key_picture_zones_layer_sensitivity_def).toInt());
    } else if (currentPage == ui.pageGeneral) {

        ui.cbStyle->blockSignals(true);
        ui.cbStyle->clear();
        const QString defaultStyleName = m_settings.value(_key_app_style, _key_app_style_def).toString();
        const QStringList styleNames = QStyleFactory::keys();
        QStringList styleNames_l;
        for (const QString& s: styleNames) {
            styleNames_l.append(s.toLower());
        }
        ui.cbStyle->addItems(styleNames_l);
        int idx = styleNames_l.indexOf(defaultStyleName);
        if (idx != -1) {
            ui.cbStyle->setCurrentIndex(idx);
        }
        ui.cbStyle->blockSignals(false);

#if defined(_WIN32) || defined(Q_OS_MAC)
        QDir dir(QCoreApplication::applicationDirPath() + "/" +
                 m_settings.value(_key_app_stylsheet_dir, _key_app_stylsheet_dir_def).toString());
#else
        QDir dir(m_settings.value(_key_app_stylsheet_dir, _key_app_stylsheet_dir_def).toString());
#endif

        QFileInfoList fl = dir.entryInfoList(QStringList("*.qss"), QDir::Files | QDir::Readable);
        ui.cbStyleSheet->blockSignals(true);
        ui.cbStyleSheet->clear();
        ui.cbStyleSheet->addItem(tr("Standart", "stylesheet"), true);
        ui.cbStyleSheet->addItem(tr("None", "stylesheet"), false);

        const QString qss_name = m_settings.value(_key_app_stylsheet_file, "").toString();
        const bool empty_palette = m_settings.value(_key_app_empty_palette, _key_app_empty_palette_def).toBool();
        idx = -1;
        for (const QFileInfo & fi: qAsConst(fl)) {
            QString fname = fi.filePath();
            if (fname == qss_name) {
                idx = ui.cbStyleSheet->count();
            }
            ui.cbStyleSheet->addItem(fi.baseName(), fname);
        }

        if (idx == -1) {
            idx = empty_palette? 1: 0;
        }
        ui.cbStyleSheet->setCurrentIndex(idx);
        ui.cbStyleSheet->blockSignals(false);

        bool val = m_settings.value(_key_batch_dialog_start_from_current, _key_batch_dialog_start_from_current_def).toBool();
        ui.startBatchProcessingDlgAllPages->setChecked(
            !val);
        ui.startBatchProcessingDlgFromSelected->setChecked(
            val);
        ui.showStartBatchProcessingDlg->setChecked(
            !m_settings.value(_key_batch_dialog_remember_choice, _key_batch_dialog_remember_choice_def).toBool());
        ui.cbDontUseNativeDlg->setChecked(
            m_settings.value(_key_dont_use_native_dialog, _key_dont_use_native_dialog_def).toBool());
    } else if (currentPage == ui.pageThumbnails) {
        ui.cbThumbsListOrder->setChecked(m_settings.value(_key_thumbnails_multiple_items_in_row, _key_thumbnails_multiple_items_in_row_def).toBool());
        ui.sbThumbsCacheImgSize->setValue(m_settings.value(_key_thumbnails_max_cache_pixmap_size, _key_thumbnails_max_cache_pixmap_size_def).toSize().height());
        ui.sbThumbsMinSpacing->setValue(m_settings.value(_key_thumbnails_min_spacing, _key_thumbnails_min_spacing_def).toInt());
        ui.sbThumbsBoundaryAdjTop->setValue(m_settings.value(_key_thumbnails_boundary_adj_top, _key_thumbnails_boundary_adj_top_def).toInt());
        ui.sbThumbsBoundaryAdjBottom->setValue(m_settings.value(_key_thumbnails_boundary_adj_bottom, _key_thumbnails_boundary_adj_bottom_def).toInt());
        ui.sbThumbsBoundaryAdjLeft->setValue(m_settings.value(_key_thumbnails_boundary_adj_left, _key_thumbnails_boundary_adj_left_def).toInt());
        ui.sbThumbsBoundaryAdjRight->setValue(m_settings.value(_key_thumbnails_boundary_adj_right, _key_thumbnails_boundary_adj_right_def).toInt());
        ui.gbFixedMaxLogicalThumbSize->setChecked(m_settings.value(_key_thumbnails_fixed_thumb_size, _key_thumbnails_fixed_thumb_size_def).toBool());
        const QSizeF max_logical_thumb_size = m_settings.value(_key_thumbnails_max_thumb_size, _key_thumbnails_max_thumb_size_def).toSizeF();
        ui.sbFixedMaxLogicalThumbSizeHeight->setValue(max_logical_thumb_size.height());
        ui.sbFixedMaxLogicalThumbSizeWidth->setValue(max_logical_thumb_size.width());
        ui.cbOrderHints->setChecked(m_settings.value(_key_thumbnails_display_order_hints, _key_thumbnails_display_order_hints_def).toBool());

    } else if (currentPage == ui.pageBlackWhiteMode) {
        ui.disableSmoothingBW->setChecked(m_settings.value(_key_mode_bw_disable_smoothing, _key_mode_bw_disable_smoothing_def).toBool());
    } else if (currentPage == ui.pageHotKeysManager) {
        ui.lblHotKeyManager->setText(GlobalStaticSettings::m_hotKeyManager.toDisplayableText());
    } else if (currentPage == ui.pageDeskew) {
        QString s("QToolButton {background: %1};");
        ui.btnColorDeskew->setStyleSheet(s.arg(m_settings.value(_key_deskew_controls_color, _key_deskew_controls_color_def).toString()));
    } else if (currentPage == ui.pageSelectContent) {
        QString s("QToolButton {background: %1};");
        ui.btnColorSelectedContent->setStyleSheet(s.arg(m_settings.value(_key_content_sel_content_color, _key_content_sel_content_color_def).toString()));
    } else if (currentPage == ui.pagePageDetecton) {
        ui.gbPageDetectionFineTuneCorners->setChecked(m_settings.value(_key_content_sel_page_detection_fine_tune_corners, _key_content_sel_page_detection_fine_tune_corners_def).toBool());
        ui.cbPageDetectionFineTuneCorners->setChecked(m_settings.value(_key_content_sel_page_detection_fine_tune_corners_is_on_by_def, _key_content_sel_page_detection_fine_tune_corners_is_on_by_def_def).toBool());
        ui.gbPageDetectionBorders->setChecked(m_settings.value(_key_content_sel_page_detection_borders, _key_content_sel_page_detection_borders_def).toBool());
        ui.gbPageDetectionTargetSize->setChecked(m_settings.value(_key_content_sel_page_detection_target_page_size_enabled, _key_content_sel_page_detection_target_page_size_enabled_def).toBool());
        ui.pageDetectionTopBorder->setValue(m_settings.value(_key_content_sel_page_detection_borders_top, _key_content_sel_page_detection_borders_top_def).toDouble());
        ui.pageDetectionLeftBorder->setValue(m_settings.value(_key_content_sel_page_detection_borders_left, _key_content_sel_page_detection_borders_left_def).toDouble());
        ui.pageDetectionRightBorder->setValue(m_settings.value(_key_content_sel_page_detection_borders_right, _key_content_sel_page_detection_borders_right).toDouble());
        ui.pageDetectionBottomBorder->setValue(m_settings.value(_key_content_sel_page_detection_borders_bottom, _key_content_sel_page_detection_borders_bottom_def).toDouble());
        const QSizeF target_size = m_settings.value(_key_content_sel_page_detection_target_page_size, _key_content_sel_page_detection_target_page_size_def).toSizeF();
        ui.pageDetectionTargetWidth->setValue(target_size.width());
        ui.pageDetectionTargetHeight->setValue(target_size.height());
    } else if (currentPage == ui.pageAlignment) {
//        m_alignment = loadAlignment(); // done via Alignment::load(QSettings*)
        bool val = m_settings.value(_key_alignment_automagnet_enabled, _key_alignment_automagnet_enabled_def).toBool();
        ui.cbAlignmentAuto->setChecked(val);
        ui.widgetAlignment->setUseAutoMagnetAlignment(val);
        val = m_settings.value(_key_alignment_original_enabled, _key_alignment_original_enabled_def).toBool();
        ui.cbAlignmentOriginal->setChecked(val);
        ui.widgetAlignment->setUseOriginalProportionsAlignment(val);

        ui.widgetAlignment->setAlignment(&m_alignment);
    } else if (currentPage == ui.pageMargins) {
        int old_idx = ui.cbMarginUnits->currentIndex();
        int idx = m_settings.value(_key_margins_default_units, _key_margins_default_units_def).toUInt();
        ui.cbMarginUnits->setCurrentIndex(idx);
        if (idx == old_idx) { // otherwise wan't be called atomatically
            on_cbMarginUnits_currentIndexChanged(idx);
        }
        ui.gbMarginsAuto->setChecked(m_settings.value(_key_margins_auto_margins_enabled, _key_margins_auto_margins_enabled_def).toBool());
        if (!ui.gbMarginsAuto->isChecked()) {
            ui.cbMarginsAuto->setChecked(false);
        } else {
            ui.cbMarginsAuto->setChecked(m_settings.value(_key_margins_auto_margins_default, _key_margins_auto_margins_default_def).toBool());
        }
    } else if (currentPage == ui.pageForegroundLayer) {
        ui.cbForegroundLayerSeparateControl->setChecked(m_settings.value(_key_output_foreground_layer_control_threshold, _key_output_foreground_layer_control_threshold_def).toBool());
    } else if (currentPage == ui.pageDewarping) {
        ui.cbTryVertHalfCorrection->setChecked(m_settings.value(_key_dewarp_auto_vert_half_correction, _key_dewarp_auto_vert_half_correction_def).toBool());
        ui.cbTryDeskewAfterDewarp->setChecked(m_settings.value(_key_dewarp_auto_deskew_after_dewarp, _key_dewarp_auto_deskew_after_dewarp_def).toBool());
    } else if (currentPage == ui.pagePublish) {
        ui.edMinidjvu->setText(m_settings.value(_key_djvu_bin_minidjvu, _key_djvu_bin_minidjvu_def).toString());
    }

}

void SettingsDialog::on_cbTiffCompressionBW_currentIndexChanged(int index)
{
    ui.lblTiffDetailsBW->setText(ui.cbTiffCompressionBW->itemData(index).toString());
    m_settings.setValue(_key_tiff_compr_method_bw, ui.cbTiffCompressionBW->currentText());
}

void SettingsDialog::on_cbTiffCompressionColor_currentIndexChanged(int index)
{
    ui.lblTiffDetailsColor->setText(ui.cbTiffCompressionColor->itemData(index).toString());
    m_settings.setValue(_key_tiff_compr_method_color, ui.cbTiffCompressionColor->currentText());
}


void SettingsDialog::on_cbTiffFilter_clicked(bool checked)
{
    m_settings.setValue(_key_tiff_compr_show_all, !checked);
    loadTiffList();
}

void SettingsDialog::on_sbSavePeriod_valueChanged(int arg1)
{
    m_settings.setValue(_key_autosave_time_period_min, arg1);
}

void SettingsDialog::onThresholdValueChanged(int)
{
    int min = ui.ThresholdMinValue->value();
    int max = ui.ThresholdMaxValue->value();

    if (min > max) { //swap values
        max = min;
        min = ui.ThresholdMaxValue->value();
    }

    if (min == max) {
        if (max < ui.ThresholdMaxValue->maximum()) {
            max++;
        } else if (min > ui.ThresholdMaxValue->minimum()) {
            min--;
        }
    }

    min = std::max(ui.ThresholdMinValue->minimum(), min);
    max = std::min(ui.ThresholdMaxValue->maximum(), max);

    if (min != ui.ThresholdMinValue->value()) {
        ui.ThresholdMinValue->setValue(min);
    }

    if (max != ui.ThresholdMaxValue->value()) {
        ui.ThresholdMaxValue->setValue(max);
    }

    m_settings.setValue(_key_output_bin_threshold_min, min);
    m_settings.setValue(_key_output_bin_threshold_max, max);

    ui.ThresholdDefaultsValue->setMinimum(min);
    ui.ThresholdDefaultsValue->setMaximum(max);
}

void SettingsDialog::on_despecklingDefaultsValue_currentIndexChanged(int index)
{
    int val = ui.despecklingDefaultsValue->itemData(index).toInt();
    m_settings.setValue(_key_output_despeckling_default_lvl, val);
}

void SettingsDialog::on_startBatchProcessingDlgAllPages_toggled(bool checked)
{
    m_settings.setValue(_key_batch_dialog_start_from_current, !checked);
}

void SettingsDialog::on_showStartBatchProcessingDlg_clicked(bool checked)
{
    m_settings.setValue(_key_batch_dialog_remember_choice, !checked);
}

void SettingsDialog::on_ThresholdDefaultsValue_valueChanged(int arg1)
{
    int val = arg1;
    val = std::max(val, ui.ThresholdMinValue->value());
    val = std::min(val, ui.ThresholdMaxValue->value());
    if (val != arg1) {
        ui.ThresholdDefaultsValue->blockSignals(true);
        ui.ThresholdDefaultsValue->setValue(val);
        ui.ThresholdDefaultsValue->blockSignals(false);
    }

    m_settings.setValue(_key_output_bin_threshold_default, val);
}

void SettingsDialog::on_dpiDefaultYValue_valueChanged(int arg1)
{
    m_settings.setValue(_key_output_default_dpi_y, arg1);
}

void SettingsDialog::on_dpiDefaultXValue_valueChanged(int arg1)
{
    m_settings.setValue(_key_output_default_dpi_x, arg1);
}

void SettingsDialog::on_useHorizontalPredictor_clicked(bool checked)
{
    m_settings.setValue(_key_tiff_compr_horiz_pred, checked);
}

void SettingsDialog::on_disableSmoothingBW_clicked(bool checked)
{
    m_settings.setValue(_key_mode_bw_disable_smoothing, checked);
}

void SettingsDialog::on_rectangularAreasSensitivityValue_valueChanged(int arg1)
{
    m_settings.setValue(_key_picture_zones_layer_sensitivity, arg1);
}

void SettingsDialog::on_originalPageDisplayOnKeyHold_clicked(bool checked)
{
    m_settings.setValue(_key_output_show_orig_on_space, checked);
}

void SettingsDialog::on_lblHotKeyManager_linkActivated(const QString& link)
{
    QStringList sl = link.split("_");
    HotKeysId id = (HotKeysId) sl[0].toUInt();
    uint seq_pos = sl[1].toUInt();
    if (const HotKeyInfo* info = GlobalStaticSettings::m_hotKeyManager.get(id)) {
        QHotKeyInputDialog input_dialog(info->editorType(), this);
        HotKeySequence seq = info->sequences()[seq_pos];
        QString val = QHotKeys::hotkeysToString(seq.m_modifierSequence, seq.m_keySequence);
        input_dialog.setTextValue(val);
        if (input_dialog.exec() == QDialog::Accepted) {
            HotKeySequence new_seq(input_dialog.modifiers(), input_dialog.keys());
            HotKeyInfo new_info = *info;
            new_info.sequences().replace(seq_pos, new_seq);
            GlobalStaticSettings::m_hotKeyManager.replace(id, new_info);
            GlobalStaticSettings::m_hotKeyManager.save(&m_settings);
            ui.lblHotKeyManager->setText(GlobalStaticSettings::m_hotKeyManager.toDisplayableText());
        }
    }

}

QHotKeyInputDialog::QHotKeyInputDialog(const KeyType& editor_type, QWidget* parent, Qt::WindowFlags flags): QInputDialog(parent, flags),
    m_editorType(editor_type),
    m_modifiersPressed(Qt::NoModifier),
    m_edit(nullptr)
{
    {
        m_modifiersList.append(Qt::Key_Control);
        m_modifiersList.append(Qt::Key_Alt);
        m_modifiersList.append(Qt::Key_Shift);
        m_modifiersList.append(Qt::Key_Meta);
    }

    setInputMode(InputMode::TextInput);
    setWindowTitle(tr("Edit key sequence"));
    if (editor_type == ModifierAllowed) {
        setLabelText(tr("Hold the modification keys (Ctrl, Shift, Alt, Meta)\nand press [Enter] to edit the shortcut:"));
    } else {
        setLabelText(tr("Hold the keys and press [Enter] to edit the shortcut:"));
    }
    QList<QLineEdit*> l = findChildren<QLineEdit*>();
    if (!l.isEmpty()) {
        m_edit = l.first();
        m_edit->setReadOnly(true);
        m_edit->installEventFilter(this);
        connect(m_edit, &QLineEdit::textChanged, this, [this](const QString & val) {
            QList<QDialogButtonBox*> l = findChildren<QDialogButtonBox*>();
            if (!l.isEmpty()) {
                l.first()->button(QDialogButtonBox::Ok)->setEnabled(!val.isEmpty());
            }
        });
    }

}

void QHotKeyInputDialog::updateLabel()
{
    setTextValue( QHotKeys::hotkeysToString(m_modifiersPressed,
                                            m_keysPressed.toList()) );
}

void QHotKeyInputDialog::keyPressEvent(QKeyEvent* event)
{
    if (event) {
        if (m_editorType & ModifierAllowed) {
            m_modifiersPressed = event->modifiers();
        }

        if (m_editorType & KeysAllowed) {
            const Qt::Key& key = (Qt::Key) event->key();
            if (!m_modifiersList.contains(key)) {
                m_keysPressed += key;
            }
        }
        updateLabel();
        event->accept();
    }
}

void QHotKeyInputDialog::keyReleaseEvent(QKeyEvent* event)
{
    if (event) {
        if (m_editorType & ModifierAllowed) {
            m_modifiersPressed = event->modifiers();
        }

        if (m_editorType & KeysAllowed) {
            const Qt::Key& key = (Qt::Key) event->key();
            if (!m_modifiersList.contains(key)) {
                m_keysPressed -= key;
            }
        }
        updateLabel();
        event->accept();
    }
}

bool QHotKeyInputDialog::eventFilter(QObject* obj, QEvent* event)
{
    if (event && event->type() == QEvent::KeyPress) {
        if (obj && obj == m_edit) {
            QKeyEvent* e = (QKeyEvent*) event;
            if (e->key() == Qt::Key_Return) {
                QList<QDialogButtonBox*> l = findChildren<QDialogButtonBox*>();
                if (!l.isEmpty()) {
                    if (l.first()->button(QDialogButtonBox::Ok)->isEnabled()) {
                        accept();
                        return true;
                    };
                }
            }
            keyPressEvent(e);
            return true;
        }
    }
    return false;
}

void SettingsDialog::on_btnResetHotKeys_clicked()
{
    GlobalStaticSettings::m_hotKeyManager.resetToDefaults();
    GlobalStaticSettings::m_hotKeyManager.save(&m_settings);
    ui.lblHotKeyManager->setText(GlobalStaticSettings::m_hotKeyManager.toDisplayableText());
}

void SettingsDialog::on_marginDefaultTopVal_valueChanged(double arg1)
{
    m_settings.setValue(_key_margins_default_top, arg1 * m_unitToMM);
}

void SettingsDialog::on_marginDefaultLeftVal_valueChanged(double arg1)
{
    m_settings.setValue(_key_margins_default_left, arg1 * m_unitToMM);
}

void SettingsDialog::on_marginDefaultRightVal_valueChanged(double arg1)
{
    m_settings.setValue(_key_margins_default_right, arg1 * m_unitToMM);
}

void SettingsDialog::on_marginDefaultBottomVal_valueChanged(double arg1)
{
    m_settings.setValue(_key_margins_default_bottom, arg1 * m_unitToMM);
}

void SettingsDialog::on_cbMarginUnits_currentIndexChanged(int index)
{
    m_settings.setValue(_key_margins_default_units, index);

    int decimals = 0;
    double step = 0.0;

    if (index == 0) { // mm
        m_mmToUnit = 1.0;
        m_unitToMM = 1.0;
        decimals = 1;
        step = 1.0;
    } else { // in
        m_mmToUnit = imageproc::constants::MM2INCH;
        m_unitToMM = imageproc::constants::INCH2MM;
        decimals = 2;
        step = 0.01;
    }

    ui.marginDefaultTopVal->setDecimals(decimals);
    ui.marginDefaultTopVal->setSingleStep(step);
    ui.marginDefaultBottomVal->setDecimals(decimals);
    ui.marginDefaultBottomVal->setSingleStep(step);
    ui.marginDefaultLeftVal->setDecimals(decimals);
    ui.marginDefaultLeftVal->setSingleStep(step);
    ui.marginDefaultRightVal->setDecimals(decimals);
    ui.marginDefaultRightVal->setSingleStep(step);

    updateMarginsDisplay();
}

void SettingsDialog::on_cbMarginsAuto_clicked(bool checked)
{
    m_settings.setValue(_key_margins_auto_margins_default, checked);
}

void SettingsDialog::updateMarginsDisplay()
{
    ui.marginDefaultTopVal->setValue(m_settings.value(_key_margins_default_top, _key_margins_default_top_def).toUInt()* m_mmToUnit);
    ui.marginDefaultBottomVal->setValue(m_settings.value(_key_margins_default_bottom, _key_margins_default_bottom_def).toUInt()* m_mmToUnit);
    ui.marginDefaultLeftVal->setValue(m_settings.value(_key_margins_default_left, _key_margins_default_left_def).toUInt()* m_mmToUnit);
    ui.marginDefaultRightVal->setValue(m_settings.value(_key_margins_default_right, _key_margins_default_right_def).toUInt()* m_mmToUnit);
}

void SettingsDialog::on_gbPageDetectionFineTuneCorners_toggled(bool arg1)
{
    m_settings.setValue(_key_content_sel_page_detection_fine_tune_corners, arg1);
}

void SettingsDialog::on_gbPageDetectionBorders_toggled(bool arg1)
{
    m_settings.setValue(_key_content_sel_page_detection_borders, arg1);
}

void SettingsDialog::on_cbPageDetectionFineTuneCorners_clicked(bool checked)
{
    m_settings.setValue(_key_content_sel_page_detection_fine_tune_corners_is_on_by_def, checked);
}

void SettingsDialog::on_pageDetectionTargetWidth_valueChanged(double arg1)
{
    m_settings.setValue(_key_content_sel_page_detection_target_page_size, QSizeF(arg1, ui.pageDetectionTargetHeight->value()));
}

void SettingsDialog::on_pageDetectionTargetHeight_valueChanged(double arg1)
{
    m_settings.setValue(_key_content_sel_page_detection_target_page_size, QSizeF(ui.pageDetectionTargetWidth->value(), arg1));
}

void SettingsDialog::on_gbPageDetectionTargetSize_toggled(bool arg1)
{
    m_settings.setValue(_key_content_sel_page_detection_target_page_size_enabled, arg1);
}

void SettingsDialog::on_pageDetectionTopBorder_valueChanged(double arg1)
{
    m_settings.setValue(_key_content_sel_page_detection_borders_top, arg1);
}

void SettingsDialog::on_pageDetectionRightBorder_valueChanged(double arg1)
{
    m_settings.setValue(_key_content_sel_page_detection_borders_right, arg1);
}

void SettingsDialog::on_pageDetectionLeftBorder_valueChanged(double arg1)
{
    m_settings.setValue(_key_content_sel_page_detection_borders_left, arg1);
}

void SettingsDialog::on_pageDetectionBottomBorder_valueChanged(double arg1)
{
    m_settings.setValue(_key_content_sel_page_detection_borders_bottom, arg1);
}

void SettingsDialog::on_cbForegroundLayerSeparateControl_clicked(bool checked)
{
    m_settings.setValue(_key_output_foreground_layer_control_threshold, checked);
}

void SettingsDialog::on_sbThumbsCacheImgSize_valueChanged(int arg1)
{
    m_settings.setValue(_key_thumbnails_max_cache_pixmap_size, QSize(arg1, arg1));
}

void SettingsDialog::on_sbThumbsMinSpacing_valueChanged(int arg1)
{
    m_settings.setValue(_key_thumbnails_min_spacing, arg1);
}

void SettingsDialog::on_sbThumbsBoundaryAdjTop_valueChanged(int arg1)
{
    m_settings.setValue(_key_thumbnails_boundary_adj_top, arg1);
}

void SettingsDialog::on_sbThumbsBoundaryAdjBottom_valueChanged(int arg1)
{
    m_settings.setValue(_key_thumbnails_boundary_adj_bottom, arg1);
}

void SettingsDialog::on_sbThumbsBoundaryAdjLeft_valueChanged(int arg1)
{
    m_settings.setValue(_key_thumbnails_boundary_adj_left, arg1);
}

void SettingsDialog::on_sbThumbsBoundaryAdjRight_valueChanged(int arg1)
{
    m_settings.setValue(_key_thumbnails_boundary_adj_right, arg1);
}

void SettingsDialog::on_cbThumbsListOrder_toggled(bool checked)
{
    m_settings.setValue(_key_thumbnails_multiple_items_in_row, checked);
}

void SettingsDialog::on_gbFixedMaxLogicalThumbSize_toggled(bool arg1)
{
    m_settings.setValue(_key_thumbnails_fixed_thumb_size, arg1);
}

void SettingsDialog::on_sbFixedMaxLogicalThumbSizeHeight_valueChanged(int arg1)
{
    m_settings.setValue(_key_thumbnails_max_thumb_size, QSizeF(ui.sbFixedMaxLogicalThumbSizeWidth->value(), arg1));
}

void SettingsDialog::on_sbFixedMaxLogicalThumbSizeWidth_valueChanged(int arg1)
{
    m_settings.setValue(_key_thumbnails_max_thumb_size, QSizeF(arg1, ui.sbFixedMaxLogicalThumbSizeHeight->value()));
}

void SettingsDialog::on_btnThumbDefaults_clicked()
{
    if (QMessageBox::question(this, tr("Restore defaults"),
                              tr("Thumbnails view settings will be reseted to their defaults. Continue?"), QMessageBox::Yes | QMessageBox::Cancel,
                              QMessageBox::Cancel) != QMessageBox::Cancel) {
        const QStringList keys = m_settings.allKeys();
        for (const QString& key : keys) {
            if (key.startsWith(_key_thumbnails_category)) {
                m_settings.remove(key);
            }
        }
    }
    on_stackedWidget_currentChanged(0);
}

void SettingsDialog::on_cbAlignmentAuto_toggled(bool checked)
{
    m_settings.setValue(_key_alignment_automagnet_enabled, checked);
    ui.widgetAlignment->setUseAutoMagnetAlignment(checked);
}

void SettingsDialog::on_cbAlignmentOriginal_toggled(bool checked)
{
    m_settings.setValue(_key_alignment_original_enabled, checked);
    ui.widgetAlignment->setUseOriginalProportionsAlignment(checked);
}

void SettingsDialog::on_gbMarginsAuto_toggled(bool arg1)
{
    m_settings.setValue(_key_margins_auto_margins_enabled, arg1);
}

void SettingsDialog::on_cbTryVertHalfCorrection_toggled(bool checked)
{
    m_settings.setValue(_key_dewarp_auto_vert_half_correction, checked);
}

void SettingsDialog::on_cbTryDeskewAfterDewarp_toggled(bool checked)
{
    m_settings.setValue(_key_dewarp_auto_deskew_after_dewarp, checked);
}

void SettingsDialog::on_cbOrderHints_toggled(bool checked)
{
    m_settings.setValue(_key_thumbnails_display_order_hints, checked);
}

void SettingsDialog::on_cbDontUseNativeDlg_clicked(bool checked)
{
    m_settings.setValue(_key_dont_use_native_dialog, checked);
}

void SettingsDialog::on_btnColorSelectedContent_clicked()
{
    QColor clr;
    clr.setNamedColor(m_settings.value(_key_content_sel_content_color, _key_content_sel_content_color_def).toString());
    clr = QColorDialog::getColor(clr, this, tr("Color selection"), QColorDialog::ShowAlphaChannel);
    if (clr.isValid()) {
        m_settings.setValue(_key_content_sel_content_color, clr.name(QColor::HexArgb));
        QString s("QToolButton {background: %1};");
        ui.btnColorSelectedContent->setStyleSheet(s.arg(clr.name(QColor::HexArgb)));
    }
}

void SettingsDialog::on_btnColorSelectedContentReset_released()
{
    m_settings.setValue(_key_content_sel_content_color, _key_content_sel_content_color_def);
    QString s("QToolButton {background: %1};");
    ui.btnColorSelectedContent->setStyleSheet(s.arg(_key_content_sel_content_color_def));
}

void SettingsDialog::on_btnColorDeskew_clicked()
{
    QColor clr;
    clr.setNamedColor(m_settings.value(_key_deskew_controls_color, _key_deskew_controls_color_def).toString());
    clr = QColorDialog::getColor(clr, this, tr("Color selection"), QColorDialog::ShowAlphaChannel);
    if (clr.isValid()) {
        m_settings.setValue(_key_deskew_controls_color, clr.name(QColor::HexArgb));
        QString s("QToolButton {background: %1};");
        ui.btnColorDeskew->setStyleSheet(s.arg(clr.name(QColor::HexArgb)));
    }
}

void SettingsDialog::on_btnColorDeskewReset_released()
{
    m_settings.setValue(_key_deskew_controls_color, _key_deskew_controls_color_def);
    QString s("QToolButton {background: %1};");
    ui.btnColorDeskew->setStyleSheet(s.arg(_key_deskew_controls_color_def));
}

void SettingsDialog::on_cbStyle_currentIndexChanged(const QString &arg1)
{
    m_settings.setValue(_key_app_style, arg1);
    GlobalStaticSettings::applyAppStyle(m_settings);
}

void SettingsDialog::on_cbStyleSheet_currentIndexChanged(int index)
{
    QVariant val = ui.cbStyleSheet->itemData(index);
    if (val.type() == QVariant::Type::Bool) {
        m_settings.setValue(_key_app_empty_palette, val.toBool());
        m_settings.setValue(_key_app_stylsheet_file, "");
    } else {
        m_settings.setValue(_key_app_empty_palette, false);
        m_settings.setValue(_key_app_stylsheet_file, val.toString());
    }
    GlobalStaticSettings::applyAppStyle(m_settings);
}

void SettingsDialog::on_btnMinidjvuChoose_clicked()
{
    QString path = qApp->applicationDirPath();
#ifdef _WIN32
    QString filter = "minidjvu-mod (minidjvu-mod.exe)";
#else
    QString filter = "minidjvu-mod (minidjvu-mod)";
#endif

    QString minidjvu = QFileDialog::getOpenFileName(this, tr("Choose minidjvu-mod executable"), path, filter);
    if (!minidjvu.isEmpty()) {
        ui.edMinidjvu->setText(minidjvu);
    }
}

void SettingsDialog::on_edMinidjvu_textChanged(const QString &txt)
{
    QProcess proc(this);
    proc.start(txt, QStringList());
    proc.waitForFinished(2000);

    const QString output(proc.readAllStandardOutput());

    // Let's grep the minidjvu version
    QRegularExpression re(".*minidjvu-mod ([0-9.a-zA-Z]+) - .*");
    QRegularExpressionMatch match = re.match(output);
    QString version(tr("Version: "));

    if (match.hasMatch() && match.lastCapturedIndex() > 0) {
        version += match.captured(1);
        m_settings.setValue(_key_djvu_bin_minidjvu, txt);
    } else {
        //"Предупреждение: версии программы и библиотеки не совпадают:
        //program version 0.9, library version 0.9m01."
        re.setPattern(".*program version ([0-9.a-zA-Z]+, library version [0-9.a-zA-Z]+)\\..*");
        match = re.match(output);
        if (match.hasMatch() && match.lastCapturedIndex() > 0) {
            version += match.captured(1);
            m_settings.setValue(_key_djvu_bin_minidjvu, txt);
        } else {
            version = tr("minidjvu-mod executable can't be found !");
        }
    }
    ui.lblMinidjvuVersion->setText(version);
}
