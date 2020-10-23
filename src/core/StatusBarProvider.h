/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C)  Joseph Artsimovich <joseph.artsimovich@gmail.com>

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

#ifndef STATUSBARPROVIDER_H_
#define STATUSBARPROVIDER_H_

#include <QStatusBar>
#include <Dpi.h>
#include <Dpm.h>
#include <QSizeF>
#include <QEvent>
#include <QApplication>
#include <assert.h>

enum StatusLabelPhysSizeDisplayMode {
    Pixels = 0,
    Inch,
    MM,
    SM
};

enum StatusLabelFileSizeDisplayMode {
    Bytes = 0,
    KiB,
    MiB
};

class QStatusBarProviderEvent: public QEvent
{
public:

    enum NitificationTypeEnum {
        PhysSizeChanged = 1,
        MousePosChanged = 2,
        FileSizeChanged = 4
    };

    Q_DECLARE_FLAGS(NitificationType, NitificationTypeEnum)

    QStatusBarProviderEvent(QEvent::Type _t, const QStatusBarProviderEvent::NitificationTypeEnum _type): QEvent(_t), m_type(_type) { }
    void setFlag(const NitificationTypeEnum _type)
    {
        m_type |= _type;
    }
    bool testFlag(const NitificationTypeEnum _type)
    {
        return m_type.testFlag(_type);
    }
private:
    NitificationType m_type;
};

class StatusBarProvider
{
public:

    static void registerStatusBar(QStatusBar* _statusBar)
    {
        m_statusBar = _statusBar;
        assert(m_statusBar);
    }

    static void changeFilterIdx(int idx);
    static void setOutputFilterIdx(int idx)
    {
        m_outputFilterIdx = idx;
        m_pageSize = QSizeF();
        notify();
    }
    static void setPagePhysSize(const QSizeF& _pageSize, const Dpi& _originalDpi);
    static void setSettingsDpi(const Dpi& _settingsDpi)
    {
        m_settingsDpi = _settingsDpi;
        notify();
    }

    static void setMousePos(const QPointF& pos)
    {
        m_mousePos = pos;
        notify(QStatusBarProviderEvent::MousePosChanged);
    }

    static QPointF getMousePos()
    {
        return m_mousePos;
    }
    static QSizeF getPageSize()
    {
        return m_pageSize;
    }
    static Dpi getOriginalDpi()
    {
        return m_originalDpi;
    }
    static Dpi getSettingsDpi()
    {
        return m_settingsDpi;
        notify();
    }

    static void setFileSize( int fileSize ) {
            m_fileSize = fileSize;
            notify(QStatusBarProviderEvent::FileSizeChanged);
        }
    static int getFileSize() { return m_fileSize; }

    static void toggleStatusLabelPhysSizeDisplayMode()
    {
        statusLabelPhysSizeDisplayMode = (statusLabelPhysSizeDisplayMode != SM) ?
                                         (StatusLabelPhysSizeDisplayMode)((int)statusLabelPhysSizeDisplayMode + 1) : Pixels;
    }

    static void toggleStatusLabelFileSizeDisplayMode()
     {
         statusLabelFileSizeDisplayMode = (statusLabelFileSizeDisplayMode != MiB) ?
                     (StatusLabelFileSizeDisplayMode) ((int)statusLabelFileSizeDisplayMode+1) : Bytes;
     }

    static QString getStatusLabelPhysSizeDisplayModeSuffix(StatusLabelPhysSizeDisplayMode const mode)
    {
        switch (mode) {
        case Pixels:
            return QObject::tr("px");
        case Inch:
            return QObject::tr("in");
        case MM:
            return QObject::tr("mm");
        case SM:
            return QObject::tr("cm");
        default:
            return QString();
        }
    }

    static QString getStatusLabelPhysSizeDisplayModeSuffix()
    {
        return getStatusLabelPhysSizeDisplayModeSuffix(statusLabelPhysSizeDisplayMode);
    }

    static QString getStatusLabelFileSizeText(int fsize, StatusLabelFileSizeDisplayMode const mode)
       {
           QString suffix;
           QString sz;

           switch (mode) {
           case Bytes: {
               suffix = QObject::tr("B");
               sz = QString::number(fsize);
           } break;
           case KiB: {
               suffix = QObject::tr("KiB");
               sz = QString::number(fsize/1024., 'f', 2);
           } break;
           case MiB: {
               suffix = QObject::tr("MiB");

               sz = fsize < 0 ? "???" :
                           QString::number(fsize/1024./1024., 'f', 2);
           } break;
           }

           return QObject::tr("%1 %2").arg(sz).arg(suffix);
       }

    static QString getStatusLabelFileSizeText(int * fsize = nullptr) {
        return getStatusLabelFileSizeText(fsize? *fsize : m_fileSize, statusLabelFileSizeDisplayMode);
    }

public:
    static QEvent::Type StatusBarEventType;
    static StatusLabelPhysSizeDisplayMode statusLabelPhysSizeDisplayMode;
    static StatusLabelFileSizeDisplayMode statusLabelFileSizeDisplayMode;
private:
    static void notify(const QStatusBarProviderEvent::NitificationTypeEnum _type = QStatusBarProviderEvent::PhysSizeChanged)
    {
        if (m_statusBar) {
            QApplication::postEvent(m_statusBar, new QStatusBarProviderEvent(StatusBarEventType, _type));
        }
    }

private:
    static QStatusBar* m_statusBar;
    static QSizeF m_pageSize;
    static Dpi m_originalDpi;
    static Dpi m_settingsDpi;
    static int m_fileSize;
    static int m_filterIdx;
    static int m_outputFilterIdx;
    static QPointF m_mousePos;

};

#endif
