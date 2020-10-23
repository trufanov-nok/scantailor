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

#include "OptionsWidget.h"
#include "OptionsWidget.moc"
#include "Filter.h"
#include "ApplyToDialog.h"
#include "Settings.h"
#include "DjbzDispatcher.h"
#include "Utils.h"

#include "Filter.h"
#include "DjbzManagerDialog.h"
#include "MetadataEditorDialog.h"
#include "djview4/qdjvuwidget.h"

#include <QColorDialog>
#include <QClipboard>
#include <QImageWriter>
#include <QFileDialog>
#include <QMessageBox>
#include <qdebug.h>

namespace publish
{

inline QDjVuWidget::DisplayMode idx2displayMode(int idx) {
    switch (idx) {
    case 0: return QDjVuWidget::DISPLAY_COLOR;
    case 1: return QDjVuWidget::DISPLAY_FG;
    case 2: return QDjVuWidget::DISPLAY_BG;
    case 3: return QDjVuWidget::DISPLAY_STENCIL;
    case 4: return QDjVuWidget::DISPLAY_TEXT;
    }
    return QDjVuWidget::DISPLAY_COLOR;
}

OptionsWidget::OptionsWidget(Filter* filter, PageSelectionAccessor const& page_selection_accessor)
    : m_filter(filter),
      m_settings(m_filter->settings()),
      m_djvu(filter->djVuWidget()),
      m_pageSelectionAccessor(page_selection_accessor),
      m_delayed_update(false)
{
    setupUi(this);

    for (ImageFilters::Info::const_iterator it = ImageFilters::info.cbegin();
         it != ImageFilters::info.cend(); ++it) {
        cbScaleMethod->addItem(it->title, it.key());
    }
    connect(m_djvu, &QDjVuWidget::pointerSelect, this, &OptionsWidget::rectSelected);

    connect(m_filter, &Filter::tabChanged, this, [this](int idx) {
        m_djvu_mode = idx2displayMode(idx);
        m_djvu->setDisplayMode((QDjVuWidget::DisplayMode)m_djvu_mode);
        if (m_djvu_mode == QDjVuWidget::DISPLAY_FG) {
            m_djvu->setModifiersForSelect(Qt::NoModifier);
        } else {
            m_djvu->setModifiersForSelect(Qt::ControlModifier);
            if (m_djvu_mode == QDjVuWidget::DISPLAY_COLOR && m_delayed_update) {
                m_delayed_update = false;
                emit this->reloadRequested();
            }
        }
        paintHighlights();
    });

    connect(m_djvu, &QDjVuWidget::layoutChanged, this, [this]() {
        paintHighlights();
    });

    m_djvu->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_djvu, &QDjVuWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        showContextMenu(m_djvu->mapToGlobal(pos), QRect(pos, pos+QPoint(1,1)));
    });

}

OptionsWidget::~OptionsWidget()
{
}

void OptionsWidget::paintHighlights()
{
    m_djvu->clearHighlights(0);
    if (m_djvu_mode == QDjVuWidget::DISPLAY_FG) {
        std::unique_ptr<Params> params_ptr = m_settings->getPageParams(m_pageId);
        if (params_ptr && !params_ptr->colorRects().isEmpty()) {
            for (const auto& pair: params_ptr->colorRects()) {
                const QRect r = pair.first;
                QColor clr = pair.second;
                clr.setAlpha(100);
                m_djvu->addHighlight(0, r.left(), r.top(), r.width(), r.height(), clr);
            }
        }
    }
}

