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

#ifndef SELECT_CONTENT_SETTINGS_H_
#define SELECT_CONTENT_SETTINGS_H_

#include "RefCountable.h"
#include "NonCopyable.h"
#include "PageId.h"
#include "Params.h"
#include <QMutex>
#include <memory>
#include <map>

class AbstractRelinker;

namespace select_content
{

class Settings : public RefCountable
{
    DECLARE_NON_COPYABLE(Settings)
public:
    Settings();

    virtual ~Settings();

    void clear();

    void performRelinking(AbstractRelinker const& relinker);

    void updateDeviation();

    void setPageParams(PageId const& page_id, Params const& params);

    void clearPageParams(PageId const& page_id);

    std::unique_ptr<Params> getPageParams(PageId const& page_id) const;

    double maxDeviation() const
    {
        return m_maxDeviation;
    }
    void setMaxDeviation(double md)
    {
        m_maxDeviation = md;
    }

    QSizeF pageDetectionBox() const
    {
        return m_pageDetectionBox;
    }
    void setPageDetectionBox(QSizeF size)
    {
        m_pageDetectionBox = size;
    }

    double pageDetectionTolerance() const
    {
        return m_pageDetectionTolerance;
    }
    void setPageDetectionTolerance(double tolerance)
    {
        m_pageDetectionTolerance = tolerance;
    }

    double avg() const
    {
        return m_avg;
    }
    void setAvg(double a)
    {
        m_avg = a;
    }

    double std() const
    {
        return m_sigma;
    }
    void setStd(double s)
    {
        m_sigma = s;
    }
private:
    typedef std::map<PageId, Params> PageParams;

    mutable QMutex m_mutex;
    PageParams m_pageParams;
    double m_avg;
    double m_sigma;
    double m_maxDeviation;
    QSizeF m_pageDetectionBox;
    double m_pageDetectionTolerance;
};

} // namespace select_content

#endif
