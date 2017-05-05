#pragma once

#include "Playlist.h"

// Track information dialog.
class DlgTrackInfo
{
public:
	// 'instance' - module instance handle.
	// 'parent' - parent window handle.
	// 'library' - media library.
	// 'items' - item(s) for which to show track information.
	DlgTrackInfo( HINSTANCE instance, HWND parent, Library& library, const Playlist::ItemList& items );

	virtual ~DlgTrackInfo();

	// Called immediately before the dialog is shown.
	// 'hwnd' - dialog window handle.
	void OnInitDialog( HWND hwnd );

	// Called when painting is required for owner drawn controls.
	void OnOwnerDraw( DRAWITEMSTRUCT* drawItemStruct );

	// Called when the Choose Artwork control is chosen.
	// 'hwnd' - dialog window handle.
	void OnChooseArtwork( HWND hwnd );

	// Called when the Export Artwork control is chosen.
	// 'hwnd' - dialog window handle.
	void OnExportArtwork( HWND hwnd );

	// Called when the Clear Artwork control is chosen.
	// 'hwnd' - dialog window handle.
	void OnClearArtwork( HWND hwnd );

	// Displays the artwork context menu.
	// 'hwnd' - dialog window handle.
	// 'position' - menu position in screen coordinates.
	void OnArtworkContextMenu( HWND hwnd, const POINT& position );

	// Cuts/copies the artwork to the clipboard.
	// 'hwnd' - dialog window handle.
	// 'cut' - true to cut the artwork, false to copy the artwork.
	void OnCutCopyArtwork( HWND hwnd, const bool cut );

	// Pastes the artwork from the clipboard.
	// 'hwnd' - dialog window handle.
	void OnPasteArtwork( HWND hwnd );

	// Called when the Save button is chosen.
	// 'hwnd' - dialog window handle.
	void OnSave( HWND hwnd );

	// Called by the save thread when saving media information.
	// 'hwnd' - dialog window handle.
	void SaveThreadHandler( HWND hwnd );

	// Shows the progress bar when saving media information.
	// 'hwnd' - dialog window handle.
	void ShowProgress( HWND hwnd );

	// Called when the Cancel control is chosen when saving media information.
	void OnCancelSave();

private:
	// Returns the artwork bitmap for 'mediaInfo', or a null pointer if there is no bitmap.
	std::shared_ptr<Gdiplus::Bitmap> GetArtwork( const MediaInfo& mediaInfo );

	// Returns the artwork bitmap for 'image', or a null pointer if a bitmap could not be created.
	std::shared_ptr<Gdiplus::Bitmap> GetArtwork( const std::vector<BYTE>& image );

	// Enables the controls according to the current dialog state.
	// 'hwnd' - dialog window handle.
	void EnableControls( HWND hwnd );

	// Module instance handle.
	HINSTANCE m_hInst;

	// Media library.
	Library& m_Library;

	// Items for which to show track information.
	Playlist::ItemList m_Items;

	// Currently displayed artwork bitmap.
	std::shared_ptr<Gdiplus::Bitmap> m_Bitmap;

	// Information with which to initialise the dialog.
	MediaInfo m_InitialInfo;

	// Information which the dialog closes with.
	MediaInfo m_ClosingInfo;

	// Chosen artwork image.
	std::vector<BYTE> m_ChosenArtworkImage;

	// List of media (pairs of previous/updated information) to update before the dialog closes.
	std::list<std::pair<MediaInfo,MediaInfo>> m_UpdateInfo;

	// Save thread handle.
	HANDLE m_SaveThread;

	// Cancel save event handle.
	HANDLE m_CancelSaveEvent;
};
