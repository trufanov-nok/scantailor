/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C) Joseph Artsimovich <joseph_a@mail.ru>

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

#include "globalstaticsettings.h"

#include <QApplication>
#include <QStyleFactory>
#include <QStyle>
#include <QPalette>
#include <QFileInfo>
#include "TiffCompressionInfo.h"
#include "config.h"

QString GlobalStaticSettings::m_tiff_compr_method_bw;
QString GlobalStaticSettings::m_tiff_compr_method_color;
int GlobalStaticSettings::m_tiff_compression_bw_id = 5;
int GlobalStaticSettings::m_tiff_compression_color_id = 5;
bool GlobalStaticSettings::m_drawDeskewDeviants = false;
bool GlobalStaticSettings::m_drawContentDeviants = false;
bool GlobalStaticSettings::m_drawMarginDeviants = false;
bool GlobalStaticSettings::m_drawDeviants = false;
int  GlobalStaticSettings::m_currentStage = 0;
int  GlobalStaticSettings::m_binrization_threshold_control_default = 0;
bool GlobalStaticSettings::m_use_horizontal_predictor = false;
bool GlobalStaticSettings::m_disable_bw_smoothing = false;
qreal GlobalStaticSettings::m_zone_editor_min_angle = 3.0;
float GlobalStaticSettings::m_picture_detection_sensitivity = 100.;
QColor GlobalStaticSettings::m_deskew_controls_color;
QColor GlobalStaticSettings::m_deskew_controls_color_pen;
QColor GlobalStaticSettings::m_deskew_controls_color_thumb;
QColor GlobalStaticSettings::m_content_sel_content_color;
QColor GlobalStaticSettings::m_content_sel_content_color_pen;
QHotKeys GlobalStaticSettings::m_hotKeyManager = QHotKeys();
int  GlobalStaticSettings::m_highlightColorAdjustment = 140;

bool GlobalStaticSettings::m_thumbsListOrderAllowed = true;
int GlobalStaticSettings::m_thumbsMinSpacing = 3;
int GlobalStaticSettings::m_thumbsBoundaryAdjTop = 5;
int GlobalStaticSettings::m_thumbsBoundaryAdjBottom = 5;
int GlobalStaticSettings::m_thumbsBoundaryAdjLeft = 5;
int GlobalStaticSettings::m_thumbsBoundaryAdjRight = 3;
bool GlobalStaticSettings::m_fixedMaxLogicalThumbSize = false;
bool GlobalStaticSettings::m_displayOrderHints = true;

bool GlobalStaticSettings::m_dewarpAutoVertHalfCorrection = false;
bool GlobalStaticSettings::m_dewarpAutoDeskewAfterDewarp = false;
bool GlobalStaticSettings::m_simulateSelectionModifier = false;
bool GlobalStaticSettings::m_simulateSelectionModifierHintEnabled = true;
bool GlobalStaticSettings::m_inversePageOrder = false;

bool GlobalStaticSettings::m_DontUseNativeDialog = true;

QString GlobalStaticSettings::m_djvu_bin_minidjvu;
QString GlobalStaticSettings::m_djvu_bin_c44;
QString GlobalStaticSettings::m_djvu_bin_djvuextract;
QString GlobalStaticSettings::m_djvu_bin_djvumake;
QString GlobalStaticSettings::m_djvu_bin_djvused;
QString GlobalStaticSettings::m_djvu_bin_tesseract;
QString GlobalStaticSettings::m_djvu_bin_djvm;
QString GlobalStaticSettings::m_djvu_pages_subfolder;
QString GlobalStaticSettings::m_djvu_layers_subfolder;
int GlobalStaticSettings::m_djvu_pages_per_djbz;
bool GlobalStaticSettings::m_djvu_djbz_erosion;
bool GlobalStaticSettings::m_djvu_djbz_use_prototypes;
bool GlobalStaticSettings::m_djvu_djbz_use_averaging;
int GlobalStaticSettings::m_djvu_djbz_aggression;
int GlobalStaticSettings::m_djvu_djbz_classifier;
QString GlobalStaticSettings::m_djvu_djbz_extension;

uint GlobalStaticSettings::m_default_bsf = 1;
FREE_IMAGE_FILTER GlobalStaticSettings::m_default_scale_filter = FREE_IMAGE_FILTER::FILTER_BICUBIC;


