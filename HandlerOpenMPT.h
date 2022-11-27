#pragma once
#include "Handler.h"

#include <string>

class HandlerOpenMPT : public Handler
{
public:
	HandlerOpenMPT();

	virtual ~HandlerOpenMPT();

	std::wstring GetDescription() const override;

	// This method should be renamed to GetSupportedFileExtensionsAsLowercaseStrings for clarity.
	std::set<std::wstring> GetSupportedFileExtensions() const override;

	// This method should be renamed to ReadTagsFromFileOrStream for clarity.
	bool GetTags( const std::wstring& filename, Tags& tags ) const override;

	// This method should be renamed to WriteTagsToFileOrStream for clarity.
	bool SetTags( const std::wstring& filename, const Tags& tags ) const override;

	// Returns nullptr on failure.
	Decoder::Ptr OpenDecoder( const std::wstring& filename ) const override;

	// Returns nullptr on failure.
	Encoder::Ptr OpenEncoder() const override;

	// This method should be renamed to CanDecode for clarity.
	bool IsDecoder() const override;

	// This method should be renamed to CanEncode for clarity.
	bool IsEncoder() const override;

	bool CanConfigureEncoder() const override;

	// This method should be renamed to DisplayEncoderConfigurationDialog for clarity.
	// Returns true if the encoder was successfully configured by the user.
	bool ConfigureEncoder( const HINSTANCE appInstance, const HWND appWindow, std::string& settings ) const override;

	// This function should be renamed to OnSettingsChanged for clarity.
	void SettingsChanged( Settings& settings ) override;
};
