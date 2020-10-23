//C-  -*- C++ -*-
//C- -------------------------------------------------------------------
//C- DjView4
//C- Copyright (c) 2006-  Leon Bottou
//C-
//C- This software is subject to, and may be distributed under, the
//C- GNU General Public License, either version 2 of the license,
//C- or (at your option) any later version. The license should have
//C- accompanied the software or you may obtain a copy of the license
//C- from the Free Software Foundation at http://www.fsf.org .
//C-
//C- This program is distributed in the hope that it will be useful,
//C- but WITHOUT ANY WARRANTY; without even the implied warranty of
//C- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//C- GNU General Public License for more details.
//C-  ------------------------------------------------------------------

#if AUTOCONF
# include "config.h"
#endif

#include <stddef.h>
#include <stdlib.h>
#include <math.h>

#include <libdjvu/ddjvuapi.h>
#include <libdjvu/miniexp.h>


#include "qdjvu.h"
#include "qdjvuwidget.h"

#include "settings/globalstaticsettings.h"

#include <QApplication>
#include <QBitmap>
#include <QCursor>
#include <QDebug>
#include <QImage>
#include <QKeyEvent>
#include <QList>
#include <QMap>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPixmap>
#include <QPolygon>
#include <QRect>
#include <QRectF>
#include <QScrollBar>
#include <QTimer>
#include <QToolTip>
#include <QVector>
#include <QWheelEvent>
#include <QWidget>
#if QT_VERSION >= 0x040600
#include <QGesture>
#endif
#ifdef QT_OPENGL_LIB
# include <QGLWidget>
# include <QGLFormat>
#endif

#if DDJVUAPI_VERSION < 17
# error "DDJVUAPI_VERSION>=17 is required !"
#endif

#if DDJVUAPI_VERSION >= 18
# ifndef QDJVUWIDGET_PIXMAP_CACHE
#  define QDJVUWIDGET_PIXMAP_CACHE 1
# endif
#endif

#ifdef Q_NO_USING_KEYWORD
# define BEGIN_ANONYMOUS_NAMESPACE 
# define END_ANONYMOUS_NAMESPACE
# define QRectMapper QDjVu_QRectMapper
# define Page        QDjVu_Page
# define Cache       QDjVu_Cache
# define MapArea     QDjVu_MapArea
# define Keywords    QDjVu_Keywords
# define DragMode    QDjVu_DragMode
#else
# define BEGIN_ANONYMOUS_NAMESPACE namespace {
# define END_ANONYMOUS_NAMESPACE }
#endif

typedef QDjVuWidget::Align Align;
typedef QDjVuWidget::DisplayMode DisplayMode;
typedef QDjVuWidget::Position Position;
typedef QDjVuWidget::PageInfo PageInfo;
typedef QDjVuWidget::Priority Priority;

#define ZOOM_STRETCH      QDjVuWidget::ZOOM_STRETCH
#define ZOOM_ONE2ONE      QDjVuWidget::ZOOM_ONE2ONE
#define ZOOM_FITPAGE      QDjVuWidget::ZOOM_FITPAGE
#define ZOOM_FITWIDTH     QDjVuWidget::ZOOM_FITWIDTH
#define ZOOM_MIN          QDjVuWidget::ZOOM_MIN
#define ZOOM_100          QDjVuWidget::ZOOM_100
#define ZOOM_MAX          QDjVuWidget::ZOOM_MAX

#define DISPLAY_COLOR     QDjVuWidget::DISPLAY_COLOR
#define DISPLAY_STENCIL   QDjVuWidget::DISPLAY_STENCIL
#define DISPLAY_BG        QDjVuWidget::DISPLAY_BG
#define DISPLAY_FG        QDjVuWidget::DISPLAY_FG
#define DISPLAY_TEXT      QDjVuWidget::DISPLAY_TEXT

#define ALIGN_TOP         QDjVuWidget::ALIGN_TOP
#define ALIGN_LEFT        QDjVuWidget::ALIGN_LEFT
#define ALIGN_CENTER      QDjVuWidget::ALIGN_CENTER
#define ALIGN_BOTTOM      QDjVuWidget::ALIGN_BOTTOM
#define ALIGN_RIGHT       QDjVuWidget::ALIGN_RIGHT

#define PRIORITY_DEFAULT  QDjVuWidget::PRIORITY_DEFAULT
#define PRIORITY_ANNO     QDjVuWidget::PRIORITY_ANNO
#define PRIORITY_CGI      QDjVuWidget::PRIORITY_CGI
#define PRIORITY_USER     QDjVuWidget::PRIORITY_USER




// ----------------------------------------
// UTILITIES

static inline void
rect_to_qrect(const ddjvu_rect_t &rect, QRect &qrect)
{
  qrect.setRect(rect.x, rect.y, rect.w, rect.h );
}

static inline void
qrect_to_rect(const QRect &qrect, ddjvu_rect_t &rect, int scale=1)
{
  qrect.getRect(&rect.x, &rect.y, (int*)&rect.w, (int*)&rect.h);
  if (scale > 1) 
    {
      rect.x *= scale;
      rect.y *= scale;
      rect.w *= scale;
      rect.h *= scale;
    }
}

static inline int 
scale_int(int x, int numerator, int denominator)
{
  qint64 y = (qint64)x * numerator;
  return (int)(y + denominator - 1)/denominator;
}

static QSize
scale_size(int w, int h, int numerator, int denominator, int rot=0)
{
  int sw = scale_int(w,numerator,denominator);
  int sh = scale_int(h,numerator,denominator);
  return (rot & 1) ? QSize(sh, sw) : QSize(sw, sh);
}

static bool
all_numbers(const char *s)
{
  for (int i=0; s[i]; i++)
    if (s[i]<'0' || s[i]>'9')
      return false;
  return true;
}

template<class T> static inline int
ksmallest(T *v, int n, int k)
{
  int lo = 0;
  int hi = n-1;
  while (lo<hi)
    {
      int m,l,h;
      T pivot;
      /* Sort v[lo], v[m], v[hi] by insertion */
      m = (lo+hi)/2;
      if (v[lo]>v[m])
        qSwap(v[lo],v[m]);
      if (v[m]>v[hi]) {
        qSwap(v[m],v[hi]);
        if (v[lo]>v[m])
          qSwap(v[lo],v[m]);
      }
      /* Extract pivot, place sentinel */
      pivot = v[m];
      v[m] = v[lo+1];
      v[lo+1] = v[lo];
      v[lo] = pivot;
      /* Partition */
      l = lo;
      h = hi;
     loop:
      do ++l; while (v[l]<pivot);
      do --h; while (v[h]>pivot);
      if (l < h) { 
        qSwap(v[l],v[h]); 
        goto loop; 
      }
      /* Finish up */
      if (k <= h)
        hi = h; 
      else if (k >= l)
        lo = l;
      else
        break;
    }
  return v[k];
}

static QCursor
qcursor_by_name(const char *s, int hotx=8, int hoty=8)
{
  QString name(s);
  QPixmap pixmap(name);
  if (pixmap.isNull()) 
    return QCursor();
  hotx = (hotx * pixmap.width()) / 16;
  hoty = (hoty * pixmap.height()) / 16;
  return QCursor(pixmap, hotx, hoty);
}


#if QT_VERSION > 0x50800
# define foreachrect(r,rgn) foreach(r,rgn)
#else
# define foreachrect(r,rgn) foreach(r,(rgn).rects())
#endif

static QRegion
cover_region(QRegion region, const QRect &brect)
{
  // dilate region
  QRegion dilated;
  foreachrect(const QRect &r, region)
    dilated |= r.adjusted(-8, -8, 8, 8).intersected(brect);
  // find nice cover
  QList<int>   myarea;
  QList<QRect> myrect;
  foreachrect(const QRect &r, dilated)
    {
      int a = r.width() * r.height();
      int j = myrect.size();
      while (--j >= 0)
        {
          int na = myarea[j] + a;
          QRect nr = myrect[j].united(r);
          if (nr.width() * nr.height() < 2 * na )
            {
              myarea[j] = na;
              myrect[j] = nr;
              break;
            }
        }
      if (j < 0)
        {
          myarea << a;
          myrect << r;
        }
    }
  // recompose region
  QRegion ret;
  for (int i=0; i<myrect.size(); i++)
    ret += myrect[i];
  return ret;
}

static qreal max_stay_in_rect(QPointF s, QRectF r, QPointF v)
{
  qreal m = 1e10;
  if (v.x() > 0)
    m = qMin(m, (r.right() - s.x()) / v.x());
  if (v.x() < 0)
    m = qMin(m, (r.left() - s.x()) / v.x());
  if (v.y() > 0)
    m = qMin(m, (r.bottom() - s.y()) / v.y());
  if (v.y() < 0)
    m = qMin(m, (r.top() - s.y()) / v.y());
  return m;
}


static bool
miniexp_get_int(miniexp_t &r, int &x)
{
  if (! miniexp_numberp(miniexp_car(r)))
    return false;
  x = miniexp_to_int(miniexp_car(r));
  r = miniexp_cdr(r);
  return true;
}

static bool
miniexp_get_rect(miniexp_t &r, QRect &rect)
{
  int x, y, w, h;
  if (! (miniexp_get_int(r, x) && miniexp_get_int(r, y) &&
         miniexp_get_int(r, w) && miniexp_get_int(r, h) ))
    return false;
  if (w<0 || h<0 || r)
    return false;
  rect.setRect(x, y, w, h);
  return true;
}

static bool 
miniexp_get_rect_from_points(miniexp_t &r, QRect &rect)
{
  int x1,y1,x2,y2;
  if (! (miniexp_get_int(r, x1) && miniexp_get_int(r, y1) &&
         miniexp_get_int(r, x2) && miniexp_get_int(r, y2) ))
    return false;
  if (x2<x1 || y2<y1)
    return false;
  rect.setCoords(x1, y1, x2, y2);
  return true;
}

static bool
miniexp_get_points(miniexp_t &r, QRect &rect, QList<QPoint> &points)
{
  rect = QRect();
  points.clear();
  while (miniexp_consp(r))
    {
      int x, y;
      if (!miniexp_get_int(r, x) || !miniexp_get_int(r, y))
        return false;
      points << QPoint(x,y);
      rect |= QRect(x,y,1,1);
    }
  if (r || points.size() < 2)
    return false;
  return true;
}

static bool
miniexp_get_color(miniexp_t &r, QColor &color)
{
  const char *s = miniexp_to_name(miniexp_car(r));
  if (! (s && s[0]=='#')) 
    return false;
  color.setNamedColor(QString::fromUtf8(s));
  if (! color.isValid()) 
    return false;
  r = miniexp_cdr(r);
  return true;
}

QString
miniexp_to_qstring(miniexp_t r)
{
  const char *s = miniexp_to_str(r);
  if (s) return QString::fromUtf8(s);
  return QString();
}

void
flatten_hiddentext_sub(miniexp_t p, minivar_t &d)
{
  miniexp_t type = miniexp_car(p);
  miniexp_t r = miniexp_cdr(p);
  QRect rect;
  if (miniexp_symbolp(type) &&
      miniexp_get_rect_from_points(r, rect) )
    {
      if (miniexp_stringp(miniexp_car(r)))
        {
          d = miniexp_cons(p, d);
        }
      else 
        {
          while (miniexp_consp(r))
            {
              flatten_hiddentext_sub(miniexp_car(r), d);
              r = miniexp_cdr(r);
            }
          d = miniexp_cons(type, d);
        }
    }
}

miniexp_t
flatten_hiddentext(miniexp_t p)
{
  // Output list contains
  // - terminals of the hidden text tree
  //     (keyword x1 y1 x2 y2 string)
  // - or a keyword symbol indicating the need for a separator
  //     page,column,region,para,line,word
  minivar_t d;
  flatten_hiddentext_sub(p, d);
  d = miniexp_reverse(d);
  // Uncomment the following line to print it:
  // miniexp_pprint(d, 72);
  return d;
}

void
invert_luminance(QImage &img)
{
  if (img.format() == QImage::Format_RGB32 ||
      img.format() == QImage::Format_ARGB32 )
    {
      int w = img.width();
      int h = img.height();
      for (int y=0; y<h; y++)
        {
          qint32 *words = (qint32*)img.scanLine(y);
          for (int x=0; x<w; x++)
            {
              qint32 word = *words;
              qint32 r = (word >> 16) & 0xff;
              qint32 g = (word >> 8) & 0xff;
              qint32 b = (word >> 0) & 0xff;
              word &= 0xff000000;
              qint32 s = 255 - qMax(r,qMax(g,b)) - qMin(r,qMin(g,b));
              word |= ((r+s)<<16) | ((b+s)<<8) | (g+s);
              *words++ = word;
            }
        }
    }
  else
    {
      // revert to standard inversion.
      img.invertPixels();
    }
}

// ----------------------------------------
// Position


/*! \class QDjVuWidget::Position
  \brief Defines a position in the document.

  Variable \a pageNo indicates the closest page number.

  Flag \a inPage indicates that the position falls inside 
  a page with known geometry. Variable \a posPage then 
  indicates the position within the full resolution 
  page coordinates (same coordinates as those used 
  for the DjVu page annotations). 

  Variable \a posView usually indicates the position 
  relative to, in general, the top-left corner of the page rectangle.
  In fact variables \a hAnchor and \a vAnchor indicate
  the coordinates of the reference point as a percentages
  of the width and height relative to the top left corner.
  These are always zero when the position is returned
  by QDjVuWidget::position. Finally flag \a doPage 
  makes sure that \a pageNo is indeed the closest page. 
  This is always false when the position is returned
  by QDjVuWidget::position. */

Position::Position()
  : pageNo(0), 
    posPage(0,0), 
    posView(0,0),
    inPage(false), 
    doPage(false),
    hAnchor(0),
    vAnchor(0)
{
}



// ----------------------------------------
// QRECTMAPPER


BEGIN_ANONYMOUS_NAMESPACE 

class QRectMapper
{
private:
  ddjvu_rectmapper_t *p;
public:
  ~QRectMapper();
  QRectMapper();
  QRectMapper(const QRect &in, const QRect &out);
  void setMap(const QRect &in, const QRect &out);
  void setTransform(int rotation, bool mirrorx=false, bool mirrory=false);
  QPoint mapped(const QPoint &p);
  QRect mapped(const QRect &r);
  QPoint unMapped(const QPoint &p);
  QRect unMapped(const QRect &r);
};

QRectMapper::~QRectMapper()
{
  ddjvu_rectmapper_release(p);
  p = 0;
}

QRectMapper::QRectMapper() 
  : p(0)
{
}

QRectMapper::QRectMapper(const QRect &in, const QRect &out)
  : p(0)
{
  setMap(in, out);
}

void 
QRectMapper::setMap(const QRect &in, const QRect &out)
{
  ddjvu_rect_t rin, rout;
  ddjvu_rectmapper_release(p);
  p = 0;
  qrect_to_rect(in, rin);
  qrect_to_rect(out, rout);
  p = ddjvu_rectmapper_create(&rin, &rout);
}

void 
QRectMapper::setTransform(int rotation, bool mirrorx, bool mirrory)
{
  if (!p) qWarning("QRectMapper: please call setMap first.");
  ddjvu_rectmapper_modify(p, rotation, (mirrorx?1:0), (mirrory?1:0));
}

QPoint
QRectMapper::mapped(const QPoint &qp)
{
  QPoint q = qp;
  if (p) ddjvu_map_point(p, &q.rx(), &q.ry());
  return q;
}

QRect
QRectMapper::mapped(const QRect &qr)
{
  QRect q;
  ddjvu_rect_t r;
  qrect_to_rect(qr, r);
  if (p) ddjvu_map_rect(p, &r);
  rect_to_qrect(r, q);
  return q;
}

QPoint
QRectMapper::unMapped(const QPoint &qp)
{
  QPoint q = qp;
  if (p) ddjvu_unmap_point(p, &q.rx(), &q.ry());
  return q;
}

QRect
QRectMapper::unMapped(const QRect &qr)
{
  QRect q;
  ddjvu_rect_t r;
  qrect_to_rect(qr, r);
  if (p) ddjvu_unmap_rect(p, &r);
  rect_to_qrect(r, q);
  return q;
}

END_ANONYMOUS_NAMESPACE


// ----------------------------------------
// PRIORITIZED SETTINGS

// Some settings can be set/unset from several sources

BEGIN_ANONYMOUS_NAMESPACE 

template<class Value>
class Prioritized
{
public:
  Prioritized();
  operator Value() const;
  void set(Priority priority, Value value);
  void unset(Priority priority);
  void reduce(Priority priority);
private:
  bool   flags[PRIORITY_USER+1];
  Value  values[PRIORITY_USER+1];
};

template<class Value>
Prioritized<Value>::Prioritized()
{
  for (int i=0; i<=PRIORITY_USER; i++)
    flags[i] = false;
}

template<class Value>
Prioritized<Value>::operator Value() const
{
  for (int i=PRIORITY_USER; i>0; i--)
    if (flags[i]) 
      return values[i];
  return values[PRIORITY_DEFAULT];
}

template<class Value> void
Prioritized<Value>::set(Priority priority, Value val)
{
  values[priority] = val;
  flags[priority] = true;
}

template<class Value> void
Prioritized<Value>::unset(Priority priority)
{
  flags[priority] = false;
}

template<class Value> void
Prioritized<Value>::reduce(Priority priority)
{
  for (int i=PRIORITY_USER; i>priority; i--)
    if (flags[i])
      {
        flags[i-1] = flags[i];
        values[i-1] = values[i];
        flags[i] = false;
      }
}

END_ANONYMOUS_NAMESPACE


// ----------------------------------------
// QDJVULENS (declaration)


class QDjVuLens : public QWidget
{
  Q_OBJECT
public:
  QDjVuLens(int size, int mag, QDjVuPrivate *priv, QDjVuWidget *widget);
protected:
  virtual bool event(QEvent *event);
  virtual void paintEvent(QPaintEvent *event);
  virtual void moveEvent(QMoveEvent *event);
  virtual void resizeEvent(QResizeEvent *event);
  virtual bool eventFilter(QObject*, QEvent*);
private:
  QDjVuPrivate *priv;
  QDjVuWidget *widget;
  QRect lensRect;
  int mag;
public slots:
  void recenter(const QPoint &p);
  void redisplay();
private:
  void refocus();
};



// ----------------------------------------
// QDJVUPRIVATE


BEGIN_ANONYMOUS_NAMESPACE 

struct MapArea
{
  minivar_t expr;
  miniexp_t url;
  miniexp_t target;
  miniexp_t comment;
  miniexp_t     areaType;
  QRect         areaRect;
  QList<QPoint> areaPoints;
  miniexp_t     borderType;
  QColor        borderColor;
  QColor        hiliteColor;
  QColor        foregroundColor;
  unsigned char borderWidth;
  bool          borderAlwaysVisible;
  unsigned char hiliteOpacity;
  bool          pushpin;
  bool          lineArrow;
  unsigned char lineWidth;
  bool          rectNeedsRotation;

  MapArea();
  bool error(const char *err, int pageno, miniexp_t info);
  bool parse(miniexp_t anno, int pageno=-1);
  bool isClickable(bool hyperlink=true);
  bool clicked(void);
  bool hasTransient(bool allLinks=false);
  bool hasPermanent();
  bool contains(const QPoint &p);
  void maybeRotate(struct Page *p);
  QPainterPath contour(QRectMapper &m, QPoint &offset);
  void update(QWidget *w, QRectMapper &m, QPoint offset, bool permanent=false);
  void paintBorder(QPaintDevice *w, QRectMapper &m, QPoint offset, bool allLinks=false);
  void paintPermanent(QPaintDevice *w, QRectMapper &m, QPoint o, double z=100);
  void paintTransient(QPaintDevice *w, QRectMapper &m, QPoint o, bool allLinks=false);
};

struct Page
{
  int            pageno;
  int            width;         // page coordinates
  int            height;        // page coordinates
  int            dpi;           // zero to indicate unknown size
  QRect          rect;          // desk coordinates
  QRect          viewRect;      // viewport coordinates
  QRectMapper    mapper;        // from page to desk coordinates
  QDjVuPage     *page;          // the page decoder
  minivar_t      annotations;   // the djvu page annotations
  minivar_t      hiddenText;    // the hidden djvu page text
  int            initialRot;    // initial rotation (used for text)
  QList<MapArea> mapAreas;      // the list of mapareas
  bool           redisplay;     // must redisplay
  bool           infoNeeded;    // we want the page info now.
  
  void clear() { delete page; page=0; } 
  ~Page()      { clear(); };
  Page()       : pageno(-1),width(0),height(0),dpi(0),page(0),
                 annotations(miniexp_dummy),hiddenText(miniexp_dummy),
                 initialRot(-1),redisplay(false),
                 infoNeeded(false) { clear(); }
};

struct Cache
{
  QRect    rect;   // desk coordinates
  QImage   image;
#if QDJVUWIDGET_PIXMAP_CACHE
  QPixmap  pixmap;
#endif
};

enum {
  CHANGE_STATS        = 0x1,    // recompute estimated page sizes
  CHANGE_PAGES        = 0x2,    // change the displayed pages
  CHANGE_SIZE         = 0x4,    // update the recorded size of displayed pages
  CHANGE_SCALE        = 0x8,    // update the scale of displayed pages
  CHANGE_POSITIONS    = 0x10,   // update the disposition of the pages
  CHANGE_SCALE_PASS2  = 0x20,   // internal
  CHANGE_VIEW         = 0x40,   // move to a new position
  CHANGE_VISIBLE      = 0x80,   // update the visible rectangle
  CHANGE_SCROLLBARS   = 0x100,  // update the scrollbars
  UPDATE_BORDERS      = 0x200,  // redisplay the visible desk surface
  UPDATE_PAGES        = 0x400,  // redisplay, possibly by scrolling smartly
  UPDATE_ALL          = 0x800,  // redisplay everything
  REFRESH_PAGES       = 0x1000, // redraw pages with new pixels
  SCHEDULED           = 0x8000  // internal: change scheduled
};

enum DragMode {
  DRAG_NONE,
  DRAG_PANNING,
  DRAG_LENSING,
  DRAG_SELECTING,
  DRAG_LINKING
};

END_ANONYMOUS_NAMESPACE

class QDjVuPrivate : public QObject
{
  Q_OBJECT

private:
  virtual ~QDjVuPrivate();
  QDjVuPrivate(QDjVuWidget *widget);
  friend class QDjVuWidget;
  friend class QDjVuLens;

public:
  QDjVuWidget * const widget;   // the widget

  QDjVuDocument  *doc;
  bool docFailed;               // page decoding has failed
  bool docStopped;              // page decoding has stopped
  bool docReady;                // page decoding is done
  int  numPages;                // total number of pages
  Position currentPos;          // last clicked position
  QPoint   currentPoint;        // last clicked point
  Position cursorPos;           // cursor position
  QPoint   cursorPoint;         // cursor point
  // display 
  int         zoom;
  int         rotation;
  DisplayMode display;
  Align       hAlign;
  Align       vAlign;
  bool        frame;
  bool        continuous;
  bool        sideBySide;
  bool        coverPage;
  bool        rightToLeft;
  QBrush      borderBrush;
  // layout
  int          layoutChange;    // scheduled changes
  int          layoutLoop;      // loop avoidance counter
  QPoint       movePoint;       // desired viewport point (CHANGE_VIEW)
  Position     movePos;         // desired page point (CHANGE_VIEW)
  QVector<Page>   pageData;     // all pages
  QList<Page*>    pageLayout;   // pages in current layout
  QList<Page*>    pageVisible;  // pages visible in viewport
  QMap<int,Page*> pageMap;      // keyed by page number
  QRect           deskRect;     // desk rectangle (0,0,w,h)
  QRect           visibleRect;  // visible rectangle (desk coordinates)
  int             zoomFactor;   // effective zoom level
  int             savedFactor;  // temporary factor used for gestures
  int        estimatedWidth;    // median of known page widths
  int        estimatedHeight;   // median of known page heights
  QSize    unknownSize;         // displayed page size when unknown (pixels)
  int      borderSize;          // size of borders around pages (pixels)
  int      separatorSize;       // size of separation between pages (pixels)
  int      shadowSize;          // size of the page shadow (pixels)
  // page requests
  int      pageRequestDelay;    // delay when scrollbars are down
  QTimer  *pageRequestTimer;    // timer for page requests
  // painting
  int         pixelCacheSize;   // size of pixel cache
  QRect       selectedRect;     // selection rectangle (viewport coord)
  double      gamma;            // display gamma
  bool        invertLuminance;  // invert images
  QColor      white;            // display white point
  int         sdpi;             // screen dpi
  ddjvu_format_t *renderFormat; // ddjvu format
  QList<Cache>    pixelCache;   // pixel cache
  QList<Cache>    pixelCacheAlt;// pixel cache for alternative image
  int         devicePixelRatio; // device pixel ratio for cached images
  // gui
  bool        keyboardEnabled;  // obey keyboard commands
  bool        mouseEnabled;     // obey mouse commands
  bool        hyperlinkEnabled; // follow hyperlinks
  bool        animationEnabled; // animate position changes
  bool        changingSBars;    // set while changing scrollbars
  bool        mouseWheelZoom;   // mouse wheel function
  DragMode    dragMode;         // dragging mode
  QPoint      dragStart;        // starting point for dragging
  QMenu*      contextMenu;      // popup menu
  QDjVuLens  *lens;             // lens (only while lensing)
  int         lensMag;          // lens maginification
  int         lensSize;         // lens size
  double      hourGlassRatio;   // hour glass timer ratio
  int         lineStep;         // pixels moved by arrow keys
  Qt::MouseButtons      buttons;            // current mouse buttons
  Qt::KeyboardModifiers modifiers;          // current modifiers
  Qt::KeyboardModifiers modifiersForLinks;  // keys to show hyperlink
  Qt::KeyboardModifiers modifiersForLens;   // keys to show lens
  Qt::KeyboardModifiers modifiersForSelect; // keys to select
  QList<Position> animationPosition; // list of positions to hit
  QPoint animationPoint;             // reference point
  QTimer *animationTimer;            // animation timer

