/******************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
******************************************************************************/

#ifndef DATA_H
#define DATA_H

#include <QtDataVisualization/Q3DScatter>
#include <QtDataVisualization/Q3DBars>
#include <QtDataVisualization/Q3DSurface>
#include <QtDataVisualization/QScatterDataProxy>
#include <QtDataVisualization/QBarDataProxy>
#include <QtDataVisualization/QHeightMapSurfaceDataProxy>
#include <QTextEdit>

using namespace QtDataVisualization;

class Data : public QObject
{
    Q_OBJECT

public:
    explicit Data(Q3DSurface *surface, Q3DScatter *scatter, Q3DBars *bars,
                  QTextEdit *statusLabel, QWidget *widget);
    ~Data();

    void start();
    void stop();

    void updateData();
    void clearData();

    void scrollDown();
    void setData(const QImage &image);
    void useGradientOne();
    void useGradientTwo();

public:
    enum VisualizationMode {
        Surface = 0,
        Scatter,
        Bars
    };

public Q_SLOTS:
    void setResolution(int selection);
    void changeMode(int mode);

private:
    Q3DSurface *m_surface;
    Q3DScatter *m_scatter;
    Q3DBars *m_bars;
    QTextEdit *m_statusArea;
    QWidget *m_widget;
    bool m_resize;
    QSize m_resolution;
    int m_resolutionLevel;
    VisualizationMode m_mode;
    QScatterDataArray *m_scatterDataArray;
    QBarDataArray *m_barDataArray;
    bool m_started;
};

class ContainerChanger : public QObject
{
    Q_OBJECT

public:
    explicit ContainerChanger(QWidget *surface, QWidget *scatter, QWidget *bars,
                              QWidget *buttonOne, QWidget *buttonTwo);
    ~ContainerChanger();

public Q_SLOTS:
    void changeContainer(int container);

private:
    QWidget *m_surface;
    QWidget *m_scatter;
    QWidget *m_bars;
    QWidget *m_button1;
    QWidget *m_button2;
};

#endif