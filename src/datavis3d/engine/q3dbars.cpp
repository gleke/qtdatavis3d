/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the documentation of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "q3dbars.h"
#include "q3dbars_p.h"
#include "meshloader_p.h"
#include "vertexindexer_p.h"
#include "camerahelper_p.h"
#include "sampledata_p.h"

#include <QMatrix4x4>
#include <QOpenGLShaderProgram>
#include <QOpenGLPaintDevice>
#include <QPainter>
#include <QScreen>
#include <QMouseEvent>

#include <qmath.h>

#include <QDebug>

QTCOMMERCIALDATAVIS3D_BEGIN_NAMESPACE

#define USE_HAX0R_SELECTION // keep this defined until the "real" method works

const float zComp = 3.0f; // Compensation for z position; move all objects to positive z, as shader can't handle negative values correctly

Q3DBars::Q3DBars()
    : d_ptr(new Q3DBarsPrivate(this))
{
}

Q3DBars::~Q3DBars()
{
}

void Q3DBars::initialize()
{
    // Initialize shaders
    if (!d_ptr->m_uniformColor) {
        d_ptr->initShaders(QStringLiteral(":/shaders/vertex")
                           , QStringLiteral(":/shaders/fragmentColorOnY"));
    }
    else {
        d_ptr->initShaders(QStringLiteral(":/shaders/vertex")
                           , QStringLiteral(":/shaders/fragment"));
    }
    d_ptr->initBackgroundShaders(QStringLiteral(":/shaders/vertex")
                                 , QStringLiteral(":/shaders/fragment"));
    d_ptr->initSelectionShader();

#ifndef USE_HAX0R_SELECTION
    // Init the selection buffer
    d_ptr->initSelectionBuffer();
#endif

    // Load default mesh
    d_ptr->loadBarMesh();

    // Load background mesh
    d_ptr->loadBackgroundMesh();

    // Set clear color
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Set OpenGL features
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Set initial camera position
    // X must be 0 for rotation to work - we can use "setCameraRotation" for setting it later
    CameraHelper::setDefaultCameraOrientation(QVector3D(0.0f, 0.0f, 6.0f + zComp)
                                              , QVector3D(0.0f, 0.0f, zComp)
                                              , QVector3D(0.0f, 1.0f, 0.0f));

    CameraHelper::setCameraRotation(QPointF(d_ptr->m_horizontalRotation
                                            , d_ptr->m_verticalRotation));

    // Set view port
    glViewport(0, 0, width(), height());

    // Set initialized -flag
    d_ptr->m_isInitialized = true;
}

void Q3DBars::render()
{
    if (!d_ptr->m_isInitialized)
        return;

    if (d_ptr->m_paintDevice) {
        QPainter painter(d_ptr->m_paintDevice);
        render(&painter);
        painter.end();
    }
    else {
        d_ptr->m_paintDevice = getDevice();
    }
}

void Q3DBars::render(QPainter *painter)
{
    painter->beginNativePainting();
    // Set OpenGL features
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    // Do native OpenGL rendering
    drawScene();
    painter->endNativePainting();

    // If a bar is selected, display it's value
    SampleData *data = d_ptr->m_selectedBar;
    if (data) {
        glDisable(GL_DEPTH_TEST);
        painter->save();
        painter->setCompositionMode(QPainter::CompositionMode_Source);//CompositionMode_SourceOver);
        painter->setPen(Qt::white);
        painter->setFont(QFont(QStringLiteral("Arial"), 15));
        painter->setBackgroundMode(Qt::OpaqueMode);
        painter->setBackground(QBrush(Qt::black));
        painter->drawText(data->position().x() - 50, data->position().y() - 30, 100, 30
                          , Qt::AlignCenter | Qt::AlignVCenter | Qt::TextExpandTabs
                          , data->valueStr());
        //painter->drawText(d_ptr->m_mousePos.x() - 50, d_ptr->m_mousePos.y() - 30, 100, 30
        //                  , Qt::AlignCenter | Qt::AlignVCenter | Qt::TextExpandTabs
        //                  , QStringLiteral("kukkuu"));
        painter->restore();
    }
}

