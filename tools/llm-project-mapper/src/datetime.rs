//! Minimal UTC timestamp formatting without external crates.

use std::time::{SystemTime, UNIX_EPOCH};

/// Current time as an ISO-8601 UTC string, e.g. `2026-05-24T22:43:50.508Z`.
pub fn now_iso8601() -> String {
    let dur = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default();
    format_iso8601(dur.as_secs() as i64, dur.subsec_millis())
}

fn format_iso8601(secs: i64, millis: u32) -> String {
    let days = secs.div_euclid(86400);
    let rem = secs.rem_euclid(86400);
    let (h, mi, s) = (rem / 3600, (rem % 3600) / 60, rem % 60);
    let (y, m, d) = civil_from_days(days);
    format!(
        "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z",
        y, m, d, h, mi, s, millis
    )
}

/// Convert days-since-epoch to (year, month, day) — Howard Hinnant's algorithm.
fn civil_from_days(z: i64) -> (i64, u32, u32) {
    let z = z + 719468;
    let era = if z >= 0 { z } else { z - 146096 } / 146097;
    let doe = z - era * 146097; // [0, 146096]
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    let y = yoe + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // [0, 365]
    let mp = (5 * doy + 2) / 153; // [0, 11]
    let d = (doy - (153 * mp + 2) / 5 + 1) as u32; // [1, 31]
    let m = (if mp < 10 { mp + 3 } else { mp - 9 }) as u32; // [1, 12]
    (y + if m <= 2 { 1 } else { 0 }, m, d)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn formats_known_instants() {
        assert_eq!(format_iso8601(0, 0), "1970-01-01T00:00:00.000Z");
        // 946684800 == 2000-01-01T00:00:00Z (well-known Unix constant).
        assert_eq!(format_iso8601(946684800, 0), "2000-01-01T00:00:00.000Z");
        // Leap day: 2024-02-29T12:00:00Z.
        assert_eq!(format_iso8601(1709208000, 7), "2024-02-29T12:00:00.007Z");
    }
}
