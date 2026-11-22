//! Deterministic synthetic video and audio generators.
//!
//! Video is raw I420 with color bars and a moving white column, so a frame
//! counter can be recovered at the receiver. Audio is 16-bit PCM with a tone
//! that changes frequency once per second, giving a sync marker. Multiple
//! tracks use distinct base frequencies so cross-track skew is measurable.

use crate::config::Config;
use std::fs::{self, File};
use std::io::{BufWriter, Write};
use std::path::{Path, PathBuf};

const BARS: usize = 8;
const BAR_Y: [u8; BARS] = [235, 210, 170, 128, 106, 70, 50, 25];
const BAR_U: [u8; BARS] = [128, 90, 40, 128, 170, 220, 240, 128];
const BAR_V: [u8; BARS] = [128, 240, 220, 128, 40, 90, 50, 128];

/// Standard scenarios from PERFORMANCE_TARGETS.md: (width, height, fps, name).
const PROFILES: [(u32, u32, u32, &str); 5] = [
    (1280, 720, 30, "720p30"),
    (1920, 1080, 30, "1080p30"),
    (1920, 1080, 60, "1080p60"),
    (2560, 1440, 60, "1440p60"),
    (3840, 2160, 30, "4K30"),
];

/// Generate the configured media files and a JSON metadata sidecar.
pub fn run(cfg: &Config) -> Result<(), Box<dyn std::error::Error>> {
    let dir = &cfg.media.output_dir;
    fs::create_dir_all(dir)?;
    let mut video_files = Vec::new();
    let mut audio_files = Vec::new();

    let video = write_video(
        dir,
        cfg.media.width,
        cfg.media.height,
        cfg.media.frames,
        "video_main",
    )?;
    video_files.push(
        video
            .file_name()
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_default(),
    );

    for track in 0..cfg.media.tracks.max(1) {
        let hz = 440 * (track as u32 + 1);
        let name = format!("track_{track}_{hz}hz");
        let audio = write_audio(cfg, dir, hz, &name)?;
        audio_files.push(
            audio
                .file_name()
                .map(|n| n.to_string_lossy().into_owned())
                .unwrap_or_default(),
        );
    }

    if cfg.media.profiles {
        for (width, height, _fps, name) in PROFILES {
            let path = write_video(
                dir,
                width,
                height,
                cfg.media.profile_frames,
                &format!("profile_{name}"),
            )?;
            video_files.push(
                path.file_name()
                    .map(|n| n.to_string_lossy().into_owned())
                    .unwrap_or_default(),
            );
        }
    }

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
        "tracks": cfg.media.tracks.max(1),
        "track_base_hz": "440*(track_index+1), doubled on odd seconds",
        "profiles": cfg.media.profiles,
        "profile_frames": cfg.media.profile_frames,
        "video_files": video_files,
        "audio_files": audio_files,
    });
    let meta_path = dir.join("metadata.json");
    fs::write(&meta_path, serde_json::to_string_pretty(&metadata)?)?;
    println!(
        "media: wrote {} frames plus {} profile videos",
        cfg.media.frames,
        if cfg.media.profiles {
            PROFILES.len()
        } else {
            0
        }
    );
    println!("media: wrote {} audio tracks", audio_files.len());
    println!("media: wrote {}", meta_path.display());
    Ok(())
}

/// Write raw I420 frames with color bars and a moving marker column.
pub fn write_video(
    dir: &Path,
    width: u32,
    height: u32,
    frames: u32,
    name: &str,
) -> Result<PathBuf, Box<dyn std::error::Error>> {
    let width = width.max(2) as usize;
    let height = height.max(2) as usize;
    let even_width = width - (width % 2);
    let even_height = height - (height % 2);
    let path = dir.join(format!(
        "{}_{}x{}_{}frames.i420",
        name, even_width, even_height, frames
    ));
    let file = File::create(&path)?;
    let mut writer = BufWriter::new(file);

    let y_size = even_width * even_height;
    let c_size = (even_width / 2) * (even_height / 2);
    let mut y_plane = vec![0_u8; y_size];
    let mut u_plane = vec![0_u8; c_size];
    let mut v_plane = vec![0_u8; c_size];

    for frame in 0..frames {
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

/// Write one deterministic S16LE WAV track with a per-second sync tone change.
pub fn write_audio(
    cfg: &Config,
    dir: &Path,
    base_hz: u32,
    name: &str,
) -> Result<PathBuf, Box<dyn std::error::Error>> {
    let sample_rate = cfg.media.sample_rate.max(1);
    let channels = cfg.media.channels.max(1) as usize;
    let frames = sample_rate * cfg.media.audio_seconds;
    let path = dir.join(format!(
        "{}_{}hz_{}ch_{}s.wav",
        name, base_hz, channels, cfg.media.audio_seconds
    ));
    let file = File::create(&path)?;
    let mut writer = BufWriter::new(file);

    let data_bytes = (frames as usize) * channels * 2;
    write_wav_header(
        &mut writer,
        sample_rate,
        cfg.media.channels.max(1),
        data_bytes,
    )?;

    for frame in 0..frames {
        let second = frame / sample_rate;
        let hz = if second % 2 == 0 {
            base_hz
        } else {
            base_hz * 2
        };
        let phase = (frame as f64) / (sample_rate as f64) * f64::from(hz) * std::f64::consts::TAU;
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

    #[test]
    fn profile_matrix_covers_target_scenarios() {
        assert_eq!(PROFILES.len(), 5);
        assert_eq!(PROFILES[0], (1280, 720, 30, "720p30"));
        assert_eq!(PROFILES[4], (3840, 2160, 30, "4K30"));
    }
}