  // prioritized settings
  Prioritized<int>         qBorderSize;
  Prioritized<int>         qZoom;
  Prioritized<QBrush>      qBorderBrush;
  Prioritized<DisplayMode> qDisplay;
  Prioritized<Align>       qHAlign;
  Prioritized<Align>       qVAlign;
  // hyperlinks
  bool        displayMapAreas;
  Page       *currentMapAreaPage;
  MapArea    *currentMapArea;
  QString     currentUrl;
  QString     currentTarget;
  QString     currentComment;
  bool        currentLinkDisplayed;
  bool        allLinksDisplayed;
  QTimer     *toolTipTimer;
  // messages
  int maxMessages;
  QList<QString> errorMessages;
  QList<QString> infoMessages;
  // cursors
  QCursor cursHandOpen;
  QCursor cursHandClosed;

  void initWidget(bool noaccel);
  void changeLayout(int change, int delay=0);
  void getAnnotationsAndText(Page *p);
  bool requestPage(Page *p);
  int findClosestPage(const QPoint&, const QList<Page*>&, Page**, int*);
  Position findPosition(const QPoint &point, bool closestAnchor=false);
  void updateModifiers(Qt::KeyboardModifiers, Qt::MouseButtons);
  void updatePosition(const QPoint &point, bool click=false, bool links=true);
  void updateCurrentPoint(const Position &pos);
  void adjustSettings(Priority priority, miniexp_t expr);
  void prepareMapAreas(Page*);
  bool mustDisplayMapArea(MapArea*);
  void checkCurrentMapArea(bool forceno=false);
  void showTransientMapAreas(bool);
  void updateTransientMapArea(Page*, MapArea*);
  void trimPixelCache();
  void addToPixelCache(const QRect &rect, QImage image);
  bool paintHiddenText(QImage&, Page*, const QRect&, const QRect *pr=0);
  bool paintMapAreas(QImage&, Page*, const QRect&, bool, const QRect *pr=0);
  bool paintPage(QPainter&, Page*, const QRegion&);
  void paintAll(QPainter&, const QRegion&);
  bool pointerScroll(const QPoint &p);
  void changeSelectedRectangle(const QRect &rect);
  bool eventFilter(QObject*, QEvent*);
  void clearAnimation(void);
  bool computeAnimation(const Position&, const QPoint&);
  void changeBorderSize(void);
  void changeZoom(void);
  void changeBorderBrush(void);
  void changeDisplay(void);
  void changeHAlign(void);
  void changeVAlign(void);
  QRect hourGlassRect(void) const;
  void pageinfoPage(QDjVuPage*);

public slots:
  void makeLayout();
  void makePageRequests();
  void docinfo();
  void pageinfo();
  void pageinfoPage();
  void redisplayPage();
  void error(QString msg, QString filename, int lineno);
  void info(QString msg);
  void makeToolTip();
  void animate();

private:
  QString alternativeImageFilename;
  std::unique_ptr<QImage> alternativeImagePtr;
  bool displayAlternative;
};

QDjVuPrivate::~QDjVuPrivate()
{
  if (doc) 
    doc->deref();
  doc = 0;
  if (renderFormat)
    ddjvu_format_release(renderFormat);
  renderFormat = 0;
}

QDjVuPrivate::QDjVuPrivate(QDjVuWidget *widget)
  : widget(widget)
{
  // doc
  doc = 0;
  docReady = false;
  docStopped = false;
  docFailed = false;
  numPages = 1;
  // aspect
  zoom = 100;
  rotation = 0;
  frame = true;
  display = DISPLAY_COLOR;
  hAlign = ALIGN_CENTER;
  vAlign = ALIGN_CENTER;
  sideBySide = false;
  continuous = false;
  coverPage = false;
  rightToLeft = false;
  borderBrush = QBrush(Qt::lightGray);
  unknownSize = QSize(128,128);
  borderSize = 8;
  separatorSize = 12;
  shadowSize = 2;
  // prioritized
  qBorderSize.set(PRIORITY_DEFAULT, borderSize);
  qZoom.set(PRIORITY_DEFAULT, zoom);
  qBorderBrush.set(PRIORITY_DEFAULT, borderBrush);
  qDisplay.set(PRIORITY_DEFAULT, display);
  qHAlign.set(PRIORITY_DEFAULT, hAlign);
  qVAlign.set(PRIORITY_DEFAULT, vAlign);
  // gui
  contextMenu = 0;
  keyboardEnabled = true;
  hyperlinkEnabled = true;
  mouseEnabled = true;
  animationEnabled = true;
  lineStep = 12;
  buttons = (Qt::MouseButtons)(-1);
  modifiers = (Qt::KeyboardModifiers)(-1);
  modifiersForLinks = Qt::ShiftModifier;
  modifiersForLens = Qt::ShiftModifier|Qt::ControlModifier;
  modifiersForSelect = Qt::ControlModifier;
  changingSBars = false;
  dragMode = DRAG_NONE;
  lens = 0;
  lensMag = 3;
  lensSize = 300;
  hourGlassRatio = 0;
  animationTimer = new QTimer(this);
  animationTimer->setSingleShot(false);
  connect(animationTimer, SIGNAL(timeout()), this, SLOT(animate()));
  // scheduled changes
  layoutChange = 0;
  layoutLoop = 0;
  pageRequestDelay = 250;
  pageRequestTimer = new QTimer(this);
  pageRequestTimer->setSingleShot(true);
  // render format
  gamma = 2.2;
  invertLuminance = false;
  white = QColor(Qt::white);
  sdpi = 100;
#if DDJVUAPI_VERSION < 18
  unsigned int masks[3] = { 0xff0000, 0xff00, 0xff };
  renderFormat = ddjvu_format_create(DDJVU_FORMAT_RGBMASK32, 3, masks);
#else
  unsigned int masks[4] = { 0xff0000, 0xff00, 0xff, 0xff000000 };
  renderFormat = ddjvu_format_create(DDJVU_FORMAT_RGBMASK32, 4, masks);
#endif
  ddjvu_format_set_row_order(renderFormat, true);
  ddjvu_format_set_y_direction(renderFormat, true);
  ddjvu_format_set_ditherbits(renderFormat, QPixmap::defaultDepth());
  ddjvu_format_set_gamma(renderFormat, gamma);
  // misc
  maxMessages = 20;
  displayMapAreas = true;
  currentMapAreaPage = 0;
  currentMapArea = 0;
  currentLinkDisplayed = false;
  allLinksDisplayed = false;
  pixelCacheSize = 256 * 1024;
  devicePixelRatio = 1;
  // cursors
  cursHandOpen = qcursor_by_name(":/images/cursor_hand_open.png");
  cursHandClosed = qcursor_by_name(":/images/cursor_hand_closed.png");
  // tooltips
  toolTipTimer = new QTimer(this);
  toolTipTimer->setSingleShot(true);
  // connect
  connect(pageRequestTimer, SIGNAL(timeout()),
          this, SLOT(makePageRequests()) );
  connect(toolTipTimer, SIGNAL(timeout()),
          this, SLOT(makeToolTip()) );
  connect(widget->horizontalScrollBar(), SIGNAL(sliderReleased()),
          this, SLOT(makePageRequests()) );
  connect(widget->verticalScrollBar(), SIGNAL(sliderReleased()),
          this, SLOT(makePageRequests()) );
  displayAlternative = false;
}



// ----------------------------------------
// LAYOUT


// Schedule a layout recomputation.
void
QDjVuPrivate::changeLayout(int change, int delay)
{
  int oldChange = layoutChange;
  layoutChange = oldChange | change | SCHEDULED;
  layoutLoop = 0;
  if (! (oldChange & SCHEDULED))
    QTimer::singleShot(delay, this, SLOT(makeLayout()));
}

// Perform a scheduled layout recomputation.
void
QDjVuPrivate::makeLayout()
{
  // save position
  int futureLayoutChange = 0;
  if (docReady && !(layoutChange & CHANGE_VIEW))
    {
      movePoint = currentPoint;
      movePos = currentPos;
    }
  // adjust layout information
  while (layoutChange && docReady)
    {
      // Recompute estimated page sizes
      if (layoutChange & CHANGE_STATS)
        {
          int k = 0;
          QVector<int> widthVec(numPages+1);
          QVector<int> heightVec(numPages+1);
          layoutChange &= ~CHANGE_STATS;
          futureLayoutChange |= CHANGE_STATS;
          for (int i=0; i<numPages; i++)
            if (pageData[i].dpi > 0)
              {
                int dpi = pageData[i].dpi;
                widthVec[k] = pageData[i].width * 100 / dpi;
                heightVec[k] = pageData[i].height * 100 / dpi;
                k += 1;
              }
          if (k > 0)
            {
              futureLayoutChange &= ~CHANGE_STATS;
              estimatedWidth = ksmallest(widthVec.data(), k, k/2);
              estimatedHeight = ksmallest(heightVec.data(), k, k/2);
              for (int i=0; i<numPages; i++)
                if (pageData[i].dpi <= 0)
                  {
                    pageData[i].width = estimatedWidth;
                    pageData[i].height = estimatedHeight;
                  }
            }
        }
      // Layout composition
      else if (layoutChange & CHANGE_PAGES)
        {
          layoutChange &= ~CHANGE_PAGES;
          layoutChange |= CHANGE_SIZE;
          layoutChange |= CHANGE_SCALE;
          pageLayout.clear();
          pageMap.clear();
          if (movePos.pageNo<0 || movePos.pageNo>=numPages)
            {
              qWarning("makeLayout: invalid page number");
              movePos.pageNo = widget->page();
            }
          int loPage = movePos.pageNo;
          int hiPage = qMin(numPages, loPage+1);
          if (continuous)
            {
              loPage = 0;
              hiPage = numPages;
            }
          else if (sideBySide && coverPage)
            {
              loPage = qMax(0, (loPage & 1) ? loPage : loPage-1);
              hiPage = qMin(numPages, (loPage & 1) ? loPage+2 : loPage+1);
            }
          else if (sideBySide)
            {
              loPage = qMax(0, (loPage & 1) ? loPage-1 : loPage);
              hiPage = qMin(numPages, loPage+2);
            }
          for (int i=loPage; i<hiPage; i++)
            {
              pageData[i].viewRect = QRect();
              pageLayout << &pageData[i];
              pageMap[i] = &pageData[i];
            }
          for(int i=0; i<numPages; i++)
            if (i < loPage - 2 && i >= hiPage + 2)
              pageData[i].clear();
          // Display settings are unclear when showing multiple page.
          // We use the annotations of the first displayed page as a proxy.
          adjustSettings(PRIORITY_ANNO, pageLayout[0]->annotations);
        }
      // Layout page sizes
      else if (layoutChange & CHANGE_SIZE)
        {
          layoutChange &= ~CHANGE_SIZE;
          Page *p;
          foreach(p, pageLayout) 
            {
              int n = p->pageno;
              if (! p->infoNeeded)
                continue;
              if (p->dpi <= 0)
                {
                  ddjvu_pageinfo_t info;
                  ddjvu_status_t status;
                  status = ddjvu_document_get_pageinfo(*doc, n, &info); 
                  if (status == DDJVU_JOB_OK)
                    if (info.dpi > 0 && info.width > 0 && info.height > 0)
                      {
                        p->dpi = info.dpi;
                        p->width = info.width;
                        p->height = info.height;
#if DDJVUAPI_VERSION >= 18
                        p->initialRot = info.rotation;
#endif
                      }
                  layoutChange |= CHANGE_SCALE;
                  layoutChange |= UPDATE_BORDERS;
                }
              getAnnotationsAndText(p);
            }
        }
      // Layout scaled page size
      else if (layoutChange & CHANGE_SCALE)
        {
          // some setups require two passes
          layoutChange &= ~CHANGE_SCALE;
          layoutChange &= ~CHANGE_SCALE_PASS2;
          layoutChange |= CHANGE_POSITIONS;
          if (pageLayout.size() > 1)
            if (zoom==ZOOM_FITWIDTH || zoom==ZOOM_FITPAGE)
              layoutChange |= CHANGE_SCALE_PASS2;
          // rescale pages
          Page *p;
          int r = rotation;
          int vpw = qMax(64, widget->viewport()->width() - 2*borderSize);
          int vph = qMax(64, widget->viewport()->height() - 2*borderSize);
          zoomFactor = zoom;
          foreach(p, pageLayout)
            {
              QSize size;
              if (p->width <= 0 || p->height <= 0) {
                size = unknownSize;
              } else if (p->dpi <= 0) {
                size = (zoom <= 0) ? unknownSize :
                  scale_size(p->width, p->height, zoom*sdpi, 10000, r);
              } else if (layoutChange & CHANGE_SCALE_PASS2) {
                size = scale_size(p->width, p->height, sdpi, p->dpi, r);
              } else if (zoom == ZOOM_ONE2ONE) {
                size = scale_size(p->width, p->height, p->dpi, p->dpi, r);
              } else if (zoom == ZOOM_STRETCH) {
                int v2pw = (sideBySide) ? (vpw-separatorSize)/2 : vpw;
                size = QSize(v2pw, vph);
              } else if (zoom == ZOOM_FITWIDTH) {
                int pgw = (r&1) ? p->height : p->width;
                size = scale_size(p->width, p->height, vpw, pgw, r);
                zoomFactor = (vpw * p->dpi * 100) / (pgw * sdpi);
              } else if (zoom == ZOOM_FITPAGE) {
                int pgw = (r&1) ? p->height : p->width;
                int pgh = (r&1) ? p->width : p->height;
                if (vpw*pgh > vph*pgw) 
                  { vpw = vph; pgw = pgh; }
                size = scale_size(p->width, p->height, vpw, pgw, r);
                zoomFactor = (vpw * p->dpi * 100) / (pgw * sdpi);
              } else
                size = scale_size(p->width, p->height, zoom*sdpi,100*p->dpi,r);
              p->rect.setSize(size);
            }
        }
      // Layout page positions
      else if (layoutChange & CHANGE_POSITIONS)
        {
          layoutChange |= CHANGE_VIEW;
          layoutChange |= CHANGE_SCROLLBARS;
          layoutChange &= ~CHANGE_POSITIONS;
          // position pages
          Page *p;
          QRect fullRect;
          if (! continuous)
            {
              // single page or side by side pages
              int x = borderSize;
              int s = pageLayout.size();
              for (int i=0; i<s; i++)
                {
                  if (rightToLeft)
                    p = pageLayout[s-i-1];
                  else
                    p = pageLayout[i];
                  p->rect.moveTo(x,borderSize);
                  fullRect |= p->rect;
                  x = x + p->rect.width() + separatorSize;
                }
              foreach(p, pageLayout)
                {
                  int dy = fullRect.height() - p->rect.height();
                  if (vAlign == ALIGN_CENTER)
                    p->rect.translate(0, dy/2);
                  else if (vAlign == ALIGN_BOTTOM)
                    p->rect.translate(0, dy);
                }
            }
          else if (!sideBySide)
            {
              // continuous single pages
              int y = borderSize;
              foreach(p, pageLayout)
                {
                  p->rect.moveTo(borderSize, y);
                  y = y + p->rect.height() + separatorSize;
                  fullRect |= p->rect;
                }
              foreach(p, pageLayout)
                {
                  int dx = fullRect.width() - p->rect.width();
                  if (hAlign == ALIGN_CENTER)
                    p->rect.translate(dx/2, 0);
                  else if (hAlign == ALIGN_RIGHT)
                    p->rect.translate(dx,0);
                }
            }
          else
            {
              // continuous side-by-side pages
              int wLeft = 0;
              int wTotal = 0;
              int k = 0;
              while(k < pageLayout.size())
                {
                  Page *lp = pageLayout[k++];
                  Page *rp = lp;
                  if (k < pageLayout.size() && (k>1 || !coverPage))
                    rp = pageLayout[k++];
                  if (rightToLeft)
                    { Page *tmp=lp; lp=rp; rp=tmp; }
                  int lw = lp->rect.width();
                  wLeft = qMax(wLeft, lw);
                  wTotal = qMax(wTotal, lw);
                  if (rp)
                    wTotal = qMax(wTotal, lw+rp->rect.width()+separatorSize);
                }
              int y = borderSize;
              k = 0;
              while(k < pageLayout.size())
                {
                  Page *lp = pageLayout[k++];
                  Page *rp = lp;
                  if (k < pageLayout.size() && (k > 1 || !coverPage))
                    rp = pageLayout[k++];
                  if (rightToLeft)
                    { Page *tmp=lp; lp=rp; rp=tmp; }
                  int h = qMax(lp->rect.height(), rp->rect.height());
                  int ly, ry, lx, rx;
                  ly = ry = y;
                  if (vAlign==ALIGN_BOTTOM) {
                    ly += (h - lp->rect.height());
                    ry += (h - rp->rect.height());
                  } else if (vAlign==ALIGN_CENTER) {
                    ly += (h - lp->rect.height())/2;
                    ry += (h - rp->rect.height())/2;
                  }
                  if (hAlign==ALIGN_LEFT) {
                    lx = borderSize;
                    rx = lx + lp->rect.width() + separatorSize;
                  } else if (hAlign==ALIGN_RIGHT) {
                    if (rp == lp)
                      rx = wTotal + borderSize + separatorSize;
                    else
                      rx = wTotal + borderSize - rp->rect.width();
                    lx = rx - separatorSize - lp->rect.width();
                  } else if (lp == rp) {
                    lx = borderSize+wLeft+(separatorSize-lp->rect.width())/2;
                    rx = lx;
                  } else {
                    lx = borderSize + wLeft - lp->rect.width();
                    rx = borderSize + wLeft + separatorSize;
                  } 
                  lp->rect.moveTo(lx, ly);
                  if (rp != lp)
                    rp->rect.moveTo(rx, ry);
                  fullRect |= lp->rect;
                  fullRect |= rp->rect;
                  y = y + h + separatorSize;
                }
            }
          // setup rectangles
          fullRect.adjust(-borderSize, -borderSize, 
                          +borderSize, +borderSize );
          deskRect = fullRect;
          // setup mappers
          foreach(p, pageLayout)
            {
              int w = (p->width>0) ? p->width : unknownSize.width();
              int h = (p->height>0) ? p->height : unknownSize.height();
              p->mapper.setMap(QRect(0,0,w,h), p->rect);
              p->mapper.setTransform(rotation, false, true);
            }
          // clear pixel cache
          pixelCache.clear();
        }
      // Layout page positions
      else if (layoutChange & CHANGE_SCALE_PASS2)
        {
          Page *p;
          // compute usable viewport size (minus margins and separators)
          int vw = widget->viewport()->width() - 2*borderSize - 2;
          int vh = widget->viewport()->height() - 2*borderSize - 2;
          if (sideBySide && pageLayout.size()>1)
            vw = vw - separatorSize;
          vw = qMax(64, vw);
          vh = qMax(64, vh);
          // compute target size for zoom 100%
          int dw = deskRect.width() - 2*borderSize;
          int dh = deskRect.height() - 2*borderSize;
          if (sideBySide && pageLayout.size()>1)
            dw = dw - separatorSize;
          if (continuous)
            { dh = 0; foreach(p, pageLayout) dh = qMax(dh, p->rect.height()); }
          // compute zoom factor into vw/dw
          if (zoom == ZOOM_FITPAGE && vw*dh > vh*dw) 
            { vw = vh; dw = dh; }
          zoomFactor = vw * 100 / dw;
          // apply zoom
          int r = rotation;
          foreach(p, pageLayout)
            {
              QSize size = unknownSize;
              int dpi = (p->dpi > 0) ? p->dpi : 100;
              if (p->width>0 && p->height>0)
                size = scale_size(p->width, p->height, vw * sdpi, dw * dpi, r);
              p->rect.setSize(size);
            }
          // prepare further adjustments
          layoutChange |= CHANGE_POSITIONS;
          layoutChange &= ~CHANGE_SCALE_PASS2;
        }
      // Viewport adjustment
      else if (layoutChange & CHANGE_VIEW)
        {
          if (pageMap.contains(movePos.pageNo))
            {
              Page *p = pageMap[movePos.pageNo];
              QPoint dp = p->rect.topLeft();
              if (movePos.hAnchor > 0 && movePos.hAnchor <= 100)
                dp.rx() += (p->rect.width() - 1) * movePos.hAnchor / 100;
              if (movePos.vAnchor > 0 && movePos.vAnchor <= 100)
                dp.ry() += (p->rect.height() - 1) * movePos.vAnchor / 100;
              QPoint sdp = dp;
              if (movePos.inPage && p->dpi)
                dp =  p->mapper.mapped(movePos.posPage);
              else if (!movePos.inPage)
                dp += movePos.posView;
              // special case: unknown page geometry
              if (! p->dpi)
                if (movePos.inPage || movePos.vAnchor || movePos.hAnchor)
                  futureLayoutChange = CHANGE_VIEW;
              // special case: not the closest page
              if (movePos.doPage)
                if (movePos.pageNo != findClosestPage(dp, pageLayout, 0, 0))
                  dp = sdp;
              // perform move
              visibleRect.moveTo(dp - movePoint);
            }
          layoutChange |= CHANGE_VISIBLE;
          layoutChange &= ~CHANGE_VIEW;
        }
      // Layout page visibility adjustment
      else if (layoutChange & CHANGE_VISIBLE)
        {
          Page *p;
          QSize vpSize = widget->viewport()->size();
          QRect &rv = visibleRect;
          QRect &rd = deskRect;
          // adjust size
          rv.setSize(vpSize);
          // reposition rectVisible according to alignment.
          rv.moveRight(qMin(rv.right(), rd.right()));
          rv.moveLeft(qMax(rv.left(), rd.left()));
          rv.moveBottom(qMin(rv.bottom(), rd.bottom()));
          rv.moveTop(qMax(rv.top(), rd.top()));
          if (hAlign == ALIGN_RIGHT)
            rv.moveRight(qMin(rv.right(), rd.right()));
          if (vAlign == ALIGN_BOTTOM)
            rv.moveBottom(qMin(rv.bottom(), rd.bottom()));
          QPoint cd = rd.center();
          QPoint cv = rv.center();
          if (hAlign==ALIGN_CENTER && rv.width()>=rd.width())
            cv.setX(cd.x());
          if (vAlign==ALIGN_CENTER && rv.height()>=rd.height())
            cv.setY(cd.y());
          rv.moveCenter(cv);
          // delete QDjVuPage that are too far away
          int sw = rv.width();
          int sh = rv.height();
          QRect v = rv.adjusted(-sw, -sh, +sw, +sh);
          foreach(p, pageLayout)
            if (p->page && !v.intersects(p->rect)) 
              p->clear();
          // construct pageVisible array and schedule page requests
          pageVisible.clear();
          foreach(p, pageLayout)
            if (rv.intersects(p->rect)) 
              pageVisible.append(p);
          // schedule page requests
          QScrollBar *hBar = widget->horizontalScrollBar();
          QScrollBar *vBar = widget->verticalScrollBar();
          if (hBar->isSliderDown() || vBar->isSliderDown())
            pageRequestTimer->start(pageRequestDelay);
          else
            pageRequestTimer->start(0);
          // synchronize currentpoint and cursorpos
          updateCurrentPoint(movePos);
          updatePosition(cursorPoint, false, false);
          // continue
          layoutChange |= UPDATE_PAGES;
          layoutChange &= ~CHANGE_VISIBLE;
        }
      // scrollbar adjustment
      else if (layoutChange & CHANGE_SCROLLBARS)
        {
          QRect &rv = visibleRect;
          QRect rd = deskRect.united(rv);
          QScrollBar *hBar = widget->horizontalScrollBar();
          QScrollBar *vBar = widget->verticalScrollBar();
          int vmin = rd.top();
          int vmax = rd.bottom() + 1 - rv.height();
          int hmin = rd.left();
          int hmax = rd.right() + 1 - rv.width();
          // loop prevention
          if (++layoutLoop >= 4)
            {
              if (hBar->isVisible() && hmax <= hmin)
                hmax = hmin + 1;
              if (vBar->isVisible() && vmax <= vmin)
                vmax = vmin + 1;
            }
          // set scrollbars
          changingSBars = true;
          vBar->setMinimum(vmin);
          vBar->setMaximum(vmax);
          vBar->setPageStep(rv.height());
          vBar->setSingleStep(lineStep * 2);
          vBar->setValue(rv.top());
          hBar->setMinimum(hmin);
          hBar->setMaximum(hmax);
          hBar->setPageStep(rv.width());
          hBar->setSingleStep(lineStep * 2);
          hBar->setValue(rv.left());
          changingSBars = false;
          layoutChange &= ~CHANGE_SCROLLBARS;
        }
      // force redraw
      else if (layoutChange & UPDATE_ALL)
        {
          Page *p;
          QRect &rv = visibleRect;
          foreach(p, pageLayout)
            p->viewRect = p->rect.translated(-rv.topLeft());
          layoutChange &= ~UPDATE_ALL;
          layoutChange &= ~UPDATE_PAGES;
          layoutChange &= ~UPDATE_BORDERS;
          widget->viewport()->update();
          checkCurrentMapArea();
          emit widget->layoutChanged();
        }
      // otherwise try scrolling
      else if (layoutChange & UPDATE_PAGES)
        {
          Page *p;
          QRect &rv = visibleRect;
          // redraw on failure
          layoutChange |= UPDATE_ALL;
          // determine potential scroll
          int dx = 0;
          int dy = 0;
          bool scroll = false;
          foreach(p, pageVisible)
            if (p->viewRect.size() == p->rect.size()) {
              scroll = true;
              dx = p->rect.left() - rv.left() - p->viewRect.left();
              dy = p->rect.top() - rv.top() - p->viewRect.top();
              break;
            }
          if (! scroll)
            continue;
          // verify potential scroll
          QRect rs = widget->viewport()->rect();
          rs &= rs.translated(-dx,-dy);
          if (rs.isEmpty())
            continue;
          foreach(p, pageLayout)
            {
              QRect rect = p->rect.translated(-rv.topLeft());
              if (rs.intersects(p->viewRect))
                p->viewRect.translate(dx,dy);
              else 
                p->viewRect = rect;
              if (rv.intersects(p->rect))
                if (p->viewRect != rect)
                  scroll = false;
            }
          if (! scroll)
            continue;
          layoutChange &= ~UPDATE_PAGES;
          layoutChange &= ~UPDATE_ALL;
          dragStart += QPoint(dx,dy);
          selectedRect.translate(dx, dy);
          // Tooltips mess things up during scrolls
          QToolTip::showText(QPoint(),QString());
          // hourGlass messes things up too
          widget->setHourGlassRatio(0);
          // Attention: scroll can generate a paintEvent
          // and call makeLayout again. This is why we
          // do these four things before.
          widget->viewport()->scroll(dx,dy);
          checkCurrentMapArea();
          emit widget->layoutChanged();
        }
      else if (layoutChange & UPDATE_BORDERS)
        {
          Page *p;
          QRegion region = widget->viewport()->rect();
          foreach(p, pageVisible)
            region -= p->viewRect;
          layoutChange &= ~UPDATE_BORDERS;
          widget->viewport()->update(region);
          emit widget->layoutChanged();
        }
      else if (layoutChange & REFRESH_PAGES)
        {
          Page *p;
          bool changes = false;
          foreach(p, pageLayout)
            if (p->redisplay)
              {
                if (visibleRect.intersects(p->rect))
                  widget->viewport()->update(p->viewRect);
                p->redisplay = false;
                changes = true;
              }
          if (changes)
            pixelCache.clear();
          layoutChange &= ~REFRESH_PAGES;
        }
      else
        {
          // deschedule
          if (layoutChange & ~SCHEDULED)
            qWarning("QDjVuWidget::makeLayout: unknown bits");
          layoutChange = 0;
        }
    }
  // deschedule
  layoutChange &= ~SCHEDULED;      
  layoutChange |= futureLayoutChange;
}


