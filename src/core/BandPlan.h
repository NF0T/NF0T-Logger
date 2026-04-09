#pragma once

#include <optional>
#include <QString>
#include <QStringList>

/// Amateur band plan utilities.
/// Band names follow ADIF v3 conventions.
namespace BandPlan {

struct BandEntry {
    const char *name;
    double      freqLow;      // MHz
    double      freqHigh;     // MHz
    double      defaultFreq;  // MHz — typical calling / activity freq
};

/// All amateur bands in ascending frequency order.
const BandEntry *allEntries();
int              entryCount();

/// Returns the ADIF band name for a given frequency, or empty if outside all bands.
QString freqToBand(double freqMhz);

/// Returns default frequency for a band (activity / calling frequency).
double bandDefaultFreq(const QString &band);

/// Ordered list of band names for UI dropdowns (LF → microwave).
QStringList bandNames();

// -----------------------------------------------------------------------
// Mode helpers
// -----------------------------------------------------------------------

struct ModeEntry {
    const char *mode;
    const char *defaultRst;
    QStringList submodes;    // empty = no submode selector
};

const ModeEntry *modeEntries();
int              modeEntryCount();

/// Ordered list of modes for the mode dropdown.
QStringList modeNames();

/// Default RST for a mode ("59" for voice, "599" for CW/data).
QString modeDefaultRst(const QString &mode);

/// Valid submodes for a mode (empty if none).
QStringList submodesForMode(const QString &mode);

} // namespace BandPlan
