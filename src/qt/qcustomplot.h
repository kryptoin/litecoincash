/***************************************************************************
**                                                                        **
**  QCustomPlot, an easy to use, modern plotting widget for Qt            **
**  Copyright (C) 2011-2018 Emanuel Eichhammer                            **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program.  If not, see http://www.gnu.org/licenses/.   **
**                                                                        **
****************************************************************************
**           Author: Emanuel Eichhammer                                   **
**  Website/Contact: http://www.qcustomplot.com/                          **
**             Date: 25.06.18                                             **
**          Version: 2.0.1                                                **
****************************************************************************/

#ifndef QCUSTOMPLOT_H
#define QCUSTOMPLOT_H

#include <QtCore/qglobal.h>

#define QT_NO_PRINTER

#ifdef QCUSTOMPLOT_USE_OPENGL
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define QCP_OPENGL_PBUFFER
#else
#define QCP_OPENGL_FBO
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 3, 0)
#define QCP_OPENGL_OFFSCREENSURFACE
#endif
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
#define QCP_DEVICEPIXELRATIO_SUPPORTED
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
#define QCP_DEVICEPIXELRATIO_FLOAT
#endif
#endif

#include <QtCore/QCache>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QFlags>
#include <QtCore/QMargins>
#include <QtCore/QMultiMap>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QStack>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QVector>
#include <QtGui/QMouseEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtGui/QWheelEvent>
#include <algorithm>
#include <limits>
#include <qmath.h>
#ifdef QCP_OPENGL_FBO
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFramebufferObject>
#ifdef QCP_OPENGL_OFFSCREENSURFACE
#include <QtGui/QOffscreenSurface>
#else
#include <QtGui/QWindow>
#endif
#endif
#ifdef QCP_OPENGL_PBUFFER
#include <QtOpenGL/QGLPixelBuffer>
#endif
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QtGui/QPrintEngine>
#include <QtGui/QPrinter>
#include <QtGui/QWidget>
#include <qnumeric.h>
#else
#include <QtNumeric>
#include <QtWidgets/QWidget>

#endif

class QCPPainter;
class QCustomPlot;
class QCPLayerable;
class QCPLayoutElement;
class QCPLayout;
class QCPAxis;
class QCPAxisRect;
class QCPAxisPainterPrivate;
class QCPAbstractPlottable;
class QCPGraph;
class QCPAbstractItem;
class QCPPlottableInterface1D;
class QCPLegend;
class QCPItemPosition;
class QCPLayer;
class QCPAbstractLegendItem;
class QCPSelectionRect;
class QCPColorMap;
class QCPColorScale;
class QCPBars;

#define QCUSTOMPLOT_VERSION_STR "2.0.1"
#define QCUSTOMPLOT_VERSION 0x020001

#if defined(QT_STATIC_BUILD)
#define QCP_LIB_DECL
#elif defined(QCUSTOMPLOT_COMPILE_LIBRARY)
#define QCP_LIB_DECL Q_DECL_EXPORT
#elif defined(QCUSTOMPLOT_USE_LIBRARY)
#define QCP_LIB_DECL Q_DECL_IMPORT
#else
#define QCP_LIB_DECL
#endif

#ifndef Q_DECL_OVERRIDE
#define Q_DECL_OVERRIDE
#endif

#ifndef Q_MOC_RUN
namespace QCP {
#else
class QCP {
  Q_GADGET
  Q_ENUMS(ExportPen)
  Q_ENUMS(ResolutionUnit)
  Q_ENUMS(SignDomain)
  Q_ENUMS(MarginSide)
  Q_FLAGS(MarginSides)
  Q_ENUMS(AntialiasedElement)
  Q_FLAGS(AntialiasedElements)
  Q_ENUMS(PlottingHint)
  Q_FLAGS(PlottingHints)
  Q_ENUMS(Interaction)
  Q_FLAGS(Interactions)
  Q_ENUMS(SelectionRectMode)
  Q_ENUMS(SelectionType)
public:
#endif

enum ResolutionUnit {
  ruDotsPerMeter

  ,
  ruDotsPerCentimeter

  ,
  ruDotsPerInch

};

enum ExportPen {
  epNoCosmetic

  ,
  epAllowCosmetic

};

enum SignDomain {
  sdNegative

  ,
  sdBoth

  ,
  sdPositive

};

enum MarginSide {
  msLeft = 0x01

  ,
  msRight = 0x02

  ,
  msTop = 0x04

  ,
  msBottom = 0x08

  ,
  msAll = 0xFF

  ,
  msNone = 0x00

};
Q_DECLARE_FLAGS(MarginSides, MarginSide)

enum AntialiasedElement {
  aeAxes = 0x0001

  ,
  aeGrid = 0x0002

  ,
  aeSubGrid = 0x0004

  ,
  aeLegend = 0x0008

  ,
  aeLegendItems = 0x0010

  ,
  aePlottables = 0x0020

  ,
  aeItems = 0x0040

  ,
  aeScatters = 0x0080

  ,
  aeFills = 0x0100

  ,
  aeZeroLine = 0x0200

  ,
  aeOther = 0x8000

  ,
  aeAll = 0xFFFF

  ,
  aeNone = 0x0000

};
Q_DECLARE_FLAGS(AntialiasedElements, AntialiasedElement)

enum PlottingHint {
  phNone = 0x000

  ,
  phFastPolylines = 0x001

  ,
  phImmediateRefresh = 0x002

  ,
  phCacheLabels = 0x004

};
Q_DECLARE_FLAGS(PlottingHints, PlottingHint)

enum Interaction {
  iRangeDrag = 0x001

  ,
  iRangeZoom = 0x002

  ,
  iMultiSelect = 0x004

  ,
  iSelectPlottables = 0x008

  ,
  iSelectAxes = 0x010

  ,
  iSelectLegend = 0x020

  ,
  iSelectItems = 0x040

  ,
  iSelectOther = 0x080

};
Q_DECLARE_FLAGS(Interactions, Interaction)

enum SelectionRectMode {
  srmNone

  ,
  srmZoom

  ,
  srmSelect

  ,
  srmCustom

};

enum SelectionType {
  stNone

  ,
  stWhole

  ,
  stSingleData

  ,
  stDataRange

  ,
  stMultipleDataRanges

};

inline bool isInvalidData(double value) {
  return qIsNaN(value) || qIsInf(value);
}

inline bool isInvalidData(double value1, double value2) {
  return isInvalidData(value1) || isInvalidData(value2);
}

inline void setMarginValue(QMargins &margins, QCP::MarginSide side, int value) {
  switch (side) {
  case QCP::msLeft:
    margins.setLeft(value);
    break;
  case QCP::msRight:
    margins.setRight(value);
    break;
  case QCP::msTop:
    margins.setTop(value);
    break;
  case QCP::msBottom:
    margins.setBottom(value);
    break;
  case QCP::msAll:
    margins = QMargins(value, value, value, value);
    break;
  default:
    break;
  }
}

inline int getMarginValue(const QMargins &margins, QCP::MarginSide side) {
  switch (side) {
  case QCP::msLeft:
    return margins.left();
  case QCP::msRight:
    return margins.right();
  case QCP::msTop:
    return margins.top();
  case QCP::msBottom:
    return margins.bottom();
  default:
    break;
  }
  return 0;
}

extern const QMetaObject staticMetaObject;
}

Q_DECLARE_OPERATORS_FOR_FLAGS(QCP::AntialiasedElements)
Q_DECLARE_OPERATORS_FOR_FLAGS(QCP::PlottingHints)
Q_DECLARE_OPERATORS_FOR_FLAGS(QCP::MarginSides)
Q_DECLARE_OPERATORS_FOR_FLAGS(QCP::Interactions)
Q_DECLARE_METATYPE(QCP::ExportPen)
Q_DECLARE_METATYPE(QCP::ResolutionUnit)
Q_DECLARE_METATYPE(QCP::SignDomain)
Q_DECLARE_METATYPE(QCP::MarginSide)
Q_DECLARE_METATYPE(QCP::AntialiasedElement)
Q_DECLARE_METATYPE(QCP::PlottingHint)
Q_DECLARE_METATYPE(QCP::Interaction)
Q_DECLARE_METATYPE(QCP::SelectionRectMode)
Q_DECLARE_METATYPE(QCP::SelectionType)

class QCP_LIB_DECL QCPVector2D {
public:
  QCPVector2D();
  QCPVector2D(double x, double y);
  QCPVector2D(const QPoint &point);
  QCPVector2D(const QPointF &point);

  double x() const { return mX; }
  double y() const { return mY; }
  double &rx() { return mX; }
  double &ry() { return mY; }

  void setX(double x) { mX = x; }
  void setY(double y) { mY = y; }

  double length() const { return qSqrt(mX * mX + mY * mY); }
  double lengthSquared() const { return mX * mX + mY * mY; }
  QPoint toPoint() const { return QPoint(mX, mY); }
  QPointF toPointF() const { return QPointF(mX, mY); }

  bool isNull() const { return qIsNull(mX) && qIsNull(mY); }
  void normalize();
  QCPVector2D normalized() const;
  QCPVector2D perpendicular() const { return QCPVector2D(-mY, mX); }
  double dot(const QCPVector2D &vec) const { return mX * vec.mX + mY * vec.mY; }
  double distanceSquaredToLine(const QCPVector2D &start,
                               const QCPVector2D &end) const;
  double distanceSquaredToLine(const QLineF &line) const;
  double distanceToStraightLine(const QCPVector2D &base,
                                const QCPVector2D &direction) const;

  QCPVector2D &operator*=(double factor);
  QCPVector2D &operator/=(double divisor);
  QCPVector2D &operator+=(const QCPVector2D &vector);
  QCPVector2D &operator-=(const QCPVector2D &vector);

private:
  double mX, mY;

  friend inline const QCPVector2D operator*(double factor,
                                            const QCPVector2D &vec);
  friend inline const QCPVector2D operator*(const QCPVector2D &vec,
                                            double factor);
  friend inline const QCPVector2D operator/(const QCPVector2D &vec,
                                            double divisor);
  friend inline const QCPVector2D operator+(const QCPVector2D &vec1,
                                            const QCPVector2D &vec2);
  friend inline const QCPVector2D operator-(const QCPVector2D &vec1,
                                            const QCPVector2D &vec2);
  friend inline const QCPVector2D operator-(const QCPVector2D &vec);
};
Q_DECLARE_TYPEINFO(QCPVector2D, Q_MOVABLE_TYPE);

inline const QCPVector2D operator*(double factor, const QCPVector2D &vec) {
  return QCPVector2D(vec.mX * factor, vec.mY * factor);
}
inline const QCPVector2D operator*(const QCPVector2D &vec, double factor) {
  return QCPVector2D(vec.mX * factor, vec.mY * factor);
}
inline const QCPVector2D operator/(const QCPVector2D &vec, double divisor) {
  return QCPVector2D(vec.mX / divisor, vec.mY / divisor);
}
inline const QCPVector2D operator+(const QCPVector2D &vec1,
                                   const QCPVector2D &vec2) {
  return QCPVector2D(vec1.mX + vec2.mX, vec1.mY + vec2.mY);
}
inline const QCPVector2D operator-(const QCPVector2D &vec1,
                                   const QCPVector2D &vec2) {
  return QCPVector2D(vec1.mX - vec2.mX, vec1.mY - vec2.mY);
}
inline const QCPVector2D operator-(const QCPVector2D &vec) {
  return QCPVector2D(-vec.mX, -vec.mY);
}

inline QDebug operator<<(QDebug d, const QCPVector2D &vec) {
  d.nospace() << "QCPVector2D(" << vec.x() << ", " << vec.y() << ")";
  return d.space();
}

class QCP_LIB_DECL QCPPainter : public QPainter {
  Q_GADGET
public:
  enum PainterMode {
    pmDefault = 0x00

    ,
    pmVectorized = 0x01

    ,
    pmNoCaching = 0x02

    ,
    pmNonCosmetic = 0x04

  };
  Q_ENUMS(PainterMode)
  Q_FLAGS(PainterModes)
  Q_DECLARE_FLAGS(PainterModes, PainterMode)

  QCPPainter();
  explicit QCPPainter(QPaintDevice *device);

  bool antialiasing() const { return testRenderHint(QPainter::Antialiasing); }
  PainterModes modes() const { return mModes; }

  void setAntialiasing(bool enabled);
  void setMode(PainterMode mode, bool enabled = true);
  void setModes(PainterModes modes);

  bool begin(QPaintDevice *device);
  void setPen(const QPen &pen);
  void setPen(const QColor &color);
  void setPen(Qt::PenStyle penStyle);
  void drawLine(const QLineF &line);
  void drawLine(const QPointF &p1, const QPointF &p2) {
    drawLine(QLineF(p1, p2));
  }
  void save();
  void restore();

  void makeNonCosmetic();

protected:
  PainterModes mModes;
  bool mIsAntialiasing;

  QStack<bool> mAntialiasingStack;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QCPPainter::PainterModes)
Q_DECLARE_METATYPE(QCPPainter::PainterMode)

class QCP_LIB_DECL QCPAbstractPaintBuffer {
public:
  explicit QCPAbstractPaintBuffer(const QSize &size, double devicePixelRatio);
  virtual ~QCPAbstractPaintBuffer();

  QSize size() const { return mSize; }
  bool invalidated() const { return mInvalidated; }
  double devicePixelRatio() const { return mDevicePixelRatio; }

  void setSize(const QSize &size);
  void setInvalidated(bool invalidated = true);
  void setDevicePixelRatio(double ratio);

  virtual QCPPainter *startPainting() = 0;
  virtual void donePainting() {}
  virtual void draw(QCPPainter *painter) const = 0;
  virtual void clear(const QColor &color) = 0;

protected:
  QSize mSize;
  double mDevicePixelRatio;

  bool mInvalidated;

  virtual void reallocateBuffer() = 0;
};

class QCP_LIB_DECL QCPPaintBufferPixmap : public QCPAbstractPaintBuffer {
public:
  explicit QCPPaintBufferPixmap(const QSize &size, double devicePixelRatio);
  virtual ~QCPPaintBufferPixmap();

  virtual QCPPainter *startPainting() Q_DECL_OVERRIDE;
  virtual void draw(QCPPainter *painter) const Q_DECL_OVERRIDE;
  void clear(const QColor &color) Q_DECL_OVERRIDE;

protected:
  QPixmap mBuffer;

  virtual void reallocateBuffer() Q_DECL_OVERRIDE;
};

#ifdef QCP_OPENGL_PBUFFER
class QCP_LIB_DECL QCPPaintBufferGlPbuffer : public QCPAbstractPaintBuffer {
public:
  explicit QCPPaintBufferGlPbuffer(const QSize &size, double devicePixelRatio,
                                   int multisamples);
  virtual ~QCPPaintBufferGlPbuffer();

  virtual QCPPainter *startPainting() Q_DECL_OVERRIDE;
  virtual void draw(QCPPainter *painter) const Q_DECL_OVERRIDE;
  void clear(const QColor &color) Q_DECL_OVERRIDE;

protected:
  QGLPixelBuffer *mGlPBuffer;
  int mMultisamples;

  virtual void reallocateBuffer() Q_DECL_OVERRIDE;
};
#endif

#ifdef QCP_OPENGL_FBO
class QCP_LIB_DECL QCPPaintBufferGlFbo : public QCPAbstractPaintBuffer {
public:
  explicit QCPPaintBufferGlFbo(const QSize &size, double devicePixelRatio,
                               QWeakPointer<QOpenGLContext> glContext,
                               QWeakPointer<QOpenGLPaintDevice> glPaintDevice);
  virtual ~QCPPaintBufferGlFbo();

  virtual QCPPainter *startPainting() Q_DECL_OVERRIDE;
  virtual void donePainting() Q_DECL_OVERRIDE;
  virtual void draw(QCPPainter *painter) const Q_DECL_OVERRIDE;
  void clear(const QColor &color) Q_DECL_OVERRIDE;

protected:
  QWeakPointer<QOpenGLContext> mGlContext;
  QWeakPointer<QOpenGLPaintDevice> mGlPaintDevice;
  QOpenGLFramebufferObject *mGlFrameBuffer;

  virtual void reallocateBuffer() Q_DECL_OVERRIDE;
};
#endif

class QCP_LIB_DECL QCPLayer : public QObject {
  Q_OBJECT

  Q_PROPERTY(QCustomPlot *parentPlot READ parentPlot)
  Q_PROPERTY(QString name READ name)
  Q_PROPERTY(int index READ index)
  Q_PROPERTY(QList<QCPLayerable *> children READ children)
  Q_PROPERTY(bool visible READ visible WRITE setVisible)
  Q_PROPERTY(LayerMode mode READ mode WRITE setMode)

public:
  enum LayerMode {
    lmLogical

    ,
    lmBuffered

  };
  Q_ENUMS(LayerMode)

  QCPLayer(QCustomPlot *parentPlot, const QString &layerName);
  virtual ~QCPLayer();

  QCustomPlot *parentPlot() const { return mParentPlot; }
  QString name() const { return mName; }
  int index() const { return mIndex; }
  QList<QCPLayerable *> children() const { return mChildren; }
  bool visible() const { return mVisible; }
  LayerMode mode() const { return mMode; }

  void setVisible(bool visible);
  void setMode(LayerMode mode);

  void replot();

protected:
  QCustomPlot *mParentPlot;
  QString mName;
  int mIndex;
  QList<QCPLayerable *> mChildren;
  bool mVisible;
  LayerMode mMode;

  QWeakPointer<QCPAbstractPaintBuffer> mPaintBuffer;

  void draw(QCPPainter *painter);
  void drawToPaintBuffer();
  void addChild(QCPLayerable *layerable, bool prepend);
  void removeChild(QCPLayerable *layerable);

private:
  Q_DISABLE_COPY(QCPLayer)

  friend class QCustomPlot;
  friend class QCPLayerable;
};
Q_DECLARE_METATYPE(QCPLayer::LayerMode)

class QCP_LIB_DECL QCPLayerable : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool visible READ visible WRITE setVisible)
  Q_PROPERTY(QCustomPlot *parentPlot READ parentPlot)
  Q_PROPERTY(QCPLayerable *parentLayerable READ parentLayerable)
  Q_PROPERTY(QCPLayer *layer READ layer WRITE setLayer NOTIFY layerChanged)
  Q_PROPERTY(bool antialiased READ antialiased WRITE setAntialiased)

public:
  QCPLayerable(QCustomPlot *plot, QString targetLayer = QString(),
               QCPLayerable *parentLayerable = 0);
  virtual ~QCPLayerable();

  bool visible() const { return mVisible; }
  QCustomPlot *parentPlot() const { return mParentPlot; }
  QCPLayerable *parentLayerable() const { return mParentLayerable.data(); }
  QCPLayer *layer() const { return mLayer; }
  bool antialiased() const { return mAntialiased; }

  void setVisible(bool on);
  Q_SLOT bool setLayer(QCPLayer *layer);
  bool setLayer(const QString &layerName);
  void setAntialiased(bool enabled);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const;

  bool realVisibility() const;

Q_SIGNALS:
  void layerChanged(QCPLayer *newLayer);

protected:
  bool mVisible;
  QCustomPlot *mParentPlot;
  QPointer<QCPLayerable> mParentLayerable;
  QCPLayer *mLayer;
  bool mAntialiased;

  virtual void parentPlotInitialized(QCustomPlot *parentPlot);
  virtual QCP::Interaction selectionCategory() const;
  virtual QRect clipRect() const;
  virtual void applyDefaultAntialiasingHint(QCPPainter *painter) const = 0;
  virtual void draw(QCPPainter *painter) = 0;

  virtual void selectEvent(QMouseEvent *event, bool additive,
                           const QVariant &details,
                           bool *selectionStateChanged);
  virtual void deselectEvent(bool *selectionStateChanged);

  virtual void mousePressEvent(QMouseEvent *event, const QVariant &details);
  virtual void mouseMoveEvent(QMouseEvent *event, const QPointF &startPos);
  virtual void mouseReleaseEvent(QMouseEvent *event, const QPointF &startPos);
  virtual void mouseDoubleClickEvent(QMouseEvent *event,
                                     const QVariant &details);
  virtual void wheelEvent(QWheelEvent *event);

  void initializeParentPlot(QCustomPlot *parentPlot);
  void setParentLayerable(QCPLayerable *parentLayerable);
  bool moveToLayer(QCPLayer *layer, bool prepend);
  void applyAntialiasingHint(QCPPainter *painter, bool localAntialiased,
                             QCP::AntialiasedElement overrideElement) const;

private:
  Q_DISABLE_COPY(QCPLayerable)

  friend class QCustomPlot;
  friend class QCPLayer;
  friend class QCPAxisRect;
};

class QCP_LIB_DECL QCPRange {
public:
  double lower, upper;

  QCPRange();
  QCPRange(double lower, double upper);

  bool operator==(const QCPRange &other) const {
    return lower == other.lower && upper == other.upper;
  }
  bool operator!=(const QCPRange &other) const { return !(*this == other); }

  QCPRange &operator+=(const double &value) {
    lower += value;
    upper += value;
    return *this;
  }
  QCPRange &operator-=(const double &value) {
    lower -= value;
    upper -= value;
    return *this;
  }
  QCPRange &operator*=(const double &value) {
    lower *= value;
    upper *= value;
    return *this;
  }
  QCPRange &operator/=(const double &value) {
    lower /= value;
    upper /= value;
    return *this;
  }
  friend inline const QCPRange operator+(const QCPRange &, double);
  friend inline const QCPRange operator+(double, const QCPRange &);
  friend inline const QCPRange operator-(const QCPRange &range, double value);
  friend inline const QCPRange operator*(const QCPRange &range, double value);
  friend inline const QCPRange operator*(double value, const QCPRange &range);
  friend inline const QCPRange operator/(const QCPRange &range, double value);

  double size() const { return upper - lower; }
  double center() const { return (upper + lower) * 0.5; }
  void normalize() {
    if (lower > upper)
      qSwap(lower, upper);
  }
  void expand(const QCPRange &otherRange);
  void expand(double includeCoord);
  QCPRange expanded(const QCPRange &otherRange) const;
  QCPRange expanded(double includeCoord) const;
  QCPRange bounded(double lowerBound, double upperBound) const;
  QCPRange sanitizedForLogScale() const;
  QCPRange sanitizedForLinScale() const;
  bool contains(double value) const { return value >= lower && value <= upper; }

  static bool validRange(double lower, double upper);
  static bool validRange(const QCPRange &range);
  static const double minRange;
  static const double maxRange;
};
Q_DECLARE_TYPEINFO(QCPRange, Q_MOVABLE_TYPE);

inline QDebug operator<<(QDebug d, const QCPRange &range) {
  d.nospace() << "QCPRange(" << range.lower << ", " << range.upper << ")";
  return d.space();
}

inline const QCPRange operator+(const QCPRange &range, double value) {
  QCPRange result(range);
  result += value;
  return result;
}

inline const QCPRange operator+(double value, const QCPRange &range) {
  QCPRange result(range);
  result += value;
  return result;
}

inline const QCPRange operator-(const QCPRange &range, double value) {
  QCPRange result(range);
  result -= value;
  return result;
}

inline const QCPRange operator*(const QCPRange &range, double value) {
  QCPRange result(range);
  result *= value;
  return result;
}

inline const QCPRange operator*(double value, const QCPRange &range) {
  QCPRange result(range);
  result *= value;
  return result;
}

inline const QCPRange operator/(const QCPRange &range, double value) {
  QCPRange result(range);
  result /= value;
  return result;
}

class QCP_LIB_DECL QCPDataRange {
public:
  QCPDataRange();
  QCPDataRange(int begin, int end);

  bool operator==(const QCPDataRange &other) const {
    return mBegin == other.mBegin && mEnd == other.mEnd;
  }
  bool operator!=(const QCPDataRange &other) const { return !(*this == other); }

  int begin() const { return mBegin; }
  int end() const { return mEnd; }
  int size() const { return mEnd - mBegin; }
  int length() const { return size(); }

  void setBegin(int begin) { mBegin = begin; }
  void setEnd(int end) { mEnd = end; }

  bool isValid() const { return (mEnd >= mBegin) && (mBegin >= 0); }
  bool isEmpty() const { return length() == 0; }
  QCPDataRange bounded(const QCPDataRange &other) const;
  QCPDataRange expanded(const QCPDataRange &other) const;
  QCPDataRange intersection(const QCPDataRange &other) const;
  QCPDataRange adjusted(int changeBegin, int changeEnd) const {
    return QCPDataRange(mBegin + changeBegin, mEnd + changeEnd);
  }
  bool intersects(const QCPDataRange &other) const;
  bool contains(const QCPDataRange &other) const;

private:
  int mBegin, mEnd;
};
Q_DECLARE_TYPEINFO(QCPDataRange, Q_MOVABLE_TYPE);

class QCP_LIB_DECL QCPDataSelection {
public:
  explicit QCPDataSelection();
  explicit QCPDataSelection(const QCPDataRange &range);

  bool operator==(const QCPDataSelection &other) const;
  bool operator!=(const QCPDataSelection &other) const {
    return !(*this == other);
  }
  QCPDataSelection &operator+=(const QCPDataSelection &other);
  QCPDataSelection &operator+=(const QCPDataRange &other);
  QCPDataSelection &operator-=(const QCPDataSelection &other);
  QCPDataSelection &operator-=(const QCPDataRange &other);
  friend inline const QCPDataSelection operator+(const QCPDataSelection &a,
                                                 const QCPDataSelection &b);
  friend inline const QCPDataSelection operator+(const QCPDataRange &a,
                                                 const QCPDataSelection &b);
  friend inline const QCPDataSelection operator+(const QCPDataSelection &a,
                                                 const QCPDataRange &b);
  friend inline const QCPDataSelection operator+(const QCPDataRange &a,
                                                 const QCPDataRange &b);
  friend inline const QCPDataSelection operator-(const QCPDataSelection &a,
                                                 const QCPDataSelection &b);
  friend inline const QCPDataSelection operator-(const QCPDataRange &a,
                                                 const QCPDataSelection &b);
  friend inline const QCPDataSelection operator-(const QCPDataSelection &a,
                                                 const QCPDataRange &b);
  friend inline const QCPDataSelection operator-(const QCPDataRange &a,
                                                 const QCPDataRange &b);

