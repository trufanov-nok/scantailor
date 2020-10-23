#include "ExportSuggestions.h"
#include <QImage>
#include <QDomDocument>
#include <QDateTime>

QDateTime ExportSuggestion::startTimerDate = [] () {
    QDate cd = QDate::currentDate();
    // we'll calc msecs from time point 3 months ago in hope
    // that this will behave properly on 32-bit machine.
    return QDateTime(QDate(cd.year(),std::min(1,cd.month()-3),1));
}();

ExportSuggestion::ExportSuggestion()
{
    hasBWLayer = false;
    hasColorLayer = false;
    isValid = false;
    updateLastChanged();
}

ExportSuggestion::ExportSuggestion(const QImage& image)
{
    bool bw_found = false;
    bool cl_found = false;
    bool has_bw = true;
    bool has_cl = true;

    const QImage::Format format = image.format();

    width  = image.width();
    height = image.height();
    dpi    = image.dotsPerMeterX() / 39.37;

    if (format == QImage::Format_Mono ||
            format == QImage::Format_MonoLSB) {
        // pixel color is a bit. 8 pixels per byte.
        // scanline data is aligned on a 32-bit boundary

        const QVector<QRgb> color_table = image.colorTable();
        assert(color_table.size() == 2);
        const int w_idx = color_table.indexOf(qRgb(0xFF,0xFF,0xFF));
        assert(w_idx != -1);
        const uchar white_byte = (w_idx == 0) ? 0x00 : 0xFF;

        has_cl = false;
        cl_found = true;

        for (int y = 0; y < image.height() ; y++ ) {
            const uchar* data = image.constScanLine(y);
            const uchar* d = data;
            const int line_size = image.bytesPerLine();
            const uchar mask = 0xFF << (8 - (image.width() % 8));
            while (!bw_found && d < data+line_size) {
                if (*d != white_byte && // not a last byte in line
                        (d != data+line_size-1 || *d != (white_byte & mask) ) ) {
                    has_bw = true;
                    bw_found = true;
                    break;
                }
                d++;
            }
        }
    } else
        if (format == QImage::Format_Indexed8) {
            // pixel color is a 0-255 index. Index is a byte.
            // scanline data is aligned on a 32-bit boundary

            const QVector<QRgb> color_table = image.colorTable();
            const int white = color_table.indexOf(qRgb(0xFF,0xFF,0xFF));
            const int black = color_table.indexOf(qRgb(0,0,0));
            if (black == -1) {
                has_bw = false;
                bw_found = true;
            } else if (white != -1 && color_table.size() == 2) {
                // color_table contains only black & white colors
                has_cl = false;
                cl_found = true;
            }

            for (int y = 0; y < image.height() ; y++ ) {
                const uchar* data = image.constScanLine(y);
                const uchar* d = data;
                const int line_size = image.bytesPerLine();
                while ((!bw_found || !cl_found) && d < data+line_size) {
                    const uchar & c = *d;
                    if (!bw_found && c == black) {
                        has_bw = true;
                        bw_found = true;
                    } else if (!cl_found && c != white && c != black) {
                        has_cl = true;
                        cl_found = true;
                    }
                    d++;
                }

            }

        } else if (format == QImage::Format_RGB32 ||
                   format == QImage::Format_ARGB32) {
            // pixel color is a 32 bit integer: 0xffRRGGBB or 0xAARRGGBB
            assert (image.sizeInBytes() % 4 == 0);
            const int size_in_int32 = image.sizeInBytes() / 4;
            const uint32_t* data = (uint32_t*) image.constBits();
            const uint32_t* d = data;
            const uint32_t white = 0x00FFFFFF;
            const uint32_t black = 0x00000000;
            while ((!bw_found || !cl_found) && d < data + size_in_int32) {
                const uint32_t c = *d & 0x00FFFFFF;
                if (!bw_found && c == black) {
                    has_bw = true;
                    bw_found = true;
                } else if (!cl_found && c != black && c != white) {
                    has_cl = true;
                    cl_found = true;
                }
                d++;
            }
        } else {
            throw "Export suggestions faced with unexpectable image format!";
        }

    hasBWLayer = bw_found && has_bw;
    hasColorLayer = cl_found && has_cl;
    isValid = true;
    updateLastChanged();
}

void
ExportSuggestion::updateLastChanged()
{
    lastChanged = QDateTime::currentDateTime().msecsTo(startTimerDate);
}

bool
ExportSuggestion::operator !=(const ExportSuggestion &other) const
{
    return (lastChanged   != other.lastChanged ||
            hasBWLayer    != other.hasBWLayer ||
            hasColorLayer != other.hasColorLayer ||
            width         != other.width ||
            height        != other.height ||
            dpi           != other.dpi ||
            isValid       != other.isValid);
}

ExportSuggestion::ExportSuggestion(QDomElement const& el)
{
    isValid = el.hasAttribute("valid") &&
            el.hasAttribute("hasBW") &&
            el.hasAttribute("hasCl");
    if (isValid) {
        isValid       = el.attribute("valid").toUInt();
        hasBWLayer    = el.attribute("hasBW", "1").toUInt();
        hasColorLayer = el.attribute("hasCl", "1").toUInt();
        width         = el.attribute("width", "0").toUInt();
        height        = el.attribute("height", "0").toUInt();
        dpi           = el.attribute("dpi", "0").toUInt();
        if (!width || !height || !dpi) {
            isValid = false;
        }
    }
    if (el.hasAttribute("changed")) {
        lastChanged = el.attribute("changed").toInt();
    } else {
        updateLastChanged();
    }
}

QDomElement
ExportSuggestion::toXml(QDomDocument& doc, QString const& name) const
{
    QDomElement el(doc.createElement(name));
    el.setAttribute("hasBW",   hasBWLayer);
    el.setAttribute("hasCl",   hasColorLayer);
    el.setAttribute("valid",   isValid);
    el.setAttribute("changed", lastChanged);
    el.setAttribute("width",   width);
    el.setAttribute("height",  height);
    el.setAttribute("dpi",     dpi);
    return el;
}
