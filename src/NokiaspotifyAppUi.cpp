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
				{
				aOut.Copy(aDir);
				aOut.Append(e.iName);
				CleanupStack::PopAndDestroy(mp3);
				return ETrue;
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
				aOut.Copy(aDir);
				aOut.Append(e.iName);
				CleanupStack::PopAndDestroy(m4a);
				return ETrue;
				}
			}
		CleanupStack::PopAndDestroy(m4a);
		}
	return EFalse;
	}

static void AppendMusicPathsInDirL(RFs& aFs, const TDesC& aDir, RPointerArray<HBufC>& aOut)
	{
	CDir* mp3 = NULL;
	TFileName mp3Pattern(aDir);
	mp3Pattern.Append(_L("*.mp3"));
	if (aFs.GetDir(mp3Pattern, KEntryAttNormal, ESortByName, mp3) == KErrNone)
		{
		CleanupStack::PushL(mp3);
		for (TInt i = 0; i < mp3->Count() && aOut.Count() < 24; ++i)
			{
			TFileName path(aDir);
			path.Append((*mp3)[i].iName);
			AppendOwnedBufL(aOut, path);
			}
		CleanupStack::PopAndDestroy(mp3);
		}

	CDir* m4a = NULL;
	TFileName m4aPattern(aDir);
	m4aPattern.Append(_L("*.m4a"));
	if (aFs.GetDir(m4aPattern, KEntryAttNormal, ESortByName, m4a) == KErrNone)
		{
		CleanupStack::PushL(m4a);
		for (TInt i = 0; i < m4a->Count() && aOut.Count() < 24; ++i)
			{
			TFileName path(aDir);
			path.Append((*m4a)[i].iName);
			AppendOwnedBufL(aOut, path);
			}
		CleanupStack::PopAndDestroy(m4a);
		}
	}

static void CopyAscii8ToDes(TDes& aOut, const TDesC8& aIn)
	{
	aOut.Zero();
	const TInt max = (aIn.Length() < aOut.MaxLength()) ? aIn.Length() : aOut.MaxLength();
	for (TInt i = 0; i < max; ++i)
		{
		aOut.Append((TText)aIn[i]);
		}
	}

