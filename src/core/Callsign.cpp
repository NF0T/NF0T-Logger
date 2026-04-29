// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "Callsign.h"

#include <QRegularExpression>

// ---------------------------------------------------------------------------
// Validation regex
// [prefix/] base [/suffix]
// base = 1–3 alphanum  +  digit  +  1–4 letters
// ---------------------------------------------------------------------------
static const QRegularExpression BASE_RE(
    QStringLiteral(R"([A-Z0-9]{1,3}[0-9][A-Z]{1,4})"));

static const QRegularExpression FULL_RE(
    QStringLiteral(
        R"(^(?:[A-Z0-9]{1,3}/)?[A-Z0-9]{1,3}[0-9][A-Z]{1,4}(?:/[A-Z0-9]{1,4})?$)"));

// ---------------------------------------------------------------------------
// DXCC prefix table — longest-prefix match, not exhaustive.
// Sorted longest-first within each region so the algorithm terminates early.
// ---------------------------------------------------------------------------
struct PrefixInfo {
    const char *prefix;
    const char *country;
    const char *continent;
    int dxcc, cq, itu;
};

static constexpr PrefixInfo PREFIX_TABLE[] = {
    // United States — 3-char special areas (must precede 1-char W/K/N entries)
    { "KH6",  "Hawaii",             "OC", 110,  31,  61 },
    { "KH7",  "Hawaii",             "OC", 110,  31,  61 },
    { "KL7",  "Alaska",             "NA",   6,   1,   1 },
    { "KP2",  "US Virgin Islands",  "NA", 285,   8,   8 },
    { "KP4",  "Puerto Rico",        "NA", 202,   8,   8 },
    { "KG4",  "Guantanamo Bay",     "NA", 105,   8,   8 },
    // United States — 2-char AA–AL (A alone is not a US prefix)
    { "AA",   "United States",      "NA", 291,   4,   7 },
    { "AB",   "United States",      "NA", 291,   4,   7 },
    { "AC",   "United States",      "NA", 291,   4,   7 },
    { "AD",   "United States",      "NA", 291,   4,   7 },
    { "AE",   "United States",      "NA", 291,   4,   7 },
    { "AF",   "United States",      "NA", 291,   4,   7 },
    { "AG",   "United States",      "NA", 291,   4,   7 },
    { "AH",   "United States",      "NA", 291,   4,   7 },
    { "AI",   "United States",      "NA", 291,   4,   7 },
    { "AJ",   "United States",      "NA", 291,   4,   7 },
    { "AK",   "United States",      "NA", 291,   4,   7 },
    { "AL",   "United States",      "NA", 291,   4,   7 },
    // United States — 1-char (KA–KZ, WA–WZ, NA–NZ all fall through to these)
    { "W",    "United States",      "NA", 291,   4,   7 },
    { "K",    "United States",      "NA", 291,   4,   7 },
    { "N",    "United States",      "NA", 291,   4,   7 },
    // Canada
    { "VA",   "Canada",             "NA",   1,   3,   4 },
    { "VB",   "Canada",             "NA",   1,   3,   4 },
    { "VC",   "Canada",             "NA",   1,   3,   4 },
    { "VD",   "Canada",             "NA",   1,   3,   4 },
    { "VE",   "Canada",             "NA",   1,   3,   4 },
    { "VF",   "Canada",             "NA",   1,   3,   4 },
    { "VG",   "Canada",             "NA",   1,   3,   4 },
    { "VO",   "Canada",             "NA",   1,   2,   9 },
    { "VY",   "Canada",             "NA",   1,   1,   4 },
    // Mexico
    { "XE",   "Mexico",             "NA",  50,   6,  10 },
    { "XF",   "Mexico",             "NA",  50,   6,  10 },
    { "XH",   "Mexico",             "NA",  50,   6,  10 },
    // United Kingdom — nations
    { "GD",   "Isle of Man",        "EU", 114,  14,  27 },
    { "GI",   "Northern Ireland",   "EU", 265,  14,  27 },
    { "GJ",   "Jersey",             "EU", 122,  14,  27 },
    { "GM",   "Scotland",           "EU", 279,  14,  27 },
    { "GU",   "Guernsey",           "EU", 106,  14,  27 },
    { "GW",   "Wales",              "EU", 294,  14,  27 },
    { "2E",   "England",            "EU", 223,  14,  27 },
    { "G",    "England",            "EU", 223,  14,  27 },
    { "M",    "England",            "EU", 223,  14,  27 },
    // Germany
    { "DA",   "Germany",            "EU", 230,  14,  28 },
    { "DB",   "Germany",            "EU", 230,  14,  28 },
    { "DC",   "Germany",            "EU", 230,  14,  28 },
    { "DD",   "Germany",            "EU", 230,  14,  28 },
    { "DE",   "Germany",            "EU", 230,  14,  28 },
    { "DF",   "Germany",            "EU", 230,  14,  28 },
    { "DG",   "Germany",            "EU", 230,  14,  28 },
    { "DH",   "Germany",            "EU", 230,  14,  28 },
    { "DJ",   "Germany",            "EU", 230,  14,  28 },
    { "DK",   "Germany",            "EU", 230,  14,  28 },
    { "DL",   "Germany",            "EU", 230,  14,  28 },
    { "DM",   "Germany",            "EU", 230,  14,  28 },
    { "DN",   "Germany",            "EU", 230,  14,  28 },
    { "DO",   "Germany",            "EU", 230,  14,  28 },
    { "DP",   "Germany",            "EU", 230,  14,  28 },
    { "DQ",   "Germany",            "EU", 230,  14,  28 },
    { "DR",   "Germany",            "EU", 230,  14,  28 },
    // France
    { "F",    "France",             "EU", 227,  14,  27 },
    // Italy
    { "IH9",  "African Italy",      "AF", 295,  33,  37 },
    { "IG9",  "African Italy",      "AF", 295,  33,  37 },
    { "I",    "Italy",              "EU", 248,  15,  28 },
    // Spain
    { "EA6",  "Balearic Islands",   "EU",  21,  14,  28 },
    { "EA8",  "Canary Islands",     "AF",  29,  33,  36 },
    { "EA9",  "Ceuta & Melilla",    "AF",  32,  33,  37 },
    { "EA",   "Spain",              "EU", 281,  14,  28 },
    { "EB",   "Spain",              "EU", 281,  14,  28 },
    { "EC",   "Spain",              "EU", 281,  14,  28 },
    { "ED",   "Spain",              "EU", 281,  14,  28 },
    { "EE",   "Spain",              "EU", 281,  14,  28 },
    { "EF",   "Spain",              "EU", 281,  14,  28 },
    { "EG",   "Spain",              "EU", 281,  14,  28 },
    { "EH",   "Spain",              "EU", 281,  14,  28 },
    // Netherlands
    { "PA",   "Netherlands",        "EU", 263,  14,  27 },
    { "PB",   "Netherlands",        "EU", 263,  14,  27 },
    { "PC",   "Netherlands",        "EU", 263,  14,  27 },
    { "PD",   "Netherlands",        "EU", 263,  14,  27 },
    { "PE",   "Netherlands",        "EU", 263,  14,  27 },
    { "PF",   "Netherlands",        "EU", 263,  14,  27 },
    { "PG",   "Netherlands",        "EU", 263,  14,  27 },
    { "PH",   "Netherlands",        "EU", 263,  14,  27 },
    { "PI",   "Netherlands",        "EU", 263,  14,  27 },
    // Brazil
    { "PP",   "Brazil",             "SA", 108,  11,  15 },
    { "PQ",   "Brazil",             "SA", 108,  11,  15 },
    { "PR",   "Brazil",             "SA", 108,  11,  15 },
    { "PS",   "Brazil",             "SA", 108,  11,  15 },
    { "PT",   "Brazil",             "SA", 108,  11,  15 },
    { "PU",   "Brazil",             "SA", 108,  11,  15 },
    { "PV",   "Brazil",             "SA", 108,  11,  15 },
    { "PW",   "Brazil",             "SA", 108,  11,  15 },
    { "PX",   "Brazil",             "SA", 108,  11,  15 },
    { "PY",   "Brazil",             "SA", 108,  11,  15 },
    { "PZ",   "Suriname",           "SA", 140,   9,  12 },
    // Belgium
    { "ON",   "Belgium",            "EU", 209,  14,  27 },
    { "OO",   "Belgium",            "EU", 209,  14,  27 },
    { "OP",   "Belgium",            "EU", 209,  14,  27 },
    { "OQ",   "Belgium",            "EU", 209,  14,  27 },
    { "OR",   "Belgium",            "EU", 209,  14,  27 },
    { "OS",   "Belgium",            "EU", 209,  14,  27 },
    { "OT",   "Belgium",            "EU", 209,  14,  27 },
    // Switzerland
    { "HB",   "Switzerland",        "EU", 287,  14,  28 },
    // Austria
    { "OE",   "Austria",            "EU", 206,  15,  28 },
    // Sweden
    { "SA",   "Sweden",             "EU", 284,  18,  18 },
    { "SB",   "Sweden",             "EU", 284,  18,  18 },
    { "SC",   "Sweden",             "EU", 284,  18,  18 },
    { "SD",   "Sweden",             "EU", 284,  18,  18 },
    { "SE",   "Sweden",             "EU", 284,  18,  18 },
    { "SF",   "Sweden",             "EU", 284,  18,  18 },
    { "SG",   "Sweden",             "EU", 284,  18,  18 },
    { "SH",   "Sweden",             "EU", 284,  18,  18 },
    { "SI",   "Sweden",             "EU", 284,  18,  18 },
    { "SJ",   "Sweden",             "EU", 284,  18,  18 },
    { "SK",   "Sweden",             "EU", 284,  18,  18 },
    { "SL",   "Sweden",             "EU", 284,  18,  18 },
    { "SM",   "Sweden",             "EU", 284,  18,  18 },
    // Finland
    { "OF",   "Finland",            "EU", 224,  18,  18 },
    { "OG",   "Finland",            "EU", 224,  18,  18 },
    { "OH",   "Finland",            "EU", 224,  18,  18 },
    { "OI",   "Finland",            "EU", 224,  18,  18 },
    // Norway
    { "LA",   "Norway",             "EU", 266,  18,  18 },
    { "LB",   "Norway",             "EU", 266,  18,  18 },
    { "LC",   "Norway",             "EU", 266,  18,  18 },
    { "LD",   "Norway",             "EU", 266,  18,  18 },
    { "LE",   "Norway",             "EU", 266,  18,  18 },
    { "LF",   "Norway",             "EU", 266,  18,  18 },
    { "LG",   "Norway",             "EU", 266,  18,  18 },
    // Denmark
    { "OZ",   "Denmark",            "EU", 221,  18,  18 },
    // Poland
    { "SP",   "Poland",             "EU", 269,  15,  28 },
    { "SQ",   "Poland",             "EU", 269,  15,  28 },
    { "SR",   "Poland",             "EU", 269,  15,  28 },
    // Czech Republic
    { "OK",   "Czech Republic",     "EU", 212,  15,  28 },
    { "OL",   "Czech Republic",     "EU", 212,  15,  28 },
    // Hungary
    { "HA",   "Hungary",            "EU", 239,  15,  28 },
    { "HG",   "Hungary",            "EU", 239,  15,  28 },
    // Romania
    { "YO",   "Romania",            "EU", 275,  20,  28 },
    { "YP",   "Romania",            "EU", 275,  20,  28 },
    { "YQ",   "Romania",            "EU", 275,  20,  28 },
    { "YR",   "Romania",            "EU", 275,  20,  28 },
    // Greece
    { "SV",   "Greece",             "EU", 236,  20,  28 },
    { "SW",   "Greece",             "EU", 236,  20,  28 },
    { "SX",   "Greece",             "EU", 236,  20,  28 },
    { "SY",   "Greece",             "EU", 236,  20,  28 },
    { "SZ",   "Greece",             "EU", 236,  20,  28 },
    // Portugal
    { "CR",   "Portugal",           "EU", 272,  14,  37 },
    { "CS",   "Portugal",           "EU", 272,  14,  37 },
    { "CT",   "Portugal",           "EU", 272,  14,  37 },
    // Croatia
    { "9A",   "Croatia",            "EU", 497,  15,  28 },
    // Serbia
    { "YT",   "Serbia",             "EU", 296,  15,  28 },
    { "YU",   "Serbia",             "EU", 296,  15,  28 },
    // Ukraine
    { "UR",   "Ukraine",            "EU", 288,  16,  29 },
    { "US",   "Ukraine",            "EU", 288,  16,  29 },
    { "UT",   "Ukraine",            "EU", 288,  16,  29 },
    { "UU",   "Ukraine",            "EU", 288,  16,  29 },
    { "UV",   "Ukraine",            "EU", 288,  16,  29 },
    { "UW",   "Ukraine",            "EU", 288,  16,  29 },
    { "UX",   "Ukraine",            "EU", 288,  16,  29 },
    { "UY",   "Ukraine",            "EU", 288,  16,  29 },
    { "UZ",   "Ukraine",            "EU", 288,  16,  29 },
    // Russia (UA/RA; Asian Russia shares prefix space — simplified to EU Russia)
    { "RA",   "Russia",             "EU",  15,  16,  29 },
    { "RC",   "Russia",             "EU",  15,  16,  29 },
    { "RD",   "Russia",             "EU",  15,  16,  29 },
    { "RE",   "Russia",             "EU",  15,  16,  29 },
    { "RF",   "Russia",             "EU",  15,  16,  29 },
    { "RG",   "Russia",             "EU",  15,  16,  29 },
    { "RJ",   "Russia",             "EU",  15,  16,  29 },
    { "RK",   "Russia",             "EU",  15,  16,  29 },
    { "RL",   "Russia",             "EU",  15,  16,  29 },
    { "RM",   "Russia",             "EU",  15,  16,  29 },
    { "RN",   "Russia",             "EU",  15,  16,  29 },
    { "RO",   "Russia",             "EU",  15,  16,  29 },
    { "RQ",   "Russia",             "EU",  15,  16,  29 },
    { "RT",   "Russia",             "EU",  15,  16,  29 },
    { "RU",   "Russia",             "EU",  15,  16,  29 },
    { "RV",   "Russia",             "EU",  15,  16,  29 },
    { "RW",   "Russia",             "EU",  15,  16,  29 },
    { "RX",   "Russia",             "EU",  15,  16,  29 },
    { "RY",   "Russia",             "EU",  15,  16,  29 },
    { "RZ",   "Russia",             "EU",  15,  16,  29 },
    { "UA",   "Russia",             "EU",  15,  16,  29 },
    { "UB",   "Russia",             "EU",  15,  16,  29 },
    { "UC",   "Russia",             "EU",  15,  16,  29 },
    { "UD",   "Russia",             "EU",  15,  16,  29 },
    { "UE",   "Russia",             "EU",  15,  16,  29 },
    { "UF",   "Russia",             "EU",  15,  16,  29 },
    { "UG",   "Russia",             "EU",  15,  16,  29 },
    { "UH",   "Russia",             "EU",  15,  16,  29 },
    { "UI",   "Russia",             "EU",  15,  16,  29 },
    // Japan
    { "JA",   "Japan",              "AS", 339,  25,  45 },
    { "JB",   "Japan",              "AS", 339,  25,  45 },
    { "JC",   "Japan",              "AS", 339,  25,  45 },
    { "JD",   "Japan",              "AS", 339,  25,  45 },
    { "JE",   "Japan",              "AS", 339,  25,  45 },
    { "JF",   "Japan",              "AS", 339,  25,  45 },
    { "JG",   "Japan",              "AS", 339,  25,  45 },
    { "JH",   "Japan",              "AS", 339,  25,  45 },
    { "JI",   "Japan",              "AS", 339,  25,  45 },
    { "JJ",   "Japan",              "AS", 339,  25,  45 },
    { "JK",   "Japan",              "AS", 339,  25,  45 },
    { "JL",   "Japan",              "AS", 339,  25,  45 },
    { "JM",   "Japan",              "AS", 339,  25,  45 },
    { "JN",   "Japan",              "AS", 339,  25,  45 },
    { "JO",   "Japan",              "AS", 339,  25,  45 },
    { "JP",   "Japan",              "AS", 339,  25,  45 },
    { "JQ",   "Japan",              "AS", 339,  25,  45 },
    { "JR",   "Japan",              "AS", 339,  25,  45 },
    { "JS",   "Japan",              "AS", 339,  25,  45 },
    { "7J",   "Japan",              "AS", 339,  25,  45 },
    { "7K",   "Japan",              "AS", 339,  25,  45 },
    { "7L",   "Japan",              "AS", 339,  25,  45 },
    { "7M",   "Japan",              "AS", 339,  25,  45 },
    { "7N",   "Japan",              "AS", 339,  25,  45 },
    // China / Taiwan
    { "BV",   "Taiwan",             "AS", 386,  24,  44 },
    { "BW",   "Taiwan",             "AS", 386,  24,  44 },
    { "BA",   "China",              "AS", 318,  24,  44 },
    { "BD",   "China",              "AS", 318,  24,  44 },
    { "BG",   "China",              "AS", 318,  24,  44 },
    { "BH",   "China",              "AS", 318,  24,  44 },
    { "BI",   "China",              "AS", 318,  24,  44 },
    { "BJ",   "China",              "AS", 318,  24,  44 },
    { "BL",   "China",              "AS", 318,  24,  44 },
    // Australia
    { "VK",   "Australia",          "OC", 150,  29,  55 },
    // New Zealand
    { "ZL",   "New Zealand",        "OC", 170,  32,  60 },
    // South Africa
    { "ZR",   "South Africa",       "AF", 173,  38,  57 },
    { "ZS",   "South Africa",       "AF", 173,  38,  57 },
    { "ZT",   "South Africa",       "AF", 173,  38,  57 },
    { "ZU",   "South Africa",       "AF", 173,  38,  57 },
    // Argentina
    { "LU",   "Argentina",          "SA", 100,  13,  14 },
    { "LV",   "Argentina",          "SA", 100,  13,  14 },
    { "LW",   "Argentina",          "SA", 100,  13,  14 },
    // India
    { "VT",   "India",              "AS", 324,  26,  41 },
    { "VU",   "India",              "AS", 324,  26,  41 },
    // South Korea
    { "DS",   "South Korea",        "AS", 137,  25,  44 },
    { "DT",   "South Korea",        "AS", 137,  25,  44 },
    { "HL",   "South Korea",        "AS", 137,  25,  44 },
    { "6K",   "South Korea",        "AS", 137,  25,  44 },
    { "6L",   "South Korea",        "AS", 137,  25,  44 },
    { "6M",   "South Korea",        "AS", 137,  25,  44 },
    { "6N",   "South Korea",        "AS", 137,  25,  44 },
    // Israel
    { "4X",   "Israel",             "AS", 336,  20,  39 },
    { "4Z",   "Israel",             "AS", 336,  20,  39 },
    // Cyprus
    { "5B",   "Cyprus",             "AS", 215,  20,  39 },
    // Bermuda
    { "VP9",  "Bermuda",            "NA",  64,   5,  11 },
};

