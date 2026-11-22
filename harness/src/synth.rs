//! Generate a deterministic capture containing Teleport-compatible packets.
//!
//! This is used to exercise the dissector, proxy and replay paths without a
//! human in the loop. It is a harness fixture, not a Teleport implementation.

use crate::config::Config;
use std::fs::{self, File};
use std::io::{BufWriter, Write};
use wh_protocol::{Header, ImageHeader, MessageKind, Packet, WaveHeader};

/// Write a capture of alternating JPEG and WAVE packets.
pub fn run(cfg: &Config) -> Result<(), Box<dyn std::error::Error>> {
    let path = &cfg.dissect.input;
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    let file = File::create(path)?;
    let mut writer = BufWriter::new(file);

    let frames = cfg.media.frames.max(1);
    let audio_payload = (cfg.media.sample_rate / 100) as usize * cfg.media.channels as usize * 2;
    let video_payload = 1024_usize;

    for frame in 0..frames {
        let timestamp = u64::from(frame) * 1_000_000_000 / cfg.media.fps.max(1) as u64;
        Packet::Jpeg {
            header: Header {
                kind: MessageKind::Jpeg,
                timestamp,
                size: video_payload as i32,
            },
            image: ImageHeader {
                color_range_max: [1.0, 1.0, 1.0],
                ..ImageHeader::default()
            },
            payload: vec![(frame % 251) as u8; video_payload],
        }
        .encode(&mut writer)?;

        Packet::Wave {
            header: Header {
                kind: MessageKind::Wave,
                timestamp,
                size: audio_payload as i32,
            },
            wave: WaveHeader {
                format: 4,
                sample_rate: cfg.media.sample_rate as i32,
                speakers: 2,
                frames: (audio_payload / (cfg.media.channels as usize * 2)) as i32,
            },
            payload: vec![(frame % 97) as u8; audio_payload],
        }
        .encode(&mut writer)?;
    }
    writer.flush()?;
    println!(
        "synth: wrote {} frames of JPEG+WAVE packets to {}",
        frames,
        path.display()
    );
    Ok(())
}
