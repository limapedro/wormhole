//! Deterministic synthetic video and audio generators.
//!
//! Video is raw I420 with color bars and a moving white column, so a frame
//! counter can be recovered at the receiver. Audio is 16-bit PCM with a tone
//! that changes frequency once per second, giving a sync marker.

use crate::config::Config;
use std::fs::{self, File};
use std::io::{BufWriter, Write};
use std::path::Path;

const BARS: usize = 8;
const BAR_Y: [u8; BARS] = [235, 210, 170, 128, 106, 70, 50, 25];
const BAR_U: [u8; BARS] = [128, 90, 40, 128, 170, 220, 240, 128];
const BAR_V: [u8; BARS] = [128, 240, 220, 128, 40, 90, 50, 128];

/// Generate the configured media files and a JSON metadata sidecar.
pub fn run(cfg: &Config) -> Result<(), Box<dyn std::error::Error>> {
    let dir = &cfg.media.output_dir;
    fs::create_dir_all(dir)?;
    let video_path = generate_video(cfg, dir)?;
    let audio_path = generate_audio(cfg, dir)?;

    let metadata = serde_json::json!({
        "experiment": cfg.general.experiment,
        "width": cfg.media.width,
        "height": cfg.media.height,
        "frames": cfg.media.frames,
        "fps": cfg.media.fps,
        "video_format": "I420",
        "audio_format": "S16LE",
        "sample_rate": cfg.media.sample_rate,
        "channels": cfg.media.channels,
        "audio_seconds": cfg.media.audio_seconds,
        "tone_hz_even_second": 440,
        "tone_hz_odd_second": 880,
        "video_file": video_path.file_name().map(|n| n.to_string_lossy()),
        "audio_file": audio_path.file_name().map(|n| n.to_string_lossy()),
    });
    let meta_path = dir.join("metadata.json");
    fs::write(&meta_path, serde_json::to_string_pretty(&metadata)?)?;
    println!(
        "media: wrote {} frames to {}",
        cfg.media.frames,
        video_path.display()
    );
    println!("media: wrote audio to {}", audio_path.display());
    println!("media: wrote {}", meta_path.display());
    Ok(())
}

fn generate_video(
    cfg: &Config,
    dir: &Path,
) -> Result<std::path::PathBuf, Box<dyn std::error::Error>> {
    let width = cfg.media.width.max(2) as usize;
    let height = cfg.media.height.max(2) as usize;
    let even_width = width - (width % 2);
    let even_height = height - (height % 2);
    let path = dir.join(format!(
        "video_{}x{}_{}frames.i420",
        even_width, even_height, cfg.media.frames
    ));
    let file = File::create(&path)?;
    let mut writer = BufWriter::new(file);

    let y_size = even_width * even_height;
    let c_size = (even_width / 2) * (even_height / 2);
    let mut y_plane = vec![0_u8; y_size];
    let mut u_plane = vec![0_u8; c_size];
    let mut v_plane = vec![0_u8; c_size];

    for frame in 0..cfg.media.frames {
        let marker = (frame as usize * 8) % even_width;
        for row in 0..even_height {
            for col in 0..even_width {
                let bar = (col * BARS) / even_width;
                let mut luma = BAR_Y[bar.min(BARS - 1)];
                if col >= marker && col < marker + 4 {
                    luma = 255;
                }
                y_plane[row * even_width + col] = luma;
            }
        }
        for row in 0..even_height / 2 {
            for col in 0..even_width / 2 {
                let bar = (col * 2 * BARS) / even_width;
                u_plane[row * (even_width / 2) + col] = BAR_U[bar.min(BARS - 1)];
                v_plane[row * (even_width / 2) + col] = BAR_V[bar.min(BARS - 1)];
            }
        }
        writer.write_all(&y_plane)?;
        writer.write_all(&u_plane)?;
        writer.write_all(&v_plane)?;
    }
    writer.flush()?;
    Ok(path)
}

fn generate_audio(
    cfg: &Config,
    dir: &Path,
) -> Result<std::path::PathBuf, Box<dyn std::error::Error>> {
    let sample_rate = cfg.media.sample_rate.max(1);
    let channels = cfg.media.channels.max(1) as usize;
    let frames = sample_rate * cfg.media.audio_seconds;
    let path = dir.join(format!(
        "audio_{}hz_{}ch_{}s.wav",
        sample_rate, channels, cfg.media.audio_seconds
    ));
    let file = File::create(&path)?;
    let mut writer = BufWriter::new(file);

    let data_bytes = (frames as usize) * channels * 2;
    write_wav_header(&mut writer, sample_rate, channels as u16, data_bytes)?;

    for frame in 0..frames {
        let second = frame / sample_rate;
        let hz = if second % 2 == 0 { 440.0 } else { 880.0 };
        let phase = (frame as f64) / (sample_rate as f64) * hz * std::f64::consts::TAU;
        let sample = (phase.sin() * 0.2 * 32767.0) as i16;
        for _ in 0..channels {
            writer.write_all(&sample.to_le_bytes())?;
        }
    }
    writer.flush()?;
    Ok(path)
}

fn write_wav_header(
    writer: &mut BufWriter<File>,
    sample_rate: u32,
    channels: u16,
    data_bytes: usize,
) -> Result<(), Box<dyn std::error::Error>> {
    let byte_rate = sample_rate * u32::from(channels) * 2;
    let block_align = channels * 2;
    writer.write_all(b"RIFF")?;
    writer.write_all(&(36_u32 + data_bytes as u32).to_le_bytes())?;
    writer.write_all(b"WAVE")?;
    writer.write_all(b"fmt ")?;
    writer.write_all(&16_u32.to_le_bytes())?;
    writer.write_all(&1_u16.to_le_bytes())?;
    writer.write_all(&channels.to_le_bytes())?;
    writer.write_all(&sample_rate.to_le_bytes())?;
    writer.write_all(&byte_rate.to_le_bytes())?;
    writer.write_all(&block_align.to_le_bytes())?;
    writer.write_all(&16_u16.to_le_bytes())?;
    writer.write_all(b"data")?;
    writer.write_all(&(data_bytes as u32).to_le_bytes())?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn color_bar_indices_stay_in_range() {
        for width in [2_usize, 17, 1280] {
            for col in 0..width {
                let bar = (col * BARS) / width;
                assert!(bar < BARS);
            }
        }
    }

    #[test]
    fn marker_wraps_within_frame_width() {
        for width in [2_usize, 17, 1280] {
            for frame in 0..100_u32 {
                let marker = (frame as usize * 8) % width;
                assert!(marker < width);
            }
        }
    }
}
