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

#include "surface3dcontroller_p.h"
#include "surface3drenderer_p.h"
#include "camerahelper_p.h"
#include "q3dabstractaxis_p.h"
#include "q3dvalueaxis_p.h"
#include "q3dcategoryaxis.h"
#include "qsurfacedataproxy_p.h"
#include "qsurface3dseries_p.h"

#include <QMatrix4x4>

#include <QDebug>

QT_DATAVISUALIZATION_BEGIN_NAMESPACE

Surface3DController::Surface3DController(QRect rect)
    : Abstract3DController(rect),
      m_renderer(0),
      m_isSmoothSurfaceEnabled(false),
      m_isSurfaceEnabled(true),
      m_isSurfaceGridEnabled(true),
      m_selectedPoint(invalidSelectionPosition()),
      m_selectedSeries(0)
{
    // Setting a null axis creates a new default axis according to orientation and graph type.
    // Note: these cannot be set in the Abstract3DController constructor, as they will call virtual
    //       functions implemented by subclasses.
    setAxisX(0);
    setAxisY(0);
    setAxisZ(0);

    // Set the default from the theme
    m_userDefinedGradient = theme()->baseGradient();
}

Surface3DController::~Surface3DController()
{
}

void Surface3DController::initializeOpenGL()
{
    // Initialization is called multiple times when Qt Quick components are used
    if (isInitialized())
        return;

    m_renderer = new Surface3DRenderer(this);
    setRenderer(m_renderer);
    synchDataToRenderer();
    QObject::connect(m_renderer, &Surface3DRenderer::pointClicked, this,
                     &Surface3DController::handlePointClicked, Qt::QueuedConnection);
    QObject::connect(m_renderer, &Surface3DRenderer::requestSmoothSurface, this,
                     &Surface3DController::handleRequestSmoothSurface, Qt::QueuedConnection);
    emitNeedRender();
}

void Surface3DController::synchDataToRenderer()
{
    if (!isInitialized())
        return;

    if (m_changeTracker.surfaceGridChanged) {
        m_renderer->updateSurfaceGridStatus(m_isSurfaceGridEnabled);
        m_changeTracker.surfaceGridChanged = false;
    }

    Abstract3DController::synchDataToRenderer();

    // Notify changes to renderer
    if (m_changeTracker.gradientColorChanged) {
        m_renderer->updateSurfaceGradient(m_userDefinedGradient);
        m_changeTracker.gradientColorChanged = false;
    }

    if (m_changeTracker.smoothStatusChanged) {
        bool oldSmoothStatus = m_isSmoothSurfaceEnabled;
        m_isSmoothSurfaceEnabled = m_renderer->updateSmoothStatus(m_isSmoothSurfaceEnabled);
        m_changeTracker.smoothStatusChanged = false;
        if (oldSmoothStatus != m_isSmoothSurfaceEnabled)
            emit smoothSurfaceEnabledChanged(m_isSmoothSurfaceEnabled);
    }

    if (m_changeTracker.selectedPointChanged) {
        m_renderer->updateSelectedPoint(m_selectedPoint, m_selectedSeries);
        m_changeTracker.selectedPointChanged = false;
    }
}

void Surface3DController::handleAxisAutoAdjustRangeChangedInOrientation(Q3DAbstractAxis::AxisOrientation orientation, bool autoAdjust)
{
    Q_UNUSED(orientation)
    Q_UNUSED(autoAdjust)

    adjustValueAxisRange();
}

void Surface3DController::handleAxisRangeChangedBySender(QObject *sender)
{
    Abstract3DController::handleAxisRangeChangedBySender(sender);

    // Update selected point - may be moved offscreen
    setSelectedPoint(m_selectedPoint, m_selectedSeries);
}

void Surface3DController::handleSeriesVisibilityChangedBySender(QObject *sender)
{
    Abstract3DController::handleSeriesVisibilityChangedBySender(sender);

    // Visibility changes may require disabling/enabling slicing,
    // so just reset selection to ensure everything is still valid.
    setSelectedPoint(m_selectedPoint, m_selectedSeries);
}

QPoint Surface3DController::invalidSelectionPosition()
{
    static QPoint invalidSelectionPoint(-1, -1);
    return invalidSelectionPoint;
}

