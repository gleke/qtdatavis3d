/****************************************************************************
**
** Copyright (C) 2014 Digia Plc
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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtDataVisualization API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#ifndef ABSTRACT3DRENDERER_P_H
#define ABSTRACT3DRENDERER_P_H

#include <QtGui/QOpenGLFunctions>

#include "datavisualizationglobal_p.h"
#include "abstract3dcontroller_p.h"
#include "axisrendercache_p.h"
#include "seriesrendercache_p.h"
#include "customrenderitem_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

class TextureHelper;
class Theme;
class Drawer;

class Abstract3DRenderer : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT

protected:
    enum SelectionState {
        SelectNone = 0,
        SelectOnScene,
        SelectOnOverview,
        SelectOnSlice
    };

    enum RenderingState {
        RenderingNormal = 0,
        RenderingSelection,
        RenderingDepth
    };

public:
    virtual ~Abstract3DRenderer();

    virtual void updateData() = 0;
    virtual void updateSeries(const QList<QAbstract3DSeries *> &seriesList);
    virtual void updateCustomData(const QList<QCustom3DItem *> &customItems);
    virtual void updateCustomItems();
    virtual void updateCustomItemPositions();
    virtual SeriesRenderCache *createNewCache(QAbstract3DSeries *series);
    virtual void cleanCache(SeriesRenderCache *cache);
    virtual void render(GLuint defaultFboHandle);

    virtual void updateTheme(Q3DTheme *theme);
    virtual void updateSelectionMode(QAbstract3DGraph::SelectionFlags newMode);
    virtual void updateOptimizationHint(QAbstract3DGraph::OptimizationHints hint);
    virtual void updateScene(Q3DScene *scene);
    virtual void updateTextures();
    virtual void initSelectionBuffer() = 0;
    virtual void updateSelectionState(SelectionState state);
    virtual void updateInputPosition(const QPoint &position);

#if !defined(QT_OPENGL_ES_2)
    virtual void updateDepthBuffer() = 0;
#endif
    virtual void updateShadowQuality(QAbstract3DGraph::ShadowQuality quality) = 0;
    virtual void initShaders(const QString &vertexShader, const QString &fragmentShader) = 0;
    virtual void initGradientShaders(const QString &vertexShader, const QString &fragmentShader);
    virtual void initBackgroundShaders(const QString &vertexShader,
                                       const QString &fragmentShader) = 0;
    virtual void initCustomItemShaders(const QString &vertexShader,
                                       const QString &fragmentShader);
    virtual void initVolumeTextureShaders(const QString &vertexShader,
                                          const QString &fragmentShader,
                                          const QString &fragmentLowDefShader,
                                          const QString &sliceShader,
                                          const QString &sliceFrameVertexShader,
                                          const QString &sliceFrameShader);
    virtual void updateAxisType(QAbstract3DAxis::AxisOrientation orientation,
                                QAbstract3DAxis::AxisType type);
    virtual void updateAxisTitle(QAbstract3DAxis::AxisOrientation orientation,
                                 const QString &title);
    virtual void updateAxisLabels(QAbstract3DAxis::AxisOrientation orientation,
                                  const QStringList &labels);
    virtual void updateAxisRange(QAbstract3DAxis::AxisOrientation orientation, float min,
                                 float max);
    virtual void updateAxisSegmentCount(QAbstract3DAxis::AxisOrientation orientation, int count);
    virtual void updateAxisSubSegmentCount(QAbstract3DAxis::AxisOrientation orientation,
                                           int count);
    virtual void updateAxisLabelFormat(QAbstract3DAxis::AxisOrientation orientation,
                                       const QString &format);
    virtual void updateAxisReversed(QAbstract3DAxis::AxisOrientation orientation,
                                    bool enable);
    virtual void updateAxisFormatter(QAbstract3DAxis::AxisOrientation orientation,
                                     QValue3DAxisFormatter *formatter);
    virtual void updateAxisLabelAutoRotation(QAbstract3DAxis::AxisOrientation orientation,
                                             float angle);
    virtual void updateAxisTitleVisibility(QAbstract3DAxis::AxisOrientation orientation,
                                      bool visible);
    virtual void updateAxisTitleFixed(QAbstract3DAxis::AxisOrientation orientation,
                                      bool fixed);
    virtual void modifiedSeriesList(const QVector<QAbstract3DSeries *> &seriesList);

    virtual void fixMeshFileName(QString &fileName, QAbstract3DSeries::Mesh mesh);

    virtual CustomRenderItem *addCustomItem(QCustom3DItem *item);
    virtual void updateCustomItem(CustomRenderItem *renderItem);

    virtual void updateAspectRatio(float ratio);
    virtual void updateHorizontalAspectRatio(float ratio);
    virtual void updatePolar(bool enable);
    virtual void updateRadialLabelOffset(float offset);

    virtual QVector3D convertPositionToTranslation(const QVector3D &position,
                                                   bool isAbsolute) = 0;

    void generateBaseColorTexture(const QColor &color, GLuint *texture);
    void fixGradientAndGenerateTexture(QLinearGradient *gradient, GLuint *gradientTexture);

    inline bool isClickPending() { return m_clickPending; }
    inline void clearClickPending() { m_clickPending = false; }
    inline QAbstract3DSeries *clickedSeries() const { return m_clickedSeries; }
    inline QAbstract3DGraph::ElementType clickedType() { return m_clickedType; }

    LabelItem &selectionLabelItem();
    void setSelectionLabel(const QString &label);
    QString &selectionLabel();

    void drawCustomItems(RenderingState state, ShaderHelper *regularShader,
                         const QMatrix4x4 &viewMatrix,
                         const QMatrix4x4 &projectionViewMatrix,
                         const QMatrix4x4 &depthProjectionViewMatrix,
                         GLuint depthTexture, GLfloat shadowQuality, GLfloat reflection = 1.0f);

    QVector4D indexToSelectionColor(GLint index);
    void calculatePolarXZ(const QVector3D &dataPos, float &x, float &z) const;

signals:
    void needRender(); // Emit this if something in renderer causes need for another render pass.
    void requestShadowQuality(QAbstract3DGraph::ShadowQuality quality); // For automatic quality adjustments

protected:
    Abstract3DRenderer(Abstract3DController *controller);

    virtual void initializeOpenGL();

    void reInitShaders();
    virtual void handleShadowQualityChange();
    virtual void handleResize();

    AxisRenderCache &axisCacheForOrientation(QAbstract3DAxis::AxisOrientation orientation);

    virtual void lowerShadowQuality();

    void fixGradient(QLinearGradient *gradient, GLuint *gradientTexture);

    void calculateZoomLevel();
    void drawAxisTitleY(const QVector3D &sideLabelRotation, const QVector3D &backLabelRotation,
                        const QVector3D &sideLabelTrans, const QVector3D &backLabelTrans,
                        const QQuaternion &totalSideRotation, const QQuaternion &totalBackRotation,
                        AbstractRenderItem &dummyItem, const Q3DCamera *activeCamera,
                        float labelsMaxWidth,
                        const QMatrix4x4 &viewMatrix, const QMatrix4x4 &projectionMatrix,
                        ShaderHelper *shader);
    void drawAxisTitleX(const QVector3D &labelRotation, const QVector3D &labelTrans,
                        const QQuaternion &totalRotation, AbstractRenderItem &dummyItem,
                        const Q3DCamera *activeCamera, float labelsMaxWidth,
                        const QMatrix4x4 &viewMatrix, const QMatrix4x4 &projectionMatrix,
                        ShaderHelper *shader, bool radial = false);
    void drawAxisTitleZ(const QVector3D &labelRotation, const QVector3D &labelTrans,
                        const QQuaternion &totalRotation, AbstractRenderItem &dummyItem,
                        const Q3DCamera *activeCamera, float labelsMaxWidth,
                        const QMatrix4x4 &viewMatrix, const QMatrix4x4 &projectionMatrix,
                        ShaderHelper *shader);

    void loadGridLineMesh();
    void loadLabelMesh();

    void drawRadialGrid(ShaderHelper *shader, float yFloorLinePos,
                        const QMatrix4x4 &projectionViewMatrix, const QMatrix4x4 &depthMatrix);
    void drawAngularGrid(ShaderHelper *shader, float yFloorLinePos,
                         const QMatrix4x4 &projectionViewMatrix, const QMatrix4x4 &depthMatrix);

    float calculatePolarBackgroundMargin();
    virtual void fixCameraTarget(QVector3D &target) = 0;
    void updateCameraViewport();

    void recalculateCustomItemScalingAndPos(CustomRenderItem *item);
    virtual void getVisibleItemBounds(QVector3D &minBounds, QVector3D &maxBounds) = 0;
    void drawVolumeSliceFrame(const CustomRenderItem *item, Qt::Axis axis,
                              const QMatrix4x4 &projectionViewMatrix);

    bool m_hasNegativeValues;
    Q3DTheme *m_cachedTheme;
    Drawer *m_drawer;
    QRect m_viewport;
    QAbstract3DGraph::ShadowQuality m_cachedShadowQuality;
    GLfloat m_autoScaleAdjustment;

    QAbstract3DGraph::SelectionFlags m_cachedSelectionMode;
    QAbstract3DGraph::OptimizationHints m_cachedOptimizationHint;

    AxisRenderCache m_axisCacheX;
    AxisRenderCache m_axisCacheY;
    AxisRenderCache m_axisCacheZ;
    TextureHelper *m_textureHelper;
    GLuint m_depthTexture;

    Q3DScene *m_cachedScene;
    bool m_selectionDirty;
    SelectionState m_selectionState;
    QPoint m_inputPosition;
    QHash<QAbstract3DSeries *, SeriesRenderCache *> m_renderCacheList;
    CustomRenderItemArray m_customRenderCache;
    QRect m_primarySubViewport;
    QRect m_secondarySubViewport;
    float m_devicePixelRatio;
    bool m_selectionLabelDirty;
    bool m_clickPending;
    QAbstract3DSeries *m_clickedSeries;
    QAbstract3DGraph::ElementType m_clickedType;
    int m_selectedLabelIndex;
    int m_selectedCustomItemIndex;

    QString m_selectionLabel;
    LabelItem *m_selectionLabelItem;
    int m_visibleSeriesCount;

    ShaderHelper *m_customItemShader;
    ShaderHelper *m_volumeTextureShader;
    ShaderHelper *m_volumeTextureLowDefShader;
    ShaderHelper *m_volumeTextureSliceShader;
    ShaderHelper *m_volumeSliceFrameShader;

    bool m_useOrthoProjection;
    bool m_xFlipped;
    bool m_yFlipped;
    bool m_zFlipped;
    bool m_yFlippedForGrid;

    ObjectHelper *m_backgroundObj; // Shared reference
    ObjectHelper *m_gridLineObj; // Shared reference
    ObjectHelper *m_labelObj; // Shared reference

    float m_graphAspectRatio;
    float m_graphHorizontalAspectRatio;
    bool m_polarGraph;
    float m_radialLabelOffset;
    float m_polarRadius;

    QQuaternion m_xRightAngleRotation;
    QQuaternion m_yRightAngleRotation;
    QQuaternion m_zRightAngleRotation;
    QQuaternion m_xRightAngleRotationNeg;
    QQuaternion m_yRightAngleRotationNeg;
    QQuaternion m_zRightAngleRotationNeg;
    QQuaternion m_xFlipRotation;
    QQuaternion m_zFlipRotation;

    float m_vBackgroundMargin;
    float m_hBackgroundMargin;

    QVector3D m_oldCameraTarget;

    bool m_reflectionEnabled;
    qreal m_reflectivity;

private:
    friend class Abstract3DController;
};

QT_END_NAMESPACE_DATAVISUALIZATION

#endif
