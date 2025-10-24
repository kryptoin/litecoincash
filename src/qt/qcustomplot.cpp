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

#include "qcustomplot.h"

QCPVector2D::QCPVector2D() : mX(0), mY(0) {}

QCPVector2D::QCPVector2D(double x, double y) : mX(x), mY(y) {}

QCPVector2D::QCPVector2D(const QPoint &point) : mX(point.x()), mY(point.y()) {}

QCPVector2D::QCPVector2D(const QPointF &point) : mX(point.x()), mY(point.y()) {}

void QCPVector2D::normalize() {
  double len = length();
  mX /= len;
  mY /= len;
}

QCPVector2D QCPVector2D::normalized() const {
  QCPVector2D result(mX, mY);
  result.normalize();
  return result;
}

double QCPVector2D::distanceSquaredToLine(const QCPVector2D &start,
                                          const QCPVector2D &end) const {
  QCPVector2D v(end - start);
  double vLengthSqr = v.lengthSquared();
  if (!qFuzzyIsNull(vLengthSqr)) {
    double mu = v.dot(*this - start) / vLengthSqr;
    if (mu < 0)
      return (*this - start).lengthSquared();
    else if (mu > 1)
      return (*this - end).lengthSquared();
    else
      return ((start + mu * v) - *this).lengthSquared();
  } else
    return (*this - start).lengthSquared();
}

double QCPVector2D::distanceSquaredToLine(const QLineF &line) const {
  return distanceSquaredToLine(QCPVector2D(line.p1()), QCPVector2D(line.p2()));
}

double QCPVector2D::distanceToStraightLine(const QCPVector2D &base,
                                           const QCPVector2D &direction) const {
  return qAbs((*this - base).dot(direction.perpendicular())) /
         direction.length();
}

QCPVector2D &QCPVector2D::operator*=(double factor) {
  mX *= factor;
  mY *= factor;
  return *this;
}

QCPVector2D &QCPVector2D::operator/=(double divisor) {
  mX /= divisor;
  mY /= divisor;
  return *this;
}

QCPVector2D &QCPVector2D::operator+=(const QCPVector2D &vector) {
  mX += vector.mX;
  mY += vector.mY;
  return *this;
}

QCPVector2D &QCPVector2D::operator-=(const QCPVector2D &vector) {
  mX -= vector.mX;
  mY -= vector.mY;
  return *this;
}

QCPPainter::QCPPainter()
    : QPainter(), mModes(pmDefault), mIsAntialiasing(false) {}

QCPPainter::QCPPainter(QPaintDevice *device)
    : QPainter(device), mModes(pmDefault), mIsAntialiasing(false) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)

  if (isActive())
    setRenderHint(QPainter::NonCosmeticDefaultPen);
#endif
}

void QCPPainter::setPen(const QPen &pen) {
  QPainter::setPen(pen);
  if (mModes.testFlag(pmNonCosmetic))
    makeNonCosmetic();
}

void QCPPainter::setPen(const QColor &color) {
  QPainter::setPen(color);
  if (mModes.testFlag(pmNonCosmetic))
    makeNonCosmetic();
}

void QCPPainter::setPen(Qt::PenStyle penStyle) {
  QPainter::setPen(penStyle);
  if (mModes.testFlag(pmNonCosmetic))
    makeNonCosmetic();
}

void QCPPainter::drawLine(const QLineF &line) {
  if (mIsAntialiasing || mModes.testFlag(pmVectorized))
    QPainter::drawLine(line);
  else
    QPainter::drawLine(line.toLine());
}

void QCPPainter::setAntialiasing(bool enabled) {
  setRenderHint(QPainter::Antialiasing, enabled);
  if (mIsAntialiasing != enabled) {
    mIsAntialiasing = enabled;
    if (!mModes.testFlag(pmVectorized))

    {
      if (mIsAntialiasing)
        translate(0.5, 0.5);
      else
        translate(-0.5, -0.5);
    }
  }
}

void QCPPainter::setModes(QCPPainter::PainterModes modes) { mModes = modes; }

bool QCPPainter::begin(QPaintDevice *device) {
  bool result = QPainter::begin(device);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)

  if (result)
    setRenderHint(QPainter::NonCosmeticDefaultPen);
#endif
  return result;
}

void QCPPainter::setMode(QCPPainter::PainterMode mode, bool enabled) {
  if (!enabled && mModes.testFlag(mode))
    mModes &= ~mode;
  else if (enabled && !mModes.testFlag(mode))
    mModes |= mode;
}

void QCPPainter::save() {
  mAntialiasingStack.push(mIsAntialiasing);
  QPainter::save();
}

void QCPPainter::restore() {
  if (!mAntialiasingStack.isEmpty())
    mIsAntialiasing = mAntialiasingStack.pop();
  else
    qDebug() << Q_FUNC_INFO << "Unbalanced save/restore";
  QPainter::restore();
}

void QCPPainter::makeNonCosmetic() {
  if (qFuzzyIsNull(pen().widthF())) {
    QPen p = pen();
    p.setWidth(1);
    QPainter::setPen(p);
  }
}

QCPAbstractPaintBuffer::QCPAbstractPaintBuffer(const QSize &size,
                                               double devicePixelRatio)
    : mSize(size), mDevicePixelRatio(devicePixelRatio), mInvalidated(true) {}

QCPAbstractPaintBuffer::~QCPAbstractPaintBuffer() {}

void QCPAbstractPaintBuffer::setSize(const QSize &size) {
  if (mSize != size) {
    mSize = size;
    reallocateBuffer();
  }
}

void QCPAbstractPaintBuffer::setInvalidated(bool invalidated) {
  mInvalidated = invalidated;
}

void QCPAbstractPaintBuffer::setDevicePixelRatio(double ratio) {
  if (!qFuzzyCompare(ratio, mDevicePixelRatio)) {
#ifdef QCP_DEVICEPIXELRATIO_SUPPORTED
    mDevicePixelRatio = ratio;
    reallocateBuffer();
#else
    qDebug() << Q_FUNC_INFO
             << "Device pixel ratios not supported for Qt versions before 5.4";
    mDevicePixelRatio = 1.0;
#endif
  }
}

QCPPaintBufferPixmap::QCPPaintBufferPixmap(const QSize &size,
                                           double devicePixelRatio)
    : QCPAbstractPaintBuffer(size, devicePixelRatio) {
  QCPPaintBufferPixmap::reallocateBuffer();
}

QCPPaintBufferPixmap::~QCPPaintBufferPixmap() {}

QCPPainter *QCPPaintBufferPixmap::startPainting() {
  QCPPainter *result = new QCPPainter(&mBuffer);
  result->setRenderHint(QPainter::HighQualityAntialiasing);
  return result;
}

void QCPPaintBufferPixmap::draw(QCPPainter *painter) const {
  if (painter && painter->isActive())
    painter->drawPixmap(0, 0, mBuffer);
  else
    qDebug() << Q_FUNC_INFO << "invalid or inactive painter passed";
}

void QCPPaintBufferPixmap::clear(const QColor &color) { mBuffer.fill(color); }

void QCPPaintBufferPixmap::reallocateBuffer() {
  setInvalidated();
  if (!qFuzzyCompare(1.0, mDevicePixelRatio)) {
#ifdef QCP_DEVICEPIXELRATIO_SUPPORTED
    mBuffer = QPixmap(mSize * mDevicePixelRatio);
    mBuffer.setDevicePixelRatio(mDevicePixelRatio);
#else
    qDebug() << Q_FUNC_INFO
             << "Device pixel ratios not supported for Qt versions before 5.4";
    mDevicePixelRatio = 1.0;
    mBuffer = QPixmap(mSize);
#endif
  } else {
    mBuffer = QPixmap(mSize);
  }
}

#ifdef QCP_OPENGL_PBUFFER

QCPPaintBufferGlPbuffer::QCPPaintBufferGlPbuffer(const QSize &size,
                                                 double devicePixelRatio,
                                                 int multisamples)
    : QCPAbstractPaintBuffer(size, devicePixelRatio), mGlPBuffer(0),
      mMultisamples(qMax(0, multisamples)) {
  QCPPaintBufferGlPbuffer::reallocateBuffer();
}

QCPPaintBufferGlPbuffer::~QCPPaintBufferGlPbuffer() {
  if (mGlPBuffer)
    delete mGlPBuffer;
}

QCPPainter *QCPPaintBufferGlPbuffer::startPainting() {
  if (!mGlPBuffer->isValid()) {
    qDebug() << Q_FUNC_INFO
             << "OpenGL frame buffer object doesn't exist, reallocateBuffer "
                "was not called?";
    return 0;
  }

  QCPPainter *result = new QCPPainter(mGlPBuffer);
  result->setRenderHint(QPainter::HighQualityAntialiasing);
  return result;
}

void QCPPaintBufferGlPbuffer::draw(QCPPainter *painter) const {
  if (!painter || !painter->isActive()) {
    qDebug() << Q_FUNC_INFO << "invalid or inactive painter passed";
    return;
  }
  if (!mGlPBuffer->isValid()) {
    qDebug() << Q_FUNC_INFO
             << "OpenGL pbuffer isn't valid, reallocateBuffer was not called?";
    return;
  }
  painter->drawImage(0, 0, mGlPBuffer->toImage());
}

void QCPPaintBufferGlPbuffer::clear(const QColor &color) {
  if (mGlPBuffer->isValid()) {
    mGlPBuffer->makeCurrent();
    glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    mGlPBuffer->doneCurrent();
  } else
    qDebug() << Q_FUNC_INFO << "OpenGL pbuffer invalid or context not current";
}

void QCPPaintBufferGlPbuffer::reallocateBuffer() {
  if (mGlPBuffer)
    delete mGlPBuffer;

  QGLFormat format;
  format.setAlpha(true);
  format.setSamples(mMultisamples);
  mGlPBuffer = new QGLPixelBuffer(mSize, format);
}
#endif

#ifdef QCP_OPENGL_FBO

QCPPaintBufferGlFbo::QCPPaintBufferGlFbo(
    const QSize &size, double devicePixelRatio,
    QWeakPointer<QOpenGLContext> glContext,
    QWeakPointer<QOpenGLPaintDevice> glPaintDevice)
    : QCPAbstractPaintBuffer(size, devicePixelRatio), mGlContext(glContext),
      mGlPaintDevice(glPaintDevice), mGlFrameBuffer(0) {
  QCPPaintBufferGlFbo::reallocateBuffer();
}

QCPPaintBufferGlFbo::~QCPPaintBufferGlFbo() {
  if (mGlFrameBuffer)
    delete mGlFrameBuffer;
}

QCPPainter *QCPPaintBufferGlFbo::startPainting() {
  if (mGlPaintDevice.isNull()) {
    qDebug() << Q_FUNC_INFO << "OpenGL paint device doesn't exist";
    return 0;
  }
  if (!mGlFrameBuffer) {
    qDebug() << Q_FUNC_INFO
             << "OpenGL frame buffer object doesn't exist, reallocateBuffer "
                "was not called?";
    return 0;
  }

  if (QOpenGLContext::currentContext() != mGlContext.data())
    mGlContext.data()->makeCurrent(mGlContext.data()->surface());
  mGlFrameBuffer->bind();
  QCPPainter *result = new QCPPainter(mGlPaintDevice.data());
  result->setRenderHint(QPainter::HighQualityAntialiasing);
  return result;
}

void QCPPaintBufferGlFbo::donePainting() {
  if (mGlFrameBuffer && mGlFrameBuffer->isBound())
    mGlFrameBuffer->release();
  else
    qDebug() << Q_FUNC_INFO
             << "Either OpenGL frame buffer not valid or was not bound";
}

void QCPPaintBufferGlFbo::draw(QCPPainter *painter) const {
  if (!painter || !painter->isActive()) {
    qDebug() << Q_FUNC_INFO << "invalid or inactive painter passed";
    return;
  }
  if (!mGlFrameBuffer) {
    qDebug() << Q_FUNC_INFO
             << "OpenGL frame buffer object doesn't exist, reallocateBuffer "
                "was not called?";
    return;
  }
  painter->drawImage(0, 0, mGlFrameBuffer->toImage());
}

void QCPPaintBufferGlFbo::clear(const QColor &color) {
  if (mGlContext.isNull()) {
    qDebug() << Q_FUNC_INFO << "OpenGL context doesn't exist";
    return;
  }
  if (!mGlFrameBuffer) {
    qDebug() << Q_FUNC_INFO
             << "OpenGL frame buffer object doesn't exist, reallocateBuffer "
                "was not called?";
    return;
  }

  if (QOpenGLContext::currentContext() != mGlContext.data())
    mGlContext.data()->makeCurrent(mGlContext.data()->surface());
  mGlFrameBuffer->bind();
  glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  mGlFrameBuffer->release();
}

void QCPPaintBufferGlFbo::reallocateBuffer() {
  if (mGlFrameBuffer) {
    if (mGlFrameBuffer->isBound())
      mGlFrameBuffer->release();
    delete mGlFrameBuffer;
    mGlFrameBuffer = 0;
  }

  if (mGlContext.isNull()) {
    qDebug() << Q_FUNC_INFO << "OpenGL context doesn't exist";
    return;
  }
  if (mGlPaintDevice.isNull()) {
    qDebug() << Q_FUNC_INFO << "OpenGL paint device doesn't exist";
    return;
  }

  mGlContext.data()->makeCurrent(mGlContext.data()->surface());
  QOpenGLFramebufferObjectFormat frameBufferFormat;
  frameBufferFormat.setSamples(mGlContext.data()->format().samples());
  frameBufferFormat.setAttachment(
      QOpenGLFramebufferObject::CombinedDepthStencil);
  mGlFrameBuffer = new QOpenGLFramebufferObject(mSize * mDevicePixelRatio,
                                                frameBufferFormat);
  if (mGlPaintDevice.data()->size() != mSize * mDevicePixelRatio)
    mGlPaintDevice.data()->setSize(mSize * mDevicePixelRatio);
#ifdef QCP_DEVICEPIXELRATIO_SUPPORTED
  mGlPaintDevice.data()->setDevicePixelRatio(mDevicePixelRatio);
#endif
}
#endif

QCPLayer::QCPLayer(QCustomPlot *parentPlot, const QString &layerName)
    : QObject(parentPlot), mParentPlot(parentPlot), mName(layerName),
      mIndex(-1),

      mVisible(true), mMode(lmLogical) {}

QCPLayer::~QCPLayer() {
  while (!mChildren.isEmpty())
    mChildren.last()->setLayer(0);

  if (mParentPlot->currentLayer() == this)
    qDebug() << Q_FUNC_INFO
             << "The parent plot's mCurrentLayer will be a dangling pointer. "
                "Should have been set to a valid layer or 0 beforehand.";
}

void QCPLayer::setVisible(bool visible) { mVisible = visible; }

void QCPLayer::setMode(QCPLayer::LayerMode mode) {
  if (mMode != mode) {
    mMode = mode;
    if (!mPaintBuffer.isNull())
      mPaintBuffer.data()->setInvalidated();
  }
}

void QCPLayer::draw(QCPPainter *painter) {
  Q_FOREACH (QCPLayerable *child, mChildren) {
    if (child->realVisibility()) {
      painter->save();
      painter->setClipRect(child->clipRect().translated(0, -1));
      child->applyDefaultAntialiasingHint(painter);
      child->draw(painter);
      painter->restore();
    }
  }
}

void QCPLayer::drawToPaintBuffer() {
  if (!mPaintBuffer.isNull()) {
    if (QCPPainter *painter = mPaintBuffer.data()->startPainting()) {
      if (painter->isActive())
        draw(painter);
      else
        qDebug() << Q_FUNC_INFO << "paint buffer returned inactive painter";
      delete painter;
      mPaintBuffer.data()->donePainting();
    } else
      qDebug() << Q_FUNC_INFO << "paint buffer returned zero painter";
  } else
    qDebug() << Q_FUNC_INFO
             << "no valid paint buffer associated with this layer";
}

void QCPLayer::replot() {
  if (mMode == lmBuffered && !mParentPlot->hasInvalidatedPaintBuffers()) {
    if (!mPaintBuffer.isNull()) {
      mPaintBuffer.data()->clear(Qt::transparent);
      drawToPaintBuffer();
      mPaintBuffer.data()->setInvalidated(false);
      mParentPlot->update();
    } else
      qDebug() << Q_FUNC_INFO
               << "no valid paint buffer associated with this layer";
  } else if (mMode == lmLogical)
    mParentPlot->replot();
}

void QCPLayer::addChild(QCPLayerable *layerable, bool prepend) {
  if (!mChildren.contains(layerable)) {
    if (prepend)
      mChildren.prepend(layerable);
    else
      mChildren.append(layerable);
    if (!mPaintBuffer.isNull())
      mPaintBuffer.data()->setInvalidated();
  } else
    qDebug() << Q_FUNC_INFO << "layerable is already child of this layer"
             << reinterpret_cast<quintptr>(layerable);
}

void QCPLayer::removeChild(QCPLayerable *layerable) {
  if (mChildren.removeOne(layerable)) {
    if (!mPaintBuffer.isNull())
      mPaintBuffer.data()->setInvalidated();
  } else
    qDebug() << Q_FUNC_INFO << "layerable is not child of this layer"
             << reinterpret_cast<quintptr>(layerable);
}

QCPLayerable::QCPLayerable(QCustomPlot *plot, QString targetLayer,
                           QCPLayerable *parentLayerable)
    : QObject(plot), mVisible(true), mParentPlot(plot),
      mParentLayerable(parentLayerable), mLayer(0), mAntialiased(true) {
  if (mParentPlot) {
    if (targetLayer.isEmpty())
      setLayer(mParentPlot->currentLayer());
    else if (!setLayer(targetLayer))
      qDebug() << Q_FUNC_INFO << "setting QCPlayerable initial layer to"
               << targetLayer << "failed.";
  }
}

QCPLayerable::~QCPLayerable() {
  if (mLayer) {
    mLayer->removeChild(this);
    mLayer = 0;
  }
}

void QCPLayerable::setVisible(bool on) { mVisible = on; }

bool QCPLayerable::setLayer(QCPLayer *layer) {
  return moveToLayer(layer, false);
}

bool QCPLayerable::setLayer(const QString &layerName) {
  if (!mParentPlot) {
    qDebug() << Q_FUNC_INFO << "no parent QCustomPlot set";
    return false;
  }
  if (QCPLayer *layer = mParentPlot->layer(layerName)) {
    return setLayer(layer);
  } else {
    qDebug() << Q_FUNC_INFO << "there is no layer with name" << layerName;
    return false;
  }
}

void QCPLayerable::setAntialiased(bool enabled) { mAntialiased = enabled; }

bool QCPLayerable::realVisibility() const {
  return mVisible && (!mLayer || mLayer->visible()) &&
         (!mParentLayerable || mParentLayerable.data()->realVisibility());
}

double QCPLayerable::selectTest(const QPointF &pos, bool onlySelectable,
                                QVariant *details) const {
  Q_UNUSED(pos)
  Q_UNUSED(onlySelectable)
  Q_UNUSED(details)
  return -1.0;
}

void QCPLayerable::initializeParentPlot(QCustomPlot *parentPlot) {
  if (mParentPlot) {
    qDebug() << Q_FUNC_INFO << "called with mParentPlot already initialized";
    return;
  }

  if (!parentPlot)
    qDebug() << Q_FUNC_INFO << "called with parentPlot zero";

  mParentPlot = parentPlot;
  parentPlotInitialized(mParentPlot);
}

void QCPLayerable::setParentLayerable(QCPLayerable *parentLayerable) {
  mParentLayerable = parentLayerable;
}

bool QCPLayerable::moveToLayer(QCPLayer *layer, bool prepend) {
  if (layer && !mParentPlot) {
    qDebug() << Q_FUNC_INFO << "no parent QCustomPlot set";
    return false;
  }
  if (layer && layer->parentPlot() != mParentPlot) {
    qDebug() << Q_FUNC_INFO << "layer" << layer->name()
             << "is not in same QCustomPlot as this layerable";
    return false;
  }

  QCPLayer *oldLayer = mLayer;
  if (mLayer)
    mLayer->removeChild(this);
  mLayer = layer;
  if (mLayer)
    mLayer->addChild(this, prepend);
  if (mLayer != oldLayer)
    Q_EMIT layerChanged(mLayer);
  return true;
}

void QCPLayerable::applyAntialiasingHint(
    QCPPainter *painter, bool localAntialiased,
    QCP::AntialiasedElement overrideElement) const {
  if (mParentPlot &&
      mParentPlot->notAntialiasedElements().testFlag(overrideElement))
    painter->setAntialiasing(false);
  else if (mParentPlot &&
           mParentPlot->antialiasedElements().testFlag(overrideElement))
    painter->setAntialiasing(true);
  else
    painter->setAntialiasing(localAntialiased);
}

void QCPLayerable::parentPlotInitialized(QCustomPlot *parentPlot) {
  Q_UNUSED(parentPlot)
}

QCP::Interaction QCPLayerable::selectionCategory() const {
  return QCP::iSelectOther;
}

QRect QCPLayerable::clipRect() const {
  if (mParentPlot)
    return mParentPlot->viewport();
  else
    return QRect();
}

void QCPLayerable::selectEvent(QMouseEvent *event, bool additive,
                               const QVariant &details,
                               bool *selectionStateChanged) {
  Q_UNUSED(event)
  Q_UNUSED(additive)
  Q_UNUSED(details)
  Q_UNUSED(selectionStateChanged)
}

void QCPLayerable::deselectEvent(bool *selectionStateChanged) {
  Q_UNUSED(selectionStateChanged)
}

void QCPLayerable::mousePressEvent(QMouseEvent *event,
                                   const QVariant &details) {
  Q_UNUSED(details)
  event->ignore();
}

void QCPLayerable::mouseMoveEvent(QMouseEvent *event, const QPointF &startPos) {
  Q_UNUSED(startPos)
  event->ignore();
}

void QCPLayerable::mouseReleaseEvent(QMouseEvent *event,
                                     const QPointF &startPos) {
  Q_UNUSED(startPos)
  event->ignore();
}

void QCPLayerable::mouseDoubleClickEvent(QMouseEvent *event,
                                         const QVariant &details) {
  Q_UNUSED(details)
  event->ignore();
}

void QCPLayerable::wheelEvent(QWheelEvent *event) { event->ignore(); }

const double QCPRange::minRange = 1e-280;

const double QCPRange::maxRange = 1e250;

QCPRange::QCPRange() : lower(0), upper(0) {}

QCPRange::QCPRange(double lower, double upper) : lower(lower), upper(upper) {
  normalize();
}

void QCPRange::expand(const QCPRange &otherRange) {
  if (lower > otherRange.lower || qIsNaN(lower))
    lower = otherRange.lower;
  if (upper < otherRange.upper || qIsNaN(upper))
    upper = otherRange.upper;
}

void QCPRange::expand(double includeCoord) {
  if (lower > includeCoord || qIsNaN(lower))
    lower = includeCoord;
  if (upper < includeCoord || qIsNaN(upper))
    upper = includeCoord;
}

QCPRange QCPRange::expanded(const QCPRange &otherRange) const {
  QCPRange result = *this;
  result.expand(otherRange);
  return result;
}

QCPRange QCPRange::expanded(double includeCoord) const {
  QCPRange result = *this;
  result.expand(includeCoord);
  return result;
}

QCPRange QCPRange::bounded(double lowerBound, double upperBound) const {
  if (lowerBound > upperBound)
    qSwap(lowerBound, upperBound);

  QCPRange result(lower, upper);
  if (result.lower < lowerBound) {
    result.lower = lowerBound;
    result.upper = lowerBound + size();
    if (result.upper > upperBound ||
        qFuzzyCompare(size(), upperBound - lowerBound))
      result.upper = upperBound;
  } else if (result.upper > upperBound) {
    result.upper = upperBound;
    result.lower = upperBound - size();
    if (result.lower < lowerBound ||
        qFuzzyCompare(size(), upperBound - lowerBound))
      result.lower = lowerBound;
  }

  return result;
}

QCPRange QCPRange::sanitizedForLogScale() const {
  double rangeFac = 1e-3;
  QCPRange sanitizedRange(lower, upper);
  sanitizedRange.normalize();

  if (sanitizedRange.lower == 0.0 && sanitizedRange.upper != 0.0) {
    if (rangeFac < sanitizedRange.upper * rangeFac)
      sanitizedRange.lower = rangeFac;
    else
      sanitizedRange.lower = sanitizedRange.upper * rangeFac;
  }

  else if (sanitizedRange.lower != 0.0 && sanitizedRange.upper == 0.0) {
    if (-rangeFac > sanitizedRange.lower * rangeFac)
      sanitizedRange.upper = -rangeFac;
    else
      sanitizedRange.upper = sanitizedRange.lower * rangeFac;
  } else if (sanitizedRange.lower < 0 && sanitizedRange.upper > 0) {
    if (-sanitizedRange.lower > sanitizedRange.upper) {
      if (-rangeFac > sanitizedRange.lower * rangeFac)
        sanitizedRange.upper = -rangeFac;
      else
        sanitizedRange.upper = sanitizedRange.lower * rangeFac;
    } else {
      if (rangeFac < sanitizedRange.upper * rangeFac)
        sanitizedRange.lower = rangeFac;
      else
        sanitizedRange.lower = sanitizedRange.upper * rangeFac;
    }
  }

  return sanitizedRange;
}

QCPRange QCPRange::sanitizedForLinScale() const {
  QCPRange sanitizedRange(lower, upper);
  sanitizedRange.normalize();
  return sanitizedRange;
}

bool QCPRange::validRange(double lower, double upper) {
  return (lower > -maxRange && upper < maxRange &&
          qAbs(lower - upper) > minRange && qAbs(lower - upper) < maxRange &&
          !(lower > 0 && qIsInf(upper / lower)) &&
          !(upper < 0 && qIsInf(lower / upper)));
}

bool QCPRange::validRange(const QCPRange &range) {
  return (range.lower > -maxRange && range.upper < maxRange &&
          qAbs(range.lower - range.upper) > minRange &&
          qAbs(range.lower - range.upper) < maxRange &&
          !(range.lower > 0 && qIsInf(range.upper / range.lower)) &&
          !(range.upper < 0 && qIsInf(range.lower / range.upper)));
}

QCPDataRange::QCPDataRange() : mBegin(0), mEnd(0) {}

QCPDataRange::QCPDataRange(int begin, int end) : mBegin(begin), mEnd(end) {}

QCPDataRange QCPDataRange::bounded(const QCPDataRange &other) const {
  QCPDataRange result(intersection(other));
  if (result.isEmpty())

  {
    if (mEnd <= other.mBegin)
      result = QCPDataRange(other.mBegin, other.mBegin);
    else
      result = QCPDataRange(other.mEnd, other.mEnd);
  }
  return result;
}

QCPDataRange QCPDataRange::expanded(const QCPDataRange &other) const {
  return QCPDataRange(qMin(mBegin, other.mBegin), qMax(mEnd, other.mEnd));
}

QCPDataRange QCPDataRange::intersection(const QCPDataRange &other) const {
  QCPDataRange result(qMax(mBegin, other.mBegin), qMin(mEnd, other.mEnd));
  if (result.isValid())
    return result;
  else
    return QCPDataRange();
}

bool QCPDataRange::intersects(const QCPDataRange &other) const {
  return !((mBegin > other.mBegin && mBegin >= other.mEnd) ||
           (mEnd <= other.mBegin && mEnd < other.mEnd));
}

bool QCPDataRange::contains(const QCPDataRange &other) const {
  return mBegin <= other.mBegin && mEnd >= other.mEnd;
}

QCPDataSelection::QCPDataSelection() {}

QCPDataSelection::QCPDataSelection(const QCPDataRange &range) {
  mDataRanges.append(range);
}

bool QCPDataSelection::operator==(const QCPDataSelection &other) const {
  if (mDataRanges.size() != other.mDataRanges.size())
    return false;
  for (int i = 0; i < mDataRanges.size(); ++i) {
    if (mDataRanges.at(i) != other.mDataRanges.at(i))
      return false;
  }
  return true;
}

QCPDataSelection &QCPDataSelection::operator+=(const QCPDataSelection &other) {
  mDataRanges << other.mDataRanges;
  simplify();
  return *this;
}

QCPDataSelection &QCPDataSelection::operator+=(const QCPDataRange &other) {
  addDataRange(other);
  return *this;
}

QCPDataSelection &QCPDataSelection::operator-=(const QCPDataSelection &other) {
  for (int i = 0; i < other.dataRangeCount(); ++i)
    *this -= other.dataRange(i);

  return *this;
}

QCPDataSelection &QCPDataSelection::operator-=(const QCPDataRange &other) {
  if (other.isEmpty() || isEmpty())
    return *this;

  simplify();
  int i = 0;
  while (i < mDataRanges.size()) {
    const int thisBegin = mDataRanges.at(i).begin();
    const int thisEnd = mDataRanges.at(i).end();
    if (thisBegin >= other.end())
      break;

    if (thisEnd > other.begin())

    {
      if (thisBegin >= other.begin())

      {
        if (thisEnd <= other.end())

        {
          mDataRanges.removeAt(i);
          continue;
        } else

          mDataRanges[i].setBegin(other.end());
      } else

      {
        if (thisEnd <= other.end())

        {
          mDataRanges[i].setEnd(other.begin());
        } else

        {
          mDataRanges[i].setEnd(other.begin());
          mDataRanges.insert(i + 1, QCPDataRange(other.end(), thisEnd));
          break;
        }
      }
    }
    ++i;
  }

  return *this;
}

int QCPDataSelection::dataPointCount() const {
  int result = 0;
  for (int i = 0; i < mDataRanges.size(); ++i)
    result += mDataRanges.at(i).length();
  return result;
}

QCPDataRange QCPDataSelection::dataRange(int index) const {
  if (index >= 0 && index < mDataRanges.size()) {
    return mDataRanges.at(index);
  } else {
    qDebug() << Q_FUNC_INFO << "index out of range:" << index;
    return QCPDataRange();
  }
}

QCPDataRange QCPDataSelection::span() const {
  if (isEmpty())
    return QCPDataRange();
  else
    return QCPDataRange(mDataRanges.first().begin(), mDataRanges.last().end());
}

void QCPDataSelection::addDataRange(const QCPDataRange &dataRange,
                                    bool simplify) {
  mDataRanges.append(dataRange);
  if (simplify)
    this->simplify();
}

void QCPDataSelection::clear() { mDataRanges.clear(); }

void QCPDataSelection::simplify() {
  for (int i = mDataRanges.size() - 1; i >= 0; --i) {
    if (mDataRanges.at(i).isEmpty())
      mDataRanges.removeAt(i);
  }
  if (mDataRanges.isEmpty())
    return;

  std::sort(mDataRanges.begin(), mDataRanges.end(), lessThanDataRangeBegin);

  int i = 1;
  while (i < mDataRanges.size()) {
    if (mDataRanges.at(i - 1).end() >= mDataRanges.at(i).begin())

    {
      mDataRanges[i - 1].setEnd(
          qMax(mDataRanges.at(i - 1).end(), mDataRanges.at(i).end()));
      mDataRanges.removeAt(i);
    } else
      ++i;
  }
}

void QCPDataSelection::enforceType(QCP::SelectionType type) {
  simplify();
  switch (type) {
  case QCP::stNone: {
    mDataRanges.clear();
    break;
  }
  case QCP::stWhole: {
    break;
  }
  case QCP::stSingleData: {
    if (!mDataRanges.isEmpty()) {
      if (mDataRanges.size() > 1)
        mDataRanges = QList<QCPDataRange>() << mDataRanges.first();
      if (mDataRanges.first().length() > 1)
        mDataRanges.first().setEnd(mDataRanges.first().begin() + 1);
    }
    break;
  }
  case QCP::stDataRange: {
    if (!isEmpty())
      mDataRanges = QList<QCPDataRange>() << span();
    break;
  }
  case QCP::stMultipleDataRanges: {
    break;
  }
  }
}

bool QCPDataSelection::contains(const QCPDataSelection &other) const {
  if (other.isEmpty())
    return false;

  int otherIndex = 0;
  int thisIndex = 0;
  while (thisIndex < mDataRanges.size() &&
         otherIndex < other.mDataRanges.size()) {
    if (mDataRanges.at(thisIndex).contains(other.mDataRanges.at(otherIndex)))
      ++otherIndex;
    else
      ++thisIndex;
  }
  return thisIndex < mDataRanges.size();
}

QCPDataSelection
QCPDataSelection::intersection(const QCPDataRange &other) const {
  QCPDataSelection result;
  for (int i = 0; i < mDataRanges.size(); ++i)
    result.addDataRange(mDataRanges.at(i).intersection(other), false);
  result.simplify();
  return result;
}

QCPDataSelection
QCPDataSelection::intersection(const QCPDataSelection &other) const {
  QCPDataSelection result;
  for (int i = 0; i < other.dataRangeCount(); ++i)
    result += intersection(other.dataRange(i));
  result.simplify();
  return result;
}

QCPDataSelection
QCPDataSelection::inverse(const QCPDataRange &outerRange) const {
  if (isEmpty())
    return QCPDataSelection(outerRange);
  QCPDataRange fullRange = outerRange.expanded(span());

  QCPDataSelection result;

  if (mDataRanges.first().begin() != fullRange.begin())
    result.addDataRange(
        QCPDataRange(fullRange.begin(), mDataRanges.first().begin()), false);

  for (int i = 1; i < mDataRanges.size(); ++i)
    result.addDataRange(
        QCPDataRange(mDataRanges.at(i - 1).end(), mDataRanges.at(i).begin()),
        false);

  if (mDataRanges.last().end() != fullRange.end())
    result.addDataRange(QCPDataRange(mDataRanges.last().end(), fullRange.end()),
                        false);
  result.simplify();
  return result;
}

QCPSelectionRect::QCPSelectionRect(QCustomPlot *parentPlot)
    : QCPLayerable(parentPlot), mPen(QBrush(Qt::gray), 0, Qt::DashLine),
      mBrush(Qt::NoBrush), mActive(false) {}

QCPSelectionRect::~QCPSelectionRect() { cancel(); }

QCPRange QCPSelectionRect::range(const QCPAxis *axis) const {
  if (axis) {
    if (axis->orientation() == Qt::Horizontal)
      return QCPRange(axis->pixelToCoord(mRect.left()),
                      axis->pixelToCoord(mRect.left() + mRect.width()));
    else
      return QCPRange(axis->pixelToCoord(mRect.top() + mRect.height()),
                      axis->pixelToCoord(mRect.top()));
  } else {
    qDebug() << Q_FUNC_INFO << "called with axis zero";
    return QCPRange();
  }
}

void QCPSelectionRect::setPen(const QPen &pen) { mPen = pen; }

void QCPSelectionRect::setBrush(const QBrush &brush) { mBrush = brush; }

void QCPSelectionRect::cancel() {
  if (mActive) {
    mActive = false;
    Q_EMIT canceled(mRect, 0);
  }
}

void QCPSelectionRect::startSelection(QMouseEvent *event) {
  mActive = true;
  mRect = QRect(event->pos(), event->pos());
  Q_EMIT started(event);
}

void QCPSelectionRect::moveSelection(QMouseEvent *event) {
  mRect.setBottomRight(event->pos());
  Q_EMIT changed(mRect, event);
  layer()->replot();
}

void QCPSelectionRect::endSelection(QMouseEvent *event) {
  mRect.setBottomRight(event->pos());
  mActive = false;
  Q_EMIT accepted(mRect, event);
}

void QCPSelectionRect::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape && mActive) {
    mActive = false;
    Q_EMIT canceled(mRect, event);
  }
}

void QCPSelectionRect::applyDefaultAntialiasingHint(QCPPainter *painter) const {
  applyAntialiasingHint(painter, mAntialiased, QCP::aeOther);
}

void QCPSelectionRect::draw(QCPPainter *painter) {
  if (mActive) {
    painter->setPen(mPen);
    painter->setBrush(mBrush);
    painter->drawRect(mRect);
  }
}

QCPMarginGroup::QCPMarginGroup(QCustomPlot *parentPlot)
    : QObject(parentPlot), mParentPlot(parentPlot) {
  mChildren.insert(QCP::msLeft, QList<QCPLayoutElement *>());
  mChildren.insert(QCP::msRight, QList<QCPLayoutElement *>());
  mChildren.insert(QCP::msTop, QList<QCPLayoutElement *>());
  mChildren.insert(QCP::msBottom, QList<QCPLayoutElement *>());
}

QCPMarginGroup::~QCPMarginGroup() { clear(); }

bool QCPMarginGroup::isEmpty() const {
  QHashIterator<QCP::MarginSide, QList<QCPLayoutElement *>> it(mChildren);
  while (it.hasNext()) {
    it.next();
    if (!it.value().isEmpty())
      return false;
  }
  return true;
}

void QCPMarginGroup::clear() {
  QHashIterator<QCP::MarginSide, QList<QCPLayoutElement *>> it(mChildren);
  while (it.hasNext()) {
    it.next();
    const QList<QCPLayoutElement *> elements = it.value();
    for (int i = elements.size() - 1; i >= 0; --i)
      elements.at(i)->setMarginGroup(it.key(), 0);
  }
}

int QCPMarginGroup::commonMargin(QCP::MarginSide side) const {
  int result = 0;
  const QList<QCPLayoutElement *> elements = mChildren.value(side);
  for (int i = 0; i < elements.size(); ++i) {
    if (!elements.at(i)->autoMargins().testFlag(side))
      continue;
    int m = qMax(elements.at(i)->calculateAutoMargin(side),
                 QCP::getMarginValue(elements.at(i)->minimumMargins(), side));
    if (m > result)
      result = m;
  }
  return result;
}

void QCPMarginGroup::addChild(QCP::MarginSide side, QCPLayoutElement *element) {
  if (!mChildren[side].contains(element))
    mChildren[side].append(element);
  else
    qDebug() << Q_FUNC_INFO
             << "element is already child of this margin group side"
             << reinterpret_cast<quintptr>(element);
}

void QCPMarginGroup::removeChild(QCP::MarginSide side,
                                 QCPLayoutElement *element) {
  if (!mChildren[side].removeOne(element))
    qDebug() << Q_FUNC_INFO << "element is not child of this margin group side"
             << reinterpret_cast<quintptr>(element);
}

QCPLayoutElement::QCPLayoutElement(QCustomPlot *parentPlot)
    : QCPLayerable(parentPlot),

      mParentLayout(0), mMinimumSize(),
      mMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX),
      mSizeConstraintRect(scrInnerRect), mRect(0, 0, 0, 0),
      mOuterRect(0, 0, 0, 0), mMargins(0, 0, 0, 0), mMinimumMargins(0, 0, 0, 0),
      mAutoMargins(QCP::msAll) {}

QCPLayoutElement::~QCPLayoutElement() {
  setMarginGroup(QCP::msAll, 0);

  if (qobject_cast<QCPLayout *>(mParentLayout))

    mParentLayout->take(this);
}

void QCPLayoutElement::setOuterRect(const QRect &rect) {
  if (mOuterRect != rect) {
    mOuterRect = rect;
    mRect = mOuterRect.adjusted(mMargins.left(), mMargins.top(),
                                -mMargins.right(), -mMargins.bottom());
  }
}

void QCPLayoutElement::setMargins(const QMargins &margins) {
  if (mMargins != margins) {
    mMargins = margins;
    mRect = mOuterRect.adjusted(mMargins.left(), mMargins.top(),
                                -mMargins.right(), -mMargins.bottom());
  }
}

void QCPLayoutElement::setMinimumMargins(const QMargins &margins) {
  if (mMinimumMargins != margins) {
    mMinimumMargins = margins;
  }
}

void QCPLayoutElement::setAutoMargins(QCP::MarginSides sides) {
  mAutoMargins = sides;
}

void QCPLayoutElement::setMinimumSize(const QSize &size) {
  if (mMinimumSize != size) {
    mMinimumSize = size;
    if (mParentLayout)
      mParentLayout->sizeConstraintsChanged();
  }
}

void QCPLayoutElement::setMinimumSize(int width, int height) {
  setMinimumSize(QSize(width, height));
}

void QCPLayoutElement::setMaximumSize(const QSize &size) {
  if (mMaximumSize != size) {
    mMaximumSize = size;
    if (mParentLayout)
      mParentLayout->sizeConstraintsChanged();
  }
}

void QCPLayoutElement::setMaximumSize(int width, int height) {
  setMaximumSize(QSize(width, height));
}

void QCPLayoutElement::setSizeConstraintRect(
    SizeConstraintRect constraintRect) {
  if (mSizeConstraintRect != constraintRect) {
    mSizeConstraintRect = constraintRect;
    if (mParentLayout)
      mParentLayout->sizeConstraintsChanged();
  }
}

void QCPLayoutElement::setMarginGroup(QCP::MarginSides sides,
                                      QCPMarginGroup *group) {
  QVector<QCP::MarginSide> sideVector;
  if (sides.testFlag(QCP::msLeft))
    sideVector.append(QCP::msLeft);
  if (sides.testFlag(QCP::msRight))
    sideVector.append(QCP::msRight);
  if (sides.testFlag(QCP::msTop))
    sideVector.append(QCP::msTop);
  if (sides.testFlag(QCP::msBottom))
    sideVector.append(QCP::msBottom);

  for (int i = 0; i < sideVector.size(); ++i) {
    QCP::MarginSide side = sideVector.at(i);
    if (marginGroup(side) != group) {
      QCPMarginGroup *oldGroup = marginGroup(side);
      if (oldGroup)

        oldGroup->removeChild(side, this);

      if (!group)

      {
        mMarginGroups.remove(side);
      } else

      {
        mMarginGroups[side] = group;
        group->addChild(side, this);
      }
    }
  }
}

void QCPLayoutElement::update(UpdatePhase phase) {
  if (phase == upMargins) {
    if (mAutoMargins != QCP::msNone) {
      QMargins newMargins = mMargins;
      QList<QCP::MarginSide> allMarginSides = QList<QCP::MarginSide>()
                                              << QCP::msLeft << QCP::msRight
                                              << QCP::msTop << QCP::msBottom;
      Q_FOREACH (QCP::MarginSide side, allMarginSides) {
        if (mAutoMargins.testFlag(side))

        {
          if (mMarginGroups.contains(side))
            QCP::setMarginValue(newMargins, side,
                                mMarginGroups[side]->commonMargin(side));

          else
            QCP::setMarginValue(newMargins, side, calculateAutoMargin(side));

          if (QCP::getMarginValue(newMargins, side) <
              QCP::getMarginValue(mMinimumMargins, side))
            QCP::setMarginValue(newMargins, side,
                                QCP::getMarginValue(mMinimumMargins, side));
        }
      }
      setMargins(newMargins);
    }
  }
}

QSize QCPLayoutElement::minimumOuterSizeHint() const {
  return QSize(mMargins.left() + mMargins.right(),
               mMargins.top() + mMargins.bottom());
}

QSize QCPLayoutElement::maximumOuterSizeHint() const {
  return QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
}

QList<QCPLayoutElement *> QCPLayoutElement::elements(bool recursive) const {
  Q_UNUSED(recursive)
  return QList<QCPLayoutElement *>();
}

double QCPLayoutElement::selectTest(const QPointF &pos, bool onlySelectable,
                                    QVariant *details) const {
  Q_UNUSED(details)

  if (onlySelectable)
    return -1;

  if (QRectF(mOuterRect).contains(pos)) {
    if (mParentPlot)
      return mParentPlot->selectionTolerance() * 0.99;
    else {
      qDebug() << Q_FUNC_INFO << "parent plot not defined";
      return -1;
    }
  } else
    return -1;
}

void QCPLayoutElement::parentPlotInitialized(QCustomPlot *parentPlot) {
  Q_FOREACH (QCPLayoutElement *el, elements(false)) {
    if (!el->parentPlot())
      el->initializeParentPlot(parentPlot);
  }
}

int QCPLayoutElement::calculateAutoMargin(QCP::MarginSide side) {
  return qMax(QCP::getMarginValue(mMargins, side),
              QCP::getMarginValue(mMinimumMargins, side));
}

void QCPLayoutElement::layoutChanged() {}

QCPLayout::QCPLayout() {}

void QCPLayout::update(UpdatePhase phase) {
  QCPLayoutElement::update(phase);

  if (phase == upLayout)
    updateLayout();

  const int elCount = elementCount();
  for (int i = 0; i < elCount; ++i) {
    if (QCPLayoutElement *el = elementAt(i))
      el->update(phase);
  }
}

QList<QCPLayoutElement *> QCPLayout::elements(bool recursive) const {
  const int c = elementCount();
  QList<QCPLayoutElement *> result;
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
  result.reserve(c);
#endif
  for (int i = 0; i < c; ++i)
    result.append(elementAt(i));
  if (recursive) {
    for (int i = 0; i < c; ++i) {
      if (result.at(i))
        result << result.at(i)->elements(recursive);
    }
  }
  return result;
}

void QCPLayout::simplify() {}

bool QCPLayout::removeAt(int index) {
  if (QCPLayoutElement *el = takeAt(index)) {
    delete el;
    return true;
  } else
    return false;
}

bool QCPLayout::remove(QCPLayoutElement *element) {
  if (take(element)) {
    delete element;
    return true;
  } else
    return false;
}

void QCPLayout::clear() {
  for (int i = elementCount() - 1; i >= 0; --i) {
    if (elementAt(i))
      removeAt(i);
  }
  simplify();
}

void QCPLayout::sizeConstraintsChanged() const {
  if (QWidget *w = qobject_cast<QWidget *>(parent()))
    w->updateGeometry();
  else if (QCPLayout *l = qobject_cast<QCPLayout *>(parent()))
    l->sizeConstraintsChanged();
}

void QCPLayout::updateLayout() {}

void QCPLayout::adoptElement(QCPLayoutElement *el) {
  if (el) {
    el->mParentLayout = this;
    el->setParentLayerable(this);
    el->setParent(this);
    if (!el->parentPlot())
      el->initializeParentPlot(mParentPlot);
    el->layoutChanged();
  } else
    qDebug() << Q_FUNC_INFO << "Null element passed";
}

void QCPLayout::releaseElement(QCPLayoutElement *el) {
  if (el) {
    el->mParentLayout = 0;
    el->setParentLayerable(0);
    el->setParent(mParentPlot);

  } else
    qDebug() << Q_FUNC_INFO << "Null element passed";
}

QVector<int> QCPLayout::getSectionSizes(QVector<int> maxSizes,
                                        QVector<int> minSizes,
                                        QVector<double> stretchFactors,
                                        int totalSize) const {
  if (maxSizes.size() != minSizes.size() ||
      minSizes.size() != stretchFactors.size()) {
    qDebug() << Q_FUNC_INFO << "Passed vector sizes aren't equal:" << maxSizes
             << minSizes << stretchFactors;
    return QVector<int>();
  }
  if (stretchFactors.isEmpty())
    return QVector<int>();
  int sectionCount = stretchFactors.size();
  QVector<double> sectionSizes(sectionCount);

  int minSizeSum = 0;
  for (int i = 0; i < sectionCount; ++i)
    minSizeSum += minSizes.at(i);
  if (totalSize < minSizeSum) {
    for (int i = 0; i < sectionCount; ++i) {
      stretchFactors[i] = minSizes.at(i);
      minSizes[i] = 0;
    }
  }

  QList<int> minimumLockedSections;
  QList<int> unfinishedSections;
  for (int i = 0; i < sectionCount; ++i)
    unfinishedSections.append(i);
  double freeSize = totalSize;

  int outerIterations = 0;
  while (!unfinishedSections.isEmpty() && outerIterations < sectionCount * 2)

  {
    ++outerIterations;
    int innerIterations = 0;
    while (!unfinishedSections.isEmpty() && innerIterations < sectionCount * 2)

    {
      ++innerIterations;

      int nextId = -1;
      double nextMax = 1e12;
      for (int i = 0; i < unfinishedSections.size(); ++i) {
        int secId = unfinishedSections.at(i);
        double hitsMaxAt = (maxSizes.at(secId) - sectionSizes.at(secId)) /
                           stretchFactors.at(secId);
        if (hitsMaxAt < nextMax) {
          nextMax = hitsMaxAt;
          nextId = secId;
        }
      }

      double stretchFactorSum = 0;
      for (int i = 0; i < unfinishedSections.size(); ++i)
        stretchFactorSum += stretchFactors.at(unfinishedSections.at(i));
      double nextMaxLimit = freeSize / stretchFactorSum;
      if (nextMax < nextMaxLimit)

      {
        for (int i = 0; i < unfinishedSections.size(); ++i) {
          sectionSizes[unfinishedSections.at(i)] +=
              nextMax * stretchFactors.at(unfinishedSections.at(i));

          freeSize -= nextMax * stretchFactors.at(unfinishedSections.at(i));
        }
        unfinishedSections.removeOne(nextId);

      } else

      {
        for (int i = 0; i < unfinishedSections.size(); ++i)
          sectionSizes[unfinishedSections.at(i)] +=
              nextMaxLimit * stretchFactors.at(unfinishedSections.at(i));

        unfinishedSections.clear();
      }
    }
    if (innerIterations == sectionCount * 2)
      qDebug() << Q_FUNC_INFO
               << "Exceeded maximum expected inner iteration count, layouting "
                  "aborted. Input was:"
               << maxSizes << minSizes << stretchFactors << totalSize;

    bool foundMinimumViolation = false;
    for (int i = 0; i < sectionSizes.size(); ++i) {
      if (minimumLockedSections.contains(i))
        continue;
      if (sectionSizes.at(i) < minSizes.at(i))

      {
        sectionSizes[i] = minSizes.at(i);

        foundMinimumViolation = true;

        minimumLockedSections.append(i);
      }
    }
    if (foundMinimumViolation) {
      freeSize = totalSize;
      for (int i = 0; i < sectionCount; ++i) {
        if (!minimumLockedSections.contains(i))

          unfinishedSections.append(i);
        else
          freeSize -= sectionSizes.at(i);
      }

      for (int i = 0; i < unfinishedSections.size(); ++i)
        sectionSizes[unfinishedSections.at(i)] = 0;
    }
  }
  if (outerIterations == sectionCount * 2)
    qDebug() << Q_FUNC_INFO
             << "Exceeded maximum expected outer iteration count, layouting "
                "aborted. Input was:"
             << maxSizes << minSizes << stretchFactors << totalSize;

  QVector<int> result(sectionCount);
  for (int i = 0; i < sectionCount; ++i)
    result[i] = qRound(sectionSizes.at(i));
  return result;
}

QSize QCPLayout::getFinalMinimumOuterSize(const QCPLayoutElement *el) {
  QSize minOuterHint = el->minimumOuterSizeHint();
  QSize minOuter = el->minimumSize();

  if (minOuter.width() > 0 &&
      el->sizeConstraintRect() == QCPLayoutElement::scrInnerRect)
    minOuter.rwidth() += el->margins().left() + el->margins().right();
  if (minOuter.height() > 0 &&
      el->sizeConstraintRect() == QCPLayoutElement::scrInnerRect)
    minOuter.rheight() += el->margins().top() + el->margins().bottom();

  return QSize(minOuter.width() > 0 ? minOuter.width() : minOuterHint.width(),
               minOuter.height() > 0 ? minOuter.height()
                                     : minOuterHint.height());
  ;
}

QSize QCPLayout::getFinalMaximumOuterSize(const QCPLayoutElement *el) {
  QSize maxOuterHint = el->maximumOuterSizeHint();
  QSize maxOuter = el->maximumSize();

  if (maxOuter.width() < QWIDGETSIZE_MAX &&
      el->sizeConstraintRect() == QCPLayoutElement::scrInnerRect)
    maxOuter.rwidth() += el->margins().left() + el->margins().right();
  if (maxOuter.height() < QWIDGETSIZE_MAX &&
      el->sizeConstraintRect() == QCPLayoutElement::scrInnerRect)
    maxOuter.rheight() += el->margins().top() + el->margins().bottom();

  return QSize(maxOuter.width() < QWIDGETSIZE_MAX ? maxOuter.width()
                                                  : maxOuterHint.width(),
               maxOuter.height() < QWIDGETSIZE_MAX ? maxOuter.height()
                                                   : maxOuterHint.height());
}

QCPLayoutGrid::QCPLayoutGrid()
    : mColumnSpacing(5), mRowSpacing(5), mWrap(0), mFillOrder(foColumnsFirst) {}

QCPLayoutGrid::~QCPLayoutGrid() { clear(); }

QCPLayoutElement *QCPLayoutGrid::element(int row, int column) const {
  if (row >= 0 && row < mElements.size()) {
    if (column >= 0 && column < mElements.first().size()) {
      if (QCPLayoutElement *result = mElements.at(row).at(column))
        return result;
      else
        qDebug() << Q_FUNC_INFO << "Requested cell is empty. Row:" << row
                 << "Column:" << column;
    } else
      qDebug() << Q_FUNC_INFO << "Invalid column. Row:" << row
               << "Column:" << column;
  } else
    qDebug() << Q_FUNC_INFO << "Invalid row. Row:" << row
             << "Column:" << column;
  return 0;
}

bool QCPLayoutGrid::addElement(int row, int column, QCPLayoutElement *element) {
  if (!hasElement(row, column)) {
    if (element && element->layout())

      element->layout()->take(element);
    expandTo(row + 1, column + 1);
    mElements[row][column] = element;
    if (element)
      adoptElement(element);
    return true;
  } else
    qDebug() << Q_FUNC_INFO
             << "There is already an element in the specified row/column:"
             << row << column;
  return false;
}

bool QCPLayoutGrid::addElement(QCPLayoutElement *element) {
  int rowIndex = 0;
  int colIndex = 0;
  if (mFillOrder == foColumnsFirst) {
    while (hasElement(rowIndex, colIndex)) {
      ++colIndex;
      if (colIndex >= mWrap && mWrap > 0) {
        colIndex = 0;
        ++rowIndex;
      }
    }
  } else {
    while (hasElement(rowIndex, colIndex)) {
      ++rowIndex;
      if (rowIndex >= mWrap && mWrap > 0) {
        rowIndex = 0;
        ++colIndex;
      }
    }
  }
  return addElement(rowIndex, colIndex, element);
}

bool QCPLayoutGrid::hasElement(int row, int column) {
  if (row >= 0 && row < rowCount() && column >= 0 && column < columnCount())
    return mElements.at(row).at(column);
  else
    return false;
}

void QCPLayoutGrid::setColumnStretchFactor(int column, double factor) {
  if (column >= 0 && column < columnCount()) {
    if (factor > 0)
      mColumnStretchFactors[column] = factor;
    else
      qDebug() << Q_FUNC_INFO
               << "Invalid stretch factor, must be positive:" << factor;
  } else
    qDebug() << Q_FUNC_INFO << "Invalid column:" << column;
}

void QCPLayoutGrid::setColumnStretchFactors(const QList<double> &factors) {
  if (factors.size() == mColumnStretchFactors.size()) {
    mColumnStretchFactors = factors;
    for (int i = 0; i < mColumnStretchFactors.size(); ++i) {
      if (mColumnStretchFactors.at(i) <= 0) {
        qDebug() << Q_FUNC_INFO << "Invalid stretch factor, must be positive:"
                 << mColumnStretchFactors.at(i);
        mColumnStretchFactors[i] = 1;
      }
    }
  } else
    qDebug() << Q_FUNC_INFO
             << "Column count not equal to passed stretch factor count:"
             << factors;
}

void QCPLayoutGrid::setRowStretchFactor(int row, double factor) {
  if (row >= 0 && row < rowCount()) {
    if (factor > 0)
      mRowStretchFactors[row] = factor;
    else
      qDebug() << Q_FUNC_INFO
               << "Invalid stretch factor, must be positive:" << factor;
  } else
    qDebug() << Q_FUNC_INFO << "Invalid row:" << row;
}

void QCPLayoutGrid::setRowStretchFactors(const QList<double> &factors) {
  if (factors.size() == mRowStretchFactors.size()) {
    mRowStretchFactors = factors;
    for (int i = 0; i < mRowStretchFactors.size(); ++i) {
      if (mRowStretchFactors.at(i) <= 0) {
        qDebug() << Q_FUNC_INFO << "Invalid stretch factor, must be positive:"
                 << mRowStretchFactors.at(i);
        mRowStretchFactors[i] = 1;
      }
    }
  } else
    qDebug() << Q_FUNC_INFO
             << "Row count not equal to passed stretch factor count:"
             << factors;
}

void QCPLayoutGrid::setColumnSpacing(int pixels) { mColumnSpacing = pixels; }

void QCPLayoutGrid::setRowSpacing(int pixels) { mRowSpacing = pixels; }

void QCPLayoutGrid::setWrap(int count) { mWrap = qMax(0, count); }

void QCPLayoutGrid::setFillOrder(FillOrder order, bool rearrange) {
  const int elCount = elementCount();
  QVector<QCPLayoutElement *> tempElements;
  if (rearrange) {
    tempElements.reserve(elCount);
    for (int i = 0; i < elCount; ++i) {
      if (elementAt(i))
        tempElements.append(takeAt(i));
    }
    simplify();
  }

  mFillOrder = order;

  if (rearrange) {
    for (int i = 0; i < tempElements.size(); ++i)
      addElement(tempElements.at(i));
  }
}

void QCPLayoutGrid::expandTo(int newRowCount, int newColumnCount) {
  while (rowCount() < newRowCount) {
    mElements.append(QList<QCPLayoutElement *>());
    mRowStretchFactors.append(1);
  }

  int newColCount = qMax(columnCount(), newColumnCount);
  for (int i = 0; i < rowCount(); ++i) {
    while (mElements.at(i).size() < newColCount)
      mElements[i].append(0);
  }
  while (mColumnStretchFactors.size() < newColCount)
    mColumnStretchFactors.append(1);
}

void QCPLayoutGrid::insertRow(int newIndex) {
  if (mElements.isEmpty() || mElements.first().isEmpty())

  {
    expandTo(1, 1);
    return;
  }

  if (newIndex < 0)
    newIndex = 0;
  if (newIndex > rowCount())
    newIndex = rowCount();

  mRowStretchFactors.insert(newIndex, 1);
  QList<QCPLayoutElement *> newRow;
  for (int col = 0; col < columnCount(); ++col)
    newRow.append((QCPLayoutElement *)0);
  mElements.insert(newIndex, newRow);
}

void QCPLayoutGrid::insertColumn(int newIndex) {
  if (mElements.isEmpty() || mElements.first().isEmpty())

  {
    expandTo(1, 1);
    return;
  }

  if (newIndex < 0)
    newIndex = 0;
  if (newIndex > columnCount())
    newIndex = columnCount();

  mColumnStretchFactors.insert(newIndex, 1);
  for (int row = 0; row < rowCount(); ++row)
    mElements[row].insert(newIndex, (QCPLayoutElement *)0);
}

int QCPLayoutGrid::rowColToIndex(int row, int column) const {
  if (row >= 0 && row < rowCount()) {
    if (column >= 0 && column < columnCount()) {
      switch (mFillOrder) {
      case foRowsFirst:
        return column * rowCount() + row;
      case foColumnsFirst:
        return row * columnCount() + column;
      }
    } else
      qDebug() << Q_FUNC_INFO << "row index out of bounds:" << row;
  } else
    qDebug() << Q_FUNC_INFO << "column index out of bounds:" << column;
  return 0;
}

void QCPLayoutGrid::indexToRowCol(int index, int &row, int &column) const {
  row = -1;
  column = -1;
  const int nCols = columnCount();
  const int nRows = rowCount();
  if (nCols == 0 || nRows == 0)
    return;
  if (index < 0 || index >= elementCount()) {
    qDebug() << Q_FUNC_INFO << "index out of bounds:" << index;
    return;
  }

  switch (mFillOrder) {
  case foRowsFirst: {
    column = index / nRows;
    row = index % nRows;
    break;
  }
  case foColumnsFirst: {
    row = index / nCols;
    column = index % nCols;
    break;
  }
  }
}

void QCPLayoutGrid::updateLayout() {
  QVector<int> minColWidths, minRowHeights, maxColWidths, maxRowHeights;
  getMinimumRowColSizes(&minColWidths, &minRowHeights);
  getMaximumRowColSizes(&maxColWidths, &maxRowHeights);

  int totalRowSpacing = (rowCount() - 1) * mRowSpacing;
  int totalColSpacing = (columnCount() - 1) * mColumnSpacing;
  QVector<int> colWidths = getSectionSizes(maxColWidths, minColWidths,
                                           mColumnStretchFactors.toVector(),
                                           mRect.width() - totalColSpacing);
  QVector<int> rowHeights = getSectionSizes(maxRowHeights, minRowHeights,
                                            mRowStretchFactors.toVector(),
                                            mRect.height() - totalRowSpacing);

  int yOffset = mRect.top();
  for (int row = 0; row < rowCount(); ++row) {
    if (row > 0)
      yOffset += rowHeights.at(row - 1) + mRowSpacing;
    int xOffset = mRect.left();
    for (int col = 0; col < columnCount(); ++col) {
      if (col > 0)
        xOffset += colWidths.at(col - 1) + mColumnSpacing;
      if (mElements.at(row).at(col))
        mElements.at(row).at(col)->setOuterRect(
            QRect(xOffset, yOffset, colWidths.at(col), rowHeights.at(row)));
    }
  }
}

QCPLayoutElement *QCPLayoutGrid::elementAt(int index) const {
  if (index >= 0 && index < elementCount()) {
    int row, col;
    indexToRowCol(index, row, col);
    return mElements.at(row).at(col);
  } else
    return 0;
}

QCPLayoutElement *QCPLayoutGrid::takeAt(int index) {
  if (QCPLayoutElement *el = elementAt(index)) {
    releaseElement(el);
    int row, col;
    indexToRowCol(index, row, col);
    mElements[row][col] = 0;
    return el;
  } else {
    qDebug() << Q_FUNC_INFO << "Attempt to take invalid index:" << index;
    return 0;
  }
}

bool QCPLayoutGrid::take(QCPLayoutElement *element) {
  if (element) {
    for (int i = 0; i < elementCount(); ++i) {
      if (elementAt(i) == element) {
        takeAt(i);
        return true;
      }
    }
    qDebug() << Q_FUNC_INFO << "Element not in this layout, couldn't take";
  } else
    qDebug() << Q_FUNC_INFO << "Can't take null element";
  return false;
}

QList<QCPLayoutElement *> QCPLayoutGrid::elements(bool recursive) const {
  QList<QCPLayoutElement *> result;
  const int elCount = elementCount();
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
  result.reserve(elCount);
#endif
  for (int i = 0; i < elCount; ++i)
    result.append(elementAt(i));
  if (recursive) {
    for (int i = 0; i < elCount; ++i) {
      if (result.at(i))
        result << result.at(i)->elements(recursive);
    }
  }
  return result;
}

void QCPLayoutGrid::simplify() {
  for (int row = rowCount() - 1; row >= 0; --row) {
    bool hasElements = false;
    for (int col = 0; col < columnCount(); ++col) {
      if (mElements.at(row).at(col)) {
        hasElements = true;
        break;
      }
    }
    if (!hasElements) {
      mRowStretchFactors.removeAt(row);
      mElements.removeAt(row);
      if (mElements.isEmpty())

        mColumnStretchFactors.clear();
    }
  }

  for (int col = columnCount() - 1; col >= 0; --col) {
    bool hasElements = false;
    for (int row = 0; row < rowCount(); ++row) {
      if (mElements.at(row).at(col)) {
        hasElements = true;
        break;
      }
    }
    if (!hasElements) {
      mColumnStretchFactors.removeAt(col);
      for (int row = 0; row < rowCount(); ++row)
        mElements[row].removeAt(col);
    }
  }
}

QSize QCPLayoutGrid::minimumOuterSizeHint() const {
  QVector<int> minColWidths, minRowHeights;
  getMinimumRowColSizes(&minColWidths, &minRowHeights);
  QSize result(0, 0);
  for (int i = 0; i < minColWidths.size(); ++i)
    result.rwidth() += minColWidths.at(i);
  for (int i = 0; i < minRowHeights.size(); ++i)
    result.rheight() += minRowHeights.at(i);
  result.rwidth() += qMax(0, columnCount() - 1) * mColumnSpacing;
  result.rheight() += qMax(0, rowCount() - 1) * mRowSpacing;
  result.rwidth() += mMargins.left() + mMargins.right();
  result.rheight() += mMargins.top() + mMargins.bottom();
  return result;
}

QSize QCPLayoutGrid::maximumOuterSizeHint() const {
  QVector<int> maxColWidths, maxRowHeights;
  getMaximumRowColSizes(&maxColWidths, &maxRowHeights);

  QSize result(0, 0);
  for (int i = 0; i < maxColWidths.size(); ++i)
    result.setWidth(qMin(result.width() + maxColWidths.at(i), QWIDGETSIZE_MAX));
  for (int i = 0; i < maxRowHeights.size(); ++i)
    result.setHeight(
        qMin(result.height() + maxRowHeights.at(i), QWIDGETSIZE_MAX));
  result.rwidth() += qMax(0, columnCount() - 1) * mColumnSpacing;
  result.rheight() += qMax(0, rowCount() - 1) * mRowSpacing;
  result.rwidth() += mMargins.left() + mMargins.right();
  result.rheight() += mMargins.top() + mMargins.bottom();
  if (result.height() > QWIDGETSIZE_MAX)
    result.setHeight(QWIDGETSIZE_MAX);
  if (result.width() > QWIDGETSIZE_MAX)
    result.setWidth(QWIDGETSIZE_MAX);
  return result;
}

void QCPLayoutGrid::getMinimumRowColSizes(QVector<int> *minColWidths,
                                          QVector<int> *minRowHeights) const {
  *minColWidths = QVector<int>(columnCount(), 0);
  *minRowHeights = QVector<int>(rowCount(), 0);
  for (int row = 0; row < rowCount(); ++row) {
    for (int col = 0; col < columnCount(); ++col) {
      if (QCPLayoutElement *el = mElements.at(row).at(col)) {
        QSize minSize = getFinalMinimumOuterSize(el);
        if (minColWidths->at(col) < minSize.width())
          (*minColWidths)[col] = minSize.width();
        if (minRowHeights->at(row) < minSize.height())
          (*minRowHeights)[row] = minSize.height();
      }
    }
  }
}

void QCPLayoutGrid::getMaximumRowColSizes(QVector<int> *maxColWidths,
                                          QVector<int> *maxRowHeights) const {
  *maxColWidths = QVector<int>(columnCount(), QWIDGETSIZE_MAX);
  *maxRowHeights = QVector<int>(rowCount(), QWIDGETSIZE_MAX);
  for (int row = 0; row < rowCount(); ++row) {
    for (int col = 0; col < columnCount(); ++col) {
      if (QCPLayoutElement *el = mElements.at(row).at(col)) {
        QSize maxSize = getFinalMaximumOuterSize(el);
        if (maxColWidths->at(col) > maxSize.width())
          (*maxColWidths)[col] = maxSize.width();
        if (maxRowHeights->at(row) > maxSize.height())
          (*maxRowHeights)[row] = maxSize.height();
      }
    }
  }
}

QCPLayoutInset::QCPLayoutInset() {}

QCPLayoutInset::~QCPLayoutInset() { clear(); }

QCPLayoutInset::InsetPlacement QCPLayoutInset::insetPlacement(int index) const {
  if (elementAt(index))
    return mInsetPlacement.at(index);
  else {
    qDebug() << Q_FUNC_INFO << "Invalid element index:" << index;
    return ipFree;
  }
}

Qt::Alignment QCPLayoutInset::insetAlignment(int index) const {
  if (elementAt(index))
    return mInsetAlignment.at(index);
  else {
    qDebug() << Q_FUNC_INFO << "Invalid element index:" << index;
    return 0;
  }
}

QRectF QCPLayoutInset::insetRect(int index) const {
  if (elementAt(index))
    return mInsetRect.at(index);
  else {
    qDebug() << Q_FUNC_INFO << "Invalid element index:" << index;
    return QRectF();
  }
}

void QCPLayoutInset::setInsetPlacement(
    int index, QCPLayoutInset::InsetPlacement placement) {
  if (elementAt(index))
    mInsetPlacement[index] = placement;
  else
    qDebug() << Q_FUNC_INFO << "Invalid element index:" << index;
}

void QCPLayoutInset::setInsetAlignment(int index, Qt::Alignment alignment) {
  if (elementAt(index))
    mInsetAlignment[index] = alignment;
  else
    qDebug() << Q_FUNC_INFO << "Invalid element index:" << index;
}

void QCPLayoutInset::setInsetRect(int index, const QRectF &rect) {
  if (elementAt(index))
    mInsetRect[index] = rect;
  else
    qDebug() << Q_FUNC_INFO << "Invalid element index:" << index;
}

void QCPLayoutInset::updateLayout() {
  for (int i = 0; i < mElements.size(); ++i) {
    QCPLayoutElement *el = mElements.at(i);
    QRect insetRect;
    QSize finalMinSize = getFinalMinimumOuterSize(el);
    QSize finalMaxSize = getFinalMaximumOuterSize(el);
    if (mInsetPlacement.at(i) == ipFree) {
      insetRect = QRect(rect().x() + rect().width() * mInsetRect.at(i).x(),
                        rect().y() + rect().height() * mInsetRect.at(i).y(),
                        rect().width() * mInsetRect.at(i).width(),
                        rect().height() * mInsetRect.at(i).height());
      if (insetRect.size().width() < finalMinSize.width())
        insetRect.setWidth(finalMinSize.width());
      if (insetRect.size().height() < finalMinSize.height())
        insetRect.setHeight(finalMinSize.height());
      if (insetRect.size().width() > finalMaxSize.width())
        insetRect.setWidth(finalMaxSize.width());
      if (insetRect.size().height() > finalMaxSize.height())
        insetRect.setHeight(finalMaxSize.height());
    } else if (mInsetPlacement.at(i) == ipBorderAligned) {
      insetRect.setSize(finalMinSize);
      Qt::Alignment al = mInsetAlignment.at(i);
      if (al.testFlag(Qt::AlignLeft))
        insetRect.moveLeft(rect().x());
      else if (al.testFlag(Qt::AlignRight))
        insetRect.moveRight(rect().x() + rect().width());
      else
        insetRect.moveLeft(rect().x() + rect().width() * 0.5 -
                           finalMinSize.width() * 0.5);

      if (al.testFlag(Qt::AlignTop))
        insetRect.moveTop(rect().y());
      else if (al.testFlag(Qt::AlignBottom))
        insetRect.moveBottom(rect().y() + rect().height());
      else
        insetRect.moveTop(rect().y() + rect().height() * 0.5 -
                          finalMinSize.height() * 0.5);
    }
    mElements.at(i)->setOuterRect(insetRect);
  }
}

int QCPLayoutInset::elementCount() const { return mElements.size(); }

QCPLayoutElement *QCPLayoutInset::elementAt(int index) const {
  if (index >= 0 && index < mElements.size())
    return mElements.at(index);
  else
    return 0;
}

QCPLayoutElement *QCPLayoutInset::takeAt(int index) {
  if (QCPLayoutElement *el = elementAt(index)) {
    releaseElement(el);
    mElements.removeAt(index);
    mInsetPlacement.removeAt(index);
    mInsetAlignment.removeAt(index);
    mInsetRect.removeAt(index);
    return el;
  } else {
    qDebug() << Q_FUNC_INFO << "Attempt to take invalid index:" << index;
    return 0;
  }
}

bool QCPLayoutInset::take(QCPLayoutElement *element) {
  if (element) {
    for (int i = 0; i < elementCount(); ++i) {
      if (elementAt(i) == element) {
        takeAt(i);
        return true;
      }
    }
    qDebug() << Q_FUNC_INFO << "Element not in this layout, couldn't take";
  } else
    qDebug() << Q_FUNC_INFO << "Can't take null element";
  return false;
}

double QCPLayoutInset::selectTest(const QPointF &pos, bool onlySelectable,
                                  QVariant *details) const {
  Q_UNUSED(details)
  if (onlySelectable)
    return -1;

  for (int i = 0; i < mElements.size(); ++i) {
    if (mElements.at(i)->realVisibility() &&
        mElements.at(i)->selectTest(pos, onlySelectable) >= 0)
      return mParentPlot->selectionTolerance() * 0.99;
  }
  return -1;
}

void QCPLayoutInset::addElement(QCPLayoutElement *element,
                                Qt::Alignment alignment) {
  if (element) {
    if (element->layout())

      element->layout()->take(element);
    mElements.append(element);
    mInsetPlacement.append(ipBorderAligned);
    mInsetAlignment.append(alignment);
    mInsetRect.append(QRectF(0.6, 0.6, 0.4, 0.4));
    adoptElement(element);
  } else
    qDebug() << Q_FUNC_INFO << "Can't add null element";
}

void QCPLayoutInset::addElement(QCPLayoutElement *element, const QRectF &rect) {
  if (element) {
    if (element->layout())

      element->layout()->take(element);
    mElements.append(element);
    mInsetPlacement.append(ipFree);
    mInsetAlignment.append(Qt::AlignRight | Qt::AlignTop);
    mInsetRect.append(rect);
    adoptElement(element);
  } else
    qDebug() << Q_FUNC_INFO << "Can't add null element";
}

QCPLineEnding::QCPLineEnding()
    : mStyle(esNone), mWidth(8), mLength(10), mInverted(false) {}

QCPLineEnding::QCPLineEnding(QCPLineEnding::EndingStyle style, double width,
                             double length, bool inverted)
    : mStyle(style), mWidth(width), mLength(length), mInverted(inverted) {}

void QCPLineEnding::setStyle(QCPLineEnding::EndingStyle style) {
  mStyle = style;
}

void QCPLineEnding::setWidth(double width) { mWidth = width; }

void QCPLineEnding::setLength(double length) { mLength = length; }

void QCPLineEnding::setInverted(bool inverted) { mInverted = inverted; }

double QCPLineEnding::boundingDistance() const {
  switch (mStyle) {
  case esNone:
    return 0;

  case esFlatArrow:
  case esSpikeArrow:
  case esLineArrow:
  case esSkewedBar:
    return qSqrt(mWidth * mWidth + mLength * mLength);

  case esDisc:
  case esSquare:
  case esDiamond:
  case esBar:
  case esHalfBar:
    return mWidth * 1.42;
  }
  return 0;
}

double QCPLineEnding::realLength() const {
  switch (mStyle) {
  case esNone:
  case esLineArrow:
  case esSkewedBar:
  case esBar:
  case esHalfBar:
    return 0;

  case esFlatArrow:
    return mLength;

  case esDisc:
  case esSquare:
  case esDiamond:
    return mWidth * 0.5;

  case esSpikeArrow:
    return mLength * 0.8;
  }
  return 0;
}

void QCPLineEnding::draw(QCPPainter *painter, const QCPVector2D &pos,
                         const QCPVector2D &dir) const {
  if (mStyle == esNone)
    return;

  QCPVector2D lengthVec = dir.normalized() * mLength * (mInverted ? -1 : 1);
  if (lengthVec.isNull())
    lengthVec = QCPVector2D(1, 0);
  QCPVector2D widthVec =
      dir.normalized().perpendicular() * mWidth * 0.5 * (mInverted ? -1 : 1);

  QPen penBackup = painter->pen();
  QBrush brushBackup = painter->brush();
  QPen miterPen = penBackup;
  miterPen.setJoinStyle(Qt::MiterJoin);

  QBrush brush(painter->pen().color(), Qt::SolidPattern);
  switch (mStyle) {
  case esNone:
    break;
  case esFlatArrow: {
    QPointF points[3] = {pos.toPointF(),
                         (pos - lengthVec + widthVec).toPointF(),
                         (pos - lengthVec - widthVec).toPointF()};
    painter->setPen(miterPen);
    painter->setBrush(brush);
    painter->drawConvexPolygon(points, 3);
    painter->setBrush(brushBackup);
    painter->setPen(penBackup);
    break;
  }
  case esSpikeArrow: {
    QPointF points[4] = {pos.toPointF(),
                         (pos - lengthVec + widthVec).toPointF(),
                         (pos - lengthVec * 0.8).toPointF(),
                         (pos - lengthVec - widthVec).toPointF()};
    painter->setPen(miterPen);
    painter->setBrush(brush);
    painter->drawConvexPolygon(points, 4);
    painter->setBrush(brushBackup);
    painter->setPen(penBackup);
    break;
  }
  case esLineArrow: {
    QPointF points[3] = {(pos - lengthVec + widthVec).toPointF(),
                         pos.toPointF(),
                         (pos - lengthVec - widthVec).toPointF()};
    painter->setPen(miterPen);
    painter->drawPolyline(points, 3);
    painter->setPen(penBackup);
    break;
  }
  case esDisc: {
    painter->setBrush(brush);
    painter->drawEllipse(pos.toPointF(), mWidth * 0.5, mWidth * 0.5);
    painter->setBrush(brushBackup);
    break;
  }
  case esSquare: {
    QCPVector2D widthVecPerp = widthVec.perpendicular();
    QPointF points[4] = {(pos - widthVecPerp + widthVec).toPointF(),
                         (pos - widthVecPerp - widthVec).toPointF(),
                         (pos + widthVecPerp - widthVec).toPointF(),
                         (pos + widthVecPerp + widthVec).toPointF()};
    painter->setPen(miterPen);
    painter->setBrush(brush);
    painter->drawConvexPolygon(points, 4);
    painter->setBrush(brushBackup);
    painter->setPen(penBackup);
    break;
  }
  case esDiamond: {
    QCPVector2D widthVecPerp = widthVec.perpendicular();
    QPointF points[4] = {
        (pos - widthVecPerp).toPointF(), (pos - widthVec).toPointF(),
        (pos + widthVecPerp).toPointF(), (pos + widthVec).toPointF()};
    painter->setPen(miterPen);
    painter->setBrush(brush);
    painter->drawConvexPolygon(points, 4);
    painter->setBrush(brushBackup);
    painter->setPen(penBackup);
    break;
  }
  case esBar: {
    painter->drawLine((pos + widthVec).toPointF(), (pos - widthVec).toPointF());
    break;
  }
  case esHalfBar: {
    painter->drawLine((pos + widthVec).toPointF(), pos.toPointF());
    break;
  }
  case esSkewedBar: {
    if (qFuzzyIsNull(painter->pen().widthF()) &&
        !painter->modes().testFlag(QCPPainter::pmNonCosmetic)) {
      painter->drawLine(
          (pos + widthVec + lengthVec * 0.2 * (mInverted ? -1 : 1)).toPointF(),
          (pos - widthVec - lengthVec * 0.2 * (mInverted ? -1 : 1)).toPointF());
    } else {
      painter->drawLine(
          (pos + widthVec + lengthVec * 0.2 * (mInverted ? -1 : 1) +
           dir.normalized() * qMax(1.0f, (float)painter->pen().widthF()) * 0.5f)
              .toPointF(),
          (pos - widthVec - lengthVec * 0.2 * (mInverted ? -1 : 1) +
           dir.normalized() * qMax(1.0f, (float)painter->pen().widthF()) * 0.5f)
              .toPointF());
    }
    break;
  }
  }
}

void QCPLineEnding::draw(QCPPainter *painter, const QCPVector2D &pos,
                         double angle) const {
  draw(painter, pos, QCPVector2D(qCos(angle), qSin(angle)));
}

QCPAxisTicker::QCPAxisTicker()
    : mTickStepStrategy(tssReadability), mTickCount(5), mTickOrigin(0) {}

QCPAxisTicker::~QCPAxisTicker() {}

void QCPAxisTicker::setTickStepStrategy(
    QCPAxisTicker::TickStepStrategy strategy) {
  mTickStepStrategy = strategy;
}

void QCPAxisTicker::setTickCount(int count) {
  if (count > 0)
    mTickCount = count;
  else
    qDebug() << Q_FUNC_INFO << "tick count must be greater than zero:" << count;
}

void QCPAxisTicker::setTickOrigin(double origin) { mTickOrigin = origin; }

void QCPAxisTicker::generate(const QCPRange &range, const QLocale &locale,
                             QChar formatChar, int precision,
                             QVector<double> &ticks, QVector<double> *subTicks,
                             QVector<QString> *tickLabels) {
  double tickStep = getTickStep(range);
  ticks = createTickVector(tickStep, range);
  trimTicks(range, ticks, true);

  if (subTicks) {
    if (ticks.size() > 0) {
      *subTicks = createSubTickVector(getSubTickCount(tickStep), ticks);
      trimTicks(range, *subTicks, false);
    } else
      *subTicks = QVector<double>();
  }

  trimTicks(range, ticks, false);

  if (tickLabels)
    *tickLabels = createLabelVector(ticks, locale, formatChar, precision);
}

double QCPAxisTicker::getTickStep(const QCPRange &range) {
  double exactStep = range.size() / (double)(mTickCount + 1e-10);

  return cleanMantissa(exactStep);
}

int QCPAxisTicker::getSubTickCount(double tickStep) {
  int result = 1;

  double epsilon = 0.01;
  double intPartf;
  int intPart;
  double fracPart = modf(getMantissa(tickStep), &intPartf);
  intPart = intPartf;

  if (fracPart < epsilon || 1.0 - fracPart < epsilon) {
    if (1.0 - fracPart < epsilon)
      ++intPart;
    switch (intPart) {
    case 1:
      result = 4;
      break;

    case 2:
      result = 3;
      break;

    case 3:
      result = 2;
      break;

    case 4:
      result = 3;
      break;

    case 5:
      result = 4;
      break;

    case 6:
      result = 2;
      break;

    case 7:
      result = 6;
      break;

    case 8:
      result = 3;
      break;

    case 9:
      result = 2;
      break;
    }
  } else {
    if (qAbs(fracPart - 0.5) < epsilon)

    {
      switch (intPart) {
      case 1:
        result = 2;
        break;

      case 2:
        result = 4;
        break;

      case 3:
        result = 4;
        break;

      case 4:
        result = 2;
        break;

      case 5:
        result = 4;
        break;

      case 6:
        result = 4;
        break;

      case 7:
        result = 2;
        break;

      case 8:
        result = 4;
        break;

      case 9:
        result = 4;
        break;
      }
    }
  }

  return result;
}

QString QCPAxisTicker::getTickLabel(double tick, const QLocale &locale,
                                    QChar formatChar, int precision) {
  return locale.toString(tick, formatChar.toLatin1(), precision);
}

QVector<double>
QCPAxisTicker::createSubTickVector(int subTickCount,
                                   const QVector<double> &ticks) {
  QVector<double> result;
  if (subTickCount <= 0 || ticks.size() < 2)
    return result;

  result.reserve((ticks.size() - 1) * subTickCount);
  for (int i = 1; i < ticks.size(); ++i) {
    double subTickStep =
        (ticks.at(i) - ticks.at(i - 1)) / (double)(subTickCount + 1);
    for (int k = 1; k <= subTickCount; ++k)
      result.append(ticks.at(i - 1) + k * subTickStep);
  }
  return result;
}

QVector<double> QCPAxisTicker::createTickVector(double tickStep,
                                                const QCPRange &range) {
  QVector<double> result;

  qint64 firstStep = floor((range.lower - mTickOrigin) / tickStep);

  qint64 lastStep = ceil((range.upper - mTickOrigin) / tickStep);

  int tickcount = lastStep - firstStep + 1;
  if (tickcount < 0)
    tickcount = 0;
  result.resize(tickcount);
  for (int i = 0; i < tickcount; ++i)
    result[i] = mTickOrigin + (firstStep + i) * tickStep;
  return result;
}

QVector<QString> QCPAxisTicker::createLabelVector(const QVector<double> &ticks,
                                                  const QLocale &locale,
                                                  QChar formatChar,
                                                  int precision) {
  QVector<QString> result;
  result.reserve(ticks.size());
  for (int i = 0; i < ticks.size(); ++i)
    result.append(getTickLabel(ticks.at(i), locale, formatChar, precision));
  return result;
}

void QCPAxisTicker::trimTicks(const QCPRange &range, QVector<double> &ticks,
                              bool keepOneOutlier) const {
  bool lowFound = false;
  bool highFound = false;
  int lowIndex = 0;
  int highIndex = -1;

  for (int i = 0; i < ticks.size(); ++i) {
    if (ticks.at(i) >= range.lower) {
      lowFound = true;
      lowIndex = i;
      break;
    }
  }
  for (int i = ticks.size() - 1; i >= 0; --i) {
    if (ticks.at(i) <= range.upper) {
      highFound = true;
      highIndex = i;
      break;
    }
  }

  if (highFound && lowFound) {
    int trimFront = qMax(0, lowIndex - (keepOneOutlier ? 1 : 0));
    int trimBack = qMax(0, ticks.size() - (keepOneOutlier ? 2 : 1) - highIndex);
    if (trimFront > 0 || trimBack > 0)
      ticks = ticks.mid(trimFront, ticks.size() - trimFront - trimBack);
  } else

    ticks.clear();
}

double QCPAxisTicker::pickClosest(double target,
                                  const QVector<double> &candidates) const {
  if (candidates.size() == 1)
    return candidates.first();
  QVector<double>::const_iterator it =
      std::lower_bound(candidates.constBegin(), candidates.constEnd(), target);
  if (it == candidates.constEnd())
    return *(it - 1);
  else if (it == candidates.constBegin())
    return *it;
  else
    return target - *(it - 1) < *it - target ? *(it - 1) : *it;
}

double QCPAxisTicker::getMantissa(double input, double *magnitude) const {
  const double mag = qPow(10.0, qFloor(qLn(input) / qLn(10.0)));
  if (magnitude)
    *magnitude = mag;
  return input / mag;
}

double QCPAxisTicker::cleanMantissa(double input) const {
  double magnitude;
  const double mantissa = getMantissa(input, &magnitude);
  switch (mTickStepStrategy) {
  case tssReadability: {
    return pickClosest(mantissa, QVector<double>()
                                     << 1.0 << 2.0 << 2.5 << 5.0 << 10.0) *
           magnitude;
  }
  case tssMeetTickCount: {
    if (mantissa <= 5.0)
      return (int)(mantissa * 2) / 2.0 * magnitude;

    else
      return (int)(mantissa / 2.0) * 2.0 * magnitude;
  }
  }
  return input;
}

QCPAxisTickerDateTime::QCPAxisTickerDateTime()
    : mDateTimeFormat(QLatin1String("hh:mm:ss\ndd.MM.yy")),
      mDateTimeSpec(Qt::LocalTime), mDateStrategy(dsNone) {
  setTickCount(4);
}

void QCPAxisTickerDateTime::setDateTimeFormat(const QString &format) {
  mDateTimeFormat = format;
}

void QCPAxisTickerDateTime::setDateTimeSpec(Qt::TimeSpec spec) {
  mDateTimeSpec = spec;
}

void QCPAxisTickerDateTime::setTickOrigin(double origin) {
  QCPAxisTicker::setTickOrigin(origin);
}

void QCPAxisTickerDateTime::setTickOrigin(const QDateTime &origin) {
  setTickOrigin(dateTimeToKey(origin));
}

double QCPAxisTickerDateTime::getTickStep(const QCPRange &range) {
  double result = range.size() / (double)(mTickCount + 1e-10);

  mDateStrategy = dsNone;
  if (result < 1)

  {
    result = cleanMantissa(result);
  } else if (result < 86400 * 30.4375 * 12)

  {
    result = pickClosest(
        result, QVector<double>()
                    << 1 << 2.5 << 5 << 10 << 15 << 30 << 60 << 2.5 * 60
                    << 5 * 60 << 10 * 60 << 15 * 60 << 30 * 60 << 60 * 60

                    << 3600 * 2 << 3600 * 3 << 3600 * 6 << 3600 * 12
                    << 3600 * 24

                    << 86400 * 2 << 86400 * 5 << 86400 * 7 << 86400 * 14
                    << 86400 * 30.4375 << 86400 * 30.4375 * 2
                    << 86400 * 30.4375 * 3 << 86400 * 30.4375 * 6
                    << 86400 * 30.4375 * 12);

    if (result > 86400 * 30.4375 - 1)

      mDateStrategy = dsUniformDayInMonth;
    else if (result > 3600 * 24 - 1)

      mDateStrategy = dsUniformTimeInDay;
  } else

  {
    const double secondsPerYear = 86400 * 30.4375 * 12;

    result = cleanMantissa(result / secondsPerYear) * secondsPerYear;
    mDateStrategy = dsUniformDayInMonth;
  }
  return result;
}

int QCPAxisTickerDateTime::getSubTickCount(double tickStep) {
  int result = QCPAxisTicker::getSubTickCount(tickStep);
  switch (qRound(tickStep))

  {
  case 5 * 60:
    result = 4;
    break;
  case 10 * 60:
    result = 1;
    break;
  case 15 * 60:
    result = 2;
    break;
  case 30 * 60:
    result = 1;
    break;
  case 60 * 60:
    result = 3;
    break;
  case 3600 * 2:
    result = 3;
    break;
  case 3600 * 3:
    result = 2;
    break;
  case 3600 * 6:
    result = 1;
    break;
  case 3600 * 12:
    result = 3;
    break;
  case 3600 * 24:
    result = 3;
    break;
  case 86400 * 2:
    result = 1;
    break;
  case 86400 * 5:
    result = 4;
    break;
  case 86400 * 7:
    result = 6;
    break;
  case 86400 * 14:
    result = 1;
    break;
  case (int)(86400 * 30.4375 + 0.5):
    result = 3;
    break;
  case (int)(86400 * 30.4375 * 2 + 0.5):
    result = 1;
    break;
  case (int)(86400 * 30.4375 * 3 + 0.5):
    result = 2;
    break;
  case (int)(86400 * 30.4375 * 6 + 0.5):
    result = 5;
    break;
  case (int)(86400 * 30.4375 * 12 + 0.5):
    result = 3;
    break;
  }
  return result;
}

QString QCPAxisTickerDateTime::getTickLabel(double tick, const QLocale &locale,
                                            QChar formatChar, int precision) {
  Q_UNUSED(precision)
  Q_UNUSED(formatChar)
  return locale.toString(keyToDateTime(tick).toTimeSpec(mDateTimeSpec),
                         mDateTimeFormat);
}

QVector<double> QCPAxisTickerDateTime::createTickVector(double tickStep,
                                                        const QCPRange &range) {
  QVector<double> result = QCPAxisTicker::createTickVector(tickStep, range);
  if (!result.isEmpty()) {
    if (mDateStrategy == dsUniformTimeInDay) {
      QDateTime uniformDateTime = keyToDateTime(mTickOrigin);

      QDateTime tickDateTime;
      for (int i = 0; i < result.size(); ++i) {
        tickDateTime = keyToDateTime(result.at(i));
        tickDateTime.setTime(uniformDateTime.time());
        result[i] = dateTimeToKey(tickDateTime);
      }
    } else if (mDateStrategy == dsUniformDayInMonth) {
      QDateTime uniformDateTime = keyToDateTime(mTickOrigin);

      QDateTime tickDateTime;
      for (int i = 0; i < result.size(); ++i) {
        tickDateTime = keyToDateTime(result.at(i));
        tickDateTime.setTime(uniformDateTime.time());
        int thisUniformDay =
            uniformDateTime.date().day() <= tickDateTime.date().daysInMonth()
                ? uniformDateTime.date().day()
                : tickDateTime.date().daysInMonth();

        if (thisUniformDay - tickDateTime.date().day() < -15)

          tickDateTime = tickDateTime.addMonths(1);
        else if (thisUniformDay - tickDateTime.date().day() > 15)

          tickDateTime = tickDateTime.addMonths(-1);
        tickDateTime.setDate(QDate(tickDateTime.date().year(),
                                   tickDateTime.date().month(),
                                   thisUniformDay));
        result[i] = dateTimeToKey(tickDateTime);
      }
    }
  }
  return result;
}

QDateTime QCPAxisTickerDateTime::keyToDateTime(double key) {
#if QT_VERSION < QT_VERSION_CHECK(4, 7, 0)
  return QDateTime::fromTime_t(key).addMSecs((key - (qint64)key) * 1000);
#else
  return QDateTime::fromMSecsSinceEpoch(key * 1000.0);
#endif
}

double QCPAxisTickerDateTime::dateTimeToKey(const QDateTime dateTime) {
#if QT_VERSION < QT_VERSION_CHECK(4, 7, 0)
  return dateTime.toTime_t() + dateTime.time().msec() / 1000.0;
#else
  return dateTime.toMSecsSinceEpoch() / 1000.0;
#endif
}

double QCPAxisTickerDateTime::dateTimeToKey(const QDate date) {
#if QT_VERSION < QT_VERSION_CHECK(4, 7, 0)
  return QDateTime(date).toTime_t();
#else
  return QDateTime(date).toMSecsSinceEpoch() / 1000.0;
#endif
}

QCPAxisTickerTime::QCPAxisTickerTime()
    : mTimeFormat(QLatin1String("%h:%m:%s")), mSmallestUnit(tuSeconds),
      mBiggestUnit(tuHours) {
  setTickCount(4);
  mFieldWidth[tuMilliseconds] = 3;
  mFieldWidth[tuSeconds] = 2;
  mFieldWidth[tuMinutes] = 2;
  mFieldWidth[tuHours] = 2;
  mFieldWidth[tuDays] = 1;

  mFormatPattern[tuMilliseconds] = QLatin1String("%z");
  mFormatPattern[tuSeconds] = QLatin1String("%s");
  mFormatPattern[tuMinutes] = QLatin1String("%m");
  mFormatPattern[tuHours] = QLatin1String("%h");
  mFormatPattern[tuDays] = QLatin1String("%d");
}

void QCPAxisTickerTime::setTimeFormat(const QString &format) {
  mTimeFormat = format;

  mSmallestUnit = tuMilliseconds;
  mBiggestUnit = tuMilliseconds;
  bool hasSmallest = false;
  for (int i = tuMilliseconds; i <= tuDays; ++i) {
    TimeUnit unit = static_cast<TimeUnit>(i);
    if (mTimeFormat.contains(mFormatPattern.value(unit))) {
      if (!hasSmallest) {
        mSmallestUnit = unit;
        hasSmallest = true;
      }
      mBiggestUnit = unit;
    }
  }
}

void QCPAxisTickerTime::setFieldWidth(QCPAxisTickerTime::TimeUnit unit,
                                      int width) {
  mFieldWidth[unit] = qMax(width, 1);
}

double QCPAxisTickerTime::getTickStep(const QCPRange &range) {
  double result = range.size() / (double)(mTickCount + 1e-10);

  if (result < 1)

  {
    if (mSmallestUnit == tuMilliseconds)
      result = qMax(cleanMantissa(result), 0.001);

    else

      result = 1.0;
  } else if (result < 3600 * 24)

  {
    QVector<double> availableSteps;

    if (mSmallestUnit <= tuSeconds)
      availableSteps << 1;
    if (mSmallestUnit == tuMilliseconds)
      availableSteps << 2.5;

    else if (mSmallestUnit == tuSeconds)
      availableSteps << 2;
    if (mSmallestUnit <= tuSeconds)
      availableSteps << 5 << 10 << 15 << 30;

    if (mSmallestUnit <= tuMinutes)
      availableSteps << 1 * 60;
    if (mSmallestUnit <= tuSeconds)
      availableSteps << 2.5 * 60;

    else if (mSmallestUnit == tuMinutes)
      availableSteps << 2 * 60;
    if (mSmallestUnit <= tuMinutes)
      availableSteps << 5 * 60 << 10 * 60 << 15 * 60 << 30 * 60;

    if (mSmallestUnit <= tuHours)
      availableSteps << 1 * 3600 << 2 * 3600 << 3 * 3600 << 6 * 3600
                     << 12 * 3600 << 24 * 3600;

    result = pickClosest(result, availableSteps);
  } else

  {
    const double secondsPerDay = 3600 * 24;
    result = cleanMantissa(result / secondsPerDay) * secondsPerDay;
  }
  return result;
}

int QCPAxisTickerTime::getSubTickCount(double tickStep) {
  int result = QCPAxisTicker::getSubTickCount(tickStep);
  switch (qRound(tickStep))

  {
  case 5 * 60:
    result = 4;
    break;
  case 10 * 60:
    result = 1;
    break;
  case 15 * 60:
    result = 2;
    break;
  case 30 * 60:
    result = 1;
    break;
  case 60 * 60:
    result = 3;
    break;
  case 3600 * 2:
    result = 3;
    break;
  case 3600 * 3:
    result = 2;
    break;
  case 3600 * 6:
    result = 1;
    break;
  case 3600 * 12:
    result = 3;
    break;
  case 3600 * 24:
    result = 3;
    break;
  }
  return result;
}

QString QCPAxisTickerTime::getTickLabel(double tick, const QLocale &locale,
                                        QChar formatChar, int precision) {
  Q_UNUSED(precision)
  Q_UNUSED(formatChar)
  Q_UNUSED(locale)
  bool negative = tick < 0;
  if (negative)
    tick *= -1;
  double values[tuDays + 1];

  double restValues[tuDays + 1];

  restValues[tuMilliseconds] = tick * 1000;
  values[tuMilliseconds] =
      modf(restValues[tuMilliseconds] / 1000, &restValues[tuSeconds]) * 1000;
  values[tuSeconds] =
      modf(restValues[tuSeconds] / 60, &restValues[tuMinutes]) * 60;
  values[tuMinutes] =
      modf(restValues[tuMinutes] / 60, &restValues[tuHours]) * 60;
  values[tuHours] = modf(restValues[tuHours] / 24, &restValues[tuDays]) * 24;

  QString result = mTimeFormat;
  for (int i = mSmallestUnit; i <= mBiggestUnit; ++i) {
    TimeUnit iUnit = static_cast<TimeUnit>(i);
    replaceUnit(
        result, iUnit,
        qRound(iUnit == mBiggestUnit ? restValues[iUnit] : values[iUnit]));
  }
  if (negative)
    result.prepend(QLatin1Char('-'));
  return result;
}

void QCPAxisTickerTime::replaceUnit(QString &text,
                                    QCPAxisTickerTime::TimeUnit unit,
                                    int value) const {
  QString valueStr = QString::number(value);
  while (valueStr.size() < mFieldWidth.value(unit))
    valueStr.prepend(QLatin1Char('0'));

  text.replace(mFormatPattern.value(unit), valueStr);
}

QCPAxisTickerFixed::QCPAxisTickerFixed()
    : mTickStep(1.0), mScaleStrategy(ssNone) {}

void QCPAxisTickerFixed::setTickStep(double step) {
  if (step > 0)
    mTickStep = step;
  else
    qDebug() << Q_FUNC_INFO << "tick step must be greater than zero:" << step;
}

void QCPAxisTickerFixed::setScaleStrategy(
    QCPAxisTickerFixed::ScaleStrategy strategy) {
  mScaleStrategy = strategy;
}

double QCPAxisTickerFixed::getTickStep(const QCPRange &range) {
  switch (mScaleStrategy) {
  case ssNone: {
    return mTickStep;
  }
  case ssMultiples: {
    double exactStep = range.size() / (double)(mTickCount + 1e-10);

    if (exactStep < mTickStep)
      return mTickStep;
    else
      return (qint64)(cleanMantissa(exactStep / mTickStep) + 0.5) * mTickStep;
  }
  case ssPowers: {
    double exactStep = range.size() / (double)(mTickCount + 1e-10);

    return qPow(mTickStep, (int)(qLn(exactStep) / qLn(mTickStep) + 0.5));
  }
  }
  return mTickStep;
}

QCPAxisTickerText::QCPAxisTickerText() : mSubTickCount(0) {}

void QCPAxisTickerText::setTicks(const QMap<double, QString> &ticks) {
  mTicks = ticks;
}

void QCPAxisTickerText::setTicks(const QVector<double> &positions,
                                 const QVector<QString> &labels) {
  clear();
  addTicks(positions, labels);
}

void QCPAxisTickerText::setSubTickCount(int subTicks) {
  if (subTicks >= 0)
    mSubTickCount = subTicks;
  else
    qDebug() << Q_FUNC_INFO << "sub tick count can't be negative:" << subTicks;
}

void QCPAxisTickerText::clear() { mTicks.clear(); }

void QCPAxisTickerText::addTick(double position, const QString &label) {
  mTicks.insert(position, label);
}

void QCPAxisTickerText::addTicks(const QMap<double, QString> &ticks) {
  mTicks.unite(ticks);
}

void QCPAxisTickerText::addTicks(const QVector<double> &positions,
                                 const QVector<QString> &labels) {
  if (positions.size() != labels.size())
    qDebug() << Q_FUNC_INFO
             << "passed unequal length vectors for positions and labels:"
             << positions.size() << labels.size();
  int n = qMin(positions.size(), labels.size());
  for (int i = 0; i < n; ++i)
    mTicks.insert(positions.at(i), labels.at(i));
}

double QCPAxisTickerText::getTickStep(const QCPRange &range) {
  Q_UNUSED(range)
  return 1.0;
}

int QCPAxisTickerText::getSubTickCount(double tickStep) {
  Q_UNUSED(tickStep)
  return mSubTickCount;
}

QString QCPAxisTickerText::getTickLabel(double tick, const QLocale &locale,
                                        QChar formatChar, int precision) {
  Q_UNUSED(locale)
  Q_UNUSED(formatChar)
  Q_UNUSED(precision)
  return mTicks.value(tick);
}

QVector<double> QCPAxisTickerText::createTickVector(double tickStep,
                                                    const QCPRange &range) {
  Q_UNUSED(tickStep)
  QVector<double> result;
  if (mTicks.isEmpty())
    return result;

  QMap<double, QString>::const_iterator start = mTicks.lowerBound(range.lower);
  QMap<double, QString>::const_iterator end = mTicks.upperBound(range.upper);

  if (start != mTicks.constBegin())
    --start;
  if (end != mTicks.constEnd())
    ++end;
  for (QMap<double, QString>::const_iterator it = start; it != end; ++it)
    result.append(it.key());

  return result;
}

QCPAxisTickerPi::QCPAxisTickerPi()
    : mPiSymbol(QLatin1String(" ") + QChar(0x03C0)), mPiValue(M_PI),
      mPeriodicity(0), mFractionStyle(fsUnicodeFractions), mPiTickStep(0) {
  setTickCount(4);
}

void QCPAxisTickerPi::setPiSymbol(QString symbol) { mPiSymbol = symbol; }

void QCPAxisTickerPi::setPiValue(double pi) { mPiValue = pi; }

void QCPAxisTickerPi::setPeriodicity(int multiplesOfPi) {
  mPeriodicity = qAbs(multiplesOfPi);
}

void QCPAxisTickerPi::setFractionStyle(QCPAxisTickerPi::FractionStyle style) {
  mFractionStyle = style;
}

double QCPAxisTickerPi::getTickStep(const QCPRange &range) {
  mPiTickStep = range.size() / mPiValue / (double)(mTickCount + 1e-10);

  mPiTickStep = cleanMantissa(mPiTickStep);
  return mPiTickStep * mPiValue;
}

int QCPAxisTickerPi::getSubTickCount(double tickStep) {
  return QCPAxisTicker::getSubTickCount(tickStep / mPiValue);
}

QString QCPAxisTickerPi::getTickLabel(double tick, const QLocale &locale,
                                      QChar formatChar, int precision) {
  double tickInPis = tick / mPiValue;
  if (mPeriodicity > 0)
    tickInPis = fmod(tickInPis, mPeriodicity);

  if (mFractionStyle != fsFloatingPoint && mPiTickStep > 0.09 &&
      mPiTickStep < 50) {
    int denominator = 1000;
    int numerator = qRound(tickInPis * denominator);
    simplifyFraction(numerator, denominator);
    if (qAbs(numerator) == 1 && denominator == 1)
      return (numerator < 0 ? QLatin1String("-") : QLatin1String("")) +
             mPiSymbol.trimmed();
    else if (numerator == 0)
      return QLatin1String("0");
    else
      return fractionToString(numerator, denominator) + mPiSymbol;
  } else {
    if (qFuzzyIsNull(tickInPis))
      return QLatin1String("0");
    else if (qFuzzyCompare(qAbs(tickInPis), 1.0))
      return (tickInPis < 0 ? QLatin1String("-") : QLatin1String("")) +
             mPiSymbol.trimmed();
    else
      return QCPAxisTicker::getTickLabel(tickInPis, locale, formatChar,
                                         precision) +
             mPiSymbol;
  }
}

void QCPAxisTickerPi::simplifyFraction(int &numerator, int &denominator) const {
  if (numerator == 0 || denominator == 0)
    return;

  int num = numerator;
  int denom = denominator;
  while (denom != 0)

  {
    int oldDenom = denom;
    denom = num % denom;
    num = oldDenom;
  }

  numerator /= num;
  denominator /= num;
}

QString QCPAxisTickerPi::fractionToString(int numerator,
                                          int denominator) const {
  if (denominator == 0) {
    qDebug() << Q_FUNC_INFO << "called with zero denominator";
    return QString();
  }
  if (mFractionStyle == fsFloatingPoint)

  {
    qDebug() << Q_FUNC_INFO
             << "shouldn't be called with fraction style fsDecimal";
    return QString::number(numerator / (double)denominator);
  }
  int sign = numerator * denominator < 0 ? -1 : 1;
  numerator = qAbs(numerator);
  denominator = qAbs(denominator);

  if (denominator == 1) {
    return QString::number(sign * numerator);
  } else {
    int integerPart = numerator / denominator;
    int remainder = numerator % denominator;
    if (remainder == 0) {
      return QString::number(sign * integerPart);
    } else {
      if (mFractionStyle == fsAsciiFractions) {
        return QString(QLatin1String("%1%2%3/%4"))
            .arg(sign == -1 ? QLatin1String("-") : QLatin1String(""))
            .arg(integerPart > 0
                     ? QString::number(integerPart) + QLatin1String(" ")
                     : QLatin1String(""))
            .arg(remainder)
            .arg(denominator);
      } else if (mFractionStyle == fsUnicodeFractions) {
        return QString(QLatin1String("%1%2%3"))
            .arg(sign == -1 ? QLatin1String("-") : QLatin1String(""))
            .arg(integerPart > 0 ? QString::number(integerPart)
                                 : QLatin1String(""))
            .arg(unicodeFraction(remainder, denominator));
      }
    }
  }
  return QString();
}

QString QCPAxisTickerPi::unicodeFraction(int numerator, int denominator) const {
  return unicodeSuperscript(numerator) + QChar(0x2044) +
         unicodeSubscript(denominator);
}

QString QCPAxisTickerPi::unicodeSuperscript(int number) const {
  if (number == 0)
    return QString(QChar(0x2070));

  QString result;
  while (number > 0) {
    const int digit = number % 10;
    switch (digit) {
    case 1: {
      result.prepend(QChar(0x00B9));
      break;
    }
    case 2: {
      result.prepend(QChar(0x00B2));
      break;
    }
    case 3: {
      result.prepend(QChar(0x00B3));
      break;
    }
    default: {
      result.prepend(QChar(0x2070 + digit));
      break;
    }
    }
    number /= 10;
  }
  return result;
}

QString QCPAxisTickerPi::unicodeSubscript(int number) const {
  if (number == 0)
    return QString(QChar(0x2080));

  QString result;
  while (number > 0) {
    result.prepend(QChar(0x2080 + number % 10));
    number /= 10;
  }
  return result;
}

QCPAxisTickerLog::QCPAxisTickerLog()
    : mLogBase(10.0), mSubTickCount(8),

      mLogBaseLnInv(1.0 / qLn(mLogBase)) {}

void QCPAxisTickerLog::setLogBase(double base) {
  if (base > 0) {
    mLogBase = base;
    mLogBaseLnInv = 1.0 / qLn(mLogBase);
  } else
    qDebug() << Q_FUNC_INFO << "log base has to be greater than zero:" << base;
}

void QCPAxisTickerLog::setSubTickCount(int subTicks) {
  if (subTicks >= 0)
    mSubTickCount = subTicks;
  else
    qDebug() << Q_FUNC_INFO << "sub tick count can't be negative:" << subTicks;
}

double QCPAxisTickerLog::getTickStep(const QCPRange &range) {
  Q_UNUSED(range)
  return 1.0;
}

int QCPAxisTickerLog::getSubTickCount(double tickStep) {
  Q_UNUSED(tickStep)
  return mSubTickCount;
}

QVector<double> QCPAxisTickerLog::createTickVector(double tickStep,
                                                   const QCPRange &range) {
  Q_UNUSED(tickStep)
  QVector<double> result;
  if (range.lower > 0 && range.upper > 0)

  {
    double exactPowerStep = qLn(range.upper / range.lower) * mLogBaseLnInv /
                            (double)(mTickCount + 1e-10);
    double newLogBase =
        qPow(mLogBase, qMax((int)cleanMantissa(exactPowerStep), 1));
    double currentTick =
        qPow(newLogBase, qFloor(qLn(range.lower) / qLn(newLogBase)));
    result.append(currentTick);
    while (currentTick < range.upper && currentTick > 0)

    {
      currentTick *= newLogBase;
      result.append(currentTick);
    }
  } else if (range.lower < 0 && range.upper < 0)

  {
    double exactPowerStep = qLn(range.lower / range.upper) * mLogBaseLnInv /
                            (double)(mTickCount + 1e-10);
    double newLogBase =
        qPow(mLogBase, qMax((int)cleanMantissa(exactPowerStep), 1));
    double currentTick =
        -qPow(newLogBase, qCeil(qLn(-range.lower) / qLn(newLogBase)));
    result.append(currentTick);
    while (currentTick < range.upper && currentTick < 0)

    {
      currentTick /= newLogBase;
      result.append(currentTick);
    }
  } else

  {
    qDebug() << Q_FUNC_INFO
             << "Invalid range for logarithmic plot: " << range.lower << ".."
             << range.upper;
  }

  return result;
}

QCPGrid::QCPGrid(QCPAxis *parentAxis)
    : QCPLayerable(parentAxis->parentPlot(), QString(), parentAxis),
      mParentAxis(parentAxis) {
  setParent(parentAxis);
  setPen(QPen(QColor(200, 200, 200), 0, Qt::DotLine));
  setSubGridPen(QPen(QColor(220, 220, 220), 0, Qt::DotLine));
  setZeroLinePen(QPen(QColor(200, 200, 200), 0, Qt::SolidLine));
  setSubGridVisible(false);
  setAntialiased(false);
  setAntialiasedSubGrid(false);
  setAntialiasedZeroLine(false);
}

void QCPGrid::setSubGridVisible(bool visible) { mSubGridVisible = visible; }

void QCPGrid::setAntialiasedSubGrid(bool enabled) {
  mAntialiasedSubGrid = enabled;
}

void QCPGrid::setAntialiasedZeroLine(bool enabled) {
  mAntialiasedZeroLine = enabled;
}

void QCPGrid::setPen(const QPen &pen) { mPen = pen; }

void QCPGrid::setSubGridPen(const QPen &pen) { mSubGridPen = pen; }

void QCPGrid::setZeroLinePen(const QPen &pen) { mZeroLinePen = pen; }

void QCPGrid::applyDefaultAntialiasingHint(QCPPainter *painter) const {
  applyAntialiasingHint(painter, mAntialiased, QCP::aeGrid);
}

void QCPGrid::draw(QCPPainter *painter) {
  if (!mParentAxis) {
    qDebug() << Q_FUNC_INFO << "invalid parent axis";
    return;
  }

  if (mParentAxis->subTicks() && mSubGridVisible)
    drawSubGridLines(painter);
  drawGridLines(painter);
}

void QCPGrid::drawGridLines(QCPPainter *painter) const {
  if (!mParentAxis) {
    qDebug() << Q_FUNC_INFO << "invalid parent axis";
    return;
  }

  const int tickCount = mParentAxis->mTickVector.size();
  double t;

  if (mParentAxis->orientation() == Qt::Horizontal) {
    int zeroLineIndex = -1;
    if (mZeroLinePen.style() != Qt::NoPen && mParentAxis->mRange.lower < 0 &&
        mParentAxis->mRange.upper > 0) {
      applyAntialiasingHint(painter, mAntialiasedZeroLine, QCP::aeZeroLine);
      painter->setPen(mZeroLinePen);
      double epsilon = mParentAxis->range().size() * 1E-6;

      for (int i = 0; i < tickCount; ++i) {
        if (qAbs(mParentAxis->mTickVector.at(i)) < epsilon) {
          zeroLineIndex = i;
          t = mParentAxis->coordToPixel(mParentAxis->mTickVector.at(i));

          painter->drawLine(QLineF(t, mParentAxis->mAxisRect->bottom(), t,
                                   mParentAxis->mAxisRect->top()));
          break;
        }
      }
    }

    applyDefaultAntialiasingHint(painter);
    painter->setPen(mPen);
    for (int i = 0; i < tickCount; ++i) {
      if (i == zeroLineIndex)
        continue;

      t = mParentAxis->coordToPixel(mParentAxis->mTickVector.at(i));

      painter->drawLine(QLineF(t, mParentAxis->mAxisRect->bottom(), t,
                               mParentAxis->mAxisRect->top()));
    }
  } else {
    int zeroLineIndex = -1;
    if (mZeroLinePen.style() != Qt::NoPen && mParentAxis->mRange.lower < 0 &&
        mParentAxis->mRange.upper > 0) {
      applyAntialiasingHint(painter, mAntialiasedZeroLine, QCP::aeZeroLine);
      painter->setPen(mZeroLinePen);
      double epsilon = mParentAxis->mRange.size() * 1E-6;

      for (int i = 0; i < tickCount; ++i) {
        if (qAbs(mParentAxis->mTickVector.at(i)) < epsilon) {
          zeroLineIndex = i;
          t = mParentAxis->coordToPixel(mParentAxis->mTickVector.at(i));

          painter->drawLine(QLineF(mParentAxis->mAxisRect->left(), t,
                                   mParentAxis->mAxisRect->right(), t));
          break;
        }
      }
    }

    applyDefaultAntialiasingHint(painter);
    painter->setPen(mPen);
    for (int i = 0; i < tickCount; ++i) {
      if (i == zeroLineIndex)
        continue;

      t = mParentAxis->coordToPixel(mParentAxis->mTickVector.at(i));

      painter->drawLine(QLineF(mParentAxis->mAxisRect->left(), t,
                               mParentAxis->mAxisRect->right(), t));
    }
  }
}

void QCPGrid::drawSubGridLines(QCPPainter *painter) const {
  if (!mParentAxis) {
    qDebug() << Q_FUNC_INFO << "invalid parent axis";
    return;
  }

  applyAntialiasingHint(painter, mAntialiasedSubGrid, QCP::aeSubGrid);
  double t;

  painter->setPen(mSubGridPen);
  if (mParentAxis->orientation() == Qt::Horizontal) {
    for (int i = 0; i < mParentAxis->mSubTickVector.size(); ++i) {
      t = mParentAxis->coordToPixel(mParentAxis->mSubTickVector.at(i));

      painter->drawLine(QLineF(t, mParentAxis->mAxisRect->bottom(), t,
                               mParentAxis->mAxisRect->top()));
    }
  } else {
    for (int i = 0; i < mParentAxis->mSubTickVector.size(); ++i) {
      t = mParentAxis->coordToPixel(mParentAxis->mSubTickVector.at(i));

      painter->drawLine(QLineF(mParentAxis->mAxisRect->left(), t,
                               mParentAxis->mAxisRect->right(), t));
    }
  }
}

QCPAxis::QCPAxis(QCPAxisRect *parent, AxisType type)
    : QCPLayerable(parent->parentPlot(), QString(), parent),

      mAxisType(type), mAxisRect(parent), mPadding(5),
      mOrientation(orientation(type)),
      mSelectableParts(spAxis | spTickLabels | spAxisLabel),
      mSelectedParts(spNone),
      mBasePen(QPen(Qt::black, 0, Qt::SolidLine, Qt::SquareCap)),
      mSelectedBasePen(QPen(Qt::blue, 2)),

      mLabel(), mLabelFont(mParentPlot->font()),
      mSelectedLabelFont(
          QFont(mLabelFont.family(), mLabelFont.pointSize(), QFont::Bold)),
      mLabelColor(Qt::black), mSelectedLabelColor(Qt::blue),

      mTickLabels(true), mTickLabelFont(mParentPlot->font()),
      mSelectedTickLabelFont(QFont(mTickLabelFont.family(),
                                   mTickLabelFont.pointSize(), QFont::Bold)),
      mTickLabelColor(Qt::black), mSelectedTickLabelColor(Qt::blue),
      mNumberPrecision(6), mNumberFormatChar('g'), mNumberBeautifulPowers(true),

      mTicks(true), mSubTicks(true),
      mTickPen(QPen(Qt::black, 0, Qt::SolidLine, Qt::SquareCap)),
      mSelectedTickPen(QPen(Qt::blue, 2)),
      mSubTickPen(QPen(Qt::black, 0, Qt::SolidLine, Qt::SquareCap)),
      mSelectedSubTickPen(QPen(Qt::blue, 2)),

      mRange(0, 5), mRangeReversed(false), mScaleType(stLinear),

      mGrid(new QCPGrid(this)),
      mAxisPainter(new QCPAxisPainterPrivate(parent->parentPlot())),
      mTicker(new QCPAxisTicker), mCachedMarginValid(false), mCachedMargin(0) {
  setParent(parent);
  mGrid->setVisible(false);
  setAntialiased(false);
  setLayer(mParentPlot->currentLayer());

  if (type == atTop) {
    setTickLabelPadding(3);
    setLabelPadding(6);
  } else if (type == atRight) {
    setTickLabelPadding(7);
    setLabelPadding(12);
  } else if (type == atBottom) {
    setTickLabelPadding(3);
    setLabelPadding(3);
  } else if (type == atLeft) {
    setTickLabelPadding(5);
    setLabelPadding(10);
  }
}

QCPAxis::~QCPAxis() {
  delete mAxisPainter;
  delete mGrid;
}

int QCPAxis::tickLabelPadding() const { return mAxisPainter->tickLabelPadding; }

double QCPAxis::tickLabelRotation() const {
  return mAxisPainter->tickLabelRotation;
}

QCPAxis::LabelSide QCPAxis::tickLabelSide() const {
  return mAxisPainter->tickLabelSide;
}

QString QCPAxis::numberFormat() const {
  QString result;
  result.append(mNumberFormatChar);
  if (mNumberBeautifulPowers) {
    result.append(QLatin1Char('b'));
    if (mAxisPainter->numberMultiplyCross)
      result.append(QLatin1Char('c'));
  }
  return result;
}

int QCPAxis::tickLengthIn() const { return mAxisPainter->tickLengthIn; }

int QCPAxis::tickLengthOut() const { return mAxisPainter->tickLengthOut; }

int QCPAxis::subTickLengthIn() const { return mAxisPainter->subTickLengthIn; }

int QCPAxis::subTickLengthOut() const { return mAxisPainter->subTickLengthOut; }

int QCPAxis::labelPadding() const { return mAxisPainter->labelPadding; }

int QCPAxis::offset() const { return mAxisPainter->offset; }

QCPLineEnding QCPAxis::lowerEnding() const { return mAxisPainter->lowerEnding; }

QCPLineEnding QCPAxis::upperEnding() const { return mAxisPainter->upperEnding; }

void QCPAxis::setScaleType(QCPAxis::ScaleType type) {
  if (mScaleType != type) {
    mScaleType = type;
    if (mScaleType == stLogarithmic)
      setRange(mRange.sanitizedForLogScale());
    mCachedMarginValid = false;
    Q_EMIT scaleTypeChanged(mScaleType);
  }
}

void QCPAxis::setRange(const QCPRange &range) {
  if (range.lower == mRange.lower && range.upper == mRange.upper)
    return;

  if (!QCPRange::validRange(range))
    return;
  QCPRange oldRange = mRange;
  if (mScaleType == stLogarithmic) {
    mRange = range.sanitizedForLogScale();
  } else {
    mRange = range.sanitizedForLinScale();
  }
  Q_EMIT rangeChanged(mRange);
  Q_EMIT rangeChanged(mRange, oldRange);
}

void QCPAxis::setSelectableParts(const SelectableParts &selectable) {
  if (mSelectableParts != selectable) {
    mSelectableParts = selectable;
    Q_EMIT selectableChanged(mSelectableParts);
  }
}

void QCPAxis::setSelectedParts(const SelectableParts &selected) {
  if (mSelectedParts != selected) {
    mSelectedParts = selected;
    Q_EMIT selectionChanged(mSelectedParts);
  }
}

void QCPAxis::setRange(double lower, double upper) {
  if (lower == mRange.lower && upper == mRange.upper)
    return;

  if (!QCPRange::validRange(lower, upper))
    return;
  QCPRange oldRange = mRange;
  mRange.lower = lower;
  mRange.upper = upper;
  if (mScaleType == stLogarithmic) {
    mRange = mRange.sanitizedForLogScale();
  } else {
    mRange = mRange.sanitizedForLinScale();
  }
  Q_EMIT rangeChanged(mRange);
  Q_EMIT rangeChanged(mRange, oldRange);
}

void QCPAxis::setRange(double position, double size,
                       Qt::AlignmentFlag alignment) {
  if (alignment == Qt::AlignLeft)
    setRange(position, position + size);
  else if (alignment == Qt::AlignRight)
    setRange(position - size, position);
  else

    setRange(position - size / 2.0, position + size / 2.0);
}

void QCPAxis::setRangeLower(double lower) {
  if (mRange.lower == lower)
    return;

  QCPRange oldRange = mRange;
  mRange.lower = lower;
  if (mScaleType == stLogarithmic) {
    mRange = mRange.sanitizedForLogScale();
  } else {
    mRange = mRange.sanitizedForLinScale();
  }
  Q_EMIT rangeChanged(mRange);
  Q_EMIT rangeChanged(mRange, oldRange);
}

void QCPAxis::setRangeUpper(double upper) {
  if (mRange.upper == upper)
    return;

  QCPRange oldRange = mRange;
  mRange.upper = upper;
  if (mScaleType == stLogarithmic) {
    mRange = mRange.sanitizedForLogScale();
  } else {
    mRange = mRange.sanitizedForLinScale();
  }
  Q_EMIT rangeChanged(mRange);
  Q_EMIT rangeChanged(mRange, oldRange);
}

void QCPAxis::setRangeReversed(bool reversed) { mRangeReversed = reversed; }

void QCPAxis::setTicker(QSharedPointer<QCPAxisTicker> ticker) {
  if (ticker)
    mTicker = ticker;
  else
    qDebug() << Q_FUNC_INFO << "can not set 0 as axis ticker";
}

void QCPAxis::setTicks(bool show) {
  if (mTicks != show) {
    mTicks = show;
    mCachedMarginValid = false;
  }
}

void QCPAxis::setTickLabels(bool show) {
  if (mTickLabels != show) {
    mTickLabels = show;
    mCachedMarginValid = false;
    if (!mTickLabels)
      mTickVectorLabels.clear();
  }
}

void QCPAxis::setTickLabelPadding(int padding) {
  if (mAxisPainter->tickLabelPadding != padding) {
    mAxisPainter->tickLabelPadding = padding;
    mCachedMarginValid = false;
  }
}

void QCPAxis::setTickLabelFont(const QFont &font) {
  if (font != mTickLabelFont) {
    mTickLabelFont = font;
    mCachedMarginValid = false;
  }
}

void QCPAxis::setTickLabelColor(const QColor &color) {
  mTickLabelColor = color;
}

void QCPAxis::setTickLabelRotation(double degrees) {
  if (!qFuzzyIsNull(degrees - mAxisPainter->tickLabelRotation)) {
    mAxisPainter->tickLabelRotation = qBound(-90.0, degrees, 90.0);
    mCachedMarginValid = false;
  }
}

void QCPAxis::setTickLabelSide(LabelSide side) {
  mAxisPainter->tickLabelSide = side;
  mCachedMarginValid = false;
}

void QCPAxis::setNumberFormat(const QString &formatCode) {
  if (formatCode.isEmpty()) {
    qDebug() << Q_FUNC_INFO << "Passed formatCode is empty";
    return;
  }
  mCachedMarginValid = false;

  QString allowedFormatChars(QLatin1String("eEfgG"));
  if (allowedFormatChars.contains(formatCode.at(0))) {
    mNumberFormatChar = QLatin1Char(formatCode.at(0).toLatin1());
  } else {
    qDebug() << Q_FUNC_INFO
             << "Invalid number format code (first char not in 'eEfgG'):"
             << formatCode;
    return;
  }
  if (formatCode.length() < 2) {
    mNumberBeautifulPowers = false;
    mAxisPainter->numberMultiplyCross = false;
    return;
  }

  if (formatCode.at(1) == QLatin1Char('b') &&
      (mNumberFormatChar == QLatin1Char('e') ||
       mNumberFormatChar == QLatin1Char('g'))) {
    mNumberBeautifulPowers = true;
  } else {
    qDebug() << Q_FUNC_INFO
             << "Invalid number format code (second char not 'b' or first char "
                "neither 'e' nor 'g'):"
             << formatCode;
    return;
  }
  if (formatCode.length() < 3) {
    mAxisPainter->numberMultiplyCross = false;
    return;
  }

  if (formatCode.at(2) == QLatin1Char('c')) {
    mAxisPainter->numberMultiplyCross = true;
  } else if (formatCode.at(2) == QLatin1Char('d')) {
    mAxisPainter->numberMultiplyCross = false;
  } else {
    qDebug() << Q_FUNC_INFO
             << "Invalid number format code (third char neither 'c' nor 'd'):"
             << formatCode;
    return;
  }
}

void QCPAxis::setNumberPrecision(int precision) {
  if (mNumberPrecision != precision) {
    mNumberPrecision = precision;
    mCachedMarginValid = false;
  }
}

void QCPAxis::setTickLength(int inside, int outside) {
  setTickLengthIn(inside);
  setTickLengthOut(outside);
}

void QCPAxis::setTickLengthIn(int inside) {
  if (mAxisPainter->tickLengthIn != inside) {
    mAxisPainter->tickLengthIn = inside;
  }
}

void QCPAxis::setTickLengthOut(int outside) {
  if (mAxisPainter->tickLengthOut != outside) {
    mAxisPainter->tickLengthOut = outside;
    mCachedMarginValid = false;
  }
}

void QCPAxis::setSubTicks(bool show) {
  if (mSubTicks != show) {
    mSubTicks = show;
    mCachedMarginValid = false;
  }
}

void QCPAxis::setSubTickLength(int inside, int outside) {
  setSubTickLengthIn(inside);
  setSubTickLengthOut(outside);
}

void QCPAxis::setSubTickLengthIn(int inside) {
  if (mAxisPainter->subTickLengthIn != inside) {
    mAxisPainter->subTickLengthIn = inside;
  }
}

void QCPAxis::setSubTickLengthOut(int outside) {
  if (mAxisPainter->subTickLengthOut != outside) {
    mAxisPainter->subTickLengthOut = outside;
    mCachedMarginValid = false;
  }
}

void QCPAxis::setBasePen(const QPen &pen) { mBasePen = pen; }

void QCPAxis::setTickPen(const QPen &pen) { mTickPen = pen; }

void QCPAxis::setSubTickPen(const QPen &pen) { mSubTickPen = pen; }

void QCPAxis::setLabelFont(const QFont &font) {
  if (mLabelFont != font) {
    mLabelFont = font;
    mCachedMarginValid = false;
  }
}

void QCPAxis::setLabelColor(const QColor &color) { mLabelColor = color; }

void QCPAxis::setLabel(const QString &str) {
  if (mLabel != str) {
    mLabel = str;
    mCachedMarginValid = false;
  }
}

void QCPAxis::setLabelPadding(int padding) {
  if (mAxisPainter->labelPadding != padding) {
    mAxisPainter->labelPadding = padding;
    mCachedMarginValid = false;
  }
}

void QCPAxis::setPadding(int padding) {
  if (mPadding != padding) {
    mPadding = padding;
    mCachedMarginValid = false;
  }
}

void QCPAxis::setOffset(int offset) { mAxisPainter->offset = offset; }

void QCPAxis::setSelectedTickLabelFont(const QFont &font) {
  if (font != mSelectedTickLabelFont) {
    mSelectedTickLabelFont = font;
  }
}

void QCPAxis::setSelectedLabelFont(const QFont &font) {
  mSelectedLabelFont = font;
}

void QCPAxis::setSelectedTickLabelColor(const QColor &color) {
  if (color != mSelectedTickLabelColor) {
    mSelectedTickLabelColor = color;
  }
}

void QCPAxis::setSelectedLabelColor(const QColor &color) {
  mSelectedLabelColor = color;
}

void QCPAxis::setSelectedBasePen(const QPen &pen) { mSelectedBasePen = pen; }

void QCPAxis::setSelectedTickPen(const QPen &pen) { mSelectedTickPen = pen; }

void QCPAxis::setSelectedSubTickPen(const QPen &pen) {
  mSelectedSubTickPen = pen;
}

void QCPAxis::setLowerEnding(const QCPLineEnding &ending) {
  mAxisPainter->lowerEnding = ending;
}

void QCPAxis::setUpperEnding(const QCPLineEnding &ending) {
  mAxisPainter->upperEnding = ending;
}

void QCPAxis::moveRange(double diff) {
  QCPRange oldRange = mRange;
  if (mScaleType == stLinear) {
    mRange.lower += diff;
    mRange.upper += diff;
  } else

  {
    mRange.lower *= diff;
    mRange.upper *= diff;
  }
  Q_EMIT rangeChanged(mRange);
  Q_EMIT rangeChanged(mRange, oldRange);
}

void QCPAxis::scaleRange(double factor) {
  scaleRange(factor, range().center());
}

void QCPAxis::scaleRange(double factor, double center) {
  QCPRange oldRange = mRange;
  if (mScaleType == stLinear) {
    QCPRange newRange;
    newRange.lower = (mRange.lower - center) * factor + center;
    newRange.upper = (mRange.upper - center) * factor + center;
    if (QCPRange::validRange(newRange))
      mRange = newRange.sanitizedForLinScale();
  } else

  {
    if ((mRange.upper < 0 && center < 0) || (mRange.upper > 0 && center > 0))

    {
      QCPRange newRange;
      newRange.lower = qPow(mRange.lower / center, factor) * center;
      newRange.upper = qPow(mRange.upper / center, factor) * center;
      if (QCPRange::validRange(newRange))
        mRange = newRange.sanitizedForLogScale();
    } else
      qDebug() << Q_FUNC_INFO
               << "Center of scaling operation doesn't lie in same logarithmic "
                  "sign domain as range:"
               << center;
  }
  Q_EMIT rangeChanged(mRange);
  Q_EMIT rangeChanged(mRange, oldRange);
}

void QCPAxis::setScaleRatio(const QCPAxis *otherAxis, double ratio) {
  int otherPixelSize, ownPixelSize;

  if (otherAxis->orientation() == Qt::Horizontal)
    otherPixelSize = otherAxis->axisRect()->width();
  else
    otherPixelSize = otherAxis->axisRect()->height();

  if (orientation() == Qt::Horizontal)
    ownPixelSize = axisRect()->width();
  else
    ownPixelSize = axisRect()->height();

  double newRangeSize =
      ratio * otherAxis->range().size() * ownPixelSize / (double)otherPixelSize;
  setRange(range().center(), newRangeSize, Qt::AlignCenter);
}

void QCPAxis::rescale(bool onlyVisiblePlottables) {
  QList<QCPAbstractPlottable *> p = plottables();
  QCPRange newRange;
  bool haveRange = false;
  for (int i = 0; i < p.size(); ++i) {
    if (!p.at(i)->realVisibility() && onlyVisiblePlottables)
      continue;
    QCPRange plottableRange;
    bool currentFoundRange;
    QCP::SignDomain signDomain = QCP::sdBoth;
    if (mScaleType == stLogarithmic)
      signDomain = (mRange.upper < 0 ? QCP::sdNegative : QCP::sdPositive);
    if (p.at(i)->keyAxis() == this)
      plottableRange = p.at(i)->getKeyRange(currentFoundRange, signDomain);
    else
      plottableRange = p.at(i)->getValueRange(currentFoundRange, signDomain);
    if (currentFoundRange) {
      if (!haveRange)
        newRange = plottableRange;
      else
        newRange.expand(plottableRange);
      haveRange = true;
    }
  }
  if (haveRange) {
    if (!QCPRange::validRange(newRange))

    {
      double center = (newRange.lower + newRange.upper) * 0.5;

      if (mScaleType == stLinear) {
        newRange.lower = center - mRange.size() / 2.0;
        newRange.upper = center + mRange.size() / 2.0;
      } else

      {
        newRange.lower = center / qSqrt(mRange.upper / mRange.lower);
        newRange.upper = center * qSqrt(mRange.upper / mRange.lower);
      }
    }
    setRange(newRange);
  }
}

double QCPAxis::pixelToCoord(double value) const {
  if (orientation() == Qt::Horizontal) {
    if (mScaleType == stLinear) {
      if (!mRangeReversed)
        return (value - mAxisRect->left()) / (double)mAxisRect->width() *
                   mRange.size() +
               mRange.lower;
      else
        return -(value - mAxisRect->left()) / (double)mAxisRect->width() *
                   mRange.size() +
               mRange.upper;
    } else

    {
      if (!mRangeReversed)
        return qPow(mRange.upper / mRange.lower,
                    (value - mAxisRect->left()) / (double)mAxisRect->width()) *
               mRange.lower;
      else
        return qPow(mRange.upper / mRange.lower,
                    (mAxisRect->left() - value) / (double)mAxisRect->width()) *
               mRange.upper;
    }
  } else

  {
    if (mScaleType == stLinear) {
      if (!mRangeReversed)
        return (mAxisRect->bottom() - value) / (double)mAxisRect->height() *
                   mRange.size() +
               mRange.lower;
      else
        return -(mAxisRect->bottom() - value) / (double)mAxisRect->height() *
                   mRange.size() +
               mRange.upper;
    } else

    {
      if (!mRangeReversed)
        return qPow(mRange.upper / mRange.lower,
                    (mAxisRect->bottom() - value) /
                        (double)mAxisRect->height()) *
               mRange.lower;
      else
        return qPow(mRange.upper / mRange.lower,
                    (value - mAxisRect->bottom()) /
                        (double)mAxisRect->height()) *
               mRange.upper;
    }
  }
}

double QCPAxis::coordToPixel(double value) const {
  if (orientation() == Qt::Horizontal) {
    if (mScaleType == stLinear) {
      if (!mRangeReversed)
        return (value - mRange.lower) / mRange.size() * mAxisRect->width() +
               mAxisRect->left();
      else
        return (mRange.upper - value) / mRange.size() * mAxisRect->width() +
               mAxisRect->left();
    } else

    {
      if (value >= 0.0 && mRange.upper < 0.0)

        return !mRangeReversed ? mAxisRect->right() + 200
                               : mAxisRect->left() - 200;
      else if (value <= 0.0 && mRange.upper >= 0.0)

        return !mRangeReversed ? mAxisRect->left() - 200
                               : mAxisRect->right() + 200;
      else {
        if (!mRangeReversed)
          return qLn(value / mRange.lower) / qLn(mRange.upper / mRange.lower) *
                     mAxisRect->width() +
                 mAxisRect->left();
        else
          return qLn(mRange.upper / value) / qLn(mRange.upper / mRange.lower) *
                     mAxisRect->width() +
                 mAxisRect->left();
      }
    }
  } else

  {
    if (mScaleType == stLinear) {
      if (!mRangeReversed)
        return mAxisRect->bottom() -
               (value - mRange.lower) / mRange.size() * mAxisRect->height();
      else
        return mAxisRect->bottom() -
               (mRange.upper - value) / mRange.size() * mAxisRect->height();
    } else

    {
      if (value >= 0.0 && mRange.upper < 0.0)

        return !mRangeReversed ? mAxisRect->top() - 200
                               : mAxisRect->bottom() + 200;
      else if (value <= 0.0 && mRange.upper >= 0.0)

        return !mRangeReversed ? mAxisRect->bottom() + 200
                               : mAxisRect->top() - 200;
      else {
        if (!mRangeReversed)
          return mAxisRect->bottom() - qLn(value / mRange.lower) /
                                           qLn(mRange.upper / mRange.lower) *
                                           mAxisRect->height();
        else
          return mAxisRect->bottom() - qLn(mRange.upper / value) /
                                           qLn(mRange.upper / mRange.lower) *
                                           mAxisRect->height();
      }
    }
  }
}

QCPAxis::SelectablePart QCPAxis::getPartAt(const QPointF &pos) const {
  if (!mVisible)
    return spNone;

  if (mAxisPainter->axisSelectionBox().contains(pos.toPoint()))
    return spAxis;
  else if (mAxisPainter->tickLabelsSelectionBox().contains(pos.toPoint()))
    return spTickLabels;
  else if (mAxisPainter->labelSelectionBox().contains(pos.toPoint()))
    return spAxisLabel;
  else
    return spNone;
}

double QCPAxis::selectTest(const QPointF &pos, bool onlySelectable,
                           QVariant *details) const {
  if (!mParentPlot)
    return -1;
  SelectablePart part = getPartAt(pos);
  if ((onlySelectable && !mSelectableParts.testFlag(part)) || part == spNone)
    return -1;

  if (details)
    details->setValue(part);
  return mParentPlot->selectionTolerance() * 0.99;
}

QList<QCPAbstractPlottable *> QCPAxis::plottables() const {
  QList<QCPAbstractPlottable *> result;
  if (!mParentPlot)
    return result;

  for (int i = 0; i < mParentPlot->mPlottables.size(); ++i) {
    if (mParentPlot->mPlottables.at(i)->keyAxis() == this ||
        mParentPlot->mPlottables.at(i)->valueAxis() == this)
      result.append(mParentPlot->mPlottables.at(i));
  }
  return result;
}

QList<QCPGraph *> QCPAxis::graphs() const {
  QList<QCPGraph *> result;
  if (!mParentPlot)
    return result;

  for (int i = 0; i < mParentPlot->mGraphs.size(); ++i) {
    if (mParentPlot->mGraphs.at(i)->keyAxis() == this ||
        mParentPlot->mGraphs.at(i)->valueAxis() == this)
      result.append(mParentPlot->mGraphs.at(i));
  }
  return result;
}

QList<QCPAbstractItem *> QCPAxis::items() const {
  QList<QCPAbstractItem *> result;
  if (!mParentPlot)
    return result;

  for (int itemId = 0; itemId < mParentPlot->mItems.size(); ++itemId) {
    QList<QCPItemPosition *> positions =
        mParentPlot->mItems.at(itemId)->positions();
    for (int posId = 0; posId < positions.size(); ++posId) {
      if (positions.at(posId)->keyAxis() == this ||
          positions.at(posId)->valueAxis() == this) {
        result.append(mParentPlot->mItems.at(itemId));
        break;
      }
    }
  }
  return result;
}

QCPAxis::AxisType QCPAxis::marginSideToAxisType(QCP::MarginSide side) {
  switch (side) {
  case QCP::msLeft:
    return atLeft;
  case QCP::msRight:
    return atRight;
  case QCP::msTop:
    return atTop;
  case QCP::msBottom:
    return atBottom;
  default:
    break;
  }
  qDebug() << Q_FUNC_INFO << "Invalid margin side passed:" << (int)side;
  return atLeft;
}

QCPAxis::AxisType QCPAxis::opposite(QCPAxis::AxisType type) {
  switch (type) {
  case atLeft:
    return atRight;
    break;
  case atRight:
    return atLeft;
    break;
  case atBottom:
    return atTop;
    break;
  case atTop:
    return atBottom;
    break;
  default:
    qDebug() << Q_FUNC_INFO << "invalid axis type";
    return atLeft;
    break;
  }
}

void QCPAxis::selectEvent(QMouseEvent *event, bool additive,
                          const QVariant &details,
                          bool *selectionStateChanged) {
  Q_UNUSED(event)
  SelectablePart part = details.value<SelectablePart>();
  if (mSelectableParts.testFlag(part)) {
    SelectableParts selBefore = mSelectedParts;
    setSelectedParts(additive ? mSelectedParts ^ part : part);
    if (selectionStateChanged)
      *selectionStateChanged = mSelectedParts != selBefore;
  }
}

void QCPAxis::deselectEvent(bool *selectionStateChanged) {
  SelectableParts selBefore = mSelectedParts;
  setSelectedParts(mSelectedParts & ~mSelectableParts);
  if (selectionStateChanged)
    *selectionStateChanged = mSelectedParts != selBefore;
}

void QCPAxis::mousePressEvent(QMouseEvent *event, const QVariant &details) {
  Q_UNUSED(details)
  if (!mParentPlot->interactions().testFlag(QCP::iRangeDrag) ||
      !mAxisRect->rangeDrag().testFlag(orientation()) ||
      !mAxisRect->rangeDragAxes(orientation()).contains(this)) {
    event->ignore();
    return;
  }

  if (event->buttons() & Qt::LeftButton) {
    mDragging = true;

    if (mParentPlot->noAntialiasingOnDrag()) {
      mAADragBackup = mParentPlot->antialiasedElements();
      mNotAADragBackup = mParentPlot->notAntialiasedElements();
    }

    if (mParentPlot->interactions().testFlag(QCP::iRangeDrag))
      mDragStartRange = mRange;
  }
}

void QCPAxis::mouseMoveEvent(QMouseEvent *event, const QPointF &startPos) {
  if (mDragging) {
    const double startPixel =
        orientation() == Qt::Horizontal ? startPos.x() : startPos.y();
    const double currentPixel =
        orientation() == Qt::Horizontal ? event->pos().x() : event->pos().y();
    if (mScaleType == QCPAxis::stLinear) {
      const double diff = pixelToCoord(startPixel) - pixelToCoord(currentPixel);
      setRange(mDragStartRange.lower + diff, mDragStartRange.upper + diff);
    } else if (mScaleType == QCPAxis::stLogarithmic) {
      const double diff = pixelToCoord(startPixel) / pixelToCoord(currentPixel);
      setRange(mDragStartRange.lower * diff, mDragStartRange.upper * diff);
    }

    if (mParentPlot->noAntialiasingOnDrag())
      mParentPlot->setNotAntialiasedElements(QCP::aeAll);
    mParentPlot->replot(QCustomPlot::rpQueuedReplot);
  }
}

void QCPAxis::mouseReleaseEvent(QMouseEvent *event, const QPointF &startPos) {
  Q_UNUSED(event)
  Q_UNUSED(startPos)
  mDragging = false;
  if (mParentPlot->noAntialiasingOnDrag()) {
    mParentPlot->setAntialiasedElements(mAADragBackup);
    mParentPlot->setNotAntialiasedElements(mNotAADragBackup);
  }
}

void QCPAxis::wheelEvent(QWheelEvent *event) {
  if (!mParentPlot->interactions().testFlag(QCP::iRangeZoom) ||
      !mAxisRect->rangeZoom().testFlag(orientation()) ||
      !mAxisRect->rangeZoomAxes(orientation()).contains(this)) {
    event->ignore();
    return;
  }

  const double wheelSteps = event->delta() / 120.0;

  const double factor =
      qPow(mAxisRect->rangeZoomFactor(orientation()), wheelSteps);
  scaleRange(factor,
             pixelToCoord(orientation() == Qt::Horizontal ? event->pos().x()
                                                          : event->pos().y()));
  mParentPlot->replot();
}

void QCPAxis::applyDefaultAntialiasingHint(QCPPainter *painter) const {
  applyAntialiasingHint(painter, mAntialiased, QCP::aeAxes);
}

void QCPAxis::draw(QCPPainter *painter) {
  QVector<double> subTickPositions;

  QVector<double> tickPositions;

  QVector<QString> tickLabels;

  tickPositions.reserve(mTickVector.size());
  tickLabels.reserve(mTickVector.size());
  subTickPositions.reserve(mSubTickVector.size());

  if (mTicks) {
    for (int i = 0; i < mTickVector.size(); ++i) {
      tickPositions.append(coordToPixel(mTickVector.at(i)));
      if (mTickLabels)
        tickLabels.append(mTickVectorLabels.at(i));
    }

    if (mSubTicks) {
      const int subTickCount = mSubTickVector.size();
      for (int i = 0; i < subTickCount; ++i)
        subTickPositions.append(coordToPixel(mSubTickVector.at(i)));
    }
  }

  mAxisPainter->type = mAxisType;
  mAxisPainter->basePen = getBasePen();
  mAxisPainter->labelFont = getLabelFont();
  mAxisPainter->labelColor = getLabelColor();
  mAxisPainter->label = mLabel;
  mAxisPainter->substituteExponent = mNumberBeautifulPowers;
  mAxisPainter->tickPen = getTickPen();
  mAxisPainter->subTickPen = getSubTickPen();
  mAxisPainter->tickLabelFont = getTickLabelFont();
  mAxisPainter->tickLabelColor = getTickLabelColor();
  mAxisPainter->axisRect = mAxisRect->rect();
  mAxisPainter->viewportRect = mParentPlot->viewport();
  mAxisPainter->abbreviateDecimalPowers = mScaleType == stLogarithmic;
  mAxisPainter->reversedEndings = mRangeReversed;
  mAxisPainter->tickPositions = tickPositions;
  mAxisPainter->tickLabels = tickLabels;
  mAxisPainter->subTickPositions = subTickPositions;
  mAxisPainter->draw(painter);
}

void QCPAxis::setupTickVectors() {
  if (!mParentPlot)
    return;
  if ((!mTicks && !mTickLabels && !mGrid->visible()) || mRange.size() <= 0)
    return;

  QVector<QString> oldLabels = mTickVectorLabels;
  mTicker->generate(mRange, mParentPlot->locale(), mNumberFormatChar,
                    mNumberPrecision, mTickVector,
                    mSubTicks ? &mSubTickVector : 0,
                    mTickLabels ? &mTickVectorLabels : 0);
  mCachedMarginValid &= mTickVectorLabels == oldLabels;
}

QPen QCPAxis::getBasePen() const {
  return mSelectedParts.testFlag(spAxis) ? mSelectedBasePen : mBasePen;
}

QPen QCPAxis::getTickPen() const {
  return mSelectedParts.testFlag(spAxis) ? mSelectedTickPen : mTickPen;
}

QPen QCPAxis::getSubTickPen() const {
  return mSelectedParts.testFlag(spAxis) ? mSelectedSubTickPen : mSubTickPen;
}

QFont QCPAxis::getTickLabelFont() const {
  return mSelectedParts.testFlag(spTickLabels) ? mSelectedTickLabelFont
                                               : mTickLabelFont;
}

QFont QCPAxis::getLabelFont() const {
  return mSelectedParts.testFlag(spAxisLabel) ? mSelectedLabelFont : mLabelFont;
}

QColor QCPAxis::getTickLabelColor() const {
  return mSelectedParts.testFlag(spTickLabels) ? mSelectedTickLabelColor
                                               : mTickLabelColor;
}

QColor QCPAxis::getLabelColor() const {
  return mSelectedParts.testFlag(spAxisLabel) ? mSelectedLabelColor
                                              : mLabelColor;
}

int QCPAxis::calculateMargin() {
  if (!mVisible)

    return 0;

  if (mCachedMarginValid)
    return mCachedMargin;

  int margin = 0;

  QVector<double> tickPositions;

  QVector<QString> tickLabels;

  tickPositions.reserve(mTickVector.size());
  tickLabels.reserve(mTickVector.size());

  if (mTicks) {
    for (int i = 0; i < mTickVector.size(); ++i) {
      tickPositions.append(coordToPixel(mTickVector.at(i)));
      if (mTickLabels)
        tickLabels.append(mTickVectorLabels.at(i));
    }
  }

  mAxisPainter->type = mAxisType;
  mAxisPainter->labelFont = getLabelFont();
  mAxisPainter->label = mLabel;
  mAxisPainter->tickLabelFont = mTickLabelFont;
  mAxisPainter->axisRect = mAxisRect->rect();
  mAxisPainter->viewportRect = mParentPlot->viewport();
  mAxisPainter->tickPositions = tickPositions;
  mAxisPainter->tickLabels = tickLabels;
  margin += mAxisPainter->size();
  margin += mPadding;

  mCachedMargin = margin;
  mCachedMarginValid = true;
  return margin;
}

QCP::Interaction QCPAxis::selectionCategory() const { return QCP::iSelectAxes; }

QCPAxisPainterPrivate::QCPAxisPainterPrivate(QCustomPlot *parentPlot)
    : type(QCPAxis::atLeft),
      basePen(QPen(Qt::black, 0, Qt::SolidLine, Qt::SquareCap)),
      lowerEnding(QCPLineEnding::esNone), upperEnding(QCPLineEnding::esNone),
      labelPadding(0), tickLabelPadding(0), tickLabelRotation(0),
      tickLabelSide(QCPAxis::lsOutside), substituteExponent(true),
      numberMultiplyCross(false), tickLengthIn(5), tickLengthOut(0),
      subTickLengthIn(2), subTickLengthOut(0),
      tickPen(QPen(Qt::black, 0, Qt::SolidLine, Qt::SquareCap)),
      subTickPen(QPen(Qt::black, 0, Qt::SolidLine, Qt::SquareCap)), offset(0),
      abbreviateDecimalPowers(false), reversedEndings(false),
      mParentPlot(parentPlot), mLabelCache(16)

{}

QCPAxisPainterPrivate::~QCPAxisPainterPrivate() {}

void QCPAxisPainterPrivate::draw(QCPPainter *painter) {
  QByteArray newHash = generateLabelParameterHash();
  if (newHash != mLabelParameterHash) {
    mLabelCache.clear();
    mLabelParameterHash = newHash;
  }

  QPoint origin;
  switch (type) {
  case QCPAxis::atLeft:
    origin = axisRect.bottomLeft() + QPoint(-offset, 0);
    break;
  case QCPAxis::atRight:
    origin = axisRect.bottomRight() + QPoint(+offset, 0);
    break;
  case QCPAxis::atTop:
    origin = axisRect.topLeft() + QPoint(0, -offset);
    break;
  case QCPAxis::atBottom:
    origin = axisRect.bottomLeft() + QPoint(0, +offset);
    break;
  }

  double xCor = 0, yCor = 0;

  switch (type) {
  case QCPAxis::atTop:
    yCor = -1;
    break;
  case QCPAxis::atRight:
    xCor = 1;
    break;
  default:
    break;
  }
  int margin = 0;

  QLineF baseLine;
  painter->setPen(basePen);
  if (QCPAxis::orientation(type) == Qt::Horizontal)
    baseLine.setPoints(origin + QPointF(xCor, yCor),
                       origin + QPointF(axisRect.width() + xCor, yCor));
  else
    baseLine.setPoints(origin + QPointF(xCor, yCor),
                       origin + QPointF(xCor, -axisRect.height() + yCor));
  if (reversedEndings)
    baseLine = QLineF(baseLine.p2(), baseLine.p1());

  painter->drawLine(baseLine);

  if (!tickPositions.isEmpty()) {
    painter->setPen(tickPen);
    int tickDir =
        (type == QCPAxis::atBottom || type == QCPAxis::atRight) ? -1 : 1;

    if (QCPAxis::orientation(type) == Qt::Horizontal) {
      for (int i = 0; i < tickPositions.size(); ++i)
        painter->drawLine(QLineF(tickPositions.at(i) + xCor,
                                 origin.y() - tickLengthOut * tickDir + yCor,
                                 tickPositions.at(i) + xCor,
                                 origin.y() + tickLengthIn * tickDir + yCor));
    } else {
      for (int i = 0; i < tickPositions.size(); ++i)
        painter->drawLine(QLineF(origin.x() - tickLengthOut * tickDir + xCor,
                                 tickPositions.at(i) + yCor,
                                 origin.x() + tickLengthIn * tickDir + xCor,
                                 tickPositions.at(i) + yCor));
    }
  }

  if (!subTickPositions.isEmpty()) {
    painter->setPen(subTickPen);

    int tickDir =
        (type == QCPAxis::atBottom || type == QCPAxis::atRight) ? -1 : 1;
    if (QCPAxis::orientation(type) == Qt::Horizontal) {
      for (int i = 0; i < subTickPositions.size(); ++i)
        painter->drawLine(
            QLineF(subTickPositions.at(i) + xCor,
                   origin.y() - subTickLengthOut * tickDir + yCor,
                   subTickPositions.at(i) + xCor,
                   origin.y() + subTickLengthIn * tickDir + yCor));
    } else {
      for (int i = 0; i < subTickPositions.size(); ++i)
        painter->drawLine(QLineF(origin.x() - subTickLengthOut * tickDir + xCor,
                                 subTickPositions.at(i) + yCor,
                                 origin.x() + subTickLengthIn * tickDir + xCor,
                                 subTickPositions.at(i) + yCor));
    }
  }
  margin += qMax(0, qMax(tickLengthOut, subTickLengthOut));

  bool antialiasingBackup = painter->antialiasing();
  painter->setAntialiasing(true);

  painter->setBrush(QBrush(basePen.color()));
  QCPVector2D baseLineVector(baseLine.dx(), baseLine.dy());
  if (lowerEnding.style() != QCPLineEnding::esNone)
    lowerEnding.draw(painter,
                     QCPVector2D(baseLine.p1()) -
                         baseLineVector.normalized() *
                             lowerEnding.realLength() *
                             (lowerEnding.inverted() ? -1 : 1),
                     -baseLineVector);
  if (upperEnding.style() != QCPLineEnding::esNone)
    upperEnding.draw(painter,
                     QCPVector2D(baseLine.p2()) +
                         baseLineVector.normalized() *
                             upperEnding.realLength() *
                             (upperEnding.inverted() ? -1 : 1),
                     baseLineVector);
  painter->setAntialiasing(antialiasingBackup);

  QRect oldClipRect;
  if (tickLabelSide == QCPAxis::lsInside)

  {
    oldClipRect = painter->clipRegion().boundingRect();
    painter->setClipRect(axisRect);
  }
  QSize tickLabelsSize(0, 0);

  if (!tickLabels.isEmpty()) {
    if (tickLabelSide == QCPAxis::lsOutside)
      margin += tickLabelPadding;
    painter->setFont(tickLabelFont);
    painter->setPen(QPen(tickLabelColor));
    const int maxLabelIndex = qMin(tickPositions.size(), tickLabels.size());
    int distanceToAxis = margin;
    if (tickLabelSide == QCPAxis::lsInside)
      distanceToAxis =
          -(qMax(tickLengthIn, subTickLengthIn) + tickLabelPadding);
    for (int i = 0; i < maxLabelIndex; ++i)
      placeTickLabel(painter, tickPositions.at(i), distanceToAxis,
                     tickLabels.at(i), &tickLabelsSize);
    if (tickLabelSide == QCPAxis::lsOutside)
      margin += (QCPAxis::orientation(type) == Qt::Horizontal)
                    ? tickLabelsSize.height()
                    : tickLabelsSize.width();
  }
  if (tickLabelSide == QCPAxis::lsInside)
    painter->setClipRect(oldClipRect);

  QRect labelBounds;
  if (!label.isEmpty()) {
    margin += labelPadding;
    painter->setFont(labelFont);
    painter->setPen(QPen(labelColor));
    labelBounds = painter->fontMetrics().boundingRect(0, 0, 0, 0,
                                                      Qt::TextDontClip, label);
    if (type == QCPAxis::atLeft) {
      QTransform oldTransform = painter->transform();
      painter->translate((origin.x() - margin - labelBounds.height()),
                         origin.y());
      painter->rotate(-90);
      painter->drawText(0, 0, axisRect.height(), labelBounds.height(),
                        Qt::TextDontClip | Qt::AlignCenter, label);
      painter->setTransform(oldTransform);
    } else if (type == QCPAxis::atRight) {
      QTransform oldTransform = painter->transform();
      painter->translate((origin.x() + margin + labelBounds.height()),
                         origin.y() - axisRect.height());
      painter->rotate(90);
      painter->drawText(0, 0, axisRect.height(), labelBounds.height(),
                        Qt::TextDontClip | Qt::AlignCenter, label);
      painter->setTransform(oldTransform);
    } else if (type == QCPAxis::atTop)
      painter->drawText(origin.x(), origin.y() - margin - labelBounds.height(),
                        axisRect.width(), labelBounds.height(),
                        Qt::TextDontClip | Qt::AlignCenter, label);
    else if (type == QCPAxis::atBottom)
      painter->drawText(origin.x(), origin.y() + margin, axisRect.width(),
                        labelBounds.height(),
                        Qt::TextDontClip | Qt::AlignCenter, label);
  }

  int selectionTolerance = 0;
  if (mParentPlot)
    selectionTolerance = mParentPlot->selectionTolerance();
  else
    qDebug() << Q_FUNC_INFO << "mParentPlot is null";
  int selAxisOutSize =
      qMax(qMax(tickLengthOut, subTickLengthOut), selectionTolerance);
  int selAxisInSize = selectionTolerance;
  int selTickLabelSize;
  int selTickLabelOffset;
  if (tickLabelSide == QCPAxis::lsOutside) {
    selTickLabelSize =
        (QCPAxis::orientation(type) == Qt::Horizontal ? tickLabelsSize.height()
                                                      : tickLabelsSize.width());
    selTickLabelOffset =
        qMax(tickLengthOut, subTickLengthOut) + tickLabelPadding;
  } else {
    selTickLabelSize = -(QCPAxis::orientation(type) == Qt::Horizontal
                             ? tickLabelsSize.height()
                             : tickLabelsSize.width());
    selTickLabelOffset =
        -(qMax(tickLengthIn, subTickLengthIn) + tickLabelPadding);
  }
  int selLabelSize = labelBounds.height();
  int selLabelOffset =
      qMax(tickLengthOut, subTickLengthOut) +
      (!tickLabels.isEmpty() && tickLabelSide == QCPAxis::lsOutside
           ? tickLabelPadding + selTickLabelSize
           : 0) +
      labelPadding;
  if (type == QCPAxis::atLeft) {
    mAxisSelectionBox.setCoords(origin.x() - selAxisOutSize, axisRect.top(),
                                origin.x() + selAxisInSize, axisRect.bottom());
    mTickLabelsSelectionBox.setCoords(
        origin.x() - selTickLabelOffset - selTickLabelSize, axisRect.top(),
        origin.x() - selTickLabelOffset, axisRect.bottom());
    mLabelSelectionBox.setCoords(origin.x() - selLabelOffset - selLabelSize,
                                 axisRect.top(), origin.x() - selLabelOffset,
                                 axisRect.bottom());
  } else if (type == QCPAxis::atRight) {
    mAxisSelectionBox.setCoords(origin.x() - selAxisInSize, axisRect.top(),
                                origin.x() + selAxisOutSize, axisRect.bottom());
    mTickLabelsSelectionBox.setCoords(
        origin.x() + selTickLabelOffset + selTickLabelSize, axisRect.top(),
        origin.x() + selTickLabelOffset, axisRect.bottom());
    mLabelSelectionBox.setCoords(origin.x() + selLabelOffset + selLabelSize,
                                 axisRect.top(), origin.x() + selLabelOffset,
                                 axisRect.bottom());
  } else if (type == QCPAxis::atTop) {
    mAxisSelectionBox.setCoords(axisRect.left(), origin.y() - selAxisOutSize,
                                axisRect.right(), origin.y() + selAxisInSize);
    mTickLabelsSelectionBox.setCoords(
        axisRect.left(), origin.y() - selTickLabelOffset - selTickLabelSize,
        axisRect.right(), origin.y() - selTickLabelOffset);
    mLabelSelectionBox.setCoords(axisRect.left(),
                                 origin.y() - selLabelOffset - selLabelSize,
                                 axisRect.right(), origin.y() - selLabelOffset);
  } else if (type == QCPAxis::atBottom) {
    mAxisSelectionBox.setCoords(axisRect.left(), origin.y() - selAxisInSize,
                                axisRect.right(), origin.y() + selAxisOutSize);
    mTickLabelsSelectionBox.setCoords(
        axisRect.left(), origin.y() + selTickLabelOffset + selTickLabelSize,
        axisRect.right(), origin.y() + selTickLabelOffset);
    mLabelSelectionBox.setCoords(axisRect.left(),
                                 origin.y() + selLabelOffset + selLabelSize,
                                 axisRect.right(), origin.y() + selLabelOffset);
  }
  mAxisSelectionBox = mAxisSelectionBox.normalized();
  mTickLabelsSelectionBox = mTickLabelsSelectionBox.normalized();
  mLabelSelectionBox = mLabelSelectionBox.normalized();
}

int QCPAxisPainterPrivate::size() const {
  int result = 0;

  if (!tickPositions.isEmpty())
    result += qMax(0, qMax(tickLengthOut, subTickLengthOut));

  if (tickLabelSide == QCPAxis::lsOutside) {
    QSize tickLabelsSize(0, 0);
    if (!tickLabels.isEmpty()) {
      for (int i = 0; i < tickLabels.size(); ++i)
        getMaxTickLabelSize(tickLabelFont, tickLabels.at(i), &tickLabelsSize);
      result += QCPAxis::orientation(type) == Qt::Horizontal
                    ? tickLabelsSize.height()
                    : tickLabelsSize.width();
      result += tickLabelPadding;
    }
  }

  if (!label.isEmpty()) {
    QFontMetrics fontMetrics(labelFont);
    QRect bounds;
    bounds = fontMetrics.boundingRect(
        0, 0, 0, 0, Qt::TextDontClip | Qt::AlignHCenter | Qt::AlignVCenter,
        label);
    result += bounds.height() + labelPadding;
  }

  return result;
}

void QCPAxisPainterPrivate::clearCache() { mLabelCache.clear(); }

QByteArray QCPAxisPainterPrivate::generateLabelParameterHash() const {
  QByteArray result;
  result.append(QByteArray::number(mParentPlot->bufferDevicePixelRatio()));
  result.append(QByteArray::number(tickLabelRotation));
  result.append(QByteArray::number((int)tickLabelSide));
  result.append(QByteArray::number((int)substituteExponent));
  result.append(QByteArray::number((int)numberMultiplyCross));
  result.append(tickLabelColor.name().toLatin1() +
                QByteArray::number(tickLabelColor.alpha(), 16));
  result.append(tickLabelFont.toString().toLatin1());
  return result;
}

void QCPAxisPainterPrivate::placeTickLabel(QCPPainter *painter, double position,
                                           int distanceToAxis,
                                           const QString &text,
                                           QSize *tickLabelsSize) {
  if (text.isEmpty())
    return;
  QSize finalSize;
  QPointF labelAnchor;
  switch (type) {
  case QCPAxis::atLeft:
    labelAnchor = QPointF(axisRect.left() - distanceToAxis - offset, position);
    break;
  case QCPAxis::atRight:
    labelAnchor = QPointF(axisRect.right() + distanceToAxis + offset, position);
    break;
  case QCPAxis::atTop:
    labelAnchor = QPointF(position, axisRect.top() - distanceToAxis - offset);
    break;
  case QCPAxis::atBottom:
    labelAnchor =
        QPointF(position, axisRect.bottom() + distanceToAxis + offset);
    break;
  }
  if (mParentPlot->plottingHints().testFlag(QCP::phCacheLabels) &&
      !painter->modes().testFlag(QCPPainter::pmNoCaching))

  {
    CachedLabel *cachedLabel = mLabelCache.take(text);

    if (!cachedLabel)

    {
      cachedLabel = new CachedLabel;
      TickLabelData labelData = getTickLabelData(painter->font(), text);
      cachedLabel->offset = getTickLabelDrawOffset(labelData) +
                            labelData.rotatedTotalBounds.topLeft();
      if (!qFuzzyCompare(1.0, mParentPlot->bufferDevicePixelRatio())) {
        cachedLabel->pixmap = QPixmap(labelData.rotatedTotalBounds.size() *
                                      mParentPlot->bufferDevicePixelRatio());
#ifdef QCP_DEVICEPIXELRATIO_SUPPORTED
#ifdef QCP_DEVICEPIXELRATIO_FLOAT
        cachedLabel->pixmap.setDevicePixelRatio(
            mParentPlot->devicePixelRatioF());
#else
        cachedLabel->pixmap.setDevicePixelRatio(
            mParentPlot->devicePixelRatio());
#endif
#endif
      } else
        cachedLabel->pixmap = QPixmap(labelData.rotatedTotalBounds.size());
      cachedLabel->pixmap.fill(Qt::transparent);
      QCPPainter cachePainter(&cachedLabel->pixmap);
      cachePainter.setPen(painter->pen());
      drawTickLabel(&cachePainter, -labelData.rotatedTotalBounds.topLeft().x(),
                    -labelData.rotatedTotalBounds.topLeft().y(), labelData);
    }

    bool labelClippedByBorder = false;
    if (tickLabelSide == QCPAxis::lsOutside) {
      if (QCPAxis::orientation(type) == Qt::Horizontal)
        labelClippedByBorder =
            labelAnchor.x() + cachedLabel->offset.x() +
                    cachedLabel->pixmap.width() /
                        mParentPlot->bufferDevicePixelRatio() >
                viewportRect.right() ||
            labelAnchor.x() + cachedLabel->offset.x() < viewportRect.left();
      else
        labelClippedByBorder =
            labelAnchor.y() + cachedLabel->offset.y() +
                    cachedLabel->pixmap.height() /
                        mParentPlot->bufferDevicePixelRatio() >
                viewportRect.bottom() ||
            labelAnchor.y() + cachedLabel->offset.y() < viewportRect.top();
    }
    if (!labelClippedByBorder) {
      painter->drawPixmap(labelAnchor + cachedLabel->offset,
                          cachedLabel->pixmap);
      finalSize =
          cachedLabel->pixmap.size() / mParentPlot->bufferDevicePixelRatio();
    }
    mLabelCache.insert(text, cachedLabel);

  } else

  {
    TickLabelData labelData = getTickLabelData(painter->font(), text);
    QPointF finalPosition = labelAnchor + getTickLabelDrawOffset(labelData);

    bool labelClippedByBorder = false;
    if (tickLabelSide == QCPAxis::lsOutside) {
      if (QCPAxis::orientation(type) == Qt::Horizontal)
        labelClippedByBorder =
            finalPosition.x() + (labelData.rotatedTotalBounds.width() +
                                 labelData.rotatedTotalBounds.left()) >
                viewportRect.right() ||
            finalPosition.x() + labelData.rotatedTotalBounds.left() <
                viewportRect.left();
      else
        labelClippedByBorder =
            finalPosition.y() + (labelData.rotatedTotalBounds.height() +
                                 labelData.rotatedTotalBounds.top()) >
                viewportRect.bottom() ||
            finalPosition.y() + labelData.rotatedTotalBounds.top() <
                viewportRect.top();
    }
    if (!labelClippedByBorder) {
      drawTickLabel(painter, finalPosition.x(), finalPosition.y(), labelData);
      finalSize = labelData.rotatedTotalBounds.size();
    }
  }

  if (finalSize.width() > tickLabelsSize->width())
    tickLabelsSize->setWidth(finalSize.width());
  if (finalSize.height() > tickLabelsSize->height())
    tickLabelsSize->setHeight(finalSize.height());
}

void QCPAxisPainterPrivate::drawTickLabel(
    QCPPainter *painter, double x, double y,
    const TickLabelData &labelData) const {
  QTransform oldTransform = painter->transform();
  QFont oldFont = painter->font();

  painter->translate(x, y);
  if (!qFuzzyIsNull(tickLabelRotation))
    painter->rotate(tickLabelRotation);

  if (!labelData.expPart.isEmpty())

  {
    painter->setFont(labelData.baseFont);
    painter->drawText(0, 0, 0, 0, Qt::TextDontClip, labelData.basePart);
    if (!labelData.suffixPart.isEmpty())
      painter->drawText(labelData.baseBounds.width() + 1 +
                            labelData.expBounds.width(),
                        0, 0, 0, Qt::TextDontClip, labelData.suffixPart);
    painter->setFont(labelData.expFont);
    painter->drawText(labelData.baseBounds.width() + 1, 0,
                      labelData.expBounds.width(), labelData.expBounds.height(),
                      Qt::TextDontClip, labelData.expPart);
  } else {
    painter->setFont(labelData.baseFont);
    painter->drawText(0, 0, labelData.totalBounds.width(),
                      labelData.totalBounds.height(),
                      Qt::TextDontClip | Qt::AlignHCenter, labelData.basePart);
  }

  painter->setTransform(oldTransform);
  painter->setFont(oldFont);
}

QCPAxisPainterPrivate::TickLabelData
QCPAxisPainterPrivate::getTickLabelData(const QFont &font,
                                        const QString &text) const {
  TickLabelData result;

  bool useBeautifulPowers = false;
  int ePos = -1;

  int eLast = -1;

  if (substituteExponent) {
    ePos = text.indexOf(QLatin1Char('e'));
    if (ePos > 0 && text.at(ePos - 1).isDigit()) {
      eLast = ePos;
      while (eLast + 1 < text.size() &&
             (text.at(eLast + 1) == QLatin1Char('+') ||
              text.at(eLast + 1) == QLatin1Char('-') ||
              text.at(eLast + 1).isDigit()))
        ++eLast;
      if (eLast > ePos)

        useBeautifulPowers = true;
    }
  }

  result.baseFont = font;
  if (result.baseFont.pointSizeF() > 0)

    result.baseFont.setPointSizeF(result.baseFont.pointSizeF() + 0.05);

  if (useBeautifulPowers) {
    result.basePart = text.left(ePos);
    result.suffixPart = text.mid(eLast + 1);

    if (abbreviateDecimalPowers && result.basePart == QLatin1String("1"))
      result.basePart = QLatin1String("10");
    else
      result.basePart +=
          (numberMultiplyCross ? QString(QChar(215)) : QString(QChar(183))) +
          QLatin1String("10");
    result.expPart = text.mid(ePos + 1, eLast - ePos);

    while (result.expPart.length() > 2 &&
           result.expPart.at(1) == QLatin1Char('0'))

      result.expPart.remove(1, 1);
    if (!result.expPart.isEmpty() && result.expPart.at(0) == QLatin1Char('+'))
      result.expPart.remove(0, 1);

    result.expFont = font;
    if (result.expFont.pointSize() > 0)
      result.expFont.setPointSize(result.expFont.pointSize() * 0.75);
    else
      result.expFont.setPixelSize(result.expFont.pixelSize() * 0.75);

    result.baseBounds =
        QFontMetrics(result.baseFont)
            .boundingRect(0, 0, 0, 0, Qt::TextDontClip, result.basePart);
    result.expBounds =
        QFontMetrics(result.expFont)
            .boundingRect(0, 0, 0, 0, Qt::TextDontClip, result.expPart);
    if (!result.suffixPart.isEmpty())
      result.suffixBounds =
          QFontMetrics(result.baseFont)
              .boundingRect(0, 0, 0, 0, Qt::TextDontClip, result.suffixPart);
    result.totalBounds = result.baseBounds.adjusted(
        0, 0, result.expBounds.width() + result.suffixBounds.width() + 2, 0);

  } else

  {
    result.basePart = text;
    result.totalBounds =
        QFontMetrics(result.baseFont)
            .boundingRect(0, 0, 0, 0, Qt::TextDontClip | Qt::AlignHCenter,
                          result.basePart);
  }
  result.totalBounds.moveTopLeft(QPoint(0, 0));

  result.rotatedTotalBounds = result.totalBounds;
  if (!qFuzzyIsNull(tickLabelRotation)) {
    QTransform transform;
    transform.rotate(tickLabelRotation);
    result.rotatedTotalBounds = transform.mapRect(result.rotatedTotalBounds);
  }

  return result;
}

QPointF QCPAxisPainterPrivate::getTickLabelDrawOffset(
    const TickLabelData &labelData) const {
  bool doRotation = !qFuzzyIsNull(tickLabelRotation);
  bool flip = qFuzzyCompare(qAbs(tickLabelRotation), 90.0);

  double radians = tickLabelRotation / 180.0 * M_PI;
  int x = 0, y = 0;
  if ((type == QCPAxis::atLeft && tickLabelSide == QCPAxis::lsOutside) ||
      (type == QCPAxis::atRight && tickLabelSide == QCPAxis::lsInside))

  {
    if (doRotation) {
      if (tickLabelRotation > 0) {
        x = -qCos(radians) * labelData.totalBounds.width();
        y = flip ? -labelData.totalBounds.width() / 2.0
                 : -qSin(radians) * labelData.totalBounds.width() -
                       qCos(radians) * labelData.totalBounds.height() / 2.0;
      } else {
        x = -qCos(-radians) * labelData.totalBounds.width() -
            qSin(-radians) * labelData.totalBounds.height();
        y = flip ? +labelData.totalBounds.width() / 2.0
                 : +qSin(-radians) * labelData.totalBounds.width() -
                       qCos(-radians) * labelData.totalBounds.height() / 2.0;
      }
    } else {
      x = -labelData.totalBounds.width();
      y = -labelData.totalBounds.height() / 2.0;
    }
  } else if ((type == QCPAxis::atRight &&
              tickLabelSide == QCPAxis::lsOutside) ||
             (type == QCPAxis::atLeft && tickLabelSide == QCPAxis::lsInside))

  {
    if (doRotation) {
      if (tickLabelRotation > 0) {
        x = +qSin(radians) * labelData.totalBounds.height();
        y = flip ? -labelData.totalBounds.width() / 2.0
                 : -qCos(radians) * labelData.totalBounds.height() / 2.0;
      } else {
        x = 0;
        y = flip ? +labelData.totalBounds.width() / 2.0
                 : -qCos(-radians) * labelData.totalBounds.height() / 2.0;
      }
    } else {
      x = 0;
      y = -labelData.totalBounds.height() / 2.0;
    }
  } else if ((type == QCPAxis::atTop && tickLabelSide == QCPAxis::lsOutside) ||
             (type == QCPAxis::atBottom && tickLabelSide == QCPAxis::lsInside))

  {
    if (doRotation) {
      if (tickLabelRotation > 0) {
        x = -qCos(radians) * labelData.totalBounds.width() +
            qSin(radians) * labelData.totalBounds.height() / 2.0;
        y = -qSin(radians) * labelData.totalBounds.width() -
            qCos(radians) * labelData.totalBounds.height();
      } else {
        x = -qSin(-radians) * labelData.totalBounds.height() / 2.0;
        y = -qCos(-radians) * labelData.totalBounds.height();
      }
    } else {
      x = -labelData.totalBounds.width() / 2.0;
      y = -labelData.totalBounds.height();
    }
  } else if ((type == QCPAxis::atBottom &&
              tickLabelSide == QCPAxis::lsOutside) ||
             (type == QCPAxis::atTop && tickLabelSide == QCPAxis::lsInside))

  {
    if (doRotation) {
      if (tickLabelRotation > 0) {
        x = +qSin(radians) * labelData.totalBounds.height() / 2.0;
        y = 0;
      } else {
        x = -qCos(-radians) * labelData.totalBounds.width() -
            qSin(-radians) * labelData.totalBounds.height() / 2.0;
        y = +qSin(-radians) * labelData.totalBounds.width();
      }
    } else {
      x = -labelData.totalBounds.width() / 2.0;
      y = 0;
    }
  }

  return QPointF(x, y);
}

void QCPAxisPainterPrivate::getMaxTickLabelSize(const QFont &font,
                                                const QString &text,
                                                QSize *tickLabelsSize) const {
  QSize finalSize;
  if (mParentPlot->plottingHints().testFlag(QCP::phCacheLabels) &&
      mLabelCache.contains(text))

  {
    const CachedLabel *cachedLabel = mLabelCache.object(text);
    finalSize =
        cachedLabel->pixmap.size() / mParentPlot->bufferDevicePixelRatio();
  } else

  {
    TickLabelData labelData = getTickLabelData(font, text);
    finalSize = labelData.rotatedTotalBounds.size();
  }

  if (finalSize.width() > tickLabelsSize->width())
    tickLabelsSize->setWidth(finalSize.width());
  if (finalSize.height() > tickLabelsSize->height())
    tickLabelsSize->setHeight(finalSize.height());
}

QCPScatterStyle::QCPScatterStyle()
    : mSize(6), mShape(ssNone), mPen(Qt::NoPen), mBrush(Qt::NoBrush),
      mPenDefined(false) {}

QCPScatterStyle::QCPScatterStyle(ScatterShape shape, double size)
    : mSize(size), mShape(shape), mPen(Qt::NoPen), mBrush(Qt::NoBrush),
      mPenDefined(false) {}

QCPScatterStyle::QCPScatterStyle(ScatterShape shape, const QColor &color,
                                 double size)
    : mSize(size), mShape(shape), mPen(QPen(color)), mBrush(Qt::NoBrush),
      mPenDefined(true) {}

QCPScatterStyle::QCPScatterStyle(ScatterShape shape, const QColor &color,
                                 const QColor &fill, double size)
    : mSize(size), mShape(shape), mPen(QPen(color)), mBrush(QBrush(fill)),
      mPenDefined(true) {}

QCPScatterStyle::QCPScatterStyle(ScatterShape shape, const QPen &pen,
                                 const QBrush &brush, double size)
    : mSize(size), mShape(shape), mPen(pen), mBrush(brush),
      mPenDefined(pen.style() != Qt::NoPen) {}

QCPScatterStyle::QCPScatterStyle(const QPixmap &pixmap)
    : mSize(5), mShape(ssPixmap), mPen(Qt::NoPen), mBrush(Qt::NoBrush),
      mPixmap(pixmap), mPenDefined(false) {}

QCPScatterStyle::QCPScatterStyle(const QPainterPath &customPath,
                                 const QPen &pen, const QBrush &brush,
                                 double size)
    : mSize(size), mShape(ssCustom), mPen(pen), mBrush(brush),
      mCustomPath(customPath), mPenDefined(pen.style() != Qt::NoPen) {}

void QCPScatterStyle::setFromOther(const QCPScatterStyle &other,
                                   ScatterProperties properties) {
  if (properties.testFlag(spPen)) {
    setPen(other.pen());
    if (!other.isPenDefined())
      undefinePen();
  }
  if (properties.testFlag(spBrush))
    setBrush(other.brush());
  if (properties.testFlag(spSize))
    setSize(other.size());
  if (properties.testFlag(spShape)) {
    setShape(other.shape());
    if (other.shape() == ssPixmap)
      setPixmap(other.pixmap());
    else if (other.shape() == ssCustom)
      setCustomPath(other.customPath());
  }
}

void QCPScatterStyle::setSize(double size) { mSize = size; }

void QCPScatterStyle::setShape(QCPScatterStyle::ScatterShape shape) {
  mShape = shape;
}

void QCPScatterStyle::setPen(const QPen &pen) {
  mPenDefined = true;
  mPen = pen;
}

void QCPScatterStyle::setBrush(const QBrush &brush) { mBrush = brush; }

void QCPScatterStyle::setPixmap(const QPixmap &pixmap) {
  setShape(ssPixmap);
  mPixmap = pixmap;
}

void QCPScatterStyle::setCustomPath(const QPainterPath &customPath) {
  setShape(ssCustom);
  mCustomPath = customPath;
}

void QCPScatterStyle::undefinePen() { mPenDefined = false; }

void QCPScatterStyle::applyTo(QCPPainter *painter,
                              const QPen &defaultPen) const {
  painter->setPen(mPenDefined ? mPen : defaultPen);
  painter->setBrush(mBrush);
}

void QCPScatterStyle::drawShape(QCPPainter *painter, const QPointF &pos) const {
  drawShape(painter, pos.x(), pos.y());
}

void QCPScatterStyle::drawShape(QCPPainter *painter, double x, double y) const {
  double w = mSize / 2.0;
  switch (mShape) {
  case ssNone:
    break;
  case ssDot: {
    painter->drawLine(QPointF(x, y), QPointF(x + 0.0001, y));
    break;
  }
  case ssCross: {
    painter->drawLine(QLineF(x - w, y - w, x + w, y + w));
    painter->drawLine(QLineF(x - w, y + w, x + w, y - w));
    break;
  }
  case ssPlus: {
    painter->drawLine(QLineF(x - w, y, x + w, y));
    painter->drawLine(QLineF(x, y + w, x, y - w));
    break;
  }
  case ssCircle: {
    painter->drawEllipse(QPointF(x, y), w, w);
    break;
  }
  case ssDisc: {
    QBrush b = painter->brush();
    painter->setBrush(painter->pen().color());
    painter->drawEllipse(QPointF(x, y), w, w);
    painter->setBrush(b);
    break;
  }
  case ssSquare: {
    painter->drawRect(QRectF(x - w, y - w, mSize, mSize));
    break;
  }
  case ssDiamond: {
    QPointF lineArray[4] = {QPointF(x - w, y), QPointF(x, y - w),
                            QPointF(x + w, y), QPointF(x, y + w)};
    painter->drawPolygon(lineArray, 4);
    break;
  }
  case ssStar: {
    painter->drawLine(QLineF(x - w, y, x + w, y));
    painter->drawLine(QLineF(x, y + w, x, y - w));
    painter->drawLine(
        QLineF(x - w * 0.707, y - w * 0.707, x + w * 0.707, y + w * 0.707));
    painter->drawLine(
        QLineF(x - w * 0.707, y + w * 0.707, x + w * 0.707, y - w * 0.707));
    break;
  }
  case ssTriangle: {
    QPointF lineArray[3] = {QPointF(x - w, y + 0.755 * w),
                            QPointF(x + w, y + 0.755 * w),
                            QPointF(x, y - 0.977 * w)};
    painter->drawPolygon(lineArray, 3);
    break;
  }
  case ssTriangleInverted: {
    QPointF lineArray[3] = {QPointF(x - w, y - 0.755 * w),
                            QPointF(x + w, y - 0.755 * w),
                            QPointF(x, y + 0.977 * w)};
    painter->drawPolygon(lineArray, 3);
    break;
  }
  case ssCrossSquare: {
    painter->drawRect(QRectF(x - w, y - w, mSize, mSize));
    painter->drawLine(QLineF(x - w, y - w, x + w * 0.95, y + w * 0.95));
    painter->drawLine(QLineF(x - w, y + w * 0.95, x + w * 0.95, y - w));
    break;
  }
  case ssPlusSquare: {
    painter->drawRect(QRectF(x - w, y - w, mSize, mSize));
    painter->drawLine(QLineF(x - w, y, x + w * 0.95, y));
    painter->drawLine(QLineF(x, y + w, x, y - w));
    break;
  }
  case ssCrossCircle: {
    painter->drawEllipse(QPointF(x, y), w, w);
    painter->drawLine(
        QLineF(x - w * 0.707, y - w * 0.707, x + w * 0.670, y + w * 0.670));
    painter->drawLine(
        QLineF(x - w * 0.707, y + w * 0.670, x + w * 0.670, y - w * 0.707));
    break;
  }
  case ssPlusCircle: {
    painter->drawEllipse(QPointF(x, y), w, w);
    painter->drawLine(QLineF(x - w, y, x + w, y));
    painter->drawLine(QLineF(x, y + w, x, y - w));
    break;
  }
  case ssPeace: {
    painter->drawEllipse(QPointF(x, y), w, w);
    painter->drawLine(QLineF(x, y - w, x, y + w));
    painter->drawLine(QLineF(x, y, x - w * 0.707, y + w * 0.707));
    painter->drawLine(QLineF(x, y, x + w * 0.707, y + w * 0.707));
    break;
  }
  case ssPixmap: {
    const double widthHalf = mPixmap.width() * 0.5;
    const double heightHalf = mPixmap.height() * 0.5;
#if QT_VERSION < QT_VERSION_CHECK(4, 8, 0)
    const QRectF clipRect = painter->clipRegion().boundingRect().adjusted(
        -widthHalf, -heightHalf, widthHalf, heightHalf);
#else
    const QRectF clipRect = painter->clipBoundingRect().adjusted(
        -widthHalf, -heightHalf, widthHalf, heightHalf);
#endif
    if (clipRect.contains(x, y))
      painter->drawPixmap(x - widthHalf, y - heightHalf, mPixmap);
    break;
  }
  case ssCustom: {
    QTransform oldTransform = painter->transform();
    painter->translate(x, y);
    painter->scale(mSize / 6.0, mSize / 6.0);
    painter->drawPath(mCustomPath);
    painter->setTransform(oldTransform);
    break;
  }
  }
}

QCPSelectionDecorator::QCPSelectionDecorator()
    : mPen(QColor(80, 80, 255), 2.5), mBrush(Qt::NoBrush), mScatterStyle(),
      mUsedScatterProperties(QCPScatterStyle::spNone), mPlottable(0) {}

QCPSelectionDecorator::~QCPSelectionDecorator() {}

void QCPSelectionDecorator::setPen(const QPen &pen) { mPen = pen; }

void QCPSelectionDecorator::setBrush(const QBrush &brush) { mBrush = brush; }

void QCPSelectionDecorator::setScatterStyle(
    const QCPScatterStyle &scatterStyle,
    QCPScatterStyle::ScatterProperties usedProperties) {
  mScatterStyle = scatterStyle;
  setUsedScatterProperties(usedProperties);
}

void QCPSelectionDecorator::setUsedScatterProperties(
    const QCPScatterStyle::ScatterProperties &properties) {
  mUsedScatterProperties = properties;
}

void QCPSelectionDecorator::applyPen(QCPPainter *painter) const {
  painter->setPen(mPen);
}

void QCPSelectionDecorator::applyBrush(QCPPainter *painter) const {
  painter->setBrush(mBrush);
}

QCPScatterStyle QCPSelectionDecorator::getFinalScatterStyle(
    const QCPScatterStyle &unselectedStyle) const {
  QCPScatterStyle result(unselectedStyle);
  result.setFromOther(mScatterStyle, mUsedScatterProperties);

  if (!result.isPenDefined())
    result.setPen(mPen);

  return result;
}

void QCPSelectionDecorator::copyFrom(const QCPSelectionDecorator *other) {
  setPen(other->pen());
  setBrush(other->brush());
  setScatterStyle(other->scatterStyle(), other->usedScatterProperties());
}

void QCPSelectionDecorator::drawDecoration(QCPPainter *painter,
                                           QCPDataSelection selection) {
  Q_UNUSED(painter)
  Q_UNUSED(selection)
}

bool QCPSelectionDecorator::registerWithPlottable(
    QCPAbstractPlottable *plottable) {
  if (!mPlottable) {
    mPlottable = plottable;
    return true;
  } else {
    qDebug() << Q_FUNC_INFO
             << "This selection decorator is already registered with plottable:"
             << reinterpret_cast<quintptr>(mPlottable);
    return false;
  }
}

QCPAbstractPlottable::QCPAbstractPlottable(QCPAxis *keyAxis, QCPAxis *valueAxis)
    : QCPLayerable(keyAxis->parentPlot(), QString(), keyAxis->axisRect()),
      mName(), mAntialiasedFill(true), mAntialiasedScatters(true),
      mPen(Qt::black), mBrush(Qt::NoBrush), mKeyAxis(keyAxis),
      mValueAxis(valueAxis), mSelectable(QCP::stWhole), mSelectionDecorator(0) {
  if (keyAxis->parentPlot() != valueAxis->parentPlot())
    qDebug() << Q_FUNC_INFO
             << "Parent plot of keyAxis is not the same as that of valueAxis.";
  if (keyAxis->orientation() == valueAxis->orientation())
    qDebug() << Q_FUNC_INFO
             << "keyAxis and valueAxis must be orthogonal to each other.";

  mParentPlot->registerPlottable(this);
  setSelectionDecorator(new QCPSelectionDecorator);
}

QCPAbstractPlottable::~QCPAbstractPlottable() {
  if (mSelectionDecorator) {
    delete mSelectionDecorator;
    mSelectionDecorator = 0;
  }
}

void QCPAbstractPlottable::setName(const QString &name) { mName = name; }

void QCPAbstractPlottable::setAntialiasedFill(bool enabled) {
  mAntialiasedFill = enabled;
}

void QCPAbstractPlottable::setAntialiasedScatters(bool enabled) {
  mAntialiasedScatters = enabled;
}

void QCPAbstractPlottable::setPen(const QPen &pen) { mPen = pen; }

void QCPAbstractPlottable::setBrush(const QBrush &brush) { mBrush = brush; }

void QCPAbstractPlottable::setKeyAxis(QCPAxis *axis) { mKeyAxis = axis; }

void QCPAbstractPlottable::setValueAxis(QCPAxis *axis) { mValueAxis = axis; }

void QCPAbstractPlottable::setSelection(QCPDataSelection selection) {
  selection.enforceType(mSelectable);
  if (mSelection != selection) {
    mSelection = selection;
    Q_EMIT selectionChanged(selected());
    Q_EMIT selectionChanged(mSelection);
  }
}

void QCPAbstractPlottable::setSelectionDecorator(
    QCPSelectionDecorator *decorator) {
  if (decorator) {
    if (decorator->registerWithPlottable(this)) {
      if (mSelectionDecorator)

        delete mSelectionDecorator;
      mSelectionDecorator = decorator;
    }
  } else if (mSelectionDecorator)

  {
    delete mSelectionDecorator;
    mSelectionDecorator = 0;
  }
}

void QCPAbstractPlottable::setSelectable(QCP::SelectionType selectable) {
  if (mSelectable != selectable) {
    mSelectable = selectable;
    QCPDataSelection oldSelection = mSelection;
    mSelection.enforceType(mSelectable);
    Q_EMIT selectableChanged(mSelectable);
    if (mSelection != oldSelection) {
      Q_EMIT selectionChanged(selected());
      Q_EMIT selectionChanged(mSelection);
    }
  }
}

void QCPAbstractPlottable::coordsToPixels(double key, double value, double &x,
                                          double &y) const {
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return;
  }

  if (keyAxis->orientation() == Qt::Horizontal) {
    x = keyAxis->coordToPixel(key);
    y = valueAxis->coordToPixel(value);
  } else {
    y = keyAxis->coordToPixel(key);
    x = valueAxis->coordToPixel(value);
  }
}

const QPointF QCPAbstractPlottable::coordsToPixels(double key,
                                                   double value) const {
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return QPointF();
  }

  if (keyAxis->orientation() == Qt::Horizontal)
    return QPointF(keyAxis->coordToPixel(key), valueAxis->coordToPixel(value));
  else
    return QPointF(valueAxis->coordToPixel(value), keyAxis->coordToPixel(key));
}

void QCPAbstractPlottable::pixelsToCoords(double x, double y, double &key,
                                          double &value) const {
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return;
  }

  if (keyAxis->orientation() == Qt::Horizontal) {
    key = keyAxis->pixelToCoord(x);
    value = valueAxis->pixelToCoord(y);
  } else {
    key = keyAxis->pixelToCoord(y);
    value = valueAxis->pixelToCoord(x);
  }
}

void QCPAbstractPlottable::pixelsToCoords(const QPointF &pixelPos, double &key,
                                          double &value) const {
  pixelsToCoords(pixelPos.x(), pixelPos.y(), key, value);
}

void QCPAbstractPlottable::rescaleAxes(bool onlyEnlarge) const {
  rescaleKeyAxis(onlyEnlarge);
  rescaleValueAxis(onlyEnlarge);
}

void QCPAbstractPlottable::rescaleKeyAxis(bool onlyEnlarge) const {
  QCPAxis *keyAxis = mKeyAxis.data();
  if (!keyAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key axis";
    return;
  }

  QCP::SignDomain signDomain = QCP::sdBoth;
  if (keyAxis->scaleType() == QCPAxis::stLogarithmic)
    signDomain =
        (keyAxis->range().upper < 0 ? QCP::sdNegative : QCP::sdPositive);

  bool foundRange;
  QCPRange newRange = getKeyRange(foundRange, signDomain);
  if (foundRange) {
    if (onlyEnlarge)
      newRange.expand(keyAxis->range());
    if (!QCPRange::validRange(newRange))

    {
      double center = (newRange.lower + newRange.upper) * 0.5;

      if (keyAxis->scaleType() == QCPAxis::stLinear) {
        newRange.lower = center - keyAxis->range().size() / 2.0;
        newRange.upper = center + keyAxis->range().size() / 2.0;
      } else

      {
        newRange.lower =
            center / qSqrt(keyAxis->range().upper / keyAxis->range().lower);
        newRange.upper =
            center * qSqrt(keyAxis->range().upper / keyAxis->range().lower);
      }
    }
    keyAxis->setRange(newRange);
  }
}

void QCPAbstractPlottable::rescaleValueAxis(bool onlyEnlarge,
                                            bool inKeyRange) const {
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return;
  }

  QCP::SignDomain signDomain = QCP::sdBoth;
  if (valueAxis->scaleType() == QCPAxis::stLogarithmic)
    signDomain =
        (valueAxis->range().upper < 0 ? QCP::sdNegative : QCP::sdPositive);

  bool foundRange;
  QCPRange newRange = getValueRange(foundRange, signDomain,
                                    inKeyRange ? keyAxis->range() : QCPRange());
  if (foundRange) {
    if (onlyEnlarge)
      newRange.expand(valueAxis->range());
    if (!QCPRange::validRange(newRange))

    {
      double center = (newRange.lower + newRange.upper) * 0.5;

      if (valueAxis->scaleType() == QCPAxis::stLinear) {
        newRange.lower = center - valueAxis->range().size() / 2.0;
        newRange.upper = center + valueAxis->range().size() / 2.0;
      } else

      {
        newRange.lower =
            center / qSqrt(valueAxis->range().upper / valueAxis->range().lower);
        newRange.upper =
            center * qSqrt(valueAxis->range().upper / valueAxis->range().lower);
      }
    }
    valueAxis->setRange(newRange);
  }
}

bool QCPAbstractPlottable::addToLegend(QCPLegend *legend) {
  if (!legend) {
    qDebug() << Q_FUNC_INFO << "passed legend is null";
    return false;
  }
  if (legend->parentPlot() != mParentPlot) {
    qDebug() << Q_FUNC_INFO
             << "passed legend isn't in the same QCustomPlot as this plottable";
    return false;
  }

  if (!legend->hasItemWithPlottable(this)) {
    legend->addItem(new QCPPlottableLegendItem(legend, this));
    return true;
  } else
    return false;
}

bool QCPAbstractPlottable::addToLegend() {
  if (!mParentPlot || !mParentPlot->legend)
    return false;
  else
    return addToLegend(mParentPlot->legend);
}

bool QCPAbstractPlottable::removeFromLegend(QCPLegend *legend) const {
  if (!legend) {
    qDebug() << Q_FUNC_INFO << "passed legend is null";
    return false;
  }

  if (QCPPlottableLegendItem *lip = legend->itemWithPlottable(this))
    return legend->removeItem(lip);
  else
    return false;
}

bool QCPAbstractPlottable::removeFromLegend() const {
  if (!mParentPlot || !mParentPlot->legend)
    return false;
  else
    return removeFromLegend(mParentPlot->legend);
}

QRect QCPAbstractPlottable::clipRect() const {
  if (mKeyAxis && mValueAxis)
    return mKeyAxis.data()->axisRect()->rect() &
           mValueAxis.data()->axisRect()->rect();
  else
    return QRect();
}

QCP::Interaction QCPAbstractPlottable::selectionCategory() const {
  return QCP::iSelectPlottables;
}

void QCPAbstractPlottable::applyDefaultAntialiasingHint(
    QCPPainter *painter) const {
  applyAntialiasingHint(painter, mAntialiased, QCP::aePlottables);
}

void QCPAbstractPlottable::applyFillAntialiasingHint(
    QCPPainter *painter) const {
  applyAntialiasingHint(painter, mAntialiasedFill, QCP::aeFills);
}

void QCPAbstractPlottable::applyScattersAntialiasingHint(
    QCPPainter *painter) const {
  applyAntialiasingHint(painter, mAntialiasedScatters, QCP::aeScatters);
}

void QCPAbstractPlottable::selectEvent(QMouseEvent *event, bool additive,
                                       const QVariant &details,
                                       bool *selectionStateChanged) {
  Q_UNUSED(event)

  if (mSelectable != QCP::stNone) {
    QCPDataSelection newSelection = details.value<QCPDataSelection>();
    QCPDataSelection selectionBefore = mSelection;
    if (additive) {
      if (mSelectable == QCP::stWhole)

      {
        if (selected())
          setSelection(QCPDataSelection());
        else
          setSelection(newSelection);
      } else

      {
        if (mSelection.contains(newSelection))

          setSelection(mSelection - newSelection);
        else
          setSelection(mSelection + newSelection);
      }
    } else
      setSelection(newSelection);
    if (selectionStateChanged)
      *selectionStateChanged = mSelection != selectionBefore;
  }
}

void QCPAbstractPlottable::deselectEvent(bool *selectionStateChanged) {
  if (mSelectable != QCP::stNone) {
    QCPDataSelection selectionBefore = mSelection;
    setSelection(QCPDataSelection());
    if (selectionStateChanged)
      *selectionStateChanged = mSelection != selectionBefore;
  }
}

QCPItemAnchor::QCPItemAnchor(QCustomPlot *parentPlot,
                             QCPAbstractItem *parentItem, const QString &name,
                             int anchorId)
    : mName(name), mParentPlot(parentPlot), mParentItem(parentItem),
      mAnchorId(anchorId) {}

QCPItemAnchor::~QCPItemAnchor() {
  Q_FOREACH (QCPItemPosition *child, mChildrenX.toList()) {
    if (child->parentAnchorX() == this)
      child->setParentAnchorX(0);
  }
  Q_FOREACH (QCPItemPosition *child, mChildrenY.toList()) {
    if (child->parentAnchorY() == this)
      child->setParentAnchorY(0);
  }
}

QPointF QCPItemAnchor::pixelPosition() const {
  if (mParentItem) {
    if (mAnchorId > -1) {
      return mParentItem->anchorPixelPosition(mAnchorId);
    } else {
      qDebug() << Q_FUNC_INFO << "no valid anchor id set:" << mAnchorId;
      return QPointF();
    }
  } else {
    qDebug() << Q_FUNC_INFO << "no parent item set";
    return QPointF();
  }
}

void QCPItemAnchor::addChildX(QCPItemPosition *pos) {
  if (!mChildrenX.contains(pos))
    mChildrenX.insert(pos);
  else
    qDebug() << Q_FUNC_INFO << "provided pos is child already"
             << reinterpret_cast<quintptr>(pos);
}

void QCPItemAnchor::removeChildX(QCPItemPosition *pos) {
  if (!mChildrenX.remove(pos))
    qDebug() << Q_FUNC_INFO << "provided pos isn't child"
             << reinterpret_cast<quintptr>(pos);
}

void QCPItemAnchor::addChildY(QCPItemPosition *pos) {
  if (!mChildrenY.contains(pos))
    mChildrenY.insert(pos);
  else
    qDebug() << Q_FUNC_INFO << "provided pos is child already"
             << reinterpret_cast<quintptr>(pos);
}

void QCPItemAnchor::removeChildY(QCPItemPosition *pos) {
  if (!mChildrenY.remove(pos))
    qDebug() << Q_FUNC_INFO << "provided pos isn't child"
             << reinterpret_cast<quintptr>(pos);
}

QCPItemPosition::QCPItemPosition(QCustomPlot *parentPlot,
                                 QCPAbstractItem *parentItem,
                                 const QString &name)
    : QCPItemAnchor(parentPlot, parentItem, name), mPositionTypeX(ptAbsolute),
      mPositionTypeY(ptAbsolute), mKey(0), mValue(0), mParentAnchorX(0),
      mParentAnchorY(0) {}

QCPItemPosition::~QCPItemPosition() {
  Q_FOREACH (QCPItemPosition *child, mChildrenX.toList()) {
    if (child->parentAnchorX() == this)
      child->setParentAnchorX(0);
  }
  Q_FOREACH (QCPItemPosition *child, mChildrenY.toList()) {
    if (child->parentAnchorY() == this)
      child->setParentAnchorY(0);
  }

  if (mParentAnchorX)
    mParentAnchorX->removeChildX(this);
  if (mParentAnchorY)
    mParentAnchorY->removeChildY(this);
}

QCPAxisRect *QCPItemPosition::axisRect() const { return mAxisRect.data(); }

void QCPItemPosition::setType(QCPItemPosition::PositionType type) {
  setTypeX(type);
  setTypeY(type);
}

void QCPItemPosition::setTypeX(QCPItemPosition::PositionType type) {
  if (mPositionTypeX != type) {
    bool retainPixelPosition = true;
    if ((mPositionTypeX == ptPlotCoords || type == ptPlotCoords) &&
        (!mKeyAxis || !mValueAxis))
      retainPixelPosition = false;
    if ((mPositionTypeX == ptAxisRectRatio || type == ptAxisRectRatio) &&
        (!mAxisRect))
      retainPixelPosition = false;

    QPointF pixel;
    if (retainPixelPosition)
      pixel = pixelPosition();

    mPositionTypeX = type;

    if (retainPixelPosition)
      setPixelPosition(pixel);
  }
}

void QCPItemPosition::setTypeY(QCPItemPosition::PositionType type) {
  if (mPositionTypeY != type) {
    bool retainPixelPosition = true;
    if ((mPositionTypeY == ptPlotCoords || type == ptPlotCoords) &&
        (!mKeyAxis || !mValueAxis))
      retainPixelPosition = false;
    if ((mPositionTypeY == ptAxisRectRatio || type == ptAxisRectRatio) &&
        (!mAxisRect))
      retainPixelPosition = false;

    QPointF pixel;
    if (retainPixelPosition)
      pixel = pixelPosition();

    mPositionTypeY = type;

    if (retainPixelPosition)
      setPixelPosition(pixel);
  }
}

bool QCPItemPosition::setParentAnchor(QCPItemAnchor *parentAnchor,
                                      bool keepPixelPosition) {
  bool successX = setParentAnchorX(parentAnchor, keepPixelPosition);
  bool successY = setParentAnchorY(parentAnchor, keepPixelPosition);
  return successX && successY;
}

bool QCPItemPosition::setParentAnchorX(QCPItemAnchor *parentAnchor,
                                       bool keepPixelPosition) {
  if (parentAnchor == this) {
    qDebug() << Q_FUNC_INFO << "can't set self as parent anchor"
             << reinterpret_cast<quintptr>(parentAnchor);
    return false;
  }

  QCPItemAnchor *currentParent = parentAnchor;
  while (currentParent) {
    if (QCPItemPosition *currentParentPos =
            currentParent->toQCPItemPosition()) {
      if (currentParentPos == this) {
        qDebug() << Q_FUNC_INFO
                 << "can't create recursive parent-child-relationship"
                 << reinterpret_cast<quintptr>(parentAnchor);
        return false;
      }
      currentParent = currentParentPos->parentAnchorX();
    } else {
      if (currentParent->mParentItem == mParentItem) {
        qDebug() << Q_FUNC_INFO
                 << "can't set parent to be an anchor which itself depends on "
                    "this position"
                 << reinterpret_cast<quintptr>(parentAnchor);
        return false;
      }
      break;
    }
  }

  if (!mParentAnchorX && mPositionTypeX == ptPlotCoords)
    setTypeX(ptAbsolute);

  QPointF pixelP;
  if (keepPixelPosition)
    pixelP = pixelPosition();

  if (mParentAnchorX)
    mParentAnchorX->removeChildX(this);

  if (parentAnchor)
    parentAnchor->addChildX(this);
  mParentAnchorX = parentAnchor;

  if (keepPixelPosition)
    setPixelPosition(pixelP);
  else
    setCoords(0, coords().y());
  return true;
}

bool QCPItemPosition::setParentAnchorY(QCPItemAnchor *parentAnchor,
                                       bool keepPixelPosition) {
  if (parentAnchor == this) {
    qDebug() << Q_FUNC_INFO << "can't set self as parent anchor"
             << reinterpret_cast<quintptr>(parentAnchor);
    return false;
  }

  QCPItemAnchor *currentParent = parentAnchor;
  while (currentParent) {
    if (QCPItemPosition *currentParentPos =
            currentParent->toQCPItemPosition()) {
      if (currentParentPos == this) {
        qDebug() << Q_FUNC_INFO
                 << "can't create recursive parent-child-relationship"
                 << reinterpret_cast<quintptr>(parentAnchor);
        return false;
      }
      currentParent = currentParentPos->parentAnchorY();
    } else {
      if (currentParent->mParentItem == mParentItem) {
        qDebug() << Q_FUNC_INFO
                 << "can't set parent to be an anchor which itself depends on "
                    "this position"
                 << reinterpret_cast<quintptr>(parentAnchor);
        return false;
      }
      break;
    }
  }

  if (!mParentAnchorY && mPositionTypeY == ptPlotCoords)
    setTypeY(ptAbsolute);

  QPointF pixelP;
  if (keepPixelPosition)
    pixelP = pixelPosition();

  if (mParentAnchorY)
    mParentAnchorY->removeChildY(this);

  if (parentAnchor)
    parentAnchor->addChildY(this);
  mParentAnchorY = parentAnchor;

  if (keepPixelPosition)
    setPixelPosition(pixelP);
  else
    setCoords(coords().x(), 0);
  return true;
}

void QCPItemPosition::setCoords(double key, double value) {
  mKey = key;
  mValue = value;
}

void QCPItemPosition::setCoords(const QPointF &pos) {
  setCoords(pos.x(), pos.y());
}

QPointF QCPItemPosition::pixelPosition() const {
  QPointF result;

  switch (mPositionTypeX) {
  case ptAbsolute: {
    result.rx() = mKey;
    if (mParentAnchorX)
      result.rx() += mParentAnchorX->pixelPosition().x();
    break;
  }
  case ptViewportRatio: {
    result.rx() = mKey * mParentPlot->viewport().width();
    if (mParentAnchorX)
      result.rx() += mParentAnchorX->pixelPosition().x();
    else
      result.rx() += mParentPlot->viewport().left();
    break;
  }
  case ptAxisRectRatio: {
    if (mAxisRect) {
      result.rx() = mKey * mAxisRect.data()->width();
      if (mParentAnchorX)
        result.rx() += mParentAnchorX->pixelPosition().x();
      else
        result.rx() += mAxisRect.data()->left();
    } else
      qDebug() << Q_FUNC_INFO
               << "Item position type x is ptAxisRectRatio, but no axis rect "
                  "was defined";
    break;
  }
  case ptPlotCoords: {
    if (mKeyAxis && mKeyAxis.data()->orientation() == Qt::Horizontal)
      result.rx() = mKeyAxis.data()->coordToPixel(mKey);
    else if (mValueAxis && mValueAxis.data()->orientation() == Qt::Horizontal)
      result.rx() = mValueAxis.data()->coordToPixel(mValue);
    else
      qDebug()
          << Q_FUNC_INFO
          << "Item position type x is ptPlotCoords, but no axes were defined";
    break;
  }
  }

  switch (mPositionTypeY) {
  case ptAbsolute: {
    result.ry() = mValue;
    if (mParentAnchorY)
      result.ry() += mParentAnchorY->pixelPosition().y();
    break;
  }
  case ptViewportRatio: {
    result.ry() = mValue * mParentPlot->viewport().height();
    if (mParentAnchorY)
      result.ry() += mParentAnchorY->pixelPosition().y();
    else
      result.ry() += mParentPlot->viewport().top();
    break;
  }
  case ptAxisRectRatio: {
    if (mAxisRect) {
      result.ry() = mValue * mAxisRect.data()->height();
      if (mParentAnchorY)
        result.ry() += mParentAnchorY->pixelPosition().y();
      else
        result.ry() += mAxisRect.data()->top();
    } else
      qDebug() << Q_FUNC_INFO
               << "Item position type y is ptAxisRectRatio, but no axis rect "
                  "was defined";
    break;
  }
  case ptPlotCoords: {
    if (mKeyAxis && mKeyAxis.data()->orientation() == Qt::Vertical)
      result.ry() = mKeyAxis.data()->coordToPixel(mKey);
    else if (mValueAxis && mValueAxis.data()->orientation() == Qt::Vertical)
      result.ry() = mValueAxis.data()->coordToPixel(mValue);
    else
      qDebug()
          << Q_FUNC_INFO
          << "Item position type y is ptPlotCoords, but no axes were defined";
    break;
  }
  }

  return result;
}

void QCPItemPosition::setAxes(QCPAxis *keyAxis, QCPAxis *valueAxis) {
  mKeyAxis = keyAxis;
  mValueAxis = valueAxis;
}

void QCPItemPosition::setAxisRect(QCPAxisRect *axisRect) {
  mAxisRect = axisRect;
}

void QCPItemPosition::setPixelPosition(const QPointF &pixelPosition) {
  double x = pixelPosition.x();
  double y = pixelPosition.y();

  switch (mPositionTypeX) {
  case ptAbsolute: {
    if (mParentAnchorX)
      x -= mParentAnchorX->pixelPosition().x();
    break;
  }
  case ptViewportRatio: {
    if (mParentAnchorX)
      x -= mParentAnchorX->pixelPosition().x();
    else
      x -= mParentPlot->viewport().left();
    x /= (double)mParentPlot->viewport().width();
    break;
  }
  case ptAxisRectRatio: {
    if (mAxisRect) {
      if (mParentAnchorX)
        x -= mParentAnchorX->pixelPosition().x();
      else
        x -= mAxisRect.data()->left();
      x /= (double)mAxisRect.data()->width();
    } else
      qDebug() << Q_FUNC_INFO
               << "Item position type x is ptAxisRectRatio, but no axis rect "
                  "was defined";
    break;
  }
  case ptPlotCoords: {
    if (mKeyAxis && mKeyAxis.data()->orientation() == Qt::Horizontal)
      x = mKeyAxis.data()->pixelToCoord(x);
    else if (mValueAxis && mValueAxis.data()->orientation() == Qt::Horizontal)
      y = mValueAxis.data()->pixelToCoord(x);
    else
      qDebug()
          << Q_FUNC_INFO
          << "Item position type x is ptPlotCoords, but no axes were defined";
    break;
  }
  }

  switch (mPositionTypeY) {
  case ptAbsolute: {
    if (mParentAnchorY)
      y -= mParentAnchorY->pixelPosition().y();
    break;
  }
  case ptViewportRatio: {
    if (mParentAnchorY)
      y -= mParentAnchorY->pixelPosition().y();
    else
      y -= mParentPlot->viewport().top();
    y /= (double)mParentPlot->viewport().height();
    break;
  }
  case ptAxisRectRatio: {
    if (mAxisRect) {
      if (mParentAnchorY)
        y -= mParentAnchorY->pixelPosition().y();
      else
        y -= mAxisRect.data()->top();
      y /= (double)mAxisRect.data()->height();
    } else
      qDebug() << Q_FUNC_INFO
               << "Item position type y is ptAxisRectRatio, but no axis rect "
                  "was defined";
    break;
  }
  case ptPlotCoords: {
    if (mKeyAxis && mKeyAxis.data()->orientation() == Qt::Vertical)
      x = mKeyAxis.data()->pixelToCoord(y);
    else if (mValueAxis && mValueAxis.data()->orientation() == Qt::Vertical)
      y = mValueAxis.data()->pixelToCoord(y);
    else
      qDebug()
          << Q_FUNC_INFO
          << "Item position type y is ptPlotCoords, but no axes were defined";
    break;
  }
  }

  setCoords(x, y);
}

QCPAbstractItem::QCPAbstractItem(QCustomPlot *parentPlot)
    : QCPLayerable(parentPlot), mClipToAxisRect(false), mSelectable(true),
      mSelected(false) {
  parentPlot->registerItem(this);

  QList<QCPAxisRect *> rects = parentPlot->axisRects();
  if (rects.size() > 0) {
    setClipToAxisRect(true);
    setClipAxisRect(rects.first());
  }
}

QCPAbstractItem::~QCPAbstractItem() { qDeleteAll(mAnchors); }

QCPAxisRect *QCPAbstractItem::clipAxisRect() const {
  return mClipAxisRect.data();
}

void QCPAbstractItem::setClipToAxisRect(bool clip) {
  mClipToAxisRect = clip;
  if (mClipToAxisRect)
    setParentLayerable(mClipAxisRect.data());
}

void QCPAbstractItem::setClipAxisRect(QCPAxisRect *rect) {
  mClipAxisRect = rect;
  if (mClipToAxisRect)
    setParentLayerable(mClipAxisRect.data());
}

void QCPAbstractItem::setSelectable(bool selectable) {
  if (mSelectable != selectable) {
    mSelectable = selectable;
    Q_EMIT selectableChanged(mSelectable);
  }
}

void QCPAbstractItem::setSelected(bool selected) {
  if (mSelected != selected) {
    mSelected = selected;
    Q_EMIT selectionChanged(mSelected);
  }
}

QCPItemPosition *QCPAbstractItem::position(const QString &name) const {
  for (int i = 0; i < mPositions.size(); ++i) {
    if (mPositions.at(i)->name() == name)
      return mPositions.at(i);
  }
  qDebug() << Q_FUNC_INFO << "position with name not found:" << name;
  return 0;
}

QCPItemAnchor *QCPAbstractItem::anchor(const QString &name) const {
  for (int i = 0; i < mAnchors.size(); ++i) {
    if (mAnchors.at(i)->name() == name)
      return mAnchors.at(i);
  }
  qDebug() << Q_FUNC_INFO << "anchor with name not found:" << name;
  return 0;
}

bool QCPAbstractItem::hasAnchor(const QString &name) const {
  for (int i = 0; i < mAnchors.size(); ++i) {
    if (mAnchors.at(i)->name() == name)
      return true;
  }
  return false;
}

QRect QCPAbstractItem::clipRect() const {
  if (mClipToAxisRect && mClipAxisRect)
    return mClipAxisRect.data()->rect();
  else
    return mParentPlot->viewport();
}

void QCPAbstractItem::applyDefaultAntialiasingHint(QCPPainter *painter) const {
  applyAntialiasingHint(painter, mAntialiased, QCP::aeItems);
}

double QCPAbstractItem::rectDistance(const QRectF &rect, const QPointF &pos,
                                     bool filledRect) const {
  double result = -1;

  QList<QLineF> lines;
  lines << QLineF(rect.topLeft(), rect.topRight())
        << QLineF(rect.bottomLeft(), rect.bottomRight())
        << QLineF(rect.topLeft(), rect.bottomLeft())
        << QLineF(rect.topRight(), rect.bottomRight());
  double minDistSqr = (std::numeric_limits<double>::max)();
  for (int i = 0; i < lines.size(); ++i) {
    double distSqr = QCPVector2D(pos).distanceSquaredToLine(lines.at(i).p1(),
                                                            lines.at(i).p2());
    if (distSqr < minDistSqr)
      minDistSqr = distSqr;
  }
  result = qSqrt(minDistSqr);

  if (filledRect && result > mParentPlot->selectionTolerance() * 0.99) {
    if (rect.contains(pos))
      result = mParentPlot->selectionTolerance() * 0.99;
  }
  return result;
}

QPointF QCPAbstractItem::anchorPixelPosition(int anchorId) const {
  qDebug() << Q_FUNC_INFO
           << "called on item which shouldn't have any anchors (this method "
              "not reimplemented). anchorId"
           << anchorId;
  return QPointF();
}

QCPItemPosition *QCPAbstractItem::createPosition(const QString &name) {
  if (hasAnchor(name))
    qDebug() << Q_FUNC_INFO
             << "anchor/position with name exists already:" << name;
  QCPItemPosition *newPosition = new QCPItemPosition(mParentPlot, this, name);
  mPositions.append(newPosition);
  mAnchors.append(newPosition);

  newPosition->setAxes(mParentPlot->xAxis, mParentPlot->yAxis);
  newPosition->setType(QCPItemPosition::ptPlotCoords);
  if (mParentPlot->axisRect())
    newPosition->setAxisRect(mParentPlot->axisRect());
  newPosition->setCoords(0, 0);
  return newPosition;
}

QCPItemAnchor *QCPAbstractItem::createAnchor(const QString &name,
                                             int anchorId) {
  if (hasAnchor(name))
    qDebug() << Q_FUNC_INFO
             << "anchor/position with name exists already:" << name;
  QCPItemAnchor *newAnchor =
      new QCPItemAnchor(mParentPlot, this, name, anchorId);
  mAnchors.append(newAnchor);
  return newAnchor;
}

void QCPAbstractItem::selectEvent(QMouseEvent *event, bool additive,
                                  const QVariant &details,
                                  bool *selectionStateChanged) {
  Q_UNUSED(event)
  Q_UNUSED(details)
  if (mSelectable) {
    bool selBefore = mSelected;
    setSelected(additive ? !mSelected : true);
    if (selectionStateChanged)
      *selectionStateChanged = mSelected != selBefore;
  }
}

void QCPAbstractItem::deselectEvent(bool *selectionStateChanged) {
  if (mSelectable) {
    bool selBefore = mSelected;
    setSelected(false);
    if (selectionStateChanged)
      *selectionStateChanged = mSelected != selBefore;
  }
}

QCP::Interaction QCPAbstractItem::selectionCategory() const {
  return QCP::iSelectItems;
}

QCustomPlot::QCustomPlot(QWidget *parent)
    : QWidget(parent), xAxis(0), yAxis(0), xAxis2(0), yAxis2(0), legend(0),
      mBufferDevicePixelRatio(1.0),

      mPlotLayout(0), mAutoAddPlottableToLegend(true),
      mAntialiasedElements(QCP::aeNone), mNotAntialiasedElements(QCP::aeNone),
      mInteractions(0), mSelectionTolerance(8), mNoAntialiasingOnDrag(false),
      mBackgroundBrush(Qt::white, Qt::SolidPattern), mBackgroundScaled(true),
      mBackgroundScaledMode(Qt::KeepAspectRatioByExpanding), mCurrentLayer(0),
      mPlottingHints(QCP::phCacheLabels | QCP::phImmediateRefresh),
      mMultiSelectModifier(Qt::ControlModifier),
      mSelectionRectMode(QCP::srmNone), mSelectionRect(0), mOpenGl(false),
      mMouseHasMoved(false), mMouseEventLayerable(0), mMouseSignalLayerable(0),
      mReplotting(false), mReplotQueued(false), mOpenGlMultisamples(16),
      mOpenGlAntialiasedElementsBackup(QCP::aeNone),
      mOpenGlCacheLabelsBackup(true) {
  setAttribute(Qt::WA_NoMousePropagation);
  setAttribute(Qt::WA_OpaquePaintEvent);
  setFocusPolicy(Qt::ClickFocus);
  setMouseTracking(true);
  QLocale currentLocale = locale();
  currentLocale.setNumberOptions(QLocale::OmitGroupSeparator);
  setLocale(currentLocale);
#ifdef QCP_DEVICEPIXELRATIO_SUPPORTED
#ifdef QCP_DEVICEPIXELRATIO_FLOAT
  setBufferDevicePixelRatio(QWidget::devicePixelRatioF());
#else
  setBufferDevicePixelRatio(QWidget::devicePixelRatio());
#endif
#endif

  mOpenGlAntialiasedElementsBackup = mAntialiasedElements;
  mOpenGlCacheLabelsBackup = mPlottingHints.testFlag(QCP::phCacheLabels);

  mLayers.append(new QCPLayer(this, QLatin1String("background")));
  mLayers.append(new QCPLayer(this, QLatin1String("grid")));
  mLayers.append(new QCPLayer(this, QLatin1String("main")));
  mLayers.append(new QCPLayer(this, QLatin1String("axes")));
  mLayers.append(new QCPLayer(this, QLatin1String("legend")));
  mLayers.append(new QCPLayer(this, QLatin1String("overlay")));
  updateLayerIndices();
  setCurrentLayer(QLatin1String("main"));
  layer(QLatin1String("overlay"))->setMode(QCPLayer::lmBuffered);

  mPlotLayout = new QCPLayoutGrid;
  mPlotLayout->initializeParentPlot(this);
  mPlotLayout->setParent(this);

  mPlotLayout->setLayer(QLatin1String("main"));
  QCPAxisRect *defaultAxisRect = new QCPAxisRect(this, true);
  mPlotLayout->addElement(0, 0, defaultAxisRect);
  xAxis = defaultAxisRect->axis(QCPAxis::atBottom);
  yAxis = defaultAxisRect->axis(QCPAxis::atLeft);
  xAxis2 = defaultAxisRect->axis(QCPAxis::atTop);
  yAxis2 = defaultAxisRect->axis(QCPAxis::atRight);
  legend = new QCPLegend;
  legend->setVisible(false);
  defaultAxisRect->insetLayout()->addElement(legend,
                                             Qt::AlignRight | Qt::AlignTop);
  defaultAxisRect->insetLayout()->setMargins(QMargins(12, 12, 12, 12));

  defaultAxisRect->setLayer(QLatin1String("background"));
  xAxis->setLayer(QLatin1String("axes"));
  yAxis->setLayer(QLatin1String("axes"));
  xAxis2->setLayer(QLatin1String("axes"));
  yAxis2->setLayer(QLatin1String("axes"));
  xAxis->grid()->setLayer(QLatin1String("grid"));
  yAxis->grid()->setLayer(QLatin1String("grid"));
  xAxis2->grid()->setLayer(QLatin1String("grid"));
  yAxis2->grid()->setLayer(QLatin1String("grid"));
  legend->setLayer(QLatin1String("legend"));

  mSelectionRect = new QCPSelectionRect(this);
  mSelectionRect->setLayer(QLatin1String("overlay"));

  setViewport(rect());

  replot(rpQueuedReplot);
}

QCustomPlot::~QCustomPlot() {
  clearPlottables();
  clearItems();

  if (mPlotLayout) {
    delete mPlotLayout;
    mPlotLayout = 0;
  }

  mCurrentLayer = 0;
  qDeleteAll(mLayers);

  mLayers.clear();
}

void QCustomPlot::setAntialiasedElements(
    const QCP::AntialiasedElements &antialiasedElements) {
  mAntialiasedElements = antialiasedElements;

  if ((mNotAntialiasedElements & mAntialiasedElements) != 0)
    mNotAntialiasedElements |= ~mAntialiasedElements;
}

void QCustomPlot::setAntialiasedElement(
    QCP::AntialiasedElement antialiasedElement, bool enabled) {
  if (!enabled && mAntialiasedElements.testFlag(antialiasedElement))
    mAntialiasedElements &= ~antialiasedElement;
  else if (enabled && !mAntialiasedElements.testFlag(antialiasedElement))
    mAntialiasedElements |= antialiasedElement;

  if ((mNotAntialiasedElements & mAntialiasedElements) != 0)
    mNotAntialiasedElements |= ~mAntialiasedElements;
}

void QCustomPlot::setNotAntialiasedElements(
    const QCP::AntialiasedElements &notAntialiasedElements) {
  mNotAntialiasedElements = notAntialiasedElements;

  if ((mNotAntialiasedElements & mAntialiasedElements) != 0)
    mAntialiasedElements |= ~mNotAntialiasedElements;
}

void QCustomPlot::setNotAntialiasedElement(
    QCP::AntialiasedElement notAntialiasedElement, bool enabled) {
  if (!enabled && mNotAntialiasedElements.testFlag(notAntialiasedElement))
    mNotAntialiasedElements &= ~notAntialiasedElement;
  else if (enabled && !mNotAntialiasedElements.testFlag(notAntialiasedElement))
    mNotAntialiasedElements |= notAntialiasedElement;

  if ((mNotAntialiasedElements & mAntialiasedElements) != 0)
    mAntialiasedElements |= ~mNotAntialiasedElements;
}

void QCustomPlot::setAutoAddPlottableToLegend(bool on) {
  mAutoAddPlottableToLegend = on;
}

void QCustomPlot::setInteractions(const QCP::Interactions &interactions) {
  mInteractions = interactions;
}

void QCustomPlot::setInteraction(const QCP::Interaction &interaction,
                                 bool enabled) {
  if (!enabled && mInteractions.testFlag(interaction))
    mInteractions &= ~interaction;
  else if (enabled && !mInteractions.testFlag(interaction))
    mInteractions |= interaction;
}

void QCustomPlot::setSelectionTolerance(int pixels) {
  mSelectionTolerance = pixels;
}

void QCustomPlot::setNoAntialiasingOnDrag(bool enabled) {
  mNoAntialiasingOnDrag = enabled;
}

void QCustomPlot::setPlottingHints(const QCP::PlottingHints &hints) {
  mPlottingHints = hints;
}

void QCustomPlot::setPlottingHint(QCP::PlottingHint hint, bool enabled) {
  QCP::PlottingHints newHints = mPlottingHints;
  if (!enabled)
    newHints &= ~hint;
  else
    newHints |= hint;

  if (newHints != mPlottingHints)
    setPlottingHints(newHints);
}

void QCustomPlot::setMultiSelectModifier(Qt::KeyboardModifier modifier) {
  mMultiSelectModifier = modifier;
}

void QCustomPlot::setSelectionRectMode(QCP::SelectionRectMode mode) {
  if (mSelectionRect) {
    if (mode == QCP::srmNone)
      mSelectionRect->cancel();

    if (mSelectionRectMode == QCP::srmSelect)
      disconnect(mSelectionRect, SIGNAL(accepted(QRect, QMouseEvent *)), this,
                 SLOT(processRectSelection(QRect, QMouseEvent *)));
    else if (mSelectionRectMode == QCP::srmZoom)
      disconnect(mSelectionRect, SIGNAL(accepted(QRect, QMouseEvent *)), this,
                 SLOT(processRectZoom(QRect, QMouseEvent *)));

    if (mode == QCP::srmSelect)
      connect(mSelectionRect, SIGNAL(accepted(QRect, QMouseEvent *)), this,
              SLOT(processRectSelection(QRect, QMouseEvent *)));
    else if (mode == QCP::srmZoom)
      connect(mSelectionRect, SIGNAL(accepted(QRect, QMouseEvent *)), this,
              SLOT(processRectZoom(QRect, QMouseEvent *)));
  }

  mSelectionRectMode = mode;
}

void QCustomPlot::setSelectionRect(QCPSelectionRect *selectionRect) {
  if (mSelectionRect)
    delete mSelectionRect;

  mSelectionRect = selectionRect;

  if (mSelectionRect) {
    if (mSelectionRectMode == QCP::srmSelect)
      connect(mSelectionRect, SIGNAL(accepted(QRect, QMouseEvent *)), this,
              SLOT(processRectSelection(QRect, QMouseEvent *)));
    else if (mSelectionRectMode == QCP::srmZoom)
      connect(mSelectionRect, SIGNAL(accepted(QRect, QMouseEvent *)), this,
              SLOT(processRectZoom(QRect, QMouseEvent *)));
  }
}

void QCustomPlot::setOpenGl(bool enabled, int multisampling) {
  mOpenGlMultisamples = qMax(0, multisampling);
#ifdef QCUSTOMPLOT_USE_OPENGL
  mOpenGl = enabled;
  if (mOpenGl) {
    if (setupOpenGl()) {
      mOpenGlAntialiasedElementsBackup = mAntialiasedElements;
      mOpenGlCacheLabelsBackup = mPlottingHints.testFlag(QCP::phCacheLabels);

      setAntialiasedElements(QCP::aeAll);
      setPlottingHint(QCP::phCacheLabels, false);
    } else {
      qDebug() << Q_FUNC_INFO
               << "Failed to enable OpenGL, continuing plotting without "
                  "hardware acceleration.";
      mOpenGl = false;
    }
  } else {
    if (mAntialiasedElements == QCP::aeAll)
      setAntialiasedElements(mOpenGlAntialiasedElementsBackup);
    if (!mPlottingHints.testFlag(QCP::phCacheLabels))
      setPlottingHint(QCP::phCacheLabels, mOpenGlCacheLabelsBackup);
    freeOpenGl();
  }

  mPaintBuffers.clear();
  setupPaintBuffers();
#else
  Q_UNUSED(enabled)
  qDebug() << Q_FUNC_INFO
           << "QCustomPlot can't use OpenGL because QCUSTOMPLOT_USE_OPENGL was "
              "not defined during compilation (add 'DEFINES += "
              "QCUSTOMPLOT_USE_OPENGL' to your qmake .pro file)";
#endif
}

void QCustomPlot::setViewport(const QRect &rect) {
  mViewport = rect;
  if (mPlotLayout)
    mPlotLayout->setOuterRect(mViewport);
}

void QCustomPlot::setBufferDevicePixelRatio(double ratio) {
  if (!qFuzzyCompare(ratio, mBufferDevicePixelRatio)) {
#ifdef QCP_DEVICEPIXELRATIO_SUPPORTED
    mBufferDevicePixelRatio = ratio;
    for (int i = 0; i < mPaintBuffers.size(); ++i)
      mPaintBuffers.at(i)->setDevicePixelRatio(mBufferDevicePixelRatio);

#else
    qDebug() << Q_FUNC_INFO
             << "Device pixel ratios not supported for Qt versions before 5.4";
    mBufferDevicePixelRatio = 1.0;
#endif
  }
}

void QCustomPlot::setBackground(const QPixmap &pm) {
  mBackgroundPixmap = pm;
  mScaledBackgroundPixmap = QPixmap();
}

void QCustomPlot::setBackground(const QBrush &brush) {
  mBackgroundBrush = brush;
}

void QCustomPlot::setBackground(const QPixmap &pm, bool scaled,
                                Qt::AspectRatioMode mode) {
  mBackgroundPixmap = pm;
  mScaledBackgroundPixmap = QPixmap();
  mBackgroundScaled = scaled;
  mBackgroundScaledMode = mode;
}

void QCustomPlot::setBackgroundScaled(bool scaled) {
  mBackgroundScaled = scaled;
}

void QCustomPlot::setBackgroundScaledMode(Qt::AspectRatioMode mode) {
  mBackgroundScaledMode = mode;
}

QCPAbstractPlottable *QCustomPlot::plottable(int index) {
  if (index >= 0 && index < mPlottables.size()) {
    return mPlottables.at(index);
  } else {
    qDebug() << Q_FUNC_INFO << "index out of bounds:" << index;
    return 0;
  }
}

QCPAbstractPlottable *QCustomPlot::plottable() {
  if (!mPlottables.isEmpty()) {
    return mPlottables.last();
  } else
    return 0;
}

bool QCustomPlot::removePlottable(QCPAbstractPlottable *plottable) {
  if (!mPlottables.contains(plottable)) {
    qDebug() << Q_FUNC_INFO << "plottable not in list:"
             << reinterpret_cast<quintptr>(plottable);
    return false;
  }

  plottable->removeFromLegend();

  if (QCPGraph *graph = qobject_cast<QCPGraph *>(plottable))
    mGraphs.removeOne(graph);

  delete plottable;
  mPlottables.removeOne(plottable);
  return true;
}

bool QCustomPlot::removePlottable(int index) {
  if (index >= 0 && index < mPlottables.size())
    return removePlottable(mPlottables[index]);
  else {
    qDebug() << Q_FUNC_INFO << "index out of bounds:" << index;
    return false;
  }
}

int QCustomPlot::clearPlottables() {
  int c = mPlottables.size();
  for (int i = c - 1; i >= 0; --i)
    removePlottable(mPlottables[i]);
  return c;
}

int QCustomPlot::plottableCount() const { return mPlottables.size(); }

QList<QCPAbstractPlottable *> QCustomPlot::selectedPlottables() const {
  QList<QCPAbstractPlottable *> result;
  Q_FOREACH (QCPAbstractPlottable *plottable, mPlottables) {
    if (plottable->selected())
      result.append(plottable);
  }
  return result;
}

QCPAbstractPlottable *QCustomPlot::plottableAt(const QPointF &pos,
                                               bool onlySelectable) const {
  QCPAbstractPlottable *resultPlottable = 0;
  double resultDistance = mSelectionTolerance;

  Q_FOREACH (QCPAbstractPlottable *plottable, mPlottables) {
    if (onlySelectable && !plottable->selectable())

      continue;
    if ((plottable->keyAxis()->axisRect()->rect() &
         plottable->valueAxis()->axisRect()->rect())
            .contains(pos.toPoint()))

    {
      double currentDistance = plottable->selectTest(pos, false);
      if (currentDistance >= 0 && currentDistance < resultDistance) {
        resultPlottable = plottable;
        resultDistance = currentDistance;
      }
    }
  }

  return resultPlottable;
}

bool QCustomPlot::hasPlottable(QCPAbstractPlottable *plottable) const {
  return mPlottables.contains(plottable);
}

QCPGraph *QCustomPlot::graph(int index) const {
  if (index >= 0 && index < mGraphs.size()) {
    return mGraphs.at(index);
  } else {
    qDebug() << Q_FUNC_INFO << "index out of bounds:" << index;
    return 0;
  }
}

QCPGraph *QCustomPlot::graph() const {
  if (!mGraphs.isEmpty()) {
    return mGraphs.last();
  } else
    return 0;
}

QCPGraph *QCustomPlot::addGraph(QCPAxis *keyAxis, QCPAxis *valueAxis) {
  if (!keyAxis)
    keyAxis = xAxis;
  if (!valueAxis)
    valueAxis = yAxis;
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO
             << "can't use default QCustomPlot xAxis or yAxis, because at "
                "least one is invalid (has been deleted)";
    return 0;
  }
  if (keyAxis->parentPlot() != this || valueAxis->parentPlot() != this) {
    qDebug() << Q_FUNC_INFO
             << "passed keyAxis or valueAxis doesn't have this QCustomPlot as "
                "parent";
    return 0;
  }

  QCPGraph *newGraph = new QCPGraph(keyAxis, valueAxis);
  newGraph->setName(QLatin1String("Graph ") + QString::number(mGraphs.size()));
  return newGraph;
}

bool QCustomPlot::removeGraph(QCPGraph *graph) {
  return removePlottable(graph);
}

bool QCustomPlot::removeGraph(int index) {
  if (index >= 0 && index < mGraphs.size())
    return removeGraph(mGraphs[index]);
  else
    return false;
}

int QCustomPlot::clearGraphs() {
  int c = mGraphs.size();
  for (int i = c - 1; i >= 0; --i)
    removeGraph(mGraphs[i]);
  return c;
}

int QCustomPlot::graphCount() const { return mGraphs.size(); }

QList<QCPGraph *> QCustomPlot::selectedGraphs() const {
  QList<QCPGraph *> result;
  Q_FOREACH (QCPGraph *graph, mGraphs) {
    if (graph->selected())
      result.append(graph);
  }
  return result;
}

QCPAbstractItem *QCustomPlot::item(int index) const {
  if (index >= 0 && index < mItems.size()) {
    return mItems.at(index);
  } else {
    qDebug() << Q_FUNC_INFO << "index out of bounds:" << index;
    return 0;
  }
}

QCPAbstractItem *QCustomPlot::item() const {
  if (!mItems.isEmpty()) {
    return mItems.last();
  } else
    return 0;
}

bool QCustomPlot::removeItem(QCPAbstractItem *item) {
  if (mItems.contains(item)) {
    delete item;
    mItems.removeOne(item);
    return true;
  } else {
    qDebug() << Q_FUNC_INFO
             << "item not in list:" << reinterpret_cast<quintptr>(item);
    return false;
  }
}

bool QCustomPlot::removeItem(int index) {
  if (index >= 0 && index < mItems.size())
    return removeItem(mItems[index]);
  else {
    qDebug() << Q_FUNC_INFO << "index out of bounds:" << index;
    return false;
  }
}

int QCustomPlot::clearItems() {
  int c = mItems.size();
  for (int i = c - 1; i >= 0; --i)
    removeItem(mItems[i]);
  return c;
}

int QCustomPlot::itemCount() const { return mItems.size(); }

QList<QCPAbstractItem *> QCustomPlot::selectedItems() const {
  QList<QCPAbstractItem *> result;
  Q_FOREACH (QCPAbstractItem *item, mItems) {
    if (item->selected())
      result.append(item);
  }
  return result;
}

QCPAbstractItem *QCustomPlot::itemAt(const QPointF &pos,
                                     bool onlySelectable) const {
  QCPAbstractItem *resultItem = 0;
  double resultDistance = mSelectionTolerance;

  Q_FOREACH (QCPAbstractItem *item, mItems) {
    if (onlySelectable && !item->selectable())

      continue;
    if (!item->clipToAxisRect() || item->clipRect().contains(pos.toPoint()))

    {
      double currentDistance = item->selectTest(pos, false);
      if (currentDistance >= 0 && currentDistance < resultDistance) {
        resultItem = item;
        resultDistance = currentDistance;
      }
    }
  }

  return resultItem;
}

bool QCustomPlot::hasItem(QCPAbstractItem *item) const {
  return mItems.contains(item);
}

QCPLayer *QCustomPlot::layer(const QString &name) const {
  Q_FOREACH (QCPLayer *layer, mLayers) {
    if (layer->name() == name)
      return layer;
  }
  return 0;
}

QCPLayer *QCustomPlot::layer(int index) const {
  if (index >= 0 && index < mLayers.size()) {
    return mLayers.at(index);
  } else {
    qDebug() << Q_FUNC_INFO << "index out of bounds:" << index;
    return 0;
  }
}

QCPLayer *QCustomPlot::currentLayer() const { return mCurrentLayer; }

bool QCustomPlot::setCurrentLayer(const QString &name) {
  if (QCPLayer *newCurrentLayer = layer(name)) {
    return setCurrentLayer(newCurrentLayer);
  } else {
    qDebug() << Q_FUNC_INFO << "layer with name doesn't exist:" << name;
    return false;
  }
}

bool QCustomPlot::setCurrentLayer(QCPLayer *layer) {
  if (!mLayers.contains(layer)) {
    qDebug() << Q_FUNC_INFO << "layer not a layer of this QCustomPlot:"
             << reinterpret_cast<quintptr>(layer);
    return false;
  }

  mCurrentLayer = layer;
  return true;
}

int QCustomPlot::layerCount() const { return mLayers.size(); }

bool QCustomPlot::addLayer(const QString &name, QCPLayer *otherLayer,
                           QCustomPlot::LayerInsertMode insertMode) {
  if (!otherLayer)
    otherLayer = mLayers.last();
  if (!mLayers.contains(otherLayer)) {
    qDebug() << Q_FUNC_INFO << "otherLayer not a layer of this QCustomPlot:"
             << reinterpret_cast<quintptr>(otherLayer);
    return false;
  }
  if (layer(name)) {
    qDebug() << Q_FUNC_INFO << "A layer exists already with the name" << name;
    return false;
  }

  QCPLayer *newLayer = new QCPLayer(this, name);
  mLayers.insert(otherLayer->index() + (insertMode == limAbove ? 1 : 0),
                 newLayer);
  updateLayerIndices();
  setupPaintBuffers();

  return true;
}

bool QCustomPlot::removeLayer(QCPLayer *layer) {
  if (!mLayers.contains(layer)) {
    qDebug() << Q_FUNC_INFO << "layer not a layer of this QCustomPlot:"
             << reinterpret_cast<quintptr>(layer);
    return false;
  }
  if (mLayers.size() < 2) {
    qDebug() << Q_FUNC_INFO << "can't remove last layer";
    return false;
  }

  int removedIndex = layer->index();
  bool isFirstLayer = removedIndex == 0;
  QCPLayer *targetLayer = isFirstLayer ? mLayers.at(removedIndex + 1)
                                       : mLayers.at(removedIndex - 1);
  QList<QCPLayerable *> children = layer->children();
  if (isFirstLayer)

  {
    for (int i = children.size() - 1; i >= 0; --i)
      children.at(i)->moveToLayer(targetLayer, true);
  } else

  {
    for (int i = 0; i < children.size(); ++i)
      children.at(i)->moveToLayer(targetLayer, false);
  }

  if (layer == mCurrentLayer)
    setCurrentLayer(targetLayer);

  if (!layer->mPaintBuffer.isNull())
    layer->mPaintBuffer.data()->setInvalidated();

  delete layer;
  mLayers.removeOne(layer);
  updateLayerIndices();
  return true;
}

bool QCustomPlot::moveLayer(QCPLayer *layer, QCPLayer *otherLayer,
                            QCustomPlot::LayerInsertMode insertMode) {
  if (!mLayers.contains(layer)) {
    qDebug() << Q_FUNC_INFO << "layer not a layer of this QCustomPlot:"
             << reinterpret_cast<quintptr>(layer);
    return false;
  }
  if (!mLayers.contains(otherLayer)) {
    qDebug() << Q_FUNC_INFO << "otherLayer not a layer of this QCustomPlot:"
             << reinterpret_cast<quintptr>(otherLayer);
    return false;
  }

  if (layer->index() > otherLayer->index())
    mLayers.move(layer->index(),
                 otherLayer->index() + (insertMode == limAbove ? 1 : 0));
  else if (layer->index() < otherLayer->index())
    mLayers.move(layer->index(),
                 otherLayer->index() + (insertMode == limAbove ? 0 : -1));

  if (!layer->mPaintBuffer.isNull())
    layer->mPaintBuffer.data()->setInvalidated();
  if (!otherLayer->mPaintBuffer.isNull())
    otherLayer->mPaintBuffer.data()->setInvalidated();

  updateLayerIndices();
  return true;
}

int QCustomPlot::axisRectCount() const { return axisRects().size(); }

QCPAxisRect *QCustomPlot::axisRect(int index) const {
  const QList<QCPAxisRect *> rectList = axisRects();
  if (index >= 0 && index < rectList.size()) {
    return rectList.at(index);
  } else {
    qDebug() << Q_FUNC_INFO << "invalid axis rect index" << index;
    return 0;
  }
}

QList<QCPAxisRect *> QCustomPlot::axisRects() const {
  QList<QCPAxisRect *> result;
  QStack<QCPLayoutElement *> elementStack;
  if (mPlotLayout)
    elementStack.push(mPlotLayout);

  while (!elementStack.isEmpty()) {
    Q_FOREACH (QCPLayoutElement *element, elementStack.pop()->elements(false)) {
      if (element) {
        elementStack.push(element);
        if (QCPAxisRect *ar = qobject_cast<QCPAxisRect *>(element))
          result.append(ar);
      }
    }
  }

  return result;
}

QCPLayoutElement *QCustomPlot::layoutElementAt(const QPointF &pos) const {
  QCPLayoutElement *currentElement = mPlotLayout;
  bool searchSubElements = true;
  while (searchSubElements && currentElement) {
    searchSubElements = false;
    Q_FOREACH (QCPLayoutElement *subElement, currentElement->elements(false)) {
      if (subElement && subElement->realVisibility() &&
          subElement->selectTest(pos, false) >= 0) {
        currentElement = subElement;
        searchSubElements = true;
        break;
      }
    }
  }
  return currentElement;
}

QCPAxisRect *QCustomPlot::axisRectAt(const QPointF &pos) const {
  QCPAxisRect *result = 0;
  QCPLayoutElement *currentElement = mPlotLayout;
  bool searchSubElements = true;
  while (searchSubElements && currentElement) {
    searchSubElements = false;
    Q_FOREACH (QCPLayoutElement *subElement, currentElement->elements(false)) {
      if (subElement && subElement->realVisibility() &&
          subElement->selectTest(pos, false) >= 0) {
        currentElement = subElement;
        searchSubElements = true;
        if (QCPAxisRect *ar = qobject_cast<QCPAxisRect *>(currentElement))
          result = ar;
        break;
      }
    }
  }
  return result;
}

QList<QCPAxis *> QCustomPlot::selectedAxes() const {
  QList<QCPAxis *> result, allAxes;
  Q_FOREACH (QCPAxisRect *rect, axisRects())
    allAxes << rect->axes();

  Q_FOREACH (QCPAxis *axis, allAxes) {
    if (axis->selectedParts() != QCPAxis::spNone)
      result.append(axis);
  }

  return result;
}

QList<QCPLegend *> QCustomPlot::selectedLegends() const {
  QList<QCPLegend *> result;

  QStack<QCPLayoutElement *> elementStack;
  if (mPlotLayout)
    elementStack.push(mPlotLayout);

  while (!elementStack.isEmpty()) {
    Q_FOREACH (QCPLayoutElement *subElement,
               elementStack.pop()->elements(false)) {
      if (subElement) {
        elementStack.push(subElement);
        if (QCPLegend *leg = qobject_cast<QCPLegend *>(subElement)) {
          if (leg->selectedParts() != QCPLegend::spNone)
            result.append(leg);
        }
      }
    }
  }

  return result;
}

void QCustomPlot::deselectAll() {
  Q_FOREACH (QCPLayer *layer, mLayers) {
    Q_FOREACH (QCPLayerable *layerable, layer->children())
      layerable->deselectEvent(0);
  }
}

void QCustomPlot::replot(QCustomPlot::RefreshPriority refreshPriority) {
  if (refreshPriority == QCustomPlot::rpQueuedReplot) {
    if (!mReplotQueued) {
      mReplotQueued = true;
      QTimer::singleShot(0, this, SLOT(replot()));
    }
    return;
  }

  if (mReplotting)

    return;
  mReplotting = true;
  mReplotQueued = false;
  Q_EMIT beforeReplot();

  updateLayout();

  setupPaintBuffers();
  Q_FOREACH (QCPLayer *layer, mLayers)
    layer->drawToPaintBuffer();
  for (int i = 0; i < mPaintBuffers.size(); ++i)
    mPaintBuffers.at(i)->setInvalidated(false);

  if ((refreshPriority == rpRefreshHint &&
       mPlottingHints.testFlag(QCP::phImmediateRefresh)) ||
      refreshPriority == rpImmediateRefresh)
    repaint();
  else
    update();

  Q_EMIT afterReplot();
  mReplotting = false;
}

void QCustomPlot::rescaleAxes(bool onlyVisiblePlottables) {
  QList<QCPAxis *> allAxes;
  Q_FOREACH (QCPAxisRect *rect, axisRects())
    allAxes << rect->axes();

  Q_FOREACH (QCPAxis *axis, allAxes)
    axis->rescale(onlyVisiblePlottables);
}

bool QCustomPlot::savePdf(const QString &fileName, int width, int height,
                          QCP::ExportPen exportPen, const QString &pdfCreator,
                          const QString &pdfTitle) {
  bool success = false;
#ifdef QT_NO_PRINTER
  Q_UNUSED(fileName)
  Q_UNUSED(exportPen)
  Q_UNUSED(width)
  Q_UNUSED(height)
  Q_UNUSED(pdfCreator)
  Q_UNUSED(pdfTitle)
  qDebug() << Q_FUNC_INFO
           << "Qt was built without printer support (QT_NO_PRINTER). PDF not "
              "created.";
#else
  int newWidth, newHeight;
  if (width == 0 || height == 0) {
    newWidth = this->width();
    newHeight = this->height();
  } else {
    newWidth = width;
    newHeight = height;
  }

  QPrinter printer(QPrinter::ScreenResolution);
  printer.setOutputFileName(fileName);
  printer.setOutputFormat(QPrinter::PdfFormat);
  printer.setColorMode(QPrinter::Color);
  printer.printEngine()->setProperty(QPrintEngine::PPK_Creator, pdfCreator);
  printer.printEngine()->setProperty(QPrintEngine::PPK_DocumentName, pdfTitle);
  QRect oldViewport = viewport();
  setViewport(QRect(0, 0, newWidth, newHeight));
#if QT_VERSION < QT_VERSION_CHECK(5, 3, 0)
  printer.setFullPage(true);
  printer.setPaperSize(viewport().size(), QPrinter::DevicePixel);
#else
  QPageLayout pageLayout;
  pageLayout.setMode(QPageLayout::FullPageMode);
  pageLayout.setOrientation(QPageLayout::Portrait);
  pageLayout.setMargins(QMarginsF(0, 0, 0, 0));
  pageLayout.setPageSize(QPageSize(viewport().size(), QPageSize::Point,
                                   QString(), QPageSize::ExactMatch));
  printer.setPageLayout(pageLayout);
#endif
  QCPPainter printpainter;
  if (printpainter.begin(&printer)) {
    printpainter.setMode(QCPPainter::pmVectorized);
    printpainter.setMode(QCPPainter::pmNoCaching);
    printpainter.setMode(QCPPainter::pmNonCosmetic,
                         exportPen == QCP::epNoCosmetic);
    printpainter.setWindow(mViewport);
    if (mBackgroundBrush.style() != Qt::NoBrush &&
        mBackgroundBrush.color() != Qt::white &&
        mBackgroundBrush.color() != Qt::transparent &&
        mBackgroundBrush.color().alpha() > 0)

      printpainter.fillRect(viewport(), mBackgroundBrush);
    draw(&printpainter);
    printpainter.end();
    success = true;
  }
  setViewport(oldViewport);
#endif

  return success;
}

bool QCustomPlot::savePng(const QString &fileName, int width, int height,
                          double scale, int quality, int resolution,
                          QCP::ResolutionUnit resolutionUnit) {
  return saveRastered(fileName, width, height, scale, "PNG", quality,
                      resolution, resolutionUnit);
}

bool QCustomPlot::saveJpg(const QString &fileName, int width, int height,
                          double scale, int quality, int resolution,
                          QCP::ResolutionUnit resolutionUnit) {
  return saveRastered(fileName, width, height, scale, "JPG", quality,
                      resolution, resolutionUnit);
}

bool QCustomPlot::saveBmp(const QString &fileName, int width, int height,
                          double scale, int resolution,
                          QCP::ResolutionUnit resolutionUnit) {
  return saveRastered(fileName, width, height, scale, "BMP", -1, resolution,
                      resolutionUnit);
}

QSize QCustomPlot::minimumSizeHint() const {
  return mPlotLayout->minimumOuterSizeHint();
}

QSize QCustomPlot::sizeHint() const {
  return mPlotLayout->minimumOuterSizeHint();
}

void QCustomPlot::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QCPPainter painter(this);
  if (painter.isActive()) {
    painter.setRenderHint(QPainter::HighQualityAntialiasing);

    if (mBackgroundBrush.style() != Qt::NoBrush)
      painter.fillRect(mViewport, mBackgroundBrush);
    drawBackground(&painter);
    for (int bufferIndex = 0; bufferIndex < mPaintBuffers.size(); ++bufferIndex)
      mPaintBuffers.at(bufferIndex)->draw(&painter);
  }
}

void QCustomPlot::resizeEvent(QResizeEvent *event) {
  Q_UNUSED(event)

  setViewport(rect());
  replot(rpQueuedRefresh);
}

void QCustomPlot::mouseDoubleClickEvent(QMouseEvent *event) {
  Q_EMIT mouseDoubleClick(event);
  mMouseHasMoved = false;
  mMousePressPos = event->pos();

  QList<QVariant> details;
  QList<QCPLayerable *> candidates =
      layerableListAt(mMousePressPos, false, &details);
  for (int i = 0; i < candidates.size(); ++i) {
    event->accept();

    candidates.at(i)->mouseDoubleClickEvent(event, details.at(i));
    if (event->isAccepted()) {
      mMouseEventLayerable = candidates.at(i);
      mMouseEventLayerableDetails = details.at(i);
      break;
    }
  }

  if (!candidates.isEmpty()) {
    if (QCPAbstractPlottable *ap =
            qobject_cast<QCPAbstractPlottable *>(candidates.first())) {
      int dataIndex = 0;
      if (!details.first().value<QCPDataSelection>().isEmpty())
        dataIndex =
            details.first().value<QCPDataSelection>().dataRange().begin();
      Q_EMIT plottableDoubleClick(ap, dataIndex, event);
    } else if (QCPAxis *ax = qobject_cast<QCPAxis *>(candidates.first()))
      Q_EMIT axisDoubleClick(
          ax, details.first().value<QCPAxis::SelectablePart>(), event);
    else if (QCPAbstractItem *ai =
                 qobject_cast<QCPAbstractItem *>(candidates.first()))
      Q_EMIT itemDoubleClick(ai, event);
    else if (QCPLegend *lg = qobject_cast<QCPLegend *>(candidates.first()))
      Q_EMIT legendDoubleClick(lg, 0, event);
    else if (QCPAbstractLegendItem *li =
                 qobject_cast<QCPAbstractLegendItem *>(candidates.first()))
      Q_EMIT legendDoubleClick(li->parentLegend(), li, event);
  }

  event->accept();
}

void QCustomPlot::mousePressEvent(QMouseEvent *event) {
  Q_EMIT mousePress(event);

  mMouseHasMoved = false;
  mMousePressPos = event->pos();

  if (mSelectionRect && mSelectionRectMode != QCP::srmNone) {
    if (mSelectionRectMode != QCP::srmZoom ||
        qobject_cast<QCPAxisRect *>(axisRectAt(mMousePressPos)))

      mSelectionRect->startSelection(event);
  } else {
    QList<QVariant> details;
    QList<QCPLayerable *> candidates =
        layerableListAt(mMousePressPos, false, &details);
    if (!candidates.isEmpty()) {
      mMouseSignalLayerable = candidates.first();

      mMouseSignalLayerableDetails = details.first();
    }

    for (int i = 0; i < candidates.size(); ++i) {
      event->accept();

      candidates.at(i)->mousePressEvent(event, details.at(i));
      if (event->isAccepted()) {
        mMouseEventLayerable = candidates.at(i);
        mMouseEventLayerableDetails = details.at(i);
        break;
      }
    }
  }

  event->accept();
}

void QCustomPlot::mouseMoveEvent(QMouseEvent *event) {
  Q_EMIT mouseMove(event);

  if (!mMouseHasMoved && (mMousePressPos - event->pos()).manhattanLength() > 3)
    mMouseHasMoved = true;

  if (mSelectionRect && mSelectionRect->isActive())
    mSelectionRect->moveSelection(event);
  else if (mMouseEventLayerable)

    mMouseEventLayerable->mouseMoveEvent(event, mMousePressPos);

  event->accept();
}

void QCustomPlot::mouseReleaseEvent(QMouseEvent *event) {
  Q_EMIT mouseRelease(event);

  if (!mMouseHasMoved)

  {
    if (mSelectionRect && mSelectionRect->isActive())

      mSelectionRect->cancel();
    if (event->button() == Qt::LeftButton)
      processPointSelection(event);

    if (QCPAbstractPlottable *ap =
            qobject_cast<QCPAbstractPlottable *>(mMouseSignalLayerable)) {
      int dataIndex = 0;
      if (!mMouseSignalLayerableDetails.value<QCPDataSelection>().isEmpty())
        dataIndex = mMouseSignalLayerableDetails.value<QCPDataSelection>()
                        .dataRange()
                        .begin();
      Q_EMIT plottableClick(ap, dataIndex, event);
    } else if (QCPAxis *ax = qobject_cast<QCPAxis *>(mMouseSignalLayerable))
      Q_EMIT axisClick(
          ax, mMouseSignalLayerableDetails.value<QCPAxis::SelectablePart>(),
          event);
    else if (QCPAbstractItem *ai =
                 qobject_cast<QCPAbstractItem *>(mMouseSignalLayerable))
      Q_EMIT itemClick(ai, event);
    else if (QCPLegend *lg = qobject_cast<QCPLegend *>(mMouseSignalLayerable))
      Q_EMIT legendClick(lg, 0, event);
    else if (QCPAbstractLegendItem *li =
                 qobject_cast<QCPAbstractLegendItem *>(mMouseSignalLayerable))
      Q_EMIT legendClick(li->parentLegend(), li, event);
    mMouseSignalLayerable = 0;
  }

  if (mSelectionRect && mSelectionRect->isActive())

  {
    mSelectionRect->endSelection(event);
  } else {
    if (mMouseEventLayerable) {
      mMouseEventLayerable->mouseReleaseEvent(event, mMousePressPos);
      mMouseEventLayerable = 0;
    }
  }

  if (noAntialiasingOnDrag())
    replot(rpQueuedReplot);

  event->accept();
}

void QCustomPlot::wheelEvent(QWheelEvent *event) {
  Q_EMIT mouseWheel(event);

  QList<QCPLayerable *> candidates = layerableListAt(event->pos(), false);
  for (int i = 0; i < candidates.size(); ++i) {
    event->accept();

    candidates.at(i)->wheelEvent(event);
    if (event->isAccepted())
      break;
  }
  event->accept();
}

void QCustomPlot::draw(QCPPainter *painter) {
  updateLayout();

  drawBackground(painter);

  Q_FOREACH (QCPLayer *layer, mLayers)
    layer->draw(painter);
}

void QCustomPlot::updateLayout() {
  mPlotLayout->update(QCPLayoutElement::upPreparation);
  mPlotLayout->update(QCPLayoutElement::upMargins);
  mPlotLayout->update(QCPLayoutElement::upLayout);
}

void QCustomPlot::drawBackground(QCPPainter *painter) {
  if (!mBackgroundPixmap.isNull()) {
    if (mBackgroundScaled) {
      QSize scaledSize(mBackgroundPixmap.size());
      scaledSize.scale(mViewport.size(), mBackgroundScaledMode);
      if (mScaledBackgroundPixmap.size() != scaledSize)
        mScaledBackgroundPixmap = mBackgroundPixmap.scaled(
            mViewport.size(), mBackgroundScaledMode, Qt::SmoothTransformation);
      painter->drawPixmap(mViewport.topLeft(), mScaledBackgroundPixmap,
                          QRect(0, 0, mViewport.width(), mViewport.height()) &
                              mScaledBackgroundPixmap.rect());
    } else {
      painter->drawPixmap(mViewport.topLeft(), mBackgroundPixmap,
                          QRect(0, 0, mViewport.width(), mViewport.height()));
    }
  }
}

void QCustomPlot::setupPaintBuffers() {
  int bufferIndex = 0;
  if (mPaintBuffers.isEmpty())
    mPaintBuffers.append(
        QSharedPointer<QCPAbstractPaintBuffer>(createPaintBuffer()));

  for (int layerIndex = 0; layerIndex < mLayers.size(); ++layerIndex) {
    QCPLayer *layer = mLayers.at(layerIndex);
    if (layer->mode() == QCPLayer::lmLogical) {
      layer->mPaintBuffer = mPaintBuffers.at(bufferIndex).toWeakRef();
    } else if (layer->mode() == QCPLayer::lmBuffered) {
      ++bufferIndex;
      if (bufferIndex >= mPaintBuffers.size())
        mPaintBuffers.append(
            QSharedPointer<QCPAbstractPaintBuffer>(createPaintBuffer()));
      layer->mPaintBuffer = mPaintBuffers.at(bufferIndex).toWeakRef();
      if (layerIndex < mLayers.size() - 1 &&
          mLayers.at(layerIndex + 1)->mode() == QCPLayer::lmLogical)

      {
        ++bufferIndex;
        if (bufferIndex >= mPaintBuffers.size())
          mPaintBuffers.append(
              QSharedPointer<QCPAbstractPaintBuffer>(createPaintBuffer()));
      }
    }
  }

  while (mPaintBuffers.size() - 1 > bufferIndex)
    mPaintBuffers.removeLast();

  for (int i = 0; i < mPaintBuffers.size(); ++i) {
    mPaintBuffers.at(i)->setSize(viewport().size());

    mPaintBuffers.at(i)->clear(Qt::transparent);
    mPaintBuffers.at(i)->setInvalidated();
  }
}

QCPAbstractPaintBuffer *QCustomPlot::createPaintBuffer() {
  if (mOpenGl) {
#if defined(QCP_OPENGL_FBO)
    return new QCPPaintBufferGlFbo(viewport().size(), mBufferDevicePixelRatio,
                                   mGlContext, mGlPaintDevice);
#elif defined(QCP_OPENGL_PBUFFER)
    return new QCPPaintBufferGlPbuffer(
        viewport().size(), mBufferDevicePixelRatio, mOpenGlMultisamples);
#else
    qDebug()
        << Q_FUNC_INFO
        << "OpenGL enabled even though no support for it compiled in, this "
           "shouldn't have happened. Falling back to pixmap paint buffer.";
    return new QCPPaintBufferPixmap(viewport().size(), mBufferDevicePixelRatio);
#endif
  } else
    return new QCPPaintBufferPixmap(viewport().size(), mBufferDevicePixelRatio);
}

bool QCustomPlot::hasInvalidatedPaintBuffers() {
  for (int i = 0; i < mPaintBuffers.size(); ++i) {
    if (mPaintBuffers.at(i)->invalidated())
      return true;
  }
  return false;
}

bool QCustomPlot::setupOpenGl() {
#ifdef QCP_OPENGL_FBO
  freeOpenGl();
  QSurfaceFormat proposedSurfaceFormat;
  proposedSurfaceFormat.setSamples(mOpenGlMultisamples);
#ifdef QCP_OPENGL_OFFSCREENSURFACE
  QOffscreenSurface *surface = new QOffscreenSurface;
#else
  QWindow *surface = new QWindow;
  surface->setSurfaceType(QSurface::OpenGLSurface);
#endif
  surface->setFormat(proposedSurfaceFormat);
  surface->create();
  mGlSurface = QSharedPointer<QSurface>(surface);
  mGlContext = QSharedPointer<QOpenGLContext>(new QOpenGLContext);
  mGlContext->setFormat(mGlSurface->format());
  if (!mGlContext->create()) {
    qDebug() << Q_FUNC_INFO << "Failed to create OpenGL context";
    mGlContext.clear();
    mGlSurface.clear();
    return false;
  }
  if (!mGlContext->makeCurrent(mGlSurface.data()))

  {
    qDebug() << Q_FUNC_INFO << "Failed to make opengl context current";
    mGlContext.clear();
    mGlSurface.clear();
    return false;
  }
  if (!QOpenGLFramebufferObject::hasOpenGLFramebufferObjects()) {
    qDebug() << Q_FUNC_INFO
             << "OpenGL of this system doesn't support frame buffer objects";
    mGlContext.clear();
    mGlSurface.clear();
    return false;
  }
  mGlPaintDevice = QSharedPointer<QOpenGLPaintDevice>(new QOpenGLPaintDevice);
  return true;
#elif defined(QCP_OPENGL_PBUFFER)
  return QGLFormat::hasOpenGL();
#else
  return false;
#endif
}

void QCustomPlot::freeOpenGl() {
#ifdef QCP_OPENGL_FBO
  mGlPaintDevice.clear();
  mGlContext.clear();
  mGlSurface.clear();
#endif
}

void QCustomPlot::axisRemoved(QCPAxis *axis) {
  if (xAxis == axis)
    xAxis = 0;
  if (xAxis2 == axis)
    xAxis2 = 0;
  if (yAxis == axis)
    yAxis = 0;
  if (yAxis2 == axis)
    yAxis2 = 0;
}

void QCustomPlot::legendRemoved(QCPLegend *legend) {
  if (this->legend == legend)
    this->legend = 0;
}

void QCustomPlot::processRectSelection(QRect rect, QMouseEvent *event) {
  bool selectionStateChanged = false;

  if (mInteractions.testFlag(QCP::iSelectPlottables)) {
    QMap<int, QPair<QCPAbstractPlottable *, QCPDataSelection>>
        potentialSelections;

    QRectF rectF(rect.normalized());
    if (QCPAxisRect *affectedAxisRect = axisRectAt(rectF.topLeft())) {
      Q_FOREACH (QCPAbstractPlottable *plottable,
                 affectedAxisRect->plottables()) {
        if (QCPPlottableInterface1D *plottableInterface =
                plottable->interface1D()) {
          QCPDataSelection dataSel =
              plottableInterface->selectTestRect(rectF, true);
          if (!dataSel.isEmpty())
            potentialSelections.insertMulti(
                dataSel.dataPointCount(),
                QPair<QCPAbstractPlottable *, QCPDataSelection>(plottable,
                                                                dataSel));
        }
      }

      if (!mInteractions.testFlag(QCP::iMultiSelect)) {
        if (!potentialSelections.isEmpty()) {
          QMap<int, QPair<QCPAbstractPlottable *, QCPDataSelection>>::iterator
              it = potentialSelections.begin();
          while (it != potentialSelections.end() - 1)

            it = potentialSelections.erase(it);
        }
      }

      bool additive = event->modifiers().testFlag(mMultiSelectModifier);

      if (!additive) {
        Q_FOREACH (QCPLayer *layer, mLayers) {
          Q_FOREACH (QCPLayerable *layerable, layer->children()) {
            if ((potentialSelections.isEmpty() ||
                 potentialSelections.constBegin()->first != layerable) &&
                mInteractions.testFlag(layerable->selectionCategory())) {
              bool selChanged = false;
              layerable->deselectEvent(&selChanged);
              selectionStateChanged |= selChanged;
            }
          }
        }
      }

      QMap<int, QPair<QCPAbstractPlottable *, QCPDataSelection>>::const_iterator
          it = potentialSelections.constEnd();
      while (it != potentialSelections.constBegin()) {
        --it;
        if (mInteractions.testFlag(it.value().first->selectionCategory())) {
          bool selChanged = false;
          it.value().first->selectEvent(event, additive,
                                        QVariant::fromValue(it.value().second),
                                        &selChanged);
          selectionStateChanged |= selChanged;
        }
      }
    }
  }

  if (selectionStateChanged) {
    Q_EMIT selectionChangedByUser();
    replot(rpQueuedReplot);
  } else if (mSelectionRect)
    mSelectionRect->layer()->replot();
}

void QCustomPlot::processRectZoom(QRect rect, QMouseEvent *event) {
  Q_UNUSED(event)
  if (QCPAxisRect *axisRect = axisRectAt(rect.topLeft())) {
    QList<QCPAxis *> affectedAxes = QList<QCPAxis *>()
                                    << axisRect->rangeZoomAxes(Qt::Horizontal)
                                    << axisRect->rangeZoomAxes(Qt::Vertical);
    affectedAxes.removeAll(static_cast<QCPAxis *>(0));
    axisRect->zoom(QRectF(rect), affectedAxes);
  }
  replot(rpQueuedReplot);
}

void QCustomPlot::processPointSelection(QMouseEvent *event) {
  QVariant details;
  QCPLayerable *clickedLayerable = layerableAt(event->pos(), true, &details);
  bool selectionStateChanged = false;
  bool additive = mInteractions.testFlag(QCP::iMultiSelect) &&
                  event->modifiers().testFlag(mMultiSelectModifier);

  if (!additive) {
    Q_FOREACH (QCPLayer *layer, mLayers) {
      Q_FOREACH (QCPLayerable *layerable, layer->children()) {
        if (layerable != clickedLayerable &&
            mInteractions.testFlag(layerable->selectionCategory())) {
          bool selChanged = false;
          layerable->deselectEvent(&selChanged);
          selectionStateChanged |= selChanged;
        }
      }
    }
  }
  if (clickedLayerable &&
      mInteractions.testFlag(clickedLayerable->selectionCategory())) {
    bool selChanged = false;
    clickedLayerable->selectEvent(event, additive, details, &selChanged);
    selectionStateChanged |= selChanged;
  }
  if (selectionStateChanged) {
    Q_EMIT selectionChangedByUser();
    replot(rpQueuedReplot);
  }
}

bool QCustomPlot::registerPlottable(QCPAbstractPlottable *plottable) {
  if (mPlottables.contains(plottable)) {
    qDebug() << Q_FUNC_INFO << "plottable already added to this QCustomPlot:"
             << reinterpret_cast<quintptr>(plottable);
    return false;
  }
  if (plottable->parentPlot() != this) {
    qDebug() << Q_FUNC_INFO
             << "plottable not created with this QCustomPlot as parent:"
             << reinterpret_cast<quintptr>(plottable);
    return false;
  }

  mPlottables.append(plottable);

  if (mAutoAddPlottableToLegend)
    plottable->addToLegend();
  if (!plottable->layer())

    plottable->setLayer(currentLayer());
  return true;
}

bool QCustomPlot::registerGraph(QCPGraph *graph) {
  if (!graph) {
    qDebug() << Q_FUNC_INFO << "passed graph is zero";
    return false;
  }
  if (mGraphs.contains(graph)) {
    qDebug() << Q_FUNC_INFO << "graph already registered with this QCustomPlot";
    return false;
  }

  mGraphs.append(graph);
  return true;
}

bool QCustomPlot::registerItem(QCPAbstractItem *item) {
  if (mItems.contains(item)) {
    qDebug() << Q_FUNC_INFO << "item already added to this QCustomPlot:"
             << reinterpret_cast<quintptr>(item);
    return false;
  }
  if (item->parentPlot() != this) {
    qDebug() << Q_FUNC_INFO
             << "item not created with this QCustomPlot as parent:"
             << reinterpret_cast<quintptr>(item);
    return false;
  }

  mItems.append(item);
  if (!item->layer())

    item->setLayer(currentLayer());
  return true;
}

void QCustomPlot::updateLayerIndices() const {
  for (int i = 0; i < mLayers.size(); ++i)
    mLayers.at(i)->mIndex = i;
}

QCPLayerable *QCustomPlot::layerableAt(const QPointF &pos, bool onlySelectable,
                                       QVariant *selectionDetails) const {
  QList<QVariant> details;
  QList<QCPLayerable *> candidates =
      layerableListAt(pos, onlySelectable, selectionDetails ? &details : 0);
  if (selectionDetails && !details.isEmpty())
    *selectionDetails = details.first();
  if (!candidates.isEmpty())
    return candidates.first();
  else
    return 0;
}

QList<QCPLayerable *>
QCustomPlot::layerableListAt(const QPointF &pos, bool onlySelectable,
                             QList<QVariant> *selectionDetails) const {
  QList<QCPLayerable *> result;
  for (int layerIndex = mLayers.size() - 1; layerIndex >= 0; --layerIndex) {
    const QList<QCPLayerable *> layerables = mLayers.at(layerIndex)->children();
    for (int i = layerables.size() - 1; i >= 0; --i) {
      if (!layerables.at(i)->realVisibility())
        continue;
      QVariant details;
      double dist = layerables.at(i)->selectTest(
          pos, onlySelectable, selectionDetails ? &details : 0);
      if (dist >= 0 && dist < selectionTolerance()) {
        result.append(layerables.at(i));
        if (selectionDetails)
          selectionDetails->append(details);
      }
    }
  }
  return result;
}

bool QCustomPlot::saveRastered(const QString &fileName, int width, int height,
                               double scale, const char *format, int quality,
                               int resolution,
                               QCP::ResolutionUnit resolutionUnit) {
  QImage buffer = toPixmap(width, height, scale).toImage();

  int dotsPerMeter = 0;
  switch (resolutionUnit) {
  case QCP::ruDotsPerMeter:
    dotsPerMeter = resolution;
    break;
  case QCP::ruDotsPerCentimeter:
    dotsPerMeter = resolution * 100;
    break;
  case QCP::ruDotsPerInch:
    dotsPerMeter = resolution / 0.0254;
    break;
  }
  buffer.setDotsPerMeterX(dotsPerMeter);

  buffer.setDotsPerMeterY(dotsPerMeter);

  if (!buffer.isNull())
    return buffer.save(fileName, format, quality);
  else
    return false;
}

QPixmap QCustomPlot::toPixmap(int width, int height, double scale) {
  int newWidth, newHeight;
  if (width == 0 || height == 0) {
    newWidth = this->width();
    newHeight = this->height();
  } else {
    newWidth = width;
    newHeight = height;
  }
  int scaledWidth = qRound(scale * newWidth);
  int scaledHeight = qRound(scale * newHeight);

  QPixmap result(scaledWidth, scaledHeight);
  result.fill(mBackgroundBrush.style() == Qt::SolidPattern
                  ? mBackgroundBrush.color()
                  : Qt::transparent);

  QCPPainter painter;
  painter.begin(&result);
  if (painter.isActive()) {
    QRect oldViewport = viewport();
    setViewport(QRect(0, 0, newWidth, newHeight));
    painter.setMode(QCPPainter::pmNoCaching);
    if (!qFuzzyCompare(scale, 1.0)) {
      if (scale > 1.0)

        painter.setMode(QCPPainter::pmNonCosmetic);
      painter.scale(scale, scale);
    }
    if (mBackgroundBrush.style() != Qt::SolidPattern &&
        mBackgroundBrush.style() != Qt::NoBrush)

      painter.fillRect(mViewport, mBackgroundBrush);
    draw(&painter);
    setViewport(oldViewport);
    painter.end();
  } else

  {
    qDebug() << Q_FUNC_INFO << "Couldn't activate painter on pixmap";
    return QPixmap();
  }
  return result;
}

void QCustomPlot::toPainter(QCPPainter *painter, int width, int height) {
  int newWidth, newHeight;
  if (width == 0 || height == 0) {
    newWidth = this->width();
    newHeight = this->height();
  } else {
    newWidth = width;
    newHeight = height;
  }

  if (painter->isActive()) {
    QRect oldViewport = viewport();
    setViewport(QRect(0, 0, newWidth, newHeight));
    painter->setMode(QCPPainter::pmNoCaching);
    if (mBackgroundBrush.style() != Qt::NoBrush)

      painter->fillRect(mViewport, mBackgroundBrush);
    draw(painter);
    setViewport(oldViewport);
  } else
    qDebug() << Q_FUNC_INFO << "Passed painter is not active";
}

QCPColorGradient::QCPColorGradient()
    : mLevelCount(350), mColorInterpolation(ciRGB), mPeriodic(false),
      mColorBufferInvalidated(true) {
  mColorBuffer.fill(qRgb(0, 0, 0), mLevelCount);
}

QCPColorGradient::QCPColorGradient(GradientPreset preset)
    : mLevelCount(350), mColorInterpolation(ciRGB), mPeriodic(false),
      mColorBufferInvalidated(true) {
  mColorBuffer.fill(qRgb(0, 0, 0), mLevelCount);
  loadPreset(preset);
}

bool QCPColorGradient::operator==(const QCPColorGradient &other) const {
  return ((other.mLevelCount == this->mLevelCount) &&
          (other.mColorInterpolation == this->mColorInterpolation) &&
          (other.mPeriodic == this->mPeriodic) &&
          (other.mColorStops == this->mColorStops));
}

void QCPColorGradient::setLevelCount(int n) {
  if (n < 2) {
    qDebug() << Q_FUNC_INFO << "n must be greater or equal 2 but was" << n;
    n = 2;
  }
  if (n != mLevelCount) {
    mLevelCount = n;
    mColorBufferInvalidated = true;
  }
}

void QCPColorGradient::setColorStops(const QMap<double, QColor> &colorStops) {
  mColorStops = colorStops;
  mColorBufferInvalidated = true;
}

void QCPColorGradient::setColorStopAt(double position, const QColor &color) {
  mColorStops.insert(position, color);
  mColorBufferInvalidated = true;
}

void QCPColorGradient::setColorInterpolation(
    QCPColorGradient::ColorInterpolation interpolation) {
  if (interpolation != mColorInterpolation) {
    mColorInterpolation = interpolation;
    mColorBufferInvalidated = true;
  }
}

void QCPColorGradient::setPeriodic(bool enabled) { mPeriodic = enabled; }

void QCPColorGradient::colorize(const double *data, const QCPRange &range,
                                QRgb *scanLine, int n, int dataIndexFactor,
                                bool logarithmic) {
  if (!data) {
    qDebug() << Q_FUNC_INFO << "null pointer given as data";
    return;
  }
  if (!scanLine) {
    qDebug() << Q_FUNC_INFO << "null pointer given as scanLine";
    return;
  }
  if (mColorBufferInvalidated)
    updateColorBuffer();

  if (!logarithmic) {
    const double posToIndexFactor = (mLevelCount - 1) / range.size();
    if (mPeriodic) {
      for (int i = 0; i < n; ++i) {
        int index = (int)((data[dataIndexFactor * i] - range.lower) *
                          posToIndexFactor) %
                    mLevelCount;
        if (index < 0)
          index += mLevelCount;
        scanLine[i] = mColorBuffer.at(index);
      }
    } else {
      for (int i = 0; i < n; ++i) {
        int index =
            (data[dataIndexFactor * i] - range.lower) * posToIndexFactor;
        if (index < 0)
          index = 0;
        else if (index >= mLevelCount)
          index = mLevelCount - 1;
        scanLine[i] = mColorBuffer.at(index);
      }
    }
  } else

  {
    if (mPeriodic) {
      for (int i = 0; i < n; ++i) {
        int index = (int)(qLn(data[dataIndexFactor * i] / range.lower) /
                          qLn(range.upper / range.lower) * (mLevelCount - 1)) %
                    mLevelCount;
        if (index < 0)
          index += mLevelCount;
        scanLine[i] = mColorBuffer.at(index);
      }
    } else {
      for (int i = 0; i < n; ++i) {
        int index = qLn(data[dataIndexFactor * i] / range.lower) /
                    qLn(range.upper / range.lower) * (mLevelCount - 1);
        if (index < 0)
          index = 0;
        else if (index >= mLevelCount)
          index = mLevelCount - 1;
        scanLine[i] = mColorBuffer.at(index);
      }
    }
  }
}

void QCPColorGradient::colorize(const double *data, const unsigned char *alpha,
                                const QCPRange &range, QRgb *scanLine, int n,
                                int dataIndexFactor, bool logarithmic) {
  if (!data) {
    qDebug() << Q_FUNC_INFO << "null pointer given as data";
    return;
  }
  if (!alpha) {
    qDebug() << Q_FUNC_INFO << "null pointer given as alpha";
    return;
  }
  if (!scanLine) {
    qDebug() << Q_FUNC_INFO << "null pointer given as scanLine";
    return;
  }
  if (mColorBufferInvalidated)
    updateColorBuffer();

  if (!logarithmic) {
    const double posToIndexFactor = (mLevelCount - 1) / range.size();
    if (mPeriodic) {
      for (int i = 0; i < n; ++i) {
        int index = (int)((data[dataIndexFactor * i] - range.lower) *
                          posToIndexFactor) %
                    mLevelCount;
        if (index < 0)
          index += mLevelCount;
        if (alpha[dataIndexFactor * i] == 255) {
          scanLine[i] = mColorBuffer.at(index);
        } else {
          const QRgb rgb = mColorBuffer.at(index);
          const float alphaF = alpha[dataIndexFactor * i] / 255.0f;
          scanLine[i] = qRgba(qRed(rgb) * alphaF, qGreen(rgb) * alphaF,
                              qBlue(rgb) * alphaF, qAlpha(rgb) * alphaF);
        }
      }
    } else {
      for (int i = 0; i < n; ++i) {
        int index =
            (data[dataIndexFactor * i] - range.lower) * posToIndexFactor;
        if (index < 0)
          index = 0;
        else if (index >= mLevelCount)
          index = mLevelCount - 1;
        if (alpha[dataIndexFactor * i] == 255) {
          scanLine[i] = mColorBuffer.at(index);
        } else {
          const QRgb rgb = mColorBuffer.at(index);
          const float alphaF = alpha[dataIndexFactor * i] / 255.0f;
          scanLine[i] = qRgba(qRed(rgb) * alphaF, qGreen(rgb) * alphaF,
                              qBlue(rgb) * alphaF, qAlpha(rgb) * alphaF);
        }
      }
    }
  } else

  {
    if (mPeriodic) {
      for (int i = 0; i < n; ++i) {
        int index = (int)(qLn(data[dataIndexFactor * i] / range.lower) /
                          qLn(range.upper / range.lower) * (mLevelCount - 1)) %
                    mLevelCount;
        if (index < 0)
          index += mLevelCount;
        if (alpha[dataIndexFactor * i] == 255) {
          scanLine[i] = mColorBuffer.at(index);
        } else {
          const QRgb rgb = mColorBuffer.at(index);
          const float alphaF = alpha[dataIndexFactor * i] / 255.0f;
          scanLine[i] = qRgba(qRed(rgb) * alphaF, qGreen(rgb) * alphaF,
                              qBlue(rgb) * alphaF, qAlpha(rgb) * alphaF);
        }
      }
    } else {
      for (int i = 0; i < n; ++i) {
        int index = qLn(data[dataIndexFactor * i] / range.lower) /
                    qLn(range.upper / range.lower) * (mLevelCount - 1);
        if (index < 0)
          index = 0;
        else if (index >= mLevelCount)
          index = mLevelCount - 1;
        if (alpha[dataIndexFactor * i] == 255) {
          scanLine[i] = mColorBuffer.at(index);
        } else {
          const QRgb rgb = mColorBuffer.at(index);
          const float alphaF = alpha[dataIndexFactor * i] / 255.0f;
          scanLine[i] = qRgba(qRed(rgb) * alphaF, qGreen(rgb) * alphaF,
                              qBlue(rgb) * alphaF, qAlpha(rgb) * alphaF);
        }
      }
    }
  }
}

QRgb QCPColorGradient::color(double position, const QCPRange &range,
                             bool logarithmic) {
  if (mColorBufferInvalidated)
    updateColorBuffer();
  int index = 0;
  if (!logarithmic)
    index = (position - range.lower) * (mLevelCount - 1) / range.size();
  else
    index = qLn(position / range.lower) / qLn(range.upper / range.lower) *
            (mLevelCount - 1);
  if (mPeriodic) {
    index = index % mLevelCount;
    if (index < 0)
      index += mLevelCount;
  } else {
    if (index < 0)
      index = 0;
    else if (index >= mLevelCount)
      index = mLevelCount - 1;
  }
  return mColorBuffer.at(index);
}

void QCPColorGradient::loadPreset(GradientPreset preset) {
  clearColorStops();
  switch (preset) {
  case gpGrayscale:
    setColorInterpolation(ciRGB);
    setColorStopAt(0, Qt::black);
    setColorStopAt(1, Qt::white);
    break;
  case gpHot:
    setColorInterpolation(ciRGB);
    setColorStopAt(0, QColor(50, 0, 0));
    setColorStopAt(0.2, QColor(180, 10, 0));
    setColorStopAt(0.4, QColor(245, 50, 0));
    setColorStopAt(0.6, QColor(255, 150, 10));
    setColorStopAt(0.8, QColor(255, 255, 50));
    setColorStopAt(1, QColor(255, 255, 255));
    break;
  case gpCold:
    setColorInterpolation(ciRGB);
    setColorStopAt(0, QColor(0, 0, 50));
    setColorStopAt(0.2, QColor(0, 10, 180));
    setColorStopAt(0.4, QColor(0, 50, 245));
    setColorStopAt(0.6, QColor(10, 150, 255));
    setColorStopAt(0.8, QColor(50, 255, 255));
    setColorStopAt(1, QColor(255, 255, 255));
    break;
  case gpNight:
    setColorInterpolation(ciHSV);
    setColorStopAt(0, QColor(10, 20, 30));
    setColorStopAt(1, QColor(250, 255, 250));
    break;
  case gpCandy:
    setColorInterpolation(ciHSV);
    setColorStopAt(0, QColor(0, 0, 255));
    setColorStopAt(1, QColor(255, 250, 250));
    break;
  case gpGeography:
    setColorInterpolation(ciRGB);
    setColorStopAt(0, QColor(70, 170, 210));
    setColorStopAt(0.20, QColor(90, 160, 180));
    setColorStopAt(0.25, QColor(45, 130, 175));
    setColorStopAt(0.30, QColor(100, 140, 125));
    setColorStopAt(0.5, QColor(100, 140, 100));
    setColorStopAt(0.6, QColor(130, 145, 120));
    setColorStopAt(0.7, QColor(140, 130, 120));
    setColorStopAt(0.9, QColor(180, 190, 190));
    setColorStopAt(1, QColor(210, 210, 230));
    break;
  case gpIon:
    setColorInterpolation(ciHSV);
    setColorStopAt(0, QColor(50, 10, 10));
    setColorStopAt(0.45, QColor(0, 0, 255));
    setColorStopAt(0.8, QColor(0, 255, 255));
    setColorStopAt(1, QColor(0, 255, 0));
    break;
  case gpThermal:
    setColorInterpolation(ciRGB);
    setColorStopAt(0, QColor(0, 0, 50));
    setColorStopAt(0.15, QColor(20, 0, 120));
    setColorStopAt(0.33, QColor(200, 30, 140));
    setColorStopAt(0.6, QColor(255, 100, 0));
    setColorStopAt(0.85, QColor(255, 255, 40));
    setColorStopAt(1, QColor(255, 255, 255));
    break;
  case gpPolar:
    setColorInterpolation(ciRGB);
    setColorStopAt(0, QColor(50, 255, 255));
    setColorStopAt(0.18, QColor(10, 70, 255));
    setColorStopAt(0.28, QColor(10, 10, 190));
    setColorStopAt(0.5, QColor(0, 0, 0));
    setColorStopAt(0.72, QColor(190, 10, 10));
    setColorStopAt(0.82, QColor(255, 70, 10));
    setColorStopAt(1, QColor(255, 255, 50));
    break;
  case gpSpectrum:
    setColorInterpolation(ciHSV);
    setColorStopAt(0, QColor(50, 0, 50));
    setColorStopAt(0.15, QColor(0, 0, 255));
    setColorStopAt(0.35, QColor(0, 255, 255));
    setColorStopAt(0.6, QColor(255, 255, 0));
    setColorStopAt(0.75, QColor(255, 30, 0));
    setColorStopAt(1, QColor(50, 0, 0));
    break;
  case gpJet:
    setColorInterpolation(ciRGB);
    setColorStopAt(0, QColor(0, 0, 100));
    setColorStopAt(0.15, QColor(0, 50, 255));
    setColorStopAt(0.35, QColor(0, 255, 255));
    setColorStopAt(0.65, QColor(255, 255, 0));
    setColorStopAt(0.85, QColor(255, 30, 0));
    setColorStopAt(1, QColor(100, 0, 0));
    break;
  case gpHues:
    setColorInterpolation(ciHSV);
    setColorStopAt(0, QColor(255, 0, 0));
    setColorStopAt(1.0 / 3.0, QColor(0, 0, 255));
    setColorStopAt(2.0 / 3.0, QColor(0, 255, 0));
    setColorStopAt(1, QColor(255, 0, 0));
    break;
  }
}

void QCPColorGradient::clearColorStops() {
  mColorStops.clear();
  mColorBufferInvalidated = true;
}

QCPColorGradient QCPColorGradient::inverted() const {
  QCPColorGradient result(*this);
  result.clearColorStops();
  for (QMap<double, QColor>::const_iterator it = mColorStops.constBegin();
       it != mColorStops.constEnd(); ++it)
    result.setColorStopAt(1.0 - it.key(), it.value());
  return result;
}

bool QCPColorGradient::stopsUseAlpha() const {
  for (QMap<double, QColor>::const_iterator it = mColorStops.constBegin();
       it != mColorStops.constEnd(); ++it) {
    if (it.value().alpha() < 255)
      return true;
  }
  return false;
}

void QCPColorGradient::updateColorBuffer() {
  if (mColorBuffer.size() != mLevelCount)
    mColorBuffer.resize(mLevelCount);
  if (mColorStops.size() > 1) {
    double indexToPosFactor = 1.0 / (double)(mLevelCount - 1);
    const bool useAlpha = stopsUseAlpha();
    for (int i = 0; i < mLevelCount; ++i) {
      double position = i * indexToPosFactor;
      QMap<double, QColor>::const_iterator it =
          mColorStops.lowerBound(position);
      if (it == mColorStops.constEnd())

      {
        if (useAlpha) {
          const QColor col = (it - 1).value();
          const float alphaPremultiplier = col.alpha() / 255.0f;

          mColorBuffer[i] = qRgba(col.red() * alphaPremultiplier,
                                  col.green() * alphaPremultiplier,
                                  col.blue() * alphaPremultiplier, col.alpha());
        } else
          mColorBuffer[i] = (it - 1).value().rgba();
      } else if (it == mColorStops.constBegin())

      {
        if (useAlpha) {
          const QColor col = it.value();
          const float alphaPremultiplier = col.alpha() / 255.0f;

          mColorBuffer[i] = qRgba(col.red() * alphaPremultiplier,
                                  col.green() * alphaPremultiplier,
                                  col.blue() * alphaPremultiplier, col.alpha());
        } else
          mColorBuffer[i] = it.value().rgba();
      } else

      {
        QMap<double, QColor>::const_iterator high = it;
        QMap<double, QColor>::const_iterator low = it - 1;
        double t = (position - low.key()) / (high.key() - low.key());

        switch (mColorInterpolation) {
        case ciRGB: {
          if (useAlpha) {
            const int alpha =
                (1 - t) * low.value().alpha() + t * high.value().alpha();
            const float alphaPremultiplier = alpha / 255.0f;

            mColorBuffer[i] = qRgba(
                ((1 - t) * low.value().red() + t * high.value().red()) *
                    alphaPremultiplier,
                ((1 - t) * low.value().green() + t * high.value().green()) *
                    alphaPremultiplier,
                ((1 - t) * low.value().blue() + t * high.value().blue()) *
                    alphaPremultiplier,
                alpha);
          } else {
            mColorBuffer[i] =
                qRgb(((1 - t) * low.value().red() + t * high.value().red()),
                     ((1 - t) * low.value().green() + t * high.value().green()),
                     ((1 - t) * low.value().blue() + t * high.value().blue()));
          }
          break;
        }
        case ciHSV: {
          QColor lowHsv = low.value().toHsv();
          QColor highHsv = high.value().toHsv();
          double hue = 0;
          double hueDiff = highHsv.hueF() - lowHsv.hueF();
          if (hueDiff > 0.5)
            hue = lowHsv.hueF() - t * (1.0 - hueDiff);
          else if (hueDiff < -0.5)
            hue = lowHsv.hueF() + t * (1.0 + hueDiff);
          else
            hue = lowHsv.hueF() + t * hueDiff;
          if (hue < 0)
            hue += 1.0;
          else if (hue >= 1.0)
            hue -= 1.0;
          if (useAlpha) {
            const QRgb rgb =
                QColor::fromHsvF(
                    hue,
                    (1 - t) * lowHsv.saturationF() + t * highHsv.saturationF(),
                    (1 - t) * lowHsv.valueF() + t * highHsv.valueF())
                    .rgb();
            const float alpha =
                (1 - t) * lowHsv.alphaF() + t * highHsv.alphaF();
            mColorBuffer[i] = qRgba(qRed(rgb) * alpha, qGreen(rgb) * alpha,
                                    qBlue(rgb) * alpha, 255 * alpha);
          } else {
            mColorBuffer[i] =
                QColor::fromHsvF(
                    hue,
                    (1 - t) * lowHsv.saturationF() + t * highHsv.saturationF(),
                    (1 - t) * lowHsv.valueF() + t * highHsv.valueF())
                    .rgb();
          }
          break;
        }
        }
      }
    }
  } else if (mColorStops.size() == 1) {
    const QRgb rgb = mColorStops.constBegin().value().rgb();
    const float alpha = mColorStops.constBegin().value().alphaF();
    mColorBuffer.fill(qRgba(qRed(rgb) * alpha, qGreen(rgb) * alpha,
                            qBlue(rgb) * alpha, 255 * alpha));
  } else

  {
    mColorBuffer.fill(qRgb(0, 0, 0));
  }
  mColorBufferInvalidated = false;
}

QCPSelectionDecoratorBracket::QCPSelectionDecoratorBracket()
    : mBracketPen(QPen(Qt::black)), mBracketBrush(Qt::NoBrush),
      mBracketWidth(5), mBracketHeight(50), mBracketStyle(bsSquareBracket),
      mTangentToData(false), mTangentAverage(2) {}

QCPSelectionDecoratorBracket::~QCPSelectionDecoratorBracket() {}

void QCPSelectionDecoratorBracket::setBracketPen(const QPen &pen) {
  mBracketPen = pen;
}

void QCPSelectionDecoratorBracket::setBracketBrush(const QBrush &brush) {
  mBracketBrush = brush;
}

void QCPSelectionDecoratorBracket::setBracketWidth(int width) {
  mBracketWidth = width;
}

void QCPSelectionDecoratorBracket::setBracketHeight(int height) {
  mBracketHeight = height;
}

void QCPSelectionDecoratorBracket::setBracketStyle(
    QCPSelectionDecoratorBracket::BracketStyle style) {
  mBracketStyle = style;
}

void QCPSelectionDecoratorBracket::setTangentToData(bool enabled) {
  mTangentToData = enabled;
}

void QCPSelectionDecoratorBracket::setTangentAverage(int pointCount) {
  mTangentAverage = pointCount;
  if (mTangentAverage < 1)
    mTangentAverage = 1;
}

void QCPSelectionDecoratorBracket::drawBracket(QCPPainter *painter,
                                               int direction) const {
  switch (mBracketStyle) {
  case bsSquareBracket: {
    painter->drawLine(QLineF(mBracketWidth * direction, -mBracketHeight * 0.5,
                             0, -mBracketHeight * 0.5));
    painter->drawLine(QLineF(mBracketWidth * direction, mBracketHeight * 0.5, 0,
                             mBracketHeight * 0.5));
    painter->drawLine(
        QLineF(0, -mBracketHeight * 0.5, 0, mBracketHeight * 0.5));
    break;
  }
  case bsHalfEllipse: {
    painter->drawArc(-mBracketWidth * 0.5, -mBracketHeight * 0.5, mBracketWidth,
                     mBracketHeight, -90 * 16, -180 * 16 * direction);
    break;
  }
  case bsEllipse: {
    painter->drawEllipse(-mBracketWidth * 0.5, -mBracketHeight * 0.5,
                         mBracketWidth, mBracketHeight);
    break;
  }
  case bsPlus: {
    painter->drawLine(
        QLineF(0, -mBracketHeight * 0.5, 0, mBracketHeight * 0.5));
    painter->drawLine(QLineF(-mBracketWidth * 0.5, 0, mBracketWidth * 0.5, 0));
    break;
  }
  default: {
    qDebug() << Q_FUNC_INFO
             << "unknown/custom bracket style can't be handeld by default "
                "implementation:"
             << static_cast<int>(mBracketStyle);
    break;
  }
  }
}

void QCPSelectionDecoratorBracket::drawDecoration(QCPPainter *painter,
                                                  QCPDataSelection selection) {
  if (!mPlottable || selection.isEmpty())
    return;

  if (QCPPlottableInterface1D *interface1d = mPlottable->interface1D()) {
    Q_FOREACH (const QCPDataRange &dataRange, selection.dataRanges()) {
      int openBracketDir =
          (mPlottable->keyAxis() && !mPlottable->keyAxis()->rangeReversed())
              ? 1
              : -1;
      int closeBracketDir = -openBracketDir;
      QPointF openBracketPos =
          getPixelCoordinates(interface1d, dataRange.begin());
      QPointF closeBracketPos =
          getPixelCoordinates(interface1d, dataRange.end() - 1);
      double openBracketAngle = 0;
      double closeBracketAngle = 0;
      if (mTangentToData) {
        openBracketAngle =
            getTangentAngle(interface1d, dataRange.begin(), openBracketDir);
        closeBracketAngle =
            getTangentAngle(interface1d, dataRange.end() - 1, closeBracketDir);
      }

      QTransform oldTransform = painter->transform();
      painter->setPen(mBracketPen);
      painter->setBrush(mBracketBrush);
      painter->translate(openBracketPos);
      painter->rotate(openBracketAngle / M_PI * 180.0);
      drawBracket(painter, openBracketDir);
      painter->setTransform(oldTransform);

      painter->setPen(mBracketPen);
      painter->setBrush(mBracketBrush);
      painter->translate(closeBracketPos);
      painter->rotate(closeBracketAngle / M_PI * 180.0);
      drawBracket(painter, closeBracketDir);
      painter->setTransform(oldTransform);
    }
  }
}

double QCPSelectionDecoratorBracket::getTangentAngle(
    const QCPPlottableInterface1D *interface1d, int dataIndex,
    int direction) const {
  if (!interface1d || dataIndex < 0 || dataIndex >= interface1d->dataCount())
    return 0;
  direction = direction < 0 ? -1 : 1;

  int averageCount;
  if (direction < 0)
    averageCount = qMin(mTangentAverage, dataIndex);
  else
    averageCount =
        qMin(mTangentAverage, interface1d->dataCount() - 1 - dataIndex);
  qDebug() << averageCount;

  QVector<QPointF> points(averageCount);
  QPointF pointsAverage;
  int currentIndex = dataIndex;
  for (int i = 0; i < averageCount; ++i) {
    points[i] = getPixelCoordinates(interface1d, currentIndex);
    pointsAverage += points[i];
    currentIndex += direction;
  }
  pointsAverage /= (double)averageCount;

  double numSum = 0;
  double denomSum = 0;
  for (int i = 0; i < averageCount; ++i) {
    const double dx = points.at(i).x() - pointsAverage.x();
    const double dy = points.at(i).y() - pointsAverage.y();
    numSum += dx * dy;
    denomSum += dx * dx;
  }
  if (!qFuzzyIsNull(denomSum) && !qFuzzyIsNull(numSum)) {
    return qAtan2(numSum, denomSum);
  } else

    return 0;
}

QPointF QCPSelectionDecoratorBracket::getPixelCoordinates(
    const QCPPlottableInterface1D *interface1d, int dataIndex) const {
  QCPAxis *keyAxis = mPlottable->keyAxis();
  QCPAxis *valueAxis = mPlottable->valueAxis();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return QPointF(0, 0);
  }

  if (keyAxis->orientation() == Qt::Horizontal)
    return QPointF(
        keyAxis->coordToPixel(interface1d->dataMainKey(dataIndex)),
        valueAxis->coordToPixel(interface1d->dataMainValue(dataIndex)));
  else
    return QPointF(
        valueAxis->coordToPixel(interface1d->dataMainValue(dataIndex)),
        keyAxis->coordToPixel(interface1d->dataMainKey(dataIndex)));
}

QCPAxisRect::QCPAxisRect(QCustomPlot *parentPlot, bool setupDefaultAxes)
    : QCPLayoutElement(parentPlot), mBackgroundBrush(Qt::NoBrush),
      mBackgroundScaled(true),
      mBackgroundScaledMode(Qt::KeepAspectRatioByExpanding),
      mInsetLayout(new QCPLayoutInset),
      mRangeDrag(Qt::Horizontal | Qt::Vertical),
      mRangeZoom(Qt::Horizontal | Qt::Vertical), mRangeZoomFactorHorz(0.85),
      mRangeZoomFactorVert(0.85), mDragging(false) {
  mInsetLayout->initializeParentPlot(mParentPlot);
  mInsetLayout->setParentLayerable(this);
  mInsetLayout->setParent(this);

  setMinimumSize(50, 50);
  setMinimumMargins(QMargins(15, 15, 15, 15));
  mAxes.insert(QCPAxis::atLeft, QList<QCPAxis *>());
  mAxes.insert(QCPAxis::atRight, QList<QCPAxis *>());
  mAxes.insert(QCPAxis::atTop, QList<QCPAxis *>());
  mAxes.insert(QCPAxis::atBottom, QList<QCPAxis *>());

  if (setupDefaultAxes) {
    QCPAxis *xAxis = addAxis(QCPAxis::atBottom);
    QCPAxis *yAxis = addAxis(QCPAxis::atLeft);
    QCPAxis *xAxis2 = addAxis(QCPAxis::atTop);
    QCPAxis *yAxis2 = addAxis(QCPAxis::atRight);
    setRangeDragAxes(xAxis, yAxis);
    setRangeZoomAxes(xAxis, yAxis);
    xAxis2->setVisible(false);
    yAxis2->setVisible(false);
    xAxis->grid()->setVisible(true);
    yAxis->grid()->setVisible(true);
    xAxis2->grid()->setVisible(false);
    yAxis2->grid()->setVisible(false);
    xAxis2->grid()->setZeroLinePen(Qt::NoPen);
    yAxis2->grid()->setZeroLinePen(Qt::NoPen);
    xAxis2->grid()->setVisible(false);
    yAxis2->grid()->setVisible(false);
  }
}

QCPAxisRect::~QCPAxisRect() {
  delete mInsetLayout;
  mInsetLayout = 0;

  QList<QCPAxis *> axesList = axes();
  for (int i = 0; i < axesList.size(); ++i)
    removeAxis(axesList.at(i));
}

int QCPAxisRect::axisCount(QCPAxis::AxisType type) const {
  return mAxes.value(type).size();
}

QCPAxis *QCPAxisRect::axis(QCPAxis::AxisType type, int index) const {
  QList<QCPAxis *> ax(mAxes.value(type));
  if (index >= 0 && index < ax.size()) {
    return ax.at(index);
  } else {
    qDebug() << Q_FUNC_INFO << "Axis index out of bounds:" << index;
    return 0;
  }
}

QList<QCPAxis *> QCPAxisRect::axes(QCPAxis::AxisTypes types) const {
  QList<QCPAxis *> result;
  if (types.testFlag(QCPAxis::atLeft))
    result << mAxes.value(QCPAxis::atLeft);
  if (types.testFlag(QCPAxis::atRight))
    result << mAxes.value(QCPAxis::atRight);
  if (types.testFlag(QCPAxis::atTop))
    result << mAxes.value(QCPAxis::atTop);
  if (types.testFlag(QCPAxis::atBottom))
    result << mAxes.value(QCPAxis::atBottom);
  return result;
}

QList<QCPAxis *> QCPAxisRect::axes() const {
  QList<QCPAxis *> result;
  QHashIterator<QCPAxis::AxisType, QList<QCPAxis *>> it(mAxes);
  while (it.hasNext()) {
    it.next();
    result << it.value();
  }
  return result;
}

QCPAxis *QCPAxisRect::addAxis(QCPAxis::AxisType type, QCPAxis *axis) {
  QCPAxis *newAxis = axis;
  if (!newAxis) {
    newAxis = new QCPAxis(this, type);
  } else

  {
    if (newAxis->axisType() != type) {
      qDebug() << Q_FUNC_INFO
               << "passed axis has different axis type than specified in type "
                  "parameter";
      return 0;
    }
    if (newAxis->axisRect() != this) {
      qDebug() << Q_FUNC_INFO
               << "passed axis doesn't have this axis rect as parent axis rect";
      return 0;
    }
    if (axes().contains(newAxis)) {
      qDebug() << Q_FUNC_INFO
               << "passed axis is already owned by this axis rect";
      return 0;
    }
  }
  if (mAxes[type].size() > 0)

  {
    bool invert = (type == QCPAxis::atRight) || (type == QCPAxis::atBottom);
    newAxis->setLowerEnding(
        QCPLineEnding(QCPLineEnding::esHalfBar, 6, 10, !invert));
    newAxis->setUpperEnding(
        QCPLineEnding(QCPLineEnding::esHalfBar, 6, 10, invert));
  }
  mAxes[type].append(newAxis);

  if (mParentPlot && mParentPlot->axisRectCount() > 0 &&
      mParentPlot->axisRect(0) == this) {
    switch (type) {
    case QCPAxis::atBottom: {
      if (!mParentPlot->xAxis)
        mParentPlot->xAxis = newAxis;
      break;
    }
    case QCPAxis::atLeft: {
      if (!mParentPlot->yAxis)
        mParentPlot->yAxis = newAxis;
      break;
    }
    case QCPAxis::atTop: {
      if (!mParentPlot->xAxis2)
        mParentPlot->xAxis2 = newAxis;
      break;
    }
    case QCPAxis::atRight: {
      if (!mParentPlot->yAxis2)
        mParentPlot->yAxis2 = newAxis;
      break;
    }
    }
  }

  return newAxis;
}

QList<QCPAxis *> QCPAxisRect::addAxes(QCPAxis::AxisTypes types) {
  QList<QCPAxis *> result;
  if (types.testFlag(QCPAxis::atLeft))
    result << addAxis(QCPAxis::atLeft);
  if (types.testFlag(QCPAxis::atRight))
    result << addAxis(QCPAxis::atRight);
  if (types.testFlag(QCPAxis::atTop))
    result << addAxis(QCPAxis::atTop);
  if (types.testFlag(QCPAxis::atBottom))
    result << addAxis(QCPAxis::atBottom);
  return result;
}

bool QCPAxisRect::removeAxis(QCPAxis *axis) {
  QHashIterator<QCPAxis::AxisType, QList<QCPAxis *>> it(mAxes);
  while (it.hasNext()) {
    it.next();
    if (it.value().contains(axis)) {
      if (it.value().first() == axis && it.value().size() > 1)

        it.value()[1]->setOffset(axis->offset());
      mAxes[it.key()].removeOne(axis);
      if (qobject_cast<QCustomPlot *>(parentPlot()))

        parentPlot()->axisRemoved(axis);
      delete axis;
      return true;
    }
  }
  qDebug() << Q_FUNC_INFO
           << "Axis isn't in axis rect:" << reinterpret_cast<quintptr>(axis);
  return false;
}

void QCPAxisRect::zoom(const QRectF &pixelRect) { zoom(pixelRect, axes()); }

void QCPAxisRect::zoom(const QRectF &pixelRect,
                       const QList<QCPAxis *> &affectedAxes) {
  Q_FOREACH (QCPAxis *axis, affectedAxes) {
    if (!axis) {
      qDebug() << Q_FUNC_INFO << "a passed axis was zero";
      continue;
    }
    QCPRange pixelRange;
    if (axis->orientation() == Qt::Horizontal)
      pixelRange = QCPRange(pixelRect.left(), pixelRect.right());
    else
      pixelRange = QCPRange(pixelRect.top(), pixelRect.bottom());
    axis->setRange(axis->pixelToCoord(pixelRange.lower),
                   axis->pixelToCoord(pixelRange.upper));
  }
}

void QCPAxisRect::setupFullAxesBox(bool connectRanges) {
  QCPAxis *xAxis, *yAxis, *xAxis2, *yAxis2;
  if (axisCount(QCPAxis::atBottom) == 0)
    xAxis = addAxis(QCPAxis::atBottom);
  else
    xAxis = axis(QCPAxis::atBottom);

  if (axisCount(QCPAxis::atLeft) == 0)
    yAxis = addAxis(QCPAxis::atLeft);
  else
    yAxis = axis(QCPAxis::atLeft);

  if (axisCount(QCPAxis::atTop) == 0)
    xAxis2 = addAxis(QCPAxis::atTop);
  else
    xAxis2 = axis(QCPAxis::atTop);

  if (axisCount(QCPAxis::atRight) == 0)
    yAxis2 = addAxis(QCPAxis::atRight);
  else
    yAxis2 = axis(QCPAxis::atRight);

  xAxis->setVisible(true);
  yAxis->setVisible(true);
  xAxis2->setVisible(true);
  yAxis2->setVisible(true);
  xAxis2->setTickLabels(false);
  yAxis2->setTickLabels(false);

  xAxis2->setRange(xAxis->range());
  xAxis2->setRangeReversed(xAxis->rangeReversed());
  xAxis2->setScaleType(xAxis->scaleType());
  xAxis2->setTicks(xAxis->ticks());
  xAxis2->setNumberFormat(xAxis->numberFormat());
  xAxis2->setNumberPrecision(xAxis->numberPrecision());
  xAxis2->ticker()->setTickCount(xAxis->ticker()->tickCount());
  xAxis2->ticker()->setTickOrigin(xAxis->ticker()->tickOrigin());

  yAxis2->setRange(yAxis->range());
  yAxis2->setRangeReversed(yAxis->rangeReversed());
  yAxis2->setScaleType(yAxis->scaleType());
  yAxis2->setTicks(yAxis->ticks());
  yAxis2->setNumberFormat(yAxis->numberFormat());
  yAxis2->setNumberPrecision(yAxis->numberPrecision());
  yAxis2->ticker()->setTickCount(yAxis->ticker()->tickCount());
  yAxis2->ticker()->setTickOrigin(yAxis->ticker()->tickOrigin());

  if (connectRanges) {
    connect(xAxis, SIGNAL(rangeChanged(QCPRange)), xAxis2,
            SLOT(setRange(QCPRange)));
    connect(yAxis, SIGNAL(rangeChanged(QCPRange)), yAxis2,
            SLOT(setRange(QCPRange)));
  }
}

QList<QCPAbstractPlottable *> QCPAxisRect::plottables() const {
  QList<QCPAbstractPlottable *> result;
  for (int i = 0; i < mParentPlot->mPlottables.size(); ++i) {
    if (mParentPlot->mPlottables.at(i)->keyAxis()->axisRect() == this ||
        mParentPlot->mPlottables.at(i)->valueAxis()->axisRect() == this)
      result.append(mParentPlot->mPlottables.at(i));
  }
  return result;
}

QList<QCPGraph *> QCPAxisRect::graphs() const {
  QList<QCPGraph *> result;
  for (int i = 0; i < mParentPlot->mGraphs.size(); ++i) {
    if (mParentPlot->mGraphs.at(i)->keyAxis()->axisRect() == this ||
        mParentPlot->mGraphs.at(i)->valueAxis()->axisRect() == this)
      result.append(mParentPlot->mGraphs.at(i));
  }
  return result;
}

QList<QCPAbstractItem *> QCPAxisRect::items() const {
  QList<QCPAbstractItem *> result;
  for (int itemId = 0; itemId < mParentPlot->mItems.size(); ++itemId) {
    if (mParentPlot->mItems.at(itemId)->clipAxisRect() == this) {
      result.append(mParentPlot->mItems.at(itemId));
      continue;
    }
    QList<QCPItemPosition *> positions =
        mParentPlot->mItems.at(itemId)->positions();
    for (int posId = 0; posId < positions.size(); ++posId) {
      if (positions.at(posId)->axisRect() == this ||
          positions.at(posId)->keyAxis()->axisRect() == this ||
          positions.at(posId)->valueAxis()->axisRect() == this) {
        result.append(mParentPlot->mItems.at(itemId));
        break;
      }
    }
  }
  return result;
}

void QCPAxisRect::update(UpdatePhase phase) {
  QCPLayoutElement::update(phase);

  switch (phase) {
  case upPreparation: {
    QList<QCPAxis *> allAxes = axes();
    for (int i = 0; i < allAxes.size(); ++i)
      allAxes.at(i)->setupTickVectors();
    break;
  }
  case upLayout: {
    mInsetLayout->setOuterRect(rect());
    break;
  }
  default:
    break;
  }

  mInsetLayout->update(phase);
}

QList<QCPLayoutElement *> QCPAxisRect::elements(bool recursive) const {
  QList<QCPLayoutElement *> result;
  if (mInsetLayout) {
    result << mInsetLayout;
    if (recursive)
      result << mInsetLayout->elements(recursive);
  }
  return result;
}

void QCPAxisRect::applyDefaultAntialiasingHint(QCPPainter *painter) const {
  painter->setAntialiasing(false);
}

void QCPAxisRect::draw(QCPPainter *painter) { drawBackground(painter); }

void QCPAxisRect::setBackground(const QPixmap &pm) {
  mBackgroundPixmap = pm;
  mScaledBackgroundPixmap = QPixmap();
}

void QCPAxisRect::setBackground(const QBrush &brush) {
  mBackgroundBrush = brush;
}

void QCPAxisRect::setBackground(const QPixmap &pm, bool scaled,
                                Qt::AspectRatioMode mode) {
  mBackgroundPixmap = pm;
  mScaledBackgroundPixmap = QPixmap();
  mBackgroundScaled = scaled;
  mBackgroundScaledMode = mode;
}

void QCPAxisRect::setBackgroundScaled(bool scaled) {
  mBackgroundScaled = scaled;
}

void QCPAxisRect::setBackgroundScaledMode(Qt::AspectRatioMode mode) {
  mBackgroundScaledMode = mode;
}

QCPAxis *QCPAxisRect::rangeDragAxis(Qt::Orientation orientation) {
  if (orientation == Qt::Horizontal)
    return mRangeDragHorzAxis.isEmpty() ? 0 : mRangeDragHorzAxis.first().data();
  else
    return mRangeDragVertAxis.isEmpty() ? 0 : mRangeDragVertAxis.first().data();
}

QCPAxis *QCPAxisRect::rangeZoomAxis(Qt::Orientation orientation) {
  if (orientation == Qt::Horizontal)
    return mRangeZoomHorzAxis.isEmpty() ? 0 : mRangeZoomHorzAxis.first().data();
  else
    return mRangeZoomVertAxis.isEmpty() ? 0 : mRangeZoomVertAxis.first().data();
}

QList<QCPAxis *> QCPAxisRect::rangeDragAxes(Qt::Orientation orientation) {
  QList<QCPAxis *> result;
  if (orientation == Qt::Horizontal) {
    for (int i = 0; i < mRangeDragHorzAxis.size(); ++i) {
      if (!mRangeDragHorzAxis.at(i).isNull())
        result.append(mRangeDragHorzAxis.at(i).data());
    }
  } else {
    for (int i = 0; i < mRangeDragVertAxis.size(); ++i) {
      if (!mRangeDragVertAxis.at(i).isNull())
        result.append(mRangeDragVertAxis.at(i).data());
    }
  }
  return result;
}

QList<QCPAxis *> QCPAxisRect::rangeZoomAxes(Qt::Orientation orientation) {
  QList<QCPAxis *> result;
  if (orientation == Qt::Horizontal) {
    for (int i = 0; i < mRangeZoomHorzAxis.size(); ++i) {
      if (!mRangeZoomHorzAxis.at(i).isNull())
        result.append(mRangeZoomHorzAxis.at(i).data());
    }
  } else {
    for (int i = 0; i < mRangeZoomVertAxis.size(); ++i) {
      if (!mRangeZoomVertAxis.at(i).isNull())
        result.append(mRangeZoomVertAxis.at(i).data());
    }
  }
  return result;
}

double QCPAxisRect::rangeZoomFactor(Qt::Orientation orientation) {
  return (orientation == Qt::Horizontal ? mRangeZoomFactorHorz
                                        : mRangeZoomFactorVert);
}

void QCPAxisRect::setRangeDrag(Qt::Orientations orientations) {
  mRangeDrag = orientations;
}

void QCPAxisRect::setRangeZoom(Qt::Orientations orientations) {
  mRangeZoom = orientations;
}

void QCPAxisRect::setRangeDragAxes(QCPAxis *horizontal, QCPAxis *vertical) {
  QList<QCPAxis *> horz, vert;
  if (horizontal)
    horz.append(horizontal);
  if (vertical)
    vert.append(vertical);
  setRangeDragAxes(horz, vert);
}

void QCPAxisRect::setRangeDragAxes(QList<QCPAxis *> axes) {
  QList<QCPAxis *> horz, vert;
  Q_FOREACH (QCPAxis *ax, axes) {
    if (ax->orientation() == Qt::Horizontal)
      horz.append(ax);
    else
      vert.append(ax);
  }
  setRangeDragAxes(horz, vert);
}

void QCPAxisRect::setRangeDragAxes(QList<QCPAxis *> horizontal,
                                   QList<QCPAxis *> vertical) {
  mRangeDragHorzAxis.clear();
  Q_FOREACH (QCPAxis *ax, horizontal) {
    QPointer<QCPAxis> axPointer(ax);
    if (!axPointer.isNull())
      mRangeDragHorzAxis.append(axPointer);
    else
      qDebug() << Q_FUNC_INFO << "invalid axis passed in horizontal list:"
               << reinterpret_cast<quintptr>(ax);
  }
  mRangeDragVertAxis.clear();
  Q_FOREACH (QCPAxis *ax, vertical) {
    QPointer<QCPAxis> axPointer(ax);
    if (!axPointer.isNull())
      mRangeDragVertAxis.append(axPointer);
    else
      qDebug() << Q_FUNC_INFO << "invalid axis passed in vertical list:"
               << reinterpret_cast<quintptr>(ax);
  }
}

void QCPAxisRect::setRangeZoomAxes(QCPAxis *horizontal, QCPAxis *vertical) {
  QList<QCPAxis *> horz, vert;
  if (horizontal)
    horz.append(horizontal);
  if (vertical)
    vert.append(vertical);
  setRangeZoomAxes(horz, vert);
}

void QCPAxisRect::setRangeZoomAxes(QList<QCPAxis *> axes) {
  QList<QCPAxis *> horz, vert;
  Q_FOREACH (QCPAxis *ax, axes) {
    if (ax->orientation() == Qt::Horizontal)
      horz.append(ax);
    else
      vert.append(ax);
  }
  setRangeZoomAxes(horz, vert);
}

void QCPAxisRect::setRangeZoomAxes(QList<QCPAxis *> horizontal,
                                   QList<QCPAxis *> vertical) {
  mRangeZoomHorzAxis.clear();
  Q_FOREACH (QCPAxis *ax, horizontal) {
    QPointer<QCPAxis> axPointer(ax);
    if (!axPointer.isNull())
      mRangeZoomHorzAxis.append(axPointer);
    else
      qDebug() << Q_FUNC_INFO << "invalid axis passed in horizontal list:"
               << reinterpret_cast<quintptr>(ax);
  }
  mRangeZoomVertAxis.clear();
  Q_FOREACH (QCPAxis *ax, vertical) {
    QPointer<QCPAxis> axPointer(ax);
    if (!axPointer.isNull())
      mRangeZoomVertAxis.append(axPointer);
    else
      qDebug() << Q_FUNC_INFO << "invalid axis passed in vertical list:"
               << reinterpret_cast<quintptr>(ax);
  }
}

void QCPAxisRect::setRangeZoomFactor(double horizontalFactor,
                                     double verticalFactor) {
  mRangeZoomFactorHorz = horizontalFactor;
  mRangeZoomFactorVert = verticalFactor;
}

void QCPAxisRect::setRangeZoomFactor(double factor) {
  mRangeZoomFactorHorz = factor;
  mRangeZoomFactorVert = factor;
}

void QCPAxisRect::drawBackground(QCPPainter *painter) {
  if (mBackgroundBrush != Qt::NoBrush)
    painter->fillRect(mRect, mBackgroundBrush);

  if (!mBackgroundPixmap.isNull()) {
    if (mBackgroundScaled) {
      QSize scaledSize(mBackgroundPixmap.size());
      scaledSize.scale(mRect.size(), mBackgroundScaledMode);
      if (mScaledBackgroundPixmap.size() != scaledSize)
        mScaledBackgroundPixmap = mBackgroundPixmap.scaled(
            mRect.size(), mBackgroundScaledMode, Qt::SmoothTransformation);
      painter->drawPixmap(mRect.topLeft() + QPoint(0, -1),
                          mScaledBackgroundPixmap,
                          QRect(0, 0, mRect.width(), mRect.height()) &
                              mScaledBackgroundPixmap.rect());
    } else {
      painter->drawPixmap(mRect.topLeft() + QPoint(0, -1), mBackgroundPixmap,
                          QRect(0, 0, mRect.width(), mRect.height()));
    }
  }
}

void QCPAxisRect::updateAxesOffset(QCPAxis::AxisType type) {
  const QList<QCPAxis *> axesList = mAxes.value(type);
  if (axesList.isEmpty())
    return;

  bool isFirstVisible = !axesList.first()->visible();

  for (int i = 1; i < axesList.size(); ++i) {
    int offset =
        axesList.at(i - 1)->offset() + axesList.at(i - 1)->calculateMargin();
    if (axesList.at(i)->visible())

    {
      if (!isFirstVisible)
        offset += axesList.at(i)->tickLengthIn();
      isFirstVisible = false;
    }
    axesList.at(i)->setOffset(offset);
  }
}

int QCPAxisRect::calculateAutoMargin(QCP::MarginSide side) {
  if (!mAutoMargins.testFlag(side))
    qDebug() << Q_FUNC_INFO
             << "Called with side that isn't specified as auto margin";

  updateAxesOffset(QCPAxis::marginSideToAxisType(side));

  const QList<QCPAxis *> axesList =
      mAxes.value(QCPAxis::marginSideToAxisType(side));
  if (axesList.size() > 0)
    return axesList.last()->offset() + axesList.last()->calculateMargin();
  else
    return 0;
}

void QCPAxisRect::layoutChanged() {
  if (mParentPlot && mParentPlot->axisRectCount() > 0 &&
      mParentPlot->axisRect(0) == this) {
    if (axisCount(QCPAxis::atBottom) > 0 && !mParentPlot->xAxis)
      mParentPlot->xAxis = axis(QCPAxis::atBottom);
    if (axisCount(QCPAxis::atLeft) > 0 && !mParentPlot->yAxis)
      mParentPlot->yAxis = axis(QCPAxis::atLeft);
    if (axisCount(QCPAxis::atTop) > 0 && !mParentPlot->xAxis2)
      mParentPlot->xAxis2 = axis(QCPAxis::atTop);
    if (axisCount(QCPAxis::atRight) > 0 && !mParentPlot->yAxis2)
      mParentPlot->yAxis2 = axis(QCPAxis::atRight);
  }
}

void QCPAxisRect::mousePressEvent(QMouseEvent *event, const QVariant &details) {
  Q_UNUSED(details)
  if (event->buttons() & Qt::LeftButton) {
    mDragging = true;

    if (mParentPlot->noAntialiasingOnDrag()) {
      mAADragBackup = mParentPlot->antialiasedElements();
      mNotAADragBackup = mParentPlot->notAntialiasedElements();
    }

    if (mParentPlot->interactions().testFlag(QCP::iRangeDrag)) {
      mDragStartHorzRange.clear();
      for (int i = 0; i < mRangeDragHorzAxis.size(); ++i)
        mDragStartHorzRange.append(mRangeDragHorzAxis.at(i).isNull()
                                       ? QCPRange()
                                       : mRangeDragHorzAxis.at(i)->range());
      mDragStartVertRange.clear();
      for (int i = 0; i < mRangeDragVertAxis.size(); ++i)
        mDragStartVertRange.append(mRangeDragVertAxis.at(i).isNull()
                                       ? QCPRange()
                                       : mRangeDragVertAxis.at(i)->range());
    }
  }
}

void QCPAxisRect::mouseMoveEvent(QMouseEvent *event, const QPointF &startPos) {
  Q_UNUSED(startPos)

  if (mDragging && mParentPlot->interactions().testFlag(QCP::iRangeDrag)) {
    if (mRangeDrag.testFlag(Qt::Horizontal)) {
      for (int i = 0; i < mRangeDragHorzAxis.size(); ++i) {
        QCPAxis *ax = mRangeDragHorzAxis.at(i).data();
        if (!ax)
          continue;
        if (i >= mDragStartHorzRange.size())
          break;
        if (ax->mScaleType == QCPAxis::stLinear) {
          double diff = ax->pixelToCoord(startPos.x()) -
                        ax->pixelToCoord(event->pos().x());
          ax->setRange(mDragStartHorzRange.at(i).lower + diff,
                       mDragStartHorzRange.at(i).upper + diff);
        } else if (ax->mScaleType == QCPAxis::stLogarithmic) {
          double diff = ax->pixelToCoord(startPos.x()) /
                        ax->pixelToCoord(event->pos().x());
          ax->setRange(mDragStartHorzRange.at(i).lower * diff,
                       mDragStartHorzRange.at(i).upper * diff);
        }
      }
    }

    if (mRangeDrag.testFlag(Qt::Vertical)) {
      for (int i = 0; i < mRangeDragVertAxis.size(); ++i) {
        QCPAxis *ax = mRangeDragVertAxis.at(i).data();
        if (!ax)
          continue;
        if (i >= mDragStartVertRange.size())
          break;
        if (ax->mScaleType == QCPAxis::stLinear) {
          double diff = ax->pixelToCoord(startPos.y()) -
                        ax->pixelToCoord(event->pos().y());
          ax->setRange(mDragStartVertRange.at(i).lower + diff,
                       mDragStartVertRange.at(i).upper + diff);
        } else if (ax->mScaleType == QCPAxis::stLogarithmic) {
          double diff = ax->pixelToCoord(startPos.y()) /
                        ax->pixelToCoord(event->pos().y());
          ax->setRange(mDragStartVertRange.at(i).lower * diff,
                       mDragStartVertRange.at(i).upper * diff);
        }
      }
    }

    if (mRangeDrag != 0)

    {
      if (mParentPlot->noAntialiasingOnDrag())
        mParentPlot->setNotAntialiasedElements(QCP::aeAll);
      mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
  }
}

void QCPAxisRect::mouseReleaseEvent(QMouseEvent *event,
                                    const QPointF &startPos) {
  Q_UNUSED(event)
  Q_UNUSED(startPos)
  mDragging = false;
  if (mParentPlot->noAntialiasingOnDrag()) {
    mParentPlot->setAntialiasedElements(mAADragBackup);
    mParentPlot->setNotAntialiasedElements(mNotAADragBackup);
  }
}

void QCPAxisRect::wheelEvent(QWheelEvent *event) {
  if (mParentPlot->interactions().testFlag(QCP::iRangeZoom)) {
    if (mRangeZoom != 0) {
      double factor;
      double wheelSteps = event->delta() / 120.0;

      if (mRangeZoom.testFlag(Qt::Horizontal)) {
        factor = qPow(mRangeZoomFactorHorz, wheelSteps);
        for (int i = 0; i < mRangeZoomHorzAxis.size(); ++i) {
          if (!mRangeZoomHorzAxis.at(i).isNull())
            mRangeZoomHorzAxis.at(i)->scaleRange(
                factor,
                mRangeZoomHorzAxis.at(i)->pixelToCoord(event->pos().x()));
        }
      }
      if (mRangeZoom.testFlag(Qt::Vertical)) {
        factor = qPow(mRangeZoomFactorVert, wheelSteps);
        for (int i = 0; i < mRangeZoomVertAxis.size(); ++i) {
          if (!mRangeZoomVertAxis.at(i).isNull())
            mRangeZoomVertAxis.at(i)->scaleRange(
                factor,
                mRangeZoomVertAxis.at(i)->pixelToCoord(event->pos().y()));
        }
      }
      mParentPlot->replot();
    }
  }
}

QCPAbstractLegendItem::QCPAbstractLegendItem(QCPLegend *parent)
    : QCPLayoutElement(parent->parentPlot()), mParentLegend(parent),
      mFont(parent->font()), mTextColor(parent->textColor()),
      mSelectedFont(parent->selectedFont()),
      mSelectedTextColor(parent->selectedTextColor()), mSelectable(true),
      mSelected(false) {
  setLayer(QLatin1String("legend"));
  setMargins(QMargins(0, 0, 0, 0));
}

void QCPAbstractLegendItem::setFont(const QFont &font) { mFont = font; }

void QCPAbstractLegendItem::setTextColor(const QColor &color) {
  mTextColor = color;
}

void QCPAbstractLegendItem::setSelectedFont(const QFont &font) {
  mSelectedFont = font;
}

void QCPAbstractLegendItem::setSelectedTextColor(const QColor &color) {
  mSelectedTextColor = color;
}

void QCPAbstractLegendItem::setSelectable(bool selectable) {
  if (mSelectable != selectable) {
    mSelectable = selectable;
    Q_EMIT selectableChanged(mSelectable);
  }
}

void QCPAbstractLegendItem::setSelected(bool selected) {
  if (mSelected != selected) {
    mSelected = selected;
    Q_EMIT selectionChanged(mSelected);
  }
}

double QCPAbstractLegendItem::selectTest(const QPointF &pos,
                                         bool onlySelectable,
                                         QVariant *details) const {
  Q_UNUSED(details)
  if (!mParentPlot)
    return -1;
  if (onlySelectable &&
      (!mSelectable ||
       !mParentLegend->selectableParts().testFlag(QCPLegend::spItems)))
    return -1;

  if (mRect.contains(pos.toPoint()))
    return mParentPlot->selectionTolerance() * 0.99;
  else
    return -1;
}

void QCPAbstractLegendItem::applyDefaultAntialiasingHint(
    QCPPainter *painter) const {
  applyAntialiasingHint(painter, mAntialiased, QCP::aeLegendItems);
}

QRect QCPAbstractLegendItem::clipRect() const { return mOuterRect; }

void QCPAbstractLegendItem::selectEvent(QMouseEvent *event, bool additive,
                                        const QVariant &details,
                                        bool *selectionStateChanged) {
  Q_UNUSED(event)
  Q_UNUSED(details)
  if (mSelectable &&
      mParentLegend->selectableParts().testFlag(QCPLegend::spItems)) {
    bool selBefore = mSelected;
    setSelected(additive ? !mSelected : true);
    if (selectionStateChanged)
      *selectionStateChanged = mSelected != selBefore;
  }
}

void QCPAbstractLegendItem::deselectEvent(bool *selectionStateChanged) {
  if (mSelectable &&
      mParentLegend->selectableParts().testFlag(QCPLegend::spItems)) {
    bool selBefore = mSelected;
    setSelected(false);
    if (selectionStateChanged)
      *selectionStateChanged = mSelected != selBefore;
  }
}

QCPPlottableLegendItem::QCPPlottableLegendItem(QCPLegend *parent,
                                               QCPAbstractPlottable *plottable)
    : QCPAbstractLegendItem(parent), mPlottable(plottable) {
  setAntialiased(false);
}

QPen QCPPlottableLegendItem::getIconBorderPen() const {
  return mSelected ? mParentLegend->selectedIconBorderPen()
                   : mParentLegend->iconBorderPen();
}

QColor QCPPlottableLegendItem::getTextColor() const {
  return mSelected ? mSelectedTextColor : mTextColor;
}

QFont QCPPlottableLegendItem::getFont() const {
  return mSelected ? mSelectedFont : mFont;
}

void QCPPlottableLegendItem::draw(QCPPainter *painter) {
  if (!mPlottable)
    return;
  painter->setFont(getFont());
  painter->setPen(QPen(getTextColor()));
  QSizeF iconSize = mParentLegend->iconSize();
  QRectF textRect = painter->fontMetrics().boundingRect(
      0, 0, 0, iconSize.height(), Qt::TextDontClip, mPlottable->name());
  QRectF iconRect(mRect.topLeft(), iconSize);
  int textHeight = qMax(textRect.height(), iconSize.height());

  painter->drawText(mRect.x() + iconSize.width() +
                        mParentLegend->iconTextPadding(),
                    mRect.y(), textRect.width(), textHeight, Qt::TextDontClip,
                    mPlottable->name());

  painter->save();
  painter->setClipRect(iconRect, Qt::IntersectClip);
  mPlottable->drawLegendIcon(painter, iconRect);
  painter->restore();

  if (getIconBorderPen().style() != Qt::NoPen) {
    painter->setPen(getIconBorderPen());
    painter->setBrush(Qt::NoBrush);
    int halfPen = qCeil(painter->pen().widthF() * 0.5) + 1;
    painter->setClipRect(
        mOuterRect.adjusted(-halfPen, -halfPen, halfPen, halfPen));

    painter->drawRect(iconRect);
  }
}

QSize QCPPlottableLegendItem::minimumOuterSizeHint() const {
  if (!mPlottable)
    return QSize();
  QSize result(0, 0);
  QRect textRect;
  QFontMetrics fontMetrics(getFont());
  QSize iconSize = mParentLegend->iconSize();
  textRect = fontMetrics.boundingRect(0, 0, 0, iconSize.height(),
                                      Qt::TextDontClip, mPlottable->name());
  result.setWidth(iconSize.width() + mParentLegend->iconTextPadding() +
                  textRect.width());
  result.setHeight(qMax(textRect.height(), iconSize.height()));
  result.rwidth() += mMargins.left() + mMargins.right();
  result.rheight() += mMargins.top() + mMargins.bottom();
  return result;
}

QCPLegend::QCPLegend() {
  setFillOrder(QCPLayoutGrid::foRowsFirst);
  setWrap(0);

  setRowSpacing(3);
  setColumnSpacing(8);
  setMargins(QMargins(7, 5, 7, 4));
  setAntialiased(false);
  setIconSize(32, 18);

  setIconTextPadding(7);

  setSelectableParts(spLegendBox | spItems);
  setSelectedParts(spNone);

  setBorderPen(QPen(Qt::black, 0));
  setSelectedBorderPen(QPen(Qt::blue, 2));
  setIconBorderPen(Qt::NoPen);
  setSelectedIconBorderPen(QPen(Qt::blue, 2));
  setBrush(Qt::white);
  setSelectedBrush(Qt::white);
  setTextColor(Qt::black);
  setSelectedTextColor(Qt::blue);
}

QCPLegend::~QCPLegend() {
  clearItems();
  if (qobject_cast<QCustomPlot *>(mParentPlot))

    mParentPlot->legendRemoved(this);
}

QCPLegend::SelectableParts QCPLegend::selectedParts() const {
  bool hasSelectedItems = false;
  for (int i = 0; i < itemCount(); ++i) {
    if (item(i) && item(i)->selected()) {
      hasSelectedItems = true;
      break;
    }
  }
  if (hasSelectedItems)
    return mSelectedParts | spItems;
  else
    return mSelectedParts & ~spItems;
}

void QCPLegend::setBorderPen(const QPen &pen) { mBorderPen = pen; }

void QCPLegend::setBrush(const QBrush &brush) { mBrush = brush; }

void QCPLegend::setFont(const QFont &font) {
  mFont = font;
  for (int i = 0; i < itemCount(); ++i) {
    if (item(i))
      item(i)->setFont(mFont);
  }
}

void QCPLegend::setTextColor(const QColor &color) {
  mTextColor = color;
  for (int i = 0; i < itemCount(); ++i) {
    if (item(i))
      item(i)->setTextColor(color);
  }
}

void QCPLegend::setIconSize(const QSize &size) { mIconSize = size; }

void QCPLegend::setIconSize(int width, int height) {
  mIconSize.setWidth(width);
  mIconSize.setHeight(height);
}

void QCPLegend::setIconTextPadding(int padding) { mIconTextPadding = padding; }

void QCPLegend::setIconBorderPen(const QPen &pen) { mIconBorderPen = pen; }

void QCPLegend::setSelectableParts(const SelectableParts &selectable) {
  if (mSelectableParts != selectable) {
    mSelectableParts = selectable;
    Q_EMIT selectableChanged(mSelectableParts);
  }
}

void QCPLegend::setSelectedParts(const SelectableParts &selected) {
  SelectableParts newSelected = selected;
  mSelectedParts = this->selectedParts();

  if (mSelectedParts != newSelected) {
    if (!mSelectedParts.testFlag(spItems) && newSelected.testFlag(spItems))

    {
      qDebug() << Q_FUNC_INFO
               << "spItems flag can not be set, it can only be unset with this "
                  "function";
      newSelected &= ~spItems;
    }
    if (mSelectedParts.testFlag(spItems) && !newSelected.testFlag(spItems))

    {
      for (int i = 0; i < itemCount(); ++i) {
        if (item(i))
          item(i)->setSelected(false);
      }
    }
    mSelectedParts = newSelected;
    Q_EMIT selectionChanged(mSelectedParts);
  }
}

void QCPLegend::setSelectedBorderPen(const QPen &pen) {
  mSelectedBorderPen = pen;
}

void QCPLegend::setSelectedIconBorderPen(const QPen &pen) {
  mSelectedIconBorderPen = pen;
}

void QCPLegend::setSelectedBrush(const QBrush &brush) {
  mSelectedBrush = brush;
}

void QCPLegend::setSelectedFont(const QFont &font) {
  mSelectedFont = font;
  for (int i = 0; i < itemCount(); ++i) {
    if (item(i))
      item(i)->setSelectedFont(font);
  }
}

void QCPLegend::setSelectedTextColor(const QColor &color) {
  mSelectedTextColor = color;
  for (int i = 0; i < itemCount(); ++i) {
    if (item(i))
      item(i)->setSelectedTextColor(color);
  }
}

QCPAbstractLegendItem *QCPLegend::item(int index) const {
  return qobject_cast<QCPAbstractLegendItem *>(elementAt(index));
}

QCPPlottableLegendItem *
QCPLegend::itemWithPlottable(const QCPAbstractPlottable *plottable) const {
  for (int i = 0; i < itemCount(); ++i) {
    if (QCPPlottableLegendItem *pli =
            qobject_cast<QCPPlottableLegendItem *>(item(i))) {
      if (pli->plottable() == plottable)
        return pli;
    }
  }
  return 0;
}

int QCPLegend::itemCount() const { return elementCount(); }

bool QCPLegend::hasItem(QCPAbstractLegendItem *item) const {
  for (int i = 0; i < itemCount(); ++i) {
    if (item == this->item(i))
      return true;
  }
  return false;
}

bool QCPLegend::hasItemWithPlottable(
    const QCPAbstractPlottable *plottable) const {
  return itemWithPlottable(plottable);
}

bool QCPLegend::addItem(QCPAbstractLegendItem *item) {
  return addElement(item);
}

bool QCPLegend::removeItem(int index) {
  if (QCPAbstractLegendItem *ali = item(index)) {
    bool success = remove(ali);
    if (success)
      setFillOrder(fillOrder(), true);

    return success;
  } else
    return false;
}

bool QCPLegend::removeItem(QCPAbstractLegendItem *item) {
  bool success = remove(item);
  if (success)
    setFillOrder(fillOrder(), true);

  return success;
}

void QCPLegend::clearItems() {
  for (int i = itemCount() - 1; i >= 0; --i)
    removeItem(i);
}

QList<QCPAbstractLegendItem *> QCPLegend::selectedItems() const {
  QList<QCPAbstractLegendItem *> result;
  for (int i = 0; i < itemCount(); ++i) {
    if (QCPAbstractLegendItem *ali = item(i)) {
      if (ali->selected())
        result.append(ali);
    }
  }
  return result;
}

void QCPLegend::applyDefaultAntialiasingHint(QCPPainter *painter) const {
  applyAntialiasingHint(painter, mAntialiased, QCP::aeLegend);
}

QPen QCPLegend::getBorderPen() const {
  return mSelectedParts.testFlag(spLegendBox) ? mSelectedBorderPen : mBorderPen;
}

QBrush QCPLegend::getBrush() const {
  return mSelectedParts.testFlag(spLegendBox) ? mSelectedBrush : mBrush;
}

void QCPLegend::draw(QCPPainter *painter) {
  painter->setBrush(getBrush());
  painter->setPen(getBorderPen());
  painter->drawRect(mOuterRect);
}

double QCPLegend::selectTest(const QPointF &pos, bool onlySelectable,
                             QVariant *details) const {
  if (!mParentPlot)
    return -1;
  if (onlySelectable && !mSelectableParts.testFlag(spLegendBox))
    return -1;

  if (mOuterRect.contains(pos.toPoint())) {
    if (details)
      details->setValue(spLegendBox);
    return mParentPlot->selectionTolerance() * 0.99;
  }
  return -1;
}

void QCPLegend::selectEvent(QMouseEvent *event, bool additive,
                            const QVariant &details,
                            bool *selectionStateChanged) {
  Q_UNUSED(event)
  mSelectedParts = selectedParts();

  if (details.value<SelectablePart>() == spLegendBox &&
      mSelectableParts.testFlag(spLegendBox)) {
    SelectableParts selBefore = mSelectedParts;
    setSelectedParts(additive ? mSelectedParts ^ spLegendBox
                              : mSelectedParts | spLegendBox);

    if (selectionStateChanged)
      *selectionStateChanged = mSelectedParts != selBefore;
  }
}

void QCPLegend::deselectEvent(bool *selectionStateChanged) {
  mSelectedParts = selectedParts();

  if (mSelectableParts.testFlag(spLegendBox)) {
    SelectableParts selBefore = mSelectedParts;
    setSelectedParts(selectedParts() & ~spLegendBox);
    if (selectionStateChanged)
      *selectionStateChanged = mSelectedParts != selBefore;
  }
}

QCP::Interaction QCPLegend::selectionCategory() const {
  return QCP::iSelectLegend;
}

QCP::Interaction QCPAbstractLegendItem::selectionCategory() const {
  return QCP::iSelectLegend;
}

void QCPLegend::parentPlotInitialized(QCustomPlot *parentPlot) {
  if (parentPlot && !parentPlot->legend)
    parentPlot->legend = this;
}

QCPTextElement::QCPTextElement(QCustomPlot *parentPlot)
    : QCPLayoutElement(parentPlot), mText(),
      mTextFlags(Qt::AlignCenter | Qt::TextWordWrap),
      mFont(QFont(QLatin1String("sans serif"), 12)),

      mTextColor(Qt::black),
      mSelectedFont(QFont(QLatin1String("sans serif"), 12)),

      mSelectedTextColor(Qt::blue), mSelectable(false), mSelected(false) {
  if (parentPlot) {
    mFont = parentPlot->font();
    mSelectedFont = parentPlot->font();
  }
  setMargins(QMargins(2, 2, 2, 2));
}

QCPTextElement::QCPTextElement(QCustomPlot *parentPlot, const QString &text)
    : QCPLayoutElement(parentPlot), mText(text),
      mTextFlags(Qt::AlignCenter | Qt::TextWordWrap),
      mFont(QFont(QLatin1String("sans serif"), 12)),

      mTextColor(Qt::black),
      mSelectedFont(QFont(QLatin1String("sans serif"), 12)),

      mSelectedTextColor(Qt::blue), mSelectable(false), mSelected(false) {
  if (parentPlot) {
    mFont = parentPlot->font();
    mSelectedFont = parentPlot->font();
  }
  setMargins(QMargins(2, 2, 2, 2));
}

QCPTextElement::QCPTextElement(QCustomPlot *parentPlot, const QString &text,
                               double pointSize)
    : QCPLayoutElement(parentPlot), mText(text),
      mTextFlags(Qt::AlignCenter | Qt::TextWordWrap),
      mFont(QFont(QLatin1String("sans serif"), pointSize)),

      mTextColor(Qt::black),
      mSelectedFont(QFont(QLatin1String("sans serif"), pointSize)),

      mSelectedTextColor(Qt::blue), mSelectable(false), mSelected(false) {
  if (parentPlot) {
    mFont = parentPlot->font();
    mFont.setPointSizeF(pointSize);
    mSelectedFont = parentPlot->font();
    mSelectedFont.setPointSizeF(pointSize);
  }
  setMargins(QMargins(2, 2, 2, 2));
}

QCPTextElement::QCPTextElement(QCustomPlot *parentPlot, const QString &text,
                               const QString &fontFamily, double pointSize)
    : QCPLayoutElement(parentPlot), mText(text),
      mTextFlags(Qt::AlignCenter | Qt::TextWordWrap),
      mFont(QFont(fontFamily, pointSize)), mTextColor(Qt::black),
      mSelectedFont(QFont(fontFamily, pointSize)), mSelectedTextColor(Qt::blue),
      mSelectable(false), mSelected(false) {
  setMargins(QMargins(2, 2, 2, 2));
}

QCPTextElement::QCPTextElement(QCustomPlot *parentPlot, const QString &text,
                               const QFont &font)
    : QCPLayoutElement(parentPlot), mText(text),
      mTextFlags(Qt::AlignCenter | Qt::TextWordWrap), mFont(font),
      mTextColor(Qt::black), mSelectedFont(font), mSelectedTextColor(Qt::blue),
      mSelectable(false), mSelected(false) {
  setMargins(QMargins(2, 2, 2, 2));
}

void QCPTextElement::setText(const QString &text) { mText = text; }

void QCPTextElement::setTextFlags(int flags) { mTextFlags = flags; }

void QCPTextElement::setFont(const QFont &font) { mFont = font; }

void QCPTextElement::setTextColor(const QColor &color) { mTextColor = color; }

void QCPTextElement::setSelectedFont(const QFont &font) {
  mSelectedFont = font;
}

void QCPTextElement::setSelectedTextColor(const QColor &color) {
  mSelectedTextColor = color;
}

void QCPTextElement::setSelectable(bool selectable) {
  if (mSelectable != selectable) {
    mSelectable = selectable;
    Q_EMIT selectableChanged(mSelectable);
  }
}

void QCPTextElement::setSelected(bool selected) {
  if (mSelected != selected) {
    mSelected = selected;
    Q_EMIT selectionChanged(mSelected);
  }
}

void QCPTextElement::applyDefaultAntialiasingHint(QCPPainter *painter) const {
  applyAntialiasingHint(painter, mAntialiased, QCP::aeOther);
}

void QCPTextElement::draw(QCPPainter *painter) {
  painter->setFont(mainFont());
  painter->setPen(QPen(mainTextColor()));
  painter->drawText(mRect, Qt::AlignCenter, mText, &mTextBoundingRect);
}

QSize QCPTextElement::minimumOuterSizeHint() const {
  QFontMetrics metrics(mFont);
  QSize result(metrics.boundingRect(0, 0, 0, 0, Qt::AlignCenter, mText).size());
  result.rwidth() += mMargins.left() + mMargins.right();
  result.rheight() += mMargins.top() + mMargins.bottom();
  return result;
}

QSize QCPTextElement::maximumOuterSizeHint() const {
  QFontMetrics metrics(mFont);
  QSize result(metrics.boundingRect(0, 0, 0, 0, Qt::AlignCenter, mText).size());
  result.setWidth(QWIDGETSIZE_MAX);
  result.rheight() += mMargins.top() + mMargins.bottom();
  return result;
}

void QCPTextElement::selectEvent(QMouseEvent *event, bool additive,
                                 const QVariant &details,
                                 bool *selectionStateChanged) {
  Q_UNUSED(event)
  Q_UNUSED(details)
  if (mSelectable) {
    bool selBefore = mSelected;
    setSelected(additive ? !mSelected : true);
    if (selectionStateChanged)
      *selectionStateChanged = mSelected != selBefore;
  }
}

void QCPTextElement::deselectEvent(bool *selectionStateChanged) {
  if (mSelectable) {
    bool selBefore = mSelected;
    setSelected(false);
    if (selectionStateChanged)
      *selectionStateChanged = mSelected != selBefore;
  }
}

double QCPTextElement::selectTest(const QPointF &pos, bool onlySelectable,
                                  QVariant *details) const {
  Q_UNUSED(details)
  if (onlySelectable && !mSelectable)
    return -1;

  if (mTextBoundingRect.contains(pos.toPoint()))
    return mParentPlot->selectionTolerance() * 0.99;
  else
    return -1;
}

void QCPTextElement::mousePressEvent(QMouseEvent *event,
                                     const QVariant &details) {
  Q_UNUSED(details)
  event->accept();
}

void QCPTextElement::mouseReleaseEvent(QMouseEvent *event,
                                       const QPointF &startPos) {
  if ((QPointF(event->pos()) - startPos).manhattanLength() <= 3)
    Q_EMIT clicked(event);
}

void QCPTextElement::mouseDoubleClickEvent(QMouseEvent *event,
                                           const QVariant &details) {
  Q_UNUSED(details)
  Q_EMIT doubleClicked(event);
}

QFont QCPTextElement::mainFont() const {
  return mSelected ? mSelectedFont : mFont;
}

QColor QCPTextElement::mainTextColor() const {
  return mSelected ? mSelectedTextColor : mTextColor;
}

QCPColorScale::QCPColorScale(QCustomPlot *parentPlot)
    : QCPLayoutElement(parentPlot), mType(QCPAxis::atTop),

      mDataScaleType(QCPAxis::stLinear), mBarWidth(20),
      mAxisRect(new QCPColorScaleAxisRectPrivate(this)) {
  setMinimumMargins(QMargins(0, 6, 0, 6));

  setType(QCPAxis::atRight);
  setDataRange(QCPRange(0, 6));
}

QCPColorScale::~QCPColorScale() { delete mAxisRect; }

QString QCPColorScale::label() const {
  if (!mColorAxis) {
    qDebug() << Q_FUNC_INFO << "internal color axis undefined";
    return QString();
  }

  return mColorAxis.data()->label();
}

bool QCPColorScale::rangeDrag() const {
  if (!mAxisRect) {
    qDebug() << Q_FUNC_INFO << "internal axis rect was deleted";
    return false;
  }

  return mAxisRect.data()->rangeDrag().testFlag(QCPAxis::orientation(mType)) &&
         mAxisRect.data()->rangeDragAxis(QCPAxis::orientation(mType)) &&
         mAxisRect.data()
                 ->rangeDragAxis(QCPAxis::orientation(mType))
                 ->orientation() == QCPAxis::orientation(mType);
}

bool QCPColorScale::rangeZoom() const {
  if (!mAxisRect) {
    qDebug() << Q_FUNC_INFO << "internal axis rect was deleted";
    return false;
  }

  return mAxisRect.data()->rangeZoom().testFlag(QCPAxis::orientation(mType)) &&
         mAxisRect.data()->rangeZoomAxis(QCPAxis::orientation(mType)) &&
         mAxisRect.data()
                 ->rangeZoomAxis(QCPAxis::orientation(mType))
                 ->orientation() == QCPAxis::orientation(mType);
}

void QCPColorScale::setType(QCPAxis::AxisType type) {
  if (!mAxisRect) {
    qDebug() << Q_FUNC_INFO << "internal axis rect was deleted";
    return;
  }
  if (mType != type) {
    mType = type;
    QCPRange rangeTransfer(0, 6);
    QString labelTransfer;
    QSharedPointer<QCPAxisTicker> tickerTransfer;

    bool doTransfer = (bool)mColorAxis;
    if (doTransfer) {
      rangeTransfer = mColorAxis.data()->range();
      labelTransfer = mColorAxis.data()->label();
      tickerTransfer = mColorAxis.data()->ticker();
      mColorAxis.data()->setLabel(QString());
      disconnect(mColorAxis.data(), SIGNAL(rangeChanged(QCPRange)), this,
                 SLOT(setDataRange(QCPRange)));
      disconnect(mColorAxis.data(),
                 SIGNAL(scaleTypeChanged(QCPAxis::ScaleType)), this,
                 SLOT(setDataScaleType(QCPAxis::ScaleType)));
    }
    QList<QCPAxis::AxisType> allAxisTypes =
        QList<QCPAxis::AxisType>() << QCPAxis::atLeft << QCPAxis::atRight
                                   << QCPAxis::atBottom << QCPAxis::atTop;
    Q_FOREACH (QCPAxis::AxisType atype, allAxisTypes) {
      mAxisRect.data()->axis(atype)->setTicks(atype == mType);
      mAxisRect.data()->axis(atype)->setTickLabels(atype == mType);
    }

    mColorAxis = mAxisRect.data()->axis(mType);

    if (doTransfer) {
      mColorAxis.data()->setRange(rangeTransfer);

      mColorAxis.data()->setLabel(labelTransfer);
      mColorAxis.data()->setTicker(tickerTransfer);
    }
    connect(mColorAxis.data(), SIGNAL(rangeChanged(QCPRange)), this,
            SLOT(setDataRange(QCPRange)));
    connect(mColorAxis.data(), SIGNAL(scaleTypeChanged(QCPAxis::ScaleType)),
            this, SLOT(setDataScaleType(QCPAxis::ScaleType)));
    mAxisRect.data()->setRangeDragAxes(QList<QCPAxis *>() << mColorAxis.data());
  }
}

void QCPColorScale::setDataRange(const QCPRange &dataRange) {
  if (mDataRange.lower != dataRange.lower ||
      mDataRange.upper != dataRange.upper) {
    mDataRange = dataRange;
    if (mColorAxis)
      mColorAxis.data()->setRange(mDataRange);
    Q_EMIT dataRangeChanged(mDataRange);
  }
}

void QCPColorScale::setDataScaleType(QCPAxis::ScaleType scaleType) {
  if (mDataScaleType != scaleType) {
    mDataScaleType = scaleType;
    if (mColorAxis)
      mColorAxis.data()->setScaleType(mDataScaleType);
    if (mDataScaleType == QCPAxis::stLogarithmic)
      setDataRange(mDataRange.sanitizedForLogScale());
    Q_EMIT dataScaleTypeChanged(mDataScaleType);
  }
}

void QCPColorScale::setGradient(const QCPColorGradient &gradient) {
  if (mGradient != gradient) {
    mGradient = gradient;
    if (mAxisRect)
      mAxisRect.data()->mGradientImageInvalidated = true;
    Q_EMIT gradientChanged(mGradient);
  }
}

void QCPColorScale::setLabel(const QString &str) {
  if (!mColorAxis) {
    qDebug() << Q_FUNC_INFO << "internal color axis undefined";
    return;
  }

  mColorAxis.data()->setLabel(str);
}

void QCPColorScale::setBarWidth(int width) { mBarWidth = width; }

void QCPColorScale::setRangeDrag(bool enabled) {
  if (!mAxisRect) {
    qDebug() << Q_FUNC_INFO << "internal axis rect was deleted";
    return;
  }

  if (enabled)
    mAxisRect.data()->setRangeDrag(QCPAxis::orientation(mType));
  else
    mAxisRect.data()->setRangeDrag(0);
}

void QCPColorScale::setRangeZoom(bool enabled) {
  if (!mAxisRect) {
    qDebug() << Q_FUNC_INFO << "internal axis rect was deleted";
    return;
  }

  if (enabled)
    mAxisRect.data()->setRangeZoom(QCPAxis::orientation(mType));
  else
    mAxisRect.data()->setRangeZoom(0);
}

QList<QCPColorMap *> QCPColorScale::colorMaps() const {
  QList<QCPColorMap *> result;
  for (int i = 0; i < mParentPlot->plottableCount(); ++i) {
    if (QCPColorMap *cm =
            qobject_cast<QCPColorMap *>(mParentPlot->plottable(i)))
      if (cm->colorScale() == this)
        result.append(cm);
  }
  return result;
}

void QCPColorScale::rescaleDataRange(bool onlyVisibleMaps) {
  QList<QCPColorMap *> maps = colorMaps();
  QCPRange newRange;
  bool haveRange = false;
  QCP::SignDomain sign = QCP::sdBoth;
  if (mDataScaleType == QCPAxis::stLogarithmic)
    sign = (mDataRange.upper < 0 ? QCP::sdNegative : QCP::sdPositive);
  for (int i = 0; i < maps.size(); ++i) {
    if (!maps.at(i)->realVisibility() && onlyVisibleMaps)
      continue;
    QCPRange mapRange;
    if (maps.at(i)->colorScale() == this) {
      bool currentFoundRange = true;
      mapRange = maps.at(i)->data()->dataBounds();
      if (sign == QCP::sdPositive) {
        if (mapRange.lower <= 0 && mapRange.upper > 0)
          mapRange.lower = mapRange.upper * 1e-3;
        else if (mapRange.lower <= 0 && mapRange.upper <= 0)
          currentFoundRange = false;
      } else if (sign == QCP::sdNegative) {
        if (mapRange.upper >= 0 && mapRange.lower < 0)
          mapRange.upper = mapRange.lower * 1e-3;
        else if (mapRange.upper >= 0 && mapRange.lower >= 0)
          currentFoundRange = false;
      }
      if (currentFoundRange) {
        if (!haveRange)
          newRange = mapRange;
        else
          newRange.expand(mapRange);
        haveRange = true;
      }
    }
  }
  if (haveRange) {
    if (!QCPRange::validRange(newRange))

    {
      double center = (newRange.lower + newRange.upper) * 0.5;

      if (mDataScaleType == QCPAxis::stLinear) {
        newRange.lower = center - mDataRange.size() / 2.0;
        newRange.upper = center + mDataRange.size() / 2.0;
      } else

      {
        newRange.lower = center / qSqrt(mDataRange.upper / mDataRange.lower);
        newRange.upper = center * qSqrt(mDataRange.upper / mDataRange.lower);
      }
    }
    setDataRange(newRange);
  }
}

void QCPColorScale::update(UpdatePhase phase) {
  QCPLayoutElement::update(phase);
  if (!mAxisRect) {
    qDebug() << Q_FUNC_INFO << "internal axis rect was deleted";
    return;
  }

  mAxisRect.data()->update(phase);

  switch (phase) {
  case upMargins: {
    if (mType == QCPAxis::atBottom || mType == QCPAxis::atTop) {
      setMaximumSize(QWIDGETSIZE_MAX, mBarWidth +
                                          mAxisRect.data()->margins().top() +
                                          mAxisRect.data()->margins().bottom());
      setMinimumSize(0, mBarWidth + mAxisRect.data()->margins().top() +
                            mAxisRect.data()->margins().bottom());
    } else {
      setMaximumSize(mBarWidth + mAxisRect.data()->margins().left() +
                         mAxisRect.data()->margins().right(),
                     QWIDGETSIZE_MAX);
      setMinimumSize(mBarWidth + mAxisRect.data()->margins().left() +
                         mAxisRect.data()->margins().right(),
                     0);
    }
    break;
  }
  case upLayout: {
    mAxisRect.data()->setOuterRect(rect());
    break;
  }
  default:
    break;
  }
}

void QCPColorScale::applyDefaultAntialiasingHint(QCPPainter *painter) const {
  painter->setAntialiasing(false);
}

void QCPColorScale::mousePressEvent(QMouseEvent *event,
                                    const QVariant &details) {
  if (!mAxisRect) {
    qDebug() << Q_FUNC_INFO << "internal axis rect was deleted";
    return;
  }
  mAxisRect.data()->mousePressEvent(event, details);
}

void QCPColorScale::mouseMoveEvent(QMouseEvent *event,
                                   const QPointF &startPos) {
  if (!mAxisRect) {
    qDebug() << Q_FUNC_INFO << "internal axis rect was deleted";
    return;
  }
  mAxisRect.data()->mouseMoveEvent(event, startPos);
}

void QCPColorScale::mouseReleaseEvent(QMouseEvent *event,
                                      const QPointF &startPos) {
  if (!mAxisRect) {
    qDebug() << Q_FUNC_INFO << "internal axis rect was deleted";
    return;
  }
  mAxisRect.data()->mouseReleaseEvent(event, startPos);
}

void QCPColorScale::wheelEvent(QWheelEvent *event) {
  if (!mAxisRect) {
    qDebug() << Q_FUNC_INFO << "internal axis rect was deleted";
    return;
  }
  mAxisRect.data()->wheelEvent(event);
}

QCPColorScaleAxisRectPrivate::QCPColorScaleAxisRectPrivate(
    QCPColorScale *parentColorScale)
    : QCPAxisRect(parentColorScale->parentPlot(), true),
      mParentColorScale(parentColorScale), mGradientImageInvalidated(true) {
  setParentLayerable(parentColorScale);
  setMinimumMargins(QMargins(0, 0, 0, 0));
  QList<QCPAxis::AxisType> allAxisTypes =
      QList<QCPAxis::AxisType>() << QCPAxis::atBottom << QCPAxis::atTop
                                 << QCPAxis::atLeft << QCPAxis::atRight;
  Q_FOREACH (QCPAxis::AxisType type, allAxisTypes) {
    axis(type)->setVisible(true);
    axis(type)->grid()->setVisible(false);
    axis(type)->setPadding(0);
    connect(axis(type), SIGNAL(selectionChanged(QCPAxis::SelectableParts)),
            this, SLOT(axisSelectionChanged(QCPAxis::SelectableParts)));
    connect(axis(type), SIGNAL(selectableChanged(QCPAxis::SelectableParts)),
            this, SLOT(axisSelectableChanged(QCPAxis::SelectableParts)));
  }

  connect(axis(QCPAxis::atLeft), SIGNAL(rangeChanged(QCPRange)),
          axis(QCPAxis::atRight), SLOT(setRange(QCPRange)));
  connect(axis(QCPAxis::atRight), SIGNAL(rangeChanged(QCPRange)),
          axis(QCPAxis::atLeft), SLOT(setRange(QCPRange)));
  connect(axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)),
          axis(QCPAxis::atTop), SLOT(setRange(QCPRange)));
  connect(axis(QCPAxis::atTop), SIGNAL(rangeChanged(QCPRange)),
          axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
  connect(axis(QCPAxis::atLeft), SIGNAL(scaleTypeChanged(QCPAxis::ScaleType)),
          axis(QCPAxis::atRight), SLOT(setScaleType(QCPAxis::ScaleType)));
  connect(axis(QCPAxis::atRight), SIGNAL(scaleTypeChanged(QCPAxis::ScaleType)),
          axis(QCPAxis::atLeft), SLOT(setScaleType(QCPAxis::ScaleType)));
  connect(axis(QCPAxis::atBottom), SIGNAL(scaleTypeChanged(QCPAxis::ScaleType)),
          axis(QCPAxis::atTop), SLOT(setScaleType(QCPAxis::ScaleType)));
  connect(axis(QCPAxis::atTop), SIGNAL(scaleTypeChanged(QCPAxis::ScaleType)),
          axis(QCPAxis::atBottom), SLOT(setScaleType(QCPAxis::ScaleType)));

  connect(parentColorScale, SIGNAL(layerChanged(QCPLayer *)), this,
          SLOT(setLayer(QCPLayer *)));
  Q_FOREACH (QCPAxis::AxisType type, allAxisTypes)
    connect(parentColorScale, SIGNAL(layerChanged(QCPLayer *)), axis(type),
            SLOT(setLayer(QCPLayer *)));
}

void QCPColorScaleAxisRectPrivate::draw(QCPPainter *painter) {
  if (mGradientImageInvalidated)
    updateGradientImage();

  bool mirrorHorz = false;
  bool mirrorVert = false;
  if (mParentColorScale->mColorAxis) {
    mirrorHorz = mParentColorScale->mColorAxis.data()->rangeReversed() &&
                 (mParentColorScale->type() == QCPAxis::atBottom ||
                  mParentColorScale->type() == QCPAxis::atTop);
    mirrorVert = mParentColorScale->mColorAxis.data()->rangeReversed() &&
                 (mParentColorScale->type() == QCPAxis::atLeft ||
                  mParentColorScale->type() == QCPAxis::atRight);
  }

  painter->drawImage(rect().adjusted(0, -1, 0, -1),
                     mGradientImage.mirrored(mirrorHorz, mirrorVert));
  QCPAxisRect::draw(painter);
}

void QCPColorScaleAxisRectPrivate::updateGradientImage() {
  if (rect().isEmpty())
    return;

  const QImage::Format format = QImage::Format_ARGB32_Premultiplied;
  int n = mParentColorScale->mGradient.levelCount();
  int w, h;
  QVector<double> data(n);
  for (int i = 0; i < n; ++i)
    data[i] = i;
  if (mParentColorScale->mType == QCPAxis::atBottom ||
      mParentColorScale->mType == QCPAxis::atTop) {
    w = n;
    h = rect().height();
    mGradientImage = QImage(w, h, format);
    QVector<QRgb *> pixels;
    for (int y = 0; y < h; ++y)
      pixels.append(reinterpret_cast<QRgb *>(mGradientImage.scanLine(y)));
    mParentColorScale->mGradient.colorize(data.constData(), QCPRange(0, n - 1),
                                          pixels.first(), n);
    for (int y = 1; y < h; ++y)
      memcpy(pixels.at(y), pixels.first(), n * sizeof(QRgb));
  } else {
    w = rect().width();
    h = n;
    mGradientImage = QImage(w, h, format);
    for (int y = 0; y < h; ++y) {
      QRgb *pixels = reinterpret_cast<QRgb *>(mGradientImage.scanLine(y));
      const QRgb lineColor = mParentColorScale->mGradient.color(
          data[h - 1 - y], QCPRange(0, n - 1));
      for (int x = 0; x < w; ++x)
        pixels[x] = lineColor;
    }
  }
  mGradientImageInvalidated = false;
}

void QCPColorScaleAxisRectPrivate::axisSelectionChanged(
    QCPAxis::SelectableParts selectedParts) {
  QList<QCPAxis::AxisType> allAxisTypes =
      QList<QCPAxis::AxisType>() << QCPAxis::atBottom << QCPAxis::atTop
                                 << QCPAxis::atLeft << QCPAxis::atRight;
  Q_FOREACH (QCPAxis::AxisType type, allAxisTypes) {
    if (QCPAxis *senderAxis = qobject_cast<QCPAxis *>(sender()))
      if (senderAxis->axisType() == type)
        continue;

    if (axis(type)->selectableParts().testFlag(QCPAxis::spAxis)) {
      if (selectedParts.testFlag(QCPAxis::spAxis))
        axis(type)->setSelectedParts(axis(type)->selectedParts() |
                                     QCPAxis::spAxis);
      else
        axis(type)->setSelectedParts(axis(type)->selectedParts() &
                                     ~QCPAxis::spAxis);
    }
  }
}

void QCPColorScaleAxisRectPrivate::axisSelectableChanged(
    QCPAxis::SelectableParts selectableParts) {
  QList<QCPAxis::AxisType> allAxisTypes =
      QList<QCPAxis::AxisType>() << QCPAxis::atBottom << QCPAxis::atTop
                                 << QCPAxis::atLeft << QCPAxis::atRight;
  Q_FOREACH (QCPAxis::AxisType type, allAxisTypes) {
    if (QCPAxis *senderAxis = qobject_cast<QCPAxis *>(sender()))
      if (senderAxis->axisType() == type)
        continue;

    if (axis(type)->selectableParts().testFlag(QCPAxis::spAxis)) {
      if (selectableParts.testFlag(QCPAxis::spAxis))
        axis(type)->setSelectableParts(axis(type)->selectableParts() |
                                       QCPAxis::spAxis);
      else
        axis(type)->setSelectableParts(axis(type)->selectableParts() &
                                       ~QCPAxis::spAxis);
    }
  }
}

QCPGraphData::QCPGraphData() : key(0), value(0) {}

QCPGraphData::QCPGraphData(double key, double value) : key(key), value(value) {}

QCPGraph::QCPGraph(QCPAxis *keyAxis, QCPAxis *valueAxis)
    : QCPAbstractPlottable1D<QCPGraphData>(keyAxis, valueAxis) {
  mParentPlot->registerGraph(this);

  setPen(QPen(Qt::blue, 0));
  setBrush(Qt::NoBrush);

  setLineStyle(lsLine);
  setScatterSkip(0);
  setChannelFillGraph(0);
  setAdaptiveSampling(true);
}

QCPGraph::~QCPGraph() {}

void QCPGraph::setData(QSharedPointer<QCPGraphDataContainer> data) {
  mDataContainer = data;
}

void QCPGraph::setData(const QVector<double> &keys,
                       const QVector<double> &values, bool alreadySorted) {
  mDataContainer->clear();
  addData(keys, values, alreadySorted);
}

void QCPGraph::setLineStyle(LineStyle ls) { mLineStyle = ls; }

void QCPGraph::setScatterStyle(const QCPScatterStyle &style) {
  mScatterStyle = style;
}

void QCPGraph::setScatterSkip(int skip) { mScatterSkip = qMax(0, skip); }

void QCPGraph::setChannelFillGraph(QCPGraph *targetGraph) {
  if (targetGraph == this) {
    qDebug() << Q_FUNC_INFO << "targetGraph is this graph itself";
    mChannelFillGraph = 0;
    return;
  }

  if (targetGraph && targetGraph->mParentPlot != mParentPlot) {
    qDebug() << Q_FUNC_INFO << "targetGraph not in same plot";
    mChannelFillGraph = 0;
    return;
  }

  mChannelFillGraph = targetGraph;
}

void QCPGraph::setAdaptiveSampling(bool enabled) {
  mAdaptiveSampling = enabled;
}

void QCPGraph::addData(const QVector<double> &keys,
                       const QVector<double> &values, bool alreadySorted) {
  if (keys.size() != values.size())
    qDebug() << Q_FUNC_INFO
             << "keys and values have different sizes:" << keys.size()
             << values.size();
  const int n = qMin(keys.size(), values.size());
  QVector<QCPGraphData> tempData(n);
  QVector<QCPGraphData>::iterator it = tempData.begin();
  const QVector<QCPGraphData>::iterator itEnd = tempData.end();
  int i = 0;
  while (it != itEnd) {
    it->key = keys[i];
    it->value = values[i];
    ++it;
    ++i;
  }
  mDataContainer->add(tempData, alreadySorted);
}

void QCPGraph::addData(double key, double value) {
  mDataContainer->add(QCPGraphData(key, value));
}

double QCPGraph::selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details) const {
  if ((onlySelectable && mSelectable == QCP::stNone) ||
      mDataContainer->isEmpty())
    return -1;
  if (!mKeyAxis || !mValueAxis)
    return -1;

  if (mKeyAxis.data()->axisRect()->rect().contains(pos.toPoint())) {
    QCPGraphDataContainer::const_iterator closestDataPoint =
        mDataContainer->constEnd();
    double result = pointDistance(pos, closestDataPoint);
    if (details) {
      int pointIndex = closestDataPoint - mDataContainer->constBegin();
      details->setValue(
          QCPDataSelection(QCPDataRange(pointIndex, pointIndex + 1)));
    }
    return result;
  } else
    return -1;
}

QCPRange QCPGraph::getKeyRange(bool &foundRange,
                               QCP::SignDomain inSignDomain) const {
  return mDataContainer->keyRange(foundRange, inSignDomain);
}

QCPRange QCPGraph::getValueRange(bool &foundRange, QCP::SignDomain inSignDomain,
                                 const QCPRange &inKeyRange) const {
  return mDataContainer->valueRange(foundRange, inSignDomain, inKeyRange);
}

void QCPGraph::draw(QCPPainter *painter) {
  if (!mKeyAxis || !mValueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return;
  }
  if (mKeyAxis.data()->range().size() <= 0 || mDataContainer->isEmpty())
    return;
  if (mLineStyle == lsNone && mScatterStyle.isNone())
    return;

  QVector<QPointF> lines, scatters;

  QList<QCPDataRange> selectedSegments, unselectedSegments, allSegments;
  getDataSegments(selectedSegments, unselectedSegments);
  allSegments << unselectedSegments << selectedSegments;
  for (int i = 0; i < allSegments.size(); ++i) {
    bool isSelectedSegment = i >= unselectedSegments.size();

    QCPDataRange lineDataRange = isSelectedSegment
                                     ? allSegments.at(i)
                                     : allSegments.at(i).adjusted(-1, 1);

    getLines(&lines, lineDataRange);

#ifdef QCUSTOMPLOT_CHECK_DATA
    QCPGraphDataContainer::const_iterator it;
    for (it = mDataContainer->constBegin(); it != mDataContainer->constEnd();
         ++it) {
      if (QCP::isInvalidData(it->key, it->value))
        qDebug() << Q_FUNC_INFO << "Data point at" << it->key << "invalid."
                 << "Plottable name:" << name();
    }
#endif

    if (isSelectedSegment && mSelectionDecorator)
      mSelectionDecorator->applyBrush(painter);
    else
      painter->setBrush(mBrush);
    painter->setPen(Qt::NoPen);
    drawFill(painter, &lines);

    if (mLineStyle != lsNone) {
      if (isSelectedSegment && mSelectionDecorator)
        mSelectionDecorator->applyPen(painter);
      else
        painter->setPen(mPen);
      painter->setBrush(Qt::NoBrush);
      if (mLineStyle == lsImpulse)
        drawImpulsePlot(painter, lines);
      else
        drawLinePlot(painter, lines);
    }

    QCPScatterStyle finalScatterStyle = mScatterStyle;
    if (isSelectedSegment && mSelectionDecorator)
      finalScatterStyle =
          mSelectionDecorator->getFinalScatterStyle(mScatterStyle);
    if (!finalScatterStyle.isNone()) {
      getScatters(&scatters, allSegments.at(i));
      drawScatterPlot(painter, scatters, finalScatterStyle);
    }
  }

  if (mSelectionDecorator)
    mSelectionDecorator->drawDecoration(painter, selection());
}

void QCPGraph::drawLegendIcon(QCPPainter *painter, const QRectF &rect) const {
  if (mBrush.style() != Qt::NoBrush) {
    applyFillAntialiasingHint(painter);
    painter->fillRect(QRectF(rect.left(), rect.top() + rect.height() / 2.0,
                             rect.width(), rect.height() / 3.0),
                      mBrush);
  }

  if (mLineStyle != lsNone) {
    applyDefaultAntialiasingHint(painter);
    painter->setPen(mPen);
    painter->drawLine(QLineF(rect.left(), rect.top() + rect.height() / 2.0,
                             rect.right() + 5,
                             rect.top() + rect.height() / 2.0));
  }

  if (!mScatterStyle.isNone()) {
    applyScattersAntialiasingHint(painter);

    if (mScatterStyle.shape() == QCPScatterStyle::ssPixmap &&
        (mScatterStyle.pixmap().size().width() > rect.width() ||
         mScatterStyle.pixmap().size().height() > rect.height())) {
      QCPScatterStyle scaledStyle(mScatterStyle);
      scaledStyle.setPixmap(scaledStyle.pixmap().scaled(
          rect.size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
      scaledStyle.applyTo(painter, mPen);
      scaledStyle.drawShape(painter, QRectF(rect).center());
    } else {
      mScatterStyle.applyTo(painter, mPen);
      mScatterStyle.drawShape(painter, QRectF(rect).center());
    }
  }
}

void QCPGraph::getLines(QVector<QPointF> *lines,
                        const QCPDataRange &dataRange) const {
  if (!lines)
    return;
  QCPGraphDataContainer::const_iterator begin, end;
  getVisibleDataBounds(begin, end, dataRange);
  if (begin == end) {
    lines->clear();
    return;
  }

  QVector<QCPGraphData> lineData;
  if (mLineStyle != lsNone)
    getOptimizedLineData(&lineData, begin, end);

  if (mKeyAxis->rangeReversed() != (mKeyAxis->orientation() == Qt::Vertical))

    std::reverse(lineData.begin(), lineData.end());

  switch (mLineStyle) {
  case lsNone:
    lines->clear();
    break;
  case lsLine:
    *lines = dataToLines(lineData);
    break;
  case lsStepLeft:
    *lines = dataToStepLeftLines(lineData);
    break;
  case lsStepRight:
    *lines = dataToStepRightLines(lineData);
    break;
  case lsStepCenter:
    *lines = dataToStepCenterLines(lineData);
    break;
  case lsImpulse:
    *lines = dataToImpulseLines(lineData);
    break;
  }
}

void QCPGraph::getScatters(QVector<QPointF> *scatters,
                           const QCPDataRange &dataRange) const {
  if (!scatters)
    return;
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    scatters->clear();
    return;
  }

  QCPGraphDataContainer::const_iterator begin, end;
  getVisibleDataBounds(begin, end, dataRange);
  if (begin == end) {
    scatters->clear();
    return;
  }

  QVector<QCPGraphData> data;
  getOptimizedScatterData(&data, begin, end);

  if (mKeyAxis->rangeReversed() != (mKeyAxis->orientation() == Qt::Vertical))

    std::reverse(data.begin(), data.end());

  scatters->resize(data.size());
  if (keyAxis->orientation() == Qt::Vertical) {
    for (int i = 0; i < data.size(); ++i) {
      if (!qIsNaN(data.at(i).value)) {
        (*scatters)[i].setX(valueAxis->coordToPixel(data.at(i).value));
        (*scatters)[i].setY(keyAxis->coordToPixel(data.at(i).key));
      }
    }
  } else {
    for (int i = 0; i < data.size(); ++i) {
      if (!qIsNaN(data.at(i).value)) {
        (*scatters)[i].setX(keyAxis->coordToPixel(data.at(i).key));
        (*scatters)[i].setY(valueAxis->coordToPixel(data.at(i).value));
      }
    }
  }
}

QVector<QPointF>
QCPGraph::dataToLines(const QVector<QCPGraphData> &data) const {
  QVector<QPointF> result;
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return result;
  }

  result.resize(data.size());

  if (keyAxis->orientation() == Qt::Vertical) {
    for (int i = 0; i < data.size(); ++i) {
      result[i].setX(valueAxis->coordToPixel(data.at(i).value));
      result[i].setY(keyAxis->coordToPixel(data.at(i).key));
    }
  } else

  {
    for (int i = 0; i < data.size(); ++i) {
      result[i].setX(keyAxis->coordToPixel(data.at(i).key));
      result[i].setY(valueAxis->coordToPixel(data.at(i).value));
    }
  }
  return result;
}

QVector<QPointF>
QCPGraph::dataToStepLeftLines(const QVector<QCPGraphData> &data) const {
  QVector<QPointF> result;
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return result;
  }

  result.resize(data.size() * 2);

  if (keyAxis->orientation() == Qt::Vertical) {
    double lastValue = valueAxis->coordToPixel(data.first().value);
    for (int i = 0; i < data.size(); ++i) {
      const double key = keyAxis->coordToPixel(data.at(i).key);
      result[i * 2 + 0].setX(lastValue);
      result[i * 2 + 0].setY(key);
      lastValue = valueAxis->coordToPixel(data.at(i).value);
      result[i * 2 + 1].setX(lastValue);
      result[i * 2 + 1].setY(key);
    }
  } else

  {
    double lastValue = valueAxis->coordToPixel(data.first().value);
    for (int i = 0; i < data.size(); ++i) {
      const double key = keyAxis->coordToPixel(data.at(i).key);
      result[i * 2 + 0].setX(key);
      result[i * 2 + 0].setY(lastValue);
      lastValue = valueAxis->coordToPixel(data.at(i).value);
      result[i * 2 + 1].setX(key);
      result[i * 2 + 1].setY(lastValue);
    }
  }
  return result;
}

QVector<QPointF>
QCPGraph::dataToStepRightLines(const QVector<QCPGraphData> &data) const {
  QVector<QPointF> result;
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return result;
  }

  result.resize(data.size() * 2);

  if (keyAxis->orientation() == Qt::Vertical) {
    double lastKey = keyAxis->coordToPixel(data.first().key);
    for (int i = 0; i < data.size(); ++i) {
      const double value = valueAxis->coordToPixel(data.at(i).value);
      result[i * 2 + 0].setX(value);
      result[i * 2 + 0].setY(lastKey);
      lastKey = keyAxis->coordToPixel(data.at(i).key);
      result[i * 2 + 1].setX(value);
      result[i * 2 + 1].setY(lastKey);
    }
  } else

  {
    double lastKey = keyAxis->coordToPixel(data.first().key);
    for (int i = 0; i < data.size(); ++i) {
      const double value = valueAxis->coordToPixel(data.at(i).value);
      result[i * 2 + 0].setX(lastKey);
      result[i * 2 + 0].setY(value);
      lastKey = keyAxis->coordToPixel(data.at(i).key);
      result[i * 2 + 1].setX(lastKey);
      result[i * 2 + 1].setY(value);
    }
  }
  return result;
}

QVector<QPointF>
QCPGraph::dataToStepCenterLines(const QVector<QCPGraphData> &data) const {
  QVector<QPointF> result;
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return result;
  }

  result.resize(data.size() * 2);

  if (keyAxis->orientation() == Qt::Vertical) {
    double lastKey = keyAxis->coordToPixel(data.first().key);
    double lastValue = valueAxis->coordToPixel(data.first().value);
    result[0].setX(lastValue);
    result[0].setY(lastKey);
    for (int i = 1; i < data.size(); ++i) {
      const double key =
          (keyAxis->coordToPixel(data.at(i).key) + lastKey) * 0.5;
      result[i * 2 - 1].setX(lastValue);
      result[i * 2 - 1].setY(key);
      lastValue = valueAxis->coordToPixel(data.at(i).value);
      lastKey = keyAxis->coordToPixel(data.at(i).key);
      result[i * 2 + 0].setX(lastValue);
      result[i * 2 + 0].setY(key);
    }
    result[data.size() * 2 - 1].setX(lastValue);
    result[data.size() * 2 - 1].setY(lastKey);
  } else

  {
    double lastKey = keyAxis->coordToPixel(data.first().key);
    double lastValue = valueAxis->coordToPixel(data.first().value);
    result[0].setX(lastKey);
    result[0].setY(lastValue);
    for (int i = 1; i < data.size(); ++i) {
      const double key =
          (keyAxis->coordToPixel(data.at(i).key) + lastKey) * 0.5;
      result[i * 2 - 1].setX(key);
      result[i * 2 - 1].setY(lastValue);
      lastValue = valueAxis->coordToPixel(data.at(i).value);
      lastKey = keyAxis->coordToPixel(data.at(i).key);
      result[i * 2 + 0].setX(key);
      result[i * 2 + 0].setY(lastValue);
    }
    result[data.size() * 2 - 1].setX(lastKey);
    result[data.size() * 2 - 1].setY(lastValue);
  }
  return result;
}

QVector<QPointF>
QCPGraph::dataToImpulseLines(const QVector<QCPGraphData> &data) const {
  QVector<QPointF> result;
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return result;
  }

  result.resize(data.size() * 2);

  if (keyAxis->orientation() == Qt::Vertical) {
    for (int i = 0; i < data.size(); ++i) {
      const double key = keyAxis->coordToPixel(data.at(i).key);
      result[i * 2 + 0].setX(valueAxis->coordToPixel(0));
      result[i * 2 + 0].setY(key);
      result[i * 2 + 1].setX(valueAxis->coordToPixel(data.at(i).value));
      result[i * 2 + 1].setY(key);
    }
  } else

  {
    for (int i = 0; i < data.size(); ++i) {
      const double key = keyAxis->coordToPixel(data.at(i).key);
      result[i * 2 + 0].setX(key);
      result[i * 2 + 0].setY(valueAxis->coordToPixel(0));
      result[i * 2 + 1].setX(key);
      result[i * 2 + 1].setY(valueAxis->coordToPixel(data.at(i).value));
    }
  }
  return result;
}

void QCPGraph::drawFill(QCPPainter *painter, QVector<QPointF> *lines) const {
  if (mLineStyle == lsImpulse)
    return;

  if (painter->brush().style() == Qt::NoBrush ||
      painter->brush().color().alpha() == 0)
    return;

  applyFillAntialiasingHint(painter);
  QVector<QCPDataRange> segments =
      getNonNanSegments(lines, keyAxis()->orientation());
  if (!mChannelFillGraph) {
    for (int i = 0; i < segments.size(); ++i)
      painter->drawPolygon(getFillPolygon(lines, segments.at(i)));
  } else {
    QVector<QPointF> otherLines;
    mChannelFillGraph->getLines(
        &otherLines, QCPDataRange(0, mChannelFillGraph->dataCount()));
    if (!otherLines.isEmpty()) {
      QVector<QCPDataRange> otherSegments = getNonNanSegments(
          &otherLines, mChannelFillGraph->keyAxis()->orientation());
      QVector<QPair<QCPDataRange, QCPDataRange>> segmentPairs =
          getOverlappingSegments(segments, lines, otherSegments, &otherLines);
      for (int i = 0; i < segmentPairs.size(); ++i)
        painter->drawPolygon(
            getChannelFillPolygon(lines, segmentPairs.at(i).first, &otherLines,
                                  segmentPairs.at(i).second));
    }
  }
}

void QCPGraph::drawScatterPlot(QCPPainter *painter,
                               const QVector<QPointF> &scatters,
                               const QCPScatterStyle &style) const {
  applyScattersAntialiasingHint(painter);
  style.applyTo(painter, mPen);
  for (int i = 0; i < scatters.size(); ++i)
    style.drawShape(painter, scatters.at(i).x(), scatters.at(i).y());
}

void QCPGraph::drawLinePlot(QCPPainter *painter,
                            const QVector<QPointF> &lines) const {
  if (painter->pen().style() != Qt::NoPen &&
      painter->pen().color().alpha() != 0) {
    applyDefaultAntialiasingHint(painter);
    drawPolyline(painter, lines);
  }
}

void QCPGraph::drawImpulsePlot(QCPPainter *painter,
                               const QVector<QPointF> &lines) const {
  if (painter->pen().style() != Qt::NoPen &&
      painter->pen().color().alpha() != 0) {
    applyDefaultAntialiasingHint(painter);
    QPen oldPen = painter->pen();
    QPen newPen = painter->pen();
    newPen.setCapStyle(Qt::FlatCap);

    painter->setPen(newPen);
    painter->drawLines(lines);
    painter->setPen(oldPen);
  }
}

void QCPGraph::getOptimizedLineData(
    QVector<QCPGraphData> *lineData,
    const QCPGraphDataContainer::const_iterator &begin,
    const QCPGraphDataContainer::const_iterator &end) const {
  if (!lineData)
    return;
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return;
  }
  if (begin == end)
    return;

  int dataCount = end - begin;
  int maxCount = (std::numeric_limits<int>::max)();
  if (mAdaptiveSampling) {
    double keyPixelSpan = qAbs(keyAxis->coordToPixel(begin->key) -
                               keyAxis->coordToPixel((end - 1)->key));
    if (2 * keyPixelSpan + 2 <
        static_cast<double>((std::numeric_limits<int>::max)()))
      maxCount = 2 * keyPixelSpan + 2;
  }

  if (mAdaptiveSampling && dataCount >= maxCount)

  {
    QCPGraphDataContainer::const_iterator it = begin;
    double minValue = it->value;
    double maxValue = it->value;
    QCPGraphDataContainer::const_iterator currentIntervalFirstPoint = it;
    int reversedFactor = keyAxis->pixelOrientation();

    int reversedRound = reversedFactor == -1 ? 1 : 0;

    double currentIntervalStartKey = keyAxis->pixelToCoord(
        (int)(keyAxis->coordToPixel(begin->key) + reversedRound));
    double lastIntervalEndKey = currentIntervalStartKey;
    double keyEpsilon = qAbs(
        currentIntervalStartKey -
        keyAxis->pixelToCoord(keyAxis->coordToPixel(currentIntervalStartKey) +
                              1.0 * reversedFactor));

    bool keyEpsilonVariable = keyAxis->scaleType() == QCPAxis::stLogarithmic;

    int intervalDataCount = 1;
    ++it;

    while (it != end) {
      if (it->key < currentIntervalStartKey + keyEpsilon)

      {
        if (it->value < minValue)
          minValue = it->value;
        else if (it->value > maxValue)
          maxValue = it->value;
        ++intervalDataCount;
      } else

      {
        if (intervalDataCount >= 2)

        {
          if (lastIntervalEndKey < currentIntervalStartKey - keyEpsilon)

            lineData->append(
                QCPGraphData(currentIntervalStartKey + keyEpsilon * 0.2,
                             currentIntervalFirstPoint->value));
          lineData->append(QCPGraphData(
              currentIntervalStartKey + keyEpsilon * 0.25, minValue));
          lineData->append(QCPGraphData(
              currentIntervalStartKey + keyEpsilon * 0.75, maxValue));
          if (it->key > currentIntervalStartKey + keyEpsilon * 2)

            lineData->append(QCPGraphData(
                currentIntervalStartKey + keyEpsilon * 0.8, (it - 1)->value));
        } else
          lineData->append(QCPGraphData(currentIntervalFirstPoint->key,
                                        currentIntervalFirstPoint->value));
        lastIntervalEndKey = (it - 1)->key;
        minValue = it->value;
        maxValue = it->value;
        currentIntervalFirstPoint = it;
        currentIntervalStartKey = keyAxis->pixelToCoord(
            (int)(keyAxis->coordToPixel(it->key) + reversedRound));
        if (keyEpsilonVariable)
          keyEpsilon = qAbs(currentIntervalStartKey -
                            keyAxis->pixelToCoord(
                                keyAxis->coordToPixel(currentIntervalStartKey) +
                                1.0 * reversedFactor));
        intervalDataCount = 1;
      }
      ++it;
    }

    if (intervalDataCount >= 2)

    {
      if (lastIntervalEndKey < currentIntervalStartKey - keyEpsilon)

        lineData->append(
            QCPGraphData(currentIntervalStartKey + keyEpsilon * 0.2,
                         currentIntervalFirstPoint->value));
      lineData->append(
          QCPGraphData(currentIntervalStartKey + keyEpsilon * 0.25, minValue));
      lineData->append(
          QCPGraphData(currentIntervalStartKey + keyEpsilon * 0.75, maxValue));
    } else
      lineData->append(QCPGraphData(currentIntervalFirstPoint->key,
                                    currentIntervalFirstPoint->value));

  } else

  {
    lineData->resize(dataCount);
    std::copy(begin, end, lineData->begin());
  }
}

void QCPGraph::getOptimizedScatterData(
    QVector<QCPGraphData> *scatterData,
    QCPGraphDataContainer::const_iterator begin,
    QCPGraphDataContainer::const_iterator end) const {
  if (!scatterData)
    return;
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return;
  }

  const int scatterModulo = mScatterSkip + 1;
  const bool doScatterSkip = mScatterSkip > 0;
  int beginIndex = begin - mDataContainer->constBegin();
  int endIndex = end - mDataContainer->constBegin();
  while (doScatterSkip && begin != end && beginIndex % scatterModulo != 0)

  {
    ++beginIndex;
    ++begin;
  }
  if (begin == end)
    return;
  int dataCount = end - begin;
  int maxCount = (std::numeric_limits<int>::max)();
  if (mAdaptiveSampling) {
    int keyPixelSpan = qAbs(keyAxis->coordToPixel(begin->key) -
                            keyAxis->coordToPixel((end - 1)->key));
    maxCount = 2 * keyPixelSpan + 2;
  }

  if (mAdaptiveSampling && dataCount >= maxCount)

  {
    double valueMaxRange = valueAxis->range().upper;
    double valueMinRange = valueAxis->range().lower;
    QCPGraphDataContainer::const_iterator it = begin;
    int itIndex = beginIndex;
    double minValue = it->value;
    double maxValue = it->value;
    QCPGraphDataContainer::const_iterator minValueIt = it;
    QCPGraphDataContainer::const_iterator maxValueIt = it;
    QCPGraphDataContainer::const_iterator currentIntervalStart = it;
    int reversedFactor = keyAxis->pixelOrientation();

    int reversedRound = reversedFactor == -1 ? 1 : 0;

    double currentIntervalStartKey = keyAxis->pixelToCoord(
        (int)(keyAxis->coordToPixel(begin->key) + reversedRound));
    double keyEpsilon = qAbs(
        currentIntervalStartKey -
        keyAxis->pixelToCoord(keyAxis->coordToPixel(currentIntervalStartKey) +
                              1.0 * reversedFactor));

    bool keyEpsilonVariable = keyAxis->scaleType() == QCPAxis::stLogarithmic;

    int intervalDataCount = 1;

    if (!doScatterSkip)
      ++it;
    else {
      itIndex += scatterModulo;
      if (itIndex < endIndex)

        it += scatterModulo;
      else {
        it = end;
        itIndex = endIndex;
      }
    }

    while (it != end) {
      if (it->key < currentIntervalStartKey + keyEpsilon)

      {
        if (it->value < minValue && it->value > valueMinRange &&
            it->value < valueMaxRange) {
          minValue = it->value;
          minValueIt = it;
        } else if (it->value > maxValue && it->value > valueMinRange &&
                   it->value < valueMaxRange) {
          maxValue = it->value;
          maxValueIt = it;
        }
        ++intervalDataCount;
      } else

      {
        if (intervalDataCount >= 2)

        {
          double valuePixelSpan = qAbs(valueAxis->coordToPixel(minValue) -
                                       valueAxis->coordToPixel(maxValue));
          int dataModulo =
              qMax(1, qRound(intervalDataCount / (valuePixelSpan / 4.0)));

          QCPGraphDataContainer::const_iterator intervalIt =
              currentIntervalStart;
          int c = 0;
          while (intervalIt != it) {
            if ((c % dataModulo == 0 || intervalIt == minValueIt ||
                 intervalIt == maxValueIt) &&
                intervalIt->value > valueMinRange &&
                intervalIt->value < valueMaxRange)
              scatterData->append(*intervalIt);
            ++c;
            if (!doScatterSkip)
              ++intervalIt;
            else
              intervalIt += scatterModulo;
          }
        } else if (currentIntervalStart->value > valueMinRange &&
                   currentIntervalStart->value < valueMaxRange)
          scatterData->append(*currentIntervalStart);
        minValue = it->value;
        maxValue = it->value;
        currentIntervalStart = it;
        currentIntervalStartKey = keyAxis->pixelToCoord(
            (int)(keyAxis->coordToPixel(it->key) + reversedRound));
        if (keyEpsilonVariable)
          keyEpsilon = qAbs(currentIntervalStartKey -
                            keyAxis->pixelToCoord(
                                keyAxis->coordToPixel(currentIntervalStartKey) +
                                1.0 * reversedFactor));
        intervalDataCount = 1;
      }

      if (!doScatterSkip)
        ++it;
      else {
        itIndex += scatterModulo;
        if (itIndex < endIndex)

          it += scatterModulo;
        else {
          it = end;
          itIndex = endIndex;
        }
      }
    }

    if (intervalDataCount >= 2)

    {
      double valuePixelSpan = qAbs(valueAxis->coordToPixel(minValue) -
                                   valueAxis->coordToPixel(maxValue));
      int dataModulo =
          qMax(1, qRound(intervalDataCount / (valuePixelSpan / 4.0)));

      QCPGraphDataContainer::const_iterator intervalIt = currentIntervalStart;
      int intervalItIndex = intervalIt - mDataContainer->constBegin();
      int c = 0;
      while (intervalIt != it) {
        if ((c % dataModulo == 0 || intervalIt == minValueIt ||
             intervalIt == maxValueIt) &&
            intervalIt->value > valueMinRange &&
            intervalIt->value < valueMaxRange)
          scatterData->append(*intervalIt);
        ++c;
        if (!doScatterSkip)
          ++intervalIt;
        else

        {
          intervalItIndex += scatterModulo;
          if (intervalItIndex < itIndex)
            intervalIt += scatterModulo;
          else {
            intervalIt = it;
            intervalItIndex = itIndex;
          }
        }
      }
    } else if (currentIntervalStart->value > valueMinRange &&
               currentIntervalStart->value < valueMaxRange)
      scatterData->append(*currentIntervalStart);

  } else

  {
    QCPGraphDataContainer::const_iterator it = begin;
    int itIndex = beginIndex;
    scatterData->reserve(dataCount);
    while (it != end) {
      scatterData->append(*it);

      if (!doScatterSkip)
        ++it;
      else {
        itIndex += scatterModulo;
        if (itIndex < endIndex)
          it += scatterModulo;
        else {
          it = end;
          itIndex = endIndex;
        }
      }
    }
  }
}

void QCPGraph::getVisibleDataBounds(
    QCPGraphDataContainer::const_iterator &begin,
    QCPGraphDataContainer::const_iterator &end,
    const QCPDataRange &rangeRestriction) const {
  if (rangeRestriction.isEmpty()) {
    end = mDataContainer->constEnd();
    begin = end;
  } else {
    QCPAxis *keyAxis = mKeyAxis.data();
    QCPAxis *valueAxis = mValueAxis.data();
    if (!keyAxis || !valueAxis) {
      qDebug() << Q_FUNC_INFO << "invalid key or value axis";
      return;
    }

    begin = mDataContainer->findBegin(keyAxis->range().lower);
    end = mDataContainer->findEnd(keyAxis->range().upper);

    mDataContainer->limitIteratorsToDataRange(begin, end, rangeRestriction);
  }
}

QVector<QCPDataRange>
QCPGraph::getNonNanSegments(const QVector<QPointF> *lineData,
                            Qt::Orientation keyOrientation) const {
  QVector<QCPDataRange> result;
  const int n = lineData->size();

  QCPDataRange currentSegment(-1, -1);
  int i = 0;

  if (keyOrientation == Qt::Horizontal) {
    while (i < n) {
      while (i < n && qIsNaN(lineData->at(i).y()))

        ++i;
      if (i == n)
        break;
      currentSegment.setBegin(i++);
      while (i < n && !qIsNaN(lineData->at(i).y()))

        ++i;
      currentSegment.setEnd(i++);
      result.append(currentSegment);
    }
  } else

  {
    while (i < n) {
      while (i < n && qIsNaN(lineData->at(i).x()))

        ++i;
      if (i == n)
        break;
      currentSegment.setBegin(i++);
      while (i < n && !qIsNaN(lineData->at(i).x()))

        ++i;
      currentSegment.setEnd(i++);
      result.append(currentSegment);
    }
  }
  return result;
}

QVector<QPair<QCPDataRange, QCPDataRange>>
QCPGraph::getOverlappingSegments(QVector<QCPDataRange> thisSegments,
                                 const QVector<QPointF> *thisData,
                                 QVector<QCPDataRange> otherSegments,
                                 const QVector<QPointF> *otherData) const {
  QVector<QPair<QCPDataRange, QCPDataRange>> result;
  if (thisData->isEmpty() || otherData->isEmpty() || thisSegments.isEmpty() ||
      otherSegments.isEmpty())
    return result;

  int thisIndex = 0;
  int otherIndex = 0;
  const bool verticalKey = mKeyAxis->orientation() == Qt::Vertical;
  while (thisIndex < thisSegments.size() && otherIndex < otherSegments.size()) {
    if (thisSegments.at(thisIndex).size() < 2)

    {
      ++thisIndex;
      continue;
    }
    if (otherSegments.at(otherIndex).size() < 2)

    {
      ++otherIndex;
      continue;
    }
    double thisLower, thisUpper, otherLower, otherUpper;
    if (!verticalKey) {
      thisLower = thisData->at(thisSegments.at(thisIndex).begin()).x();
      thisUpper = thisData->at(thisSegments.at(thisIndex).end() - 1).x();
      otherLower = otherData->at(otherSegments.at(otherIndex).begin()).x();
      otherUpper = otherData->at(otherSegments.at(otherIndex).end() - 1).x();
    } else {
      thisLower = thisData->at(thisSegments.at(thisIndex).begin()).y();
      thisUpper = thisData->at(thisSegments.at(thisIndex).end() - 1).y();
      otherLower = otherData->at(otherSegments.at(otherIndex).begin()).y();
      otherUpper = otherData->at(otherSegments.at(otherIndex).end() - 1).y();
    }

    int bPrecedence;
    if (segmentsIntersect(thisLower, thisUpper, otherLower, otherUpper,
                          bPrecedence))
      result.append(QPair<QCPDataRange, QCPDataRange>(
          thisSegments.at(thisIndex), otherSegments.at(otherIndex)));

    if (bPrecedence <= 0)

      ++otherIndex;
    else

      ++thisIndex;
  }

  return result;
}

bool QCPGraph::segmentsIntersect(double aLower, double aUpper, double bLower,
                                 double bUpper, int &bPrecedence) const {
  bPrecedence = 0;
  if (aLower > bUpper) {
    bPrecedence = -1;
    return false;
  } else if (bLower > aUpper) {
    bPrecedence = 1;
    return false;
  } else {
    if (aUpper > bUpper)
      bPrecedence = -1;
    else if (aUpper < bUpper)
      bPrecedence = 1;

    return true;
  }
}

QPointF QCPGraph::getFillBasePoint(QPointF matchingDataPoint) const {
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return QPointF();
  }

  QPointF result;
  if (valueAxis->scaleType() == QCPAxis::stLinear) {
    if (keyAxis->orientation() == Qt::Horizontal) {
      result.setX(matchingDataPoint.x());
      result.setY(valueAxis->coordToPixel(0));
    } else

    {
      result.setX(valueAxis->coordToPixel(0));
      result.setY(matchingDataPoint.y());
    }
  } else

  {
    if (keyAxis->orientation() == Qt::Vertical) {
      if ((valueAxis->range().upper < 0 && !valueAxis->rangeReversed()) ||
          (valueAxis->range().upper > 0 && valueAxis->rangeReversed()))

        result.setX(keyAxis->axisRect()->right());
      else
        result.setX(keyAxis->axisRect()->left());
      result.setY(matchingDataPoint.y());
    } else if (keyAxis->axisType() == QCPAxis::atTop ||
               keyAxis->axisType() == QCPAxis::atBottom) {
      result.setX(matchingDataPoint.x());
      if ((valueAxis->range().upper < 0 && !valueAxis->rangeReversed()) ||
          (valueAxis->range().upper > 0 && valueAxis->rangeReversed()))

        result.setY(keyAxis->axisRect()->top());
      else
        result.setY(keyAxis->axisRect()->bottom());
    }
  }
  return result;
}

const QPolygonF QCPGraph::getFillPolygon(const QVector<QPointF> *lineData,
                                         QCPDataRange segment) const {
  if (segment.size() < 2)
    return QPolygonF();
  QPolygonF result(segment.size() + 2);

  result[0] = getFillBasePoint(lineData->at(segment.begin()));
  std::copy(lineData->constBegin() + segment.begin(),
            lineData->constBegin() + segment.end(), result.begin() + 1);
  result[result.size() - 1] = getFillBasePoint(lineData->at(segment.end() - 1));

  return result;
}

const QPolygonF QCPGraph::getChannelFillPolygon(
    const QVector<QPointF> *thisData, QCPDataRange thisSegment,
    const QVector<QPointF> *otherData, QCPDataRange otherSegment) const {
  if (!mChannelFillGraph)
    return QPolygonF();

  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return QPolygonF();
  }
  if (!mChannelFillGraph.data()->mKeyAxis) {
    qDebug() << Q_FUNC_INFO << "channel fill target key axis invalid";
    return QPolygonF();
  }

  if (mChannelFillGraph.data()->mKeyAxis.data()->orientation() !=
      keyAxis->orientation())
    return QPolygonF();

  if (thisData->isEmpty())
    return QPolygonF();
  QVector<QPointF> thisSegmentData(thisSegment.size());
  QVector<QPointF> otherSegmentData(otherSegment.size());
  std::copy(thisData->constBegin() + thisSegment.begin(),
            thisData->constBegin() + thisSegment.end(),
            thisSegmentData.begin());
  std::copy(otherData->constBegin() + otherSegment.begin(),
            otherData->constBegin() + otherSegment.end(),
            otherSegmentData.begin());

  QVector<QPointF> *staticData = &thisSegmentData;
  QVector<QPointF> *croppedData = &otherSegmentData;

  if (keyAxis->orientation() == Qt::Horizontal) {
    if (staticData->first().x() < croppedData->first().x())

      qSwap(staticData, croppedData);
    const int lowBound = findIndexBelowX(croppedData, staticData->first().x());
    if (lowBound == -1)
      return QPolygonF();

    croppedData->remove(0, lowBound);

    if (croppedData->size() < 2)
      return QPolygonF();

    double slope;
    if (!qFuzzyCompare(croppedData->at(1).x(), croppedData->at(0).x()))
      slope = (croppedData->at(1).y() - croppedData->at(0).y()) /
              (croppedData->at(1).x() - croppedData->at(0).x());
    else
      slope = 0;
    (*croppedData)[0].setY(
        croppedData->at(0).y() +
        slope * (staticData->first().x() - croppedData->at(0).x()));
    (*croppedData)[0].setX(staticData->first().x());

    if (staticData->last().x() > croppedData->last().x())

      qSwap(staticData, croppedData);
    int highBound = findIndexAboveX(croppedData, staticData->last().x());
    if (highBound == -1)
      return QPolygonF();

    croppedData->remove(highBound + 1, croppedData->size() - (highBound + 1));

    if (croppedData->size() < 2)
      return QPolygonF();

    const int li = croppedData->size() - 1;

    if (!qFuzzyCompare(croppedData->at(li).x(), croppedData->at(li - 1).x()))
      slope = (croppedData->at(li).y() - croppedData->at(li - 1).y()) /
              (croppedData->at(li).x() - croppedData->at(li - 1).x());
    else
      slope = 0;
    (*croppedData)[li].setY(
        croppedData->at(li - 1).y() +
        slope * (staticData->last().x() - croppedData->at(li - 1).x()));
    (*croppedData)[li].setX(staticData->last().x());
  } else

  {
    if (staticData->first().y() < croppedData->first().y())

      qSwap(staticData, croppedData);
    int lowBound = findIndexBelowY(croppedData, staticData->first().y());
    if (lowBound == -1)
      return QPolygonF();

    croppedData->remove(0, lowBound);

    if (croppedData->size() < 2)
      return QPolygonF();

    double slope;
    if (!qFuzzyCompare(croppedData->at(1).y(), croppedData->at(0).y()))

      slope = (croppedData->at(1).x() - croppedData->at(0).x()) /
              (croppedData->at(1).y() - croppedData->at(0).y());
    else
      slope = 0;
    (*croppedData)[0].setX(
        croppedData->at(0).x() +
        slope * (staticData->first().y() - croppedData->at(0).y()));
    (*croppedData)[0].setY(staticData->first().y());

    if (staticData->last().y() > croppedData->last().y())

      qSwap(staticData, croppedData);
    int highBound = findIndexAboveY(croppedData, staticData->last().y());
    if (highBound == -1)
      return QPolygonF();

    croppedData->remove(highBound + 1, croppedData->size() - (highBound + 1));

    if (croppedData->size() < 2)
      return QPolygonF();

    int li = croppedData->size() - 1;

    if (!qFuzzyCompare(croppedData->at(li).y(), croppedData->at(li - 1).y()))

      slope = (croppedData->at(li).x() - croppedData->at(li - 1).x()) /
              (croppedData->at(li).y() - croppedData->at(li - 1).y());
    else
      slope = 0;
    (*croppedData)[li].setX(
        croppedData->at(li - 1).x() +
        slope * (staticData->last().y() - croppedData->at(li - 1).y()));
    (*croppedData)[li].setY(staticData->last().y());
  }

  for (int i = otherSegmentData.size() - 1; i >= 0; --i)

    thisSegmentData << otherSegmentData.at(i);
  return QPolygonF(thisSegmentData);
}

int QCPGraph::findIndexAboveX(const QVector<QPointF> *data, double x) const {
  for (int i = data->size() - 1; i >= 0; --i) {
    if (data->at(i).x() < x) {
      if (i < data->size() - 1)
        return i + 1;
      else
        return data->size() - 1;
    }
  }
  return -1;
}

int QCPGraph::findIndexBelowX(const QVector<QPointF> *data, double x) const {
  for (int i = 0; i < data->size(); ++i) {
    if (data->at(i).x() > x) {
      if (i > 0)
        return i - 1;
      else
        return 0;
    }
  }
  return -1;
}

int QCPGraph::findIndexAboveY(const QVector<QPointF> *data, double y) const {
  for (int i = data->size() - 1; i >= 0; --i) {
    if (data->at(i).y() < y) {
      if (i < data->size() - 1)
        return i + 1;
      else
        return data->size() - 1;
    }
  }
  return -1;
}

double QCPGraph::pointDistance(
    const QPointF &pixelPoint,
    QCPGraphDataContainer::const_iterator &closestData) const {
  closestData = mDataContainer->constEnd();
  if (mDataContainer->isEmpty())
    return -1.0;
  if (mLineStyle == lsNone && mScatterStyle.isNone())
    return -1.0;

  double minDistSqr = (std::numeric_limits<double>::max)();

  double posKeyMin, posKeyMax, dummy;
  pixelsToCoords(pixelPoint - QPointF(mParentPlot->selectionTolerance(),
                                      mParentPlot->selectionTolerance()),
                 posKeyMin, dummy);
  pixelsToCoords(pixelPoint + QPointF(mParentPlot->selectionTolerance(),
                                      mParentPlot->selectionTolerance()),
                 posKeyMax, dummy);
  if (posKeyMin > posKeyMax)
    qSwap(posKeyMin, posKeyMax);

  QCPGraphDataContainer::const_iterator begin =
      mDataContainer->findBegin(posKeyMin, true);
  QCPGraphDataContainer::const_iterator end =
      mDataContainer->findEnd(posKeyMax, true);
  for (QCPGraphDataContainer::const_iterator it = begin; it != end; ++it) {
    const double currentDistSqr =
        QCPVector2D(coordsToPixels(it->key, it->value) - pixelPoint)
            .lengthSquared();
    if (currentDistSqr < minDistSqr) {
      minDistSqr = currentDistSqr;
      closestData = it;
    }
  }

  if (mLineStyle != lsNone) {
    QVector<QPointF> lineData;
    getLines(&lineData, QCPDataRange(0, dataCount()));
    QCPVector2D p(pixelPoint);
    const int step = mLineStyle == lsImpulse ? 2 : 1;

    for (int i = 0; i < lineData.size() - 1; i += step) {
      const double currentDistSqr =
          p.distanceSquaredToLine(lineData.at(i), lineData.at(i + 1));
      if (currentDistSqr < minDistSqr)
        minDistSqr = currentDistSqr;
    }
  }

  return qSqrt(minDistSqr);
}

int QCPGraph::findIndexBelowY(const QVector<QPointF> *data, double y) const {
  for (int i = 0; i < data->size(); ++i) {
    if (data->at(i).y() > y) {
      if (i > 0)
        return i - 1;
      else
        return 0;
    }
  }
  return -1;
}

QCPCurveData::QCPCurveData() : t(0), key(0), value(0) {}

QCPCurveData::QCPCurveData(double t, double key, double value)
    : t(t), key(key), value(value) {}

QCPCurve::QCPCurve(QCPAxis *keyAxis, QCPAxis *valueAxis)
    : QCPAbstractPlottable1D<QCPCurveData>(keyAxis, valueAxis) {
  setPen(QPen(Qt::blue, 0));
  setBrush(Qt::NoBrush);

  setScatterStyle(QCPScatterStyle());
  setLineStyle(lsLine);
  setScatterSkip(0);
}

QCPCurve::~QCPCurve() {}

void QCPCurve::setData(QSharedPointer<QCPCurveDataContainer> data) {
  mDataContainer = data;
}

void QCPCurve::setData(const QVector<double> &t, const QVector<double> &keys,
                       const QVector<double> &values, bool alreadySorted) {
  mDataContainer->clear();
  addData(t, keys, values, alreadySorted);
}

void QCPCurve::setData(const QVector<double> &keys,
                       const QVector<double> &values) {
  mDataContainer->clear();
  addData(keys, values);
}

void QCPCurve::setScatterStyle(const QCPScatterStyle &style) {
  mScatterStyle = style;
}

void QCPCurve::setScatterSkip(int skip) { mScatterSkip = qMax(0, skip); }

void QCPCurve::setLineStyle(QCPCurve::LineStyle style) { mLineStyle = style; }

void QCPCurve::addData(const QVector<double> &t, const QVector<double> &keys,
                       const QVector<double> &values, bool alreadySorted) {
  if (t.size() != keys.size() || t.size() != values.size())
    qDebug() << Q_FUNC_INFO
             << "ts, keys and values have different sizes:" << t.size()
             << keys.size() << values.size();
  const int n = qMin(qMin(t.size(), keys.size()), values.size());
  QVector<QCPCurveData> tempData(n);
  QVector<QCPCurveData>::iterator it = tempData.begin();
  const QVector<QCPCurveData>::iterator itEnd = tempData.end();
  int i = 0;
  while (it != itEnd) {
    it->t = t[i];
    it->key = keys[i];
    it->value = values[i];
    ++it;
    ++i;
  }
  mDataContainer->add(tempData, alreadySorted);
}

void QCPCurve::addData(const QVector<double> &keys,
                       const QVector<double> &values) {
  if (keys.size() != values.size())
    qDebug() << Q_FUNC_INFO
             << "keys and values have different sizes:" << keys.size()
             << values.size();
  const int n = qMin(keys.size(), values.size());
  double tStart;
  if (!mDataContainer->isEmpty())
    tStart = (mDataContainer->constEnd() - 1)->t + 1.0;
  else
    tStart = 0;
  QVector<QCPCurveData> tempData(n);
  QVector<QCPCurveData>::iterator it = tempData.begin();
  const QVector<QCPCurveData>::iterator itEnd = tempData.end();
  int i = 0;
  while (it != itEnd) {
    it->t = tStart + i;
    it->key = keys[i];
    it->value = values[i];
    ++it;
    ++i;
  }
  mDataContainer->add(tempData, true);
}

void QCPCurve::addData(double t, double key, double value) {
  mDataContainer->add(QCPCurveData(t, key, value));
}

void QCPCurve::addData(double key, double value) {
  if (!mDataContainer->isEmpty())
    mDataContainer->add(
        QCPCurveData((mDataContainer->constEnd() - 1)->t + 1.0, key, value));
  else
    mDataContainer->add(QCPCurveData(0.0, key, value));
}

double QCPCurve::selectTest(const QPointF &pos, bool onlySelectable,
                            QVariant *details) const {
  if ((onlySelectable && mSelectable == QCP::stNone) ||
      mDataContainer->isEmpty())
    return -1;
  if (!mKeyAxis || !mValueAxis)
    return -1;

  if (mKeyAxis.data()->axisRect()->rect().contains(pos.toPoint())) {
    QCPCurveDataContainer::const_iterator closestDataPoint =
        mDataContainer->constEnd();
    double result = pointDistance(pos, closestDataPoint);
    if (details) {
      int pointIndex = closestDataPoint - mDataContainer->constBegin();
      details->setValue(
          QCPDataSelection(QCPDataRange(pointIndex, pointIndex + 1)));
    }
    return result;
  } else
    return -1;
}

QCPRange QCPCurve::getKeyRange(bool &foundRange,
                               QCP::SignDomain inSignDomain) const {
  return mDataContainer->keyRange(foundRange, inSignDomain);
}

QCPRange QCPCurve::getValueRange(bool &foundRange, QCP::SignDomain inSignDomain,
                                 const QCPRange &inKeyRange) const {
  return mDataContainer->valueRange(foundRange, inSignDomain, inKeyRange);
}

void QCPCurve::draw(QCPPainter *painter) {
  if (mDataContainer->isEmpty())
    return;

  QVector<QPointF> lines, scatters;

  QList<QCPDataRange> selectedSegments, unselectedSegments, allSegments;
  getDataSegments(selectedSegments, unselectedSegments);
  allSegments << unselectedSegments << selectedSegments;
  for (int i = 0; i < allSegments.size(); ++i) {
    bool isSelectedSegment = i >= unselectedSegments.size();

    QPen finalCurvePen = mPen;

    if (isSelectedSegment && mSelectionDecorator)
      finalCurvePen = mSelectionDecorator->pen();

    QCPDataRange lineDataRange = isSelectedSegment
                                     ? allSegments.at(i)
                                     : allSegments.at(i).adjusted(-1, 1);

    getCurveLines(&lines, lineDataRange, finalCurvePen.widthF());

#ifdef QCUSTOMPLOT_CHECK_DATA
    for (QCPCurveDataContainer::const_iterator it =
             mDataContainer->constBegin();
         it != mDataContainer->constEnd(); ++it) {
      if (QCP::isInvalidData(it->t) || QCP::isInvalidData(it->key, it->value))
        qDebug() << Q_FUNC_INFO << "Data point at" << it->key << "invalid."
                 << "Plottable name:" << name();
    }
#endif

    applyFillAntialiasingHint(painter);
    if (isSelectedSegment && mSelectionDecorator)
      mSelectionDecorator->applyBrush(painter);
    else
      painter->setBrush(mBrush);
    painter->setPen(Qt::NoPen);
    if (painter->brush().style() != Qt::NoBrush &&
        painter->brush().color().alpha() != 0)
      painter->drawPolygon(QPolygonF(lines));

    if (mLineStyle != lsNone) {
      painter->setPen(finalCurvePen);
      painter->setBrush(Qt::NoBrush);
      drawCurveLine(painter, lines);
    }

    QCPScatterStyle finalScatterStyle = mScatterStyle;
    if (isSelectedSegment && mSelectionDecorator)
      finalScatterStyle =
          mSelectionDecorator->getFinalScatterStyle(mScatterStyle);
    if (!finalScatterStyle.isNone()) {
      getScatters(&scatters, allSegments.at(i), finalScatterStyle.size());
      drawScatterPlot(painter, scatters, finalScatterStyle);
    }
  }

  if (mSelectionDecorator)
    mSelectionDecorator->drawDecoration(painter, selection());
}

void QCPCurve::drawLegendIcon(QCPPainter *painter, const QRectF &rect) const {
  if (mBrush.style() != Qt::NoBrush) {
    applyFillAntialiasingHint(painter);
    painter->fillRect(QRectF(rect.left(), rect.top() + rect.height() / 2.0,
                             rect.width(), rect.height() / 3.0),
                      mBrush);
  }

  if (mLineStyle != lsNone) {
    applyDefaultAntialiasingHint(painter);
    painter->setPen(mPen);
    painter->drawLine(QLineF(rect.left(), rect.top() + rect.height() / 2.0,
                             rect.right() + 5,
                             rect.top() + rect.height() / 2.0));
  }

  if (!mScatterStyle.isNone()) {
    applyScattersAntialiasingHint(painter);

    if (mScatterStyle.shape() == QCPScatterStyle::ssPixmap &&
        (mScatterStyle.pixmap().size().width() > rect.width() ||
         mScatterStyle.pixmap().size().height() > rect.height())) {
      QCPScatterStyle scaledStyle(mScatterStyle);
      scaledStyle.setPixmap(scaledStyle.pixmap().scaled(
          rect.size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
      scaledStyle.applyTo(painter, mPen);
      scaledStyle.drawShape(painter, QRectF(rect).center());
    } else {
      mScatterStyle.applyTo(painter, mPen);
      mScatterStyle.drawShape(painter, QRectF(rect).center());
    }
  }
}

void QCPCurve::drawCurveLine(QCPPainter *painter,
                             const QVector<QPointF> &lines) const {
  if (painter->pen().style() != Qt::NoPen &&
      painter->pen().color().alpha() != 0) {
    applyDefaultAntialiasingHint(painter);
    drawPolyline(painter, lines);
  }
}

void QCPCurve::drawScatterPlot(QCPPainter *painter,
                               const QVector<QPointF> &points,
                               const QCPScatterStyle &style) const {
  applyScattersAntialiasingHint(painter);
  style.applyTo(painter, mPen);
  for (int i = 0; i < points.size(); ++i)
    if (!qIsNaN(points.at(i).x()) && !qIsNaN(points.at(i).y()))
      style.drawShape(painter, points.at(i));
}

void QCPCurve::getCurveLines(QVector<QPointF> *lines,
                             const QCPDataRange &dataRange,
                             double penWidth) const {
  if (!lines)
    return;
  lines->clear();
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return;
  }

  const double strokeMargin = qMax(qreal(1.0), qreal(penWidth * 0.75));

  const double keyMin =
      keyAxis->pixelToCoord(keyAxis->coordToPixel(keyAxis->range().lower) -
                            strokeMargin * keyAxis->pixelOrientation());
  const double keyMax =
      keyAxis->pixelToCoord(keyAxis->coordToPixel(keyAxis->range().upper) +
                            strokeMargin * keyAxis->pixelOrientation());
  const double valueMin = valueAxis->pixelToCoord(
      valueAxis->coordToPixel(valueAxis->range().lower) -
      strokeMargin * valueAxis->pixelOrientation());
  const double valueMax = valueAxis->pixelToCoord(
      valueAxis->coordToPixel(valueAxis->range().upper) +
      strokeMargin * valueAxis->pixelOrientation());
  QCPCurveDataContainer::const_iterator itBegin = mDataContainer->constBegin();
  QCPCurveDataContainer::const_iterator itEnd = mDataContainer->constEnd();
  mDataContainer->limitIteratorsToDataRange(itBegin, itEnd, dataRange);
  if (itBegin == itEnd)
    return;
  QCPCurveDataContainer::const_iterator it = itBegin;
  QCPCurveDataContainer::const_iterator prevIt = itEnd - 1;
  int prevRegion =
      getRegion(prevIt->key, prevIt->value, keyMin, valueMax, keyMax, valueMin);
  QVector<QPointF> trailingPoints;

  while (it != itEnd) {
    const int currentRegion =
        getRegion(it->key, it->value, keyMin, valueMax, keyMax, valueMin);
    if (currentRegion != prevRegion)

    {
      if (currentRegion != 5)

      {
        QPointF crossA, crossB;
        if (prevRegion == 5)

        {
          lines->append(getOptimizedPoint(currentRegion, it->key, it->value,
                                          prevIt->key, prevIt->value, keyMin,
                                          valueMax, keyMax, valueMin));

          *lines << getOptimizedCornerPoints(
              prevRegion, currentRegion, prevIt->key, prevIt->value, it->key,
              it->value, keyMin, valueMax, keyMax, valueMin);
        } else if (mayTraverse(prevRegion, currentRegion) &&
                   getTraverse(prevIt->key, prevIt->value, it->key, it->value,
                               keyMin, valueMax, keyMax, valueMin, crossA,
                               crossB)) {
          QVector<QPointF> beforeTraverseCornerPoints,
              afterTraverseCornerPoints;
          getTraverseCornerPoints(prevRegion, currentRegion, keyMin, valueMax,
                                  keyMax, valueMin, beforeTraverseCornerPoints,
                                  afterTraverseCornerPoints);
          if (it != itBegin) {
            *lines << beforeTraverseCornerPoints;
            lines->append(crossA);
            lines->append(crossB);
            *lines << afterTraverseCornerPoints;
          } else {
            lines->append(crossB);
            *lines << afterTraverseCornerPoints;
            trailingPoints << beforeTraverseCornerPoints << crossA;
          }
        } else

        {
          *lines << getOptimizedCornerPoints(
              prevRegion, currentRegion, prevIt->key, prevIt->value, it->key,
              it->value, keyMin, valueMax, keyMax, valueMin);
        }
      } else

      {
        if (it == itBegin)

          trailingPoints << getOptimizedPoint(
              prevRegion, prevIt->key, prevIt->value, it->key, it->value,
              keyMin, valueMax, keyMax, valueMin);
        else
          lines->append(getOptimizedPoint(prevRegion, prevIt->key,
                                          prevIt->value, it->key, it->value,
                                          keyMin, valueMax, keyMax, valueMin));
        lines->append(coordsToPixels(it->key, it->value));
      }
    } else

    {
      if (currentRegion == 5)

      {
        lines->append(coordsToPixels(it->key, it->value));
      } else

      {
      }
    }
    prevIt = it;
    prevRegion = currentRegion;
    ++it;
  }
  *lines << trailingPoints;
}

void QCPCurve::getScatters(QVector<QPointF> *scatters,
                           const QCPDataRange &dataRange,
                           double scatterWidth) const {
  if (!scatters)
    return;
  scatters->clear();
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return;
  }

  QCPCurveDataContainer::const_iterator begin = mDataContainer->constBegin();
  QCPCurveDataContainer::const_iterator end = mDataContainer->constEnd();
  mDataContainer->limitIteratorsToDataRange(begin, end, dataRange);
  if (begin == end)
    return;
  const int scatterModulo = mScatterSkip + 1;
  const bool doScatterSkip = mScatterSkip > 0;
  int endIndex = end - mDataContainer->constBegin();

  QCPRange keyRange = keyAxis->range();
  QCPRange valueRange = valueAxis->range();

  keyRange.lower =
      keyAxis->pixelToCoord(keyAxis->coordToPixel(keyRange.lower) -
                            scatterWidth * keyAxis->pixelOrientation());
  keyRange.upper =
      keyAxis->pixelToCoord(keyAxis->coordToPixel(keyRange.upper) +
                            scatterWidth * keyAxis->pixelOrientation());
  valueRange.lower =
      valueAxis->pixelToCoord(valueAxis->coordToPixel(valueRange.lower) -
                              scatterWidth * valueAxis->pixelOrientation());
  valueRange.upper =
      valueAxis->pixelToCoord(valueAxis->coordToPixel(valueRange.upper) +
                              scatterWidth * valueAxis->pixelOrientation());

  QCPCurveDataContainer::const_iterator it = begin;
  int itIndex = begin - mDataContainer->constBegin();
  while (doScatterSkip && it != end && itIndex % scatterModulo != 0)

  {
    ++itIndex;
    ++it;
  }
  if (keyAxis->orientation() == Qt::Vertical) {
    while (it != end) {
      if (!qIsNaN(it->value) && keyRange.contains(it->key) &&
          valueRange.contains(it->value))
        scatters->append(QPointF(valueAxis->coordToPixel(it->value),
                                 keyAxis->coordToPixel(it->key)));

      if (!doScatterSkip)
        ++it;
      else {
        itIndex += scatterModulo;
        if (itIndex < endIndex)

          it += scatterModulo;
        else {
          it = end;
          itIndex = endIndex;
        }
      }
    }
  } else {
    while (it != end) {
      if (!qIsNaN(it->value) && keyRange.contains(it->key) &&
          valueRange.contains(it->value))
        scatters->append(QPointF(keyAxis->coordToPixel(it->key),
                                 valueAxis->coordToPixel(it->value)));

      if (!doScatterSkip)
        ++it;
      else {
        itIndex += scatterModulo;
        if (itIndex < endIndex)

          it += scatterModulo;
        else {
          it = end;
          itIndex = endIndex;
        }
      }
    }
  }
}

int QCPCurve::getRegion(double key, double value, double keyMin,
                        double valueMax, double keyMax, double valueMin) const {
  if (key < keyMin)

  {
    if (value > valueMax)
      return 1;
    else if (value < valueMin)
      return 3;
    else
      return 2;
  } else if (key > keyMax)

  {
    if (value > valueMax)
      return 7;
    else if (value < valueMin)
      return 9;
    else
      return 8;
  } else

  {
    if (value > valueMax)
      return 4;
    else if (value < valueMin)
      return 6;
    else
      return 5;
  }
}

QPointF QCPCurve::getOptimizedPoint(int otherRegion, double otherKey,
                                    double otherValue, double key, double value,
                                    double keyMin, double valueMax,
                                    double keyMax, double valueMin) const {
  const double keyMinPx = mKeyAxis->coordToPixel(keyMin);
  const double keyMaxPx = mKeyAxis->coordToPixel(keyMax);
  const double valueMinPx = mValueAxis->coordToPixel(valueMin);
  const double valueMaxPx = mValueAxis->coordToPixel(valueMax);
  const double otherValuePx = mValueAxis->coordToPixel(otherValue);
  const double valuePx = mValueAxis->coordToPixel(value);
  const double otherKeyPx = mKeyAxis->coordToPixel(otherKey);
  const double keyPx = mKeyAxis->coordToPixel(key);
  double intersectKeyPx = keyMinPx;

  double intersectValuePx = valueMinPx;

  switch (otherRegion) {
  case 1:

  {
    intersectValuePx = valueMaxPx;
    intersectKeyPx = otherKeyPx + (keyPx - otherKeyPx) /
                                      (valuePx - otherValuePx) *
                                      (intersectValuePx - otherValuePx);
    if (intersectKeyPx < qMin(keyMinPx, keyMaxPx) ||
        intersectKeyPx > qMax(keyMinPx, keyMaxPx))

    {
      intersectKeyPx = keyMinPx;
      intersectValuePx = otherValuePx + (valuePx - otherValuePx) /
                                            (keyPx - otherKeyPx) *
                                            (intersectKeyPx - otherKeyPx);
    }
    break;
  }
  case 2:

  {
    intersectKeyPx = keyMinPx;
    intersectValuePx = otherValuePx + (valuePx - otherValuePx) /
                                          (keyPx - otherKeyPx) *
                                          (intersectKeyPx - otherKeyPx);
    break;
  }
  case 3:

  {
    intersectValuePx = valueMinPx;
    intersectKeyPx = otherKeyPx + (keyPx - otherKeyPx) /
                                      (valuePx - otherValuePx) *
                                      (intersectValuePx - otherValuePx);
    if (intersectKeyPx < qMin(keyMinPx, keyMaxPx) ||
        intersectKeyPx > qMax(keyMinPx, keyMaxPx))

    {
      intersectKeyPx = keyMinPx;
      intersectValuePx = otherValuePx + (valuePx - otherValuePx) /
                                            (keyPx - otherKeyPx) *
                                            (intersectKeyPx - otherKeyPx);
    }
    break;
  }
  case 4:

  {
    intersectValuePx = valueMaxPx;
    intersectKeyPx = otherKeyPx + (keyPx - otherKeyPx) /
                                      (valuePx - otherValuePx) *
                                      (intersectValuePx - otherValuePx);
    break;
  }
  case 5: {
    break;
  }
  case 6:

  {
    intersectValuePx = valueMinPx;
    intersectKeyPx = otherKeyPx + (keyPx - otherKeyPx) /
                                      (valuePx - otherValuePx) *
                                      (intersectValuePx - otherValuePx);
    break;
  }
  case 7:

  {
    intersectValuePx = valueMaxPx;
    intersectKeyPx = otherKeyPx + (keyPx - otherKeyPx) /
                                      (valuePx - otherValuePx) *
                                      (intersectValuePx - otherValuePx);
    if (intersectKeyPx < qMin(keyMinPx, keyMaxPx) ||
        intersectKeyPx > qMax(keyMinPx, keyMaxPx))

    {
      intersectKeyPx = keyMaxPx;
      intersectValuePx = otherValuePx + (valuePx - otherValuePx) /
                                            (keyPx - otherKeyPx) *
                                            (intersectKeyPx - otherKeyPx);
    }
    break;
  }
  case 8:

  {
    intersectKeyPx = keyMaxPx;
    intersectValuePx = otherValuePx + (valuePx - otherValuePx) /
                                          (keyPx - otherKeyPx) *
                                          (intersectKeyPx - otherKeyPx);
    break;
  }
  case 9:

  {
    intersectValuePx = valueMinPx;
    intersectKeyPx = otherKeyPx + (keyPx - otherKeyPx) /
                                      (valuePx - otherValuePx) *
                                      (intersectValuePx - otherValuePx);
    if (intersectKeyPx < qMin(keyMinPx, keyMaxPx) ||
        intersectKeyPx > qMax(keyMinPx, keyMaxPx))

    {
      intersectKeyPx = keyMaxPx;
      intersectValuePx = otherValuePx + (valuePx - otherValuePx) /
                                            (keyPx - otherKeyPx) *
                                            (intersectKeyPx - otherKeyPx);
    }
    break;
  }
  }
  if (mKeyAxis->orientation() == Qt::Horizontal)
    return QPointF(intersectKeyPx, intersectValuePx);
  else
    return QPointF(intersectValuePx, intersectKeyPx);
}

QVector<QPointF>
QCPCurve::getOptimizedCornerPoints(int prevRegion, int currentRegion,
                                   double prevKey, double prevValue, double key,
                                   double value, double keyMin, double valueMax,
                                   double keyMax, double valueMin) const {
  QVector<QPointF> result;
  switch (prevRegion) {
  case 1: {
    switch (currentRegion) {
    case 2: {
      result << coordsToPixels(keyMin, valueMax);
      break;
    }
    case 4: {
      result << coordsToPixels(keyMin, valueMax);
      break;
    }
    case 3: {
      result << coordsToPixels(keyMin, valueMax)
             << coordsToPixels(keyMin, valueMin);
      break;
    }
    case 7: {
      result << coordsToPixels(keyMin, valueMax)
             << coordsToPixels(keyMax, valueMax);
      break;
    }
    case 6: {
      result << coordsToPixels(keyMin, valueMax)
             << coordsToPixels(keyMin, valueMin);
      result.append(result.last());
      break;
    }
    case 8: {
      result << coordsToPixels(keyMin, valueMax)
             << coordsToPixels(keyMax, valueMax);
      result.append(result.last());
      break;
    }
    case 9: {
      if ((value - prevValue) / (key - prevKey) * (keyMin - key) + value <
          valueMin)

      {
        result << coordsToPixels(keyMin, valueMax)
               << coordsToPixels(keyMin, valueMin);
        result.append(result.last());
        result << coordsToPixels(keyMax, valueMin);
      } else {
        result << coordsToPixels(keyMin, valueMax)
               << coordsToPixels(keyMax, valueMax);
        result.append(result.last());
        result << coordsToPixels(keyMax, valueMin);
      }
      break;
    }
    }
    break;
  }
  case 2: {
    switch (currentRegion) {
    case 1: {
      result << coordsToPixels(keyMin, valueMax);
      break;
    }
    case 3: {
      result << coordsToPixels(keyMin, valueMin);
      break;
    }
    case 4: {
      result << coordsToPixels(keyMin, valueMax);
      result.append(result.last());
      break;
    }
    case 6: {
      result << coordsToPixels(keyMin, valueMin);
      result.append(result.last());
      break;
    }
    case 7: {
      result << coordsToPixels(keyMin, valueMax);
      result.append(result.last());
      result << coordsToPixels(keyMax, valueMax);
      break;
    }
    case 9: {
      result << coordsToPixels(keyMin, valueMin);
      result.append(result.last());
      result << coordsToPixels(keyMax, valueMin);
      break;
    }
    }
    break;
  }
  case 3: {
    switch (currentRegion) {
    case 2: {
      result << coordsToPixels(keyMin, valueMin);
      break;
    }
    case 6: {
      result << coordsToPixels(keyMin, valueMin);
      break;
    }
    case 1: {
      result << coordsToPixels(keyMin, valueMin)
             << coordsToPixels(keyMin, valueMax);
      break;
    }
    case 9: {
      result << coordsToPixels(keyMin, valueMin)
             << coordsToPixels(keyMax, valueMin);
      break;
    }
    case 4: {
      result << coordsToPixels(keyMin, valueMin)
             << coordsToPixels(keyMin, valueMax);
      result.append(result.last());
      break;
    }
    case 8: {
      result << coordsToPixels(keyMin, valueMin)
             << coordsToPixels(keyMax, valueMin);
      result.append(result.last());
      break;
    }
    case 7: {
      if ((value - prevValue) / (key - prevKey) * (keyMax - key) + value <
          valueMin)

      {
        result << coordsToPixels(keyMin, valueMin)
               << coordsToPixels(keyMax, valueMin);
        result.append(result.last());
        result << coordsToPixels(keyMax, valueMax);
      } else {
        result << coordsToPixels(keyMin, valueMin)
               << coordsToPixels(keyMin, valueMax);
        result.append(result.last());
        result << coordsToPixels(keyMax, valueMax);
      }
      break;
    }
    }
    break;
  }
  case 4: {
    switch (currentRegion) {
    case 1: {
      result << coordsToPixels(keyMin, valueMax);
      break;
    }
    case 7: {
      result << coordsToPixels(keyMax, valueMax);
      break;
    }
    case 2: {
      result << coordsToPixels(keyMin, valueMax);
      result.append(result.last());
      break;
    }
    case 8: {
      result << coordsToPixels(keyMax, valueMax);
      result.append(result.last());
      break;
    }
    case 3: {
      result << coordsToPixels(keyMin, valueMax);
      result.append(result.last());
      result << coordsToPixels(keyMin, valueMin);
      break;
    }
    case 9: {
      result << coordsToPixels(keyMax, valueMax);
      result.append(result.last());
      result << coordsToPixels(keyMax, valueMin);
      break;
    }
    }
    break;
  }
  case 5: {
    switch (currentRegion) {
    case 1: {
      result << coordsToPixels(keyMin, valueMax);
      break;
    }
    case 7: {
      result << coordsToPixels(keyMax, valueMax);
      break;
    }
    case 9: {
      result << coordsToPixels(keyMax, valueMin);
      break;
    }
    case 3: {
      result << coordsToPixels(keyMin, valueMin);
      break;
    }
    }
    break;
  }
  case 6: {
    switch (currentRegion) {
    case 3: {
      result << coordsToPixels(keyMin, valueMin);
      break;
    }
    case 9: {
      result << coordsToPixels(keyMax, valueMin);
      break;
    }
    case 2: {
      result << coordsToPixels(keyMin, valueMin);
      result.append(result.last());
      break;
    }
    case 8: {
      result << coordsToPixels(keyMax, valueMin);
      result.append(result.last());
      break;
    }
    case 1: {
      result << coordsToPixels(keyMin, valueMin);
      result.append(result.last());
      result << coordsToPixels(keyMin, valueMax);
      break;
    }
    case 7: {
      result << coordsToPixels(keyMax, valueMin);
      result.append(result.last());
      result << coordsToPixels(keyMax, valueMax);
      break;
    }
    }
    break;
  }
  case 7: {
    switch (currentRegion) {
    case 4: {
      result << coordsToPixels(keyMax, valueMax);
      break;
    }
    case 8: {
      result << coordsToPixels(keyMax, valueMax);
      break;
    }
    case 1: {
      result << coordsToPixels(keyMax, valueMax)
             << coordsToPixels(keyMin, valueMax);
      break;
    }
    case 9: {
      result << coordsToPixels(keyMax, valueMax)
             << coordsToPixels(keyMax, valueMin);
      break;
    }
    case 2: {
      result << coordsToPixels(keyMax, valueMax)
             << coordsToPixels(keyMin, valueMax);
      result.append(result.last());
      break;
    }
    case 6: {
      result << coordsToPixels(keyMax, valueMax)
             << coordsToPixels(keyMax, valueMin);
      result.append(result.last());
      break;
    }
    case 3: {
      if ((value - prevValue) / (key - prevKey) * (keyMax - key) + value <
          valueMin)

      {
        result << coordsToPixels(keyMax, valueMax)
               << coordsToPixels(keyMax, valueMin);
        result.append(result.last());
        result << coordsToPixels(keyMin, valueMin);
      } else {
        result << coordsToPixels(keyMax, valueMax)
               << coordsToPixels(keyMin, valueMax);
        result.append(result.last());
        result << coordsToPixels(keyMin, valueMin);
      }
      break;
    }
    }
    break;
  }
  case 8: {
    switch (currentRegion) {
    case 7: {
      result << coordsToPixels(keyMax, valueMax);
      break;
    }
    case 9: {
      result << coordsToPixels(keyMax, valueMin);
      break;
    }
    case 4: {
      result << coordsToPixels(keyMax, valueMax);
      result.append(result.last());
      break;
    }
    case 6: {
      result << coordsToPixels(keyMax, valueMin);
      result.append(result.last());
      break;
    }
    case 1: {
      result << coordsToPixels(keyMax, valueMax);
      result.append(result.last());
      result << coordsToPixels(keyMin, valueMax);
      break;
    }
    case 3: {
      result << coordsToPixels(keyMax, valueMin);
      result.append(result.last());
      result << coordsToPixels(keyMin, valueMin);
      break;
    }
    }
    break;
  }
  case 9: {
    switch (currentRegion) {
    case 6: {
      result << coordsToPixels(keyMax, valueMin);
      break;
    }
    case 8: {
      result << coordsToPixels(keyMax, valueMin);
      break;
    }
    case 3: {
      result << coordsToPixels(keyMax, valueMin)
             << coordsToPixels(keyMin, valueMin);
      break;
    }
    case 7: {
      result << coordsToPixels(keyMax, valueMin)
             << coordsToPixels(keyMax, valueMax);
      break;
    }
    case 2: {
      result << coordsToPixels(keyMax, valueMin)
             << coordsToPixels(keyMin, valueMin);
      result.append(result.last());
      break;
    }
    case 4: {
      result << coordsToPixels(keyMax, valueMin)
             << coordsToPixels(keyMax, valueMax);
      result.append(result.last());
      break;
    }
    case 1: {
      if ((value - prevValue) / (key - prevKey) * (keyMin - key) + value <
          valueMin)

      {
        result << coordsToPixels(keyMax, valueMin)
               << coordsToPixels(keyMin, valueMin);
        result.append(result.last());
        result << coordsToPixels(keyMin, valueMax);
      } else {
        result << coordsToPixels(keyMax, valueMin)
               << coordsToPixels(keyMax, valueMax);
        result.append(result.last());
        result << coordsToPixels(keyMin, valueMax);
      }
      break;
    }
    }
    break;
  }
  }
  return result;
}

bool QCPCurve::mayTraverse(int prevRegion, int currentRegion) const {
  switch (prevRegion) {
  case 1: {
    switch (currentRegion) {
    case 4:
    case 7:
    case 2:
    case 3:
      return false;
    default:
      return true;
    }
  }
  case 2: {
    switch (currentRegion) {
    case 1:
    case 3:
      return false;
    default:
      return true;
    }
  }
  case 3: {
    switch (currentRegion) {
    case 1:
    case 2:
    case 6:
    case 9:
      return false;
    default:
      return true;
    }
  }
  case 4: {
    switch (currentRegion) {
    case 1:
    case 7:
      return false;
    default:
      return true;
    }
  }
  case 5:
    return false;

  case 6: {
    switch (currentRegion) {
    case 3:
    case 9:
      return false;
    default:
      return true;
    }
  }
  case 7: {
    switch (currentRegion) {
    case 1:
    case 4:
    case 8:
    case 9:
      return false;
    default:
      return true;
    }
  }
  case 8: {
    switch (currentRegion) {
    case 7:
    case 9:
      return false;
    default:
      return true;
    }
  }
  case 9: {
    switch (currentRegion) {
    case 3:
    case 6:
    case 8:
    case 7:
      return false;
    default:
      return true;
    }
  }
  default:
    return true;
  }
}

bool QCPCurve::getTraverse(double prevKey, double prevValue, double key,
                           double value, double keyMin, double valueMax,
                           double keyMax, double valueMin, QPointF &crossA,
                           QPointF &crossB) const {
  QList<QPointF> intersections;
  const double valueMinPx = mValueAxis->coordToPixel(valueMin);
  const double valueMaxPx = mValueAxis->coordToPixel(valueMax);
  const double keyMinPx = mKeyAxis->coordToPixel(keyMin);
  const double keyMaxPx = mKeyAxis->coordToPixel(keyMax);
  const double keyPx = mKeyAxis->coordToPixel(key);
  const double valuePx = mValueAxis->coordToPixel(value);
  const double prevKeyPx = mKeyAxis->coordToPixel(prevKey);
  const double prevValuePx = mValueAxis->coordToPixel(prevValue);
  if (qFuzzyIsNull(key - prevKey))

  {
    intersections.append(mKeyAxis->orientation() == Qt::Horizontal
                             ? QPointF(keyPx, valueMinPx)
                             : QPointF(valueMinPx, keyPx));

    intersections.append(mKeyAxis->orientation() == Qt::Horizontal
                             ? QPointF(keyPx, valueMaxPx)
                             : QPointF(valueMaxPx, keyPx));
  } else if (qFuzzyIsNull(value - prevValue))

  {
    intersections.append(mKeyAxis->orientation() == Qt::Horizontal
                             ? QPointF(keyMinPx, valuePx)
                             : QPointF(valuePx, keyMinPx));

    intersections.append(mKeyAxis->orientation() == Qt::Horizontal
                             ? QPointF(keyMaxPx, valuePx)
                             : QPointF(valuePx, keyMaxPx));
  } else

  {
    double gamma;
    double keyPerValuePx = (keyPx - prevKeyPx) / (valuePx - prevValuePx);

    gamma = prevKeyPx + (valueMaxPx - prevValuePx) * keyPerValuePx;
    if (gamma >= qMin(keyMinPx, keyMaxPx) && gamma <= qMax(keyMinPx, keyMaxPx))

      intersections.append(mKeyAxis->orientation() == Qt::Horizontal
                               ? QPointF(gamma, valueMaxPx)
                               : QPointF(valueMaxPx, gamma));

    gamma = prevKeyPx + (valueMinPx - prevValuePx) * keyPerValuePx;
    if (gamma >= qMin(keyMinPx, keyMaxPx) && gamma <= qMax(keyMinPx, keyMaxPx))

      intersections.append(mKeyAxis->orientation() == Qt::Horizontal
                               ? QPointF(gamma, valueMinPx)
                               : QPointF(valueMinPx, gamma));
    const double valuePerKeyPx = 1.0 / keyPerValuePx;

    gamma = prevValuePx + (keyMinPx - prevKeyPx) * valuePerKeyPx;
    if (gamma >= qMin(valueMinPx, valueMaxPx) &&
        gamma <= qMax(valueMinPx, valueMaxPx))

      intersections.append(mKeyAxis->orientation() == Qt::Horizontal
                               ? QPointF(keyMinPx, gamma)
                               : QPointF(gamma, keyMinPx));

    gamma = prevValuePx + (keyMaxPx - prevKeyPx) * valuePerKeyPx;
    if (gamma >= qMin(valueMinPx, valueMaxPx) &&
        gamma <= qMax(valueMinPx, valueMaxPx))

      intersections.append(mKeyAxis->orientation() == Qt::Horizontal
                               ? QPointF(keyMaxPx, gamma)
                               : QPointF(gamma, keyMaxPx));
  }

  if (intersections.size() > 2) {
    double distSqrMax = 0;
    QPointF pv1, pv2;
    for (int i = 0; i < intersections.size() - 1; ++i) {
      for (int k = i + 1; k < intersections.size(); ++k) {
        QPointF distPoint = intersections.at(i) - intersections.at(k);
        double distSqr =
            distPoint.x() * distPoint.x() + distPoint.y() + distPoint.y();
        if (distSqr > distSqrMax) {
          pv1 = intersections.at(i);
          pv2 = intersections.at(k);
          distSqrMax = distSqr;
        }
      }
    }
    intersections = QList<QPointF>() << pv1 << pv2;
  } else if (intersections.size() != 2) {
    return false;
  }

  double xDelta = keyPx - prevKeyPx;
  double yDelta = valuePx - prevValuePx;
  if (mKeyAxis->orientation() != Qt::Horizontal)
    qSwap(xDelta, yDelta);
  if (xDelta * (intersections.at(1).x() - intersections.at(0).x()) +
          yDelta * (intersections.at(1).y() - intersections.at(0).y()) <
      0)

    intersections.move(0, 1);
  crossA = intersections.at(0);
  crossB = intersections.at(1);
  return true;
}

void QCPCurve::getTraverseCornerPoints(int prevRegion, int currentRegion,
                                       double keyMin, double valueMax,
                                       double keyMax, double valueMin,
                                       QVector<QPointF> &beforeTraverse,
                                       QVector<QPointF> &afterTraverse) const {
  switch (prevRegion) {
  case 1: {
    switch (currentRegion) {
    case 6: {
      beforeTraverse << coordsToPixels(keyMin, valueMax);
      break;
    }
    case 9: {
      beforeTraverse << coordsToPixels(keyMin, valueMax);
      afterTraverse << coordsToPixels(keyMax, valueMin);
      break;
    }
    case 8: {
      beforeTraverse << coordsToPixels(keyMin, valueMax);
      break;
    }
    }
    break;
  }
  case 2: {
    switch (currentRegion) {
    case 7: {
      afterTraverse << coordsToPixels(keyMax, valueMax);
      break;
    }
    case 9: {
      afterTraverse << coordsToPixels(keyMax, valueMin);
      break;
    }
    }
    break;
  }
  case 3: {
    switch (currentRegion) {
    case 4: {
      beforeTraverse << coordsToPixels(keyMin, valueMin);
      break;
    }
    case 7: {
      beforeTraverse << coordsToPixels(keyMin, valueMin);
      afterTraverse << coordsToPixels(keyMax, valueMax);
      break;
    }
    case 8: {
      beforeTraverse << coordsToPixels(keyMin, valueMin);
      break;
    }
    }
    break;
  }
  case 4: {
    switch (currentRegion) {
    case 3: {
      afterTraverse << coordsToPixels(keyMin, valueMin);
      break;
    }
    case 9: {
      afterTraverse << coordsToPixels(keyMax, valueMin);
      break;
    }
    }
    break;
  }
  case 5: {
    break;
  }

  case 6: {
    switch (currentRegion) {
    case 1: {
      afterTraverse << coordsToPixels(keyMin, valueMax);
      break;
    }
    case 7: {
      afterTraverse << coordsToPixels(keyMax, valueMax);
      break;
    }
    }
    break;
  }
  case 7: {
    switch (currentRegion) {
    case 2: {
      beforeTraverse << coordsToPixels(keyMax, valueMax);
      break;
    }
    case 3: {
      beforeTraverse << coordsToPixels(keyMax, valueMax);
      afterTraverse << coordsToPixels(keyMin, valueMin);
      break;
    }
    case 6: {
      beforeTraverse << coordsToPixels(keyMax, valueMax);
      break;
    }
    }
    break;
  }
  case 8: {
    switch (currentRegion) {
    case 1: {
      afterTraverse << coordsToPixels(keyMin, valueMax);
      break;
    }
    case 3: {
      afterTraverse << coordsToPixels(keyMin, valueMin);
      break;
    }
    }
    break;
  }
  case 9: {
    switch (currentRegion) {
    case 2: {
      beforeTraverse << coordsToPixels(keyMax, valueMin);
      break;
    }
    case 1: {
      beforeTraverse << coordsToPixels(keyMax, valueMin);
      afterTraverse << coordsToPixels(keyMin, valueMax);
      break;
    }
    case 4: {
      beforeTraverse << coordsToPixels(keyMax, valueMin);
      break;
    }
    }
    break;
  }
  }
}

double QCPCurve::pointDistance(
    const QPointF &pixelPoint,
    QCPCurveDataContainer::const_iterator &closestData) const {
  closestData = mDataContainer->constEnd();
  if (mDataContainer->isEmpty())
    return -1.0;
  if (mLineStyle == lsNone && mScatterStyle.isNone())
    return -1.0;

  if (mDataContainer->size() == 1) {
    QPointF dataPoint = coordsToPixels(mDataContainer->constBegin()->key,
                                       mDataContainer->constBegin()->value);
    closestData = mDataContainer->constBegin();
    return QCPVector2D(dataPoint - pixelPoint).length();
  }

  double minDistSqr = (std::numeric_limits<double>::max)();

  QCPCurveDataContainer::const_iterator begin = mDataContainer->constBegin();
  QCPCurveDataContainer::const_iterator end = mDataContainer->constEnd();
  for (QCPCurveDataContainer::const_iterator it = begin; it != end; ++it) {
    const double currentDistSqr =
        QCPVector2D(coordsToPixels(it->key, it->value) - pixelPoint)
            .lengthSquared();
    if (currentDistSqr < minDistSqr) {
      minDistSqr = currentDistSqr;
      closestData = it;
    }
  }

  if (mLineStyle != lsNone) {
    QVector<QPointF> lines;
    getCurveLines(&lines, QCPDataRange(0, dataCount()),
                  mParentPlot->selectionTolerance() * 1.2);

    for (int i = 0; i < lines.size() - 1; ++i) {
      double currentDistSqr =
          QCPVector2D(pixelPoint)
              .distanceSquaredToLine(lines.at(i), lines.at(i + 1));
      if (currentDistSqr < minDistSqr)
        minDistSqr = currentDistSqr;
    }
  }

  return qSqrt(minDistSqr);
}

QCPBarsGroup::QCPBarsGroup(QCustomPlot *parentPlot)
    : QObject(parentPlot), mParentPlot(parentPlot), mSpacingType(stAbsolute),
      mSpacing(4) {}

QCPBarsGroup::~QCPBarsGroup() { clear(); }

void QCPBarsGroup::setSpacingType(SpacingType spacingType) {
  mSpacingType = spacingType;
}

void QCPBarsGroup::setSpacing(double spacing) { mSpacing = spacing; }

QCPBars *QCPBarsGroup::bars(int index) const {
  if (index >= 0 && index < mBars.size()) {
    return mBars.at(index);
  } else {
    qDebug() << Q_FUNC_INFO << "index out of bounds:" << index;
    return 0;
  }
}

void QCPBarsGroup::clear() {
  Q_FOREACH (QCPBars *bars, mBars)

    bars->setBarsGroup(0);
}

void QCPBarsGroup::append(QCPBars *bars) {
  if (!bars) {
    qDebug() << Q_FUNC_INFO << "bars is 0";
    return;
  }

  if (!mBars.contains(bars))
    bars->setBarsGroup(this);
  else
    qDebug() << Q_FUNC_INFO << "bars plottable is already in this bars group:"
             << reinterpret_cast<quintptr>(bars);
}

void QCPBarsGroup::insert(int i, QCPBars *bars) {
  if (!bars) {
    qDebug() << Q_FUNC_INFO << "bars is 0";
    return;
  }

  if (!mBars.contains(bars))
    bars->setBarsGroup(this);

  mBars.move(mBars.indexOf(bars), qBound(0, i, mBars.size() - 1));
}

void QCPBarsGroup::remove(QCPBars *bars) {
  if (!bars) {
    qDebug() << Q_FUNC_INFO << "bars is 0";
    return;
  }

  if (mBars.contains(bars))
    bars->setBarsGroup(0);
  else
    qDebug() << Q_FUNC_INFO << "bars plottable is not in this bars group:"
             << reinterpret_cast<quintptr>(bars);
}

void QCPBarsGroup::registerBars(QCPBars *bars) {
  if (!mBars.contains(bars))
    mBars.append(bars);
}

void QCPBarsGroup::unregisterBars(QCPBars *bars) { mBars.removeOne(bars); }

double QCPBarsGroup::keyPixelOffset(const QCPBars *bars, double keyCoord) {
  QList<const QCPBars *> baseBars;
  Q_FOREACH (const QCPBars *b, mBars) {
    while (b->barBelow())
      b = b->barBelow();
    if (!baseBars.contains(b))
      baseBars.append(b);
  }

  const QCPBars *thisBase = bars;
  while (thisBase->barBelow())
    thisBase = thisBase->barBelow();

  double result = 0;
  int index = baseBars.indexOf(thisBase);
  if (index >= 0) {
    if (baseBars.size() % 2 == 1 && index == (baseBars.size() - 1) / 2)

    {
      return result;
    } else {
      double lowerPixelWidth, upperPixelWidth;
      int startIndex;
      int dir = (index <= (baseBars.size() - 1) / 2) ? -1 : 1;

      if (baseBars.size() % 2 == 0)

      {
        startIndex = baseBars.size() / 2 + (dir < 0 ? -1 : 0);
        result += getPixelSpacing(baseBars.at(startIndex), keyCoord) * 0.5;

      } else

      {
        startIndex = (baseBars.size() - 1) / 2 + dir;
        baseBars.at((baseBars.size() - 1) / 2)
            ->getPixelWidth(keyCoord, lowerPixelWidth, upperPixelWidth);
        result += qAbs(upperPixelWidth - lowerPixelWidth) * 0.5;

        result +=
            getPixelSpacing(baseBars.at((baseBars.size() - 1) / 2), keyCoord);
      }
      for (int i = startIndex; i != index; i += dir)

      {
        baseBars.at(i)->getPixelWidth(keyCoord, lowerPixelWidth,
                                      upperPixelWidth);
        result += qAbs(upperPixelWidth - lowerPixelWidth);
        result += getPixelSpacing(baseBars.at(i), keyCoord);
      }

      baseBars.at(index)->getPixelWidth(keyCoord, lowerPixelWidth,
                                        upperPixelWidth);
      result += qAbs(upperPixelWidth - lowerPixelWidth) * 0.5;

      result *= dir * thisBase->keyAxis()->pixelOrientation();
    }
  }
  return result;
}

double QCPBarsGroup::getPixelSpacing(const QCPBars *bars, double keyCoord) {
  switch (mSpacingType) {
  case stAbsolute: {
    return mSpacing;
  }
  case stAxisRectRatio: {
    if (bars->keyAxis()->orientation() == Qt::Horizontal)
      return bars->keyAxis()->axisRect()->width() * mSpacing;
    else
      return bars->keyAxis()->axisRect()->height() * mSpacing;
  }
  case stPlotCoords: {
    double keyPixel = bars->keyAxis()->coordToPixel(keyCoord);
    return qAbs(bars->keyAxis()->coordToPixel(keyCoord + mSpacing) - keyPixel);
  }
  }
  return 0;
}

QCPBarsData::QCPBarsData() : key(0), value(0) {}

QCPBarsData::QCPBarsData(double key, double value) : key(key), value(value) {}

QCPBars::QCPBars(QCPAxis *keyAxis, QCPAxis *valueAxis)
    : QCPAbstractPlottable1D<QCPBarsData>(keyAxis, valueAxis), mWidth(0.75),
      mWidthType(wtPlotCoords), mBarsGroup(0), mBaseValue(0), mStackingGap(0) {
  mPen.setColor(Qt::blue);
  mPen.setStyle(Qt::SolidLine);
  mBrush.setColor(QColor(40, 50, 255, 30));
  mBrush.setStyle(Qt::SolidPattern);
  mSelectionDecorator->setBrush(QBrush(QColor(160, 160, 255)));
}

QCPBars::~QCPBars() {
  setBarsGroup(0);
  if (mBarBelow || mBarAbove)
    connectBars(mBarBelow.data(), mBarAbove.data());
}

void QCPBars::setData(QSharedPointer<QCPBarsDataContainer> data) {
  mDataContainer = data;
}

void QCPBars::setData(const QVector<double> &keys,
                      const QVector<double> &values, bool alreadySorted) {
  mDataContainer->clear();
  addData(keys, values, alreadySorted);
}

void QCPBars::setWidth(double width) { mWidth = width; }

void QCPBars::setWidthType(QCPBars::WidthType widthType) {
  mWidthType = widthType;
}

void QCPBars::setBarsGroup(QCPBarsGroup *barsGroup) {
  if (mBarsGroup)
    mBarsGroup->unregisterBars(this);
  mBarsGroup = barsGroup;

  if (mBarsGroup)
    mBarsGroup->registerBars(this);
}

void QCPBars::setBaseValue(double baseValue) { mBaseValue = baseValue; }

void QCPBars::setStackingGap(double pixels) { mStackingGap = pixels; }

void QCPBars::addData(const QVector<double> &keys,
                      const QVector<double> &values, bool alreadySorted) {
  if (keys.size() != values.size())
    qDebug() << Q_FUNC_INFO
             << "keys and values have different sizes:" << keys.size()
             << values.size();
  const int n = qMin(keys.size(), values.size());
  QVector<QCPBarsData> tempData(n);
  QVector<QCPBarsData>::iterator it = tempData.begin();
  const QVector<QCPBarsData>::iterator itEnd = tempData.end();
  int i = 0;
  while (it != itEnd) {
    it->key = keys[i];
    it->value = values[i];
    ++it;
    ++i;
  }
  mDataContainer->add(tempData, alreadySorted);
}

void QCPBars::addData(double key, double value) {
  mDataContainer->add(QCPBarsData(key, value));
}

void QCPBars::moveBelow(QCPBars *bars) {
  if (bars == this)
    return;
  if (bars && (bars->keyAxis() != mKeyAxis.data() ||
               bars->valueAxis() != mValueAxis.data())) {
    qDebug() << Q_FUNC_INFO
             << "passed QCPBars* doesn't have same key and value axis as this "
                "QCPBars";
    return;
  }

  connectBars(mBarBelow.data(), mBarAbove.data());

  if (bars) {
    if (bars->mBarBelow)
      connectBars(bars->mBarBelow.data(), this);
    connectBars(this, bars);
  }
}

void QCPBars::moveAbove(QCPBars *bars) {
  if (bars == this)
    return;
  if (bars && (bars->keyAxis() != mKeyAxis.data() ||
               bars->valueAxis() != mValueAxis.data())) {
    qDebug() << Q_FUNC_INFO
             << "passed QCPBars* doesn't have same key and value axis as this "
                "QCPBars";
    return;
  }

  connectBars(mBarBelow.data(), mBarAbove.data());

  if (bars) {
    if (bars->mBarAbove)
      connectBars(this, bars->mBarAbove.data());
    connectBars(bars, this);
  }
}

QCPDataSelection QCPBars::selectTestRect(const QRectF &rect,
                                         bool onlySelectable) const {
  QCPDataSelection result;
  if ((onlySelectable && mSelectable == QCP::stNone) ||
      mDataContainer->isEmpty())
    return result;
  if (!mKeyAxis || !mValueAxis)
    return result;

  QCPBarsDataContainer::const_iterator visibleBegin, visibleEnd;
  getVisibleDataBounds(visibleBegin, visibleEnd);

  for (QCPBarsDataContainer::const_iterator it = visibleBegin; it != visibleEnd;
       ++it) {
    if (rect.intersects(getBarRect(it->key, it->value)))
      result.addDataRange(QCPDataRange(it - mDataContainer->constBegin(),
                                       it - mDataContainer->constBegin() + 1),
                          false);
  }
  result.simplify();
  return result;
}

double QCPBars::selectTest(const QPointF &pos, bool onlySelectable,
                           QVariant *details) const {
  Q_UNUSED(details)
  if ((onlySelectable && mSelectable == QCP::stNone) ||
      mDataContainer->isEmpty())
    return -1;
  if (!mKeyAxis || !mValueAxis)
    return -1;

  if (mKeyAxis.data()->axisRect()->rect().contains(pos.toPoint())) {
    QCPBarsDataContainer::const_iterator visibleBegin, visibleEnd;
    getVisibleDataBounds(visibleBegin, visibleEnd);
    for (QCPBarsDataContainer::const_iterator it = visibleBegin;
         it != visibleEnd; ++it) {
      if (getBarRect(it->key, it->value).contains(pos)) {
        if (details) {
          int pointIndex = it - mDataContainer->constBegin();
          details->setValue(
              QCPDataSelection(QCPDataRange(pointIndex, pointIndex + 1)));
        }
        return mParentPlot->selectionTolerance() * 0.99;
      }
    }
  }
  return -1;
}

QCPRange QCPBars::getKeyRange(bool &foundRange,
                              QCP::SignDomain inSignDomain) const {
  QCPRange range;
  range = mDataContainer->keyRange(foundRange, inSignDomain);

  if (foundRange && mKeyAxis) {
    double lowerPixelWidth, upperPixelWidth, keyPixel;

    getPixelWidth(range.lower, lowerPixelWidth, upperPixelWidth);
    keyPixel = mKeyAxis.data()->coordToPixel(range.lower) + lowerPixelWidth;
    if (mBarsGroup)
      keyPixel += mBarsGroup->keyPixelOffset(this, range.lower);
    const double lowerCorrected = mKeyAxis.data()->pixelToCoord(keyPixel);
    if (!qIsNaN(lowerCorrected) && qIsFinite(lowerCorrected) &&
        range.lower > lowerCorrected)
      range.lower = lowerCorrected;

    getPixelWidth(range.upper, lowerPixelWidth, upperPixelWidth);
    keyPixel = mKeyAxis.data()->coordToPixel(range.upper) + upperPixelWidth;
    if (mBarsGroup)
      keyPixel += mBarsGroup->keyPixelOffset(this, range.upper);
    const double upperCorrected = mKeyAxis.data()->pixelToCoord(keyPixel);
    if (!qIsNaN(upperCorrected) && qIsFinite(upperCorrected) &&
        range.upper < upperCorrected)
      range.upper = upperCorrected;
  }
  return range;
}

QCPRange QCPBars::getValueRange(bool &foundRange, QCP::SignDomain inSignDomain,
                                const QCPRange &inKeyRange) const {
  QCPRange range;
  range.lower = mBaseValue;
  range.upper = mBaseValue;
  bool haveLower = true;

  bool haveUpper = true;

  QCPBarsDataContainer::const_iterator itBegin = mDataContainer->constBegin();
  QCPBarsDataContainer::const_iterator itEnd = mDataContainer->constEnd();
  if (inKeyRange != QCPRange()) {
    itBegin = mDataContainer->findBegin(inKeyRange.lower);
    itEnd = mDataContainer->findEnd(inKeyRange.upper);
  }
  for (QCPBarsDataContainer::const_iterator it = itBegin; it != itEnd; ++it) {
    const double current =
        it->value + getStackedBaseValue(it->key, it->value >= 0);
    if (qIsNaN(current))
      continue;
    if (inSignDomain == QCP::sdBoth ||
        (inSignDomain == QCP::sdNegative && current < 0) ||
        (inSignDomain == QCP::sdPositive && current > 0)) {
      if (current < range.lower || !haveLower) {
        range.lower = current;
        haveLower = true;
      }
      if (current > range.upper || !haveUpper) {
        range.upper = current;
        haveUpper = true;
      }
    }
  }

  foundRange = true;

  return range;
}

QPointF QCPBars::dataPixelPosition(int index) const {
  if (index >= 0 && index < mDataContainer->size()) {
    QCPAxis *keyAxis = mKeyAxis.data();
    QCPAxis *valueAxis = mValueAxis.data();
    if (!keyAxis || !valueAxis) {
      qDebug() << Q_FUNC_INFO << "invalid key or value axis";
      return QPointF();
    }

    const QCPDataContainer<QCPBarsData>::const_iterator it =
        mDataContainer->constBegin() + index;
    const double valuePixel = valueAxis->coordToPixel(
        getStackedBaseValue(it->key, it->value >= 0) + it->value);
    const double keyPixel =
        keyAxis->coordToPixel(it->key) +
        (mBarsGroup ? mBarsGroup->keyPixelOffset(this, it->key) : 0);
    if (keyAxis->orientation() == Qt::Horizontal)
      return QPointF(keyPixel, valuePixel);
    else
      return QPointF(valuePixel, keyPixel);
  } else {
    qDebug() << Q_FUNC_INFO << "Index out of bounds" << index;
    return QPointF();
  }
}

void QCPBars::draw(QCPPainter *painter) {
  if (!mKeyAxis || !mValueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return;
  }
  if (mDataContainer->isEmpty())
    return;

  QCPBarsDataContainer::const_iterator visibleBegin, visibleEnd;
  getVisibleDataBounds(visibleBegin, visibleEnd);

  QList<QCPDataRange> selectedSegments, unselectedSegments, allSegments;
  getDataSegments(selectedSegments, unselectedSegments);
  allSegments << unselectedSegments << selectedSegments;
  for (int i = 0; i < allSegments.size(); ++i) {
    bool isSelectedSegment = i >= unselectedSegments.size();
    QCPBarsDataContainer::const_iterator begin = visibleBegin;
    QCPBarsDataContainer::const_iterator end = visibleEnd;
    mDataContainer->limitIteratorsToDataRange(begin, end, allSegments.at(i));
    if (begin == end)
      continue;

    for (QCPBarsDataContainer::const_iterator it = begin; it != end; ++it) {
#ifdef QCUSTOMPLOT_CHECK_DATA
      if (QCP::isInvalidData(it->key, it->value))
        qDebug() << Q_FUNC_INFO << "Data point at" << it->key
                 << "of drawn range invalid." << "Plottable name:" << name();
#endif

      if (isSelectedSegment && mSelectionDecorator) {
        mSelectionDecorator->applyBrush(painter);
        mSelectionDecorator->applyPen(painter);
      } else {
        painter->setBrush(mBrush);
        painter->setPen(mPen);
      }
      applyDefaultAntialiasingHint(painter);
      painter->drawPolygon(getBarRect(it->key, it->value));
    }
  }

  if (mSelectionDecorator)
    mSelectionDecorator->drawDecoration(painter, selection());
}

void QCPBars::drawLegendIcon(QCPPainter *painter, const QRectF &rect) const {
  applyDefaultAntialiasingHint(painter);
  painter->setBrush(mBrush);
  painter->setPen(mPen);
  QRectF r = QRectF(0, 0, rect.width() * 0.67, rect.height() * 0.67);
  r.moveCenter(rect.center());
  painter->drawRect(r);
}

void QCPBars::getVisibleDataBounds(
    QCPBarsDataContainer::const_iterator &begin,
    QCPBarsDataContainer::const_iterator &end) const {
  if (!mKeyAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key axis";
    begin = mDataContainer->constEnd();
    end = mDataContainer->constEnd();
    return;
  }
  if (mDataContainer->isEmpty()) {
    begin = mDataContainer->constEnd();
    end = mDataContainer->constEnd();
    return;
  }

  begin = mDataContainer->findBegin(mKeyAxis.data()->range().lower);
  end = mDataContainer->findEnd(mKeyAxis.data()->range().upper);
  double lowerPixelBound =
      mKeyAxis.data()->coordToPixel(mKeyAxis.data()->range().lower);
  double upperPixelBound =
      mKeyAxis.data()->coordToPixel(mKeyAxis.data()->range().upper);
  bool isVisible = false;

  QCPBarsDataContainer::const_iterator it = begin;
  while (it != mDataContainer->constBegin()) {
    --it;
    const QRectF barRect = getBarRect(it->key, it->value);
    if (mKeyAxis.data()->orientation() == Qt::Horizontal)
      isVisible = ((!mKeyAxis.data()->rangeReversed() &&
                    barRect.right() >= lowerPixelBound) ||
                   (mKeyAxis.data()->rangeReversed() &&
                    barRect.left() <= lowerPixelBound));
    else

      isVisible = ((!mKeyAxis.data()->rangeReversed() &&
                    barRect.top() <= lowerPixelBound) ||
                   (mKeyAxis.data()->rangeReversed() &&
                    barRect.bottom() >= lowerPixelBound));
    if (isVisible)
      begin = it;
    else
      break;
  }

  it = end;
  while (it != mDataContainer->constEnd()) {
    const QRectF barRect = getBarRect(it->key, it->value);
    if (mKeyAxis.data()->orientation() == Qt::Horizontal)
      isVisible = ((!mKeyAxis.data()->rangeReversed() &&
                    barRect.left() <= upperPixelBound) ||
                   (mKeyAxis.data()->rangeReversed() &&
                    barRect.right() >= upperPixelBound));
    else

      isVisible = ((!mKeyAxis.data()->rangeReversed() &&
                    barRect.bottom() >= upperPixelBound) ||
                   (mKeyAxis.data()->rangeReversed() &&
                    barRect.top() <= upperPixelBound));
    if (isVisible)
      end = it + 1;
    else
      break;
    ++it;
  }
}

QRectF QCPBars::getBarRect(double key, double value) const {
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return QRectF();
  }

  double lowerPixelWidth, upperPixelWidth;
  getPixelWidth(key, lowerPixelWidth, upperPixelWidth);
  double base = getStackedBaseValue(key, value >= 0);
  double basePixel = valueAxis->coordToPixel(base);
  double valuePixel = valueAxis->coordToPixel(base + value);
  double keyPixel = keyAxis->coordToPixel(key);
  if (mBarsGroup)
    keyPixel += mBarsGroup->keyPixelOffset(this, key);
  double bottomOffset = (mBarBelow && mPen != Qt::NoPen ? 1 : 0) *
                        (mPen.isCosmetic() ? 1 : mPen.widthF());
  bottomOffset += mBarBelow ? mStackingGap : 0;
  bottomOffset *= (value < 0 ? -1 : 1) * valueAxis->pixelOrientation();
  if (qAbs(valuePixel - basePixel) <= qAbs(bottomOffset))
    bottomOffset = valuePixel - basePixel;
  if (keyAxis->orientation() == Qt::Horizontal) {
    return QRectF(QPointF(keyPixel + lowerPixelWidth, valuePixel),
                  QPointF(keyPixel + upperPixelWidth, basePixel + bottomOffset))
        .normalized();
  } else {
    return QRectF(QPointF(basePixel + bottomOffset, keyPixel + lowerPixelWidth),
                  QPointF(valuePixel, keyPixel + upperPixelWidth))
        .normalized();
  }
}

void QCPBars::getPixelWidth(double key, double &lower, double &upper) const {
  lower = 0;
  upper = 0;
  switch (mWidthType) {
  case wtAbsolute: {
    upper = mWidth * 0.5 * mKeyAxis.data()->pixelOrientation();
    lower = -upper;
    break;
  }
  case wtAxisRectRatio: {
    if (mKeyAxis && mKeyAxis.data()->axisRect()) {
      if (mKeyAxis.data()->orientation() == Qt::Horizontal)
        upper = mKeyAxis.data()->axisRect()->width() * mWidth * 0.5 *
                mKeyAxis.data()->pixelOrientation();
      else
        upper = mKeyAxis.data()->axisRect()->height() * mWidth * 0.5 *
                mKeyAxis.data()->pixelOrientation();
      lower = -upper;
    } else
      qDebug() << Q_FUNC_INFO << "No key axis or axis rect defined";
    break;
  }
  case wtPlotCoords: {
    if (mKeyAxis) {
      double keyPixel = mKeyAxis.data()->coordToPixel(key);
      upper = mKeyAxis.data()->coordToPixel(key + mWidth * 0.5) - keyPixel;
      lower = mKeyAxis.data()->coordToPixel(key - mWidth * 0.5) - keyPixel;

    } else
      qDebug() << Q_FUNC_INFO << "No key axis defined";
    break;
  }
  }
}

double QCPBars::getStackedBaseValue(double key, bool positive) const {
  if (mBarBelow) {
    double max = 0;

    double epsilon = qAbs(key) * (sizeof(key) == 4 ? 1e-6 : 1e-14);

    if (key == 0)
      epsilon = (sizeof(key) == 4 ? 1e-6 : 1e-14);
    QCPBarsDataContainer::const_iterator it =
        mBarBelow.data()->mDataContainer->findBegin(key - epsilon);
    QCPBarsDataContainer::const_iterator itEnd =
        mBarBelow.data()->mDataContainer->findEnd(key + epsilon);
    while (it != itEnd) {
      if (it->key > key - epsilon && it->key < key + epsilon) {
        if ((positive && it->value > max) || (!positive && it->value < max))
          max = it->value;
      }
      ++it;
    }

    return max + mBarBelow.data()->getStackedBaseValue(key, positive);
  } else
    return mBaseValue;
}

void QCPBars::connectBars(QCPBars *lower, QCPBars *upper) {
  if (!lower && !upper)
    return;

  if (!lower)

  {
    if (upper->mBarBelow && upper->mBarBelow.data()->mBarAbove.data() == upper)
      upper->mBarBelow.data()->mBarAbove = 0;
    upper->mBarBelow = 0;
  } else if (!upper)

  {
    if (lower->mBarAbove && lower->mBarAbove.data()->mBarBelow.data() == lower)
      lower->mBarAbove.data()->mBarBelow = 0;
    lower->mBarAbove = 0;
  } else

  {
    if (lower->mBarAbove && lower->mBarAbove.data()->mBarBelow.data() == lower)
      lower->mBarAbove.data()->mBarBelow = 0;

    if (upper->mBarBelow && upper->mBarBelow.data()->mBarAbove.data() == upper)
      upper->mBarBelow.data()->mBarAbove = 0;
    lower->mBarAbove = upper;
    upper->mBarBelow = lower;
  }
}

QCPStatisticalBoxData::QCPStatisticalBoxData()
    : key(0), minimum(0), lowerQuartile(0), median(0), upperQuartile(0),
      maximum(0) {}

QCPStatisticalBoxData::QCPStatisticalBoxData(
    double key, double minimum, double lowerQuartile, double median,
    double upperQuartile, double maximum, const QVector<double> &outliers)
    : key(key), minimum(minimum), lowerQuartile(lowerQuartile), median(median),
      upperQuartile(upperQuartile), maximum(maximum), outliers(outliers) {}

QCPStatisticalBox::QCPStatisticalBox(QCPAxis *keyAxis, QCPAxis *valueAxis)
    : QCPAbstractPlottable1D<QCPStatisticalBoxData>(keyAxis, valueAxis),
      mWidth(0.5), mWhiskerWidth(0.2),
      mWhiskerPen(Qt::black, 0, Qt::DashLine, Qt::FlatCap),
      mWhiskerBarPen(Qt::black), mWhiskerAntialiased(false),
      mMedianPen(Qt::black, 3, Qt::SolidLine, Qt::FlatCap),
      mOutlierStyle(QCPScatterStyle::ssCircle, Qt::blue, 6) {
  setPen(QPen(Qt::black));
  setBrush(Qt::NoBrush);
}

void QCPStatisticalBox::setData(
    QSharedPointer<QCPStatisticalBoxDataContainer> data) {
  mDataContainer = data;
}

void QCPStatisticalBox::setData(const QVector<double> &keys,
                                const QVector<double> &minimum,
                                const QVector<double> &lowerQuartile,
                                const QVector<double> &median,
                                const QVector<double> &upperQuartile,
                                const QVector<double> &maximum,
                                bool alreadySorted) {
  mDataContainer->clear();
  addData(keys, minimum, lowerQuartile, median, upperQuartile, maximum,
          alreadySorted);
}

void QCPStatisticalBox::setWidth(double width) { mWidth = width; }

void QCPStatisticalBox::setWhiskerWidth(double width) { mWhiskerWidth = width; }

void QCPStatisticalBox::setWhiskerPen(const QPen &pen) { mWhiskerPen = pen; }

void QCPStatisticalBox::setWhiskerBarPen(const QPen &pen) {
  mWhiskerBarPen = pen;
}

void QCPStatisticalBox::setWhiskerAntialiased(bool enabled) {
  mWhiskerAntialiased = enabled;
}

void QCPStatisticalBox::setMedianPen(const QPen &pen) { mMedianPen = pen; }

void QCPStatisticalBox::setOutlierStyle(const QCPScatterStyle &style) {
  mOutlierStyle = style;
}

void QCPStatisticalBox::addData(const QVector<double> &keys,
                                const QVector<double> &minimum,
                                const QVector<double> &lowerQuartile,
                                const QVector<double> &median,
                                const QVector<double> &upperQuartile,
                                const QVector<double> &maximum,
                                bool alreadySorted) {
  if (keys.size() != minimum.size() || minimum.size() != lowerQuartile.size() ||
      lowerQuartile.size() != median.size() ||
      median.size() != upperQuartile.size() ||
      upperQuartile.size() != maximum.size() || maximum.size() != keys.size())
    qDebug() << Q_FUNC_INFO
             << "keys, minimum, lowerQuartile, median, upperQuartile, maximum "
                "have different sizes:"
             << keys.size() << minimum.size() << lowerQuartile.size()
             << median.size() << upperQuartile.size() << maximum.size();
  const int n =
      qMin(keys.size(), qMin(minimum.size(),
                             qMin(lowerQuartile.size(),
                                  qMin(median.size(), qMin(upperQuartile.size(),
                                                           maximum.size())))));
  QVector<QCPStatisticalBoxData> tempData(n);
  QVector<QCPStatisticalBoxData>::iterator it = tempData.begin();
  const QVector<QCPStatisticalBoxData>::iterator itEnd = tempData.end();
  int i = 0;
  while (it != itEnd) {
    it->key = keys[i];
    it->minimum = minimum[i];
    it->lowerQuartile = lowerQuartile[i];
    it->median = median[i];
    it->upperQuartile = upperQuartile[i];
    it->maximum = maximum[i];
    ++it;
    ++i;
  }
  mDataContainer->add(tempData, alreadySorted);
}

void QCPStatisticalBox::addData(double key, double minimum,
                                double lowerQuartile, double median,
                                double upperQuartile, double maximum,
                                const QVector<double> &outliers) {
  mDataContainer->add(QCPStatisticalBoxData(key, minimum, lowerQuartile, median,
                                            upperQuartile, maximum, outliers));
}

QCPDataSelection QCPStatisticalBox::selectTestRect(const QRectF &rect,
                                                   bool onlySelectable) const {
  QCPDataSelection result;
  if ((onlySelectable && mSelectable == QCP::stNone) ||
      mDataContainer->isEmpty())
    return result;
  if (!mKeyAxis || !mValueAxis)
    return result;

  QCPStatisticalBoxDataContainer::const_iterator visibleBegin, visibleEnd;
  getVisibleDataBounds(visibleBegin, visibleEnd);

  for (QCPStatisticalBoxDataContainer::const_iterator it = visibleBegin;
       it != visibleEnd; ++it) {
    if (rect.intersects(getQuartileBox(it)))
      result.addDataRange(QCPDataRange(it - mDataContainer->constBegin(),
                                       it - mDataContainer->constBegin() + 1),
                          false);
  }
  result.simplify();
  return result;
}

double QCPStatisticalBox::selectTest(const QPointF &pos, bool onlySelectable,
                                     QVariant *details) const {
  Q_UNUSED(details)
  if ((onlySelectable && mSelectable == QCP::stNone) ||
      mDataContainer->isEmpty())
    return -1;
  if (!mKeyAxis || !mValueAxis)
    return -1;

  if (mKeyAxis->axisRect()->rect().contains(pos.toPoint())) {
    QCPStatisticalBoxDataContainer::const_iterator visibleBegin, visibleEnd;
    QCPStatisticalBoxDataContainer::const_iterator closestDataPoint =
        mDataContainer->constEnd();
    getVisibleDataBounds(visibleBegin, visibleEnd);
    double minDistSqr = (std::numeric_limits<double>::max)();
    for (QCPStatisticalBoxDataContainer::const_iterator it = visibleBegin;
         it != visibleEnd; ++it) {
      if (getQuartileBox(it).contains(pos))

      {
        double currentDistSqr = mParentPlot->selectionTolerance() * 0.99 *
                                mParentPlot->selectionTolerance() * 0.99;
        if (currentDistSqr < minDistSqr) {
          minDistSqr = currentDistSqr;
          closestDataPoint = it;
        }
      } else

      {
        const QVector<QLineF> whiskerBackbones(getWhiskerBackboneLines(it));
        for (int i = 0; i < whiskerBackbones.size(); ++i) {
          double currentDistSqr =
              QCPVector2D(pos).distanceSquaredToLine(whiskerBackbones.at(i));
          if (currentDistSqr < minDistSqr) {
            minDistSqr = currentDistSqr;
            closestDataPoint = it;
          }
        }
      }
    }
    if (details) {
      int pointIndex = closestDataPoint - mDataContainer->constBegin();
      details->setValue(
          QCPDataSelection(QCPDataRange(pointIndex, pointIndex + 1)));
    }
    return qSqrt(minDistSqr);
  }
  return -1;
}

QCPRange QCPStatisticalBox::getKeyRange(bool &foundRange,
                                        QCP::SignDomain inSignDomain) const {
  QCPRange range = mDataContainer->keyRange(foundRange, inSignDomain);

  if (foundRange) {
    if (inSignDomain != QCP::sdPositive || range.lower - mWidth * 0.5 > 0)
      range.lower -= mWidth * 0.5;
    if (inSignDomain != QCP::sdNegative || range.upper + mWidth * 0.5 < 0)
      range.upper += mWidth * 0.5;
  }
  return range;
}

QCPRange QCPStatisticalBox::getValueRange(bool &foundRange,
                                          QCP::SignDomain inSignDomain,
                                          const QCPRange &inKeyRange) const {
  return mDataContainer->valueRange(foundRange, inSignDomain, inKeyRange);
}

void QCPStatisticalBox::draw(QCPPainter *painter) {
  if (mDataContainer->isEmpty())
    return;
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return;
  }

  QCPStatisticalBoxDataContainer::const_iterator visibleBegin, visibleEnd;
  getVisibleDataBounds(visibleBegin, visibleEnd);

  QList<QCPDataRange> selectedSegments, unselectedSegments, allSegments;
  getDataSegments(selectedSegments, unselectedSegments);
  allSegments << unselectedSegments << selectedSegments;
  for (int i = 0; i < allSegments.size(); ++i) {
    bool isSelectedSegment = i >= unselectedSegments.size();
    QCPStatisticalBoxDataContainer::const_iterator begin = visibleBegin;
    QCPStatisticalBoxDataContainer::const_iterator end = visibleEnd;
    mDataContainer->limitIteratorsToDataRange(begin, end, allSegments.at(i));
    if (begin == end)
      continue;

    for (QCPStatisticalBoxDataContainer::const_iterator it = begin; it != end;
         ++it) {
#ifdef QCUSTOMPLOT_CHECK_DATA
      if (QCP::isInvalidData(it->key, it->minimum) ||
          QCP::isInvalidData(it->lowerQuartile, it->median) ||
          QCP::isInvalidData(it->upperQuartile, it->maximum))
        qDebug() << Q_FUNC_INFO << "Data point at" << it->key
                 << "of drawn range has invalid data."
                 << "Plottable name:" << name();
      for (int i = 0; i < it->outliers.size(); ++i)
        if (QCP::isInvalidData(it->outliers.at(i)))
          qDebug() << Q_FUNC_INFO << "Data point outlier at" << it->key
                   << "of drawn range invalid." << "Plottable name:" << name();
#endif

      if (isSelectedSegment && mSelectionDecorator) {
        mSelectionDecorator->applyPen(painter);
        mSelectionDecorator->applyBrush(painter);
      } else {
        painter->setPen(mPen);
        painter->setBrush(mBrush);
      }
      QCPScatterStyle finalOutlierStyle = mOutlierStyle;
      if (isSelectedSegment && mSelectionDecorator)
        finalOutlierStyle =
            mSelectionDecorator->getFinalScatterStyle(mOutlierStyle);
      drawStatisticalBox(painter, it, finalOutlierStyle);
    }
  }

  if (mSelectionDecorator)
    mSelectionDecorator->drawDecoration(painter, selection());
}

void QCPStatisticalBox::drawLegendIcon(QCPPainter *painter,
                                       const QRectF &rect) const {
  applyDefaultAntialiasingHint(painter);
  painter->setPen(mPen);
  painter->setBrush(mBrush);
  QRectF r = QRectF(0, 0, rect.width() * 0.67, rect.height() * 0.67);
  r.moveCenter(rect.center());
  painter->drawRect(r);
}

void QCPStatisticalBox::drawStatisticalBox(
    QCPPainter *painter, QCPStatisticalBoxDataContainer::const_iterator it,
    const QCPScatterStyle &outlierStyle) const {
  applyDefaultAntialiasingHint(painter);
  const QRectF quartileBox = getQuartileBox(it);
  painter->drawRect(quartileBox);

  painter->save();
  painter->setClipRect(quartileBox, Qt::IntersectClip);
  painter->setPen(mMedianPen);
  painter->drawLine(QLineF(coordsToPixels(it->key - mWidth * 0.5, it->median),
                           coordsToPixels(it->key + mWidth * 0.5, it->median)));
  painter->restore();

  applyAntialiasingHint(painter, mWhiskerAntialiased, QCP::aePlottables);
  painter->setPen(mWhiskerPen);
  painter->drawLines(getWhiskerBackboneLines(it));
  painter->setPen(mWhiskerBarPen);
  painter->drawLines(getWhiskerBarLines(it));

  applyScattersAntialiasingHint(painter);
  outlierStyle.applyTo(painter, mPen);
  for (int i = 0; i < it->outliers.size(); ++i)
    outlierStyle.drawShape(painter,
                           coordsToPixels(it->key, it->outliers.at(i)));
}

void QCPStatisticalBox::getVisibleDataBounds(
    QCPStatisticalBoxDataContainer::const_iterator &begin,
    QCPStatisticalBoxDataContainer::const_iterator &end) const {
  if (!mKeyAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key axis";
    begin = mDataContainer->constEnd();
    end = mDataContainer->constEnd();
    return;
  }
  begin =
      mDataContainer->findBegin(mKeyAxis.data()->range().lower - mWidth * 0.5);

  end = mDataContainer->findEnd(mKeyAxis.data()->range().upper + mWidth * 0.5);
}

QRectF QCPStatisticalBox::getQuartileBox(
    QCPStatisticalBoxDataContainer::const_iterator it) const {
  QRectF result;
  result.setTopLeft(coordsToPixels(it->key - mWidth * 0.5, it->upperQuartile));
  result.setBottomRight(
      coordsToPixels(it->key + mWidth * 0.5, it->lowerQuartile));
  return result;
}

QVector<QLineF> QCPStatisticalBox::getWhiskerBackboneLines(
    QCPStatisticalBoxDataContainer::const_iterator it) const {
  QVector<QLineF> result(2);
  result[0].setPoints(coordsToPixels(it->key, it->lowerQuartile),
                      coordsToPixels(it->key, it->minimum));

  result[1].setPoints(coordsToPixels(it->key, it->upperQuartile),
                      coordsToPixels(it->key, it->maximum));

  return result;
}

QVector<QLineF> QCPStatisticalBox::getWhiskerBarLines(
    QCPStatisticalBoxDataContainer::const_iterator it) const {
  QVector<QLineF> result(2);
  result[0].setPoints(
      coordsToPixels(it->key - mWhiskerWidth * 0.5, it->minimum),
      coordsToPixels(it->key + mWhiskerWidth * 0.5, it->minimum));

  result[1].setPoints(
      coordsToPixels(it->key - mWhiskerWidth * 0.5, it->maximum),
      coordsToPixels(it->key + mWhiskerWidth * 0.5, it->maximum));

  return result;
}

QCPColorMapData::QCPColorMapData(int keySize, int valueSize,
                                 const QCPRange &keyRange,
                                 const QCPRange &valueRange)
    : mKeySize(0), mValueSize(0), mKeyRange(keyRange), mValueRange(valueRange),
      mIsEmpty(true), mData(0), mAlpha(0), mDataModified(true) {
  setSize(keySize, valueSize);
  fill(0);
}

QCPColorMapData::~QCPColorMapData() {
  if (mData)
    delete[] mData;
  if (mAlpha)
    delete[] mAlpha;
}

QCPColorMapData::QCPColorMapData(const QCPColorMapData &other)
    : mKeySize(0), mValueSize(0), mIsEmpty(true), mData(0), mAlpha(0),
      mDataModified(true) {
  *this = other;
}

QCPColorMapData &QCPColorMapData::operator=(const QCPColorMapData &other) {
  if (&other != this) {
    const int keySize = other.keySize();
    const int valueSize = other.valueSize();
    if (!other.mAlpha && mAlpha)
      clearAlpha();
    setSize(keySize, valueSize);
    if (other.mAlpha && !mAlpha)
      createAlpha(false);
    setRange(other.keyRange(), other.valueRange());
    if (!isEmpty()) {
      memcpy(mData, other.mData, sizeof(mData[0]) * keySize * valueSize);
      if (mAlpha)
        memcpy(mAlpha, other.mAlpha, sizeof(mAlpha[0]) * keySize * valueSize);
    }
    mDataBounds = other.mDataBounds;
    mDataModified = true;
  }
  return *this;
}

double QCPColorMapData::data(double key, double value) {
  int keyCell = (key - mKeyRange.lower) / (mKeyRange.upper - mKeyRange.lower) *
                    (mKeySize - 1) +
                0.5;
  int valueCell = (value - mValueRange.lower) /
                      (mValueRange.upper - mValueRange.lower) *
                      (mValueSize - 1) +
                  0.5;
  if (keyCell >= 0 && keyCell < mKeySize && valueCell >= 0 &&
      valueCell < mValueSize)
    return mData[valueCell * mKeySize + keyCell];
  else
    return 0;
}

double QCPColorMapData::cell(int keyIndex, int valueIndex) {
  if (keyIndex >= 0 && keyIndex < mKeySize && valueIndex >= 0 &&
      valueIndex < mValueSize)
    return mData[valueIndex * mKeySize + keyIndex];
  else
    return 0;
}

unsigned char QCPColorMapData::alpha(int keyIndex, int valueIndex) {
  if (mAlpha && keyIndex >= 0 && keyIndex < mKeySize && valueIndex >= 0 &&
      valueIndex < mValueSize)
    return mAlpha[valueIndex * mKeySize + keyIndex];
  else
    return 255;
}

void QCPColorMapData::setSize(int keySize, int valueSize) {
  if (keySize != mKeySize || valueSize != mValueSize) {
    mKeySize = keySize;
    mValueSize = valueSize;
    if (mData)
      delete[] mData;
    mIsEmpty = mKeySize == 0 || mValueSize == 0;
    if (!mIsEmpty) {
#ifdef __EXCEPTIONS
      try {
#endif
        mData = new double[mKeySize * mValueSize];
#ifdef __EXCEPTIONS
      } catch (...) {
        mData = 0;
      }
#endif
      if (mData)
        fill(0);
      else
        qDebug() << Q_FUNC_INFO << "out of memory for data dimensions "
                 << mKeySize << "*" << mValueSize;
    } else
      mData = 0;

    if (mAlpha)

      createAlpha();

    mDataModified = true;
  }
}

void QCPColorMapData::setKeySize(int keySize) { setSize(keySize, mValueSize); }

void QCPColorMapData::setValueSize(int valueSize) {
  setSize(mKeySize, valueSize);
}

void QCPColorMapData::setRange(const QCPRange &keyRange,
                               const QCPRange &valueRange) {
  setKeyRange(keyRange);
  setValueRange(valueRange);
}

void QCPColorMapData::setKeyRange(const QCPRange &keyRange) {
  mKeyRange = keyRange;
}

void QCPColorMapData::setValueRange(const QCPRange &valueRange) {
  mValueRange = valueRange;
}

void QCPColorMapData::setData(double key, double value, double z) {
  int keyCell = (key - mKeyRange.lower) / (mKeyRange.upper - mKeyRange.lower) *
                    (mKeySize - 1) +
                0.5;
  int valueCell = (value - mValueRange.lower) /
                      (mValueRange.upper - mValueRange.lower) *
                      (mValueSize - 1) +
                  0.5;
  if (keyCell >= 0 && keyCell < mKeySize && valueCell >= 0 &&
      valueCell < mValueSize) {
    mData[valueCell * mKeySize + keyCell] = z;
    if (z < mDataBounds.lower)
      mDataBounds.lower = z;
    if (z > mDataBounds.upper)
      mDataBounds.upper = z;
    mDataModified = true;
  }
}

void QCPColorMapData::setCell(int keyIndex, int valueIndex, double z) {
  if (keyIndex >= 0 && keyIndex < mKeySize && valueIndex >= 0 &&
      valueIndex < mValueSize) {
    mData[valueIndex * mKeySize + keyIndex] = z;
    if (z < mDataBounds.lower)
      mDataBounds.lower = z;
    if (z > mDataBounds.upper)
      mDataBounds.upper = z;
    mDataModified = true;
  } else
    qDebug() << Q_FUNC_INFO << "index out of bounds:" << keyIndex << valueIndex;
}

void QCPColorMapData::setAlpha(int keyIndex, int valueIndex,
                               unsigned char alpha) {
  if (keyIndex >= 0 && keyIndex < mKeySize && valueIndex >= 0 &&
      valueIndex < mValueSize) {
    if (mAlpha || createAlpha()) {
      mAlpha[valueIndex * mKeySize + keyIndex] = alpha;
      mDataModified = true;
    }
  } else
    qDebug() << Q_FUNC_INFO << "index out of bounds:" << keyIndex << valueIndex;
}

void QCPColorMapData::recalculateDataBounds() {
  if (mKeySize > 0 && mValueSize > 0) {
    double minHeight = mData[0];
    double maxHeight = mData[0];
    const int dataCount = mValueSize * mKeySize;
    for (int i = 0; i < dataCount; ++i) {
      if (mData[i] > maxHeight)
        maxHeight = mData[i];
      if (mData[i] < minHeight)
        minHeight = mData[i];
    }
    mDataBounds.lower = minHeight;
    mDataBounds.upper = maxHeight;
  }
}

void QCPColorMapData::clear() { setSize(0, 0); }

void QCPColorMapData::clearAlpha() {
  if (mAlpha) {
    delete[] mAlpha;
    mAlpha = 0;
    mDataModified = true;
  }
}

void QCPColorMapData::fill(double z) {
  const int dataCount = mValueSize * mKeySize;
  for (int i = 0; i < dataCount; ++i)
    mData[i] = z;
  mDataBounds = QCPRange(z, z);
  mDataModified = true;
}

void QCPColorMapData::fillAlpha(unsigned char alpha) {
  if (mAlpha || createAlpha(false)) {
    const int dataCount = mValueSize * mKeySize;
    for (int i = 0; i < dataCount; ++i)
      mAlpha[i] = alpha;
    mDataModified = true;
  }
}

void QCPColorMapData::coordToCell(double key, double value, int *keyIndex,
                                  int *valueIndex) const {
  if (keyIndex)
    *keyIndex = (key - mKeyRange.lower) / (mKeyRange.upper - mKeyRange.lower) *
                    (mKeySize - 1) +
                0.5;
  if (valueIndex)
    *valueIndex = (value - mValueRange.lower) /
                      (mValueRange.upper - mValueRange.lower) *
                      (mValueSize - 1) +
                  0.5;
}

void QCPColorMapData::cellToCoord(int keyIndex, int valueIndex, double *key,
                                  double *value) const {
  if (key)
    *key = keyIndex / (double)(mKeySize - 1) *
               (mKeyRange.upper - mKeyRange.lower) +
           mKeyRange.lower;
  if (value)
    *value = valueIndex / (double)(mValueSize - 1) *
                 (mValueRange.upper - mValueRange.lower) +
             mValueRange.lower;
}

bool QCPColorMapData::createAlpha(bool initializeOpaque) {
  clearAlpha();
  if (isEmpty())
    return false;

#ifdef __EXCEPTIONS
  try {
#endif
    mAlpha = new unsigned char[mKeySize * mValueSize];
#ifdef __EXCEPTIONS
  } catch (...) {
    mAlpha = 0;
  }
#endif
  if (mAlpha) {
    if (initializeOpaque)
      fillAlpha(255);
    return true;
  } else {
    qDebug() << Q_FUNC_INFO << "out of memory for data dimensions " << mKeySize
             << "*" << mValueSize;
    return false;
  }
}

QCPColorMap::QCPColorMap(QCPAxis *keyAxis, QCPAxis *valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis),
      mDataScaleType(QCPAxis::stLinear),
      mMapData(new QCPColorMapData(10, 10, QCPRange(0, 5), QCPRange(0, 5))),
      mGradient(QCPColorGradient::gpCold), mInterpolate(true),
      mTightBoundary(false), mMapImageInvalidated(true) {}

QCPColorMap::~QCPColorMap() { delete mMapData; }

void QCPColorMap::setData(QCPColorMapData *data, bool copy) {
  if (mMapData == data) {
    qDebug() << Q_FUNC_INFO
             << "The data pointer is already in (and owned by) this plottable"
             << reinterpret_cast<quintptr>(data);
    return;
  }
  if (copy) {
    *mMapData = *data;
  } else {
    delete mMapData;
    mMapData = data;
  }
  mMapImageInvalidated = true;
}

void QCPColorMap::setDataRange(const QCPRange &dataRange) {
  if (!QCPRange::validRange(dataRange))
    return;
  if (mDataRange.lower != dataRange.lower ||
      mDataRange.upper != dataRange.upper) {
    if (mDataScaleType == QCPAxis::stLogarithmic)
      mDataRange = dataRange.sanitizedForLogScale();
    else
      mDataRange = dataRange.sanitizedForLinScale();
    mMapImageInvalidated = true;
    Q_EMIT dataRangeChanged(mDataRange);
  }
}

void QCPColorMap::setDataScaleType(QCPAxis::ScaleType scaleType) {
  if (mDataScaleType != scaleType) {
    mDataScaleType = scaleType;
    mMapImageInvalidated = true;
    Q_EMIT dataScaleTypeChanged(mDataScaleType);
    if (mDataScaleType == QCPAxis::stLogarithmic)
      setDataRange(mDataRange.sanitizedForLogScale());
  }
}

void QCPColorMap::setGradient(const QCPColorGradient &gradient) {
  if (mGradient != gradient) {
    mGradient = gradient;
    mMapImageInvalidated = true;
    Q_EMIT gradientChanged(mGradient);
  }
}

void QCPColorMap::setInterpolate(bool enabled) {
  mInterpolate = enabled;
  mMapImageInvalidated = true;
}

void QCPColorMap::setTightBoundary(bool enabled) { mTightBoundary = enabled; }

void QCPColorMap::setColorScale(QCPColorScale *colorScale) {
  if (mColorScale)

  {
    disconnect(this, SIGNAL(dataRangeChanged(QCPRange)), mColorScale.data(),
               SLOT(setDataRange(QCPRange)));
    disconnect(this, SIGNAL(dataScaleTypeChanged(QCPAxis::ScaleType)),
               mColorScale.data(), SLOT(setDataScaleType(QCPAxis::ScaleType)));
    disconnect(this, SIGNAL(gradientChanged(QCPColorGradient)),
               mColorScale.data(), SLOT(setGradient(QCPColorGradient)));
    disconnect(mColorScale.data(), SIGNAL(dataRangeChanged(QCPRange)), this,
               SLOT(setDataRange(QCPRange)));
    disconnect(mColorScale.data(), SIGNAL(gradientChanged(QCPColorGradient)),
               this, SLOT(setGradient(QCPColorGradient)));
    disconnect(mColorScale.data(),
               SIGNAL(dataScaleTypeChanged(QCPAxis::ScaleType)), this,
               SLOT(setDataScaleType(QCPAxis::ScaleType)));
  }
  mColorScale = colorScale;
  if (mColorScale)

  {
    setGradient(mColorScale.data()->gradient());
    setDataRange(mColorScale.data()->dataRange());
    setDataScaleType(mColorScale.data()->dataScaleType());
    connect(this, SIGNAL(dataRangeChanged(QCPRange)), mColorScale.data(),
            SLOT(setDataRange(QCPRange)));
    connect(this, SIGNAL(dataScaleTypeChanged(QCPAxis::ScaleType)),
            mColorScale.data(), SLOT(setDataScaleType(QCPAxis::ScaleType)));
    connect(this, SIGNAL(gradientChanged(QCPColorGradient)), mColorScale.data(),
            SLOT(setGradient(QCPColorGradient)));
    connect(mColorScale.data(), SIGNAL(dataRangeChanged(QCPRange)), this,
            SLOT(setDataRange(QCPRange)));
    connect(mColorScale.data(), SIGNAL(gradientChanged(QCPColorGradient)), this,
            SLOT(setGradient(QCPColorGradient)));
    connect(mColorScale.data(),
            SIGNAL(dataScaleTypeChanged(QCPAxis::ScaleType)), this,
            SLOT(setDataScaleType(QCPAxis::ScaleType)));
  }
}

void QCPColorMap::rescaleDataRange(bool recalculateDataBounds) {
  if (recalculateDataBounds)
    mMapData->recalculateDataBounds();
  setDataRange(mMapData->dataBounds());
}

void QCPColorMap::updateLegendIcon(Qt::TransformationMode transformMode,
                                   const QSize &thumbSize) {
  if (mMapImage.isNull() && !data()->isEmpty())
    updateMapImage();

  if (!mMapImage.isNull())

  {
    bool mirrorX =
        (keyAxis()->orientation() == Qt::Horizontal ? keyAxis() : valueAxis())
            ->rangeReversed();
    bool mirrorY =
        (valueAxis()->orientation() == Qt::Vertical ? valueAxis() : keyAxis())
            ->rangeReversed();
    mLegendIcon = QPixmap::fromImage(mMapImage.mirrored(mirrorX, mirrorY))
                      .scaled(thumbSize, Qt::KeepAspectRatio, transformMode);
  }
}

double QCPColorMap::selectTest(const QPointF &pos, bool onlySelectable,
                               QVariant *details) const {
  Q_UNUSED(details)
  if ((onlySelectable && mSelectable == QCP::stNone) || mMapData->isEmpty())
    return -1;
  if (!mKeyAxis || !mValueAxis)
    return -1;

  if (mKeyAxis.data()->axisRect()->rect().contains(pos.toPoint())) {
    double posKey, posValue;
    pixelsToCoords(pos, posKey, posValue);
    if (mMapData->keyRange().contains(posKey) &&
        mMapData->valueRange().contains(posValue)) {
      if (details)
        details->setValue(QCPDataSelection(QCPDataRange(0, 1)));

      return mParentPlot->selectionTolerance() * 0.99;
    }
  }
  return -1;
}

QCPRange QCPColorMap::getKeyRange(bool &foundRange,
                                  QCP::SignDomain inSignDomain) const {
  foundRange = true;
  QCPRange result = mMapData->keyRange();
  result.normalize();
  if (inSignDomain == QCP::sdPositive) {
    if (result.lower <= 0 && result.upper > 0)
      result.lower = result.upper * 1e-3;
    else if (result.lower <= 0 && result.upper <= 0)
      foundRange = false;
  } else if (inSignDomain == QCP::sdNegative) {
    if (result.upper >= 0 && result.lower < 0)
      result.upper = result.lower * 1e-3;
    else if (result.upper >= 0 && result.lower >= 0)
      foundRange = false;
  }
  return result;
}

QCPRange QCPColorMap::getValueRange(bool &foundRange,
                                    QCP::SignDomain inSignDomain,
                                    const QCPRange &inKeyRange) const {
  if (inKeyRange != QCPRange()) {
    if (mMapData->keyRange().upper < inKeyRange.lower ||
        mMapData->keyRange().lower > inKeyRange.upper) {
      foundRange = false;
      return QCPRange();
    }
  }

  foundRange = true;
  QCPRange result = mMapData->valueRange();
  result.normalize();
  if (inSignDomain == QCP::sdPositive) {
    if (result.lower <= 0 && result.upper > 0)
      result.lower = result.upper * 1e-3;
    else if (result.lower <= 0 && result.upper <= 0)
      foundRange = false;
  } else if (inSignDomain == QCP::sdNegative) {
    if (result.upper >= 0 && result.lower < 0)
      result.upper = result.lower * 1e-3;
    else if (result.upper >= 0 && result.lower >= 0)
      foundRange = false;
  }
  return result;
}

void QCPColorMap::updateMapImage() {
  QCPAxis *keyAxis = mKeyAxis.data();
  if (!keyAxis)
    return;
  if (mMapData->isEmpty())
    return;

  const QImage::Format format = QImage::Format_ARGB32_Premultiplied;
  const int keySize = mMapData->keySize();
  const int valueSize = mMapData->valueSize();
  int keyOversamplingFactor =
      mInterpolate ? 1 : (int)(1.0 + 100.0 / (double)keySize);

  int valueOversamplingFactor =
      mInterpolate ? 1 : (int)(1.0 + 100.0 / (double)valueSize);

  if (keyAxis->orientation() == Qt::Horizontal &&
      (mMapImage.width() != keySize * keyOversamplingFactor ||
       mMapImage.height() != valueSize * valueOversamplingFactor))
    mMapImage = QImage(QSize(keySize * keyOversamplingFactor,
                             valueSize * valueOversamplingFactor),
                       format);
  else if (keyAxis->orientation() == Qt::Vertical &&
           (mMapImage.width() != valueSize * valueOversamplingFactor ||
            mMapImage.height() != keySize * keyOversamplingFactor))
    mMapImage = QImage(QSize(valueSize * valueOversamplingFactor,
                             keySize * keyOversamplingFactor),
                       format);

  if (mMapImage.isNull()) {
    qDebug() << Q_FUNC_INFO
             << "Couldn't create map image (possibly too large for memory)";
    mMapImage = QImage(QSize(10, 10), format);
    mMapImage.fill(Qt::black);
  } else {
    QImage *localMapImage = &mMapImage;

    if (keyOversamplingFactor > 1 || valueOversamplingFactor > 1) {
      if (keyAxis->orientation() == Qt::Horizontal &&
          (mUndersampledMapImage.width() != keySize ||
           mUndersampledMapImage.height() != valueSize))
        mUndersampledMapImage = QImage(QSize(keySize, valueSize), format);
      else if (keyAxis->orientation() == Qt::Vertical &&
               (mUndersampledMapImage.width() != valueSize ||
                mUndersampledMapImage.height() != keySize))
        mUndersampledMapImage = QImage(QSize(valueSize, keySize), format);
      localMapImage = &mUndersampledMapImage;

    } else if (!mUndersampledMapImage.isNull())
      mUndersampledMapImage = QImage();

    const double *rawData = mMapData->mData;
    const unsigned char *rawAlpha = mMapData->mAlpha;
    if (keyAxis->orientation() == Qt::Horizontal) {
      const int lineCount = valueSize;
      const int rowCount = keySize;
      for (int line = 0; line < lineCount; ++line) {
        QRgb *pixels = reinterpret_cast<QRgb *>(
            localMapImage->scanLine(lineCount - 1 - line));

        if (rawAlpha)
          mGradient.colorize(
              rawData + line * rowCount, rawAlpha + line * rowCount, mDataRange,
              pixels, rowCount, 1, mDataScaleType == QCPAxis::stLogarithmic);
        else
          mGradient.colorize(rawData + line * rowCount, mDataRange, pixels,
                             rowCount, 1,
                             mDataScaleType == QCPAxis::stLogarithmic);
      }
    } else

    {
      const int lineCount = keySize;
      const int rowCount = valueSize;
      for (int line = 0; line < lineCount; ++line) {
        QRgb *pixels = reinterpret_cast<QRgb *>(
            localMapImage->scanLine(lineCount - 1 - line));

        if (rawAlpha)
          mGradient.colorize(rawData + line, rawAlpha + line, mDataRange,
                             pixels, rowCount, lineCount,
                             mDataScaleType == QCPAxis::stLogarithmic);
        else
          mGradient.colorize(rawData + line, mDataRange, pixels, rowCount,
                             lineCount,
                             mDataScaleType == QCPAxis::stLogarithmic);
      }
    }

    if (keyOversamplingFactor > 1 || valueOversamplingFactor > 1) {
      if (keyAxis->orientation() == Qt::Horizontal)
        mMapImage = mUndersampledMapImage.scaled(
            keySize * keyOversamplingFactor,
            valueSize * valueOversamplingFactor, Qt::IgnoreAspectRatio,
            Qt::FastTransformation);
      else
        mMapImage = mUndersampledMapImage.scaled(
            valueSize * valueOversamplingFactor,
            keySize * keyOversamplingFactor, Qt::IgnoreAspectRatio,
            Qt::FastTransformation);
    }
  }
  mMapData->mDataModified = false;
  mMapImageInvalidated = false;
}

void QCPColorMap::draw(QCPPainter *painter) {
  if (mMapData->isEmpty())
    return;
  if (!mKeyAxis || !mValueAxis)
    return;
  applyDefaultAntialiasingHint(painter);

  if (mMapData->mDataModified || mMapImageInvalidated)
    updateMapImage();

  const bool useBuffer = painter->modes().testFlag(QCPPainter::pmVectorized);
  QCPPainter *localPainter = painter;

  QRectF mapBufferTarget;

  QPixmap mapBuffer;
  if (useBuffer) {
    const double mapBufferPixelRatio = 3;

    mapBufferTarget = painter->clipRegion().boundingRect();
    mapBuffer =
        QPixmap((mapBufferTarget.size() * mapBufferPixelRatio).toSize());
    mapBuffer.fill(Qt::transparent);
    localPainter = new QCPPainter(&mapBuffer);
    localPainter->scale(mapBufferPixelRatio, mapBufferPixelRatio);
    localPainter->translate(-mapBufferTarget.topLeft());
  }

  QRectF imageRect = QRectF(coordsToPixels(mMapData->keyRange().lower,
                                           mMapData->valueRange().lower),
                            coordsToPixels(mMapData->keyRange().upper,
                                           mMapData->valueRange().upper))
                         .normalized();

  double halfCellWidth = 0;

  double halfCellHeight = 0;

  if (keyAxis()->orientation() == Qt::Horizontal) {
    if (mMapData->keySize() > 1)
      halfCellWidth =
          0.5 * imageRect.width() / (double)(mMapData->keySize() - 1);
    if (mMapData->valueSize() > 1)
      halfCellHeight =
          0.5 * imageRect.height() / (double)(mMapData->valueSize() - 1);
  } else

  {
    if (mMapData->keySize() > 1)
      halfCellHeight =
          0.5 * imageRect.height() / (double)(mMapData->keySize() - 1);
    if (mMapData->valueSize() > 1)
      halfCellWidth =
          0.5 * imageRect.width() / (double)(mMapData->valueSize() - 1);
  }
  imageRect.adjust(-halfCellWidth, -halfCellHeight, halfCellWidth,
                   halfCellHeight);
  const bool mirrorX =
      (keyAxis()->orientation() == Qt::Horizontal ? keyAxis() : valueAxis())
          ->rangeReversed();
  const bool mirrorY =
      (valueAxis()->orientation() == Qt::Vertical ? valueAxis() : keyAxis())
          ->rangeReversed();
  const bool smoothBackup =
      localPainter->renderHints().testFlag(QPainter::SmoothPixmapTransform);
  localPainter->setRenderHint(QPainter::SmoothPixmapTransform, mInterpolate);
  QRegion clipBackup;
  if (mTightBoundary) {
    clipBackup = localPainter->clipRegion();
    QRectF tightClipRect = QRectF(coordsToPixels(mMapData->keyRange().lower,
                                                 mMapData->valueRange().lower),
                                  coordsToPixels(mMapData->keyRange().upper,
                                                 mMapData->valueRange().upper))
                               .normalized();
    localPainter->setClipRect(tightClipRect, Qt::IntersectClip);
  }
  localPainter->drawImage(imageRect, mMapImage.mirrored(mirrorX, mirrorY));
  if (mTightBoundary)
    localPainter->setClipRegion(clipBackup);
  localPainter->setRenderHint(QPainter::SmoothPixmapTransform, smoothBackup);

  if (useBuffer)

  {
    delete localPainter;
    painter->drawPixmap(mapBufferTarget.toRect(), mapBuffer);
  }
}

void QCPColorMap::drawLegendIcon(QCPPainter *painter,
                                 const QRectF &rect) const {
  applyDefaultAntialiasingHint(painter);

  if (!mLegendIcon.isNull()) {
    QPixmap scaledIcon = mLegendIcon.scaled(
        rect.size().toSize(), Qt::KeepAspectRatio, Qt::FastTransformation);
    QRectF iconRect = QRectF(0, 0, scaledIcon.width(), scaledIcon.height());
    iconRect.moveCenter(rect.center());
    painter->drawPixmap(iconRect.topLeft(), scaledIcon);
  }
}

QCPFinancialData::QCPFinancialData()
    : key(0), open(0), high(0), low(0), close(0) {}

QCPFinancialData::QCPFinancialData(double key, double open, double high,
                                   double low, double close)
    : key(key), open(open), high(high), low(low), close(close) {}

QCPFinancial::QCPFinancial(QCPAxis *keyAxis, QCPAxis *valueAxis)
    : QCPAbstractPlottable1D<QCPFinancialData>(keyAxis, valueAxis),
      mChartStyle(csCandlestick), mWidth(0.5), mWidthType(wtPlotCoords),
      mTwoColored(true), mBrushPositive(QBrush(QColor(50, 160, 0))),
      mBrushNegative(QBrush(QColor(180, 0, 15))),
      mPenPositive(QPen(QColor(40, 150, 0))),
      mPenNegative(QPen(QColor(170, 5, 5))) {
  mSelectionDecorator->setBrush(QBrush(QColor(160, 160, 255)));
}

QCPFinancial::~QCPFinancial() {}

void QCPFinancial::setData(QSharedPointer<QCPFinancialDataContainer> data) {
  mDataContainer = data;
}

void QCPFinancial::setData(const QVector<double> &keys,
                           const QVector<double> &open,
                           const QVector<double> &high,
                           const QVector<double> &low,
                           const QVector<double> &close, bool alreadySorted) {
  mDataContainer->clear();
  addData(keys, open, high, low, close, alreadySorted);
}

void QCPFinancial::setChartStyle(QCPFinancial::ChartStyle style) {
  mChartStyle = style;
}

void QCPFinancial::setWidth(double width) { mWidth = width; }

void QCPFinancial::setWidthType(QCPFinancial::WidthType widthType) {
  mWidthType = widthType;
}

void QCPFinancial::setTwoColored(bool twoColored) { mTwoColored = twoColored; }

void QCPFinancial::setBrushPositive(const QBrush &brush) {
  mBrushPositive = brush;
}

void QCPFinancial::setBrushNegative(const QBrush &brush) {
  mBrushNegative = brush;
}

void QCPFinancial::setPenPositive(const QPen &pen) { mPenPositive = pen; }

void QCPFinancial::setPenNegative(const QPen &pen) { mPenNegative = pen; }

void QCPFinancial::addData(const QVector<double> &keys,
                           const QVector<double> &open,
                           const QVector<double> &high,
                           const QVector<double> &low,
                           const QVector<double> &close, bool alreadySorted) {
  if (keys.size() != open.size() || open.size() != high.size() ||
      high.size() != low.size() || low.size() != close.size() ||
      close.size() != keys.size())
    qDebug() << Q_FUNC_INFO
             << "keys, open, high, low, close have different sizes:"
             << keys.size() << open.size() << high.size() << low.size()
             << close.size();
  const int n = qMin(
      keys.size(),
      qMin(open.size(), qMin(high.size(), qMin(low.size(), close.size()))));
  QVector<QCPFinancialData> tempData(n);
  QVector<QCPFinancialData>::iterator it = tempData.begin();
  const QVector<QCPFinancialData>::iterator itEnd = tempData.end();
  int i = 0;
  while (it != itEnd) {
    it->key = keys[i];
    it->open = open[i];
    it->high = high[i];
    it->low = low[i];
    it->close = close[i];
    ++it;
    ++i;
  }
  mDataContainer->add(tempData, alreadySorted);
}

void QCPFinancial::addData(double key, double open, double high, double low,
                           double close) {
  mDataContainer->add(QCPFinancialData(key, open, high, low, close));
}

QCPDataSelection QCPFinancial::selectTestRect(const QRectF &rect,
                                              bool onlySelectable) const {
  QCPDataSelection result;
  if ((onlySelectable && mSelectable == QCP::stNone) ||
      mDataContainer->isEmpty())
    return result;
  if (!mKeyAxis || !mValueAxis)
    return result;

  QCPFinancialDataContainer::const_iterator visibleBegin, visibleEnd;
  getVisibleDataBounds(visibleBegin, visibleEnd);

  for (QCPFinancialDataContainer::const_iterator it = visibleBegin;
       it != visibleEnd; ++it) {
    if (rect.intersects(selectionHitBox(it)))
      result.addDataRange(QCPDataRange(it - mDataContainer->constBegin(),
                                       it - mDataContainer->constBegin() + 1),
                          false);
  }
  result.simplify();
  return result;
}

double QCPFinancial::selectTest(const QPointF &pos, bool onlySelectable,
                                QVariant *details) const {
  Q_UNUSED(details)
  if ((onlySelectable && mSelectable == QCP::stNone) ||
      mDataContainer->isEmpty())
    return -1;
  if (!mKeyAxis || !mValueAxis)
    return -1;

  if (mKeyAxis.data()->axisRect()->rect().contains(pos.toPoint())) {
    QCPFinancialDataContainer::const_iterator visibleBegin, visibleEnd;
    QCPFinancialDataContainer::const_iterator closestDataPoint =
        mDataContainer->constEnd();
    getVisibleDataBounds(visibleBegin, visibleEnd);

    double result = -1;
    switch (mChartStyle) {
    case QCPFinancial::csOhlc:
      result = ohlcSelectTest(pos, visibleBegin, visibleEnd, closestDataPoint);
      break;
    case QCPFinancial::csCandlestick:
      result = candlestickSelectTest(pos, visibleBegin, visibleEnd,
                                     closestDataPoint);
      break;
    }
    if (details) {
      int pointIndex = closestDataPoint - mDataContainer->constBegin();
      details->setValue(
          QCPDataSelection(QCPDataRange(pointIndex, pointIndex + 1)));
    }
    return result;
  }

  return -1;
}

QCPRange QCPFinancial::getKeyRange(bool &foundRange,
                                   QCP::SignDomain inSignDomain) const {
  QCPRange range = mDataContainer->keyRange(foundRange, inSignDomain);

  if (foundRange) {
    if (inSignDomain != QCP::sdPositive || range.lower - mWidth * 0.5 > 0)
      range.lower -= mWidth * 0.5;
    if (inSignDomain != QCP::sdNegative || range.upper + mWidth * 0.5 < 0)
      range.upper += mWidth * 0.5;
  }
  return range;
}

QCPRange QCPFinancial::getValueRange(bool &foundRange,
                                     QCP::SignDomain inSignDomain,
                                     const QCPRange &inKeyRange) const {
  return mDataContainer->valueRange(foundRange, inSignDomain, inKeyRange);
}

QCPFinancialDataContainer
QCPFinancial::timeSeriesToOhlc(const QVector<double> &time,
                               const QVector<double> &value, double timeBinSize,
                               double timeBinOffset) {
  QCPFinancialDataContainer data;
  int count = qMin(time.size(), value.size());
  if (count == 0)
    return QCPFinancialDataContainer();

  QCPFinancialData currentBinData(0, value.first(), value.first(),
                                  value.first(), value.first());
  int currentBinIndex =
      qFloor((time.first() - timeBinOffset) / timeBinSize + 0.5);
  for (int i = 0; i < count; ++i) {
    int index = qFloor((time.at(i) - timeBinOffset) / timeBinSize + 0.5);
    if (currentBinIndex == index)

    {
      if (value.at(i) < currentBinData.low)
        currentBinData.low = value.at(i);
      if (value.at(i) > currentBinData.high)
        currentBinData.high = value.at(i);
      if (i == count - 1)

      {
        currentBinData.close = value.at(i);
        currentBinData.key = timeBinOffset + (index)*timeBinSize;
        data.add(currentBinData);
      }
    } else

    {
      currentBinData.close = value.at(i - 1);
      currentBinData.key = timeBinOffset + (index - 1) * timeBinSize;
      data.add(currentBinData);

      currentBinIndex = index;
      currentBinData.open = value.at(i);
      currentBinData.high = value.at(i);
      currentBinData.low = value.at(i);
    }
  }

  return data;
}

void QCPFinancial::draw(QCPPainter *painter) {
  QCPFinancialDataContainer::const_iterator visibleBegin, visibleEnd;
  getVisibleDataBounds(visibleBegin, visibleEnd);

  QList<QCPDataRange> selectedSegments, unselectedSegments, allSegments;
  getDataSegments(selectedSegments, unselectedSegments);
  allSegments << unselectedSegments << selectedSegments;
  for (int i = 0; i < allSegments.size(); ++i) {
    bool isSelectedSegment = i >= unselectedSegments.size();
    QCPFinancialDataContainer::const_iterator begin = visibleBegin;
    QCPFinancialDataContainer::const_iterator end = visibleEnd;
    mDataContainer->limitIteratorsToDataRange(begin, end, allSegments.at(i));
    if (begin == end)
      continue;

    switch (mChartStyle) {
    case QCPFinancial::csOhlc:
      drawOhlcPlot(painter, begin, end, isSelectedSegment);
      break;
    case QCPFinancial::csCandlestick:
      drawCandlestickPlot(painter, begin, end, isSelectedSegment);
      break;
    }
  }

  if (mSelectionDecorator)
    mSelectionDecorator->drawDecoration(painter, selection());
}

void QCPFinancial::drawLegendIcon(QCPPainter *painter,
                                  const QRectF &rect) const {
  painter->setAntialiasing(false);

  if (mChartStyle == csOhlc) {
    if (mTwoColored) {
      painter->setBrush(mBrushPositive);
      painter->setPen(mPenPositive);
      painter->setClipRegion(QRegion(QPolygon() << rect.bottomLeft().toPoint()
                                                << rect.topRight().toPoint()
                                                << rect.topLeft().toPoint()));
      painter->drawLine(
          QLineF(0, rect.height() * 0.5, rect.width(), rect.height() * 0.5)
              .translated(rect.topLeft()));
      painter->drawLine(QLineF(rect.width() * 0.2, rect.height() * 0.3,
                               rect.width() * 0.2, rect.height() * 0.5)
                            .translated(rect.topLeft()));
      painter->drawLine(QLineF(rect.width() * 0.8, rect.height() * 0.5,
                               rect.width() * 0.8, rect.height() * 0.7)
                            .translated(rect.topLeft()));

      painter->setBrush(mBrushNegative);
      painter->setPen(mPenNegative);
      painter->setClipRegion(QRegion(
          QPolygon() << rect.bottomLeft().toPoint() << rect.topRight().toPoint()
                     << rect.bottomRight().toPoint()));
      painter->drawLine(
          QLineF(0, rect.height() * 0.5, rect.width(), rect.height() * 0.5)
              .translated(rect.topLeft()));
      painter->drawLine(QLineF(rect.width() * 0.2, rect.height() * 0.3,
                               rect.width() * 0.2, rect.height() * 0.5)
                            .translated(rect.topLeft()));
      painter->drawLine(QLineF(rect.width() * 0.8, rect.height() * 0.5,
                               rect.width() * 0.8, rect.height() * 0.7)
                            .translated(rect.topLeft()));
    } else {
      painter->setBrush(mBrush);
      painter->setPen(mPen);
      painter->drawLine(
          QLineF(0, rect.height() * 0.5, rect.width(), rect.height() * 0.5)
              .translated(rect.topLeft()));
      painter->drawLine(QLineF(rect.width() * 0.2, rect.height() * 0.3,
                               rect.width() * 0.2, rect.height() * 0.5)
                            .translated(rect.topLeft()));
      painter->drawLine(QLineF(rect.width() * 0.8, rect.height() * 0.5,
                               rect.width() * 0.8, rect.height() * 0.7)
                            .translated(rect.topLeft()));
    }
  } else if (mChartStyle == csCandlestick) {
    if (mTwoColored) {
      painter->setBrush(mBrushPositive);
      painter->setPen(mPenPositive);
      painter->setClipRegion(QRegion(QPolygon() << rect.bottomLeft().toPoint()
                                                << rect.topRight().toPoint()
                                                << rect.topLeft().toPoint()));
      painter->drawLine(QLineF(0, rect.height() * 0.5, rect.width() * 0.25,
                               rect.height() * 0.5)
                            .translated(rect.topLeft()));
      painter->drawLine(QLineF(rect.width() * 0.75, rect.height() * 0.5,
                               rect.width(), rect.height() * 0.5)
                            .translated(rect.topLeft()));
      painter->drawRect(QRectF(rect.width() * 0.25, rect.height() * 0.25,
                               rect.width() * 0.5, rect.height() * 0.5)
                            .translated(rect.topLeft()));

      painter->setBrush(mBrushNegative);
      painter->setPen(mPenNegative);
      painter->setClipRegion(QRegion(
          QPolygon() << rect.bottomLeft().toPoint() << rect.topRight().toPoint()
                     << rect.bottomRight().toPoint()));
      painter->drawLine(QLineF(0, rect.height() * 0.5, rect.width() * 0.25,
                               rect.height() * 0.5)
                            .translated(rect.topLeft()));
      painter->drawLine(QLineF(rect.width() * 0.75, rect.height() * 0.5,
                               rect.width(), rect.height() * 0.5)
                            .translated(rect.topLeft()));
      painter->drawRect(QRectF(rect.width() * 0.25, rect.height() * 0.25,
                               rect.width() * 0.5, rect.height() * 0.5)
                            .translated(rect.topLeft()));
    } else {
      painter->setBrush(mBrush);
      painter->setPen(mPen);
      painter->drawLine(QLineF(0, rect.height() * 0.5, rect.width() * 0.25,
                               rect.height() * 0.5)
                            .translated(rect.topLeft()));
      painter->drawLine(QLineF(rect.width() * 0.75, rect.height() * 0.5,
                               rect.width(), rect.height() * 0.5)
                            .translated(rect.topLeft()));
      painter->drawRect(QRectF(rect.width() * 0.25, rect.height() * 0.25,
                               rect.width() * 0.5, rect.height() * 0.5)
                            .translated(rect.topLeft()));
    }
  }
}

void QCPFinancial::drawOhlcPlot(
    QCPPainter *painter, const QCPFinancialDataContainer::const_iterator &begin,
    const QCPFinancialDataContainer::const_iterator &end, bool isSelected) {
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return;
  }

  if (keyAxis->orientation() == Qt::Horizontal) {
    for (QCPFinancialDataContainer::const_iterator it = begin; it != end;
         ++it) {
      if (isSelected && mSelectionDecorator)
        mSelectionDecorator->applyPen(painter);
      else if (mTwoColored)
        painter->setPen(it->close >= it->open ? mPenPositive : mPenNegative);
      else
        painter->setPen(mPen);
      double keyPixel = keyAxis->coordToPixel(it->key);
      double openPixel = valueAxis->coordToPixel(it->open);
      double closePixel = valueAxis->coordToPixel(it->close);

      painter->drawLine(QPointF(keyPixel, valueAxis->coordToPixel(it->high)),
                        QPointF(keyPixel, valueAxis->coordToPixel(it->low)));

      double pixelWidth = getPixelWidth(it->key, keyPixel);

      painter->drawLine(QPointF(keyPixel - pixelWidth, openPixel),
                        QPointF(keyPixel, openPixel));

      painter->drawLine(QPointF(keyPixel, closePixel),
                        QPointF(keyPixel + pixelWidth, closePixel));
    }
  } else {
    for (QCPFinancialDataContainer::const_iterator it = begin; it != end;
         ++it) {
      if (isSelected && mSelectionDecorator)
        mSelectionDecorator->applyPen(painter);
      else if (mTwoColored)
        painter->setPen(it->close >= it->open ? mPenPositive : mPenNegative);
      else
        painter->setPen(mPen);
      double keyPixel = keyAxis->coordToPixel(it->key);
      double openPixel = valueAxis->coordToPixel(it->open);
      double closePixel = valueAxis->coordToPixel(it->close);

      painter->drawLine(QPointF(valueAxis->coordToPixel(it->high), keyPixel),
                        QPointF(valueAxis->coordToPixel(it->low), keyPixel));

      double pixelWidth = getPixelWidth(it->key, keyPixel);

      painter->drawLine(QPointF(openPixel, keyPixel - pixelWidth),
                        QPointF(openPixel, keyPixel));

      painter->drawLine(QPointF(closePixel, keyPixel),
                        QPointF(closePixel, keyPixel + pixelWidth));
    }
  }
}

void QCPFinancial::drawCandlestickPlot(
    QCPPainter *painter, const QCPFinancialDataContainer::const_iterator &begin,
    const QCPFinancialDataContainer::const_iterator &end, bool isSelected) {
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return;
  }

  if (keyAxis->orientation() == Qt::Horizontal) {
    for (QCPFinancialDataContainer::const_iterator it = begin; it != end;
         ++it) {
      if (isSelected && mSelectionDecorator) {
        mSelectionDecorator->applyPen(painter);
        mSelectionDecorator->applyBrush(painter);
      } else if (mTwoColored) {
        painter->setPen(it->close >= it->open ? mPenPositive : mPenNegative);
        painter->setBrush(it->close >= it->open ? mBrushPositive
                                                : mBrushNegative);
      } else {
        painter->setPen(mPen);
        painter->setBrush(mBrush);
      }
      double keyPixel = keyAxis->coordToPixel(it->key);
      double openPixel = valueAxis->coordToPixel(it->open);
      double closePixel = valueAxis->coordToPixel(it->close);

      painter->drawLine(QPointF(keyPixel, valueAxis->coordToPixel(it->high)),
                        QPointF(keyPixel, valueAxis->coordToPixel(
                                              qMax(it->open, it->close))));

      painter->drawLine(QPointF(keyPixel, valueAxis->coordToPixel(it->low)),
                        QPointF(keyPixel, valueAxis->coordToPixel(
                                              qMin(it->open, it->close))));

      double pixelWidth = getPixelWidth(it->key, keyPixel);
      painter->drawRect(QRectF(QPointF(keyPixel - pixelWidth, closePixel),
                               QPointF(keyPixel + pixelWidth, openPixel)));
    }
  } else

  {
    for (QCPFinancialDataContainer::const_iterator it = begin; it != end;
         ++it) {
      if (isSelected && mSelectionDecorator) {
        mSelectionDecorator->applyPen(painter);
        mSelectionDecorator->applyBrush(painter);
      } else if (mTwoColored) {
        painter->setPen(it->close >= it->open ? mPenPositive : mPenNegative);
        painter->setBrush(it->close >= it->open ? mBrushPositive
                                                : mBrushNegative);
      } else {
        painter->setPen(mPen);
        painter->setBrush(mBrush);
      }
      double keyPixel = keyAxis->coordToPixel(it->key);
      double openPixel = valueAxis->coordToPixel(it->open);
      double closePixel = valueAxis->coordToPixel(it->close);

      painter->drawLine(
          QPointF(valueAxis->coordToPixel(it->high), keyPixel),
          QPointF(valueAxis->coordToPixel(qMax(it->open, it->close)),
                  keyPixel));

      painter->drawLine(
          QPointF(valueAxis->coordToPixel(it->low), keyPixel),
          QPointF(valueAxis->coordToPixel(qMin(it->open, it->close)),
                  keyPixel));

      double pixelWidth = getPixelWidth(it->key, keyPixel);
      painter->drawRect(QRectF(QPointF(closePixel, keyPixel - pixelWidth),
                               QPointF(openPixel, keyPixel + pixelWidth)));
    }
  }
}

double QCPFinancial::getPixelWidth(double key, double keyPixel) const {
  double result = 0;
  switch (mWidthType) {
  case wtAbsolute: {
    if (mKeyAxis)
      result = mWidth * 0.5 * mKeyAxis.data()->pixelOrientation();
    break;
  }
  case wtAxisRectRatio: {
    if (mKeyAxis && mKeyAxis.data()->axisRect()) {
      if (mKeyAxis.data()->orientation() == Qt::Horizontal)
        result = mKeyAxis.data()->axisRect()->width() * mWidth * 0.5 *
                 mKeyAxis.data()->pixelOrientation();
      else
        result = mKeyAxis.data()->axisRect()->height() * mWidth * 0.5 *
                 mKeyAxis.data()->pixelOrientation();
    } else
      qDebug() << Q_FUNC_INFO << "No key axis or axis rect defined";
    break;
  }
  case wtPlotCoords: {
    if (mKeyAxis)
      result = mKeyAxis.data()->coordToPixel(key + mWidth * 0.5) - keyPixel;
    else
      qDebug() << Q_FUNC_INFO << "No key axis defined";
    break;
  }
  }
  return result;
}

double QCPFinancial::ohlcSelectTest(
    const QPointF &pos, const QCPFinancialDataContainer::const_iterator &begin,
    const QCPFinancialDataContainer::const_iterator &end,
    QCPFinancialDataContainer::const_iterator &closestDataPoint) const {
  closestDataPoint = mDataContainer->constEnd();
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return -1;
  }

  double minDistSqr = (std::numeric_limits<double>::max)();
  if (keyAxis->orientation() == Qt::Horizontal) {
    for (QCPFinancialDataContainer::const_iterator it = begin; it != end;
         ++it) {
      double keyPixel = keyAxis->coordToPixel(it->key);

      double currentDistSqr = QCPVector2D(pos).distanceSquaredToLine(
          QCPVector2D(keyPixel, valueAxis->coordToPixel(it->high)),
          QCPVector2D(keyPixel, valueAxis->coordToPixel(it->low)));
      if (currentDistSqr < minDistSqr) {
        minDistSqr = currentDistSqr;
        closestDataPoint = it;
      }
    }
  } else

  {
    for (QCPFinancialDataContainer::const_iterator it = begin; it != end;
         ++it) {
      double keyPixel = keyAxis->coordToPixel(it->key);

      double currentDistSqr = QCPVector2D(pos).distanceSquaredToLine(
          QCPVector2D(valueAxis->coordToPixel(it->high), keyPixel),
          QCPVector2D(valueAxis->coordToPixel(it->low), keyPixel));
      if (currentDistSqr < minDistSqr) {
        minDistSqr = currentDistSqr;
        closestDataPoint = it;
      }
    }
  }
  return qSqrt(minDistSqr);
}

double QCPFinancial::candlestickSelectTest(
    const QPointF &pos, const QCPFinancialDataContainer::const_iterator &begin,
    const QCPFinancialDataContainer::const_iterator &end,
    QCPFinancialDataContainer::const_iterator &closestDataPoint) const {
  closestDataPoint = mDataContainer->constEnd();
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return -1;
  }

  double minDistSqr = (std::numeric_limits<double>::max)();
  if (keyAxis->orientation() == Qt::Horizontal) {
    for (QCPFinancialDataContainer::const_iterator it = begin; it != end;
         ++it) {
      double currentDistSqr;

      QCPRange boxKeyRange(it->key - mWidth * 0.5, it->key + mWidth * 0.5);
      QCPRange boxValueRange(it->close, it->open);
      double posKey, posValue;
      pixelsToCoords(pos, posKey, posValue);
      if (boxKeyRange.contains(posKey) && boxValueRange.contains(posValue))

      {
        currentDistSqr = mParentPlot->selectionTolerance() * 0.99 *
                         mParentPlot->selectionTolerance() * 0.99;
      } else {
        double keyPixel = keyAxis->coordToPixel(it->key);
        double highLineDistSqr = QCPVector2D(pos).distanceSquaredToLine(
            QCPVector2D(keyPixel, valueAxis->coordToPixel(it->high)),
            QCPVector2D(keyPixel,
                        valueAxis->coordToPixel(qMax(it->open, it->close))));
        double lowLineDistSqr = QCPVector2D(pos).distanceSquaredToLine(
            QCPVector2D(keyPixel, valueAxis->coordToPixel(it->low)),
            QCPVector2D(keyPixel,
                        valueAxis->coordToPixel(qMin(it->open, it->close))));
        currentDistSqr = qMin(highLineDistSqr, lowLineDistSqr);
      }
      if (currentDistSqr < minDistSqr) {
        minDistSqr = currentDistSqr;
        closestDataPoint = it;
      }
    }
  } else

  {
    for (QCPFinancialDataContainer::const_iterator it = begin; it != end;
         ++it) {
      double currentDistSqr;

      QCPRange boxKeyRange(it->key - mWidth * 0.5, it->key + mWidth * 0.5);
      QCPRange boxValueRange(it->close, it->open);
      double posKey, posValue;
      pixelsToCoords(pos, posKey, posValue);
      if (boxKeyRange.contains(posKey) && boxValueRange.contains(posValue))

      {
        currentDistSqr = mParentPlot->selectionTolerance() * 0.99 *
                         mParentPlot->selectionTolerance() * 0.99;
      } else {
        double keyPixel = keyAxis->coordToPixel(it->key);
        double highLineDistSqr = QCPVector2D(pos).distanceSquaredToLine(
            QCPVector2D(valueAxis->coordToPixel(it->high), keyPixel),
            QCPVector2D(valueAxis->coordToPixel(qMax(it->open, it->close)),
                        keyPixel));
        double lowLineDistSqr = QCPVector2D(pos).distanceSquaredToLine(
            QCPVector2D(valueAxis->coordToPixel(it->low), keyPixel),
            QCPVector2D(valueAxis->coordToPixel(qMin(it->open, it->close)),
                        keyPixel));
        currentDistSqr = qMin(highLineDistSqr, lowLineDistSqr);
      }
      if (currentDistSqr < minDistSqr) {
        minDistSqr = currentDistSqr;
        closestDataPoint = it;
      }
    }
  }
  return qSqrt(minDistSqr);
}

void QCPFinancial::getVisibleDataBounds(
    QCPFinancialDataContainer::const_iterator &begin,
    QCPFinancialDataContainer::const_iterator &end) const {
  if (!mKeyAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key axis";
    begin = mDataContainer->constEnd();
    end = mDataContainer->constEnd();
    return;
  }
  begin =
      mDataContainer->findBegin(mKeyAxis.data()->range().lower - mWidth * 0.5);

  end = mDataContainer->findEnd(mKeyAxis.data()->range().upper + mWidth * 0.5);
}

QRectF QCPFinancial::selectionHitBox(
    QCPFinancialDataContainer::const_iterator it) const {
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return QRectF();
  }

  double keyPixel = keyAxis->coordToPixel(it->key);
  double highPixel = valueAxis->coordToPixel(it->high);
  double lowPixel = valueAxis->coordToPixel(it->low);
  double keyWidthPixels =
      keyPixel - keyAxis->coordToPixel(it->key - mWidth * 0.5);
  if (keyAxis->orientation() == Qt::Horizontal)
    return QRectF(keyPixel - keyWidthPixels, highPixel, keyWidthPixels * 2,
                  lowPixel - highPixel)
        .normalized();
  else
    return QRectF(highPixel, keyPixel - keyWidthPixels, lowPixel - highPixel,
                  keyWidthPixels * 2)
        .normalized();
}

QCPErrorBarsData::QCPErrorBarsData() : errorMinus(0), errorPlus(0) {}

QCPErrorBarsData::QCPErrorBarsData(double error)
    : errorMinus(error), errorPlus(error) {}

QCPErrorBarsData::QCPErrorBarsData(double errorMinus, double errorPlus)
    : errorMinus(errorMinus), errorPlus(errorPlus) {}

QCPErrorBars::QCPErrorBars(QCPAxis *keyAxis, QCPAxis *valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis),
      mDataContainer(new QVector<QCPErrorBarsData>), mErrorType(etValueError),
      mWhiskerWidth(9), mSymbolGap(10) {
  setPen(QPen(Qt::black, 0));
  setBrush(Qt::NoBrush);
}

QCPErrorBars::~QCPErrorBars() {}

void QCPErrorBars::setData(QSharedPointer<QCPErrorBarsDataContainer> data) {
  mDataContainer = data;
}

void QCPErrorBars::setData(const QVector<double> &error) {
  mDataContainer->clear();
  addData(error);
}

void QCPErrorBars::setData(const QVector<double> &errorMinus,
                           const QVector<double> &errorPlus) {
  mDataContainer->clear();
  addData(errorMinus, errorPlus);
}

void QCPErrorBars::setDataPlottable(QCPAbstractPlottable *plottable) {
  if (plottable && qobject_cast<QCPErrorBars *>(plottable)) {
    mDataPlottable = 0;
    qDebug() << Q_FUNC_INFO
             << "can't set another QCPErrorBars instance as data plottable";
    return;
  }
  if (plottable && !plottable->interface1D()) {
    mDataPlottable = 0;
    qDebug() << Q_FUNC_INFO
             << "passed plottable doesn't implement 1d interface, can't "
                "associate with QCPErrorBars";
    return;
  }

  mDataPlottable = plottable;
}

void QCPErrorBars::setErrorType(ErrorType type) { mErrorType = type; }

void QCPErrorBars::setWhiskerWidth(double pixels) { mWhiskerWidth = pixels; }

void QCPErrorBars::setSymbolGap(double pixels) { mSymbolGap = pixels; }

void QCPErrorBars::addData(const QVector<double> &error) {
  addData(error, error);
}

void QCPErrorBars::addData(const QVector<double> &errorMinus,
                           const QVector<double> &errorPlus) {
  if (errorMinus.size() != errorPlus.size())
    qDebug() << Q_FUNC_INFO
             << "minus and plus error vectors have different sizes:"
             << errorMinus.size() << errorPlus.size();
  const int n = qMin(errorMinus.size(), errorPlus.size());
  mDataContainer->reserve(n);
  for (int i = 0; i < n; ++i)
    mDataContainer->append(QCPErrorBarsData(errorMinus.at(i), errorPlus.at(i)));
}

void QCPErrorBars::addData(double error) {
  mDataContainer->append(QCPErrorBarsData(error));
}

void QCPErrorBars::addData(double errorMinus, double errorPlus) {
  mDataContainer->append(QCPErrorBarsData(errorMinus, errorPlus));
}

int QCPErrorBars::dataCount() const { return mDataContainer->size(); }

double QCPErrorBars::dataMainKey(int index) const {
  if (mDataPlottable)
    return mDataPlottable->interface1D()->dataMainKey(index);
  else
    qDebug() << Q_FUNC_INFO << "no data plottable set";
  return 0;
}

double QCPErrorBars::dataSortKey(int index) const {
  if (mDataPlottable)
    return mDataPlottable->interface1D()->dataSortKey(index);
  else
    qDebug() << Q_FUNC_INFO << "no data plottable set";
  return 0;
}

double QCPErrorBars::dataMainValue(int index) const {
  if (mDataPlottable)
    return mDataPlottable->interface1D()->dataMainValue(index);
  else
    qDebug() << Q_FUNC_INFO << "no data plottable set";
  return 0;
}

QCPRange QCPErrorBars::dataValueRange(int index) const {
  if (mDataPlottable) {
    const double value = mDataPlottable->interface1D()->dataMainValue(index);
    if (index >= 0 && index < mDataContainer->size() &&
        mErrorType == etValueError)
      return QCPRange(value - mDataContainer->at(index).errorMinus,
                      value + mDataContainer->at(index).errorPlus);
    else
      return QCPRange(value, value);
  } else {
    qDebug() << Q_FUNC_INFO << "no data plottable set";
    return QCPRange();
  }
}

QPointF QCPErrorBars::dataPixelPosition(int index) const {
  if (mDataPlottable)
    return mDataPlottable->interface1D()->dataPixelPosition(index);
  else
    qDebug() << Q_FUNC_INFO << "no data plottable set";
  return QPointF();
}

bool QCPErrorBars::sortKeyIsMainKey() const {
  if (mDataPlottable) {
    return mDataPlottable->interface1D()->sortKeyIsMainKey();
  } else {
    qDebug() << Q_FUNC_INFO << "no data plottable set";
    return true;
  }
}

QCPDataSelection QCPErrorBars::selectTestRect(const QRectF &rect,
                                              bool onlySelectable) const {
  QCPDataSelection result;
  if (!mDataPlottable)
    return result;
  if ((onlySelectable && mSelectable == QCP::stNone) ||
      mDataContainer->isEmpty())
    return result;
  if (!mKeyAxis || !mValueAxis)
    return result;

  QCPErrorBarsDataContainer::const_iterator visibleBegin, visibleEnd;
  getVisibleDataBounds(visibleBegin, visibleEnd, QCPDataRange(0, dataCount()));

  QVector<QLineF> backbones, whiskers;
  for (QCPErrorBarsDataContainer::const_iterator it = visibleBegin;
       it != visibleEnd; ++it) {
    backbones.clear();
    whiskers.clear();
    getErrorBarLines(it, backbones, whiskers);
    for (int i = 0; i < backbones.size(); ++i) {
      if (rectIntersectsLine(rect, backbones.at(i))) {
        result.addDataRange(QCPDataRange(it - mDataContainer->constBegin(),
                                         it - mDataContainer->constBegin() + 1),
                            false);
        break;
      }
    }
  }
  result.simplify();
  return result;
}

int QCPErrorBars::findBegin(double sortKey, bool expandedRange) const {
  if (mDataPlottable) {
    if (mDataContainer->isEmpty())
      return 0;
    int beginIndex =
        mDataPlottable->interface1D()->findBegin(sortKey, expandedRange);
    if (beginIndex >= mDataContainer->size())
      beginIndex = mDataContainer->size() - 1;
    return beginIndex;
  } else
    qDebug() << Q_FUNC_INFO << "no data plottable set";
  return 0;
}

int QCPErrorBars::findEnd(double sortKey, bool expandedRange) const {
  if (mDataPlottable) {
    if (mDataContainer->isEmpty())
      return 0;
    int endIndex =
        mDataPlottable->interface1D()->findEnd(sortKey, expandedRange);
    if (endIndex > mDataContainer->size())
      endIndex = mDataContainer->size();
    return endIndex;
  } else
    qDebug() << Q_FUNC_INFO << "no data plottable set";
  return 0;
}

double QCPErrorBars::selectTest(const QPointF &pos, bool onlySelectable,
                                QVariant *details) const {
  if (!mDataPlottable)
    return -1;

  if ((onlySelectable && mSelectable == QCP::stNone) ||
      mDataContainer->isEmpty())
    return -1;
  if (!mKeyAxis || !mValueAxis)
    return -1;

  if (mKeyAxis.data()->axisRect()->rect().contains(pos.toPoint())) {
    QCPErrorBarsDataContainer::const_iterator closestDataPoint =
        mDataContainer->constEnd();
    double result = pointDistance(pos, closestDataPoint);
    if (details) {
      int pointIndex = closestDataPoint - mDataContainer->constBegin();
      details->setValue(
          QCPDataSelection(QCPDataRange(pointIndex, pointIndex + 1)));
    }
    return result;
  } else
    return -1;
}

void QCPErrorBars::draw(QCPPainter *painter) {
  if (!mDataPlottable)
    return;
  if (!mKeyAxis || !mValueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return;
  }
  if (mKeyAxis.data()->range().size() <= 0 || mDataContainer->isEmpty())
    return;

  bool checkPointVisibility =
      !mDataPlottable->interface1D()->sortKeyIsMainKey();

#ifdef QCUSTOMPLOT_CHECK_DATA
  QCPErrorBarsDataContainer::const_iterator it;
  for (it = mDataContainer->constBegin(); it != mDataContainer->constEnd();
       ++it) {
    if (QCP::isInvalidData(it->errorMinus, it->errorPlus))
      qDebug() << Q_FUNC_INFO << "Data point at index"
               << it - mDataContainer->constBegin() << "invalid."
               << "Plottable name:" << name();
  }
#endif

  applyDefaultAntialiasingHint(painter);
  painter->setBrush(Qt::NoBrush);

  QList<QCPDataRange> selectedSegments, unselectedSegments, allSegments;
  getDataSegments(selectedSegments, unselectedSegments);
  allSegments << unselectedSegments << selectedSegments;
  QVector<QLineF> backbones, whiskers;
  for (int i = 0; i < allSegments.size(); ++i) {
    QCPErrorBarsDataContainer::const_iterator begin, end;
    getVisibleDataBounds(begin, end, allSegments.at(i));
    if (begin == end)
      continue;

    bool isSelectedSegment = i >= unselectedSegments.size();
    if (isSelectedSegment && mSelectionDecorator)
      mSelectionDecorator->applyPen(painter);
    else
      painter->setPen(mPen);
    if (painter->pen().capStyle() == Qt::SquareCap) {
      QPen capFixPen(painter->pen());
      capFixPen.setCapStyle(Qt::FlatCap);
      painter->setPen(capFixPen);
    }
    backbones.clear();
    whiskers.clear();
    for (QCPErrorBarsDataContainer::const_iterator it = begin; it != end;
         ++it) {
      if (!checkPointVisibility ||
          errorBarVisible(it - mDataContainer->constBegin()))
        getErrorBarLines(it, backbones, whiskers);
    }
    painter->drawLines(backbones);
    painter->drawLines(whiskers);
  }

  if (mSelectionDecorator)
    mSelectionDecorator->drawDecoration(painter, selection());
}

void QCPErrorBars::drawLegendIcon(QCPPainter *painter,
                                  const QRectF &rect) const {
  applyDefaultAntialiasingHint(painter);
  painter->setPen(mPen);
  if (mErrorType == etValueError && mValueAxis &&
      mValueAxis->orientation() == Qt::Vertical) {
    painter->drawLine(QLineF(rect.center().x(), rect.top() + 2,
                             rect.center().x(), rect.bottom() - 1));
    painter->drawLine(QLineF(rect.center().x() - 4, rect.top() + 2,
                             rect.center().x() + 4, rect.top() + 2));
    painter->drawLine(QLineF(rect.center().x() - 4, rect.bottom() - 1,
                             rect.center().x() + 4, rect.bottom() - 1));
  } else {
    painter->drawLine(QLineF(rect.left() + 2, rect.center().y(),
                             rect.right() - 2, rect.center().y()));
    painter->drawLine(QLineF(rect.left() + 2, rect.center().y() - 4,
                             rect.left() + 2, rect.center().y() + 4));
    painter->drawLine(QLineF(rect.right() - 2, rect.center().y() - 4,
                             rect.right() - 2, rect.center().y() + 4));
  }
}

QCPRange QCPErrorBars::getKeyRange(bool &foundRange,
                                   QCP::SignDomain inSignDomain) const {
  if (!mDataPlottable) {
    foundRange = false;
    return QCPRange();
  }

  QCPRange range;
  bool haveLower = false;
  bool haveUpper = false;
  QCPErrorBarsDataContainer::const_iterator it;
  for (it = mDataContainer->constBegin(); it != mDataContainer->constEnd();
       ++it) {
    if (mErrorType == etValueError) {
      const double current = mDataPlottable->interface1D()->dataMainKey(
          it - mDataContainer->constBegin());
      if (qIsNaN(current))
        continue;
      if (inSignDomain == QCP::sdBoth ||
          (inSignDomain == QCP::sdNegative && current < 0) ||
          (inSignDomain == QCP::sdPositive && current > 0)) {
        if (current < range.lower || !haveLower) {
          range.lower = current;
          haveLower = true;
        }
        if (current > range.upper || !haveUpper) {
          range.upper = current;
          haveUpper = true;
        }
      }
    } else

    {
      const double dataKey = mDataPlottable->interface1D()->dataMainKey(
          it - mDataContainer->constBegin());
      if (qIsNaN(dataKey))
        continue;

      double current = dataKey + (qIsNaN(it->errorPlus) ? 0 : it->errorPlus);
      if (inSignDomain == QCP::sdBoth ||
          (inSignDomain == QCP::sdNegative && current < 0) ||
          (inSignDomain == QCP::sdPositive && current > 0)) {
        if (current > range.upper || !haveUpper) {
          range.upper = current;
          haveUpper = true;
        }
      }

      current = dataKey - (qIsNaN(it->errorMinus) ? 0 : it->errorMinus);
      if (inSignDomain == QCP::sdBoth ||
          (inSignDomain == QCP::sdNegative && current < 0) ||
          (inSignDomain == QCP::sdPositive && current > 0)) {
        if (current < range.lower || !haveLower) {
          range.lower = current;
          haveLower = true;
        }
      }
    }
  }

  if (haveUpper && !haveLower) {
    range.lower = range.upper;
    haveLower = true;
  } else if (haveLower && !haveUpper) {
    range.upper = range.lower;
    haveUpper = true;
  }

  foundRange = haveLower && haveUpper;
  return range;
}

QCPRange QCPErrorBars::getValueRange(bool &foundRange,
                                     QCP::SignDomain inSignDomain,
                                     const QCPRange &inKeyRange) const {
  if (!mDataPlottable) {
    foundRange = false;
    return QCPRange();
  }

  QCPRange range;
  const bool restrictKeyRange = inKeyRange != QCPRange();
  bool haveLower = false;
  bool haveUpper = false;
  QCPErrorBarsDataContainer::const_iterator itBegin =
      mDataContainer->constBegin();
  QCPErrorBarsDataContainer::const_iterator itEnd = mDataContainer->constEnd();
  if (mDataPlottable->interface1D()->sortKeyIsMainKey() && restrictKeyRange) {
    itBegin = mDataContainer->constBegin() + findBegin(inKeyRange.lower);
    itEnd = mDataContainer->constBegin() + findEnd(inKeyRange.upper);
  }
  for (QCPErrorBarsDataContainer::const_iterator it = itBegin; it != itEnd;
       ++it) {
    if (restrictKeyRange) {
      const double dataKey = mDataPlottable->interface1D()->dataMainKey(
          it - mDataContainer->constBegin());
      if (dataKey < inKeyRange.lower || dataKey > inKeyRange.upper)
        continue;
    }
    if (mErrorType == etValueError) {
      const double dataValue = mDataPlottable->interface1D()->dataMainValue(
          it - mDataContainer->constBegin());
      if (qIsNaN(dataValue))
        continue;

      double current = dataValue + (qIsNaN(it->errorPlus) ? 0 : it->errorPlus);
      if (inSignDomain == QCP::sdBoth ||
          (inSignDomain == QCP::sdNegative && current < 0) ||
          (inSignDomain == QCP::sdPositive && current > 0)) {
        if (current > range.upper || !haveUpper) {
          range.upper = current;
          haveUpper = true;
        }
      }

      current = dataValue - (qIsNaN(it->errorMinus) ? 0 : it->errorMinus);
      if (inSignDomain == QCP::sdBoth ||
          (inSignDomain == QCP::sdNegative && current < 0) ||
          (inSignDomain == QCP::sdPositive && current > 0)) {
        if (current < range.lower || !haveLower) {
          range.lower = current;
          haveLower = true;
        }
      }
    } else

    {
      const double current = mDataPlottable->interface1D()->dataMainValue(
          it - mDataContainer->constBegin());
      if (qIsNaN(current))
        continue;
      if (inSignDomain == QCP::sdBoth ||
          (inSignDomain == QCP::sdNegative && current < 0) ||
          (inSignDomain == QCP::sdPositive && current > 0)) {
        if (current < range.lower || !haveLower) {
          range.lower = current;
          haveLower = true;
        }
        if (current > range.upper || !haveUpper) {
          range.upper = current;
          haveUpper = true;
        }
      }
    }
  }

  if (haveUpper && !haveLower) {
    range.lower = range.upper;
    haveLower = true;
  } else if (haveLower && !haveUpper) {
    range.upper = range.lower;
    haveUpper = true;
  }

  foundRange = haveLower && haveUpper;
  return range;
}

void QCPErrorBars::getErrorBarLines(
    QCPErrorBarsDataContainer::const_iterator it, QVector<QLineF> &backbones,
    QVector<QLineF> &whiskers) const {
  if (!mDataPlottable)
    return;

  int index = it - mDataContainer->constBegin();
  QPointF centerPixel = mDataPlottable->interface1D()->dataPixelPosition(index);
  if (qIsNaN(centerPixel.x()) || qIsNaN(centerPixel.y()))
    return;
  QCPAxis *errorAxis =
      mErrorType == etValueError ? mValueAxis.data() : mKeyAxis.data();
  QCPAxis *orthoAxis =
      mErrorType == etValueError ? mKeyAxis.data() : mValueAxis.data();
  const double centerErrorAxisPixel = errorAxis->orientation() == Qt::Horizontal
                                          ? centerPixel.x()
                                          : centerPixel.y();
  const double centerOrthoAxisPixel = orthoAxis->orientation() == Qt::Horizontal
                                          ? centerPixel.x()
                                          : centerPixel.y();
  const double centerErrorAxisCoord =
      errorAxis->pixelToCoord(centerErrorAxisPixel);

  const double symbolGap = mSymbolGap * 0.5 * errorAxis->pixelOrientation();

  double errorStart, errorEnd;
  if (!qIsNaN(it->errorPlus)) {
    errorStart = centerErrorAxisPixel + symbolGap;
    errorEnd = errorAxis->coordToPixel(centerErrorAxisCoord + it->errorPlus);
    if (errorAxis->orientation() == Qt::Vertical) {
      if ((errorStart > errorEnd) != errorAxis->rangeReversed())
        backbones.append(QLineF(centerOrthoAxisPixel, errorStart,
                                centerOrthoAxisPixel, errorEnd));
      whiskers.append(
          QLineF(centerOrthoAxisPixel - mWhiskerWidth * 0.5, errorEnd,
                 centerOrthoAxisPixel + mWhiskerWidth * 0.5, errorEnd));
    } else {
      if ((errorStart < errorEnd) != errorAxis->rangeReversed())
        backbones.append(QLineF(errorStart, centerOrthoAxisPixel, errorEnd,
                                centerOrthoAxisPixel));
      whiskers.append(
          QLineF(errorEnd, centerOrthoAxisPixel - mWhiskerWidth * 0.5, errorEnd,
                 centerOrthoAxisPixel + mWhiskerWidth * 0.5));
    }
  }

  if (!qIsNaN(it->errorMinus)) {
    errorStart = centerErrorAxisPixel - symbolGap;
    errorEnd = errorAxis->coordToPixel(centerErrorAxisCoord - it->errorMinus);
    if (errorAxis->orientation() == Qt::Vertical) {
      if ((errorStart < errorEnd) != errorAxis->rangeReversed())
        backbones.append(QLineF(centerOrthoAxisPixel, errorStart,
                                centerOrthoAxisPixel, errorEnd));
      whiskers.append(
          QLineF(centerOrthoAxisPixel - mWhiskerWidth * 0.5, errorEnd,
                 centerOrthoAxisPixel + mWhiskerWidth * 0.5, errorEnd));
    } else {
      if ((errorStart > errorEnd) != errorAxis->rangeReversed())
        backbones.append(QLineF(errorStart, centerOrthoAxisPixel, errorEnd,
                                centerOrthoAxisPixel));
      whiskers.append(
          QLineF(errorEnd, centerOrthoAxisPixel - mWhiskerWidth * 0.5, errorEnd,
                 centerOrthoAxisPixel + mWhiskerWidth * 0.5));
    }
  }
}

void QCPErrorBars::getVisibleDataBounds(
    QCPErrorBarsDataContainer::const_iterator &begin,
    QCPErrorBarsDataContainer::const_iterator &end,
    const QCPDataRange &rangeRestriction) const {
  QCPAxis *keyAxis = mKeyAxis.data();
  QCPAxis *valueAxis = mValueAxis.data();
  if (!keyAxis || !valueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    end = mDataContainer->constEnd();
    begin = end;
    return;
  }
  if (!mDataPlottable || rangeRestriction.isEmpty()) {
    end = mDataContainer->constEnd();
    begin = end;
    return;
  }
  if (!mDataPlottable->interface1D()->sortKeyIsMainKey()) {
    QCPDataRange dataRange(0, mDataContainer->size());
    dataRange = dataRange.bounded(rangeRestriction);
    begin = mDataContainer->constBegin() + dataRange.begin();
    end = mDataContainer->constBegin() + dataRange.end();
    return;
  }

  const int n =
      qMin(mDataContainer->size(), mDataPlottable->interface1D()->dataCount());
  int beginIndex =
      mDataPlottable->interface1D()->findBegin(keyAxis->range().lower);
  int endIndex = mDataPlottable->interface1D()->findEnd(keyAxis->range().upper);
  int i = beginIndex;
  while (i > 0 && i < n && i > rangeRestriction.begin()) {
    if (errorBarVisible(i))
      beginIndex = i;
    --i;
  }
  i = endIndex;
  while (i >= 0 && i < n && i < rangeRestriction.end()) {
    if (errorBarVisible(i))
      endIndex = i + 1;
    ++i;
  }
  QCPDataRange dataRange(beginIndex, endIndex);
  dataRange = dataRange.bounded(
      rangeRestriction.bounded(QCPDataRange(0, mDataContainer->size())));
  begin = mDataContainer->constBegin() + dataRange.begin();
  end = mDataContainer->constBegin() + dataRange.end();
}

double QCPErrorBars::pointDistance(
    const QPointF &pixelPoint,
    QCPErrorBarsDataContainer::const_iterator &closestData) const {
  closestData = mDataContainer->constEnd();
  if (!mDataPlottable || mDataContainer->isEmpty())
    return -1.0;
  if (!mKeyAxis || !mValueAxis) {
    qDebug() << Q_FUNC_INFO << "invalid key or value axis";
    return -1.0;
  }

  QCPErrorBarsDataContainer::const_iterator begin, end;
  getVisibleDataBounds(begin, end, QCPDataRange(0, dataCount()));

  double minDistSqr = (std::numeric_limits<double>::max)();
  QVector<QLineF> backbones, whiskers;
  for (QCPErrorBarsDataContainer::const_iterator it = begin; it != end; ++it) {
    getErrorBarLines(it, backbones, whiskers);
    for (int i = 0; i < backbones.size(); ++i) {
      const double currentDistSqr =
          QCPVector2D(pixelPoint).distanceSquaredToLine(backbones.at(i));
      if (currentDistSqr < minDistSqr) {
        minDistSqr = currentDistSqr;
        closestData = it;
      }
    }
  }
  return qSqrt(minDistSqr);
}

void QCPErrorBars::getDataSegments(
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

bool QCPErrorBars::errorBarVisible(int index) const {
  QPointF centerPixel = mDataPlottable->interface1D()->dataPixelPosition(index);
  const double centerKeyPixel = mKeyAxis->orientation() == Qt::Horizontal
                                    ? centerPixel.x()
                                    : centerPixel.y();
  if (qIsNaN(centerKeyPixel))
    return false;

  double keyMin, keyMax;
  if (mErrorType == etKeyError) {
    const double centerKey = mKeyAxis->pixelToCoord(centerKeyPixel);
    const double errorPlus = mDataContainer->at(index).errorPlus;
    const double errorMinus = mDataContainer->at(index).errorMinus;
    keyMax = centerKey + (qIsNaN(errorPlus) ? 0 : errorPlus);
    keyMin = centerKey - (qIsNaN(errorMinus) ? 0 : errorMinus);
  } else

  {
    keyMax = mKeyAxis->pixelToCoord(
        centerKeyPixel + mWhiskerWidth * 0.5 * mKeyAxis->pixelOrientation());
    keyMin = mKeyAxis->pixelToCoord(
        centerKeyPixel - mWhiskerWidth * 0.5 * mKeyAxis->pixelOrientation());
  }
  return ((keyMax > mKeyAxis->range().lower) &&
          (keyMin < mKeyAxis->range().upper));
}

bool QCPErrorBars::rectIntersectsLine(const QRectF &pixelRect,
                                      const QLineF &line) const {
  if (pixelRect.left() > line.x1() && pixelRect.left() > line.x2())
    return false;
  else if (pixelRect.right() < line.x1() && pixelRect.right() < line.x2())
    return false;
  else if (pixelRect.top() > line.y1() && pixelRect.top() > line.y2())
    return false;
  else if (pixelRect.bottom() < line.y1() && pixelRect.bottom() < line.y2())
    return false;
  else
    return true;
}

QCPItemStraightLine::QCPItemStraightLine(QCustomPlot *parentPlot)
    : QCPAbstractItem(parentPlot),
      point1(createPosition(QLatin1String("point1"))),
      point2(createPosition(QLatin1String("point2"))) {
  point1->setCoords(0, 0);
  point2->setCoords(1, 1);

  setPen(QPen(Qt::black));
  setSelectedPen(QPen(Qt::blue, 2));
}

QCPItemStraightLine::~QCPItemStraightLine() {}

void QCPItemStraightLine::setPen(const QPen &pen) { mPen = pen; }

void QCPItemStraightLine::setSelectedPen(const QPen &pen) {
  mSelectedPen = pen;
}

double QCPItemStraightLine::selectTest(const QPointF &pos, bool onlySelectable,
                                       QVariant *details) const {
  Q_UNUSED(details)
  if (onlySelectable && !mSelectable)
    return -1;

  return QCPVector2D(pos).distanceToStraightLine(point1->pixelPosition(),
                                                 point2->pixelPosition() -
                                                     point1->pixelPosition());
}

void QCPItemStraightLine::draw(QCPPainter *painter) {
  QCPVector2D start(point1->pixelPosition());
  QCPVector2D end(point2->pixelPosition());

  double clipPad = mainPen().widthF();
  QLineF line = getRectClippedStraightLine(
      start, end - start,
      clipRect().adjusted(-clipPad, -clipPad, clipPad, clipPad));

  if (!line.isNull()) {
    painter->setPen(mainPen());
    painter->drawLine(line);
  }
}

QLineF QCPItemStraightLine::getRectClippedStraightLine(
    const QCPVector2D &base, const QCPVector2D &vec, const QRect &rect) const {
  double bx, by;
  double gamma;
  QLineF result;
  if (vec.x() == 0 && vec.y() == 0)
    return result;
  if (qFuzzyIsNull(vec.x()))

  {
    bx = rect.left();
    by = rect.top();
    gamma = base.x() - bx + (by - base.y()) * vec.x() / vec.y();
    if (gamma >= 0 && gamma <= rect.width())
      result.setLine(bx + gamma, rect.top(), bx + gamma, rect.bottom());

  } else if (qFuzzyIsNull(vec.y()))

  {
    bx = rect.left();
    by = rect.top();
    gamma = base.y() - by + (bx - base.x()) * vec.y() / vec.x();
    if (gamma >= 0 && gamma <= rect.height())
      result.setLine(rect.left(), by + gamma, rect.right(), by + gamma);

  } else

  {
    QList<QCPVector2D> pointVectors;

    bx = rect.left();
    by = rect.top();
    gamma = base.x() - bx + (by - base.y()) * vec.x() / vec.y();
    if (gamma >= 0 && gamma <= rect.width())
      pointVectors.append(QCPVector2D(bx + gamma, by));

    bx = rect.left();
    by = rect.bottom();
    gamma = base.x() - bx + (by - base.y()) * vec.x() / vec.y();
    if (gamma >= 0 && gamma <= rect.width())
      pointVectors.append(QCPVector2D(bx + gamma, by));

    bx = rect.left();
    by = rect.top();
    gamma = base.y() - by + (bx - base.x()) * vec.y() / vec.x();
    if (gamma >= 0 && gamma <= rect.height())
      pointVectors.append(QCPVector2D(bx, by + gamma));

    bx = rect.right();
    by = rect.top();
    gamma = base.y() - by + (bx - base.x()) * vec.y() / vec.x();
    if (gamma >= 0 && gamma <= rect.height())
      pointVectors.append(QCPVector2D(bx, by + gamma));

    if (pointVectors.size() == 2) {
      result.setPoints(pointVectors.at(0).toPointF(),
                       pointVectors.at(1).toPointF());
    } else if (pointVectors.size() > 2) {
      double distSqrMax = 0;
      QCPVector2D pv1, pv2;
      for (int i = 0; i < pointVectors.size() - 1; ++i) {
        for (int k = i + 1; k < pointVectors.size(); ++k) {
          double distSqr =
              (pointVectors.at(i) - pointVectors.at(k)).lengthSquared();
          if (distSqr > distSqrMax) {
            pv1 = pointVectors.at(i);
            pv2 = pointVectors.at(k);
            distSqrMax = distSqr;
          }
        }
      }
      result.setPoints(pv1.toPointF(), pv2.toPointF());
    }
  }
  return result;
}

QPen QCPItemStraightLine::mainPen() const {
  return mSelected ? mSelectedPen : mPen;
}

QCPItemLine::QCPItemLine(QCustomPlot *parentPlot)
    : QCPAbstractItem(parentPlot),
      start(createPosition(QLatin1String("start"))),
      end(createPosition(QLatin1String("end"))) {
  start->setCoords(0, 0);
  end->setCoords(1, 1);

  setPen(QPen(Qt::black));
  setSelectedPen(QPen(Qt::blue, 2));
}

QCPItemLine::~QCPItemLine() {}

void QCPItemLine::setPen(const QPen &pen) { mPen = pen; }

void QCPItemLine::setSelectedPen(const QPen &pen) { mSelectedPen = pen; }

void QCPItemLine::setHead(const QCPLineEnding &head) { mHead = head; }

void QCPItemLine::setTail(const QCPLineEnding &tail) { mTail = tail; }

double QCPItemLine::selectTest(const QPointF &pos, bool onlySelectable,
                               QVariant *details) const {
  Q_UNUSED(details)
  if (onlySelectable && !mSelectable)
    return -1;

  return qSqrt(QCPVector2D(pos).distanceSquaredToLine(start->pixelPosition(),
                                                      end->pixelPosition()));
}

void QCPItemLine::draw(QCPPainter *painter) {
  QCPVector2D startVec(start->pixelPosition());
  QCPVector2D endVec(end->pixelPosition());
  if (qFuzzyIsNull((startVec - endVec).lengthSquared()))
    return;

  double clipPad = qMax(mHead.boundingDistance(), mTail.boundingDistance());
  clipPad = qMax(clipPad, (double)mainPen().widthF());
  QLineF line = getRectClippedLine(
      startVec, endVec,
      clipRect().adjusted(-clipPad, -clipPad, clipPad, clipPad));

  if (!line.isNull()) {
    painter->setPen(mainPen());
    painter->drawLine(line);
    painter->setBrush(Qt::SolidPattern);
    if (mTail.style() != QCPLineEnding::esNone)
      mTail.draw(painter, startVec, startVec - endVec);
    if (mHead.style() != QCPLineEnding::esNone)
      mHead.draw(painter, endVec, endVec - startVec);
  }
}

QLineF QCPItemLine::getRectClippedLine(const QCPVector2D &start,
                                       const QCPVector2D &end,
                                       const QRect &rect) const {
  bool containsStart = rect.contains(start.x(), start.y());
  bool containsEnd = rect.contains(end.x(), end.y());
  if (containsStart && containsEnd)
    return QLineF(start.toPointF(), end.toPointF());

  QCPVector2D base = start;
  QCPVector2D vec = end - start;
  double bx, by;
  double gamma, mu;
  QLineF result;
  QList<QCPVector2D> pointVectors;

  if (!qFuzzyIsNull(vec.y()))

  {
    bx = rect.left();
    by = rect.top();
    mu = (by - base.y()) / vec.y();
    if (mu >= 0 && mu <= 1) {
      gamma = base.x() - bx + mu * vec.x();
      if (gamma >= 0 && gamma <= rect.width())
        pointVectors.append(QCPVector2D(bx + gamma, by));
    }

    bx = rect.left();
    by = rect.bottom();
    mu = (by - base.y()) / vec.y();
    if (mu >= 0 && mu <= 1) {
      gamma = base.x() - bx + mu * vec.x();
      if (gamma >= 0 && gamma <= rect.width())
        pointVectors.append(QCPVector2D(bx + gamma, by));
    }
  }
  if (!qFuzzyIsNull(vec.x()))

  {
    bx = rect.left();
    by = rect.top();
    mu = (bx - base.x()) / vec.x();
    if (mu >= 0 && mu <= 1) {
      gamma = base.y() - by + mu * vec.y();
      if (gamma >= 0 && gamma <= rect.height())
        pointVectors.append(QCPVector2D(bx, by + gamma));
    }

    bx = rect.right();
    by = rect.top();
    mu = (bx - base.x()) / vec.x();
    if (mu >= 0 && mu <= 1) {
      gamma = base.y() - by + mu * vec.y();
      if (gamma >= 0 && gamma <= rect.height())
        pointVectors.append(QCPVector2D(bx, by + gamma));
    }
  }

  if (containsStart)
    pointVectors.append(start);
  if (containsEnd)
    pointVectors.append(end);

  if (pointVectors.size() == 2) {
    result.setPoints(pointVectors.at(0).toPointF(),
                     pointVectors.at(1).toPointF());
  } else if (pointVectors.size() > 2) {
    double distSqrMax = 0;
    QCPVector2D pv1, pv2;
    for (int i = 0; i < pointVectors.size() - 1; ++i) {
      for (int k = i + 1; k < pointVectors.size(); ++k) {
        double distSqr =
            (pointVectors.at(i) - pointVectors.at(k)).lengthSquared();
        if (distSqr > distSqrMax) {
          pv1 = pointVectors.at(i);
          pv2 = pointVectors.at(k);
          distSqrMax = distSqr;
        }
      }
    }
    result.setPoints(pv1.toPointF(), pv2.toPointF());
  }
  return result;
}

QPen QCPItemLine::mainPen() const { return mSelected ? mSelectedPen : mPen; }

QCPItemCurve::QCPItemCurve(QCustomPlot *parentPlot)
    : QCPAbstractItem(parentPlot),
      start(createPosition(QLatin1String("start"))),
      startDir(createPosition(QLatin1String("startDir"))),
      endDir(createPosition(QLatin1String("endDir"))),
      end(createPosition(QLatin1String("end"))) {
  start->setCoords(0, 0);
  startDir->setCoords(0.5, 0);
  endDir->setCoords(0, 0.5);
  end->setCoords(1, 1);

  setPen(QPen(Qt::black));
  setSelectedPen(QPen(Qt::blue, 2));
}

QCPItemCurve::~QCPItemCurve() {}

void QCPItemCurve::setPen(const QPen &pen) { mPen = pen; }

void QCPItemCurve::setSelectedPen(const QPen &pen) { mSelectedPen = pen; }

void QCPItemCurve::setHead(const QCPLineEnding &head) { mHead = head; }

void QCPItemCurve::setTail(const QCPLineEnding &tail) { mTail = tail; }

double QCPItemCurve::selectTest(const QPointF &pos, bool onlySelectable,
                                QVariant *details) const {
  Q_UNUSED(details)
  if (onlySelectable && !mSelectable)
    return -1;

  QPointF startVec(start->pixelPosition());
  QPointF startDirVec(startDir->pixelPosition());
  QPointF endDirVec(endDir->pixelPosition());
  QPointF endVec(end->pixelPosition());

  QPainterPath cubicPath(startVec);
  cubicPath.cubicTo(startDirVec, endDirVec, endVec);

  QList<QPolygonF> polygons = cubicPath.toSubpathPolygons();
  if (polygons.isEmpty())
    return -1;
  const QPolygonF polygon = polygons.first();
  QCPVector2D p(pos);
  double minDistSqr = (std::numeric_limits<double>::max)();
  for (int i = 1; i < polygon.size(); ++i) {
    double distSqr = p.distanceSquaredToLine(polygon.at(i - 1), polygon.at(i));
    if (distSqr < minDistSqr)
      minDistSqr = distSqr;
  }
  return qSqrt(minDistSqr);
}

void QCPItemCurve::draw(QCPPainter *painter) {
  QCPVector2D startVec(start->pixelPosition());
  QCPVector2D startDirVec(startDir->pixelPosition());
  QCPVector2D endDirVec(endDir->pixelPosition());
  QCPVector2D endVec(end->pixelPosition());
  if ((endVec - startVec).length() > 1e10)

    return;

  QPainterPath cubicPath(startVec.toPointF());
  cubicPath.cubicTo(startDirVec.toPointF(), endDirVec.toPointF(),
                    endVec.toPointF());

  QRect clip = clipRect().adjusted(-mainPen().widthF(), -mainPen().widthF(),
                                   mainPen().widthF(), mainPen().widthF());
  QRect cubicRect = cubicPath.controlPointRect().toRect();
  if (cubicRect.isEmpty())

    cubicRect.adjust(0, 0, 1, 1);
  if (clip.intersects(cubicRect)) {
    painter->setPen(mainPen());
    painter->drawPath(cubicPath);
    painter->setBrush(Qt::SolidPattern);
    if (mTail.style() != QCPLineEnding::esNone)
      mTail.draw(painter, startVec,
                 M_PI - cubicPath.angleAtPercent(0) / 180.0 * M_PI);
    if (mHead.style() != QCPLineEnding::esNone)
      mHead.draw(painter, endVec, -cubicPath.angleAtPercent(1) / 180.0 * M_PI);
  }
}

QPen QCPItemCurve::mainPen() const { return mSelected ? mSelectedPen : mPen; }

QCPItemRect::QCPItemRect(QCustomPlot *parentPlot)
    : QCPAbstractItem(parentPlot),
      topLeft(createPosition(QLatin1String("topLeft"))),
      bottomRight(createPosition(QLatin1String("bottomRight"))),
      top(createAnchor(QLatin1String("top"), aiTop)),
      topRight(createAnchor(QLatin1String("topRight"), aiTopRight)),
      right(createAnchor(QLatin1String("right"), aiRight)),
      bottom(createAnchor(QLatin1String("bottom"), aiBottom)),
      bottomLeft(createAnchor(QLatin1String("bottomLeft"), aiBottomLeft)),
      left(createAnchor(QLatin1String("left"), aiLeft)) {
  topLeft->setCoords(0, 1);
  bottomRight->setCoords(1, 0);

  setPen(QPen(Qt::black));
  setSelectedPen(QPen(Qt::blue, 2));
  setBrush(Qt::NoBrush);
  setSelectedBrush(Qt::NoBrush);
}

QCPItemRect::~QCPItemRect() {}

void QCPItemRect::setPen(const QPen &pen) { mPen = pen; }

void QCPItemRect::setSelectedPen(const QPen &pen) { mSelectedPen = pen; }

void QCPItemRect::setBrush(const QBrush &brush) { mBrush = brush; }

void QCPItemRect::setSelectedBrush(const QBrush &brush) {
  mSelectedBrush = brush;
}

double QCPItemRect::selectTest(const QPointF &pos, bool onlySelectable,
                               QVariant *details) const {
  Q_UNUSED(details)
  if (onlySelectable && !mSelectable)
    return -1;

  QRectF rect = QRectF(topLeft->pixelPosition(), bottomRight->pixelPosition())
                    .normalized();
  bool filledRect =
      mBrush.style() != Qt::NoBrush && mBrush.color().alpha() != 0;
  return rectDistance(rect, pos, filledRect);
}

void QCPItemRect::draw(QCPPainter *painter) {
  QPointF p1 = topLeft->pixelPosition();
  QPointF p2 = bottomRight->pixelPosition();
  if (p1.toPoint() == p2.toPoint())
    return;
  QRectF rect = QRectF(p1, p2).normalized();
  double clipPad = mainPen().widthF();
  QRectF boundingRect = rect.adjusted(-clipPad, -clipPad, clipPad, clipPad);
  if (boundingRect.intersects(clipRect()))

  {
    painter->setPen(mainPen());
    painter->setBrush(mainBrush());
    painter->drawRect(rect);
  }
}

QPointF QCPItemRect::anchorPixelPosition(int anchorId) const {
  QRectF rect = QRectF(topLeft->pixelPosition(), bottomRight->pixelPosition());
  switch (anchorId) {
  case aiTop:
    return (rect.topLeft() + rect.topRight()) * 0.5;
  case aiTopRight:
    return rect.topRight();
  case aiRight:
    return (rect.topRight() + rect.bottomRight()) * 0.5;
  case aiBottom:
    return (rect.bottomLeft() + rect.bottomRight()) * 0.5;
  case aiBottomLeft:
    return rect.bottomLeft();
  case aiLeft:
    return (rect.topLeft() + rect.bottomLeft()) * 0.5;
  }

  qDebug() << Q_FUNC_INFO << "invalid anchorId" << anchorId;
  return QPointF();
}

QPen QCPItemRect::mainPen() const { return mSelected ? mSelectedPen : mPen; }

QBrush QCPItemRect::mainBrush() const {
  return mSelected ? mSelectedBrush : mBrush;
}

QCPItemText::QCPItemText(QCustomPlot *parentPlot)
    : QCPAbstractItem(parentPlot),
      position(createPosition(QLatin1String("position"))),
      topLeft(createAnchor(QLatin1String("topLeft"), aiTopLeft)),
      top(createAnchor(QLatin1String("top"), aiTop)),
      topRight(createAnchor(QLatin1String("topRight"), aiTopRight)),
      right(createAnchor(QLatin1String("right"), aiRight)),
      bottomRight(createAnchor(QLatin1String("bottomRight"), aiBottomRight)),
      bottom(createAnchor(QLatin1String("bottom"), aiBottom)),
      bottomLeft(createAnchor(QLatin1String("bottomLeft"), aiBottomLeft)),
      left(createAnchor(QLatin1String("left"), aiLeft)),
      mText(QLatin1String("text")), mPositionAlignment(Qt::AlignCenter),
      mTextAlignment(Qt::AlignTop | Qt::AlignHCenter), mRotation(0) {
  position->setCoords(0, 0);

  setPen(Qt::NoPen);
  setSelectedPen(Qt::NoPen);
  setBrush(Qt::NoBrush);
  setSelectedBrush(Qt::NoBrush);
  setColor(Qt::black);
  setSelectedColor(Qt::blue);
}

QCPItemText::~QCPItemText() {}

void QCPItemText::setColor(const QColor &color) { mColor = color; }

void QCPItemText::setSelectedColor(const QColor &color) {
  mSelectedColor = color;
}

void QCPItemText::setPen(const QPen &pen) { mPen = pen; }

void QCPItemText::setSelectedPen(const QPen &pen) { mSelectedPen = pen; }

void QCPItemText::setBrush(const QBrush &brush) { mBrush = brush; }

void QCPItemText::setSelectedBrush(const QBrush &brush) {
  mSelectedBrush = brush;
}

void QCPItemText::setFont(const QFont &font) { mFont = font; }

void QCPItemText::setSelectedFont(const QFont &font) { mSelectedFont = font; }

void QCPItemText::setText(const QString &text) { mText = text; }

void QCPItemText::setPositionAlignment(Qt::Alignment alignment) {
  mPositionAlignment = alignment;
}

void QCPItemText::setTextAlignment(Qt::Alignment alignment) {
  mTextAlignment = alignment;
}

void QCPItemText::setRotation(double degrees) { mRotation = degrees; }

void QCPItemText::setPadding(const QMargins &padding) { mPadding = padding; }

double QCPItemText::selectTest(const QPointF &pos, bool onlySelectable,
                               QVariant *details) const {
  Q_UNUSED(details)
  if (onlySelectable && !mSelectable)
    return -1;

  QPointF positionPixels(position->pixelPosition());
  QTransform inputTransform;
  inputTransform.translate(positionPixels.x(), positionPixels.y());
  inputTransform.rotate(-mRotation);
  inputTransform.translate(-positionPixels.x(), -positionPixels.y());
  QPointF rotatedPos = inputTransform.map(pos);
  QFontMetrics fontMetrics(mFont);
  QRect textRect = fontMetrics.boundingRect(
      0, 0, 0, 0, Qt::TextDontClip | mTextAlignment, mText);
  QRect textBoxRect = textRect.adjusted(-mPadding.left(), -mPadding.top(),
                                        mPadding.right(), mPadding.bottom());
  QPointF textPos =
      getTextDrawPoint(positionPixels, textBoxRect, mPositionAlignment);
  textBoxRect.moveTopLeft(textPos.toPoint());

  return rectDistance(textBoxRect, rotatedPos, true);
}

void QCPItemText::draw(QCPPainter *painter) {
  QPointF pos(position->pixelPosition());
  QTransform transform = painter->transform();
  transform.translate(pos.x(), pos.y());
  if (!qFuzzyIsNull(mRotation))
    transform.rotate(mRotation);
  painter->setFont(mainFont());
  QRect textRect = painter->fontMetrics().boundingRect(
      0, 0, 0, 0, Qt::TextDontClip | mTextAlignment, mText);
  QRect textBoxRect = textRect.adjusted(-mPadding.left(), -mPadding.top(),
                                        mPadding.right(), mPadding.bottom());
  QPointF textPos =
      getTextDrawPoint(QPointF(0, 0), textBoxRect, mPositionAlignment);

  textRect.moveTopLeft(textPos.toPoint() +
                       QPoint(mPadding.left(), mPadding.top()));
  textBoxRect.moveTopLeft(textPos.toPoint());
  double clipPad = mainPen().widthF();
  QRect boundingRect =
      textBoxRect.adjusted(-clipPad, -clipPad, clipPad, clipPad);
  if (transform.mapRect(boundingRect)
          .intersects(painter->transform().mapRect(clipRect()))) {
    painter->setTransform(transform);
    if ((mainBrush().style() != Qt::NoBrush &&
         mainBrush().color().alpha() != 0) ||
        (mainPen().style() != Qt::NoPen && mainPen().color().alpha() != 0)) {
      painter->setPen(mainPen());
      painter->setBrush(mainBrush());
      painter->drawRect(textBoxRect);
    }
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(mainColor()));
    painter->drawText(textRect, Qt::TextDontClip | mTextAlignment, mText);
  }
}

QPointF QCPItemText::anchorPixelPosition(int anchorId) const {
  QPointF pos(position->pixelPosition());
  QTransform transform;
  transform.translate(pos.x(), pos.y());
  if (!qFuzzyIsNull(mRotation))
    transform.rotate(mRotation);
  QFontMetrics fontMetrics(mainFont());
  QRect textRect = fontMetrics.boundingRect(
      0, 0, 0, 0, Qt::TextDontClip | mTextAlignment, mText);
  QRectF textBoxRect = textRect.adjusted(-mPadding.left(), -mPadding.top(),
                                         mPadding.right(), mPadding.bottom());
  QPointF textPos =
      getTextDrawPoint(QPointF(0, 0), textBoxRect, mPositionAlignment);

  textBoxRect.moveTopLeft(textPos.toPoint());
  QPolygonF rectPoly = transform.map(QPolygonF(textBoxRect));

  switch (anchorId) {
  case aiTopLeft:
    return rectPoly.at(0);
  case aiTop:
    return (rectPoly.at(0) + rectPoly.at(1)) * 0.5;
  case aiTopRight:
    return rectPoly.at(1);
  case aiRight:
    return (rectPoly.at(1) + rectPoly.at(2)) * 0.5;
  case aiBottomRight:
    return rectPoly.at(2);
  case aiBottom:
    return (rectPoly.at(2) + rectPoly.at(3)) * 0.5;
  case aiBottomLeft:
    return rectPoly.at(3);
  case aiLeft:
    return (rectPoly.at(3) + rectPoly.at(0)) * 0.5;
  }

  qDebug() << Q_FUNC_INFO << "invalid anchorId" << anchorId;
  return QPointF();
}

QPointF QCPItemText::getTextDrawPoint(const QPointF &pos, const QRectF &rect,
                                      Qt::Alignment positionAlignment) const {
  if (positionAlignment == 0 ||
      positionAlignment == (Qt::AlignLeft | Qt::AlignTop))
    return pos;

  QPointF result = pos;

  if (positionAlignment.testFlag(Qt::AlignHCenter))
    result.rx() -= rect.width() / 2.0;
  else if (positionAlignment.testFlag(Qt::AlignRight))
    result.rx() -= rect.width();
  if (positionAlignment.testFlag(Qt::AlignVCenter))
    result.ry() -= rect.height() / 2.0;
  else if (positionAlignment.testFlag(Qt::AlignBottom))
    result.ry() -= rect.height();
  return result;
}

QFont QCPItemText::mainFont() const {
  return mSelected ? mSelectedFont : mFont;
}

QColor QCPItemText::mainColor() const {
  return mSelected ? mSelectedColor : mColor;
}

QPen QCPItemText::mainPen() const { return mSelected ? mSelectedPen : mPen; }

QBrush QCPItemText::mainBrush() const {
  return mSelected ? mSelectedBrush : mBrush;
}

QCPItemEllipse::QCPItemEllipse(QCustomPlot *parentPlot)
    : QCPAbstractItem(parentPlot),
      topLeft(createPosition(QLatin1String("topLeft"))),
      bottomRight(createPosition(QLatin1String("bottomRight"))),
      topLeftRim(createAnchor(QLatin1String("topLeftRim"), aiTopLeftRim)),
      top(createAnchor(QLatin1String("top"), aiTop)),
      topRightRim(createAnchor(QLatin1String("topRightRim"), aiTopRightRim)),
      right(createAnchor(QLatin1String("right"), aiRight)),
      bottomRightRim(
          createAnchor(QLatin1String("bottomRightRim"), aiBottomRightRim)),
      bottom(createAnchor(QLatin1String("bottom"), aiBottom)),
      bottomLeftRim(
          createAnchor(QLatin1String("bottomLeftRim"), aiBottomLeftRim)),
      left(createAnchor(QLatin1String("left"), aiLeft)),
      center(createAnchor(QLatin1String("center"), aiCenter)) {
  topLeft->setCoords(0, 1);
  bottomRight->setCoords(1, 0);

  setPen(QPen(Qt::black));
  setSelectedPen(QPen(Qt::blue, 2));
  setBrush(Qt::NoBrush);
  setSelectedBrush(Qt::NoBrush);
}

QCPItemEllipse::~QCPItemEllipse() {}

void QCPItemEllipse::setPen(const QPen &pen) { mPen = pen; }

void QCPItemEllipse::setSelectedPen(const QPen &pen) { mSelectedPen = pen; }

void QCPItemEllipse::setBrush(const QBrush &brush) { mBrush = brush; }

void QCPItemEllipse::setSelectedBrush(const QBrush &brush) {
  mSelectedBrush = brush;
}

double QCPItemEllipse::selectTest(const QPointF &pos, bool onlySelectable,
                                  QVariant *details) const {
  Q_UNUSED(details)
  if (onlySelectable && !mSelectable)
    return -1;

  QPointF p1 = topLeft->pixelPosition();
  QPointF p2 = bottomRight->pixelPosition();
  QPointF center((p1 + p2) / 2.0);
  double a = qAbs(p1.x() - p2.x()) / 2.0;
  double b = qAbs(p1.y() - p2.y()) / 2.0;
  double x = pos.x() - center.x();
  double y = pos.y() - center.y();

  double c = 1.0 / qSqrt(x * x / (a * a) + y * y / (b * b));
  double result = qAbs(c - 1) * qSqrt(x * x + y * y);

  if (result > mParentPlot->selectionTolerance() * 0.99 &&
      mBrush.style() != Qt::NoBrush && mBrush.color().alpha() != 0) {
    if (x * x / (a * a) + y * y / (b * b) <= 1)
      result = mParentPlot->selectionTolerance() * 0.99;
  }
  return result;
}

void QCPItemEllipse::draw(QCPPainter *painter) {
  QPointF p1 = topLeft->pixelPosition();
  QPointF p2 = bottomRight->pixelPosition();
  if (p1.toPoint() == p2.toPoint())
    return;
  QRectF ellipseRect = QRectF(p1, p2).normalized();
  QRect clip = clipRect().adjusted(-mainPen().widthF(), -mainPen().widthF(),
                                   mainPen().widthF(), mainPen().widthF());
  if (ellipseRect.intersects(clip))

  {
    painter->setPen(mainPen());
    painter->setBrush(mainBrush());
#ifdef __EXCEPTIONS
    try

    {
#endif
      painter->drawEllipse(ellipseRect);
#ifdef __EXCEPTIONS
    } catch (...) {
      qDebug() << Q_FUNC_INFO << "Item too large for memory, setting invisible";
      setVisible(false);
    }
#endif
  }
}

QPointF QCPItemEllipse::anchorPixelPosition(int anchorId) const {
  QRectF rect = QRectF(topLeft->pixelPosition(), bottomRight->pixelPosition());
  switch (anchorId) {
  case aiTopLeftRim:
    return rect.center() + (rect.topLeft() - rect.center()) * 1 / qSqrt(2);
  case aiTop:
    return (rect.topLeft() + rect.topRight()) * 0.5;
  case aiTopRightRim:
    return rect.center() + (rect.topRight() - rect.center()) * 1 / qSqrt(2);
  case aiRight:
    return (rect.topRight() + rect.bottomRight()) * 0.5;
  case aiBottomRightRim:
    return rect.center() + (rect.bottomRight() - rect.center()) * 1 / qSqrt(2);
  case aiBottom:
    return (rect.bottomLeft() + rect.bottomRight()) * 0.5;
  case aiBottomLeftRim:
    return rect.center() + (rect.bottomLeft() - rect.center()) * 1 / qSqrt(2);
  case aiLeft:
    return (rect.topLeft() + rect.bottomLeft()) * 0.5;
  case aiCenter:
    return (rect.topLeft() + rect.bottomRight()) * 0.5;
  }

  qDebug() << Q_FUNC_INFO << "invalid anchorId" << anchorId;
  return QPointF();
}

QPen QCPItemEllipse::mainPen() const { return mSelected ? mSelectedPen : mPen; }

QBrush QCPItemEllipse::mainBrush() const {
  return mSelected ? mSelectedBrush : mBrush;
}

QCPItemPixmap::QCPItemPixmap(QCustomPlot *parentPlot)
    : QCPAbstractItem(parentPlot),
      topLeft(createPosition(QLatin1String("topLeft"))),
      bottomRight(createPosition(QLatin1String("bottomRight"))),
      top(createAnchor(QLatin1String("top"), aiTop)),
      topRight(createAnchor(QLatin1String("topRight"), aiTopRight)),
      right(createAnchor(QLatin1String("right"), aiRight)),
      bottom(createAnchor(QLatin1String("bottom"), aiBottom)),
      bottomLeft(createAnchor(QLatin1String("bottomLeft"), aiBottomLeft)),
      left(createAnchor(QLatin1String("left"), aiLeft)), mScaled(false),
      mScaledPixmapInvalidated(true), mAspectRatioMode(Qt::KeepAspectRatio),
      mTransformationMode(Qt::SmoothTransformation) {
  topLeft->setCoords(0, 1);
  bottomRight->setCoords(1, 0);

  setPen(Qt::NoPen);
  setSelectedPen(QPen(Qt::blue));
}

QCPItemPixmap::~QCPItemPixmap() {}

void QCPItemPixmap::setPixmap(const QPixmap &pixmap) {
  mPixmap = pixmap;
  mScaledPixmapInvalidated = true;
  if (mPixmap.isNull())
    qDebug() << Q_FUNC_INFO << "pixmap is null";
}

void QCPItemPixmap::setScaled(bool scaled, Qt::AspectRatioMode aspectRatioMode,
                              Qt::TransformationMode transformationMode) {
  mScaled = scaled;
  mAspectRatioMode = aspectRatioMode;
  mTransformationMode = transformationMode;
  mScaledPixmapInvalidated = true;
}

void QCPItemPixmap::setPen(const QPen &pen) { mPen = pen; }

void QCPItemPixmap::setSelectedPen(const QPen &pen) { mSelectedPen = pen; }

double QCPItemPixmap::selectTest(const QPointF &pos, bool onlySelectable,
                                 QVariant *details) const {
  Q_UNUSED(details)
  if (onlySelectable && !mSelectable)
    return -1;

  return rectDistance(getFinalRect(), pos, true);
}

void QCPItemPixmap::draw(QCPPainter *painter) {
  bool flipHorz = false;
  bool flipVert = false;
  QRect rect = getFinalRect(&flipHorz, &flipVert);
  double clipPad = mainPen().style() == Qt::NoPen ? 0 : mainPen().widthF();
  QRect boundingRect = rect.adjusted(-clipPad, -clipPad, clipPad, clipPad);
  if (boundingRect.intersects(clipRect())) {
    updateScaledPixmap(rect, flipHorz, flipVert);
    painter->drawPixmap(rect.topLeft(), mScaled ? mScaledPixmap : mPixmap);
    QPen pen = mainPen();
    if (pen.style() != Qt::NoPen) {
      painter->setPen(pen);
      painter->setBrush(Qt::NoBrush);
      painter->drawRect(rect);
    }
  }
}

QPointF QCPItemPixmap::anchorPixelPosition(int anchorId) const {
  bool flipHorz;
  bool flipVert;
  QRect rect = getFinalRect(&flipHorz, &flipVert);

  if (flipHorz)
    rect.adjust(rect.width(), 0, -rect.width(), 0);
  if (flipVert)
    rect.adjust(0, rect.height(), 0, -rect.height());

  switch (anchorId) {
  case aiTop:
    return (rect.topLeft() + rect.topRight()) * 0.5;
  case aiTopRight:
    return rect.topRight();
  case aiRight:
    return (rect.topRight() + rect.bottomRight()) * 0.5;
  case aiBottom:
    return (rect.bottomLeft() + rect.bottomRight()) * 0.5;
  case aiBottomLeft:
    return rect.bottomLeft();
  case aiLeft:
    return (rect.topLeft() + rect.bottomLeft()) * 0.5;
    ;
  }

  qDebug() << Q_FUNC_INFO << "invalid anchorId" << anchorId;
  return QPointF();
}

void QCPItemPixmap::updateScaledPixmap(QRect finalRect, bool flipHorz,
                                       bool flipVert) {
  if (mPixmap.isNull())
    return;

  if (mScaled) {
#ifdef QCP_DEVICEPIXELRATIO_SUPPORTED
    double devicePixelRatio = mPixmap.devicePixelRatio();
#else
    double devicePixelRatio = 1.0;
#endif
    if (finalRect.isNull())
      finalRect = getFinalRect(&flipHorz, &flipVert);
    if (mScaledPixmapInvalidated ||
        finalRect.size() != mScaledPixmap.size() / devicePixelRatio) {
      mScaledPixmap = mPixmap.scaled(finalRect.size() * devicePixelRatio,
                                     mAspectRatioMode, mTransformationMode);
      if (flipHorz || flipVert)
        mScaledPixmap = QPixmap::fromImage(
            mScaledPixmap.toImage().mirrored(flipHorz, flipVert));
#ifdef QCP_DEVICEPIXELRATIO_SUPPORTED
      mScaledPixmap.setDevicePixelRatio(devicePixelRatio);
#endif
    }
  } else if (!mScaledPixmap.isNull())
    mScaledPixmap = QPixmap();
  mScaledPixmapInvalidated = false;
}

QRect QCPItemPixmap::getFinalRect(bool *flippedHorz, bool *flippedVert) const {
  QRect result;
  bool flipHorz = false;
  bool flipVert = false;
  QPoint p1 = topLeft->pixelPosition().toPoint();
  QPoint p2 = bottomRight->pixelPosition().toPoint();
  if (p1 == p2)
    return QRect(p1, QSize(0, 0));
  if (mScaled) {
    QSize newSize = QSize(p2.x() - p1.x(), p2.y() - p1.y());
    QPoint topLeft = p1;
    if (newSize.width() < 0) {
      flipHorz = true;
      newSize.rwidth() *= -1;
      topLeft.setX(p2.x());
    }
    if (newSize.height() < 0) {
      flipVert = true;
      newSize.rheight() *= -1;
      topLeft.setY(p2.y());
    }
    QSize scaledSize = mPixmap.size();
#ifdef QCP_DEVICEPIXELRATIO_SUPPORTED
    scaledSize /= mPixmap.devicePixelRatio();
    scaledSize.scale(newSize * mPixmap.devicePixelRatio(), mAspectRatioMode);
#else
    scaledSize.scale(newSize, mAspectRatioMode);
#endif
    result = QRect(topLeft, scaledSize);
  } else {
#ifdef QCP_DEVICEPIXELRATIO_SUPPORTED
    result = QRect(p1, mPixmap.size() / mPixmap.devicePixelRatio());
#else
    result = QRect(p1, mPixmap.size());
#endif
  }
  if (flippedHorz)
    *flippedHorz = flipHorz;
  if (flippedVert)
    *flippedVert = flipVert;
  return result;
}

QPen QCPItemPixmap::mainPen() const { return mSelected ? mSelectedPen : mPen; }

QCPItemTracer::QCPItemTracer(QCustomPlot *parentPlot)
    : QCPAbstractItem(parentPlot),
      position(createPosition(QLatin1String("position"))), mSize(6),
      mStyle(tsCrosshair), mGraph(0), mGraphKey(0), mInterpolating(false) {
  position->setCoords(0, 0);

  setBrush(Qt::NoBrush);
  setSelectedBrush(Qt::NoBrush);
  setPen(QPen(Qt::black));
  setSelectedPen(QPen(Qt::blue, 2));
}

QCPItemTracer::~QCPItemTracer() {}

void QCPItemTracer::setPen(const QPen &pen) { mPen = pen; }

void QCPItemTracer::setSelectedPen(const QPen &pen) { mSelectedPen = pen; }

void QCPItemTracer::setBrush(const QBrush &brush) { mBrush = brush; }

void QCPItemTracer::setSelectedBrush(const QBrush &brush) {
  mSelectedBrush = brush;
}

void QCPItemTracer::setSize(double size) { mSize = size; }

void QCPItemTracer::setStyle(QCPItemTracer::TracerStyle style) {
  mStyle = style;
}

void QCPItemTracer::setGraph(QCPGraph *graph) {
  if (graph) {
    if (graph->parentPlot() == mParentPlot) {
      position->setType(QCPItemPosition::ptPlotCoords);
      position->setAxes(graph->keyAxis(), graph->valueAxis());
      mGraph = graph;
      updatePosition();
    } else
      qDebug() << Q_FUNC_INFO
               << "graph isn't in same QCustomPlot instance as this item";
  } else {
    mGraph = 0;
  }
}

void QCPItemTracer::setGraphKey(double key) { mGraphKey = key; }

void QCPItemTracer::setInterpolating(bool enabled) { mInterpolating = enabled; }

double QCPItemTracer::selectTest(const QPointF &pos, bool onlySelectable,
                                 QVariant *details) const {
  Q_UNUSED(details)
  if (onlySelectable && !mSelectable)
    return -1;

  QPointF center(position->pixelPosition());
  double w = mSize / 2.0;
  QRect clip = clipRect();
  switch (mStyle) {
  case tsNone:
    return -1;
  case tsPlus: {
    if (clipRect().intersects(
            QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect()))
      return qSqrt(qMin(QCPVector2D(pos).distanceSquaredToLine(
                            center + QPointF(-w, 0), center + QPointF(w, 0)),
                        QCPVector2D(pos).distanceSquaredToLine(
                            center + QPointF(0, -w), center + QPointF(0, w))));
    break;
  }
  case tsCrosshair: {
    return qSqrt(qMin(QCPVector2D(pos).distanceSquaredToLine(
                          QCPVector2D(clip.left(), center.y()),
                          QCPVector2D(clip.right(), center.y())),
                      QCPVector2D(pos).distanceSquaredToLine(
                          QCPVector2D(center.x(), clip.top()),
                          QCPVector2D(center.x(), clip.bottom()))));
  }
  case tsCircle: {
    if (clip.intersects(
            QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect())) {
      double centerDist = QCPVector2D(center - pos).length();
      double circleLine = w;
      double result = qAbs(centerDist - circleLine);

      if (result > mParentPlot->selectionTolerance() * 0.99 &&
          mBrush.style() != Qt::NoBrush && mBrush.color().alpha() != 0) {
        if (centerDist <= circleLine)
          result = mParentPlot->selectionTolerance() * 0.99;
      }
      return result;
    }
    break;
  }
  case tsSquare: {
    if (clip.intersects(
            QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect())) {
      QRectF rect = QRectF(center - QPointF(w, w), center + QPointF(w, w));
      bool filledRect =
          mBrush.style() != Qt::NoBrush && mBrush.color().alpha() != 0;
      return rectDistance(rect, pos, filledRect);
    }
    break;
  }
  }
  return -1;
}

void QCPItemTracer::draw(QCPPainter *painter) {
  updatePosition();
  if (mStyle == tsNone)
    return;

  painter->setPen(mainPen());
  painter->setBrush(mainBrush());
  QPointF center(position->pixelPosition());
  double w = mSize / 2.0;
  QRect clip = clipRect();
  switch (mStyle) {
  case tsNone:
    return;
  case tsPlus: {
    if (clip.intersects(
            QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect())) {
      painter->drawLine(
          QLineF(center + QPointF(-w, 0), center + QPointF(w, 0)));
      painter->drawLine(
          QLineF(center + QPointF(0, -w), center + QPointF(0, w)));
    }
    break;
  }
  case tsCrosshair: {
    if (center.y() > clip.top() && center.y() < clip.bottom())
      painter->drawLine(
          QLineF(clip.left(), center.y(), clip.right(), center.y()));
    if (center.x() > clip.left() && center.x() < clip.right())
      painter->drawLine(
          QLineF(center.x(), clip.top(), center.x(), clip.bottom()));
    break;
  }
  case tsCircle: {
    if (clip.intersects(
            QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect()))
      painter->drawEllipse(center, w, w);
    break;
  }
  case tsSquare: {
    if (clip.intersects(
            QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect()))
      painter->drawRect(QRectF(center - QPointF(w, w), center + QPointF(w, w)));
    break;
  }
  }
}

void QCPItemTracer::updatePosition() {
  if (mGraph) {
    if (mParentPlot->hasPlottable(mGraph)) {
      if (mGraph->data()->size() > 1) {
        QCPGraphDataContainer::const_iterator first =
            mGraph->data()->constBegin();
        QCPGraphDataContainer::const_iterator last =
            mGraph->data()->constEnd() - 1;
        if (mGraphKey <= first->key)
          position->setCoords(first->key, first->value);
        else if (mGraphKey >= last->key)
          position->setCoords(last->key, last->value);
        else {
          QCPGraphDataContainer::const_iterator it =
              mGraph->data()->findBegin(mGraphKey);
          if (it != mGraph->data()->constEnd())

          {
            QCPGraphDataContainer::const_iterator prevIt = it;
            ++it;

            if (mInterpolating) {
              double slope = 0;
              if (!qFuzzyCompare((double)it->key, (double)prevIt->key))
                slope = (it->value - prevIt->value) / (it->key - prevIt->key);
              position->setCoords(mGraphKey, (mGraphKey - prevIt->key) * slope +
                                                 prevIt->value);
            } else {
              if (mGraphKey < (prevIt->key + it->key) * 0.5)
                position->setCoords(prevIt->key, prevIt->value);
              else
                position->setCoords(it->key, it->value);
            }
          } else

            position->setCoords(it->key, it->value);
        }
      } else if (mGraph->data()->size() == 1) {
        QCPGraphDataContainer::const_iterator it = mGraph->data()->constBegin();
        position->setCoords(it->key, it->value);
      } else
        qDebug() << Q_FUNC_INFO << "graph has no data";
    } else
      qDebug() << Q_FUNC_INFO
               << "graph not contained in QCustomPlot instance (anymore)";
  }
}

QPen QCPItemTracer::mainPen() const { return mSelected ? mSelectedPen : mPen; }

QBrush QCPItemTracer::mainBrush() const {
  return mSelected ? mSelectedBrush : mBrush;
}

QCPItemBracket::QCPItemBracket(QCustomPlot *parentPlot)
    : QCPAbstractItem(parentPlot), left(createPosition(QLatin1String("left"))),
      right(createPosition(QLatin1String("right"))),
      center(createAnchor(QLatin1String("center"), aiCenter)), mLength(8),
      mStyle(bsCalligraphic) {
  left->setCoords(0, 0);
  right->setCoords(1, 1);

  setPen(QPen(Qt::black));
  setSelectedPen(QPen(Qt::blue, 2));
}

QCPItemBracket::~QCPItemBracket() {}

void QCPItemBracket::setPen(const QPen &pen) { mPen = pen; }

void QCPItemBracket::setSelectedPen(const QPen &pen) { mSelectedPen = pen; }

void QCPItemBracket::setLength(double length) { mLength = length; }

void QCPItemBracket::setStyle(QCPItemBracket::BracketStyle style) {
  mStyle = style;
}

double QCPItemBracket::selectTest(const QPointF &pos, bool onlySelectable,
                                  QVariant *details) const {
  Q_UNUSED(details)
  if (onlySelectable && !mSelectable)
    return -1;

  QCPVector2D p(pos);
  QCPVector2D leftVec(left->pixelPosition());
  QCPVector2D rightVec(right->pixelPosition());
  if (leftVec.toPoint() == rightVec.toPoint())
    return -1;

  QCPVector2D widthVec = (rightVec - leftVec) * 0.5;
  QCPVector2D lengthVec = widthVec.perpendicular().normalized() * mLength;
  QCPVector2D centerVec = (rightVec + leftVec) * 0.5 - lengthVec;

  switch (mStyle) {
  case QCPItemBracket::bsSquare:
  case QCPItemBracket::bsRound: {
    double a =
        p.distanceSquaredToLine(centerVec - widthVec, centerVec + widthVec);
    double b = p.distanceSquaredToLine(centerVec - widthVec + lengthVec,
                                       centerVec - widthVec);
    double c = p.distanceSquaredToLine(centerVec + widthVec + lengthVec,
                                       centerVec + widthVec);
    return qSqrt(qMin(qMin(a, b), c));
  }
  case QCPItemBracket::bsCurly:
  case QCPItemBracket::bsCalligraphic: {
    double a =
        p.distanceSquaredToLine(centerVec - widthVec * 0.75 + lengthVec * 0.15,
                                centerVec + lengthVec * 0.3);
    double b =
        p.distanceSquaredToLine(centerVec - widthVec + lengthVec * 0.7,
                                centerVec - widthVec * 0.75 + lengthVec * 0.15);
    double c =
        p.distanceSquaredToLine(centerVec + widthVec * 0.75 + lengthVec * 0.15,
                                centerVec + lengthVec * 0.3);
    double d =
        p.distanceSquaredToLine(centerVec + widthVec + lengthVec * 0.7,
                                centerVec + widthVec * 0.75 + lengthVec * 0.15);
    return qSqrt(qMin(qMin(a, b), qMin(c, d)));
  }
  }
  return -1;
}

void QCPItemBracket::draw(QCPPainter *painter) {
  QCPVector2D leftVec(left->pixelPosition());
  QCPVector2D rightVec(right->pixelPosition());
  if (leftVec.toPoint() == rightVec.toPoint())
    return;

  QCPVector2D widthVec = (rightVec - leftVec) * 0.5;
  QCPVector2D lengthVec = widthVec.perpendicular().normalized() * mLength;
  QCPVector2D centerVec = (rightVec + leftVec) * 0.5 - lengthVec;

  QPolygon boundingPoly;
  boundingPoly << leftVec.toPoint() << rightVec.toPoint()
               << (rightVec - lengthVec).toPoint()
               << (leftVec - lengthVec).toPoint();
  QRect clip = clipRect().adjusted(-mainPen().widthF(), -mainPen().widthF(),
                                   mainPen().widthF(), mainPen().widthF());
  if (clip.intersects(boundingPoly.boundingRect())) {
    painter->setPen(mainPen());
    switch (mStyle) {
    case bsSquare: {
      painter->drawLine((centerVec + widthVec).toPointF(),
                        (centerVec - widthVec).toPointF());
      painter->drawLine((centerVec + widthVec).toPointF(),
                        (centerVec + widthVec + lengthVec).toPointF());
      painter->drawLine((centerVec - widthVec).toPointF(),
                        (centerVec - widthVec + lengthVec).toPointF());
      break;
    }
    case bsRound: {
      painter->setBrush(Qt::NoBrush);
      QPainterPath path;
      path.moveTo((centerVec + widthVec + lengthVec).toPointF());
      path.cubicTo((centerVec + widthVec).toPointF(),
                   (centerVec + widthVec).toPointF(), centerVec.toPointF());
      path.cubicTo((centerVec - widthVec).toPointF(),
                   (centerVec - widthVec).toPointF(),
                   (centerVec - widthVec + lengthVec).toPointF());
      painter->drawPath(path);
      break;
    }
    case bsCurly: {
      painter->setBrush(Qt::NoBrush);
      QPainterPath path;
      path.moveTo((centerVec + widthVec + lengthVec).toPointF());
      path.cubicTo((centerVec + widthVec - lengthVec * 0.8).toPointF(),
                   (centerVec + 0.4 * widthVec + lengthVec).toPointF(),
                   centerVec.toPointF());
      path.cubicTo((centerVec - 0.4 * widthVec + lengthVec).toPointF(),
                   (centerVec - widthVec - lengthVec * 0.8).toPointF(),
                   (centerVec - widthVec + lengthVec).toPointF());
      painter->drawPath(path);
      break;
    }
    case bsCalligraphic: {
      painter->setPen(Qt::NoPen);
      painter->setBrush(QBrush(mainPen().color()));
      QPainterPath path;
      path.moveTo((centerVec + widthVec + lengthVec).toPointF());

      path.cubicTo((centerVec + widthVec - lengthVec * 0.8).toPointF(),
                   (centerVec + 0.4 * widthVec + 0.8 * lengthVec).toPointF(),
                   centerVec.toPointF());
      path.cubicTo((centerVec - 0.4 * widthVec + 0.8 * lengthVec).toPointF(),
                   (centerVec - widthVec - lengthVec * 0.8).toPointF(),
                   (centerVec - widthVec + lengthVec).toPointF());

      path.cubicTo((centerVec - widthVec - lengthVec * 0.5).toPointF(),
                   (centerVec - 0.2 * widthVec + 1.2 * lengthVec).toPointF(),
                   (centerVec + lengthVec * 0.2).toPointF());
      path.cubicTo((centerVec + 0.2 * widthVec + 1.2 * lengthVec).toPointF(),
                   (centerVec + widthVec - lengthVec * 0.5).toPointF(),
                   (centerVec + widthVec + lengthVec).toPointF());

      painter->drawPath(path);
      break;
    }
    }
  }
}

QPointF QCPItemBracket::anchorPixelPosition(int anchorId) const {
  QCPVector2D leftVec(left->pixelPosition());
  QCPVector2D rightVec(right->pixelPosition());
  if (leftVec.toPoint() == rightVec.toPoint())
    return leftVec.toPointF();

  QCPVector2D widthVec = (rightVec - leftVec) * 0.5;
  QCPVector2D lengthVec = widthVec.perpendicular().normalized() * mLength;
  QCPVector2D centerVec = (rightVec + leftVec) * 0.5 - lengthVec;

  switch (anchorId) {
  case aiCenter:
    return centerVec.toPointF();
  }
  qDebug() << Q_FUNC_INFO << "invalid anchorId" << anchorId;
  return QPointF();
}

QPen QCPItemBracket::mainPen() const { return mSelected ? mSelectedPen : mPen; }
