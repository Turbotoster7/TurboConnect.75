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

static void ExtractTrackLinesFromHtml8L(const TDesC8& aHtml, RPointerArray<HBufC>& aOut, TInt aLimit)
	{
	TInt pos = 0;
	while (pos < aHtml.Length() && aOut.Count() < aLimit)
		{
		TInt end = aHtml.Mid(pos).Locate('\n');
		if (end < 0)
			{
			end = aHtml.Length() - pos;
			}
		TPtrC8 line = aHtml.Mid(pos, end);
		while (line.Length() > 0 && (line[0] == ' ' || line[0] == '\t' || line[0] == '\r'))
			{
			line.Set(line.Mid(1));
			}
		while (line.Length() > 0)
			{
			const TUint c = line[line.Length() - 1];
			if (c == ' ' || c == '\t' || c == '\r')
				{
				line.Set(line.Left(line.Length() - 1));
				}
			else
				{
				break;
				}
			}
		if (IsLikelyTrackLine8(line))
			{
			AppendOwnedBufFrom8L(aOut, line);
			}
		pos += end + 1;
		}
	}

static TPtrC8 HttpBodyFromResponse8(const TDesC8& aResp)
	{
	const TInt bodyPos = aResp.Find(_L8("\r\n\r\n"));
	return (bodyPos >= 0) ? aResp.Mid(bodyPos + 4) : aResp.Left(aResp.Length());
	}

static TBool ExtractFirstDownloadLinkFromHtml8(const TDesC8& aHtml, TDes& aUrlOut)
	{
	const TInt pos = aHtml.Find(_L8("/library/file/"));
	if (pos < 0)
		{
		return EFalse;
		}
	TPtrC8 rest = aHtml.Mid(pos);
	TInt end = rest.Locate('"');
	if (end < 0)
		{
		end = rest.Locate('\'');
		}
	if (end < 0)
		{
		end = rest.Length();
		}
	CopyAscii8ToDes(aUrlOut, rest.Left(end));
	return aUrlOut.Length() > 0;
	}

static HBufC8* HttpGetSmallResponseL(const TDesC& aHost, const TDesC& aPath, TInt aMaxBytes)
	{
	RSocketServ ss;
	User::LeaveIfError(ss.Connect());
	CleanupClosePushL(ss);

	RHostResolver resolver;
	User::LeaveIfError(resolver.Open(ss, KAfInet, KProtocolInetUdp));
	CleanupClosePushL(resolver);
	TNameEntry nameEntry;
	User::LeaveIfError(resolver.GetByName(aHost, nameEntry));
	TInetAddr addr = TInetAddr::Cast(nameEntry().iAddr);
	addr.SetPort(80);

	RSocket sock;
	User::LeaveIfError(sock.Open(ss, KAfInet, KSockStream, KProtocolInetTcp));
	CleanupClosePushL(sock);
	TRequestStatus st;
	sock.Connect(addr, st);
	User::WaitForRequest(st);
	User::LeaveIfError(st.Int());

	HBufC8* path8 = ToAscii8LC(aPath);
	HBufC8* req = HBufC8::NewLC(path8->Length() + 128);
	req->Des().Copy(_L8("GET "));
	req->Des().Append(*path8);
	req->Des().Append(_L8(" HTTP/1.0\r\nHost: turboconect.pl\r\nConnection: close\r\n\r\n"));

	sock.Write(req->Des(), st);
	User::WaitForRequest(st);
	User::LeaveIfError(st.Int());

	HBufC8* resp8 = HBufC8::NewLC(aMaxBytes);
	TPtr8 resp = resp8->Des();
	for (;;)
		{
		TBuf8<1024> chunk;
		TSockXfrLength len;
		sock.RecvOneOrMore(chunk, 0, st, len);
		User::WaitForRequest(st);
		if (st.Int() == KErrEof || chunk.Length() == 0)
			{
			break;
			}
		User::LeaveIfError(st.Int());
		if (resp.Length() + chunk.Length() > resp.MaxLength())
			{
			break;
			}
		resp.Append(chunk);
		}

	CleanupStack::Pop(resp8);
	CleanupStack::PopAndDestroy(req);
	CleanupStack::PopAndDestroy(path8);
	CleanupStack::PopAndDestroy(&sock);
	CleanupStack::PopAndDestroy(&resolver);
	CleanupStack::PopAndDestroy(&ss);
	return resp8;
	}