// Make all page requests
void
QDjVuPrivate::makePageRequests(void)
{
  Page *p;
  QRect &rv = visibleRect;
  bool found = false;
  if (! doc)
    return;
  // visible pages
  foreach(p, pageVisible)
    if (p && rv.intersects(p->rect)) 
      found |= requestPage(p);
  // search more if document is idle (prefetch/predecode)
  if (!found && !doc->runningProcesses())
    {
      if (continuous)
        {
          int sw = rv.width() / 2;
          int sh = rv.height() / 2;
          QRect brv = rv.adjusted(-sw,-sh,sw,sh);
          foreach(p, pageLayout)
            if (!p->page && brv.intersects(p->rect)) 
              found |= requestPage(p);
        }
      else if (pageVisible.size() > 0)
        {
          int pmin = pageVisible[0]->pageno;
          int pmax = pageVisible[0]->pageno;
          foreach(p, pageVisible)
            {
              pmax = qMax(pmax, p->pageno);
              pmin = qMin(pmin, p->pageno);
            }
          if (pmax+1 < numPages)
            found |= requestPage(&pageData[pmax+1]);
          if (! found && pmin-1 >= 0)
            found |= requestPage(&pageData[pmin-1]);
          if (! found && pmax+2 < numPages)
            found |= requestPage(&pageData[pmax+2]);
          if (! found && pmin-2 >= 0)
            found |= requestPage(&pageData[pmin-2]);
        }
    }
}

// Obtain page annotations and hidden text if not present
void
QDjVuPrivate::getAnnotationsAndText(Page *p)
{
  if (p->annotations == miniexp_dummy)
    {
      p->annotations = doc->getPageAnnotations(p->pageno);
      if (p->annotations)
        if (pageLayout.size() > 0 && p == pageLayout[0])
          adjustSettings(PRIORITY_ANNO, p->annotations);
      if (p->annotations)
        prepareMapAreas(p);
    }
  if (p->hiddenText == miniexp_dummy)
    {
      miniexp_t expr = doc->getPageText(p->pageno);
      if (expr != miniexp_dummy)
        p->hiddenText = flatten_hiddentext(expr);
    }
}


// Create the page decoder and connects its slots.
bool
QDjVuPrivate::requestPage(Page *p)
{
  bool result = false;
  if (! p->page) 
    {
      // create and connect page
      p->page = new QDjVuPage(doc, p->pageno); 
      connect(p->page, SIGNAL(pageinfo()), 
              this, SLOT(pageinfoPage()));
      connect(p->page, SIGNAL(redisplay()),
              this, SLOT(redisplayPage()));
      connect(p->page, SIGNAL(error(QString,QString,int)), 
              this, SLOT(error(QString,QString,int)) );
      connect(p->page, SIGNAL(info(QString)), 
              this, SLOT(info(QString)) );
      if (! p->page->isValid())
        emit widget->errorCondition(p->pageno);
      // decoded page found in the cache (needed with djvulibre <= 3.5.27)
      if (ddjvu_page_decoding_status(*(p->page)) >= DDJVU_JOB_OK)
        pageinfoPage(p->page);
      // schedule redisplay
      p->redisplay = true;
      changeLayout(REFRESH_PAGES);
      result = true;
    }
  if (! p->infoNeeded)
    {
      p->infoNeeded = true;
      changeLayout(CHANGE_SIZE);
    }
  getAnnotationsAndText(p);
  return result;
}


// finds closest page to a point in desk coordinates
int
QDjVuPrivate::findClosestPage(const QPoint &deskPoint, 
                              const QList<Page*> &pages, 
                              Page **pp, int *pd)
{
  Page *p;
  Page *bestPage = 0;
  int bestDistance = 0;
  foreach(p, pages)
    {
      int distance = 0;
      if (deskPoint.y() < p->rect.top())
        distance += p->rect.top() - deskPoint.y();
      else if (deskPoint.y() > p->rect.bottom() - 1)
        distance += deskPoint.y() - p->rect.bottom() + 1;
      if (deskPoint.x() < p->rect.left())
        distance += p->rect.left() - deskPoint.x();
      else if (deskPoint.x() > p->rect.right() - 1)
        distance += deskPoint.x() - p->rect.right() + 1;
      if (bestPage==0 || distance<bestDistance)
        {
          bestPage = p;
          bestDistance = distance;
          if (! bestDistance)
            break;
        }
    }
  if (! bestPage)
    return -1;
  if (pp)
    *pp = bestPage;
  if (pd)
    *pd = bestDistance;
  return bestPage->pageno;
}

// compute Position for a given viewport point.
Position
QDjVuPrivate::findPosition(const QPoint &point, bool closestAnchor)
{
  Position pos; 
  Page *bestPage = 0;
  int bestDistance = 0;
  QPoint deskPoint = visibleRect.topLeft() + point;
  findClosestPage(deskPoint, pageVisible, &bestPage, &bestDistance);
  if (! bestPage)
    findClosestPage(deskPoint, pageLayout, &bestPage, &bestDistance);
  if (bestPage)
    {
      pos.pageNo = bestPage->pageno;
      pos.posView = deskPoint - bestPage->rect.topLeft(); 
      pos.posPage = bestPage->mapper.unMapped(deskPoint);
      pos.inPage = (bestDistance == 0 && bestPage->dpi > 0);
      if (closestAnchor)
        {
          int w = bestPage->rect.width() - 1;
          int h = bestPage->rect.height() - 1;
          pos.hAnchor = 100 * qBound(0, pos.posView.x(), w) / w;
          pos.vAnchor = 100 * qBound(0, pos.posView.y(), h) / h;
          int x = w * pos.hAnchor / 100;
          int y = h * pos.vAnchor / 100;
          pos.posView = pos.posView - QPoint(x,y);
        }
    }
  return pos;
}

// when the pointer moves
void 
QDjVuPrivate::updatePosition(const QPoint &point, bool click, bool links)
{
  if (point.isNull())
    return;
  // locate
  bool changed = false;
  cursorPoint = point;
  Position pos = findPosition(point);
  if (! pageMap.contains(pos.pageNo))
    return;
  if (pos.pageNo  != cursorPos.pageNo  ||
      pos.inPage  != cursorPos.inPage  ||
      pos.posView != cursorPos.posView ||
      pos.posPage != cursorPos.posPage )
    changed = true;
  // update
  cursorPos = pos;
  if (click)
    updateCurrentPoint(pos);
  if (! changed)
    return;
  // emit pointerposition signal
  PageInfo info;
  Page *p = pageMap.value(pos.pageNo,0);
  if (p != 0)
    {
      info.pageno = pos.pageNo;
      info.width = p->width;
      info.height = p->height;
      info.dpi = p->dpi;
      info.segment = widget->getSegmentForRect(selectedRect, info.pageno);
      info.selected = selectedRect;
      emit widget->pointerPosition(pos, info);
    }
  // check mapareas
  if (links)
    checkCurrentMapArea();
}


// when the view moves
void 
QDjVuPrivate::updateCurrentPoint(const Position &pos)
{
  QPoint point;
  bool changePos = !pos.doPage;
  if (pageMap.contains(pos.pageNo))
    {
      Page *p = pageMap[pos.pageNo];
      point = p->rect.topLeft();
      if (pos.hAnchor > 0 && pos.hAnchor <= 100)
        point.rx() += (p->rect.width() - 1) * pos.hAnchor / 100;
      if (pos.vAnchor > 0 && pos.vAnchor <= 100)
        point.ry() += (p->rect.height() - 1) * pos.vAnchor / 100;
      if (pos.inPage && p->dpi > 0)
        point = p->mapper.mapped(pos.posPage);
      else if (!pos.inPage)
        point += pos.posView;
    }
  else 
    {
      int n = pageLayout.size();
      if (n > 0 && pos.pageNo < pageLayout[0]->pageno)
        point = pageLayout[0]->rect.topLeft();
      else if (n > 0 && pos.pageNo > pageLayout[n-1]->pageno)
        point = pageLayout[n-1]->rect.bottomRight();
    }
  // check visibility
  if (! visibleRect.contains(point))
    {
      changePos = true;
      point.ry() = qBound(visibleRect.top() + borderSize,
                          point.y(),
                          visibleRect.bottom() - borderSize);
      point.rx() = qBound(visibleRect.left() + borderSize,
                          point.x(),
                          visibleRect.right() - borderSize);
    }
  // update currentPoint
  currentPoint = point - visibleRect.topLeft();
  // update currentPos
  int oldPage = currentPos.pageNo;
  if (changePos || pos.hAnchor || pos.vAnchor)
    currentPos = findPosition(currentPoint);
  else
    currentPos = pos;
  if (oldPage != currentPos.pageNo)
    emit widget->pageChanged(currentPos.pageNo);
}


// ----------------------------------------
// QDJVUPRIVATE SLOTS

void 
QDjVuPrivate::docinfo()
{
  ddjvu_status_t status;
  status = ddjvu_document_decoding_status(*doc);
  if (status == DDJVU_JOB_OK && !docReady)
    {
      docReady = true;
      numPages = ddjvu_document_get_pagenum(*doc);
      pageData.clear();
      pageData.resize(numPages);
      for (int i=0; i<numPages; i++)
        pageData[i].pageno = i;
      changeLayout(CHANGE_PAGES|UPDATE_ALL);
      // request first page immediately
      if (numPages > 0)
        requestPage((pageMap[0] = &pageData[0]));
    }
  else if (status == DDJVU_JOB_STOPPED && !docStopped)
    {
      docStopped = true;
      emit widget->stopCondition(-1);
      widget->viewport()->update();
    }
  else if (status == DDJVU_JOB_FAILED && !docFailed)
    {
      docFailed = true;
      emit widget->errorCondition(-1);
      widget->viewport()->update();
    }
}

void 
QDjVuPrivate::pageinfo()
{
  changeLayout(CHANGE_SIZE, 250);
}

void 
QDjVuPrivate::pageinfoPage()
{
  QObject *send = sender();
  pageinfoPage(qobject_cast<QDjVuPage*>(send));
}

void 
QDjVuPrivate::pageinfoPage(QDjVuPage *page)
{
  if (page) 
    {
      Page *p = 0;
      int pageno = page->pageNo();
      if (pageno>=0 && pageno<pageData.size())
        p = &pageData[pageno];
      // decoding finished?
      switch(ddjvu_page_decoding_status(*page))
        {
        case DDJVU_JOB_OK:
          if (p)
            getAnnotationsAndText(p);
          /* FALLTHRU */
        case DDJVU_JOB_STARTED:
          if (p && p->dpi <= 0)
            {
              ddjvu_page_rotation_t rot;
              rot = ddjvu_page_get_initial_rotation(*page);
              ddjvu_page_set_rotation(*page, rot);
              int w = ddjvu_page_get_width(*page);
              int h = ddjvu_page_get_height(*page);
              int d = ddjvu_page_get_resolution(*page);
              if (d > 0 && w > 0 && h > 0)
                {
                  p->width = w;
                  p->height = h;
                  p->dpi = d;
                  p->initialRot = rot;
                  changeLayout(CHANGE_STATS|CHANGE_SCALE|UPDATE_BORDERS);
                }
            }
          break;
        case DDJVU_JOB_STOPPED:
          docStopped = true;
          emit widget->stopCondition(page->pageNo());
          break;
        case DDJVU_JOB_FAILED:
          emit widget->errorCondition(page->pageNo());
          break;
        default:
          break;
        }
    }
}

void 
QDjVuPrivate::redisplayPage()
{
  QObject *send = sender();
  QDjVuPage *page = qobject_cast<QDjVuPage*>(send);
  if (page) 
    {
      int pageno = page->pageNo();
      if (pageMap.contains(pageno)) 
        {
          Page *p = pageMap[pageno];
          if (p && !p->redisplay) 
            {
              p->redisplay = true;
              changeLayout(REFRESH_PAGES);
            }
        }
    }
}

void 
QDjVuPrivate::error(QString message, QString filename, int lineno)
{
  emit widget->error(message, filename, lineno);
  errorMessages.prepend(message);
  while (errorMessages.size() > maxMessages)
    errorMessages.removeLast();
}

void 
QDjVuPrivate::info(QString message)
{
  emit widget->info(message);
  infoMessages.prepend(message);
  while (infoMessages.size() > maxMessages)
    infoMessages.removeLast();
}



// ----------------------------------------
// QDJVUWIDGET


/*! \class QDjVuWidget
  \brief Widget for display DjVu documents.

  Class \a QDjVuWidget implements the elementary widget for displaying DjVu
  documents. It appears as a display area possibly surrounded by horizontal
  and vertical scrollbars.  The display area can show a single page, two pages
  side by side, or all pages in continuously scrollable area. */

QDjVuWidget::~QDjVuWidget()
{
  delete priv;
  priv = 0;
}

// This is a helper for the constructors
void
QDjVuPrivate::initWidget(bool opengl)
{

  // set widget policies
  widget->setFocusPolicy(Qt::StrongFocus);
  widget->setSizePolicy(QSizePolicy::MinimumExpanding, 
                        QSizePolicy::MinimumExpanding);
  // set opengl acceleration
#if QT_VERSION >= 0x040400
  if (opengl)
    {
      const char *ge = 0;
#ifdef QT_OPENGL_LIB
      QGLWidget *gw = 0;
      if (! QGLFormat::hasOpenGL())
        ge = "cannot find openGL on this system";
      if (!ge)
        gw = new QGLWidget();
      if (gw && !ge && !gw->isValid())
        ge = "cannot setup openGL context";
      if (gw && !ge && !gw->format().directRendering())
        ge = "cannot setup openGL direct rendering";
      if (gw && !ge) {
        gw->setFocusPolicy(Qt::ClickFocus);
        widget->setViewport(gw);
      } else if (gw)
        delete gw;
#else
      ge = "disabled at compilation time";
#endif
      if (!ge)
        qWarning("Using openGL rendering");
      else
        qWarning("Using default rendering (%s)", ge);
    }
#endif
  // setup viewport
  QWidget *vp = widget->viewport();
#if QT_VERSION >= 0x50000
  vp->setAttribute(Qt::WA_OpaquePaintEvent);
#elif QT_VERSION >= 0x040100
  vp->setAttribute(Qt::WA_OpaquePaintEvent);
  vp->setAttribute(Qt::WA_NativeWindow);
#else
  vp->setAttribute(Qt::WA_StaticContents);
  vp->setAttribute(Qt::WA_NoSystemBackground);
  vp->setAttribute(Qt::WA_NativeWindow);
#endif
#if QT_VERSION >= 0x040600
  vp->grabGesture(Qt::PinchGesture);
#endif
  vp->setMouseTracking(true);
}

/*! Construct a \a QDjVuWidget instance.
  Argument \a parent is the parent of this widget 
  in the \a QObject hierarchy. Setting argument
  \a opengl to true enables openGL acceleration */

QDjVuWidget::QDjVuWidget(bool opengl, QWidget *parent)
  : QAbstractScrollArea(parent), priv(new QDjVuPrivate(this))
{
  priv->initWidget(opengl);
}

/*! Overloaded constructor */

QDjVuWidget::QDjVuWidget(QWidget *parent)
  : QAbstractScrollArea(parent), priv(new QDjVuPrivate(this))
{
  priv->initWidget(false);
}

/*! This overloaded constructor calls \a setDocument. */

QDjVuWidget::QDjVuWidget(QDjVuDocument *doc, bool opengl, QWidget *parent)
  : QAbstractScrollArea(parent), priv(new QDjVuPrivate(this))
{
  priv->initWidget(opengl);
  setDocument(doc);
}

/*! This overloaded constructor calls \a setDocument. */

QDjVuWidget::QDjVuWidget(QDjVuDocument *doc, QWidget *parent)
  : QAbstractScrollArea(parent), priv(new QDjVuPrivate(this))
{
  priv->initWidget(false);
  setDocument(doc);
}


// ----------------------------------------
// MESSAGE


/*! Return one of the last error messages.
  Argument \a n indicates which message should be returned. 
  Value 0 corresponds to the most recent message. 
*/

QString 
QDjVuWidget::pastErrorMessage(int n)
{
  if (0 <= n && n < priv->errorMessages.size())
    return priv->errorMessages.at(n);
  return QString();
}

/*! Return one of the last informational messages.
  Argument \a n indicates which message should be returned. 
  Value 0 corresponds to the most recent message.
*/

QString 
QDjVuWidget::pastInfoMessage(int n)
{
  if (0 <= n && n < priv->infoMessages.size())
    return priv->infoMessages.at(n);
  return QString();
}




// ----------------------------------------
// QDJVUWIDGET PROPERTIES


/*! \property QDjVuWidget::document
  The \a QDjVuDocument object shown in this widget. */

QDjVuDocument *
QDjVuWidget::document(void) const
{
  return priv->doc;
}

/*! Specify the \a QDjVuDocument shown by this widget.
  The caller is responsible to ensure that the 
  document object \a d remains allocated until
  the djvu widget is either destroyed or directed
  to use another document.
  Two techniques are interesting:
  The first consists of capturing the document signal \a destroyed()
  and to call \a setDocument(0) before the destruction occurs.
  The second consists of allocating the document object 
  in \a autoDelete mode, since the djvu widget calls
  methods \a ref() and \a deref() adequately. */

void 
QDjVuWidget::setDocument(QDjVuDocument *d)
{
  if (d != priv->doc)
    {
      // cleanup
      if (priv->doc)
        {
          priv->adjustSettings(PRIORITY_ANNO, miniexp_nil);
          priv->adjustSettings(PRIORITY_CGI, miniexp_nil);
          disconnect(priv->doc, 0, priv, 0);
          priv->doc->deref();
        }
      priv->doc = 0;
      priv->pageData.clear();
      priv->pageLayout.clear();
      priv->pageMap.clear();
      priv->pageVisible.clear();
      priv->currentMapAreaPage = 0;
      priv->currentMapArea = 0;
      priv->dragMode = DRAG_NONE;
      delete priv->lens;
      priv->lens = 0;
      // setup
      if (d)
        d->ref();
      priv->doc = d;
      priv->docReady = false;
      priv->docStopped = false;
      priv->docFailed = false;
      priv->numPages = 1;
      if (! (priv->doc && priv->doc->isValid()))
        priv->docFailed = true;
      // connect
      if (priv->doc)
        {
          connect(priv->doc, SIGNAL(docinfo()), 
                  priv, SLOT(docinfo()));
          connect(priv->doc, SIGNAL(pageinfo()), 
                  priv, SLOT(pageinfo()));
          connect(priv->doc, SIGNAL(error(QString,QString,int)),
                  priv, SLOT(error(QString,QString,int)));
          connect(priv->doc, SIGNAL(info(QString)),
                  priv, SLOT(info(QString)));
          connect(priv->doc, SIGNAL(idle()),
                  priv, SLOT(makePageRequests()));
          // the document may already be ready
          QTimer::singleShot(0, priv, SLOT(docinfo()));
        }
      // update
      priv->estimatedWidth = 0;
      priv->estimatedHeight = 0;
      priv->visibleRect.setRect(0,0,0,0);
      priv->currentPos = Position();
      priv->currentPoint = QPoint(priv->borderSize,priv->borderSize);
      priv->cursorPos = Position();
      priv->cursorPoint = QPoint(0,0);
      priv->layoutChange = 0;
      priv->changeLayout(CHANGE_STATS|CHANGE_PAGES|UPDATE_ALL);
      priv->alternativeImageFilename.clear();
      priv->alternativeImagePtr.reset();
    }
}

