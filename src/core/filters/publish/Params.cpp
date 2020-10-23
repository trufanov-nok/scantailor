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

#include "Params.h"
#include "XmlMarshaller.h"
#include "XmlUnmarshaller.h"
#include "CommandLine.h"
#include "OutputParams.h"
#include "DjbzDispatcher.h"
#include "settings/globalstaticsettings.h"

#include <QDomDocument>
#include <QDomElement>
#include <QString>
#include <QCryptographicHash>
#include <QRegularExpression>


namespace publish
{

Params::Params()
:  RegenParams(),
  m_dpi(CommandLine::get().getDefaultOutputDpi()),
  m_djvuSize(0), m_djbzRevision(QDateTime::currentDateTimeUtc()),
  m_clean(false), m_erosion(false), m_smooth(true),
  m_bsf(GlobalStaticSettings::m_default_bsf),
  m_scale_method(GlobalStaticSettings::m_default_scale_filter),
  m_FGbz_options("#black"),
  m_rotation(0)
{    
}

Params::Params(QDomElement const& el)
:	RegenParams(),
    m_dpi(XmlUnmarshaller::dpi(el.namedItem("dpi").toElement())),
    m_sourceImagesInfo(el.namedItem("source_images_info").toElement())
{
    m_djvuFilename = el.attribute("djvu_filename");
    m_djvuSize     = el.attribute("djvu_filesize").toUInt();
    m_djvuChanged  = QDateTime::fromString(el.attribute("djvu_modified"), "dd.MM.yyyy hh:mm:ss.zzz");
    m_djbzId       = el.attribute("djbz_id");
    m_djbzRevision = QDateTime::fromString(el.attribute("djbz_rev"), "dd.MM.yyyy hh:mm:ss.zzz");
    m_clean        = el.attribute("clean").toInt();
    m_erosion      = el.attribute("erosion").toInt();
    m_smooth       = el.attribute("smooth").toInt();
    m_bsf          = el.attribute("bsf", QString::number(GlobalStaticSettings::m_default_bsf)).toUInt();
    m_scale_method = str2scale_filter(el.attribute("method", scale_filter2str(GlobalStaticSettings::m_default_scale_filter)));
    m_FGbz_options = el.attribute("fgbz", "#black").trimmed();
    if (el.hasAttribute("fgbz_rects")) {
        colorRectsFromTxt(el.attribute("fgbz_rects", ""));
    }

    m_title = el.attribute("title", "");
    m_rotation = el.attribute("rotation", "0").toUInt();



    QDomNode item = el.namedItem("processed_with");
    if (!item.isNull() && item.isElement()) {
        m_ptrOutputParams.reset(new OutputParams(item.toElement()));
    }
}

QDomElement
Params::toXml(QDomDocument& doc, QString const& name) const
{
	XmlMarshaller marshaller(doc);
	
	QDomElement el(doc.createElement(name));
	el.appendChild(marshaller.dpi(m_dpi, "dpi"));
    el.setAttribute("djvu_filename", m_djvuFilename);
    el.setAttribute("djvu_filesize", m_djvuSize);
    el.setAttribute("djvu_modified", m_djvuChanged.toString("dd.MM.yyyy hh:mm:ss.zzz")); // default format string doesn't include msecs
    el.setAttribute("djbz_id", m_djbzId);
    el.setAttribute("djbz_rev", m_djbzRevision.toString("dd.MM.yyyy hh:mm:ss.zzz"));
    el.setAttribute("clean", m_clean);
    el.setAttribute("erosion", m_erosion);
    el.setAttribute("smooth", m_smooth);
    el.setAttribute("bsf", m_bsf);
    if (m_bsf > 1) {
        el.setAttribute("method", scale_filter2str(m_scale_method));
    }

    el.setAttribute("fgbz", m_FGbz_options);
    if (!m_colorRects.isEmpty()) {
        el.setAttribute("fgbz_rects", colorRectsAsTxt());
    }

    if (!m_title.isEmpty()) {
        el.setAttribute("title", m_title);
    }

    if (m_rotation) {
        el.setAttribute("rotation", m_rotation);
    }

    if (m_ptrOutputParams) {
        el.appendChild(m_ptrOutputParams->toXml(doc, "processed_with"));
    }

    el.appendChild(m_sourceImagesInfo.toXml(doc, "source_images_info"));
	return el;
}

bool
Params::operator !=(const Params &other) const
{
    return (m_djvuSize != other.m_djvuSize ||
            m_djvuFilename != other.m_djvuFilename ||
            m_djvuChanged != other.m_djvuChanged ||
            m_djbzId != other.m_djbzId ||
            m_djbzRevision != other.m_djbzRevision ||
            m_clean != other.m_clean ||
            m_erosion != other.m_erosion ||
            m_smooth != other.m_smooth ||
            m_dpi != other.m_dpi ||
            m_bsf != other.m_bsf ||
            m_scale_method != other.m_scale_method ||
            m_FGbz_options != other.m_FGbz_options ||
            m_colorRects != other.m_colorRects ||
            m_title != other.m_title ||
            m_rotation != other.m_rotation ||
            m_sourceImagesInfo != other.m_sourceImagesInfo);
}

bool
Params::matchJb2Part(const Params &other) const
{
    return (m_djvuFilename == other.m_djvuFilename &&
            m_dpi          == other.m_dpi &&
            m_djbzId       == other.m_djbzId &&
            m_djbzRevision == other.m_djbzRevision &&
            m_clean        == other.m_clean &&
            m_erosion      == other.m_erosion &&
            m_smooth       == other.m_smooth &&
            m_dpi          == other.m_dpi &&
            m_sourceImagesInfo.export_jb2Filename() ==
            other.m_sourceImagesInfo.export_jb2Filename());
}

bool
Params::matchBg44Part(const Params &other) const
{
    return (m_djvuFilename == other.m_djvuFilename &&
            m_dpi          == other.m_dpi &&
            m_bsf          == other.m_bsf &&
            m_scale_method == other.m_scale_method &&
            m_sourceImagesInfo.export_bg44Filename() ==
            other.m_sourceImagesInfo.export_bg44Filename());
}

bool
Params::matchAssemblePart(const Params &other) const
{
    return (m_FGbz_options == other.m_FGbz_options &&
            m_colorRects == other.m_colorRects);
}

bool
Params::matchPostprocessPart(const Params &other) const
{
    return (m_title == other.m_title &&
            m_rotation == other.m_rotation);
}

void
Params::rememberOutputParams(const DjbzParams &djbz_params)
{
    m_ptrOutputParams.reset( new OutputParams(*this,
                                              m_djbzId, m_djbzRevision,
                                              djbz_params)
                             );
}

QString
Params::colorRectsAsTxt() const
{
    QString res;
    for (const QPair<QRect, QColor>& pair: qAsConst(m_colorRects)) {
        const QRect r = pair.first;
        res += QString("%1:%2,%3,%4,%5")
                .arg(pair.second.name(QColor::HexRgb))
                .arg(r.left())
                .arg(r.top())
                .arg(r.width())
                .arg(r.height());
    }
    return res;
}

void
Params::colorRectsFromTxt(const QString& txt)
{
    m_colorRects.clear();
    if (txt.isEmpty()) {
        return;
    }
    const QRegularExpression re("(#[0-9A-Fa-f]+):([0-9]+),([0-9]+),([0-9]+),([0-9]+)");
    QRegularExpressionMatch m = re.match(txt);
    while (m.hasMatch()) {
        m_colorRects.append( QPair<QRect, QColor>(
                                 QRect(m.captured(2).toInt(),
                                       m.captured(3).toInt(),
                                       m.captured(4).toInt(),
                                       m.captured(5).toInt()),
                                 QColor(m.captured(1))
                                 ));
        m = re.match(txt, m.capturedEnd());
    }
}

bool
Params::containsColorRectsIn(const QRect& rect)
{
    for (const auto & pair: qAsConst(m_colorRects)) {
        if (rect.intersects(pair.first)) {
            return true;
        }
    }
    return false;
}

void
Params::removeColorRectsIn(const QRect& rect)
{
    for (auto it = m_colorRects.begin(); it != m_colorRects.end();) {
        if (rect.intersects(it->first)) {
            it = m_colorRects.erase(it);
        } else {
            it++;
        }
    }
}

bool
Params::isDjVuCached() const
{
    const Params& old_params = outputParams().params();
    if (!m_djvuFilename.isEmpty() && m_djvuSize &&
            m_djvuFilename == old_params.djvuFilename() &&
            m_djvuSize == old_params.djvuSize() &&
            m_djvuChanged == old_params.djvuLastChanged()) {
        const QFileInfo fi(m_djvuFilename);
        if (fi.exists() && fi.size() == m_djvuSize &&
                fi.lastModified() == m_djvuChanged) {
            return true;
        }
    }
    return false;
}

} // namespace publish
