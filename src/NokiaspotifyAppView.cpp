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
					case 2: line.Copy(KMenu3); break;
					case 3: line.Copy(KMenu4); break;
					case 4: line.Copy(KMenu5); break;
					case 5: line.Copy(KMenu6); break;
					default: line.Copy(KMenu7); break;
					}
				}
			gc.DrawText(line, TPoint(row.iTl.iX + 6, row.iTl.iY + 14));
			}
		gc.DiscardFont();
		}

	gc.UseFont(textFont);
	gc.SetPenColor(TRgb(140, 170, 150));
	if (iNowPlayingVisible)
		{
		gc.DrawText(KNowPlayingHelp1, TPoint(r.iTl.iX + 10, r.iBr.iY - 64));
		gc.DrawText(KNowPlayingHelp2, TPoint(r.iTl.iX + 10, r.iBr.iY - 50));
		}
	else if (iTrackListVisible)
		{
		gc.DrawText(KListHint, TPoint(r.iTl.iX + 10, r.iBr.iY - 64));
		}
	else
		{
		gc.DrawText(KMenuHint, TPoint(r.iTl.iX + 10, r.iBr.iY - 64));
		}
	gc.DiscardFont();

	if (iInlineInputActive)
		{
		TRect box(r.iTl.iX + 8, r.iTl.iY + 48, r.iBr.iX - 8, r.iTl.iY + 128);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(TRgb(80, 220, 120));
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(TRgb(20, 20, 20));
		gc.DrawRoundRect(box, ClassicRound());

		const CFont* inputFont = CEikonEnv::Static()->LegendFont();
		gc.UseFont(inputFont);
		gc.SetPenColor(TRgb(255, 255, 255));
		gc.DrawText(iInlinePrompt, TPoint(box.iTl.iX + 8, box.iTl.iY + 16));
		gc.DrawText(iInlineInput, TPoint(box.iTl.iX + 8, box.iTl.iY + 34));
		_LIT(KInputHelp1, "Enter=szukaj");
		_LIT(KInputHelp2, "Backspace=kasuj, #=anuluj");
		gc.SetPenColor(TRgb(180, 180, 180));
		gc.DrawText(KInputHelp1, TPoint(box.iTl.iX + 8, box.iTl.iY + 54));
		gc.DrawText(KInputHelp2, TPoint(box.iTl.iX + 8, box.iTl.iY + 68));
		gc.DiscardFont();
		}

	if (iPlaybackPanelVisible)
		{
		TRect panel(r.iTl.iX + 8, r.iBr.iY - 56, r.iBr.iX - 8, r.iBr.iY - 8);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(TRgb(30, 215, 96));
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(TRgb(12, 30, 18));
		gc.DrawRoundRect(panel, ClassicRound());

		gc.SetBrushColor(TRgb(30, 215, 96));
		gc.DrawRect(TRect(panel.iTl.iX + 6, panel.iBr.iY - 18, panel.iTl.iX + 10, panel.iBr.iY - 8));
		gc.DrawRect(TRect(panel.iTl.iX + 13, panel.iBr.iY - 24, panel.iTl.iX + 17, panel.iBr.iY - 8));
		gc.DrawRect(TRect(panel.iTl.iX + 20, panel.iBr.iY - 15, panel.iTl.iX + 24, panel.iBr.iY - 8));

		const CFont* panelFont = CEikonEnv::Static()->LegendFont();
		gc.UseFont(panelFont);
		gc.SetPenColor(TRgb(160, 255, 190));
		gc.DrawText(iPlaybackTitle, TPoint(panel.iTl.iX + 30, panel.iTl.iY + 15));
		gc.SetPenColor(TRgb(255, 255, 255));
		gc.DrawText(iPlaybackDetail, TPoint(panel.iTl.iX + 30, panel.iTl.iY + 31));
		gc.DiscardFont();
		}

	gc.SetPenStyle(CGraphicsContext::ENullPen);
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::SizeChanged()
// Called by framework when the view size is changed.
// -----------------------------------------------------------------------------
//
void CNokiaspotifyAppView::SizeChanged()
	{
	DrawNow();
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppView::HandlePointerEventL()
// Called by framework to handle pointer touch events.
// Note: although this method is compatible with earlier SDKs,
// it will not be called in SDKs without Touch support.
// -----------------------------------------------------------------------------
//
void CNokiaspotifyAppView::HandlePointerEventL(
		const TPointerEvent& aPointerEvent)
	{
	CCoeControl::HandlePointerEventL(aPointerEvent);
	}

void CNokiaspotifyAppView::BeginInlineSearchInputL(TBool aOnline)
	{
	iInlineInputActive = ETrue;
	iInlineInputOnline = aOnline;
	iNowPlayingVisible = EFalse;
	iInlineInput.Zero();
	iPlaybackPanelVisible = EFalse;
	iPlaybackTitle.Zero();
	iPlaybackDetail.Zero();
	if (aOnline)
		{
		iInlinePrompt.Copy(_L("Wpisz utwor online:"));
		}
	else
		{
		iInlinePrompt.Copy(_L("Wpisz utwor lokalnie:"));
		}
	DrawNow();
	}

void CNokiaspotifyAppView::ShowTrackListL(const RPointerArray<HBufC>& aTracks, const TDesC& aTitle)
	{
	iNowPlayingVisible = EFalse;
	iTrackListVisible = ETrue;
	iTrackCount = (aTracks.Count() < 24) ? aTracks.Count() : 24;
	iTrackSelection = 0;
	iListTitle.Copy(aTitle.Left(iListTitle.MaxLength()));
	for (TInt i = 0; i < iTrackCount; ++i)
		{
		iTrackItems[i].Copy((*aTracks[i]).Left(iTrackItems[i].MaxLength()));
		}
	DrawNow();
	}

void CNokiaspotifyAppView::ShowHomeScreen()
	{
	iNowPlayingVisible = EFalse;
	iTrackListVisible = EFalse;
	iTrackCount = 0;
	iTrackSelection = 0;
	iListTitle.Zero();
	DrawNow();
	}

void CNokiaspotifyAppView::SetPlaybackPanel(const TDesC& aTitle, const TDesC& aDetail)
	{
	iPlaybackPanelVisible = ETrue;
	iPlaybackTitle.Copy(aTitle.Left(iPlaybackTitle.MaxLength()));
	iPlaybackDetail.Copy(aDetail.Left(iPlaybackDetail.MaxLength()));
	DrawNow();
	}

void CNokiaspotifyAppView::ClearPlaybackPanel()
	{
	iPlaybackPanelVisible = EFalse;
	iPlaybackTitle.Zero();
	iPlaybackDetail.Zero();
	DrawNow();
	}

void CNokiaspotifyAppView::SetNowPlayingState(const TDesC& aTitle, const TDesC& aDetail, const TDesC& aStatus, TBool aShuffleEnabled)
	{
	iNowPlayingTitle.Copy(aTitle.Left(iNowPlayingTitle.MaxLength()));
	iNowPlayingDetail.Copy(aDetail.Left(iNowPlayingDetail.MaxLength()));
	iNowPlayingStatus.Copy(aStatus.Left(iNowPlayingStatus.MaxLength()));
	iNowPlayingShuffle = aShuffleEnabled;
	DrawNow();
	}

void CNokiaspotifyAppView::ShowNowPlayingScreen()
	{
	iNowPlayingVisible = ETrue;
	iTrackListVisible = EFalse;
	DrawNow();
	}

TBool CNokiaspotifyAppView::IsNowPlayingVisible() const
	{
	return iNowPlayingVisible;
	}

void CNokiaspotifyAppView::ExecuteSelectedMenuItemL()
	{
	CEikAppUi* ui = CEikonEnv::Static()->EikAppUi();
	CNokiaspotifyAppUi* appUi = STATIC_CAST(CNokiaspotifyAppUi*, ui);
	switch (iMenuSelection)
		{
		case 0:
			appUi->HandleQuickSearchFromViewL();
			break;
		case 1:
			appUi->HandleQuickShowTrackListFromViewL();
			break;
		case 2:
			appUi->HandleQuickToggleInternetFromViewL();
			break;
		case 3:
			appUi->HandleQuickOnlineSearchFromViewL();
			break;
		case 4:
			appUi->HandleQuickReindexLibraryFromViewL();
			break;
		case 5:
			appUi->HandleQuickCleanCacheFromViewL();
			break;
		default:
			appUi->HandleQuickPingFromViewL();
			break;
		}
	}

void CNokiaspotifyAppView::SubmitInlineInputL()
	{
	if (!iInlineInputActive)
		{
		return;
		}

	TBuf<96> query;
	query.Copy(iInlineInput);
	const TBool online = iInlineInputOnline;
	iInlineInputActive = EFalse;
	iInlineInputOnline = EFalse;
	iInlineInput.Zero();
	DrawNow();

	CEikAppUi* ui = CEikonEnv::Static()->EikAppUi();
	CNokiaspotifyAppUi* appUi = STATIC_CAST(CNokiaspotifyAppUi*, ui);
	if (online)
		{
		appUi->HandleInlineOnlineSearchQueryFromViewL(query);
		}
	else
		{
		appUi->HandleInlineSearchQueryFromViewL(query);
		}
	}

TKeyResponse CNokiaspotifyAppView::OfferKeyEventL(
		const TKeyEvent& aKeyEvent,
		TEventCode aType)
	{
	if (aType == EEventKey && aKeyEvent.iRepeats == 0)
		{
		const TInt c = static_cast<TInt>(aKeyEvent.iCode);
		CEikAppUi* ui = CEikonEnv::Static()->EikAppUi();
		CNokiaspotifyAppUi* appUi = STATIC_CAST(CNokiaspotifyAppUi*, ui);
		if (iInlineInputActive)
			{
			if (c == EKeyEnter || c == EStdKeyDevice3)
				{
				SubmitInlineInputL();
				return EKeyWasConsumed;
				}
			if (c == EKeyBackspace)
				{
				if (iInlineInput.Length() > 0)
					{
					iInlineInput.SetLength(iInlineInput.Length() - 1);
					DrawNow();
					}
				return EKeyWasConsumed;
				}
			if (c == '#')
				{
				iInlineInputActive = EFalse;
				iInlineInputOnline = EFalse;
				iInlineInput.Zero();
				DrawNow();
				return EKeyWasConsumed;
				}
			if (c >= 32 && c <= 126 && iInlineInput.Length() < iInlineInput.MaxLength())
				{
				iInlineInput.Append((TText)c);
				DrawNow();
				return EKeyWasConsumed;
				}
			return EKeyWasConsumed;
			}
		if (iNowPlayingVisible)
			{
			if (c == '0' || c == EKeyBackspace)
				{
				ShowHomeScreen();
				return EKeyWasConsumed;
				}
			if (c == '4')
				{
				appUi->HandlePlaybackPrevFromViewL();
				return EKeyWasConsumed;
				}
			if (c == '5' || c == EStdKeyDevice3 || c == EKeyEnter)
				{
				appUi->HandlePlaybackToggleFromViewL();
				return EKeyWasConsumed;
				}
			if (c == '6')
				{
				appUi->HandlePlaybackNextFromViewL();
				return EKeyWasConsumed;
				}