static void ExtractTrackLinesFromHtmlL(const TDesC& aHtml, RPointerArray<HBufC>& aOut, TInt aLimit)
	{
	TInt pos = 0;
	while (pos < aHtml.Length() && aOut.Count() < aLimit)
		{
		TInt end = aHtml.Mid(pos).Locate('\n');
		if (end < 0)
			{
			end = aHtml.Length() - pos;
			}
		TPtrC line = aHtml.Mid(pos, end);
		TrimAsciiLine(line);
		if (IsLikelyTrackLine(line))
			{
			AppendOwnedBufL(aOut, line);
			}
		pos += end + 1;
		}
	}

static TBool FirstTrackNameL(RFs& aFs, TDes& aOut)
	{
	CDir* dir = NULL;
	TFileName pattern(KMusicDirE);
	pattern.Append(_L("*.mp3"));
	if (aFs.GetDir(pattern, KEntryAttNormal, ESortByName, dir) == KErrNone && dir && dir->Count() > 0)
		{
		aOut.Copy((*dir)[0].iName);
		delete dir;
		return ETrue;
		}
	delete dir;
	dir = NULL;
	TFileName pattern2(KMusicDirC);
	pattern2.Append(_L("*.mp3"));
	if (aFs.GetDir(pattern2, KEntryAttNormal, ESortByName, dir) == KErrNone && dir && dir->Count() > 0)
		{
		aOut.Copy((*dir)[0].iName);
		delete dir;
		return ETrue;
		}
	delete dir;
	return EFalse;
	}

static void EnsureFolderL(RFs& aFs, const TDesC& aFolder)
	{
	const TInt err = aFs.MkDirAll(aFolder);
	if (err != KErrNone && err != KErrAlreadyExists)
		{
		User::Leave(err);
		}
	}

static void CopyFileSimpleL(RFs& aFs, const TDesC& aSource, const TDesC& aTarget)
	{
	RFile inFile;
	User::LeaveIfError(inFile.Open(aFs, aSource, EFileRead | EFileShareReadersOnly));
	CleanupClosePushL(inFile);

	RFile outFile;
	User::LeaveIfError(outFile.Replace(aFs, aTarget, EFileWrite | EFileShareExclusive));
	CleanupClosePushL(outFile);

	TBuf8<1024> buffer;
	for (;;)
		{
		buffer.Zero();
		User::LeaveIfError(inFile.Read(buffer));
		if (buffer.Length() == 0)
			{
			break;
			}
		User::LeaveIfError(outFile.Write(buffer));
		}

	CleanupStack::PopAndDestroy(&outFile);
	CleanupStack::PopAndDestroy(&inFile);
	}

static void AppendLineToTextFileL(RFs& aFs, const TDesC& aPath, const TDesC& aLine)
	{
	RFile file;
	TInt openErr = file.Open(aFs, aPath, EFileWrite | EFileShareAny);
	if (openErr == KErrNotFound)
		{
		User::LeaveIfError(file.Create(aFs, aPath, EFileWrite | EFileShareAny));
		}
	else
		{
		User::LeaveIfError(openErr);
		}
	CleanupClosePushL(file);
	TInt pos = 0;
	User::LeaveIfError(file.Seek(ESeekEnd, pos));

	TFileText writer;
	writer.Set(file);
	User::LeaveIfError(writer.Write(aLine));
	User::LeaveIfError(writer.Write(_L("\r\n")));
	CleanupStack::PopAndDestroy(&file);
	}