void Surface3DController::addSeries(QAbstract3DSeries *series)
{
    Q_ASSERT(series && series->type() == QAbstract3DSeries::SeriesTypeSurface);

    if (!m_seriesList.size()) {
        Abstract3DController::addSeries(series);

        adjustValueAxisRange();
    } else {
        qWarning("Surface graph only supports a single series.");
    }

    QSurface3DSeries *surfaceSeries =  static_cast<QSurface3DSeries *>(series);
    if (surfaceSeries->selectedPoint() != invalidSelectionPosition())
        setSelectedPoint(surfaceSeries->selectedPoint(), surfaceSeries);
}

void Surface3DController::removeSeries(QAbstract3DSeries *series)
{
    if (series && series->d_ptr->m_controller == this) {
        Abstract3DController::removeSeries(series);

        if (m_selectedSeries == series)
            setSelectedPoint(invalidSelectionPosition(), 0);

        adjustValueAxisRange();
    }
}

QList<QSurface3DSeries *> Surface3DController::surfaceSeriesList()
{
    QList<QAbstract3DSeries *> abstractSeriesList = seriesList();
    QList<QSurface3DSeries *> surfaceSeriesList;
    foreach (QAbstract3DSeries *abstractSeries, abstractSeriesList) {
        QSurface3DSeries *surfaceSeries = qobject_cast<QSurface3DSeries *>(abstractSeries);
        if (surfaceSeries)
            surfaceSeriesList.append(surfaceSeries);
    }

    return surfaceSeriesList;
}

void Surface3DController::setSmoothSurface(bool enable)
{
    if (enable != m_isSmoothSurfaceEnabled) {
        m_isSmoothSurfaceEnabled = enable;
        m_changeTracker.smoothStatusChanged = true;
        emit smoothSurfaceEnabledChanged(enable);
        emitNeedRender();
    }
}

bool Surface3DController::smoothSurface()
{
    return m_isSmoothSurfaceEnabled;
}

void Surface3DController::setSurfaceGrid(bool enable)
{
    if (enable != m_isSurfaceGridEnabled) {
        m_isSurfaceGridEnabled = enable;
        m_changeTracker.surfaceGridChanged = true;
        emit surfaceGridEnabledChanged(enable);
        emitNeedRender();
        m_isDataDirty = true;
    }
}

bool Surface3DController::surfaceGrid()
{
    return m_isSurfaceGridEnabled;
}

void Surface3DController::setGradient(const QLinearGradient &gradient)
{
    if (gradient != m_userDefinedGradient) {
        m_userDefinedGradient = gradient;
        m_changeTracker.gradientColorChanged = true;
        emitNeedRender();
    }
}

QLinearGradient Surface3DController::gradient() const
{
    return m_userDefinedGradient;
}

void Surface3DController::setGradientColorAt(qreal pos, const QColor &color)
{
    m_userDefinedGradient.setColorAt(pos, color);
    m_changeTracker.gradientColorChanged = true;
    emitNeedRender();
}

void Surface3DController::setSelectionMode(QDataVis::SelectionFlags mode)
{
    // Currently surface only supports row and column modes when also slicing
    if ((mode.testFlag(QDataVis::SelectionRow) || mode.testFlag(QDataVis::SelectionColumn))
            && !mode.testFlag(QDataVis::SelectionSlice)) {
        qWarning("Unsupported selection mode.");
        return;
    } else if (mode.testFlag(QDataVis::SelectionSlice)
               && (mode.testFlag(QDataVis::SelectionRow) == mode.testFlag(QDataVis::SelectionColumn))) {
        qWarning("Must specify one of either row or column selection mode in conjunction with slicing mode.");
    } else {
        QDataVis::SelectionFlags oldMode = selectionMode();

        Abstract3DController::setSelectionMode(mode);

        if (mode != oldMode) {
            // Refresh selection upon mode change to ensure slicing is correctly updated
            // according to series the visibility.
            setSelectedPoint(m_selectedPoint, m_selectedSeries);

            // Special case: Always deactivate slicing when changing away from slice
            // automanagement, as this can't be handled in setSelectedBar.
            if (!mode.testFlag(QDataVis::SelectionSlice)
                    && oldMode.testFlag(QDataVis::SelectionSlice)) {
                scene()->setSlicingActive(false);
            }
        }
    }
}

