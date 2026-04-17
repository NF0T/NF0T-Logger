#pragma once

#include <optional>
#include <QString>

/// Maidenhead grid square utilities.
///
/// Supports 2-char (field), 4-char (field+square), and 6-char
/// (field+square+subsquare) locators.  All functions accept mixed-case input;
/// output uses the standard convention: uppercase field/square, lowercase
/// subsquare (e.g. "EM29jx").
///
/// Coordinate convention: WGS-84 decimal degrees.
///   Latitude  -90 … +90   (south negative)
///   Longitude -180 … +180 (west negative)
namespace Maidenhead {

/// Returns true if grid is a syntactically valid 2-, 4-, or 6-character
/// Maidenhead locator.
bool isValid(const QString &grid);

/// Converts a Maidenhead locator to the lat/lon of its center point.
/// Returns false and leaves lat/lon unchanged if grid is invalid.
bool toLatLon(const QString &grid, double &lat, double &lon);

/// Converts a lat/lon coordinate to a Maidenhead locator.
/// precision must be 4 or 6 (default 6).
/// Returns an empty string if lat/lon is out of range.
QString fromLatLon(double lat, double lon, int precision = 6);

/// Great-circle distance between two lat/lon points using the Haversine
/// formula.  Returns kilometres.
double distanceKm(double lat1, double lon1, double lat2, double lon2);

/// Great-circle distance between two grid squares (center points).
/// Returns nullopt if either grid is invalid.
std::optional<double> distanceKm(const QString &grid1, const QString &grid2);

/// Great-circle distance between a grid square center and a lat/lon point.
/// Returns nullopt if grid is invalid.
std::optional<double> distanceKm(const QString &grid, double lat, double lon);

} // namespace Maidenhead
