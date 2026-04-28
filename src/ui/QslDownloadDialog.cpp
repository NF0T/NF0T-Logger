// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "QslDownloadDialog.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDateEdit>
#include <QFont>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTime>
#include <QVBoxLayout>

#include "app/settings/Settings.h"
#include "qsl/QslService.h"

// ---------------------------------------------------------------------------
// QSL confirmation matching helpers
// ---------------------------------------------------------------------------

// Resolve both ADIF-spec and common-logger representations of a mode to a
// canonical (MODE, SUBMODE) pair so QSL matching is service-agnostic.
//
// Rules derived from ADIF 3.1.4 mode enumeration:
//   • FT4, FST4, FST4W, JS8, Q65 are submodes of MFSK
//   • PSK31, PSK63 (and variants) are submodes of PSK
//   • Renamed modes: PACKET→PKT, THROB→THRB, CONTESTIA→CONTESTI
static QPair<QString,QString> normaliseMode(const QString &mode, const QString &submode)
{
    const QString m = mode.toUpper().trimmed();
    const QString s = submode.toUpper().trimmed();

    // ---- MFSK submodes ----
    // Accept both "FT4" standalone (our UI convention) and "MFSK"/"FT4" (ADIF spec)
    static const QStringList mfskSubs = {
        "FT4", "FST4", "FST4W", "JS8", "Q65",
        "FSQCALL", "JTMS",
        "MFSK4","MFSK8","MFSK11","MFSK16","MFSK22",
        "MFSK31","MFSK32","MFSK64","MFSK64L","MFSK128","MFSK128L"
    };
    if (mfskSubs.contains(m))                       return {"MFSK", m};
    if (m == QLatin1String("MFSK") && !s.isEmpty()) return {"MFSK", s};

    // ---- PSK submodes ----
    // PSK31/PSK63 are commonly stored as top-level modes in older loggers
    static const QStringList pskSubs = {
        "PSK31","PSK63","PSK125","PSK250","PSK500","PSK1000",
        "PSK10","PSK63F","PSK2K","PSKAM10","PSKAM31","PSKAM50",
        "BPSK31","BPSK63","BPSK125","BPSK250","BPSK500","BPSK1000",
        "QPSK31","QPSK63","QPSK125","QPSK250","QPSK500"
    };
    if (pskSubs.contains(m))                       return {"PSK", m};
    if (m == QLatin1String("PSK") && !s.isEmpty()) return {"PSK", s};

    // ---- Renamed modes ----
    if (m == QLatin1String("PACKET") || m == QLatin1String("PKT"))       return {"PKT",      s};
    if (m == QLatin1String("THROB")  || m == QLatin1String("THRB"))      return {"THRB",     s};
    if (m == QLatin1String("CONTESTIA") || m == QLatin1String("CONTESTI")) return {"CONTESTI", s};

    return {m, s};
}

// Returns true if a normalised (mode, submode) pair from a QSL confirmation
// is compatible with a normalised pair from the local log.
//
// Fuzzy rules handle service-specific mode representations:
//   • LoTW DATA — catch-all for digital modes without a distinct LoTW code
//     (FT4, JT65, JT9, WSPR, JS8, FST4, Q65, Olivia, …). Modes with explicit
//     LoTW codes (CW, SSB, AM, FM, RTTY, FT8, PSK) are never reported as DATA.
//   • No submode — eQSL and others sometimes omit the submode (e.g. MFSK with
//     no FT4). Match if the parent modes agree.
static bool modesCompatible(const QPair<QString,QString> &conf,
                             const QPair<QString,QString> &local)
{
    if (conf == local) return true;
    if (conf.first == QLatin1String("DATA")) {
        static const QSet<QString> lotwNamed = {
            QStringLiteral("CW"),  QStringLiteral("SSB"), QStringLiteral("AM"),
            QStringLiteral("FM"),  QStringLiteral("RTTY"),QStringLiteral("FT8"),
            QStringLiteral("PSK")
        };
        return !lotwNamed.contains(local.first);
    }
    // Either side may omit the submode while the other carries it — the logger
    // may have stored only the parent mode, or the service may omit the submode.
    // Match if the parent modes agree and at least one side has no submode.
    if (conf.first == local.first && (conf.second.isEmpty() || local.second.isEmpty())) return true;
    return false;
}

