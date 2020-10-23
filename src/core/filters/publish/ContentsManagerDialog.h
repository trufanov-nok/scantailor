#ifndef CONTENTSMANAGERDIALOG_H
#define CONTENTSMANAGERDIALOG_H

#include <QDialog>
#include <QSizeF>

#include "IntrusivePtr.h"
#include "PageSequence.h"

#include <QTreeWidget>

namespace Ui {
class ContentsManagerDialog;
}

class QTreeWidgetItem;
class PageId;
class ThumbnailSequence;
class ThumbnailPixmapCache;
class ProjectPages;
class QAbstractButton;

namespace publish {

class DjbzDispatcher;

class QContentsTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    QContentsTreeWidget(QWidget *parent = nullptr);
    ~QContentsTreeWidget();

    static const QString mimeType;
signals:
    void updateItemPage(QTreeWidgetItem* item);
private:
    void addBookmarksForPages(QTreeWidgetItem* parent, const QVector<PageId>& pages);
protected:
    QStringList mimeTypes() const override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};


class Filter;

class ContentsManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ContentsManagerDialog(Filter* filter, QWidget *parent = nullptr);
    ~ContentsManagerDialog();

    void displayBookmarks(const QString& text);
    QStringList getBookmarks();
private slots:
    void on_buttonBox_rejected();
    void on_buttonBox_accepted();
    void updateItemPage(QTreeWidgetItem* item);
private:
    void setupThumbView();
    void resetThumbSequence();
    QTreeWidgetItem* readBookmark(QTextStream& ts);


    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    Ui::ContentsManagerDialog *ui;
    Filter* m_filter;
    QStringList m_page_uids;
    std::unique_ptr<ThumbnailSequence> m_ptrThumbSequence;
    const PageSequence m_pageSequence;
    IntrusivePtr<ThumbnailPixmapCache> m_ptrThumbnailCache;
    QSizeF m_maxLogicalThumbSize;
    QStringList m_oldContents_copy;
    QStringList m_contents;
};

}

#endif // CONTENTSMANAGERDIALOG_H
