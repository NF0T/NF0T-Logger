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

    // Base call draws the themed background. headerData() returns "" for QSL
    // columns so no text is rendered by the base.
    QHeaderView::paintSection(painter, rect, logicalIndex);

    if (logicalIndex < QsoTableModel::ColQslFirst) {
        painter->restore();
        return;
    }

    const int halfH  = rect.height() / 2;
    const int offset = logicalIndex - QsoTableModel::ColQslFirst;

    // Horizontal divider between group row and sub-label row
    painter->setPen(palette().mid().color());
    painter->drawLine(rect.left(), rect.top() + halfH, rect.right(), rect.top() + halfH);

    // Bottom half — S / R sub-label
    static const char *kSubLabels[] = { "S", "R", "S", "R", "S", "R", "S", "" };
    const QRect botRect(rect.x(), rect.top() + halfH, rect.width(), halfH);
    painter->setPen(palette().windowText().color());
    painter->drawText(botRect, Qt::AlignCenter, QString::fromLatin1(kSubLabels[offset]));

    // Top half — service group name, painted only by the left sub-column of each pair.
    // We disable clipping so the text can span across the adjacent sub-column.
    if (offset % 2 == 0) {
        static const char *kGroupNames[] = { "LoTW", "eQSL", "QRZ", "ClubLog" };
        const int nextW = (logicalIndex + 1 < count()) ? sectionSize(logicalIndex + 1) : 0;
        const QRect groupRect(rect.x(), rect.top(), rect.width() + nextW, halfH);

        painter->setClipping(false);
        QFont f = painter->font();
        f.setBold(true);
        painter->setFont(f);
        painter->setPen(palette().windowText().color());
        painter->drawText(groupRect, Qt::AlignCenter,
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
