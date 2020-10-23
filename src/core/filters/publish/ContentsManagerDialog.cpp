#include "ContentsManagerDialog.h"
#include "ui_ContentsManagerDialog.h"

#include <QScrollBar>
#include <QSettings>
#include <QGraphicsItem>
#include <QMimeData>
#include <cmath>
#include <QShortcut>
#include <QAbstractButton>
#include <QInputDialog>
#include <QLabel>

#include "settings/globalstaticsettings.h"
#include "ThumbnailSequence.h"
#include "ThumbnailPixmapCache.h"
#include "ThumbnailFactory.h"

#include "ProjectPages.h"
#include "Filter.h"
#include "Settings.h"
#include "PageId.h"

#include <qdebug.h>

namespace publish {

const QString QContentsTreeWidget::mimeType = "application/stu-contents-entry";

QContentsTreeWidget::QContentsTreeWidget(QWidget *parent): QTreeWidget(parent)
{
    setAcceptDrops(true);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(true);
}

QContentsTreeWidget::~QContentsTreeWidget()
{
}

QStringList
QContentsTreeWidget::mimeTypes() const
{
    return QStringList() << QContentsTreeWidget::mimeType;
}

void QContentsTreeWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event && reinterpret_cast<QGraphicsScene*>( event->source())) {
        if (QTreeWidgetItem* item = itemAt(event->pos())) {
            if (item != currentItem()) {
                qDebug() << item->text(0);
                event->accept();
            } else {
                event->ignore();
            }
        }

    }

    QTreeWidget::dragMoveEvent(event);
}

void QContentsTreeWidget::dropEvent(QDropEvent *event)
{
    if (const QMimeData* mime = event->mimeData()) {
        if (mime->hasFormat(PageId::mimeType)) {
            QByteArray data = mime->data(PageId::mimeType);

            int pages_cnt;
            int read = sizeof(pages_cnt);
            assert(data.size() > read);
            memcpy(&pages_cnt, data.data(), read);

            QVector<PageId> pages;
            PageId pageId;

            for (int i = 0; i < pages_cnt; i++) {
                data.setRawData(data.data() + read, data.size() - read);
                if (!data.isEmpty()) {
                    read = PageId::fromByteArray(data, pageId);
                    pages += pageId;
                }
            }

            assert(!pages.isEmpty());

            QTreeWidgetItem* item = itemAt(event->pos());
            addBookmarksForPages(item, pages);

            event->acceptProposedAction();
            return;
        } else if (mime->hasFormat(QContentsTreeWidget::mimeType)) {

        }
    }

    QTreeWidget::dropEvent(event);
}

void QContentsTreeWidget::addBookmarksForPages(QTreeWidgetItem* parent, const QVector<PageId>& pages)
{
    for (const PageId& p: pages) {
        QTreeWidgetItem* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(this);
        item->setFlags(item->flags().setFlag(Qt::ItemIsDropEnabled, true));
        item->setText(0, tr("title"));
        QString url = QString("%1/%2/%3").arg(p.imageId().filePath()).
                arg(p.imageId().page()).arg(p.subPageAsString());
        item->setData(1, Qt::UserRole, url);
        emit updateItemPage(item);
    }
}


ContentsManagerDialog::ContentsManagerDialog(Filter* filter, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ContentsManagerDialog),
    m_filter(filter),
    m_pageSequence(m_filter->pages()->toPageSequence(PAGE_VIEW))
{
    for (const PageInfo& p: qAsConst(m_pageSequence)) {
        m_page_uids += QString("%1/%2/%3").arg(p.imageId().filePath()).
                arg(p.imageId().page()).arg(p.id().subPageAsString());
    }

    QSettings settings;
    m_maxLogicalThumbSize = settings.value(_key_thumbnails_max_thumb_size, _key_thumbnails_max_thumb_size_def).toSizeF();
    m_maxLogicalThumbSize /= 3;
    m_ptrThumbSequence.reset(new ThumbnailSequence(m_maxLogicalThumbSize));
    m_ptrThumbSequence->setDraggingEnabled(true);
    m_ptrThumbSequence->setIsDjbzView(true);

    ui->setupUi(this);
    setupThumbView();
    resetThumbSequence();

    //    displayBookmarks(m_filter->settings()->contents().join('\n'));
    displayBookmarks("(bookmarks \n(\"as da\" \"1\" \n  (\"asd s\" \"sad\") \n  (\"2asd s\" \"2sad\"))  )");

    ui->thumbView->ensureVisible(m_ptrThumbSequence->selectionLeaderSceneRect(), 0, 0);

    connect(ui->treeContents, &QContentsTreeWidget::updateItemPage, this, &ContentsManagerDialog::updateItemPage);
}

