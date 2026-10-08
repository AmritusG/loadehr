#pragma once
// W2 Stage 5: the projector plays video FILES. One decoder per file on its own worker thread, frames queued ahead of the play
// position; the render thread only picks the best decoded frame (never waits: a late frame shows the last one).
//   H.264 / HEVC / ProRes (incl. 4444 alpha): AVAssetReader -> BGRA IOSurface-backed CVPixelBuffers, bound to GL zero-copy
//     (CGLTexImageIOSurface2D, GL_TEXTURE_RECTANGLE) by the renderer.
//   HAP / HAP Alpha / HAP Q / HAP Q Alpha / HAP Alpha-Only: the raw compressed samples (outputSettings nil), the HAP sections parsed
//     and Snappy-decompressed here (our own decoder, written from the HAP and Snappy format descriptions), uploaded as DXT / BC
//     textures: no CPU pixel decode.
//   DXV: not supported (the only reachable decoder ports FFmpeg's LGPL code).
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace mehr
{
struct VideoFrame
{
	double pts      = 0;   // seconds from the clip start
	int index       = 0;   // frame number
	int w = 0, h = 0;      // picture size
	void* pixelBuffer = nullptr; // CVPixelBufferRef (BGRA, IOSurface-backed), retained; null for HAP
	// HAP: block-compressed planes, rows top-first. format: 1 = DXT1 RGB, 5 = DXT5 RGBA, 6 = scaled YCoCg DXT5, 4 = BC4 alone
	int format = 0;
	std::vector< uint8_t > blocks, alpha; // alpha: Hap Q Alpha's BC4 plane
	uint64_t serial = 0;
	size_t Bytes() const;
	~VideoFrame();
};

class VideoPlayer
{
public:
	explicit VideoPlayer( const std::string& path );
	~VideoPlayer();
	bool Ready() const { return ready.load(); }
	bool Failed() const { return failed.load(); }
	std::string Info() const; // codec, size, fps, duration (or the error)
	double Duration() const { return duration; }
	double FrameDuration() const { return frameDur; }
	// render thread: the frame to show at `pos` seconds while playing forward (or backward); never blocks. `pos` is the play
	// position already wrapped / mirrored into 0..Duration by the caller; `loop` = the frames after the end are the first ones.
	std::shared_ptr< const VideoFrame > Frame( double pos, bool forward, bool loop );
	// stats (Debug Log): frames decoded, decode ms (worker), frames shown late
	uint64_t decoded = 0, late = 0;
	double decodeMs = 0;
	size_t cacheBytes = 0;

private:
	void Run();
	std::string path, info;
	std::atomic< bool > ready{ false }, failed{ false }, stop{ false };
	double duration = 0, frameDur = 1.0 / 30.0;
	int frames = 0;
	std::thread worker;
	mutable std::mutex mtx;
	std::condition_variable cv;
	// the request (render thread -> worker) and the decoded frames (worker -> render thread)
	double wantPos = 0;
	bool wantForward = true, wantLoop = true;
	uint64_t wantSerial = 0;
	std::map< int, std::shared_ptr< const VideoFrame > > cache;
	std::shared_ptr< const VideoFrame > last;
};

// Snappy decompression (format_description.txt): false on malformed input
bool SnappyDecompress( const uint8_t* in, size_t n, std::vector< uint8_t >& out );
// one HAP frame (a sample's bytes) -> its texture planes; false + error on anything unsupported
bool DecodeHapFrame( const uint8_t* data, size_t n, int w, int h, VideoFrame& out, std::string& error );
}
