#include "CNokiaspotifyNetwork.h"
#include <commdbconnpref.h>

CNokiaspotifyNetwork::CNokiaspotifyNetwork()
	: iSelectedIapId(0)
	, iIsConnected(EFalse)
	, iConnectionStarting(EFalse)
	, iLastError(KErrNone)
	, iSocketServerOpen(EFalse)
	, iConnectionOpen(EFalse)
	{
	}

CNokiaspotifyNetwork* CNokiaspotifyNetwork::NewL()
	{
	return new (ELeave) CNokiaspotifyNetwork();
	}

CNokiaspotifyNetwork::~CNokiaspotifyNetwork()
	{
	Disconnect();
	}

void CNokiaspotifyNetwork::ConnectL()
	{
	if (iIsConnected || iConnectionStarting)
		{
		return;
		}
	iConnectionStarting = ETrue;
	iLastError = KErrNone;

	if (!iSocketServerOpen)
		{
		iLastError = iSocketServer.Connect();
		if (iLastError != KErrNone)
			{
			iConnectionStarting = EFalse;
			User::Leave(iLastError);
			}
		iSocketServerOpen = ETrue;
		}

	if (!iConnectionOpen)
		{
		iLastError = iConnection.Open(iSocketServer);
		if (iLastError != KErrNone)
			{
			iConnectionStarting = EFalse;
			User::Leave(iLastError);
			}
		iConnectionOpen = ETrue;
		}

	TCommDbConnPref pref;
	pref.SetDialogPreference(iSelectedIapId ? ECommDbDialogPrefDoNotPrompt : ECommDbDialogPrefPrompt);
	pref.SetDirection(ECommDbConnectionDirectionOutgoing);
	if (iSelectedIapId)
		{
		pref.SetIapId(iSelectedIapId);
		}

	iLastError = iConnection.Start(pref);
	iConnectionStarting = EFalse;
	if (iLastError != KErrNone)
		{
		Disconnect();
		User::Leave(iLastError);
		}

	iIsConnected = ETrue;
	}

void CNokiaspotifyNetwork::Disconnect()
	{
	iConnectionStarting = EFalse;
	iIsConnected = EFalse;
	if (iConnectionOpen)
		{
		iConnection.Stop();
		iConnection.Close();
		iConnectionOpen = EFalse;
		}
	if (iSocketServerOpen)
		{
		iSocketServer.Close();
		iSocketServerOpen = EFalse;
		}
	}

TBool CNokiaspotifyNetwork::IsConnected() const
	{
	return iIsConnected;
	}

TInt CNokiaspotifyNetwork::LastError() const
	{
	return iLastError;
	}

void CNokiaspotifyNetwork::LoadSavedIapL()
	{
	// TODO: load from persistent settings
	iSelectedIapId = 0;
	}

void CNokiaspotifyNetwork::SaveIapL(TUint32 aId)
	{
	// TODO: save to persistent settings
	iSelectedIapId = aId;
	}

void CNokiaspotifyNetwork::ClearSavedIap()
	{
	iSelectedIapId = 0;
	}

TBool CNokiaspotifyNetwork::HasSavedIap() const
	{
	return (iSelectedIapId != 0);
	}