void Q3DBars::drawScene()
{
    int startBar = 0;
    int stopBar = 0;
    int stepBar = 0;

    int startRow = 0;
    int stopRow = 0;
    int stepRow = 0;

    float backgroundRotation = 0;

    float barPos = 0;
    float rowPos = 0;

    static QVector3D selection = QVector3D(0, 0, 0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 projectionMatrix;
    projectionMatrix.perspective(45.0f, (float)width() / (float)height(), 0.1f, 100.0f);

    QMatrix4x4 viewMatrix = CameraHelper::calculateViewMatrix(d_ptr->m_mousePos, d_ptr->m_zoomLevel
                                                              , width(), height());

    // Calculate drawing order
    //qDebug() << "viewMatrix z" << viewMatrix.row(0).z(); // jos negatiivinen, käännä bar -piirtojärjestys
    //qDebug() << "viewMatrix x" << viewMatrix.row(0).x(); // jos negatiivinen, käännä row -piirtojärjestys
    // TODO: Needs more tuning unless we get depth test working correctly
    if (viewMatrix.row(0).x() < 0) {
        startRow = 0;
        stopRow = d_ptr->m_sampleCount.y();
        stepRow = 1;
    }
    else {
        startRow = d_ptr->m_sampleCount.y() - 1;
        stopRow = -1;
        stepRow = -1;
    }
    if (viewMatrix.row(0).z() > 0) {
        startBar = 0;
        stopBar = d_ptr->m_sampleCount.x();
        stepBar = 1;
    }
    else {
        startBar = d_ptr->m_sampleCount.x() - 1;
        stopBar = -1;
        stepBar = -1;
    }

    // calculate background rotation based on view matrix rotation
    if (viewMatrix.row(0).x() >= 0 && viewMatrix.row(0).z() <= 0) {
        backgroundRotation = 270.0f;
    }
    else if (viewMatrix.row(0).x() > 0 && viewMatrix.row(0).z() > 0) {
        backgroundRotation = 180.0f;
    }
    else if (viewMatrix.row(0).x() <= 0 && viewMatrix.row(0).z() >= 0) {
        backgroundRotation = 90.0f;
    }
    else if (viewMatrix.row(0).x() < 0 && viewMatrix.row(0).z() < 0) {
        backgroundRotation = 0.0f;
    }

    // TODO: Rotate light with camera (position light a bit above camera)
    QVector3D lightPos = QVector3D(0.5f, 3.0f, zComp * 2.5f);

    // Bind selection shader
    d_ptr->m_selectionShader->bind();

    // Draw bars to selection buffer
    glDisable(GL_DITHER); // disable dithering, it may affect colors if enabled
    for (int row = startRow; row != stopRow; row += stepRow) {
        for (int bar = startBar; bar != stopBar; bar += stepBar) {
            float barHeight = d_ptr->m_dataSet.at(row).at(bar);
            QMatrix4x4 modelMatrix;
            QMatrix4x4 MVPMatrix;
            barPos = (bar + 1) * (d_ptr->m_barSpacing.x());
            rowPos = (row + 1) * (d_ptr->m_barSpacing.y());
            modelMatrix.translate((d_ptr->m_rowWidth - barPos) / d_ptr->m_scaleFactorX
                                  , barHeight - 1.0f
                                  , (d_ptr->m_columnDepth - rowPos) / d_ptr->m_scaleFactorZ + zComp);
            modelMatrix.scale(QVector3D(d_ptr->m_scaleX, barHeight, d_ptr->m_scaleZ));

            MVPMatrix = projectionMatrix * viewMatrix * modelMatrix;

            // add +2 to avoid black
            QVector3D barColor = QVector3D((float)(row + 2) / (float)(d_ptr->m_sampleCount.y() + 2)
                                           , (float)(bar + 2) / (float)(d_ptr->m_sampleCount.x() + 2)
                                           , 0.0f);

            d_ptr->m_selectionShader->setUniformValue(d_ptr->m_mvpMatrixUniformSelection
                                                      , MVPMatrix);
            d_ptr->m_selectionShader->setUniformValue(d_ptr->m_colorUniformSelection
                                                      , barColor);

#ifdef USE_HAX0R_SELECTION
            // 1st attribute buffer : vertices
            glEnableVertexAttribArray(d_ptr->m_positionAttrSelection);
            glBindBuffer(GL_ARRAY_BUFFER, d_ptr->m_vertexbuffer);
            glVertexAttribPointer(d_ptr->m_positionAttrSelection
                                  , 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

            // Index buffer
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d_ptr->m_elementbuffer);

            // Draw the triangles
            glDrawElements(GL_TRIANGLES, d_ptr->m_indexCount, GL_UNSIGNED_SHORT, (void*)0);

            // Free buffers
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            glDisableVertexAttribArray(d_ptr->m_positionAttrSelection);
#else // TODO: fix this - doesn't work yet
            glBindFramebuffer(GL_FRAMEBUFFER, d_ptr->m_framebufferSelection);
            //glReadBuffer(GL_COLOR_ATTACHMENT0);

            // 1st attribute buffer : vertices
            glEnableVertexAttribArray(d_ptr->m_positionAttrSelection);
            glBindBuffer(GL_ARRAY_BUFFER, d_ptr->m_vertexbuffer);
            glVertexAttribPointer(d_ptr->m_positionAttrSelection
                                  , 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

            // Index buffer
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d_ptr->m_elementbuffer);

            // Draw the triangles
            GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
            glDrawElements(GL_TRIANGLES, d_ptr->m_indexCount, GL_UNSIGNED_SHORT, DrawBuffers);

            glDisableVertexAttribArray(d_ptr->m_positionAttrSelection);

            // Free buffers
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            //glReadBuffer(GL_NONE);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
        }
    }
    glEnable(GL_DITHER);

    // Read color under cursor
    // TODO: Move into a separate function?
    if (d_ptr->m_mousePressed) {
#ifndef USE_HAX0R_SELECTION
        glBindFramebuffer(GL_FRAMEBUFFER, d_ptr->m_framebufferSelection);
#endif
        // This is the only one that works with ANGLE (ES 2.0)
        // Item count will be limited to 256*256*256
        GLubyte pixel[4];
        glReadPixels(d_ptr->m_mousePos.x(), height() - d_ptr->m_mousePos.y(), 1, 1,
                     GL_RGBA, GL_UNSIGNED_BYTE, (void *)pixel);

        // These work with desktop OpenGL
        // They offer a lot higher possible object count and a possibility to use object id's
        //GLuint pixel2[3];
        //glReadPixels(d_ptr->m_mousePos.x(), height() - d_ptr->m_mousePos.y(), 1, 1,
        //             GL_RGB, GL_UNSIGNED_INT, (void *)pixel2);

        //GLfloat pixel3[3];
        //glReadPixels(d_ptr->m_mousePos.x(), height() - d_ptr->m_mousePos.y(), 1, 1,
        //             GL_RGB, GL_FLOAT, (void *)pixel3);

        //qDebug() << "rgba" << pixel[0] << pixel[1] << pixel[2];// << pixel[3];
        //qDebug() << "rgba2" << pixel2[0] << pixel2[1] << pixel2[2];
        //qDebug() << "rgba3" << pixel3[0] << pixel3[1] << pixel3[2];
        selection = QVector3D(pixel[0], pixel[1], pixel[2]);
        //qDebug() << selection;
#ifndef USE_HAX0R_SELECTION
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
    }

    // Release selection shader
    d_ptr->m_selectionShader->release();

    // Clear after selection
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Bind background shader
    d_ptr->m_backgroundShader->bind();

    // Draw background
    if (d_ptr->m_background) {
        QMatrix4x4 modelMatrix;
        QMatrix4x4 MVPMatrix;
        if (zComp != 0)
            modelMatrix.translate(0.0f, 0.0f, zComp);
        modelMatrix.scale(QVector3D(d_ptr->m_rowWidth * d_ptr->m_sceneScale
                                    , 1.0f
                                    , d_ptr->m_columnDepth * d_ptr->m_sceneScale));
        modelMatrix.rotate(backgroundRotation, 0.0f, 1.0f, 0.0f);

        MVPMatrix = projectionMatrix * viewMatrix * modelMatrix;

        QVector3D backgroundColor = QVector3D(0.75, 0.75, 0.75);

        d_ptr->m_backgroundShader->setUniformValue(d_ptr->m_lightPositionUniformBackground
                                                   , lightPos);
        d_ptr->m_backgroundShader->setUniformValue(d_ptr->m_viewMatrixUniformBackground
                                                   , viewMatrix);
        d_ptr->m_backgroundShader->setUniformValue(d_ptr->m_modelMatrixUniformBackground
                                                   , modelMatrix);
        d_ptr->m_backgroundShader->setUniformValue(d_ptr->m_invTransModelMatrixUniformBackground
                                                   , modelMatrix.inverted().transposed());
        d_ptr->m_backgroundShader->setUniformValue(d_ptr->m_mvpMatrixUniformBackground
                                                   , MVPMatrix);
        d_ptr->m_backgroundShader->setUniformValue(d_ptr->m_colorUniformBackground
                                                   , backgroundColor);
        d_ptr->m_backgroundShader->setUniformValue(d_ptr->m_lightStrengthUniformBackground
                                                   , 4.0f);

        // 1st attribute buffer : vertices
        glEnableVertexAttribArray(d_ptr->m_positionAttrBackground);
        glBindBuffer(GL_ARRAY_BUFFER, d_ptr->m_vertexbufferBackground);
        glVertexAttribPointer(d_ptr->m_positionAttrBackground, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

        // 2nd attribute buffer : normals
        glEnableVertexAttribArray(d_ptr->m_normalAttrBackground);
        glBindBuffer(GL_ARRAY_BUFFER, d_ptr->m_normalbufferBackground);
        glVertexAttribPointer(d_ptr->m_normalAttrBackground, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

        // 3rd attribute buffer : UVs
        //glEnableVertexAttribArray(d_ptr->m_uvAttrBackground);
        //glBindBuffer(GL_ARRAY_BUFFER, d_ptr->m_uvbufferBackground);
        //glVertexAttribPointer(d_ptr->m_uvAttrBackground, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

        // Index buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d_ptr->m_elementbufferBackground);

        // Draw the triangles
        glDrawElements(GL_TRIANGLES, d_ptr->m_indexCountBackground, GL_UNSIGNED_SHORT, (void*)0);

        // Free buffers
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        //glDisableVertexAttribArray(d_ptr->m_uvAttrBackground);
        glDisableVertexAttribArray(d_ptr->m_normalAttrBackground);
        glDisableVertexAttribArray(d_ptr->m_positionAttrBackground);
    }

    // Release background shader
    d_ptr->m_backgroundShader->release();

    // Bind bar shader
    d_ptr->m_barShader->bind();

    // Draw bars
    //int barIndex = 1; // TODO: Remove when done debugging
    bool barSelectionFound = false;
    for (int row = startRow; row != stopRow; row += stepRow) {
        for (int bar = startBar; bar != stopBar; bar += stepBar) {
            float barHeight = d_ptr->m_dataSet.at(row).at(bar);
            if (barHeight == 0)
                continue;
            QMatrix4x4 modelMatrix;
            QMatrix4x4 MVPMatrix;
            barPos = (bar + 1) * (d_ptr->m_barSpacing.x());
            //qDebug() << "x" << d_ptr->m_rowWidth << "-" << barPos << "=" << d_ptr->m_rowWidth - barPos;
            rowPos = (row + 1) * (d_ptr->m_barSpacing.y());
            //qDebug() << "z" << rowPos << "-" << d_ptr->m_columnDepth << "=" << rowPos - d_ptr->m_columnDepth;
            modelMatrix.translate((d_ptr->m_rowWidth - barPos) / d_ptr->m_scaleFactorX
                                  , barHeight - 1.0f
                                  , (d_ptr->m_columnDepth - rowPos) / d_ptr->m_scaleFactorZ + zComp);
            modelMatrix.scale(QVector3D(d_ptr->m_scaleX, barHeight, d_ptr->m_scaleZ));

            MVPMatrix = projectionMatrix * viewMatrix * modelMatrix;

            QVector3D baseColor = QVector3D(d_ptr->m_baseColor.redF()
                                            , d_ptr->m_baseColor.greenF()
                                            , d_ptr->m_baseColor.blueF());
            QVector3D heightColor = QVector3D(d_ptr->m_heightColor.redF()
                                              , d_ptr->m_heightColor.greenF()
                                              , d_ptr->m_heightColor.blueF())
                    * barHeight;
            QVector3D depthColor = QVector3D(d_ptr->m_depthColor.redF()
                                             , d_ptr->m_depthColor.greenF()
                                             , d_ptr->m_depthColor.blueF())
                    * (float(row) / float(d_ptr->m_sampleCount.y()));

            QVector3D barColor = baseColor + heightColor + depthColor;

            float lightStrength = 5.0f;
            if (d_ptr->m_selectionMode > None) {
                Q3DBarsPrivate::SelectionType selectionType = d_ptr->isSelected(row, bar
                                                                                , selection);
                switch (selectionType) {
                case Q3DBarsPrivate::Bar:
                {
                    // highlight bar by inverting the color of the bar
                    barColor = QVector3D(1.0f, 1.0f, 1.0f) - barColor;
                    lightStrength = 10.0f;
                    //if (d_ptr->m_mousePressed) {
                    //    qDebug() << "selected object:" << barIndex << "( row:" << row + 1 << ", column:" << bar + 1 << ")";
                    //    qDebug() << barIndex << "object position:" << modelMatrix.column(3).toVector3D();
                    //}
                    // Save data to SampleData
                    if (d_ptr->m_selectedBar)
                        delete d_ptr->m_selectedBar;
                    d_ptr->m_selectedBar = new SampleData(d_ptr->m_mousePos, barHeight);
                    barSelectionFound = true;
                    break;
                }
                case Q3DBarsPrivate::Row:
                {
                    // Current bar is on the same row as the selected bar
                    barColor = QVector3D(1.0f, 1.0f, 1.0f) - barColor;
                    lightStrength = 1.0f;
                    break;
                }
                case Q3DBarsPrivate::Column:
                {
                    // Current bar is on the same column as the selected bar
                    barColor = QVector3D(1.0f, 1.0f, 1.0f) - barColor;
                    lightStrength = 1.0f;
                    break;
                }
                }
            }

            //if (d_ptr->m_mousePressed) {
                //qDebug() << "baseColor:" << baseColor;
                //qDebug() << "heightColor:" << heightColor;
                //qDebug() << "depthColor:" << depthColor;
                //qDebug() << "barColor:" << barColor;
                //qDebug() << barIndex << "object position:" << modelMatrix.column(3).toVector3D();
            //}
            //barIndex++; // TODO: Remove when done debugging

            d_ptr->m_barShader->setUniformValue(d_ptr->m_lightPositionUniform, lightPos);
            d_ptr->m_barShader->setUniformValue(d_ptr->m_viewMatrixUniform, viewMatrix);
            d_ptr->m_barShader->setUniformValue(d_ptr->m_modelMatrixUniform, modelMatrix);
            d_ptr->m_barShader->setUniformValue(d_ptr->m_invTransModelMatrixUniform
                                                , modelMatrix.inverted().transposed());
            d_ptr->m_barShader->setUniformValue(d_ptr->m_mvpMatrixUniform, MVPMatrix);
            d_ptr->m_barShader->setUniformValue(d_ptr->m_colorUniform, barColor);
            d_ptr->m_barShader->setUniformValue(d_ptr->m_lightStrengthUniform, lightStrength);
            //qDebug() << "height:" << barHeight;

            // 1st attribute buffer : vertices
            glEnableVertexAttribArray(d_ptr->m_positionAttr);
            glBindBuffer(GL_ARRAY_BUFFER, d_ptr->m_vertexbuffer);
            glVertexAttribPointer(d_ptr->m_positionAttr, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

            // 2nd attribute buffer : normals
            glEnableVertexAttribArray(d_ptr->m_normalAttr);
            glBindBuffer(GL_ARRAY_BUFFER, d_ptr->m_normalbuffer);
            glVertexAttribPointer(d_ptr->m_normalAttr, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

            // 3rd attribute buffer : UVs
            //glEnableVertexAttribArray(d_ptr->m_uvAttr);
            //glBindBuffer(GL_ARRAY_BUFFER, d_ptr->m_uvbuffer);
            //glVertexAttribPointer(d_ptr->m_uvAttr, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

            // Index buffer
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d_ptr->m_elementbuffer);

            // Draw the triangles
            glDrawElements(GL_TRIANGLES, d_ptr->m_indexCount, GL_UNSIGNED_SHORT, (void*)0);

            // Free buffers
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            //glDisableVertexAttribArray(d_ptr->m_uvAttr);
            glDisableVertexAttribArray(d_ptr->m_normalAttr);
            glDisableVertexAttribArray(d_ptr->m_positionAttr);
        }
    }
    if (!barSelectionFound) {
        delete d_ptr->m_selectedBar;
        d_ptr->m_selectedBar = NULL;
    }

    // Release bar shader
    d_ptr->m_barShader->release();
}

void Q3DBars::mousePressEvent(QMouseEvent *event)
{
    // TODO: for testing shaders
    static int shaderNo = 1;
    //qDebug() << "mouse button pressed" << event->button();
    if (Qt::LeftButton == event->button()) {
        d_ptr->m_mousePressed = true;
        // update mouse positions to prevent jumping when releasing or repressing a button
        d_ptr->m_mousePos = event->pos();
    }
    else if (Qt::RightButton == event->button()) {
        // reset rotations
        d_ptr->m_mousePos = QPoint(0, 0);
    }
    else if (Qt::MiddleButton == event->button()) {
        // TODO: testing shaders
        if (++shaderNo > 3)
            shaderNo = 1;
        switch (shaderNo) {
        case 1:
            d_ptr->initShaders(QStringLiteral(":/shaders/vertex")
                               , QStringLiteral(":/shaders/fragment"));
            break;
        case 2:
            d_ptr->initShaders(QStringLiteral(":/shaders/vertex")
                               , QStringLiteral(":/shaders/fragmentColorOnY"));
            break;
        case 3:
            d_ptr->initShaders(QStringLiteral(":/shaders/vertex")
                               , QStringLiteral(":/shaders/fragmentAmbient"));
            break;
        }
    }
    CameraHelper::updateMousePos(d_ptr->m_mousePos);
}

void Q3DBars::mouseReleaseEvent(QMouseEvent *event)
{
    //qDebug() << "mouse button released" << event->button();
    d_ptr->m_mousePressed = false;
    // update mouse positions to prevent jumping when releasing or repressing a button
    d_ptr->m_mousePos = event->pos();
    CameraHelper::updateMousePos(event->pos());
}

void Q3DBars::mouseMoveEvent(QMouseEvent *event)
{
    if (d_ptr->m_mousePressed) {
        //qDebug() << "mouse moved while pressed" << event->pos();
        d_ptr->m_mousePos = event->pos();
    }
#if 0
    // TODO: Testi - laske kursorin sijainti scenessä
    QPointF mouse3D((2.0f * event->pos().x() - width()) / height()
                    , 1.0f - (2.0f * event->pos().y()) / height());
    //qDebug() << "mouse position in scene" << mouse3D;

    // TODO: Testi laske focal point
    float focalPoint = tan(45.0f / 2.0f);

    // TODO: Testi - laske viewmatriisin kerroin
    QVector3D worldRay = QVector3D(0.0f, 0.0f, 0.0f)
            - QVector3D(mouse3D.x(), mouse3D.y(), -focalPoint);
    //qDebug() << "worldRay" << worldRay;
    // multiply viewmatrix with this to get something?
#endif
}

void Q3DBars::wheelEvent(QWheelEvent *event)
{
    if (d_ptr->m_zoomLevel > 100) {
        d_ptr->m_zoomLevel += event->angleDelta().y() / 12;
    }
    else if (d_ptr->m_zoomLevel > 50) {
        d_ptr->m_zoomLevel += event->angleDelta().y() / 60;
    }
    else {
        d_ptr->m_zoomLevel += event->angleDelta().y() / 120;
    }
    if (d_ptr->m_zoomLevel > 500) {
        d_ptr->m_zoomLevel = 500;
    }
    else if (d_ptr->m_zoomLevel < 10) {
        d_ptr->m_zoomLevel = 10;
    }
}

void Q3DBars::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);

    // Set view port
    glViewport(0, 0, width(), height());

    // If orientation changes, we need to scale again
    // TODO: Handle it
}

void Q3DBars::setBarSpecs(QPointF thickness, QPointF spacing, bool relative)
{
    d_ptr->m_barThickness = thickness;
    if (relative) {
        d_ptr->m_barSpacing.setX((thickness.x() * 2) * (spacing.x() + 1.0f));
        d_ptr->m_barSpacing.setY((thickness.y() * 2) * (spacing.y() + 1.0f));
    }
    else {
        d_ptr->m_barSpacing = thickness * 2 + spacing * 2;
    }
    // Calculate here and at setting sample space
    d_ptr->calculateSceneScalingFactors();
}

void Q3DBars::setBarType(BarStyle style, bool smooth)
{
    if (style == Bars) {
        if (smooth) {
            d_ptr->m_objFile = QStringLiteral(":/defaultMeshes/barSmooth");
        }
        else {
            d_ptr->m_objFile = QStringLiteral(":/defaultMeshes/bar");
        }
    }
    else if (style == Pyramids) {
        if (smooth) {
            d_ptr->m_objFile = QStringLiteral(":/defaultMeshes/pyramidSmooth");
        }
        else {
            d_ptr->m_objFile = QStringLiteral(":/defaultMeshes/pyramid");
        }
    }
    else if (style == Cones) {
        if (smooth) {
            d_ptr->m_objFile = QStringLiteral(":/defaultMeshes/coneSmooth");
        }
        else {
            d_ptr->m_objFile = QStringLiteral(":/defaultMeshes/cone");
        }
    }
    else if (style == Cylinders) {
        if (smooth) {
            d_ptr->m_objFile = QStringLiteral(":/defaultMeshes/cylinderSmooth");
        }
        else {
            d_ptr->m_objFile = QStringLiteral(":/defaultMeshes/cylinder");
        }
    }
    if (d_ptr->m_isInitialized) {
        // Reload mesh data
        d_ptr->loadBarMesh();
    }
}

void Q3DBars::setMeshFileName(const QString &objFileName)
{
    d_ptr->m_objFile = objFileName;
}

void Q3DBars::setupSampleSpace(QPoint sampleCount)
{
    d_ptr->m_sampleCount = sampleCount;
    // Initialize data set
    QVector<float> row;
    for (int columns = 0; columns < sampleCount.x(); columns ++) {
        row.append(0.0f);
    }
    for (int rows = 0; rows < sampleCount.y(); rows++) {
        d_ptr->m_dataSet.append(row);
    }
    // TODO: Invent "idiotproof" max scene size formula..
    // This seems to work ok if spacing is not negative
    d_ptr->m_maxSceneSize = 2 * qSqrt(sampleCount.x() * sampleCount.y());
    //qDebug() << "maxSceneSize" << d_ptr->m_maxSceneSize;
    // Calculate here and at setting bar specs
    d_ptr->calculateSceneScalingFactors();
}

void Q3DBars::setCameraPosition(float horizontal, float vertical, int distance)
{
    d_ptr->m_horizontalRotation = qBound(-180.0f, horizontal, 180.0f);
    d_ptr->m_verticalRotation = qBound(0.0f, vertical, 90.0f);
    d_ptr->m_zoomLevel = qBound(10, distance, 500);
    CameraHelper::setCameraRotation(QPointF(d_ptr->m_horizontalRotation
                                            , d_ptr->m_verticalRotation));
    qDebug() << "camera rotation set to" << d_ptr->m_horizontalRotation << d_ptr->m_verticalRotation;
}

void Q3DBars::setBarColor(QColor baseColor, QColor heightColor, QColor depthColor, bool uniform)
{
    d_ptr->m_baseColor = baseColor;
    d_ptr->m_heightColor = heightColor;
    d_ptr->m_depthColor = depthColor;
    //qDebug() << "colors:" << d_ptr->m_baseColor << d_ptr->m_heightColor << d_ptr->m_depthColor;
    d_ptr->m_uniformColor = uniform;
}

void Q3DBars::setSelectionMode(SelectionMode mode)
{
    d_ptr->m_selectionMode = mode;
}

void Q3DBars::addDataRow(const QVector<float> &dataRow)
{
    QVector<float> row = dataRow;
    // Check that the input data fits into sample space, and resize if it doesn't
    if (row.size() > d_ptr->m_sampleCount.x()) {
        row.resize(d_ptr->m_sampleCount.x());
        qWarning("Data set too large for sample space");
    }
    // The vector contains data (=height) for each bar, a row at a time
    // With each new row, the previous data set (=row) must be moved back
    // ie. we need as many vectors as we have rows in the sample space
    d_ptr->m_dataSet.prepend(row);
    // if the added data pushed us over sample space, remove the oldest data set
    if (d_ptr->m_dataSet.size() > d_ptr->m_sampleCount.y())
        d_ptr->m_dataSet.resize(d_ptr->m_sampleCount.y());
}

void Q3DBars::addDataSet(const QVector< QVector<float> > &data)
{
    d_ptr->m_dataSet.clear();
    // Check sizes
    if (data.at(0).size() > d_ptr->m_sampleCount.x()) {
        qCritical("Too much data per row, aborting");
        return;
    }
    d_ptr->m_dataSet = data;
    if (d_ptr->m_dataSet.size() > d_ptr->m_sampleCount.y()) {
        qWarning("Data set too large for sample space. Cropping it to fit.");
        d_ptr->m_dataSet.resize(d_ptr->m_sampleCount.y());
    }
}

Q3DBarsPrivate::Q3DBarsPrivate(Q3DBars *q)
    : q_ptr(q)
    , m_paintDevice(0)
    , m_barShader(0)
    , m_selectionShader(0)
    , m_backgroundShader(0)
    , m_sampleCount(QPoint(0, 0))
    , m_objFile(QStringLiteral(":/defaultMeshes/bar"))
    , m_indexCount(0)
    , m_indexCountBackground(0)
    , m_mousePressed(false)
    , m_mousePos(QPoint(0, 0))
    , m_zoomLevel(100)
    , m_horizontalRotation(-45.0f)
    , m_verticalRotation(15.0f)
    , m_barThickness(QPointF(0.75f, 0.75f))
    , m_barSpacing(m_barThickness * 3.0f)
    , m_meshDataLoaded(false)
    , m_background(false)
    , m_dataSet(0)
    , m_rowWidth(0)
    , m_columnDepth(0)
    , m_maxDimension(0)
    , m_scaleX(0)
    , m_scaleZ(0)
    , m_scaleFactorX(0)
    , m_scaleFactorZ(0)
    , m_sceneScale(0)
    , m_maxSceneSize(40.0)
    , m_baseColor(QColor(Qt::gray))
    , m_heightColor(QColor(Qt::white))
    , m_depthColor(QColor(Qt::darkGray))
    , m_uniformColor(true)
    , m_isInitialized(false)
    , m_selectionMode(Q3DBars::Bar)
    , m_selectedBar(0)
{
}

Q3DBarsPrivate::~Q3DBarsPrivate()
{
    qDebug() << "Destroying Q3DBarsPrivate";
    delete m_barShader;
    delete m_selectionShader;
    delete m_backgroundShader;
    delete m_selectedBar;

    q_ptr->glDeleteBuffers(1, &m_vertexbuffer);
    q_ptr->glDeleteBuffers(1, &m_uvbuffer);
    q_ptr->glDeleteBuffers(1, &m_normalbuffer);
    q_ptr->glDeleteBuffers(1, &m_elementbuffer);

#ifndef USE_HAX0R_SELECTION
    q_ptr->glDeleteFramebuffers(1, &m_framebufferSelection);
    q_ptr->glDeleteTextures(1, &m_selectionTexture);
    q_ptr->glDeleteTextures(1, &m_depthTexture);
#endif

    q_ptr->glDeleteBuffers(1, &m_vertexbufferBackground);
    q_ptr->glDeleteBuffers(1, &m_uvbufferBackground);
    q_ptr->glDeleteBuffers(1, &m_normalbufferBackground);
    q_ptr->glDeleteBuffers(1, &m_elementbufferBackground);
}

void Q3DBarsPrivate::loadBarMesh()
{
    if (m_meshDataLoaded) {
        // Delete old data
        q_ptr->glDeleteBuffers(1, &m_vertexbuffer);
        q_ptr->glDeleteBuffers(1, &m_uvbuffer);
        q_ptr->glDeleteBuffers(1, &m_normalbuffer);
        q_ptr->glDeleteBuffers(1, &m_elementbuffer);
    }
    QVector<QVector3D> vertices;
    QVector<QVector2D> uvs;
    QVector<QVector3D> normals;
    bool loadOk = MeshLoader::loadOBJ(m_objFile, vertices, uvs, normals);
    if (!loadOk)
        qFatal("loading failed");

    qDebug() << "vertex count" << vertices.size();;

    // Index vertices
    QVector<unsigned short> indices;
    QVector<QVector3D> indexed_vertices;
    QVector<QVector2D> indexed_uvs;
    QVector<QVector3D> indexed_normals;
    VertexIndexer::indexVBO(vertices, uvs, normals, indices, indexed_vertices, indexed_uvs
                            , indexed_normals);

    m_indexCount = indices.size();
    //qDebug() << "index count" << m_indexCount;

    q_ptr->glGenBuffers(1, &m_vertexbuffer);
    q_ptr->glBindBuffer(GL_ARRAY_BUFFER, m_vertexbuffer);
    q_ptr->glBufferData(GL_ARRAY_BUFFER, indexed_vertices.size() * sizeof(QVector3D)
                        , &indexed_vertices.at(0)
                        , GL_STATIC_DRAW);

    q_ptr->glGenBuffers(1, &m_normalbuffer);
    q_ptr->glBindBuffer(GL_ARRAY_BUFFER, m_normalbuffer);
    q_ptr->glBufferData(GL_ARRAY_BUFFER, indexed_normals.size() * sizeof(QVector3D)
                        , &indexed_normals.at(0)
                        , GL_STATIC_DRAW);

    //q_ptr->glGenBuffers(1, &m_uvbuffer);
    //q_ptr->glBindBuffer(GL_ARRAY_BUFFER, m_uvbuffer);
    //q_ptr->glBufferData(GL_ARRAY_BUFFER, indexed_uvs.size() * sizeof(QVector2D), &indexed_uvs.at(0)
    //        , GL_STATIC_DRAW);

    q_ptr->glGenBuffers(1, &m_elementbuffer);
    q_ptr->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementbuffer);
    q_ptr->glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices.at(0)
                        , GL_STATIC_DRAW);

    q_ptr->glBindBuffer(GL_ARRAY_BUFFER, 0);
    q_ptr->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    m_meshDataLoaded = true;
}

