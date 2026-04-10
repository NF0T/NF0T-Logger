#include "QsoTableModel.h"

#include <QColor>
#include "app/settings/Settings.h"

static const char *COL_HEADERS[] = {
    "Date", "Time (UTC)", "Callsign", "Band", "Mode", "Freq (MHz)",
    "RST Sent", "RST Rcvd", "Name", "Country", "Grid",
    "",    // ColDistance — dynamic, returned by headerData()
    // QSL sub-columns — group names and S/R labels are painted by QslGroupHeaderView
    "", "", "", "", "", ""
};
static_assert(sizeof(COL_HEADERS) / sizeof(COL_HEADERS[0]) == QsoTableModel::ColCount,
              "COL_HEADERS size mismatch with Column enum");

QsoTableModel::QsoTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

int QsoTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_qsos.size();
}

int QsoTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

QVariant QsoTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_qsos.size())
        return {};

    const Qso &q = m_qsos.at(index.row());

    // QSL bubble columns — status char returned via UserRole; delegate paints the bubble
    if (index.column() >= ColQslFirst) {
        if (role != Qt::UserRole) return {};
        switch (index.column()) {
        case ColLotwS:    return QVariant(q.lotwQslSent);
        case ColLotwR:    return QVariant(q.lotwQslRcvd);
        case ColEqslS:    return QVariant(q.eqslQslSent);
        case ColEqslR:    return QVariant(q.eqslQslRcvd);
        case ColQrzS:     return QVariant(q.qrzQslSent);
        case ColQrzR:     return QVariant(q.qrzQslRcvd);
        default:          return {};
        }
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColDate:      return q.datetimeOn.toUTC().toString("yyyy-MM-dd");
        case ColTime:      return q.datetimeOn.toUTC().toString("HH:mm");
        case ColCallsign:  return q.callsign;
        case ColBand:      return q.band;
        case ColMode:      return q.submode.isEmpty() ? q.mode
                                                       : QString("%1/%2").arg(q.mode, q.submode);
        case ColFreq:      return QString::number(q.freq, 'f', 3);
        case ColRstSent:   return q.rstSent;
        case ColRstRcvd:   return q.rstRcvd;
        case ColName:      return q.name;
        case ColCountry:   return q.country;
        case ColGrid:      return q.gridsquare;
        case ColDistance: {
            if (!q.distance.has_value()) return {};
            const bool metric = Settings::instance().useMetricUnits();
            const double val  = metric ? *q.distance : *q.distance * 0.621371;
            return QString::number(val, 'f', 0);
        }
        default:           return {};
        }
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case ColFreq:
        case ColDistance:
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    return {};
}

QVariant QsoTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section >= ColCount)
        return {};
    if (section == ColDistance)
        return Settings::instance().useMetricUnits() ? tr("Dist (km)") : tr("Dist (mi)");
    return QString::fromUtf8(COL_HEADERS[section]);
}

// ---------------------------------------------------------------------------
// Data management
// ---------------------------------------------------------------------------

void QsoTableModel::setQsos(const QList<Qso> &qsos)
{
    beginResetModel();
    m_qsos = qsos;
    endResetModel();
}

void QsoTableModel::appendQso(const Qso &qso)
{
    beginInsertRows({}, m_qsos.size(), m_qsos.size());
    m_qsos.append(qso);
    endInsertRows();
}

void QsoTableModel::prependQso(const Qso &qso)
{
    beginInsertRows({}, 0, 0);
    m_qsos.prepend(qso);
    endInsertRows();
}

void QsoTableModel::updateQso(int row, const Qso &qso)
{
    if (row < 0 || row >= m_qsos.size())
        return;
    m_qsos[row] = qso;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

void QsoTableModel::removeQso(int row)
{
    if (row < 0 || row >= m_qsos.size())
        return;
    beginRemoveRows({}, row, row);
    m_qsos.removeAt(row);
    endRemoveRows();
}

void QsoTableModel::clear()
{
    beginResetModel();
    m_qsos.clear();
    endResetModel();
}

const Qso &QsoTableModel::qsoAt(int row) const
{
    return m_qsos.at(row);
}

