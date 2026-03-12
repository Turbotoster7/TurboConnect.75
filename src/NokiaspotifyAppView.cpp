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
	return TSize(15, 15);
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::~CNokiaspotifyAppView()
// Destructor.
// -----------------------------------------------------------------------------
//
CNokiaspotifyAppView::~CNokiaspotifyAppView()
	{
	// No implementation required
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::Draw()
// Draws the display.
// -----------------------------------------------------------------------------
//
void CNokiaspotifyAppView::Draw(const TRect& /*aRect*/) const
	{
	CWindowGc& gc = SystemGc();
	const TRect r(Rect());
	gc.SetPenStyle(CGraphicsContext::ENullPen);
	gc.SetBrushColor(TRgb(0,0,0));
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.DrawRect(r);

	const TRgb kGreen(30, 215, 96);
	const TRgb kDarkGreen(12, 30, 18);
	const TRgb kLine(20, 60, 32);
	const TInt top = r.iTl.iY + 6;

	gc.SetPenStyle(CGraphicsContext::ESolidPen);
	gc.SetPenColor(kGreen);
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetBrushColor(kDarkGreen);
	gc.DrawRoundRect(TRect(r.iTl.iX + 6, top, r.iBr.iX - 6, top + 24), ClassicRound());

	const CFont* titleFont = CEikonEnv::Static()->DenseFont();
	gc.UseFont(titleFont);
	gc.SetPenColor(TRgb(180, 255, 200));
	gc.DrawText(KTitleText, TPoint(r.iTl.iX + 12, top + 15));
	gc.DiscardFont();

	const CFont* textFont = CEikonEnv::Static()->LegendFont();
	gc.UseFont(textFont);
	gc.SetPenColor(TRgb(180, 200, 185));
	if (iNowPlayingVisible)
		{
		gc.DrawText(iNowPlayingTitle, TPoint(r.iTl.iX + 10, top + 38));
		}
	else if (iTrackListVisible)
		{
		gc.DrawText(iListTitle, TPoint(r.iTl.iX + 10, top + 38));
		}
	else
		{
		gc.DrawText(KIntroText, TPoint(r.iTl.iX + 10, top + 38));
		}
	gc.DiscardFont();

	const TInt listLeft = r.iTl.iX + 8;
	const TInt listRight = r.iBr.iX - 8;
	const TInt listTop = top + 46;
	const TInt rowHeight = 20;
	const CFont* rowFont = CEikonEnv::Static()->LegendFont();
	if (iNowPlayingVisible)
		{
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(kGreen);
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(TRgb(5, 12, 8));
		TRect card(listLeft, listTop, listRight, listTop + 112);
		gc.DrawRoundRect(card, TSize(12, 12));

		const CFont* panelFont = CEikonEnv::Static()->LegendFont();
		gc.UseFont(panelFont);
		gc.SetPenColor(TRgb(160, 255, 190));
		gc.DrawText(iNowPlayingStatus, TPoint(card.iTl.iX + 8, card.iTl.iY + 16));
		gc.SetPenColor(TRgb(255, 255, 255));
		gc.DrawText(iNowPlayingDetail, TPoint(card.iTl.iX + 8, card.iTl.iY + 38));
		TBuf<32> shuffle;
		shuffle.Copy(_L("Losowo: "));
		shuffle.Append(iNowPlayingShuffle ? _L("TAK") : _L("NIE"));
		gc.SetPenColor(TRgb(180, 230, 190));
		gc.DrawText(shuffle, TPoint(card.iTl.iX + 8, card.iTl.iY + 60));
		gc.DrawText(KNowPlayingHelp1, TPoint(card.iTl.iX + 8, card.iTl.iY + 84));
		gc.DrawText(KNowPlayingHelp2, TPoint(card.iTl.iX + 8, card.iTl.iY + 102));
		gc.DiscardFont();
		}
	else
		{
		gc.UseFont(rowFont);
		for (TInt i = 0; i < (iTrackListVisible ? iTrackCount : 7); ++i)
			{
			TRect row(listLeft, listTop + i * rowHeight, listRight, listTop + (i + 1) * rowHeight - 2);
			const TBool selected = iTrackListVisible ? (i == iTrackSelection) : (i == iMenuSelection);
			gc.SetPenStyle(CGraphicsContext::ESolidPen);
			gc.SetPenColor(selected ? kGreen : kLine);
			gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
			gc.SetBrushColor(selected ? TRgb(20, 70, 35) : TRgb(5, 12, 8));
			gc.DrawRoundRect(row, TSize(8, 8));
			gc.SetPenColor(selected ? TRgb(255, 255, 255) : TRgb(170, 230, 190));
			TBuf<96> line;
			if (iTrackListVisible)
				{
				line.Copy(iTrackItems[i].Left(line.MaxLength()));
				}
			else
				{
				switch (i)
					{
					case 0: line.Copy(KMenu1); break;
					case 1: line.Copy(KMenu2); break;