void Q3DBarsPrivate::loadBackgroundMesh()
{
    QVector<QVector3D> vertices;
    QVector<QVector2D> uvs;
    QVector<QVector3D> normals;
    bool loadOk = MeshLoader::loadOBJ(QStringLiteral(":/defaultMeshes/background")
                                      , vertices, uvs, normals);
    if (!loadOk)
        qFatal("loading failed");

    // Index vertices
    QVector<unsigned short> indices;
    QVector<QVector3D> indexed_vertices;
    QVector<QVector2D> indexed_uvs;
    QVector<QVector3D> indexed_normals;
    VertexIndexer::indexVBO(vertices, uvs, normals, indices, indexed_vertices, indexed_uvs
                            , indexed_normals);

    m_indexCountBackground = indices.size();

    q_ptr->glGenBuffers(1, &m_vertexbufferBackground);
    q_ptr->glBindBuffer(GL_ARRAY_BUFFER, m_vertexbufferBackground);
    q_ptr->glBufferData(GL_ARRAY_BUFFER, indexed_vertices.size() * sizeof(QVector3D)
                        , &indexed_vertices.at(0)
                        , GL_STATIC_DRAW);

    q_ptr->glGenBuffers(1, &m_normalbufferBackground);
    q_ptr->glBindBuffer(GL_ARRAY_BUFFER, m_normalbufferBackground);
    q_ptr->glBufferData(GL_ARRAY_BUFFER, indexed_normals.size() * sizeof(QVector3D)
                        , &indexed_normals.at(0)
                        , GL_STATIC_DRAW);

    //q_ptr->glGenBuffers(1, &m_uvbufferBackground);
    //q_ptr->glBindBuffer(GL_ARRAY_BUFFER, m_uvbufferBackground);
    //q_ptr->glBufferData(GL_ARRAY_BUFFER, indexed_uvs.size() * sizeof(QVector2D), &indexed_uvs.at(0)
    //        , GL_STATIC_DRAW);

    q_ptr->glGenBuffers(1, &m_elementbufferBackground);
    q_ptr->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementbufferBackground);
    q_ptr->glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices.at(0)
                        , GL_STATIC_DRAW);

    q_ptr->glBindBuffer(GL_ARRAY_BUFFER, 0);
    q_ptr->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    m_background = true;
}

