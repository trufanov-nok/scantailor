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
#include "DjVuReader.h"
#include <QIODevice>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QMutexLocker>
#include <QFile>
#include <QDebug>
#include "ImageId.h"

QImage
DjVuReader::load(ImageId const& image_id)
{
    return load(image_id.filePath(), image_id.zeroBasedPage());
}

QImage
DjVuReader::load(QString const& file_path, int const page_num)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QImage();
    }

    if (canRead(file)) {
        return readImage(file_path, page_num);
    }

    if (page_num != 0) {
        // Qt can only load the first page of multi-page images.
        return QImage();
    }

    return readImage(file_path, page_num);
}

bool
DjVuReader::canRead(QIODevice& device)
{
    if (!device.isReadable()) {
        return false;
    }
    if (device.isSequential()) {
        // libtiff needs to be able to seek.
        return false;
    }

    unsigned char data[4];
    if (device.peek((char*)data, sizeof(data)) != sizeof(data)) {
        if (memcmp(data, "AT&T", 4)) {
            //        if (data[0] == 'A' && data[1] == 'T' && data[2] == '&' && data[3] == 'T') {
            return true;
        }
    }
    return false;
}

static QMutex _mutex;
static ddjvu_context_t *_context = nullptr;


void handle_ddjvu_messages(ddjvu_context_t *ctx, int wait)
{
    qDebug() << "handle_ddjvu_messages";
    if (!ctx) {
        return;
    }

    const ddjvu_message_t *msg;

    if (wait) {
        qDebug() << "wait 2";
        msg = ddjvu_message_wait(ctx);
        qDebug() << "wait 1";
    }


    while ((msg = ddjvu_message_peek(ctx))) {
        if (msg->m_any.tag == DDJVU_ERROR) {
            qDebug() << msg->m_error.message;
            if (msg->m_error.filename) {
                qDebug() << msg->m_error.filename << ":" << msg->m_error.lineno;
            }
        }
//        qDebug() << msg->m_error.;
        ddjvu_message_pop(ctx);
        qDebug() << "peek";
    }
}

QImage
DjVuReader::readImage(const QString& filename, int const page_num)
{
    QImage res;
    if (page_num < 0) {
        return res;
    }
qDebug() << "DjVuReader::readImage";
    QMutexLocker locker(&_mutex);
    if (!_context) {
        _context = ddjvu_context_create("scan_tailor_universal-thumb");
    }

    ddjvu_cache_clear(_context);

#if DDJVUAPI_VERSION >= 19
    QByteArray b = filename.toUtf8();
    ddjvu_document_s * document = ddjvu_document_create_by_filename_utf8(_context, b, true);
#else
    QByteArray b = QFile::encodeName(filename);
    ddjvu_document_s * document = ddjvu_document_create_by_filename(_context, b, true);
#endif

    if (! document) {
        return res;
    }

    qDebug() << "DjVuReader::readImage 1";

    ddjvu_status_t status;
    while ((status = ddjvu_document_decoding_status(document)) < DDJVU_JOB_OK) {
        handle_ddjvu_messages(_context, true);
    }
qDebug() << "DjVuReader::readImage 1_";
    if (ddjvu_document_decoding_done(document)) {

        int page_cnt = ddjvu_document_get_pagenum(document);
        qDebug() << "DjVuReader::readImage 2";

        if (page_num < page_cnt) {
            ddjvu_pageinfo_t info;

             qDebug() << "ddjvu_document_get_pageinfo";
            while ((status = ddjvu_document_get_pageinfo(document, page_num, &info)) < DDJVU_JOB_OK) {
                handle_ddjvu_messages(_context, true);
            }

            if (status == DDJVU_JOB_OK) {
qDebug() << "ddjvu_thumbnail_status";
                int start = 0;
                while ((status = ddjvu_thumbnail_status(document, page_num, start)) < DDJVU_JOB_OK) {
                    if (status == DDJVU_JOB_NOTSTARTED && !start) {
                        start = 1;
                    } else if (status != DDJVU_JOB_NOTSTARTED) {
                        handle_ddjvu_messages(_context, true);
                    }
                }


                if (status == DDJVU_JOB_OK) {
#if DDJVUAPI_VERSION < 18
                    unsigned int masks[3] = { 0xff0000, 0xff00, 0xff };
                    ddjvu_format_t * format = ddjvu_format_create(DDJVU_FORMAT_RGBMASK32, 3, masks);
#else
                    unsigned int masks[4] = { 0xff0000, 0xff00, 0xff, 0xff000000 };
                    ddjvu_format_t * format = ddjvu_format_create(DDJVU_FORMAT_RGBMASK32, 4, masks);
#endif

                    if (format) {
                        ddjvu_format_set_row_order(format, 1);
                        int w = info.width;
                        int h = info.height;
                        QImage img(w, h, QImage::Format_RGB32);
                        qDebug() << "ddjvu_thumbnail_render";
                        if (ddjvu_thumbnail_render(document, page_num, &w, &h, format,
                                                   img.bytesPerLine(), (char*)img.bits() )) {
                            qDebug() << "ok";
                            res = img;
                        }
                        ddjvu_format_release(format);
                    }

                }
            }



        }
    }

    qDebug() << "DjVuReader::readImage end";
    qDebug() << res.isNull();
    ddjvu_document_release(document);
    return res;
}
