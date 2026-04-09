#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include "core/logbook/Qso.h"

/// Abstract base for all QSL upload/download services.
///
/// Workflow
/// --------
/// Upload:
///   - Caller passes ALL local QSOs; each service filters to unsent ones.
///   - On completion, uploadFinished() carries the updated Qso objects
///     (with sent flag + date set) so MainWindow can call DB::updateQso.
///
/// Download (LoTW, eQSL, QRZ):
///   - Service fetches confirmations from the remote service.
///   - downloadFinished() carries partial Qso stubs with enough fields to
///     match against local records (callsign, date, band, mode) and the
///     rcvd flag + date filled in. MainWindow does the matching and DB update.
///
/// ClubLog does not support download; canDownload() returns false.
class QslService : public QObject
{
    Q_OBJECT

public:
    explicit QslService(QObject *parent = nullptr) : QObject(parent) {}
    ~QslService() override = default;

    virtual QString displayName() const = 0;
    virtual bool    isEnabled()   const = 0;
    virtual bool    canDownload() const { return true; }

    /// Start uploading. Filters internally to unsent QSOs.
    virtual void startUpload(const QList<Qso> &allQsos) = 0;

    /// Start downloading confirmations. No-op if canDownload() is false.
    virtual void startDownload() = 0;

    /// Cancel any in-progress operation.
    virtual void abort() = 0;

signals:
    /// Emitted periodically during upload (done=0, total=0 = indeterminate).
    void uploadProgress(int done, int total);

    /// Upload complete. updatedQsos have the sent flag + date already set.
    /// errors contains non-fatal messages; a fatal error emits only errors
    /// and an empty updatedQsos list.
    void uploadFinished(const QList<Qso> &updatedQsos, const QStringList &errors);

    /// Download complete. confirmed are stubs with callsign/datetimeOn/band/mode
    /// plus the rcvd flag and rcvd date filled in. MainWindow matches these
    /// against the local log and calls DB::updateQso.
    void downloadFinished(const QList<Qso> &confirmed, const QStringList &errors);
};
