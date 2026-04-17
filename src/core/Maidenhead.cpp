#include "Maidenhead.h"

#include <cmath>

namespace Maidenhead {

// ---------------------------------------------------------------------------
// Internal constants
// ---------------------------------------------------------------------------

static constexpr double kEarthRadiusKm = 6371.0;
static constexpr double kPi            = M_PI;

// Subsquare cell sizes in degrees
static constexpr double kSubLon = 5.0  / 60.0;   // 5 arcmin
static constexpr double kSubLat = 2.5  / 60.0;   // 2.5 arcmin

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

bool isValid(const QString &grid)
{
    const int len = grid.size();
    if (len != 2 && len != 4 && len != 6)
        return false;

    const QChar f0 = grid[0].toUpper();
    const QChar f1 = grid[1].toUpper();
    if (f0 < 'A' || f0 > 'R') return false;
    if (f1 < 'A' || f1 > 'R') return false;

    if (len >= 4) {
        if (!grid[2].isDigit()) return false;
        if (!grid[3].isDigit()) return false;
    }

    if (len == 6) {
        const QChar s0 = grid[4].toUpper();
        const QChar s1 = grid[5].toUpper();
        if (s0 < 'A' || s0 > 'X') return false;
        if (s1 < 'A' || s1 > 'X') return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Grid → lat/lon
// ---------------------------------------------------------------------------

bool toLatLon(const QString &grid, double &lat, double &lon)
{
    if (!isValid(grid))
        return false;

    const int len = grid.size();

    // Field (covers 20° lon × 10° lat)
    double lo = (grid[0].toUpper().unicode() - 'A') * 20.0 - 180.0;
    double la = (grid[1].toUpper().unicode() - 'A') * 10.0 -  90.0;

    if (len >= 4) {
        lo += (grid[2].unicode() - '0') * 2.0;
        la += (grid[3].unicode() - '0') * 1.0;
    }

    if (len == 6) {
        lo += (grid[4].toUpper().unicode() - 'A') * kSubLon;
        la += (grid[5].toUpper().unicode() - 'A') * kSubLat;
    }

    // Add half a cell to return the center of the locator, not its SW corner
    if (len == 2) {
        lo += 10.0;
        la +=  5.0;
    } else if (len == 4) {
        lo += 1.0;
        la += 0.5;
    } else {
        lo += kSubLon / 2.0;
        la += kSubLat / 2.0;
    }

    lon = lo;
    lat = la;
    return true;
}

// ---------------------------------------------------------------------------
// Lat/lon → grid
// ---------------------------------------------------------------------------

QString fromLatLon(double lat, double lon, int precision)
{
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0)
        return {};
    if (precision != 4 && precision != 6)
        precision = 6;

    // Normalize to positive offsets (SW corner of field AA)
    const double lo = lon + 180.0;   // 0 … 360
    const double la = lat +  90.0;   // 0 … 180

    // Field
    const int flo = qMin(static_cast<int>(lo / 20.0), 17);
    const int fla = qMin(static_cast<int>(la / 10.0), 17);

    // Square
    const double rlo = lo - flo * 20.0;
    const double rla = la - fla * 10.0;
    const int slo = qMin(static_cast<int>(rlo / 2.0), 9);
    const int sla = qMin(static_cast<int>(rla / 1.0), 9);

    QString result;
    result.reserve(precision);
    result += QChar('A' + flo);
    result += QChar('A' + fla);
    result += QChar('0' + slo);
    result += QChar('0' + sla);

    if (precision == 6) {
        const double rlo2 = rlo - slo * 2.0;
        const double rla2 = rla - sla * 1.0;
        const int sslo = qMin(static_cast<int>(rlo2 / kSubLon), 23);
        const int ssla = qMin(static_cast<int>(rla2 / kSubLat), 23);
        result += QChar('a' + sslo);
        result += QChar('a' + ssla);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Distance (Haversine)
// ---------------------------------------------------------------------------

double distanceKm(double lat1, double lon1, double lat2, double lon2)
{
    const double phi1 = lat1 * kPi / 180.0;
    const double phi2 = lat2 * kPi / 180.0;
    const double dphi = (lat2 - lat1) * kPi / 180.0;
    const double dlam = (lon2 - lon1) * kPi / 180.0;

    const double a = std::sin(dphi / 2.0) * std::sin(dphi / 2.0)
                   + std::cos(phi1) * std::cos(phi2)
                   * std::sin(dlam / 2.0) * std::sin(dlam / 2.0);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return kEarthRadiusKm * c;
}

std::optional<double> distanceKm(const QString &grid1, const QString &grid2)
{
    double lat1, lon1, lat2, lon2;
    if (!toLatLon(grid1, lat1, lon1)) return std::nullopt;
    if (!toLatLon(grid2, lat2, lon2)) return std::nullopt;
    return distanceKm(lat1, lon1, lat2, lon2);
}

std::optional<double> distanceKm(const QString &grid, double lat, double lon)
{
    double glat, glon;
    if (!toLatLon(grid, glat, glon)) return std::nullopt;
    return distanceKm(glat, glon, lat, lon);
}

} // namespace Maidenhead
