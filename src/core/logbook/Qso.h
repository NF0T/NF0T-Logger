// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <optional>
#include <QChar>
#include <QDate>
#include <QDateTime>
#include <QMetaType>
#include <QString>

/// Represents a single amateur radio contact (QSO).
/// Column names in comments correspond to the database schema.
struct Qso
{
    qint64   id = -1;   // -1 = not yet persisted

    // -----------------------------------------------------------------------
    // Core — required fields
    // -----------------------------------------------------------------------
    QString   callsign;          // callsign
    QDateTime datetimeOn;        // datetime_on  (UTC)
    QString   band;              // band
    double    freq = 0.0;        // freq  (MHz)
    QString   mode;              // mode

    // -----------------------------------------------------------------------
    // Core — optional
    // -----------------------------------------------------------------------
    QDateTime datetimeOff;                // datetime_off (UTC, invalid = NULL)
    QString   submode;                   // submode
    std::optional<double> freqRx;        // freq_rx (split RX, MHz)
    QString   bandRx;                    // band_rx (cross-band split)

    // -----------------------------------------------------------------------
    // RST
    // -----------------------------------------------------------------------
    QString   rstSent;           // rst_sent
    QString   rstRcvd;           // rst_rcvd

    // -----------------------------------------------------------------------
    // Contacted station
    // -----------------------------------------------------------------------
    QString   name;              // name
    QString   contactedOp;       // contacted_op (operator at club/multi-op stn)
    QString   qth;               // qth
    QString   gridsquare;        // gridsquare
    QString   country;           // country
    QString   state;             // state
    QString   county;            // county
    QString   cont;              // cont  (2-char continent code)
    int       dxcc     = 0;      // dxcc
    int       cqZone   = 0;      // cq_zone  (0 = unknown)
    int       ituZone  = 0;      // itu_zone (0 = unknown)
    QString   pfx;               // pfx
    QString   arrlSect;          // arrl_sect
    std::optional<double> lat;   // lat
    std::optional<double> lon;   // lon
    std::optional<double> distance; // distance (km)
    std::optional<int>    age;   // age
    QString   eqCall;            // eq_call (alternate eQSL callsign)

    // -----------------------------------------------------------------------
    // My station
    // -----------------------------------------------------------------------
    QString   stationCallsign;   // station_callsign (callsign used for QSO)
    QString   operatorCall;      // operator_call
    QString   ownerCallsign;     // owner_callsign
    QString   myName;            // my_name
    QString   myGridsquare;      // my_gridsquare
    QString   myCity;            // my_city
    QString   myState;           // my_state
    QString   myCounty;          // my_county
    QString   myCountry;         // my_country
    int       myDxcc    = 0;     // my_dxcc
    int       myCqZone  = 0;     // my_cq_zone
    int       myItuZone = 0;     // my_itu_zone
    std::optional<double> myLat; // my_lat
    std::optional<double> myLon; // my_lon
    std::optional<double> txPwr; // tx_pwr (Watts)
    std::optional<double> rxPwr; // rx_pwr (Watts)
    QString   antenna;           // antenna
    std::optional<double> antAz; // ant_az (degrees)
    std::optional<double> antEl; // ant_el (degrees)
    QString   antPath;           // ant_path (GS/LP/SP/O)
    QString   rig;               // rig

    // -----------------------------------------------------------------------
    // QSL status — CHAR(1): Y=Yes N=No R=Requested Q=Queued I=Ignore
    // -----------------------------------------------------------------------
    QChar     lotwQslSent    = 'N';   // lotw_qsl_sent
    QChar     lotwQslRcvd   = 'N';   // lotw_qsl_rcvd
    QDate     lotwSentDate;           // lotw_sent_date
    QDate     lotwRcvdDate;           // lotw_rcvd_date

    QChar     eqslQslSent   = 'N';   // eqsl_qsl_sent
    QChar     eqslQslRcvd   = 'N';   // eqsl_qsl_rcvd
    QDate     eqslSentDate;           // eqsl_sent_date
    QDate     eqslRcvdDate;           // eqsl_rcvd_date

    QChar     qrzQslSent    = 'N';   // qrz_qsl_sent
    QChar     qrzQslRcvd    = 'N';   // qrz_qsl_rcvd
    QDate     qrzSentDate;            // qrz_sent_date
    QDate     qrzRcvdDate;            // qrz_rcvd_date

    QChar     clublogQslSent = 'N';  // clublog_qsl_sent
    QDate     clublogSentDate;        // clublog_sent_date (no inbound confirmation)

    QChar     qslSent       = 'N';   // qsl_sent  (paper)
    QChar     qslRcvd       = 'N';   // qsl_rcvd
    QDate     qslSentDate;            // qsl_sent_date
    QDate     qslRcvdDate;            // qsl_rcvd_date
    QString   qslVia;                 // qsl_via
    QString   qslMsg;                 // qsl_msg

    // -----------------------------------------------------------------------
    // Propagation conditions at time of QSO
    // -----------------------------------------------------------------------
    QString              propMode;    // prop_mode
    std::optional<double> sfi;        // sfi  (solar flux index)
    std::optional<double> aIndex;     // a_index
    std::optional<double> kIndex;     // k_index
    std::optional<int>   sunspots;    // sunspots

    // -----------------------------------------------------------------------
    // Contest
    // -----------------------------------------------------------------------
    QString              contestId;   // contest_id
    std::optional<int>   srx;         // srx  (received serial, numeric)
    QString              srxString;   // srx_string (received exchange, freeform)
    std::optional<int>   stx;         // stx
    QString              stxString;   // stx_string
    QString              precedence;  // precedence (e.g. Sweepstakes)
    QString              contestClass;// contest_class
    QString              arrlCheck;   // arrl_check

    // -----------------------------------------------------------------------
    // Special activities (SOTA, POTA, WWFF, IOTA, etc.)
    // -----------------------------------------------------------------------
    QString   sig;        // sig       (activity name, e.g. "POTA")
    QString   sigInfo;    // sig_info  (reference, e.g. "K-0001")
    QString   mySig;      // my_sig
    QString   mySigInfo;  // my_sig_info

    // -----------------------------------------------------------------------
    // Satellite / meteor scatter
    // -----------------------------------------------------------------------
    QString              satName;     // sat_name
    QString              satMode;     // sat_mode
    std::optional<double> maxBursts;  // max_bursts
    std::optional<int>   nrBursts;    // nr_bursts
    std::optional<int>   nrPings;     // nr_pings
    QString              msShower;    // ms_shower

    // -----------------------------------------------------------------------
    // Free text
    // -----------------------------------------------------------------------
    QString   comment;       // comment
    QString   notes;         // notes
    QString   qsoComplete;   // qso_complete
};

Q_DECLARE_METATYPE(Qso)