static void SanitizeFileName(TDes& aName)
	{
	for (TInt i = 0; i < aName.Length(); ++i)
		{
		const TText ch = aName[i];
		if (ch < 32 || ch == '\\' || ch == '/' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|')
			{
			aName[i] = '_';
			}
		}
	}

static void ResolveMusicDirL(RFs& aFs, TDes& aOut)
	{
	TDriveInfo info;
	if (aFs.Drive(info, EDriveE) == KErrNone && info.iType != EMediaNotPresent)
		{
		aOut.Copy(KTurboMusicDirE);
		}
	else
		{
		aOut.Copy(KTurboMusicDirC);
		}
	}

static void LeafNameFromPath(const TDesC& aPath, TDes& aOut)
	{
	const TInt slash = aPath.LocateReverse('\\');
	if (slash >= 0 && slash + 1 < aPath.Length())
		{
		aOut.Copy(aPath.Mid(slash + 1));
		}
	else
		{
		aOut.Copy(aPath.Left(aOut.MaxLength()));
		}
	}

class CTurboMusicCacheManager : public CBase
	{
public:
	static CTurboMusicCacheManager* NewL();
	~CTurboMusicCacheManager();

	void RecalculatePolicyL();
	void TrimIfNeededL();
	TInt64 CacheUsedBytesL();
	TInt64 CacheLimitBytes();
	const TDesC& CacheDir();

private:
	CTurboMusicCacheManager();
	void ConstructL();
	void SelectCacheDriveL();
	void EnsureCacheDirectoryL();
	TInt64 ComputeRecommendedLimitBytesL();
	void ScanCacheL(TInt64& aTotalBytes, TFileName& aOldestFile, TTime& aOldestTime, TBool& aHasOldest);

private:
	RFs iFs;
	TFileName iCacheDir;
	TInt iCacheDrive;
	TInt64 iCacheLimitBytes;
	};

CTurboMusicCacheManager* CTurboMusicCacheManager::NewL()
	{
	CTurboMusicCacheManager* self = new (ELeave) CTurboMusicCacheManager();
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

CTurboMusicCacheManager::CTurboMusicCacheManager()
	: iCacheDrive(EDriveC)
	, iCacheLimitBytes(100 * KOneMb)
	{
	}

void CTurboMusicCacheManager::ConstructL()
	{
	User::LeaveIfError(iFs.Connect());
	SelectCacheDriveL();
	EnsureCacheDirectoryL();
	RecalculatePolicyL();
	TrimIfNeededL();
	}

CTurboMusicCacheManager::~CTurboMusicCacheManager()
	{
	iFs.Close();
	}

void CTurboMusicCacheManager::SelectCacheDriveL()
	{
	TDriveInfo info;
	if (iFs.Drive(info, KTurboCacheDefaultDrive) == KErrNone &&
		info.iType != EMediaNotPresent)
		{
		iCacheDrive = KTurboCacheDefaultDrive;
		iCacheDir = KCacheDirOnE;
		}
	else
		{
		iCacheDrive = EDriveC;
		iCacheDir = KCacheDirOnC;
		}
	}

void CTurboMusicCacheManager::EnsureCacheDirectoryL()
	{
	const TInt err = iFs.MkDirAll(iCacheDir);
	if (err != KErrNone && err != KErrAlreadyExists)
		{
		User::Leave(err);
		}
	}

TInt64 CTurboMusicCacheManager::ComputeRecommendedLimitBytesL()
	{
	TVolumeInfo vol;
	User::LeaveIfError(iFs.Volume(vol, iCacheDrive));
	TInt64 freeBytes = vol.iFree;
	TInt64 recommended = freeBytes / 10; // 10% wolnego miejsca
	const TInt64 minLimit = 64 * KOneMb;
	const TInt64 maxLimit = 256 * KOneMb;
	if (recommended < minLimit)
		{
		recommended = minLimit;
		}
	if (recommended > maxLimit)
		{
		recommended = maxLimit;
		}
	// Dla ~1GB wolnego otrzymujemy ~102MB, czyli praktycznie oczekiwane 100MB.
	return recommended;
	}

void CTurboMusicCacheManager::RecalculatePolicyL()
	{
	iCacheLimitBytes = ComputeRecommendedLimitBytesL();
	}

void CTurboMusicCacheManager::ScanCacheL(
		TInt64& aTotalBytes,
		TFileName& aOldestFile,
		TTime& aOldestTime,
		TBool& aHasOldest)
	{
	aTotalBytes = 0;
	aHasOldest = EFalse;
	CDir* dir = NULL;
	User::LeaveIfError(iFs.GetDir(iCacheDir, KEntryAttNormal, ESortNone, dir));
	CleanupStack::PushL(dir);
	for (TInt i = 0; i < dir->Count(); ++i)
		{
		const TEntry& entry = (*dir)[i];
		if (entry.IsDir())
			{
			continue;
			}
		aTotalBytes += entry.iSize;
		if (!aHasOldest || entry.iModified < aOldestTime)
			{
			aHasOldest = ETrue;
			aOldestTime = entry.iModified;
			aOldestFile = iCacheDir;
			aOldestFile.Append(entry.iName);
			}
		}
	CleanupStack::PopAndDestroy(dir);
	}

void CTurboMusicCacheManager::TrimIfNeededL()
	{
	for (;;)
		{
		TInt64 total = 0;
		TFileName oldestFile;
		TTime oldestTime(0);
		TBool hasOldest = EFalse;
		ScanCacheL(total, oldestFile, oldestTime, hasOldest);
		if (total <= iCacheLimitBytes || !hasOldest)
			{
			break;
			}
		User::LeaveIfError(iFs.Delete(oldestFile));
		}
	}

TInt64 CTurboMusicCacheManager::CacheUsedBytesL()
	{
	TInt64 total = 0;
	TFileName oldestFile;
	TTime oldestTime(0);
	TBool hasOldest = EFalse;
	ScanCacheL(total, oldestFile, oldestTime, hasOldest);
	return total;
	}

TInt64 CTurboMusicCacheManager::CacheLimitBytes()
	{
	return iCacheLimitBytes;
	}

const TDesC& CTurboMusicCacheManager::CacheDir()
	{
	return iCacheDir;
	}

class CTurboTrackEntry : public CBase
	{
public:
	static CTurboTrackEntry* NewLC(
		const TDesC& aDisplay,
		const TDesC* aLocalPath,
		const TDesC* aRemoteUrl,
		const TDesC* aFileName,
		TBool aCached,
		TBool aPermanent,
		TBool aOnlineOnly)
		{
		CTurboTrackEntry* self = new (ELeave) CTurboTrackEntry;
		CleanupStack::PushL(self);
		self->iDisplay = aDisplay.AllocL();
		if (aLocalPath)
			{
			self->iLocalPath = aLocalPath->AllocL();
			}
		if (aRemoteUrl)
			{
			self->iRemoteUrl = aRemoteUrl->AllocL();
			}
		if (aFileName)
			{
			self->iFileName = aFileName->AllocL();
			}
		self->iCached = aCached;
		self->iPermanent = aPermanent;
		self->iOnlineOnly = aOnlineOnly;
		return self;
		}

	~CTurboTrackEntry()
		{
		delete iDisplay;
		delete iLocalPath;
		delete iRemoteUrl;
		delete iFileName;
		}

	void SetDisplayL(const TDesC& aDisplay)
		{
		delete iDisplay;
		iDisplay = NULL;
		iDisplay = aDisplay.AllocL();
		}

	void SetLocalPathL(const TDesC& aPath)
		{
		delete iLocalPath;
		iLocalPath = NULL;
		iLocalPath = aPath.AllocL();
		}

	void SetFileNameL(const TDesC& aFileName)
		{
		delete iFileName;
		iFileName = NULL;
		iFileName = aFileName.AllocL();
		}

public:
	HBufC* iDisplay;
	HBufC* iLocalPath;
	HBufC* iRemoteUrl;
	HBufC* iFileName;
	TBool iCached;
	TBool iPermanent;
	TBool iOnlineOnly;
	};

static CTurboTrackEntry* CloneTrackEntryLC(const CTurboTrackEntry& aEntry)
	{
	TPtrC display(KNullDesC);
	if (aEntry.iDisplay)
		{
		display.Set(*aEntry.iDisplay);
		}
	return CTurboTrackEntry::NewLC(
		display,
		aEntry.iLocalPath,
		aEntry.iRemoteUrl,
		aEntry.iFileName,
		aEntry.iCached,
		aEntry.iPermanent,
		aEntry.iOnlineOnly);
	}

static void CleanupTrackArray(TAny* aPtr)
	{
	RPointerArray<CTurboTrackEntry>* arr = (RPointerArray<CTurboTrackEntry>*)aPtr;
	if (arr)
		{
		arr->ResetAndDestroy();
		arr->Close();
		}
	}

class CTurboMusicService : public CBase
	{
public:
	static CTurboMusicService* NewL(CTurboMusicCacheManager& aCacheManager);
	~CTurboMusicService();

	void SearchHybridL(const TDesC& aQuery, RPointerArray<CTurboTrackEntry>& aOut);
	void SearchOnlineOnlyL(const TDesC& aQuery, RPointerArray<CTurboTrackEntry>& aOut);
	void ListLibraryL(RPointerArray<CTurboTrackEntry>& aOut);
	void PrepareTrackForPlaybackL(CTurboTrackEntry& aEntry, TDes& aLocalPath, TBool& aFromInternet);
	void SaveTrackPermanentL(CTurboTrackEntry& aEntry);
	void DeleteTrackL(CTurboTrackEntry& aEntry);
	TInt CleanCacheL();
	void TrimCacheL();

private:
	CTurboMusicService(CTurboMusicCacheManager& aCacheManager);
	void ConstructL();
	void AppendLocalMatchesInDirL(RFs& aFs, const TDesC& aDir, const TDesC& aQuery, TBool aPermanent, RPointerArray<CTurboTrackEntry>& aOut, TInt aLimit);
	void AppendAllInDirL(RFs& aFs, const TDesC& aDir, TBool aPermanent, RPointerArray<CTurboTrackEntry>& aOut, TInt aLimit);
	void AppendRemoteSearchResultsL(const TDesC& aQuery, RPointerArray<CTurboTrackEntry>& aOut, TInt aLimit);
	void FetchQueryToServerL(const TDesC& aQuery);
	void DownloadToCacheL(CTurboTrackEntry& aEntry, TDes& aOutPath);
	void ResolvePermanentMusicDirL(RFs& aFs, TDes& aOut);
	void BuildPrefixedDisplay(const TDesC& aPrefix, const TDesC& aName, TDes& aOut);

private:
	CTurboMusicCacheManager& iCacheManager;
	};

CTurboMusicService* CTurboMusicService::NewL(CTurboMusicCacheManager& aCacheManager)
	{
	CTurboMusicService* self = new (ELeave) CTurboMusicService(aCacheManager);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

CTurboMusicService::CTurboMusicService(CTurboMusicCacheManager& aCacheManager)
	: iCacheManager(aCacheManager)
	{
	}

void CTurboMusicService::ConstructL()
	{
	TrimCacheL();
	}

CTurboMusicService::~CTurboMusicService()
	{
	}

void CTurboMusicService::BuildPrefixedDisplay(const TDesC& aPrefix, const TDesC& aName, TDes& aOut)
	{
	aOut.Copy(aPrefix.Left(aOut.MaxLength()));
	const TInt free = aOut.MaxLength() - aOut.Length();
	if (free > 0)
		{
		if (aName.Length() <= free)
			{
			aOut.Append(aName);
			}
		else
			{
			aOut.Append(aName.Left(free));
			}
		}
	}

void CTurboMusicService::AppendLocalMatchesInDirL(RFs& aFs, const TDesC& aDir, const TDesC& aQuery, TBool aPermanent, RPointerArray<CTurboTrackEntry>& aOut, TInt aLimit)
	{
	CDir* dir = NULL;
	TFileName pattern(aDir);
	pattern.Append(_L("*.*"));
	if (aFs.GetDir(pattern, KEntryAttNormal, ESortByName, dir) != KErrNone)
		{
		return;
		}
	CleanupStack::PushL(dir);
	for (TInt i = 0; i < dir->Count() && aOut.Count() < aLimit; ++i)
		{
		const TEntry& entry = (*dir)[i];
		if (entry.IsDir())
			{
			continue;
			}
		if (!(entry.iName.Right(4).CompareF(_L(".mp3")) == 0 || entry.iName.Right(4).CompareF(_L(".m4a")) == 0 || entry.iName.Right(4).CompareF(_L(".aac")) == 0))
			{
			continue;
			}
		if (aQuery.Length() > 0 && entry.iName.FindF(aQuery) < 0)
			{
			continue;
			}
		TFileName fullPath(aDir);
		fullPath.Append(entry.iName);
		TBuf<160> display;
		BuildPrefixedDisplay(aPermanent ? _L("Zapisane: ") : _L("Cache: "), entry.iName, display);
		CTurboTrackEntry* item = CTurboTrackEntry::NewLC(display, &fullPath, NULL, &entry.iName, !aPermanent, aPermanent, EFalse);
		aOut.AppendL(item);
		CleanupStack::Pop(item);
		}
	CleanupStack::PopAndDestroy(dir);
	}

void CTurboMusicService::AppendAllInDirL(RFs& aFs, const TDesC& aDir, TBool aPermanent, RPointerArray<CTurboTrackEntry>& aOut, TInt aLimit)
	{
	TBuf<1> empty;
	AppendLocalMatchesInDirL(aFs, aDir, empty, aPermanent, aOut, aLimit);
	}

void CTurboMusicService::FetchQueryToServerL(const TDesC& aQuery)
	{
	_LIT(KHost, "turboconect.pl");
	HBufC* encoded = UrlEncodeSimpleLC(aQuery);
	HBufC* path = HBufC::NewLC(64 + encoded->Length());
	path->Des().Copy(_L("/api/fetch"));
	if (encoded->Length() > 0)
		{
		path->Des().Append(_L("?q="));
		path->Des().Append(*encoded);
		}
	HBufC8* resp = HttpGetSmallResponseL(KHost, *path, 12288);
	CleanupStack::PushL(resp);
	CleanupStack::PopAndDestroy(resp);
	CleanupStack::PopAndDestroy(path);
	CleanupStack::PopAndDestroy(encoded);
	}

void CTurboMusicService::AppendRemoteSearchResultsL(const TDesC& aQuery, RPointerArray<CTurboTrackEntry>& aOut, TInt aLimit)
	{
	if (aOut.Count() >= aLimit)
		{
		return;
		}
	_LIT(KHost, "turboconect.pl");
	HBufC* encoded = UrlEncodeSimpleLC(aQuery);
	HBufC* path = HBufC::NewLC(64 + encoded->Length());
	path->Des().Copy(_L("/api/search_plain"));
	if (encoded->Length() > 0)
		{
		path->Des().Append(_L("?q="));
		path->Des().Append(*encoded);
		}
	HBufC8* resp = HttpGetSmallResponseL(KHost, *path, KOnlineSearchResponseBytes);
	CleanupStack::PushL(resp);
	TPtrC8 body = HttpBodyFromResponse8(*resp);
	if (body.Length() == 0)
		{
		CleanupStack::PopAndDestroy(resp);
		FetchQueryToServerL(aQuery);
		resp = HttpGetSmallResponseL(KHost, *path, KOnlineSearchResponseBytes);
		CleanupStack::PushL(resp);
		body.Set(HttpBodyFromResponse8(*resp));
		}

	TInt pos = 0;
	while (pos < body.Length() && aOut.Count() < aLimit)
		{
		TInt end = body.Mid(pos).Locate('\n');
		if (end < 0)
			{
			end = body.Length() - pos;
			}
		TPtrC8 line = body.Mid(pos, end);
		while (line.Length() > 0 && (line[line.Length() - 1] == '\r' || line[line.Length() - 1] == '\n'))
			{
			line.Set(line.Left(line.Length() - 1));
			}
		const TInt sep1 = line.Locate('|');
		const TInt sep2rel = (sep1 >= 0) ? line.Mid(sep1 + 1).Locate('|') : KErrNotFound;
		if (sep1 > 0 && sep2rel >= 0)
			{
			const TInt sep2 = sep1 + 1 + sep2rel;
			TPtrC8 title8 = line.Left(sep1);
			TPtrC8 fileName8 = line.Mid(sep1 + 1, sep2 - sep1 - 1);
			TPtrC8 url8 = line.Mid(sep2 + 1);
			TBuf<128> title;
			TBuf<128> fileName;
			TBuf<256> url;
			CopyAscii8ToDes(title, title8);
			CopyAscii8ToDes(fileName, fileName8);
			CopyAscii8ToDes(url, url8);
			TBuf<160> display;
			BuildPrefixedDisplay(_L("Online: "), title, display);
			CTurboTrackEntry* item = CTurboTrackEntry::NewLC(display, NULL, &url, &fileName, EFalse, EFalse, ETrue);
			aOut.AppendL(item);
			CleanupStack::Pop(item);
			}
		pos += end + 1;
		}

	CleanupStack::PopAndDestroy(resp);
	CleanupStack::PopAndDestroy(path);
	CleanupStack::PopAndDestroy(encoded);
	}

void CTurboMusicService::SearchHybridL(const TDesC& aQuery, RPointerArray<CTurboTrackEntry>& aOut)
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	AppendLocalMatchesInDirL(fs, iCacheManager.CacheDir(), aQuery, EFalse, aOut, KTrackListMax);
	AppendLocalMatchesInDirL(fs, KMusicDirE, aQuery, ETrue, aOut, KTrackListMax);
	AppendLocalMatchesInDirL(fs, KMusicDirC, aQuery, ETrue, aOut, KTrackListMax);
	AppendLocalMatchesInDirL(fs, KTurboMusicDirE, aQuery, ETrue, aOut, KTrackListMax);
	AppendLocalMatchesInDirL(fs, KTurboMusicDirC, aQuery, ETrue, aOut, KTrackListMax);
	CleanupStack::PopAndDestroy(&fs);
	if (aQuery.Length() > 0 && aOut.Count() == 0)
		{
		AppendRemoteSearchResultsL(aQuery, aOut, KOnlineSearchMaxResults);
		}
	}

void CTurboMusicService::SearchOnlineOnlyL(const TDesC& aQuery, RPointerArray<CTurboTrackEntry>& aOut)
	{
	AppendRemoteSearchResultsL(aQuery, aOut, KOnlineSearchMaxResults);
	}

void CTurboMusicService::ListLibraryL(RPointerArray<CTurboTrackEntry>& aOut)
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	AppendAllInDirL(fs, iCacheManager.CacheDir(), EFalse, aOut, KTrackListMax);
	AppendAllInDirL(fs, KMusicDirE, ETrue, aOut, KTrackListMax);
	AppendAllInDirL(fs, KMusicDirC, ETrue, aOut, KTrackListMax);
	AppendAllInDirL(fs, KTurboMusicDirE, ETrue, aOut, KTrackListMax);
	AppendAllInDirL(fs, KTurboMusicDirC, ETrue, aOut, KTrackListMax);
	CleanupStack::PopAndDestroy(&fs);
	}

void CTurboMusicService::ResolvePermanentMusicDirL(RFs& aFs, TDes& aOut)
	{
	ResolveMusicDirL(aFs, aOut);
	}

void CTurboMusicService::DownloadToCacheL(CTurboTrackEntry& aEntry, TDes& aOutPath)
	{
	if (!aEntry.iRemoteUrl)
		{
		User::Leave(KErrNotFound);
		}
	_LIT(KHost, "turboconect.pl");
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	const TDesC& cacheDir = iCacheManager.CacheDir();
	EnsureFolderL(fs, cacheDir);

	TBuf<128> safeName;
	if (aEntry.iFileName && aEntry.iFileName->Length() > 0)
		{
		safeName.Copy(aEntry.iFileName->Left(safeName.MaxLength()));
		}
	else
		{
		safeName.Copy(_L("track.mp3"));
		}
	SanitizeFileName(safeName);

	TFileName target(cacheDir);
	target.Append(safeName);
	TEntry existing;
	if (fs.Entry(target, existing) == KErrNone)
		{
		aOutPath.Copy(target);
		CleanupStack::PopAndDestroy(&fs);
		return;
		}

	RFile outFile;
	User::LeaveIfError(outFile.Replace(fs, target, EFileWrite | EFileShareExclusive));
	CleanupClosePushL(outFile);

	RSocketServ ss;
	User::LeaveIfError(ss.Connect());
	CleanupClosePushL(ss);
	RHostResolver resolver;
	User::LeaveIfError(resolver.Open(ss, KAfInet, KProtocolInetUdp));
	CleanupClosePushL(resolver);
	TNameEntry nameEntry;
	User::LeaveIfError(resolver.GetByName(KHost, nameEntry));
	TInetAddr addr = TInetAddr::Cast(nameEntry().iAddr);
	addr.SetPort(80);
	RSocket sock;
	User::LeaveIfError(sock.Open(ss, KAfInet, KSockStream, KProtocolInetTcp));
	CleanupClosePushL(sock);
	TRequestStatus st;
	sock.Connect(addr, st);
	User::WaitForRequest(st);
	User::LeaveIfError(st.Int());

	HBufC8* path8 = ToAscii8LC(*aEntry.iRemoteUrl);
	HBufC8* req = HBufC8::NewLC(path8->Length() + 128);
	req->Des().Copy(_L8("GET "));
	req->Des().Append(*path8);
	req->Des().Append(_L8(" HTTP/1.0\r\nHost: turboconect.pl\r\nConnection: close\r\n\r\n"));
	sock.Write(req->Des(), st);
	User::WaitForRequest(st);
	User::LeaveIfError(st.Int());

	HBufC8* headerBuf = HBufC8::NewLC(4096);
	TPtr8 header = headerBuf->Des();
	TBool headerDone = EFalse;
	TBool wroteBody = EFalse;
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
		if (!headerDone)
			{
			if (header.Length() + chunk.Length() > header.MaxLength())
				{
				User::Leave(KErrOverflow);
				}
			header.Append(chunk);
			const TInt hdrEnd = header.Find(_L8("\r\n\r\n"));
			if (hdrEnd >= 0)
				{
				if (header.Find(_L8("HTTP/1.1 200")) < 0 && header.Find(_L8("HTTP/1.0 200")) < 0)
					{
					User::Leave(KErrGeneral);
					}
				TPtrC8 bodyPart = header.Mid(hdrEnd + 4);
				if (bodyPart.Length() > 0)
					{
					User::LeaveIfError(outFile.Write(bodyPart));
					wroteBody = ETrue;
					}
				headerDone = ETrue;
				}
			}
		else
			{
			User::LeaveIfError(outFile.Write(chunk));
			wroteBody = ETrue;
			}
		}

	if (!headerDone || !wroteBody)
		{
		User::Leave(KErrCorrupt);
		}

	CleanupStack::PopAndDestroy(headerBuf);
	CleanupStack::PopAndDestroy(req);
	CleanupStack::PopAndDestroy(path8);
	CleanupStack::PopAndDestroy(&sock);
	CleanupStack::PopAndDestroy(&resolver);
	CleanupStack::PopAndDestroy(&ss);
	CleanupStack::PopAndDestroy(&outFile);
	CleanupStack::PopAndDestroy(&fs);

	TrimCacheL();
	aOutPath.Copy(target);
	aEntry.SetLocalPathL(target);
	aEntry.iCached = ETrue;
	aEntry.iOnlineOnly = EFalse;
	}

