//! Image-quality analysis of JPEG packets in a capture.
//!
//! Decodes each JPEG frame and reports dimensions, mean RGB and PSNR either
//! against a configured expected colour or against the frame's own mean
//! (a uniformity/fidelity measure for synthetic colour-bar sources).
//! This module is temporary Rust tooling; the product core is all C.

use crate::config::Config;
use jpeg_decoder::Decoder;
use serde::Serialize;
use std::fs;
use std::io::Cursor;
use wh_protocol::{MessageKind, Packet};

/// Per-frame and aggregate quality report.
#[derive(Debug, Serialize)]
struct Report {
    experiment: String,
    input: String,
    frames_analyzed: usize,
    width: u32,
    height: u32,
    comparison: String,
    mean_r: f64,
    mean_g: f64,
    mean_b: f64,
    mse: f64,
    psnr_db: f64,
}

/// Decode JPEG packets from the configured capture and write a JSON report.
pub fn run(cfg: &Config) -> Result<(), Box<dyn std::error::Error>> {
    let bytes = fs::read(&cfg.dissect.input)?;
    let mut cursor = Cursor::new(&bytes);
    let mut frames = Vec::new();
    while cursor.position() < bytes.len() as u64 {
        let packet = Packet::decode(&mut cursor, cfg.dissect.max_payload_bytes)?;
        if packet.header().kind != MessageKind::Jpeg {
            continue;
        }
        if let Packet::Jpeg {
            payload, header, ..
        } = packet
        {
            let mut decoder = Decoder::new(Cursor::new(payload));
            let pixels = decoder.decode()?;
            let info = decoder.info().ok_or("JPEG has no frame info")?;
            frames.push(Frame {
                timestamp: header.timestamp,
                width: u32::from(info.width),
                height: u32::from(info.height),
                pixels,
            });
        }
    }
    if frames.is_empty() {
        return Err("no JPEG frames found in capture".into());
    }

    let first = &frames[0];
    let expected = cfg.media.expected_color;
    let comparison = if expected.is_some() {
        "configured expected colour"
    } else {
        "per-frame mean (uniformity)"
    };

    let mut sum_r = 0.0;
    let mut sum_g = 0.0;
    let mut sum_b = 0.0;
    let mut sum_mse = 0.0;
    let mut count = 0usize;

    for frame in &frames {
        let n = frame.pixels.len() / 3;
        let mut r = 0.0;
        let mut g = 0.0;
        let mut b = 0.0;
        for pixel in frame.pixels.chunks_exact(3) {
            r += f64::from(pixel[0]);
            g += f64::from(pixel[1]);
            b += f64::from(pixel[2]);
        }
        let mean = [r / n as f64, g / n as f64, b / n as f64];
        let target = expected.unwrap_or([
            mean[0].round() as u8,
            mean[1].round() as u8,
            mean[2].round() as u8,
        ]);
        let mut mse = 0.0;
        for pixel in frame.pixels.chunks_exact(3) {
            let dr = f64::from(pixel[0]) - f64::from(target[0]);
            let dg = f64::from(pixel[1]) - f64::from(target[1]);
            let db = f64::from(pixel[2]) - f64::from(target[2]);
            mse += dr * dr + dg * dg + db * db;
        }
        mse /= n as f64 * 3.0;
        sum_r += mean[0];
        sum_g += mean[1];
        sum_b += mean[2];
        sum_mse += mse;
        count += 1;
    }

    let frames_n = count as f64;
    let mse = sum_mse / frames_n;
    // JSON cannot represent infinity; cap it and record the raw MSE so an
    // unbounded PSNR is still unambiguous.
    let psnr = if mse <= 0.0 {
        120.0
    } else {
        (10.0 * (255.0_f64 * 255.0 / mse).log10()).min(120.0)
    };
    let report = Report {
        experiment: cfg.general.experiment.clone(),
        input: cfg.dissect.input.to_string_lossy().into_owned(),
        frames_analyzed: count,
        width: first.width,
        height: first.height,
        comparison: comparison.to_string(),
        mean_r: sum_r / frames_n,
        mean_g: sum_g / frames_n,
        mean_b: sum_b / frames_n,
        mse,
        psnr_db: psnr,
    };
    let path = &cfg.media.quality_output;
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(path, serde_json::to_string_pretty(&report)?)?;
    println!(
        "quality: {} frames {}x{}, mean RGB=({:.1},{:.1},{:.1}), MSE={:.3}, PSNR={:.2} dB",
        count, first.width, first.height, report.mean_r, report.mean_g, report.mean_b, mse, psnr
    );
    println!("wrote {}", path.display());
    Ok(())
}

struct Frame {
    #[allow(dead_code)]
    timestamp: u64,
    width: u32,
    height: u32,
    pixels: Vec<u8>,
}