  int dataRangeCount() const { return mDataRanges.size(); }
  int dataPointCount() const;
  QCPDataRange dataRange(int index = 0) const;
  QList<QCPDataRange> dataRanges() const { return mDataRanges; }
  QCPDataRange span() const;

  void addDataRange(const QCPDataRange &dataRange, bool simplify = true);
  void clear();
  bool isEmpty() const { return mDataRanges.isEmpty(); }
  void simplify();
  void enforceType(QCP::SelectionType type);
  bool contains(const QCPDataSelection &other) const;
  QCPDataSelection intersection(const QCPDataRange &other) const;
  QCPDataSelection intersection(const QCPDataSelection &other) const;
  QCPDataSelection inverse(const QCPDataRange &outerRange) const;

private:
  QList<QCPDataRange> mDataRanges;

  inline static bool lessThanDataRangeBegin(const QCPDataRange &a,
                                            const QCPDataRange &b) {
    return a.begin() < b.begin();
  }
};
Q_DECLARE_METATYPE(QCPDataSelection)

inline const QCPDataSelection operator+(const QCPDataSelection &a,
                                        const QCPDataSelection &b) {
  QCPDataSelection result(a);
  result += b;
  return result;
}

inline const QCPDataSelection operator+(const QCPDataRange &a,
                                        const QCPDataSelection &b) {
  QCPDataSelection result(a);
  result += b;
  return result;
}

inline const QCPDataSelection operator+(const QCPDataSelection &a,
                                        const QCPDataRange &b) {
  QCPDataSelection result(a);
  result += b;
  return result;
}

inline const QCPDataSelection operator+(const QCPDataRange &a,
                                        const QCPDataRange &b) {
  QCPDataSelection result(a);
  result += b;
  return result;
}

inline const QCPDataSelection operator-(const QCPDataSelection &a,
                                        const QCPDataSelection &b) {
  QCPDataSelection result(a);
  result -= b;
  return result;
}

inline const QCPDataSelection operator-(const QCPDataRange &a,
                                        const QCPDataSelection &b) {
  QCPDataSelection result(a);
  result -= b;
  return result;
}

inline const QCPDataSelection operator-(const QCPDataSelection &a,
                                        const QCPDataRange &b) {
  QCPDataSelection result(a);
  result -= b;
  return result;
}

inline const QCPDataSelection operator-(const QCPDataRange &a,
                                        const QCPDataRange &b) {
  QCPDataSelection result(a);
  result -= b;
  return result;
}

inline QDebug operator<<(QDebug d, const QCPDataRange &dataRange) {
  d.nospace() << "QCPDataRange(" << dataRange.begin() << ", " << dataRange.end()
              << ")";
  return d;
}

inline QDebug operator<<(QDebug d, const QCPDataSelection &selection) {
  d.nospace() << "QCPDataSelection(";
  for (int i = 0; i < selection.dataRangeCount(); ++i) {
    if (i != 0)
      d << ", ";
    d << selection.dataRange(i);
  }
  d << ")";
  return d;
}

class QCP_LIB_DECL QCPSelectionRect : public QCPLayerable {
  Q_OBJECT
public:
  explicit QCPSelectionRect(QCustomPlot *parentPlot);
  virtual ~QCPSelectionRect();

  QRect rect() const { return mRect; }
  QCPRange range(const QCPAxis *axis) const;
  QPen pen() const { return mPen; }
  QBrush brush() const { return mBrush; }
  bool isActive() const { return mActive; }

  void setPen(const QPen &pen);
  void setBrush(const QBrush &brush);

  Q_SLOT void cancel();

Q_SIGNALS:
  void started(QMouseEvent *event);
  void changed(const QRect &rect, QMouseEvent *event);
  void canceled(const QRect &rect, QInputEvent *event);
  void accepted(const QRect &rect, QMouseEvent *event);

protected:
  QRect mRect;
  QPen mPen;
  QBrush mBrush;

  bool mActive;

  virtual void startSelection(QMouseEvent *event);
  virtual void moveSelection(QMouseEvent *event);
  virtual void endSelection(QMouseEvent *event);
  virtual void keyPressEvent(QKeyEvent *event);

  virtual void
  applyDefaultAntialiasingHint(QCPPainter *painter) const Q_DECL_OVERRIDE;
  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;

  friend class QCustomPlot;
};

class QCP_LIB_DECL QCPMarginGroup : public QObject {
  Q_OBJECT
public:
  explicit QCPMarginGroup(QCustomPlot *parentPlot);
  virtual ~QCPMarginGroup();

  QList<QCPLayoutElement *> elements(QCP::MarginSide side) const {
    return mChildren.value(side);
  }
  bool isEmpty() const;
  void clear();

protected:
  QCustomPlot *mParentPlot;
  QHash<QCP::MarginSide, QList<QCPLayoutElement *>> mChildren;

  virtual int commonMargin(QCP::MarginSide side) const;

  void addChild(QCP::MarginSide side, QCPLayoutElement *element);
  void removeChild(QCP::MarginSide side, QCPLayoutElement *element);

private:
  Q_DISABLE_COPY(QCPMarginGroup)

  friend class QCPLayoutElement;
};

class QCP_LIB_DECL QCPLayoutElement : public QCPLayerable {
  Q_OBJECT

  Q_PROPERTY(QCPLayout *layout READ layout)
  Q_PROPERTY(QRect rect READ rect)
  Q_PROPERTY(QRect outerRect READ outerRect WRITE setOuterRect)
  Q_PROPERTY(QMargins margins READ margins WRITE setMargins)
  Q_PROPERTY(
      QMargins minimumMargins READ minimumMargins WRITE setMinimumMargins)
  Q_PROPERTY(QSize minimumSize READ minimumSize WRITE setMinimumSize)
  Q_PROPERTY(QSize maximumSize READ maximumSize WRITE setMaximumSize)
  Q_PROPERTY(SizeConstraintRect sizeConstraintRect READ sizeConstraintRect WRITE
                 setSizeConstraintRect)

public:
  enum UpdatePhase {
    upPreparation

    ,
    upMargins

    ,
    upLayout

  };
  Q_ENUMS(UpdatePhase)

  enum SizeConstraintRect {
    scrInnerRect

    ,
    scrOuterRect

  };
  Q_ENUMS(SizeConstraintRect)

  explicit QCPLayoutElement(QCustomPlot *parentPlot = 0);
  virtual ~QCPLayoutElement();

  QCPLayout *layout() const { return mParentLayout; }
  QRect rect() const { return mRect; }
  QRect outerRect() const { return mOuterRect; }
  QMargins margins() const { return mMargins; }
  QMargins minimumMargins() const { return mMinimumMargins; }
  QCP::MarginSides autoMargins() const { return mAutoMargins; }
  QSize minimumSize() const { return mMinimumSize; }
  QSize maximumSize() const { return mMaximumSize; }
  SizeConstraintRect sizeConstraintRect() const { return mSizeConstraintRect; }
  QCPMarginGroup *marginGroup(QCP::MarginSide side) const {
    return mMarginGroups.value(side, (QCPMarginGroup *)0);
  }
  QHash<QCP::MarginSide, QCPMarginGroup *> marginGroups() const {
    return mMarginGroups;
  }

  void setOuterRect(const QRect &rect);
  void setMargins(const QMargins &margins);
  void setMinimumMargins(const QMargins &margins);
  void setAutoMargins(QCP::MarginSides sides);
  void setMinimumSize(const QSize &size);
  void setMinimumSize(int width, int height);
  void setMaximumSize(const QSize &size);
  void setMaximumSize(int width, int height);
  void setSizeConstraintRect(SizeConstraintRect constraintRect);
  void setMarginGroup(QCP::MarginSides sides, QCPMarginGroup *group);

  virtual void update(UpdatePhase phase);
  virtual QSize minimumOuterSizeHint() const;
  virtual QSize maximumOuterSizeHint() const;
  virtual QList<QCPLayoutElement *> elements(bool recursive) const;

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;

protected:
  QCPLayout *mParentLayout;
  QSize mMinimumSize, mMaximumSize;
  SizeConstraintRect mSizeConstraintRect;
  QRect mRect, mOuterRect;
  QMargins mMargins, mMinimumMargins;
  QCP::MarginSides mAutoMargins;
  QHash<QCP::MarginSide, QCPMarginGroup *> mMarginGroups;

  virtual int calculateAutoMargin(QCP::MarginSide side);
  virtual void layoutChanged();

  virtual void
  applyDefaultAntialiasingHint(QCPPainter *painter) const Q_DECL_OVERRIDE {
    Q_UNUSED(painter)
  }
  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE { Q_UNUSED(painter) }
  virtual void parentPlotInitialized(QCustomPlot *parentPlot) Q_DECL_OVERRIDE;

private:
  Q_DISABLE_COPY(QCPLayoutElement)

  friend class QCustomPlot;
  friend class QCPLayout;
  friend class QCPMarginGroup;
};
Q_DECLARE_METATYPE(QCPLayoutElement::UpdatePhase)

class QCP_LIB_DECL QCPLayout : public QCPLayoutElement {
  Q_OBJECT
public:
  explicit QCPLayout();

  virtual void update(UpdatePhase phase) Q_DECL_OVERRIDE;
  virtual QList<QCPLayoutElement *>
  elements(bool recursive) const Q_DECL_OVERRIDE;

  virtual int elementCount() const = 0;
  virtual QCPLayoutElement *elementAt(int index) const = 0;
  virtual QCPLayoutElement *takeAt(int index) = 0;
  virtual bool take(QCPLayoutElement *element) = 0;
  virtual void simplify();

  bool removeAt(int index);
  bool remove(QCPLayoutElement *element);
  void clear();

protected:
  virtual void updateLayout();

  void sizeConstraintsChanged() const;
  void adoptElement(QCPLayoutElement *el);
  void releaseElement(QCPLayoutElement *el);
  QVector<int> getSectionSizes(QVector<int> maxSizes, QVector<int> minSizes,
                               QVector<double> stretchFactors,
                               int totalSize) const;
  static QSize getFinalMinimumOuterSize(const QCPLayoutElement *el);
  static QSize getFinalMaximumOuterSize(const QCPLayoutElement *el);

private:
  Q_DISABLE_COPY(QCPLayout)
  friend class QCPLayoutElement;
};

class QCP_LIB_DECL QCPLayoutGrid : public QCPLayout {
  Q_OBJECT

  Q_PROPERTY(int rowCount READ rowCount)
  Q_PROPERTY(int columnCount READ columnCount)
  Q_PROPERTY(QList<double> columnStretchFactors READ columnStretchFactors WRITE
                 setColumnStretchFactors)
  Q_PROPERTY(QList<double> rowStretchFactors READ rowStretchFactors WRITE
                 setRowStretchFactors)
  Q_PROPERTY(int columnSpacing READ columnSpacing WRITE setColumnSpacing)
  Q_PROPERTY(int rowSpacing READ rowSpacing WRITE setRowSpacing)
  Q_PROPERTY(FillOrder fillOrder READ fillOrder WRITE setFillOrder)
  Q_PROPERTY(int wrap READ wrap WRITE setWrap)

public:
  enum FillOrder {
    foRowsFirst

    ,
    foColumnsFirst

  };
  Q_ENUMS(FillOrder)

  explicit QCPLayoutGrid();
  virtual ~QCPLayoutGrid();

  int rowCount() const { return mElements.size(); }
  int columnCount() const {
    return mElements.size() > 0 ? mElements.first().size() : 0;
  }
  QList<double> columnStretchFactors() const { return mColumnStretchFactors; }
  QList<double> rowStretchFactors() const { return mRowStretchFactors; }
  int columnSpacing() const { return mColumnSpacing; }
  int rowSpacing() const { return mRowSpacing; }
  int wrap() const { return mWrap; }
  FillOrder fillOrder() const { return mFillOrder; }

  void setColumnStretchFactor(int column, double factor);
  void setColumnStretchFactors(const QList<double> &factors);
  void setRowStretchFactor(int row, double factor);
  void setRowStretchFactors(const QList<double> &factors);
  void setColumnSpacing(int pixels);
  void setRowSpacing(int pixels);
  void setWrap(int count);
  void setFillOrder(FillOrder order, bool rearrange = true);

  virtual void updateLayout() Q_DECL_OVERRIDE;
  virtual int elementCount() const Q_DECL_OVERRIDE {
    return rowCount() * columnCount();
  }
  virtual QCPLayoutElement *elementAt(int index) const Q_DECL_OVERRIDE;
  virtual QCPLayoutElement *takeAt(int index) Q_DECL_OVERRIDE;
  virtual bool take(QCPLayoutElement *element) Q_DECL_OVERRIDE;
  virtual QList<QCPLayoutElement *>
  elements(bool recursive) const Q_DECL_OVERRIDE;
  virtual void simplify() Q_DECL_OVERRIDE;
  virtual QSize minimumOuterSizeHint() const Q_DECL_OVERRIDE;
  virtual QSize maximumOuterSizeHint() const Q_DECL_OVERRIDE;

  QCPLayoutElement *element(int row, int column) const;
  bool addElement(int row, int column, QCPLayoutElement *element);
  bool addElement(QCPLayoutElement *element);
  bool hasElement(int row, int column);
  void expandTo(int newRowCount, int newColumnCount);
  void insertRow(int newIndex);
  void insertColumn(int newIndex);
  int rowColToIndex(int row, int column) const;
  void indexToRowCol(int index, int &row, int &column) const;

protected:
  QList<QList<QCPLayoutElement *>> mElements;
  QList<double> mColumnStretchFactors;
  QList<double> mRowStretchFactors;
  int mColumnSpacing, mRowSpacing;
  int mWrap;
  FillOrder mFillOrder;

  void getMinimumRowColSizes(QVector<int> *minColWidths,
                             QVector<int> *minRowHeights) const;
  void getMaximumRowColSizes(QVector<int> *maxColWidths,
                             QVector<int> *maxRowHeights) const;

private:
  Q_DISABLE_COPY(QCPLayoutGrid)
};
Q_DECLARE_METATYPE(QCPLayoutGrid::FillOrder)

class QCP_LIB_DECL QCPLayoutInset : public QCPLayout {
  Q_OBJECT
public:
  enum InsetPlacement {
    ipFree

    ,
    ipBorderAligned

  };
  Q_ENUMS(InsetPlacement)

  explicit QCPLayoutInset();
  virtual ~QCPLayoutInset();

  InsetPlacement insetPlacement(int index) const;
  Qt::Alignment insetAlignment(int index) const;
  QRectF insetRect(int index) const;

  void setInsetPlacement(int index, InsetPlacement placement);
  void setInsetAlignment(int index, Qt::Alignment alignment);
  void setInsetRect(int index, const QRectF &rect);

  virtual void updateLayout() Q_DECL_OVERRIDE;
  virtual int elementCount() const Q_DECL_OVERRIDE;
  virtual QCPLayoutElement *elementAt(int index) const Q_DECL_OVERRIDE;
  virtual QCPLayoutElement *takeAt(int index) Q_DECL_OVERRIDE;
  virtual bool take(QCPLayoutElement *element) Q_DECL_OVERRIDE;
  virtual void simplify() Q_DECL_OVERRIDE {}
  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;

  void addElement(QCPLayoutElement *element, Qt::Alignment alignment);
  void addElement(QCPLayoutElement *element, const QRectF &rect);

protected:
  QList<QCPLayoutElement *> mElements;
  QList<InsetPlacement> mInsetPlacement;
  QList<Qt::Alignment> mInsetAlignment;
  QList<QRectF> mInsetRect;

private:
  Q_DISABLE_COPY(QCPLayoutInset)
};
Q_DECLARE_METATYPE(QCPLayoutInset::InsetPlacement)

class QCP_LIB_DECL QCPLineEnding {
  Q_GADGET
public:
  enum EndingStyle {
    esNone

    ,
    esFlatArrow

    ,
    esSpikeArrow

    ,
    esLineArrow

    ,
    esDisc

    ,
    esSquare

    ,
    esDiamond

    ,
    esBar

    ,
    esHalfBar

    ,
    esSkewedBar

  };
  Q_ENUMS(EndingStyle)

  QCPLineEnding();
  QCPLineEnding(EndingStyle style, double width = 8, double length = 10,
                bool inverted = false);

  EndingStyle style() const { return mStyle; }
  double width() const { return mWidth; }
  double length() const { return mLength; }
  bool inverted() const { return mInverted; }

  void setStyle(EndingStyle style);
  void setWidth(double width);
  void setLength(double length);
  void setInverted(bool inverted);

