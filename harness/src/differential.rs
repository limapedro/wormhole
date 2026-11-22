//! Compare two protocol captures packet by packet.
//!
//! The eventual differential tester will drive live senders; this first stage
//! compares captured streams by message type, timestamp and payload hash so a
//! mismatch can be diagnosed without relying on visual inspection.

use crate::config::Config;
use serde::Serialize;
use std::fs;
use std::io::Cursor;
use std::path::Path;
use wh_protocol::Packet;

/// Report written for one comparison.
#[derive(Debug, Serialize)]
struct Report {
    left: String,
    right: String,
    left_packets: usize,
    right_packets: usize,
    identical: bool,
    differences: Vec<String>,
}

/// Decode and compare both captures. Returns an error when they differ so the
/// command can be used as a CI gate.
pub fn run(cfg: &Config) -> Result<(), Box<dyn std::error::Error>> {
    let left_packets = decode(&cfg.differential.left, cfg.dissect.max_payload_bytes)?;
    let right_packets = decode(&cfg.differential.right, cfg.dissect.max_payload_bytes)?;

    let mut differences = Vec::new();
    if left_packets.len() != right_packets.len() {
        differences.push(format!(
            "packet count differs: {} vs {}",
            left_packets.len(),
            right_packets.len()
        ));
    }
    for (index, (left, right)) in left_packets.iter().zip(right_packets.iter()).enumerate() {
        let lh = left.header();
        let rh = right.header();
        if lh.kind != rh.kind {
            differences.push(format!(
                "packet {index}: kind {:?} vs {:?}",
                lh.kind, rh.kind
            ));
        }
        if lh.timestamp != rh.timestamp {
            differences.push(format!(
                "packet {index}: timestamp {} vs {}",
                lh.timestamp, rh.timestamp
            ));
        }
        let lp = payload(left);
        let rp = payload(right);
        if fnv1a(lp) != fnv1a(rp) {
            differences.push(format!(
                "packet {index}: payload hash {:08x} vs {:08x} ({} vs {} bytes)",
                fnv1a(lp),
                fnv1a(rp),
                lp.len(),
                rp.len()
            ));
        }
    }

    let identical = differences.is_empty();
    let report = Report {
        left: cfg.differential.left.to_string_lossy().into_owned(),
        right: cfg.differential.right.to_string_lossy().into_owned(),
        left_packets: left_packets.len(),
        right_packets: right_packets.len(),
        identical,
        differences,
    };
    write_report(&cfg.differential.output, &report)?;
    println!(
        "differential: {} vs {} packets, identical={}, {} difference(s)",
        report.left_packets,
        report.right_packets,
        report.identical,
        report.differences.len()
    );
    println!("wrote {}", cfg.differential.output.display());

    if report.identical {
        Ok(())
    } else {
        Err("captures differ".into())
    }
}

fn decode(path: &Path, max_payload: usize) -> Result<Vec<Packet>, Box<dyn std::error::Error>> {
    let bytes = fs::read(path)?;
    let mut cursor = Cursor::new(&bytes);
    let mut packets = Vec::new();
    while cursor.position() < bytes.len() as u64 {
        // Packet::decode consumes exactly header + type header + size bytes,
        // which is the packet's wire length; no manual seek is needed.
        let packet = Packet::decode(&mut cursor, max_payload)?;
        packets.push(packet);
    }
    Ok(packets)
}

fn payload(packet: &Packet) -> &[u8] {
    match packet {
        Packet::Jpeg { payload, .. }
        | Packet::Wave { payload, .. }
        | Packet::Unknown { payload, .. } => payload,
    }
}

fn fnv1a(bytes: &[u8]) -> u32 {
    let mut hash = 0x811C_9DC5_u32;
    for byte in bytes {
        hash ^= u32::from(*byte);
        hash = hash.wrapping_mul(0x0100_0193);
    }
    hash
}

fn write_report(path: &Path, report: &Report) -> Result<(), Box<dyn std::error::Error>> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(path, serde_json::to_string_pretty(report)?)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use wh_protocol::{Header, ImageHeader, MessageKind, Packet};

    fn fixture_packet(timestamp: u64) -> Packet {
        Packet::Jpeg {
            header: Header {
                kind: MessageKind::Jpeg,
                timestamp,
                size: 2,
            },
            image: ImageHeader::default(),
            payload: vec![1, 2],
        }
    }

    #[test]
    fn fnv1a_is_stable_and_order_sensitive() {
        assert_eq!(fnv1a(b"abc"), fnv1a(b"abc"));
        assert_ne!(fnv1a(b"abc"), fnv1a(b"cba"));
    }

    #[test]
    fn timestamps_and_payloads_are_compared() {
        let a = fixture_packet(10);
        let b = fixture_packet(11);
        assert_ne!(a.header().timestamp, b.header().timestamp);
        assert_ne!(
            fnv1a(payload(&a)),
            fnv1a(payload(&fixture_packet_with_payload(3)))
        );
    }

    fn fixture_packet_with_payload(value: u8) -> Packet {
        Packet::Jpeg {
            header: Header {
                kind: MessageKind::Jpeg,
                timestamp: 10,
                size: 2,
            },
            image: ImageHeader::default(),
            payload: vec![value, 2],
        }
    }
}
