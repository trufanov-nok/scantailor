/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C) 2007-2009  Joseph Artsimovich <joseph_a@mail.ru>

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

#include "Zone.h"
#include <QDomDocument>
#include <QDomElement>
#include <QString>

Zone::Zone(SerializableSpline const& spline, PropertySet const& props)
    :
      m_type(SplineType),
      m_spline(spline),
      m_props(props)
{
}

Zone::Zone(SerializableEllipse const& ellipse, PropertySet const& props)
    :
      m_type(EllipseType),
      m_ellipse(ellipse),
      m_props(props)
{
}

Zone::Zone(QDomElement const& el, PropertyFactory const& prop_factory) :
    m_props(el.namedItem("properties").toElement(), prop_factory)
{
    m_type = el.namedItem("spline").isNull() ? EllipseType : SplineType;
    if (m_type == SplineType) {
        m_spline = SerializableSpline(el.namedItem("spline").toElement());
    } else {
        m_ellipse = SerializableEllipse(el.namedItem("ellipse").toElement());
    }
}

//begin of modified by monday2000
//Quadro_Zoner
Zone::Zone(QPolygonF const& polygon)
    : m_type(SplineType), m_spline(polygon)
{
    m_props.locateOrCreate<output::PictureLayerProperty>()->
    setLayer(output::PictureLayerProperty::PAINTER2);

    m_props.locateOrCreate<output::ZoneCategoryProperty>()->
    setZoneCategory(output::ZoneCategoryProperty::RECTANGULAR_OUTLINE);
}
//end of modified by monday2000

QDomElement
Zone::toXml(QDomDocument& doc, QString const& name) const
{
    QDomElement el(doc.createElement(name));
    if (m_type == SplineType) {
        el.appendChild(m_spline.toXml(doc, "spline"));
    } else {
        el.appendChild(m_ellipse.toXml(doc, "ellipse"));
    }
    el.appendChild(m_props.toXml(doc, "properties"));
    return el;
}

bool
Zone::isValid() const
{
    if (m_type == SplineType) {
        if (!m_spline.isValid()) {
            return false;
        }

        QPolygonF const& shape = m_spline.toPolygon();

        switch (shape.size()) {
        case 0:
        case 1:
        case 2:
            return false;
        case 3:
            if (shape.front() == shape.back()) {
                return false;
            }
            // fall through
        default:
            return true;
        }
    } else { // ellipse
        return m_ellipse.isValid();
    }
}