  double boundingDistance() const;
  double realLength() const;
  void draw(QCPPainter *painter, const QCPVector2D &pos,
            const QCPVector2D &dir) const;
  void draw(QCPPainter *painter, const QCPVector2D &pos, double angle) const;

protected:
  EndingStyle mStyle;
  double mWidth, mLength;
  bool mInverted;
};
Q_DECLARE_TYPEINFO(QCPLineEnding, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(QCPLineEnding::EndingStyle)

class QCP_LIB_DECL QCPAxisTicker {
  Q_GADGET
public:
  enum TickStepStrategy {
    tssReadability

    ,
    tssMeetTickCount

  };
  Q_ENUMS(TickStepStrategy)

  QCPAxisTicker();
  virtual ~QCPAxisTicker();

  TickStepStrategy tickStepStrategy() const { return mTickStepStrategy; }
  int tickCount() const { return mTickCount; }
  double tickOrigin() const { return mTickOrigin; }

  void setTickStepStrategy(TickStepStrategy strategy);
  void setTickCount(int count);
  void setTickOrigin(double origin);

  virtual void generate(const QCPRange &range, const QLocale &locale,
                        QChar formatChar, int precision, QVector<double> &ticks,
                        QVector<double> *subTicks,
                        QVector<QString> *tickLabels);

protected:
  TickStepStrategy mTickStepStrategy;
  int mTickCount;
  double mTickOrigin;

  virtual double getTickStep(const QCPRange &range);
  virtual int getSubTickCount(double tickStep);
  virtual QString getTickLabel(double tick, const QLocale &locale,
                               QChar formatChar, int precision);
  virtual QVector<double> createTickVector(double tickStep,
                                           const QCPRange &range);
  virtual QVector<double> createSubTickVector(int subTickCount,
                                              const QVector<double> &ticks);
  virtual QVector<QString> createLabelVector(const QVector<double> &ticks,
                                             const QLocale &locale,
                                             QChar formatChar, int precision);

  void trimTicks(const QCPRange &range, QVector<double> &ticks,
                 bool keepOneOutlier) const;
  double pickClosest(double target, const QVector<double> &candidates) const;
  double getMantissa(double input, double *magnitude = 0) const;
  double cleanMantissa(double input) const;

private:
  Q_DISABLE_COPY(QCPAxisTicker)
};
Q_DECLARE_METATYPE(QCPAxisTicker::TickStepStrategy)
Q_DECLARE_METATYPE(QSharedPointer<QCPAxisTicker>)

class QCP_LIB_DECL QCPAxisTickerDateTime : public QCPAxisTicker {
public:
  QCPAxisTickerDateTime();

  QString dateTimeFormat() const { return mDateTimeFormat; }
  Qt::TimeSpec dateTimeSpec() const { return mDateTimeSpec; }

  void setDateTimeFormat(const QString &format);
  void setDateTimeSpec(Qt::TimeSpec spec);
  void setTickOrigin(double origin);

  void setTickOrigin(const QDateTime &origin);

  static QDateTime keyToDateTime(double key);
  static double dateTimeToKey(const QDateTime dateTime);
  static double dateTimeToKey(const QDate date);

protected:
  QString mDateTimeFormat;
  Qt::TimeSpec mDateTimeSpec;

  enum DateStrategy {
    dsNone,
    dsUniformTimeInDay,
    dsUniformDayInMonth
  } mDateStrategy;

  virtual double getTickStep(const QCPRange &range) Q_DECL_OVERRIDE;
  virtual int getSubTickCount(double tickStep) Q_DECL_OVERRIDE;
  virtual QString getTickLabel(double tick, const QLocale &locale,
                               QChar formatChar, int precision) Q_DECL_OVERRIDE;
  virtual QVector<double>
  createTickVector(double tickStep, const QCPRange &range) Q_DECL_OVERRIDE;
};

class QCP_LIB_DECL QCPAxisTickerTime : public QCPAxisTicker {
  Q_GADGET
public:
  enum TimeUnit {
    tuMilliseconds

    ,
    tuSeconds

    ,
    tuMinutes

    ,
    tuHours

    ,
    tuDays

  };
  Q_ENUMS(TimeUnit)

  QCPAxisTickerTime();

  QString timeFormat() const { return mTimeFormat; }
  int fieldWidth(TimeUnit unit) const { return mFieldWidth.value(unit); }

  void setTimeFormat(const QString &format);
  void setFieldWidth(TimeUnit unit, int width);

protected:
  QString mTimeFormat;
  QHash<TimeUnit, int> mFieldWidth;

  TimeUnit mSmallestUnit, mBiggestUnit;
  QHash<TimeUnit, QString> mFormatPattern;

  virtual double getTickStep(const QCPRange &range) Q_DECL_OVERRIDE;
  virtual int getSubTickCount(double tickStep) Q_DECL_OVERRIDE;
  virtual QString getTickLabel(double tick, const QLocale &locale,
                               QChar formatChar, int precision) Q_DECL_OVERRIDE;

  void replaceUnit(QString &text, TimeUnit unit, int value) const;
};
Q_DECLARE_METATYPE(QCPAxisTickerTime::TimeUnit)

class QCP_LIB_DECL QCPAxisTickerFixed : public QCPAxisTicker {
  Q_GADGET
public:
  enum ScaleStrategy {
    ssNone

    ,
    ssMultiples

    ,
    ssPowers

  };
  Q_ENUMS(ScaleStrategy)

  QCPAxisTickerFixed();

  double tickStep() const { return mTickStep; }
  ScaleStrategy scaleStrategy() const { return mScaleStrategy; }

  void setTickStep(double step);
  void setScaleStrategy(ScaleStrategy strategy);

protected:
  double mTickStep;
  ScaleStrategy mScaleStrategy;

  virtual double getTickStep(const QCPRange &range) Q_DECL_OVERRIDE;
};
Q_DECLARE_METATYPE(QCPAxisTickerFixed::ScaleStrategy)

class QCP_LIB_DECL QCPAxisTickerText : public QCPAxisTicker {
public:
  QCPAxisTickerText();

  QMap<double, QString> &ticks() { return mTicks; }
  int subTickCount() const { return mSubTickCount; }

  void setTicks(const QMap<double, QString> &ticks);
  void setTicks(const QVector<double> &positions,
                const QVector<QString> &labels);
  void setSubTickCount(int subTicks);

  void clear();
  void addTick(double position, const QString &label);
  void addTicks(const QMap<double, QString> &ticks);
  void addTicks(const QVector<double> &positions,
                const QVector<QString> &labels);

protected:
  QMap<double, QString> mTicks;
  int mSubTickCount;

  virtual double getTickStep(const QCPRange &range) Q_DECL_OVERRIDE;
  virtual int getSubTickCount(double tickStep) Q_DECL_OVERRIDE;
  virtual QString getTickLabel(double tick, const QLocale &locale,
                               QChar formatChar, int precision) Q_DECL_OVERRIDE;
  virtual QVector<double>
  createTickVector(double tickStep, const QCPRange &range) Q_DECL_OVERRIDE;
};

class QCP_LIB_DECL QCPAxisTickerPi : public QCPAxisTicker {
  Q_GADGET
public:
  enum FractionStyle {
    fsFloatingPoint

    ,
    fsAsciiFractions

    ,
    fsUnicodeFractions

  };
  Q_ENUMS(FractionStyle)

  QCPAxisTickerPi();

  QString piSymbol() const { return mPiSymbol; }
  double piValue() const { return mPiValue; }
  bool periodicity() const { return mPeriodicity; }
  FractionStyle fractionStyle() const { return mFractionStyle; }

  void setPiSymbol(QString symbol);
  void setPiValue(double pi);
  void setPeriodicity(int multiplesOfPi);
  void setFractionStyle(FractionStyle style);

protected:
  QString mPiSymbol;
  double mPiValue;
  int mPeriodicity;
  FractionStyle mFractionStyle;

  double mPiTickStep;

  virtual double getTickStep(const QCPRange &range) Q_DECL_OVERRIDE;
  virtual int getSubTickCount(double tickStep) Q_DECL_OVERRIDE;
  virtual QString getTickLabel(double tick, const QLocale &locale,
                               QChar formatChar, int precision) Q_DECL_OVERRIDE;

  void simplifyFraction(int &numerator, int &denominator) const;
  QString fractionToString(int numerator, int denominator) const;
  QString unicodeFraction(int numerator, int denominator) const;
  QString unicodeSuperscript(int number) const;
  QString unicodeSubscript(int number) const;
};
Q_DECLARE_METATYPE(QCPAxisTickerPi::FractionStyle)

class QCP_LIB_DECL QCPAxisTickerLog : public QCPAxisTicker {
public:
  QCPAxisTickerLog();

  double logBase() const { return mLogBase; }
  int subTickCount() const { return mSubTickCount; }

  void setLogBase(double base);
  void setSubTickCount(int subTicks);

protected:
  double mLogBase;
  int mSubTickCount;

  double mLogBaseLnInv;

  virtual double getTickStep(const QCPRange &range) Q_DECL_OVERRIDE;
  virtual int getSubTickCount(double tickStep) Q_DECL_OVERRIDE;
  virtual QVector<double>
  createTickVector(double tickStep, const QCPRange &range) Q_DECL_OVERRIDE;
};

class QCP_LIB_DECL QCPGrid : public QCPLayerable {
  Q_OBJECT

  Q_PROPERTY(bool subGridVisible READ subGridVisible WRITE setSubGridVisible)
  Q_PROPERTY(bool antialiasedSubGrid READ antialiasedSubGrid WRITE
                 setAntialiasedSubGrid)
  Q_PROPERTY(bool antialiasedZeroLine READ antialiasedZeroLine WRITE
                 setAntialiasedZeroLine)
  Q_PROPERTY(QPen pen READ pen WRITE setPen)
  Q_PROPERTY(QPen subGridPen READ subGridPen WRITE setSubGridPen)
  Q_PROPERTY(QPen zeroLinePen READ zeroLinePen WRITE setZeroLinePen)

public:
  explicit QCPGrid(QCPAxis *parentAxis);

  bool subGridVisible() const { return mSubGridVisible; }
  bool antialiasedSubGrid() const { return mAntialiasedSubGrid; }
  bool antialiasedZeroLine() const { return mAntialiasedZeroLine; }
  QPen pen() const { return mPen; }
  QPen subGridPen() const { return mSubGridPen; }
  QPen zeroLinePen() const { return mZeroLinePen; }

  void setSubGridVisible(bool visible);
  void setAntialiasedSubGrid(bool enabled);
  void setAntialiasedZeroLine(bool enabled);
  void setPen(const QPen &pen);
  void setSubGridPen(const QPen &pen);
  void setZeroLinePen(const QPen &pen);

protected:
  bool mSubGridVisible;
  bool mAntialiasedSubGrid, mAntialiasedZeroLine;
  QPen mPen, mSubGridPen, mZeroLinePen;

  QCPAxis *mParentAxis;

  virtual void
  applyDefaultAntialiasingHint(QCPPainter *painter) const Q_DECL_OVERRIDE;
  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;

  void drawGridLines(QCPPainter *painter) const;
  void drawSubGridLines(QCPPainter *painter) const;

  friend class QCPAxis;
};

class QCP_LIB_DECL QCPAxis : public QCPLayerable {
  Q_OBJECT

  Q_PROPERTY(AxisType axisType READ axisType)
  Q_PROPERTY(QCPAxisRect *axisRect READ axisRect)
  Q_PROPERTY(ScaleType scaleType READ scaleType WRITE setScaleType NOTIFY
                 scaleTypeChanged)
  Q_PROPERTY(QCPRange range READ range WRITE setRange NOTIFY rangeChanged)
  Q_PROPERTY(bool rangeReversed READ rangeReversed WRITE setRangeReversed)
  Q_PROPERTY(QSharedPointer<QCPAxisTicker> ticker READ ticker WRITE setTicker)
  Q_PROPERTY(bool ticks READ ticks WRITE setTicks)
  Q_PROPERTY(bool tickLabels READ tickLabels WRITE setTickLabels)
  Q_PROPERTY(
      int tickLabelPadding READ tickLabelPadding WRITE setTickLabelPadding)
  Q_PROPERTY(QFont tickLabelFont READ tickLabelFont WRITE setTickLabelFont)
  Q_PROPERTY(QColor tickLabelColor READ tickLabelColor WRITE setTickLabelColor)
  Q_PROPERTY(double tickLabelRotation READ tickLabelRotation WRITE
                 setTickLabelRotation)
  Q_PROPERTY(LabelSide tickLabelSide READ tickLabelSide WRITE setTickLabelSide)
  Q_PROPERTY(QString numberFormat READ numberFormat WRITE setNumberFormat)
  Q_PROPERTY(int numberPrecision READ numberPrecision WRITE setNumberPrecision)
  Q_PROPERTY(QVector<double> tickVector READ tickVector)
  Q_PROPERTY(QVector<QString> tickVectorLabels READ tickVectorLabels)
  Q_PROPERTY(int tickLengthIn READ tickLengthIn WRITE setTickLengthIn)
  Q_PROPERTY(int tickLengthOut READ tickLengthOut WRITE setTickLengthOut)
  Q_PROPERTY(bool subTicks READ subTicks WRITE setSubTicks)
  Q_PROPERTY(int subTickLengthIn READ subTickLengthIn WRITE setSubTickLengthIn)
  Q_PROPERTY(
      int subTickLengthOut READ subTickLengthOut WRITE setSubTickLengthOut)
  Q_PROPERTY(QPen basePen READ basePen WRITE setBasePen)
  Q_PROPERTY(QPen tickPen READ tickPen WRITE setTickPen)
  Q_PROPERTY(QPen subTickPen READ subTickPen WRITE setSubTickPen)
  Q_PROPERTY(QFont labelFont READ labelFont WRITE setLabelFont)
  Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor)
  Q_PROPERTY(QString label READ label WRITE setLabel)
  Q_PROPERTY(int labelPadding READ labelPadding WRITE setLabelPadding)
  Q_PROPERTY(int padding READ padding WRITE setPadding)
  Q_PROPERTY(int offset READ offset WRITE setOffset)
  Q_PROPERTY(SelectableParts selectedParts READ selectedParts WRITE
                 setSelectedParts NOTIFY selectionChanged)
  Q_PROPERTY(SelectableParts selectableParts READ selectableParts WRITE
                 setSelectableParts NOTIFY selectableChanged)
  Q_PROPERTY(QFont selectedTickLabelFont READ selectedTickLabelFont WRITE
                 setSelectedTickLabelFont)
  Q_PROPERTY(
      QFont selectedLabelFont READ selectedLabelFont WRITE setSelectedLabelFont)
  Q_PROPERTY(QColor selectedTickLabelColor READ selectedTickLabelColor WRITE
                 setSelectedTickLabelColor)
  Q_PROPERTY(QColor selectedLabelColor READ selectedLabelColor WRITE
                 setSelectedLabelColor)
  Q_PROPERTY(QPen selectedBasePen READ selectedBasePen WRITE setSelectedBasePen)
  Q_PROPERTY(QPen selectedTickPen READ selectedTickPen WRITE setSelectedTickPen)
  Q_PROPERTY(QPen selectedSubTickPen READ selectedSubTickPen WRITE
                 setSelectedSubTickPen)
  Q_PROPERTY(QCPLineEnding lowerEnding READ lowerEnding WRITE setLowerEnding)
  Q_PROPERTY(QCPLineEnding upperEnding READ upperEnding WRITE setUpperEnding)
  Q_PROPERTY(QCPGrid *grid READ grid)

public:
  enum AxisType {
    atLeft = 0x01

    ,
    atRight = 0x02

    ,
    atTop = 0x04

    ,
    atBottom = 0x08

  };
  Q_ENUMS(AxisType)
  Q_FLAGS(AxisTypes)
  Q_DECLARE_FLAGS(AxisTypes, AxisType)

  enum LabelSide {
    lsInside

    ,
    lsOutside

  };
  Q_ENUMS(LabelSide)

  enum ScaleType {
    stLinear

    ,
    stLogarithmic

  };
  Q_ENUMS(ScaleType)

  enum SelectablePart {
    spNone = 0

    ,
    spAxis = 0x001

    ,
    spTickLabels = 0x002

    ,
    spAxisLabel = 0x004

  };
  Q_ENUMS(SelectablePart)
  Q_FLAGS(SelectableParts)
  Q_DECLARE_FLAGS(SelectableParts, SelectablePart)

  explicit QCPAxis(QCPAxisRect *parent, AxisType type);
  virtual ~QCPAxis();

  AxisType axisType() const { return mAxisType; }
  QCPAxisRect *axisRect() const { return mAxisRect; }
  ScaleType scaleType() const { return mScaleType; }
  const QCPRange range() const { return mRange; }
  bool rangeReversed() const { return mRangeReversed; }
  QSharedPointer<QCPAxisTicker> ticker() const { return mTicker; }
  bool ticks() const { return mTicks; }
  bool tickLabels() const { return mTickLabels; }
  int tickLabelPadding() const;
  QFont tickLabelFont() const { return mTickLabelFont; }
  QColor tickLabelColor() const { return mTickLabelColor; }
  double tickLabelRotation() const;
  LabelSide tickLabelSide() const;
  QString numberFormat() const;
  int numberPrecision() const { return mNumberPrecision; }
  QVector<double> tickVector() const { return mTickVector; }
  QVector<QString> tickVectorLabels() const { return mTickVectorLabels; }
  int tickLengthIn() const;
  int tickLengthOut() const;
  bool subTicks() const { return mSubTicks; }
  int subTickLengthIn() const;
  int subTickLengthOut() const;
  QPen basePen() const { return mBasePen; }
  QPen tickPen() const { return mTickPen; }
  QPen subTickPen() const { return mSubTickPen; }
  QFont labelFont() const { return mLabelFont; }
  QColor labelColor() const { return mLabelColor; }
  QString label() const { return mLabel; }
  int labelPadding() const;
  int padding() const { return mPadding; }
  int offset() const;
  SelectableParts selectedParts() const { return mSelectedParts; }
  SelectableParts selectableParts() const { return mSelectableParts; }
  QFont selectedTickLabelFont() const { return mSelectedTickLabelFont; }
  QFont selectedLabelFont() const { return mSelectedLabelFont; }
  QColor selectedTickLabelColor() const { return mSelectedTickLabelColor; }
  QColor selectedLabelColor() const { return mSelectedLabelColor; }
  QPen selectedBasePen() const { return mSelectedBasePen; }
  QPen selectedTickPen() const { return mSelectedTickPen; }
  QPen selectedSubTickPen() const { return mSelectedSubTickPen; }
  QCPLineEnding lowerEnding() const;
  QCPLineEnding upperEnding() const;
  QCPGrid *grid() const { return mGrid; }

  Q_SLOT void setScaleType(QCPAxis::ScaleType type);
  Q_SLOT void setRange(const QCPRange &range);
  void setRange(double lower, double upper);
  void setRange(double position, double size, Qt::AlignmentFlag alignment);
  void setRangeLower(double lower);
  void setRangeUpper(double upper);
  void setRangeReversed(bool reversed);
  void setTicker(QSharedPointer<QCPAxisTicker> ticker);
  void setTicks(bool show);
  void setTickLabels(bool show);
  void setTickLabelPadding(int padding);
  void setTickLabelFont(const QFont &font);
  void setTickLabelColor(const QColor &color);
  void setTickLabelRotation(double degrees);
  void setTickLabelSide(LabelSide side);
  void setNumberFormat(const QString &formatCode);
  void setNumberPrecision(int precision);
  void setTickLength(int inside, int outside = 0);
  void setTickLengthIn(int inside);
  void setTickLengthOut(int outside);
  void setSubTicks(bool show);
  void setSubTickLength(int inside, int outside = 0);
  void setSubTickLengthIn(int inside);
  void setSubTickLengthOut(int outside);
  void setBasePen(const QPen &pen);
  void setTickPen(const QPen &pen);
  void setSubTickPen(const QPen &pen);
  void setLabelFont(const QFont &font);
  void setLabelColor(const QColor &color);
  void setLabel(const QString &str);
  void setLabelPadding(int padding);
  void setPadding(int padding);
  void setOffset(int offset);
  void setSelectedTickLabelFont(const QFont &font);
  void setSelectedLabelFont(const QFont &font);
  void setSelectedTickLabelColor(const QColor &color);
  void setSelectedLabelColor(const QColor &color);
  void setSelectedBasePen(const QPen &pen);
  void setSelectedTickPen(const QPen &pen);
  void setSelectedSubTickPen(const QPen &pen);
  Q_SLOT void
  setSelectableParts(const QCPAxis::SelectableParts &selectableParts);
  Q_SLOT void setSelectedParts(const QCPAxis::SelectableParts &selectedParts);
  void setLowerEnding(const QCPLineEnding &ending);
  void setUpperEnding(const QCPLineEnding &ending);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;

  Qt::Orientation orientation() const { return mOrientation; }
  int pixelOrientation() const {
    return rangeReversed() != (orientation() == Qt::Vertical) ? -1 : 1;
  }
  void moveRange(double diff);
  void scaleRange(double factor);
  void scaleRange(double factor, double center);
  void setScaleRatio(const QCPAxis *otherAxis, double ratio = 1.0);
  void rescale(bool onlyVisiblePlottables = false);
  double pixelToCoord(double value) const;
  double coordToPixel(double value) const;
  SelectablePart getPartAt(const QPointF &pos) const;
  QList<QCPAbstractPlottable *> plottables() const;
  QList<QCPGraph *> graphs() const;
  QList<QCPAbstractItem *> items() const;

  static AxisType marginSideToAxisType(QCP::MarginSide side);
  static Qt::Orientation orientation(AxisType type) {
    return type == atBottom || type == atTop ? Qt::Horizontal : Qt::Vertical;
  }
  static AxisType opposite(AxisType type);

Q_SIGNALS:
  void rangeChanged(const QCPRange &newRange);
  void rangeChanged(const QCPRange &newRange, const QCPRange &oldRange);
  void scaleTypeChanged(QCPAxis::ScaleType scaleType);
  void selectionChanged(const QCPAxis::SelectableParts &parts);
  void selectableChanged(const QCPAxis::SelectableParts &parts);

protected:
  AxisType mAxisType;
  QCPAxisRect *mAxisRect;

  int mPadding;
  Qt::Orientation mOrientation;
  SelectableParts mSelectableParts, mSelectedParts;
  QPen mBasePen, mSelectedBasePen;

  QString mLabel;
  QFont mLabelFont, mSelectedLabelFont;
  QColor mLabelColor, mSelectedLabelColor;

  bool mTickLabels;

  QFont mTickLabelFont, mSelectedTickLabelFont;
  QColor mTickLabelColor, mSelectedTickLabelColor;
  int mNumberPrecision;
  QLatin1Char mNumberFormatChar;
  bool mNumberBeautifulPowers;

  bool mTicks;
  bool mSubTicks;

  QPen mTickPen, mSelectedTickPen;
  QPen mSubTickPen, mSelectedSubTickPen;

  QCPRange mRange;
  bool mRangeReversed;
  ScaleType mScaleType;

  QCPGrid *mGrid;
  QCPAxisPainterPrivate *mAxisPainter;
  QSharedPointer<QCPAxisTicker> mTicker;
  QVector<double> mTickVector;
  QVector<QString> mTickVectorLabels;
  QVector<double> mSubTickVector;
  bool mCachedMarginValid;
  int mCachedMargin;
  bool mDragging;
  QCPRange mDragStartRange;
  QCP::AntialiasedElements mAADragBackup, mNotAADragBackup;

  virtual int calculateMargin();

  virtual void
  applyDefaultAntialiasingHint(QCPPainter *painter) const Q_DECL_OVERRIDE;
  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual QCP::Interaction selectionCategory() const Q_DECL_OVERRIDE;

  virtual void selectEvent(QMouseEvent *event, bool additive,
                           const QVariant &details,
                           bool *selectionStateChanged) Q_DECL_OVERRIDE;
  virtual void deselectEvent(bool *selectionStateChanged) Q_DECL_OVERRIDE;

  virtual void mousePressEvent(QMouseEvent *event,
                               const QVariant &details) Q_DECL_OVERRIDE;
  virtual void mouseMoveEvent(QMouseEvent *event,
                              const QPointF &startPos) Q_DECL_OVERRIDE;
  virtual void mouseReleaseEvent(QMouseEvent *event,
                                 const QPointF &startPos) Q_DECL_OVERRIDE;
  virtual void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;

  void setupTickVectors();
  QPen getBasePen() const;
  QPen getTickPen() const;
  QPen getSubTickPen() const;
  QFont getTickLabelFont() const;
  QFont getLabelFont() const;
  QColor getTickLabelColor() const;
  QColor getLabelColor() const;

private:
  Q_DISABLE_COPY(QCPAxis)

  friend class QCustomPlot;
  friend class QCPGrid;
  friend class QCPAxisRect;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QCPAxis::SelectableParts)
Q_DECLARE_OPERATORS_FOR_FLAGS(QCPAxis::AxisTypes)
Q_DECLARE_METATYPE(QCPAxis::AxisType)
Q_DECLARE_METATYPE(QCPAxis::LabelSide)
Q_DECLARE_METATYPE(QCPAxis::ScaleType)
Q_DECLARE_METATYPE(QCPAxis::SelectablePart)

class QCPAxisPainterPrivate {
public:
  explicit QCPAxisPainterPrivate(QCustomPlot *parentPlot);
  virtual ~QCPAxisPainterPrivate();

  virtual void draw(QCPPainter *painter);
  virtual int size() const;
  void clearCache();

  QRect axisSelectionBox() const { return mAxisSelectionBox; }
  QRect tickLabelsSelectionBox() const { return mTickLabelsSelectionBox; }
  QRect labelSelectionBox() const { return mLabelSelectionBox; }

  QCPAxis::AxisType type;
  QPen basePen;
  QCPLineEnding lowerEnding, upperEnding;

  int labelPadding;

  QFont labelFont;
  QColor labelColor;
  QString label;
  int tickLabelPadding;

  double tickLabelRotation;

  QCPAxis::LabelSide tickLabelSide;

  bool substituteExponent;
  bool numberMultiplyCross;

  int tickLengthIn, tickLengthOut, subTickLengthIn, subTickLengthOut;

  QPen tickPen, subTickPen;
  QFont tickLabelFont;
  QColor tickLabelColor;
  QRect axisRect, viewportRect;
  double offset;

  bool abbreviateDecimalPowers;
  bool reversedEndings;

  QVector<double> subTickPositions;
  QVector<double> tickPositions;
  QVector<QString> tickLabels;

protected:
  struct CachedLabel {
    QPointF offset;
    QPixmap pixmap;
  };
  struct TickLabelData {
    QString basePart, expPart, suffixPart;
    QRect baseBounds, expBounds, suffixBounds, totalBounds, rotatedTotalBounds;
    QFont baseFont, expFont;
  };
  QCustomPlot *mParentPlot;
  QByteArray mLabelParameterHash;

  QCache<QString, CachedLabel> mLabelCache;
  QRect mAxisSelectionBox, mTickLabelsSelectionBox, mLabelSelectionBox;

  virtual QByteArray generateLabelParameterHash() const;

  virtual void placeTickLabel(QCPPainter *painter, double position,
                              int distanceToAxis, const QString &text,
                              QSize *tickLabelsSize);
  virtual void drawTickLabel(QCPPainter *painter, double x, double y,
                             const TickLabelData &labelData) const;
  virtual TickLabelData getTickLabelData(const QFont &font,
                                         const QString &text) const;
  virtual QPointF getTickLabelDrawOffset(const TickLabelData &labelData) const;
  virtual void getMaxTickLabelSize(const QFont &font, const QString &text,
                                   QSize *tickLabelsSize) const;
};

class QCP_LIB_DECL QCPScatterStyle {
  Q_GADGET
public:
  enum ScatterProperty {
    spNone = 0x00

    ,
    spPen = 0x01

    ,
    spBrush = 0x02

    ,
    spSize = 0x04

    ,
    spShape = 0x08

    ,
    spAll = 0xFF

  };
  Q_ENUMS(ScatterProperty)
  Q_FLAGS(ScatterProperties)
  Q_DECLARE_FLAGS(ScatterProperties, ScatterProperty)

  enum ScatterShape {
    ssNone

    ,
    ssDot

    ,
    ssCross

    ,
    ssPlus

    ,
    ssCircle

    ,
    ssDisc

    ,
    ssSquare

    ,
    ssDiamond

    ,
    ssStar

    ,
    ssTriangle

    ,
    ssTriangleInverted

    ,
    ssCrossSquare

    ,
    ssPlusSquare

    ,
    ssCrossCircle

    ,
    ssPlusCircle

    ,
    ssPeace

    ,
    ssPixmap

    ,
    ssCustom

  };
  Q_ENUMS(ScatterShape)

  QCPScatterStyle();
  QCPScatterStyle(ScatterShape shape, double size = 6);
  QCPScatterStyle(ScatterShape shape, const QColor &color, double size);
  QCPScatterStyle(ScatterShape shape, const QColor &color, const QColor &fill,
                  double size);
  QCPScatterStyle(ScatterShape shape, const QPen &pen, const QBrush &brush,
                  double size);
  QCPScatterStyle(const QPixmap &pixmap);
  QCPScatterStyle(const QPainterPath &customPath, const QPen &pen,
                  const QBrush &brush = Qt::NoBrush, double size = 6);

  double size() const { return mSize; }
  ScatterShape shape() const { return mShape; }
  QPen pen() const { return mPen; }
  QBrush brush() const { return mBrush; }
  QPixmap pixmap() const { return mPixmap; }
  QPainterPath customPath() const { return mCustomPath; }

  void setFromOther(const QCPScatterStyle &other, ScatterProperties properties);
  void setSize(double size);
  void setShape(ScatterShape shape);
  void setPen(const QPen &pen);
  void setBrush(const QBrush &brush);
  void setPixmap(const QPixmap &pixmap);
  void setCustomPath(const QPainterPath &customPath);

  bool isNone() const { return mShape == ssNone; }
  bool isPenDefined() const { return mPenDefined; }
  void undefinePen();
  void applyTo(QCPPainter *painter, const QPen &defaultPen) const;
  void drawShape(QCPPainter *painter, const QPointF &pos) const;
  void drawShape(QCPPainter *painter, double x, double y) const;

protected:
  double mSize;
  ScatterShape mShape;
  QPen mPen;
  QBrush mBrush;
  QPixmap mPixmap;
  QPainterPath mCustomPath;

  bool mPenDefined;
};
Q_DECLARE_TYPEINFO(QCPScatterStyle, Q_MOVABLE_TYPE);
Q_DECLARE_OPERATORS_FOR_FLAGS(QCPScatterStyle::ScatterProperties)
Q_DECLARE_METATYPE(QCPScatterStyle::ScatterProperty)
Q_DECLARE_METATYPE(QCPScatterStyle::ScatterShape)

template <class DataType>
inline bool qcpLessThanSortKey(const DataType &a, const DataType &b) {
  return a.sortKey() < b.sortKey();
}

template <class DataType>
class QCPDataContainer

{
public:
  typedef typename QVector<DataType>::const_iterator const_iterator;
  typedef typename QVector<DataType>::iterator iterator;

  QCPDataContainer();

  int size() const { return mData.size() - mPreallocSize; }
  bool isEmpty() const { return size() == 0; }
  bool autoSqueeze() const { return mAutoSqueeze; }

  void setAutoSqueeze(bool enabled);

  void set(const QCPDataContainer<DataType> &data);
  void set(const QVector<DataType> &data, bool alreadySorted = false);
  void add(const QCPDataContainer<DataType> &data);
  void add(const QVector<DataType> &data, bool alreadySorted = false);
  void add(const DataType &data);
  void removeBefore(double sortKey);
  void removeAfter(double sortKey);
  void remove(double sortKeyFrom, double sortKeyTo);
  void remove(double sortKey);
  void clear();
  void sort();
  void squeeze(bool preAllocation = true, bool postAllocation = true);

  const_iterator constBegin() const {
    return mData.constBegin() + mPreallocSize;
  }
  const_iterator constEnd() const { return mData.constEnd(); }
  iterator begin() { return mData.begin() + mPreallocSize; }
  iterator end() { return mData.end(); }
  const_iterator findBegin(double sortKey, bool expandedRange = true) const;
  const_iterator findEnd(double sortKey, bool expandedRange = true) const;
  const_iterator at(int index) const {
    return constBegin() + qBound(0, index, size());
  }
  QCPRange keyRange(bool &foundRange, QCP::SignDomain signDomain = QCP::sdBoth);
  QCPRange valueRange(bool &foundRange,
                      QCP::SignDomain signDomain = QCP::sdBoth,
                      const QCPRange &inKeyRange = QCPRange());
  QCPDataRange dataRange() const { return QCPDataRange(0, size()); }
  void limitIteratorsToDataRange(const_iterator &begin, const_iterator &end,
                                 const QCPDataRange &dataRange) const;

protected:
  bool mAutoSqueeze;

  QVector<DataType> mData;
  int mPreallocSize;
  int mPreallocIteration;

  void preallocateGrow(int minimumPreallocSize);
  void performAutoSqueeze();
};

template <class DataType>
QCPDataContainer<DataType>::QCPDataContainer()
    : mAutoSqueeze(true), mPreallocSize(0), mPreallocIteration(0) {}

template <class DataType>
void QCPDataContainer<DataType>::setAutoSqueeze(bool enabled) {
  if (mAutoSqueeze != enabled) {
    mAutoSqueeze = enabled;
    if (mAutoSqueeze)
      performAutoSqueeze();
  }
}

template <class DataType>
void QCPDataContainer<DataType>::set(const QCPDataContainer<DataType> &data) {
  clear();
  add(data);
}

template <class DataType>
void QCPDataContainer<DataType>::set(const QVector<DataType> &data,
                                     bool alreadySorted) {
  mData = data;
  mPreallocSize = 0;
  mPreallocIteration = 0;
  if (!alreadySorted)
    sort();
}

template <class DataType>
void QCPDataContainer<DataType>::add(const QCPDataContainer<DataType> &data) {
  if (data.isEmpty())
    return;

  const int n = data.size();
  const int oldSize = size();

  if (oldSize > 0 &&
      !qcpLessThanSortKey<DataType>(*constBegin(), *(data.constEnd() - 1)))

  {
    if (mPreallocSize < n)
      preallocateGrow(n);
    mPreallocSize -= n;
    std::copy(data.constBegin(), data.constEnd(), begin());
  } else

  {
    mData.resize(mData.size() + n);
    std::copy(data.constBegin(), data.constEnd(), end() - n);
    if (oldSize > 0 &&
        !qcpLessThanSortKey<DataType>(*(constEnd() - n - 1), *(constEnd() - n)))

      std::inplace_merge(begin(), end() - n, end(),
                         qcpLessThanSortKey<DataType>);
  }
}

template <class DataType>
void QCPDataContainer<DataType>::add(const QVector<DataType> &data,
                                     bool alreadySorted) {
  if (data.isEmpty())
    return;
  if (isEmpty()) {
    set(data, alreadySorted);
    return;
  }

  const int n = data.size();
  const int oldSize = size();

  if (alreadySorted && oldSize > 0 &&
      !qcpLessThanSortKey<DataType>(*constBegin(), *(data.constEnd() - 1)))

  {
    if (mPreallocSize < n)
      preallocateGrow(n);
    mPreallocSize -= n;
    std::copy(data.constBegin(), data.constEnd(), begin());
  } else

  {
    mData.resize(mData.size() + n);
    std::copy(data.constBegin(), data.constEnd(), end() - n);
    if (!alreadySorted)

      std::sort(end() - n, end(), qcpLessThanSortKey<DataType>);
    if (oldSize > 0 &&
        !qcpLessThanSortKey<DataType>(*(constEnd() - n - 1), *(constEnd() - n)))

      std::inplace_merge(begin(), end() - n, end(),
                         qcpLessThanSortKey<DataType>);
  }
}

