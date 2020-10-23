/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C) 2020 Alexander Trufanov <trufanovan@gmail.com>

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

#ifndef PUBLISH_PARAMS_H_
#define PUBLISH_PARAMS_H_

#include "Dpi.h"
#include "RegenParams.h"
#include "PageId.h"
#include "SourceImagesInfo.h"
#include "FreeImageFilters.h"
#include "settings/globalstaticsettings.h"

#include <QVariantMap>
#include <QFileInfo>
#include <QDir>
#include <QImage>

class QDomDocument;
class QDomElement;

namespace publish
{

class OutputParams;

typedef QVector<QPair<QRect, QColor> > ColorRects;

class Params: public RegenParams
{
public:
    Params();
    Params(QDomElement const& el);
//    bool isNull() const { return m_inputImageInfo.fileName.isEmpty() ||
//                m_inputImageInfo.imageColorMode == DjVuPageInfo::ColorMode::Unknown; }

    Dpi const& outputDpi() const { return m_dpi; }
    void setOutputDpi(Dpi const& dpi) { m_dpi = dpi; }

    void setSourceImagesInfo(const SourceImagesInfo& info) { m_sourceImagesInfo = info; }
    const SourceImagesInfo& sourceImagesInfo() const { return m_sourceImagesInfo; }

    void setDjvuFilename(const QString& fname) { m_djvuFilename = fname; }
    QString const djvuFilename() const { return m_djvuFilename; }

    void setDjVuSize(uint size) { m_djvuSize = size; }
    uint djvuSize() const { return m_djvuSize; }

    // sometimes changes in DjVu doc doesn't affect its size (for ex. `FGbz=#color` set)
    void setDjVuLastChanged(const QDateTime& dt) { m_djvuChanged = dt; }
    QDateTime djvuLastChanged() const { return m_djvuChanged; }

    void setDjbzId(QString const& djbzId) { m_djbzId = djbzId; }
    QString djbzId() const { return m_djbzId; }

    void setDjbzRevision(const QDateTime& val) { m_djbzRevision = val; }
    const QDateTime& djbzRevision() const { return m_djbzRevision; }

    void setClean(bool clean) { m_clean = clean; }
    bool clean() const { return m_clean; }

    void setErosion(bool erosion) { m_erosion = erosion; }
    bool erosion() const { return m_erosion; }

    void setSmooth(bool smooth) { m_smooth = smooth; }
    bool smooth() const { return m_smooth; }

    int bsf() const { return m_bsf; }
    void setBsf(int val) { m_bsf = val; }

    FREE_IMAGE_FILTER scaleMethod() const { return m_scale_method; }
    void setScaleMethod(FREE_IMAGE_FILTER val) { m_scale_method = val; }

    QString fgbzOptions() const { return m_FGbz_options; }
    void setFGbzOptions(const QString& val) { m_FGbz_options = val.trimmed(); }

    void resetOutputParams() { m_ptrOutputParams.reset(); }
    bool hasOutputParams() const { return m_ptrOutputParams != nullptr; }
    const OutputParams& outputParams() const { return *m_ptrOutputParams; }

    QDomElement toXml(QDomDocument& doc, QString const& name) const;

    ~Params(){}

    bool operator !=(const Params &other) const;
    bool matchJb2Part(const Params &other) const;
    bool matchBg44Part(const Params &other) const;
    bool matchAssemblePart(const Params &other) const;
    bool matchPostprocessPart(const Params &other) const;
    void rememberOutputParams(const DjbzParams &djbz_params);
    bool isDjVuCached() const;

    const ColorRects& colorRects() const { return m_colorRects; }
    ColorRects& colorRectsRef() { return m_colorRects; }
    void addColorRect(const QRect& rect, const QColor& color) { m_colorRects.append(QPair<QRect, QColor>(rect, color)); }
    void clearColorRects() { m_colorRects.clear(); }
    QString colorRectsAsTxt() const;
    void colorRectsFromTxt(const QString& txt);
    bool containsColorRectsIn(const QRect& rect);
    void removeColorRectsIn(const QRect& rect);

    QString title() const { return m_title; }
    void setTitle(const QString& title) { m_title = title; }
    uint rotation() const { return m_rotation; }
    void setRotation(uint val) { m_rotation = val; }
private:
    Dpi m_dpi;
    SourceImagesInfo m_sourceImagesInfo;
    QString m_djvuFilename;
    uint m_djvuSize;
    QDateTime m_djvuChanged;
    QString m_djbzId;
    QDateTime m_djbzRevision;
    bool m_clean;
    bool m_erosion;
    bool m_smooth;
    int m_bsf;
    FREE_IMAGE_FILTER m_scale_method;
    QString m_FGbz_options;
    std::shared_ptr<OutputParams> m_ptrOutputParams;
    ColorRects m_colorRects;
    QString m_title;
    uint m_rotation;
};

} // namespace publish

#endif
