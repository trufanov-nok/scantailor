/*
    Scan Tailor Universal - Interactive post-processing tool for scanned
    pages. A fork of Scan Tailor by Joseph Artsimovich.
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

#include "EditableEllipse.h"
#include "SerializableEllipse.h"
#include <QString>
#include <assert.h>
#include <cmath>


void
EditableEllipse::changePoint(int idx, QPointF const & new_point, bool make_circle) {
    const double d = SerializableEllipse::distance(m_center, new_point);
    if (make_circle) {
        m_rx = m_ry = d;
    } else
    if (idx == 0 || idx == 2) {
        m_rx = d;
    } else {
        m_ry = d;
    }

    const QPointF p = new_point - m_center;
    m_angle = p.x() ? atan(p.y()/p.x()) : 0.;

    while (idx-- > 0) {
        m_angle -= M_PI_2;
    }

    update_points();
}

EditableEllipse::EditableEllipse(SerializableEllipse const& ellipse)
{
    copyFromSerializableEllipse(ellipse);
}

void
EditableEllipse::copyFromSerializableEllipse(SerializableEllipse const& ellipse)
{
    m_center = ellipse.center();
    m_rx = ellipse.rx();
    m_ry = ellipse.ry();
    m_angle = ellipse.angleRad();
    m_points.resize(4);
    update_points();
}

bool
EditableEllipse::contains(QPointF const & point, bool *exactly_on_border) const
{
    const qreal s = sin(-m_angle);
    const qreal c = cos(-m_angle);
    const qreal rx2 = m_rx*m_rx;
    const qreal ry2 = m_ry*m_ry;
    const qreal rx2ry2 = rx2*ry2;

    const qreal centered_p_x = (point.x() - m_center.x());
    const qreal centered_p_y = (point.y() - m_center.y());
    const qreal new_p_x = centered_p_x*c - centered_p_y*s;
    const qreal new_p_y = centered_p_x*s + centered_p_y*c;

    const qreal res = (new_p_x*new_p_x*ry2 +
                        new_p_y*new_p_y*rx2);
    if (exactly_on_border) {
        *exactly_on_border = (res == rx2ry2);
    }
    return res <= rx2ry2;
}

QTransform get_transform(double dx, double dy, double angle)
{
    return QTransform().translate(dx, dy).rotate(angle).translate(-dx, -dy);
}

QTransform
EditableEllipse::transform() const
{
    return get_transform(center().x(), center().y(), angle());
}

QTransform
EditableEllipse::transform(QTransform const& trans) const
{
    return trans * get_transform(center(trans).x(), center(trans).y(), angle());
}

double
EditableEllipse::area() const
{
    return M_PI * m_rx * m_ry / 4;
}
