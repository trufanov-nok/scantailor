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

#ifndef GLOBALSTATICSETTINGS_H
#define GLOBALSTATICSETTINGS_H
#include "settings/hotkeysmanager.h"
#include "settings/ini_keys.h"
#include <memory>
#include <QColor>
#include <QResource>
#include <QRegularExpression>

class SettingsChangesSignaller: public QObject
{
    Q_OBJECT
signals:
    void StyleChanged();
public:
    static SettingsChangesSignaller* self();
};

class GlobalStaticSettings
{
public:

    inline static bool doDrawDeviants()
    {
        return m_drawDeviants;
    }

    static void setDrawDeskewDeviants(bool val);
    static void setDrawContentDeviants(bool val);
    static void setDrawMarginDeviants(bool val);
    static void stageChanged(int i);

    static void applyAppStyle(const QSettings& settings);
    static void updateSettings();

    static void updateHotkeys();
    static QKeySequence createShortcut(const HotKeysId& id, int idx = 0);
    static QString getShortcutText(const HotKeysId& id, int idx = 0);
    static bool checkKeysMatch(const HotKeysId& id, const Qt::KeyboardModifiers& modifiers, const Qt::Key& key);
    static bool checkModifiersMatch(const HotKeysId& id, const Qt::KeyboardModifiers& modifiers);

    static void setTiffCompressionBW(QString const& compression_name);
    static void setTiffCompressionColor(QString const& compression_name);

private:
    GlobalStaticSettings() {}

    inline static void updateParams();

    static bool m_drawDeviants;
    static bool m_drawDeskewDeviants;
    static bool m_drawContentDeviants;
    static bool m_drawMarginDeviants;
    static int m_currentStage;
public:
    static QString m_tiff_compr_method_bw;
    static QString m_tiff_compr_method_color;
    static int m_tiff_compression_bw_id;
    static int m_tiff_compression_color_id;
    static int m_binrization_threshold_control_default;
    static bool m_use_horizontal_predictor;
    static bool m_disable_bw_smoothing;
    static qreal m_zone_editor_min_angle;
    static float m_picture_detection_sensitivity;
    static QColor m_deskew_controls_color;
    static QColor m_deskew_controls_color_pen;
    static QColor m_deskew_controls_color_thumb;
    static QColor m_content_sel_content_color;
    static QColor m_content_sel_content_color_pen;
    static QHotKeys m_hotKeyManager;
    static int m_highlightColorAdjustment;

    static bool m_thumbsListOrderAllowed;
    static int m_thumbsMinSpacing;
    static int m_thumbsBoundaryAdjTop;
    static int m_thumbsBoundaryAdjBottom;
    static int m_thumbsBoundaryAdjLeft;
    static int m_thumbsBoundaryAdjRight;
    static bool m_fixedMaxLogicalThumbSize;
    static bool m_displayOrderHints;

    static bool m_dewarpAutoVertHalfCorrection;
    static bool m_dewarpAutoDeskewAfterDewarp;
    static bool m_simulateSelectionModifier;
    static bool m_simulateSelectionModifierHintEnabled;
    static bool m_inversePageOrder;

    static bool m_DontUseNativeDialog;

    static QString m_djvu_bin_minidjvu;
    static QString m_djvu_bin_c44;
    static QString m_djvu_bin_djvuextract;
    static QString m_djvu_bin_djvumake;
    static QString m_djvu_bin_djvused;
    static QString m_djvu_bin_tesseract;
    static QString m_djvu_bin_djvm;
    static QString m_djvu_pages_subfolder;
    static QString m_djvu_layers_subfolder;
    static int m_djvu_pages_per_djbz;
    static bool m_djvu_djbz_erosion;
    static bool m_djvu_djbz_use_prototypes;
    static bool m_djvu_djbz_use_averaging;
    static int m_djvu_djbz_aggression;
    static int m_djvu_djbz_classifier;
    static QString m_djvu_djbz_extension;

    static uint m_default_bsf;
    static FREE_IMAGE_FILTER m_default_scale_filter;

};

#endif // GLOBALSTATICSETTINGS_H
