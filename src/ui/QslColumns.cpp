// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "QslColumns.h"

#include <QPainter>
#include <QStyleOptionHeader>

#include "core/logbook/QsoTableModel.h"

// ---------------------------------------------------------------------------
// QslGroupHeaderView
// ---------------------------------------------------------------------------

QslGroupHeaderView::QslGroupHeaderView(Qt::Orientation orientation, QWidget *parent)
    : QHeaderView(orientation, parent)
{}

QSize QslGroupHeaderView::sizeHint() const
{
    QSize s = QHeaderView::sizeHint();
    s.setHeight(s.height() * 2);
    return s;
}

void QslGroupHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    if (!rect.isValid()) return;

    painter->save();
    painter->setClipRect(rect);
    QHeaderView::paintSection(painter, rect, logicalIndex);

    if (logicalIndex < QsoTableModel::ColQslFirst) {
        painter->restore();
        return;
    }

    const int halfH  = rect.height() / 2;
    const int offset = logicalIndex - QsoTableModel::ColQslFirst;
    const bool isRight = (offset % 2 == 1);

    // Horizontal divider between top (group) row and bottom (S/R) row
    painter->setPen(palette().mid().color());
    painter->drawLine(rect.left(), rect.top() + halfH, rect.right(), rect.top() + halfH);

    // Bottom half — S / R sub-label
    static const char *kSubLabels[] = { "S", "R", "S", "R", "S", "R" };
    const QRect botRect(rect.x(), rect.top() + halfH, rect.width(), halfH);
    painter->setPen(palette().windowText().color());
    painter->drawText(botRect, Qt::AlignCenter, QString::fromLatin1(kSubLabels[offset]));

    // Top half — only the RIGHT column of each pair repaints the full merged span.
    // Because Qt paints left-to-right, the right column runs last and can safely
    // overdraw the inner vertical divider that the base class drew for the left column.
    if (isRight && logicalIndex - 1 >= 0) {
        static const char *kGroupNames[] = { "LoTW", "eQSL", "QRZ" };

        const int leftW   = sectionSize(logicalIndex - 1);
        const QRect span(rect.x() - leftW, rect.top(), leftW + rect.width(), halfH);

        painter->setClipping(false);

        // Flood-fill both cells' top halves to erase the inner divider
        painter->fillRect(span, palette().button());

        // Redraw outer border of the merged cell
        painter->setPen(palette().mid().color());
        painter->drawRect(span.adjusted(0, 0, -1, -1));

        // Group name centred across the full span
        QFont f = painter->font();
        f.setBold(true);
        painter->setFont(f);
        painter->setPen(palette().windowText().color());
        painter->drawText(span, Qt::AlignCenter,
                          QString::fromLatin1(kGroupNames[offset / 2]));
    }

    painter->restore();
}

// ---------------------------------------------------------------------------
// QslBubbleDelegate
// ---------------------------------------------------------------------------

QslBubbleDelegate::QslBubbleDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{}

void QslBubbleDelegate::paint(QPainter *painter,
                               const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
    // Draw base (selection highlight, alternate row colour, etc.).
    // DisplayRole returns {} for QSL columns so no text is rendered.
    QStyledItemDelegate::paint(painter, option, index);

    const QVariant v = index.data(Qt::UserRole);
    if (!v.isValid() || !v.canConvert<QChar>()) return;

    const QChar status = v.value<QChar>();
    QColor color;
    if      (status == QChar('Y')) color = QColor(0x22, 0x8B, 0x22);  // forest green
    else if (status == QChar('N')) color = QColor(0xCC, 0x22, 0x22);  // crimson red
    else return;   // R / Q / I / blank — no bubble

    const int d = qMin(option.rect.width(), option.rect.height()) - 6;
    if (d <= 0) return;

    const QPoint c = option.rect.center();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(color);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QRect(c.x() - d / 2, c.y() - d / 2, d, d));
    painter->restore();
}

QSize QslBubbleDelegate::sizeHint(const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    return {24, 22};
}
