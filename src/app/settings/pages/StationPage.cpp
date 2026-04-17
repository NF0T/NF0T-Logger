#include "StationPage.h"
#include "app/settings/Settings.h"
#include "core/Maidenhead.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

StationPage::StationPage(QWidget *parent)
    : SettingsPage(parent)
{
    m_callsign = new QLineEdit(this);
    m_callsign->setPlaceholderText(tr("e.g. NF0T"));
    m_callsign->setMaxLength(20);

    m_name     = new QLineEdit(this);
    m_grid     = new QLineEdit(this);
    m_grid->setPlaceholderText(tr("e.g. EM38"));
    m_grid->setMaxLength(10);
    connect(m_grid, &QLineEdit::editingFinished, this, [this]() {
        const QString g = m_grid->text().trimmed().toUpper();
        double lat, lon;
        if (Maidenhead::toLatLon(g, lat, lon)) {
            m_lat->setValue(lat);
            m_lon->setValue(lon);
        }
    });

    m_city     = new QLineEdit(this);
    m_state    = new QLineEdit(this);
    m_state->setMaxLength(100);
    m_country  = new QLineEdit(this);

    m_dxcc     = new QSpinBox(this);
    m_dxcc->setRange(0, 9999);
    m_dxcc->setSpecialValueText(tr("Unknown"));

    m_cqZone   = new QSpinBox(this);
    m_cqZone->setRange(0, 40);
    m_cqZone->setSpecialValueText(tr("Unknown"));

    m_ituZone  = new QSpinBox(this);
    m_ituZone->setRange(0, 90);
    m_ituZone->setSpecialValueText(tr("Unknown"));

    m_lat      = new QDoubleSpinBox(this);
    m_lat->setRange(-90.0, 90.0);
    m_lat->setDecimals(6);
    m_lat->setSuffix(tr("°"));
    m_lat->setSpecialValueText(tr("Not set"));
    m_lat->setValue(-91.0);   // sentinel = "not set"

    m_lon      = new QDoubleSpinBox(this);
    m_lon->setRange(-181.0, 180.0);
    m_lon->setDecimals(6);
    m_lon->setSuffix(tr("°"));
    m_lon->setSpecialValueText(tr("Not set"));
    m_lon->setValue(-181.0);  // sentinel = "not set"

    auto *form = new QFormLayout;
    form->addRow(tr("Callsign *:"),   m_callsign);
    form->addRow(tr("Name:"),         m_name);
    form->addRow(tr("Grid square:"),  m_grid);
    form->addRow(tr("City:"),         m_city);
    form->addRow(tr("State/Region:"), m_state);
    form->addRow(tr("Country:"),      m_country);
    form->addRow(tr("DXCC entity:"),  m_dxcc);
    form->addRow(tr("CQ zone:"),      m_cqZone);
    form->addRow(tr("ITU zone:"),     m_ituZone);
    form->addRow(tr("Latitude:"),     m_lat);
    form->addRow(tr("Longitude:"),    m_lon);

    auto *inner = new QWidget(this);
    inner->setLayout(form);

    auto *scroll = new QScrollArea(this);
    scroll->setWidget(inner);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll);
}

void StationPage::load()
{
    const Settings &s = Settings::instance();
    m_callsign->setText(s.stationCallsign());
    m_name->setText(s.stationName());
    m_grid->setText(s.stationGridsquare());
    m_city->setText(s.stationCity());
    m_state->setText(s.stationState());
    m_country->setText(s.stationCountry());
    m_dxcc->setValue(s.stationDxcc());
    m_cqZone->setValue(s.stationCqZone());
    m_ituZone->setValue(s.stationItuZone());

    const auto lat = s.stationLat();
    m_lat->setValue(lat.has_value() ? *lat : -91.0);

    const auto lon = s.stationLon();
    m_lon->setValue(lon.has_value() ? *lon : -181.0);
}

void StationPage::apply()
{
    Settings &s = Settings::instance();
    s.setStationCallsign(m_callsign->text().toUpper().trimmed());
    s.setStationName(m_name->text().trimmed());
    s.setStationGridsquare(m_grid->text().toUpper().trimmed());
    s.setStationCity(m_city->text().trimmed());
    s.setStationState(m_state->text().trimmed());
    s.setStationCountry(m_country->text().trimmed());
    s.setStationDxcc(m_dxcc->value());
    s.setStationCqZone(m_cqZone->value());
    s.setStationItuZone(m_ituZone->value());

    s.setStationLat(m_lat->value() < -90.0 ? std::nullopt : std::optional<double>(m_lat->value()));
    s.setStationLon(m_lon->value() < -180.0 ? std::nullopt : std::optional<double>(m_lon->value()));
}
