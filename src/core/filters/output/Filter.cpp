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

#include "Filter.h"
#include "FilterUiInterface.h"
#include "OptionsWidget.h"
#include "Task.h"
#include "PageId.h"
#include "Settings.h"
#include "Params.h"
#include "OutputParams.h"
#include "ProjectReader.h"
#include "ProjectWriter.h"
#include "CacheDrivenTask.h"
#include <QString>
#include <QObject>
#include <QCoreApplication>
#include <QDomDocument>
#include <QDomElement>
#include <memory>
#include <tiff.h>

#include "CommandLine.h"
#include "ImageViewTab.h"
#include "OrderByModeProvider.h"
#include "OrderBySourceColor.h"
#include "version.h"
#include "FillColorProperty.h"
#include "ExportSuggestions.h"

namespace output
{

Filter::Filter(
    IntrusivePtr<ProjectPages> const& pages,
    PageSelectionAccessor const& page_selection_accessor)
    :   m_ptrPages(pages), m_ptrSettings(new Settings), m_selectedPageOrder(0)
{
    if (CommandLine::get().isGui()) {
        m_ptrOptionsWidget.reset(
            new OptionsWidget(m_ptrSettings, page_selection_accessor)
        );
    }

    typedef PageOrderOption::ProviderPtr ProviderPtr;
    ProviderPtr const default_order;
    ProviderPtr const order_by_mode(new OrderByModeProvider(m_ptrSettings));
    ProviderPtr const order_by_source_color(new OrderBySourceColor(m_ptrSettings, pages));
    m_pageOrderOptions.push_back(PageOrderOption(tr("Natural order"), default_order));
    m_pageOrderOptions.push_back(PageOrderOption(QObject::tr("Processed then unprocessed"), ProviderPtr(new OrderByReadiness())));
    m_pageOrderOptions.push_back(PageOrderOption(tr("Order by mode"), order_by_mode));
    m_pageOrderOptions.push_back(PageOrderOption(tr("Grayscale sources on top"), order_by_source_color,
                                 tr("Groups the pages by presence\nof a non grey color in the source files")));
}

Filter::~Filter()
{
}

QString
Filter::getName() const
{
    return QCoreApplication::translate("output::Filter", "Output");
}

PageView
Filter::getView() const
{
    return PAGE_VIEW;
}

void
Filter::performRelinking(AbstractRelinker const& relinker)
{
    m_ptrSettings->performRelinking(relinker);
}

void
Filter::preUpdateUI(FilterUiInterface* ui, PageId const& page_id)
{
    m_ptrOptionsWidget->preUpdateUI(page_id);
    ui->setOptionsWidget(m_ptrOptionsWidget.get(), ui->KEEP_OWNERSHIP);
}

QDomElement
Filter::saveSettings(
    ProjectWriter const& writer, QDomDocument& doc) const
{

    QDomElement filter_el(doc.createElement("output"));

    writer.enumPages([this, &doc, &filter_el](PageId const& page_id, int numeric_id) {
        writePageSettings(doc, filter_el, page_id, numeric_id);
    });

    return filter_el;
}

void
Filter::writePageSettings(
    QDomDocument& doc, QDomElement& filter_el,
    PageId const& page_id, int numeric_id) const
{
    Params const params(m_ptrSettings->getParams(page_id));

    QDomElement page_el(doc.createElement("page"));
    page_el.setAttribute("id", numeric_id);

    page_el.appendChild(m_ptrSettings->pictureZonesForPage(page_id).toXml(doc, "zones"));
    page_el.appendChild(m_ptrSettings->fillZonesForPage(page_id).toXml(doc, "fill-zones"));
    page_el.appendChild(params.toXml(doc, "params"));

    std::unique_ptr<OutputParams> output_params(m_ptrSettings->getOutputParams(page_id));
    if (output_params.get()) {
        page_el.appendChild(output_params->toXml(doc, "output-params"));
    }

    const ExportSuggestions& export_suggestion = m_ptrSettings->exportSuggestions();
    if (export_suggestion.contains(page_id)) {
        const ExportSuggestion& es = export_suggestion[page_id];
        page_el.appendChild(es.toXml(doc, "exp_sugg"));
    }

    filter_el.appendChild(page_el);
}

void
Filter::loadSettings(ProjectReader const& reader, QDomElement const& filters_el)
{
    m_ptrSettings->clear();

    QDomElement const filter_el(
        filters_el.namedItem("output").toElement()
    );

    QString const page_tag_name("page");
    QDomNode node(filter_el.firstChild());
    for (; !node.isNull(); node = node.nextSibling()) {
        if (!node.isElement()) {
            continue;
        }
        if (node.nodeName() != page_tag_name) {
            continue;
        }
        QDomElement const el(node.toElement());

        bool ok = true;
        int const id = el.attribute("id").toInt(&ok);
        if (!ok) {
            continue;
        }

        PageId const page_id(reader.pageId(id));
        if (page_id.isNull()) {
            continue;
        }

        ZoneSet const picture_zones(el.namedItem("zones").toElement(), m_pictureZonePropFactory);
        if (!picture_zones.empty()) {
            m_ptrSettings->setPictureZones(page_id, picture_zones);
        }

        ZoneSet const fill_zones(el.namedItem("fill-zones").toElement(), m_fillZonePropFactory);
        if (!fill_zones.empty()) {
            m_ptrSettings->setFillZones(page_id, fill_zones);
        }

        QDomElement const params_el(el.namedItem("params").toElement());
        if (!params_el.isNull()) {
            Params const params(params_el);
            m_ptrSettings->setParams(page_id, params);
        }

        QDomElement const output_params_el(el.namedItem("output-params").toElement());
        if (!output_params_el.isNull()) {
            OutputParams const output_params(output_params_el);
            m_ptrSettings->setOutputParams(page_id, output_params);
        }

        QDomElement exp = el.namedItem("exp_sugg").toElement();
        m_ptrSettings->setExportSuggestion(page_id, ExportSuggestion(exp));
    }
}

IntrusivePtr<Task>
Filter::createTask(
    PageId const& page_id,
    IntrusivePtr<ThumbnailPixmapCache> const& thumbnail_cache,
    OutputFileNameGenerator const& out_file_name_gen,
    IntrusivePtr<publish::Task> const& next_task,
    bool const batch, bool const debug,
    bool keep_orig_fore_subscan,
//Original_Foreground_Mixed
    QImage* p_orig_fore_subscan)
{
    ImageViewTab lastTab(TAB_OUTPUT);
    if (m_ptrOptionsWidget.get() != nullptr) {
        lastTab = m_ptrOptionsWidget->lastTab();
    }
    return IntrusivePtr<Task>(
               new Task(
                   IntrusivePtr<Filter>(this), next_task, m_ptrSettings,
                   thumbnail_cache, page_id, out_file_name_gen,
                   lastTab, batch, debug,
                   keep_orig_fore_subscan,
//Original_Foreground_Mixed
                   p_orig_fore_subscan
               )
           );
}

IntrusivePtr<CacheDrivenTask>
Filter::createCacheDrivenTask(OutputFileNameGenerator const& out_file_name_gen,
                              IntrusivePtr<publish::CacheDrivenTask> const& next_task)
{
    return IntrusivePtr<CacheDrivenTask>(
               new CacheDrivenTask(next_task, m_ptrSettings, out_file_name_gen)
           );
}

std::vector<PageOrderOption>
Filter::pageOrderOptions() const
{
    return m_pageOrderOptions;
}

int
Filter::selectedPageOrder() const
{
    return m_selectedPageOrder;
}

void
Filter::selectPageOrder(int option)
{
    assert((unsigned)option < m_pageOrderOptions.size());
    m_selectedPageOrder = option;
}

void
Filter::invalidateSetting(PageId const& page)
{
    Params p = m_ptrSettings->getParams(page);
    p.setForceReprocess(Params::RegenerateAll);
    m_ptrSettings->setParams(page, p);
}

QString zone2Coords(const Zone& zone)
{
    QString res;
    if (zone.type() == Zone::SplineType) {
        for (const QPointF& p : zone.spline().points()) {
            if (!res.isEmpty()) {
                res += "\t";
            }
            res += QString("%1, %2").arg(p.x()).arg(p.y());
        }
    } else if (zone.type() == Zone::EllipseType) {
        const SerializableEllipse& e = zone.ellipse();
        res += QString("rx: %1\try: %2\tangle: %3\tcenter: %4, %5")
                .arg(e.rx()).arg(e.ry()).arg(e.angle())
                .arg(e.center().x()).arg(e.center().y());
    }
    return res;
}

QStringList exportZonesInfo(ZoneSet const& picture_zones, ZoneSet const& fill_zones)
{
    QStringList res;
    if (picture_zones.empty() && fill_zones.empty()) {
        return res;
    }

    res.append(QString("; STU ").append(VERSION));
    res.append("; Format: Type[tab]Info[tab]x1, y1<...>[tab]xN, yN");
    res.append("; Types: 0 - layer zone (has Mode in Info field), 1 - fill zone (has Color in Info field)");
    res.append("; Mode : 1 - subtract from auto layer, 2 - add to auto layer, 3 - subtract from all layers");
    res.append("; Color: has format #RRGGBB/n");

    for (const Zone& z : picture_zones) {
        QString info = "0\t" +
                       QString::number((int) z.properties().locateOrDefault<output::PictureLayerProperty>()->layer()) + "\t" +
                       zone2Coords(z);
        res.append(info);
    }

    for (const Zone& z : fill_zones) {
        QColor const color(z.properties().locateOrDefault<output::FillColorProperty>()->color());
        QString info = "1\t" + color.name().toUpper() + "\t" + zone2Coords(z);
        res.append(info);

    }
    return res;
}

QStringList
Filter::getZonesInfo(const PageId& id) const
{
    ZoneSet const new_picture_zones(m_ptrSettings->pictureZonesForPage(id));
    ZoneSet const new_fill_zones(m_ptrSettings->fillZonesForPage(id));
    return exportZonesInfo(new_picture_zones, new_fill_zones);
}

bool
Filter::checkReadyToPublish(OutputFileNameGenerator const& filename_gen,
                                ProjectPages const& pages, PageId const* ignore) const
{
    if (CommandLine::get().hasDisableCheckOutput())
        return true;
    PageSequence const snapshot(pages.toPageSequence(PAGE_VIEW));
    return m_ptrSettings->checkOutputComplete(filename_gen, snapshot, ignore);
}

const ExportSuggestions*
Filter::exportSuggestions() const
{
 return &m_ptrSettings->exportSuggestions();
}

} // namespace output