void CTurboMusicService::PrepareTrackForPlaybackL(CTurboTrackEntry& aEntry, TDes& aLocalPath, TBool& aFromInternet)
	{
	aFromInternet = EFalse;
	if (aEntry.iLocalPath && aEntry.iLocalPath->Length() > 0)
		{
		aLocalPath.Copy(*aEntry.iLocalPath);
		aFromInternet = aEntry.iCached && !aEntry.iPermanent;
		return;
		}
	DownloadToCacheL(aEntry, aLocalPath);
	aFromInternet = ETrue;
	}

void CTurboMusicService::SaveTrackPermanentL(CTurboTrackEntry& aEntry)
	{
	TFileName sourcePath;
	TBool fromInternet = EFalse;
	PrepareTrackForPlaybackL(aEntry, sourcePath, fromInternet);

	if (aEntry.iPermanent)
		{
		return;
		}

	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	TFileName musicDir;
	ResolvePermanentMusicDirL(fs, musicDir);
	EnsureFolderL(fs, musicDir);

	TBuf<128> safeName;
	if (aEntry.iFileName && aEntry.iFileName->Length() > 0)
		{
		safeName.Copy(aEntry.iFileName->Left(safeName.MaxLength()));
		}
	else
		{
		LeafNameFromPath(sourcePath, safeName);
		}
	SanitizeFileName(safeName);

	TFileName target(musicDir);
	target.Append(safeName);
	TEntry existing;
	if (fs.Entry(target, existing) != KErrNone)
		{
		CopyFileSimpleL(fs, sourcePath, target);
		}
	if ((fromInternet || aEntry.iCached) && sourcePath.CompareF(target) != 0)
		{
		fs.Delete(sourcePath);
		}
	aEntry.SetLocalPathL(target);
	aEntry.SetFileNameL(safeName);
	aEntry.iPermanent = ETrue;
	aEntry.iCached = EFalse;
	aEntry.iOnlineOnly = EFalse;
	TBuf<160> display;
	BuildPrefixedDisplay(_L("Zapisane: "), safeName, display);
	aEntry.SetDisplayL(display);
	CleanupStack::PopAndDestroy(&fs);
	}

void CTurboMusicService::DeleteTrackL(CTurboTrackEntry& aEntry)
	{
	if (!aEntry.iLocalPath || aEntry.iLocalPath->Length() == 0)
		{
		User::Leave(KErrNotFound);
		}

	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	const TInt err = fs.Delete(*aEntry.iLocalPath);
	if (err != KErrNone)
		{
		User::Leave(err);
		}
	CleanupStack::PopAndDestroy(&fs);
	}

TInt CTurboMusicService::CleanCacheL()
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	TInt removed = 0;
	CDir* dir = NULL;
	if (fs.GetDir(iCacheManager.CacheDir(), KEntryAttNormal, ESortNone, dir) == KErrNone)
		{
		CleanupStack::PushL(dir);
		for (TInt i = 0; i < dir->Count(); ++i)
			{
			const TEntry& entry = (*dir)[i];
			if (entry.IsDir())
				{
				continue;
				}
			TFileName path(iCacheManager.CacheDir());
			path.Append(entry.iName);
			if (fs.Delete(path) == KErrNone)
				{
				++removed;
				}
			}
		CleanupStack::PopAndDestroy(dir);
		}
	CleanupStack::PopAndDestroy(&fs);
	return removed;
	}

void CTurboMusicService::TrimCacheL()
	{
	iCacheManager.RecalculatePolicyL();
	iCacheManager.TrimIfNeededL();
	}

// ============================ MEMBER FUNCTIONS ===============================


// -----------------------------------------------------------------------------
// CNokiaspotifyAppUi::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CNokiaspotifyAppUi::ConstructL()
	{
	// Initialise app UI with standard value.
	BaseConstructL(CAknAppUi::EAknEnableSkin);
	InitializeCacheManagerL();
	delete iMusicService;
	iMusicService = NULL;
	iMusicService = CTurboMusicService::NewL(*iCacheManager);
	delete iNetwork;
	iNetwork = NULL;
	iNetwork = CNokiaspotifyNetwork::NewL();

	iAppView = CNokiaspotifyAppView::NewL(ClientRect());
	AddToStackL(iAppView);
	iAppView->SetFocus(ETrue);
	TRAP_IGNORE(ShowWelcomeDialogL());
	TRAP_IGNORE(RebuildLocalLibraryIndexL());
	iAppView->DrawNow();
	}
