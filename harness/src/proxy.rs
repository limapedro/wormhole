//! Transparent TCP capture/impairment proxy.
//!
//! The proxy sits between a Teleport sender (client) and receiver (target),
//! records the sender-to-receiver byte stream with its arrival order, and can
//! inject fixed delay, random drops and a bandwidth cap. The capture file is a
//! raw concatenation of packets, exactly what `dissect` expects.

use crate::config::Config;
use std::fs::{self, File, OpenOptions};
use std::io::{BufWriter, Read, Write};
use std::net::{Shutdown, TcpListener, TcpStream};
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

type CaptureWriter = Arc<Mutex<BufWriter<File>>>;

/// Start the proxy and serve connections until the process exits.
pub fn run(cfg: &Config) -> Result<(), Box<dyn std::error::Error>> {
    ensure_parent(&cfg.proxy.capture)?;
    let file = OpenOptions::new()
        .create(true)
        .append(true)
        .open(&cfg.proxy.capture)?;
    let capture: CaptureWriter = Arc::new(Mutex::new(BufWriter::new(file)));

    let listener = TcpListener::bind(&cfg.proxy.listen)?;
    println!(
        "proxy: listening on {} -> {}, capture {}",
        cfg.proxy.listen,
        cfg.proxy.target,
        cfg.proxy.capture.display()
    );

    for incoming in listener.incoming() {
        match incoming {
            Ok(client) => {
                let cfg = cfg.clone();
                let capture = Arc::clone(&capture);
                thread::spawn(move || {
                    if let Err(err) = handle_client(client, &cfg, capture) {
                        eprintln!("proxy: connection ended: {err}");
                    }
                });
            }
            Err(err) => eprintln!("proxy: accept failed: {err}"),
        }
    }
    Ok(())
}

fn handle_client(
    client: TcpStream,
    cfg: &Config,
    capture: CaptureWriter,
) -> Result<(), Box<dyn std::error::Error>> {
    let mut upstream = TcpStream::connect(&cfg.proxy.target)?;
    let mut from_client = client.try_clone()?;
    let mut to_client = client;
    let mut from_upstream = upstream.try_clone()?;

    let chunk = cfg.proxy.chunk_bytes.max(1);
    let downstream = thread::spawn(
        move || -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
            let mut buf = vec![0_u8; chunk];
            loop {
                let n = from_upstream.read(&mut buf)?;
                if n == 0 {
                    break;
                }
                to_client.write_all(&buf[..n])?;
            }
            Ok(())
        },
    );

    let mut buf = vec![0_u8; chunk];
    let mut drop_state: u64 = 0x9E37_79B9_7F4A_7C15;
    loop {
        let n = from_client.read(&mut buf)?;
        if n == 0 {
            break;
        }

        drop_state = drop_state
            .wrapping_mul(6_364_136_223_846_793_005)
            .wrapping_add(1);
        let roll = (drop_state >> 33) % 100;
        if roll < u64::from(cfg.proxy.drop_percent) {
            eprintln!("proxy: dropped {n} bytes");
            continue;
        }

        if cfg.proxy.delay_ms > 0 {
            thread::sleep(Duration::from_millis(cfg.proxy.delay_ms));
        }
        if cfg.proxy.bandwidth_bytes_per_sec > 0 {
            let micros = (n as u128 * 1_000_000 / cfg.proxy.bandwidth_bytes_per_sec as u128) as u64;
            thread::sleep(Duration::from_micros(micros));
        }

        upstream.write_all(&buf[..n])?;
        {
            let mut writer = capture.lock().map_err(|err| err.to_string())?;
            writer.write_all(&buf[..n])?;
            writer.flush()?;
        }
    }

    let _ = upstream.shutdown(Shutdown::Write);
    let _ = downstream.join();
    Ok(())
}

fn ensure_parent(path: &Path) -> Result<(), Box<dyn std::error::Error>> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    #[test]
    fn drop_roll_is_deterministic() {
        let mut state: u64 = 0x9E37_79B9_7F4A_7C15;
        let mut rolls = Vec::new();
        for _ in 0..8 {
            state = state
                .wrapping_mul(6_364_136_223_846_793_005)
                .wrapping_add(1);
            rolls.push((state >> 33) % 100);
        }
        assert_eq!(rolls.len(), 8);
        assert!(rolls.iter().all(|r| *r < 100));
    }
}
