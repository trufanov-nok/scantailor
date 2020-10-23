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

#include "Task.h"
#include "Filter.h"
#include "OptionsWidget.h"
#include "Settings.h"
#include "ThumbnailPixmapCache.h"
#include "FilterData.h"
#include "ImageTransformation.h"
#include "TaskStatus.h"
#include "FilterUiInterface.h"
#include "OutputParams.h"
#include "DjbzDispatcher.h"
#include "PageSequence.h"
#include "CommandLine.h"
#include "ExportSuggestions.h"
#include "exporting/ExportThread.h"
#include "EncodingProgressInfo.h"

#include <QFileDialog>
#include <QTemporaryFile>
#include <QProcess>
#include <QTextStream>
#include <QDebug>

#include <iostream>

using namespace exporting;

namespace publish
{

class Task::UiUpdater : public FilterResult
{
public:
    UiUpdater(IntrusivePtr<Filter> const& filter, const PageId &page_id,
              const DjbzDispatcher &djbzDispatcher, bool batch_processing);

    virtual void updateUI(FilterUiInterface* wnd);

    virtual IntrusivePtr<AbstractFilter> filter()
    {
        return m_ptrFilter;
    }
private:
    IntrusivePtr<Filter> m_ptrFilter;
    PageId m_pageId;
    const DjbzDispatcher &m_refDjbzDispatcher;
    bool m_batchProcessing;
};

Task::Task(PageId const& page_id,
           IntrusivePtr<Filter> const& filter,
           IntrusivePtr<Settings> const& settings,
           const IntrusivePtr<ThumbnailPixmapCache> &thumbnail_cache,
           const OutputFileNameGenerator &out_file_name_gen,
           bool const batch_processing)
    :
      m_pageId(page_id),
      m_ptrFilter(filter),
      m_ptrSettings(settings),
      m_ptrThumbnailCache(thumbnail_cache),
      m_refOutFileNameGen(out_file_name_gen),
      m_refDjbzDispatcher(m_ptrSettings->djbzDispatcher()),
      m_batchProcessing(batch_processing),
      m_ptrExportSuggestions(nullptr),

