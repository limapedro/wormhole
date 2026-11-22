//! Turn a raw capture into structured messages and verify it round-trips.

use crate::config::Config;
use serde::Serialize;
use std::fs;
use std::io::Cursor;
use std::path::Path;
use wh_protocol::Packet;

/// Machine-readable dissection result.
#[derive(Debug, Serialize)]
struct Summary {
    experiment: String,
    input: String,
    packet_count: usize,
    byte_count: usize,
    round_trip_ok: bool,
    packets: Vec<PacketSummary>,
}

/// One decoded message.
#[derive(Debug, Serialize)]
struct PacketSummary {
    index: usize,
    offset: usize,
    kind: String,
    timestamp_ns: u64,
    payload_bytes: usize,
    wire_bytes: usize,
    #[serde(skip_serializing_if = "Option::is_none")]
    color_full_range: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    audio_format: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    sample_rate: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    speakers: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    frames: Option<i32>,
}

/// Decode the configured capture, verify byte-identical re-encoding, write JSON.
pub fn run(cfg: &Config) -> Result<(), Box<dyn std::error::Error>> {
    let input = &cfg.dissect.input;
    let bytes =
        fs::read(input).map_err(|err| format!("cannot read capture {}: {err}", input.display()))?;

    let mut packets = Vec::new();
    let mut summaries = Vec::new();
    let mut offset = 0_usize;

    while offset < bytes.len() {
        let mut cursor = Cursor::new(&bytes[offset..]);
        let packet = Packet::decode(&mut cursor, cfg.dissect.max_payload_bytes)
            .map_err(|err| format!("decode failed at byte offset {offset}: {err}"))?;
        let wire_len = packet.wire_len();
        if wire_len == 0 || offset + wire_len > bytes.len() {
            return Err(format!("invalid wire length {wire_len} at offset {offset}").into());
        }
        summaries.push(summarize(summaries.len(), offset, &packet));
        packets.push(packet);
        offset += wire_len;
    }

    let mut reencoded = Vec::with_capacity(bytes.len());
    for packet in &packets {
        packet.encode(&mut reencoded)?;
    }
    let round_trip_ok = reencoded == bytes;
    if !round_trip_ok {
        return Err("decode/re-encode was not byte-identical".into());
    }

    let summary = Summary {
        experiment: cfg.general.experiment.clone(),
        input: input.to_string_lossy().into_owned(),
        packet_count: packets.len(),
        byte_count: bytes.len(),
        round_trip_ok,
        packets: summaries,
    };

    write_json(&cfg.dissect.output, &summary)?;
    println!(
        "dissect: {} packets, {} bytes, round-trip byte-identical",
        summary.packet_count, summary.byte_count
    );
    println!("wrote {}", cfg.dissect.output.display());
    Ok(())
}

fn summarize(index: usize, offset: usize, packet: &Packet) -> PacketSummary {
    let header = packet.header();
    let kind = String::from_utf8_lossy(&header.kind.as_bytes()).into_owned();
    let mut row = PacketSummary {
        index,
        offset,
        kind,
        timestamp_ns: header.timestamp,
        payload_bytes: header.size as usize,
        wire_bytes: packet.wire_len(),
        color_full_range: None,
        audio_format: None,
        sample_rate: None,
        speakers: None,
        frames: None,
    };
    match packet {
        Packet::Jpeg { image, .. } => row.color_full_range = Some(image.is_full_range()),
        Packet::Wave { wave, .. } => {
            row.audio_format = Some(wave.format);
            row.sample_rate = Some(wave.sample_rate);
            row.speakers = Some(wave.speakers);
            row.frames = Some(wave.frames);
        }
        Packet::Unknown { .. } => {}
    }
    row
}

fn write_json<T: Serialize>(path: &Path, value: &T) -> Result<(), Box<dyn std::error::Error>> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    let text = serde_json::to_string_pretty(value)?;
    fs::write(path, text)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use wh_protocol::{Header, ImageHeader, MessageKind, WaveHeader};

    #[test]
    fn summarize_records_wave_fields() {
        let packet = Packet::Wave {
            header: Header {
                kind: MessageKind::Wave,
                timestamp: 12,
                size: 4,
            },
            wave: WaveHeader {
                format: 4,
                sample_rate: 48_000,
                speakers: 2,
                frames: 1,
            },
            payload: vec![0; 4],
        };
        let row = summarize(0, 0, &packet);
        assert_eq!(row.kind, "WAVE");
        assert_eq!(row.sample_rate, Some(48_000));
        assert_eq!(row.wire_bytes, 16 + 16 + 4);
    }

    #[test]
    fn summarize_records_jpeg_full_range() {
        let packet = Packet::Jpeg {
            header: Header {
                kind: MessageKind::Jpeg,
                timestamp: 1,
                size: 0,
            },
            image: ImageHeader {
                color_range_min: [0.0; 3],
                color_range_max: [1.0; 3],
                ..ImageHeader::default()
            },
            payload: Vec::new(),
        };
        assert_eq!(summarize(0, 0, &packet).color_full_range, Some(true));
    }
}
