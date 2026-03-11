/*
 ============================================================================
 Name		: NokiaspotifyApplication.cpp
 Author	  : 
 Copyright   : Your copyright notice
 Description : Main application class
 ============================================================================
 */

// INCLUDE FILES
#include "Nokiaspotify.hrh"
#include "NokiaspotifyDocument.h"
#include "NokiaspotifyApplication.h"

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CNokiaspotifyApplication::CreateDocumentL()
// Creates CApaDocument object
// -----------------------------------------------------------------------------
//
CApaDocument* CNokiaspotifyApplication::CreateDocumentL()
	{
	// Create an Nokiaspotify document, and return a pointer to it
	return CNokiaspotifyDocument::NewL(*this);
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyApplication::AppDllUid()
// Returns application UID
// -----------------------------------------------------------------------------
//
TUid CNokiaspotifyApplication::AppDllUid() const
	{
	// Return the UID for the Nokiaspotify application
	return KUidNokiaspotifyApp;
	}

// End of File
