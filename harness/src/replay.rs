//! Replay a captured byte stream to a TCP peer, and a minimal sink for tests.

use crate::config::Config;
use std::fs::{self, File};
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};

/// Send the configured capture file to the configured TCP target.
pub fn replay(cfg: &Config) -> Result<(), Box<dyn std::error::Error>> {
    let bytes = fs::read(&cfg.dissect.input)?;
    // Replay feeds the proxy's listen socket; the proxy forwards to its
    // configured target. This keeps one destination concept in the config.
    let mut stream = TcpStream::connect(&cfg.proxy.listen)?;
    stream.write_all(&bytes)?;
    stream.flush()?;
    println!(
        "replay: sent {} bytes from {} to proxy {}",
        bytes.len(),
        cfg.dissect.input.display(),
        cfg.proxy.listen
    );
    Ok(())
}

/// Listen on the configured target address and store everything received.
pub fn sink(cfg: &Config) -> Result<(), Box<dyn std::error::Error>> {
    let path = cfg.general.output_dir.join("sink.bin");
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    let listener = TcpListener::bind(&cfg.proxy.target)?;
    println!(
        "sink: listening on {} -> {}",
        cfg.proxy.target,
        path.display()
    );
    if let Some(incoming) = listener.incoming().next() {
        let mut stream = incoming?;
        let mut file = File::create(&path)?;
        let mut buf = vec![0_u8; 64 * 1024];
        loop {
            let n = stream.read(&mut buf)?;
            if n == 0 {
                break;
            }
            file.write_all(&buf[..n])?;
        }
        println!("sink: wrote {}", path.display());
    }
    Ok(())
}
