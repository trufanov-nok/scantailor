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

#ifndef STAGESEQUENCE_H_
#define STAGESEQUENCE_H_

#include "NonCopyable.h"
#include "RefCountable.h"
#include "IntrusivePtr.h"
#include "AbstractFilter.h"
#include "filters/fix_orientation/Filter.h"
#include "filters/page_split/Filter.h"
#include "filters/deskew/Filter.h"
#include "filters/select_content/Filter.h"
#include "filters/page_layout/Filter.h"
#include "filters/output/Filter.h"
#include "filters/publish/Filter.h"
#include <vector>

class PageId;
class ProjectPages;
class PageSelectionAccessor;
class AbstractRelinker;
class OutputFileNameGenerator;
class ThumbnailPixmapCache;
class CompositeCacheDrivenTask;

class StageSequence : public RefCountable
{
    DECLARE_NON_COPYABLE(StageSequence)
public:
    typedef IntrusivePtr<AbstractFilter> FilterPtr;

    StageSequence(IntrusivePtr<ProjectPages> const& pages,
                  PageSelectionAccessor const& page_selection_accessor);

    void performRelinking(AbstractRelinker const& relinker);

    std::vector<FilterPtr> const& filters() const
    {
        return m_filters;
    }

    int count() const
    {
        return m_filters.size();
    }

    FilterPtr const& filterAt(int idx) const
    {
        return m_filters[idx];
    }

    int findFilter(FilterPtr const& filter) const;

    IntrusivePtr<fix_orientation::Filter> const& fixOrientationFilter() const
    {
        return m_ptrFixOrientationFilter;
    }

    IntrusivePtr<page_split::Filter> const& pageSplitFilter() const
    {
        return m_ptrPageSplitFilter;
    }

    IntrusivePtr<deskew::Filter> const& deskewFilter() const
    {
        return m_ptrDeskewFilter;
    }

    IntrusivePtr<select_content::Filter> const& selectContentFilter() const
    {
        return m_ptrSelectContentFilter;
    }

    IntrusivePtr<page_layout::Filter> const& pageLayoutFilter() const
    {
        return m_ptrPageLayoutFilter;
    }

    IntrusivePtr<output::Filter> const& outputFilter() const
    {
        return m_ptrOutputFilter;
    }

    IntrusivePtr<publish::Filter> const& publishFilter() const {
        return m_ptrPublishFilter;
    }

    int fixOrientationFilterIdx() const
    {
        return m_fixOrientationFilterIdx;
    }

    int pageSplitFilterIdx() const
    {
        return m_pageSplitFilterIdx;
    }

    int deskewFilterIdx() const
    {
        return m_deskewFilterIdx;
    }

    int selectContentFilterIdx() const
    {
        return m_selectContentFilterIdx;
    }

    int pageLayoutFilterIdx() const
    {
        return m_pageLayoutFilterIdx;
    }

    int outputFilterIdx() const
    {
        return m_outputFilterIdx;
    }

    int publishFilterIdx() const
    {
        return m_publishFilterIdx;
    }

    IntrusivePtr<CompositeCacheDrivenTask>
    createCompositeCacheDrivenTask(OutputFileNameGenerator& name_generator, int const last_filter_idx);

    void setOutputFileNameGenerator(OutputFileNameGenerator* outputFileNameGenerator, ThumbnailPixmapCache* cache) {
        m_ptrPublishFilter->setOutputFileNameGenerator(outputFileNameGenerator, cache);
    }

private:
    IntrusivePtr<fix_orientation::Filter> m_ptrFixOrientationFilter;
    IntrusivePtr<page_split::Filter> m_ptrPageSplitFilter;
    IntrusivePtr<deskew::Filter> m_ptrDeskewFilter;
    IntrusivePtr<select_content::Filter> m_ptrSelectContentFilter;
    IntrusivePtr<page_layout::Filter> m_ptrPageLayoutFilter;
    IntrusivePtr<output::Filter> m_ptrOutputFilter;
    IntrusivePtr<publish::Filter> m_ptrPublishFilter;
    std::vector<FilterPtr> m_filters;
    int m_fixOrientationFilterIdx;
    int m_pageSplitFilterIdx;
    int m_deskewFilterIdx;
    int m_selectContentFilterIdx;
    int m_pageLayoutFilterIdx;
    int m_outputFilterIdx;
    int m_publishFilterIdx;
};

#endif
