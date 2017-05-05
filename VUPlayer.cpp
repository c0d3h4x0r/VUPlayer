#include "VUPlayer.h"

#include "DlgTrackInfo.h"
#include "DlgOptions.h"
#include "Utility.h"

#include <fstream>

// Main application instance.
VUPlayer* VUPlayer::s_VUPlayer = nullptr;

// Timer ID.
static const UINT_PTR s_TimerID = 42;

// Timer millisecond interval.
static const UINT s_TimerInterval = 100;

// Minimum application width.
static const int s_MinAppWidth = 1024;

// Minimum application height.
static const int s_MinAppHeight = 768;

// Skip duration, in seconds.
static const float s_SkipDuration = 5.0f;

// Skip repeat limit interval, in seconds.
static const float s_SkipLimitInterval = 0.1f;

// Online documentation location.
static const wchar_t s_OnlineDocs[] = L"https://github.com/jfchapman/vuplayer/wiki";

VUPlayer* VUPlayer::Get( const HINSTANCE instance, const HWND hwnd )
{
	if ( ( nullptr == s_VUPlayer ) && ( NULL != instance ) && ( NULL != hwnd ) ) {
		s_VUPlayer = new VUPlayer( instance, hwnd );
	}
	return s_VUPlayer;
}

void VUPlayer::Release()
{
	delete s_VUPlayer;
	s_VUPlayer = nullptr;
}