template <class DataType>
void QCPDataContainer<DataType>::add(const DataType &data) {
  if (isEmpty() || !qcpLessThanSortKey<DataType>(data, *(constEnd() - 1)))

  {
    mData.append(data);
  } else if (qcpLessThanSortKey<DataType>(data, *constBegin()))

  {
    if (mPreallocSize < 1)
      preallocateGrow(1);
    --mPreallocSize;
    *begin() = data;
  } else

  {
    QCPDataContainer<DataType>::iterator insertionPoint =
        std::lower_bound(begin(), end(), data, qcpLessThanSortKey<DataType>);
    mData.insert(insertionPoint, data);
  }
}

template <class DataType>
void QCPDataContainer<DataType>::removeBefore(double sortKey) {
  QCPDataContainer<DataType>::iterator it = begin();
  QCPDataContainer<DataType>::iterator itEnd =
      std::lower_bound(begin(), end(), DataType::fromSortKey(sortKey),
                       qcpLessThanSortKey<DataType>);
  mPreallocSize += itEnd - it;

  if (mAutoSqueeze)
    performAutoSqueeze();
}

template <class DataType>
void QCPDataContainer<DataType>::removeAfter(double sortKey) {
  QCPDataContainer<DataType>::iterator it =
      std::upper_bound(begin(), end(), DataType::fromSortKey(sortKey),
                       qcpLessThanSortKey<DataType>);
  QCPDataContainer<DataType>::iterator itEnd = end();
  mData.erase(it, itEnd);

  if (mAutoSqueeze)
    performAutoSqueeze();
}

template <class DataType>
void QCPDataContainer<DataType>::remove(double sortKeyFrom, double sortKeyTo) {
  if (sortKeyFrom >= sortKeyTo || isEmpty())
    return;

  QCPDataContainer<DataType>::iterator it =
      std::lower_bound(begin(), end(), DataType::fromSortKey(sortKeyFrom),
                       qcpLessThanSortKey<DataType>);
  QCPDataContainer<DataType>::iterator itEnd =
      std::upper_bound(it, end(), DataType::fromSortKey(sortKeyTo),
                       qcpLessThanSortKey<DataType>);
  mData.erase(it, itEnd);
  if (mAutoSqueeze)
    performAutoSqueeze();
}

template <class DataType>
void QCPDataContainer<DataType>::remove(double sortKey) {
  QCPDataContainer::iterator it =
      std::lower_bound(begin(), end(), DataType::fromSortKey(sortKey),
                       qcpLessThanSortKey<DataType>);
  if (it != end() && it->sortKey() == sortKey) {
    if (it == begin())
      ++mPreallocSize;

    else
      mData.erase(it);
  }
  if (mAutoSqueeze)
    performAutoSqueeze();
}

template <class DataType> void QCPDataContainer<DataType>::clear() {
  mData.clear();
  mPreallocIteration = 0;
  mPreallocSize = 0;
}

template <class DataType> void QCPDataContainer<DataType>::sort() {
  std::sort(begin(), end(), qcpLessThanSortKey<DataType>);
}

template <class DataType>
void QCPDataContainer<DataType>::squeeze(bool preAllocation,
                                         bool postAllocation) {
  if (preAllocation) {
    if (mPreallocSize > 0) {
      std::copy(begin(), end(), mData.begin());
      mData.resize(size());
      mPreallocSize = 0;
    }
    mPreallocIteration = 0;
  }
  if (postAllocation)
    mData.squeeze();
}

template <class DataType>
typename QCPDataContainer<DataType>::const_iterator
QCPDataContainer<DataType>::findBegin(double sortKey,
                                      bool expandedRange) const {
  if (isEmpty())
    return constEnd();

  QCPDataContainer<DataType>::const_iterator it =
      std::lower_bound(constBegin(), constEnd(), DataType::fromSortKey(sortKey),
                       qcpLessThanSortKey<DataType>);
  if (expandedRange && it != constBegin())

    --it;
  return it;
}

template <class DataType>
typename QCPDataContainer<DataType>::const_iterator
QCPDataContainer<DataType>::findEnd(double sortKey, bool expandedRange) const {
  if (isEmpty())
    return constEnd();

  QCPDataContainer<DataType>::const_iterator it =
      std::upper_bound(constBegin(), constEnd(), DataType::fromSortKey(sortKey),
                       qcpLessThanSortKey<DataType>);
  if (expandedRange && it != constEnd())
    ++it;
  return it;
}

template <class DataType>
QCPRange QCPDataContainer<DataType>::keyRange(bool &foundRange,
                                              QCP::SignDomain signDomain) {
  if (isEmpty()) {
    foundRange = false;
    return QCPRange();
  }
  QCPRange range;
  bool haveLower = false;
  bool haveUpper = false;
  double current;

  QCPDataContainer<DataType>::const_iterator it = constBegin();
  QCPDataContainer<DataType>::const_iterator itEnd = constEnd();
  if (signDomain == QCP::sdBoth)

  {
    if (DataType::sortKeyIsMainKey())

    {
      while (it != itEnd)

      {
        if (!qIsNaN(it->mainValue())) {
          range.lower = it->mainKey();
          haveLower = true;
          break;
        }
        ++it;
      }
      it = itEnd;
      while (it != constBegin())

      {
        --it;
        if (!qIsNaN(it->mainValue())) {
          range.upper = it->mainKey();
          haveUpper = true;
          break;
        }
      }
    } else

    {
      while (it != itEnd) {
        if (!qIsNaN(it->mainValue())) {
          current = it->mainKey();
          if (current < range.lower || !haveLower) {
            range.lower = current;
            haveLower = true;
          }
          if (current > range.upper || !haveUpper) {
            range.upper = current;
            haveUpper = true;
          }
        }
        ++it;
      }
    }
  } else if (signDomain == QCP::sdNegative)

  {
    while (it != itEnd) {
      if (!qIsNaN(it->mainValue())) {
        current = it->mainKey();
        if ((current < range.lower || !haveLower) && current < 0) {
          range.lower = current;
          haveLower = true;
        }
        if ((current > range.upper || !haveUpper) && current < 0) {
          range.upper = current;
          haveUpper = true;
        }
      }
      ++it;
    }
  } else if (signDomain == QCP::sdPositive)

  {
    while (it != itEnd) {
      if (!qIsNaN(it->mainValue())) {
        current = it->mainKey();
        if ((current < range.lower || !haveLower) && current > 0) {
          range.lower = current;
          haveLower = true;
        }
        if ((current > range.upper || !haveUpper) && current > 0) {
          range.upper = current;
          haveUpper = true;
        }
      }
      ++it;
    }
  }

  foundRange = haveLower && haveUpper;
  return range;
}

template <class DataType>
QCPRange QCPDataContainer<DataType>::valueRange(bool &foundRange,
                                                QCP::SignDomain signDomain,
                                                const QCPRange &inKeyRange) {
  if (isEmpty()) {
    foundRange = false;
    return QCPRange();
  }
  QCPRange range;
  const bool restrictKeyRange = inKeyRange != QCPRange();
  bool haveLower = false;
  bool haveUpper = false;
  QCPRange current;
  QCPDataContainer<DataType>::const_iterator itBegin = constBegin();
  QCPDataContainer<DataType>::const_iterator itEnd = constEnd();
  if (DataType::sortKeyIsMainKey() && restrictKeyRange) {
    itBegin = findBegin(inKeyRange.lower);
    itEnd = findEnd(inKeyRange.upper);
  }
  if (signDomain == QCP::sdBoth)

  {
    for (QCPDataContainer<DataType>::const_iterator it = itBegin; it != itEnd;
         ++it) {
      if (restrictKeyRange && (it->mainKey() < inKeyRange.lower ||
                               it->mainKey() > inKeyRange.upper))
        continue;
      current = it->valueRange();
      if ((current.lower < range.lower || !haveLower) &&
          !qIsNaN(current.lower)) {
        range.lower = current.lower;
        haveLower = true;
      }
      if ((current.upper > range.upper || !haveUpper) &&
          !qIsNaN(current.upper)) {
        range.upper = current.upper;
        haveUpper = true;
      }
    }
  } else if (signDomain == QCP::sdNegative)

  {
    for (QCPDataContainer<DataType>::const_iterator it = itBegin; it != itEnd;
         ++it) {
      if (restrictKeyRange && (it->mainKey() < inKeyRange.lower ||
                               it->mainKey() > inKeyRange.upper))
        continue;
      current = it->valueRange();
      if ((current.lower < range.lower || !haveLower) && current.lower < 0 &&
          !qIsNaN(current.lower)) {
        range.lower = current.lower;
        haveLower = true;
      }
      if ((current.upper > range.upper || !haveUpper) && current.upper < 0 &&
          !qIsNaN(current.upper)) {
        range.upper = current.upper;
        haveUpper = true;
      }
    }
  } else if (signDomain == QCP::sdPositive)

  {
    for (QCPDataContainer<DataType>::const_iterator it = itBegin; it != itEnd;
         ++it) {
      if (restrictKeyRange && (it->mainKey() < inKeyRange.lower ||
                               it->mainKey() > inKeyRange.upper))
        continue;
      current = it->valueRange();
      if ((current.lower < range.lower || !haveLower) && current.lower > 0 &&
          !qIsNaN(current.lower)) {
        range.lower = current.lower;
        haveLower = true;
      }
      if ((current.upper > range.upper || !haveUpper) && current.upper > 0 &&
          !qIsNaN(current.upper)) {
        range.upper = current.upper;
        haveUpper = true;
      }
    }
  }

  foundRange = haveLower && haveUpper;
  return range;
}

template <class DataType>
void QCPDataContainer<DataType>::limitIteratorsToDataRange(
    const_iterator &begin, const_iterator &end,
    const QCPDataRange &dataRange) const {
  QCPDataRange iteratorRange(begin - constBegin(), end - constBegin());
  iteratorRange = iteratorRange.bounded(dataRange.bounded(this->dataRange()));
  begin = constBegin() + iteratorRange.begin();
  end = constBegin() + iteratorRange.end();
}

template <class DataType>
void QCPDataContainer<DataType>::preallocateGrow(int minimumPreallocSize) {
  if (minimumPreallocSize <= mPreallocSize)
    return;

  int newPreallocSize = minimumPreallocSize;
  newPreallocSize += (1u << qBound(4, mPreallocIteration + 4, 15)) - 12;

  ++mPreallocIteration;

  int sizeDifference = newPreallocSize - mPreallocSize;
  mData.resize(mData.size() + sizeDifference);
  std::copy_backward(mData.begin() + mPreallocSize,
                     mData.end() - sizeDifference, mData.end());
  mPreallocSize = newPreallocSize;
}

template <class DataType>
void QCPDataContainer<DataType>::performAutoSqueeze() {
  const int totalAlloc = mData.capacity();
  const int postAllocSize = totalAlloc - mData.size();
  const int usedSize = size();
  bool shrinkPostAllocation = false;
  bool shrinkPreAllocation = false;
  if (totalAlloc > 650000)

  {
    shrinkPostAllocation = postAllocSize > usedSize * 1.5;

    shrinkPreAllocation = mPreallocSize * 10 > usedSize;
  } else if (totalAlloc > 1000)

  {
    shrinkPostAllocation = postAllocSize > usedSize * 5;
    shrinkPreAllocation = mPreallocSize > usedSize * 1.5;
  }

  if (shrinkPreAllocation || shrinkPostAllocation)
    squeeze(shrinkPreAllocation, shrinkPostAllocation);
}

class QCP_LIB_DECL QCPSelectionDecorator {
  Q_GADGET
public:
  QCPSelectionDecorator();
  virtual ~QCPSelectionDecorator();

  QPen pen() const { return mPen; }
  QBrush brush() const { return mBrush; }
  QCPScatterStyle scatterStyle() const { return mScatterStyle; }
  QCPScatterStyle::ScatterProperties usedScatterProperties() const {
    return mUsedScatterProperties;
  }

  void setPen(const QPen &pen);
  void setBrush(const QBrush &brush);
  void setScatterStyle(const QCPScatterStyle &scatterStyle,
                       QCPScatterStyle::ScatterProperties usedProperties =
                           QCPScatterStyle::spPen);
  void setUsedScatterProperties(
      const QCPScatterStyle::ScatterProperties &properties);

  void applyPen(QCPPainter *painter) const;
  void applyBrush(QCPPainter *painter) const;
  QCPScatterStyle
  getFinalScatterStyle(const QCPScatterStyle &unselectedStyle) const;

  virtual void copyFrom(const QCPSelectionDecorator *other);
  virtual void drawDecoration(QCPPainter *painter, QCPDataSelection selection);

protected:
  QPen mPen;
  QBrush mBrush;
  QCPScatterStyle mScatterStyle;
  QCPScatterStyle::ScatterProperties mUsedScatterProperties;

  QCPAbstractPlottable *mPlottable;

  virtual bool registerWithPlottable(QCPAbstractPlottable *plottable);

private:
  Q_DISABLE_COPY(QCPSelectionDecorator)
  friend class QCPAbstractPlottable;
};
Q_DECLARE_METATYPE(QCPSelectionDecorator *)

class QCP_LIB_DECL QCPAbstractPlottable : public QCPLayerable {
  Q_OBJECT

  Q_PROPERTY(QString name READ name WRITE setName)
  Q_PROPERTY(bool antialiasedFill READ antialiasedFill WRITE setAntialiasedFill)
  Q_PROPERTY(bool antialiasedScatters READ antialiasedScatters WRITE
                 setAntialiasedScatters)
  Q_PROPERTY(QPen pen READ pen WRITE setPen)
  Q_PROPERTY(QBrush brush READ brush WRITE setBrush)
  Q_PROPERTY(QCPAxis *keyAxis READ keyAxis WRITE setKeyAxis)
  Q_PROPERTY(QCPAxis *valueAxis READ valueAxis WRITE setValueAxis)
  Q_PROPERTY(QCP::SelectionType selectable READ selectable WRITE setSelectable
                 NOTIFY selectableChanged)
  Q_PROPERTY(QCPDataSelection selection READ selection WRITE setSelection NOTIFY
                 selectionChanged)
  Q_PROPERTY(QCPSelectionDecorator *selectionDecorator READ selectionDecorator
                 WRITE setSelectionDecorator)

public:
  QCPAbstractPlottable(QCPAxis *keyAxis, QCPAxis *valueAxis);
  virtual ~QCPAbstractPlottable();

  QString name() const { return mName; }
  bool antialiasedFill() const { return mAntialiasedFill; }
  bool antialiasedScatters() const { return mAntialiasedScatters; }
  QPen pen() const { return mPen; }
  QBrush brush() const { return mBrush; }
  QCPAxis *keyAxis() const { return mKeyAxis.data(); }
  QCPAxis *valueAxis() const { return mValueAxis.data(); }
  QCP::SelectionType selectable() const { return mSelectable; }
  bool selected() const { return !mSelection.isEmpty(); }
  QCPDataSelection selection() const { return mSelection; }
  QCPSelectionDecorator *selectionDecorator() const {
    return mSelectionDecorator;
  }

  void setName(const QString &name);
  void setAntialiasedFill(bool enabled);
  void setAntialiasedScatters(bool enabled);
  void setPen(const QPen &pen);
  void setBrush(const QBrush &brush);
  void setKeyAxis(QCPAxis *axis);
  void setValueAxis(QCPAxis *axis);
  Q_SLOT void setSelectable(QCP::SelectionType selectable);
  Q_SLOT void setSelection(QCPDataSelection selection);
  void setSelectionDecorator(QCPSelectionDecorator *decorator);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE = 0;

  virtual QCPPlottableInterface1D *interface1D() { return 0; }
  virtual QCPRange
  getKeyRange(bool &foundRange,
              QCP::SignDomain inSignDomain = QCP::sdBoth) const = 0;
  virtual QCPRange
  getValueRange(bool &foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                const QCPRange &inKeyRange = QCPRange()) const = 0;

  void coordsToPixels(double key, double value, double &x, double &y) const;
  const QPointF coordsToPixels(double key, double value) const;
  void pixelsToCoords(double x, double y, double &key, double &value) const;
  void pixelsToCoords(const QPointF &pixelPos, double &key,
                      double &value) const;
  void rescaleAxes(bool onlyEnlarge = false) const;
  void rescaleKeyAxis(bool onlyEnlarge = false) const;
  void rescaleValueAxis(bool onlyEnlarge = false,
                        bool inKeyRange = false) const;
  bool addToLegend(QCPLegend *legend);
  bool addToLegend();
  bool removeFromLegend(QCPLegend *legend) const;
  bool removeFromLegend() const;

Q_SIGNALS:
  void selectionChanged(bool selected);
  void selectionChanged(const QCPDataSelection &selection);
  void selectableChanged(QCP::SelectionType selectable);

protected:
  QString mName;
  bool mAntialiasedFill, mAntialiasedScatters;
  QPen mPen;
  QBrush mBrush;
  QPointer<QCPAxis> mKeyAxis, mValueAxis;
  QCP::SelectionType mSelectable;
  QCPDataSelection mSelection;
  QCPSelectionDecorator *mSelectionDecorator;

  virtual QRect clipRect() const Q_DECL_OVERRIDE;
  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE = 0;
  virtual QCP::Interaction selectionCategory() const Q_DECL_OVERRIDE;
  void applyDefaultAntialiasingHint(QCPPainter *painter) const Q_DECL_OVERRIDE;

  virtual void selectEvent(QMouseEvent *event, bool additive,
                           const QVariant &details,
                           bool *selectionStateChanged) Q_DECL_OVERRIDE;
  virtual void deselectEvent(bool *selectionStateChanged) Q_DECL_OVERRIDE;

  virtual void drawLegendIcon(QCPPainter *painter,
                              const QRectF &rect) const = 0;

  void applyFillAntialiasingHint(QCPPainter *painter) const;
  void applyScattersAntialiasingHint(QCPPainter *painter) const;

private:
  Q_DISABLE_COPY(QCPAbstractPlottable)

  friend class QCustomPlot;
  friend class QCPAxis;
  friend class QCPPlottableLegendItem;
};

class QCP_LIB_DECL QCPItemAnchor {
  Q_GADGET
public:
  QCPItemAnchor(QCustomPlot *parentPlot, QCPAbstractItem *parentItem,
                const QString &name, int anchorId = -1);
  virtual ~QCPItemAnchor();

  QString name() const { return mName; }
  virtual QPointF pixelPosition() const;

protected:
  QString mName;

  QCustomPlot *mParentPlot;
  QCPAbstractItem *mParentItem;
  int mAnchorId;
  QSet<QCPItemPosition *> mChildrenX, mChildrenY;

  virtual QCPItemPosition *toQCPItemPosition() { return 0; }

  void addChildX(QCPItemPosition *pos);

  void removeChildX(QCPItemPosition *pos);

  void addChildY(QCPItemPosition *pos);

  void removeChildY(QCPItemPosition *pos);

private:
  Q_DISABLE_COPY(QCPItemAnchor)

  friend class QCPItemPosition;
};

class QCP_LIB_DECL QCPItemPosition : public QCPItemAnchor {
  Q_GADGET
public:
  enum PositionType {
    ptAbsolute

    ,
    ptViewportRatio

    ,
    ptAxisRectRatio

    ,
    ptPlotCoords

  };
  Q_ENUMS(PositionType)

  QCPItemPosition(QCustomPlot *parentPlot, QCPAbstractItem *parentItem,
                  const QString &name);
  virtual ~QCPItemPosition();

  PositionType type() const { return typeX(); }
  PositionType typeX() const { return mPositionTypeX; }
  PositionType typeY() const { return mPositionTypeY; }
  QCPItemAnchor *parentAnchor() const { return parentAnchorX(); }
  QCPItemAnchor *parentAnchorX() const { return mParentAnchorX; }
  QCPItemAnchor *parentAnchorY() const { return mParentAnchorY; }
  double key() const { return mKey; }
  double value() const { return mValue; }
  QPointF coords() const { return QPointF(mKey, mValue); }
  QCPAxis *keyAxis() const { return mKeyAxis.data(); }
  QCPAxis *valueAxis() const { return mValueAxis.data(); }
  QCPAxisRect *axisRect() const;
  virtual QPointF pixelPosition() const Q_DECL_OVERRIDE;

  void setType(PositionType type);
  void setTypeX(PositionType type);
  void setTypeY(PositionType type);
  bool setParentAnchor(QCPItemAnchor *parentAnchor,
                       bool keepPixelPosition = false);
  bool setParentAnchorX(QCPItemAnchor *parentAnchor,
                        bool keepPixelPosition = false);
  bool setParentAnchorY(QCPItemAnchor *parentAnchor,
                        bool keepPixelPosition = false);
  void setCoords(double key, double value);
  void setCoords(const QPointF &coords);
  void setAxes(QCPAxis *keyAxis, QCPAxis *valueAxis);
  void setAxisRect(QCPAxisRect *axisRect);
  void setPixelPosition(const QPointF &pixelPosition);

protected:
  PositionType mPositionTypeX, mPositionTypeY;
  QPointer<QCPAxis> mKeyAxis, mValueAxis;
  QPointer<QCPAxisRect> mAxisRect;
  double mKey, mValue;
  QCPItemAnchor *mParentAnchorX, *mParentAnchorY;

  virtual QCPItemPosition *toQCPItemPosition() Q_DECL_OVERRIDE { return this; }

private:
  Q_DISABLE_COPY(QCPItemPosition)
};
Q_DECLARE_METATYPE(QCPItemPosition::PositionType)

class QCP_LIB_DECL QCPAbstractItem : public QCPLayerable {
  Q_OBJECT

  Q_PROPERTY(bool clipToAxisRect READ clipToAxisRect WRITE setClipToAxisRect)
  Q_PROPERTY(QCPAxisRect *clipAxisRect READ clipAxisRect WRITE setClipAxisRect)
  Q_PROPERTY(bool selectable READ selectable WRITE setSelectable NOTIFY
                 selectableChanged)
  Q_PROPERTY(
      bool selected READ selected WRITE setSelected NOTIFY selectionChanged)

public:
  explicit QCPAbstractItem(QCustomPlot *parentPlot);
  virtual ~QCPAbstractItem();

  bool clipToAxisRect() const { return mClipToAxisRect; }
  QCPAxisRect *clipAxisRect() const;
  bool selectable() const { return mSelectable; }
  bool selected() const { return mSelected; }

  void setClipToAxisRect(bool clip);
  void setClipAxisRect(QCPAxisRect *rect);
  Q_SLOT void setSelectable(bool selectable);
  Q_SLOT void setSelected(bool selected);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE = 0;

  QList<QCPItemPosition *> positions() const { return mPositions; }
  QList<QCPItemAnchor *> anchors() const { return mAnchors; }
  QCPItemPosition *position(const QString &name) const;
  QCPItemAnchor *anchor(const QString &name) const;
  bool hasAnchor(const QString &name) const;

Q_SIGNALS:
  void selectionChanged(bool selected);
  void selectableChanged(bool selectable);

protected:
  bool mClipToAxisRect;
  QPointer<QCPAxisRect> mClipAxisRect;
  QList<QCPItemPosition *> mPositions;
  QList<QCPItemAnchor *> mAnchors;
  bool mSelectable, mSelected;

  virtual QCP::Interaction selectionCategory() const Q_DECL_OVERRIDE;
  virtual QRect clipRect() const Q_DECL_OVERRIDE;
  virtual void
  applyDefaultAntialiasingHint(QCPPainter *painter) const Q_DECL_OVERRIDE;
  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE = 0;

  virtual void selectEvent(QMouseEvent *event, bool additive,
                           const QVariant &details,
                           bool *selectionStateChanged) Q_DECL_OVERRIDE;
  virtual void deselectEvent(bool *selectionStateChanged) Q_DECL_OVERRIDE;

  virtual QPointF anchorPixelPosition(int anchorId) const;

  double rectDistance(const QRectF &rect, const QPointF &pos,
                      bool filledRect) const;
  QCPItemPosition *createPosition(const QString &name);
  QCPItemAnchor *createAnchor(const QString &name, int anchorId);

private:
  Q_DISABLE_COPY(QCPAbstractItem)

  friend class QCustomPlot;
  friend class QCPItemAnchor;
};

class QCP_LIB_DECL QCustomPlot : public QWidget {
  Q_OBJECT

  Q_PROPERTY(QRect viewport READ viewport WRITE setViewport)
  Q_PROPERTY(QPixmap background READ background WRITE setBackground)
  Q_PROPERTY(
      bool backgroundScaled READ backgroundScaled WRITE setBackgroundScaled)
  Q_PROPERTY(Qt::AspectRatioMode backgroundScaledMode READ backgroundScaledMode
                 WRITE setBackgroundScaledMode)
  Q_PROPERTY(QCPLayoutGrid *plotLayout READ plotLayout)
  Q_PROPERTY(bool autoAddPlottableToLegend READ autoAddPlottableToLegend WRITE
                 setAutoAddPlottableToLegend)
  Q_PROPERTY(int selectionTolerance READ selectionTolerance WRITE
                 setSelectionTolerance)
  Q_PROPERTY(bool noAntialiasingOnDrag READ noAntialiasingOnDrag WRITE
                 setNoAntialiasingOnDrag)
  Q_PROPERTY(Qt::KeyboardModifier multiSelectModifier READ multiSelectModifier
                 WRITE setMultiSelectModifier)
  Q_PROPERTY(bool openGl READ openGl WRITE setOpenGl)

public:
  enum LayerInsertMode {
    limBelow

    ,
    limAbove

  };
  Q_ENUMS(LayerInsertMode)

  enum RefreshPriority {
    rpImmediateRefresh

    ,
    rpQueuedRefresh

    ,
    rpRefreshHint

    ,
    rpQueuedReplot

  };
  Q_ENUMS(RefreshPriority)

  explicit QCustomPlot(QWidget *parent = 0);
  virtual ~QCustomPlot();

