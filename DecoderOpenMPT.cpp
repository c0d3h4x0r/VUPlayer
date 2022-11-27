#include "DecoderOpenMPT.h"

#include "VUPlayer.h"
#include "Utility.h"
#include "libopenmpt.hpp"

#include <fstream>

static constexpr DWORD sDefaultPanSeparation = 35;

// In seconds.
static constexpr float sFadeOutLength = 15.0f;

// TODO: Consider switching to OpenMPT for all tracker module formats it supports, because
// it's generally more accurate than BASS.
static const std::set<std::wstring> sMusicFileExtensions { L"mptm" };

#include <array>
#include <fstream>
#include <optional>

DecoderOpenMPT::DecoderOpenMPT() :
	Decoder()
{
}

DecoderOpenMPT::DecoderOpenMPT( const std::wstring& filename ) :
	Decoder(),
	m_moduleFile( nullptr ),
	m_FadeStartPosition( 0 ),
	m_FadeEndPosition( 0 ),
	m_CurrentPosition( 0 ),
	m_CurrentSilenceSamples( 0 )
{

	const auto fileExt = GetFileExtension( filename );
	if ( sMusicFileExtensions.end() != std::find( sMusicFileExtensions.begin(), sMusicFileExtensions.end(), fileExt ) ) {
		try {
			std::ifstream fileStream;
			fileStream.open( filename );
			m_moduleFile = std::make_unique<openmpt::module>( fileStream );
		}
		catch ( const std::runtime_error& ) {
		}
	}

	if ( m_moduleFile ) {
		BASS_CHANNELINFO info = {};
		BASS_ChannelGetInfo( m_moduleFile, &info );
		__super::SetSampleRate( static_cast<long>( info.freq ) );
		__super::SetBPS( static_cast<long>( info.origres ) );
		__super::SetChannels( static_cast<long>( info.chans ) );

		if ( const QWORD bytes = BASS_ChannelGetLength( m_moduleFile, BASS_POS_BYTE ); -1 != bytes ) {
			__super::SetDuration( static_cast<float>( BASS_ChannelBytes2Seconds( m_moduleFile, bytes ) ) );
		}

		float bitrate = 0;
		if ( TRUE == BASS_ChannelGetAttribute( m_moduleFile, BASS_ATTRIB_BITRATE, &bitrate ) ) {
			__super::SetBitrate( bitrate );
		}
	} else {
		throw std::runtime_error( "DecoderOpenMPT could not load file" );
	}
}

DecoderOpenMPT::~DecoderOpenMPT()
{
}

long DecoderOpenMPT::Read( float* buffer, const long sampleCount )
{
	long samplesRead = 0;
	const long channels = GetChannels();
	if ( ( channels > 0 ) && ( sampleCount > 0 ) ) {
		samplesRead = static_cast<long>( BASS_ChannelGetData( m_moduleFile, buffer, static_cast<DWORD>( sampleCount * channels * 4 ) ) );
		if ( samplesRead < 0 ) {
			samplesRead = 0;
		} else {
			samplesRead /= ( channels * 4 );
		}
	}

	if ( m_FadeStartPosition > 0 ) {
		if ( m_CurrentPosition > m_FadeEndPosition ) {
			samplesRead = 0;
		} else if ( m_CurrentPosition > m_FadeStartPosition ) {
			for ( long pos = 0; pos < samplesRead; pos++ ) {
				float scale = static_cast<float>( m_FadeEndPosition - m_CurrentPosition - pos ) / ( m_FadeEndPosition - m_FadeStartPosition );
				if ( ( scale < 0 ) || ( scale > 1.0f ) ) {
					scale = 0;
				}
				for ( long channel = 0; channel < channels; channel++ ) {
					buffer[ pos * channels + channel ] *= scale;
				}
			}
		}
		m_CurrentPosition += samplesRead;
	}

	return samplesRead;
}

float DecoderOpenMPT::Seek( const float position )
{
	DWORD flags = BASS_POS_BYTE;
	BASS_CHANNELINFO info = {};
	BASS_ChannelGetInfo( m_moduleFile, &info );
	if ( BASS_CTYPE_MUSIC_MOD & info.ctype ) {
		flags |= BASS_POS_DECODETO;
	}
	float seconds = 0;
	if ( QWORD bytes = BASS_ChannelSeconds2Bytes(m_moduleFile, position ); -1 != bytes ) {
		if ( BASS_ChannelSetPosition(m_moduleFile, bytes, flags ) ) {
			bytes = BASS_ChannelGetPosition(m_moduleFile, BASS_POS_BYTE );
			if ( -1 != bytes ) {
				seconds = static_cast<float>( BASS_ChannelBytes2Seconds(m_moduleFile, bytes ) );
				if ( seconds < 0 ) {
					seconds = 0;
				} else if ( m_FadeEndPosition > 0 ) {
					if ( const QWORD length = BASS_ChannelGetLength(m_moduleFile, BASS_POS_BYTE ); ( -1 != length ) && ( length > bytes ) ) {
						bytes = length - bytes;
						m_FadeStartPosition = bytes / ( info.chans * 4 );				
						m_FadeEndPosition = m_FadeStartPosition + BASS_ChannelSeconds2Bytes(m_moduleFile, sFadeOutLength ) / ( info.chans * 4 );
					}
				}
			}
		}
	}
	return seconds;
}

std::set<std::wstring> DecoderOpenMPT::GetSupportedFileExtensionsAsLowercaseStrings() const
{
	return sMusicFileExtensions;
}