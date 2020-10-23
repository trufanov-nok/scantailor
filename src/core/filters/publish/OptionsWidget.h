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

#ifndef PUBLISH_OPTIONSWIDGET_H_
#define PUBLISH_OPTIONSWIDGET_H_

#include "ui_PublishingOptionsWidget.h"
#include "FilterOptionsWidget.h"
#include "PageId.h"
#include "IntrusivePtr.h"
#include "PageSelectionAccessor.h"

class ProjectPages;
class QDjVuWidget;

namespace publish
{

class Filter;
class Settings;

class OptionsWidget :
    public FilterOptionsWidget, private Ui::PublishingOptionsWidget
{
    Q_OBJECT
public:
    OptionsWidget(Filter* filter, PageSelectionAccessor const& page_selection_accessor);

    virtual ~OptionsWidget();

    void preUpdateUI(const PageId &page_id);

    void postUpdateUI();
    void paintHighlights();
public slots:
    void on_lblDjbzId_linkActivated(const QString &/*link*/);
    void rectSelected(const QPoint &pointerPos, const QRect &rect);
private slots:
    void on_cbScaleMethod_currentIndexChanged(int index);
    void on_sbBSF_valueChanged(int arg1);
    void on_cbSmooth_clicked(bool checked);
    void on_cbErosion_clicked(bool checked);
    void on_cbClean_clicked(bool checked);
    void on_lblTextColorValue_linkActivated(const QString &link);
    void on_lblPageTitleVal_linkActivated(const QString &link);

    void on_lblPageRotationVal_linkActivated(const QString &link);

protected:
    virtual void resizeEvent(QResizeEvent *event) override;
private:
    void showContextMenu(const QPoint &pos, const QRect &rect, bool is_selection = false);
    bool saveImageFile(const QImage& image);
private:

    Filter* m_filter;
    Settings* m_settings;
    QDjVuWidget* m_djvu;
    int m_djvu_mode;
    PageSelectionAccessor m_pageSelectionAccessor;
    PageId m_pageId;
    bool m_delayed_update;
    QString m_recent_folder;
};

} // namespace publish

#endif
