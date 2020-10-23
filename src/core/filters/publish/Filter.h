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

#ifndef PUBLISH_FILTER_H_
#define PUBLISH_FILTER_H_

#include "NonCopyable.h"
#include "AbstractFilter.h"
#include "PageView.h"
#include "IntrusivePtr.h"
#include "FilterResult.h"
#include "SafeDeletingQObjectPtr.h"
#include "ProjectPages.h"
#include "CompositeCacheDrivenTask.h"
#include <QCoreApplication>
#include <QImage>
#include <QSet>
#include <memory> // std::unique_ptr

#include "djview4/qdjvu.h"
#include "djview4/qdjvuwidget.h"

class StageSequence;
class PageId;
class PageSelectionAccessor;
class ThumbnailPixmapCache;
class OutputFileNameGenerator;
class QString;

namespace publish
{

class OptionsWidget;
class Task;
class CacheDrivenTask;
class Settings;
class DjbzDispatcher;

/**
 * \note All methods of this class except the destructor
 *       must be called from the GUI thread only.
 */
class Filter : public QObject, public AbstractFilter
{
    Q_OBJECT
    DECLARE_NON_COPYABLE(Filter)
//    Q_DECLARE_TR_FUNCTIONS(publishing::Filter)
public:
    Filter(StageSequence* stages,
           IntrusivePtr<ProjectPages> const& pages, PageSelectionAccessor const& page_selection_accessor);

    virtual ~Filter() override;

    virtual QString getName() const override;

    virtual PageView getView() const override;

    virtual int selectedPageOrder() const override;

    virtual void selectPageOrder(int option) override;

    virtual std::vector<PageOrderOption> pageOrderOptions() const override;

    virtual void performRelinking(AbstractRelinker const& relinker) override;

    virtual void preUpdateUI(FilterUiInterface* ui, PageId const&) override;

    virtual QDomElement saveSettings(
        ProjectWriter const& writer, QDomDocument& doc) const override;

    virtual void loadSettings(
        ProjectReader const& reader, QDomElement const& filters_el) override;

    virtual void invalidateSetting(PageId const& page_id) override;

    IntrusivePtr<Task> createTask(
        PageId const& page_id,
        IntrusivePtr<ThumbnailPixmapCache> const& thumbnail_cache,
        OutputFileNameGenerator const& out_file_name_gen,
        bool batch_processing);

    IntrusivePtr<CacheDrivenTask> createCacheDrivenTask(OutputFileNameGenerator const& out_file_name_gen);

    OptionsWidget* optionsWidget() const
    {
        return m_ptrOptionsWidget.get();
    }

    StageSequence* stages() const
    {
        return m_ptrStages;
    }

    Settings* settings() const
    {
        return m_ptrSettings.get();
    }

    ThumbnailPixmapCache* thumbnailPixmapCache() const
    {
        return m_ThumbnailPixmapCache;
    }

    IntrusivePtr<CompositeCacheDrivenTask> createCompositeCacheDrivenTask();

    const ProjectPages* pages() const {
        return m_ptrPages.get();
    };

    QWidget* imageViewer() const { return m_ptrImageViewer.get(); }
    QDjVuWidget* djVuWidget() const { return m_ptrDjVuWidget.get(); }

    void suppressDjVuDisplay(const PageId &page_id, bool val);

    void setOutputFileNameGenerator(OutputFileNameGenerator* outputFileNameGenerator, ThumbnailPixmapCache* cache) {
        m_outputFileNameGenerator = outputFileNameGenerator;
        m_ThumbnailPixmapCache = cache;
    }

    void ensureAllPagesHaveDjbz();
    void ensureAllPagesHaveDjbz(DjbzDispatcher& dispatcher);
    void reassignAllPagesExceptLocked(DjbzDispatcher& dispatcher);

    void displayDbjzManagerDlg();
    void displayContentsManagerDlg();

    void setDjVuFilenameSuggestion(const QString& fname) { m_bundledDjVuSuggestion = fname; }
    const QString& djVuFilenameSuggestion() const { return m_bundledDjVuSuggestion; }
    void setBundledFilename(const QString& fname);
    const QString& bundledFilename() const;
    bool checkReadyToBundle() const;

    QVector<PageInfo> filterBatchPages(const QVector<PageInfo>& pages) const;
public Q_SLOTS:
    void updateDjVuDocument(const PageId &page_id);
    void makeBundledDjVu();
Q_SIGNALS:
    void setProgressPanelVisible(bool visible);
    void displayProgressInfo(float progress, int process, int state);
    void tabChanged(int idx);
    void enableBundledDjVuButton(bool enabled);
    void setBundledDjVuDoc(const QString& filename);

private:
    void setupImageViewer();
    void writePageSettings(QDomDocument& doc, QDomElement& filter_el,
        const PageId &page_id, int numeric_id) const;

    StageSequence* m_ptrStages;
    OutputFileNameGenerator* m_outputFileNameGenerator;
    ThumbnailPixmapCache* m_ThumbnailPixmapCache;
    IntrusivePtr<ProjectPages> m_ptrPages;
    IntrusivePtr<Settings> m_ptrSettings;
    std::unique_ptr<QWidget> m_ptrImageViewer;
    std::unique_ptr<OptionsWidget> m_ptrOptionsWidget;
    QDjVuContext m_DjVuContext;
    std::unique_ptr<QDjVuWidget> m_ptrDjVuWidget;

    std::vector<PageOrderOption> m_pageOrderOptions;
    int m_selectedPageOrder;
    bool m_suppressDjVuDisplay;
    QString m_bundledDjVuSuggestion;
};

} // publish

#endif
