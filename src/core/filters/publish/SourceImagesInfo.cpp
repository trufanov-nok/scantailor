/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C) 2021 Alexander Trufanov <trufanovan@gmail.com>

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

#include "SourceImagesInfo.h"
#include "OutputFileNameGenerator.h"
#include "settings/globalstaticsettings.h"
#include "XmlMarshaller.h"
#include "XmlUnmarshaller.h"

#include <QFileInfo>
#include <QDir>

namespace publish
{

bool
SourceImagesInfo::operator !=(const SourceImagesInfo &other) const
{
    return (m_export_suggestion         != other.m_export_suggestion         ||
            m_output_filename           != other.m_output_filename           ||
            m_export_foregroundFilename != other.m_export_foregroundFilename ||
            m_export_foregroundFilesize != other.m_export_foregroundFilesize ||
            m_export_backgroundFilename != other.m_export_backgroundFilename ||
            m_export_backgroundFilesize != other.m_export_backgroundFilesize ||
            m_export_bg44Filename       != other.m_export_bg44Filename       ||
            m_export_bg44Filesize       != other.m_export_bg44Filesize       ||
            m_export_jb2Filename        != other.m_export_jb2Filename        ||
            m_export_jb2Filesize        != other.m_export_jb2Filesize
            );
}

SourceImagesInfo::SourceImagesInfo():
    m_output_filesize(0),
    m_export_foregroundFilesize(0),
    m_export_backgroundFilesize(0),
    m_export_bg44Filesize(0),
    m_export_jb2Filesize(0)
{
}

SourceImagesInfo::SourceImagesInfo(const PageId& page_id,
                                   const OutputFileNameGenerator &fname_gen,
                                   const ExportSuggestions& export_suggestions):
    m_export_foregroundFilesize(0),
    m_export_backgroundFilesize(0),
    m_export_bg44Filesize(0),
    m_export_jb2Filesize(0)
{
    const QString out_path = fname_gen.outDir() + "/";
    const QString djvu_path = out_path + GlobalStaticSettings::m_djvu_pages_subfolder + "/";
    const QString export_path = djvu_path + GlobalStaticSettings::m_djvu_layers_subfolder + "/";

    m_export_suggestion = export_suggestions[page_id];
    m_output_filename = out_path + fname_gen.fileNameFor(page_id);
    const QFileInfo fi(m_output_filename);
    m_output_filesize = fi.exists() ? fi.size() : 0;

    QDir export_dir(export_path);
    if (!export_dir.exists()) {
        export_dir.mkpath(export_dir.path());
    }

    const bool will_be_layered = m_export_suggestion.hasColorLayer && m_export_suggestion.hasBWLayer;

    QString layer_fname =
            will_be_layered ? export_dir.path() + "/pic/" + fi.fileName() : "";
    set_export_backgroundFilename(layer_fname);

    layer_fname = will_be_layered ? export_dir.path() + "/txt/" + fi.fileName() : "";
    set_export_foregroundFilename(layer_fname);

    if (m_export_suggestion.hasColorLayer) {
        set_export_bg44Filename(djvu_path + QFileInfo(m_output_filename).completeBaseName() + ".bg44");
    }
    if (m_export_suggestion.hasBWLayer) {
        set_export_jb2Filename(djvu_path + QFileInfo(m_output_filename).completeBaseName() + ".jb2");
    }
}

void
SourceImagesInfo::setFilename(QString& fname, uint& fsize, const QString& new_fname)
{
    if (fname != new_fname) {
        if (!fname.isEmpty()) {
            QDir().remove(fname);
            fsize = 0;
        }
    }
    fname = new_fname;
    if (!fname.isEmpty()) {
        const QFileInfo fi(fname);
        fsize = fi.exists() ? fi.size() : 0;
    }
}

void
SourceImagesInfo::set_export_backgroundFilename(const QString& new_fname)
{
    setFilename(m_export_backgroundFilename, m_export_backgroundFilesize, new_fname);
}

void
SourceImagesInfo::set_export_foregroundFilename(const QString& new_fname)
{
    setFilename(m_export_foregroundFilename, m_export_foregroundFilesize, new_fname);
}

void
SourceImagesInfo::set_export_bg44Filename(const QString& new_fname)
{
    setFilename(m_export_bg44Filename, m_export_bg44Filesize, new_fname);
}

void
SourceImagesInfo::set_export_jb2Filename(const QString& new_fname)
{
    setFilename(m_export_jb2Filename, m_export_jb2Filesize, new_fname);
}


void
SourceImagesInfo::update()
{
    QFileInfo fi(m_output_filename);
    m_output_filesize = fi.exists() ? fi.size() : 0;
    m_export_backgroundFilesize = m_export_foregroundFilesize = 0;

    if (!m_export_backgroundFilename.isEmpty()) {
        fi.setFile(m_export_backgroundFilename);
        m_export_backgroundFilesize = fi.exists() ? fi.size() : 0;
    }

    if (!m_export_foregroundFilename.isEmpty()) {
        fi.setFile(m_export_foregroundFilename);
        m_export_foregroundFilesize = fi.exists() ? fi.size() : 0;
    }

    if (!m_export_bg44Filename.isEmpty()) {
        fi.setFile(m_export_bg44Filename);
        m_export_bg44Filesize = fi.exists() ? fi.size() : 0;
    }

    if (!m_export_jb2Filename.isEmpty()) {
        fi.setFile(m_export_jb2Filename);
        m_export_jb2Filesize = fi.exists() ? fi.size() : 0;
    }
}

SourceImagesInfo::SourceImagesInfo(QDomElement const& el)
{
    XmlUnmarshaller unmarshaller;

    QDomElement e = el.namedItem("output").toElement();
    m_output_filename = unmarshaller.string(e);
    m_output_filesize = e.attribute("size", "0").toUInt();
    e = el.namedItem("foreground").toElement();
    m_export_foregroundFilename = unmarshaller.string(e);
    m_export_foregroundFilesize = e.attribute("size", "0").toUInt();
    e = el.namedItem("background").toElement();
    m_export_backgroundFilename = unmarshaller.string(e);
    m_export_backgroundFilesize = e.attribute("size", "0").toUInt();
    e = el.namedItem("bg44").toElement();
    m_export_bg44Filename = unmarshaller.string(e);
    m_export_bg44Filesize = e.attribute("size", "0").toUInt();
    e = el.namedItem("jb2").toElement();
    m_export_jb2Filename = unmarshaller.string(e);
    m_export_jb2Filesize = e.attribute("size", "0").toUInt();

    e = el.namedItem("suggest").toElement();
    m_export_suggestion = ExportSuggestion(e);
}

QDomElement
SourceImagesInfo::toXml(QDomDocument& doc, QString const& name) const
{
    XmlMarshaller marshaller(doc);
    QDomElement el(doc.createElement(name));

    QDomElement e = marshaller.string(m_output_filename, "output");
    e.setAttribute("size", QString::number(m_output_filesize));
    el.appendChild(e);
    e = marshaller.string(m_export_foregroundFilename, "foreground");
    e.setAttribute("size", QString::number(m_export_foregroundFilesize));
    el.appendChild(e);
    e = marshaller.string(m_export_backgroundFilename, "background");
    e.setAttribute("size", QString::number(m_export_backgroundFilesize));
    el.appendChild(e);
    e = marshaller.string(m_export_bg44Filename, "bg44");
    e.setAttribute("size", QString::number(m_export_bg44Filesize));
    el.appendChild(e);
    e = marshaller.string(m_export_jb2Filename, "jb2");
    e.setAttribute("size", QString::number(m_export_jb2Filesize));
    el.appendChild(e);
    el.appendChild(m_export_suggestion.toXml(doc, "suggest"));

    return el;
}

} // namespace publish