void Q3DBarsPrivate::initShaders(const QString &vertexShader, const QString &fragmentShader)
{
    if (m_barShader)
        delete m_barShader;
    m_barShader = new QOpenGLShaderProgram(q_ptr);
    if (!m_barShader->addShaderFromSourceFile(QOpenGLShader::Vertex, vertexShader))
        qFatal("Compiling Vertex shader failed");
    if (!m_barShader->addShaderFromSourceFile(QOpenGLShader::Fragment, fragmentShader))
        qFatal("Compiling Fragment shader failed");
    m_barShader->link();
    m_mvpMatrixUniform = m_barShader->uniformLocation("MVP");
    m_viewMatrixUniform = m_barShader->uniformLocation("V");
    m_modelMatrixUniform = m_barShader->uniformLocation("M");
    m_invTransModelMatrixUniform = m_barShader->uniformLocation("itM");
    m_positionAttr = m_barShader->attributeLocation("vertexPosition_mdl");
    m_uvAttr = m_barShader->attributeLocation("vertexUV");
    m_normalAttr = m_barShader->attributeLocation("vertexNormal_mdl");
    m_lightPositionUniform = m_barShader->uniformLocation("lightPosition_wrld");
    m_colorUniform = m_barShader->uniformLocation("color_mdl");
    m_lightStrengthUniform = m_barShader->uniformLocation("lightStrength");
}

