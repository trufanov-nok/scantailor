/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C)  Joseph Artsimovich <joseph.artsimovich@gmail.com>

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

#ifndef OUTPUT_FILTER_H_
#define OUTPUT_FILTER_H_

#include "NonCopyable.h"
#include "AbstractFilter.h"
#include "PageView.h"
#include "IntrusivePtr.h"
#include "FilterResult.h"
#include "SafeDeletingQObjectPtr.h"
#include "PictureZonePropFactory.h"
#include "FillZonePropFactory.h"
#include "ProjectPages.h"
#include <QCoreApplication>
#include <QImage>

class PageId;
class PageSelectionAccessor;
class ThumbnailPixmapCache;
class OutputFileNameGenerator;
class QString;
class ExportSuggestion;
class ExportSuggestions;

namespace publish
{
class Task;
class CacheDrivenTask;
}

namespace output
{

class OptionsWidget;
class Task;
class CacheDrivenTask;
class Settings;

class Filter : public AbstractFilter
{
    DECLARE_NON_COPYABLE(Filter)
    Q_DECLARE_TR_FUNCTIONS(output::Filter)
public:
    Filter(IntrusivePtr<ProjectPages> const& pages, PageSelectionAccessor const& page_selection_accessor);

    virtual ~Filter();

    virtual QString getName() const;

    virtual PageView getView() const;

    virtual void performRelinking(AbstractRelinker const& relinker);

    virtual void preUpdateUI(FilterUiInterface* ui, PageId const& page_id);

    virtual QDomElement saveSettings(
        ProjectWriter const& writer, QDomDocument& doc) const;

    virtual void loadSettings(
        ProjectReader const& reader, QDomElement const& filters_el);

    virtual void invalidateSetting(PageId const& page);

    IntrusivePtr<Task> createTask(PageId const& page_id,
                                  IntrusivePtr<ThumbnailPixmapCache> const& thumbnail_cache,
                                  OutputFileNameGenerator const& out_file_name_gen,
                                  IntrusivePtr<publish::Task> const& next_task,
//begin of modified by monday2000
//Original_Foreground_Mixed
                                  //bool batch, bool debug);
                                  bool batch, bool debug,
                                  bool keep_orig_fore_subscan = false,
                                  QImage* p_orig_fore_subscan = nullptr);
//end of modified by monday2000

    IntrusivePtr<CacheDrivenTask> createCacheDrivenTask(
        OutputFileNameGenerator const& out_file_name_gen,
        IntrusivePtr<publish::CacheDrivenTask> const& next_task);

    OptionsWidget* optionsWidget()
    {
        return m_ptrOptionsWidget.get();
    }
    Settings* getSettings()
    {
        return m_ptrSettings.get();
    }

    const ExportSuggestions* exportSuggestions() const;

    QStringList getZonesInfo(const PageId& id) const;

    virtual std::vector<PageOrderOption> pageOrderOptions() const;
    virtual int selectedPageOrder() const;
    virtual void selectPageOrder(int option);

    bool checkReadyToPublish(OutputFileNameGenerator const& filename_gen,
                             ProjectPages const& pages, PageId const* ignore) const;
private:
    void writePageSettings(
        QDomDocument& doc, QDomElement& filter_el,
        PageId const& page_id, int numeric_id) const;

    IntrusivePtr<ProjectPages> m_ptrPages;
    IntrusivePtr<Settings> m_ptrSettings;
    SafeDeletingQObjectPtr<OptionsWidget> m_ptrOptionsWidget;
    PictureZonePropFactory m_pictureZonePropFactory;
    FillZonePropFactory m_fillZonePropFactory;
    std::vector<PageOrderOption> m_pageOrderOptions;
    int m_selectedPageOrder;
};

} // namespace output

#endif
