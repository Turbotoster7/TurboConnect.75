/*
 ============================================================================
 Name		: NokiaspotifyAppUi.cpp
 Author	  :
 Copyright   : Your copyright notice
 Description : CNokiaspotifyAppUi implementation
 ============================================================================
 */

// INCLUDE FILES
#include <avkon.hrh>
#include <avkon.rsg>
#include <aknmessagequerydialog.h>
#include <aknnotewrappers.h>
#include <aknquerydialog.h>
#include <apgcli.h>
#include <coemain.h>
#include <e32std.h>
#include <f32file.h>
#include <in_sock.h>
#include <s32file.h>
#include <stringloader.h>

#include <Nokiaspotify_0xE0A04525.rsg>

#ifdef _HELP_AVAILABLE_
//#include "Nokiaspotify_0xE0A04525.hlp.hrh"
#endif
#include "Nokiaspotify.hrh"
#include "Nokiaspotify.pan"
#include "NokiaspotifyApplication.h"
#include "NokiaspotifyAppUi.h"
#include "NokiaspotifyAppView.h"
#include "CNokiaspotifyNetwork.h"

_LIT(KinformationText,
	"TurboMusic dziala bez logowania.\n"
	"Cache audio jest automatycznie zarzadzany (LRU).\n"
	"Menu Login: polacz/rozlacz internet.");
_LIT(KinformationHead, "TurboMusic");
_LIT(KCacheDirOnE, "E:\\Data\\TurboMusic\\Cache\\");
_LIT(KCacheDirOnC, "C:\\Data\\TurboMusic\\Cache\\");
_LIT(KDataDirOnE, "E:\\Data\\TurboMusic\\");
_LIT(KDataDirOnC, "C:\\Data\\TurboMusic\\");
_LIT(KPlaylistsFileName, "playlists.txt");
_LIT(KLibraryIndexFileName, "library_index.txt");
_LIT(KMusicDirE, "E:\\Music\\");
_LIT(KMusicDirC, "C:\\Data\\Sounds\\Digital\\");
_LIT(KTurboMusicDirE, "E:\\Data\\TurboMusic\\Music\\");
_LIT(KTurboMusicDirC, "C:\\Data\\TurboMusic\\Music\\");

const TInt64 KOneMb = 1024 * 1024;
const TInt KTurboCacheDefaultDrive = EDriveE;
const TInt KTrackListMax = 24;
const TInt KOnlineSearchMaxResults = 10;
const TInt KOnlineSearchResponseBytes = 16384;

#if defined(R_AVKON_DIALOG_QUERY_VALUE)
const TInt KTurboTextQueryRes = R_AVKON_DIALOG_QUERY_VALUE;
#elif defined(R_AVKON_TEXT_QUERY_DIALOG)
const TInt KTurboTextQueryRes = R_AVKON_TEXT_QUERY_DIALOG;
#elif defined(R_AVKON_SINGLE_LINE_QUERY_DIALOG)
const TInt KTurboTextQueryRes = R_AVKON_SINGLE_LINE_QUERY_DIALOG;
#else
const TInt KTurboTextQueryRes = -1;
#endif

static TBool PromptTextL(TDes& aBuffer, const TDesC& aPrompt)
	{
	if (KTurboTextQueryRes < 0)
		{
		_LIT(KNoDialogRes, "SDK bez dialogu tekstu: uzywam wartosci domyslnej.");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KNoDialogRes);
		return ETrue;
		}

	CAknTextQueryDialog* dlg = new (ELeave) CAknTextQueryDialog(aBuffer, CAknQueryDialog::ENoTone);
	dlg->SetPromptL(aPrompt);
	return dlg->ExecuteLD(KTurboTextQueryRes);
	}

