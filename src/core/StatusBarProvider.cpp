#include "StatusBarProvider.h"

QStatusBar* StatusBarProvider::m_statusBar = nullptr;
int StatusBarProvider::m_filterIdx = 0;
QEvent::Type StatusBarProvider::StatusBarEventType = static_cast<QEvent::Type>(QEvent::registerEventType());
int StatusBarProvider::m_outputFilterIdx = 0;
StatusLabelPhysSizeDisplayMode StatusBarProvider::statusLabelPhysSizeDisplayMode = Pixels;
StatusLabelFileSizeDisplayMode StatusBarProvider::statusLabelFileSizeDisplayMode = KiB;

QSizeF StatusBarProvider::m_pageSize = QSizeF();
QPointF StatusBarProvider::m_mousePos;
Dpi StatusBarProvider::m_originalDpi = Dpi();
Dpi StatusBarProvider::m_settingsDpi = StatusBarProvider::m_originalDpi;

int StatusBarProvider::m_fileSize = 0;

void
StatusBarProvider::setPagePhysSize(const QSizeF& _pageSize, const Dpi& _originalDpi)
{
    m_pageSize = _pageSize;
    m_originalDpi = _originalDpi;
    notify();
}

void
StatusBarProvider::changeFilterIdx(int idx)
{
    m_filterIdx = idx;
    if (idx != m_outputFilterIdx) {
        m_settingsDpi =  m_originalDpi;
        notify();
    }
}
