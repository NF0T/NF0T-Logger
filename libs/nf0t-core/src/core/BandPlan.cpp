// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "BandPlan.h"

namespace BandPlan {

// ---------------------------------------------------------------------------
// Band table — ADIF v3 names, ITU band edges, typical activity frequencies
// ---------------------------------------------------------------------------
static const BandEntry s_bands[] = {
    {"2190m",  0.1357,    0.1378,    0.13775 },
    {"630m",   0.472,     0.479,     0.475   },
    {"560m",   0.501,     0.504,     0.502   },
    {"160m",   1.800,     2.000,     1.840   },
    {"80m",    3.500,     4.000,     3.750   },
    {"60m",    5.0605,    5.4535,    5.357   },
    {"40m",    7.000,     7.300,     7.200   },
    {"30m",   10.100,    10.150,    10.136   },
    {"20m",   14.000,    14.350,    14.225   },
    {"17m",   18.068,    18.168,    18.130   },
    {"15m",   21.000,    21.450,    21.300   },
    {"12m",   24.890,    24.990,    24.940   },
    {"10m",   28.000,    29.700,    28.400   },
    {"8m",    40.000,    45.000,    40.680   },
    {"6m",    50.000,    54.000,    50.125   },
    {"5m",    54.000,    69.900,    54.000   },
    {"4m",    70.000,    71.000,    70.200   },
    {"2m",   144.000,   148.000,   144.200   },
    {"1.25m",222.000,   225.000,   222.100   },
    {"70cm", 420.000,   450.000,   432.100   },
    {"33cm", 902.000,   928.000,   903.100   },
    {"23cm",1240.000,  1300.000,  1296.100   },
    {"13cm",2300.000,  2450.000,  2304.100   },
    {"9cm", 3300.000,  3500.000,  3456.100   },
    {"6cm", 5650.000,  5925.000,  5760.100   },
    {"3cm",10000.000, 10500.000, 10368.100   },
    {"1.25cm",24000.0, 24050.0,  24048.100   },
    {"6mm", 47000.000, 47200.000, 47045.0    },
    {"4mm", 75500.000, 81000.000, 76032.0    },
    {"2.5mm",119980.0, 120020.0,  120000.0   },
    {"2mm", 142000.000,149000.000, 144000.0  },
    {"1mm", 241000.000,250000.000, 241000.0  },
    {"submm",300000.0,  7500000.0, 300000.0  },
};
static const int s_bandCount = sizeof(s_bands) / sizeof(s_bands[0]);

// ---------------------------------------------------------------------------
// Mode table
// ---------------------------------------------------------------------------
static const ModeEntry s_modes[] = {
    // Voice / traditional
    {"SSB",          "59",  {"USB", "LSB"}},
    {"CW",           "599", {}},
    {"AM",           "59",  {}},
    {"FM",           "59",  {"DV"}},
    {"SSTV",         "59",  {}},
    {"ATV",          "59",  {}},
    {"DIGITALVOICE", "59",  {"C4FM","DMR","DSTAR","FREEDV","M17","P25","YSF"}},

    // Weak-signal digital (WSJT-X family) — standalone per ADIF spec
    {"FT8",          "+0",  {}},
    // FT4, JS8, FST4, FST4W, Q65 are ADIF submodes of MFSK, but are listed
    // here as top-level entries so users can select them directly in the UI.
    // normaliseMode() in QslDownloadDialog maps them to MFSK/submode for matching.
    {"FT4",          "+0",  {}},
    {"FST4",         "+0",  {}},
    {"FST4W",        "+0",  {}},
    {"JS8",          "+0",  {}},
    {"Q65",          "+0",  {}},
    {"MSK144",       "+0",  {}},
    {"JT65",         "+0",  {"JT65A","JT65B","JT65C"}},
    {"JT9",          "+0",  {"JT9-1","JT9-2","JT9-5","JT9-10","JT9-30"}},
    {"WSPR",         "-10", {}},

    // Data / keyboard modes
    {"RTTY",         "599", {"ASCI"}},
    {"PSK",          "599", {"BPSK31","BPSK63","BPSK125","BPSK250","BPSK500",
                              "BPSK1000","QPSK31","QPSK63","QPSK125","QPSK250",
                              "QPSK500","PSK10","PSK31","PSK63","PSK63F","PSK125",
                              "PSK250","PSK500","PSK1000","PSKAM10","PSKAM31",
                              "PSKAM50","PSK2K"}},
    {"OLIVIA",       "599", {"OLIVIA 4/125","OLIVIA 4/250","OLIVIA 8/250",
                              "OLIVIA 8/500","OLIVIA 16/500","OLIVIA 16/1000",
                              "OLIVIA 32/1000"}},
    {"MFSK",         "599", {"FT4","FST4","FST4W","FSQCALL","JS8","JTMS",
                              "MFSK4","MFSK8","MFSK11","MFSK16","MFSK22",
                              "MFSK31","MFSK32","MFSK64","MFSK64L",
                              "MFSK128","MFSK128L","Q65"}},
    {"CONTESTI",     "599", {}},
    {"DOMINO",       "599", {"DOM-M","DOM4","DOM5","DOM8","DOM11","DOM16",
                              "DOM22","DOM44","DOM88","DOMINOEX","DOMINOF"}},
    {"HELL",         "599", {"FMHELL","FSKHELL","HELL80","HELLX5","HELLX9",
                              "HFSK","PSKHELL","SLOWHELL"}},
    {"MT63",         "599", {}},
    {"THOR",         "599", {"THOR-M","THOR4","THOR5","THOR8","THOR11",
                              "THOR16","THOR22","THOR25X4","THOR50X1",
                              "THOR50X2","THOR100"}},
    {"THRB",         "599", {"THRBX","THRBX1","THRBX2","THRBX4",
                              "THROB1","THROB2","THROB4"}},

    // Other
    {"PKT",          "599", {}},
};
static const int s_modeCount = sizeof(s_modes) / sizeof(s_modes[0]);

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

const BandEntry *allEntries()  { return s_bands; }
int              entryCount()  { return s_bandCount; }

const ModeEntry *modeEntries() { return s_modes; }
int              modeEntryCount() { return s_modeCount; }

QString freqToBand(double freqMhz)
{
    for (int i = 0; i < s_bandCount; ++i) {
        if (freqMhz >= s_bands[i].freqLow && freqMhz <= s_bands[i].freqHigh)
            return QString::fromLatin1(s_bands[i].name);
    }
    return {};
}

double bandDefaultFreq(const QString &band)
{
    const QByteArray ba = band.toLatin1();
    for (int i = 0; i < s_bandCount; ++i) {
        if (ba == s_bands[i].name)
            return s_bands[i].defaultFreq;
    }
    return 0.0;
}

QStringList bandNames()
{
    QStringList list;
    list.reserve(s_bandCount);
    for (int i = 0; i < s_bandCount; ++i)
        list << QString::fromLatin1(s_bands[i].name);
    return list;
}

QStringList modeNames()
{
    QStringList list;
    list.reserve(s_modeCount);
    for (int i = 0; i < s_modeCount; ++i)
        list << QString::fromLatin1(s_modes[i].mode);
    return list;
}

QString modeDefaultRst(const QString &mode)
{
    const QByteArray ba = mode.toLatin1();
    for (int i = 0; i < s_modeCount; ++i) {
        if (ba == s_modes[i].mode)
            return QString::fromLatin1(s_modes[i].defaultRst);
    }
    return QStringLiteral("59");
}

QStringList submodesForMode(const QString &mode)
{
    const QByteArray ba = mode.toLatin1();
    for (int i = 0; i < s_modeCount; ++i) {
        if (ba == s_modes[i].mode)
            return s_modes[i].submodes;
    }
    return {};
}

} // namespace BandPlan