// adjusted stdcolors from DjVuLibre's djvumake.cpp
static struct stdcol {
    const QString name;
    unsigned char r, g, b;
} stdcols[] = {
{QObject::tr("aqua"),    0x00, 0xFF, 0xFF},
{QObject::tr("black"),   0x00, 0x00, 0x00},
{QObject::tr("blue"),    0x00, 0x00, 0xFF},
{QObject::tr("fuchsia"), 0xFF, 0x00, 0xFF},
{QObject::tr("gray"),    0x80, 0x80, 0x80},
{QObject::tr("green"),   0x00, 0x80, 0x00},
{QObject::tr("lime"),    0x00, 0xFF, 0x00},
{QObject::tr("maroon"),  0x80, 0x00, 0x00},
{QObject::tr("navy"),    0x00, 0x00, 0x80},
{QObject::tr("olive"),   0x80, 0x80, 0x00},
{QObject::tr("purple"),  0x80, 0x00, 0x80},
{QObject::tr("red"),     0xFF, 0x00, 0x00},
{QObject::tr("silver"),  0xC0, 0xC0, 0xC0},
{QObject::tr("teal"),    0x00, 0x80, 0x80},
{QObject::tr("white"),   0xFF, 0xFF, 0xFF},
{QObject::tr("yellow"),  0xFF, 0xFF, 0x00},
{0, 0, 0, 0}
};

QString findStdColor(const QColor& clr)
{
    int r = clr.red(); int g = clr.green(); int b = clr.blue();
    for (int i = 0; i < 16; i++) {
        if (stdcols[i].r == r && stdcols[i].g == g && stdcols[i].b == b) {
            return stdcols[i].name;
        }
    }
    return clr.name();
}

void
OptionsWidget::preUpdateUI(PageId const& page_id)
{
    m_pageId = page_id;
    std::unique_ptr<Params> params_ptr = m_settings->getPageParams(page_id);
    if (!params_ptr || params_ptr->djbzId().isEmpty()) {
        m_filter->ensureAllPagesHaveDjbz();
        params_ptr = m_settings->getPageParams(page_id);
    }

    QString djbz = params_ptr->djbzId();
    if (m_settings->djbzDispatcherConst().isDummyDjbzId(djbz)) {
        djbz = tr("none");
    }
    lblDjbzId->setText(Utils::richTextForLink(djbz));

    const bool no_fgbz = params_ptr->fgbzOptions().isEmpty();
    const QColor clr = no_fgbz ? Qt::GlobalColor::black :
                                 QColor(params_ptr->fgbzOptions());


    lblTextColorValue->setText(Utils::richTextForLink(
                                   findStdColor(clr), clr.name(QColor::HexRgb))
                               );
    lblTextColorClear->setVisible(clr != Qt::GlobalColor::black);
    connect(lblTextColorClear, &ClickableLabel::clicked, this, [this](){
        lblTextColorClear->hide();
        lblTextColorValue->setText(Utils::richTextForLink(findStdColor(Qt::GlobalColor::black)));
        std::unique_ptr<Params> params_ptr = m_settings->getPageParams(m_pageId);
        params_ptr->setFGbzOptions("#000000");
        m_settings->setPageParams(m_pageId, *params_ptr);
        emit reloadRequested();
    });

    lblPageRotationVal->setText(Utils::richTextForLink(tr("%1°").arg(90*params_ptr->rotation())));
    lblPageRotationClear->setVisible(params_ptr->rotation());
    connect(lblPageRotationClear, &ClickableLabel::clicked, this, [this](){
        lblPageRotationClear->hide();
        std::unique_ptr<Params> params_ptr = m_settings->getPageParams(m_pageId);
        params_ptr->setRotation(0);
        m_settings->setPageParams(m_pageId, *params_ptr);
        lblPageRotationVal->setText(Utils::richTextForLink(tr("%1°").arg(0)));
        emit reloadRequested();
    });

    cbClean->setChecked(params_ptr->clean());
    cbErosion->setChecked(params_ptr->erosion());
    cbSmooth->setChecked(params_ptr->smooth());
    if (sbBSF->value() != params_ptr->bsf()) {
        sbBSF->setValue(params_ptr->bsf());
    } else {
        emit sbBSF->valueChanged(sbBSF->value());
    }

    int idx = cbScaleMethod->findData((uint)params_ptr->scaleMethod());
    if (idx == -1) idx = cbScaleMethod->findData(GlobalStaticSettings::m_default_scale_filter);
    if (cbScaleMethod->currentIndex() != idx) {
        cbScaleMethod->setCurrentIndex(idx);
    }
}

void
OptionsWidget::postUpdateUI()
{
}

