/****************************************************************************
**
** Copyright (C) 2013 Digia Plc
** All rights reserved.
** For any questions to Digia, please use contact form at http://qt.digia.com
**
** This file is part of the QtDataVisualization module.
**
** Licensees holding valid Qt Enterprise licenses may use this file in
** accordance with the Qt Enterprise License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.
**
** If you have questions regarding the use of this file, please use
** contact form at http://qt.digia.com
**
****************************************************************************/

#include "declarativebars_p.h"
#include "declarativebarsrenderer_p.h"
#include "q3dvalueaxis.h"
#include "qitemmodelbardataproxy.h"

QT_DATAVISUALIZATION_BEGIN_NAMESPACE

const QString smoothString(QStringLiteral("Smooth"));

DeclarativeBars::DeclarativeBars(QQuickItem *parent)
    : QQuickItem(parent),
      m_shared(0),
      m_initialisedSize(0, 0),
      m_theme(QDataVis::ThemeDefault)
{
    setFlags(QQuickItem::ItemHasContents);
    setAcceptedMouseButtons(Qt::AllButtons);

    // TODO: These seem to have no effect; find a way to activate anti-aliasing
    setAntialiasing(true);
    setSmooth(true);

    // Create the shared component on the main GUI thread.
    m_shared = new Bars3DController(boundingRect().toRect());
    QObject::connect(m_shared, &Abstract3DController::shadowQualityChanged, this,
                     &DeclarativeBars::handleShadowQualityUpdate);
    QObject::connect(m_shared, &Bars3DController::selectedBarPosChanged, this,
                     &DeclarativeBars::selectedBarPosChanged);

    QItemModelBarDataProxy *proxy = new QItemModelBarDataProxy;
    m_shared->setActiveDataProxy(proxy);
}

DeclarativeBars::~DeclarativeBars()
{
    delete m_shared;
}

void DeclarativeBars::handleShadowQualityUpdate(QDataVis::ShadowQuality quality)
{
    emit shadowQualityChanged(quality);
}

QSGNode *DeclarativeBars::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    // If old node exists and has right size, reuse it.
    if (oldNode && m_initialisedSize == boundingRect().size().toSize()) {
        // Update bounding rectangle (that has same size as before).
        static_cast<DeclarativeBarsRenderer *>( oldNode )->setRect(boundingRect());
        return oldNode;
    }

    // Create a new render node when size changes or if there is no node yet
    m_initialisedSize = boundingRect().size().toSize();

    // Delete old node
    if (oldNode)
        delete oldNode;

    // Create a new one and set it's bounding rectangle
    DeclarativeBarsRenderer *node = new DeclarativeBarsRenderer(window(), m_shared);
    node->setRect(boundingRect());
    m_shared->setBoundingRect(boundingRect().toRect());
    return node;
}

void DeclarativeBars::setBarColor(const QColor &baseColor, bool uniform)
{
    m_shared->setObjectColor(baseColor, uniform);
}

void DeclarativeBars::setCameraPosition(qreal horizontal, qreal vertical, int distance)
{
    m_shared->setCameraPosition(GLfloat(horizontal), GLfloat(vertical), GLint(distance));
}

void DeclarativeBars::setDataProxy(QBarDataProxy *dataProxy)
{
    m_shared->setActiveDataProxy(dataProxy);
}

QBarDataProxy *DeclarativeBars::dataProxy() const
{
    return static_cast<QBarDataProxy *>(m_shared->activeDataProxy());
}

Q3DCategoryAxis *DeclarativeBars::rowAxis() const
{
    return static_cast<Q3DCategoryAxis *>(m_shared->axisX());
}

void DeclarativeBars::setRowAxis(Q3DCategoryAxis *axis)
{
    m_shared->setAxisX(axis);
}

Q3DValueAxis *DeclarativeBars::valueAxis() const
{
    return static_cast<Q3DValueAxis *>(m_shared->axisY());
}

void DeclarativeBars::setValueAxis(Q3DValueAxis *axis)
{
    m_shared->setAxisY(axis);
}

Q3DCategoryAxis *DeclarativeBars::columnAxis() const
{
    return static_cast<Q3DCategoryAxis *>(m_shared->axisZ());
}

void DeclarativeBars::setColumnAxis(Q3DCategoryAxis *axis)
{
    m_shared->setAxisZ(axis);
}

void DeclarativeBars::setBarThickness(qreal thicknessRatio)
{
    m_shared->setBarSpecs(GLfloat(thicknessRatio), barSpacing(), isBarSpacingRelative());
}

qreal DeclarativeBars::barThickness()
{
    return m_shared->barThickness();
}

void DeclarativeBars::setBarSpacing(QSizeF spacing)
{
    m_shared->setBarSpecs(GLfloat(barThickness()), spacing, isBarSpacingRelative());
}

QSizeF DeclarativeBars::barSpacing()
{
    return m_shared->barSpacing();
}

void DeclarativeBars::setBarSpacingRelative(bool relative)
{
    m_shared->setBarSpecs(GLfloat(barThickness()), barSpacing(), relative);
}

bool DeclarativeBars::isBarSpacingRelative()
{
    return m_shared->isBarSpecRelative();
}