void
ContentsManagerDialog::setupThumbView()
{
    int const sb = ui->thumbView->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    int inner_width = ui->thumbView->maximumViewportSize().width() - sb;
    if (ui->thumbView->style()->styleHint(QStyle::SH_ScrollView_FrameOnlyAroundContents, 0, ui->thumbView)) {
        inner_width -= ui->thumbView->frameWidth() * 2;
    }
    int const delta_x = ui->thumbView->size().width() - inner_width;
    ui->thumbView->setMinimumWidth((int)ceil(m_maxLogicalThumbSize.width() + delta_x));

    ui->thumbView->setBackgroundBrush(palette().color(QPalette::Window));
    m_ptrThumbSequence->attachView(ui->thumbView);

    ui->thumbView->removeEventFilter(this); // make sure installed once
    ui->thumbView->installEventFilter(this);
    if (ui->thumbView->verticalScrollBar()) {
        ui->thumbView->verticalScrollBar()->removeEventFilter(this);
        ui->thumbView->verticalScrollBar()->installEventFilter(this);
    }

    connect(ui->thumbView, &QGraphicsView::rubberBandChanged, this, [=](QRect viewportRect, QPointF /*fromScenePoint*/, QPointF /*toScenePoint*/){
        if (!viewportRect.isNull()) {
            QRectF rect_scene = ui->thumbView->mapToScene(viewportRect).boundingRect();
            const QList<QGraphicsItem*> list = ui->thumbView->scene()->items(rect_scene);
            QSet<PageId> items_to_select;
            for (QGraphicsItem* i: list) {
                PageId page;
                if (m_ptrThumbSequence->findPageByGraphicsItem(i, page)) {
                    items_to_select += page;
                }
            }

            m_ptrThumbSequence->setSelection(items_to_select);
        }
    });

}

QString unquote(const QString word)
{
    QString res = word;
    if (res.startsWith("\"") && res.endsWith("\"")) {
        res = res.mid(1, res.size()-2).trimmed();
    }
    return res;
}

QString read_text(QTextStream& ts)
{
    ts.skipWhiteSpace();
    size_t pos = ts.pos();
    QString line;
    ts >> line;
    if (line.startsWith("(") ||
            line.startsWith(")")) {
        ts.seek(pos+1);
        return line.at(0);
    }
    if (line.endsWith(")")) {
        do {
            line = line.left(line.size()-1);
            ts.seek(ts.pos()-1);
        } while ( line.endsWith(")") );
        return line;
    }

    if (line.startsWith("\"")) {

        if (line.size() > 1 && line.endsWith("\"")) {
            return unquote(line);
        }

        QString word;
        do {
            word = read_text(ts);
            line += " " + word;
        } while ( !word.endsWith("\"") );
    }
    return unquote(line);
}


QTreeWidgetItem*
ContentsManagerDialog::readBookmark(QTextStream& ts)
{
    QString word;
    do {
        word = read_text(ts);
        if (word.isEmpty()) {
            return nullptr;
        }
    } while (word != "(");

    word = read_text(ts); // text is "( )"
    if (word == ")") return nullptr;

    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setFlags(item->flags().setFlag(Qt::ItemIsDropEnabled, true));
    item->setText(0, word);

    word = read_text(ts);
    if (word == ")") return item;
    item->setData(1, Qt::UserRole, word);
    int page_no = m_page_uids.indexOf(word);
    if (page_no != -1) {
        item->setText(1, QString::number(page_no));
    } else {
        item->setText(1, word);
    }

    QTreeWidgetItem* child;
    do {
        child = nullptr;
        word = read_text(ts);
        if (word == "(") {
            ts.seek(ts.pos()-1);
            child = readBookmark(ts);
            if (child) {
                item->addChild(child);
            }
        } else {
            assert(word == ")");
        }
    } while (child);

    return item;
}


void
ContentsManagerDialog::displayBookmarks(const QString& text)
{
    ui->treeContents->clear();

    QTextStream ts(text.toUtf8(), QIODevice::ReadOnly);

    if (read_text(ts) == "(" &&
            read_text(ts) == "bookmarks") {
        while (QTreeWidgetItem* item = readBookmark(ts)) {
            ui->treeContents->addTopLevelItem(item);
        }
    }

    if (ui->treeContents->topLevelItemCount() == 0) {
        QTreeWidgetItem* hint = new QTreeWidgetItem(ui->treeContents);
        hint->setData(0, Qt::UserRole+1, "hint");
        QLabel* label = new QLabel(tr("Drag'n'drop pages here to create a new entry."));
        label->setWordWrap(true);
        ui->treeContents->setItemWidget(hint, 0, label);
        ui->treeContents->setColumnCount(1);
    } else if (ui->treeContents->columnCount() != 2) {
        ui->treeContents->setColumnCount(2);
    }

    ui->treeContents->expandAll();
}