VUPlayer::VUPlayer( const HINSTANCE instance, const HWND hwnd ) :
	m_hInst( instance ),
	m_hWnd( hwnd ),
	m_Handlers(),
	m_Database( DocumentsFolder() + L"VUPlayer.db" ),
	m_Library( m_Database, m_Handlers ),
	m_Settings( m_Database, m_Library ),
	m_Output( m_hWnd, m_Handlers, m_Settings, m_Settings.GetVolume() ),
	m_ReplayGain( m_Library, m_Handlers ),
	m_Rebar( m_hInst, m_hWnd ),
	m_Status( m_hInst, m_hWnd ),
	m_Tree( m_hInst, m_hWnd, m_Library, m_Settings ),
	m_Visual( m_hInst, m_hWnd, m_Settings, m_Output, m_Library ),
	m_List( m_hInst, m_hWnd, m_Settings, m_Output ),
	m_SeekControl( m_hInst, m_Rebar.GetWindowHandle(), m_Output ),
	m_VolumeControl( m_hInst, m_Rebar.GetWindowHandle(), m_Output ),
	m_ToolbarCrossfade( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarFile( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarFlow( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarInfo( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarOptions( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarPlayback( m_hInst, m_Rebar.GetWindowHandle() ),
	m_ToolbarPlaylist( m_hInst, m_Rebar.GetWindowHandle() ),
	m_Counter( m_hInst, m_Rebar.GetWindowHandle(), m_Settings, m_Output, m_ToolbarPlayback.GetHeight() - 1 ),
	m_Splitter( m_hInst, m_hWnd, m_Rebar.GetWindowHandle(), m_Status.GetWindowHandle(), m_Tree.GetWindowHandle(), m_Visual.GetWindowHandle(), m_List.GetWindowHandle(), m_Settings ),
	m_Tray( m_hInst, m_hWnd, m_Library, m_Settings, m_Output, m_Tree, m_List ),
	m_CurrentOutput(),
	m_CustomColours(),
	m_Hotkeys( hwnd, m_Settings ),
	m_LastSkipCount( {} )
{
	ReadWindowSettings();

	m_Rebar.AddControl( m_SeekControl.GetWindowHandle() );
	m_Rebar.AddControl( m_Counter.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarFile.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarPlaylist.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarOptions.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarInfo.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarCrossfade.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarFlow.GetWindowHandle() );
	m_Rebar.AddControl( m_ToolbarPlayback.GetWindowHandle() );

	WndRebar::IconCallback iconCallback( [ &output = m_Output ]() -> UINT {
		const UINT iconID = output.GetMuted() ? IDI_VOLUME_MUTE : IDI_VOLUME;
		return iconID;
	} );
	WndRebar::ClickCallback clickCallback( [ &output = m_Output ]() { output.ToggleMuted(); } );
	m_Rebar.AddControl( m_VolumeControl.GetWindowHandle(), { IDI_VOLUME, IDI_VOLUME_MUTE }, iconCallback, clickCallback );

	const int seekPosition = m_Rebar.GetPosition( m_SeekControl.GetWindowHandle() );
	if ( 0 != seekPosition ) {
		m_Rebar.MoveToStart( seekPosition );
	}

	m_Splitter.Resize();

	for ( auto& iter : m_CustomColours ) {
		iter = RGB( 0xff /*red*/, 0xff /*green*/, 0xff /*blue*/ );
	}

	RedrawWindow( m_Rebar.GetWindowHandle(), NULL /*rect*/, NULL /*rgn*/, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW );

	m_Tree.Initialise();
	Playlist::Ptr initialPlaylist = m_Tree.GetSelectedPlaylist();
	m_List.SetPlaylist( initialPlaylist, false /*initSelection*/ );
	int initialSelectedIndex = m_Settings.GetStartupPlaylistSelection();
	if ( initialSelectedIndex < 0 ) {
		initialSelectedIndex = 0;
	}
	if ( initialPlaylist ) {
		const Playlist::ItemList items = initialPlaylist->GetItems();
		if ( initialSelectedIndex >= static_cast<int>( items.size() ) ) {
			initialSelectedIndex = 0;
		}
		auto itemIter = items.begin();
		if ( items.end() != itemIter ) {
			std::advance( itemIter, initialSelectedIndex );
			m_List.EnsureVisible( *itemIter );
		}
	}
	m_Status.SetPlaylist( m_List.GetPlaylist() );

	OnListSelectionChanged();

	SetTimer( m_hWnd, s_TimerID, s_TimerInterval, NULL /*timerProc*/ );
}

VUPlayer::~VUPlayer()
{
}

void VUPlayer::ReadWindowSettings()
{
	int x = -1;
	int y = -1;
	int width = -1;
	int height = -1;
	bool maximised = false;
	bool minimised = false;
	m_Settings.GetStartupPosition( x, y, width, height, maximised, minimised );
	const int screenWidth = GetSystemMetrics( SM_CXSCREEN );
	const int screenHeight = GetSystemMetrics( SM_CYSCREEN );
	if ( ( x > 0 ) && ( y > 0 ) && ( width > 0 ) && ( height > 0 ) && ( ( x + width ) <= screenWidth ) && ( ( y + height ) <= screenHeight ) ) {
		MoveWindow( m_hWnd, x, y, width, height, FALSE /*repaint*/ );
	}

	bool systrayEnabled = false;
	Settings::SystrayCommand systraySingleClick = Settings::SystrayCommand::None;
	Settings::SystrayCommand systrayDoubleClick = Settings::SystrayCommand::None;
	m_Settings.GetSystraySettings( systrayEnabled, systraySingleClick, systrayDoubleClick );
	if ( minimised ) {
		ShowWindow( m_hWnd, SW_SHOWMINIMIZED );
	} else if ( maximised ) {
		ShowWindow( m_hWnd, SW_SHOWMAXIMIZED );
	} else {
		ShowWindow( m_hWnd, SW_SHOW );
	}
	if ( systrayEnabled ) {
		m_Tray.Show();
	}
	UpdateWindow( m_hWnd );

	// Force the status bar to update.
	RECT rect = {};
	GetClientRect( m_hWnd, &rect );
	LPARAM lParam = MAKELPARAM( static_cast<WORD>( rect.right - rect.left ), static_cast<WORD>( rect.bottom - rect.top ) );
	SendMessage( m_Status.GetWindowHandle(), WM_SIZE, 0 /*wParam*/, lParam );
	RedrawWindow( m_Status.GetWindowHandle(), NULL, NULL, RDW_UPDATENOW );
}

void VUPlayer::WriteWindowSettings()
{
	int x = -1;
	int y = -1;
	int width = -1;
	int height = -1;
	bool maximised = false;
	bool minimised = false;
	m_Settings.GetStartupPosition( x, y, width, height, maximised, minimised );
	maximised = ( FALSE != IsZoomed( m_hWnd ) );
	minimised = ( FALSE != IsIconic( m_hWnd ) );
	if ( !maximised && !minimised ) {
		RECT rect = {};
		GetWindowRect( m_hWnd, &rect );
		x = rect.left;
		y = rect.top;
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;
	}
	m_Settings.SetStartupPosition( x, y, width, height, maximised, minimised );
}

std::wstring VUPlayer::DocumentsFolder()
{
	std::wstring folder;
	PWSTR path = nullptr;
	HRESULT hr = SHGetKnownFolderPath( FOLDERID_Documents, KF_FLAG_DEFAULT, NULL /*token*/, &path );
	if ( SUCCEEDED( hr ) ) {
		folder = path;
		CoTaskMemFree( path );
		folder += L"\\VUPlayer\\";
		CreateDirectory( folder.c_str(), NULL /*attributes*/ ); 
	}
	return folder;
}

void VUPlayer::OnSize( WPARAM wParam, LPARAM lParam )
{
	if ( SIZE_MINIMIZED != wParam ) {
		SendMessage( m_Rebar.GetWindowHandle(), WM_SIZE, wParam, lParam );
		RedrawWindow( m_Rebar.GetWindowHandle(), NULL, NULL, RDW_UPDATENOW );
		SendMessage( m_Status.GetWindowHandle(), WM_SIZE, wParam, lParam );
		RedrawWindow( m_Status.GetWindowHandle(), NULL, NULL, RDW_UPDATENOW );
		SendMessage( m_Splitter.GetWindowHandle(), WM_SIZE, wParam, lParam );
	}
}

bool VUPlayer::OnNotify( WPARAM wParam, LPARAM lParam, LRESULT& result )
{
	bool handled = false;
	LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>( lParam );
	if ( nullptr != nmhdr ) {
		switch ( nmhdr->code ) {
			case TVN_BEGINLABELEDIT : {
				result = m_Tree.OnBeginLabelEdit( wParam, lParam );
				handled = true;
				break;
			}
			case TVN_ENDLABELEDIT : {
				m_Tree.OnEndLabelEdit( wParam, lParam );
				break;
			}
			case TVN_SELCHANGED : {
				LPNMTREEVIEW nmTreeView = reinterpret_cast<LPNMTREEVIEW>( lParam );
				if ( ( nullptr != nmTreeView ) && ( nullptr != nmTreeView->itemNew.hItem ) ) {
					Playlist::Ptr playlist = m_Tree.GetPlaylist( nmTreeView->itemNew.hItem );
					if ( playlist ) {
						m_List.SetPlaylist( playlist );
						m_Status.SetPlaylist( playlist );
					}
				}
				break;
			}
			case NM_RCLICK : {
				if ( m_Tree.GetWindowHandle() == nmhdr->hwndFrom ) {
					POINT pt = {};
					GetCursorPos( &pt );
					m_Tree.OnContextMenu( pt );
				}
				break;
			}
			case HDN_ITEMCLICK : {
				LPNMHEADER hdr = reinterpret_cast<LPNMHEADER>( lParam );
				if ( nullptr != hdr ) {
					HDITEM headerItem = {};
					headerItem.mask = HDI_LPARAM;
					if ( TRUE == Header_GetItem( hdr->hdr.hwndFrom, hdr->iItem, &headerItem ) ) {
						const Playlist::Column column = static_cast<Playlist::Column>( headerItem.lParam );
						m_List.SortPlaylist( column );
					}
				}
				break;
			}
			case NM_CUSTOMDRAW : {
				LPNMHDR hdr = reinterpret_cast<LPNMHDR>( lParam );
				if ( nullptr != hdr ) {
					if ( hdr->hwndFrom == m_List.GetWindowHandle() ) {
						LPNMLVCUSTOMDRAW nmlvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>( lParam );
						handled = true;
						switch ( nmlvcd->nmcd.dwDrawStage ) {
							case CDDS_PREPAINT : {
								result = CDRF_NOTIFYITEMDRAW;
								break;
							}
							case CDDS_ITEMPREPAINT : {
								const COLORREF backColour = ListView_GetBkColor( hdr->hwndFrom );
								const COLORREF highlightColour = m_List.GetHighlightColour();
								const bool selected = ( LVIS_SELECTED == ListView_GetItemState( hdr->hwndFrom, nmlvcd->nmcd.dwItemSpec, LVIS_SELECTED ) );
								if ( selected ) {
									nmlvcd->clrText = backColour;
									nmlvcd->clrTextBk = highlightColour;
								} else if ( m_CurrentOutput.PlaylistItem.ID == nmlvcd->nmcd.lItemlParam ) {
									nmlvcd->clrText = highlightColour;
									nmlvcd->clrTextBk = backColour;
								}

								// Mask out selection state so that the custom highlight colour takes effect.
								nmlvcd->nmcd.uItemState &= ~CDIS_SELECTED;

								// Mask out focus state so that a focus rectangle is not drawn.
								if ( nmlvcd->nmcd.uItemState & CDIS_FOCUS ) {
									nmlvcd->nmcd.uItemState ^= CDIS_FOCUS;
								}

								result = CDRF_DODEFAULT;
								break;
							}
							default : {
								result = CDRF_DODEFAULT;
								break;
							}
						}
					} else if ( hdr->hwndFrom == m_Tree.GetWindowHandle() ) {
						LPNMTVCUSTOMDRAW nmtvcd = reinterpret_cast<LPNMTVCUSTOMDRAW>( lParam );
						handled = true;
						switch ( nmtvcd->nmcd.dwDrawStage ) {
							case CDDS_PREPAINT : {
								result = CDRF_NOTIFYITEMDRAW;
								break;
							}
							case CDDS_ITEMPREPAINT : {
								const COLORREF backColour = TreeView_GetBkColor( hdr->hwndFrom );
								const COLORREF highlightColour = m_Tree.GetHighlightColour();
								const HTREEITEM treeItem = reinterpret_cast<HTREEITEM>( nmtvcd->nmcd.dwItemSpec );
								const bool selected = ( TreeView_GetSelection( m_Tree.GetWindowHandle() ) == treeItem );
								if ( selected ) {
									nmtvcd->clrText = backColour;
									nmtvcd->clrTextBk = highlightColour;
								}

								// Mask out selection state so that the custom highlight colour takes effect.
								nmtvcd->nmcd.uItemState &= ~CDIS_SELECTED;

								// Mask out focus state so that a focus rectangle is not drawn.
								if ( nmtvcd->nmcd.uItemState & CDIS_FOCUS ) {
									nmtvcd->nmcd.uItemState ^= CDIS_FOCUS;
								}

								result = CDRF_DODEFAULT;
								break;
							}
							default : {
								result = CDRF_DODEFAULT;
								break;
							}
						}
					}
				}
				break;
			}
			case LVN_BEGINLABELEDIT : {
				LPNMHDR hdr = reinterpret_cast<LPNMHDR>( lParam );
				if ( ( nullptr != hdr ) && ( hdr->hwndFrom == m_List.GetWindowHandle() ) ) {
					NMLVDISPINFO* dispInfo = reinterpret_cast<NMLVDISPINFO*>( lParam );
					result = m_List.OnBeginLabelEdit( dispInfo->item );
					handled = true;
				}
				break;
			}
			case LVN_ENDLABELEDIT : {
				LPNMHDR hdr = reinterpret_cast<LPNMHDR>( lParam );
				if ( ( nullptr != hdr ) && ( hdr->hwndFrom == m_List.GetWindowHandle() ) ) {
					NMLVDISPINFO* dispInfo = reinterpret_cast<NMLVDISPINFO*>( lParam );
					m_List.OnEndLabelEdit( dispInfo->item );
					result = FALSE;
					handled = true;
				}
				break;
			}
			case LVN_BEGINDRAG : {
				LPNMLISTVIEW nmListView = reinterpret_cast<LPNMLISTVIEW>( lParam );
				if ( nullptr != nmListView ) {
					m_List.OnBeginDrag( nmListView->iItem );
				}
				break;
			}
			case LVN_ITEMCHANGED : {
				LPNMLISTVIEW nmListView = reinterpret_cast<LPNMLISTVIEW>( lParam );
				if ( nullptr != nmListView ) {
					if ( nmListView->uNewState & LVIS_FOCUSED ) {
						OnListSelectionChanged();
					}
				}
				break;				
			}
			case HDN_BEGINTRACK : {
				LPNMHEADER hdr = reinterpret_cast<LPNMHEADER>( lParam );
				if ( ( nullptr != hdr ) && ( 0 == hdr->iItem ) ) {
					// Prevent tracking of dummy column.
					result = TRUE;
					handled = true;
				}
				break;
			}
			case HDN_ENDDRAG : {
				LPNMHEADER hdr = reinterpret_cast<LPNMHEADER>( lParam );
				if ( nullptr != hdr ) {
					m_List.OnEndDragColumn();
				}
				break;
			}
			default : {
				break;
			}
		}
	}
	return handled;
}

bool VUPlayer::OnTimer( const UINT_PTR timerID )
{
	bool handled = ( s_TimerID == timerID );
	if ( handled ) {
		Output::Item currentPlaying = m_Output.GetCurrentPlaying();
		if ( m_CurrentOutput.PlaylistItem.ID != currentPlaying.PlaylistItem.ID ) {
			OnOutputChanged( currentPlaying );
		}
		m_CurrentOutput = currentPlaying;

		const Playlist::Ptr& currentPlaylist = m_List.GetPlaylist();
		const Playlist::Item currentSelectedPlaylistItem = m_List.GetCurrentSelectedItem();

		m_SeekControl.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarFile.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarPlaylist.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarOptions.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarInfo.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarCrossfade.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarFlow.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_ToolbarPlayback.Update( m_Output, currentPlaylist, currentSelectedPlaylistItem );
		m_Rebar.Update();
		m_Counter.Refresh();
		m_Status.Update( m_ReplayGain );
		m_Tray.Update( m_CurrentOutput );
	}
	if ( !handled ) {
		handled = ( TIMER_SYSTRAY == timerID );
		if ( handled ) {
			KillTimer( m_hWnd, TIMER_SYSTRAY );
			m_Tray.OnSingleClick();
		}
	}
	return handled;
}

void VUPlayer::OnOutputChanged( const Output::Item& item )
{
	if ( ID_VISUAL_ARTWORK == m_Visual.GetCurrentVisualID() ) {
		m_Splitter.Resize();
		m_Visual.DoRender();
	}
	RedrawWindow( m_List.GetWindowHandle(), NULL /*rect*/, NULL /*region*/, RDW_INVALIDATE | RDW_NOERASE );

	std::wstring titlebarText;
	if ( item.PlaylistItem.ID > 0 ) {
		const std::wstring title = item.PlaylistItem.Info.GetTitle( true /*filenameAsTitle*/ );
		if ( !title.empty() ) {
			titlebarText = item.PlaylistItem.Info.GetArtist();
			if ( !titlebarText.empty() ) {
				titlebarText += L" - ";
			}
			titlebarText += title;
		} 
	}
	if ( titlebarText.empty() ) {
		const int buffersize = 16;
		WCHAR buffer[ buffersize ] = {};
		LoadString( m_hInst, IDS_APP_TITLE, buffer, buffersize );
		titlebarText = buffer;
	}
	SetWindowText( m_hWnd, titlebarText.c_str() );
}

void VUPlayer::OnMinMaxInfo( LPMINMAXINFO minMaxInfo )
{
	if ( nullptr != minMaxInfo ) {
		const float dpiScaling = GetDPIScaling();
		minMaxInfo->ptMinTrackSize = { static_cast<LONG>( s_MinAppWidth * dpiScaling ), static_cast<LONG>( s_MinAppHeight * dpiScaling ) };
	}
}

void VUPlayer::OnPlaylistItemAdded( Playlist* playlist, const Playlist::Item& item, const int position )
{
	m_List.OnFileAdded( playlist, item, position );
	m_Status.Update( playlist );
}

void VUPlayer::OnPlaylistItemRemoved( Playlist* playlist, const Playlist::Item& item )
{
	m_List.OnFileRemoved( playlist, item );
	m_Status.Update( playlist );
}

void VUPlayer::OnDestroy()
{
	int playlistSelectedIndex = -1;
	const Output::Item currentPlaying = m_Output.GetCurrentPlaying();
	Playlist::Ptr playlist;
	if ( currentPlaying.PlaylistItem.ID > 0 ) {
		playlist = m_Output.GetPlaylist();
		if ( playlist ) {
			Playlist::ItemList items = playlist->GetItems();
			int playlistIndex = 0;
			for ( auto item = items.begin(); item != items.end(); item++, playlistIndex++ ) {
				if ( item->ID == currentPlaying.PlaylistItem.ID ) {
					playlistSelectedIndex = playlistIndex;
					break;
				}
			}
		}
	} else {
		playlist = m_List.GetPlaylist();
		if ( playlist ) {
			playlistSelectedIndex = m_List.GetCurrentSelectedIndex();
		}
	}
	m_Tree.SaveStartupPlaylist( playlist );
	m_Settings.SetStartupPlaylistSelection( playlistSelectedIndex );

	m_Output.Stop();
	m_Settings.SetVolume( m_Output.GetVolume() );
	m_Settings.SetPlaybackSettings( m_Output.GetRandomPlay(), m_Output.GetRepeatTrack(), m_Output.GetRepeatPlaylist(), m_Output.GetCrossfade() );

	m_ReplayGain.Stop();

	WriteWindowSettings();
}

void VUPlayer::OnCommand( const int commandID )
{
	switch ( commandID ) {
		case ID_VISUAL_VUMETER_STEREO :
		case ID_VISUAL_VUMETER_MONO :
		case ID_VISUAL_OSCILLOSCOPE :
		case ID_VISUAL_SPECTRUMANALYSER :
		case ID_VISUAL_ARTWORK :
		case ID_VISUAL_NONE : {
			m_Visual.SelectVisual( commandID );
			m_Splitter.Resize();
			break;
		}
		case ID_OSCILLOSCOPE_COLOUR : {
			m_Visual.OnOscilloscopeColour();
			break;
		}
		case ID_OSCILLOSCOPE_BACKGROUNDCOLOUR : {
			m_Visual.OnOscilloscopeBackground();
			break;
		}
		case ID_OSCILLOSCOPE_WEIGHT_FINE : 
		case ID_OSCILLOSCOPE_WEIGHT_NORMAL :
		case ID_OSCILLOSCOPE_WEIGHT_BOLD : {
			m_Visual.OnOscilloscopeWeight( commandID );
			break;
		}
		case ID_SPECTRUMANALYSER_BASECOLOUR :
		case ID_SPECTRUMANALYSER_PEAKCOLOUR : 
		case ID_SPECTRUMANALYSER_BACKGROUNDCOLOUR : {
			m_Visual.OnSpectrumAnalyserColour( commandID );
			break;
		}
		case ID_VUMETER_SLOWDECAY :
		case ID_VUMETER_NORMALDECAY :
		case ID_VUMETER_FASTDECAY : {
			m_Visual.OnVUMeterDecay( commandID );
			break;
		}
		case ID_CONTROL_PLAY :
		case ID_TRAY_PLAY : {
			const Output::State state = m_Output.GetState();
			switch ( state ) {
				case Output::State::Paused :
				case Output::State::Playing : {
					m_Output.Pause();
					break;
				}
				default : {
					const Playlist::Ptr playlist = m_List.GetPlaylist();
					if ( playlist ) {
						const Playlist::Item item = m_List.GetCurrentSelectedItem();
						if ( 0 != item.ID ) {
							m_Output.SetPlaylist( playlist );
							m_Output.Play( item.ID );
						}
					}
					break;
				}
			}
			break;
		}
		case ID_CONTROL_STOP : {
			m_Output.Stop();
			break;
		}
		case ID_CONTROL_PREVIOUSTRACK : {
			const Output::State state = m_Output.GetState();
			switch ( state ) {
				case Output::State::Paused :
				case Output::State::Playing : {
					m_Output.Previous();
					const Playlist::Item item = m_Output.GetCurrentPlaying().PlaylistItem;
					m_List.EnsureVisible( item );
					break;
				}
				default : {
					m_List.SelectPreviousItem();
					break;
				}
			}
			break;
		}
		case ID_CONTROL_NEXTTRACK : {
			const Output::State state = m_Output.GetState();
			switch ( state ) {
				case Output::State::Paused :
				case Output::State::Playing : {
					m_Output.Next();
					const Playlist::Item item = m_Output.GetCurrentPlaying().PlaylistItem;
					m_List.EnsureVisible( item );
					break;
				}
				default : {
					m_List.SelectNextItem();
					break;
				}
			}
			break;
		}
		case ID_CONTROL_STOPTRACKEND : {
			m_Output.ToggleStopAtTrackEnd();
			break;
		}
		case ID_CONTROL_FADEOUT : {
			m_Output.ToggleFadeOut();
			break;
		}
		case ID_CONTROL_FADETONEXT : {
			m_Output.ToggleFadeToNext();
			break;
		}
		case ID_CONTROL_MUTE : {
			m_Output.ToggleMuted();
			break;
		}
		case ID_CONTROL_VOLUMEDOWN : {
			float volume = m_Output.GetVolume() - 0.01f;
			if ( volume < 0.0f ) {
				volume = 0.0f;
			}
			m_Output.SetVolume( volume );
			m_VolumeControl.Update();
			break;
		}
		case ID_CONTROL_VOLUMEUP : {
			float volume = m_Output.GetVolume() + 0.01f;
			if ( volume > 1.0f ) {
				volume = 1.0f;
			}
			m_Output.SetVolume( volume );
			m_VolumeControl.Update();
			break;
		}
		case ID_VOLUME_100 :
		case ID_VOLUME_90 :
		case ID_VOLUME_80 :
		case ID_VOLUME_70 :
		case ID_VOLUME_60 :
		case ID_VOLUME_50 :
		case ID_VOLUME_40 :
		case ID_VOLUME_30 :
		case ID_VOLUME_20 :
		case ID_VOLUME_10 :
		case ID_VOLUME_0 : {
			m_Output.SetVolume( m_Tray.GetMenuVolume( commandID ) );
			m_VolumeControl.Update();
			break;
		}

		case ID_CONTROL_SKIPBACKWARDS : {
			if ( AllowSkip() ) {
				const Output::State state = m_Output.GetState();
				if ( ( Output::State::Playing == state ) || ( Output::State::Paused == state ) ) {
					const Output::Item item = m_Output.GetCurrentPlaying();
					float position = item.Position - s_SkipDuration;
					if ( position < 0 ) {
						m_Output.Previous( true /*forcePrevious*/, -s_SkipDuration );
					} else {
						m_Output.Play( item.PlaylistItem.ID, position );
					}
				}
				ResetLastSkipCount();
			}
			break;
		}
		case ID_CONTROL_SKIPFORWARDS : {
			if ( AllowSkip() ) {
				const Output::State state = m_Output.GetState();
				if ( ( Output::State::Playing == state ) || ( Output::State::Paused == state ) ) {
					const Output::Item item = m_Output.GetCurrentPlaying();
					float position = item.Position + s_SkipDuration;
					if ( position > item.PlaylistItem.Info.GetDuration() ) {
						m_Output.Next();
					} else {
						m_Output.Play( item.PlaylistItem.ID, position );
					}
				}
				ResetLastSkipCount();
			}
			break;
		}
		case ID_CONTROL_CROSSFADE : {
			m_Output.SetCrossfade( !m_Output.GetCrossfade() );
			break;
		}
		case ID_CONTROL_RANDOMPLAY : {
			m_Output.SetRandomPlay( !m_Output.GetRandomPlay() );
			break;
		}
		case ID_CONTROL_REPEATTRACK : {
			m_Output.SetRepeatTrack( !m_Output.GetRepeatTrack() );
			break;
		}
		case ID_CONTROL_REPEATPLAYLIST : {
			m_Output.SetRepeatPlaylist( !m_Output.GetRepeatPlaylist() );
			break;
		}
		case ID_FILE_CALCULATEREPLAYGAIN : {
			OnCalculateReplayGain();
			break;
		}
		case ID_VIEW_TRACKINFORMATION : {
			OnTrackInformation();
			break;
		}
		case ID_FILE_NEWPLAYLIST : {
			m_Tree.NewPlaylist();
			SetFocus( m_Tree.GetWindowHandle() );
			break;
		}
		case ID_FILE_DELETEPLAYLIST : {
			m_Tree.DeleteSelectedPlaylist();
			SetFocus( m_Tree.GetWindowHandle() );
			break;
		}
		case ID_FILE_IMPORTPLAYLIST : {
			m_Tree.ImportPlaylist();
			SetFocus( m_Tree.GetWindowHandle() );
			break;
		}
		case ID_FILE_EXPORTPLAYLIST : {
			m_Tree.ExportSelectedPlaylist();
			SetFocus( m_Tree.GetWindowHandle() );
			break;
		}
		case ID_FILE_PLAYLISTADDFOLDER : {
			m_List.OnCommandAddFolder();
			SetFocus( m_List.GetWindowHandle() );
			break;
		}
		case ID_FILE_PLAYLISTADDFILES : {
			m_List.OnCommandAddFiles();
			SetFocus( m_List.GetWindowHandle() );
			break;
		}
		case ID_FILE_PLAYLISTREMOVEFILES : {
			m_List.DeleteSelectedItems();
			SetFocus( m_List.GetWindowHandle() );
			break;
		}
		case ID_FILE_OPTIONS : {
			OnOptions();
			break;
		}
		case ID_VIEW_COUNTER_FONTSTYLE : {
			m_Counter.OnSelectFont();
			break;
		}
		case ID_VIEW_COUNTER_FONTCOLOUR : {
			m_Counter.OnSelectColour();
			break;
		}
		case ID_VIEW_COUNTER_TRACKELAPSED : 
		case ID_VIEW_COUNTER_TRACKREMAINING : {
			m_Counter.SetTrackRemaining( ID_VIEW_COUNTER_TRACKREMAINING == commandID );
			break;
		}
		case ID_SHOWCOLUMNS_ARTIST :
		case ID_SHOWCOLUMNS_ALBUM :
		case ID_SHOWCOLUMNS_GENRE :
		case ID_SHOWCOLUMNS_YEAR :
		case ID_SHOWCOLUMNS_TRACK :
		case ID_SHOWCOLUMNS_TYPE :
		case ID_SHOWCOLUMNS_VERSION :
		case ID_SHOWCOLUMNS_SAMPLERATE :
		case ID_SHOWCOLUMNS_CHANNELS :
		case ID_SHOWCOLUMNS_BITRATE :
		case ID_SHOWCOLUMNS_BITSPERSAMPLE :
		case ID_SHOWCOLUMNS_DURATION :
		case ID_SHOWCOLUMNS_FILESIZE :
		case ID_SHOWCOLUMNS_FILENAME :
		case ID_SHOWCOLUMNS_FILETIME :
		case ID_SHOWCOLUMNS_TRACKGAIN :
		case ID_SHOWCOLUMNS_ALBUMGAIN : {
			m_List.OnShowColumn( commandID );
			break;
		}
		case ID_SORTPLAYLIST_ARTIST :
		case ID_SORTPLAYLIST_ALBUM :
		case ID_SORTPLAYLIST_GENRE :
		case ID_SORTPLAYLIST_YEAR :
		case ID_SORTPLAYLIST_TRACK :
		case ID_SORTPLAYLIST_TYPE :
		case ID_SORTPLAYLIST_VERSION :
		case ID_SORTPLAYLIST_SAMPLERATE :
		case ID_SORTPLAYLIST_CHANNELS :
		case ID_SORTPLAYLIST_BITRATE :
		case ID_SORTPLAYLIST_BITSPERSAMPLE :
		case ID_SORTPLAYLIST_DURATION :
		case ID_SORTPLAYLIST_FILESIZE :
		case ID_SORTPLAYLIST_FILENAME :
		case ID_SORTPLAYLIST_FILETIME :
		case ID_SORTPLAYLIST_TRACKGAIN :
		case ID_SORTPLAYLIST_ALBUMGAIN : {
			m_List.OnSortPlaylist( commandID );
			break;
		}
		case ID_LISTMENU_FONTSTYLE : {
			m_List.OnSelectFont();
			break;
		}
		case ID_LISTMENU_FONTCOLOUR : 
		case ID_LISTMENU_BACKGROUNDCOLOUR :
		case ID_LISTMENU_HIGHLIGHTCOLOUR : {
			m_List.OnSelectColour( commandID );
			break;
		}
		case ID_TREEMENU_FONTSTYLE : {
			m_Tree.OnSelectFont();
			break;
		}
		case ID_TREEMENU_FONTCOLOUR : 
		case ID_TREEMENU_BACKGROUNDCOLOUR :
		case ID_TREEMENU_HIGHLIGHTCOLOUR : {
			m_Tree.OnSelectColour( commandID );
			break;
		}
		case ID_TREEMENU_ALLTRACKS : {
			m_Tree.OnAllTracks();
			break;
		}
		case ID_TREEMENU_ARTISTS : {
			m_Tree.OnArtists();
			break;
		}
		case ID_TREEMENU_ALBUMS : {
			m_Tree.OnAlbums();
			break;
		}
		case ID_TREEMENU_GENRES : {
			m_Tree.OnGenres();
			break;
		}
		case ID_TREEMENU_YEARS : {
			m_Tree.OnYears();
			break;
		}
		case ID_TRAYMENU_SHOWVUPLAYER : {
			if ( IsIconic( m_hWnd ) ) {
				ShowWindow( m_hWnd, SW_RESTORE );
			} else {
				ShowWindow( m_hWnd, SW_MINIMIZE );
			}
			break;
		}
		case ID_HELP_ONLINEDOCUMENTATION : {
			ShellExecute( NULL, L"open", s_OnlineDocs, NULL, NULL, SW_SHOWNORMAL );
			break;
		}
		default : {
			if ( ( commandID >= MSG_TRAYMENUSTART ) && ( commandID <= MSG_TRAYMENUEND ) ) {
				m_Tray.OnContextMenuCommand( commandID );
			}
			break;
		}
	}
}

void VUPlayer::OnInitMenu( const HMENU menu )
{
	if ( nullptr != menu ) {
		// File menu
		Playlist::Ptr playlist = m_List.GetPlaylist();
		const bool selectedItems = ( m_List.GetSelectedCount() > 0 );
		const UINT deletePlaylistEnabled = ( playlist && ( Playlist::Type::User == playlist->GetType() ) ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_DELETEPLAYLIST, MF_BYCOMMAND | deletePlaylistEnabled );
		const UINT exportPlaylistEnabled = playlist ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_EXPORTPLAYLIST, MF_BYCOMMAND | exportPlaylistEnabled );
		const UINT addFolderEnabled = ( !playlist || ( playlist && ( ( Playlist::Type::User == playlist->GetType() ) || ( Playlist::Type::All == playlist->GetType( ) ) ) ) ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_PLAYLISTADDFOLDER, MF_BYCOMMAND | addFolderEnabled );
		EnableMenuItem( menu, ID_FILE_PLAYLISTADDFILES, MF_BYCOMMAND | addFolderEnabled );
		const UINT removeFilesEnabled = ( playlist && ( Playlist::Type::User == playlist->GetType() ) && selectedItems ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_PLAYLISTREMOVEFILES, MF_BYCOMMAND | removeFilesEnabled );
		const UINT replaygainEnabled = selectedItems ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_FILE_CALCULATEREPLAYGAIN, MF_BYCOMMAND | replaygainEnabled );

		// View menu
		CheckMenuItem( menu, ID_TREEMENU_ALLTRACKS, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_ALLTRACKS ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_ARTISTS, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_ARTISTS ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_ALBUMS, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_ALBUMS ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_GENRES, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_GENRES ) ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_TREEMENU_YEARS, MF_BYCOMMAND | ( m_Tree.IsShown( ID_TREEMENU_YEARS ) ? MF_CHECKED : MF_UNCHECKED ) );

		const UINT trackInfoEnabled = ( m_List.GetCurrentSelectedIndex() >= 0 ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_VIEW_TRACKINFORMATION, MF_BYCOMMAND | trackInfoEnabled );
		const bool trackRemaining = m_Counter.GetTrackRemaining();
		CheckMenuItem( menu, ID_VIEW_COUNTER_TRACKREMAINING, trackRemaining ? ( MF_BYCOMMAND | MF_CHECKED ) : ( MF_BYCOMMAND | MF_UNCHECKED ) );
		CheckMenuItem( menu, ID_VIEW_COUNTER_TRACKELAPSED, !trackRemaining ? ( MF_BYCOMMAND | MF_CHECKED ) : ( MF_BYCOMMAND | MF_UNCHECKED ) );

		std::set<UINT> shownColumns;
		std::set<UINT> hiddenColumns;
		m_List.GetColumnVisibility( shownColumns, hiddenColumns );
		for ( const auto& hiddenColumn : hiddenColumns ) {
			CheckMenuItem( menu, hiddenColumn, MF_BYCOMMAND | MF_UNCHECKED );
		}
		for ( const auto& shownColumn : shownColumns ) {
			CheckMenuItem( menu, shownColumn, MF_BYCOMMAND | MF_CHECKED );
		}

		const UINT currentVisual = m_Visual.GetCurrentVisualID();
		const std::set<UINT> visualIDs = m_Visual.GetVisualIDs();
		for ( const auto& visualID : visualIDs ) {
			if ( currentVisual == visualID ) {
				CheckMenuItem( menu, visualID, MF_BYCOMMAND | MF_CHECKED );
			} else {
				CheckMenuItem( menu, visualID, MF_BYCOMMAND | MF_UNCHECKED );
			}
		}
		const float oscilloscopeWeight = m_Settings.GetOscilloscopeWeight();
		const std::map<UINT,float> oscilloscopeWeights = m_Visual.GetOscilloscopeWeights();
		for ( const auto& weight : oscilloscopeWeights ) {
			if ( oscilloscopeWeight == weight.second ) {
				CheckMenuItem( menu, weight.first, MF_BYCOMMAND | MF_CHECKED );
			} else {
				CheckMenuItem( menu, weight.first, MF_BYCOMMAND | MF_UNCHECKED );
			}
		}
		const float vumeterDecay = m_Settings.GetVUMeterDecay();
		const std::map<UINT,float> vumeterDecayRates = m_Visual.GetVUMeterDecayRates();
		for ( const auto& decay : vumeterDecayRates ) {
			if ( vumeterDecay == decay.second ) {
				CheckMenuItem( menu, decay.first, MF_BYCOMMAND | MF_CHECKED );
			} else {
				CheckMenuItem( menu, decay.first, MF_BYCOMMAND | MF_UNCHECKED );
			}
		}

		// Control menu
		const Output::State outputState = m_Output.GetState();
		const int buffersize = 16;
		WCHAR buffer[ buffersize ] = {};
		LoadString( m_hInst, ( Output::State::Playing == outputState ) ? IDS_CONTROL_PAUSEMENU : IDS_CONTROL_PLAYMENU, buffer, buffersize );
		ModifyMenu( menu, ID_CONTROL_PLAY, MF_BYCOMMAND | MF_STRING, ID_CONTROL_PLAY, buffer );

		const UINT playEnabled = m_ToolbarPlayback.IsButtonEnabled( ID_CONTROL_PLAY ) ? MF_ENABLED : MF_DISABLED;
		const UINT stopEnabled = m_ToolbarPlayback.IsButtonEnabled( ID_CONTROL_STOP ) ? MF_ENABLED : MF_DISABLED;
		const UINT previousEnabled = m_ToolbarPlayback.IsButtonEnabled( ID_CONTROL_PREVIOUSTRACK ) ? MF_ENABLED : MF_DISABLED;
		const UINT nextEnabled = m_ToolbarPlayback.IsButtonEnabled( ID_CONTROL_NEXTTRACK ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_CONTROL_PLAY, MF_BYCOMMAND | playEnabled );
		EnableMenuItem( menu, ID_CONTROL_STOP, MF_BYCOMMAND | stopEnabled );
		EnableMenuItem( menu, ID_CONTROL_PREVIOUSTRACK, MF_BYCOMMAND | previousEnabled );
		EnableMenuItem( menu, ID_CONTROL_NEXTTRACK, MF_BYCOMMAND | nextEnabled );

		const bool isStopAtTrackEnd = m_Output.GetStopAtTrackEnd();
		const bool isMuted = m_Output.GetMuted();
		const bool isFadeOut = m_Output.GetFadeOut();
		const bool isFadeToNext = m_Output.GetFadeToNext();
		const bool isCrossfade = m_Output.GetCrossfade();

		const UINT enableFadeOut = ( !isFadeToNext && ( Output::State::Playing == outputState ) ) ? MF_ENABLED : MF_DISABLED;
		const UINT enableFadeToNext = ( !isFadeOut && ( Output::State::Playing == outputState ) ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_CONTROL_FADEOUT, MF_BYCOMMAND | enableFadeOut );
		EnableMenuItem( menu, ID_CONTROL_FADETONEXT, MF_BYCOMMAND | enableFadeOut );

		const UINT checkStopAtTrackEnd = isStopAtTrackEnd ? MF_CHECKED : MF_UNCHECKED;
		const UINT checkMuted = isMuted ? MF_CHECKED : MF_UNCHECKED;
		const UINT checkFadeOut = isFadeOut ? MF_CHECKED : MF_UNCHECKED;
		const UINT checkFadeToNext = isFadeToNext ? MF_CHECKED : MF_UNCHECKED;
		const UINT checkCrossfade = isCrossfade ? MF_CHECKED : MF_UNCHECKED;
		CheckMenuItem( menu, ID_CONTROL_STOPTRACKEND, MF_BYCOMMAND | checkStopAtTrackEnd );
		CheckMenuItem( menu, ID_CONTROL_MUTE, MF_BYCOMMAND | checkMuted );
		CheckMenuItem( menu, ID_CONTROL_FADEOUT, MF_BYCOMMAND | checkFadeOut );
		CheckMenuItem( menu, ID_CONTROL_FADETONEXT, MF_BYCOMMAND | checkFadeToNext );
		CheckMenuItem( menu, ID_CONTROL_CROSSFADE, MF_BYCOMMAND | checkCrossfade );

		const UINT enableSkip = ( Output::State::Stopped != outputState ) ? MF_ENABLED : MF_DISABLED;
		EnableMenuItem( menu, ID_CONTROL_SKIPBACKWARDS, MF_BYCOMMAND | enableSkip );
		EnableMenuItem( menu, ID_CONTROL_SKIPFORWARDS, MF_BYCOMMAND | enableSkip );

		const UINT randomPlay = m_Output.GetRandomPlay() ? MF_CHECKED : MF_UNCHECKED;
		const UINT repeatTrack = m_Output.GetRepeatTrack() ? MF_CHECKED : MF_UNCHECKED;
		const UINT repeatPlaylist = m_Output.GetRepeatPlaylist() ? MF_CHECKED : MF_UNCHECKED;
		CheckMenuItem( menu, ID_CONTROL_RANDOMPLAY, MF_BYCOMMAND | randomPlay );
		CheckMenuItem( menu, ID_CONTROL_REPEATTRACK, MF_BYCOMMAND | repeatTrack );
		CheckMenuItem( menu, ID_CONTROL_REPEATPLAYLIST, MF_BYCOMMAND | repeatPlaylist );
	}
}

void VUPlayer::OnMediaUpdated( const MediaInfo& previousMediaInfo, const MediaInfo& updatedMediaInfo )
{
	MediaInfo* previousInfo = new MediaInfo( previousMediaInfo );
	MediaInfo* updatedInfo = new MediaInfo( updatedMediaInfo );
	PostMessage( m_hWnd, MSG_MEDIAUPDATED, reinterpret_cast<WPARAM>( previousInfo ), reinterpret_cast<LPARAM>( updatedInfo ) );
}

void VUPlayer::OnHandleMediaUpdate( const MediaInfo* previousMediaInfo, const MediaInfo* updatedMediaInfo )
{
	if ( ( nullptr != previousMediaInfo ) && ( nullptr != updatedMediaInfo ) ) {
		const Playlist::Set updatedPlaylists = m_Tree.OnUpdatedMedia( *previousMediaInfo, *updatedMediaInfo );
		const Playlist::Ptr currentPlaylist = m_List.GetPlaylist();
		if ( currentPlaylist && ( updatedPlaylists.end() != updatedPlaylists.find( currentPlaylist ) ) ) {
			m_List.OnUpdatedMedia( *updatedMediaInfo );
		}

		if ( m_Output.OnUpdatedMedia( *updatedMediaInfo ) ) {
			if ( ID_VISUAL_ARTWORK == m_Visual.GetCurrentVisualID() ) {
				m_Splitter.Resize();
				m_Visual.DoRender();
			}
		}	
	}
}

void VUPlayer::OnRestartPlayback( const long itemID )
{
	m_Output.Play( itemID );
}

void VUPlayer::OnTrackInformation()
{
	Playlist::ItemList selectedItems = m_List.GetSelectedPlaylistItems();
	if ( !selectedItems.empty() ) {
		DlgTrackInfo trackInfo( m_hInst, m_hWnd, m_Library, selectedItems );
		SetFocus( m_List.GetWindowHandle() );
	}
}

void VUPlayer::OnListSelectionChanged()
{
	const Playlist::Item currentSelectedPlaylistItem = m_List.GetCurrentSelectedItem();
	const Playlist::Item currentSelectedOutputItem = m_Output.GetCurrentSelectedPlaylistItem();
	m_Output.SetCurrentSelectedPlaylistItem( currentSelectedPlaylistItem );
	if ( ( Output::State::Stopped == m_Output.GetState() ) && ( ID_VISUAL_ARTWORK == m_Visual.GetCurrentVisualID() ) &&
			( currentSelectedPlaylistItem.Info.GetArtworkID() != currentSelectedOutputItem.Info.GetArtworkID() ) ) {
		m_Splitter.Resize();
		m_Visual.DoRender();
	}
}

COLORREF* VUPlayer::GetCustomColours()
{
	return m_CustomColours;
}

std::shared_ptr<Gdiplus::Bitmap> VUPlayer::GetPlaceholderImage()
{
	std::shared_ptr<Gdiplus::Bitmap> bitmapPtr;
	HRSRC resource = FindResource( m_hInst, MAKEINTRESOURCE( IDB_PLACEHOLDER ), L"PNG" );
	if ( nullptr != resource ) {
		HGLOBAL hGlobal = LoadResource( m_hInst, resource );
		if ( nullptr != hGlobal ) {
			LPVOID resourceBytes = LockResource( hGlobal );
			if ( nullptr != resourceBytes ) {
				const DWORD resourceSize = SizeofResource( m_hInst, resource );
				if ( resourceSize > 0 ) {
					IStream* stream = nullptr;
					if ( SUCCEEDED( CreateStreamOnHGlobal( NULL /*hGlobal*/, TRUE /*deleteOnRelease*/, &stream ) ) ) {
						if ( SUCCEEDED( stream->Write( resourceBytes, static_cast<ULONG>( resourceSize ), NULL /*bytesWritten*/ ) ) ) {
							try {
								bitmapPtr = std::shared_ptr<Gdiplus::Bitmap>( new Gdiplus::Bitmap( stream ) );
							} catch ( ... ) {
							}
						}
						stream->Release();
					}				
				}
			}
		}
	}
	if ( bitmapPtr && ( ( bitmapPtr->GetWidth() == 0 ) || ( bitmapPtr->GetHeight() == 0 ) ) ) {
		bitmapPtr.reset();
	}
	return bitmapPtr;
}

void VUPlayer::OnOptions()
{
	m_Hotkeys.Unregister();
	DlgOptions options( m_hInst, m_hWnd, m_Settings, m_Output );
	m_Hotkeys.Update();

	bool systrayEnable = false;
	Settings::SystrayCommand systraySingleClick = Settings::SystrayCommand::None;
	Settings::SystrayCommand systrayDoubleClick = Settings::SystrayCommand::None;
	m_Settings.GetSystraySettings( systrayEnable, systraySingleClick, systrayDoubleClick );
	if ( !systrayEnable && m_Tray.IsShown() ) {
		m_Tray.Hide();
	} else if ( systrayEnable && !m_Tray.IsShown() ) {
		m_Tray.Show();
	}
}

Settings& VUPlayer::GetApplicationSettings()
{
	return m_Settings;
}

void VUPlayer::OnTrayNotify( WPARAM wParam, LPARAM lParam )
{
	m_Tray.OnNotify( wParam, lParam );
}

void VUPlayer::OnHotkey( WPARAM wParam )
{
	m_Hotkeys.OnHotkey( wParam );
}

void VUPlayer::OnCalculateReplayGain()
{
	MediaInfo::List mediaList;
	const Playlist::ItemList selectedItems = m_List.GetSelectedPlaylistItems();
	for ( const auto& item : selectedItems ) {
		mediaList.push_back( item.Info );
	}
	m_ReplayGain.Calculate( mediaList );
}

Playlist::Ptr VUPlayer::NewPlaylist()
{
	Playlist::Ptr playlist = m_Tree.NewPlaylist();
	return playlist;
}

bool VUPlayer::AllowSkip() const
{
	LARGE_INTEGER perfFreq = {};
	LARGE_INTEGER perfCount = {};
	QueryPerformanceFrequency( &perfFreq );
	QueryPerformanceCounter( &perfCount );
	const float secondsSinceLastSkip = static_cast<float>( perfCount.QuadPart - m_LastSkipCount.QuadPart ) / perfFreq.QuadPart;
	const bool allowSkip = ( secondsSinceLastSkip > s_SkipLimitInterval ) || ( secondsSinceLastSkip < 0 );
	return allowSkip;
}

void VUPlayer::ResetLastSkipCount()
{
	QueryPerformanceCounter( &m_LastSkipCount );
}

std::wstring VUPlayer::GetBassVersion() const
{
	const std::wstring version = m_Handlers.GetBassVersion();
	return version;
}