// -----------------------------------------------------------------------------
// CNokiaspotifyAppUi::CNokiaspotifyAppUi()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CNokiaspotifyAppUi::CNokiaspotifyAppUi()
	: iAppView(NULL)
	, iCacheManager(NULL)
	, iNetwork(NULL)
	, iMusicService(NULL)
	, iAudioPlayer(NULL)
	, iPlaybackIndex(KErrNotFound)
	, iAudioReady(EFalse)
	, iAudioPlaying(EFalse)
	, iShuffleEnabled(EFalse)
	, iPendingAutoPlay(EFalse)
	, iStopRequested(EFalse)
	, iCurrentTrackFromInternet(EFalse)
	{
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyAppUi::~CNokiaspotifyAppUi()
// Destructor.
// -----------------------------------------------------------------------------
//
CNokiaspotifyAppUi::~CNokiaspotifyAppUi()
	{
	if (iAppView)
		{
		RemoveFromStack(iAppView);
		delete iAppView;
		iAppView = NULL;
		}
	delete iCacheManager;
	iCacheManager = NULL;
	delete iNetwork;
	iNetwork = NULL;
	delete iMusicService;
	iMusicService = NULL;
	delete iAudioPlayer;
	iAudioPlayer = NULL;
	iCurrentTracks.ResetAndDestroy();
	iCurrentTracks.Close();
	iPlaybackQueue.ResetAndDestroy();
	iPlaybackQueue.Close();

	}


// -----------------------------------------------------------------------------
// CNokiaspotifyAppUi::HandleCommandL()
// Takes care of command handling.
// -----------------------------------------------------------------------------
//
void CNokiaspotifyAppUi::HandleCommandL(TInt aCommand)
	{
	switch (aCommand)
		{
		case EEikCmdExit:
		case EAknSoftkeyExit:
			Exit();
			break;

		case ECommand1:
			SearchMusicL();
			break;
		case ECommand2:
			CreatePlaylistL();
			break;
		case EHelp:
			ShowPlaylistsL();
			break;
		case ELogin:
		case EConnectInternet:
			ToggleInternetConnectionL();
			break;
		case EAbout:
			AddTrackToPlaylistL();
			break;
		case EPlaylistOpen:
			ShowPlaylistByNameL();
			break;
		case EPlaylistRemoveTrack:
			RemoveTrackFromPlaylistL();
			break;
		case EPlaylistDelete:
			DeletePlaylistL();
			break;
		case ELibraryReindex:
			RebuildLocalLibraryIndexL();
			break;
		default:
			// Do not Panic: the shell sends many command ids; unknown ones are OK.
			break;
		}
	}

void CNokiaspotifyAppUi::ShowWelcomeDialogL()
	{
	const TInt len = KinformationHead().Length() + KinformationText().Length() + 4;
	HBufC* msg = HBufC::NewLC(len);
	msg->Des().Copy(KinformationHead);
	msg->Des().Append(_L("\n\n"));
	msg->Des().Append(KinformationText);
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(*msg);
	CleanupStack::PopAndDestroy(msg);
	}

void CNokiaspotifyAppUi::InitializeCacheManagerL()
	{
	delete iCacheManager;
	iCacheManager = NULL;
	iCacheManager = CTurboMusicCacheManager::NewL();
	}

void CNokiaspotifyAppUi::ResolveDataDir(TDes& aOut)
	{
	aOut.Copy((iCacheManager && iCacheManager->CacheDir().Length() > 0 &&
		(iCacheManager->CacheDir()[0] == 'E' || iCacheManager->CacheDir()[0] == 'e'))
		? KDataDirOnE
		: KDataDirOnC);
	}

void CNokiaspotifyAppUi::ShowCacheStatusL()
	{
	if (!iCacheManager)
		{
		_LIT(KNoCache, "Cache manager nie jest zainicjalizowany.");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KNoCache);
		return;
		}

	iCacheManager->RecalculatePolicyL();
	iCacheManager->TrimIfNeededL();
	const TInt64 usedBytes = iCacheManager->CacheUsedBytesL();
	const TInt64 limitBytes = iCacheManager->CacheLimitBytes();
	const TInt usedMb = (TInt)(usedBytes / KOneMb);
	const TInt limitMb = (TInt)(limitBytes / KOneMb);
	const TDesC& cacheDir = iCacheManager->CacheDir();

	_LIT(KFmt, "TurboMusic Cache\nFolder: %S\nUzycie: %d MB / %d MB\nPolityka: LRU (auto-clean)");
	HBufC* text = HBufC::NewLC(256);
	text->Des().Format(KFmt, &cacheDir, usedMb, limitMb);
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(*text);
	CleanupStack::PopAndDestroy(text);
	}

void CNokiaspotifyAppUi::ShowAppStatusL()
	{
	const TBool online = (iNetwork && iNetwork->IsConnected());
	const TInt err = (iNetwork ? iNetwork->LastError() : KErrNotReady);
	_LIT(KFmt, "TurboMusic\nInternet: %S\nLast error: %d");
	_LIT(KOn, "ON");
	_LIT(KOff, "OFF");
	HBufC* text = HBufC::NewLC(128);
	text->Des().Format(KFmt, online ? &KOn : &KOff, err);
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(*text);
	CleanupStack::PopAndDestroy(text);
	}

void CNokiaspotifyAppUi::ToggleInternetConnectionL()
	{
	if (!iNetwork)
		{
		_LIT(KNoNet, "Modul sieci niedostepny.");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KNoNet);
		return;
		}

	if (iNetwork->IsConnected())
		{
		iNetwork->Disconnect();
		_LIT(KDisconnected, "Internet: ROZLACZONY");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KDisconnected);
		return;
		}

	TRAPD(err, iNetwork->ConnectL());
	if (err == KErrNone)
		{
		_LIT(KConnected, "Internet: POLACZONO");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KConnected);
		}
	else
		{
		_LIT(KConnFailFmt, "Blad polaczenia: %d");
		HBufC* msg = HBufC::NewLC(64);
		msg->Des().Format(KConnFailFmt, err);
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(*msg);
		CleanupStack::PopAndDestroy(msg);
		}
	}

void CNokiaspotifyAppUi::OpenOnlineSearchL(const TDesC& aQuery)
	{
	if (!iNetwork || !iNetwork->IsConnected())
		{
		_LIT(KNeedNet, "Najpierw polacz internet (5 / Login).");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KNeedNet);
		return;
		}

	if (iAppView)
		{
		iAppView->SetPlaybackPanel(_L("INTERNET"), _L("Szukam na serwerze..."));
		}

	ResetCurrentTrackList();
	if (iMusicService)
		{
		iMusicService->SearchOnlineOnlyL(aQuery, iCurrentTracks);
		}
	if (iCurrentTracks.Count() == 0)
		{
		_LIT(KNoOnline, "Brak wynikow online.");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KNoOnline);
		return;
		}
	ShowCurrentTrackListL(_L("Online"));
	}

void CNokiaspotifyAppUi::ResetPlaybackQueue()
	{
	iPlaybackQueue.ResetAndDestroy();
	iPlaybackIndex = KErrNotFound;
	}

void CNokiaspotifyAppUi::CopyCurrentTracksToPlaybackQueueL()
	{
	ResetPlaybackQueue();
	for (TInt i = 0; i < iCurrentTracks.Count(); ++i)
		{
		if (!iCurrentTracks[i])
			{
			continue;
			}
		CTurboTrackEntry* copy = CloneTrackEntryLC(*iCurrentTracks[i]);
		iPlaybackQueue.AppendL(copy);
		CleanupStack::Pop(copy);
		}
	}

TInt CNokiaspotifyAppUi::ResolveNextQueueIndex() const
	{
	const TInt count = iPlaybackQueue.Count();
	if (count <= 0)
		{
		return KErrNotFound;
		}
	if (count == 1)
		{
		return 0;
		}
	if (!iShuffleEnabled)
		{
		return (iPlaybackIndex >= 0) ? ((iPlaybackIndex + 1) % count) : 0;
		}
	TTime now;
	now.HomeTime();
	TInt next = (TInt)(now.Int64() % count);
	if (next < 0)
		{
		next = -next;
		}
	if (next == iPlaybackIndex)
		{
		next = (next + 1) % count;
		}
	return next;
	}

TInt CNokiaspotifyAppUi::ResolvePrevQueueIndex() const
	{
	const TInt count = iPlaybackQueue.Count();
	if (count <= 0)
		{
		return KErrNotFound;
		}
	if (count == 1)
		{
		return 0;
		}
	if (!iShuffleEnabled)
		{
		return (iPlaybackIndex > 0) ? (iPlaybackIndex - 1) : (count - 1);
		}
	TTime now;
	now.HomeTime();
	TInt prev = (TInt)((now.Int64() / 2) % count);
	if (prev < 0)
		{
		prev = -prev;
		}
	if (prev == iPlaybackIndex)
		{
		prev = (prev + count - 1) % count;
		}
	return prev;
	}

void CNokiaspotifyAppUi::UpdatePlaybackUi()
	{
	if (!iAppView)
		{
		return;
		}
	if (iCurrentPlaybackName.Length() == 0)
		{
		iAppView->SetNowPlayingState(_L("PLAYER"), _L("Nic nie gra"), _L("Stop"), iShuffleEnabled);
		iAppView->ClearPlaybackPanel();
		return;
		}

	TBuf<96> detail;
	detail.Copy(iCurrentTrackFromInternet ? _L("Online: ") : _L("Lokalnie: "));
	AppendLimited(detail, iCurrentPlaybackName);

	TBuf<32> status;
	if (!iAudioReady && iPendingAutoPlay)
		{
		status.Copy(_L("Laduje"));
		}
	else if (iAudioPlaying)
		{
		status.Copy(_L("Gra"));
		}
	else
		{
		status.Copy(_L("Stop"));
		}

	iAppView->SetPlaybackPanel(_L("TERAZ GRA"), detail);
	iAppView->SetNowPlayingState(_L("TERAZ GRA"), detail, status, iShuffleEnabled);
	}

void CNokiaspotifyAppUi::PlayLocalFileL(const TDesC& aPath, TBool aFromInternet, const TDesC& aDisplayName)
	{
	if (iAudioPlayer)
		{
		iStopRequested = ETrue;
		iAudioPlayer->Stop();
		delete iAudioPlayer;
		iAudioPlayer = NULL;
		}

	iAudioReady = EFalse;
	iAudioPlaying = EFalse;
	iPendingAutoPlay = ETrue;
	iStopRequested = EFalse;
	iCurrentTrackFromInternet = aFromInternet;
	iPendingAudioPath.Copy(aPath.Left(iPendingAudioPath.MaxLength()));
	iPendingPlaybackName.Copy(aDisplayName.Left(iPendingPlaybackName.MaxLength()));
	iCurrentPlaybackName.Copy(aDisplayName.Left(iCurrentPlaybackName.MaxLength()));
	UpdatePlaybackUi();
	iAudioPlayer = CMdaAudioPlayerUtility::NewFilePlayerL(aPath, *this);
	}

void CNokiaspotifyAppUi::PlayQueueIndexL(TInt aIndex)
	{
	if (aIndex < 0 || aIndex >= iPlaybackQueue.Count() || !iPlaybackQueue[aIndex] || !iMusicService)
		{
		User::Leave(KErrNotFound);
		}

	TFileName localPath;
	TBool fromInternet = EFalse;
	iMusicService->PrepareTrackForPlaybackL(*iPlaybackQueue[aIndex], localPath, fromInternet);

	TBuf<96> name;
	if (iPlaybackQueue[aIndex]->iFileName && iPlaybackQueue[aIndex]->iFileName->Length() > 0)
		{
		name.Copy(iPlaybackQueue[aIndex]->iFileName->Left(name.MaxLength()));
		}
	else if (localPath.Length() > 0)
		{
		LeafNameFromPath(localPath, name);
		}
	else if (iPlaybackQueue[aIndex]->iDisplay)
		{
		name.Copy(iPlaybackQueue[aIndex]->iDisplay->Left(name.MaxLength()));
		}
	else
		{
		name.Copy(_L("track"));
		}

	iPlaybackIndex = aIndex;
	PlayLocalFileL(localPath, fromInternet, name);
	}

