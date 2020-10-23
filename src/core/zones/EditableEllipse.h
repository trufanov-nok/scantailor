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

#ifndef EDITABLEELIPSE_H_
#define EDITABLEELIPSE_H_

#include "RefCountable.h"
#include "IntrusivePtr.h"
#include "SerializableEllipse.h"
#include <QPointF>
#include <QVector>
#include <QTransform>
#include <cmath>
#ifndef Q_MOC_RUN
#include <boost/function.hpp>
#endif

class EditableEllipse : public RefCountable
{
public:
    typedef IntrusivePtr<EditableEllipse> Ptr;

    EditableEllipse() { m_points.resize(4); }
    EditableEllipse(SerializableEllipse const& ellipse);
    void copyFromSerializableEllipse(SerializableEllipse const& ellipse);

    bool isValid() const { return !center().isNull(); }

    double angle() const { return 180./M_PI * m_angle; }

    double angleRad() const { return m_angle; }

    void setAngle(double angle) { m_angle = M_PI/180. * angle; update_points(); }

    double rx() const { return m_rx; } //distance(co_vertex1(), center())
    double ry() const { return m_ry; }
    double rx(QTransform const& trans) const { return SerializableEllipse::distance(trans.map(m_points[0]), trans.map(center())); }
    double ry(QTransform const& trans) const { return SerializableEllipse::distance(trans.map(m_points[1]), trans.map(center())); }

    void setRx(double rx) { m_rx = rx; update_points(); }
    void setRy(double ry) { m_ry = ry; update_points(); }

    const QPointF & center() const { return m_center; }
    QPointF center(QTransform const& trans) const { return trans.map(m_center); }

    void setCenter(const QPointF & new_point) {
        QPointF diff = new_point - m_center;
        m_center = new_point;
        for (QPointF& p: m_points) {
            p += diff;
        }
    }

    const QVector<QPointF> & const_data() const { return m_points; }
    QVector<QPointF> & data() { return m_points; }

    void changePoint(int idx, QPointF const & new_point, bool make_circle = false);

    bool contains(QPointF const & point, bool *exactly_on_border = nullptr) const;

    QTransform transform() const;
    QTransform transform(const QTransform &trans) const;

    double area() const;

private:
    void update_points() {
        const double c = cos(m_angle);
        const double s = sin(m_angle);
        const QPointF rx(m_rx*c, m_rx*s);
        const QPointF ry(m_ry*s, -m_ry*c);
        m_points[0] = m_center + rx;
        m_points[1] = m_center + ry;
        m_points[2] = m_center - rx;
        m_points[3] = m_center - ry;
    }

private:
    QPointF m_center;
    double m_rx;
    double m_ry;
    double m_angle;
    QVector<QPointF> m_points;
};

#endif // EDITABLEELIPSE_H_
