/*
 ============================================================================
 Name		: NokiaspotifyAppUi.h
 Author	  :
 Copyright   : Your copyright notice
 Description : Declares UI class for application.
 ============================================================================
 */

#ifndef __NOKIASPOTIFYAPPUI_h__
#define __NOKIASPOTIFYAPPUI_h__

// INCLUDES
#include <aknappui.h>
#include <MdaAudioSamplePlayer.h>

// FORWARD DECLARATIONS
class CNokiaspotifyAppView;
class CTurboMusicCacheManager;
class CNokiaspotifyNetwork;
class CTurboMusicService;
class CTurboTrackEntry;

// CLASS DECLARATION
/**
 * CNokiaspotifyAppUi application UI class.
 * Interacts with the user through the UI and request message processing
 * from the handler class
 */
class CMdaAudioPlayerUtility;

class CNokiaspotifyAppUi : public CAknAppUi, public MMdaAudioPlayerCallback
	{
public:
	// Constructors and destructor

	/**
	 * ConstructL.
	 * 2nd phase constructor.
	 */
	void ConstructL();

	/**
	 * CNokiaspotifyAppUi.
	 * C++ default constructor. This needs to be public due to
	 * the way the framework constructs the AppUi
	 */
	CNokiaspotifyAppUi();

	/**
	 * ~CNokiaspotifyAppUi.
	 * Virtual Destructor.
	 */
	virtual ~CNokiaspotifyAppUi();

	/** Wywoływane z widoku (klawiatura: Enter / środek joysticka) — otwiera status cache */
	void HandleLoginFromViewL();
	void HandleQuickSearchFromViewL();
	void HandleQuickCreatePlaylistFromViewL();
	void HandleQuickAddTrackToPlaylistFromViewL();
	void HandleQuickShowPlaylistsFromViewL();
	void HandleQuickToggleInternetFromViewL();
	void HandleQuickOpenPlaylistFromViewL();
	void HandleQuickReindexLibraryFromViewL();
	void HandleQuickPingFromViewL();
	void HandleQuickOnlineSearchFromViewL();
	void HandleQuickShowTrackListFromViewL();
	void HandleQuickCleanCacheFromViewL();
	void HandleInlineSearchQueryFromViewL(const TDesC& aQuery);
	void HandleInlineOnlineSearchQueryFromViewL(const TDesC& aQuery);
	void HandleTrackChosenFromViewL(TInt aIndex);
	void HandleSaveTrackFromViewL(TInt aIndex);
	void HandleDeleteTrackFromViewL(TInt aIndex);
	void HandleOpenNowPlayingFromViewL();
	void HandlePlaybackPrevFromViewL();
	void HandlePlaybackToggleFromViewL();
	void HandlePlaybackNextFromViewL();
	void HandlePlaybackShuffleFromViewL();

private:
	// Functions from base classes

	/**
	 * From CEikAppUi, HandleCommandL.
	 * Takes care of command handling.
	 * @param aCommand Command to be handled.
	 */
	void HandleCommandL(TInt aCommand);

	/**
	 *  HandleStatusPaneSizeChange.
	 *  Called by the framework when the application status pane
	 *  size is changed.
	 */
	void HandleStatusPaneSizeChange();
	void MapcInitComplete(TInt aError, const TTimeIntervalMicroSeconds& aDuration);
	void MapcPlayComplete(TInt aError);

	/**
	 *  From CCoeAppUi, HelpContextL.
	 *  Provides help context for the application.
	 *  size is changed.
	 */
	CArrayFix<TCoeHelpContext>* HelpContextL() const;

private:
	void ShowWelcomeDialogL();
	void ShowLoginInfoL();
	void ShowCacheStatusL();
	void InitializeCacheManagerL();
	void SearchMusicL();
	void SearchMusicByQueryL(const TDesC& aQuery);
	void SearchMusicOnlineL();
	void SearchMusicOnlineByQueryL(const TDesC& aQuery);
	void ShowTrackListL();
	void ShowCurrentTrackListL(const TDesC& aTitle);
	void ResetCurrentTrackList();
	void PlayLocalFileL(const TDesC& aPath, TBool aFromInternet, const TDesC& aDisplayName);
	void DownloadRemoteTrackL(const TDesC& aRelativeUrl, const TDesC& aFileName);
	void CleanCacheL();
	void ShowOperationErrorL(TInt aErr);
	void ResetPlaybackQueue();
	void CopyCurrentTracksToPlaybackQueueL();
	void PlayQueueIndexL(TInt aIndex);
	void UpdatePlaybackUi();
	void OpenNowPlayingScreenL();
	void StopPlayback();
	void ToggleShuffle();
	TInt ResolveNextQueueIndex() const;
	TInt ResolvePrevQueueIndex() const;
	void CreatePlaylistL();
	void AddTrackToPlaylistL();
	void ShowPlaylistsL();
	void ShowPlaylistByNameL();
	void RemoveTrackFromPlaylistL();
	void DeletePlaylistL();
	void RebuildLocalLibraryIndexL();
	void ToggleInternetConnectionL();
	void ShowAppStatusL();
	void ResolveDataDir(TDes& aOut);
	void OpenOnlineSearchL(const TDesC& aQuery);
	void PingServerL();

private:
	// Data

	/**
	 * The application view
	 * Owned by CNokiaspotifyAppUi
	 */
	CNokiaspotifyAppView* iAppView;
	CTurboMusicCacheManager* iCacheManager;
	CNokiaspotifyNetwork* iNetwork;
	CTurboMusicService* iMusicService;
	RPointerArray<CTurboTrackEntry> iCurrentTracks;
	RPointerArray<CTurboTrackEntry> iPlaybackQueue;
	TBuf<32> iCurrentTrackListTitle;
	CMdaAudioPlayerUtility* iAudioPlayer;
	TInt iPlaybackIndex;
	TBool iAudioReady;
	TBool iAudioPlaying;
	TBool iShuffleEnabled;
	TBool iPendingAutoPlay;
	TBool iStopRequested;
	TBool iCurrentTrackFromInternet;
	TBuf<96> iCurrentPlaybackName;
	TFileName iPendingAudioPath;
	TBuf<96> iPendingPlaybackName;

	};

#endif // __NOKIASPOTIFYAPPUI_h__
// End of File