void CNokiaspotifyAppUi::OpenNowPlayingScreenL()
	{
	if (iCurrentPlaybackName.Length() == 0 || !iAppView)
		{
		_LIT(KNothingPlaying, "Nic nie jest zaladowane.");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KNothingPlaying);
		return;
		}
	UpdatePlaybackUi();
	iAppView->ShowNowPlayingScreen();
	}

void CNokiaspotifyAppUi::StopPlayback()
	{
	iStopRequested = ETrue;
	iPendingAutoPlay = EFalse;
	if (iAudioPlayer)
		{
		iAudioPlayer->Stop();
		delete iAudioPlayer;
		iAudioPlayer = NULL;
		}
	iAudioReady = EFalse;
	iAudioPlaying = EFalse;
	UpdatePlaybackUi();
	}

void CNokiaspotifyAppUi::ToggleShuffle()
	{
	iShuffleEnabled = !iShuffleEnabled;
	UpdatePlaybackUi();
	}

void CNokiaspotifyAppUi::MapcInitComplete(TInt aError, const TTimeIntervalMicroSeconds& /*aDuration*/)
	{
	if (aError != KErrNone)
		{
		delete iAudioPlayer;
		iAudioPlayer = NULL;
		iAudioReady = EFalse;
		iAudioPlaying = EFalse;
		iPendingAutoPlay = EFalse;
		TRAP_IGNORE(ShowOperationErrorL(aError));
		UpdatePlaybackUi();
		return;
		}

	iAudioReady = ETrue;
	if (iAudioPlayer)
		{
		iAudioPlayer->SetVolume(iAudioPlayer->MaxVolume());
		}
	if (iPendingAutoPlay && iAudioPlayer)
		{
		iAudioPlayer->Play();
		iAudioPlaying = ETrue;
		iPendingAutoPlay = EFalse;
		}
	UpdatePlaybackUi();
	}

void CNokiaspotifyAppUi::MapcPlayComplete(TInt aError)
	{
	iAudioPlaying = EFalse;
	iAudioReady = EFalse;
	delete iAudioPlayer;
	iAudioPlayer = NULL;

	if (aError == KErrNone && !iStopRequested && iPlaybackQueue.Count() > 1)
		{
		const TInt next = ResolveNextQueueIndex();
		if (next != KErrNotFound)
			{
			TRAP_IGNORE(PlayQueueIndexL(next));
			return;
			}
		}

	iStopRequested = EFalse;
	UpdatePlaybackUi();
	}

void CNokiaspotifyAppUi::DownloadRemoteTrackL(const TDesC& aRelativeUrl, const TDesC& aFileName)
	{
	_LIT(KHost, "turboconect.pl");
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	TFileName musicDir;
	ResolveMusicDirL(fs, musicDir);
	EnsureFolderL(fs, musicDir);

	TFileName target(musicDir);
	TBuf<128> safeName;
	safeName.Copy(aFileName.Left(safeName.MaxLength()));
	SanitizeFileName(safeName);
	if (safeName.Length() == 0)
		{
		safeName.Copy(_L("track.mp3"));
		}
	target.Append(safeName);

	if (iAppView)
		{
		TBuf<96> detail;
		detail.Copy(_L("Pobieram: "));
		AppendLimited(detail, safeName);
		iAppView->SetPlaybackPanel(_L("INTERNET"), detail);
		}

	RFile outFile;
	User::LeaveIfError(outFile.Replace(fs, target, EFileWrite | EFileShareExclusive));
	CleanupClosePushL(outFile);

	RSocketServ ss;
	User::LeaveIfError(ss.Connect());
	CleanupClosePushL(ss);

	RHostResolver resolver;
	User::LeaveIfError(resolver.Open(ss, KAfInet, KProtocolInetUdp));
	CleanupClosePushL(resolver);
	TNameEntry nameEntry;
	User::LeaveIfError(resolver.GetByName(KHost, nameEntry));
	TInetAddr addr = TInetAddr::Cast(nameEntry().iAddr);
	addr.SetPort(80);

	RSocket sock;
	User::LeaveIfError(sock.Open(ss, KAfInet, KSockStream, KProtocolInetTcp));
	CleanupClosePushL(sock);
	TRequestStatus st;
	sock.Connect(addr, st);
	User::WaitForRequest(st);
	User::LeaveIfError(st.Int());

	HBufC8* path8 = ToAscii8LC(aRelativeUrl);
	HBufC8* req = HBufC8::NewLC(path8->Length() + 128);
	req->Des().Copy(_L8("GET "));
	req->Des().Append(*path8);
	req->Des().Append(_L8(" HTTP/1.0\r\nHost: turboconect.pl\r\nConnection: close\r\n\r\n"));

	sock.Write(req->Des(), st);
	User::WaitForRequest(st);
	User::LeaveIfError(st.Int());

	HBufC8* headerBuf = HBufC8::NewLC(4096);
	TPtr8 header = headerBuf->Des();
	TBool headerDone = EFalse;
	TBool wroteBody = EFalse;
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
		if (!headerDone)
			{
			if (header.Length() + chunk.Length() > header.MaxLength())
				{
				User::Leave(KErrOverflow);
				}
			header.Append(chunk);
			const TInt hdrEnd = header.Find(_L8("\r\n\r\n"));
			if (hdrEnd >= 0)
				{
				if (header.Find(_L8("HTTP/1.1 200")) < 0 && header.Find(_L8("HTTP/1.0 200")) < 0)
					{
					User::Leave(KErrGeneral);
					}
				TPtrC8 bodyPart = header.Mid(hdrEnd + 4);
				if (bodyPart.Length() > 0)
					{
					User::LeaveIfError(outFile.Write(bodyPart));
					wroteBody = ETrue;
					}
				headerDone = ETrue;
				}
			}
		else
			{
			User::LeaveIfError(outFile.Write(chunk));
			wroteBody = ETrue;
			}
		}

	if (!headerDone || !wroteBody)
		{
		User::Leave(KErrCorrupt);
		}

	CleanupStack::PopAndDestroy(headerBuf);
	CleanupStack::PopAndDestroy(req);
	CleanupStack::PopAndDestroy(path8);
	CleanupStack::PopAndDestroy(&sock);
	CleanupStack::PopAndDestroy(&resolver);
	CleanupStack::PopAndDestroy(&ss);
	CleanupStack::PopAndDestroy(&outFile);
	CleanupStack::PopAndDestroy(&fs);

	_LIT(KDownloaded, "Pobrano. Odtwarzam...");
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(KDownloaded);
	PlayLocalFileL(target, ETrue, safeName);
	}

void CNokiaspotifyAppUi::PingServerL()
	{
	if (!iNetwork || !iNetwork->IsConnected())
		{
		_LIT(KNeedNet, "Najpierw polacz internet (5 / Login).");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KNeedNet);
		return;
		}

	_LIT(KHost, "turboconect.pl");
	// /ping nie zawsze jest publicznie wystawione; /music jest stabilnym endpointem.
	_LIT8(KReq, "GET /music HTTP/1.0\r\nHost: turboconect.pl\r\nConnection: close\r\n\r\n");

	RSocketServ ss;
	User::LeaveIfError(ss.Connect());
	CleanupClosePushL(ss);

	RHostResolver resolver;
	User::LeaveIfError(resolver.Open(ss, KAfInet, KProtocolInetUdp));
	CleanupClosePushL(resolver);
	TNameEntry nameEntry;
	User::LeaveIfError(resolver.GetByName(KHost, nameEntry));
	TInetAddr addr = TInetAddr::Cast(nameEntry().iAddr);
	addr.SetPort(80);

	RSocket sock;
	User::LeaveIfError(sock.Open(ss, KAfInet, KSockStream, KProtocolInetTcp));
	CleanupClosePushL(sock);
	TRequestStatus st;
	sock.Connect(addr, st);
	User::WaitForRequest(st);
	User::LeaveIfError(st.Int());

	sock.Write(KReq, st);
	User::WaitForRequest(st);
	User::LeaveIfError(st.Int());

	TBuf8<512> chunk;
	TSockXfrLength len;
	sock.RecvOneOrMore(chunk, 0, st, len);
	User::WaitForRequest(st);
	User::LeaveIfError(st.Int());

	const TBool http200 =
		(chunk.Find(_L8("HTTP/1.1 200")) >= 0) ||
		(chunk.Find(_L8("HTTP/1.0 200")) >= 0);
	const TBool httpRedirect =
		(chunk.Find(_L8("HTTP/1.1 301")) >= 0) ||
		(chunk.Find(_L8("HTTP/1.1 302")) >= 0) ||
		(chunk.Find(_L8("HTTP/1.0 301")) >= 0) ||
		(chunk.Find(_L8("HTTP/1.0 302")) >= 0);
	if (http200 || httpRedirect)
		{
		_LIT(KOk, "Ping OK: serwer odpowiada HTTP.");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KOk);
		}
	else
		{
		_LIT(KBad, "Ping FAIL: brak odpowiedzi HTTP 200/30x.");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KBad);
		}

	CleanupStack::PopAndDestroy(&sock);
	CleanupStack::PopAndDestroy(&resolver);
	CleanupStack::PopAndDestroy(&ss);
	}

void CNokiaspotifyAppUi::SearchMusicL()
	{
	TBuf<64> query;
	_LIT(KPrompt, "Szukaj muzyki");
	if (!PromptTextL(query, KPrompt))
		{
		return;
		}
	SearchMusicByQueryL(query);
	}

void CNokiaspotifyAppUi::SearchMusicByQueryL(const TDesC& aQuery)
	{
	if (aQuery.Length() == 0)
		{
		_LIT(KNoQuery, "Podaj nazwe utworu.");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KNoQuery);
		return;
		}
	ResetCurrentTrackList();
	if (iMusicService)
		{
		iMusicService->SearchHybridL(aQuery, iCurrentTracks);
		}
	if (iCurrentTracks.Count() == 0)
		{
		_LIT(KNoneResults, "Brak wynikow.");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KNoneResults);
		return;
		}
	ShowCurrentTrackListL(_L("Wyniki"));
	}

