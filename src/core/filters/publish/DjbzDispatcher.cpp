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

#include "DjbzDispatcher.h"
#include "settings/globalstaticsettings.h"
#include "PageSequence.h"
#include "Settings.h"
#include <QDateTime>
#include <QDomDocument>
#include <QDebug>

namespace publish {

DjbzParams::DjbzParams():
    m_usePrototypes(GlobalStaticSettings::m_djvu_djbz_use_prototypes),
    m_useAveraging(GlobalStaticSettings::m_djvu_djbz_use_averaging),
    m_aggression(GlobalStaticSettings::m_djvu_djbz_aggression),
    m_useErosion(GlobalStaticSettings::m_djvu_djbz_erosion),
    m_classifier((ClassifierType) GlobalStaticSettings::m_djvu_djbz_classifier),
    m_extension(GlobalStaticSettings::m_djvu_djbz_extension)
{
}

DjbzParams::ClassifierType
ClassifierTypeFromString(QString const& str)
{
    if (str == "legacy") return DjbzParams::ClassifierType::Legacy;
    if (str == "normal") return DjbzParams::ClassifierType::Normal;
    return DjbzParams::ClassifierType::Maximal;
}

QString
ClassifierTypeToString(DjbzParams::ClassifierType val)
{
    switch (val) {
    case DjbzParams::ClassifierType::Legacy: return "legacy";
    case DjbzParams::ClassifierType::Normal: return "normal";
    case DjbzParams::ClassifierType::Maximal: return "maximal";
    default: return "maximal";
    }
}

DjbzParams::DjbzParams(QDomElement const& el)
{
    m_usePrototypes = el.attribute("prototypes", QString::number(GlobalStaticSettings::m_djvu_djbz_use_prototypes)).toInt();
    m_useAveraging = el.attribute("averaging", QString::number(GlobalStaticSettings::m_djvu_djbz_use_averaging)).toInt();
    m_useErosion = el.attribute("erosion", QString::number(GlobalStaticSettings::m_djvu_djbz_erosion)).toInt();
    m_aggression = el.attribute("aggression", QString::number(GlobalStaticSettings::m_djvu_djbz_aggression)).toInt();
    m_classifier = ClassifierTypeFromString(el.attribute("type", QString::number(GlobalStaticSettings::m_djvu_djbz_classifier)).toLocal8Bit());
    m_extension = el.attribute("ext", GlobalStaticSettings::m_djvu_djbz_extension);
}

QDomElement
DjbzParams::toXml(QDomDocument& doc, QString const& name) const
{
    QDomElement el(doc.createElement(name));
    el.setAttribute("prototypes", m_usePrototypes);
    el.setAttribute("averaging", m_useAveraging);
    el.setAttribute("erosion", m_useErosion);
    el.setAttribute("aggression", m_aggression);
    el.setAttribute("type", ClassifierTypeToString(m_classifier));
    el.setAttribute("ext", m_extension);
    return el;
}

bool
DjbzParams::operator !=(const DjbzParams &b) const
{
    return (m_useErosion != b.m_useErosion ||
            m_useAveraging != b.m_useAveraging ||
            m_usePrototypes != b.m_usePrototypes ||
            m_aggression != b.m_aggression ||
            m_classifier != b.m_classifier ||
            m_extension != b.m_extension);
}

////////////////////////////////////////////
///            DjbzDict
////////////////////////////////////////////

DjbzDict::DjbzDict():
    m_type(AutoFill),
    m_max_pages(GlobalStaticSettings::m_djvu_pages_per_djbz),
    m_last_changed(QDateTime::currentDateTimeUtc()),
    m_output_file_size(0),
    m_output_file_last_changed(m_last_changed)
{
}

void
DjbzDict::addPage(const PageId& page, bool no_rev_change)
{
    m_pages += page;
    if (m_pages.count() > m_max_pages) {
        m_max_pages = m_pages.count();
    }

    if (!no_rev_change) {
        m_last_changed = QDateTime::currentDateTimeUtc();
    }
}

void
DjbzDict::removePage(const PageId& page, bool no_rev_change)
{
    if (m_pages.remove(page)) {
        if (!no_rev_change) {
            m_last_changed = QDateTime::currentDateTimeUtc();
        }
    }
}
const QSet<PageId>&
DjbzDict::pages() const
{
    return m_pages;
};

void
DjbzDict::setParams(const DjbzParams& params, bool no_rev_change)
{
    if (m_params != params) {
        m_params = params;
        if (!no_rev_change) {
            m_last_changed = QDateTime::currentDateTimeUtc();
        }
    }
}
const DjbzParams&
DjbzDict::params() const
{
    return m_params;
}

DjbzParams&
DjbzDict::paramsRef()
{
    return m_params;
}

QDateTime DjbzDict::revision() const
{
    return m_last_changed;
}

void
DjbzDict::setRevision(QDateTime val)
{
    m_last_changed = val;
}

DjbzDict::Type
DjbzDict::type() const
{
    return m_type;
};

void
DjbzDict::setType(Type type)
{
    m_type = type;
};

int
DjbzDict::maxPages() const
{
    return m_max_pages;
};

void
DjbzDict::setMaxPages(int max)
{
    m_max_pages = max;
};

void
DjbzDict::updateOutputFileInfo()
{
    if (!m_output_file.isEmpty()) {
        QFileInfo fi;
        fi.setFile(m_output_file);
        m_output_file_size = fi.exists() ? fi.size() : 0;
        m_output_file_last_changed = fi.lastModified();
    }
}

////////////////////////////////////////////
///            DjbzDispatcher
////////////////////////////////////////////


const QString DjbzDispatcher::dummyDjbzId = "[none]";

DjbzDispatcher::DjbzDispatcher():
    m_id_counter(0)
{
    m_dictionaries[DjbzDispatcher::dummyDjbzId].setType(DjbzDict::Type::None);
}

QString
DjbzDispatcher::findDjbzForPage(PageId const& page_id) const
{
    if (m_page_to_dict.contains(page_id)) {
        return m_page_to_dict[page_id];
    }
    return QString();
}

DjbzDict
DjbzDispatcher::djbzDict(QString const& dict_id) const
{
    if (m_dictionaries.contains(dict_id)) {
        return m_dictionaries[dict_id];
    }
    return DjbzDict();
}

DjbzDict&
DjbzDispatcher::djbzDictRef(QString const& dict_id)
{
    assert (m_dictionaries.contains(dict_id));
    return m_dictionaries[dict_id];
}

void
DjbzDispatcher::SetDjbzDict(QString const& dict_id, DjbzDict const& dict)
{
    m_dictionaries[dict_id] = dict;
}

QStringList
DjbzDispatcher::listAllDjbz() const
{
    QStringList res = m_dictionaries.keys();
    // let dummy djbz be first in the list
    int idx = res.indexOf(dummyDjbzId);
    if (idx != -1) {
        res.move(idx, 0);
    }
    return res;
}

QMap<QString, int>
DjbzDispatcher::listAllDjbzAndTheirSize() const
{
    QMap<QString, int> res;
    const QStringList sl = listAllDjbz();
    for(const QString& s: sl) {
        res[s] = m_dictionaries[s].pages().count();
    }
    return res;
}


QString
DjbzDispatcher::nextDjbzId()
{
    QString djbz;
    const QChar c('0');
    do {
        djbz = QString("%1").arg(++m_id_counter, 4, 10, c);
    } while (m_dictionaries.contains(djbz));

    return djbz;
}

QString
DjbzDispatcher::addNewPage(PageId const& page_id)
{
    if (GlobalStaticSettings::m_djvu_pages_per_djbz < 2) {
        m_dictionaries[DjbzDispatcher::dummyDjbzId].addPage(page_id);
        m_page_to_dict[page_id] = DjbzDispatcher::dummyDjbzId;
        return DjbzDispatcher::dummyDjbzId;
    }

    QString djbz = findDjbzForPage(page_id);
    if (djbz.isEmpty()) {
        // Find some dict that still isn't full enough
        for (DjbzContent::const_iterator it = m_dictionaries.cbegin(); it != m_dictionaries.cend(); ++it) {
            if (it->type() == DjbzDict::Type::AutoFill
                    && it->pages().size() < it->maxPages()) {
                djbz = it.key();
                break;
            }
        }

        if (djbz.isEmpty()) {
            // page goes to a new djbz
            djbz = nextDjbzId();
        }

        m_dictionaries[djbz].addPage(page_id);
        m_page_to_dict[page_id] = djbz;
    }
    return djbz;
}

void
DjbzDispatcher::deleteFromDjbz(PageId const& page_id)
{
    if (m_page_to_dict.contains(page_id)) {
        m_dictionaries[m_page_to_dict[page_id]].removePage(page_id);
        m_page_to_dict.remove(page_id);
    }
}

void
DjbzDispatcher::setToDjbz(PageId const& page_id, QString const& new_djbz, bool no_rev_change)
{
    if (m_page_to_dict[page_id] != new_djbz) {
        m_dictionaries[new_djbz].addPage(page_id, no_rev_change);
        m_page_to_dict[page_id] = new_djbz;
    }
}

void
DjbzDispatcher::moveToDjbz(PageId const& page_id, QString const& new_djbz)
{
    deleteFromDjbz(page_id);
    setToDjbz(page_id, new_djbz);
}

void
DjbzDispatcher::resetAllDictsExceptLocked()
{

    DjbzContent locked_dicts;
    PageToDjbz locked_pages;

    for (DjbzContent::const_iterator it = m_dictionaries.cbegin(); it != m_dictionaries.cend(); ++it) {
        if (it->type() == DjbzDict::Type::Locked) {
            locked_dicts[it.key()] = *it;
            for (const PageId& p: it->pages()) {
                locked_pages[p] = it.key();
            }
        }
    }

    m_dictionaries.clear();
    m_id_counter = 0;
    m_page_to_dict.clear();

    m_dictionaries = locked_dicts;
    m_dictionaries[DjbzDispatcher::dummyDjbzId].setType(DjbzDict::Type::None);
    m_page_to_dict = locked_pages;
}

void
DjbzDispatcher::autosetPagesToDjbz(const PageSequence& pages, const ExportSuggestions* export_suggestions, IntrusivePtr<Settings> settings)
{
    for (const PageInfo& p: pages) {
        PageId const& page_id = p.id();
        std::unique_ptr<Params> params_ptr(settings->getPageParams(page_id));
        const ExportSuggestion es = export_suggestions->value(page_id);

        bool need_new_djbz = (!params_ptr || params_ptr->djbzId().isEmpty());
        if (!need_new_djbz) {
            if ( (es.hasBWLayer && (params_ptr->djbzId() == dummyDjbzId)) ||
                 (!es.hasBWLayer && (params_ptr->djbzId() != dummyDjbzId)) ) {
                deleteFromDjbz(page_id);
                need_new_djbz = true;
            }
        }

        if (need_new_djbz) {
            const QString new_djbz_id = es.hasBWLayer ? addNewPage(page_id) : dummyDjbzId;
            setToDjbz(page_id, new_djbz_id);

            Params params = params_ptr ? Params(*params_ptr.get()) : Params();
            params.setDjbzId(new_djbz_id);
            settings->setPageParams(page_id, params);
        }

    }
}


QString type2str(DjbzDict::Type t) {
    if (t == DjbzDict::Type::Locked) return "locked";
    else if (t == DjbzDict::Type::None) return "no_dict";

    return "auto";
}

DjbzDict::Type str2type( QString const& s) {
    if (s == "locked") return DjbzDict::Type::Locked;
    else if (s == "no_dict") return DjbzDict::Type::None;

    return DjbzDict::Type::AutoFill;
}

QDomElement
DjbzDispatcher::toXml(QDomDocument& doc, QString const& name) const
{
    QDomElement root_el(doc.createElement(name));
    const QStringList keys = m_dictionaries.keys();
    for (QString const& id: keys) {
        QDomElement el(doc.createElement("djbz"));
        el.setAttribute("id", id);
        DjbzDict const& dict = m_dictionaries[id];
        el.setAttribute("type", type2str(dict.type()));
        el.setAttribute("max", dict.maxPages());
        el.setAttribute("last_changed", dict.revision().toString("dd.MM.yyyy hh:mm:ss.zzz"));
        el.setAttribute("output_file", dict.outputFilename());
        el.setAttribute("output_file_size", dict.outputFileSize());
        el.setAttribute("output_file_last_changed", dict.outputLastChanged().toString("dd.MM.yyyy hh:mm:ss.zzz"));
        el.appendChild(dict.params().toXml(doc, "djbz_params"));
        root_el.appendChild(el);
    }

    return root_el;
}

DjbzDispatcher::DjbzDispatcher(QDomElement const& el):
    m_id_counter(0)
{
    qDebug() << el.toElement().isNull();
    QDomNode node(el.firstChild());
    for (; !node.isNull(); node = node.nextSibling()) {
        if (!node.isElement()) {
            continue;
        }
        if (node.nodeName() != "djbz") {
            continue;
        }
        QDomElement const el(node.toElement());

        DjbzDict & dict = m_dictionaries[el.attribute("id", dummyDjbzId)];
        dict.setType(str2type(el.attribute("type", "auto")));
        dict.setMaxPages(el.attribute("max", QString::number(GlobalStaticSettings::m_djvu_pages_per_djbz)).toInt());
        DjbzParams params(node.namedItem("djbz_params").toElement());
        dict.setParams(params);
        dict.setRevision(QDateTime::fromString(el.attribute("last_changed", QDateTime::currentDateTimeUtc().toString("dd.MM.yyyy hh:mm:ss.zzz")), "dd.MM.yyyy hh:mm:ss.zzz"));
        dict.setOutputFilename(el.attribute("output_file"));
        dict.setOutputFileSize(el.attribute("output_file_size", 0).toUInt());
        el.attribute("output_file_last_changed", dict.outputLastChanged().toString("dd.MM.yyyy hh:mm:ss.zzz"));
        dict.setOutputLastChanged(QDateTime::fromString(el.attribute("output_file_last_changed", QDateTime::currentDateTimeUtc().toString("dd.MM.yyyy hh:mm:ss.zzz")), "dd.MM.yyyy hh:mm:ss.zzz"));
    }

    // should be ready, but just to make sure that project isn't damaged or created with old ST version
    m_dictionaries[DjbzDispatcher::dummyDjbzId].setType(DjbzDict::Type::None);
}

bool
DjbzDispatcher::isDjbzEncodingRequired(const PageId& page) const
{
    const QString& dict_id = m_page_to_dict[page];
    assert(!dict_id.isEmpty());
    return dict_id != DjbzDispatcher::dummyDjbzId;
}

bool
DjbzDispatcher::isDummyDjbzId(const QString id) const
{
    return id == DjbzDispatcher::dummyDjbzId;
}

QSet<PageId>
DjbzDispatcher::listPagesFromDict(const QString& djbz_id) const
{
    return m_dictionaries[djbz_id].pages();
}

QSet<PageId>
DjbzDispatcher::listPagesFromSameDict(const PageId& page) const
{
    const QString& dict_id = m_page_to_dict[page];
    assert(!dict_id.isEmpty());

    if (dict_id == DjbzDispatcher::dummyDjbzId) {
        return QSet<PageId> ({page});
    } else {
        return listPagesFromDict(dict_id);
    }
}

const QString
getFileToEncode(SourceImagesInfo const & si)
{
    return !si.export_foregroundFilename().isEmpty() ? // check if has been exported by layers
                                                       si.export_foregroundFilename() :
                                                       si.output_filename();
}

void
DjbzDispatcher::generateDjbzEncoderParams(const PageId& page, Settings const& page_settings, QStringList& encoder_settings, QString& output_file) const
{
    const QString& dict_id = m_page_to_dict[page];
    assert(!dict_id.isEmpty());

    const DjbzDict & dict = m_dictionaries[dict_id];
    output_file = dict.pageCount() > 1 ? "_djbz_" + dict_id + ".djvu" :
                                         // single page djbz is ignored and page is encoded without shared dictionary
                                         page_settings.getPageParams(page)->djvuFilename();

    const DjbzParams & dict_params = dict.params();
    if (dict_id != DjbzDispatcher::dummyDjbzId) {
        encoder_settings << "(djbz "
                         << "  id            " + dict_id
                         << "  xtension      " + dict_params.extension()
                         << "  averaging     " + QString(dict_params.useAveraging() ? "1" :"0")
                         << "  aggression    " + QString::number(dict_params.agression())
                         << "  classifier    " + QString::number(dict_params.classifierType())
                         << "  no-prototypes " + QString(dict_params.usePrototypes() ? "0" :"1")
                         << "  erosion       " + QString(dict_params.useErosion() ? "1" :"0");
        encoder_settings << "      (files";
        for (const PageId& p : dict.pages()) {
            const SourceImagesInfo si = page_settings.getPageParams(p)->sourceImagesInfo();
            if (si.export_suggestion().hasBWLayer) {
                encoder_settings << "            " + getFileToEncode(si);
            }
        }
        encoder_settings << "      ) #files"
                         << ") #djbz";
    }/* else {
        // single page djbz are ignored and pages are encoded without shared dictionary
        const std::unique_ptr<Params> params = ;
        encoder_settings << "(djbz"
                         << getFileToEncode(params->sourceImagesInfo())
                         << ") #djbz";
    }*/
}

bool DjbzDispatcher::isDjbzCached(const QString& dict_id) const
{
    if (dict_id.isEmpty()) {
        return false;
    }

    if (dummyDjbzId == dict_id) {
        return true;
    }

    const DjbzDict dict = djbzDict(dict_id);
    if (!dict.outputFilename().isEmpty()) {
        QFileInfo fi(dict.outputFilename());
        return (fi.exists() &&
                fi.size() == dict.outputFileSize() &&
                fi.lastModified() == dict.outputLastChanged());
    }
    return false;
}

} // namespace
