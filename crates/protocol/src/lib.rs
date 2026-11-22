//! Wire-format primitives for the original Teleport protocol and Wormhole v2.
//!
//! The original protocol is documented byte-for-byte in `PROTOCOL.md`. It has
//! no magic number, no version field and no total-length field: every packet
//! is a 16-byte [`Header`], an optional type-specific header, and `Header.size`
//! payload bytes. All integers are little-endian and structs are serialized
//! field-by-field, without host-language padding.
//!
//! This crate performs strict, bounded parsing: a malformed packet returns an
//! error rather than panicking, because network input is hostile.

use std::io::{self, ErrorKind, Read, Write};

/// Length of the common header in bytes: 4 + 8 + 4.
pub const HEADER_LEN: usize = 16;
/// Length of the `JPEG` image header: 16 + 3 + 3 floats.
pub const IMAGE_HEADER_LEN: usize = 88;
/// Length of the `WAVE` audio header: 4 int32 fields.
pub const WAVE_HEADER_LEN: usize = 16;
/// Default upper bound for a decoded payload (64 MiB).
pub const DEFAULT_MAX_PAYLOAD: usize = 64 * 1024 * 1024;

/// The four-byte packet type tag.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum MessageKind {
    /// Compressed video frame.
    Jpeg,
    /// Uncompressed interleaved PCM audio.
    Wave,
    /// Present in the upstream receiver's switch, never emitted by the
    /// reference version. Treated as an unknown type on the wire.
    Anja,
    /// Any other four-byte tag.
    Unknown([u8; 4]),
}

impl MessageKind {
    /// Interpret four wire bytes as a message kind.
    pub const fn from_bytes(b: [u8; 4]) -> Self {
        match b {
            [b'J', b'P', b'E', b'G'] => Self::Jpeg,
            [b'W', b'A', b'V', b'E'] => Self::Wave,
            [b'A', b'N', b'J', b'A'] => Self::Anja,
            _ => Self::Unknown(b),
        }
    }

    /// Return the four wire bytes for this kind.
    pub const fn as_bytes(&self) -> [u8; 4] {
        match self {
            Self::Jpeg => *b"JPEG",
            Self::Wave => *b"WAVE",
            Self::Anja => *b"ANJA",
            Self::Unknown(b) => *b,
        }
    }

    /// Number of type-specific header bytes that follow the common header.
    pub const fn extra_len(&self) -> usize {
        match self {
            Self::Jpeg => IMAGE_HEADER_LEN,
            Self::Wave => WAVE_HEADER_LEN,
            Self::Anja | Self::Unknown(_) => 0,
        }
    }

    /// Whether this type carries audio.
    pub const fn is_audio(&self) -> bool {
        matches!(self, Self::Wave)
    }
}

/// Errors produced while decoding network data.
#[derive(Debug, thiserror::Error)]
pub enum DecodeError {
    /// The stream ended before a complete packet could be read.
    #[error("unexpected end of input")]
    UnexpectedEof,
    /// The wire payload size was negative.
    #[error("invalid payload size: {0}")]
    InvalidSize(i32),
    /// The wire payload size exceeded the configured limit.
    #[error("payload size {0} exceeds limit {1}")]
    PayloadTooLarge(usize, usize),
    /// Underlying I/O failure.
    #[error("i/o error: {0}")]
    Io(#[from] io::Error),
}

/// The common 16-byte packet header.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Header {
    /// Packet type tag.
    pub kind: MessageKind,
    /// OBS timestamp in nanoseconds.
    pub timestamp: u64,
    /// Payload length in bytes, excluding both headers.
    pub size: i32,
}

impl Header {
    /// Decode a header from exactly 16 bytes.
    pub fn decode<R: Read>(reader: &mut R) -> Result<Self, DecodeError> {
        let mut buf = [0_u8; HEADER_LEN];
        read_exact(reader, &mut buf)?;
        Ok(Self {
            kind: MessageKind::from_bytes([buf[0], buf[1], buf[2], buf[3]]),
            timestamp: u64::from_le_bytes(buf[4..12].try_into().expect("fixed size slice")),
            size: i32::from_le_bytes(buf[12..16].try_into().expect("fixed size slice")),
        })
    }

    /// Encode this header to a writer.
    pub fn encode<W: Write>(&self, writer: &mut W) -> Result<(), io::Error> {
        writer.write_all(&self.kind.as_bytes())?;
        writer.write_all(&self.timestamp.to_le_bytes())?;
        writer.write_all(&self.size.to_le_bytes())
    }
}