void CNokiaspotifyAppUi::SearchMusicOnlineL()
	{
	TBuf<64> query;
	_LIT(KPrompt, "Szukaj online");
	if (!PromptTextL(query, KPrompt))
		{
		return;
		}
	SearchMusicOnlineByQueryL(query);
	}

void CNokiaspotifyAppUi::SearchMusicOnlineByQueryL(const TDesC& aQuery)
	{
	if (aQuery.Length() == 0)
		{
		_LIT(KNoQuery, "Podaj nazwe utworu.");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KNoQuery);
		return;
		}
	OpenOnlineSearchL(aQuery);
	}

void CNokiaspotifyAppUi::ShowTrackListL()
	{
	ResetCurrentTrackList();
	if (iMusicService)
		{
		iMusicService->ListLibraryL(iCurrentTracks);
		}
	if (iCurrentTracks.Count() == 0)
		{
		_LIT(KNoTracks, "Brak utworow w bibliotece.");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KNoTracks);
		}
	else
		{
		ShowCurrentTrackListL(_L("Biblioteka"));
		}
	}

void CNokiaspotifyAppUi::ResetCurrentTrackList()
	{
	iCurrentTracks.ResetAndDestroy();
	iCurrentTracks.Close();
	}

void CNokiaspotifyAppUi::ShowCurrentTrackListL(const TDesC& aTitle)
	{
	if (!iAppView)
		{
		return;
		}
	iCurrentTrackListTitle.Copy(aTitle.Left(iCurrentTrackListTitle.MaxLength()));
	RPointerArray<HBufC> labels;
	CleanupStack::PushL(TCleanupItem(CleanupOwnedBufArray, &labels));
	for (TInt i = 0; i < iCurrentTracks.Count(); ++i)
		{
		if (iCurrentTracks[i] && iCurrentTracks[i]->iDisplay)
			{
			AppendOwnedBufL(labels, *iCurrentTracks[i]->iDisplay);
			}
		}
	iAppView->ShowTrackListL(labels, aTitle);
	CleanupStack::PopAndDestroy(&labels);
	}

void CNokiaspotifyAppUi::ShowOperationErrorL(TInt aErr)
	{
	_LIT(KNoMemory, "Brak pamieci RAM. Sprobuj krotszego szukania albo zamknij inne aplikacje.");
	_LIT(KNotFound, "Nie znaleziono lokalnego pliku do tej operacji.");
	_LIT(KOverflow, "Za duzo danych z serwera. Sprobuj dokladniejszej frazy.");
	_LIT(KCorrupt, "Serwer zwrocil niepelne dane.");
	_LIT(KGenericFmt, "Blad operacji: %d");
	const TDesC* text = NULL;
	switch (aErr)
		{
		case KErrNoMemory:
			text = &KNoMemory;
			break;
		case KErrNotFound:
			text = &KNotFound;
			break;
		case KErrOverflow:
			text = &KOverflow;
			break;
		case KErrCorrupt:
			text = &KCorrupt;
			break;
		default:
			break;
		}

	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	if (text)
		{
		note->ExecuteLD(*text);
		return;
		}

	HBufC* msg = HBufC::NewLC(64);
	msg->Des().Format(KGenericFmt, aErr);
	note->ExecuteLD(*msg);
	CleanupStack::PopAndDestroy(msg);
	}

void CNokiaspotifyAppUi::CleanCacheL()
	{
	if (!iMusicService)
		{
		return;
		}
	const TInt removed = iMusicService->CleanCacheL();
	_LIT(KCleanFmt, "Cache wyczyszczony. Plikow: %d");
	HBufC* msg = HBufC::NewLC(96);
	msg->Des().Format(KCleanFmt, removed);
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(*msg);
	CleanupStack::PopAndDestroy(msg);
	if (iAppView)
		{
		iAppView->ClearPlaybackPanel();
		}
	}

void CNokiaspotifyAppUi::CreatePlaylistL()
	{
	TBuf<64> playlistName;
	playlistName.Copy(_L("MojaPlaylista"));
	_LIT(KPrompt, "Nazwa playlisty");
	if (!PromptTextL(playlistName, KPrompt))
		{
		return;
		}
	if (playlistName.Length() == 0)
		{
		playlistName.Copy(_L("MojaPlaylista"));
		}

	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	TFileName dataDir;
	ResolveDataDir(dataDir);
	EnsureFolderL(fs, dataDir);
	TFileName playlistFile(dataDir);
	playlistFile.Append(KPlaylistsFileName);

	TBuf<128> line;
	line.Append(playlistName);
	line.Append(_L("|"));
	AppendLineToTextFileL(fs, playlistFile, line);

	_LIT(KOk, "Playlista utworzona.");
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(KOk);
	CleanupStack::PopAndDestroy(&fs);
	}

void CNokiaspotifyAppUi::AddTrackToPlaylistL()
	{
	TBuf<64> playlistName;
	playlistName.Copy(_L("MojaPlaylista"));
	_LIT(KPlaylistPrompt, "Playlista");
	if (!PromptTextL(playlistName, KPlaylistPrompt))
		{
		return;
		}
	if (playlistName.Length() == 0)
		{
		playlistName.Copy(_L("MojaPlaylista"));
		}

	TBuf<128> trackName;
	_LIT(KTrackPrompt, "Nazwa utworu");
	PromptTextL(trackName, KTrackPrompt);
	if (trackName.Length() == 0)
		{
		RFs fsAuto;
		User::LeaveIfError(fsAuto.Connect());
		CleanupClosePushL(fsAuto);
		if (!FirstTrackNameL(fsAuto, trackName))
			{
			CleanupStack::PopAndDestroy(&fsAuto);
			_LIT(KNoTrack, "Brak utworow lokalnych do dodania.");
			CAknInformationNote* note = new (ELeave) CAknInformationNote;
			note->ExecuteLD(KNoTrack);
			return;
			}
		CleanupStack::PopAndDestroy(&fsAuto);
		}

	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	TFileName dataDir;
	ResolveDataDir(dataDir);
	EnsureFolderL(fs, dataDir);
	TFileName playlistFile(dataDir);
	playlistFile.Append(KPlaylistsFileName);

	TBuf<256> line;
	line.Append(playlistName);
	line.Append(_L("|"));
	line.Append(trackName);
	AppendLineToTextFileL(fs, playlistFile, line);

	_LIT(KOk, "Utwor dodany do playlisty.");
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(KOk);
	CleanupStack::PopAndDestroy(&fs);
	}

void CNokiaspotifyAppUi::ShowPlaylistsL()
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	TFileName dataDir;
	ResolveDataDir(dataDir);
	TFileName playlistFile(dataDir);
	playlistFile.Append(KPlaylistsFileName);

	RPointerArray<HBufC> lines;
	CleanupStack::PushL(TCleanupItem(CleanupOwnedBufArray, &lines));
	ReadTextFileLinesL(fs, playlistFile, lines);
	if (lines.Count() == 0)
		{
		_LIT(KNoPlaylists, "Brak playlist. Utworz nowa z menu.");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KNoPlaylists);
		}
	else
		{
		HBufC* msg = HBufC::NewLC(1024);
		TPtr msgDes = msg->Des();
		AppendLineLimited(msgDes, _L("Playlisty:"));
		const TInt shown = (lines.Count() < 20) ? lines.Count() : 20;
		for (TInt i = 0; i < shown; ++i)
			{
			TPtrC line(*lines[i]);
			const TInt sep = line.Locate('|');
			if (sep > 0)
				{
				AppendLimited(msgDes, line.Left(sep));
				if (sep + 1 < line.Length())
					{
					AppendLimited(msgDes, _L(" -> "));
					AppendLimited(msgDes, line.Mid(sep + 1));
					}
				AppendLimited(msgDes, _L("\n"));
				}
			}
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(*msg);
		CleanupStack::PopAndDestroy(msg);
		}

	CleanupStack::PopAndDestroy(&lines);
	CleanupStack::PopAndDestroy(&fs);
	}

void CNokiaspotifyAppUi::ShowPlaylistByNameL()
	{
	TBuf<64> playlistName;
	playlistName.Copy(_L("MojaPlaylista"));
	_LIT(KPrompt, "Nazwa playlisty");
	if (!PromptTextL(playlistName, KPrompt))
		{
		return;
		}
	if (playlistName.Length() == 0)
		{
		playlistName.Copy(_L("MojaPlaylista"));
		}

	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	TFileName dataDir;
	ResolveDataDir(dataDir);
	TFileName playlistFile(dataDir);
	playlistFile.Append(KPlaylistsFileName);

	RPointerArray<HBufC> lines;
	CleanupStack::PushL(TCleanupItem(CleanupOwnedBufArray, &lines));
	ReadTextFileLinesL(fs, playlistFile, lines);

	HBufC* msg = HBufC::NewLC(1024);
	TPtr msgDes = msg->Des();
	AppendLimited(msgDes, _L("Playlista: "));
	AppendLimited(msgDes, playlistName);
	AppendLimited(msgDes, _L("\n"));
	TInt found = 0;
	for (TInt i = 0; i < lines.Count(); ++i)
		{
		TPtrC line(*lines[i]);
		const TInt sep = line.Locate('|');
		if (sep <= 0)
			{
			continue;
			}
		TPtrC left = line.Left(sep);
		TPtrC right = line.Mid(sep + 1);
		if (left.CompareF(playlistName) == 0 && right.Length() > 0)
			{
			AppendLineLimited(msgDes, right);
			++found;
			if (found >= 20)
				{
				break;
				}
			}
		}
	if (found == 0)
		{
		msg->Des().Append(_L("Brak utworow."));
		}
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(*msg);
	CleanupStack::PopAndDestroy(msg);
	CleanupStack::PopAndDestroy(&lines);
	CleanupStack::PopAndDestroy(&fs);
	}

