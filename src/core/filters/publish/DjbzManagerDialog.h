#ifndef DJBZMANAGER_H
#define DJBZMANAGER_H

#include <QDialog>
#include <QSizeF>

#include "IntrusivePtr.h"
#include "PageSequence.h"
#include "DjbzDispatcher.h"

#include <QTreeWidget>

namespace Ui {
class DjbzManagerDialog;
}

class QTreeWidgetItem;
class PageId;
class ThumbnailSequence;
class ThumbnailPixmapCache;
class ProjectPages;
class QAbstractButton;

namespace publish {

class DjbzDispatcher;

class QDjbzTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    QDjbzTreeWidget(QWidget *parent = nullptr);
    ~QDjbzTreeWidget();
    static QString mimeType;
signals:
    void movePages(const QSet<PageId>&, QTreeWidgetItem*);
protected:
    QStringList mimeTypes() const override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};


class Filter;

class DjbzManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DjbzManagerDialog(Filter* filter, PageId const& page_id, QWidget *parent = nullptr);
    ~DjbzManagerDialog();
private slots:
    void on_sbAggression_valueChanged(int arg1);
    void on_buttonBox_clicked(QAbstractButton *button);
    void on_sbMaxPages_valueChanged(int arg1);
    void on_buttonBox_rejected();
    void on_buttonBox_accepted();
    void on_actionSelectAll_triggered();
    void on_cbExtension_currentIndexChanged(const QString &arg1);
    void on_cbLock_clicked(bool checked);
    void on_cbErosion_clicked(bool checked);
    void on_spinBox_valueChanged(int arg1);
    void on_cbAveraging_clicked(bool checked);
    void on_cbPrototypes_clicked(bool checked);
    void on_cbClassifier_currentIndexChanged(int index);
    void on_treeDjbz_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

private:
    void setupThumbView();
    void displayTree(const PageId& page_id);
    void updateItemText(QTreeWidgetItem* item);
    void resetThumbSequence();
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    Ui::DjbzManagerDialog *ui;
    Filter* m_filter;
    PageId const& m_PageId;
    std::unique_ptr<ThumbnailSequence> m_ptrThumbSequence;
    PageSequence m_pageSequence;
    IntrusivePtr<ThumbnailPixmapCache> m_ptrThumbnailCache;
    QSizeF m_maxLogicalThumbSize;
    DjbzDispatcher m_DjbzDispatcher_copy;
    const QIcon m_lockedIcon;
};

}

#endif // DJBZMANAGER_H