/*! \property QDjVuWidget::page
  The page currently displayed.
  When getting the property, this is the number of the page
  that is closest to the center of the display area.
  When setting the property, the topleft corner of the
  desired page replaces the topleft corner of the current page.
  Default: page 0.
*/

int 
QDjVuWidget::page(void) const
{
  return qBound(0, priv->currentPos.pageNo, priv->numPages);
}

void 
QDjVuWidget::setPage(int n, bool keep)
{
  int currentPageNo = page();
  if (n != currentPageNo && n>=0 && n<priv->numPages)
    {
      Position pos;
      QPoint pnt(priv->borderSize, priv->borderSize);
      if (keep)
        {
          pnt = priv->currentPoint;
          pos = priv->findPosition(pnt);
        }
      pos.pageNo = n;
      pos.doPage = true;
      pos.inPage = false;
      setPosition(pos, pnt);
    }
}

/*! \property QDjVuWidget::position
  The document position associated with a viewport point.
  The default point is close to the topLeft corner of the viewport.
  When setting the position, flag \a Position::inPage
  is used to determine if the document position is expressed
  in full resolution page coordinates (\a Position::posPage) or
  in pixels relative to the top left page corner (\a Position::posView)
  or the point specified by the \a Position::hAnchor 
  and \a Position::vAnchor.
  When \a Position::inPage is \a true, 
  variable \a Position::posPage must be contained inside 
  the full resolution page rectangle.
*/

Position 
QDjVuWidget::position(void) const
{
  QPoint topLeft(priv->borderSize, priv->borderSize);
  return position(topLeft);
}


QPoint
QDjVuWidget::hotSpot() const
{
  return priv->currentPoint;
}


Position 
QDjVuWidget::position(const QPoint &point) const
{
  return priv->findPosition(point);
}

void 
QDjVuWidget::setPosition(const Position &pos)
{
  QPoint topLeft(priv->borderSize, priv->borderSize);
  setPosition(pos, topLeft);
}

void 
QDjVuWidget::setPosition(const Position &pos, const QPoint &p, bool animate)
{
  // animation
  priv->clearAnimation();
  if (animate && priv->animationEnabled)
    if (priv->computeAnimation(pos, p))
      return;
  // standard procedure
  priv->movePoint = p;
  priv->movePos = pos;
  if (priv->pageMap.contains(pos.pageNo))
    priv->changeLayout(CHANGE_VIEW|CHANGE_SCROLLBARS);
  else
    priv->changeLayout(CHANGE_PAGES|CHANGE_VIEW|CHANGE_SCROLLBARS);
}


/*! Unlike \a position() which sets the anchor fields
    \a pos.hAnchor and \a pos.vAnchor to zero, 
    this version determines where to place the anchor
    to minimize the length of vector \a pos.posView. */

Position 
QDjVuWidget::positionWithClosestAnchor(const QPoint &point) const
{
  return priv->findPosition(point, true);
}


/*! \property QDjVuWidget::rotation
  Number of counter-clockwise quarter turns applied to the pages. 
  Only the low two bits of this number are meaningful. 
  Default: 0. */

int 
QDjVuWidget::rotation(void) const
{
  return priv->rotation;
}

void 
QDjVuWidget::setRotation(int r)
{
  if (r != priv->rotation)
    {
      priv->rotation = r;
      priv->changeLayout(CHANGE_SCALE|UPDATE_ALL);
    }
}

/*! \property QDjVuWidget::zoom
  Zoom factor applied to the pages.
  Positive zoom factors take into account the resolution
  of the page images and assumes that the display resolution
  is 100 dpi. Negative zoom factors define special behaviors:
  \a ZOOM_ONE2ONE displays one image pixel for one screen pixel. 
  \a ZOOM_FITWIDTH and \a ZOOM_FITPAGE dynamically compute
  the zoom factor to fit the page(s) inside the display area. 
  Default: \a ZOOM_100. 
  
*/

int
QDjVuWidget::zoom(void) const
{
  return priv->zoom;
}

void 
QDjVuWidget::setZoom(int z)
{
  priv->qZoom.set(PRIORITY_USER, z);    
  priv->changeZoom();
}

void
QDjVuPrivate::changeZoom(void)
{
  int z = qZoom;
  switch(z)
    {
    case ZOOM_STRETCH:
    case ZOOM_ONE2ONE:
    case ZOOM_FITWIDTH:
    case ZOOM_FITPAGE:
      break;
    default:
      z = qBound((int)ZOOM_MIN, z, (int)ZOOM_MAX);
      break;
    }
  if (zoom != z)
    {
      zoom = z;
      changeLayout(CHANGE_SCALE|UPDATE_ALL);
    }
}


/*! Return the effective zoom factor of the current page.
  This is the same as \a zoom() when the zoom factor is positive.
  Otherwise this function returns the dynamically computed
  zoom factor.  */

int 
QDjVuWidget::zoomFactor(void) const
{
  if (priv->zoom >= ZOOM_MIN && priv->zoom <= ZOOM_MAX)
    return priv->zoom;
  if (priv->zoomFactor >= ZOOM_MIN && priv->zoomFactor <= ZOOM_MAX)
    return priv->zoomFactor;
  // Only when ZOOM_ONE2ONE or ZOOM_STRETCH is selected:
  priv->makeLayout();
  if (! priv->pageMap.contains(priv->currentPos.pageNo))
    return 100;
  Page *p = priv->pageMap[priv->currentPos.pageNo];
  if (p->dpi>0 && p->width>0)
    return (p->rect.width() * p->dpi * 100) / (p->width * priv->sdpi); 
  return 100;
}

/*! \property QDjVuWidget::gamma
  Gamma factor of the display. 
  Default 2.2. */

double
QDjVuWidget::gamma(void) const
{
  return priv->gamma;
}

void
QDjVuWidget::setGamma(double gamma)
{
  priv->gamma = gamma;
  ddjvu_format_set_gamma(priv->renderFormat, gamma);
  priv->pixelCache.clear();
  priv->changeLayout(UPDATE_ALL);
}


/*! \property QDjVuWidget::invertLuminance
  Display image with inverted luminance.
*/


bool
QDjVuWidget::invertLuminance(void) const
{
  return priv->invertLuminance;
}

void
QDjVuWidget::setInvertLuminance(bool b)
{
  priv->invertLuminance = b;
  priv->pixelCache.clear();
  priv->changeLayout(UPDATE_ALL);
}

/*! \property QDjVuWidget::invertLuminance
  Specify whether mouse wheel zooms the image or scrolls the page.
*/

bool
QDjVuWidget::mouseWheelZoom(void) const
{
  return priv->mouseWheelZoom;
}

void
QDjVuWidget::setMouseWheelZoom(bool b)
{
  priv->mouseWheelZoom = b;
}


/*! \property QDjVuWidget::white
  White point of the display. 
  This is a noop when DDJVUAPI_VERSION<20.
  Default 0xffffff. */

QColor
QDjVuWidget::white(void) const
{
  return priv->white;
}

void
QDjVuWidget::setWhite(QColor w)
{
#if DDJVUAPI_VERSION >= 20
  priv->white = w;
  ddjvu_format_set_white(priv->renderFormat, w.blue(), w.green(), w.red());
  priv->pixelCache.clear();
  priv->changeLayout(UPDATE_ALL);
#endif
}


/*! \property QDjVuWidget::screenDpi
  Resolution of the screen.
  Default is 100dpi. */

int
QDjVuWidget::screenDpi(void) const
{
  return priv->sdpi;
}

void
QDjVuWidget::setScreenDpi(int sdpi)
{
  sdpi = qBound(25, sdpi, 600);
  if (priv->sdpi != sdpi)
    {
      priv->sdpi = sdpi;
      priv->changeLayout(CHANGE_SCALE|UPDATE_ALL);
    }
}


/*! \property QDjVuWidget::displayMode
  Display mode for the DjVu images.
  Default: \a DISPLAY_COLOR. */


DisplayMode 
QDjVuWidget::displayMode(void) const
{
  return priv->display;
}

void 
QDjVuWidget::setDisplayMode(DisplayMode m)
{
  priv->qDisplay.set(PRIORITY_USER, m);
  priv->changeDisplay();
}

void
QDjVuPrivate::changeDisplay()
{
  if (display != qDisplay)
    {
      display = qDisplay;
      pixelCache.clear();
      changeLayout(UPDATE_ALL);
    }
}

/*! \property QDjVuWidget::displayFrame
  When this property is true, a frame 
  is displayed around each page.
  Default: \a true. */

bool
QDjVuWidget::displayFrame(void) const
{
  return priv->frame;
}

void 
QDjVuWidget::setDisplayFrame(bool b)
{
  if (b != priv->frame)
    {
      priv->frame = b;
      priv->changeLayout(UPDATE_ALL);
    }
}

/*! \property QDjVuWidget::sideBySide
  Determine the layout of the pages shown in the displayed area.
  Setting this property to \a true displays pages side by side. 
  This can be combined with the \a continuous property.
  Default: \a false. */

bool 
QDjVuWidget::sideBySide(void) const
{
  return priv->sideBySide;
}

void 
QDjVuWidget::setSideBySide(bool b)
{
  if (b != priv->sideBySide)
    {
      priv->sideBySide = b;
      priv->changeLayout(CHANGE_PAGES|UPDATE_ALL);
    }
}

/*! \property QDjVuWidget::coverPage
  Determine whether the first page must be show alone when
  multipage documents are displayed side-by-side.
  Default: \a false. */

bool 
QDjVuWidget::coverPage(void) const
{
  return priv->coverPage;
}

void 
QDjVuWidget::setCoverPage(bool b)
{
  if (b != priv->coverPage)
    {
      priv->coverPage = b;
      if (priv->sideBySide)
        priv->changeLayout(CHANGE_PAGES|UPDATE_ALL);
    }
}


/*! \property QDjVuWidget::rightToLeft
  Determine whether the side-by-side pages should be displayed
  right-to-left instead of left-to-right.
  Default: \a false. */

bool 
QDjVuWidget::rightToLeft(void) const
{
  return priv->rightToLeft;
}

void 
QDjVuWidget::setRightToLeft(bool b)
{
  if (b != priv->rightToLeft)
    {
      priv->rightToLeft = b;
      if (priv->sideBySide)
        priv->changeLayout(CHANGE_PAGES|UPDATE_ALL);
    }
}



/*! \property QDjVuWidget::continuous
  Determine the layout of the pages shown in the displayed area.
  Setting this property to \a true displays all the document
  pages in a continuously scrollable area. This can be combined
  with the \a sideBySide property. Default: \a false. */


bool 
QDjVuWidget::continuous(void) const
{
  return priv->continuous;
}

void 
QDjVuWidget::setContinuous(bool b)
{
  if (b != priv->continuous)
    {
      priv->continuous = b;
      priv->changeLayout(CHANGE_PAGES|UPDATE_ALL);
    }
}

/*! \property QDjVuWidget::horizAlign
  Determine the horizontal alignment of the pages in the display area.
  Horizontal alignment can be complicated in continuous mode.
  Default: \a ALIGN_CENTER. */

Align 
QDjVuWidget::horizAlign(void) const
{
  return priv->hAlign;
}

void 
QDjVuWidget::setHorizAlign(Align a)
{
  priv->qHAlign.set(PRIORITY_USER, a);
  priv->changeHAlign();
}

void
QDjVuPrivate::changeHAlign(void)
{
  if (hAlign != qHAlign)
    {
      hAlign = qHAlign;
      changeLayout(CHANGE_POSITIONS);
    }
}

/*! \property QDjVuWidget::vertAlign
  Determine the vertical alignment of the pages in the display area. 
  Vertical alignment can be very subtle in continuous mode
  because it only matters when pages are also displayed side by side. 
  Default: \a ALIGN_CENTER. */

Align 
QDjVuWidget::vertAlign(void) const
{
  return priv->vAlign;
}

void 
QDjVuWidget::setVertAlign(Align a)
{
  priv->qVAlign.set(PRIORITY_USER, a);
  priv->changeVAlign();
}

void
QDjVuPrivate::changeVAlign(void)
{
  if (vAlign != qVAlign)
    {
      vAlign = qVAlign;
      changeLayout(CHANGE_POSITIONS);
    }
}

/*! \property QDjVuWidget::borderBrush
  Brush used to fill the part of the
  display area that is not covered by a page. 
  Default: light gray. */

QBrush 
QDjVuWidget::borderBrush(void) const
{
  return priv->borderBrush;
}

void 
QDjVuWidget::setBorderBrush(QBrush b)
{
  priv->qBorderBrush.set(PRIORITY_USER, b);
  priv->changeBorderBrush();
}

void
QDjVuPrivate::changeBorderBrush(void)
{
  if (borderBrush != qBorderBrush)
    {
      borderBrush = qBorderBrush;
      changeLayout(UPDATE_BORDERS);
    }
}


/*! \property QDjVuWidget::borderSize
  The minimal size of the border around the pages.
  Default: 8 pixels. */


int 
QDjVuWidget::borderSize(void) const
{
  return priv->borderSize;
}

void 
QDjVuWidget::setBorderSize(int b)
{
  priv->qBorderSize.set(PRIORITY_USER, b);
  priv->changeBorderSize();
}

void
QDjVuPrivate::changeBorderSize(void)
{
  if (borderSize != qBorderSize)
    {
      borderSize = qBorderSize;
      changeLayout(CHANGE_POSITIONS);
    }
}


/*! \property QDjVuWidget::separatorSize
  The size of the gap between pages in side-by-side mode.
  Default: 12 pixels. */


int 
QDjVuWidget::separatorSize(void) const
{
  return priv->separatorSize;
}

void 
QDjVuWidget::setSeparatorSize(int b)
{
  b = qMax(0, b);
  if (b != priv->separatorSize)
    {
      priv->separatorSize = b;
      if (priv->sideBySide || priv->continuous)
        priv->changeLayout(CHANGE_PAGES|UPDATE_ALL);
    }
}



/*! \property QDjVuWidget::contextMenu
  Menu displayed when the user invokes a context menu for this widget. 
  Default: no context menu. */

QMenu* 
QDjVuWidget::contextMenu(void) const
{
  return priv->contextMenu;
}

void 
QDjVuWidget::setContextMenu(QMenu *m)
{
  priv->contextMenu = m;
}


/*! \property QDjVuWidget::displayMapAreas
  Indicates whether the mapareas specified in the annotations
  should be displayed. Default: \a true. */

bool 
QDjVuWidget::displayMapAreas(void) const
{
  return priv->displayMapAreas;
}

void 
QDjVuWidget::setDisplayMapAreas(bool b)
{
  if (b != priv->displayMapAreas)
    {
      priv->displayMapAreas = b;
      viewport()->update();
      priv->checkCurrentMapArea();
      priv->showTransientMapAreas(priv->allLinksDisplayed);
    }
}


/*! \property QDjVuWidget::keyboardEnabled
  Enables keyboard interaction. 
  This property controls the behavior of 
  arrows keys, page movement keys, and 
  various shortcut keys.
  Default: \a true. */

bool 
QDjVuWidget::keyboardEnabled(void) const
{
  return priv->keyboardEnabled;
}

void 
QDjVuWidget::enableKeyboard(bool b)
{
  if (b != priv->keyboardEnabled)
    priv->keyboardEnabled = b;
}

/*! \property QDjVuWidget::mouseEnabled
  Enables mouse interaction. 
  Default: \a true. */

bool 
QDjVuWidget::mouseEnabled(void) const
{
  return priv->mouseEnabled;
}

void 
QDjVuWidget::enableMouse(bool b)
{
  if (b != priv->mouseEnabled)
    priv->mouseEnabled = b;
}

/*! \property QDjVuWidget::hyperlinkEnabled
  Enables hyperlinks. 
  This propery indicates whether hyperlink feedback
  is displayed, and whether the signals \a pointerEnter,
  \a pointerLeave and \a pointerClick are emitted.
  You must also set \a mouseEnabled and \a displayMapAreas to \a true.
  Default: \a true. */

bool 
QDjVuWidget::hyperlinkEnabled(void) const
{
  return priv->hyperlinkEnabled;
}

void 
QDjVuWidget::enableHyperlink(bool b)
{
  if (b != priv->hyperlinkEnabled)
    {
      priv->hyperlinkEnabled = b;
      priv->checkCurrentMapArea();
    }
}


/*! \property QDjVuWidget::animationEnabled
  Enables animation on position changes.
  This property indicates whether certain position changes
  are performed using a display animation.
  Default: \a true. */

bool 
QDjVuWidget::animationEnabled(void) const
{
  return priv->animationEnabled;
}

void
QDjVuWidget::enableAnimation(bool b)
{
  priv->clearAnimation();
  priv->animationEnabled = b;
}


/*! \property QDjVuWidget::animationInProgress
  Read-only property that tells whether an animation is in progress. */

bool 
QDjVuWidget::animationInProgress(void) const
{
  if (priv->animationEnabled)
    if (! priv->animationPosition.isEmpty())
      return true;
  return false;
}


/*! \property QDjVuWidget::pixelCacheSize
  Defines the maximal number of pixels in the cache
  for decoded images segments. */

int 
QDjVuWidget::pixelCacheSize(void) const
{
  return priv->pixelCacheSize;
}

void
QDjVuWidget::setPixelCacheSize(int s)
{
  int oldPixelCacheSize = priv->pixelCacheSize;
  priv->pixelCacheSize = s;
  if (s < oldPixelCacheSize)
    priv->trimPixelCache();
}


/*! \property QDjVuWidget::lensPower
  Sets the power of the magnification lens.
  Legal value range from 0x to 10x.
  Value 0x disables the lens.
  Default is 3x. */

int 
QDjVuWidget::lensPower(void) const
{
  return priv->lensMag;
}

void 
QDjVuWidget::setLensPower(int mag)
{
  priv->lensMag = qBound(0,mag,10);
}

/*! \property QDjVuWidget::lensSize
  Sets the size of the magnification lens.
  Legal value range from 0 to 500 pixels.
  Value 0 disables the lens.
  Default is 300. */

int 
QDjVuWidget::lensSize(void) const
{
  return priv->lensSize;
}

void 
QDjVuWidget::setLensSize(int size)
{
  priv->lensSize = qBound(0,size,500);
}


/*! \property QDjVuWidget::hourGlassRatio
  Setups the hour glass timer indicator.
  Nothing is displayed when value is zero.
  Otherwise it indicates that one should
  display the specified fraction of time
  before switching page in slideshow more. */

double
QDjVuWidget::hourGlassRatio() const
{
  return priv->hourGlassRatio;
}

QRect
QDjVuPrivate::hourGlassRect() const
{
  int bs = qBound(4, borderSize / 2, 16);
  int hs = qBound(16, borderSize * 6, 64);
  int w = widget->viewport()->width();
  int h = widget->viewport()->height();
  if (w > hs+bs+bs && h > hs+bs+bs)
    return QRect(w-bs-hs,h-bs-hs,hs,hs);
  return QRect();
}

void
QDjVuWidget::setHourGlassRatio(double ratio)
{
  if (ratio != priv->hourGlassRatio && ratio>=0 && ratio<=1)
    {
      priv->hourGlassRatio = ratio;
      QRect rect = priv->hourGlassRect();
      if (!rect.isEmpty())
        viewport()->update(rect.adjusted(-2,-2,2,2));
    }
}

/*! \property QDjVuWidget::modifiersForLens
  Sets the modifier that should be depressed in 
  order to display the magnifying lens. 
  Default: Control + Shift. */

Qt::KeyboardModifiers 
QDjVuWidget::modifiersForLens() const
{
  return priv->modifiersForLens;
}

void
QDjVuWidget::setModifiersForLens(Qt::KeyboardModifiers m)
{
  if (priv->modifiersForLens != m)
    {
      priv->modifiersForLens = m;
      if (priv->dragMode == DRAG_NONE)
        modifierEvent(priv->modifiers, priv->buttons, priv->cursorPoint);
    }
}

/*! \property QDjVuWidget::modifiersForSelect
  Sets the modifier that should be depressed 
  in conjunction with the left mouse button in 
  order to select a rectangular area.
  Default: Control. */

Qt::KeyboardModifiers 
QDjVuWidget::modifiersForSelect() const
{
  return priv->modifiersForSelect;
}

void
QDjVuWidget::setModifiersForSelect(Qt::KeyboardModifiers m)
{
  if (priv->modifiersForSelect != m)
    {
      priv->modifiersForSelect = m;
      if (priv->dragMode == DRAG_NONE)
        modifierEvent(priv->modifiers, priv->buttons, priv->cursorPoint);
    }
}


/*! \property QDjVuWidget::modifiersForLinks
  Sets the modifier that should be depressed in 
  order to display all hyperlinks.
  Default: Shift. */

Qt::KeyboardModifiers 
QDjVuWidget::modifiersForLinks() const
{
  return priv->modifiersForLinks;
}

void
QDjVuWidget::setModifiersForLinks(Qt::KeyboardModifiers m)
{
  if (priv->modifiersForLinks != m)
    {
      priv->modifiersForLinks = m;
      priv->showTransientMapAreas(m!=Qt::NoModifier && m==priv->modifiers);
      if (priv->dragMode == DRAG_NONE)
        modifierEvent(priv->modifiers, priv->buttons, priv->cursorPoint);
    }
}


// ----------------------------------------
// MAPAREAS AND HIDDEN TEXT

BEGIN_ANONYMOUS_NAMESPACE 

struct Keywords {
  // maparea keywords
  miniexp_t url, rect, oval, poly, line, text;
  miniexp_t none, xxor, border;
  miniexp_t shadow_in, shadow_out, shadow_ein, shadow_eout;
  miniexp_t border_avis, hilite, opacity;
  miniexp_t arrow, width, lineclr;
  miniexp_t backclr, textclr, pushpin;
  // hiddentext keywords
  miniexp_t h[7];
  Keywords() {
#define S(n,s) n=miniexp_symbol(s)
#define D(n) S(n,#n)
    // maparea
    D(url); D(rect); D(oval); D(poly); D(line); D(text);
    D(none); S(xxor,"xor"); D(border); 
    D(shadow_in); D(shadow_out); D(shadow_ein); D(shadow_eout);
    D(border_avis); D(hilite); D(opacity);
    D(arrow); D(width); D(lineclr);
    D(backclr); D(textclr); D(pushpin);
    // hidden text
    S(h[0],"page"); S(h[1],"column"); S(h[2],"region"); 
    S(h[3],"para"); S(h[4],"line"); S(h[5],"word"); S(h[6],"char");
#undef D
#undef S
  }
};

Q_GLOBAL_STATIC(Keywords, keywords)

END_ANONYMOUS_NAMESPACE


MapArea::MapArea()
{
  expr = miniexp_nil;
  url = target = comment = miniexp_nil;
  areaType = borderType = miniexp_nil;
  borderWidth = 1;
  borderAlwaysVisible = false;
  hiliteOpacity = 50;
  lineArrow = false;
  lineWidth = 1;
  foregroundColor = Qt::black;
  pushpin = false;
  rectNeedsRotation = false;
}

bool
MapArea::error(const char *err, int pageno, miniexp_t info)
{
  if (pageno >= 0)
    qWarning("Error in maparea for page %d\n%s\n%s\n", 
             pageno+1, err, miniexp_to_str(miniexp_pname(info, 72)) );
  return false;
}

