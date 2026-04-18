// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <QHeaderView>
#include <QStyledItemDelegate>

/// Two-row horizontal header that groups QSL sub-columns under service names.
///
/// Top row:    "LoTW"    "eQSL"    "QRZ"   (spanning two sub-cols each)
/// Bottom row:  S    R    S    R    S    R
class QslGroupHeaderView : public QHeaderView
{
    Q_OBJECT

public:
    explicit QslGroupHeaderView(Qt::Orientation orientation, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
};

/// Delegate that paints a coloured circle for QSL status values.
///
/// Reads Qt::UserRole from the model, which returns a QChar:
///   'Y' → green bubble
///   'N' → red bubble
///   anything else / invalid → nothing drawn
class QslBubbleDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit QslBubbleDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};
