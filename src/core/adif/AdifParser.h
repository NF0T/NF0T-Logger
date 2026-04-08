#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "core/logbook/Qso.h"

/// Parses ADIF v3 ADI files into Qso records.
/// Uses the spec-correct length-based tokeniser — not delimiter splitting —
/// so values containing angle brackets are handled correctly.
class AdifParser
{
public:
    struct Result {
        QList<Qso>   qsos;
        QStringList  warnings;     // non-fatal field/record issues
        int          skipped = 0;  // records dropped due to missing required fields
    };

    static Result parseFile  (const QString &path);
    static Result parseString(const QString &adif);

private:
    using FieldMap = QMap<QString, QString>;   // UPPER_NAME → value (private type, used in .cpp)

    struct Token { QString name; QString value; bool isEoh; bool isEor; };

    static QList<Token> tokenise(const QString &text);
    static Qso          toQso   (const FieldMap &fields, QStringList &warnings);

    // Field conversion helpers
    static QDateTime    parseDateTime (const QString &date, const QString &time);
    static QDate        parseAdifDate (const QString &s);
    static QChar        parseQslStatus(const QString &s);
    static std::optional<double> parseLatLon(const QString &adifCoord);
    static QString      formatLatLon (double degrees, bool isLat);  // used by writer via friend
};