bool
MapArea::parse(miniexp_t full, int pageno)
{
  Keywords &k = *keywords();
  expr = full;
  miniexp_t anno = miniexp_cdr(full);
  miniexp_t q = miniexp_car(anno);
  int itmp;
  // hyperlink
  if (miniexp_stringp(q)) {
    const char *s = miniexp_to_str(q);
    url = (s && s[0]) ? q : miniexp_nil;
  } else if (miniexp_consp(q) && miniexp_car(q)==k.url) {
    if (! (miniexp_stringp(miniexp_cadr(q)) && 
           miniexp_stringp(miniexp_caddr(q)) &&
           ! miniexp_cdddr(q) ) )
      return error("Bad url", pageno, q);
    url = miniexp_cadr(q);
    target = miniexp_caddr(q);
    const char *s = miniexp_to_str(url);
    if (! (s && s[0]))
      url = target = miniexp_nil;
  } else if (q)
    return error("Bad url", pageno, full);
  else
    error("Some viewers prefer \"\" over () for empty urls.",
          pageno, full );
  // comment
  anno = miniexp_cdr(anno);
  q = miniexp_car(anno);
  if (miniexp_stringp(q))
    {
      const char *s = miniexp_to_str(q);
      if (s && s[0])
        comment = q;
    }
  else if (q)
    return error("Bad comment", pageno, full);
  else
    error("Some viewers prefer \"\" over () for empty comments.",
          pageno, full );
  // area 
  anno = miniexp_cdr(anno);
  q = miniexp_car(anno);
  areaType = miniexp_car(q);
  if (areaType == k.rect || areaType == k.oval || areaType == k.text)
    {
      q = miniexp_cdr(q);
      if (!miniexp_get_rect(q, areaRect) || q)
        return error("Bad rectangle", pageno, miniexp_car(anno));
    } 
  else if (areaType == k.poly) 
    {
      q = miniexp_cdr(q);
      if (!miniexp_get_points(q, areaRect, areaPoints))
        return error("Bad polygon", pageno, miniexp_car(anno));
    } 
  else if (areaType == k.line) 
    {
      q = miniexp_cdr(q);
      if (!miniexp_get_points(q,areaRect,areaPoints) || areaPoints.size()!=2)
        return error("Bad line", pageno, miniexp_car(anno));
    } 
  else
    return error("Bad area", pageno, full);
  // remaining
  while (miniexp_consp(anno = miniexp_cdr(anno)))
    {
      q = miniexp_car(anno);
      miniexp_t s = miniexp_car(q);
      miniexp_t a = miniexp_cdr(q);
      // borders
      if (s==k.none || s==k.xxor || s==k.border 
          || s==k.shadow_in || s==k.shadow_out 
          || s==k.shadow_ein || s==k.shadow_eout )
        {
          if (s==k.none) 
            {
              borderWidth = 0;
            } 
          else if (s==k.xxor)
            {
              borderWidth = 1;
            } 
          else if (s==k.border) 
            {
              if (! miniexp_get_color(a, borderColor))
                return error("Color expected", pageno, q);
              borderWidth = 1;
            } 
          else 
            {
              if (! miniexp_get_int(a, itmp))
                return error("Integer expected", pageno, q);
              borderWidth = qBound(1, itmp, 32);
              if (areaType != k.rect)
                return error("Only for rectangle maparea", pageno, q);
            }
          if (borderType)
            error("Multiple border specification", pageno, full);
          borderType = s;
        }
      // border avis
      else if (s == k.border_avis)
        {
          borderAlwaysVisible = true;
        }
      // hilite
      else if (s == k.hilite)
        {
          if (areaType != k.rect)
            error("Only for rectangle maparea", pageno, q);
          else if (! miniexp_get_color(a, hiliteColor))
            return error("Color expected", pageno, q);
        }
      else if (s == k.opacity)
        {
          if (areaType != k.rect)
            error("Only for rectangle maparea", pageno, q);
          else if (! miniexp_get_int(a, itmp))
            return error("Integer expected", pageno, q);
          hiliteOpacity = qBound(0, itmp, 200);
        }
      // line stuff
      else if (s == k.arrow)
        {
          if (areaType != k.line)
            error("Only for line maparea", pageno, q);
          lineArrow = true;
        }
      else if (s == k.width)
        {
          if (areaType != k.line)
            error("Only for line maparea", pageno, q);
          if (! miniexp_get_int(a, itmp))
            return error("Integer expected", pageno, q);
          lineWidth = qBound(1, itmp, 32);
        }
      else if (s == k.lineclr)
        {
          if (areaType != k.line)
            error("Only for line maparea", pageno, q);
          if (! miniexp_get_color(a, foregroundColor))
            return error("Color expected", pageno, q);
        }
      // text stuff
      else if (s == k.backclr)
        {
          if (areaType != k.text)
            error("Only for text maparea", pageno, q);
          if (! miniexp_get_color(a, hiliteColor))
            return error("Color expected", pageno, q);
          hiliteOpacity = 200;
        }
      else if (s == k.textclr)
        {
          if (areaType != k.text)
            error("Only for text maparea", pageno, q);
          if (! miniexp_get_color(a, foregroundColor))
            return error("Color expected", pageno, q);
        }
      else if (s == k.pushpin)
        {
          if (areaType != k.text)
            error("Only for text maparea", pageno, q);
          pushpin = true;
        }
      else
        error("Unrecognized specification", pageno, q);
      // test for extra arguments
      if (a)
        error("Extra arguments were ignored", pageno, q);
    }
  // border type sanity checks
  if (areaType == k.line)
    {
      if (borderType && borderType != k.none)
        error("Line maparea should not specify a border.", pageno, expr);
      if (miniexp_stringp(url))
        error("Line maparea should not specify a url.", pageno, expr);
      borderType = miniexp_nil;
      url = miniexp_nil;
    }
  else if (borderType == miniexp_nil)
    {
      error("Maparea without border type defaults to (xor).", pageno, expr);
      borderType = k.xxor;
    }
  return true;
}

bool
MapArea::clicked(void)
{
  Keywords &k = *keywords();
  if (areaType == k.text && pushpin)
    {
      areaType = k.pushpin;     // fake area type
      borderType = miniexp_nil; // no border
      hiliteColor = QColor();   // no hilite
      return true;
    }
  else if (areaType == k.pushpin)
    {
      parse(expr);              // restore
      return true;
    }
  return false;
}

bool
MapArea::isClickable(bool hyperlink)
{
  Keywords &k = *keywords();
  if (url && hyperlink)
    return true;
  if (areaType == k.pushpin)
    return true;
  if (areaType == k.text && pushpin)
    return true;
  return false;
}

bool 
MapArea::hasTransient(bool allLinks)
{
  Keywords &k = *keywords();
  if (areaType == miniexp_nil)
    return false;
  if (allLinks && miniexp_stringp(url))
    return true;
  if (borderAlwaysVisible)
    return false;
  if (borderType == k.none || borderType == miniexp_nil)
    return false;
  return true;
}

bool 
MapArea::hasPermanent()
{
  Keywords &k = *keywords();
  if (hiliteColor.isValid() 
      || areaType == k.line || areaType == k.text 
      || areaType == k.pushpin )
    return true;
  return false;
}

void 
MapArea::maybeRotate(struct Page *p)
{
  if (rectNeedsRotation && p->dpi >= 0)
    {
      int rot = p->initialRot;
      if (rot > 0)
        {
          int rw = (rot&1) ? p->height : p->width;
          int rh = (rot&1) ? p->width : p->height;
          QRectMapper mapper;
          mapper.setMap(QRect(0,0,rw,rh), QRect(0,0,p->width,p->height));
          mapper.setTransform(rot, false, false);
          areaRect = mapper.mapped(areaRect);
        }
      rectNeedsRotation = false;
    }
}

QPainterPath 
MapArea::contour(QRectMapper &m, QPoint &offset)
{
  Keywords &k = *keywords();
  QRect rect = m.mapped(areaRect).translated(-offset);
  QPainterPath path;
  if (areaType == k.poly)
    {
      QPoint p = m.mapped(areaPoints[0]) - offset;
      path.moveTo(p);
      for (int j=1; j<areaPoints.size(); j++)
        path.lineTo(m.mapped(areaPoints[j]) - offset);
    }
  else if (areaType == k.oval)
    path.addEllipse(rect);
  else
    path.addRect(rect);
  path.closeSubpath();
  return path;
}

bool 
MapArea::contains(const QPoint &p)
{
  Keywords &k = *keywords();
  if (! areaRect.contains(p))
    return false;
  if (areaType == k.oval || areaType == k.poly)
    {
      QRectMapper nullmapper;
      QPoint nullpoint;
      QPainterPath path = contour(nullmapper, nullpoint);
      if (! path .contains(p))
        return false;
    }
  return true;
}

void 
MapArea::update(QWidget *w, QRectMapper &m, QPoint offset, bool permanent)
{
  // The mapper <m> maps page coordinates to 
  // widget coordinates translated by <offset>.
  Keywords &k = *keywords();
  int bw = borderWidth;
  QRect rect = m.mapped(areaRect).translated(-offset);
  if (! rect.intersects(w->rect()))
    {
      return;
    }
  else if (permanent && hiliteColor.isValid() && hiliteOpacity>0)
    {
      int bw2 = (bw / 2) + 1;
      w->update(rect.adjusted(-bw2-1, -bw2-1, bw2+1, bw2+1));
    }
  else if (permanent && (areaType == k.text || areaType == k.pushpin))
    {
      int bw2 = (bw / 2) + 1;
      w->update(rect.adjusted(-bw2-1, -bw2-1, bw2+1, bw2+1));
    }
  else if (areaType == k.oval || areaType == k.poly)
    {
      int bw2 = (bw / 2) + 1;
      QPainterPath path = contour(m, offset);
      rect.adjust(-bw2, -bw2, bw2, bw2);
      QBitmap bm(rect.width(),rect.height());
      bm.clear();
      QPainter paint;
      paint.begin(&bm);
      paint.translate(-rect.topLeft());
      paint.strokePath(path, QPen(Qt::black, bw+4));
      paint.end();
      QRegion region(bm);
      region.translate(rect.topLeft());
      w->update(region);
    }
  else
    {
      int bw2 = (bw / 2) + 1;
      QRegion region = rect.adjusted(-bw2-1, -bw2-1, bw2+1, bw2+1);
      region = region.subtracted(rect.adjusted(bw+1, bw+1, -bw-1, -bw-1));
      w->update(region);
    }  
}

void 
MapArea::paintBorder(QPaintDevice *w, QRectMapper &m, QPoint offset, bool allLinks)
{
  // The mapper <m> maps page coordinates to 
  // widget coordinates translated by <offset>.
  Keywords &k = *keywords();
  QRect rect = m.mapped(areaRect).translated(-offset);
  bool forceXor = false;
  if (allLinks && miniexp_stringp(url))
    forceXor = (borderType == k.none || borderType == miniexp_nil);
  QPainter paint(w);
  paint.setRenderHint(QPainter::Antialiasing);
  paint.setRenderHint(QPainter::TextAntialiasing);
  if (borderType == k.border)
    {
      QPainterPath path = contour(m, offset);
      paint.strokePath(path, QPen(borderColor, borderWidth));
    }
  else if (borderType == k.xxor || forceXor)
    {
      QPainterPath path = contour(m, offset);
      paint.strokePath(path, QPen(Qt::white, borderWidth));
      paint.strokePath(path, QPen(Qt::black, borderWidth, Qt::DashLine));
    }
  else if (borderType == k.shadow_in || borderType==k.shadow_out)
    {
      rect.adjust(0,0,+1,+1);
      int bw = borderWidth;
      bw = qMin(bw, qMin(rect.width()/4, rect.height()/4));
      QRect irect = rect.adjusted(bw, bw, -bw, -bw);
      paint.setPen(Qt::NoPen);
      QColor c1(255,255,255,100);
      QColor c2(0,0,0,100);
      QPolygon p(6);
      p[0] = irect.bottomLeft();
      p[1] = rect.bottomLeft();
      p[2] = rect.bottomRight();
      p[3] = rect.topRight();
      p[4] = irect.topRight();
      p[5] = irect.bottomRight();
      paint.setBrush((borderType==k.shadow_in) ? c1 : c2);
      paint.drawPolygon(p);      
      p[0] = irect.bottomLeft();
      p[1] = rect.bottomLeft();
      p[2] = rect.topLeft();
      p[3] = rect.topRight();
      p[4] = irect.topRight();
      p[5] = irect.topLeft();
      paint.setBrush((borderType==k.shadow_in) ? c2 : c1);
      paint.drawPolygon(p);      
    }
  else if (borderType == k.shadow_ein || borderType==k.shadow_eout)
    {
      rect.adjust(0,0,+1,+1);
      int bw = borderWidth;
      bw = qMin(bw, qMin(rect.width()/4, rect.height()/4));
      QRect irect = rect.adjusted(bw, bw, -bw, -bw);
      paint.setBrush(Qt::NoBrush);
      QColor c1(255,255,255,100);
      QColor c2(0,0,0,100);
      QPolygon p(3);
      paint.setPen(QPen((borderType==k.shadow_ein) ? c1 : c2, 1));
      p[0] = irect.bottomLeft();
      p[1] = irect.topLeft();
      p[2] = irect.topRight();
      paint.drawPolyline(p);
      p[0] = rect.bottomLeft();
      p[1] = rect.bottomRight();
      p[2] = rect.topRight();
      paint.drawPolyline(p);
      paint.setPen(QPen((borderType==k.shadow_ein) ? c2 : c1, 1));
      p[0] = irect.bottomLeft();
      p[1] = irect.bottomRight();
      p[2] = irect.topRight();
      paint.drawPolyline(p);
      p[0] = rect.bottomLeft();
      p[1] = rect.topLeft();
      p[2] = rect.topRight();
      paint.drawPolyline(p);
    }
}

void 
MapArea::paintPermanent(QPaintDevice *w, QRectMapper &m, 
                        QPoint offset, double z)
{
  // The mapper <m> maps page coordinates to 
  // widget coordinates translated by <offset>.
  Keywords &k = *keywords();
  QRect rect = m.mapped(areaRect).translated(-offset);
  if (hasPermanent())
    {
      QPainter paint(w);
      paint.setRenderHint(QPainter::Antialiasing);
      paint.setRenderHint(QPainter::TextAntialiasing);
      if (hiliteColor.isValid() && hiliteOpacity>0)
        {
          QColor color = hiliteColor;
          color.setAlpha(hiliteOpacity*255/200);
          paint.fillRect(rect, color);
        }
      if (areaType == k.line)
        {
          QPen pen(foregroundColor, lineWidth);
          pen.setJoinStyle(Qt::MiterJoin);
          paint.setPen(pen);
          paint.setBrush(foregroundColor);
          QPoint pFrom = m.mapped(areaPoints[0]) - offset;
          QPoint pTo = m.mapped(areaPoints[1]) - offset;
          if (lineArrow)
            {
              QPointF v = pFrom - pTo;
              qreal vn = sqrt(v.x() * v.x() + v.y() * v.y());
              v = v * qMin(0.25, (10.0 / vn));
              QPointF v90 = QPointF(-v.y()/2.0, v.x()/2.0);
              QPointF p[3];
              p[0] = pTo;
              p[1] = pTo + v - v90;
              p[2] = pTo + v + v90;
              paint.drawPolygon(p, 3);
              paint.drawLine(pFrom, pTo+ v * 0.5);
            }
          else
            {
              paint.drawLine(pFrom, pTo);
            }
        }
      else if (areaType == k.text)
        {
          int bw = borderWidth + 2;
          bw = qMin(bw, qMin(rect.width()/4, rect.height()/4));
          QRect r = rect.adjusted(bw, bw, -bw, -bw);
          QString s = miniexp_to_qstring(comment);
          paint.setPen(foregroundColor);
          int flags = Qt::AlignCenter|Qt::AlignVCenter|Qt::TextWordWrap;
          QFont font = paint.font();
          // estimate font size
          int size = (int)(z * 0.12);
          while (size > 1)
            {
              QRect br;
              font.setPixelSize(size);
              paint.setFont(font);
              paint.drawText(r,flags|Qt::TextDontPrint,s,&br);
              if (r.contains(br))
                {
                  // found good font size
                  paint.drawText(r,flags,s,0);
                  break;
                }
              size -= 1;
            }
        }
      else if (areaType == k.pushpin)
        {
          QPixmap pixmap;
          pixmap.load(":/images/text_pushpin.png");
          if (! pixmap.isNull())
            {
              QRect r = pixmap.rect().translated(rect.topLeft());
              if (r.width() > rect.width())
                {
                  r.setHeight(r.height() * rect.width() / r.width());
                  r.setWidth(rect.width());
                }
              if (r.height() > rect.height())
                {
                  r.setWidth(r.width() * rect.height() / r.height());
                  r.setHeight(rect.height());
                }
              paint.drawPixmap(r, pixmap);
            }
        }
    }
  if (! hasTransient())
    paintBorder(w, m, offset);
}

void 
MapArea::paintTransient(QPaintDevice *w, QRectMapper &m, QPoint offset, bool allLinks)
{
  if (hasTransient(allLinks))
    paintBorder(w, m, offset, allLinks);
}

void
QDjVuPrivate::prepareMapAreas(Page *p)
{
  // remove annotation mapareas.
  int j = p->mapAreas.size();
  while (--j >= 0)
    if (p->mapAreas[j].expr)
      p->mapAreas.removeAt(j);
  // parse annotations.
  if (p->annotations && p->annotations != miniexp_dummy)
    {
      miniexp_t *annos;
      annos = ddjvu_anno_get_hyperlinks(p->annotations);
      if (annos)
        {
          for (int i=0; annos[i]; i++)
            {
              MapArea data;
              miniexp_t anno = annos[i];
              if (data.parse(anno, p->pageno))
                {
                  data.clicked(); // collapse pushpins
                  p->mapAreas << data;
                }
            }
          free(annos);
        }
    }
}

bool
QDjVuPrivate::mustDisplayMapArea(MapArea *area)
{
  if (allLinksDisplayed)
    return true;
  if (area != currentMapArea)
    return false;
  if (hyperlinkEnabled && mouseEnabled)
    return true;
  return false;
}

void
QDjVuPrivate::checkCurrentMapArea(bool forceno)
{
  const Position &pos = cursorPos;
  Page *savedMapAreaPage = currentMapAreaPage;
  MapArea *savedMapArea = currentMapArea;
  Page *newMapAreaPage = 0;
  MapArea *newMapArea = 0;
  // locate new maparea
  if (displayMapAreas && !forceno && pos.inPage)
    if (pageMap.contains(pos.pageNo))
      {
        Page *p = pageMap[pos.pageNo];
        for (int i=0; i<p->mapAreas.size(); i++)
          {
            MapArea &area = p->mapAreas[i];
            area.maybeRotate(p);
            if (area.expr && 
                area.areaRect.contains(pos.posPage) &&
                area.contains(pos.posPage) )
              {
                newMapArea = &area;
                newMapAreaPage = p;
                break;
              }
          }
      }
  // change map area
  if (savedMapArea != newMapArea)
    {
      currentMapArea = 0;
      currentMapAreaPage = 0;
      if (savedMapArea)
        {
          if (savedMapArea->hasTransient())
            if (currentLinkDisplayed && !allLinksDisplayed)
              savedMapArea->update(widget->viewport(),
                                   savedMapAreaPage->mapper,
                                   visibleRect.topLeft());
          emit widget->pointerLeave(pos, savedMapArea->expr);
        }
      currentUrl = QString();
      currentTarget = QString();
      currentComment = QString();
      currentLinkDisplayed = false;
      if (newMapArea)
        {
          currentMapArea = newMapArea;
          currentMapAreaPage = newMapAreaPage;
          currentUrl = miniexp_to_qstring(currentMapArea->url);
          currentTarget = miniexp_to_qstring(currentMapArea->target);
          currentComment = miniexp_to_qstring(currentMapArea->comment);
          if (mustDisplayMapArea(newMapArea))
            newMapArea->update(widget->viewport(),
                               newMapAreaPage->mapper,
                               visibleRect.topLeft());
          emit widget->pointerEnter(pos, currentMapArea->expr);
        }
      widget->modifierEvent(modifiers, buttons, cursorPoint);
      widget->chooseTooltip();
    }
}

void 
QDjVuPrivate::showTransientMapAreas(bool b)
{
  b = b && displayMapAreas && hyperlinkEnabled && mouseEnabled;
  if (b != allLinksDisplayed)
    {
      Page *p;
      allLinksDisplayed = b;
      foreach (p, pageVisible)
        for (int i=0; i<p->mapAreas.size(); i++)
          {
            MapArea &area = p->mapAreas[i];
            area.maybeRotate(p);
            if (area.hasTransient(true))
              area.update(widget->viewport(), p->mapper, 
                          visibleRect.topLeft() );
          }
    }
}



/*! Removes all highlight rectangles 
  installed for page \a pageno. */

void 
QDjVuWidget::clearHighlights(int pageno)
{
  if (pageno>=0 && pageno<priv->pageData.size())
    {
      Page *p = &priv->pageData[pageno];
      int j = p->mapAreas.size();
      while (--j >= 0)
        if (! p->mapAreas[j].expr)
          {
            MapArea &area = p->mapAreas[j];
            if (priv->pageMap.contains(pageno) && p->dpi>0)
              area.update(viewport(), p->mapper, priv->visibleRect.topLeft(), true);
            priv->pixelCache.clear();
            p->mapAreas.removeAt(j);
          }
    }
}

/*! Add a new highlight rectangle with color \a color,
  at position \a x, \a y, \a w, \a h on page \a pageno. 
  Argument \a rc indicates whether coordinates are
  rotated (like annotations) or unrotated (like text). */

void 
QDjVuWidget::addHighlight(int pageno, int x, int y, int w, int h, 
                          QColor color, bool rc)
{
  if (!priv->docReady)
    priv->docinfo();
  if (pageno>=0 && pageno<priv->pageData.size() && w>0 && h>0)
    {
      Page *p = &priv->pageData[pageno];
      Keywords &k = *keywords();
      MapArea area;
      area.areaType = k.rect;
      area.areaRect = QRect(x,y,w,h);
      area.hiliteColor = color;
      area.hiliteColor.setAlpha(255);
      area.hiliteOpacity = color.alpha() * 200 / 255;
      area.borderType = miniexp_nil;
      area.rectNeedsRotation = rc;
      area.maybeRotate(p);
      p->mapAreas << area;
      priv->pixelCache.clear();
      if (priv->pageMap.contains(pageno) && p->dpi>0)
        area.update(viewport(), p->mapper, priv->visibleRect.topLeft(), true);
    }
}

void
QDjVuWidget::setAlternativeImage(const QString& fname)
{
    if (QSettings().value(_key_output_show_orig_on_space, _key_output_show_orig_on_space_def).toBool()) {
        priv->alternativeImageFilename = fname;
    }
}


/*! Returns the url string for the maparea located
  below the pointer. This function is valid when called from signal 
  \a pointerEnter, \a pointerLeave, or \a pointerClick. */

QString 
QDjVuWidget::linkUrl(void)
{
  if (priv->hyperlinkEnabled && priv->mouseEnabled)
    return priv->currentUrl;
  return QString();
}

/*! Returns the target string for the maparea located 
  below the pointer. This function is valid when called from signal 
  \a pointerEnter, \a pointerLeave, or \a pointerClick. */

QString 
QDjVuWidget::linkTarget(void)
{
  if (priv->hyperlinkEnabled && priv->mouseEnabled)
    return priv->currentTarget;
  return QString();
}

/*! Returns the comment string for the maparea located
  below the pointer. This function is valid when called from signal 
  \a pointerEnter, \a pointerLeave, or \a pointerClick. */

QString 
QDjVuWidget::linkComment(void)
{
  return priv->currentComment;
}


/*! Determine the text around a particular point.
  Produces an array of three string. The middle string
  is the selected information. All the strings represent the line. 
  Returns true on success. */
bool
QDjVuWidget::getTextForPointer(QString results[])
{
  Position &pos = priv->cursorPos;
  if (!pos.inPage || !priv->pageMap.contains(pos.pageNo))
    return false;
  Page *page = priv->pageMap[pos.pageNo];
  // map point
  int rot = page->initialRot;
  int w = (rot&1) ? page->height : page->width;
  int h = (rot&1) ? page->width : page->height;
  QRectMapper mapper;
  mapper.setMap(QRect(0,0,w,h), page->rect);
  mapper.setTransform(rot + priv->rotation, false, true);
  QPoint posRot = mapper.unMapped(pos.posView + page->rect.topLeft());
  // search zones
  Keywords &k = *keywords();
  miniexp_t q = page->hiddenText;
  miniexp_t l = page->hiddenText;
  while(miniexp_consp(q))
    {
      QRect rect;
      miniexp_t type = miniexp_car(q);
      miniexp_t r = miniexp_cdr(type);
      if (miniexp_consp(type))
        type = miniexp_car(type);
      if (miniexp_consp(r) && miniexp_get_rect_from_points(r, rect)
          && rect.adjusted(-2,-2,2,2).contains(posRot) ) 
        break;
      q = miniexp_cdr(q);
      int sep = 0;
      for (sep=0; sep<7; sep++)
        if (type == k.h[sep])
          break;
      if (sep <= 4)
        l = q;
    }
  if (miniexp_consp(q))
    {
      int state = 0;
      bool separator = false;
      results[0] = results[1] = results[2] = QString();
      while (miniexp_consp(l))
        {
          miniexp_t type = miniexp_car(l);
          miniexp_t s = miniexp_nth(5, type);
          bool is = miniexp_stringp(s);
          if (miniexp_consp(type))
            type = miniexp_car(type);
          if (state == 1)
            state = 2;
          if (is && separator)
            results[state] += " ";
          if (l == q)
            state = 1;
          l = miniexp_cdr(l);
          if (miniexp_stringp(s))
            results[state] += miniexp_to_qstring(s);
          int sep = 0;
          for (sep=0; sep<7; sep++)
            if (type == k.h[sep])
              break;
          if (sep <= 5)
            separator = true;
          else if (is)
            separator = false;
          if (sep <= 4 && state >= 1)
            break;
        }
      return true;
    }
  return false;
}