void Q3DBarsPrivate::initSelectionShader()
{
    m_selectionShader = new QOpenGLShaderProgram(q_ptr);
    if (!m_selectionShader->addShaderFromSourceFile(QOpenGLShader::Vertex
                                                    , QStringLiteral(":/shaders/vertexSelection")))
        qFatal("Compiling Vertex shader failed");
    if (!m_selectionShader->addShaderFromSourceFile(QOpenGLShader::Fragment
                                                    , QStringLiteral(":/shaders/fragmentSelection")))
        qFatal("Compiling Fragment shader failed");
    m_selectionShader->link();
    m_mvpMatrixUniformSelection = m_selectionShader->uniformLocation("MVP");
    m_colorUniformSelection = m_selectionShader->uniformLocation("color_mdl");
    m_positionAttrSelection = m_selectionShader->attributeLocation("vertexPosition_mdl");
}

void Q3DBarsPrivate::initSelectionBuffer()
{
#if 0
    // Create frame buffer object
    q_ptr->glGenFramebuffers(1, &m_framebufferSelection);
    q_ptr->glBindFramebuffer(GL_FRAMEBUFFER, m_framebufferSelection);

    // Create texture object for the primitive information buffer
    q_ptr->glGenTextures(1, &m_selectionTexture);
    q_ptr->glBindTexture(GL_TEXTURE_2D, m_selectionTexture);
    q_ptr->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, q_ptr->width(), q_ptr->height(),
                        0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    q_ptr->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                  m_selectionTexture, 0);

    // Create texture object for the depth buffer
    q_ptr->glGenTextures(1, &m_depthTexture);
    q_ptr->glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    q_ptr->glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, q_ptr->width(), q_ptr->height(),
                        0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    q_ptr->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                  m_depthTexture, 0);

    // Verify that the frame buffer is complete
    GLenum status = q_ptr->glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        qCritical() << "Frame buffer creation failed" << status;
        return;
    }

    // Restore the default framebuffer
    q_ptr->glBindTexture(GL_TEXTURE_2D, 0);
    q_ptr->glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
}