// ---------------------------------------------------------------------------
// Callsign implementation
// ---------------------------------------------------------------------------

Callsign::Callsign(const QString &raw)
{
    parse(raw.toUpper().trimmed());
}

bool Callsign::isValid(const QString &raw)
{
    return FULL_RE.match(raw.toUpper().trimmed()).hasMatch();
}

bool Callsign::looksLikeBase(const QString &s)
{
    return BASE_RE.match(s).captured() == s;
}

// Extract everything up to and including the first digit in the base.
// "W1AW"  → "W1"   "KH6XY" → "KH6"   "DL1ABC" → "DL1"
QString Callsign::extractCallPrefix(const QString &base)
{
    for (int i = 0; i < base.size(); ++i) {
        if (base[i].isDigit())
            return base.left(i + 1);
    }
    return {};
}

void Callsign::parse(const QString &raw)
{
    if (!FULL_RE.match(raw).hasMatch())
        return;

    m_full  = raw;
    m_valid = true;

    const QStringList parts = raw.split(QLatin1Char('/'));

    QString outerPrefix;

    if (parts.size() == 3) {
        outerPrefix = parts[0];
        m_base      = parts[1];
        m_suffix    = parts[2];
    } else if (parts.size() == 2) {
        if (looksLikeBase(parts[1])) {
            outerPrefix = parts[0];
            m_base      = parts[1];
        } else {
            m_base   = parts[0];
            m_suffix = parts[1];
        }
    } else {
        m_base = parts[0];
    }

    // DXCC lookup: use outer prefix when present, otherwise extract from base
    const QString lookupSrc = outerPrefix.isEmpty()
                              ? extractCallPrefix(m_base)
                              : outerPrefix;
    resolvePrefix(lookupSrc);
}

void Callsign::resolvePrefix(const QString &lookupSrc)
{
    // Try the full prefix, then progressively shorter substrings (longest match)
    for (int len = lookupSrc.size(); len > 0; --len) {
        const QString candidate = lookupSrc.left(len);
        for (const PrefixInfo &info : PREFIX_TABLE) {
            if (QLatin1String(info.prefix) == candidate) {
                m_country   = QString::fromLatin1(info.country);
                m_continent = QString::fromLatin1(info.continent);
                m_dxcc      = info.dxcc;
                m_cqZone    = info.cq;
                m_ituZone   = info.itu;
                return;
            }
        }
    }
}