void
OptionsWidget::on_lblDjbzId_linkActivated(const QString &/*link*/)
{
    DjbzManagerDialog dlg(m_filter, m_pageId, this);
    const DjbzDispatcher& djbzs = m_settings->djbzDispatcherConst();
    const QString djbz_id = djbzs.findDjbzForPage(m_pageId);
    QDateTime rev = djbzs.djbzDict(djbz_id).revision();
    if (dlg.exec() == QDialog::DialogCode::Accepted) {
        preUpdateUI(m_pageId);
        emit invalidateAllThumbnails();
        if (djbz_id != djbzs.findDjbzForPage(m_pageId) ||
                rev != djbzs.djbzDict(djbz_id).revision()) {
            emit reloadRequested();
        }
    }
}

void OptionsWidget::on_cbClean_clicked(bool checked)
{
    std::unique_ptr<Params> params_ptr = m_settings->getPageParams(m_pageId);
    if (params_ptr && params_ptr->clean() != checked) {
        params_ptr->setClean(checked);
        m_settings->setPageParams(m_pageId, *params_ptr);
        emit reloadRequested();
    }
}

void OptionsWidget::on_cbErosion_clicked(bool checked)
{
    std::unique_ptr<Params> params_ptr = m_settings->getPageParams(m_pageId);
    if (params_ptr && params_ptr->erosion() != checked) {
        params_ptr->setErosion(checked);
        m_settings->setPageParams(m_pageId, *params_ptr);
        emit reloadRequested();
    }
}

void OptionsWidget::on_cbSmooth_clicked(bool checked)
{
    std::unique_ptr<Params> params_ptr = m_settings->getPageParams(m_pageId);
    if (params_ptr && params_ptr->smooth() != checked) {
        params_ptr->setSmooth(checked);
        m_settings->setPageParams(m_pageId, *params_ptr);
        emit reloadRequested();
    }
}

void OptionsWidget::on_sbBSF_valueChanged(int arg1)
{
    cbScaleMethod->setEnabled(arg1 > 1);

    std::unique_ptr<Params> params_ptr = m_settings->getPageParams(m_pageId);
    if (params_ptr){
        if (arg1 != params_ptr->bsf()) {
            params_ptr->setBsf(arg1);
            m_settings->setPageParams(m_pageId, *params_ptr);
            emit reloadRequested();
        }
    }
}

void OptionsWidget::on_cbScaleMethod_currentIndexChanged(int index)
{
    std::unique_ptr<Params> params_ptr = m_settings->getPageParams(m_pageId);
    if (params_ptr) {
        const FREE_IMAGE_FILTER val = (FREE_IMAGE_FILTER) cbScaleMethod->itemData(index).toUInt();
        if (val != params_ptr->scaleMethod()) {
            params_ptr->setScaleMethod(val);
            m_settings->setPageParams(m_pageId, *params_ptr);
            emit reloadRequested();
        }
    }
}

void OptionsWidget::on_lblTextColorValue_linkActivated(const QString & link)
{
    QColorDialog dialog(this);
    dialog.setOption(QColorDialog::DontUseNativeDialog, true);

    for (int i = 0; i < 16; i++) {
        const stdcol* const clr = stdcols+i;
        dialog.setCustomColor(i, QColor(clr->r, clr->g, clr->b));
    }

    if (link != "#") {
        dialog.setCurrentColor(QColor(link));
    }

    if (dialog.exec() == QDialog::Accepted) {
        const QColor clr = dialog.selectedColor();
        lblTextColorValue->setText(
                    Utils::richTextForLink(findStdColor(clr),
                                           clr.name(QColor::HexRgb))
                    );
        lblTextColorClear->show();

        std::unique_ptr<Params> params_ptr = m_settings->getPageParams(m_pageId);
        if (params_ptr) {
            params_ptr->setFGbzOptions(clr.name(QColor::HexRgb));
            m_settings->setPageParams(m_pageId, *params_ptr);
            emit reloadRequested();
        }

    }
}

/* The code below is an adapted piece of DjView4's QDjView.cpp */

/*! Save \a image info the specified file
  using a format derived from the filename suffix.
  When argument \a filename is omitted,
  a file dialog is presented to the user. */

