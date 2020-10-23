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

#include "Settings.h"
#include "Utils.h"
#include "RelinkablePath.h"
#include "AbstractRelinker.h"
#include "ExportSuggestions.h"
#include "OutputParams.h"

namespace publish
{

Settings::Settings(): QObject(nullptr), m_bundledDocFilesize(0)
{
}

Settings::~Settings()
{
}

void
Settings::clear()
{
    QMutexLocker locker(&m_mutex);
    m_perPageParams.clear();
}

void
Settings::performRelinking(AbstractRelinker const& relinker)
{
    QMutexLocker locker(&m_mutex);
    PerPageParams new_params;

    for (PerPageParams::value_type const& kv : m_perPageParams) {
        RelinkablePath const old_path(kv.first.imageId().filePath(), RelinkablePath::File);
        PageId new_page_id(kv.first);
        new_page_id.imageId().setFilePath(relinker.substitutionPathFor(old_path));
        new_params.insert(PerPageParams::value_type(new_page_id, kv.second));
    }

    m_perPageParams.swap(new_params);
}

void
Settings::setPageParams(PageId const& page_id, Params const& params)
{
    QMutexLocker locker(&m_mutex);
    Utils::mapSetValue(m_perPageParams, page_id, params);
}

void
Settings::clearPageParams(PageId const& page_id)
{
    QMutexLocker locker(&m_mutex);
    m_perPageParams.erase(page_id);
}

std::unique_ptr<Params>
Settings::getPageParams(PageId const& page_id) const
{
    QMutexLocker locker(&m_mutex);

    PerPageParams::const_iterator it(m_perPageParams.find(page_id));
    if (it != m_perPageParams.end()) {
        return std::unique_ptr<Params>(new Params(it->second));
    } else {
        return std::unique_ptr<Params>();
    }
}

void
Settings::generateEncoderSettings(const QVector<PageId>& pages, const ExportSuggestions& export_suggestions, QStringList& settings)
{
    settings.clear();
    settings << "(options"
             << "  indirect 1"
             << ") # options";
    settings << "(input-files";
    for (const PageId& p: pages) {
        const Params& params = m_perPageParams[p];
        const SourceImagesInfo si = params.sourceImagesInfo();
        const QString fname = si.export_foregroundFilename().isEmpty() ?
                    si.output_filename() :
                    si.export_foregroundFilename();
        settings << "  (file"
                 << "    " + fname
                 << "    (image";
        const ExportSuggestion& es = export_suggestions[p];
        if (!es.hasBWLayer && !es.hasColorLayer) {
            settings << QString("       virtual %1 %2").arg(es.width).arg(es.height)
                     << "       dpi     " + QString::number(es.dpi);
        } else {
            settings << "       dpi     " + QString::number(params.outputDpi().horizontal())
                     << "       smooth  " + QString(params.smooth() ? "1" : "0")
                     << "       clean   " + QString(params.clean() ? "1" : "0")
                     << "       erosion " + QString(params.erosion() ? "1" : "0");
        }
        settings << "    ) #image"
                 << "  ) #file";
    }
    settings << ") #input-files";
}

const DjbzDispatcher &Settings::djbzDispatcherConst() const
{
    return m_ptrDjbzDispatcher;
}

DjbzDispatcher &Settings::djbzDispatcher()
{
    return m_ptrDjbzDispatcher;
}

bool
Settings::checkPagesReady(const std::vector<PageId>& pages) const
{
    for (const PageId& p : pages) {

        const std::unique_ptr<Params> params = getPageParams(p);
        if (!params) {
            return false;
        }

        const QString djbz_id = params->djbzId();
        if (!m_ptrDjbzDispatcher.isDjbzCached(djbz_id)) {
            return false;
        }

        const DjbzDict dict = m_ptrDjbzDispatcher.djbzDict(djbz_id);
        OutputParams const output_params_to_use(*params, djbz_id, dict.revision(), dict.params());
        OutputParams const& output_params_was_used(params->outputParams());
        if (!output_params_was_used.matches(output_params_to_use)) {
            return false;
        }

        if (params->djvuFilename().isEmpty()) {
            return false;
        }

        const QFileInfo fi(params->djvuFilename());
        if (!fi.exists() || fi.size() != params->djvuSize()) {
            return false;
        }
        if (fi.lastModified() != params->djvuLastChanged()) {
            return false;
        }
    }

    return true;
}


} // namespace publish
