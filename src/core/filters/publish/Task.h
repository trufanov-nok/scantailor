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

#ifndef PUBLISH_TASK_H_
#define PUBLISH_TASK_H_

#include "NonCopyable.h"
#include "RefCountable.h"
#include "FilterResult.h"
#include "IntrusivePtr.h"
#include "PageId.h"
#include "OutputFileNameGenerator.h"
#include "EncodingProgressInfo.h"
#include <QObject>

class TaskStatus;
class FilterData;
class QImage;
class ExportSuggestions;
class ThumbnailPixmapCache;

namespace publish
{

class Filter;
class Settings;
class DjbzDispatcher;

class Task : public QObject, public RefCountable
{
    Q_OBJECT
    DECLARE_NON_COPYABLE(Task)
    public:
        Task(const PageId &page_id,
             IntrusivePtr<Filter> const& filter,
             IntrusivePtr<Settings> const& settings,
             IntrusivePtr<ThumbnailPixmapCache> const& thumbnail_cache,
             OutputFileNameGenerator const& out_file_name_gen,
             bool batch_processing);

    virtual ~Task();

    FilterResultPtr process(TaskStatus const& status, FilterData const& data);

Q_SIGNALS:
//    void displayDjVu(const PageId& page_id);
    void setProgressPanelVisible(bool visible);
    void displayProgressInfo(float progress, int process, int state);
    void generateBundledDocument();
private:
    bool adjustSharedDjbz();
    bool needPageReprocess(const PageId &page_id) const;
    bool needDjbzReprocess(const QString& djbz_id) const;
    bool needReprocess(bool* djbz_is_cached) const;
    void validateParams(const PageId& page_id);
    void validateDjbzParams(const QString& djbz_id);
    void validateParams();
    void startExport(TaskStatus const& status, const QSet<PageId> &require_export);

    class UiUpdater;
    class EncoderThread;

    PageId m_pageId;
    IntrusivePtr<Filter> m_ptrFilter;
    IntrusivePtr<Settings> m_ptrSettings;
    IntrusivePtr<ThumbnailPixmapCache> m_ptrThumbnailCache;
    OutputFileNameGenerator const& m_refOutFileNameGen;
    DjbzDispatcher& m_refDjbzDispatcher;
    bool m_batchProcessing;
    const ExportSuggestions* m_ptrExportSuggestions;

    const QString m_out_path;
    const QString m_djvu_path;
    const QString m_export_path;
};

} // namespace publish

#endif