bool
OptionsWidget::saveImageFile(const QImage &image)
{
    // obtain filename with suitable suffix
    QString filters;
    QString tiff_filter;
//    QString png_filter;
    const QList<QByteArray> formats = QImageWriter::supportedImageFormats();
    for ( const QByteArray& format : formats) {
        const QString id = QString(format).toUpper();
        const QString filter = tr("%1 files (*.%2)", "save image filter (publish)")
                .arg(id, QString(format).toLower());

        filters += filter + ";;";

        /*if (id == "PNG") {
            png_filter = filter;
        } else*/ if (id == "TIFF") {
            tiff_filter = filter;
        }
    }
    filters += tr("All files", "save filter") + " (*)";

    const std::unique_ptr<Params> params_ptr = m_settings->getPageParams(m_pageId);
    QFileInfo fi(params_ptr->djvuFilename());

    QString filename = fi.baseName();
    if (formats.size()) {
        filename += "." + QString(formats[0]);
    }
    if (!m_recent_folder.isEmpty()) {
        filename = m_recent_folder + filename;
    }
    const QString caption = tr("Save Image", "dialog caption (publish)");
    filename = QFileDialog::getSaveFileName(this, caption,
                                            filename, filters, tiff_filter.isEmpty() ?
//                                                ( png_filter.isEmpty()?
                                                      nullptr :
//                                                      &png_filter ) :
                                                &tiff_filter);
    if (filename.isEmpty())
        return false;

    fi.setFile(filename);
    m_recent_folder = fi.path() + "/";

    // suffix
    QString suffix = fi.suffix();
    if (suffix.isEmpty())
    {
        QMessageBox::critical(this,
                              tr("Error", "dialog caption (publish)"),
                              tr("Cannot determine file format.\n"
                               "Filename '%1' has no suffix.")
                              .arg(QFileInfo(filename).fileName()) );
        return false;
    }
    // write

    // TODO: use TiffWriter for saving to tiff format

    errno = 0;
    QFile file(filename);
    QImageWriter writer(&file, suffix.toLatin1());
    if (! writer.write(image))
    {
        QString message = file.errorString();
        if (writer.error() == QImageWriter::UnsupportedFormatError)
            message = tr("Image format %1 not supported.").arg(suffix.toUpper());
#if HAVE_STRERROR
        else if (file.error() == QFile::OpenError && errno > 0)
            message = strerror(errno);
#endif
        QMessageBox::critical(this,
                              tr("Error", "dialog caption (publish)"),
                              tr("Cannot write file '%1'.\n%2.")
                              .arg(QFileInfo(filename).fileName(), message) );
        file.remove();
        return false;
    }
    return true;
}

/* end of QDjView code */