  QRect viewport() const { return mViewport; }
  double bufferDevicePixelRatio() const { return mBufferDevicePixelRatio; }
  QPixmap background() const { return mBackgroundPixmap; }
  bool backgroundScaled() const { return mBackgroundScaled; }
  Qt::AspectRatioMode backgroundScaledMode() const {
    return mBackgroundScaledMode;
  }
  QCPLayoutGrid *plotLayout() const { return mPlotLayout; }
  QCP::AntialiasedElements antialiasedElements() const {
    return mAntialiasedElements;
  }
  QCP::AntialiasedElements notAntialiasedElements() const {
    return mNotAntialiasedElements;
  }
  bool autoAddPlottableToLegend() const { return mAutoAddPlottableToLegend; }
  const QCP::Interactions interactions() const { return mInteractions; }
  int selectionTolerance() const { return mSelectionTolerance; }
  bool noAntialiasingOnDrag() const { return mNoAntialiasingOnDrag; }
  QCP::PlottingHints plottingHints() const { return mPlottingHints; }
  Qt::KeyboardModifier multiSelectModifier() const {
    return mMultiSelectModifier;
  }
  QCP::SelectionRectMode selectionRectMode() const {
    return mSelectionRectMode;
  }
  QCPSelectionRect *selectionRect() const { return mSelectionRect; }
  bool openGl() const { return mOpenGl; }

  void setViewport(const QRect &rect);
  void setBufferDevicePixelRatio(double ratio);
  void setBackground(const QPixmap &pm);
  void setBackground(const QPixmap &pm, bool scaled,
                     Qt::AspectRatioMode mode = Qt::KeepAspectRatioByExpanding);
  void setBackground(const QBrush &brush);
  void setBackgroundScaled(bool scaled);
  void setBackgroundScaledMode(Qt::AspectRatioMode mode);
  void
  setAntialiasedElements(const QCP::AntialiasedElements &antialiasedElements);
  void setAntialiasedElement(QCP::AntialiasedElement antialiasedElement,
                             bool enabled = true);
  void setNotAntialiasedElements(
      const QCP::AntialiasedElements &notAntialiasedElements);
  void setNotAntialiasedElement(QCP::AntialiasedElement notAntialiasedElement,
                                bool enabled = true);
  void setAutoAddPlottableToLegend(bool on);
  void setInteractions(const QCP::Interactions &interactions);
  void setInteraction(const QCP::Interaction &interaction, bool enabled = true);
  void setSelectionTolerance(int pixels);
  void setNoAntialiasingOnDrag(bool enabled);
  void setPlottingHints(const QCP::PlottingHints &hints);
  void setPlottingHint(QCP::PlottingHint hint, bool enabled = true);
  void setMultiSelectModifier(Qt::KeyboardModifier modifier);
  void setSelectionRectMode(QCP::SelectionRectMode mode);
  void setSelectionRect(QCPSelectionRect *selectionRect);
  void setOpenGl(bool enabled, int multisampling = 16);

  QCPAbstractPlottable *plottable(int index);
  QCPAbstractPlottable *plottable();
  bool removePlottable(QCPAbstractPlottable *plottable);
  bool removePlottable(int index);
  int clearPlottables();
  int plottableCount() const;
  QList<QCPAbstractPlottable *> selectedPlottables() const;
  QCPAbstractPlottable *plottableAt(const QPointF &pos,
                                    bool onlySelectable = false) const;
  bool hasPlottable(QCPAbstractPlottable *plottable) const;

  QCPGraph *graph(int index) const;
  QCPGraph *graph() const;
  QCPGraph *addGraph(QCPAxis *keyAxis = 0, QCPAxis *valueAxis = 0);
  bool removeGraph(QCPGraph *graph);
  bool removeGraph(int index);
  int clearGraphs();
  int graphCount() const;
  QList<QCPGraph *> selectedGraphs() const;

  QCPAbstractItem *item(int index) const;
  QCPAbstractItem *item() const;
  bool removeItem(QCPAbstractItem *item);
  bool removeItem(int index);
  int clearItems();
  int itemCount() const;
  QList<QCPAbstractItem *> selectedItems() const;
  QCPAbstractItem *itemAt(const QPointF &pos,
                          bool onlySelectable = false) const;
  bool hasItem(QCPAbstractItem *item) const;

  QCPLayer *layer(const QString &name) const;
  QCPLayer *layer(int index) const;
  QCPLayer *currentLayer() const;
  bool setCurrentLayer(const QString &name);
  bool setCurrentLayer(QCPLayer *layer);
  int layerCount() const;
  bool addLayer(const QString &name, QCPLayer *otherLayer = 0,
                LayerInsertMode insertMode = limAbove);
  bool removeLayer(QCPLayer *layer);
  bool moveLayer(QCPLayer *layer, QCPLayer *otherLayer,
                 LayerInsertMode insertMode = limAbove);

  int axisRectCount() const;
  QCPAxisRect *axisRect(int index = 0) const;
  QList<QCPAxisRect *> axisRects() const;
  QCPLayoutElement *layoutElementAt(const QPointF &pos) const;
  QCPAxisRect *axisRectAt(const QPointF &pos) const;
  Q_SLOT void rescaleAxes(bool onlyVisiblePlottables = false);

  QList<QCPAxis *> selectedAxes() const;
  QList<QCPLegend *> selectedLegends() const;
  Q_SLOT void deselectAll();

  bool savePdf(const QString &fileName, int width = 0, int height = 0,
               QCP::ExportPen exportPen = QCP::epAllowCosmetic,
               const QString &pdfCreator = QString(),
               const QString &pdfTitle = QString());
  bool savePng(const QString &fileName, int width = 0, int height = 0,
               double scale = 1.0, int quality = -1, int resolution = 96,
               QCP::ResolutionUnit resolutionUnit = QCP::ruDotsPerInch);
  bool saveJpg(const QString &fileName, int width = 0, int height = 0,
               double scale = 1.0, int quality = -1, int resolution = 96,
               QCP::ResolutionUnit resolutionUnit = QCP::ruDotsPerInch);
  bool saveBmp(const QString &fileName, int width = 0, int height = 0,
               double scale = 1.0, int resolution = 96,
               QCP::ResolutionUnit resolutionUnit = QCP::ruDotsPerInch);
  bool saveRastered(const QString &fileName, int width, int height,
                    double scale, const char *format, int quality = -1,
                    int resolution = 96,
                    QCP::ResolutionUnit resolutionUnit = QCP::ruDotsPerInch);
  QPixmap toPixmap(int width = 0, int height = 0, double scale = 1.0);
  void toPainter(QCPPainter *painter, int width = 0, int height = 0);
  Q_SLOT void replot(QCustomPlot::RefreshPriority refreshPriority =
                         QCustomPlot::rpRefreshHint);

  QCPAxis *xAxis, *yAxis, *xAxis2, *yAxis2;
  QCPLegend *legend;

Q_SIGNALS:
  void mouseDoubleClick(QMouseEvent *event);
  void mousePress(QMouseEvent *event);
  void mouseMove(QMouseEvent *event);
  void mouseRelease(QMouseEvent *event);
  void mouseWheel(QWheelEvent *event);

  void plottableClick(QCPAbstractPlottable *plottable, int dataIndex,
                      QMouseEvent *event);
  void plottableDoubleClick(QCPAbstractPlottable *plottable, int dataIndex,
                            QMouseEvent *event);
  void itemClick(QCPAbstractItem *item, QMouseEvent *event);
  void itemDoubleClick(QCPAbstractItem *item, QMouseEvent *event);
  void axisClick(QCPAxis *axis, QCPAxis::SelectablePart part,
                 QMouseEvent *event);
  void axisDoubleClick(QCPAxis *axis, QCPAxis::SelectablePart part,
                       QMouseEvent *event);
  void legendClick(QCPLegend *legend, QCPAbstractLegendItem *item,
                   QMouseEvent *event);
  void legendDoubleClick(QCPLegend *legend, QCPAbstractLegendItem *item,
                         QMouseEvent *event);

  void selectionChangedByUser();
  void beforeReplot();
  void afterReplot();

protected:
  QRect mViewport;
  double mBufferDevicePixelRatio;
  QCPLayoutGrid *mPlotLayout;
  bool mAutoAddPlottableToLegend;
  QList<QCPAbstractPlottable *> mPlottables;
  QList<QCPGraph *> mGraphs;

  QList<QCPAbstractItem *> mItems;
  QList<QCPLayer *> mLayers;
  QCP::AntialiasedElements mAntialiasedElements, mNotAntialiasedElements;
  QCP::Interactions mInteractions;
  int mSelectionTolerance;
  bool mNoAntialiasingOnDrag;
  QBrush mBackgroundBrush;
  QPixmap mBackgroundPixmap;
  QPixmap mScaledBackgroundPixmap;
  bool mBackgroundScaled;
  Qt::AspectRatioMode mBackgroundScaledMode;
  QCPLayer *mCurrentLayer;
  QCP::PlottingHints mPlottingHints;
  Qt::KeyboardModifier mMultiSelectModifier;
  QCP::SelectionRectMode mSelectionRectMode;
  QCPSelectionRect *mSelectionRect;
  bool mOpenGl;

  QList<QSharedPointer<QCPAbstractPaintBuffer>> mPaintBuffers;
  QPoint mMousePressPos;
  bool mMouseHasMoved;
  QPointer<QCPLayerable> mMouseEventLayerable;
  QPointer<QCPLayerable> mMouseSignalLayerable;
  QVariant mMouseEventLayerableDetails;
  QVariant mMouseSignalLayerableDetails;
  bool mReplotting;
  bool mReplotQueued;
  int mOpenGlMultisamples;
  QCP::AntialiasedElements mOpenGlAntialiasedElementsBackup;
  bool mOpenGlCacheLabelsBackup;
#ifdef QCP_OPENGL_FBO
  QSharedPointer<QOpenGLContext> mGlContext;
  QSharedPointer<QSurface> mGlSurface;
  QSharedPointer<QOpenGLPaintDevice> mGlPaintDevice;
#endif

  virtual QSize minimumSizeHint() const Q_DECL_OVERRIDE;
  virtual QSize sizeHint() const Q_DECL_OVERRIDE;
  virtual void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
  virtual void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
  virtual void mouseDoubleClickEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
  virtual void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
  virtual void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
  virtual void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
  virtual void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;

  virtual void draw(QCPPainter *painter);
  virtual void updateLayout();
  virtual void axisRemoved(QCPAxis *axis);
  virtual void legendRemoved(QCPLegend *legend);
  Q_SLOT virtual void processRectSelection(QRect rect, QMouseEvent *event);
  Q_SLOT virtual void processRectZoom(QRect rect, QMouseEvent *event);
  Q_SLOT virtual void processPointSelection(QMouseEvent *event);

  bool registerPlottable(QCPAbstractPlottable *plottable);
  bool registerGraph(QCPGraph *graph);
  bool registerItem(QCPAbstractItem *item);
  void updateLayerIndices() const;
  QCPLayerable *layerableAt(const QPointF &pos, bool onlySelectable,
                            QVariant *selectionDetails = 0) const;
  QList<QCPLayerable *>
  layerableListAt(const QPointF &pos, bool onlySelectable,
                  QList<QVariant> *selectionDetails = 0) const;
  void drawBackground(QCPPainter *painter);
  void setupPaintBuffers();
  QCPAbstractPaintBuffer *createPaintBuffer();
  bool hasInvalidatedPaintBuffers();
  bool setupOpenGl();
  void freeOpenGl();

  friend class QCPLegend;
  friend class QCPAxis;
  friend class QCPLayer;
  friend class QCPAxisRect;
  friend class QCPAbstractPlottable;
  friend class QCPGraph;
  friend class QCPAbstractItem;
};
Q_DECLARE_METATYPE(QCustomPlot::LayerInsertMode)
Q_DECLARE_METATYPE(QCustomPlot::RefreshPriority)

class QCPPlottableInterface1D {
public:
  virtual ~QCPPlottableInterface1D() {}

  virtual int dataCount() const = 0;
  virtual double dataMainKey(int index) const = 0;
  virtual double dataSortKey(int index) const = 0;
  virtual double dataMainValue(int index) const = 0;
  virtual QCPRange dataValueRange(int index) const = 0;
  virtual QPointF dataPixelPosition(int index) const = 0;
  virtual bool sortKeyIsMainKey() const = 0;
  virtual QCPDataSelection selectTestRect(const QRectF &rect,
                                          bool onlySelectable) const = 0;
  virtual int findBegin(double sortKey, bool expandedRange = true) const = 0;
  virtual int findEnd(double sortKey, bool expandedRange = true) const = 0;
};

template <class DataType>
class QCPAbstractPlottable1D : public QCPAbstractPlottable,
                               public QCPPlottableInterface1D

{
public:
  QCPAbstractPlottable1D(QCPAxis *keyAxis, QCPAxis *valueAxis);
  virtual ~QCPAbstractPlottable1D();

  virtual int dataCount() const Q_DECL_OVERRIDE;
  virtual double dataMainKey(int index) const Q_DECL_OVERRIDE;
  virtual double dataSortKey(int index) const Q_DECL_OVERRIDE;
  virtual double dataMainValue(int index) const Q_DECL_OVERRIDE;
  virtual QCPRange dataValueRange(int index) const Q_DECL_OVERRIDE;
  virtual QPointF dataPixelPosition(int index) const Q_DECL_OVERRIDE;
  virtual bool sortKeyIsMainKey() const Q_DECL_OVERRIDE;
  virtual QCPDataSelection
  selectTestRect(const QRectF &rect, bool onlySelectable) const Q_DECL_OVERRIDE;
  virtual int findBegin(double sortKey,
                        bool expandedRange = true) const Q_DECL_OVERRIDE;
  virtual int findEnd(double sortKey,
                      bool expandedRange = true) const Q_DECL_OVERRIDE;

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;
  virtual QCPPlottableInterface1D *interface1D() Q_DECL_OVERRIDE {
    return this;
  }

protected:
  QSharedPointer<QCPDataContainer<DataType>> mDataContainer;

  void getDataSegments(QList<QCPDataRange> &selectedSegments,
                       QList<QCPDataRange> &unselectedSegments) const;
  void drawPolyline(QCPPainter *painter,
                    const QVector<QPointF> &lineData) const;

private:
  Q_DISABLE_COPY(QCPAbstractPlottable1D)
};

template <class DataType>
QCPAbstractPlottable1D<DataType>::QCPAbstractPlottable1D(QCPAxis *keyAxis,
                                                         QCPAxis *valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis),
      mDataContainer(new QCPDataContainer<DataType>) {}

template <class DataType>
QCPAbstractPlottable1D<DataType>::~QCPAbstractPlottable1D() {}

template <class DataType>
int QCPAbstractPlottable1D<DataType>::dataCount() const {
  return mDataContainer->size();
}

template <class DataType>
double QCPAbstractPlottable1D<DataType>::dataMainKey(int index) const {
  if (index >= 0 && index < mDataContainer->size()) {
    return (mDataContainer->constBegin() + index)->mainKey();
  } else {
    qDebug() << Q_FUNC_INFO << "Index out of bounds" << index;
    return 0;
  }
}

template <class DataType>
double QCPAbstractPlottable1D<DataType>::dataSortKey(int index) const {
  if (index >= 0 && index < mDataContainer->size()) {
    return (mDataContainer->constBegin() + index)->sortKey();
  } else {
    qDebug() << Q_FUNC_INFO << "Index out of bounds" << index;
    return 0;
  }
}

template <class DataType>
double QCPAbstractPlottable1D<DataType>::dataMainValue(int index) const {
  if (index >= 0 && index < mDataContainer->size()) {
    return (mDataContainer->constBegin() + index)->mainValue();
  } else {
    qDebug() << Q_FUNC_INFO << "Index out of bounds" << index;
    return 0;
  }
}

template <class DataType>
QCPRange QCPAbstractPlottable1D<DataType>::dataValueRange(int index) const {
  if (index >= 0 && index < mDataContainer->size()) {
    return (mDataContainer->constBegin() + index)->valueRange();
  } else {
    qDebug() << Q_FUNC_INFO << "Index out of bounds" << index;
    return QCPRange(0, 0);
  }
}

template <class DataType>
QPointF QCPAbstractPlottable1D<DataType>::dataPixelPosition(int index) const {
  if (index >= 0 && index < mDataContainer->size()) {
    const typename QCPDataContainer<DataType>::const_iterator it =
        mDataContainer->constBegin() + index;
    return coordsToPixels(it->mainKey(), it->mainValue());
  } else {
    qDebug() << Q_FUNC_INFO << "Index out of bounds" << index;
    return QPointF();
  }
}

template <class DataType>
bool QCPAbstractPlottable1D<DataType>::sortKeyIsMainKey() const {
  return DataType::sortKeyIsMainKey();
}

template <class DataType>
QCPDataSelection
QCPAbstractPlottable1D<DataType>::selectTestRect(const QRectF &rect,
                                                 bool onlySelectable) const {
  QCPDataSelection result;
  if ((onlySelectable && mSelectable == QCP::stNone) ||
      mDataContainer->isEmpty())
    return result;
  if (!mKeyAxis || !mValueAxis)
    return result;

  double key1, value1, key2, value2;
  pixelsToCoords(rect.topLeft(), key1, value1);
  pixelsToCoords(rect.bottomRight(), key2, value2);
  QCPRange keyRange(key1, key2);

  QCPRange valueRange(value1, value2);
  typename QCPDataContainer<DataType>::const_iterator begin =
      mDataContainer->constBegin();
  typename QCPDataContainer<DataType>::const_iterator end =
      mDataContainer->constEnd();
  if (DataType::sortKeyIsMainKey())

  {
    begin = mDataContainer->findBegin(keyRange.lower, false);
    end = mDataContainer->findEnd(keyRange.upper, false);
  }
  if (begin == end)
    return result;

  int currentSegmentBegin = -1;

  for (typename QCPDataContainer<DataType>::const_iterator it = begin;
       it != end; ++it) {
    if (currentSegmentBegin == -1) {
      if (valueRange.contains(it->mainValue()) &&
          keyRange.contains(it->mainKey()))

        currentSegmentBegin = it - mDataContainer->constBegin();
    } else if (!valueRange.contains(it->mainValue()) ||
               !keyRange.contains(it->mainKey()))

    {
      result.addDataRange(
          QCPDataRange(currentSegmentBegin, it - mDataContainer->constBegin()),
          false);
      currentSegmentBegin = -1;
    }
  }

  if (currentSegmentBegin != -1)
    result.addDataRange(
        QCPDataRange(currentSegmentBegin, end - mDataContainer->constBegin()),
        false);

  result.simplify();
  return result;
}

template <class DataType>
int QCPAbstractPlottable1D<DataType>::findBegin(double sortKey,
                                                bool expandedRange) const {
  return mDataContainer->findBegin(sortKey, expandedRange) -
         mDataContainer->constBegin();
}

template <class DataType>
int QCPAbstractPlottable1D<DataType>::findEnd(double sortKey,
                                              bool expandedRange) const {
  return mDataContainer->findEnd(sortKey, expandedRange) -
         mDataContainer->constBegin();
}

template <class DataType>
double QCPAbstractPlottable1D<DataType>::selectTest(const QPointF &pos,
                                                    bool onlySelectable,
                                                    QVariant *details) const {
  if ((onlySelectable && mSelectable == QCP::stNone) ||
      mDataContainer->isEmpty())
    return -1;
  if (!mKeyAxis || !mValueAxis)
    return -1;

  QCPDataSelection selectionResult;
  double minDistSqr = (std::numeric_limits<double>::max)();
  int minDistIndex = mDataContainer->size();

  typename QCPDataContainer<DataType>::const_iterator begin =
      mDataContainer->constBegin();
  typename QCPDataContainer<DataType>::const_iterator end =
      mDataContainer->constEnd();
  if (DataType::sortKeyIsMainKey())

  {
    double posKeyMin, posKeyMax, dummy;
    pixelsToCoords(pos - QPointF(mParentPlot->selectionTolerance(),
                                 mParentPlot->selectionTolerance()),
                   posKeyMin, dummy);
    pixelsToCoords(pos + QPointF(mParentPlot->selectionTolerance(),
                                 mParentPlot->selectionTolerance()),
                   posKeyMax, dummy);
    if (posKeyMin > posKeyMax)
      qSwap(posKeyMin, posKeyMax);
    begin = mDataContainer->findBegin(posKeyMin, true);
    end = mDataContainer->findEnd(posKeyMax, true);
  }
  if (begin == end)
    return -1;
  QCPRange keyRange(mKeyAxis->range());
  QCPRange valueRange(mValueAxis->range());
  for (typename QCPDataContainer<DataType>::const_iterator it = begin;
       it != end; ++it) {
    const double mainKey = it->mainKey();
    const double mainValue = it->mainValue();
    if (keyRange.contains(mainKey) && valueRange.contains(mainValue))

    {
      const double currentDistSqr =
          QCPVector2D(coordsToPixels(mainKey, mainValue) - pos).lengthSquared();
      if (currentDistSqr < minDistSqr) {
        minDistSqr = currentDistSqr;
        minDistIndex = it - mDataContainer->constBegin();
      }
    }
  }
  if (minDistIndex != mDataContainer->size())
    selectionResult.addDataRange(QCPDataRange(minDistIndex, minDistIndex + 1),
                                 false);

  selectionResult.simplify();
  if (details)
    details->setValue(selectionResult);
  return qSqrt(minDistSqr);
}

template <class DataType>
void QCPAbstractPlottable1D<DataType>::getDataSegments(
    QList<QCPDataRange> &selectedSegments,
    QList<QCPDataRange> &unselectedSegments) const {
  selectedSegments.clear();
  unselectedSegments.clear();
  if (mSelectable == QCP::stWhole)

  {
    if (selected())
      selectedSegments << QCPDataRange(0, dataCount());
    else
      unselectedSegments << QCPDataRange(0, dataCount());
  } else {
    QCPDataSelection sel(selection());
    sel.simplify();
    selectedSegments = sel.dataRanges();
    unselectedSegments = sel.inverse(QCPDataRange(0, dataCount())).dataRanges();
  }
}

template <class DataType>
void QCPAbstractPlottable1D<DataType>::drawPolyline(
    QCPPainter *painter, const QVector<QPointF> &lineData) const {
  if (mParentPlot->plottingHints().testFlag(QCP::phFastPolylines) &&
      painter->pen().style() == Qt::SolidLine &&
      !painter->modes().testFlag(QCPPainter::pmVectorized) &&
      !painter->modes().testFlag(QCPPainter::pmNoCaching)) {
    int i = 0;
    bool lastIsNan = false;
    const int lineDataSize = lineData.size();
    while (i < lineDataSize &&
           (qIsNaN(lineData.at(i).y()) || qIsNaN(lineData.at(i).x())))

      ++i;
    ++i;

    while (i < lineDataSize) {
      if (!qIsNaN(lineData.at(i).y()) && !qIsNaN(lineData.at(i).x()))

      {
        if (!lastIsNan)
          painter->drawLine(lineData.at(i - 1), lineData.at(i));
        else
          lastIsNan = false;
      } else
        lastIsNan = true;
      ++i;
    }
  } else {
    int segmentStart = 0;
    int i = 0;
    const int lineDataSize = lineData.size();
    while (i < lineDataSize) {
      if (qIsNaN(lineData.at(i).y()) || qIsNaN(lineData.at(i).x()) ||
          qIsInf(lineData.at(i).y()))

      {
        painter->drawPolyline(lineData.constData() + segmentStart,
                              i - segmentStart);

        segmentStart = i + 1;
      }
      ++i;
    }

    painter->drawPolyline(lineData.constData() + segmentStart,
                          lineDataSize - segmentStart);
  }
}

class QCP_LIB_DECL QCPColorGradient {
  Q_GADGET
public:
  enum ColorInterpolation {
    ciRGB

    ,
    ciHSV

  };
  Q_ENUMS(ColorInterpolation)

  enum GradientPreset {
    gpGrayscale

    ,
    gpHot

    ,
    gpCold

    ,
    gpNight

    ,
    gpCandy

    ,
    gpGeography

    ,
    gpIon

    ,
    gpThermal

    ,
    gpPolar

    ,
    gpSpectrum

    ,
    gpJet

    ,
    gpHues

  };
  Q_ENUMS(GradientPreset)

  QCPColorGradient();
  QCPColorGradient(GradientPreset preset);
  bool operator==(const QCPColorGradient &other) const;
  bool operator!=(const QCPColorGradient &other) const {
    return !(*this == other);
  }

  int levelCount() const { return mLevelCount; }
  QMap<double, QColor> colorStops() const { return mColorStops; }
  ColorInterpolation colorInterpolation() const { return mColorInterpolation; }
  bool periodic() const { return mPeriodic; }

  void setLevelCount(int n);
  void setColorStops(const QMap<double, QColor> &colorStops);
  void setColorStopAt(double position, const QColor &color);
  void setColorInterpolation(ColorInterpolation interpolation);
  void setPeriodic(bool enabled);

  void colorize(const double *data, const QCPRange &range, QRgb *scanLine,
                int n, int dataIndexFactor = 1, bool logarithmic = false);
  void colorize(const double *data, const unsigned char *alpha,
                const QCPRange &range, QRgb *scanLine, int n,
                int dataIndexFactor = 1, bool logarithmic = false);
  QRgb color(double position, const QCPRange &range, bool logarithmic = false);
  void loadPreset(GradientPreset preset);
  void clearColorStops();
  QCPColorGradient inverted() const;

protected:
  int mLevelCount;
  QMap<double, QColor> mColorStops;
  ColorInterpolation mColorInterpolation;
  bool mPeriodic;

  QVector<QRgb> mColorBuffer;

  bool mColorBufferInvalidated;

  bool stopsUseAlpha() const;
  void updateColorBuffer();
};
Q_DECLARE_METATYPE(QCPColorGradient::ColorInterpolation)
Q_DECLARE_METATYPE(QCPColorGradient::GradientPreset)

class QCP_LIB_DECL QCPSelectionDecoratorBracket : public QCPSelectionDecorator {
  Q_GADGET
public:
  enum BracketStyle {
    bsSquareBracket

    ,
    bsHalfEllipse

    ,
    bsEllipse

    ,
    bsPlus