// Returns true if a local QSO matches a QSL confirmation.
//
// Criteria 1–3 and 5 are always required. Criterion 4 (time) is controlled
// by requireTime — pass false to get a date/band/mode-only match when the
// confirmation time is unavailable or known to be unreliable (e.g. eQSL
// stores the other station's clock, which may differ by more than the window).
//
//   1. Callsign — case-insensitive exact match
//   2. Band     — case-insensitive exact match
//   3. UTC date — same calendar day
//   4. UTC time — within ±5 minutes when the confirmation carries a real time
//                 (not a midnight default) AND requireTime is true
//   5. Mode     — normalised and fuzzy-matched via modesCompatible()
static bool qsoMatchesConfirmation(const Qso &local, const Qso &conf,
                                   bool requireTime = true)
{
    if (local.callsign.compare(conf.callsign, Qt::CaseInsensitive) != 0) return false;
    if (local.band.compare(conf.band, Qt::CaseInsensitive) != 0) return false;

    const QDateTime localUtc = local.datetimeOn.toUTC();
    const QDateTime confUtc  = conf.datetimeOn.toUTC();
    if (localUtc.date() != confUtc.date()) return false;

    if (requireTime && confUtc.time() != QTime(0, 0)) {
        if (qAbs(localUtc.secsTo(confUtc)) > 5 * 60) return false;
    }

    const auto [confMode,  confSub]  = normaliseMode(conf.mode,  conf.submode);
    const auto [localMode, localSub] = normaliseMode(local.mode, local.submode);
    return modesCompatible({confMode, confSub}, {localMode, localSub});
}

QslDownloadDialog::QslDownloadDialog(const QList<QslService*> &services,
                                       const QList<Qso>         &localQsos,
                                       QWidget                  *parent)
    : QDialog(parent)
    , m_localQsos(localQsos)
{
    setWindowTitle(tr("Download QSL Confirmations"));
    setMinimumSize(640, 480);
    resize(700, 520);

    // Collect only downloadable services
    for (QslService *svc : services) {
        if (svc->canDownload())
            m_services.append(svc);
    }

    // -----------------------------------------------------------------------
    // Top form: service + date range
    // -----------------------------------------------------------------------
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_serviceCombo = new QComboBox(this);
    for (QslService *svc : m_services)
        m_serviceCombo->addItem(svc->displayName());
    form->addRow(tr("Service:"), m_serviceCombo);

    auto *dateRow = new QHBoxLayout;
    m_fromDate = new QDateEdit(this);
    m_fromDate->setCalendarPopup(true);
    m_fromDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));

    m_toDate = new QDateEdit(QDate::currentDate(), this);
    m_toDate->setCalendarPopup(true);
    m_toDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_toDate->setMaximumDate(QDate::currentDate());

    dateRow->addWidget(m_fromDate);
    dateRow->addWidget(new QLabel(tr("to"), this));
    dateRow->addWidget(m_toDate);
    dateRow->addStretch();
    form->addRow(tr("Date range:"), dateRow);

    // -----------------------------------------------------------------------
    // Separator
    // -----------------------------------------------------------------------
    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);

    // -----------------------------------------------------------------------
    // Log area
    // -----------------------------------------------------------------------
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_log->setPlaceholderText(tr("Download log will appear here..."));

    // -----------------------------------------------------------------------
    // Status + buttons
    // -----------------------------------------------------------------------
    m_statusLabel = new QLabel(tr("Ready."), this);

    m_downloadBtn = new QPushButton(tr("Download"), this);
    m_closeBtn    = new QPushButton(tr("Close"),    this);
    m_closeBtn->setDefault(false);
    m_downloadBtn->setDefault(true);

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_statusLabel, 1);
    btnRow->addWidget(m_downloadBtn);
    btnRow->addWidget(m_closeBtn);

    // -----------------------------------------------------------------------
    // Main layout
    // -----------------------------------------------------------------------
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(form);
    mainLayout->addWidget(sep);
    mainLayout->addWidget(m_log, 1);
    mainLayout->addLayout(btnRow);

    // -----------------------------------------------------------------------
    // Connections
    // -----------------------------------------------------------------------
    connect(m_serviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QslDownloadDialog::onServiceChanged);
    connect(m_downloadBtn, &QPushButton::clicked, this, &QslDownloadDialog::onDownloadClicked);
    connect(m_closeBtn,    &QPushButton::clicked, this, &QDialog::close);

    // Initialise date for the first service
    if (!m_services.isEmpty())
        onServiceChanged(0);

    if (m_services.isEmpty()) {
        m_downloadBtn->setEnabled(false);
        m_statusLabel->setText(tr("No download-capable services are configured."));
    }
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void QslDownloadDialog::onServiceChanged(int index)
{
    if (index < 0 || index >= m_services.size()) return;
    m_currentService = m_services.at(index);

    const QDate lastDl = Settings::instance().qslLastDownloadDate(m_currentService->displayName());
    m_fromDate->setDate(lastDl);
    m_fromDate->setMaximumDate(QDate::currentDate());

    m_statusLabel->setText(tr("Ready."));
}