/// Color metadata attached to a `JPEG` packet (88 bytes).
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ImageHeader {
    /// libobs 4x4 color matrix, row-major, little-endian floats.
    pub color_matrix: [f32; 16],
    /// Minimum component values; full range is exactly `{0,0,0}`.
    pub color_range_min: [f32; 3],
    /// Maximum component values; full range is exactly `{1,1,1}`.
    pub color_range_max: [f32; 3],
}

impl Default for ImageHeader {
    fn default() -> Self {
        Self {
            color_matrix: [0.0; 16],
            color_range_min: [0.0; 3],
            color_range_max: [0.0; 3],
        }
    }
}

impl ImageHeader {
    /// Decode 88 bytes of color metadata.
    pub fn decode<R: Read>(reader: &mut R) -> Result<Self, DecodeError> {
        let mut header = Self::default();
        for value in &mut header.color_matrix {
            *value = read_f32(reader)?;
        }
        for value in &mut header.color_range_min {
            *value = read_f32(reader)?;
        }
        for value in &mut header.color_range_max {
            *value = read_f32(reader)?;
        }
        Ok(header)
    }

    /// Encode the color metadata.
    pub fn encode<W: Write>(&self, writer: &mut W) -> Result<(), io::Error> {
        for value in self.color_matrix {
            writer.write_all(&value.to_le_bytes())?;
        }
        for value in self.color_range_min {
            writer.write_all(&value.to_le_bytes())?;
        }
        for value in self.color_range_max {
            writer.write_all(&value.to_le_bytes())?;
        }
        Ok(())
    }

    /// True when the metadata describes a full-range color image.
    pub fn is_full_range(&self) -> bool {
        self.color_range_min == [0.0, 0.0, 0.0] && self.color_range_max == [1.0, 1.0, 1.0]
    }
}

/// Audio description attached to a `WAVE` packet (16 bytes).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct WaveHeader {
    /// libobs `enum audio_format` integer as sent by the peer.
    pub format: i32,
    /// Samples per second.
    pub sample_rate: i32,
    /// libobs `enum speaker_layout` integer, not a channel count.
    pub speakers: i32,
    /// Number of sample frames in the payload.
    pub frames: i32,
}

impl WaveHeader {
    /// Decode 16 bytes of audio metadata.
    pub fn decode<R: Read>(reader: &mut R) -> Result<Self, DecodeError> {
        let mut buf = [0_u8; WAVE_HEADER_LEN];
        read_exact(reader, &mut buf)?;
        Ok(Self {
            format: i32::from_le_bytes(buf[0..4].try_into().expect("fixed size slice")),
            sample_rate: i32::from_le_bytes(buf[4..8].try_into().expect("fixed size slice")),
            speakers: i32::from_le_bytes(buf[8..12].try_into().expect("fixed size slice")),
            frames: i32::from_le_bytes(buf[12..16].try_into().expect("fixed size slice")),
        })
    }

    /// Encode the audio metadata.
    pub fn encode<W: Write>(&self, writer: &mut W) -> Result<(), io::Error> {
        for field in [self.format, self.sample_rate, self.speakers, self.frames] {
            writer.write_all(&field.to_le_bytes())?;
        }
        Ok(())
    }

    /// Channel count for the known libobs speaker-layout values.
    pub fn channels(&self) -> Option<usize> {
        match self.speakers {
            1 => Some(1),
            2 => Some(2),
            3 => Some(3),
            4 => Some(4),
            5 => Some(5),
            6 => Some(6),
            8 => Some(8),
            _ => None,
        }
    }

    /// Bytes per sample for the known libobs audio-format values.
    pub fn bytes_per_sample(&self) -> Option<usize> {
        match self.format {
            1 | 5 => Some(1),
            2 | 6 => Some(2),
            3 | 4 | 7 | 8 => Some(4),
            _ => None,
        }
    }
}

/// A decoded packet. Payload bytes are owned and bounded by the decoder.
#[derive(Debug, Clone, PartialEq)]
pub enum Packet {
    /// JPEG video frame with color metadata.
    Jpeg {
        /// Common header.
        header: Header,
        /// Color metadata.
        image: ImageHeader,
        /// JPEG bitstream.
        payload: Vec<u8>,
    },
    /// Interleaved PCM audio frame.
    Wave {
        /// Common header.
        header: Header,
        /// Audio metadata.
        wave: WaveHeader,
        /// Interleaved PCM samples.
        payload: Vec<u8>,
    },
    /// Unknown or `ANJA` packet; payload is retained but not interpreted.
    Unknown {
        /// Common header.
        header: Header,
        /// Raw payload bytes.
        payload: Vec<u8>,
    },
}