    ,
    bsUserStyle

  };
  Q_ENUMS(BracketStyle)

  QCPSelectionDecoratorBracket();
  virtual ~QCPSelectionDecoratorBracket();

  QPen bracketPen() const { return mBracketPen; }
  QBrush bracketBrush() const { return mBracketBrush; }
  int bracketWidth() const { return mBracketWidth; }
  int bracketHeight() const { return mBracketHeight; }
  BracketStyle bracketStyle() const { return mBracketStyle; }
  bool tangentToData() const { return mTangentToData; }
  int tangentAverage() const { return mTangentAverage; }

  void setBracketPen(const QPen &pen);
  void setBracketBrush(const QBrush &brush);
  void setBracketWidth(int width);
  void setBracketHeight(int height);
  void setBracketStyle(BracketStyle style);
  void setTangentToData(bool enabled);
  void setTangentAverage(int pointCount);

  virtual void drawBracket(QCPPainter *painter, int direction) const;

  virtual void drawDecoration(QCPPainter *painter,
                              QCPDataSelection selection) Q_DECL_OVERRIDE;

protected:
  QPen mBracketPen;
  QBrush mBracketBrush;
  int mBracketWidth;
  int mBracketHeight;
  BracketStyle mBracketStyle;
  bool mTangentToData;
  int mTangentAverage;

  double getTangentAngle(const QCPPlottableInterface1D *interface1d,
                         int dataIndex, int direction) const;
  QPointF getPixelCoordinates(const QCPPlottableInterface1D *interface1d,
                              int dataIndex) const;
};
Q_DECLARE_METATYPE(QCPSelectionDecoratorBracket::BracketStyle)

class QCP_LIB_DECL QCPAxisRect : public QCPLayoutElement {
  Q_OBJECT

  Q_PROPERTY(QPixmap background READ background WRITE setBackground)
  Q_PROPERTY(
      bool backgroundScaled READ backgroundScaled WRITE setBackgroundScaled)
  Q_PROPERTY(Qt::AspectRatioMode backgroundScaledMode READ backgroundScaledMode
                 WRITE setBackgroundScaledMode)
  Q_PROPERTY(Qt::Orientations rangeDrag READ rangeDrag WRITE setRangeDrag)
  Q_PROPERTY(Qt::Orientations rangeZoom READ rangeZoom WRITE setRangeZoom)

public:
  explicit QCPAxisRect(QCustomPlot *parentPlot, bool setupDefaultAxes = true);
  virtual ~QCPAxisRect();

  QPixmap background() const { return mBackgroundPixmap; }
  QBrush backgroundBrush() const { return mBackgroundBrush; }
  bool backgroundScaled() const { return mBackgroundScaled; }
  Qt::AspectRatioMode backgroundScaledMode() const {
    return mBackgroundScaledMode;
  }
  Qt::Orientations rangeDrag() const { return mRangeDrag; }
  Qt::Orientations rangeZoom() const { return mRangeZoom; }
  QCPAxis *rangeDragAxis(Qt::Orientation orientation);
  QCPAxis *rangeZoomAxis(Qt::Orientation orientation);
  QList<QCPAxis *> rangeDragAxes(Qt::Orientation orientation);
  QList<QCPAxis *> rangeZoomAxes(Qt::Orientation orientation);
  double rangeZoomFactor(Qt::Orientation orientation);

  void setBackground(const QPixmap &pm);
  void setBackground(const QPixmap &pm, bool scaled,
                     Qt::AspectRatioMode mode = Qt::KeepAspectRatioByExpanding);
  void setBackground(const QBrush &brush);
  void setBackgroundScaled(bool scaled);
  void setBackgroundScaledMode(Qt::AspectRatioMode mode);
  void setRangeDrag(Qt::Orientations orientations);
  void setRangeZoom(Qt::Orientations orientations);
  void setRangeDragAxes(QCPAxis *horizontal, QCPAxis *vertical);
  void setRangeDragAxes(QList<QCPAxis *> axes);
  void setRangeDragAxes(QList<QCPAxis *> horizontal, QList<QCPAxis *> vertical);
  void setRangeZoomAxes(QCPAxis *horizontal, QCPAxis *vertical);
  void setRangeZoomAxes(QList<QCPAxis *> axes);
  void setRangeZoomAxes(QList<QCPAxis *> horizontal, QList<QCPAxis *> vertical);
  void setRangeZoomFactor(double horizontalFactor, double verticalFactor);
  void setRangeZoomFactor(double factor);

  int axisCount(QCPAxis::AxisType type) const;
  QCPAxis *axis(QCPAxis::AxisType type, int index = 0) const;
  QList<QCPAxis *> axes(QCPAxis::AxisTypes types) const;
  QList<QCPAxis *> axes() const;
  QCPAxis *addAxis(QCPAxis::AxisType type, QCPAxis *axis = 0);
  QList<QCPAxis *> addAxes(QCPAxis::AxisTypes types);
  bool removeAxis(QCPAxis *axis);
  QCPLayoutInset *insetLayout() const { return mInsetLayout; }

  void zoom(const QRectF &pixelRect);
  void zoom(const QRectF &pixelRect, const QList<QCPAxis *> &affectedAxes);
  void setupFullAxesBox(bool connectRanges = false);
  QList<QCPAbstractPlottable *> plottables() const;
  QList<QCPGraph *> graphs() const;
  QList<QCPAbstractItem *> items() const;

  int left() const { return mRect.left(); }
  int right() const { return mRect.right(); }
  int top() const { return mRect.top(); }
  int bottom() const { return mRect.bottom(); }
  int width() const { return mRect.width(); }
  int height() const { return mRect.height(); }
  QSize size() const { return mRect.size(); }
  QPoint topLeft() const { return mRect.topLeft(); }
  QPoint topRight() const { return mRect.topRight(); }
  QPoint bottomLeft() const { return mRect.bottomLeft(); }
  QPoint bottomRight() const { return mRect.bottomRight(); }
  QPoint center() const { return mRect.center(); }

  virtual void update(UpdatePhase phase) Q_DECL_OVERRIDE;
  virtual QList<QCPLayoutElement *>
  elements(bool recursive) const Q_DECL_OVERRIDE;

protected:
  QBrush mBackgroundBrush;
  QPixmap mBackgroundPixmap;
  QPixmap mScaledBackgroundPixmap;
  bool mBackgroundScaled;
  Qt::AspectRatioMode mBackgroundScaledMode;
  QCPLayoutInset *mInsetLayout;
  Qt::Orientations mRangeDrag, mRangeZoom;
  QList<QPointer<QCPAxis>> mRangeDragHorzAxis, mRangeDragVertAxis;
  QList<QPointer<QCPAxis>> mRangeZoomHorzAxis, mRangeZoomVertAxis;
  double mRangeZoomFactorHorz, mRangeZoomFactorVert;

  QList<QCPRange> mDragStartHorzRange, mDragStartVertRange;
  QCP::AntialiasedElements mAADragBackup, mNotAADragBackup;
  bool mDragging;
  QHash<QCPAxis::AxisType, QList<QCPAxis *>> mAxes;

  virtual void
  applyDefaultAntialiasingHint(QCPPainter *painter) const Q_DECL_OVERRIDE;
  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual int calculateAutoMargin(QCP::MarginSide side) Q_DECL_OVERRIDE;
  virtual void layoutChanged() Q_DECL_OVERRIDE;

  virtual void mousePressEvent(QMouseEvent *event,
                               const QVariant &details) Q_DECL_OVERRIDE;
  virtual void mouseMoveEvent(QMouseEvent *event,
                              const QPointF &startPos) Q_DECL_OVERRIDE;
  virtual void mouseReleaseEvent(QMouseEvent *event,
                                 const QPointF &startPos) Q_DECL_OVERRIDE;
  virtual void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;

  void drawBackground(QCPPainter *painter);
  void updateAxesOffset(QCPAxis::AxisType type);

private:
  Q_DISABLE_COPY(QCPAxisRect)

  friend class QCustomPlot;
};

class QCP_LIB_DECL QCPAbstractLegendItem : public QCPLayoutElement {
  Q_OBJECT

  Q_PROPERTY(QCPLegend *parentLegend READ parentLegend)
  Q_PROPERTY(QFont font READ font WRITE setFont)
  Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
  Q_PROPERTY(QFont selectedFont READ selectedFont WRITE setSelectedFont)
  Q_PROPERTY(QColor selectedTextColor READ selectedTextColor WRITE
                 setSelectedTextColor)
  Q_PROPERTY(bool selectable READ selectable WRITE setSelectable NOTIFY
                 selectionChanged)
  Q_PROPERTY(
      bool selected READ selected WRITE setSelected NOTIFY selectableChanged)

public:
  explicit QCPAbstractLegendItem(QCPLegend *parent);

  QCPLegend *parentLegend() const { return mParentLegend; }
  QFont font() const { return mFont; }
  QColor textColor() const { return mTextColor; }
  QFont selectedFont() const { return mSelectedFont; }
  QColor selectedTextColor() const { return mSelectedTextColor; }
  bool selectable() const { return mSelectable; }
  bool selected() const { return mSelected; }

  void setFont(const QFont &font);
  void setTextColor(const QColor &color);
  void setSelectedFont(const QFont &font);
  void setSelectedTextColor(const QColor &color);
  Q_SLOT void setSelectable(bool selectable);
  Q_SLOT void setSelected(bool selected);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;

Q_SIGNALS:
  void selectionChanged(bool selected);
  void selectableChanged(bool selectable);

protected:
  QCPLegend *mParentLegend;
  QFont mFont;
  QColor mTextColor;
  QFont mSelectedFont;
  QColor mSelectedTextColor;
  bool mSelectable, mSelected;

  virtual QCP::Interaction selectionCategory() const Q_DECL_OVERRIDE;
  virtual void
  applyDefaultAntialiasingHint(QCPPainter *painter) const Q_DECL_OVERRIDE;
  virtual QRect clipRect() const Q_DECL_OVERRIDE;
  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE = 0;

  virtual void selectEvent(QMouseEvent *event, bool additive,
                           const QVariant &details,
                           bool *selectionStateChanged) Q_DECL_OVERRIDE;
  virtual void deselectEvent(bool *selectionStateChanged) Q_DECL_OVERRIDE;

private:
  Q_DISABLE_COPY(QCPAbstractLegendItem)

  friend class QCPLegend;
};

class QCP_LIB_DECL QCPPlottableLegendItem : public QCPAbstractLegendItem {
  Q_OBJECT
public:
  QCPPlottableLegendItem(QCPLegend *parent, QCPAbstractPlottable *plottable);

  QCPAbstractPlottable *plottable() { return mPlottable; }

protected:
  QCPAbstractPlottable *mPlottable;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual QSize minimumOuterSizeHint() const Q_DECL_OVERRIDE;

  QPen getIconBorderPen() const;
  QColor getTextColor() const;
  QFont getFont() const;
};

class QCP_LIB_DECL QCPLegend : public QCPLayoutGrid {
  Q_OBJECT

  Q_PROPERTY(QPen borderPen READ borderPen WRITE setBorderPen)
  Q_PROPERTY(QBrush brush READ brush WRITE setBrush)
  Q_PROPERTY(QFont font READ font WRITE setFont)
  Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
  Q_PROPERTY(QSize iconSize READ iconSize WRITE setIconSize)
  Q_PROPERTY(int iconTextPadding READ iconTextPadding WRITE setIconTextPadding)
  Q_PROPERTY(QPen iconBorderPen READ iconBorderPen WRITE setIconBorderPen)
  Q_PROPERTY(SelectableParts selectableParts READ selectableParts WRITE
                 setSelectableParts NOTIFY selectionChanged)
  Q_PROPERTY(SelectableParts selectedParts READ selectedParts WRITE
                 setSelectedParts NOTIFY selectableChanged)
  Q_PROPERTY(
      QPen selectedBorderPen READ selectedBorderPen WRITE setSelectedBorderPen)
  Q_PROPERTY(QPen selectedIconBorderPen READ selectedIconBorderPen WRITE
                 setSelectedIconBorderPen)
  Q_PROPERTY(QBrush selectedBrush READ selectedBrush WRITE setSelectedBrush)
  Q_PROPERTY(QFont selectedFont READ selectedFont WRITE setSelectedFont)
  Q_PROPERTY(QColor selectedTextColor READ selectedTextColor WRITE
                 setSelectedTextColor)

public:
  enum SelectablePart {
    spNone = 0x000

    ,
    spLegendBox = 0x001

    ,
    spItems = 0x002

  };
  Q_ENUMS(SelectablePart)
  Q_FLAGS(SelectableParts)
  Q_DECLARE_FLAGS(SelectableParts, SelectablePart)

  explicit QCPLegend();
  virtual ~QCPLegend();

  QPen borderPen() const { return mBorderPen; }
  QBrush brush() const { return mBrush; }
  QFont font() const { return mFont; }
  QColor textColor() const { return mTextColor; }
  QSize iconSize() const { return mIconSize; }
  int iconTextPadding() const { return mIconTextPadding; }
  QPen iconBorderPen() const { return mIconBorderPen; }
  SelectableParts selectableParts() const { return mSelectableParts; }
  SelectableParts selectedParts() const;
  QPen selectedBorderPen() const { return mSelectedBorderPen; }
  QPen selectedIconBorderPen() const { return mSelectedIconBorderPen; }
  QBrush selectedBrush() const { return mSelectedBrush; }
  QFont selectedFont() const { return mSelectedFont; }
  QColor selectedTextColor() const { return mSelectedTextColor; }

  void setBorderPen(const QPen &pen);
  void setBrush(const QBrush &brush);
  void setFont(const QFont &font);
  void setTextColor(const QColor &color);
  void setIconSize(const QSize &size);
  void setIconSize(int width, int height);
  void setIconTextPadding(int padding);
  void setIconBorderPen(const QPen &pen);
  Q_SLOT void setSelectableParts(const SelectableParts &selectableParts);
  Q_SLOT void setSelectedParts(const SelectableParts &selectedParts);
  void setSelectedBorderPen(const QPen &pen);
  void setSelectedIconBorderPen(const QPen &pen);
  void setSelectedBrush(const QBrush &brush);
  void setSelectedFont(const QFont &font);
  void setSelectedTextColor(const QColor &color);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;

  QCPAbstractLegendItem *item(int index) const;
  QCPPlottableLegendItem *
  itemWithPlottable(const QCPAbstractPlottable *plottable) const;
  int itemCount() const;
  bool hasItem(QCPAbstractLegendItem *item) const;
  bool hasItemWithPlottable(const QCPAbstractPlottable *plottable) const;
  bool addItem(QCPAbstractLegendItem *item);
  bool removeItem(int index);
  bool removeItem(QCPAbstractLegendItem *item);
  void clearItems();
  QList<QCPAbstractLegendItem *> selectedItems() const;

Q_SIGNALS:
  void selectionChanged(QCPLegend::SelectableParts parts);
  void selectableChanged(QCPLegend::SelectableParts parts);

protected:
  QPen mBorderPen, mIconBorderPen;
  QBrush mBrush;
  QFont mFont;
  QColor mTextColor;
  QSize mIconSize;
  int mIconTextPadding;
  SelectableParts mSelectedParts, mSelectableParts;
  QPen mSelectedBorderPen, mSelectedIconBorderPen;
  QBrush mSelectedBrush;
  QFont mSelectedFont;
  QColor mSelectedTextColor;

  virtual void parentPlotInitialized(QCustomPlot *parentPlot) Q_DECL_OVERRIDE;
  virtual QCP::Interaction selectionCategory() const Q_DECL_OVERRIDE;
  virtual void
  applyDefaultAntialiasingHint(QCPPainter *painter) const Q_DECL_OVERRIDE;
  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;

  virtual void selectEvent(QMouseEvent *event, bool additive,
                           const QVariant &details,
                           bool *selectionStateChanged) Q_DECL_OVERRIDE;
  virtual void deselectEvent(bool *selectionStateChanged) Q_DECL_OVERRIDE;

  QPen getBorderPen() const;
  QBrush getBrush() const;

private:
  Q_DISABLE_COPY(QCPLegend)

  friend class QCustomPlot;
  friend class QCPAbstractLegendItem;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QCPLegend::SelectableParts)
Q_DECLARE_METATYPE(QCPLegend::SelectablePart)

class QCP_LIB_DECL QCPTextElement : public QCPLayoutElement {
  Q_OBJECT

  Q_PROPERTY(QString text READ text WRITE setText)
  Q_PROPERTY(QFont font READ font WRITE setFont)
  Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
  Q_PROPERTY(QFont selectedFont READ selectedFont WRITE setSelectedFont)
  Q_PROPERTY(QColor selectedTextColor READ selectedTextColor WRITE
                 setSelectedTextColor)
  Q_PROPERTY(bool selectable READ selectable WRITE setSelectable NOTIFY
                 selectableChanged)
  Q_PROPERTY(
      bool selected READ selected WRITE setSelected NOTIFY selectionChanged)

public:
  explicit QCPTextElement(QCustomPlot *parentPlot);
  QCPTextElement(QCustomPlot *parentPlot, const QString &text);
  QCPTextElement(QCustomPlot *parentPlot, const QString &text,
                 double pointSize);
  QCPTextElement(QCustomPlot *parentPlot, const QString &text,
                 const QString &fontFamily, double pointSize);
  QCPTextElement(QCustomPlot *parentPlot, const QString &text,
                 const QFont &font);

  QString text() const { return mText; }
  int textFlags() const { return mTextFlags; }
  QFont font() const { return mFont; }
  QColor textColor() const { return mTextColor; }
  QFont selectedFont() const { return mSelectedFont; }
  QColor selectedTextColor() const { return mSelectedTextColor; }
  bool selectable() const { return mSelectable; }
  bool selected() const { return mSelected; }

  void setText(const QString &text);
  void setTextFlags(int flags);
  void setFont(const QFont &font);
  void setTextColor(const QColor &color);
  void setSelectedFont(const QFont &font);
  void setSelectedTextColor(const QColor &color);
  Q_SLOT void setSelectable(bool selectable);
  Q_SLOT void setSelected(bool selected);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;
  virtual void mousePressEvent(QMouseEvent *event,
                               const QVariant &details) Q_DECL_OVERRIDE;
  virtual void mouseReleaseEvent(QMouseEvent *event,
                                 const QPointF &startPos) Q_DECL_OVERRIDE;
  virtual void mouseDoubleClickEvent(QMouseEvent *event,
                                     const QVariant &details) Q_DECL_OVERRIDE;

Q_SIGNALS:
  void selectionChanged(bool selected);
  void selectableChanged(bool selectable);
  void clicked(QMouseEvent *event);
  void doubleClicked(QMouseEvent *event);

protected:
  QString mText;
  int mTextFlags;
  QFont mFont;
  QColor mTextColor;
  QFont mSelectedFont;
  QColor mSelectedTextColor;
  QRect mTextBoundingRect;
  bool mSelectable, mSelected;

  virtual void
  applyDefaultAntialiasingHint(QCPPainter *painter) const Q_DECL_OVERRIDE;
  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual QSize minimumOuterSizeHint() const Q_DECL_OVERRIDE;
  virtual QSize maximumOuterSizeHint() const Q_DECL_OVERRIDE;

  virtual void selectEvent(QMouseEvent *event, bool additive,
                           const QVariant &details,
                           bool *selectionStateChanged) Q_DECL_OVERRIDE;
  virtual void deselectEvent(bool *selectionStateChanged) Q_DECL_OVERRIDE;

  QFont mainFont() const;
  QColor mainTextColor() const;

private:
  Q_DISABLE_COPY(QCPTextElement)
};

class QCPColorScaleAxisRectPrivate : public QCPAxisRect {
  Q_OBJECT
public:
  explicit QCPColorScaleAxisRectPrivate(QCPColorScale *parentColorScale);

protected:
  QCPColorScale *mParentColorScale;
  QImage mGradientImage;
  bool mGradientImageInvalidated;

  using QCPAxisRect::calculateAutoMargin;
  using QCPAxisRect::mouseMoveEvent;
  using QCPAxisRect::mousePressEvent;
  using QCPAxisRect::mouseReleaseEvent;
  using QCPAxisRect::update;
  using QCPAxisRect::wheelEvent;
  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  void updateGradientImage();
  Q_SLOT void axisSelectionChanged(QCPAxis::SelectableParts selectedParts);
  Q_SLOT void axisSelectableChanged(QCPAxis::SelectableParts selectableParts);
  friend class QCPColorScale;
};

class QCP_LIB_DECL QCPColorScale : public QCPLayoutElement {
  Q_OBJECT

  Q_PROPERTY(QCPAxis::AxisType type READ type WRITE setType)
  Q_PROPERTY(QCPRange dataRange READ dataRange WRITE setDataRange NOTIFY
                 dataRangeChanged)
  Q_PROPERTY(QCPAxis::ScaleType dataScaleType READ dataScaleType WRITE
                 setDataScaleType NOTIFY dataScaleTypeChanged)
  Q_PROPERTY(QCPColorGradient gradient READ gradient WRITE setGradient NOTIFY
                 gradientChanged)
  Q_PROPERTY(QString label READ label WRITE setLabel)
  Q_PROPERTY(int barWidth READ barWidth WRITE setBarWidth)
  Q_PROPERTY(bool rangeDrag READ rangeDrag WRITE setRangeDrag)
  Q_PROPERTY(bool rangeZoom READ rangeZoom WRITE setRangeZoom)

public:
  explicit QCPColorScale(QCustomPlot *parentPlot);
  virtual ~QCPColorScale();

  QCPAxis *axis() const { return mColorAxis.data(); }
  QCPAxis::AxisType type() const { return mType; }
  QCPRange dataRange() const { return mDataRange; }
  QCPAxis::ScaleType dataScaleType() const { return mDataScaleType; }
  QCPColorGradient gradient() const { return mGradient; }
  QString label() const;
  int barWidth() const { return mBarWidth; }
  bool rangeDrag() const;
  bool rangeZoom() const;

  void setType(QCPAxis::AxisType type);
  Q_SLOT void setDataRange(const QCPRange &dataRange);
  Q_SLOT void setDataScaleType(QCPAxis::ScaleType scaleType);
  Q_SLOT void setGradient(const QCPColorGradient &gradient);
  void setLabel(const QString &str);
  void setBarWidth(int width);
  void setRangeDrag(bool enabled);
  void setRangeZoom(bool enabled);

  QList<QCPColorMap *> colorMaps() const;
  void rescaleDataRange(bool onlyVisibleMaps);

  virtual void update(UpdatePhase phase) Q_DECL_OVERRIDE;

Q_SIGNALS:
  void dataRangeChanged(const QCPRange &newRange);
  void dataScaleTypeChanged(QCPAxis::ScaleType scaleType);
  void gradientChanged(const QCPColorGradient &newGradient);

protected:
  QCPAxis::AxisType mType;
  QCPRange mDataRange;
  QCPAxis::ScaleType mDataScaleType;
  QCPColorGradient mGradient;
  int mBarWidth;

  QPointer<QCPColorScaleAxisRectPrivate> mAxisRect;
  QPointer<QCPAxis> mColorAxis;

  virtual void
  applyDefaultAntialiasingHint(QCPPainter *painter) const Q_DECL_OVERRIDE;

  virtual void mousePressEvent(QMouseEvent *event,
                               const QVariant &details) Q_DECL_OVERRIDE;
  virtual void mouseMoveEvent(QMouseEvent *event,
                              const QPointF &startPos) Q_DECL_OVERRIDE;
  virtual void mouseReleaseEvent(QMouseEvent *event,
                                 const QPointF &startPos) Q_DECL_OVERRIDE;
  virtual void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;

private:
  Q_DISABLE_COPY(QCPColorScale)

  friend class QCPColorScaleAxisRectPrivate;
};

class QCP_LIB_DECL QCPGraphData {
public:
  QCPGraphData();
  QCPGraphData(double key, double value);

  inline double sortKey() const { return key; }
  inline static QCPGraphData fromSortKey(double sortKey) {
    return QCPGraphData(sortKey, 0);
  }
  inline static bool sortKeyIsMainKey() { return true; }

  inline double mainKey() const { return key; }
  inline double mainValue() const { return value; }

  inline QCPRange valueRange() const { return QCPRange(value, value); }

  double key, value;
};
Q_DECLARE_TYPEINFO(QCPGraphData, Q_PRIMITIVE_TYPE);

typedef QCPDataContainer<QCPGraphData> QCPGraphDataContainer;

class QCP_LIB_DECL QCPGraph : public QCPAbstractPlottable1D<QCPGraphData> {
  Q_OBJECT

  Q_PROPERTY(LineStyle lineStyle READ lineStyle WRITE setLineStyle)
  Q_PROPERTY(
      QCPScatterStyle scatterStyle READ scatterStyle WRITE setScatterStyle)
  Q_PROPERTY(int scatterSkip READ scatterSkip WRITE setScatterSkip)
  Q_PROPERTY(QCPGraph *channelFillGraph READ channelFillGraph WRITE
                 setChannelFillGraph)
  Q_PROPERTY(
      bool adaptiveSampling READ adaptiveSampling WRITE setAdaptiveSampling)

public:
  enum LineStyle {
    lsNone

    ,
    lsLine

    ,
    lsStepLeft

    ,
    lsStepRight

    ,
    lsStepCenter

    ,
    lsImpulse

  };
  Q_ENUMS(LineStyle)

  explicit QCPGraph(QCPAxis *keyAxis, QCPAxis *valueAxis);
  virtual ~QCPGraph();

  QSharedPointer<QCPGraphDataContainer> data() const { return mDataContainer; }
  LineStyle lineStyle() const { return mLineStyle; }
  QCPScatterStyle scatterStyle() const { return mScatterStyle; }
  int scatterSkip() const { return mScatterSkip; }
  QCPGraph *channelFillGraph() const { return mChannelFillGraph.data(); }
  bool adaptiveSampling() const { return mAdaptiveSampling; }

  void setData(QSharedPointer<QCPGraphDataContainer> data);
  void setData(const QVector<double> &keys, const QVector<double> &values,
               bool alreadySorted = false);
  void setLineStyle(LineStyle ls);
  void setScatterStyle(const QCPScatterStyle &style);
  void setScatterSkip(int skip);
  void setChannelFillGraph(QCPGraph *targetGraph);
  void setAdaptiveSampling(bool enabled);

