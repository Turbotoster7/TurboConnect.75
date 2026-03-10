#ifndef CNOKIASPOTIFYNETWORK_H
#define CNOKIASPOTIFYNETWORK_H

#include <e32base.h>
#include <es_sock.h>

class CNokiaspotifyNetwork
	{
public:
	static CNokiaspotifyNetwork* NewL();
	CNokiaspotifyNetwork();
	~CNokiaspotifyNetwork();

	void ConnectL();
	void Disconnect();
	TBool IsConnected() const;
	TInt LastError() const;

	void LoadSavedIapL();
	void SaveIapL(TUint32 aId);
	void ClearSavedIap();
	TBool HasSavedIap() const;

private:
	TUint32 iSelectedIapId;
	TBool iIsConnected;
	TBool iConnectionStarting;
	TInt iLastError;
	TBool iSocketServerOpen;
	TBool iConnectionOpen;
	RSocketServ iSocketServer;
	RConnection iConnection;
	};

#endif