static HBufC* UrlEncodeSimpleLC(const TDesC& aText)
	{
	// Wystarczy do query: spacje + podstawowe znaki specjalne.
	HBufC* out = HBufC::NewLC(aText.Length() * 3 + 1);
	TPtr p = out->Des();
	for (TInt i = 0; i < aText.Length(); ++i)
		{
		const TChar ch = aText[i];
		const TUint c = ch;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
			{
			p.Append(ch);
			}
		else if (c == ' ')
			{
			p.Append(_L("%20"));
			}
		else
			{
			TBuf<4> hex;
			hex.Format(_L("%%%02X"), c & 0xFF);
			p.Append(hex);
			}
		}
	return out;
	}

static HBufC8* ToAscii8LC(const TDesC& aText)
	{
	HBufC8* out = HBufC8::NewLC(aText.Length() + 1);
	TPtr8 p = out->Des();
	for (TInt i = 0; i < aText.Length(); ++i)
		{
		TUint c = aText[i];
		if (c > 0x7F)
			{
			c = '?';
			}
		p.Append((TUint8)c);
		}
	return out;
	}

static void AppendLimited(TDes& aDst, const TDesC& aText)
	{
	const TInt free = aDst.MaxLength() - aDst.Length();
	if (free <= 0)
		{
		return;
		}
	if (aText.Length() <= free)
		{
		aDst.Append(aText);
		}
	else
		{
		aDst.Append(aText.Left(free));
		}
	}

static void AppendLineLimited(TDes& aDst, const TDesC& aText)
	{
	AppendLimited(aDst, aText);
	AppendLimited(aDst, _L("\n"));
	}

static void TrimAsciiLine(TPtrC& aLine)
	{
	while (aLine.Length() > 0 && (aLine[0] == ' ' || aLine[0] == '\t' || aLine[0] == '\r'))
		{
		aLine.Set(aLine.Mid(1));
		}
	while (aLine.Length() > 0)
		{
		const TUint c = aLine[aLine.Length() - 1];
		if (c == ' ' || c == '\t' || c == '\r')
			{
			aLine.Set(aLine.Left(aLine.Length() - 1));
			}
		else
			{
			break;
			}
		}
	}

static void AppendOwnedBufL(RPointerArray<HBufC>& aOut, const TDesC& aText);
static void CopyAscii8ToDes(TDes& aOut, const TDesC8& aIn);

static TBool IsLikelyTrackLine(const TDesC& aLine)
	{
	if (aLine.Length() < 2)
		{
		return EFalse;
		}
	if (aLine[0] == '<')
		{
		return EFalse;
		}
	if (aLine.FindF(_L("[ GRAJ ]")) >= 0 || aLine.FindF(_L("[ POBIERZ ]")) >= 0)
		{
		return EFalse;
		}
	if (aLine.FindF(_L("TURBOCONNECT")) >= 0 || aLine.FindF(_L("POBIERZ NOWY")) >= 0 || aLine.FindF(_L("FFmpeg")) >= 0)
		{
		return EFalse;
		}
	return ETrue;
	}

static TBool IsLikelyTrackLine8(const TDesC8& aLine)
	{
	if (aLine.Length() < 2)
		{
		return EFalse;
		}
	if (aLine[0] == '<')
		{
		return EFalse;
		}
	if (aLine.FindF(_L8("[ GRAJ ]")) >= 0 || aLine.FindF(_L8("[ POBIERZ ]")) >= 0)
		{
		return EFalse;
		}
	if (aLine.FindF(_L8("TURBOCONNECT")) >= 0 || aLine.FindF(_L8("POBIERZ NOWY")) >= 0 || aLine.FindF(_L8("FFmpeg")) >= 0)
		{
		return EFalse;
		}
	return ETrue;
	}

static void AppendOwnedBufFrom8L(RPointerArray<HBufC>& aOut, const TDesC8& aText8)
	{
	TBuf<256> temp;
	const TInt n = (aText8.Length() < temp.MaxLength()) ? aText8.Length() : temp.MaxLength();
	for (TInt i = 0; i < n; ++i)
		{
		temp.Append((TText)aText8[i]);
		}
	AppendOwnedBufL(aOut, temp);
	}
