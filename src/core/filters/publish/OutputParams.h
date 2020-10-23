/*
    Scan Tailor Universal - Interactive post-processing tool for scanned
    pages. A fork of Scan Tailor by Joseph Artsimovich.
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


#ifndef PUBLISH_OUTPUTPARAMS_H_
#define PUBLISH_OUTPUTPARAMS_H_

#include "Params.h"

#include <QDomDocument>


class ImageInfo;

namespace publish
{

class DjbzParams;
struct SourceImagesInfo;

class OutputParams
{
public:
    // Member-wise copying is OK.

    OutputParams();

    OutputParams(const Params& params, const QString& djbzId,
                 const QDateTime& djbzRevision, const DjbzParams& djbzParams);

    OutputParams(QDomElement const& deps_el);

    ~OutputParams();

    bool matches(OutputParams const& other) const;

    QDomElement toXml(QDomDocument& doc, QString const& name) const;

    const Params& params() const {return m_params; };
    const QString& djbzId() const { return m_djbzId; }
    const QDateTime& djbzRevision() const { return m_djbzRevision; }
    const DjbzParams& djbzParams() const { return m_djbzParams; }
    const SourceImagesInfo& sourceImagesInfo() const { return m_sourceImagesInfo; }
private:
    Params m_params;
    QString m_djbzId;
    QDateTime m_djbzRevision;
    DjbzParams m_djbzParams;
    SourceImagesInfo m_sourceImagesInfo;
};

} // namespace publish

#endif