void
OptionsWidget::showContextMenu(const QPoint &pos, const QRect &rect, bool is_selection)
{
    std::unique_ptr<Params> params_ptr = m_settings->getPageParams(m_pageId);
    const QRect seg = m_djvu->getSegmentForRect(rect, 0);
    const bool has_regions_below = params_ptr->containsColorRectsIn(seg);

    std::unique_ptr<QMenu> menu(new QMenu);
    QAction* color_change = nullptr;
    if (is_selection) {
        color_change = menu->addAction(tr("Set text color..."));
        menu->addSeparator();
    }
    QAction* delete_regions = menu->addAction(tr("Delete region(s) below"));
    delete_regions->setEnabled(has_regions_below);
    QAction* delete_all_regions = menu->addAction(tr("Delete all regions"));
    delete_all_regions->setEnabled(!params_ptr->colorRects().isEmpty());

    menu->addSeparator();


    QString title = is_selection ?
                tr("Copy image (%1x%2 pixels)").arg(rect.width()).arg(rect.height()) :
                tr("Copy image");
    QAction *copy_image = menu->addAction(title);
    copy_image->setStatusTip(tr("Copy part of the image into the clipboard."));
    title = is_selection ?
                    tr("Save image (%1x%2 pixels) as...").arg(rect.width()).arg(rect.height()) :
                    tr("Save image as...");
    QAction *save_image = menu->addAction(title);
    save_image->setStatusTip(tr("Save part of the image into a file."));


    QAction *zoom = nullptr;
    if (is_selection) {
        menu->addSeparator();

        zoom = menu->addAction(tr("Zoom to rectangle"));
        zoom->setStatusTip(tr("Zoom the selection to fit the window."));
    }

    bool do_it = false;
    QAction *action = menu->exec(pos-QPoint(5,5));
    if (is_selection && action == color_change) {
        QColorDialog dialog(this);
        dialog.setOption(QColorDialog::DontUseNativeDialog, true);

        for (int i = 0; i < 16; i++) {
            const stdcol* const clr = stdcols+i;
            dialog.setCustomColor(i, QColor(clr->r, clr->g, clr->b));
        }

        if (dialog.exec() == QDialog::Accepted) {
            QColor clr = dialog.selectedColor();
            params_ptr->addColorRect(seg, clr);
            m_settings->setPageParams(m_pageId, *params_ptr);
            do_it = true;
        }
    } else if (action == delete_regions) {
        params_ptr->removeColorRectsIn(seg);
        m_settings->setPageParams(m_pageId, *params_ptr);
        do_it = true;
    } else if (action == delete_all_regions) {
        params_ptr->clearColorRects();
        m_settings->setPageParams(m_pageId, *params_ptr);
        do_it = true;
    } else if (action == copy_image) {
        QApplication::clipboard()->setImage(m_djvu->renderImageForRect(0, is_selection ? seg : QRect()));
    } else if (action == save_image) {
        saveImageFile(m_djvu->renderImageForRect(0, is_selection ? seg : QRect()));
    } else if (is_selection && action == zoom) {
        m_djvu->zoomRect(rect);
    }


    if (do_it) {
        //        if (m_djvu_mode == QDjVuWidget::DISPLAY_COLOR) {
        emit reloadRequested();
        //        } else {
        //            m_delayed_update = true;
        //            paintHighlights();
        //        }
    }
}

void
OptionsWidget::rectSelected(const QPoint &pointerPos, const QRect &rect)
{
    if (!rect.isEmpty()) {
        showContextMenu(pointerPos, rect, true);
    }
}

void
OptionsWidget::resizeEvent(QResizeEvent *event)
{
    const int w = event->size().width();
    if (event && event->oldSize().width() != w) {
        const int max_text_length = w - lblPageTitle->width() - 50;
        QFontMetrics metrix(lblPageTitleVal->font());
        const std::unique_ptr<Params> params_ptr = m_settings->getPageParams(m_pageId);
        if (params_ptr) {
            const QString title = params_ptr->title();
            if (title.isEmpty()) {
                lblPageTitleVal->setText(Utils::richTextForLink( metrix.elidedText( tr("none"), Qt::ElideMiddle, max_text_length) ));
            } else {
                lblPageTitleVal->setText(Utils::richTextForLink( metrix.elidedText( title, Qt::ElideMiddle, max_text_length), title) );
            }

        }
    }

    FilterOptionsWidget::resizeEvent(event);
}

void OptionsWidget::on_lblPageTitleVal_linkActivated(const QString &/*link*/)
{
    MetadataEditorDialog dialog(m_settings->metadataRef());
    if (dialog.exec() == QDialog::Accepted) {
        m_settings->setMetadata(dialog.getMetadata());
    }
}

} // namespace publish



void publish::OptionsWidget::on_lblPageRotationVal_linkActivated(const QString &/*link*/)
{
    std::unique_ptr<QMenu> menu(new QMenu);
                      menu->addAction(tr("0°"));
    QAction* act90  = menu->addAction(tr("90°"));
    QAction* act180 = menu->addAction(tr("180°"));
    QAction* act270 = menu->addAction(tr("270°"));

    QAction *action = menu->exec(QCursor::pos()-QPoint(5,5));
    if (action) {
        uint val = 0;
        if (action == act90) {
            val = 1;
        } else if (action == act180) {
            val = 2;
        } else if (action == act270) {
            val = 3;
        }
        const std::unique_ptr<Params> params_ptr = m_settings->getPageParams(m_pageId);
        params_ptr->setRotation(val);
        m_settings->setPageParams(m_pageId, *params_ptr);
        lblPageRotationVal->setText(Utils::richTextForLink(tr("%1°").arg(90*val)));
        emit this->reloadRequested();
    }

}