SettingsChangesSignaller _instance; // we need one instance just to be able to send signals

SettingsChangesSignaller* SettingsChangesSignaller::self()
{
    return &_instance;
}

void GlobalStaticSettings::setDrawDeskewDeviants(bool val)
{
    if (m_drawDeskewDeviants != val) {
        m_drawDeskewDeviants = val;
        updateParams();
    }
}

void GlobalStaticSettings::setDrawContentDeviants(bool val)
{
    if (m_drawContentDeviants != val) {
        m_drawContentDeviants = val;
        updateParams();
    }
}

void GlobalStaticSettings::setDrawMarginDeviants(bool val)
{
    if (m_drawMarginDeviants != val) {
        m_drawMarginDeviants = val;
        updateParams();
    }
}

void GlobalStaticSettings::stageChanged(int i)
{
    if (m_currentStage != i) {
        m_currentStage = i;
        updateParams();
    }
}

void GlobalStaticSettings::applyAppStyle(const QSettings& settings)
{
    if (qApp) {
        const QString style = settings.value(_key_app_style, _key_app_style_def).toString().toLower();
        bool style_changed = false;
        if (!qApp->style() || style != qApp->style()->objectName().toLower()) {
            qApp->setStyleSheet(""); // is a must!
            QApplication::setStyle(QStyleFactory::create(style));
            style_changed = true;
        }


        const QString qss_fname = settings.value(_key_app_stylsheet_file, "").toString();
        if (qss_fname.isEmpty()) {
            if (!qApp->styleSheet().isEmpty()) {
                qApp->setStyleSheet("");
                style_changed = true;
            }
        } else {
            QFile f(qss_fname);
            if (f.open(QIODevice::ReadOnly)) {
                QString new_qss(f.readAll());

                int idx = new_qss.indexOf("@path_to_pics@");
                if (idx != -1) {
#if defined(_WIN32) or defined(Q_OS_MAC)
                    QFileInfo fi(qss_fname);
                    const QString path_to_pix = fi.absolutePath();

#else
                    const QString path_to_pix(PIXMAPS_DIR_ABS);
#endif
                    new_qss = new_qss.replace("@path_to_pics@", path_to_pix);
                }

                if (new_qss != qApp->styleSheet()) {
                    qApp->setStyleSheet(""); // must be here
                    qApp->setStyleSheet(new_qss);
                    style_changed = true;
                }
                f.close();
            } else {
                qApp->setStyleSheet("");
                style_changed = true;
            }
        }

        const bool empty_palette = settings.value(_key_app_empty_palette, _key_app_empty_palette_def).toBool();
        if (empty_palette) {
            const QPalette empty_palette;
            QApplication::setPalette(empty_palette);
        } else {
            const QPalette std_palette = QApplication::style()->standardPalette();
            QApplication::setPalette(std_palette);
        }

        QStyle* s = qApp->style();
        if (s) {
            s->setObjectName(style);
        }

        if (style_changed) {
            emit SettingsChangesSignaller::self()->StyleChanged();
        }
    }
}

