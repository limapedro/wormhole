//! Wormhole reverse-engineering and test harness.
//!
//! Only two flags exist: `--config <path>` and `--help`. Every behavior is
//! controlled by a typed TOML config (see `config.rs`).

mod bench;
mod config;
mod dissect;
mod media;
mod proxy;
mod reference;
mod replay;
mod synth;

use clap::{Parser, Subcommand};
use std::path::PathBuf;
use std::process::ExitCode;

/// Wormhole harness: observe, decode and measure network video protocols.
#[derive(Debug, Parser)]
#[command(name = "wormhole-harness", version, about, disable_version_flag = true)]
struct Args {
    /// Path to a typed TOML configuration file.
    #[arg(long, global = true)]
    config: Option<PathBuf>,

    /// Command to run.
    #[command(subcommand)]
    command: Command,
}

/// Available harness commands.
#[derive(Debug, Subcommand)]
enum Command {
    /// Print the effective typed configuration.
    Check,
    /// Decode a capture and verify byte-identical re-encoding.
    Dissect,
    /// Run a transparent TCP capture/impairment proxy.
    Proxy,
    /// Generate deterministic synthetic video and audio.
    Media,
    /// Generate a deterministic Teleport-compatible capture fixture.
    Synth,
    /// Replay a capture to the configured TCP target.
    Replay,
    /// Accept one TCP connection and store its bytes (test sink).
    Sink,
    /// Standalone Teleport-protocol reference sender.
    RefSender,
    /// Standalone Teleport-protocol reference receiver.
    RefReceiver,
    /// Measure dissector throughput (harness self-benchmark, not Teleport).
    Bench,
}

fn main() -> ExitCode {
    let args = Args::parse();
    let cfg = match args.config {
        Some(ref path) => match config::load(path) {
            Ok(cfg) => cfg,
            Err(err) => {
                eprintln!("error: cannot load config {}: {err}", path.display());
                return ExitCode::FAILURE;
            }
        },
        None => config::Config::default(),
    };

    let result = match args.command {
        Command::Check => {
            println!(
                "{}",
                toml::to_string_pretty(&cfg).expect("config serializes")
            );
            Ok(())
        }
        Command::Dissect => dissect::run(&cfg),
        Command::Proxy => proxy::run(&cfg),
        Command::Media => media::run(&cfg),
        Command::Synth => synth::run(&cfg),
        Command::Replay => replay::replay(&cfg),
        Command::Sink => replay::sink(&cfg),
        Command::RefSender => reference::sender(&cfg),
        Command::RefReceiver => reference::receiver(&cfg),
        Command::Bench => bench::run(&cfg),
    };

    match result {
        Ok(()) => ExitCode::SUCCESS,
        Err(err) => {
            eprintln!("error: {err}");
            ExitCode::FAILURE
        }
    }
}