static void RewriteTextFileL(RFs& aFs, const TDesC& aPath, const RPointerArray<HBufC>& aLines)
	{
	RFile file;
	TInt err = file.Replace(aFs, aPath, EFileWrite | EFileShareAny);
	if (err == KErrPathNotFound)
		{
		TFileName parent(aPath);
		const TInt slash = parent.LocateReverse('\\');
		if (slash > 2)
			{
			parent.SetLength(slash + 1);
			EnsureFolderL(aFs, parent);
			}
		err = file.Replace(aFs, aPath, EFileWrite | EFileShareAny);
		}
	User::LeaveIfError(err);
	CleanupClosePushL(file);
	TFileText writer;
	writer.Set(file);
	for (TInt i = 0; i < aLines.Count(); ++i)
		{
		User::LeaveIfError(writer.Write(*aLines[i]));
		User::LeaveIfError(writer.Write(_L("\r\n")));
		}
	CleanupStack::PopAndDestroy(&file);
	}

static void CleanupOwnedBufArray(TAny* aPtr)
	{
	RPointerArray<HBufC>* arr = (RPointerArray<HBufC>*)aPtr;
	if (arr)
		{
		arr->ResetAndDestroy();
		arr->Close();
		}
	}

static void AppendOwnedBufL(RPointerArray<HBufC>& aOut, const TDesC& aText)
	{
	HBufC* item = aText.AllocLC();
	aOut.AppendL(item);
	CleanupStack::Pop(item);
	}

static void ReadTextFileLinesL(RFs& aFs, const TDesC& aPath, RPointerArray<HBufC>& aOut)
	{
	RFile file;
	if (file.Open(aFs, aPath, EFileRead | EFileShareReadersOnly) != KErrNone)
		{
		return;
		}
	CleanupClosePushL(file);
	TFileText reader;
	reader.Set(file);
	TBuf<256> line;
	for (;;)
		{
		const TInt err = reader.Read(line);
		if (err == KErrEof)
			{
			break;
			}
		User::LeaveIfError(err);
		if (line.Length() > 0)
			{
			AppendOwnedBufL(aOut, line);
			}
		}
	CleanupStack::PopAndDestroy(&file);
	}

static void ScanMusicFilesInDirL(RFs& aFs, const TDesC& aDir, const TDesC& aQuery, RPointerArray<HBufC>& aOut)
	{
	CDir* mp3 = NULL;
	TFileName mp3Pattern(aDir);
	mp3Pattern.Append(_L("*.mp3"));
	if (aFs.GetDir(mp3Pattern, KEntryAttNormal, ESortByName, mp3) == KErrNone)
		{
		CleanupStack::PushL(mp3);
		for (TInt i = 0; i < mp3->Count(); ++i)
			{
			const TEntry& e = (*mp3)[i];
			if (aQuery.Length() == 0 || e.iName.FindF(aQuery) >= 0)
				{
				AppendOwnedBufL(aOut, e.iName);
				if (aOut.Count() >= 40)
					{
					break;
					}
				}
			}
		CleanupStack::PopAndDestroy(mp3);
		}

	CDir* m4a = NULL;
	TFileName m4aPattern(aDir);
	m4aPattern.Append(_L("*.m4a"));
	if (aFs.GetDir(m4aPattern, KEntryAttNormal, ESortByName, m4a) == KErrNone)
		{
		CleanupStack::PushL(m4a);
		for (TInt i = 0; i < m4a->Count(); ++i)
			{
			const TEntry& e = (*m4a)[i];
			if (aQuery.Length() == 0 || e.iName.FindF(aQuery) >= 0)
				{
				AppendOwnedBufL(aOut, e.iName);
				if (aOut.Count() >= 40)
					{
					break;
					}
				}
			}
		CleanupStack::PopAndDestroy(m4a);
		}
	}

static TBool FindFirstMusicFileInDirL(RFs& aFs, const TDesC& aDir, const TDesC& aQuery, TDes& aOut)
	{
	CDir* mp3 = NULL;
	TFileName mp3Pattern(aDir);
	mp3Pattern.Append(_L("*.mp3"));
	if (aFs.GetDir(mp3Pattern, KEntryAttNormal, ESortByName, mp3) == KErrNone)
		{
		CleanupStack::PushL(mp3);
		for (TInt i = 0; i < mp3->Count(); ++i)
			{
			const TEntry& e = (*mp3)[i];
			if (aQuery.Length() == 0 || e.iName.FindF(aQuery) >= 0)