/*! Returns the text spanned with a particular 
  rectangle \a target in viewport coordinates.
  Returns the empty string if no text is available. */

QString
QDjVuWidget::getTextForRect(const QRect &vtarget)
{
  QRect target = vtarget;
  target.translate(priv->visibleRect.topLeft());
  Keywords &k = *keywords();
  int separator = 6;
  QString ans;
  Page *p;
  foreach(p, priv->pageVisible)
    {
      // quick check
      miniexp_t q = p->hiddenText;
      if (p->initialRot < 0 || q == miniexp_nil || q == miniexp_dummy)
        continue;
      QRect pagerect = target.intersected(p->rect);
      if (!p->hiddenText || pagerect.isEmpty())
        { separator = 0; continue; }
      // map rectangle
      int rot = p->initialRot;
      int w = (rot&1) ? p->height : p->width;
      int h = (rot&1) ? p->width : p->height;
      QRectMapper mapper;
      mapper.setMap(QRect(0,0,w,h), p->rect);
      mapper.setTransform(rot + priv->rotation, false, true);
      pagerect = mapper.unMapped(pagerect);
      // loop
      while (miniexp_consp(q))
        {
          miniexp_t r = miniexp_car(q);
          q = miniexp_cdr(q);
          miniexp_t type = r;
          if (miniexp_consp(r))
            {
              QRect rect;
              type = miniexp_car(r);
              r = miniexp_cdr(r);
              if (miniexp_symbolp(type) && 
                  miniexp_get_rect_from_points(r, rect) &&
                  pagerect.intersects(rect) )
                {
                  if (!ans.isEmpty())
                    {
                      if (separator == 0)
                        ans += "\n\f";
                      else if (separator <= 4)
                        ans += "\n";
                      else if (separator <= 5)
                        ans += " ";
                    }
                  separator = 6;
                  ans += miniexp_to_qstring(miniexp_car(r));
                }
            }
          for (int s=separator-1; s>=0; s--)
            if (type == k.h[s])
              separator = s;
        }
    }
  return ans;
}

/*! Returns the image corresponding to a particular 
  rectangle \a rect in viewport coordinates. 
  Image is painted using the current display mode. */

QImage
QDjVuWidget::getImageForRect(const QRect &rect)
{
  priv->changeSelectedRectangle(QRect());
  QImage img(rect.width(), rect.height(), QImage::Format_RGB32);
  QRegion region = rect;
  QPainter paint;
  paint.begin(&img);
  paint.translate(- rect.topLeft());
  priv->paintAll(paint, region);

  paint.end();
  return img;
}


/*! Returns a page coordinate rectangle describing
    the part of page \a pageNo that is covered by 
    rectangle \a rect. */

QRect   
QDjVuWidget::getSegmentForRect(const QRect &rect, int pageNo)
{
  QRect ans;
  if (priv->pageMap.contains(pageNo))
    {
      Page *p = priv->pageMap[pageNo];
      const QRect &v = priv->visibleRect;
      QRect d = rect.translated(v.topLeft()).intersected(p->rect);
      if (p->dpi && !d.isEmpty())
        ans = p->mapper.unMapped(d.intersected(p->rect));
    }
  return ans;
}

QImage
QDjVuWidget::renderImageForRect(int pageno, const QRect& r) const
{
    // simplified adaptation of DjVuLibre's QDjViewTiffExporter::doPage()
    QDjVuPage *page = priv->pageData[pageno].page;
    ddjvu_rect_t page_rect;
    page_rect.x = page_rect.y = 0;
    page_rect.w = ddjvu_page_get_width(*page);
    page_rect.h = ddjvu_page_get_height(*page);


    ddjvu_rect_t image_rect;
    if (r.isEmpty()) {
        image_rect = page_rect;
    } else {
        image_rect.x = r.x(); image_rect.y = r.y();
        image_rect.w = r.width();
        image_rect.h = r.height();
    }

    ddjvu_render_mode_t mode = DDJVU_RENDER_COLOR;

    QDjVuWidget::DisplayMode display_mode = displayMode();
    if (display_mode == QDjVuWidget::DISPLAY_STENCIL) {
        mode = DDJVU_RENDER_BLACK;
    } else if (display_mode == QDjVuWidget::DISPLAY_BG) {
        mode = DDJVU_RENDER_BACKGROUND;
    } else if (display_mode == QDjVuWidget::DISPLAY_FG) {
        mode = DDJVU_RENDER_FOREGROUND;
    }

    ddjvu_page_type_t type = ddjvu_page_get_type(*page);
    ddjvu_format_style_t style = DDJVU_FORMAT_RGB24;
    QImage::Format qimage_format = QImage::Format_RGB888;
    if (mode == DDJVU_RENDER_BLACK || type == DDJVU_PAGETYPE_BITONAL) {
        style = DDJVU_FORMAT_MSBTOLSB;
        qimage_format = QImage::Format_Mono;
    }

    ddjvu_format_t * fmt = ddjvu_format_create(style, 0, 0);
    ddjvu_format_set_row_order(fmt, 1);
    ddjvu_format_set_gamma(fmt, 2.2);

    // render and save
    char white = (char)0xff;
    int rowsize = image_rect.w * 3;

    if (style == DDJVU_FORMAT_MSBTOLSB) {
        white = 0x00;
        rowsize = (image_rect.w + 7) / 8;
    }

    QImage res_image;
    char *image = nullptr;
    if (! (image = (char*)malloc(rowsize * image_rect.h))) {
        throw(tr("renderImageForRect: Out of memory."));
    } else {
        if (! ddjvu_page_render(*page, mode, &page_rect, &image_rect, fmt, rowsize, image)) {
                memset(image, white, rowsize * image_rect.h);
        }

        res_image = QImage(image_rect.w, image_rect.h, qimage_format);
        const int bpl = res_image.bytesPerLine();
        assert(bpl >= rowsize);
        char *s = image;
        for (int i=0; i<(int)image_rect.h; i++, s+=rowsize) {
            memcpy(res_image.scanLine(i), s, rowsize);
        }

        if (style == DDJVU_FORMAT_MSBTOLSB) {
            res_image.invertPixels();
        }
    }

    if (fmt) {
        ddjvu_format_release(fmt);
    }

    if (image) {
        free(image);
    }

    return res_image;
}

/*! Indicate whether the page size if known */

bool
QDjVuWidget::pageSizeKnown(int pageno) const
{
  if (pageno >= 0 && pageno < priv->pageData.size())
    return (priv->pageData[pageno].dpi > 0);
  return false;
}


// ----------------------------------------
// SETTINGS FROM ANNOTATIONS



/*! \enum QDjVuWidget::Priority
  Levels for prioritized properties.
  Certain properties can be set at various priority levels for 
  defining the default values, annotation options, cgi options,
  and user specified values. Priority levels matter when one changes
  the current document or the current page: new property values
  might be unmasked when the annotation or cgi level values are unset.  
  The prioritized properties are: \a borderSize, \a zoom, 
  \a borderBrush, \a displayMode, \a horizAlign, \a vertAlign. 
*/


/*! The property setting function always set the prioritized 
  properties at priority level \a PRIORITY_USER. 
  This function downgrades all options set with a 
  priority higher than \a priority to priority 
  level \a priority.
*/

void
QDjVuWidget::reduceOptionsToPriority(Priority priority)
{
  priv->qBorderSize.reduce(priority);
  priv->qZoom.reduce(priority);
  priv->qBorderBrush.reduce(priority);
  priv->qDisplay.reduce(priority);
  priv->qHAlign.reduce(priority);
  priv->qVAlign.reduce(priority);
  // Apply changes
  priv->changeZoom();
  priv->changeHAlign();
  priv->changeVAlign();
  priv->changeDisplay();
  priv->changeBorderSize();
  priv->changeBorderBrush();
}



void
QDjVuPrivate::adjustSettings(Priority priority, miniexp_t annotations)
{
  // Reset everything
  qZoom.unset(priority);
  qBorderBrush.unset(priority);
  qDisplay.unset(priority);
  qHAlign.unset(priority);
  qVAlign.unset(priority);
  qBorderSize.unset(priority);

  // Analyse annotations
  if (annotations && annotations != miniexp_dummy)
    {
      const char *xbgcolor = ddjvu_anno_get_bgcolor(annotations);
      const char *xzoom = ddjvu_anno_get_zoom(annotations);
      const char *xmode = ddjvu_anno_get_mode(annotations);
      const char *xhorizalign = ddjvu_anno_get_horizalign(annotations);
      const char *xvertalign = ddjvu_anno_get_vertalign(annotations);
      // border color
      if (xbgcolor && xbgcolor[0]=='#')
        {
          QColor color(xbgcolor);
          qBorderBrush.set(priority, QBrush(color));
        }
      // zoom
      if (xzoom == QLatin1String("stretch"))
        qZoom.set(priority, ZOOM_STRETCH);
      else if (xzoom == QLatin1String("one2one"))
        qZoom.set(priority, ZOOM_ONE2ONE);
      else if (xzoom == QLatin1String("width"))
        qZoom.set(priority, ZOOM_FITWIDTH);
      else if (xzoom == QLatin1String("page"))
        qZoom.set(priority, ZOOM_FITPAGE);
      else if (xzoom && xzoom[0]=='d' && all_numbers(xzoom+1))
        {
          int zoom = strtol(xzoom+1,0,10);
          qZoom.set(priority, qBound((int)ZOOM_MIN, zoom, (int)ZOOM_MAX));
        }
      // display mode
      if (xmode == QLatin1String("color"))
        qDisplay.set(priority, DISPLAY_COLOR);
      else if (xmode == QLatin1String("bw") || 
               xmode == QLatin1String("black"))
        qDisplay.set(priority, DISPLAY_STENCIL);
      else if (xmode == QLatin1String("fore") || 
               xmode == QLatin1String("fg"))
        qDisplay.set(priority, DISPLAY_FG);
      else if (xmode == QLatin1String("back") || 
               xmode == QLatin1String("bg"))
        qDisplay.set(priority, DISPLAY_BG);
      // horiz align
      if (xhorizalign == QLatin1String("left"))
        qHAlign.set(priority, ALIGN_LEFT);
      else if (xhorizalign == QLatin1String("center"))
        qHAlign.set(priority, ALIGN_CENTER);
      else if (xhorizalign == QLatin1String("right"))
        qHAlign.set(priority, ALIGN_RIGHT);
      // vert align
      if (xvertalign == QLatin1String("top"))
        qVAlign.set(priority, ALIGN_TOP);
      else if (xvertalign == QLatin1String("center"))
        qVAlign.set(priority, ALIGN_CENTER);
      else if (xvertalign == QLatin1String("bottom"))
        qVAlign.set(priority, ALIGN_BOTTOM);
      // border size
      if (xbgcolor || xzoom || xhorizalign || xvertalign)
        qBorderSize.set(priority, 0);
    }
  
  // Apply changes
  changeZoom();
  changeHAlign();
  changeVAlign();
  changeDisplay();
  changeBorderSize();
  changeBorderBrush();
}



// ----------------------------------------
// PAINTING

void
QDjVuPrivate::trimPixelCache()
{
  int pos;
  int pixels = 0;
  int sz = pixelCache.size();
  for (pos = 0; pos < sz; pos++)
    {
      Cache &p = pixelCache[pos];
      pixels += p.rect.width() * p.rect.height();
      if (pixels > pixelCacheSize)
        break;
    }
  pos = qMin(pos, 256); // hard limit!
  while (sz-- > pos)
    pixelCache.removeLast();
}

void
QDjVuPrivate::addToPixelCache(const QRect &rect, QImage image)
{
  if (qMin(rect.width(), rect.height()) < 128)
    {
      Cache c;
      c.rect = rect;
      c.image = image;
      pixelCache.prepend(c);
      trimPixelCache();
    }
}

bool
QDjVuPrivate::paintHiddenText(QImage &img, Page *p, const QRect &drect, 
                              const QRect *prect)
{
  // do not paint anything unless display is display text
  bool okay = false;
  if (display != DISPLAY_TEXT)
    return okay;
  miniexp_t q = p->hiddenText;
  if (p->initialRot < 0 || q == miniexp_nil || q == miniexp_dummy)
    return okay;
  if (! prect)
    prect = &p->rect;
  // setup mapper
  int rot = p->initialRot;
  int w = (rot&1) ? p->height : p->width;
  int h = (rot&1) ? p->width : p->height;
  QRectMapper mapper;
  mapper.setMap(QRect(0,0,w,h), *prect);
  mapper.setTransform(rot + rotation, false, true);
  // setup painter
  QPainter paint(&img);
  paint.setRenderHint(QPainter::TextAntialiasing);
  QColor qblue = Qt::blue;
  QColor qwhite = Qt::white;
  qwhite.setAlpha(192);
  // loop over text
  int orientation = (4 - rot - rotation) & 3;
  while (miniexp_consp(q))
    {
      miniexp_t r = miniexp_car(q);
      q = miniexp_cdr(q);
      if (miniexp_consp(r))
        {
          QRect rect;
          miniexp_t s = miniexp_nth(5, r);
          miniexp_t g = miniexp_cdr(r);
          if (miniexp_symbolp(miniexp_car(r)) && 
              miniexp_stringp(s) &&
              miniexp_get_rect_from_points(g, rect) )
            {
              rect = mapper.mapped(rect);
              // heuristically choose orientation
              int w = rect.width();
              int h = rect.height();
              QString text = miniexp_to_qstring(s).trimmed();
              if (text.size() >= 2)
                if (((orientation & 1) && (w > 2*h)) ||
                    (!(orientation & 1) && (h > 2*w)) )
                  orientation ^= 1;
              if (!drect.intersects(rect))
                continue;
              // paint frame
              okay = true;
              rect.translate(-drect.topLeft());
              paint.setPen(QPen(qblue, 1));
              paint.setBrush(qwhite);
              paint.drawRect(rect.adjusted(0,0,-1,-1));
              if (w < 6 || h < 6)
                continue;
              // paint text
              paint.save();
              paint.setClipRect(rect);
              rect = rect.adjusted(2,2,-2,-2);
              paint.translate(rect.topLeft());
              paint.setPen(Qt::black);
              paint.setBrush(Qt::NoBrush);
              QFont font = paint.font();
              font.setPixelSize(128);
              QFontMetrics metrics(font);
#if QT_VERSION >= 0x40300 && !defined(Q_OS_WIN)
              QRect brect = metrics.tightBoundingRect(text);
#else
              QRect brect = metrics.boundingRect(text);
#endif
              double dw = rect.width();
              double dh = rect.height();
              double bw = brect.width();
              double bh = brect.height();
              if (dw > 1 && dh > 1 && bw > 1 && bh > 1)
                {
                  switch(orientation)
                    {
                    case 2:
                      paint.translate(dw, dh);
                      paint.rotate(180);
                      /* FALLTHRU */
                    default:
                      paint.scale(dw/bw, dh/bh);
                      break;
                    case 3:
                      paint.translate(dw, dh);
                      paint.rotate(180);
                      /* FALLTHRU */
                    case 1:
                      paint.translate(dw, 0);
                      paint.rotate(90);
                      paint.scale(dh/bw, dw/bh);
                      break;
                    }
                  paint.setFont(font);
                  paint.drawText(-brect.topLeft(), text);
                }
              paint.restore();
            }
        }
    }
  return okay;
}

bool
QDjVuPrivate::paintMapAreas(QImage &img, Page *p, const QRect &r, 
                            bool perm, const QRect *prect)
{
  // do not paint anything when disabled
  if (! displayMapAreas)
    return false;
  // setup mapper
  QRectMapper mapper;
  QRectMapper *pmapper = &p->mapper;
  if (! prect)
    prect = &p->rect;
  else {
    mapper.setMap(QRect(0,0,p->width,p->height), *prect);
    mapper.setTransform(rotation, false, true);
    pmapper = &mapper;
  }
  // warning: rect in desk coordinates.
  bool changed = false;
  for (int i=0; i<p->mapAreas.size(); i++)
    {
      QRect arect;
      MapArea &area = p->mapAreas[i];
      QPoint p1 = pmapper->mapped(area.areaRect.topLeft());
      QPoint p2 = pmapper->mapped(area.areaRect.bottomRight());
      int bw2 = (area.borderWidth + 1) / 2;
      arect.setLeft(qMin(p1.x(),p2.x())-bw2);
      arect.setTop(qMin(p1.y(),p2.y())-bw2);
      arect.setRight(qMax(p1.x(),p2.x())+bw2);
      arect.setBottom(qMax(p1.y(),p2.y())+bw2);
      // The above code is necessary because mapping a small rect 
      // can round to a rect of zero width or height.
      if (r.intersects(arect.adjusted(-8,-8,8,8)))
        {
          if (perm) 
            {
              double z = (prect->width() * p->dpi * 100.0) / (p->width * sdpi);
              area.paintPermanent(&img, *pmapper, r.topLeft(), z);
              changed = true;
            }
          else if (mustDisplayMapArea(&area) && area.hasTransient(allLinksDisplayed))
            {
              img.detach();
              area.paintTransient(&img, *pmapper, r.topLeft(), allLinksDisplayed);
              changed = true;
            }
        }
    }
  return changed;
}

bool
QDjVuPrivate::paintPage(QPainter &paint, Page *p, const QRegion &region)
{
  // check
  if (region.isEmpty())
    return true;

  if (displayAlternative) {
      if (!alternativeImagePtr) {
          alternativeImagePtr.reset(new QImage(alternativeImageFilename));
      }
      if (alternativeImagePtr->isNull()) {
          paint.fillRect(visibleRect, Qt::white);
          return true;
      }
  }

  if (p->dpi<=0 || p->page==0)
    return false;
  // invalidate cache when device pixel ratio changes
#if QT_VERSION >= 0x50200
  int dpr = paint.device()->devicePixelRatio();
  if (devicePixelRatio != dpr)
    pixelCache.clear();
  devicePixelRatio = dpr;
#else
  int dpr = 1;
#endif

  // caching
  QList<Cache>& cur_pixelCache = !displayAlternative ? pixelCache : pixelCacheAlt;
  QList<Cache*> cachelist;
  QRegion remainder = region;
  QPoint deskToView = - visibleRect.topLeft();
  for (int i=0; i<cur_pixelCache.size(); i++)
    {
      QRect rect = cur_pixelCache[i].rect.translated(deskToView);
      if (! region.intersects(rect)) continue;
      cachelist << &cur_pixelCache[i];
      remainder -= rect;
    }
  // cover
  QRegion cover = cover_region(remainder, p->viewRect);
  // prune cache list
  QRegion shown = cover;
  for (int i=cachelist.size()-1; i>=0; i--)
    {
      QRect r = cachelist[i]->rect.translated(deskToView);
      r = region.intersected(r).boundingRect();
      if (shown.intersected(r) == r)
        cachelist.removeAt(i);
      shown += r;
    }
  // process cached segments
  QRegion displayed;
  for (int i=0; i<cachelist.size(); i++)
    {
      Cache *cache = cachelist[i];
      QImage img = cache->image;
      QRect r = cache->rect;
#if QDJVUWIDGET_PIXMAP_CACHE
      if (cache->pixmap.isNull())
        cache->pixmap = QPixmap::fromImage(img, Qt::ThresholdDither);
      bool hastransient = paintMapAreas(img, p, r, false);
#else
      paintMapAreas(img, p, r, false);
#endif
      r.translate(deskToView);
      QRegion dr = region.intersected(r) - displayed;
      if (dr.isEmpty()) continue;
      QRect d = dr.boundingRect();
      displayed += d;
      QRect s = d.translated(-r.topLeft());
      if (dpr > 1) 
        s.setRect(s.left()*dpr, s.top()*dpr, s.width()*dpr, s.height()*dpr);
#if QDJVUWIDGET_PIXMAP_CACHE
      if (hastransient)
        paint.drawImage(d.topLeft(), img, s, Qt::ThresholdDither);
      else
        paint.drawPixmap(d.topLeft(), cachelist[i]->pixmap, s);
#else
      paint.drawImage(d.topLeft(), img, s, Qt::ThresholdDither);
#endif
    }
  // mode for new segments
  ddjvu_render_mode_t mode = DDJVU_RENDER_COLOR;
  if (display == DISPLAY_STENCIL)
    mode = DDJVU_RENDER_BLACK;
  else if (display == DISPLAY_BG)
    mode = DDJVU_RENDER_BACKGROUND;
  else if (display == DISPLAY_FG)
    mode = DDJVU_RENDER_FOREGROUND;
  // render new segments
  foreachrect(const QRect &ri, cover)
    {
      int rot;
      ddjvu_rect_t pr, rr;
      QRect r = ri.translated(visibleRect.topLeft());
      QImage img(r.width()*dpr, r.height()*dpr, QImage::Format_RGB32);
#if QT_VERSION >= 0x50200
      img.setDevicePixelRatio(dpr);
#endif
      QDjVuPage *dp = p->page;
      qrect_to_rect(r, rr, dpr);
      qrect_to_rect(p->rect, pr, dpr);
#if DDJVUAPI_VERSION < 18
      p->initialRot = ddjvu_page_get_initial_rotation(*dp);
#endif
      rot = p->initialRot + rotation;
      ddjvu_page_set_rotation(*dp, (ddjvu_page_rotation_t)(rot & 0x3));
      if (!displayAlternative) {
          if (! ddjvu_page_render(*dp, mode, &pr, &rr, renderFormat,
                                  img.bytesPerLine(), (char*)img.bits() ))
              return false;
          paintHiddenText(img, p, r);
          if (invertLuminance)
              invert_luminance(img);
          paintMapAreas(img, p, r, true);
      } else {
          if (rot) {
              QTransform trans;
              img = alternativeImagePtr->transformed(trans.rotate(-90*rot));
          } else {
              img = *alternativeImagePtr; // shadow copy
          }

          img = img.scaled(p->rect.size());
          img = img.copy(r.translated(-p->rect.topLeft()));
      }
      addToPixelCache(r, img);
      paintMapAreas(img, p, r, false);
      r.translate(deskToView);
      QRegion dr = region.intersected(r) - displayed;
      if (dr.isEmpty()) continue;
      QRect d = dr.boundingRect();
      displayed += d;
      QRect s = d.translated(-r.topLeft());
      if (dpr > 1) 
        s.setRect(s.left()*dpr, s.top()*dpr, s.width()*dpr, s.height()*dpr);
      paint.drawImage(d.topLeft(), img, s, Qt::ThresholdDither);
    }
  return true;
}

void
QDjVuPrivate::paintAll(QPainter &paint, const QRegion &paintRegion)
{
  // Document not ready yet
  if (pageLayout.isEmpty())
    {
      bool waiting = (docReady || !docFailed) && !docStopped;
      // patched by truf to get a more fancy look for stub page
      QSize viewport = widget->viewport()? widget->viewport()->size() : widget->size();
      viewport -= unknownSize;
      viewport /= 2;
      // other option is /*QPoint(borderSize,borderSize)*/, /*vewport - 2*QSize(borderSize,borderSize)*/
      QRect rect(QPoint(viewport.width(), viewport.height()), unknownSize);
      widget->paintEmpty(paint, rect, waiting, docStopped, docFailed);
      widget->paintDesk(paint, paintRegion-rect);
      widget->paintFrame(paint, rect, shadowSize);
      return;
    }
  // Document ready.
  Page *p;
  foreach(p, pageVisible)
    {
      // Paint page
      QRegion region = paintRegion & p->viewRect;
      if (! paintPage(paint, p, region))
        {
          // Cannot paint page yet
          ddjvu_status_t s = DDJVU_JOB_FAILED;
          if (p->page && *(p->page)) 
            s = ddjvu_page_decoding_status(*(p->page));
          if (!p->page && pageRequestTimer->isActive())
            s = DDJVU_JOB_STARTED;
          widget->paintEmpty(paint, p->viewRect, 
                             s==DDJVU_JOB_STARTED,
                             s==DDJVU_JOB_STOPPED,
                             s==DDJVU_JOB_FAILED);
        }
      // Paint frames to reduce flashing.
      if (frame)
        widget->paintFrame(paint, p->viewRect, shadowSize);
    }
  // Paint desk
  QRegion deskRegion = paintRegion;
  foreach(p, pageVisible)
    deskRegion -= p->viewRect;
  widget->paintDesk(paint, deskRegion);
  // Paint frames again
  if (frame)
    foreach(p, pageLayout)
    {
      QRect rect = p->rect.adjusted(0,0,shadowSize,shadowSize);
      if (visibleRect.intersects(rect))
        widget->paintFrame(paint, p->viewRect, shadowSize);
    }
  // Paint selected rectangle
  if (! selectedRect.isEmpty())
    {
      paint.setBrush(QColor(128,128,192,64));
      paint.setPen(QPen(QColor(64,64,96,255), 1));
      paint.drawRect(selectedRect.adjusted(0,0,-1,-1));
    }
  // Paint hour glass
  if (hourGlassRatio > 0)
    {
      QRect rect = hourGlassRect();
      if (! rect.isEmpty())
        {
          const int tp = 360*16;
          paint.setPen(QPen());
          paint.setBrush(QColor(128,128,192,128));
          paint.drawPie(rect, tp/4, (int)(tp*hourGlassRatio));
        }
    }
}