impl Packet {
    /// Decode one packet, rejecting payloads larger than `max_payload`.
    pub fn decode<R: Read>(reader: &mut R, max_payload: usize) -> Result<Self, DecodeError> {
        let header = Header::decode(reader)?;
        if header.size < 0 {
            return Err(DecodeError::InvalidSize(header.size));
        }
        let size = header.size as usize;
        if size > max_payload {
            return Err(DecodeError::PayloadTooLarge(size, max_payload));
        }

        // Wire order is: common header, type-specific header, payload.
        match header.kind {
            MessageKind::Jpeg => {
                let image = ImageHeader::decode(reader)?;
                let mut payload = vec![0_u8; size];
                read_exact(reader, &mut payload)?;
                Ok(Self::Jpeg {
                    header,
                    image,
                    payload,
                })
            }
            MessageKind::Wave => {
                let wave = WaveHeader::decode(reader)?;
                let mut payload = vec![0_u8; size];
                read_exact(reader, &mut payload)?;
                Ok(Self::Wave {
                    header,
                    wave,
                    payload,
                })
            }
            MessageKind::Anja | MessageKind::Unknown(_) => {
                let mut payload = vec![0_u8; size];
                read_exact(reader, &mut payload)?;
                Ok(Self::Unknown { header, payload })
            }
        }
    }

    /// Encode this packet back to its exact wire representation.
    pub fn encode<W: Write>(&self, writer: &mut W) -> Result<(), io::Error> {
        match self {
            Self::Jpeg {
                header,
                image,
                payload,
            } => {
                header.encode(writer)?;
                image.encode(writer)?;
                writer.write_all(payload)
            }
            Self::Wave {
                header,
                wave,
                payload,
            } => {
                header.encode(writer)?;
                wave.encode(writer)?;
                writer.write_all(payload)
            }
            Self::Unknown { header, payload } => {
                header.encode(writer)?;
                writer.write_all(payload)
            }
        }
    }

    /// The common header of this packet.
    pub fn header(&self) -> &Header {
        match self {
            Self::Jpeg { header, .. }
            | Self::Wave { header, .. }
            | Self::Unknown { header, .. } => header,
        }
    }

    /// Total number of bytes this packet occupies on the wire.
    pub fn wire_len(&self) -> usize {
        HEADER_LEN + self.header().kind.extra_len() + self.header().size as usize
    }
}

fn read_exact<R: Read>(reader: &mut R, buf: &mut [u8]) -> Result<(), DecodeError> {
    match reader.read_exact(buf) {
        Ok(()) => Ok(()),
        Err(err) if err.kind() == ErrorKind::UnexpectedEof => Err(DecodeError::UnexpectedEof),
        Err(err) => Err(err.into()),
    }
}

