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

#include "CacheDrivenTask.h"
#include "DjbzDispatcher.h"
#include "Settings.h"
#include "PageInfo.h"
#include "PageId.h"
#include "ImageId.h"
#include "IncompleteThumbnail.h"
#include "ImageTransformation.h"
#include "ThumbnailBase.h"
#include "AbstractFilterDataCollector.h"
#include "ThumbnailCollector.h"
#include "OutputParams.h"
#include "OutputFileNameGenerator.h"

namespace publish
{

CacheDrivenTask::CacheDrivenTask(IntrusivePtr<Settings> const& settings, OutputFileNameGenerator const& out_file_name_gen)
    : m_ptrSettings(settings),
      m_refDjbzDispatcher(m_ptrSettings->djbzDispatcher()),
      m_refOutputFileNameGenerator(out_file_name_gen)
{
}

CacheDrivenTask::~CacheDrivenTask()
{
}

bool
CacheDrivenTask::needPageReprocess(const PageId& page_id) const
{
    std::unique_ptr<Params> params(m_ptrSettings->getPageParams(page_id));
    if (!params.get()) {
        return true;
    }

    Params::Regenerate val = params->getForceReprocess();
    if (val & Params::RegenerateThumbnail) {
        val = (Params::Regenerate)(val & ~Params::RegenerateThumbnail);
        params->setForceReprocess(val);
        m_ptrSettings->setPageParams(page_id, *params);
        return true;
    }

    const QString djbz_id = m_refDjbzDispatcher.findDjbzForPage(page_id);
    if (djbz_id.isEmpty()) {
        return true;
    }

    if (params->hasOutputParams()) {
        const DjbzDict dict = m_refDjbzDispatcher.djbzDict(djbz_id);
        if (  *params != params->outputParams().params() ||
                params->djbzId() != params->outputParams().djbzId() ||
                params->djbzRevision() != params->outputParams().djbzRevision() ||
                dict.params() != params->outputParams().djbzParams() ||
                params->sourceImagesInfo() != params->outputParams().sourceImagesInfo()) {
            return false;
        }
    } else {
        return true;
    }

    return false;

}


void
CacheDrivenTask::process(
        PageInfo const& page_info, AbstractFilterDataCollector* collector,
        ImageTransformation const& xform)
{
    m_pageId = page_info.id();



    if (ThumbnailCollector* thumb_col = dynamic_cast<ThumbnailCollector*>(collector)) {

        bool need_reprocess = false;

        std::unique_ptr<Params> params = m_ptrSettings->getPageParams(m_pageId);
        if (!params.get()) {
            params.reset(new Params());
            m_ptrSettings->setPageParams(m_pageId, *params);
            need_reprocess = true;
        }

        if (!need_reprocess) {
            const QString djbz_id = m_refDjbzDispatcher.findDjbzForPage(m_pageId);
            need_reprocess = !m_refDjbzDispatcher.isDjbzCached(djbz_id);
        }

        if (!need_reprocess) {
            need_reprocess = !params->isDjVuCached();
        }

        if (!need_reprocess) {
            need_reprocess = needPageReprocess(m_pageId);
        }

        if (need_reprocess) {
            QString thumbnail_source;
            thumbnail_source = params->sourceImagesInfo().output_filename();
            if (thumbnail_source.isEmpty() || !QFileInfo::exists(thumbnail_source)) {
                thumbnail_source = m_refOutputFileNameGenerator.outDir() + "/" +
                        m_refOutputFileNameGenerator.fileNameFor(m_pageId);
                if (!QFileInfo::exists(thumbnail_source)) {
                    thumbnail_source = page_info.id().imageId().filePath();
                }
            }

            thumb_col->processThumbnail(
                        std::unique_ptr<QGraphicsItem>(
                            new IncompleteThumbnail(
                                thumb_col->thumbnailCache(),
                                thumb_col->maxLogicalThumbSize(),
                                ImageId(thumbnail_source), xform
                                )
                            )
                        );

        } else {

            thumb_col->processThumbnail(
                        std::unique_ptr<QGraphicsItem>(
                            new ThumbnailBase(
                                thumb_col->thumbnailCache(),
                                thumb_col->maxLogicalThumbSize(),
                                ImageId(params->djvuFilename()), xform
                                )
                            )
                        );
        }
    }
}

} // namespace publishing
