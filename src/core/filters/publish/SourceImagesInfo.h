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

#ifndef SOURCEIMAGESINFO_H
#define SOURCEIMAGESINFO_H

#include "ExportSuggestions.h"

class OutputFileNameGenerator;
class PageId;

namespace publish
{

class SourceImagesInfo
{
public:
    const ExportSuggestion& export_suggestion() const { return m_export_suggestion; };

    const QString& output_filename() const { return m_output_filename; };
    inline void set_export_foregroundFilename(const QString& new_fname);
    inline void set_export_backgroundFilename(const QString& new_fname);
    inline void set_export_bg44Filename(const QString& new_fname);
    inline void set_export_jb2Filename(const QString& new_fname);
    const QString& export_foregroundFilename() const { return m_export_foregroundFilename; };
    const QString& export_backgroundFilename() const { return m_export_backgroundFilename; };
    const QString& export_bg44Filename() const { return m_export_bg44Filename; };
    const QString& export_jb2Filename() const { return m_export_jb2Filename; };
    uint output_filesize() const { return m_output_filesize; };
    uint export_foregroundFilesize() const { return m_export_foregroundFilesize; };
    uint export_backgroundFilesize() const { return m_export_backgroundFilesize; };
    uint export_bg44Filesize() const { return m_export_bg44Filesize; };
    uint export_jb2Filesize() const { return m_export_jb2Filesize; };

    bool operator !=(const SourceImagesInfo &other) const;
    SourceImagesInfo();
    SourceImagesInfo(const PageId& page_id,
                     const OutputFileNameGenerator & fname_gen,
                     const ExportSuggestions& export_suggestions);
    SourceImagesInfo(QDomElement const& el);
    QDomElement toXml(QDomDocument& doc, QString const& name) const;
    void update();
    bool isValid() const { return !m_output_filename.isEmpty(); }

private:
    void setFilename(QString& fname, uint& fsize, const QString& new_fname);
private:
    ExportSuggestion m_export_suggestion;
    QString m_output_filename;
    QString m_export_foregroundFilename;
    QString m_export_backgroundFilename;
    QString m_export_bg44Filename;
    QString m_export_jb2Filename;
    uint m_output_filesize;
    uint m_export_foregroundFilesize;
    uint m_export_backgroundFilesize;
    uint m_export_bg44Filesize;
    uint m_export_jb2Filesize;
};

} //namespace publish

#endif // SOURCEIMAGESINFO_H
