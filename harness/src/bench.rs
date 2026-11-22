//! Dissector throughput self-benchmark.
//!
//! This measures the harness's own encode/decode path on synthetic packets.
//! It is **not** a Teleport benchmark; performance targets for Wormhole are
//! measured with the differential tester and written into `exp_NNN/` by the
//! benchmark runner described in `HARNESS.md`.

use crate::config::Config;
use std::fs;
use std::io::Cursor;
use std::time::Instant;
use wh_protocol::{Header, ImageHeader, MessageKind, Packet};

/// Run the self-benchmark and write a CSV result file.
pub fn run(cfg: &Config) -> Result<(), Box<dyn std::error::Error>> {
    let count = cfg.bench.packets as usize;
    if count == 0 {
        return Err("bench.packets must be greater than zero".into());
    }
    let payload_size = cfg.bench.payload_bytes;

    let mut stream = Vec::with_capacity(count * (16 + 88 + payload_size));
    for index in 0..count {
        Packet::Jpeg {
            header: Header {
                kind: MessageKind::Jpeg,
                timestamp: index as u64,
                size: payload_size as i32,
            },
            image: ImageHeader::default(),
            payload: vec![(index % 251) as u8; payload_size],
        }
        .encode(&mut stream)?;
    }
    let total_bytes = stream.len();

    let start = Instant::now();
    let mut packets = Vec::with_capacity(count);
    let mut cursor = Cursor::new(&stream);
    while cursor.position() < total_bytes as u64 {
        let packet = Packet::decode(&mut cursor, cfg.dissect.max_payload_bytes)?;
        packets.push(packet);
    }
    let decode_seconds = start.elapsed().as_secs_f64();

    let mut round_trip = Vec::with_capacity(total_bytes);
    for packet in &packets {
        packet.encode(&mut round_trip)?;
    }
    let round_trip_ok = round_trip == stream;

    let bytes_per_sec = total_bytes as f64 / decode_seconds.max(f64::EPSILON);
    let packets_per_sec = count as f64 / decode_seconds.max(f64::EPSILON);

    if let Some(parent) = cfg.bench.output.parent() {
        fs::create_dir_all(parent)?;
    }
    let mut csv = fs::OpenOptions::new()
        .create(true)
        .append(true)
        .open(&cfg.bench.output)?;
    if csv.metadata()?.len() == 0 {
        use std::io::Write;
        csv.write_all(
            b"experiment,packets,payload_bytes,bytes_processed,seconds,bytes_per_sec,packets_per_sec,round_trip_ok\n",
        )?;
    }
    use std::io::Write;
    csv.write_all(
        format!(
            "{},{},{},{},{:.6},{:.1},{:.1},{}\n",
            cfg.general.experiment,
            count,
            payload_size,
            total_bytes,
            decode_seconds,
            bytes_per_sec,
            packets_per_sec,
            round_trip_ok
        )
        .as_bytes(),
    )?;

    println!(
        "bench: {} packets ({} bytes) decoded in {:.3}s = {:.1} packets/s, round-trip {}",
        count, total_bytes, decode_seconds, packets_per_sec, round_trip_ok
    );
    println!("wrote {}", cfg.bench.output.display());
    Ok(())
}
