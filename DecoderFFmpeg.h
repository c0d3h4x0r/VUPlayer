#pragma once

#include "Decoder.h"

#include <string>

struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;

class DecoderFFmpeg : public Decoder
{
public:
	// 'filename' - file name.
	// Throws a std::runtime_error exception if the file could not be loaded.
	DecoderFFmpeg( const std::wstring& filename );

	~DecoderFFmpeg() override;

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	long Read( float* buffer, const long sampleCount ) override;

	// Seeks to a 'position' in the stream, in seconds.
	// Returns the new position in seconds.
	float Seek( const float position ) override;

private:
	// Deccodes the next chunk of data into the sample buffer, returning whether any data was decoded.
	bool Decode();

	// Converts data from the 'frame' into the sample 'buffer'.
	static void ConvertSampleData( const AVFrame* frame, std::vector<float>& buffer ); 

	// FFmpeg format context.
	AVFormatContext* m_FormatContext = nullptr;

	// FFmpeg decoder context.
	AVCodecContext* m_DecoderContext = nullptr;

	// FFmpeg current packet.
	AVPacket* m_Packet = nullptr;

	// FFmpeg current frame.
	AVFrame* m_Frame = nullptr;

	// Index of the 'best' stream.
	int m_StreamIndex = 0;

	// Interleaved sample buffer.
	std::vector<float> m_Buffer;

	// Current buffer position.
	size_t m_BufferPosition = 0;
};
