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

#include "OrderByFileSize.h"
#include "Params.h"
#include "StatusBarProvider.h"

namespace publish
{

OrderByFileSize::OrderByFileSize(IntrusivePtr<Settings> const& settings)
:	m_ptrSettings(settings)
{
}

bool
OrderByFileSize::precedes(
    PageId const& lhs_page, bool const lhs_incomplete,
    PageId const& rhs_page, bool const rhs_incomplete) const
{

    if (lhs_incomplete != rhs_incomplete) {
        return lhs_incomplete;
    }

    std::unique_ptr<Params> const lparam(m_ptrSettings->getPageParams(lhs_page));
    std::unique_ptr<Params> const rparam(m_ptrSettings->getPageParams(rhs_page));

    if (lparam->djvuSize() != rparam->djvuSize()) {
        return lparam->djvuSize() < rparam->djvuSize();
    }

    return lhs_page < rhs_page;

}


QString
OrderByFileSize::hint(PageId const& page) const
{
    int fsize = m_ptrSettings->getPageParams(page)->djvuSize();
    return QObject::tr("File size: %1").arg(StatusBarProvider::getStatusLabelFileSizeText(&fsize));
}

} // namespace publish
