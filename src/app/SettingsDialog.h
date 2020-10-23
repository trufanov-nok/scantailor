/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C) 2007-2009  Joseph Artsimovich <joseph_a@mail.ru>

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

#ifndef SETTINGS_DIALOG_H_
#define SETTINGS_DIALOG_H_

#include "ui_SettingsDialog.h"
#include "settings/hotkeysmanager.h"
#include "filters/page_layout/Alignment.h"
#include "settings/ini_keys.h"
#include <QDialog>
#include <QInputDialog>

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    SettingsDialog(QWidget* parent = 0);

    virtual ~SettingsDialog();
signals:
    void settingsChanged();
private slots:
    void on_edMinidjvu_textChanged(const QString &arg1);

private slots:
    void on_btnMinidjvuChoose_clicked();

private slots:
    void on_dialogButtonClicked(QAbstractButton* btn);
    void commitChanges();
    void filterChanged(const QString&);
    void on_treeWidget_itemActivated(QTreeWidgetItem* item, int column);
    void on_treeWidget_itemSelectionChanged();
    void on_actionExpand_all_triggered();
    void on_actionCollapse_all_triggered();
    void on_treeWidget_itemChanged(QTreeWidgetItem* item, int column);
    void on_language_currentIndexChanged(int index);
    void on_cbApplyCutDefault_clicked(bool checked);
    void on_stackedWidget_currentChanged(int arg1);

    void on_cbTiffCompressionBW_currentIndexChanged(int index);
    void on_cbTiffCompressionColor_currentIndexChanged(int index);

    void on_cbTiffFilter_clicked(bool checked);

    void on_sbSavePeriod_valueChanged(int arg1);

    void onThresholdValueChanged(int);

    void on_despecklingDefaultsValue_currentIndexChanged(int index);

    void on_startBatchProcessingDlgAllPages_toggled(bool checked);

    void on_showStartBatchProcessingDlg_clicked(bool checked);

    void on_ThresholdDefaultsValue_valueChanged(int arg1);

    void on_dpiDefaultYValue_valueChanged(int arg1);

    void on_dpiDefaultXValue_valueChanged(int arg1);

    void on_useHorizontalPredictor_clicked(bool checked);

    void on_disableSmoothingBW_clicked(bool checked);

    void on_rectangularAreasSensitivityValue_valueChanged(int arg1);

    void on_originalPageDisplayOnKeyHold_clicked(bool checked);

    void on_lblHotKeyManager_linkActivated(const QString& link);

    void on_btnResetHotKeys_clicked();

    void on_marginDefaultTopVal_valueChanged(double arg1);

    void on_marginDefaultLeftVal_valueChanged(double arg1);

    void on_marginDefaultRightVal_valueChanged(double arg1);

    void on_marginDefaultBottomVal_valueChanged(double arg1);

    void on_cbMarginUnits_currentIndexChanged(int index);

    void on_cbMarginsAuto_clicked(bool checked);

    void on_gbPageDetectionFineTuneCorners_toggled(bool arg1);

    void on_gbPageDetectionBorders_toggled(bool arg1);

    void on_cbPageDetectionFineTuneCorners_clicked(bool checked);

    void on_pageDetectionTargetWidth_valueChanged(double arg1);

    void on_pageDetectionTargetHeight_valueChanged(double arg1);

    void on_gbPageDetectionTargetSize_toggled(bool arg1);

    void on_pageDetectionTopBorder_valueChanged(double arg1);

    void on_pageDetectionRightBorder_valueChanged(double arg1);

    void on_pageDetectionLeftBorder_valueChanged(double arg1);

    void on_pageDetectionBottomBorder_valueChanged(double arg1);

    void on_cbForegroundLayerSeparateControl_clicked(bool checked);

    void on_sbThumbsCacheImgSize_valueChanged(int arg1);

    void on_sbThumbsMinSpacing_valueChanged(int arg1);

    void on_sbThumbsBoundaryAdjTop_valueChanged(int arg1);

    void on_sbThumbsBoundaryAdjBottom_valueChanged(int arg1);

    void on_sbThumbsBoundaryAdjLeft_valueChanged(int arg1);

    void on_sbThumbsBoundaryAdjRight_valueChanged(int arg1);

    void on_cbThumbsListOrder_toggled(bool checked);

    void on_gbFixedMaxLogicalThumbSize_toggled(bool arg1);

    void on_sbFixedMaxLogicalThumbSizeHeight_valueChanged(int arg1);

    void on_sbFixedMaxLogicalThumbSizeWidth_valueChanged(int arg1);

    void on_btnThumbDefaults_clicked();

    void on_cbAlignmentAuto_toggled(bool checked);

    void on_cbAlignmentOriginal_toggled(bool checked);

    void on_gbMarginsAuto_toggled(bool arg1);

    void on_cbTryVertHalfCorrection_toggled(bool checked);

    void on_cbTryDeskewAfterDewarp_toggled(bool checked);

    void on_cbOrderHints_toggled(bool checked);

    void on_cbDontUseNativeDlg_clicked(bool checked);

    void on_btnColorSelectedContent_clicked();

    void on_btnColorSelectedContentReset_released();

    void on_btnColorDeskew_clicked();

    void on_btnColorDeskewReset_released();

    void on_cbStyle_currentIndexChanged(const QString &arg1);

    void on_cbStyleSheet_currentIndexChanged(int index);

private:
    void initLanguageList(QString cur_lang);
    void loadTiffList();
    void populateTreeWidget(QTreeWidget* treeWidget);
    void setupItem(QTreeWidgetItem* item, QWidget* w = nullptr, QString s = "", bool default_val = true);
    void backupSettings();
    void restoreSettings();
    void restoreSettingsTreeState(QTreeWidget* treeWidget);
    void storeSettingsTreeState(QTreeWidget* treeWidget);
    void setupPictureShapeComboBox();
    void updateMarginsDisplay();
private:
    Ui::SettingsDialog ui;
    QSettings m_settings;
    QSettings::SettingsMap m_oldSettings;
    bool m_accepted;
    page_layout::Alignment m_alignment;
    double m_mmToUnit;
    double m_unitToMM;
};

class QHotKeyInputDialog: public QInputDialog
{
    Q_OBJECT
public:
    QHotKeyInputDialog(const KeyType& editor_type, QWidget* parent = Q_NULLPTR, Qt::WindowFlags flags = Qt::WindowFlags());
    const Qt::KeyboardModifiers& modifiers() const
    {
        return m_modifiersPressed;
    }
    const QList<Qt::Key> keys() const
    {
        return m_keysPressed.toList();
    }
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
private:
    void updateLabel();
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

    KeyType m_editorType;
    QSet<Qt::Key> m_keysPressed;
    Qt::KeyboardModifiers m_modifiersPressed;
    QVector<Qt::Key> m_modifiersList;
    QLineEdit* m_edit;
};

#endif
