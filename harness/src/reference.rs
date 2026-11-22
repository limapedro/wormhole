//! Minimal standalone sender and receiver for the original Teleport protocol.
//!
//! These are reference peers for differential testing: they speak the wire
//! format documented in `PROTOCOL.md` without depending on libobs or Go. They
//! do not claim to reproduce Teleport internals, only its observable wire
//! behavior.

use crate::config::Config;
use jpeg_decoder::Decoder;
use jpeg_encoder::{ColorType, Encoder};
use std::fs::{self, File};
use std::io::{BufWriter, Cursor, Read, Write};
use std::net::{TcpListener, TcpStream};
use std::path::Path;
use std::thread;
use std::time::{Duration, Instant};
use wh_protocol::{Header, ImageHeader, MessageKind, Packet, WaveHeader};

/// Encode and send a deterministic stream of JPEG video and PCM audio packets.
pub fn sender(cfg: &Config) -> Result<(), Box<dyn std::error::Error>> {
    let width = u16::try_from(cfg.media.width.max(2))?;
    let height = u16::try_from(cfg.media.height.max(2))?;
    let frames = cfg.reference.frames;
    let fps = cfg.reference.fps.max(1);
    let quality = cfg.reference.quality.clamp(1, 100);

    let mut stream = TcpStream::connect(&cfg.reference.target)?;
    let mut rgb = vec![0_u8; usize::from(width) * usize::from(height) * 3];
    let audio_frames = 1024_usize;
    let audio_payload = vec![0_u8; audio_frames * 2 * 2];

    let start = Instant::now();
    for frame in 0..frames {
        fill_color_bars(&mut rgb, usize::from(width), usize::from(height), frame);
        let mut jpeg = Vec::new();
        Encoder::new(&mut jpeg, quality).encode(&rgb, width, height, ColorType::Rgb)?;

        let timestamp = (u64::from(frame) * 1_000_000_000) / u64::from(fps);
        Packet::Jpeg {
            header: Header {
                kind: MessageKind::Jpeg,
                timestamp,
                size: i32::try_from(jpeg.len())?,
            },
            image: ImageHeader {
                color_range_max: [1.0, 1.0, 1.0],
                ..ImageHeader::default()
            },
            payload: jpeg,
        }
        .encode(&mut stream)?;

        Packet::Wave {
            header: Header {
                kind: MessageKind::Wave,
                timestamp,
                size: i32::try_from(audio_payload.len())?,
            },
            wave: WaveHeader {
                format: 2,
                sample_rate: 48_000,
                speakers: 2,
                frames: audio_frames as i32,
            },
            payload: audio_payload.clone(),
        }
        .encode(&mut stream)?;

        let target = start + Duration::from_nanos(timestamp);
        if let Some(wait) = target.checked_duration_since(Instant::now()) {
            thread::sleep(wait);
        }
    }
    stream.flush()?;
    println!(
        "ref-sender: sent {} frames ({}x{}, q={}) to {}",
        frames, width, height, quality, cfg.reference.target
    );
    Ok(())
}

/// Receive one connection, decode packets and write a CSV timing summary.
pub fn receiver(cfg: &Config) -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind(&cfg.reference.listen)?;
    println!("ref-receiver: listening on {}", cfg.reference.listen);
    let (mut stream, peer) = listener.accept()?;
    println!("ref-receiver: connected from {peer}");

    let path = &cfg.reference.output;
    fs::create_dir_all(path.parent().unwrap_or_else(|| Path::new(".")))?;
    let file = File::create(path)?;
    let mut csv = BufWriter::new(file);
    csv.write_all(b"index,kind,timestamp_ns,payload_bytes,width,height,decoded\n")?;

    let mut bytes = Vec::new();
    stream.read_to_end(&mut bytes)?;
    let mut cursor = Cursor::new(&bytes);
    let mut index = 0_usize;
    let mut video_count = 0_usize;
    let mut audio_count = 0_usize;
    let mut decode_failures = 0_usize;

    while cursor.position() < bytes.len() as u64 {
        let packet = wh_protocol::Packet::decode(&mut cursor, cfg.dissect.max_payload_bytes)?;
        let header = *packet.header();
        let kind = String::from_utf8_lossy(&header.kind.as_bytes()).into_owned();
        let mut width = 0;
        let mut height = 0;
        let mut decoded = "n/a";
        match &packet {
            Packet::Jpeg { payload, .. } => {
                let mut decoder = Decoder::new(Cursor::new(payload));
                match decoder.decode() {
                    Ok(pixels) => {
                        if let Some(info) = decoder.info() {
                            width = u32::from(info.width);
                            height = u32::from(info.height);
                        }
                        decoded = if pixels.is_empty() { "empty" } else { "ok" };
                    }
                    Err(_) => {
                        decoded = "error";
                        decode_failures += 1;
                    }
                }
                video_count += 1;
            }
            Packet::Wave { .. } => audio_count += 1,
            Packet::Unknown { .. } => {}
        }
        csv.write_all(
            format!(
                "{},{},{},{},{},{},{}\n",
                index, kind, header.timestamp, header.size, width, height, decoded
            )
            .as_bytes(),
        )?;
        index += 1;
    }
    csv.flush()?;
    println!(
        "ref-receiver: {} packets ({} video, {} audio), {} decode failures -> {}",
        index,
        video_count,
        audio_count,
        decode_failures,
        path.display()
    );
    Ok(())
}

fn fill_color_bars(rgb: &mut [u8], width: usize, height: usize, frame: u32) {
    const BARS: [[u8; 3]; 8] = [
        [235, 128, 128],
        [210, 90, 240],
        [170, 40, 220],
        [128, 128, 128],
        [106, 170, 40],
        [70, 220, 90],
        [50, 240, 50],
        [25, 128, 128],
    ];
    let marker = (frame as usize * 8) % width;
    for row in 0..height {
        for col in 0..width {
            let bar = (col * 8) / width;
            let mut color = BARS[bar.min(7)];
            if col >= marker && col < marker + 4 {
                color = [255, 255, 255];
            }
            let offset = (row * width + col) * 3;
            rgb[offset..offset + 3].copy_from_slice(&color);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn color_bars_fill_the_buffer() {
        let mut rgb = vec![0_u8; 32 * 16 * 3];
        fill_color_bars(&mut rgb, 32, 16, 5);
        assert!(rgb.iter().any(|v| *v != 0));
    }
}
