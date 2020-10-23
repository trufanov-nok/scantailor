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

#ifndef DJBZDISPATCHER_H
#define DJBZDISPATCHER_H

#include "PageId.h"
#include "IntrusivePtr.h"

#include <QMap>
#include <QSet>
#include <QDateTime>

class QDomDocument;
class QDomElement;
class QDateTime;
class PageSequence;
class OutputFileNameGenerator;
class ExportSuggestions;

namespace publish {

class Settings;

class DjbzParams
{
public:
    enum ClassifierType {
        Legacy = 1,
        Normal = 2,
        Maximal= 3
    };

    DjbzParams();
    DjbzParams(QDomElement const& el);

    bool usePrototypes() const { return m_usePrototypes; }
    void setUsePrototypes(bool prototypes) { m_usePrototypes = prototypes; }

    bool useAveraging() const { return m_useAveraging; }
    void setUseAveraging(bool averaging) { m_useAveraging = averaging; }

    int agression() const { return m_aggression; }
    void setAgression(int agression) { m_aggression = agression; }

    bool useErosion() const { return m_useErosion; }
    void setUseErosion(bool erosion) { m_useErosion = erosion; }

    int classifierType() const { return m_classifier; }
    void setClassifierType(ClassifierType classifierType) { m_classifier = classifierType; }

    QString extension() const { return m_extension; }
    void setExtension(const QString & ext) { m_extension = ext; }

    QDomElement toXml(QDomDocument& doc, QString const& name) const;

    bool operator !=(const DjbzParams &b) const;

    ~DjbzParams(){}
private:
    bool m_usePrototypes;
    bool m_useAveraging;
    int  m_aggression;
    bool  m_useErosion;
    ClassifierType m_classifier;
    QString m_extension;
};


class DjbzDict
{
public:
    enum Type {
        AutoFill, // page can automatically added to this djbz
        Locked,   // only user can add pages to this djbz
        None      // that's a virtual djbz. Page won't be added to any djbz
    };

    DjbzDict();

    Type type() const;
    void setType(Type type);

    int  maxPages() const;
    void setMaxPages(int max);

    void addPage(const PageId& page, bool no_rev_change = false);
    void removePage(const PageId& page, bool no_rev_change = false);
    const QSet<PageId>& pages() const;

    void setParams(const DjbzParams& params, bool no_rev_change = false);
    const DjbzParams& params() const;
    DjbzParams& paramsRef();

    QDateTime revision() const;
    void setRevision(QDateTime val);

    int pageCount() const { return m_pages.count(); }

    void setOutputFilename(const QString& val) { m_output_file = val; updateOutputFileInfo(); }
    const QString& outputFilename() const { return m_output_file; }
    uint outputFileSize() const { return m_output_file_size; }
    void setOutputFileSize(uint val) { m_output_file_size = val; }
    const QDateTime& outputLastChanged() const { return m_output_file_last_changed; }
    void setOutputLastChanged(const QDateTime& val) { m_output_file_last_changed = val; }
    void updateOutputFileInfo();

private:
    QSet<PageId> m_pages;
    DjbzParams m_params;
    Type m_type;
    int m_max_pages;
    QDateTime m_last_changed; // last change made by user
    QString m_output_file;
    uint m_output_file_size;
    QDateTime m_output_file_last_changed; // time when djbz filde was generated
};

typedef QMap<QString, DjbzDict > DjbzContent;
typedef QMap<PageId, QString> PageToDjbz;

class DjbzDispatcher
{
public:
    DjbzDispatcher();
    DjbzDispatcher(QDomElement const& el);

    QString findDjbzForPage(PageId const& page_id) const;
    DjbzDict djbzDict(const QString &dict_id) const;
    DjbzDict& djbzDictRef(const QString &dict_id);
    void SetDjbzDict(QString const& dict_id, DjbzDict const& dict);
    QStringList listAllDjbz() const;
    QMap<QString, int> listAllDjbzAndTheirSize() const;
    void ensurePageHasDjbz(const PageId& page_id);
    QString addNewPage(PageId const& page_id);
    void deleteFromDjbz(PageId const& page_id);
    void setToDjbz(PageId const& page_id, QString const& new_djbz, bool no_rev_change = false);
    void moveToDjbz(PageId const& page_id, QString const& new_djbz);
    void resetAllDictsExceptLocked();
    void autosetPagesToDjbz(const PageSequence& pages, const ExportSuggestions* export_suggestions, IntrusivePtr<Settings> settings);
    bool isInitialised() const {return !m_page_to_dict.isEmpty();}

    QSet<PageId> listPagesFromDict(const QString& djbz_id) const;
    QSet<PageId> listPagesFromSameDict(const PageId& page) const;
    void generateDjbzEncoderParams(const PageId& page, Settings const& page_settings, QStringList& encoder_settings, QString& output_file) const;
    bool isDjbzEncodingRequired(const PageId& page) const;
    bool isDummyDjbzId(const QString id) const;
    bool isDjbzCached(const QString& dict_id) const;
    QDomElement toXml(QDomDocument& doc, QString const& name) const;
public:
    static const QString dummyDjbzId;
private:
    QString nextDjbzId();
private:
    DjbzContent m_dictionaries;
    PageToDjbz m_page_to_dict;
    int m_id_counter;
};

}

#endif // DJBZDISPATCHER_H
