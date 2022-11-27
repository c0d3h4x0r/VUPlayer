#pragma once

#include "Decoder.h"

#include "bass.h"

#include <mutex>
#include <string>

namespace openmpt
{
	class module;
};

class DecoderOpenMPT : public Decoder
{
public:
	DecoderOpenMPT();

	// 'filename' - file name.
	// Throws a std::runtime_error exception if the file could not be loaded.
	DecoderOpenMPT( const std::wstring& filename );

	~DecoderOpenMPT() override;

	// Reads sample data.
	// 'buffer' - output buffer (floating point format scaled to +/-1.0f).
	// 'sampleCount' - number of samples to read.
	// Returns the number of samples read, or zero if the stream has ended.
	long Read( float* buffer, const long sampleCount ) override;

	// Returns the new position in seconds.
	float Seek( const float positionInSeconds ) override;

	std::set<std::wstring> GetSupportedFileExtensionsAsLowercaseStrings() const override;

protected:
	std::unique_ptr<openmpt::module> m_moduleFile;
};