/*! \internal */
void 
QDjVuWidget::paintEvent(QPaintEvent *event)
{
  // mark maparea
  if (priv->currentMapArea)
    {
      MapArea *a = priv->currentMapArea;
      priv->currentLinkDisplayed = priv->mustDisplayMapArea(a);
    }
  // paint everything
  QPainter paint(viewport());
  QRegion region = event->region() & viewport()->rect();
  priv->paintAll(paint, region);
  // debugging code
#ifdef DEBUG_SHOW_PAINTED_AREAS
  QColor color(rand()%256,rand()%256,rand()%256);
  color.setAlpha(128);
  paint.setClipRegion(region);
  paint.fillRect(region.boundingRect(), color);
#endif
}


// ----------------------------------------
// USER INTERFACE


/*! Overridden \a QAbstractScrollArea virtual function. */
void 
QDjVuWidget::scrollContentsBy(int, int)
{
  if (! priv->changingSBars)
    {
      QScrollBar *hBar = horizontalScrollBar();
      QScrollBar *vBar = verticalScrollBar();
      QPoint p = priv->visibleRect.topLeft();
      if (hBar->maximum() > hBar->minimum())
        p.rx() = hBar->sliderPosition();
      if (vBar->maximum() > vBar->minimum())
        p.ry() = vBar->sliderPosition();
      QPoint np = p - priv->visibleRect.topLeft();
      priv->movePoint = priv->currentPoint;
      priv->movePos = priv->findPosition(np + priv->movePoint);
      priv->changeLayout(CHANGE_VIEW);
      priv->clearAnimation();
    }
}


/*! Overridden \a QAbstractScrollArea virtual function. */
bool 
QDjVuWidget::event(QEvent *event)
{
  // Actual widget resize instead of viewport.
  if (event->type() == QEvent::Resize)
    priv->layoutLoop = 0;
  // Default function
  return QAbstractScrollArea::event(event);
}

/*! Overridden \a QAbstractScrollArea virtual function. */
bool 
QDjVuWidget::viewportEvent(QEvent *event)
{
  switch (event->type())
    {
    case QEvent::Enter:
      // Install filter to capture modifiers
      QApplication::instance()->installEventFilter(priv);
      break;
    case QEvent::Leave:
      // Remove filter to capture modifiers
      QApplication::instance()->removeEventFilter(priv);
      // Uncheck any active map area
      priv->checkCurrentMapArea(true);
      break;
#if QT_VERSION >= 0x040600
    case QEvent::Gesture:
      gestureEvent(event);
      if (event->isAccepted())
        return true;
      break;
#endif
    default:
      break;
    }
  // The default function calls the following
  // QAbstractScrollArea handlers:
  // - resizeEvent, paintEvent
  // - mouse{Press,Release,Move}Event
  // - contextMenuEvent
  return QAbstractScrollArea::viewportEvent(event);
}


/*! Overridden \a QAbstractScrollArea virtual function. */
void 
QDjVuWidget::resizeEvent(QResizeEvent *event)
{
  QAbstractScrollArea::resizeEvent(event);
  int change = CHANGE_VISIBLE|CHANGE_SCROLLBARS;
  if (priv->zoom == ZOOM_FITWIDTH ||
      priv->zoom == ZOOM_FITPAGE ||
      priv->zoom == ZOOM_STRETCH )
    change |= CHANGE_SCALE;
  // Do not reset loop counter
  // Update layout immediately because Qt40 gets confused 
  // by asynchronous mixes of scrolls and resizes.
  // Also we do not want to reset the loop counter!
  priv->layoutChange |= change;
  priv->makeLayout();
}


/* capture modifier changes */
bool
QDjVuPrivate::eventFilter(QObject *, QEvent *event)
{
  QEvent::Type type = event->type();
  switch (type)
    {
    default:
      return false;
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
      Qt::KeyboardModifiers change = Qt::NoModifier;
      QKeyEvent *kevent = (QKeyEvent*)event;

      switch (kevent->key())
        {
        case Qt::Key_Shift:
          change = Qt::ShiftModifier; 
          break;
        case Qt::Key_Control:
          change = Qt::ControlModifier; 
          break;
        case Qt::Key_Alt:
          change = Qt::AltModifier; 
          break;
        case Qt::Key_Meta:
          change = Qt::MetaModifier; 
          break;
        default:
          return false;
        }
      if (type == QEvent::KeyPress)
        updateModifiers(modifiers | change, buttons);
      else
        updateModifiers(modifiers & ~change, buttons);        
    }
  return false;
}

void 
QDjVuPrivate::updateModifiers(Qt::KeyboardModifiers newModifiers,
                              Qt::MouseButtons newButtons)
{
  Qt::KeyboardModifiers oldModifiers = modifiers;
  Qt::MouseButtons oldButtons = buttons;
  modifiers = newModifiers;
  buttons = newButtons;
  if (modifiers != oldModifiers)
    showTransientMapAreas(modifiers == modifiersForLinks && 
                          modifiers != Qt::NoModifier );
  if (modifiers != oldModifiers ||
      buttons != oldButtons )
    widget->modifierEvent(modifiers, buttons, cursorPoint);
}

void 
QDjVuPrivate::changeSelectedRectangle(const QRect& rect)
{
  QRect normRect = rect.normalized();
  QRect newRect;
  if (normRect.width() >= 2 && normRect.height() >= 2)
    newRect = normRect;
  QRect oldRect = selectedRect;
  selectedRect = newRect;
  if (oldRect.isEmpty())
    widget->viewport()->update(newRect.adjusted(-2,-2,2,2));
  else if (newRect.isEmpty())
    widget->viewport()->update(oldRect.adjusted(-2,-2,2,2));    
  else
    {
      QRegion region = QRegion(newRect) ^ QRegion(oldRect);
      QRegion dilated;
      foreachrect(const QRect &r, region)
        dilated += r.adjusted(-2,-2,2,2);
      widget->viewport()->update(dilated);
    }   
}


/*! This function should be called from \a modifierEvent.
  It sets the specified cursor, initiates the interactively
  selection of a rectangular area, and returns immediately. 
  The interactive selection stops when all mouse buttons and
  modifiers are released.
*/

bool 
QDjVuWidget::startSelecting(const QPoint &point)
{
  priv->updatePosition(point, true);
  if (priv->dragMode == DRAG_NONE)
    {
      priv->dragStart = point;
      priv->dragMode = DRAG_SELECTING;
      return true;
    }
  return false;
}

/*! This function should be called from \a modifierEvent.
  It initiates panning the display area, and returns immediately. 
  Panning stops when all mouse buttons and modifiers are released. */

bool
QDjVuWidget::startPanning(const QPoint &point)
{
  priv->updatePosition(point, true);
  if (priv->dragMode == DRAG_NONE)
    {
      priv->dragStart = point;
      priv->dragMode = DRAG_PANNING;
      return true;
    }
  return false;
}

/*! This function should be called from \a modifierEvent.  
  It initiates selecting the current hyperlink and returns immediately. 
  Interaction stops when all mouse buttons and modifiers are released.  
  Interaction changes to panning of the mouse moves more than 8 pixels. */

bool
QDjVuWidget::startLinking(const QPoint &point)
{
  priv->updatePosition(point, true);
  if (priv->dragMode == DRAG_NONE)
    {
      priv->dragStart = point;
      priv->dragMode = DRAG_LINKING;
      return true;
    }
  return false;
}

/*! This function should be called from \a modifierEvent.  
  It initiates user interaction with the magnification lens
  and returns immediately.  Lensing stops when all mouse buttons 
  and modifiers are released. */

bool
QDjVuWidget::startLensing(const QPoint &point)
{
  if (priv->dragMode == DRAG_NONE 
      && priv->lensMag>0 && priv->lensSize>0 )
    {
      priv->dragStart = point;
      priv->dragMode = DRAG_LENSING;
      priv->lens = new QDjVuLens(priv->lensSize, priv->lensMag, priv, this);
      QRect r = priv->lens->geometry();
      QPoint p = viewport()->mapToGlobal(point);
      r.translate(p - r.center());
      priv->lens->setGeometry(r);
      priv->lens->show();
      return true;
    }
  return false;
}

/*! Terminates user interaction initiated by \a startSelecting,
  \a startPanning ot \a startLensing.  Returns \a true if such
  interaction was ongoing, \a false otherwise. */

bool
QDjVuWidget::stopInteraction(void)
{
  QRect temp;
  switch (priv->dragMode)
    {
    case DRAG_LINKING:
      priv->updatePosition(priv->cursorPoint, true);
      if (priv->currentMapArea && priv->currentMapArea->clicked())
        {
          priv->pixelCache.clear();
          priv->currentMapArea->update(priv->widget->viewport(),
                                       priv->currentMapAreaPage->mapper,
                                       priv->visibleRect.topLeft(),
                                       true); 
        }
      else if (priv->currentMapArea && priv->currentMapArea->url 
               && priv->hyperlinkEnabled )
        emit pointerClick(priv->cursorPos, priv->currentMapArea->expr);
      break;
    case DRAG_SELECTING:
      priv->updatePosition(priv->cursorPoint, true);
      temp = priv->selectedRect;
      emit pointerSelect(viewport()->mapToGlobal(priv->cursorPoint), temp);
      priv->changeSelectedRectangle(QRect());
      break;
    case DRAG_LENSING:
      priv->lens->hide();
      priv->lens->deleteLater();
      priv->lens = 0;
      break;
    case DRAG_PANNING:
      priv->updatePosition(priv->cursorPoint, true);
      break;
    default:
      return false;
    }
  priv->dragMode = DRAG_NONE;
  return true;
}

/*!
  This function is called when the set of depressed
  modifiers and mouse button changes while the cursor
  is at position \a point in the viewport.  It is also called 
  when the pointer crosses hyperlink boundaries or when the 
  gui interaction settings change.
*/

void 
QDjVuWidget::modifierEvent(Qt::KeyboardModifiers modifiers,
                           Qt::MouseButtons buttons, 
                           QPoint point)
{
  if (priv->dragMode != DRAG_NONE &&
      modifiers == Qt::NoModifier &&
      buttons == Qt::NoButton)
    {
      stopInteraction();
    }
  if (priv->dragMode == DRAG_NONE)
    {
      if (! priv->mouseEnabled)
        {
          viewport()->setCursor(Qt::ArrowCursor);
        }
      else if (buttons & Qt::RightButton)
        {
          return; // Wait for the contextMenuEvent
        }
      else if (modifiers == Qt::NoModifier &&
               buttons == Qt::MidButton )
        {
          viewport()->setCursor(Qt::CrossCursor);
          startSelecting(point);
        }
      else if (modifiers == Qt::ControlModifier &&
               buttons == Qt::MidButton )
        {
          viewport()->setCursor(priv->cursHandClosed);
          startPanning(point);
        }
      else if (modifiers == priv->modifiersForLens &&
               modifiers != Qt::NoModifier )
        {
          viewport()->setCursor(Qt::CrossCursor);
          startLensing(point);
        }
      else if (modifiers == priv->modifiersForSelect)
        {
          viewport()->setCursor(Qt::CrossCursor);
          if (buttons != Qt::NoButton)
            startSelecting(point);
        }
      else if (priv->currentMapArea && 
               priv->currentMapArea->isClickable(priv->hyperlinkEnabled) )
        {
          viewport()->setCursor(Qt::ArrowCursor);
          if (buttons != Qt::NoButton)
            startLinking(point);
        }
      else if (buttons != Qt::NoButton)
        {
          viewport()->setCursor(priv->cursHandClosed);
          startPanning(point);
        }
      else
        {
          viewport()->setCursor(priv->cursHandOpen);
        }
    }
}

/*! Override \a QAbstractScrollArea virtual function. */
void 
QDjVuWidget::mousePressEvent(QMouseEvent *event)
{
  mouseMoveEvent(event);
}

/*! Override \a QAbstractScrollArea virtual function. */
void 
QDjVuWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  mouseMoveEvent(event);
}

/*! Override \a QAbstractScrollArea virtual function. */
void 
QDjVuWidget::mouseReleaseEvent(QMouseEvent *event)
{
  mouseMoveEvent(event);
}


bool
QDjVuPrivate::pointerScroll(const QPoint &p)
{
  int dx = 0;
  int dy = 0;
  QRect r = widget->viewport()->rect();
  if (p.x() >= r.right())
    dx = lineStep;
  else if (p.x() < r.left())
    dx = -lineStep;
  if (p.y() >= r.bottom())
    dy = lineStep;
  else if (p.y() < r.top())
    dy = -lineStep;
  if (dx == 0 && dy == 0)
    return false;
  movePos = cursorPos;
  movePoint = cursorPoint - QPoint(dx,dy);
  changeLayout(CHANGE_VIEW|CHANGE_SCROLLBARS);
  clearAnimation();
  return true;
}


/*! Override \a QAbstractScrollArea virtual function. */
void 
QDjVuWidget::mouseMoveEvent(QMouseEvent *event)
{
  event->accept();
  QPoint p = event->pos();
  priv->cursorPoint = p;
  priv->updateModifiers(event->modifiers(), event->buttons());
  switch (priv->dragMode)
    {
    case DRAG_LINKING:
      priv->updatePosition(event->pos());
      p = p - priv->dragStart;
      if (p.manhattanLength() <= 8)
        break;
      viewport()->setCursor(priv->cursHandClosed);
      priv->dragMode = DRAG_PANNING;
      // fall through
    case DRAG_PANNING:
      priv->movePoint = priv->cursorPoint;
      priv->movePos = priv->cursorPos;
      priv->changeLayout(CHANGE_VIEW|CHANGE_SCROLLBARS);
      priv->clearAnimation();
      break;
    case DRAG_SELECTING:
      priv->updatePosition(p);
      priv->changeSelectedRectangle(QRect(priv->dragStart, event->pos()));
      priv->pointerScroll(p);
      break;
    case DRAG_LENSING:
      priv->updatePosition(p);
      if (! priv->pointerScroll(p))
        priv->lens->recenter(event->pos());
      break;
    default:
      priv->updatePosition(p);
      break;
    }
}

void
QDjVuWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (priv->keyboardEnabled)
      {
        // Capturing this signal can override any key binding

        if (!event->isAutoRepeat() &&
                !priv->alternativeImageFilename.isEmpty() &&
                GlobalStaticSettings::checkKeysMatch(PageViewDisplayOriginal,
                                                     event->modifiers(),
                                                     (Qt::Key)event->key())) {
                priv->displayAlternative = false;
                viewport()->update();
        }
    }
}

/*! Override \a QAbstractScrollArea virtual function. */
void 
QDjVuWidget::keyPressEvent(QKeyEvent *event)
{
  if (priv->keyboardEnabled)
    {
      // Capturing this signal can override any key binding
      bool done = false;
      emit keyPressSignal(event, done);
      if (done) 
        return;

      if (!event->isAutoRepeat() &&
              !priv->alternativeImageFilename.isEmpty() &&
              GlobalStaticSettings::checkKeysMatch(PageViewDisplayOriginal,
                                                   event->modifiers(),
                                                   (Qt::Key)event->key())) {

          priv->displayAlternative = true;
          viewport()->update();
      }
      // Standard key bindings
      Qt::KeyboardModifiers modifiers = event->modifiers();
      switch(event->key())
        {
        case Qt::Key_1:
          setZoom(100);
          break;
        case Qt::Key_2:
          setZoom(200);
          break;
        case Qt::Key_3:
          setZoom(300);
          break;
        case Qt::Key_W:
          setZoom(ZOOM_FITWIDTH);
          break;
        case Qt::Key_P:
          setZoom(ZOOM_FITPAGE);
          break;
        case Qt::Key_BracketLeft: 
          priv->updateCurrentPoint(priv->cursorPos);
          rotateLeft(); 
          break;
        case Qt::Key_BracketRight: 
          priv->updateCurrentPoint(priv->cursorPos);
          rotateRight(); 
          break;
        case Qt::Key_Plus: 
        case Qt::Key_Equal: 
          priv->updateCurrentPoint(priv->cursorPos);
          zoomIn(); 
          break;
        case Qt::Key_Minus: 
          priv->updateCurrentPoint(priv->cursorPos);
          zoomOut(); 
          break;
        case Qt::Key_Home:
          if (modifiers == Qt::ControlModifier)
            firstPage();
          else if (modifiers == Qt::NoModifier)
            moveToPageTop();
          else 
            return;
          break;
        case Qt::Key_End:
          if (modifiers==Qt::ControlModifier)
            lastPage();
          else if (modifiers == Qt::NoModifier)
            moveToPageBottom();
          else 
            return;
          break;
        case Qt::Key_PageUp:
          prevPage(); 
          break;
        case Qt::Key_PageDown:
          nextPage(); 
          break;
        case Qt::Key_Space:
          if (modifiers==Qt::ShiftModifier)
            readPrev();
          else if (modifiers == Qt::NoModifier)
            readNext();
          else 
            return;
          break;
        case Qt::Key_B:
        case Qt::Key_Backspace:
          if (modifiers==Qt::ShiftModifier)
            readNext();
          else if (modifiers == Qt::NoModifier)
            readPrev();
          else
            return;
          break;
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down: 
          if (modifiers == Qt::NoModifier) {
            int svstep = verticalScrollBar()->singleStep();
            int shstep = horizontalScrollBar()->singleStep();
            verticalScrollBar()->setSingleStep(priv->lineStep);
            horizontalScrollBar()->setSingleStep(priv->lineStep);
            QAbstractScrollArea::keyPressEvent(event);           
            verticalScrollBar()->setSingleStep(svstep);
            horizontalScrollBar()->setSingleStep(shstep);
          } else
            return;
          break; 
        case Qt::Key_H:
          {
            int pageno = page();
            if (pageno>=0)
              clearHighlights(pageno);
            break;
          }
        default:
          // Return without accepting the event
          return;
        }
      // Only reach this point when key is accepted
      event->accept();
    }
}

/*! Override \a QAbstractScrollArea virtual function. */
void 
QDjVuWidget::contextMenuEvent (QContextMenuEvent *event)
{
  if (priv->contextMenu)
    {
      priv->contextMenu->exec(event->globalPos());
      event->accept();
    }
}


/*! Override \a QAbstractScrollArea virtual function. */
void 
QDjVuWidget::wheelEvent (QWheelEvent *event)
{
  if (priv->mouseEnabled)
    {
#if QT_VERSION < 0x50200
      int delta = 0;
      if (event->orientation() == Qt::Vertical)
        delta = event->delta();
#else
      int delta = event->angleDelta().y();
#endif
      bool zoom = priv->mouseWheelZoom;
      if (event->modifiers() == Qt::ControlModifier)
        zoom = ! zoom;
      if (zoom && delta)
        {
	  static int zWheel = 0;
	  zWheel += delta;
	  if (qAbs(zWheel) >= 120)
	    {
	      priv->updateCurrentPoint(priv->cursorPos);
	      if (zWheel > 0)
		zoomIn();
	      else
		zoomOut();
	      zWheel = 0;
	    }
	  return;
        }
    }
  QAbstractScrollArea::wheelEvent(event);
}


/*! New virtual function. */
void
QDjVuWidget::gestureEvent(QEvent *e)
{
#if QT_VERSION >= 0x040600
  if (e->type() == QEvent::Gesture)
    {
      QGestureEvent *g = (QGestureEvent*)(e);
      QPinchGesture *p = (QPinchGesture*)(g->gesture(Qt::PinchGesture));
      if (p)
	{
	  g->accept(p);
	  if (p->state() == Qt::GestureStarted)
	    priv->savedFactor = priv->zoomFactor;
	  if (p->state() == Qt::GestureCanceled)
	    setZoom(priv->savedFactor);
	  else if (p->changeFlags() & QPinchGesture::ScaleFactorChanged)
	    {
	      qreal z = priv->savedFactor * p->scaleFactor();
	      setZoom(qBound((int)ZOOM_MIN, (int)z, (int)ZOOM_MAX));
	    }
	  return;
	}
    }
#endif
  e->ignore();
}
 
bool
QDjVuPrivate::computeAnimation(const Position &pos, const QPoint &p)
{
  // no animation unless layout is finalized.
  if (layoutChange)
    return false;
  if (0 > pos.pageNo || pos.pageNo >= numPages)
    return false;
  Position currentPos = findPosition(p);
  Position targetPos = pos;
  Page *currentPage = pageMap.value(currentPos.pageNo, 0);
  Page *targetPage = pageMap.value(targetPos.pageNo, 0);
  // Standardize target position
  if (targetPos.inPage && targetPage && targetPage->dpi > 0)
    {
      QPoint dp = targetPage->mapper.mapped(targetPos.posPage);
      targetPos.hAnchor = targetPos.vAnchor = 0;
      targetPos.posView = dp - targetPage->rect.topLeft();
      targetPos.inPage = false;
    }
  if (targetPos.inPage)
    return false;
  // Both position on the same page
  if (currentPos.pageNo == targetPos.pageNo)
    {
      char dHAnchor = targetPos.hAnchor - currentPos.hAnchor;
      char dVAnchor = targetPos.vAnchor - currentPos.vAnchor;
      QPoint dPosView = targetPos.posView - currentPos.posView;
      const double profile[] = { 0.05, 0.3, 0.5, 0.7, 0.95 };
      const int profileSize = (int)(sizeof(profile)/sizeof(double));
      for (int i=0; i<profileSize; i++)
        {
          double s = profile[i];
          Position ipos = currentPos;
          ipos.hAnchor += (char)(dHAnchor * s);
          ipos.vAnchor += (char)(dVAnchor * s);
          ipos.posView.rx() += (int)(dPosView.x() * s);
          ipos.posView.ry() += (int)(dPosView.y() * s);
          ipos.inPage = false;
          ipos.doPage = false;
          animationPosition << ipos;
        }
      animationPoint = p;
      animationPosition << pos;
      animationTimer->start(10);
      return true;
    }
  // Both position in the same view
  if (currentPage && targetPage)
    {
      // compute desk positions.
      QPointF cPoint = currentPage->rect.topLeft() + currentPos.posView;
      if (currentPos.hAnchor > 0 && currentPos.hAnchor <= 100)
        cPoint.rx() += currentPage->rect.width() * currentPos.hAnchor / 100.0;
      if (currentPos.vAnchor > 0 && currentPos.vAnchor <= 100)
        cPoint.ry() += currentPage->rect.height() * currentPos.vAnchor / 100.0;
      QPointF tPoint = targetPage->rect.topLeft() + targetPos.posView;
      if (targetPos.hAnchor > 0 && targetPos.hAnchor <= 100)
        tPoint.rx() += targetPage->rect.width() * targetPos.hAnchor / 100.0;
      if (targetPos.vAnchor > 0 && targetPos.vAnchor <= 100)
        tPoint.ry() += targetPage->rect.height() * targetPos.vAnchor / 100.0;
      // compute delta
      QPointF v = tPoint - cPoint;
      qreal cs = max_stay_in_rect(cPoint, currentPage->rect, v);
      qreal ts = max_stay_in_rect(tPoint, targetPage->rect, -v);
      const double profile[] = { 0.05, 0.3, 0.5, 0.75, 1 };
      const int profileSize = (int)(sizeof(profile)/sizeof(double));
      for (int i=0; i<profileSize; i++)
        {
          QPointF ipoint = cPoint + cs * profile[i] * v;
          Position ipos;
          ipos.pageNo = currentPos.pageNo;
          ipos.hAnchor = ipos.vAnchor = 0;
          ipos.inPage = ipos.doPage = false;
          ipos.posView = ipoint.toPoint() - currentPage->rect.topLeft();
          animationPosition << ipos;
        }
      for (int i=profileSize-1; i>=0; i--)
        {
          QPointF ipoint = tPoint - ts * profile[i] * v;
          Position ipos;
          ipos.pageNo = targetPos.pageNo;
          ipos.hAnchor = ipos.vAnchor = 0;
          ipos.inPage = ipos.doPage = false;
          ipos.posView = ipoint.toPoint() - targetPage->rect.topLeft();
          animationPosition << ipos;
        }
      animationPoint = p;
      animationPosition << pos;
      animationTimer->start(10);
      return true;
    }
  // Too complex
  return false;
}