QStringList getBookmark(QTreeWidgetItem* item, QString span)
{
    span += "\t";
    QStringList res;
    res += span + QString("(\"%1\"\t\"%2\"").arg(item->text(0)).arg(item->data(1, Qt::UserRole).toString());
    if (item->childCount() == 0) {
        res.last() += ")";
        return res;
    }

    for (int i = 0; i < item->childCount(); i++) {
        res += getBookmark(item->child(i), span);
    }
    res += span + ")";
    return res;
}

QStringList
ContentsManagerDialog::getBookmarks()
{
    QStringList res;
    res += "(bookmarks";

    if (ui->treeContents->topLevelItemCount() != 1 ||
            ui->treeContents->topLevelItem(0)->data(0, Qt::UserRole+1).isNull()) { // not a hint widget
        for (int i = 0; ui->treeContents->topLevelItemCount(); i++) {
            res += getBookmark(ui->treeContents->topLevelItem(i), "\t");
        }
    }
    res += ")";
    return res;
}

static int _wheel_val_sum_thumbs = 0;
bool
ContentsManagerDialog::eventFilter(QObject* obj, QEvent* ev)
{

    if (obj == ui->thumbView) {
        if (ev->type() == QEvent::Resize) {
            m_ptrThumbSequence->invalidateAllThumbnails();
        }
    }


    if (!GlobalStaticSettings::m_fixedMaxLogicalThumbSize) {
        if (obj == ui->thumbView || obj == ui->thumbView->verticalScrollBar()) {
            if (ev->type() == QEvent::Wheel) {
                Qt::KeyboardModifiers mods = GlobalStaticSettings::m_hotKeyManager.get(ThumbSizeChange)->
                        sequences().first().m_modifierSequence;
                QWheelEvent* wev = static_cast<QWheelEvent*>(ev);

                if ((wev->modifiers() & mods) == mods) {

                    const QPoint& angleDelta = wev->angleDelta();
                    _wheel_val_sum_thumbs += angleDelta.x() + angleDelta.y();

                    if (abs(_wheel_val_sum_thumbs) >= 30) {
                        wev->accept();

                        const int dy = (_wheel_val_sum_thumbs > 0) ? 10 : -10;
                        _wheel_val_sum_thumbs = 0;

                        m_maxLogicalThumbSize += QSizeF(dy, dy);
                        if (m_maxLogicalThumbSize.width() < 25.) {
                            m_maxLogicalThumbSize.setWidth(25.);
                        }
                        if (m_maxLogicalThumbSize.height() < 16.) {
                            m_maxLogicalThumbSize.setHeight(16.);
                        }

                        if (m_ptrThumbSequence.get()) {
                            m_ptrThumbSequence->setMaxLogicalThumbSize(m_maxLogicalThumbSize);
                        }

                        setupThumbView();
                        resetThumbSequence();
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void
ContentsManagerDialog::resetThumbSequence()
{
    if (m_filter->thumbnailPixmapCache()) {
        IntrusivePtr<CompositeCacheDrivenTask> const task(
                    m_filter->createCompositeCacheDrivenTask()
                    );

        assert(task);

        m_ptrThumbSequence->setThumbnailFactory(
                    IntrusivePtr<ThumbnailFactory>(
                        new ThumbnailFactory(
                            IntrusivePtr<ThumbnailPixmapCache>(m_filter->thumbnailPixmapCache()),
                            m_maxLogicalThumbSize, task
                            )
                        )
                    );
    }




    m_ptrThumbSequence->reset(m_pageSequence, ThumbnailSequence::SelectionAction::RESET_SELECTION);

    if (!m_filter->thumbnailPixmapCache()) {
        // Empty project.
        assert(m_filter->pages()->numImages() == 0);
        m_ptrThumbSequence->setThumbnailFactory(
                    IntrusivePtr<ThumbnailFactory>()
                    );
    }

}

ContentsManagerDialog::~ContentsManagerDialog()
{
    delete ui;
}

void ContentsManagerDialog::updateItemPage(QTreeWidgetItem* item)
{
    if (item) {
        QString url = item->data(1, Qt::UserRole).toString();
        int page_no = m_page_uids.indexOf(url);
        if (page_no != -1) {
            item->setText(1, QString::number(page_no));
        } else {
            item->setText(1, url);
        }
    }
}

void ContentsManagerDialog::on_buttonBox_accepted()
{
    m_filter->settings()->setContents(getBookmarks());
    accept();
}

void ContentsManagerDialog::on_buttonBox_rejected()
{
    reject();
}

}