void Q3DBarsPrivate::initBackgroundShaders(const QString &vertexShader
                                           , const QString &fragmentShader)
{
    if (m_backgroundShader)
        delete m_backgroundShader;
    m_backgroundShader = new QOpenGLShaderProgram(q_ptr);
    if (!m_backgroundShader->addShaderFromSourceFile(QOpenGLShader::Vertex, vertexShader))
        qFatal("Compiling Vertex shader failed");
    if (!m_backgroundShader->addShaderFromSourceFile(QOpenGLShader::Fragment, fragmentShader))
        qFatal("Compiling Fragment shader failed");
    m_backgroundShader->link();
    m_mvpMatrixUniformBackground = m_backgroundShader->uniformLocation("MVP");
    m_viewMatrixUniformBackground = m_backgroundShader->uniformLocation("V");
    m_modelMatrixUniformBackground = m_backgroundShader->uniformLocation("M");
    m_invTransModelMatrixUniformBackground = m_backgroundShader->uniformLocation("itM");
    m_positionAttrBackground = m_backgroundShader->attributeLocation("vertexPosition_mdl");
    m_uvAttrBackground = m_backgroundShader->attributeLocation("vertexUV");
    m_normalAttrBackground = m_backgroundShader->attributeLocation("vertexNormal_mdl");
    m_lightPositionUniformBackground = m_backgroundShader->uniformLocation("lightPosition_wrld");
    m_colorUniformBackground = m_backgroundShader->uniformLocation("color_mdl");
    m_lightStrengthUniformBackground = m_backgroundShader->uniformLocation("lightStrength");
}