  void addData(const QVector<double> &keys, const QVector<double> &values,
               bool alreadySorted = false);
  void addData(double key, double value);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;
  virtual QCPRange
  getKeyRange(bool &foundRange,
              QCP::SignDomain inSignDomain = QCP::sdBoth) const Q_DECL_OVERRIDE;
  virtual QCPRange
  getValueRange(bool &foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                const QCPRange &inKeyRange = QCPRange()) const Q_DECL_OVERRIDE;

protected:
  LineStyle mLineStyle;
  QCPScatterStyle mScatterStyle;
  int mScatterSkip;
  QPointer<QCPGraph> mChannelFillGraph;
  bool mAdaptiveSampling;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual void drawLegendIcon(QCPPainter *painter,
                              const QRectF &rect) const Q_DECL_OVERRIDE;

  virtual void drawFill(QCPPainter *painter, QVector<QPointF> *lines) const;
  virtual void drawScatterPlot(QCPPainter *painter,
                               const QVector<QPointF> &scatters,
                               const QCPScatterStyle &style) const;
  virtual void drawLinePlot(QCPPainter *painter,
                            const QVector<QPointF> &lines) const;
  virtual void drawImpulsePlot(QCPPainter *painter,
                               const QVector<QPointF> &lines) const;

  virtual void
  getOptimizedLineData(QVector<QCPGraphData> *lineData,
                       const QCPGraphDataContainer::const_iterator &begin,
                       const QCPGraphDataContainer::const_iterator &end) const;
  virtual void
  getOptimizedScatterData(QVector<QCPGraphData> *scatterData,
                          QCPGraphDataContainer::const_iterator begin,
                          QCPGraphDataContainer::const_iterator end) const;

  void getVisibleDataBounds(QCPGraphDataContainer::const_iterator &begin,
                            QCPGraphDataContainer::const_iterator &end,
                            const QCPDataRange &rangeRestriction) const;
  void getLines(QVector<QPointF> *lines, const QCPDataRange &dataRange) const;
  void getScatters(QVector<QPointF> *scatters,
                   const QCPDataRange &dataRange) const;
  QVector<QPointF> dataToLines(const QVector<QCPGraphData> &data) const;
  QVector<QPointF> dataToStepLeftLines(const QVector<QCPGraphData> &data) const;
  QVector<QPointF>
  dataToStepRightLines(const QVector<QCPGraphData> &data) const;
  QVector<QPointF>
  dataToStepCenterLines(const QVector<QCPGraphData> &data) const;
  QVector<QPointF> dataToImpulseLines(const QVector<QCPGraphData> &data) const;
  QVector<QCPDataRange> getNonNanSegments(const QVector<QPointF> *lineData,
                                          Qt::Orientation keyOrientation) const;
  QVector<QPair<QCPDataRange, QCPDataRange>>
  getOverlappingSegments(QVector<QCPDataRange> thisSegments,
                         const QVector<QPointF> *thisData,
                         QVector<QCPDataRange> otherSegments,
                         const QVector<QPointF> *otherData) const;
  bool segmentsIntersect(double aLower, double aUpper, double bLower,
                         double bUpper, int &bPrecedence) const;
  QPointF getFillBasePoint(QPointF matchingDataPoint) const;
  const QPolygonF getFillPolygon(const QVector<QPointF> *lineData,
                                 QCPDataRange segment) const;
  const QPolygonF getChannelFillPolygon(const QVector<QPointF> *lineData,
                                        QCPDataRange thisSegment,
                                        const QVector<QPointF> *otherData,
                                        QCPDataRange otherSegment) const;
  int findIndexBelowX(const QVector<QPointF> *data, double x) const;
  int findIndexAboveX(const QVector<QPointF> *data, double x) const;
  int findIndexBelowY(const QVector<QPointF> *data, double y) const;
  int findIndexAboveY(const QVector<QPointF> *data, double y) const;
  double
  pointDistance(const QPointF &pixelPoint,
                QCPGraphDataContainer::const_iterator &closestData) const;

  friend class QCustomPlot;
  friend class QCPLegend;
};
Q_DECLARE_METATYPE(QCPGraph::LineStyle)

class QCP_LIB_DECL QCPCurveData {
public:
  QCPCurveData();
  QCPCurveData(double t, double key, double value);

  inline double sortKey() const { return t; }
  inline static QCPCurveData fromSortKey(double sortKey) {
    return QCPCurveData(sortKey, 0, 0);
  }
  inline static bool sortKeyIsMainKey() { return false; }

  inline double mainKey() const { return key; }
  inline double mainValue() const { return value; }

  inline QCPRange valueRange() const { return QCPRange(value, value); }

  double t, key, value;
};
Q_DECLARE_TYPEINFO(QCPCurveData, Q_PRIMITIVE_TYPE);

typedef QCPDataContainer<QCPCurveData> QCPCurveDataContainer;

class QCP_LIB_DECL QCPCurve : public QCPAbstractPlottable1D<QCPCurveData> {
  Q_OBJECT

  Q_PROPERTY(
      QCPScatterStyle scatterStyle READ scatterStyle WRITE setScatterStyle)
  Q_PROPERTY(int scatterSkip READ scatterSkip WRITE setScatterSkip)
  Q_PROPERTY(LineStyle lineStyle READ lineStyle WRITE setLineStyle)

public:
  enum LineStyle {
    lsNone

    ,
    lsLine

  };
  Q_ENUMS(LineStyle)

  explicit QCPCurve(QCPAxis *keyAxis, QCPAxis *valueAxis);
  virtual ~QCPCurve();

  QSharedPointer<QCPCurveDataContainer> data() const { return mDataContainer; }
  QCPScatterStyle scatterStyle() const { return mScatterStyle; }
  int scatterSkip() const { return mScatterSkip; }
  LineStyle lineStyle() const { return mLineStyle; }

  void setData(QSharedPointer<QCPCurveDataContainer> data);
  void setData(const QVector<double> &t, const QVector<double> &keys,
               const QVector<double> &values, bool alreadySorted = false);
  void setData(const QVector<double> &keys, const QVector<double> &values);
  void setScatterStyle(const QCPScatterStyle &style);
  void setScatterSkip(int skip);
  void setLineStyle(LineStyle style);

  void addData(const QVector<double> &t, const QVector<double> &keys,
               const QVector<double> &values, bool alreadySorted = false);
  void addData(const QVector<double> &keys, const QVector<double> &values);
  void addData(double t, double key, double value);
  void addData(double key, double value);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;
  virtual QCPRange
  getKeyRange(bool &foundRange,
              QCP::SignDomain inSignDomain = QCP::sdBoth) const Q_DECL_OVERRIDE;
  virtual QCPRange
  getValueRange(bool &foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                const QCPRange &inKeyRange = QCPRange()) const Q_DECL_OVERRIDE;

protected:
  QCPScatterStyle mScatterStyle;
  int mScatterSkip;
  LineStyle mLineStyle;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual void drawLegendIcon(QCPPainter *painter,
                              const QRectF &rect) const Q_DECL_OVERRIDE;

  virtual void drawCurveLine(QCPPainter *painter,
                             const QVector<QPointF> &lines) const;
  virtual void drawScatterPlot(QCPPainter *painter,
                               const QVector<QPointF> &points,
                               const QCPScatterStyle &style) const;

  void getCurveLines(QVector<QPointF> *lines, const QCPDataRange &dataRange,
                     double penWidth) const;
  void getScatters(QVector<QPointF> *scatters, const QCPDataRange &dataRange,
                   double scatterWidth) const;
  int getRegion(double key, double value, double keyMin, double valueMax,
                double keyMax, double valueMin) const;
  QPointF getOptimizedPoint(int prevRegion, double prevKey, double prevValue,
                            double key, double value, double keyMin,
                            double valueMax, double keyMax,
                            double valueMin) const;
  QVector<QPointF> getOptimizedCornerPoints(int prevRegion, int currentRegion,
                                            double prevKey, double prevValue,
                                            double key, double value,
                                            double keyMin, double valueMax,
                                            double keyMax,
                                            double valueMin) const;
  bool mayTraverse(int prevRegion, int currentRegion) const;
  bool getTraverse(double prevKey, double prevValue, double key, double value,
                   double keyMin, double valueMax, double keyMax,
                   double valueMin, QPointF &crossA, QPointF &crossB) const;
  void getTraverseCornerPoints(int prevRegion, int currentRegion, double keyMin,
                               double valueMax, double keyMax, double valueMin,
                               QVector<QPointF> &beforeTraverse,
                               QVector<QPointF> &afterTraverse) const;
  double
  pointDistance(const QPointF &pixelPoint,
                QCPCurveDataContainer::const_iterator &closestData) const;

  friend class QCustomPlot;
  friend class QCPLegend;
};
Q_DECLARE_METATYPE(QCPCurve::LineStyle)

class QCP_LIB_DECL QCPBarsGroup : public QObject {
  Q_OBJECT

  Q_PROPERTY(SpacingType spacingType READ spacingType WRITE setSpacingType)
  Q_PROPERTY(double spacing READ spacing WRITE setSpacing)

public:
  enum SpacingType {
    stAbsolute

    ,
    stAxisRectRatio

    ,
    stPlotCoords

  };
  Q_ENUMS(SpacingType)

  explicit QCPBarsGroup(QCustomPlot *parentPlot);
  virtual ~QCPBarsGroup();

  SpacingType spacingType() const { return mSpacingType; }
  double spacing() const { return mSpacing; }

  void setSpacingType(SpacingType spacingType);
  void setSpacing(double spacing);

  QList<QCPBars *> bars() const { return mBars; }
  QCPBars *bars(int index) const;
  int size() const { return mBars.size(); }
  bool isEmpty() const { return mBars.isEmpty(); }
  void clear();
  bool contains(QCPBars *bars) const { return mBars.contains(bars); }
  void append(QCPBars *bars);
  void insert(int i, QCPBars *bars);
  void remove(QCPBars *bars);

protected:
  QCustomPlot *mParentPlot;
  SpacingType mSpacingType;
  double mSpacing;
  QList<QCPBars *> mBars;

  void registerBars(QCPBars *bars);
  void unregisterBars(QCPBars *bars);

  double keyPixelOffset(const QCPBars *bars, double keyCoord);
  double getPixelSpacing(const QCPBars *bars, double keyCoord);

private:
  Q_DISABLE_COPY(QCPBarsGroup)

  friend class QCPBars;
};
Q_DECLARE_METATYPE(QCPBarsGroup::SpacingType)

class QCP_LIB_DECL QCPBarsData {
public:
  QCPBarsData();
  QCPBarsData(double key, double value);

  inline double sortKey() const { return key; }
  inline static QCPBarsData fromSortKey(double sortKey) {
    return QCPBarsData(sortKey, 0);
  }
  inline static bool sortKeyIsMainKey() { return true; }

  inline double mainKey() const { return key; }
  inline double mainValue() const { return value; }

  inline QCPRange valueRange() const { return QCPRange(value, value); }

  double key, value;
};
Q_DECLARE_TYPEINFO(QCPBarsData, Q_PRIMITIVE_TYPE);

typedef QCPDataContainer<QCPBarsData> QCPBarsDataContainer;

class QCP_LIB_DECL QCPBars : public QCPAbstractPlottable1D<QCPBarsData> {
  Q_OBJECT

  Q_PROPERTY(double width READ width WRITE setWidth)
  Q_PROPERTY(WidthType widthType READ widthType WRITE setWidthType)
  Q_PROPERTY(QCPBarsGroup *barsGroup READ barsGroup WRITE setBarsGroup)
  Q_PROPERTY(double baseValue READ baseValue WRITE setBaseValue)
  Q_PROPERTY(double stackingGap READ stackingGap WRITE setStackingGap)
  Q_PROPERTY(QCPBars *barBelow READ barBelow)
  Q_PROPERTY(QCPBars *barAbove READ barAbove)

public:
  enum WidthType {
    wtAbsolute

    ,
    wtAxisRectRatio

    ,
    wtPlotCoords

  };
  Q_ENUMS(WidthType)

  explicit QCPBars(QCPAxis *keyAxis, QCPAxis *valueAxis);
  virtual ~QCPBars();

  double width() const { return mWidth; }
  WidthType widthType() const { return mWidthType; }
  QCPBarsGroup *barsGroup() const { return mBarsGroup; }
  double baseValue() const { return mBaseValue; }
  double stackingGap() const { return mStackingGap; }
  QCPBars *barBelow() const { return mBarBelow.data(); }
  QCPBars *barAbove() const { return mBarAbove.data(); }
  QSharedPointer<QCPBarsDataContainer> data() const { return mDataContainer; }

  void setData(QSharedPointer<QCPBarsDataContainer> data);
  void setData(const QVector<double> &keys, const QVector<double> &values,
               bool alreadySorted = false);
  void setWidth(double width);
  void setWidthType(WidthType widthType);
  void setBarsGroup(QCPBarsGroup *barsGroup);
  void setBaseValue(double baseValue);
  void setStackingGap(double pixels);

  void addData(const QVector<double> &keys, const QVector<double> &values,
               bool alreadySorted = false);
  void addData(double key, double value);
  void moveBelow(QCPBars *bars);
  void moveAbove(QCPBars *bars);

  virtual QCPDataSelection
  selectTestRect(const QRectF &rect, bool onlySelectable) const Q_DECL_OVERRIDE;
  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;
  virtual QCPRange
  getKeyRange(bool &foundRange,
              QCP::SignDomain inSignDomain = QCP::sdBoth) const Q_DECL_OVERRIDE;
  virtual QCPRange
  getValueRange(bool &foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                const QCPRange &inKeyRange = QCPRange()) const Q_DECL_OVERRIDE;
  virtual QPointF dataPixelPosition(int index) const Q_DECL_OVERRIDE;

protected:
  double mWidth;
  WidthType mWidthType;
  QCPBarsGroup *mBarsGroup;
  double mBaseValue;
  double mStackingGap;
  QPointer<QCPBars> mBarBelow, mBarAbove;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual void drawLegendIcon(QCPPainter *painter,
                              const QRectF &rect) const Q_DECL_OVERRIDE;

  void getVisibleDataBounds(QCPBarsDataContainer::const_iterator &begin,
                            QCPBarsDataContainer::const_iterator &end) const;
  QRectF getBarRect(double key, double value) const;
  void getPixelWidth(double key, double &lower, double &upper) const;
  double getStackedBaseValue(double key, bool positive) const;
  static void connectBars(QCPBars *lower, QCPBars *upper);

  friend class QCustomPlot;
  friend class QCPLegend;
  friend class QCPBarsGroup;
};
Q_DECLARE_METATYPE(QCPBars::WidthType)

class QCP_LIB_DECL QCPStatisticalBoxData {
public:
  QCPStatisticalBoxData();
  QCPStatisticalBoxData(double key, double minimum, double lowerQuartile,
                        double median, double upperQuartile, double maximum,
                        const QVector<double> &outliers = QVector<double>());

  inline double sortKey() const { return key; }
  inline static QCPStatisticalBoxData fromSortKey(double sortKey) {
    return QCPStatisticalBoxData(sortKey, 0, 0, 0, 0, 0);
  }
  inline static bool sortKeyIsMainKey() { return true; }

  inline double mainKey() const { return key; }
  inline double mainValue() const { return median; }

  inline QCPRange valueRange() const {
    QCPRange result(minimum, maximum);
    for (QVector<double>::const_iterator it = outliers.constBegin();
         it != outliers.constEnd(); ++it)
      result.expand(*it);
    return result;
  }

  double key, minimum, lowerQuartile, median, upperQuartile, maximum;
  QVector<double> outliers;
};
Q_DECLARE_TYPEINFO(QCPStatisticalBoxData, Q_MOVABLE_TYPE);

typedef QCPDataContainer<QCPStatisticalBoxData> QCPStatisticalBoxDataContainer;

class QCP_LIB_DECL QCPStatisticalBox
    : public QCPAbstractPlottable1D<QCPStatisticalBoxData> {
  Q_OBJECT

  Q_PROPERTY(double width READ width WRITE setWidth)
  Q_PROPERTY(double whiskerWidth READ whiskerWidth WRITE setWhiskerWidth)
  Q_PROPERTY(QPen whiskerPen READ whiskerPen WRITE setWhiskerPen)
  Q_PROPERTY(QPen whiskerBarPen READ whiskerBarPen WRITE setWhiskerBarPen)
  Q_PROPERTY(bool whiskerAntialiased READ whiskerAntialiased WRITE
                 setWhiskerAntialiased)
  Q_PROPERTY(QPen medianPen READ medianPen WRITE setMedianPen)
  Q_PROPERTY(
      QCPScatterStyle outlierStyle READ outlierStyle WRITE setOutlierStyle)

public:
  explicit QCPStatisticalBox(QCPAxis *keyAxis, QCPAxis *valueAxis);

  QSharedPointer<QCPStatisticalBoxDataContainer> data() const {
    return mDataContainer;
  }
  double width() const { return mWidth; }
  double whiskerWidth() const { return mWhiskerWidth; }
  QPen whiskerPen() const { return mWhiskerPen; }
  QPen whiskerBarPen() const { return mWhiskerBarPen; }
  bool whiskerAntialiased() const { return mWhiskerAntialiased; }
  QPen medianPen() const { return mMedianPen; }
  QCPScatterStyle outlierStyle() const { return mOutlierStyle; }

  void setData(QSharedPointer<QCPStatisticalBoxDataContainer> data);
  void setData(const QVector<double> &keys, const QVector<double> &minimum,
               const QVector<double> &lowerQuartile,
               const QVector<double> &median,
               const QVector<double> &upperQuartile,
               const QVector<double> &maximum, bool alreadySorted = false);
  void setWidth(double width);
  void setWhiskerWidth(double width);
  void setWhiskerPen(const QPen &pen);
  void setWhiskerBarPen(const QPen &pen);
  void setWhiskerAntialiased(bool enabled);
  void setMedianPen(const QPen &pen);
  void setOutlierStyle(const QCPScatterStyle &style);

  void addData(const QVector<double> &keys, const QVector<double> &minimum,
               const QVector<double> &lowerQuartile,
               const QVector<double> &median,
               const QVector<double> &upperQuartile,
               const QVector<double> &maximum, bool alreadySorted = false);
  void addData(double key, double minimum, double lowerQuartile, double median,
               double upperQuartile, double maximum,
               const QVector<double> &outliers = QVector<double>());

  virtual QCPDataSelection
  selectTestRect(const QRectF &rect, bool onlySelectable) const Q_DECL_OVERRIDE;
  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;
  virtual QCPRange
  getKeyRange(bool &foundRange,
              QCP::SignDomain inSignDomain = QCP::sdBoth) const Q_DECL_OVERRIDE;
  virtual QCPRange
  getValueRange(bool &foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                const QCPRange &inKeyRange = QCPRange()) const Q_DECL_OVERRIDE;

protected:
  double mWidth;
  double mWhiskerWidth;
  QPen mWhiskerPen, mWhiskerBarPen;
  bool mWhiskerAntialiased;
  QPen mMedianPen;
  QCPScatterStyle mOutlierStyle;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual void drawLegendIcon(QCPPainter *painter,
                              const QRectF &rect) const Q_DECL_OVERRIDE;

  virtual void
  drawStatisticalBox(QCPPainter *painter,
                     QCPStatisticalBoxDataContainer::const_iterator it,
                     const QCPScatterStyle &outlierStyle) const;

  void getVisibleDataBounds(
      QCPStatisticalBoxDataContainer::const_iterator &begin,
      QCPStatisticalBoxDataContainer::const_iterator &end) const;
  QRectF
  getQuartileBox(QCPStatisticalBoxDataContainer::const_iterator it) const;
  QVector<QLineF> getWhiskerBackboneLines(
      QCPStatisticalBoxDataContainer::const_iterator it) const;
  QVector<QLineF>
  getWhiskerBarLines(QCPStatisticalBoxDataContainer::const_iterator it) const;

  friend class QCustomPlot;
  friend class QCPLegend;
};

class QCP_LIB_DECL QCPColorMapData {
public:
  QCPColorMapData(int keySize, int valueSize, const QCPRange &keyRange,
                  const QCPRange &valueRange);
  ~QCPColorMapData();
  QCPColorMapData(const QCPColorMapData &other);
  QCPColorMapData &operator=(const QCPColorMapData &other);

  int keySize() const { return mKeySize; }
  int valueSize() const { return mValueSize; }
  QCPRange keyRange() const { return mKeyRange; }
  QCPRange valueRange() const { return mValueRange; }
  QCPRange dataBounds() const { return mDataBounds; }
  double data(double key, double value);
  double cell(int keyIndex, int valueIndex);
  unsigned char alpha(int keyIndex, int valueIndex);

  void setSize(int keySize, int valueSize);
  void setKeySize(int keySize);
  void setValueSize(int valueSize);
  void setRange(const QCPRange &keyRange, const QCPRange &valueRange);
  void setKeyRange(const QCPRange &keyRange);
  void setValueRange(const QCPRange &valueRange);
  void setData(double key, double value, double z);
  void setCell(int keyIndex, int valueIndex, double z);
  void setAlpha(int keyIndex, int valueIndex, unsigned char alpha);

  void recalculateDataBounds();
  void clear();
  void clearAlpha();
  void fill(double z);
  void fillAlpha(unsigned char alpha);
  bool isEmpty() const { return mIsEmpty; }
  void coordToCell(double key, double value, int *keyIndex,
                   int *valueIndex) const;
  void cellToCoord(int keyIndex, int valueIndex, double *key,
                   double *value) const;

protected:
  int mKeySize, mValueSize;
  QCPRange mKeyRange, mValueRange;
  bool mIsEmpty;

  double *mData;
  unsigned char *mAlpha;
  QCPRange mDataBounds;
  bool mDataModified;

  bool createAlpha(bool initializeOpaque = true);

  friend class QCPColorMap;
};

class QCP_LIB_DECL QCPColorMap : public QCPAbstractPlottable {
  Q_OBJECT

  Q_PROPERTY(QCPRange dataRange READ dataRange WRITE setDataRange NOTIFY
                 dataRangeChanged)
  Q_PROPERTY(QCPAxis::ScaleType dataScaleType READ dataScaleType WRITE
                 setDataScaleType NOTIFY dataScaleTypeChanged)
  Q_PROPERTY(QCPColorGradient gradient READ gradient WRITE setGradient NOTIFY
                 gradientChanged)
  Q_PROPERTY(bool interpolate READ interpolate WRITE setInterpolate)
  Q_PROPERTY(bool tightBoundary READ tightBoundary WRITE setTightBoundary)
  Q_PROPERTY(QCPColorScale *colorScale READ colorScale WRITE setColorScale)

public:
  explicit QCPColorMap(QCPAxis *keyAxis, QCPAxis *valueAxis);
  virtual ~QCPColorMap();

  QCPColorMapData *data() const { return mMapData; }
  QCPRange dataRange() const { return mDataRange; }
  QCPAxis::ScaleType dataScaleType() const { return mDataScaleType; }
  bool interpolate() const { return mInterpolate; }
  bool tightBoundary() const { return mTightBoundary; }
  QCPColorGradient gradient() const { return mGradient; }
  QCPColorScale *colorScale() const { return mColorScale.data(); }

  void setData(QCPColorMapData *data, bool copy = false);
  Q_SLOT void setDataRange(const QCPRange &dataRange);
  Q_SLOT void setDataScaleType(QCPAxis::ScaleType scaleType);
  Q_SLOT void setGradient(const QCPColorGradient &gradient);
  void setInterpolate(bool enabled);
  void setTightBoundary(bool enabled);
  void setColorScale(QCPColorScale *colorScale);

  void rescaleDataRange(bool recalculateDataBounds = false);
  Q_SLOT void updateLegendIcon(
      Qt::TransformationMode transformMode = Qt::SmoothTransformation,
      const QSize &thumbSize = QSize(32, 18));

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;
  virtual QCPRange
  getKeyRange(bool &foundRange,
              QCP::SignDomain inSignDomain = QCP::sdBoth) const Q_DECL_OVERRIDE;
  virtual QCPRange
  getValueRange(bool &foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                const QCPRange &inKeyRange = QCPRange()) const Q_DECL_OVERRIDE;

Q_SIGNALS:
  void dataRangeChanged(const QCPRange &newRange);
  void dataScaleTypeChanged(QCPAxis::ScaleType scaleType);
  void gradientChanged(const QCPColorGradient &newGradient);

protected:
  QCPRange mDataRange;
  QCPAxis::ScaleType mDataScaleType;
  QCPColorMapData *mMapData;
  QCPColorGradient mGradient;
  bool mInterpolate;
  bool mTightBoundary;
  QPointer<QCPColorScale> mColorScale;

  QImage mMapImage, mUndersampledMapImage;
  QPixmap mLegendIcon;
  bool mMapImageInvalidated;

  virtual void updateMapImage();

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual void drawLegendIcon(QCPPainter *painter,
                              const QRectF &rect) const Q_DECL_OVERRIDE;

  friend class QCustomPlot;
  friend class QCPLegend;
};

class QCP_LIB_DECL QCPFinancialData {
public:
  QCPFinancialData();
  QCPFinancialData(double key, double open, double high, double low,
                   double close);

  inline double sortKey() const { return key; }
  inline static QCPFinancialData fromSortKey(double sortKey) {
    return QCPFinancialData(sortKey, 0, 0, 0, 0);
  }
  inline static bool sortKeyIsMainKey() { return true; }

  inline double mainKey() const { return key; }
  inline double mainValue() const { return open; }

  inline QCPRange valueRange() const { return QCPRange(low, high); }

  double key, open, high, low, close;
};
Q_DECLARE_TYPEINFO(QCPFinancialData, Q_PRIMITIVE_TYPE);

typedef QCPDataContainer<QCPFinancialData> QCPFinancialDataContainer;

