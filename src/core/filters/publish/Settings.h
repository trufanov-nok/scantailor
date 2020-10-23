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

#ifndef PUBLISH_SETTINGS_H_
#define PUBLISH_SETTINGS_H_

#include "RefCountable.h"
#include "NonCopyable.h"
#include "Params.h"
#include "DjbzDispatcher.h"
#include <QMutex>
#include <map>
#include <set>

class AbstractRelinker;

namespace publish
{

class Settings : public QObject, public RefCountable
{
    Q_OBJECT
    DECLARE_NON_COPYABLE(Settings)
public:
    Settings();

    virtual ~Settings();

    void setPageParams(PageId const& page_id, Params const& params);

    void clearPageParams(PageId const& page_id);

    std::unique_ptr<Params> getPageParams(PageId const& page_id) const;

    void clear();

    void performRelinking(AbstractRelinker const& relinker);

    void generateEncoderSettings(const QVector<PageId> &pages,  const ExportSuggestions& export_suggestions, QStringList& settings);

    const DjbzDispatcher& djbzDispatcherConst() const;
    DjbzDispatcher& djbzDispatcher();

    QMap<QString, QString> metadata() const { return m_metadata; }
    const QMap<QString, QString>& metadataRef() const { return m_metadata; }
    void setMetadata( const QMap<QString, QString>& metadata) { m_metadata = metadata; }

    const QString& bundledDocFilename() const {return m_bundledDocFilename; }
    void setBundledDocFilename(const QString& fname) {
        m_bundledDocFilename = fname;
        updateBundledDoc();
        emit bundledDjVuFilenameChanged(m_bundledDocFilename);
    }

    void updateBundledDoc() {
        if (!m_bundledDocFilename.isEmpty() &&
                QFileInfo::exists(m_bundledDocFilename)) {
            const QFileInfo fi(m_bundledDocFilename);
            m_bundledDocFilesize = fi.size();
            m_bundledDocModified = fi.lastModified();
            emit bundledDocReady(true);
        } else {
            resetBundledDoc();
        }
    }

    bool bundledDocNeedsUpdate() {
        if (!m_bundledDocFilename.isEmpty() &&
                QFileInfo::exists(m_bundledDocFilename)) {
            const QFileInfo fi(m_bundledDocFilename);
            if ( m_bundledDocFilesize == fi.size() &&
                 m_bundledDocModified == fi.lastModified() ) {
                return false;
            }
        }
        return true;
    }

    void resetBundledDoc() {
        m_bundledDocFilesize = 0;
        m_bundledDocModified = QDateTime();
        emit bundledDocReady(false);
    }

    uint bundledDocFilesize() const {return m_bundledDocFilesize; }
    const QDateTime& bundledDocModified() const {return m_bundledDocModified; }

    const QStringList& contents() const { return m_contents; }
    void setContents(const QStringList val) { m_contents = val; }
    bool checkPagesReady(const std::vector<PageId> &pages) const;

Q_SIGNALS:
    void bundledDocReady(bool);
    void bundledDjVuFilenameChanged(const QString& filename);
private:
    typedef std::map<PageId, Params> PerPageParams;

    mutable QMutex m_mutex;
    PerPageParams m_perPageParams;

    DjbzDispatcher m_ptrDjbzDispatcher;
    QMap<QString, QString> m_metadata;

    QString m_bundledDocFilename;
    uint m_bundledDocFilesize;
    QDateTime m_bundledDocModified;

    QStringList m_contents;
};

} // namespace publish

#endif
