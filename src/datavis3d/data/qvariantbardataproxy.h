/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDataVis3D module.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QVARIANTBARDATAPROXY_H
#define QVARIANTBARDATAPROXY_H

#include "qbardataproxy.h"
#include "qvariantdataset.h"
#include "qvariantbardatamapping.h"
#include <QStringList>
#include <QMap>

QT_DATAVIS3D_BEGIN_NAMESPACE

class QVariantBarDataProxyPrivate;

class QT_DATAVIS3D_EXPORT QVariantBarDataProxy : public QBarDataProxy
{
    Q_OBJECT

public:
    explicit QVariantBarDataProxy();
    explicit QVariantBarDataProxy(QVariantDataSet *newSet, QVariantBarDataMapping *mapping);
    virtual ~QVariantBarDataProxy();

    // Doesn't gain ownership of the dataset, but does connect to it to listen for data changes.
    void setDataSet(QVariantDataSet *newSet);
    QVariantDataSet *dataSet();

    // Map key (row, column, value) to value index in data item (QVariantItem).
    // Doesn't gain ownership of mapping, but does connect to it to listen for mapping changes.
    // Modifying mapping that is set to proxy will trigger dataset re-resolving.
    void setMapping(QVariantBarDataMapping *mapping);
    QVariantBarDataMapping *mapping();

protected:
    QVariantBarDataProxyPrivate *dptr();

private:
    Q_DISABLE_COPY(QVariantBarDataProxy)
};

QT_DATAVIS3D_END_NAMESPACE

#endif