void Q3DBarsPrivate::calculateSceneScalingFactors()
{
    // Calculate scene scaling and translation factors
    m_rowWidth = ((m_sampleCount.x() + 1) * m_barSpacing.x()) / 2.0f;
    m_columnDepth = ((m_sampleCount.y() + 1) * m_barSpacing.y()) / 2.0f;
    m_maxDimension = qMax(m_rowWidth, m_columnDepth);//(m_rowWidth > m_columnDepth) ? m_rowWidth : m_columnDepth;
    m_scaleX = m_barThickness.x() / m_sampleCount.x() * (m_maxSceneSize / m_maxDimension);
    m_scaleZ = m_barThickness.y() / m_sampleCount.x() * (m_maxSceneSize / m_maxDimension);
    m_sceneScale = qMin(m_scaleX, m_scaleZ);//(m_scaleX < m_scaleZ) ? m_scaleX : m_scaleZ;
    float minThickness = qMin(m_barThickness.x(), m_barThickness.y());//(m_barThickness.x() < m_barThickness.y()) ? m_barThickness.x() : m_barThickness.y();
    m_sceneScale = m_sceneScale / minThickness;
    m_scaleFactorX = m_sampleCount.x() * (m_maxDimension / m_maxSceneSize);
    m_scaleFactorZ = m_sampleCount.x() * (m_maxDimension / m_maxSceneSize);
    qDebug() << "m_scaleX" << m_scaleX << "m_scaleFactorX" << m_scaleFactorX;
    qDebug() << "m_scaleZ" << m_scaleZ << "m_scaleFactorZ" << m_scaleFactorZ;
    qDebug() << "m_rowWidth:" << m_rowWidth << "m_columnDepth:" << m_columnDepth << "m_maxDimension:" << m_maxDimension;
    qDebug() << m_rowWidth * m_sceneScale << m_columnDepth * m_sceneScale;
}