void GlobalStaticSettings::updateSettings()
{
    QSettings settings;

    applyAppStyle(settings);

    setDrawDeskewDeviants(settings.value(_key_deskew_deviant_enabled, _key_deskew_deviant_enabled_def).toBool());
    setDrawContentDeviants(settings.value(_key_select_content_deviant_enabled, _key_select_content_deviant_enabled_def).toBool());
    setDrawMarginDeviants(settings.value(_key_margins_deviant_enabled, _key_margins_deviant_enabled_def).toBool());

    setTiffCompressionBW( settings.value(_key_tiff_compr_method_bw, _key_tiff_compr_method_bw_def).toString() );
    setTiffCompressionColor( settings.value(_key_tiff_compr_method_color, _key_tiff_compr_method_color_def).toString() );
    m_binrization_threshold_control_default = settings.value(_key_output_bin_threshold_default, _key_output_bin_threshold_default_def).toInt();
    m_use_horizontal_predictor = settings.value(_key_tiff_compr_horiz_pred, _key_tiff_compr_horiz_pred_def).toBool();
    m_disable_bw_smoothing = settings.value(_key_mode_bw_disable_smoothing, _key_mode_bw_disable_smoothing_def).toBool();
    m_zone_editor_min_angle = settings.value(_key_zone_editor_min_angle, _key_zone_editor_min_angle_def).toReal();
    m_picture_detection_sensitivity = settings.value(_key_picture_zones_layer_sensitivity, _key_picture_zones_layer_sensitivity_def).toInt();
    m_deskew_controls_color.setNamedColor(settings.value(_key_deskew_controls_color, _key_deskew_controls_color_def).toString());
    m_deskew_controls_color_pen = m_deskew_controls_color;
    m_deskew_controls_color_pen.setAlpha(255); // ignore transparency
    m_content_sel_content_color.setNamedColor(settings.value(_key_content_sel_content_color, _key_content_sel_content_color_def).toString());
    m_content_sel_content_color_pen = m_content_sel_content_color;
    m_content_sel_content_color_pen.setAlpha(255); // ignore transparency
    m_deskew_controls_color_thumb = m_deskew_controls_color;
    if (m_deskew_controls_color_thumb.alpha() > 70) {
        m_deskew_controls_color_thumb.setAlpha(70);
    }

    m_highlightColorAdjustment = 100 + settings.value(_key_thumbnails_non_focused_selection_highlight_color_adj, _key_thumbnails_non_focused_selection_highlight_color_adj_def).toInt();

    m_thumbsListOrderAllowed = settings.value(_key_thumbnails_multiple_items_in_row, _key_thumbnails_multiple_items_in_row_def).toBool();
    m_thumbsMinSpacing = settings.value(_key_thumbnails_min_spacing, _key_thumbnails_min_spacing_def).toInt();
    m_thumbsBoundaryAdjTop = settings.value(_key_thumbnails_boundary_adj_top, _key_thumbnails_boundary_adj_top_def).toInt();
    m_thumbsBoundaryAdjBottom = settings.value(_key_thumbnails_boundary_adj_bottom, _key_thumbnails_boundary_adj_bottom_def).toInt();
    m_thumbsBoundaryAdjLeft = settings.value(_key_thumbnails_boundary_adj_left, _key_thumbnails_boundary_adj_left_def).toInt();
    m_thumbsBoundaryAdjRight = settings.value(_key_thumbnails_boundary_adj_right, _key_thumbnails_boundary_adj_right_def).toInt();
    m_fixedMaxLogicalThumbSize = settings.value(_key_thumbnails_fixed_thumb_size, _key_thumbnails_fixed_thumb_size_def).toBool();
    m_displayOrderHints = settings.value(_key_thumbnails_display_order_hints, _key_thumbnails_display_order_hints_def).toBool();

    m_dewarpAutoVertHalfCorrection = settings.value(_key_dewarp_auto_vert_half_correction, _key_dewarp_auto_vert_half_correction_def).toBool();
    m_dewarpAutoDeskewAfterDewarp = settings.value(_key_dewarp_auto_deskew_after_dewarp, _key_dewarp_auto_deskew_after_dewarp_def).toBool();

    m_simulateSelectionModifierHintEnabled = settings.value(_key_thumbnails_simulate_key_press_hint, _key_thumbnails_simulate_key_press_hint_def).toBool();

    m_DontUseNativeDialog = settings.value(_key_dont_use_native_dialog, _key_dont_use_native_dialog_def).toBool();

    m_djvu_bin_c44 = settings.value(_key_djvu_bin_c44, _key_djvu_bin_c44_def).toString();
    m_djvu_bin_minidjvu = settings.value(_key_djvu_bin_minidjvu, _key_djvu_bin_minidjvu_def).toString();
    m_djvu_bin_djvuextract = settings.value(_key_djvu_bin_djvuextract, _key_djvu_bin_djvuextract_def).toString();
    m_djvu_bin_djvumake = settings.value(_key_djvu_bin_djvumake, _key_djvu_bin_djvumake_def).toString();
    m_djvu_bin_djvused = settings.value(_key_djvu_bin_djvused, _key_djvu_bin_djvused_def).toString();
    m_djvu_bin_tesseract = settings.value(_key_djvu_bin_tesseract, _key_djvu_bin_tesseract_def).toString();
    m_djvu_bin_djvm = settings.value(_key_djvu_bin_djvm, _key_djvu_bin_djvm_def).toString();
    m_djvu_pages_subfolder = settings.value(_key_djvu_pages_subfolder, _key_djvu_pages_subfolder_def).toString();
    m_djvu_layers_subfolder = settings.value(_key_djvu_layers_subfolder, _key_djvu_layers_subfolder_def).toString();
    m_djvu_pages_per_djbz  = settings.value(_key_djvu_pages_per_djbz, _key_djvu_pages_per_djbz_def).toInt();
    m_djvu_djbz_erosion    = settings.value(_key_djvu_djbz_erosion, _key_djvu_djbz_erosion_def).toBool();
    m_djvu_djbz_use_prototypes = settings.value(_key_djvu_djbz_use_prototypes, _key_djvu_djbz_use_prototypes_def).toBool();
    m_djvu_djbz_use_averaging = settings.value(_key_djvu_djbz_use_averaging, _key_djvu_djbz_use_averaging_def).toBool();
    m_djvu_djbz_aggression = settings.value(_key_djvu_djbz_aggression, _key_djvu_djbz_aggression_def).toInt();
    m_djvu_djbz_classifier = settings.value(_key_djvu_djbz_classifier, _key_djvu_djbz_classifier_def).toInt();
    m_djvu_djbz_extension  = settings.value(_key_djvu_djbz_extension, _key_djvu_djbz_extension_def).toString();

    m_default_bsf = settings.value(_key_scale_bsf, _key_scale_bsf_def).toUInt();
    m_default_scale_filter = (FREE_IMAGE_FILTER) settings.value(_key_scale_filter, _key_scale_filter_def).toUInt();
}

