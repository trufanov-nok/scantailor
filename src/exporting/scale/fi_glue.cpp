/*
    Scan Tailor Universal - Interactive post-processing tool for scanned
    pages. A fork of Scan Tailor by Joseph Artsimovich.
    Copyright (C) 2021 Alexander Trufanov <trufanovan@gmail.com>

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

#include "fi_glue.h"
#include "settings/globalstaticsettings.h"

const ImageFilters::Info initImageFiltersInfo() {
    ImageFilters::Info data;
    data[FILTER_BOX]        = FilterInfo("box",        QObject::tr("Box spline"),         QObject::tr("Box, pulse, Fourier window, 1st order (constant) b-spline"));
    data[FILTER_BICUBIC]    = FilterInfo("bicubic",    QObject::tr("Bicubic filter"),     QObject::tr("Mitchell & Netravali's two-param cubic filter"));
    data[FILTER_BILINEAR]   = FilterInfo("bilinear",   QObject::tr("Bilinear filter"),    QObject::tr(""));
    data[FILTER_BSPLINE]    = FilterInfo("bspline",    QObject::tr("Cubic B-spline"),     QObject::tr("4th order (cubic) b-spline"));
    data[FILTER_CATMULLROM] = FilterInfo("catmullrom", QObject::tr("Catmull-Rom spline"), QObject::tr("Catmull-Rom spline, Overhauser spline"));
    data[FILTER_LANCZOS3]   = FilterInfo("lanczos3",   QObject::tr("Lanczos3 filter"),    QObject::tr(""));
    data[FILTER_GAUSSIAN]   = FilterInfo("gaussian",   QObject::tr("Gaussian filter"),    QObject::tr(""));
    data[FILTER_HAMMING]    = FilterInfo("hamming",    QObject::tr("Hamming filter"),     QObject::tr(""));
    return data;
  }

const ImageFilters::Info ImageFilters::info = initImageFiltersInfo();

FilterInfo::FilterInfo(const QString& id, const QString& title, const QString& description):
    id(id),
    title(title),
    description(description)
{
}

FilterInfo::FilterInfo(){}

QString scale_filter2str(FREE_IMAGE_FILTER filter)
{
    if (ImageFilters::info.contains(filter)) {
        return ImageFilters::info[filter].id;
    }
    return QString();
}

FREE_IMAGE_FILTER str2scale_filter(const QString & str)
{
    for (ImageFilters::Info::const_iterator it = ImageFilters::info.cbegin();
         it != ImageFilters::info.cend(); ++it) {
        if (it.value().id == str) {
            return it.key();
        }
    }

    return GlobalStaticSettings::m_default_scale_filter;
}
