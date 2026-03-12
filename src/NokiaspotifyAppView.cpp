/*
 ============================================================================
 Name		: NokiaspotifyAppView.cpp
 Author	  :
 Copyright   : Your copyright notice
 Description : Application view implementation
 ============================================================================
 */

// INCLUDE FILES
#include <coemain.h>
#include <gdi.h>
#include <eikenv.h>
#include <e32keys.h>
#include <eikappui.h>
#include "NokiaspotifyAppView.h"
#include "NokiaspotifyAppUi.h"

_LIT(KTitleText, "TurboMusic");
_LIT(KIntroText, "E75 player: lokalnie, cache i internet.");
_LIT(KMenu1, "1. Szukaj muzyki");
_LIT(KMenu2, "2. Lista utworow");
_LIT(KMenu3, "3. Internet ON/OFF");
_LIT(KMenu4, "4. Szukaj online");
_LIT(KMenu5, "5. Reindex biblioteki");
_LIT(KMenu6, "6. Clean cache");
_LIT(KMenu7, "7. Ping serwera");
_LIT(KMenuHint, "Gora/Dol + OK, 0=player");
_LIT(KListHint, "OK=graj, 5=zapisz, 9=usun, 0=player");
_LIT(KNowPlayingHelp1, "4=prev 5=play/stop 6=next");
_LIT(KNowPlayingHelp2, "7=losowo 0=powrot");

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CNokiaspotifyAppView* CNokiaspotifyAppView::NewL(const TRect& aRect)
	{
	CNokiaspotifyAppView* self = CNokiaspotifyAppView::NewLC(aRect);
	CleanupStack::Pop(self);
	return self;
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CNokiaspotifyAppView* CNokiaspotifyAppView::NewLC(const TRect& aRect)
	{
	CNokiaspotifyAppView* self = new (ELeave) CNokiaspotifyAppView;
	CleanupStack::PushL(self);
	self->ConstructL(aRect);
	return self;
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CNokiaspotifyAppView::ConstructL(const TRect& aRect)
	{
	CreateWindowL();
	SetRect(aRect);
	ActivateL();
	}
// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::CNokiaspotifyAppView()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CNokiaspotifyAppView::CNokiaspotifyAppView()
	: iLoginButtonDown(EFalse)
	, iLoginKeyFocus(ETrue)
	, iInlineInputActive(EFalse)
	, iInlineInputOnline(EFalse)
	, iPlaybackPanelVisible(EFalse)
	, iTrackListVisible(EFalse)
	, iNowPlayingVisible(EFalse)
	, iNowPlayingShuffle(EFalse)
	, iMenuSelection(0)
	, iTrackSelection(0)
	, iTrackCount(0)
	{
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::LoginButtonRect()
// -----------------------------------------------------------------------------
//
TRect CNokiaspotifyAppView::LoginButtonRect()
	{
	return TRect(70, 140, 170, 170);
	}

TRect CNokiaspotifyAppView::IntroduceTextBox(
		const TRect& aClient,
		const TRect& aLoginButton)
	{
	const TInt kOuterMargin = 8;
	const TInt kGapAboveButton = 8;
	const TInt left = aClient.iTl.iX + kOuterMargin;
	const TInt top = aClient.iTl.iY + kOuterMargin;
	const TInt right = aClient.iBr.iX - kOuterMargin;
	const TInt bottom = aLoginButton.iTl.iY - kGapAboveButton;
	return TRect(left, top, right, bottom);
	}

TSize CNokiaspotifyAppView::ClassicRound()
	{
