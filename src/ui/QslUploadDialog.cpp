#include "QslUploadDialog.h"

#include <QCloseEvent>
#include <QComboBox>
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

#include "qsl/QslService.h"

QslUploadDialog::QslUploadDialog(const QList<QslService*> &services,
                                   const QList<Qso>         &allQsos,
                                   QWidget                  *parent)
    : QDialog(parent)
    , m_allQsos(allQsos)
{
    setWindowTitle(tr("Upload QSOs to QSL Service"));
    setMinimumSize(640, 480);
    resize(700, 520);

    // Only include enabled services
    for (QslService *svc : services) {
        if (svc->isEnabled())
            m_services.append(svc);
    }

    // -----------------------------------------------------------------------
    // Top form: service selector + pending count
    // -----------------------------------------------------------------------
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_serviceCombo = new QComboBox(this);
    for (QslService *svc : m_services)
        m_serviceCombo->addItem(svc->displayName());
    form->addRow(tr("Service:"), m_serviceCombo);

    m_pendingLabel = new QLabel(this);
    form->addRow(tr("Log size:"), m_pendingLabel);

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
    m_log->setPlaceholderText(tr("Upload log will appear here..."));

    // -----------------------------------------------------------------------
    // Status + buttons
    // -----------------------------------------------------------------------
    m_statusLabel = new QLabel(tr("Ready."), this);

    m_uploadBtn = new QPushButton(tr("Upload"), this);
    m_closeBtn  = new QPushButton(tr("Close"),  this);
    m_closeBtn->setDefault(false);
    m_uploadBtn->setDefault(true);

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_statusLabel, 1);
    btnRow->addWidget(m_uploadBtn);
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
            this, &QslUploadDialog::onServiceChanged);
    connect(m_uploadBtn, &QPushButton::clicked, this, &QslUploadDialog::onUploadClicked);
    connect(m_closeBtn,  &QPushButton::clicked, this, &QDialog::close);

    // Initialise for the first service
    if (!m_services.isEmpty())
        onServiceChanged(0);

    if (m_services.isEmpty()) {
        m_uploadBtn->setEnabled(false);
        m_pendingLabel->setText(tr("No QSL services are enabled."));
        m_statusLabel->setText(tr("Enable at least one service in Settings → QSL Services."));
    }
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void QslUploadDialog::onServiceChanged(int /*index*/)
{
    if (m_services.isEmpty()) return;
    m_currentService = m_services.at(m_serviceCombo->currentIndex());
    m_pendingLabel->setText(tr("%1 QSO(s) in log").arg(m_allQsos.size()));
    m_statusLabel->setText(tr("Ready."));
}

void QslUploadDialog::onUploadClicked()
{
    if (!m_currentService) return;

    m_log->clear();
    setRunning(true);

    appendLog(tr("Starting upload to %1...").arg(m_currentService->displayName()));

    m_serviceConns << connect(m_currentService, &QslService::logMessage,
                              this, &QslUploadDialog::appendLog);
    m_serviceConns << connect(m_currentService, &QslService::uploadFinished,
                              this, &QslUploadDialog::onUploadFinished);

    m_currentService->startUpload(m_allQsos);
}

void QslUploadDialog::onUploadFinished(const QList<Qso> &updated, const QStringList &errors)
{
    disconnectService();
    setRunning(false);

    if (!errors.isEmpty())
        appendLog(tr("Messages: %1").arg(errors.join(QLatin1String("; "))));

    const QString summary = updated.isEmpty()
        ? tr("Upload complete — no new QSOs sent.")
        : tr("Upload complete — %1 QSO(s) sent.").arg(updated.size());

    appendLog(summary);
    m_statusLabel->setText(summary);

    emit uploadCompleted(updated, errors);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void QslUploadDialog::appendLog(const QString &msg)
{
    const QString ts = QTime::currentTime().toString(QStringLiteral("[HH:mm:ss] "));
    const QStringList lines = msg.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines)
        m_log->appendPlainText(ts + line.trimmed());
}

void QslUploadDialog::setRunning(bool running)
{
    m_running = running;
    m_uploadBtn->setEnabled(!running);
    m_serviceCombo->setEnabled(!running);
    m_statusLabel->setText(running ? tr("Uploading...") : tr("Ready."));
}

void QslUploadDialog::disconnectService()
{
    for (auto &c : m_serviceConns)
        disconnect(c);
    m_serviceConns.clear();
}

void QslUploadDialog::closeEvent(QCloseEvent *event)
{
    if (m_running && m_currentService) {
        disconnectService();
        m_currentService->abort();
        m_running = false;
        appendLog(tr("Upload aborted."));
    }
    QDialog::closeEvent(event);
}