void DeclarativeBars::setBarType(QDataVis::MeshStyle style)
{
    QString objFile = m_shared->meshFileName();
    bool smooth = objFile.endsWith(smoothString);
    m_shared->setBarType(style, smooth);
}

QDataVis::MeshStyle DeclarativeBars::barType()
{
    QString objFile = m_shared->meshFileName();
    if (objFile.contains("/sphere"))
        return QDataVis::Spheres;
    else
        return QDataVis::Dots;
}

void DeclarativeBars::setBarSmoothingEnabled(bool enabled)
{
    QString objFile = m_shared->meshFileName();
    if (objFile.endsWith(smoothString)) {
        if (enabled)
            return; // Already smooth; do nothing
        else // Rip Smooth off the end
            objFile.resize(objFile.indexOf(smoothString));
    } else {
        if (!enabled) // Already flat; do nothing
            return;
        else // Append Smooth to the end
            objFile.append(smoothString);
    }
    m_shared->setMeshFileName(objFile);
}

bool DeclarativeBars::isBarSmoothingEnabled()
{
    QString objFile = m_shared->meshFileName();
    return objFile.endsWith(smoothString);
}

void DeclarativeBars::setMeshFileName(const QString &objFileName)
{
    m_shared->setMeshFileName(objFileName);
}

QString DeclarativeBars::meshFileName()
{
    return m_shared->meshFileName();
}

void DeclarativeBars::setCameraPreset(QDataVis::CameraPreset preset)
{
    m_shared->setCameraPreset(preset);
}

QDataVis::CameraPreset DeclarativeBars::cameraPreset()
{
    return m_shared->cameraPreset();
}

void DeclarativeBars::setTheme(QDataVis::ColorTheme theme)
{
    // TODO: Implement correctly once "user-modifiable themes" (QTRD-2120) is implemented
    // We need to save this locally, as there are no getters for it in controller
    m_theme = theme;
    m_shared->setColorTheme(theme);
}

QDataVis::ColorTheme DeclarativeBars::theme()
{
    return m_theme;
}

void DeclarativeBars::setFont(const QFont &font)
{
    m_shared->setFont(font);
}

QFont DeclarativeBars::font()
{
    return m_shared->font();
}

void DeclarativeBars::setLabelTransparency(QDataVis::LabelTransparency transparency)
{
    m_shared->setLabelTransparency(transparency);
}

QDataVis::LabelTransparency DeclarativeBars::labelTransparency()
{
    return m_shared->labelTransparency();
}

void DeclarativeBars::setGridVisible(bool visible)
{
    m_shared->setGridEnabled(visible);
}

bool DeclarativeBars::isGridVisible()
{
    return m_shared->gridEnabled();
}

void DeclarativeBars::setBackgroundVisible(bool visible)
{
    m_shared->setBackgroundEnabled(visible);
}

bool DeclarativeBars::isBackgroundVisible()
{
    return m_shared->backgroundEnabled();
}

void DeclarativeBars::setSelectionMode(QDataVis::SelectionMode mode)
{
    m_shared->setSelectionMode(mode);
}

QDataVis::SelectionMode DeclarativeBars::selectionMode()
{
    return m_shared->selectionMode();
}

void DeclarativeBars::setShadowQuality(QDataVis::ShadowQuality quality)
{
    m_shared->setShadowQuality(quality);
}

QDataVis::ShadowQuality DeclarativeBars::shadowQuality()
{
    return m_shared->shadowQuality();
}

void DeclarativeBars::setItemLabelFormat(const QString &format)
{
    m_shared->activeDataProxy()->setItemLabelFormat(format);
}

QString DeclarativeBars::itemLabelFormat()
{
    return m_shared->activeDataProxy()->itemLabelFormat();
}

void DeclarativeBars::setSelectedBarPos(const QPointF &position)
{
    m_shared->setSelectedBarPos(position.toPoint());
}

QPointF DeclarativeBars::selectedBarPos() const
{
    return QPointF(m_shared->selectedBarPos());
}

void DeclarativeBars::mouseDoubleClickEvent(QMouseEvent *event)
{
#if defined(Q_OS_ANDROID)
    m_shared->mouseDoubleClickEvent(event);
#else
    Q_UNUSED(event)
#endif
}

void DeclarativeBars::touchEvent(QTouchEvent *event)
{
#if defined(Q_OS_ANDROID)
    m_shared->touchEvent(event);
    update();
#else
    Q_UNUSED(event)
#endif
}

void DeclarativeBars::mousePressEvent(QMouseEvent *event)
{
    QPoint mousePos = event->pos();
    //mousePos.setY(height() - mousePos.y());
    m_shared->mousePressEvent(event, mousePos);
}

void DeclarativeBars::mouseReleaseEvent(QMouseEvent *event)
{
    QPoint mousePos = event->pos();
    //mousePos.setY(height() - mousePos.y());
    m_shared->mouseReleaseEvent(event, mousePos);
}

void DeclarativeBars::mouseMoveEvent(QMouseEvent *event)
{
    QPoint mousePos = event->pos();
    //mousePos.setY(height() - mousePos.y());
    m_shared->mouseMoveEvent(event, mousePos);
}

void DeclarativeBars::wheelEvent(QWheelEvent *event)
{
    m_shared->wheelEvent(event);
}

QT_DATAVISUALIZATION_END_NAMESPACE