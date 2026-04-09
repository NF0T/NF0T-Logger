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
    {"SSB",          "59",  {"USB", "LSB"}},
    {"CW",           "599", {}},
    {"AM",           "59",  {}},
    {"FM",           "59",  {"FM", "DV"}},
    {"FT8",          "+0",  {}},
    {"FT4",          "+0",  {}},
    {"JS8",          "+0",  {}},
    {"MSK144",       "+0",  {}},
    {"JT65",         "+0",  {}},
    {"JT9",          "+0",  {}},
    {"WSPR",         "-10", {}},
    {"RTTY",         "599", {"RTTY", "AFSK"}},
    {"PSK31",        "599", {"BPSK31","QPSK31","PSK31R"}},
    {"PSK63",        "599", {"BPSK63","QPSK63"}},
    {"OLIVIA",       "599", {}},
    {"MFSK",         "599", {"MFSK8","MFSK16","MFSK32","MFSK64"}},
    {"THOR",         "599", {}},
    {"DOMINO",       "599", {}},
    {"MT63",         "599", {}},
    {"THROB",        "599", {}},
    {"SSTV",         "59",  {}},
    {"DIGITALVOICE", "59",  {"C4FM","DMR","DSTAR","P25","YSF"}},
    {"PACKET",       "599", {}},
    {"ATV",          "59",  {}},
    {"HELL",         "599", {}},
    {"CONTESTIA",    "599", {}},
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