void QslDownloadDialog::onDownloadClicked()
{
    if (!m_currentService) return;

    m_log->clear();
    setRunning(true);

    const QDate from = m_fromDate->date();
    const QDate to   = m_toDate->date();

    appendLog(tr("Starting %1 download (from %2 to %3)...")
                  .arg(m_currentService->displayName(),
                       from.toString(Qt::ISODate),
                       to.toString(Qt::ISODate)));

    m_serviceConns << connect(m_currentService, &QslService::logMessage,
                              this, &QslDownloadDialog::appendLog);
    m_serviceConns << connect(m_currentService, &QslService::downloadFinished,
                              this, &QslDownloadDialog::onDownloadFinished);

    m_currentService->startDownload(from, to);
}

void QslDownloadDialog::onDownloadFinished(const QList<Qso> &confirmed,
                                            const QStringList &errors)
{
    disconnectService();
    setRunning(false);

    if (!errors.isEmpty())
        appendLog(tr("Warnings: %1").arg(errors.join(QLatin1String("; "))));

    // -----------------------------------------------------------------------
    // Match each confirmed stub against the local log and log per-record status
    // -----------------------------------------------------------------------
    QList<Qso> matched;

    if (!confirmed.isEmpty()) {
        appendLog(tr("--- Matching %1 record(s) against local log ---")
                      .arg(confirmed.size()));

        int cntNew = 0, cntAlready = 0, cntNotFound = 0;

        for (const Qso &conf : confirmed) {
            const auto [confMode, confSub] = normaliseMode(conf.mode, conf.submode);
            const QString key = QStringLiteral("%1  %2  %3  %4")
                                    .arg(conf.callsign,
                                         conf.datetimeOn.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm")),
                                         conf.band, confMode + (confSub.isEmpty() ? QString() : QStringLiteral("/") + confSub));

            // Two-pass: prefer a time-verified match; fall back to date/band/mode
            // only if nothing time-matches (e.g. eQSL stores the other station's
            // clock, which can differ by more than the ±5 min window).
            auto findMatch = [&](bool requireTime) -> std::optional<Qso> {
                for (const Qso &local : m_localQsos)
                    if (qsoMatchesConfirmation(local, conf, requireTime))
                        return local;
                return std::nullopt;
            };
            auto localOpt = findMatch(true);
            const bool timeVerified = localOpt.has_value();
            if (!localOpt) localOpt = findMatch(false);

            const bool found = localOpt.has_value();
            if (found) {
                Qso local = *localOpt;

                const bool alreadyLoTW = (conf.lotwQslRcvd == 'Y' && local.lotwQslRcvd == 'Y');
                const bool alreadyEqsl = (conf.eqslQslRcvd == 'Y' && local.eqslQslRcvd == 'Y');
                const bool alreadyQrz  = (conf.qrzQslRcvd  == 'Y' && local.qrzQslRcvd  == 'Y');

                if (alreadyLoTW || alreadyEqsl || alreadyQrz) {
                    appendLog(tr("  %1 — already confirmed, skipping").arg(key));
                    ++cntAlready;
                } else {
                    if (conf.lotwQslRcvd == 'Y') { local.lotwQslRcvd = 'Y'; local.lotwRcvdDate = conf.lotwRcvdDate; }
                    if (conf.eqslQslRcvd == 'Y') { local.eqslQslRcvd = 'Y'; local.eqslRcvdDate = conf.eqslRcvdDate; }
                    if (conf.qrzQslRcvd  == 'Y') { local.qrzQslRcvd  = 'Y'; local.qrzRcvdDate  = conf.qrzRcvdDate; }

                    // Backfill blank contacted-station fields from the confirmation ADIF.
                    // Only write fields that are empty/unknown in the local record.
                    auto fillStr = [](QString &dst, const QString &src) {
                        if (dst.isEmpty() && !src.isEmpty()) dst = src;
                    };
                    auto fillInt = [](int &dst, int src) {
                        if (dst == 0 && src != 0) dst = src;
                    };
                    fillStr(local.name,       conf.name);
                    fillStr(local.qth,        conf.qth);
                    fillStr(local.gridsquare, conf.gridsquare);
                    fillStr(local.country,    conf.country);
                    fillStr(local.state,      conf.state);
                    fillStr(local.county,     conf.county);
                    fillStr(local.cont,       conf.cont);
                    fillStr(local.pfx,        conf.pfx);
                    fillStr(local.arrlSect,   conf.arrlSect);
                    fillInt(local.dxcc,       conf.dxcc);
                    fillInt(local.cqZone,     conf.cqZone);
                    fillInt(local.ituZone,    conf.ituZone);
                    if (!local.lat.has_value()      && conf.lat.has_value())      local.lat      = conf.lat;
                    if (!local.lon.has_value()      && conf.lon.has_value())      local.lon      = conf.lon;
                    if (!local.distance.has_value() && conf.distance.has_value()) local.distance = conf.distance;

                    const QString note = timeVerified ? QString() : tr(" (time unverified)");
                    appendLog(tr("  %1 — new confirmation%2").arg(key, note));
                    matched.append(local);
                    ++cntNew;
                }
            }

            if (!found) {
                appendLog(tr("  %1 — no matching QSO in log").arg(key));
                // Diagnostic: show why each same-callsign candidate failed
                for (const Qso &local : m_localQsos) {
                    if (local.callsign.compare(conf.callsign, Qt::CaseInsensitive) != 0) continue;
                    const QDateTime lu = local.datetimeOn.toUTC();
                    const QDateTime cu = conf.datetimeOn.toUTC();
                    const auto [lm, ls] = normaliseMode(local.mode, local.submode);
                    const QString localDesc = QStringLiteral("%1  %2  %3  %4")
                        .arg(local.callsign,
                             lu.toString(QStringLiteral("yyyy-MM-dd HH:mm")),
                             local.band,
                             lm + (ls.isEmpty() ? QString() : QStringLiteral("/") + ls));
                    QStringList why;
                    if (local.band.compare(conf.band, Qt::CaseInsensitive) != 0)
                        why << QStringLiteral("band %1≠%2").arg(local.band, conf.band);
                    if (lu.date() != cu.date())
                        why << QStringLiteral("date %1≠%2").arg(lu.date().toString(), cu.date().toString());
                    else if (cu.time() != QTime(0, 0))
                        why << QStringLiteral("time diff %1s").arg(qAbs(lu.secsTo(cu)));
                    if (!modesCompatible({confMode, confSub}, {lm, ls}))
                        why << QStringLiteral("mode %1/%2≠%3/%4").arg(lm, ls, confMode, confSub);
                    appendLog(QStringLiteral("    candidate: %1 [%2]").arg(
                        localDesc, why.isEmpty() ? QStringLiteral("all match?") : why.join(QStringLiteral(", "))));
                }
                ++cntNotFound;
            }
        }

        appendLog(tr("--- Result: %1 new, %2 already confirmed, %3 not found ---")
                      .arg(cntNew).arg(cntAlready).arg(cntNotFound));
    }

    // Save today as last-download date on any non-error result
    if (errors.isEmpty() || !confirmed.isEmpty()) {
        Settings::instance().setQslLastDownloadDate(
            m_currentService->displayName(), QDate::currentDate());
    }

    const QString summary = matched.isEmpty()
        ? tr("No new confirmations to apply.")
        : tr("Download complete — %1 new confirmation(s) applied.").arg(matched.size());

    appendLog(summary);
    m_statusLabel->setText(summary);

    emit downloadCompleted(matched, errors);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void QslDownloadDialog::appendLog(const QString &msg)
{
    const QString ts = QTime::currentTime().toString(QStringLiteral("[HH:mm:ss] "));
    // Split multi-line messages so each line gets a timestamp
    const QStringList lines = msg.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines)
        m_log->appendPlainText(ts + line.trimmed());
}

void QslDownloadDialog::setRunning(bool running)
{
    m_running = running;
    m_downloadBtn->setEnabled(!running);
    m_serviceCombo->setEnabled(!running);
    m_fromDate->setEnabled(!running);
    m_toDate->setEnabled(!running);
    m_statusLabel->setText(running ? tr("Downloading...") : tr("Ready."));
}

void QslDownloadDialog::disconnectService()
{
    for (auto &c : m_serviceConns)
        disconnect(c);
    m_serviceConns.clear();
}

void QslDownloadDialog::closeEvent(QCloseEvent *event)
{
    if (m_running && m_currentService) {
        disconnectService();
        m_currentService->abort();
        m_running = false;
        appendLog(tr("Download aborted."));
    }
    QDialog::closeEvent(event);
}
