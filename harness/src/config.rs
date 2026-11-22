//! Typed TOML configuration with explicit defaults.
//!
//! Harness tools take only `--config <path>` and `--help`; all behavior is
//! described by these structs. Defaults are loaded when no config path is
//! supplied, so a tool can always be run reproducibly with a single file.

use serde::{Deserialize, Serialize};
use std::fs;
use std::path::{Path, PathBuf};

/// Top-level harness configuration.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct Config {
    /// Experiment identity and shared output location.
    pub general: GeneralConfig,
    /// Protocol dissector settings.
    pub dissect: DissectConfig,
    /// TCP capture/impairment proxy settings.
    pub proxy: ProxyConfig,
    /// Deterministic synthetic media settings.
    pub media: MediaConfig,
    /// Harness self-benchmark settings.
    pub bench: BenchConfig,
    /// Standalone reference sender/receiver settings.
    pub reference: ReferenceConfig,
}

/// Shared experiment settings.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct GeneralConfig {
    /// Experiment folder name used in result paths.
    pub experiment: String,
    /// Root directory for generated results.
    pub output_dir: PathBuf,
}

impl Default for GeneralConfig {
    fn default() -> Self {
        Self {
            experiment: "exp_001".to_string(),
            output_dir: PathBuf::from("exp_results"),
        }
    }
}

/// Dissector settings.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct DissectConfig {
    /// Raw capture file produced by the proxy.
    pub input: PathBuf,
    /// JSON summary output path.
    pub output: PathBuf,
    /// Maximum accepted payload size in bytes.
    pub max_payload_bytes: usize,
}

impl Default for DissectConfig {
    fn default() -> Self {
        Self {
            input: PathBuf::from("exp_results/capture.bin"),
            output: PathBuf::from("exp_results/dissection.json"),
            max_payload_bytes: wh_protocol::DEFAULT_MAX_PAYLOAD,
        }
    }
}

/// Proxy settings.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct ProxyConfig {
    /// Address to listen on.
    pub listen: String,
    /// Address of the real receiver/sender to forward to.
    pub target: String,
    /// File receiving the sender-to-receiver byte stream.
    pub capture: PathBuf,
    /// Fixed delay applied to each forwarded chunk, in milliseconds.
    pub delay_ms: u64,
    /// Percentage of forwarded chunks dropped (0-100).
    pub drop_percent: u8,
    /// Bandwidth cap in bytes per second; 0 disables the cap.
    pub bandwidth_bytes_per_sec: u64,
    /// Read buffer size in bytes.
    pub chunk_bytes: usize,
}

impl Default for ProxyConfig {
    fn default() -> Self {
        Self {
            listen: "127.0.0.1:18080".to_string(),
            target: "127.0.0.1:18081".to_string(),
            capture: PathBuf::from("exp_results/capture.bin"),
            delay_ms: 0,
            drop_percent: 0,
            bandwidth_bytes_per_sec: 0,
            chunk_bytes: 64 * 1024,
        }
    }
}

/// Synthetic media settings.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct MediaConfig {
    /// Output directory.
    pub output_dir: PathBuf,
    /// Frame width in pixels.
    pub width: u32,
    /// Frame height in pixels.
    pub height: u32,
    /// Number of video frames to generate.
    pub frames: u32,
    /// Nominal frame rate, recorded in the sidecar metadata.
    pub fps: u32,
    /// Audio sample rate.
    pub sample_rate: u32,
    /// Audio duration in seconds.
    pub audio_seconds: u32,
    /// Audio channel count.
    pub channels: u16,
}

impl Default for MediaConfig {
    fn default() -> Self {
        Self {
            output_dir: PathBuf::from("exp_results/media"),
            width: 1280,
            height: 720,
            frames: 60,
            fps: 30,
            sample_rate: 48_000,
            audio_seconds: 2,
            channels: 2,
        }
    }
}

/// Dissector throughput self-benchmark settings.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct BenchConfig {
    /// Number of synthetic packets to process.
    pub packets: u32,
    /// Payload size per packet in bytes.
    pub payload_bytes: usize,
    /// CSV output path.
    pub output: PathBuf,
}

impl Default for BenchConfig {
    fn default() -> Self {
        Self {
            packets: 1_000,
            payload_bytes: 4096,
            output: PathBuf::from("exp_results/dissector-bench.csv"),
        }
    }
}

/// Standalone Teleport-protocol reference peer settings.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct ReferenceConfig {
    /// Address the reference sender connects to.
    pub target: String,
    /// Address the reference receiver listens on.
    pub listen: String,
    /// Number of video frames to send.
    pub frames: u32,
    /// Nominal frame rate used for pacing and timestamps.
    pub fps: u32,
    /// JPEG quality (1-100), matching Teleport's default of 90.
    pub quality: u8,
    /// Receiver CSV output path.
    pub output: PathBuf,
}

impl Default for ReferenceConfig {
    fn default() -> Self {
        Self {
            target: "127.0.0.1:18081".to_string(),
            listen: "127.0.0.1:18081".to_string(),
            frames: 30,
            fps: 30,
            quality: 90,
            output: PathBuf::from("exp_results/reference-receiver.csv"),
        }
    }
}

/// Load a config from a TOML file, replacing only the fields present in it.
pub fn load(path: &Path) -> Result<Config, Box<dyn std::error::Error>> {
    let text = fs::read_to_string(path)?;
    let config: Config = toml::from_str(&text)?;
    Ok(config)
}