void
QDjVuPrivate::clearAnimation(void)
{
  animationTimer->stop();
  animationPosition.clear();
}

void
QDjVuPrivate::animate(void)
{
  if (! animationPosition.isEmpty())
    {
      movePoint = animationPoint;
      movePos = animationPosition.takeFirst();
      if (! pageMap.contains(movePos.pageNo))
        changeLayout(CHANGE_VIEW|CHANGE_SCROLLBARS);
      else
        changeLayout(CHANGE_PAGES|CHANGE_VIEW|CHANGE_SCROLLBARS);        
    }
  if (animationPosition.isEmpty())
    animationTimer->stop();
}


/*!
  Terminates all animations in progress and 
  make sure the page layout is fully updated.
*/
void
QDjVuWidget::terminateAnimation(void)
{
  while (priv->animationPosition.size() > 1)
    priv->animationPosition.removeFirst();
  priv->animate();
  if (priv->layoutChange)
    priv->makeLayout();
}


// ----------------------------------------
// VIRTUALS

/*!
  Paint the gray area that surrounds the pages.
  Overload this function to redefine the 
  appearance of this area.
*/

void 
QDjVuWidget::paintDesk(QPainter &p, const QRegion &region)
{
  p.save();
  p.setClipRegion(region, Qt::IntersectClip);
  p.setBrushOrigin(- priv->visibleRect.topLeft());
  p.fillRect(region.boundingRect(), priv->borderBrush);
  p.restore();
}

/*!
  Paint the frame and the shadow surrounding a page.
  Argument \a rect is the page rectangle.
  Overload this function to redefine the 
  appearance of the page frame.
*/

void 
QDjVuWidget::paintFrame(QPainter &p, const QRect &crect, int sw)
{
  QRect rect = crect.adjusted(0,0,-1,-1);
  if (!rect.isValid())
    return;
  // draw shadow 
  QBrush brush(QColor(0,0,0,80));
  p.setPen(Qt::NoPen);
  p.setBrush(brush);
  p.drawRect(rect.right()+1, rect.top()+sw, sw, rect.height());
  if (rect.width() > sw)
    p.drawRect(rect.left()+sw, rect.bottom()+1, rect.width()-sw, sw);
  // draw frame
  p.setBrush(Qt::NoBrush);
  QPen pen(Qt::black, 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
  p.setPen(pen);
  p.drawRect(rect);
}


/*!
  Paint the page area while waiting for page data
  or when the decoding job has failed or has been
  stopped. Argument \a rect is the page rectangle.
  Argument \a status indicates what is going on.
  Overload this function to redefine 
  the transient appearance of such pages.
*/

void 
QDjVuWidget::paintEmpty(QPainter &p, const QRect &rect,
                        bool waiting, bool stopped, bool error )
{
  QString name;
  QPixmap pixmap;
  if (waiting || !priv->doc) // truf: i'm using setDocument(0) to reset page and don't like how djvu_fail.png looks for this case
    name = ":/images/djvu_logo.png";
  else if (stopped)
    name = ":/images/djvu_stop.png";
  else if (error)
    name = ":/images/djvu_fail.png";
  if (!name.isEmpty())
    pixmap.load(name);
  // start painting
  QBrush brush(priv->white);
  p.fillRect(rect, brush);
  if (pixmap.isNull()) return;
  if (pixmap.width() > rect.width()) return;
  if (pixmap.height() > rect.height()) return;
  p.save();
  p.setClipRect(rect, Qt::IntersectClip);
  QSize s = pixmap.size() * 5 / 4;
  QPoint c = rect.center();
  int imin = (rect.left() - c.x())/s.width();
  int imax = (rect.right() - c.x())/s.width();
  int jmin = (rect.top() - c.y())/s.height();
  int jmax = (rect.bottom() - c.y())/s.height();
  c -= QPoint( pixmap.width()/2, pixmap.height()/2 );
  for (int i=imin; i<=imax; i++)
    for (int j=jmin; j<=jmax; j++)
      if (! ((i + j) & 1))
        p.drawPixmap(c.x()+i*s.width(), c.y()+j*s.height(), pixmap);
  p.restore();
}

/*!
  This function displays the current hyperlink comment in a tooltip.
  It is called whenever the pointer crosses hyperlink boundaries.
*/

void 
QDjVuWidget::chooseTooltip(void)
{
  QToolTip::showText(QPoint(), QString());
  priv->toolTipTimer->stop();
  if (!linkComment().isEmpty())
    priv->toolTipTimer->start(250);
}


void
QDjVuPrivate::makeToolTip(void)
{
  QWidget *w = widget->viewport();
  QPoint p = w->mapToGlobal(cursorPoint);
  QToolTip::showText(p, currentComment, w);
}


// ----------------------------------------
// QDJVULENS


QDjVuLens::QDjVuLens(int size, int magx, 
                     QDjVuPrivate *priv, QDjVuWidget *widget)
  : QWidget(widget, Qt::Window|Qt::Popup), priv(priv), widget(widget)
{
  mag = qBound(1,magx,10);
  size = qBound(50,size,500);
  setGeometry(0,0,size,size);
  setCursor(Qt::CrossCursor);
  setMouseTracking(true);
  connect(widget,SIGNAL(layoutChanged()), this, SLOT(redisplay()));
  QApplication::instance()->installEventFilter(this);
}

bool 
QDjVuLens::eventFilter(QObject *object, QEvent *event)
{
  return priv->eventFilter(object, event);
}

bool
QDjVuLens::event(QEvent *event)
{
  switch (event->type())
    {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
      {
        QMouseEvent *oldEvent = (QMouseEvent*)event;
        QPoint pos = oldEvent->globalPos();
        QMouseEvent newEvent(oldEvent->type(),
                             widget->viewport()->mapFromGlobal(pos),
                             pos, oldEvent->button(), oldEvent->buttons(),
                             oldEvent->modifiers());
        return widget->viewportEvent(&newEvent);
      }
    default:
      break;
    }
  return QWidget::event(event);
}

void 
QDjVuLens::recenter(const QPoint &p)
{
  QPoint npos;
  QRect rect = geometry();
  QPoint pos = rect.center();
  QRect vrect = widget->viewport()->rect(); 
  npos.rx() = qBound(vrect.left(), p.x(), vrect.right());
  npos.ry() = qBound(vrect.top(), p.y(), vrect.bottom());
  npos = widget->viewport()->mapToGlobal(npos);
  rect.translate(npos - pos);
  vrect.moveTo(widget->viewport()->mapToGlobal(vrect.topLeft()));
  setVisible(vrect.intersects(rect));
  setGeometry(rect);
#if QT_VERSION >= 0x50E00
  QCoreApplication::sendPostedEvents();
#else
  QCoreApplication::flush();
#endif
}

void 
QDjVuLens::refocus(void)
{
  QRect r = rect();
  QPoint p = mapToGlobal(rect().center());
  int sx = ( r.width() + mag - 1 ) / mag;
  int sy = ( r.height() + mag - 1 ) / mag;
  lensRect.setRect(0,0,sx,sy);
  QPoint vp = widget->viewport()->mapFromGlobal(p);
  lensRect.translate(vp - lensRect.center());
}

void 
QDjVuLens::moveEvent(QMoveEvent *event)
{
  refocus();
#if QT_VERSION >= 0x50200
  int dpr = devicePixelRatio();
#else
  int dpr = 1;
#endif
  QPoint delta = event->pos() - event->oldPos();
  QRect r = rect().adjusted(dpr,dpr,-dpr,-dpr);
  scroll(-mag*delta.x(), -mag*delta.y(), r);
}

void 
QDjVuLens::resizeEvent(QResizeEvent *)
{
  refocus();
}

void 
QDjVuLens::redisplay(void)
{
  refocus();
  update();
}

void 
QDjVuLens::paintEvent(QPaintEvent *event)
{
  // Copied from main painting code but simpler
  // TODO: maybe hyperlinks.
  QPainter paint(this);
  QRegion  paintRegion = event->region();
  QRegion  deskRegion = paintRegion;
  QRectMapper mapper(lensRect, rect());
  Page *p;
  ddjvu_render_mode_t mode = DDJVU_RENDER_COLOR;
  if (priv->display == DISPLAY_STENCIL)
    mode = DDJVU_RENDER_BLACK;
  else if (priv->display == DISPLAY_BG)
    mode = DDJVU_RENDER_BACKGROUND;
  else if (priv->display == DISPLAY_FG)
    mode = DDJVU_RENDER_FOREGROUND;
#if QT_VERSION >= 0x50200
  int dpr = devicePixelRatio();
#else
  int dpr = 1;
#endif
  foreach(p, priv->pageVisible)
    {
      QRect prect = mapper.mapped(p->viewRect);
      QRegion region = paintRegion & prect;
      if (region.isEmpty()) 
        continue;
      paint.fillRect(prect, Qt::white);
      if (p->dpi>0 && p->page)
        {
          QRegion prgn = cover_region(region, prect);
          foreachrect(const QRect &r, prgn)
            {
              int rot;
              ddjvu_rect_t pr, rr;
              QDjVuPage *dp = p->page;
              QImage img(r.width()*dpr, r.height()*dpr, QImage::Format_RGB32);
#if QT_VERSION >= 0x50200
              img.setDevicePixelRatio(dpr);
#endif
              qrect_to_rect(prect, pr, dpr);
              qrect_to_rect(r, rr, dpr);
              rot = p->initialRot + priv->rotation;
              ddjvu_page_set_rotation(*dp,(ddjvu_page_rotation_t)(rot & 0x3));
              if (ddjvu_page_render(*dp, mode, &pr, &rr, priv->renderFormat,
                                    img.bytesPerLine(), (char*)img.bits() ))
                {
                  // priv->paintHiddenText(img, p, r, &prect);
                  //   When displaying in hidden text mode, 
                  //   the above line paints the hidden text in the lens.
                  //   But the opposite behavior is more convenient 
                  //   for proofreading.
                  if (priv->invertLuminance)
                    invert_luminance(img);
                  priv->paintMapAreas(img, p, r, true, &prect);
                  paint.drawImage(r.topLeft(), img, img.rect(),
                                  Qt::ThresholdDither);
                }
            }
        }
      deskRegion -= prect;
    }
  widget->paintDesk(paint, deskRegion);
  if (priv->frame)
    foreach(p, priv->pageVisible)
    {
      QRect prect = mapper.mapped(p->viewRect);
      widget->paintFrame(paint, prect, priv->shadowSize);
    }
  paint.setPen(Qt::gray);
  paint.setBrush(Qt::NoBrush);
  paint.drawRect(rect().adjusted(0,0,-1,-1));
}


// ----------------------------------------
// MORE SLOTS

static int preferredZoom[] = {
  ZOOM_MIN, ZOOM_MIN,
  10, 25, 50, 75, 100, 150, 
  200, 300, 400, 600, 800, 
  ZOOM_MAX, ZOOM_MAX 
};


/*! Increase the zoom factor. */
void 
QDjVuWidget::zoomIn(void)
{
  int s = 1;
  int z = qBound((int)ZOOM_MIN, zoomFactor(), (int)ZOOM_MAX);
  while (z >= preferredZoom[s]) { s += 1; }
  setZoom(preferredZoom[s]);
}

/*! Decrease the zoom factor. */
void 
QDjVuWidget::zoomOut(void)
{
  int s = 1;
  int z = qBound((int)ZOOM_MIN, zoomFactor(), (int)ZOOM_MAX);
  while (z > preferredZoom[s]) { s += 1; }
  setZoom(preferredZoom[s-1]);
}

/*! Maximize the maginification in order to keep rectangle \a rect visible.
    The rectangle is expressed in viewport coordinates. */

void 
QDjVuWidget::zoomRect(QRect rect)
{
  rect = rect.normalized();
  Position pos = priv->findPosition(rect.center());
  pos.inPage = true;
  int z = zoomFactor();
  int zw = z * viewport()->width() / qMax(1,rect.width());
  int zh = z * viewport()->height() / qMax(1,rect.height());
  z = qBound((int)ZOOM_MIN, qMin(zw, zh), (int)ZOOM_MAX);
  setZoom(z);
  setPosition(pos, viewport()->rect().center());
}

/*! Rotate the page images clockwise. */
void 
QDjVuWidget::rotateRight(void)
{
  setRotation((rotation() - 1) & 0x3);
}

/*! Rotate the page images counter-clockwise. */
void 
QDjVuWidget::rotateLeft(void)
{
  setRotation((rotation() + 1) & 0x3);
}

/*! Set the display mode to \a DISPLAY_COLOR. */
void 
QDjVuWidget::displayModeColor(void)
{
  setDisplayMode(DISPLAY_COLOR);
}

/*! Set the display mode to \a DISPLAY_STENCIL. */
void 
QDjVuWidget::displayModeStencil(void)
{
  setDisplayMode(DISPLAY_STENCIL);
}

/*! Set the display mode to \a DISPLAY_BG. */
void 
QDjVuWidget::displayModeBackground(void)
{
  setDisplayMode(DISPLAY_BG);
}

/*! Set the display mode to \a DISPLAY_FG. */
void 
QDjVuWidget::displayModeForeground(void)
{
  setDisplayMode(DISPLAY_FG);
}

/*! Set the display mode to \a DISPLAY_TEXT. */
void 
QDjVuWidget::displayModeText(void)
{
  setDisplayMode(DISPLAY_TEXT);
}

/*! Move to the next page. */
void 
QDjVuWidget::nextPage(void)
{
  terminateAnimation();
  int pageNo = page();
  const QRect &dr = priv->deskRect;
  const QRect &vr = priv->visibleRect;
  const Page *pg = priv->pageMap.value(pageNo,0);
  while (pageNo < priv->numPages - 1)
    {
      pageNo += 1;
      if (priv->layoutChange || !priv->pageMap.contains(pageNo) || !pg)
        break;
      // Skip pages until we get a meaningful change.
      const QRect &pr = priv->pageMap[pageNo]->rect;
      if (! vr.contains(pr))
        if (pr.top() != pg->rect.top() || vr.width() < dr.width())
          break;
    }
  setPage(pageNo, true);
}

/*! Move to the previous page. */
void 
QDjVuWidget::prevPage(void)
{
  terminateAnimation();
  int pageNo = page();
  const QRect &dr = priv->deskRect;
  const QRect &vr = priv->visibleRect;
  const Page *pg = priv->pageMap.value(pageNo,0);
  while (pageNo > 0)
    {
      pageNo -= 1;
      if (priv->layoutChange || !priv->pageMap.contains(pageNo) || !pg)
        break;
      // Skip pages until we get a meaningful change.
      const QRect &pr = priv->pageMap[pageNo]->rect;
      if (! vr.contains(pr))
        if (pr.top() != pg->rect.top() || vr.width() < dr.width())
          break;
    }
  setPage(pageNo, true);
}

/*! Move to the first page. */
void 
QDjVuWidget::firstPage(void)
{
  terminateAnimation();
  Position pos;
  pos.pageNo = 0;
  pos.inPage = false;
  pos.posView = pos.posPage = QPoint(0,0);
  setPosition(pos);
}

/*! Move to the last page. */
void 
QDjVuWidget::lastPage(void)
{
  terminateAnimation();
  Position pos;
  pos.pageNo = priv->numPages - 1;
  pos.inPage = false;
  pos.posView = pos.posPage = QPoint(0,0);
  pos.hAnchor = pos.vAnchor = 100;
  QPoint p;
  p.rx() = priv->visibleRect.width() - priv->borderSize;
  p.ry() = priv->visibleRect.height() - priv->borderSize;
  setPosition(pos, p);
}


/*! Move to top of current page. */
void 
QDjVuWidget::moveToPageTop(void)
{
  terminateAnimation();
  Position pos = priv->currentPos;
  pos.inPage = false;
  pos.posView = pos.posPage = QPoint(0,0);
  pos.hAnchor = 0;
  pos.vAnchor = 0;
  setPosition(pos);
}


/*! Move to bottom of current page. */
void 
QDjVuWidget::moveToPageBottom(void)
{
  terminateAnimation();
  Position pos = priv->currentPos;
  pos.inPage = false;
  pos.posView = pos.posPage = QPoint(0,0);
  pos.hAnchor = 0;
  pos.vAnchor = 100;
  QPoint p;
  p.rx() = priv->visibleRect.width() - priv->borderSize;
  p.ry() = priv->visibleRect.height() - priv->borderSize;
  setPosition(pos, p);
}


/*! Move to next position in approximate reading order. */
void 
QDjVuWidget::readNext(void)
{
  terminateAnimation();
  QPoint point = priv->currentPoint;
  Position pos = priv->currentPos;
  int bs = priv->borderSize;
  if (priv->pageMap.contains(pos.pageNo))
    {
      Page *p = priv->pageMap[pos.pageNo];
      QRect nv = priv->visibleRect.adjusted(bs,bs,-bs,-bs);
      QRect v = priv->visibleRect.intersected(p->rect);
      if (v.bottom() < p->rect.bottom())
        {
          // scroll in page
          point.ry() = bs;
          nv.moveTop(v.bottom());
          nv.moveBottom(qMin(nv.bottom(), p->rect.bottom()));
          pos.inPage = false;
          pos.posView.ry() = nv.top() - p->rect.top();
          setPosition(pos, point);
          return;
        }
    }
  // find forward a partially hidden page
  while (pos.pageNo < priv->numPages - 1)
    {
      pos.pageNo += 1;
      pos.posView.ry() = 0;
      pos.vAnchor = 0;
      pos.hAnchor = 0;
      point.ry() = bs;
      if (! priv->pageMap.contains(pos.pageNo) ||
          ! priv->visibleRect.contains(priv->pageMap[pos.pageNo]->rect) )
        break;
    }
  pos.inPage = false;
  pos.doPage = true;
  setPosition(pos, point);
}


/*! Move to previous position in approximate reading order. */
void 
QDjVuWidget::readPrev(void)
{
  terminateAnimation();
  QPoint point = priv->currentPoint;
  Position pos = priv->currentPos;
  int bs = priv->borderSize;
  if (priv->pageMap.contains(pos.pageNo))
    {
      Page *p = priv->pageMap[pos.pageNo];
      QRect nv = priv->visibleRect.adjusted(bs,bs,-bs,-bs);
      QRect v = priv->visibleRect.intersected(p->rect);
      if (v.top() > p->rect.top())
        {
          // scroll in page
          point.ry() = bs;
          nv.moveBottom(v.top());
          nv.moveTop(qMax(nv.top(), p->rect.top()));
          pos.inPage = false;
          pos.posView.ry() = nv.top() - p->rect.top();
          setPosition(pos, point);
          return;
        }
    }
  // find backward a partially hidden page
  while (pos.pageNo > 0)
    {
      pos.pageNo -= 1;
      pos.posView.ry() = 0;
      pos.vAnchor = 100;
      pos.hAnchor = 0;
      point.ry() = priv->visibleRect.height() - bs;
      if (! priv->pageMap.contains(pos.pageNo) ||
          ! priv->visibleRect.contains(priv->pageMap[pos.pageNo]->rect) )
        break;
    }
  pos.inPage = false;
  pos.doPage = true;
  setPosition(pos, point);
}





// ----------------------------------------
// MOC

#include "qdjvuwidget.moc"


// ----------------------------------------
// DOCUMENTATION


/*! \class QDjVuWidget::PageInfo
  \brief Additional information for pointerPosition signal.
  Variable \a pageno is the page number.
  Variables \a width and \a height give the page size
  in pixels. Variable \a dpi gives the page resolution.
  Variable \a segment gives the page coordinates of 
  the page segment covered by the selected rectangle 
  All these variables are null when the page geometry 
  is still unknown.
*/
  
/*! \enum QDjVuWidget::DisplayMode
  Modes for displaying a DjVu image.
  Only mode \a DISPLAY_COLOR is really useful.
  The other modes show various layers of the DjVu image. 
*/

/*! \enum QDjVuWidget::Align
  Possible values of the alignment properties.
 */

/*! \fn QDjVuWidget::layoutChanged()
  This signal is emitted when the layout of the
  displayed pages has changed. This can happen
  when more data is available, or when the user
  changes zoom, rotation, layout, display mode, etc. */

/*! \fn QDjVuWidget::pageChanged(int pageno)
  This signal is emitted when the current page number is changed.
  This refer to the current page used when moving the document
  to keep reading (space bar). */

/*! \fn QDjVuWidget::pointerPosition(const Position &pos, const PageInfo &page)
  This signal is emitted when the mouse move. 
  Variable \a pos reports the position of the mouse in the document.
  Variable \a page reports the page geometry. */

/*! \fn QDjVuWidget::pointerEnter(const Position &pos, miniexp_t maparea)
  This signal is emitted when the mouse enters a maparea.
  Variable \a pos reports the position of the mouse in the document.
  Variable \a maparea contains the area description.
  Additional information can be obtained using functions
  \a linkUrl, \a linkTarget, and \a linkComment. */

/*! \fn QDjVuWidget::pointerLeave(const Position &pos, miniexp_t maparea)
  This signal is emitted when the mouse leaves a maparea.
  Variable \a pos reports the position of the mouse in the document.
  Variable \a maparea contains the area description.
  Additional information can be obtained using functions
  \a linkUrl, \a linkTarget, and \a linkComment. */

/*! \fn QDjVuWidget::pointerClick(const Position &pos, miniexp_t maparea)
  This signal is emitted when the user clicks an hyperlink maparea.
  Variable \a pos reports the position of the mouse in the document.
  Variable \a maparea contains the area description.
  Additional information can be obtained using functions
  \a linkUrl, \a linkTarget, and \a linkComment. */

/*! \fn QDjVuWidget::pointerSelect(const QPoint &pos, const QRect &rect)
  This signal is emitted when the user has selected a 
  rectangular area in the image. Argument \a rect gives the
  rectangle coordinates in the viewport. Argument \a pos
  is the global mouse position. Use \a getTextForRect and
  \a getImageForRect to extract associated information. */

/*! \fn QDjVuWidget::errorCondition(int pageno)
  This signal is emitted when an error occurs during decoding.
  Recorded error messages can be obtained using function \a errorMessage.
  Argument \a pageno is -1 if the error occurs at the document level.
  Otherwise it indicates the page with the error. */

/*! \fn QDjVuWidget::stopCondition(int pageno)
  This signal is emitted when decoding stops.
  Recorded error messages can be obtained using function \a errorMessage.
  Argument \a pageno is -1 if the condition occurs at the document level.
  Otherwise it indicates the page with the error. */

/*! \fn QDjVuWidget::keyPressSignal(QKeyEvent *event, bool &done)
  This signal is emitted from the widget's \a keyPressEvent routine.
  Setting flag \a done to \a true suppress any further processing.
  Otherwise default key bindings are applied. */

/*! \fn QDjVuWidget::error(QString message, QString filename, int lineno) 
  This signal relays all error messages generated by the \a QDjVuDocument and
  \a QDjVuPage objects associated with this widget. The last few error
  messages are saved and are available via function \a pastErrorMessage().
  Error message occur asynchronously and might be related to a background job
  such as prefetching pages.  Use instead signal \a errorCondition() as an
  indicator that an error has just ocurred. */

/*! \fn QDjVuWidget::info(QString message)
  This signal relays all informational messages generated by the \a
  QDjVuDocument and \a QDjVuPage objects associated with this widget. The last
  few messages are saved and are available via function \a pastInfoMessage().
  Note that informational message occur asynchronously and might not be
  relevant. */



/* -------------------------------------------------------------
   Local Variables:
   c++-font-lock-extra-types: ( "\\sw+_t" "[A-Z]\\sw*[a-z]\\sw*" )
   End:
   ------------------------------------------------------------- */