void GlobalStaticSettings::updateHotkeys()
{
    m_hotKeyManager.load();
}

QKeySequence GlobalStaticSettings::createShortcut(const HotKeysId& id, int idx)
{
    const HotKeySequence& seq = m_hotKeyManager.get(id)->sequences()[idx];
    QString str = QHotKeys::hotkeysToString(seq.m_modifierSequence, seq.m_keySequence);
    return QKeySequence(str);
}

QString GlobalStaticSettings::getShortcutText(const HotKeysId& id, int idx)
{
    const HotKeySequence& seq = m_hotKeyManager.get(id)->sequences()[idx];
    return QHotKeys::hotkeysToString(seq.m_modifierSequence, seq.m_keySequence);
}

bool GlobalStaticSettings::checkKeysMatch(const HotKeysId& id, const Qt::KeyboardModifiers& modifiers, const Qt::Key& key)
{
    const HotKeyInfo* i = m_hotKeyManager.get(id);
    for (const HotKeySequence& seq : i->sequences()) {
        if (seq.m_modifierSequence == modifiers &&
                seq.m_keySequence.count() == 1 &&
                seq.m_keySequence[0] == key) {
            return true;
        }
    }
    return false;
}

bool GlobalStaticSettings::checkModifiersMatch(const HotKeysId& id, const Qt::KeyboardModifiers& modifiers)
{
    if (modifiers == Qt::NoModifier) {
        return false;
    }

    const HotKeyInfo* i = m_hotKeyManager.get(id);
    for (const HotKeySequence& seq : i->sequences()) {
        if (seq.m_modifierSequence == modifiers) {
            return true;
        }
    }
    return false;
}

void GlobalStaticSettings::setTiffCompressionBW(QString const& compression_name)
{
    if (m_tiff_compr_method_bw != compression_name) {
        m_tiff_compression_bw_id = TiffCompressions::info(compression_name).id;
        m_tiff_compr_method_bw = compression_name;
    }
    // QSettings might be out of sync
    QSettings().setValue(_key_tiff_compr_method_bw, m_tiff_compr_method_bw);
}

void GlobalStaticSettings::setTiffCompressionColor(QString const& compression_name)
{
    if (m_tiff_compr_method_color != compression_name) {
        m_tiff_compression_color_id = TiffCompressions::info(compression_name).id;
        m_tiff_compr_method_color = compression_name;
    }
    // QSettings might be out of sync
    QSettings().setValue(_key_tiff_compr_method_color, m_tiff_compr_method_color);
}

void GlobalStaticSettings::updateParams()
{
    switch (m_currentStage) {
    case 2:
        m_drawDeviants = m_drawDeskewDeviants;
        break;
    case 3:
        m_drawDeviants = m_drawContentDeviants;
        break;
    case 4:
        m_drawDeviants = m_drawMarginDeviants;
        break;
    default:
        m_drawDeviants = false;
    }
}
