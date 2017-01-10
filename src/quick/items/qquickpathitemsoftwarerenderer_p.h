/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKPATHITEMSOFTWARERENDERER_P_H
#define QQUICKPATHITEMSOFTWARERENDERER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of a number of Qt sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include "qquickpathitem_p_p.h"
#include <qsgrendernode.h>
#include <QPen>
#include <QBrush>

QT_BEGIN_NAMESPACE

class QQuickPathItemSoftwareRenderNode;

class QQuickPathItemSoftwareRenderer : public QQuickAbstractPathRenderer
{
public:
    enum Dirty {
        DirtyPath = 0x01,
        DirtyPen = 0x02,
        DirtyFillRule = 0x04,
        DirtyBrush = 0x08,
        DirtyList = 0x10
    };

    void beginSync(int totalCount) override;
    void setPath(int index, const QQuickPath *path) override;
    void setStrokeColor(int index, const QColor &color) override;
    void setStrokeWidth(int index, qreal w) override;
    void setFillColor(int index, const QColor &color) override;
    void setFillRule(int index, QQuickVisualPath::FillRule fillRule) override;
    void setJoinStyle(int index, QQuickVisualPath::JoinStyle joinStyle, int miterLimit) override;
    void setCapStyle(int index, QQuickVisualPath::CapStyle capStyle) override;
    void setStrokeStyle(int index, QQuickVisualPath::StrokeStyle strokeStyle,
                        qreal dashOffset, const QVector<qreal> &dashPattern) override;
    void setFillGradient(int index, QQuickPathGradient *gradient) override;
    void endSync() override;

    void updateNode() override;

    void setNode(QQuickPathItemSoftwareRenderNode *node);

private:
    QQuickPathItemSoftwareRenderNode *m_node = nullptr;
    int m_accDirty = 0;
    struct VisualPathGuiData {
        int dirty = 0;
        QPainterPath path;
        QPen pen;
        float strokeWidth;
        QColor fillColor;
        QBrush brush;
        Qt::FillRule fillRule;
    };
    QVector<VisualPathGuiData> m_vp;
};

class QQuickPathItemSoftwareRenderNode : public QSGRenderNode
{
public:
    QQuickPathItemSoftwareRenderNode(QQuickPathItem *item);
    ~QQuickPathItemSoftwareRenderNode();

    void render(const RenderState *state) override;
    void releaseResources() override;
    StateFlags changedStates() const override;
    RenderingFlags flags() const override;
    QRectF rect() const override;

private:
    QQuickPathItem *m_item;

    struct VisualPathRenderData {
        QPainterPath path;
        QPen pen;
        float strokeWidth;
        QBrush brush;
    };
    QVector<VisualPathRenderData> m_vp;

    friend class QQuickPathItemSoftwareRenderer;
};

QT_END_NAMESPACE

#endif // QQUICKPATHITEMSOFTWARERENDERER_P_H