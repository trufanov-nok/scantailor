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

#ifndef PUBLISH_CACHEDRIVENTASK_H_
#define PUBLISH_CACHEDRIVENTASK_H_

#include "NonCopyable.h"
#include "CompositeCacheDrivenTask.h"
#include "IntrusivePtr.h"
#include "PageId.h"

class PageInfo;
class AbstractFilterDataCollector;
class OutputFileNameGenerator;
class ImageTransformation;

namespace publish
{

class Settings;
class DjbzDispatcher;

class CacheDrivenTask : public RefCountable
{
    DECLARE_NON_COPYABLE(CacheDrivenTask)
public:
        CacheDrivenTask(
            IntrusivePtr<Settings> const& settings,
            OutputFileNameGenerator const& out_file_name_gen);

    virtual ~CacheDrivenTask() override;

    void process(PageInfo const& page_info, AbstractFilterDataCollector* collector, const ImageTransformation &xform);
private:
    bool needPageReprocess(const PageId &page_id) const;
private:
    IntrusivePtr<Settings> m_ptrSettings;
    DjbzDispatcher& m_refDjbzDispatcher;
    const OutputFileNameGenerator& m_refOutputFileNameGenerator;
    PageId m_pageId;
};

} // namespace publishing

#endif