void Surface3DController::setSelectedPoint(const QPoint &position, QSurface3DSeries *series)
{
    // If the selection targets non-existent point, clear selection instead.
    QPoint pos = position;

    // Series may already have been removed, so check it before setting the selection.
    if (!m_seriesList.contains(series))
        series = 0;

    const QSurfaceDataProxy *proxy = 0;
    if (series)
        proxy = series->dataProxy();

    if (!proxy)
        pos = invalidSelectionPosition();

    if (pos != invalidSelectionPosition()) {
        int maxRow = proxy->rowCount() - 1;
        int maxCol = proxy->columnCount() - 1;

        if (pos.x() < 0 || pos.x() > maxRow || pos.y() < 0 || pos.y() > maxCol)
            pos = invalidSelectionPosition();
    }

    if (selectionMode().testFlag(QDataVis::SelectionSlice)) {
        if (pos == invalidSelectionPosition() || !series->isVisible()) {
            scene()->setSlicingActive(false);
        } else {
            // If the selected point is outside data window, or there is no selected point, disable slicing
            // TODO: (QTRD-2351) This logic doesn't match the renderer logic for non straight surfaces,
            // but that logic needs to change anyway, so this is good for now.
            float axisMinX = m_axisX->min();
            float axisMaxX = m_axisX->max();
            float axisMinZ = m_axisZ->min();
            float axisMaxZ = m_axisZ->max();

            QSurfaceDataItem item = proxy->array()->at(pos.x())->at(pos.y());
            if (item.x() < axisMinX || item.x() > axisMaxX
                    || item.z() < axisMinZ || item.z() > axisMaxZ) {
                scene()->setSlicingActive(false);
            } else {
                scene()->setSlicingActive(true);
            }
        }
        emitNeedRender();
    }

    if (pos != m_selectedPoint) {
        m_selectedPoint = pos;
        m_selectedSeries = series;
        m_changeTracker.selectedPointChanged = true;

        // Clear selection from other series and finally set new selection to the specified series
        foreach (QAbstract3DSeries *otherSeries, m_seriesList) {
            QSurface3DSeries *surfaceSeries = static_cast<QSurface3DSeries *>(otherSeries);
            if (surfaceSeries != m_selectedSeries)
                surfaceSeries->dptr()->setSelectedPoint(invalidSelectionPosition());
        }
        if (m_selectedSeries)
            m_selectedSeries->dptr()->setSelectedPoint(m_selectedPoint);

        emitNeedRender();
    }
}

void Surface3DController::handleArrayReset()
{
    adjustValueAxisRange();
    m_isDataDirty = true;
    // Clear selection unless still valid
    setSelectedPoint(m_selectedPoint, m_selectedSeries);
    emitNeedRender();
}

void Surface3DController::handlePointClicked(const QPoint &position, QSurface3DSeries *series)
{
    setSelectedPoint(position, series);
    // TODO: pass clicked to parent. (QTRD-2517)
    // TODO: Also hover needed? (QTRD-2131)
}

void Surface3DController::handleRequestSmoothSurface(bool enable)
{
    setSmoothSurface(enable);
    emitNeedRender();
}

void Surface3DController::adjustValueAxisRange()
{
    if (m_seriesList.size()) {
        QVector3D minLimits;
        QVector3D maxLimits;
        QSurface3DSeries *series = static_cast<QSurface3DSeries *>(m_seriesList.at(0));
        static_cast<QSurfaceDataProxy *>(series->dataProxy())->dptrc()->limitValues(minLimits,
                                                                                   maxLimits);
        Q3DValueAxis *valueAxis = static_cast<Q3DValueAxis *>(m_axisX);
        if (valueAxis && valueAxis->isAutoAdjustRange()) {
            if (minLimits.x() != maxLimits.x())
                valueAxis->dptr()->setRange(minLimits.x(), maxLimits.x());
            else
                valueAxis->dptr()->setRange(minLimits.x() - 1.0f, minLimits.x() + 1.0f); // Default to some valid range
        }

        valueAxis = static_cast<Q3DValueAxis *>(m_axisY);
        if (valueAxis && valueAxis->isAutoAdjustRange()) {
            if (minLimits.y() != maxLimits.y())
                valueAxis->dptr()->setRange(minLimits.y(), maxLimits.y());
            else
                valueAxis->dptr()->setRange(minLimits.y() - 1.0f, minLimits.y() + 1.0f); // Default to some valid range
        }

        valueAxis = static_cast<Q3DValueAxis *>(m_axisZ);
        if (valueAxis && valueAxis->isAutoAdjustRange()) {
            if (minLimits.z() != maxLimits.z())
                valueAxis->dptr()->setRange(minLimits.z(), maxLimits.z());
            else
                valueAxis->dptr()->setRange(minLimits.z() - 1.0f, minLimits.z() + 1.0f); // Default to some valid range
        }
    }
}

QT_DATAVISUALIZATION_END_NAMESPACE
