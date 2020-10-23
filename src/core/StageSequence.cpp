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

#include "StageSequence.h"
#include "ProjectPages.h"
//#include "OutputFileNameGenerator.h"
#include "CompositeCacheDrivenTask.h"
#include "filters/fix_orientation/CacheDrivenTask.h"
#include "filters/page_split/CacheDrivenTask.h"
#include "filters/deskew/CacheDrivenTask.h"
#include "filters/select_content/CacheDrivenTask.h"
#include "filters/page_layout/CacheDrivenTask.h"
#include "filters/output/CacheDrivenTask.h"
#include "filters/publish/CacheDrivenTask.h"

StageSequence::StageSequence(IntrusivePtr<ProjectPages> const& pages,
                             PageSelectionAccessor const& page_selection_accessor)
    :   m_ptrFixOrientationFilter(new fix_orientation::Filter(page_selection_accessor)),
        m_ptrPageSplitFilter(new page_split::Filter(pages, page_selection_accessor)),
        m_ptrDeskewFilter(new deskew::Filter(page_selection_accessor)),
        m_ptrSelectContentFilter(new select_content::Filter(page_selection_accessor)),
        m_ptrPageLayoutFilter(new page_layout::Filter(pages, page_selection_accessor)),
        m_ptrOutputFilter(new output::Filter(pages, page_selection_accessor)),
        m_ptrPublishFilter(new publish::Filter(this, pages, page_selection_accessor))
{
    m_fixOrientationFilterIdx = m_filters.size();
    m_filters.push_back(m_ptrFixOrientationFilter);

    m_pageSplitFilterIdx = m_filters.size();
    m_filters.push_back(m_ptrPageSplitFilter);

    m_deskewFilterIdx = m_filters.size();
    m_filters.push_back(m_ptrDeskewFilter);

    m_selectContentFilterIdx = m_filters.size();
    m_filters.push_back(m_ptrSelectContentFilter);

    m_pageLayoutFilterIdx = m_filters.size();
    m_filters.push_back(m_ptrPageLayoutFilter);

    m_outputFilterIdx = m_filters.size();
    m_filters.push_back(m_ptrOutputFilter);

    m_publishFilterIdx = m_filters.size();
    m_filters.push_back(m_ptrPublishFilter);
}

void
StageSequence::performRelinking(AbstractRelinker const& relinker)
{
    for (FilterPtr& filter : m_filters) {
        filter->performRelinking(relinker);
    }
}

int
StageSequence::findFilter(FilterPtr const& filter) const
{
    int idx = 0;
    for (FilterPtr const& f : m_filters) {
        if (f == filter) {
            return idx;
        }
        ++idx;
    }
    return -1;
}

IntrusivePtr<CompositeCacheDrivenTask>
StageSequence::createCompositeCacheDrivenTask(OutputFileNameGenerator &name_generator, int const last_filter_idx)
{
    IntrusivePtr<fix_orientation::CacheDrivenTask> fix_orientation_task;
    IntrusivePtr<page_split::CacheDrivenTask> page_split_task;
    IntrusivePtr<deskew::CacheDrivenTask> deskew_task;
    IntrusivePtr<select_content::CacheDrivenTask> select_content_task;
    IntrusivePtr<page_layout::CacheDrivenTask> page_layout_task;
    IntrusivePtr<output::CacheDrivenTask> output_task;
    IntrusivePtr<publish::CacheDrivenTask> publish_task;

    if (last_filter_idx >= publishFilterIdx()) {
        publish_task = publishFilter()
                ->createCacheDrivenTask(name_generator);
    }
    if (last_filter_idx >= outputFilterIdx()) {
        output_task = outputFilter()
                      ->createCacheDrivenTask(name_generator, publish_task);
    }
    if (last_filter_idx >= pageLayoutFilterIdx()) {
        page_layout_task = pageLayoutFilter()
                           ->createCacheDrivenTask(output_task);
    }
    if (last_filter_idx >= selectContentFilterIdx()) {
        select_content_task = selectContentFilter()
                              ->createCacheDrivenTask(page_layout_task);
    }
    if (last_filter_idx >= deskewFilterIdx()) {
        deskew_task = deskewFilter()
                      ->createCacheDrivenTask(select_content_task);
    }
    if (last_filter_idx >= pageSplitFilterIdx()) {
        page_split_task = pageSplitFilter()
                          ->createCacheDrivenTask(deskew_task);
    }
    if (last_filter_idx >= fixOrientationFilterIdx()) {
        fix_orientation_task = fixOrientationFilter()
                               ->createCacheDrivenTask(page_split_task);
    }

    assert(fix_orientation_task);

    return fix_orientation_task;
}