      m_out_path(m_refOutFileNameGen.outDir() + "/"),
      m_djvu_path( m_out_path + GlobalStaticSettings::m_djvu_pages_subfolder + "/"),
      m_export_path( m_djvu_path + GlobalStaticSettings::m_djvu_layers_subfolder + "/")
{
    QDir dir(m_djvu_path);
    if (!dir.exists()) {
        dir.mkpath(m_djvu_path);
    }
}

Task::~Task()
{
}

bool
Task::needPageReprocess(const PageId& page_id) const
{
    assert(m_ptrExportSuggestions->contains(page_id));

    std::unique_ptr<Params> params(m_ptrSettings->getPageParams(page_id));
    if (!params.get()) {
        return true;
    }

    Params::Regenerate val = params->getForceReprocess();
    if (val & Params::RegeneratePage) {
        return true;
    }

    const QString djbz_id = m_refDjbzDispatcher.findDjbzForPage(page_id);
    if (djbz_id.isEmpty()) {
        return true;
    }

    const DjbzDict dict = m_refDjbzDispatcher.djbzDict(djbz_id);

    SourceImagesInfo new_ImagesInfo(page_id, m_refOutFileNameGen, *m_ptrExportSuggestions);
    if (new_ImagesInfo != params->sourceImagesInfo()) {
        return true;
    }

    if (params->hasOutputParams()) {
        OutputParams const output_params_to_use(*params, djbz_id, dict.revision(), dict.params());
        OutputParams const& output_params_was_used(params->outputParams());
        return !output_params_was_used.matches(output_params_to_use) ||
                !params->isDjVuCached();
    } else {
        return true;
    }

    return false;

}

bool
Task::needDjbzReprocess(const QString& djbz_id) const
{
    // needPageReprocess(m_PageId) must be called first !

    if (m_refDjbzDispatcher.isDummyDjbzId(djbz_id)) {
        return false;
    }

    const DjbzDict dict = m_refDjbzDispatcher.djbzDict(djbz_id);
    for (const PageId& p: dict.pages()) {
        if (p != m_pageId) { // should be already checked in Task::process
            if (needPageReprocess(p)) {
                return true;
            }
        }
    }
    return false;
}

bool
Task::needReprocess(bool* djbz_is_cached) const
{
    const QString djbz_id = m_refDjbzDispatcher.findDjbzForPage(m_pageId);
    *djbz_is_cached = m_refDjbzDispatcher.isDjbzCached(djbz_id);
    if (*djbz_is_cached) {
        if (!needPageReprocess(m_pageId)) {
            // will check all pages except m_pageId
            return needDjbzReprocess(djbz_id);
        }
    }

    return true;
}

void
Task::validateParams(const PageId& page_id)
{
    std::unique_ptr<Params> params_ptr = m_ptrSettings->getPageParams(page_id);
    if (!params_ptr) {
        params_ptr.reset(new Params());
        m_ptrSettings->setPageParams(page_id, *params_ptr);
    }

    const QString djbz_id = m_refDjbzDispatcher.findDjbzForPage(page_id);
    if (djbz_id.isEmpty()) {
        params_ptr->setDjbzId( m_refDjbzDispatcher.addNewPage(page_id) );
        m_ptrSettings->setPageParams(page_id, *params_ptr);
    }

    const SourceImagesInfo new_ImagesInfo(page_id, m_refOutFileNameGen, *m_ptrExportSuggestions);
    const SourceImagesInfo info = params_ptr->sourceImagesInfo();
    if (!info.isValid() || info != new_ImagesInfo) {
        params_ptr->setSourceImagesInfo(
                    new_ImagesInfo
                    );
        m_ptrSettings->setPageParams(page_id, *params_ptr);
    }
}

void
Task::validateDjbzParams(const QString& djbz_id)
{
    // validateParams(m_PageId) must be called first !

    const DjbzDict dict = m_refDjbzDispatcher.djbzDict(djbz_id);
    for (const PageId& p: dict.pages()) {
        if (p != m_pageId) {
            validateParams(p);
        }
    }
}

void
Task::validateParams()
{
    validateParams(m_pageId);
    validateDjbzParams(m_refDjbzDispatcher.findDjbzForPage(m_pageId));
}

void
Task::startExport(TaskStatus const& status, const QSet<PageId> &pages_to_export)
{
    if (pages_to_export.isEmpty()) {
        return;
    }

    ExportSettings sett;
    sett.mode = ExportMode(ExportMode::Foreground | ExportMode::Background);
    sett.page_gen_tweaks = PageGenTweak(PageGenTweak::NoTweaks);
    sett.save_blank_background = false;
    sett.save_blank_foreground = false;
    sett.export_selected_pages_only = true;
    sett.export_to_multipage = false;
    sett.use_sep_suffix_for_pics = true;

    QVector<ExportThread::ExportRec> recs;
    for (const PageId& p : pages_to_export) {
        std::unique_ptr<Params> params_ptr = m_ptrSettings->getPageParams(p);
        const SourceImagesInfo info = params_ptr->sourceImagesInfo();
        ExportThread::ExportRec rec;
        rec.page_id = p;
        rec.page_no = 0;
        rec.filename = info.output_filename();
        rec.override_background_filename = info.export_backgroundFilename();
        rec.override_foreground_filename = info.export_foregroundFilename();
        recs += rec;
    }

    const int total_pages = recs.size();
    int processed_pages_no = 0;
    ExportThread thread(sett, recs, m_export_path, *m_ptrExportSuggestions, this);
    connect(&thread, &ExportThread::imageProcessed, this, [&processed_pages_no, total_pages, this](){
        processed_pages_no++;
        const float progress = 100.*processed_pages_no/total_pages;
        emit displayProgressInfo(progress, EncodingProgressProcess::Export, EncodingProgressState::InProgress);

    });
    thread.start();
    while (!thread.wait(1000)) {
        if (status.isCancelled()) {
            thread.requestInterruption();
        }
        status.throwIfCancelled();
    }

    emit displayProgressInfo(100., EncodingProgressProcess::Export, EncodingProgressState::Completed);

    for (const PageId& p : pages_to_export) {
        std::unique_ptr<Params> params_ptr = m_ptrSettings->getPageParams(p);
        SourceImagesInfo info = params_ptr->sourceImagesInfo();
        info.update();
        params_ptr->setSourceImagesInfo(info);
        m_ptrSettings->setPageParams(p, *params_ptr);
    }
}

void canUseCache(const std::unique_ptr<Params>& params, bool & use_jb2, bool& use_bg44)
{
    use_bg44 = use_jb2 = false;
    if (params->hasOutputParams()) {
        const Params& used_params = params->outputParams().params();
        use_jb2 = params->matchJb2Part(used_params);
        use_bg44 = params->matchBg44Part(used_params);
    }
}

bool requireReassembling(const std::unique_ptr<Params>& params)
{
    if (params->hasOutputParams()) {
        const Params& used_params = params->outputParams().params();
        return !params->matchAssemblePart(used_params);
    }
    return true;
}

bool requirePostprocessing(const std::unique_ptr<Params>& params, bool& force_reassambling)
{
    force_reassambling = false;
    bool require_postprocessing = true;
    if (params->hasOutputParams()) {
        const Params& used_params = params->outputParams().params();

        require_postprocessing = !params->matchPostprocessPart(used_params);

        if (require_postprocessing) {
            if (params->title().isEmpty() &&
                    !used_params.title().isEmpty()) {
                // the only way to clear title is to reassemble doc and not set it at all.
                force_reassambling = true;
            }
        }
    }
    return require_postprocessing;
}

bool isJB2cached(const SourceImagesInfo& info)
{
    if (!info.export_jb2Filename().isEmpty()) {
        const QFileInfo fi(info.export_jb2Filename());
        if (fi.exists() && fi.size() == info.export_jb2Filesize()) {
            return true;
        }
    }
    return false;
}

bool isBG44cached(const SourceImagesInfo& info)
{
    if (!info.export_bg44Filename().isEmpty()) {
        const QFileInfo fi(info.export_bg44Filename());
        if (fi.exists() && fi.size() == info.export_bg44Filesize()) {
            return true;
        }
    }
    return false;
}


FilterResultPtr
Task::process(TaskStatus const& status, FilterData const& data)
{
    status.throwIfCancelled();

    m_ptrFilter->imageViewer()->hide();
    m_ptrFilter->djVuWidget()->setDocument(new QDjVuDocument(true)/*nullptr*/);

    m_ptrExportSuggestions = data.exportSuggestions();
    assert(m_ptrExportSuggestions);

    const PageSequence page_seq = m_ptrFilter->pages()->toPageSequence(PAGE_VIEW);
    const std::vector<PageId> all_pages_ordered = page_seq.asPageIdVector();

    m_refDjbzDispatcher.autosetPagesToDjbz(page_seq, m_ptrExportSuggestions, m_ptrSettings);


    bool djbz_is_cached;
    // check if this page or any of pages in shared dictionary that this page belongs is changed
    const bool need_reprocess = needReprocess(&djbz_is_cached);

    if (need_reprocess) {

        emit m_ptrSettings->bundledDocReady(false);
        emit setProgressPanelVisible(true);

        // check that all params are exist for every page in shared dictionary
        // and create default params if needed
        validateParams();

        // Ok, we are going to rebuild the shared dictionary

        const QString djbz_id = m_refDjbzDispatcher.findDjbzForPage(m_pageId);
        const DjbzDict dict = m_refDjbzDispatcher.djbzDict(djbz_id);
        const QSet<PageId> dictionary_pages = m_refDjbzDispatcher.listPagesFromSameDict(m_pageId);

        // There are several processing steps that page may or may not
        // pass depending on its content.
        // 1. Export foreground and background layers
        // 2. Encode source images or just their background layers with c44-fi encoder to bg44 chunk
        // 3. Encode source image or just their foreground layers with minidjvu-mod encoder to
        //    indirect multipage document. Such document has one djbz.
        // 4. Export jb2 chunks from pages and reassemble them with corresponding bg44 chunks back into djvu
        // 5. Apply postprocessing (djvused) settings to djvu pages.
        // 6. If doable - bundle whole pages in project into bundled multipage document (djvm).
        // 7. Apply postprocessing (djvused) settings to bundled document.

        QSet<PageId> to_export;
        QSet<PageId> to_c44;
        QSet<PageId> to_c44_cached; // pages that can reuse existing bg44 chunk
        QSet<PageId> to_minidjvu;
        QSet<PageId> to_minidjvu_cached; // pages that can reuse existing jb2 chunk
        QSet<PageId> to_assemble;
        QSet<PageId> to_postprocess;

        for (const PageId& p: dictionary_pages) {
            const ExportSuggestion es = m_ptrExportSuggestions->value(p);
            assert (es.isValid);

            bool may_reuse_jb2 = false;
            bool may_reuse_bg44 = false;
            bool reuse_jb2 = false;
            bool reuse_bg44 = false;

            const std::unique_ptr<Params> params = m_ptrSettings->getPageParams(p);
            Params::Regenerate val = params->getForceReprocess();
            if (val & Params::RegeneratePage) {
                val = (Params::Regenerate)(val & ~Params::RegeneratePage);
                params->setForceReprocess(val);
                m_ptrSettings->setPageParams(p, *params);
            } else {
                canUseCache(params, may_reuse_jb2, may_reuse_bg44);
            }

            const SourceImagesInfo& info = params->sourceImagesInfo();

            //            const bool djvu_cached = isDjVuCached(params);
            if (es.hasBWLayer) { // the page or its layer require jb2 encoding
                if ( may_reuse_jb2 && djbz_is_cached &&
                     isJB2cached(info) ) {
                    reuse_jb2 = true;
                    to_minidjvu_cached += p; // reuse existing result
                } else {
                    to_minidjvu += p;
                }
            }

            if (es.hasColorLayer) { // the page or its layer require bg44 encoding
                if ( may_reuse_bg44 && isBG44cached(info) ) {
                    reuse_bg44 = true;
                    to_c44_cached += p; // reuse existing result
                } else {
                    to_c44 += p;
                }
            }

            if (es.hasColorLayer && es.hasBWLayer) {
                if (!reuse_bg44 || !reuse_jb2) {
                    to_export += p; // the page require splitting to layers
                }
            } else // empty page
                if (!es.hasBWLayer && !es.hasColorLayer) {
                    if ( params->isDjVuCached() ) {
                        to_minidjvu_cached += p; // reuse existing result
                    } else {
                        to_minidjvu += p; // empty page will generate virtual entry in encoder settings
                    }
                }

            if (requireReassembling(params) || !params->isDjVuCached()) {
                to_assemble += p;
            }

            bool force_reassembling;
            if (requirePostprocessing(params, force_reassembling)) {
                to_postprocess += p;
                if (force_reassembling) {
                    // some postprocessing can't be undone without reassambling (page titles)
                    to_assemble += p;
                }
            }

        }

        to_assemble += to_c44;
        to_assemble += to_minidjvu;

        /************************************
         * Export pages to layers
         ************************************/

        if (!to_export.isEmpty()) {
            // some pages require splitting to layers
            startExport(status, to_export);
        } else {
            emit displayProgressInfo(100., EncodingProgressProcess::Export, EncodingProgressState::Skipped);
        }

        // we don't need to_export anymore

        /************************************
         * Encode bg44 chunks
         ************************************/


        if (!to_c44.isEmpty()) {
            const int total_pages = to_c44.size();
            int pages_processed = 0;
            const QVector<PageId> pages = to_c44.toList().toVector(); // omp don't work with qset
#pragma omp parallel shared(pages_processed, status)
#pragma omp for schedule(dynamic)
            for (const PageId& p: qAsConst(pages)) {

                status.throwIfCancelled();

                QStringList args;
                // params
                const std::unique_ptr<Params> params = m_ptrSettings->getPageParams(p);
                const int bsf = params->bsf();
                args << "-iff" << "-dpi" << QString::number(params->outputDpi().horizontal());
                if (bsf > 1) {
                    args << "-bsf" << QString::number(bsf)
                         << "-bsm" << scale_filter2str(params->scaleMethod());
                }

                // image to encode
                const SourceImagesInfo& info = params->sourceImagesInfo();
                const QString base_name = m_djvu_path + QFileInfo(info.output_filename()).completeBaseName();
                const bool layered = !info.export_backgroundFilename().isEmpty();
                const QString source = layered? info.export_backgroundFilename() :
                                                info.output_filename();

                args << source << base_name + ".bg44";

                qDebug() << args.join(' ');
                QProcess proc;
                proc.setProcessChannelMode(QProcess::QProcess::ForwardedChannels);
                proc.start(GlobalStaticSettings::m_djvu_bin_c44, args);
                proc.waitForFinished(-1);

                pages_processed++;
                const float progress = 100.*pages_processed/total_pages;
                emit displayProgressInfo(progress, EncodingProgressProcess::EncodePic, EncodingProgressState::InProgress);
            }
            emit displayProgressInfo(100., EncodingProgressProcess::EncodePic, EncodingProgressState::Completed);
        } else {
            emit displayProgressInfo(100., EncodingProgressProcess::EncodePic, EncodingProgressState::Skipped);
        }


        /************************************
         * Encode jb2 chunks
         ************************************/

        if (!to_minidjvu.isEmpty()) {

            QStringList encoder_params;
            QString output_filename;

            if (!to_minidjvu_cached.isEmpty()) {
                // as jb2 pages are coded with shared dictionary
                // we can reuse cache if only all pages
                // in shared dictionary can do the same
                to_minidjvu += to_minidjvu_cached;
                // pages must be reassembled with new djbz
                to_assemble += to_minidjvu_cached;

                to_minidjvu_cached.clear();

            }

            for (const PageId& p: qAsConst(to_c44_cached)) {
                // fulfill pages with those which were cached
                // if they need to take part in reassembling djvu page
                if (to_minidjvu.contains(p)) {
                    to_c44 += p;
                }
            }


            QVector<PageId> to_minidjvu_ordered;
            for (const PageId& p: all_pages_ordered) {
                if (to_minidjvu.contains(p)) {
                    to_minidjvu_ordered.append(p);
                    if (to_minidjvu_ordered.size() == dictionary_pages.size()) break;
                }
            }


            m_ptrSettings->generateEncoderSettings(to_minidjvu_ordered, *m_ptrExportSuggestions, encoder_params);
            m_refDjbzDispatcher.generateDjbzEncoderParams(m_pageId, *m_ptrSettings, encoder_params, output_filename);

            qDebug() << "" << encoder_params << "";

            QTemporaryFile encoder_settings;
            if (encoder_settings.open()) {
                encoder_settings.write(encoder_params.join('\n').toUtf8());
                encoder_settings.flush();

                {
                    QProcess proc;
                    QStringList args;
                    args << "-u" << "-r" << "-j" << "-S" << encoder_settings.fileName() << m_djvu_path + output_filename;
                    proc.setProcessChannelMode(QProcess::QProcess::ForwardedErrorChannel);
                    proc.start(GlobalStaticSettings::m_djvu_bin_minidjvu, args);
                    proc.waitForStarted();
                    while (proc.state() != QProcess::NotRunning) {
                        while (proc.waitForReadyRead(1000)) {
                            if (proc.canReadLine()) {
                                const QString val(proc.readAll());
#if QT_VERSION >= 0x050E00
                                const QStringList sl = val.split("\n", Qt::SkipEmptyParts);
#else
                                const QStringList sl = val.split("\n", QString::SkipEmptyParts);
#endif
                                float progress = -1;
                                for (const QString& s: sl) {
                                    printf(" found %s\n", s.toStdString().c_str());
                                    if (s.startsWith("[") && s.endsWith("%]")) {
                                        progress = s.midRef(1,s.length()-3).toFloat();
                                    }
                                }
                                if (progress != -1) {
                                    emit displayProgressInfo(progress, EncodingProgressProcess::EncodeTxt, EncodingProgressState::InProgress);
                                }
                            }

                            if (status.isCancelled() && proc.state() != QProcess::NotRunning) {
                                proc.terminate();
                            }
                            status.throwIfCancelled();
                        }

                        if (status.isCancelled() && proc.state() != QProcess::NotRunning) {
                            proc.terminate();
                        }
                        status.throwIfCancelled();
                    }

                    emit displayProgressInfo(100., EncodingProgressProcess::EncodeTxt, EncodingProgressState::Completed);
                }
            }
        } else {
            emit displayProgressInfo(100., EncodingProgressProcess::EncodeTxt, EncodingProgressState::Skipped);
        }

        /************************************
         * Assemble djvu pages
         ************************************/

        if (!to_assemble.isEmpty()) {
            const int total_pages = to_assemble.size();
            int pages_processed = 0;
            const QVector<PageId> pages = to_assemble.toList().toVector(); // omp don't work with qset
#pragma omp parallel shared(pages_processed)
#pragma omp for schedule(dynamic)
            for (const PageId& p: qAsConst(pages)) {

                status.throwIfCancelled();

                QFileInfo fi;
                fi.setFile(m_refOutFileNameGen.fileNameFor(p));
                const QString base_name = m_djvu_path + fi.completeBaseName();

                QProcess proc;
                QStringList args;
                // overwrites target djvu if any
                args << base_name + ".djvu";
                const ExportSuggestion es = m_ptrExportSuggestions->value(p);
                args << QString("INFO=%1,%2,%3").arg(es.width).arg(es.height).arg(es.dpi);
                const std::unique_ptr<Params> params = m_ptrSettings->getPageParams(p);
                QString dict_id = params->djbzId();
                if (!m_refDjbzDispatcher.isDummyDjbzId(dict_id)) {
                    // must be before Sjbz chunk
                    dict_id += "." + m_refDjbzDispatcher.djbzDict(params->djbzId()).params().extension();
                    args << "INCL=" + dict_id;
                }

                if (params->fgbzOptions().isEmpty()) {
                    args << "FGbz=#black" + params->colorRectsAsTxt();
                } else {
                    args << "FGbz=" + params->fgbzOptions() + params->colorRectsAsTxt();
                }

                if (es.hasBWLayer) {
                    args << "Sjbz=" + base_name + ".jb2";
                }
                if (es.hasColorLayer) {
                    args << "BG44=" + base_name + ".bg44";
                }


                proc.setWorkingDirectory(m_djvu_path);
                qDebug() << args.join(" ");
                proc.start(GlobalStaticSettings::m_djvu_bin_djvumake, args);
                proc.waitForFinished(-1);


                pages_processed++;
                const float progress = 100.*pages_processed/total_pages;
                emit displayProgressInfo(progress, EncodingProgressProcess::Assemble, EncodingProgressState::InProgress);
            }
            emit displayProgressInfo(100., EncodingProgressProcess::Assemble, EncodingProgressState::Completed);
        } else {
            emit displayProgressInfo(100., EncodingProgressProcess::Assemble, EncodingProgressState::Skipped);
        }

        /************************************
         * Postprocess djvu pages
         ************************************/

        // check if we need to set page title or metadata
        if (!to_postprocess.isEmpty()) {
            //const int total_pages = to_postprocess.size();
            int pages_processed = 0;
            const QVector<PageId> pages = to_postprocess.toList().toVector(); // omp don't work with qset
#pragma omp parallel shared(pages_processed)
#pragma omp for schedule(dynamic)
            for (const PageId& p: qAsConst(pages)) {
                const std::unique_ptr<Params> params = m_ptrSettings->getPageParams(p);
                QString djvused_cmd;
                if (!params->title().isEmpty()) {
                    djvused_cmd += QString("select 1; set-page-title \"%1\"; ")
                            .arg(params->title());
                }

                djvused_cmd += QString("select 1; set-rotation \"%1\"; ")
                        .arg(params->rotation());


                if (!djvused_cmd.isEmpty()) {
                    QFileInfo fi;
                    fi.setFile(m_refOutFileNameGen.fileNameFor(p));
                    const QString fname = m_djvu_path + fi.completeBaseName() + ".djvu";
                    QStringList args;
                    args << fname << "-e" << djvused_cmd << "-s";

                    QProcess proc;
                    proc.setProcessChannelMode(QProcess::QProcess::ForwardedChannels);
                    proc.start(GlobalStaticSettings::m_djvu_bin_djvused, args);
                    proc.waitForFinished(-1);
                }
                pages_processed++;
            }
        }

        /************************************
         * Update params
         ************************************/
        {
            const std::unique_ptr<Params> params = m_ptrSettings->getPageParams(m_pageId);

            const QString dict_id = params->djbzId();
            DjbzDict& dict = m_refDjbzDispatcher.djbzDictRef(djbz_id);
            if (!m_refDjbzDispatcher.isDummyDjbzId(dict_id)) {
                const QString djbz_filename = m_djvu_path + "/" + dict_id + "." + m_refDjbzDispatcher.djbzDict(params->djbzId()).params().extension();
                dict.setOutputFilename(djbz_filename);
                dict.updateOutputFileInfo();
            }
            QDateTime revision = dict.revision();


            QFileInfo fi;
            for (const PageId& p: dictionary_pages) {
                const std::unique_ptr<Params> params = m_ptrSettings->getPageParams(p);
                params->setDjbzRevision(revision);
                fi.setFile(m_refOutFileNameGen.fileNameFor(p));
                const QString fname = m_djvu_path + fi.completeBaseName() + ".djvu";
                params->setDjvuFilename(fname);
                fi.setFile(fname);
                assert(fi.exists());
                params->setDjVuSize(fi.size());
                params->setDjVuLastChanged(fi.lastModified());

                SourceImagesInfo new_ImagesInfo = params->sourceImagesInfo();
                new_ImagesInfo.update();
                params->setSourceImagesInfo(new_ImagesInfo);

                params->rememberOutputParams(dict.params());

                m_ptrSettings->setPageParams(p, *params);
                m_ptrThumbnailCache->recreateThumbnail(ImageId(fname));
            }
        }

    } //need_reprocess

    emit setProgressPanelVisible(false);

    if (need_reprocess || m_ptrSettings->bundledDocNeedsUpdate()) {
        if (m_ptrSettings->checkPagesReady(all_pages_ordered)) {
            emit generateBundledDocument();
        }
    }


    if (CommandLine::get().isGui()) {
        return FilterResultPtr(
                    new UiUpdater(
                        m_ptrFilter, m_pageId,
                        m_refDjbzDispatcher,
                        m_batchProcessing
                        )
                    );
    } else {
        return FilterResultPtr(nullptr);
    }

}


/*============================ Task::UiUpdater ========================*/

Task::UiUpdater::UiUpdater(
        IntrusivePtr<Filter> const& filter,
        PageId const& page_id,
        const DjbzDispatcher& djbzDispatcher,
        bool const batch_processing)
    :   m_ptrFilter(filter),
      m_pageId(page_id),
      m_refDjbzDispatcher(djbzDispatcher),
      m_batchProcessing(batch_processing)
{
}

void
Task::UiUpdater::updateUI(FilterUiInterface* ui)
{
    // This function is executed from the GUI thread.
    OptionsWidget* const opt_widget = m_ptrFilter->optionsWidget();
    if (!m_batchProcessing) {
        opt_widget->postUpdateUI();
    }
    ui->setOptionsWidget(opt_widget, ui->KEEP_OWNERSHIP);

    m_ptrFilter->suppressDjVuDisplay(m_pageId, m_batchProcessing);
    const QSet<PageId> pages = m_refDjbzDispatcher.listPagesFromSameDict(m_pageId);
    for (const PageId& p: pages) {
        ui->invalidateThumbnail(p);
    }


    if (m_batchProcessing) {
        return;
    }

    ui->setImageWidget(m_ptrFilter->imageViewer(), ui->KEEP_OWNERSHIP);
    m_ptrFilter->updateDjVuDocument(m_pageId);
}

} // namespace publish