void CNokiaspotifyAppUi::RemoveTrackFromPlaylistL()
	{
	TBuf<64> playlistName;
	playlistName.Copy(_L("MojaPlaylista"));
	_LIT(KPlaylistPrompt, "Playlista");
	if (!PromptTextL(playlistName, KPlaylistPrompt))
		{
		return;
		}
	if (playlistName.Length() == 0)
		{
		playlistName.Copy(_L("MojaPlaylista"));
		}
	TBuf<128> trackName;
	_LIT(KTrackPrompt, "Utwor do usuniecia");
	PromptTextL(trackName, KTrackPrompt);
	if (trackName.Length() == 0)
		{
		_LIT(KNoTrackName, "Podaj nazwe utworu do usuniecia.");
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(KNoTrackName);
		return;
		}

	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	TFileName dataDir;
	ResolveDataDir(dataDir);
	EnsureFolderL(fs, dataDir);
	TFileName playlistFile(dataDir);
	playlistFile.Append(KPlaylistsFileName);

	RPointerArray<HBufC> lines;
	CleanupStack::PushL(TCleanupItem(CleanupOwnedBufArray, &lines));
	ReadTextFileLinesL(fs, playlistFile, lines);
	RPointerArray<HBufC> keep;
	CleanupStack::PushL(TCleanupItem(CleanupOwnedBufArray, &keep));

	TInt removed = 0;
	for (TInt i = 0; i < lines.Count(); ++i)
		{
		TPtrC line(*lines[i]);
		const TInt sep = line.Locate('|');
		if (sep <= 0)
			{
			AppendOwnedBufL(keep, line);
			continue;
			}
		TPtrC left = line.Left(sep);
		TPtrC right = line.Mid(sep + 1);
		if (left.CompareF(playlistName) == 0 && right.CompareF(trackName) == 0)
			{
			++removed;
			continue;
			}
		AppendOwnedBufL(keep, line);
		}

	RewriteTextFileL(fs, playlistFile, keep);
	_LIT(KRemovedFmt, "Usunieto wpisow: %d");
	HBufC* msg = HBufC::NewLC(64);
	msg->Des().Format(KRemovedFmt, removed);
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(*msg);
	CleanupStack::PopAndDestroy(msg);
	CleanupStack::PopAndDestroy(&keep);
	CleanupStack::PopAndDestroy(&lines);
	CleanupStack::PopAndDestroy(&fs);
	}

void CNokiaspotifyAppUi::DeletePlaylistL()
	{
	TBuf<64> playlistName;
	playlistName.Copy(_L("MojaPlaylista"));
	_LIT(KPrompt, "Usun playliste");
	if (!PromptTextL(playlistName, KPrompt))
		{
		return;
		}
	if (playlistName.Length() == 0)
		{
		playlistName.Copy(_L("MojaPlaylista"));
		}

	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	TFileName dataDir;
	ResolveDataDir(dataDir);
	EnsureFolderL(fs, dataDir);
	TFileName playlistFile(dataDir);
	playlistFile.Append(KPlaylistsFileName);

	RPointerArray<HBufC> lines;
	CleanupStack::PushL(TCleanupItem(CleanupOwnedBufArray, &lines));
	ReadTextFileLinesL(fs, playlistFile, lines);
	RPointerArray<HBufC> keep;
	CleanupStack::PushL(TCleanupItem(CleanupOwnedBufArray, &keep));

	TInt removed = 0;
	for (TInt i = 0; i < lines.Count(); ++i)
		{
		TPtrC line(*lines[i]);
		const TInt sep = line.Locate('|');
		if (sep <= 0)
			{
			AppendOwnedBufL(keep, line);
			continue;
			}
		TPtrC left = line.Left(sep);
		if (left.CompareF(playlistName) == 0)
			{
			++removed;
			continue;
			}
		AppendOwnedBufL(keep, line);
		}

	RewriteTextFileL(fs, playlistFile, keep);
	_LIT(KDeletedFmt, "Usunieto playlist entries: %d");
	HBufC* msg = HBufC::NewLC(64);
	msg->Des().Format(KDeletedFmt, removed);
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(*msg);
	CleanupStack::PopAndDestroy(msg);
	CleanupStack::PopAndDestroy(&keep);
	CleanupStack::PopAndDestroy(&lines);
	CleanupStack::PopAndDestroy(&fs);
	}

void CNokiaspotifyAppUi::RebuildLocalLibraryIndexL()
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);

	TFileName dataDir;
	ResolveDataDir(dataDir);
	EnsureFolderL(fs, dataDir);
	TFileName indexFile(dataDir);
	indexFile.Append(KLibraryIndexFileName);

	RPointerArray<HBufC> lines;
	CleanupStack::PushL(TCleanupItem(CleanupOwnedBufArray, &lines));

	CDir* dir = NULL;
	TFileName pattern(KMusicDirE);
	pattern.Append(_L("*.mp3"));
	if (fs.GetDir(pattern, KEntryAttNormal, ESortByName, dir) == KErrNone)
		{
		CleanupStack::PushL(dir);
		for (TInt i = 0; i < dir->Count(); ++i)
			{
			TFileName path(KMusicDirE);
			path.Append((*dir)[i].iName);
			AppendOwnedBufL(lines, path);
			}
		CleanupStack::PopAndDestroy(dir);
		}

	dir = NULL;
	TFileName patternE2(KMusicDirE);
	patternE2.Append(_L("*.m4a"));
	if (fs.GetDir(patternE2, KEntryAttNormal, ESortByName, dir) == KErrNone)
		{
		CleanupStack::PushL(dir);
		for (TInt i = 0; i < dir->Count(); ++i)
			{
			TFileName path(KMusicDirE);
			path.Append((*dir)[i].iName);
			AppendOwnedBufL(lines, path);
			}
		CleanupStack::PopAndDestroy(dir);
		}

	dir = NULL;
	TFileName pattern2(KMusicDirC);
	pattern2.Append(_L("*.mp3"));
	if (fs.GetDir(pattern2, KEntryAttNormal, ESortByName, dir) == KErrNone)
		{
		CleanupStack::PushL(dir);
		for (TInt i = 0; i < dir->Count(); ++i)
			{
			TFileName path(KMusicDirC);
			path.Append((*dir)[i].iName);
			AppendOwnedBufL(lines, path);
			}
		CleanupStack::PopAndDestroy(dir);
		}

	dir = NULL;
	TFileName patternC2(KMusicDirC);
	patternC2.Append(_L("*.m4a"));
	if (fs.GetDir(patternC2, KEntryAttNormal, ESortByName, dir) == KErrNone)
		{
		CleanupStack::PushL(dir);
		for (TInt i = 0; i < dir->Count(); ++i)
			{
			TFileName path(KMusicDirC);
			path.Append((*dir)[i].iName);
			AppendOwnedBufL(lines, path);
			}
		CleanupStack::PopAndDestroy(dir);
		}

	dir = NULL;
	TFileName pattern3(KTurboMusicDirE);
	pattern3.Append(_L("*.mp3"));
	if (fs.GetDir(pattern3, KEntryAttNormal, ESortByName, dir) == KErrNone)
		{
		CleanupStack::PushL(dir);
		for (TInt i = 0; i < dir->Count(); ++i)
			{
			TFileName path(KTurboMusicDirE);
			path.Append((*dir)[i].iName);
			AppendOwnedBufL(lines, path);
			}
		CleanupStack::PopAndDestroy(dir);
		}

	dir = NULL;
	TFileName pattern4(KTurboMusicDirE);
	pattern4.Append(_L("*.m4a"));
	if (fs.GetDir(pattern4, KEntryAttNormal, ESortByName, dir) == KErrNone)
		{
		CleanupStack::PushL(dir);
		for (TInt i = 0; i < dir->Count(); ++i)
			{
			TFileName path(KTurboMusicDirE);
			path.Append((*dir)[i].iName);
			AppendOwnedBufL(lines, path);
			}
		CleanupStack::PopAndDestroy(dir);
		}

	dir = NULL;
	TFileName pattern5(KTurboMusicDirC);
	pattern5.Append(_L("*.mp3"));
	if (fs.GetDir(pattern5, KEntryAttNormal, ESortByName, dir) == KErrNone)
		{
		CleanupStack::PushL(dir);
		for (TInt i = 0; i < dir->Count(); ++i)
			{
			TFileName path(KTurboMusicDirC);
			path.Append((*dir)[i].iName);
			AppendOwnedBufL(lines, path);
			}
		CleanupStack::PopAndDestroy(dir);
		}

	dir = NULL;
	TFileName pattern6(KTurboMusicDirC);
	pattern6.Append(_L("*.m4a"));
	if (fs.GetDir(pattern6, KEntryAttNormal, ESortByName, dir) == KErrNone)
		{
		CleanupStack::PushL(dir);
		for (TInt i = 0; i < dir->Count(); ++i)
			{
			TFileName path(KTurboMusicDirC);
			path.Append((*dir)[i].iName);
			AppendOwnedBufL(lines, path);
			}
		CleanupStack::PopAndDestroy(dir);
		}

	RewriteTextFileL(fs, indexFile, lines);

	_LIT(KDoneFmt, "Indeks biblioteki odswiezony. Utworow: %d");
	HBufC* msg = HBufC::NewLC(96);
	msg->Des().Format(KDoneFmt, lines.Count());
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(*msg);
	CleanupStack::PopAndDestroy(msg);
	CleanupStack::PopAndDestroy(&lines);
	CleanupStack::PopAndDestroy(&fs);
	}

void CNokiaspotifyAppUi::HandleLoginFromViewL()
	{
	TRAP_IGNORE(ShowAppStatusL());
	}

void CNokiaspotifyAppUi::HandleQuickSearchFromViewL()
	{
	if (iAppView)
		{
		iAppView->BeginInlineSearchInputL(EFalse);
		return;
		}
	TRAP_IGNORE(SearchMusicL());
	}

void CNokiaspotifyAppUi::HandleQuickCreatePlaylistFromViewL()
	{
	TRAP_IGNORE(CreatePlaylistL());
	}

void CNokiaspotifyAppUi::HandleQuickAddTrackToPlaylistFromViewL()
	{
	TRAP_IGNORE(AddTrackToPlaylistL());
	}

void CNokiaspotifyAppUi::HandleQuickShowPlaylistsFromViewL()
	{
	TRAP_IGNORE(ShowPlaylistsL());
	}

void CNokiaspotifyAppUi::HandleQuickToggleInternetFromViewL()
	{
	TRAP_IGNORE(ToggleInternetConnectionL());
	}

void CNokiaspotifyAppUi::HandleQuickOpenPlaylistFromViewL()
	{
	TRAP_IGNORE(ShowPlaylistByNameL());
	}

void CNokiaspotifyAppUi::HandleQuickReindexLibraryFromViewL()
	{
	TRAP_IGNORE(RebuildLocalLibraryIndexL());
	}