fn read_f32<R: Read>(reader: &mut R) -> Result<f32, DecodeError> {
    let mut buf = [0_u8; 4];
    read_exact(reader, &mut buf)?;
    Ok(f32::from_le_bytes(buf))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Cursor;

    fn sample_packets() -> Vec<Packet> {
        vec![
            Packet::Jpeg {
                header: Header {
                    kind: MessageKind::Jpeg,
                    timestamp: 0x0102_0304_0506_0708,
                    size: 5,
                },
                image: ImageHeader {
                    color_matrix: [0.25; 16],
                    color_range_min: [0.0, 0.1, 0.2],
                    color_range_max: [0.8, 0.9, 1.0],
                },
                payload: vec![0xFF, 0xD8, 0x00, 0x01, 0xFF],
            },
            Packet::Wave {
                header: Header {
                    kind: MessageKind::Wave,
                    timestamp: 42,
                    size: 8,
                },
                wave: WaveHeader {
                    format: 4,
                    sample_rate: 48_000,
                    speakers: 2,
                    frames: 1,
                },
                payload: vec![0, 1, 2, 3, 4, 5, 6, 7],
            },
            Packet::Unknown {
                header: Header {
                    kind: MessageKind::from_bytes(*b"ANJA"),
                    timestamp: 7,
                    size: 2,
                },
                payload: vec![9, 9],
            },
        ]
    }

    #[test]
    fn header_layout_is_sixteen_bytes_little_endian() {
        let header = Header {
            kind: MessageKind::Jpeg,
            timestamp: 0x0102_0304_0506_0708,
            size: 0x1122_3344,
        };
        let mut bytes = Vec::new();
        header.encode(&mut bytes).unwrap();
        assert_eq!(bytes.len(), HEADER_LEN);
        assert_eq!(&bytes[0..4], b"JPEG");
        assert_eq!(&bytes[4..12], &0x0102_0304_0506_0708_u64.to_le_bytes());
        assert_eq!(&bytes[12..16], &0x1122_3344_i32.to_le_bytes());
    }

    #[test]
    fn image_header_is_eighty_eight_bytes() {
        let mut bytes = Vec::new();
        ImageHeader::default().encode(&mut bytes).unwrap();
        assert_eq!(bytes.len(), IMAGE_HEADER_LEN);
    }

    #[test]
    fn wave_header_is_sixteen_bytes() {
        let mut bytes = Vec::new();
        WaveHeader {
            format: 1,
            sample_rate: 2,
            speakers: 3,
            frames: 4,
        }
        .encode(&mut bytes)
        .unwrap();
        assert_eq!(bytes.len(), WAVE_HEADER_LEN);
    }

    #[test]
    fn decode_encode_round_trip_is_byte_identical() {
        for packet in sample_packets() {
            let mut first = Vec::new();
            packet.encode(&mut first).unwrap();
            let decoded = Packet::decode(&mut Cursor::new(&first), DEFAULT_MAX_PAYLOAD).unwrap();
            let mut second = Vec::new();
            decoded.encode(&mut second).unwrap();
            assert_eq!(first, second);
            assert_eq!(decoded.wire_len(), first.len());
            assert_eq!(decoded, packet);
        }
    }

    #[test]
    fn multiple_packets_can_be_read_from_one_stream() {
        let mut stream = Vec::new();
        for packet in sample_packets() {
            packet.encode(&mut stream).unwrap();
        }
        let mut cursor = Cursor::new(stream);
        let mut count = 0;
        while cursor.position() < cursor.get_ref().len() as u64 {
            let _ = Packet::decode(&mut cursor, DEFAULT_MAX_PAYLOAD).unwrap();
            count += 1;
        }
        assert_eq!(count, 3);
    }

    #[test]
    fn malformed_sizes_are_rejected() {
        let mut negative = Vec::new();
        Header {
            kind: MessageKind::Jpeg,
            timestamp: 0,
            size: -1,
        }
        .encode(&mut negative)
        .unwrap();
        assert!(matches!(
            Packet::decode(&mut Cursor::new(negative), DEFAULT_MAX_PAYLOAD),
            Err(DecodeError::InvalidSize(-1))
        ));

        let mut too_large = Vec::new();
        Header {
            kind: MessageKind::Wave,
            timestamp: 0,
            size: 100,
        }
        .encode(&mut too_large)
        .unwrap();
        assert!(matches!(
            Packet::decode(&mut Cursor::new(too_large), 16),
            Err(DecodeError::PayloadTooLarge(100, 16))
        ));
    }

    #[test]
    fn truncated_packets_return_eof_not_panic() {
        let mut packet = Vec::new();
        sample_packets()[0].encode(&mut packet).unwrap();
        for cut in 0..packet.len() {
            let _ = Packet::decode(&mut Cursor::new(&packet[..cut]), DEFAULT_MAX_PAYLOAD);
        }
    }

    #[test]
    fn arbitrary_bytes_never_panic() {
        // Deterministic pseudo-random fuzz-ish sweep using a linear
        // congruential generator; no external test dependency needed.
        let mut state: u64 = 0x1234_5678_9ABC_DEF0;
        for len in 0..2048 {
            let mut data = Vec::with_capacity(len);
            for _ in 0..len {
                state = state.wrapping_mul(6364136223846793005).wrapping_add(1);
                data.push((state >> 33) as u8);
            }
            let _ = Packet::decode(&mut Cursor::new(data), DEFAULT_MAX_PAYLOAD);
        }
    }

    #[test]
    fn full_range_detection_matches_upstream_rule() {
        let mut image = ImageHeader::default();
        assert!(!image.is_full_range());
        image.color_range_min = [0.0, 0.0, 0.0];
        image.color_range_max = [1.0, 1.0, 1.0];
        assert!(image.is_full_range());
    }

    #[test]
    fn speaker_and_format_helpers_cover_known_enums() {
        let wave = WaveHeader {
            format: 4,
            sample_rate: 48_000,
            speakers: 2,
            frames: 1024,
        };
        assert_eq!(wave.channels(), Some(2));
        assert_eq!(wave.bytes_per_sample(), Some(4));
        assert_eq!(
            WaveHeader {
                speakers: 8,
                ..wave
            }
            .channels(),
            Some(8)
        );
    }
}
