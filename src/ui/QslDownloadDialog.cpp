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
// Mode normalisation
//
// Some services (eQSL) follow the ADIF spec strictly and return FT4 as
// MODE=MFSK / SUBMODE=FT4.  Our entry panel stores FT4 as MODE=FT4 (a
// common logger convention).  Normalise both to a canonical pair so the
// matching logic handles either representation.
// ---------------------------------------------------------------------------
static QPair<QString,QString> normaliseMode(const QString &mode, const QString &submode)
{
    const QString m = mode.toUpper();
    const QString s = submode.toUpper();

    // ADIF spec: FT4 is MFSK / FT4 — accept either spelling
    if (m == QLatin1String("FT4"))                                return {"MFSK", "FT4"};
    if (m == QLatin1String("MFSK") && s == QLatin1String("FT4")) return {"MFSK", "FT4"};

    // FST4 / FST4W live under MFSK too
    if (m == QLatin1String("FST4"))                                return {"MFSK", "FST4"};
    if (m == QLatin1String("MFSK") && s == QLatin1String("FST4")) return {"MFSK", "FST4"};
    if (m == QLatin1String("FST4W"))                               return {"MFSK", "FST4W"};
    if (m == QLatin1String("MFSK") && s == QLatin1String("FST4W"))return {"MFSK", "FST4W"};

    // JS8 / JS8Call
    if (m == QLatin1String("JS8") || (m == QLatin1String("MFSK") && s == QLatin1String("JS8")))
        return {"MFSK", "JS8"};

    return {m, s};
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
                                         conf.datetimeOn.toUTC().toString(QStringLiteral("yyyy-MM-dd")),
                                         conf.band, confMode + (confSub.isEmpty() ? QString() : QStringLiteral("/") + confSub));

            bool found = false;
            for (Qso local : m_localQsos) {
                if (local.callsign.compare(conf.callsign, Qt::CaseInsensitive) != 0) continue;
                if (local.band.compare(conf.band, Qt::CaseInsensitive) != 0) continue;
                if (local.datetimeOn.toUTC().date() != conf.datetimeOn.toUTC().date()) continue;
                const auto [localMode, localSub] = normaliseMode(local.mode, local.submode);
                if (localMode != confMode || localSub != confSub) continue;

                found = true;

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
                    appendLog(tr("  %1 — new confirmation").arg(key));
                    matched.append(local);
                    ++cntNew;
                }
                break;
            }

            if (!found) {
                appendLog(tr("  %1 — no matching QSO in log").arg(key));
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
