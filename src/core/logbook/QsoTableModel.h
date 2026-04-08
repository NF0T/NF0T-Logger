#pragma once

#include <QAbstractTableModel>
#include <QList>
#include "Qso.h"

/// QAbstractTableModel backed by a list of Qso records.
/// MainWindow owns the database; call setQsos() after each load/reload.
class QsoTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColDate = 0,
        ColTime,
        ColCallsign,
        ColBand,
        ColMode,
        ColFreq,
        ColRstSent,
        ColRstRcvd,
        ColName,
        ColCountry,
        ColGrid,
        ColDistance,
        ColLotw,
        ColEqsl,
        ColQrz,
        ColClublog,
        ColCount   // sentinel — always last
    };

    explicit QsoTableModel(QObject *parent = nullptr);

    // QAbstractTableModel
    int      rowCount   (const QModelIndex &parent = {}) const override;
    int      columnCount(const QModelIndex &parent = {}) const override;
    QVariant data       (const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Data management
    void setQsos    (const QList<Qso> &qsos);
    void appendQso  (const Qso &qso);
    void updateQso  (int row, const Qso &qso);
    void removeQso  (int row);
    void clear      ();

    const Qso &qsoAt(int row) const;

private:
    static QString qslDisplay(QChar sent, QChar rcvd);

    QList<Qso> m_qsos;
};