class QCP_LIB_DECL QCPFinancial
    : public QCPAbstractPlottable1D<QCPFinancialData> {
  Q_OBJECT

  Q_PROPERTY(ChartStyle chartStyle READ chartStyle WRITE setChartStyle)
  Q_PROPERTY(double width READ width WRITE setWidth)
  Q_PROPERTY(WidthType widthType READ widthType WRITE setWidthType)
  Q_PROPERTY(bool twoColored READ twoColored WRITE setTwoColored)
  Q_PROPERTY(QBrush brushPositive READ brushPositive WRITE setBrushPositive)
  Q_PROPERTY(QBrush brushNegative READ brushNegative WRITE setBrushNegative)
  Q_PROPERTY(QPen penPositive READ penPositive WRITE setPenPositive)
  Q_PROPERTY(QPen penNegative READ penNegative WRITE setPenNegative)

public:
  enum WidthType {
    wtAbsolute

    ,
    wtAxisRectRatio

    ,
    wtPlotCoords

  };
  Q_ENUMS(WidthType)

  enum ChartStyle {
    csOhlc

    ,
    csCandlestick

  };
  Q_ENUMS(ChartStyle)

  explicit QCPFinancial(QCPAxis *keyAxis, QCPAxis *valueAxis);
  virtual ~QCPFinancial();

  QSharedPointer<QCPFinancialDataContainer> data() const {
    return mDataContainer;
  }
  ChartStyle chartStyle() const { return mChartStyle; }
  double width() const { return mWidth; }
  WidthType widthType() const { return mWidthType; }
  bool twoColored() const { return mTwoColored; }
  QBrush brushPositive() const { return mBrushPositive; }
  QBrush brushNegative() const { return mBrushNegative; }
  QPen penPositive() const { return mPenPositive; }
  QPen penNegative() const { return mPenNegative; }

  void setData(QSharedPointer<QCPFinancialDataContainer> data);
  void setData(const QVector<double> &keys, const QVector<double> &open,
               const QVector<double> &high, const QVector<double> &low,
               const QVector<double> &close, bool alreadySorted = false);
  void setChartStyle(ChartStyle style);
  void setWidth(double width);
  void setWidthType(WidthType widthType);
  void setTwoColored(bool twoColored);
  void setBrushPositive(const QBrush &brush);
  void setBrushNegative(const QBrush &brush);
  void setPenPositive(const QPen &pen);
  void setPenNegative(const QPen &pen);

  void addData(const QVector<double> &keys, const QVector<double> &open,
               const QVector<double> &high, const QVector<double> &low,
               const QVector<double> &close, bool alreadySorted = false);
  void addData(double key, double open, double high, double low, double close);

  virtual QCPDataSelection
  selectTestRect(const QRectF &rect, bool onlySelectable) const Q_DECL_OVERRIDE;
  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;
  virtual QCPRange
  getKeyRange(bool &foundRange,
              QCP::SignDomain inSignDomain = QCP::sdBoth) const Q_DECL_OVERRIDE;
  virtual QCPRange
  getValueRange(bool &foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                const QCPRange &inKeyRange = QCPRange()) const Q_DECL_OVERRIDE;

  static QCPFinancialDataContainer
  timeSeriesToOhlc(const QVector<double> &time, const QVector<double> &value,
                   double timeBinSize, double timeBinOffset = 0);

protected:
  ChartStyle mChartStyle;
  double mWidth;
  WidthType mWidthType;
  bool mTwoColored;
  QBrush mBrushPositive, mBrushNegative;
  QPen mPenPositive, mPenNegative;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual void drawLegendIcon(QCPPainter *painter,
                              const QRectF &rect) const Q_DECL_OVERRIDE;

  void drawOhlcPlot(QCPPainter *painter,
                    const QCPFinancialDataContainer::const_iterator &begin,
                    const QCPFinancialDataContainer::const_iterator &end,
                    bool isSelected);
  void
  drawCandlestickPlot(QCPPainter *painter,
                      const QCPFinancialDataContainer::const_iterator &begin,
                      const QCPFinancialDataContainer::const_iterator &end,
                      bool isSelected);
  double getPixelWidth(double key, double keyPixel) const;
  double ohlcSelectTest(
      const QPointF &pos,
      const QCPFinancialDataContainer::const_iterator &begin,
      const QCPFinancialDataContainer::const_iterator &end,
      QCPFinancialDataContainer::const_iterator &closestDataPoint) const;
  double candlestickSelectTest(
      const QPointF &pos,
      const QCPFinancialDataContainer::const_iterator &begin,
      const QCPFinancialDataContainer::const_iterator &end,
      QCPFinancialDataContainer::const_iterator &closestDataPoint) const;
  void
  getVisibleDataBounds(QCPFinancialDataContainer::const_iterator &begin,
                       QCPFinancialDataContainer::const_iterator &end) const;
  QRectF selectionHitBox(QCPFinancialDataContainer::const_iterator it) const;

  friend class QCustomPlot;
  friend class QCPLegend;
};
Q_DECLARE_METATYPE(QCPFinancial::ChartStyle)

class QCP_LIB_DECL QCPErrorBarsData {
public:
  QCPErrorBarsData();
  explicit QCPErrorBarsData(double error);
  QCPErrorBarsData(double errorMinus, double errorPlus);

  double errorMinus, errorPlus;
};
Q_DECLARE_TYPEINFO(QCPErrorBarsData, Q_PRIMITIVE_TYPE);

typedef QVector<QCPErrorBarsData> QCPErrorBarsDataContainer;

class QCP_LIB_DECL QCPErrorBars : public QCPAbstractPlottable,
                                  public QCPPlottableInterface1D {
  Q_OBJECT

  Q_PROPERTY(
      QSharedPointer<QCPErrorBarsDataContainer> data READ data WRITE setData)
  Q_PROPERTY(QCPAbstractPlottable *dataPlottable READ dataPlottable WRITE
                 setDataPlottable)
  Q_PROPERTY(ErrorType errorType READ errorType WRITE setErrorType)
  Q_PROPERTY(double whiskerWidth READ whiskerWidth WRITE setWhiskerWidth)
  Q_PROPERTY(double symbolGap READ symbolGap WRITE setSymbolGap)

public:
  enum ErrorType {
    etKeyError

    ,
    etValueError

  };
  Q_ENUMS(ErrorType)

  explicit QCPErrorBars(QCPAxis *keyAxis, QCPAxis *valueAxis);
  virtual ~QCPErrorBars();

  QSharedPointer<QCPErrorBarsDataContainer> data() const {
    return mDataContainer;
  }
  QCPAbstractPlottable *dataPlottable() const { return mDataPlottable.data(); }
  ErrorType errorType() const { return mErrorType; }
  double whiskerWidth() const { return mWhiskerWidth; }
  double symbolGap() const { return mSymbolGap; }

  void setData(QSharedPointer<QCPErrorBarsDataContainer> data);
  void setData(const QVector<double> &error);
  void setData(const QVector<double> &errorMinus,
               const QVector<double> &errorPlus);
  void setDataPlottable(QCPAbstractPlottable *plottable);
  void setErrorType(ErrorType type);
  void setWhiskerWidth(double pixels);
  void setSymbolGap(double pixels);

  void addData(const QVector<double> &error);
  void addData(const QVector<double> &errorMinus,
               const QVector<double> &errorPlus);
  void addData(double error);
  void addData(double errorMinus, double errorPlus);

  virtual int dataCount() const Q_DECL_OVERRIDE;
  virtual double dataMainKey(int index) const Q_DECL_OVERRIDE;
  virtual double dataSortKey(int index) const Q_DECL_OVERRIDE;
  virtual double dataMainValue(int index) const Q_DECL_OVERRIDE;
  virtual QCPRange dataValueRange(int index) const Q_DECL_OVERRIDE;
  virtual QPointF dataPixelPosition(int index) const Q_DECL_OVERRIDE;
  virtual bool sortKeyIsMainKey() const Q_DECL_OVERRIDE;
  virtual QCPDataSelection
  selectTestRect(const QRectF &rect, bool onlySelectable) const Q_DECL_OVERRIDE;
  virtual int findBegin(double sortKey,
                        bool expandedRange = true) const Q_DECL_OVERRIDE;
  virtual int findEnd(double sortKey,
                      bool expandedRange = true) const Q_DECL_OVERRIDE;

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;
  virtual QCPPlottableInterface1D *interface1D() Q_DECL_OVERRIDE {
    return this;
  }

protected:
  QSharedPointer<QCPErrorBarsDataContainer> mDataContainer;
  QPointer<QCPAbstractPlottable> mDataPlottable;
  ErrorType mErrorType;
  double mWhiskerWidth;
  double mSymbolGap;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual void drawLegendIcon(QCPPainter *painter,
                              const QRectF &rect) const Q_DECL_OVERRIDE;
  virtual QCPRange
  getKeyRange(bool &foundRange,
              QCP::SignDomain inSignDomain = QCP::sdBoth) const Q_DECL_OVERRIDE;
  virtual QCPRange
  getValueRange(bool &foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                const QCPRange &inKeyRange = QCPRange()) const Q_DECL_OVERRIDE;

  void getErrorBarLines(QCPErrorBarsDataContainer::const_iterator it,
                        QVector<QLineF> &backbones,
                        QVector<QLineF> &whiskers) const;
  void getVisibleDataBounds(QCPErrorBarsDataContainer::const_iterator &begin,
                            QCPErrorBarsDataContainer::const_iterator &end,
                            const QCPDataRange &rangeRestriction) const;
  double
  pointDistance(const QPointF &pixelPoint,
                QCPErrorBarsDataContainer::const_iterator &closestData) const;

  void getDataSegments(QList<QCPDataRange> &selectedSegments,
                       QList<QCPDataRange> &unselectedSegments) const;
  bool errorBarVisible(int index) const;
  bool rectIntersectsLine(const QRectF &pixelRect, const QLineF &line) const;

  friend class QCustomPlot;
  friend class QCPLegend;
};

class QCP_LIB_DECL QCPItemStraightLine : public QCPAbstractItem {
  Q_OBJECT

  Q_PROPERTY(QPen pen READ pen WRITE setPen)
  Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)

public:
  explicit QCPItemStraightLine(QCustomPlot *parentPlot);
  virtual ~QCPItemStraightLine();

  QPen pen() const { return mPen; }
  QPen selectedPen() const { return mSelectedPen; }

  void setPen(const QPen &pen);
  void setSelectedPen(const QPen &pen);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;

  QCPItemPosition *const point1;
  QCPItemPosition *const point2;

protected:
  QPen mPen, mSelectedPen;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;

  QLineF getRectClippedStraightLine(const QCPVector2D &point1,
                                    const QCPVector2D &vec,
                                    const QRect &rect) const;
  QPen mainPen() const;
};

class QCP_LIB_DECL QCPItemLine : public QCPAbstractItem {
  Q_OBJECT

  Q_PROPERTY(QPen pen READ pen WRITE setPen)
  Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)
  Q_PROPERTY(QCPLineEnding head READ head WRITE setHead)
  Q_PROPERTY(QCPLineEnding tail READ tail WRITE setTail)

public:
  explicit QCPItemLine(QCustomPlot *parentPlot);
  virtual ~QCPItemLine();

  QPen pen() const { return mPen; }
  QPen selectedPen() const { return mSelectedPen; }
  QCPLineEnding head() const { return mHead; }
  QCPLineEnding tail() const { return mTail; }

  void setPen(const QPen &pen);
  void setSelectedPen(const QPen &pen);
  void setHead(const QCPLineEnding &head);
  void setTail(const QCPLineEnding &tail);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;

  QCPItemPosition *const start;
  QCPItemPosition *const end;

protected:
  QPen mPen, mSelectedPen;
  QCPLineEnding mHead, mTail;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;

  QLineF getRectClippedLine(const QCPVector2D &start, const QCPVector2D &end,
                            const QRect &rect) const;
  QPen mainPen() const;
};

class QCP_LIB_DECL QCPItemCurve : public QCPAbstractItem {
  Q_OBJECT

  Q_PROPERTY(QPen pen READ pen WRITE setPen)
  Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)
  Q_PROPERTY(QCPLineEnding head READ head WRITE setHead)
  Q_PROPERTY(QCPLineEnding tail READ tail WRITE setTail)

public:
  explicit QCPItemCurve(QCustomPlot *parentPlot);
  virtual ~QCPItemCurve();

  QPen pen() const { return mPen; }
  QPen selectedPen() const { return mSelectedPen; }
  QCPLineEnding head() const { return mHead; }
  QCPLineEnding tail() const { return mTail; }

  void setPen(const QPen &pen);
  void setSelectedPen(const QPen &pen);
  void setHead(const QCPLineEnding &head);
  void setTail(const QCPLineEnding &tail);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;

  QCPItemPosition *const start;
  QCPItemPosition *const startDir;
  QCPItemPosition *const endDir;
  QCPItemPosition *const end;

protected:
  QPen mPen, mSelectedPen;
  QCPLineEnding mHead, mTail;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;

  QPen mainPen() const;
};

class QCP_LIB_DECL QCPItemRect : public QCPAbstractItem {
  Q_OBJECT

  Q_PROPERTY(QPen pen READ pen WRITE setPen)
  Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)
  Q_PROPERTY(QBrush brush READ brush WRITE setBrush)
  Q_PROPERTY(QBrush selectedBrush READ selectedBrush WRITE setSelectedBrush)

public:
  explicit QCPItemRect(QCustomPlot *parentPlot);
  virtual ~QCPItemRect();

  QPen pen() const { return mPen; }
  QPen selectedPen() const { return mSelectedPen; }
  QBrush brush() const { return mBrush; }
  QBrush selectedBrush() const { return mSelectedBrush; }

  void setPen(const QPen &pen);
  void setSelectedPen(const QPen &pen);
  void setBrush(const QBrush &brush);
  void setSelectedBrush(const QBrush &brush);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;

  QCPItemPosition *const topLeft;
  QCPItemPosition *const bottomRight;
  QCPItemAnchor *const top;
  QCPItemAnchor *const topRight;
  QCPItemAnchor *const right;
  QCPItemAnchor *const bottom;
  QCPItemAnchor *const bottomLeft;
  QCPItemAnchor *const left;

protected:
  enum AnchorIndex {
    aiTop,
    aiTopRight,
    aiRight,
    aiBottom,
    aiBottomLeft,
    aiLeft
  };

  QPen mPen, mSelectedPen;
  QBrush mBrush, mSelectedBrush;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual QPointF anchorPixelPosition(int anchorId) const Q_DECL_OVERRIDE;

  QPen mainPen() const;
  QBrush mainBrush() const;
};

class QCP_LIB_DECL QCPItemText : public QCPAbstractItem {
  Q_OBJECT

  Q_PROPERTY(QColor color READ color WRITE setColor)
  Q_PROPERTY(QColor selectedColor READ selectedColor WRITE setSelectedColor)
  Q_PROPERTY(QPen pen READ pen WRITE setPen)
  Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)
  Q_PROPERTY(QBrush brush READ brush WRITE setBrush)
  Q_PROPERTY(QBrush selectedBrush READ selectedBrush WRITE setSelectedBrush)
  Q_PROPERTY(QFont font READ font WRITE setFont)
  Q_PROPERTY(QFont selectedFont READ selectedFont WRITE setSelectedFont)
  Q_PROPERTY(QString text READ text WRITE setText)
  Q_PROPERTY(Qt::Alignment positionAlignment READ positionAlignment WRITE
                 setPositionAlignment)
  Q_PROPERTY(
      Qt::Alignment textAlignment READ textAlignment WRITE setTextAlignment)
  Q_PROPERTY(double rotation READ rotation WRITE setRotation)
  Q_PROPERTY(QMargins padding READ padding WRITE setPadding)

public:
  explicit QCPItemText(QCustomPlot *parentPlot);
  virtual ~QCPItemText();

  QColor color() const { return mColor; }
  QColor selectedColor() const { return mSelectedColor; }
  QPen pen() const { return mPen; }
  QPen selectedPen() const { return mSelectedPen; }
  QBrush brush() const { return mBrush; }
  QBrush selectedBrush() const { return mSelectedBrush; }
  QFont font() const { return mFont; }
  QFont selectedFont() const { return mSelectedFont; }
  QString text() const { return mText; }
  Qt::Alignment positionAlignment() const { return mPositionAlignment; }
  Qt::Alignment textAlignment() const { return mTextAlignment; }
  double rotation() const { return mRotation; }
  QMargins padding() const { return mPadding; }

  void setColor(const QColor &color);
  void setSelectedColor(const QColor &color);
  void setPen(const QPen &pen);
  void setSelectedPen(const QPen &pen);
  void setBrush(const QBrush &brush);
  void setSelectedBrush(const QBrush &brush);
  void setFont(const QFont &font);
  void setSelectedFont(const QFont &font);
  void setText(const QString &text);
  void setPositionAlignment(Qt::Alignment alignment);
  void setTextAlignment(Qt::Alignment alignment);
  void setRotation(double degrees);
  void setPadding(const QMargins &padding);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;

  QCPItemPosition *const position;
  QCPItemAnchor *const topLeft;
  QCPItemAnchor *const top;
  QCPItemAnchor *const topRight;
  QCPItemAnchor *const right;
  QCPItemAnchor *const bottomRight;
  QCPItemAnchor *const bottom;
  QCPItemAnchor *const bottomLeft;
  QCPItemAnchor *const left;

protected:
  enum AnchorIndex {
    aiTopLeft,
    aiTop,
    aiTopRight,
    aiRight,
    aiBottomRight,
    aiBottom,
    aiBottomLeft,
    aiLeft
  };

  QColor mColor, mSelectedColor;
  QPen mPen, mSelectedPen;
  QBrush mBrush, mSelectedBrush;
  QFont mFont, mSelectedFont;
  QString mText;
  Qt::Alignment mPositionAlignment;
  Qt::Alignment mTextAlignment;
  double mRotation;
  QMargins mPadding;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual QPointF anchorPixelPosition(int anchorId) const Q_DECL_OVERRIDE;

  QPointF getTextDrawPoint(const QPointF &pos, const QRectF &rect,
                           Qt::Alignment positionAlignment) const;
  QFont mainFont() const;
  QColor mainColor() const;
  QPen mainPen() const;
  QBrush mainBrush() const;
};

class QCP_LIB_DECL QCPItemEllipse : public QCPAbstractItem {
  Q_OBJECT

  Q_PROPERTY(QPen pen READ pen WRITE setPen)
  Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)
  Q_PROPERTY(QBrush brush READ brush WRITE setBrush)
  Q_PROPERTY(QBrush selectedBrush READ selectedBrush WRITE setSelectedBrush)

public:
  explicit QCPItemEllipse(QCustomPlot *parentPlot);
  virtual ~QCPItemEllipse();

  QPen pen() const { return mPen; }
  QPen selectedPen() const { return mSelectedPen; }
  QBrush brush() const { return mBrush; }
  QBrush selectedBrush() const { return mSelectedBrush; }

  void setPen(const QPen &pen);
  void setSelectedPen(const QPen &pen);
  void setBrush(const QBrush &brush);
  void setSelectedBrush(const QBrush &brush);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;

  QCPItemPosition *const topLeft;
  QCPItemPosition *const bottomRight;
  QCPItemAnchor *const topLeftRim;
  QCPItemAnchor *const top;
  QCPItemAnchor *const topRightRim;
  QCPItemAnchor *const right;
  QCPItemAnchor *const bottomRightRim;
  QCPItemAnchor *const bottom;
  QCPItemAnchor *const bottomLeftRim;
  QCPItemAnchor *const left;
  QCPItemAnchor *const center;

protected:
  enum AnchorIndex {
    aiTopLeftRim,
    aiTop,
    aiTopRightRim,
    aiRight,
    aiBottomRightRim,
    aiBottom,
    aiBottomLeftRim,
    aiLeft,
    aiCenter
  };

  QPen mPen, mSelectedPen;
  QBrush mBrush, mSelectedBrush;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual QPointF anchorPixelPosition(int anchorId) const Q_DECL_OVERRIDE;

  QPen mainPen() const;
  QBrush mainBrush() const;
};

class QCP_LIB_DECL QCPItemPixmap : public QCPAbstractItem {
  Q_OBJECT

  Q_PROPERTY(QPixmap pixmap READ pixmap WRITE setPixmap)
  Q_PROPERTY(bool scaled READ scaled WRITE setScaled)
  Q_PROPERTY(Qt::AspectRatioMode aspectRatioMode READ aspectRatioMode)
  Q_PROPERTY(Qt::TransformationMode transformationMode READ transformationMode)
  Q_PROPERTY(QPen pen READ pen WRITE setPen)
  Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)

public:
  explicit QCPItemPixmap(QCustomPlot *parentPlot);
  virtual ~QCPItemPixmap();

  QPixmap pixmap() const { return mPixmap; }
  bool scaled() const { return mScaled; }
  Qt::AspectRatioMode aspectRatioMode() const { return mAspectRatioMode; }
  Qt::TransformationMode transformationMode() const {
    return mTransformationMode;
  }
  QPen pen() const { return mPen; }
  QPen selectedPen() const { return mSelectedPen; }

  void setPixmap(const QPixmap &pixmap);
  void setScaled(
      bool scaled, Qt::AspectRatioMode aspectRatioMode = Qt::KeepAspectRatio,
      Qt::TransformationMode transformationMode = Qt::SmoothTransformation);
  void setPen(const QPen &pen);
  void setSelectedPen(const QPen &pen);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;

  QCPItemPosition *const topLeft;
  QCPItemPosition *const bottomRight;
  QCPItemAnchor *const top;
  QCPItemAnchor *const topRight;
  QCPItemAnchor *const right;
  QCPItemAnchor *const bottom;
  QCPItemAnchor *const bottomLeft;
  QCPItemAnchor *const left;

protected:
  enum AnchorIndex {
    aiTop,
    aiTopRight,
    aiRight,
    aiBottom,
    aiBottomLeft,
    aiLeft
  };

  QPixmap mPixmap;
  QPixmap mScaledPixmap;
  bool mScaled;
  bool mScaledPixmapInvalidated;
  Qt::AspectRatioMode mAspectRatioMode;
  Qt::TransformationMode mTransformationMode;
  QPen mPen, mSelectedPen;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual QPointF anchorPixelPosition(int anchorId) const Q_DECL_OVERRIDE;

  void updateScaledPixmap(QRect finalRect = QRect(), bool flipHorz = false,
                          bool flipVert = false);
  QRect getFinalRect(bool *flippedHorz = 0, bool *flippedVert = 0) const;
  QPen mainPen() const;
};

class QCP_LIB_DECL QCPItemTracer : public QCPAbstractItem {
  Q_OBJECT

  Q_PROPERTY(QPen pen READ pen WRITE setPen)
  Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)
  Q_PROPERTY(QBrush brush READ brush WRITE setBrush)
  Q_PROPERTY(QBrush selectedBrush READ selectedBrush WRITE setSelectedBrush)
  Q_PROPERTY(double size READ size WRITE setSize)
  Q_PROPERTY(TracerStyle style READ style WRITE setStyle)
  Q_PROPERTY(QCPGraph *graph READ graph WRITE setGraph)
  Q_PROPERTY(double graphKey READ graphKey WRITE setGraphKey)
  Q_PROPERTY(bool interpolating READ interpolating WRITE setInterpolating)

public:
  enum TracerStyle {
    tsNone

    ,
    tsPlus

    ,
    tsCrosshair

    ,
    tsCircle

    ,
    tsSquare

  };
  Q_ENUMS(TracerStyle)

  explicit QCPItemTracer(QCustomPlot *parentPlot);
  virtual ~QCPItemTracer();

  QPen pen() const { return mPen; }
  QPen selectedPen() const { return mSelectedPen; }
  QBrush brush() const { return mBrush; }
  QBrush selectedBrush() const { return mSelectedBrush; }
  double size() const { return mSize; }
  TracerStyle style() const { return mStyle; }
  QCPGraph *graph() const { return mGraph; }
  double graphKey() const { return mGraphKey; }
  bool interpolating() const { return mInterpolating; }

  void setPen(const QPen &pen);
  void setSelectedPen(const QPen &pen);
  void setBrush(const QBrush &brush);
  void setSelectedBrush(const QBrush &brush);
  void setSize(double size);
  void setStyle(TracerStyle style);
  void setGraph(QCPGraph *graph);
  void setGraphKey(double key);
  void setInterpolating(bool enabled);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;

  void updatePosition();

  QCPItemPosition *const position;

protected:
  QPen mPen, mSelectedPen;
  QBrush mBrush, mSelectedBrush;
  double mSize;
  TracerStyle mStyle;
  QCPGraph *mGraph;
  double mGraphKey;
  bool mInterpolating;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;

  QPen mainPen() const;
  QBrush mainBrush() const;
};
Q_DECLARE_METATYPE(QCPItemTracer::TracerStyle)

class QCP_LIB_DECL QCPItemBracket : public QCPAbstractItem {
  Q_OBJECT

  Q_PROPERTY(QPen pen READ pen WRITE setPen)
  Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)
  Q_PROPERTY(double length READ length WRITE setLength)
  Q_PROPERTY(BracketStyle style READ style WRITE setStyle)

public:
  enum BracketStyle {
    bsSquare

    ,
    bsRound

    ,
    bsCurly

    ,
    bsCalligraphic

  };
  Q_ENUMS(BracketStyle)

  explicit QCPItemBracket(QCustomPlot *parentPlot);
  virtual ~QCPItemBracket();

  QPen pen() const { return mPen; }
  QPen selectedPen() const { return mSelectedPen; }
  double length() const { return mLength; }
  BracketStyle style() const { return mStyle; }

  void setPen(const QPen &pen);
  void setSelectedPen(const QPen &pen);
  void setLength(double length);
  void setStyle(BracketStyle style);

  virtual double selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details = 0) const Q_DECL_OVERRIDE;

  QCPItemPosition *const left;
  QCPItemPosition *const right;
  QCPItemAnchor *const center;

protected:
  enum AnchorIndex { aiCenter };
  QPen mPen, mSelectedPen;
  double mLength;
  BracketStyle mStyle;

  virtual void draw(QCPPainter *painter) Q_DECL_OVERRIDE;
  virtual QPointF anchorPixelPosition(int anchorId) const Q_DECL_OVERRIDE;

  QPen mainPen() const;
};
Q_DECLARE_METATYPE(QCPItemBracket::BracketStyle)

#endif