Q3DBarsPrivate::SelectionType Q3DBarsPrivate::isSelected(int row, int bar, const QVector3D &selection)
{
    SelectionType isSelectedType = None;
    if (selection == QVector3D(0, 0, 0))
        return isSelectedType; // skip background (= black)
    QVector3D current = QVector3D((GLubyte)(((float)(row + 2) / (float)(m_sampleCount.y() + 2))
                                            * 255 + 0.49) // add 0.49 to fix rounding
                                  , (GLubyte)(((float)(bar + 2) / (float)(m_sampleCount.x() + 2))
                                              * 255 + 0.49) // add 0.49 to fix rounding
                                  , 0);
    //if (m_mousePressed)
    //    qDebug() << "selected:" << selection << "current:" << current;
    if (current == selection) {
        isSelectedType = Bar;
    }
    else if (current.y() == selection.y() && (m_selectionMode == Q3DBars::BarAndColumn
                                              || m_selectionMode == Q3DBars::BarRowAndColumn)) {
        isSelectedType = Column;
    }
    else if (current.x() == selection.x() && (m_selectionMode == Q3DBars::BarAndRow
                                              || m_selectionMode == Q3DBars::BarRowAndColumn)) {
        isSelectedType = Row;
    }
    return isSelectedType;
}

QTCOMMERCIALDATAVIS3D_END_NAMESPACE
