/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C) 2007-2008  Joseph Artsimovich <joseph_a@mail.ru>

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

#include "PageId.h"
#include <QString>
#include <assert.h>

const QString PageId::mimeType = "application/stu-page-ids";

PageId::PageId()
    :   m_subPage(SINGLE_PAGE)
{
}

PageId::PageId(ImageId const& image_id, SubPage subpage)
    :   m_imageId(image_id),
        m_subPage(subpage)
{
}

QString
PageId::subPageToString(SubPage const sub_page)
{
    char const* str = 0;

    switch (sub_page) {
    case SINGLE_PAGE:
        str = "single";
        break;
    case LEFT_PAGE:
        str = "left";
        break;
    case RIGHT_PAGE:
        str = "right";
        break;
    }

    assert(str);
    return QLatin1String(str);
}

PageId::SubPage
PageId::subPageFromString(QString const& string, bool* ok)
{
    bool recognized = true;
    SubPage sub_page = SINGLE_PAGE;

    if (string == "single") {
        sub_page = SINGLE_PAGE;
    } else if (string == "left") {
        sub_page = LEFT_PAGE;
    } else if (string == "right") {
        sub_page = RIGHT_PAGE;
    } else {
        recognized = false;
    }

    if (ok) {
        *ok = recognized;
    }
    return sub_page;
}

QByteArray PageId::toByteArray() const
{
    QByteArray res;
    res.resize(sizeof(m_subPage) + sizeof(int));
    memcpy(res.data(), &m_subPage, sizeof(m_subPage));

    QByteArray payload = m_imageId.toByteArray();
    int payload_len = payload.size();
    memcpy(res.data() + sizeof(m_subPage), &payload_len, sizeof(int));
    res.append(payload);
    return res;
}

int PageId::fromByteArray(const QByteArray& data, PageId& pageId)
{
    assert(data.size() > (int) (sizeof(m_subPage) + sizeof(int)));
    SubPage page;

    int bytes_read = 0;
    memcpy(&page, data.data(), sizeof(m_subPage));
    bytes_read += sizeof(m_subPage);

    int payload_len = 0;
    memcpy(&payload_len, data.data() + bytes_read, sizeof(int));
    bytes_read += sizeof(int);

    ImageId image_id;
    bytes_read += ImageId::fromByteArray(data.mid(bytes_read, payload_len), image_id);
    pageId = PageId(image_id, page);
    return bytes_read;
}

bool operator==(PageId const& lhs, PageId const& rhs)
{
    return lhs.subPage() == rhs.subPage() && lhs.imageId() == rhs.imageId();
}

bool operator!=(PageId const& lhs, PageId const& rhs)
{
    return !(lhs == rhs);
}

bool operator<(PageId const& lhs, PageId const& rhs)
{
    if (lhs.imageId() < rhs.imageId()) {
        return true;
    } else if (rhs.imageId() < lhs.imageId()) {
        return false;
    } else {
        return lhs.subPage() < rhs.subPage();
    }
}

uint qHash(const PageId& tag, uint seed)
{
    return qHash(tag.imageId().filePath(), seed) ^
           qHash(tag.imageId().page(), seed) ^
           qHash(tag.subPage(), seed ^ 0xA11A);
}

