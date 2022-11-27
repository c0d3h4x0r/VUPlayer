#include "HandlerOpenMPT.h"

#include "DecoderOpenMPT.h"
#include "libopenmpt_version.h"
#include "libopenmpt.hpp"

#include <fstream>
#include <sstream>

HandlerOpenMPT::HandlerOpenMPT() :
	Handler()
{
}

HandlerOpenMPT::~HandlerOpenMPT()
{
}

std::wstring HandlerOpenMPT::GetDescription() const
{
	std::wstringstream description;
	description << L"OpenMPT " << OPENMPT_API_VERSION_STRING;
	return description.str();
}

std::set<std::wstring> HandlerOpenMPT::GetSupportedFileExtensions() const
{
	return DecoderOpenMPT().GetSupportedFileExtensionsAsLowercaseStrings();
}

bool HandlerOpenMPT::GetTags( const std::wstring& filename, Tags& tags ) const
{
	bool success = false;
	tags.clear();
	try {
		std::ifstream fileStream;
		fileStream.open( filename );
		openmpt::module moduleFile( fileStream );
		const auto availableKeys = moduleFile.get_metadata_keys();
		for ( auto &key : availableKeys ) {
			Tag tag;
			if ( key.compare( "artist" ) == 0 ) {
				tag = Tag::Artist;
			}
			else if ( key.compare( "title" ) == 0 ) {
				tag = Tag::Title;
			}
			else if ( key.compare( "message" ) == 0 ) {
				tag = Tag::Comment;
			}
			else {
				continue;
			}
			const std::string value = moduleFile.get_metadata( key );
			tags.insert( Tags::value_type( tag, value ) );
		}
		
		success = true;
	} catch ( const std::runtime_error& ) {
	}
	return success;
}

bool HandlerOpenMPT::SetTags( const std::wstring& filename, const Tags& tags ) const
{
	return false;
}

Decoder::Ptr HandlerOpenMPT::OpenDecoder( const std::wstring& filename ) const
{
	DecoderOpenMPT* decoder = nullptr;
	try {
		decoder = new DecoderOpenMPT( filename );
	} catch ( const std::runtime_error& ) {
	}
	const Decoder::Ptr stream( decoder );
	return stream;
}

Encoder::Ptr HandlerOpenMPT::OpenEncoder() const
{
	return nullptr;
}

bool HandlerOpenMPT::IsDecoder() const
{
	return true;
}

bool HandlerOpenMPT::IsEncoder() const
{
	return false;
}

bool HandlerOpenMPT::CanConfigureEncoder() const
{
	return false;
}

bool HandlerOpenMPT::ConfigureEncoder( const HINSTANCE /*appInstance*/, const HWND /*appWindow*/, std::string& /*settings*/ ) const
{
	return false;
}

void HandlerOpenMPT::SettingsChanged( Settings& settings )
{
}