void CNokiaspotifyAppUi::HandleQuickPingFromViewL()
	{
	TRAPD(err, PingServerL());
	if (err != KErrNone)
		{
		_LIT(KPingErr, "Ping blad: %d");
		HBufC* msg = HBufC::NewLC(64);
		msg->Des().Format(KPingErr, err);
		CAknInformationNote* note = new (ELeave) CAknInformationNote;
		note->ExecuteLD(*msg);
		CleanupStack::PopAndDestroy(msg);
		}
	}

void CNokiaspotifyAppUi::HandleQuickOnlineSearchFromViewL()
	{
	if (iAppView)
		{
		iAppView->BeginInlineSearchInputL(ETrue);
		return;
		}
	TRAP_IGNORE(SearchMusicOnlineL());
	}

void CNokiaspotifyAppUi::HandleQuickShowTrackListFromViewL()
	{
	TRAP_IGNORE(ShowTrackListL());
	}

void CNokiaspotifyAppUi::HandleQuickCleanCacheFromViewL()
	{
	TRAP_IGNORE(CleanCacheL());
	}

void CNokiaspotifyAppUi::HandleInlineSearchQueryFromViewL(const TDesC& aQuery)
	{
	TRAPD(err, SearchMusicByQueryL(aQuery));
	if (err != KErrNone)
		{
		TRAP_IGNORE(ShowOperationErrorL(err));
		return;
		}
	if (iCurrentTracks.Count() > 0)
		{
		HandleTrackChosenFromViewL(0);
		}
	}

void CNokiaspotifyAppUi::HandleInlineOnlineSearchQueryFromViewL(const TDesC& aQuery)
	{
	TRAPD(err, SearchMusicOnlineByQueryL(aQuery));
	if (err != KErrNone)
		{
		TRAP_IGNORE(ShowOperationErrorL(err));
		return;
		}
	if (iCurrentTracks.Count() > 0)
		{
		HandleTrackChosenFromViewL(0);
		}
	}

void CNokiaspotifyAppUi::HandleTrackChosenFromViewL(TInt aIndex)
	{
	if (aIndex < 0 || aIndex >= iCurrentTracks.Count() || !iMusicService || !iCurrentTracks[aIndex])
		{
		return;
		}
	TRAPD(err, {
		CopyCurrentTracksToPlaybackQueueL();
		PlayQueueIndexL(aIndex);
	});
	if (err != KErrNone)
		{
		TRAP_IGNORE(ShowOperationErrorL(err));
		}
	}

void CNokiaspotifyAppUi::HandleSaveTrackFromViewL(TInt aIndex)
	{
	if (aIndex < 0 || aIndex >= iCurrentTracks.Count() || !iMusicService || !iCurrentTracks[aIndex])
		{
		return;
		}
	TRAPD(err, {
		TFileName oldLocalPath;
		if (iCurrentTracks[aIndex]->iLocalPath)
			{
			oldLocalPath.Copy(*iCurrentTracks[aIndex]->iLocalPath);
			}
		TBuf<128> oldFileName;
		if (iCurrentTracks[aIndex]->iFileName)
			{
			oldFileName.Copy(iCurrentTracks[aIndex]->iFileName->Left(oldFileName.MaxLength()));
			}
		TBuf<256> oldRemoteUrl;
		if (iCurrentTracks[aIndex]->iRemoteUrl)
			{
			oldRemoteUrl.Copy(iCurrentTracks[aIndex]->iRemoteUrl->Left(oldRemoteUrl.MaxLength()));
			}
		iMusicService->SaveTrackPermanentL(*iCurrentTracks[aIndex]);
		for (TInt i = 0; i < iPlaybackQueue.Count(); ++i)
			{
			if (!iPlaybackQueue[i])
				{
				continue;
				}
			const TBool sameLocal = (oldLocalPath.Length() > 0 && iPlaybackQueue[i]->iLocalPath &&
				iPlaybackQueue[i]->iLocalPath->CompareF(oldLocalPath) == 0);
			const TBool sameFile = (oldFileName.Length() > 0 && iPlaybackQueue[i]->iFileName &&
				iPlaybackQueue[i]->iFileName->CompareF(oldFileName) == 0);
			const TBool sameRemote = (oldRemoteUrl.Length() > 0 && iPlaybackQueue[i]->iRemoteUrl &&
				iPlaybackQueue[i]->iRemoteUrl->CompareF(oldRemoteUrl) == 0);
			if (!sameLocal && !sameFile && !sameRemote)
				{
				continue;
				}
			if (iCurrentTracks[aIndex]->iLocalPath)
				{
				iPlaybackQueue[i]->SetLocalPathL(*iCurrentTracks[aIndex]->iLocalPath);
				}
			if (iCurrentTracks[aIndex]->iFileName)
				{
				iPlaybackQueue[i]->SetFileNameL(*iCurrentTracks[aIndex]->iFileName);
				}
			if (iCurrentTracks[aIndex]->iDisplay)
				{
				iPlaybackQueue[i]->SetDisplayL(*iCurrentTracks[aIndex]->iDisplay);
				}
			iPlaybackQueue[i]->iPermanent = iCurrentTracks[aIndex]->iPermanent;
			iPlaybackQueue[i]->iCached = iCurrentTracks[aIndex]->iCached;
			iPlaybackQueue[i]->iOnlineOnly = iCurrentTracks[aIndex]->iOnlineOnly;
			}
		if (oldFileName.Length() > 0 && iCurrentPlaybackName.CompareF(oldFileName) == 0)
			{
			iCurrentPlaybackName.Copy(oldFileName);
			iCurrentTrackFromInternet = EFalse;
			UpdatePlaybackUi();
			}
		ShowCurrentTrackListL(iCurrentTrackListTitle);
	});
	if (err != KErrNone)
		{
		TRAP_IGNORE(ShowOperationErrorL(err));
		}
	}

void CNokiaspotifyAppUi::HandleDeleteTrackFromViewL(TInt aIndex)
	{
	if (aIndex < 0 || aIndex >= iCurrentTracks.Count() || !iMusicService || !iCurrentTracks[aIndex])
		{
		return;
		}
	TFileName deletedPath;
	if (iCurrentTracks[aIndex]->iLocalPath)
		{
		deletedPath.Copy(*iCurrentTracks[aIndex]->iLocalPath);
		}

	TRAPD(err, iMusicService->DeleteTrackL(*iCurrentTracks[aIndex]));
	if (err != KErrNone)
		{
		TRAP_IGNORE(ShowOperationErrorL(err));
		return;
		}

	delete iCurrentTracks[aIndex];
	iCurrentTracks.Remove(aIndex);
	for (TInt i = iPlaybackQueue.Count() - 1; i >= 0; --i)
		{
		if (!iPlaybackQueue[i] || !iPlaybackQueue[i]->iLocalPath || deletedPath.Length() == 0)
			{
			continue;
			}
		if (iPlaybackQueue[i]->iLocalPath->CompareF(deletedPath) == 0)
			{
			delete iPlaybackQueue[i];
			iPlaybackQueue.Remove(i);
			if (iPlaybackIndex == i)
				{
				iPlaybackIndex = KErrNotFound;
				iCurrentPlaybackName.Zero();
				StopPlayback();
				}
			else if (iPlaybackIndex > i)
				{
				--iPlaybackIndex;
				}
			}
		}
	if (iAppView)
		{
		iAppView->ClearPlaybackPanel();
		}
	if (iCurrentTracks.Count() == 0)
		{
		iCurrentPlaybackName.Zero();
		StopPlayback();
		if (iAppView)
			{
			iAppView->ShowHomeScreen();
			}
		return;
		}
	TRAP_IGNORE(ShowCurrentTrackListL(iCurrentTrackListTitle));
	}

void CNokiaspotifyAppUi::HandleOpenNowPlayingFromViewL()
	{
	TRAP_IGNORE(OpenNowPlayingScreenL());
	}

void CNokiaspotifyAppUi::HandlePlaybackPrevFromViewL()
	{
	const TInt prev = ResolvePrevQueueIndex();
	if (prev != KErrNotFound)
		{
		TRAP_IGNORE(PlayQueueIndexL(prev));
		}
	}

void CNokiaspotifyAppUi::HandlePlaybackToggleFromViewL()
	{
	if (iCurrentPlaybackName.Length() == 0)
		{
		return;
		}
	if (iAudioPlayer && iAudioPlaying)
		{
		StopPlayback();
		return;
		}
	if (iPlaybackIndex != KErrNotFound)
		{
		TRAP_IGNORE(PlayQueueIndexL(iPlaybackIndex));
		}
	}

void CNokiaspotifyAppUi::HandlePlaybackNextFromViewL()
	{
	const TInt next = ResolveNextQueueIndex();
	if (next != KErrNotFound)
		{
		TRAP_IGNORE(PlayQueueIndexL(next));
		}
	}

void CNokiaspotifyAppUi::HandlePlaybackShuffleFromViewL()
	{
	ToggleShuffle();
	}

void CNokiaspotifyAppUi::ShowLoginInfoL()
	{
	_LIT(KLoginInfo, "TurboMusic: bez logowania. Otworz cache status klawiszem OK.");
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(KLoginInfo);
	}
// -----------------------------------------------------------------------------
//  Called by the framework when the application status pane
//  size is changed.  Passes the new client rectangle to the
//  AppView
// -----------------------------------------------------------------------------
//
void CNokiaspotifyAppUi::HandleStatusPaneSizeChange()
	{
	if (iAppView)
		{
		iAppView->SetRect(ClientRect());
		}
	}

CArrayFix<TCoeHelpContext>* CNokiaspotifyAppUi::HelpContextL() const
	{
//#warning "Please see comment about help and UID3..."
	// Note: Help will not work if the application uid3 is not in the
	// protected range.  The default uid3 range for projects created
	// from this template (0xE0000000 - 0xEFFFFFFF) are not in the protected range so that they
	// can be self signed and installed on the device during testing.
	// Once you get your official uid3 from Symbian Ltd. and find/replace
	// all occurrences of uid3 in your project, the context help will
	// work. Alternatively, a patch now exists for the versions of
	// HTML help compiler in SDKs and can be found here along with an FAQ:
	// http://www3.symbian.com/faq.nsf/AllByDate/E9DF3257FD565A658025733900805EA2?OpenDocument
#ifdef _HELP_AVAILABLE_
	CArrayFixFlat<TCoeHelpContext>* array = new(ELeave)CArrayFixFlat<TCoeHelpContext>(1);
	CleanupStack::PushL(array);
//	array->AppendL(TCoeHelpContext(KUidNokiaspotifyApp, KGeneral_Information));
	CleanupStack::Pop(array);
	return array;
#else
	return NULL;
#endif
	}

// End of File
