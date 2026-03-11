/*
 ============================================================================
 Name		: NokiaspotifyDocument.cpp
 Author	  : 
 Copyright   : Your copyright notice
 Description : CNokiaspotifyDocument implementation
 ============================================================================
 */

// INCLUDE FILES
#include "NokiaspotifyAppUi.h"
#include "NokiaspotifyDocument.h"

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CNokiaspotifyDocument::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CNokiaspotifyDocument* CNokiaspotifyDocument::NewL(CEikApplication& aApp)
	{
	CNokiaspotifyDocument* self = NewLC(aApp);
	CleanupStack::Pop(self);
	return self;
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyDocument::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CNokiaspotifyDocument* CNokiaspotifyDocument::NewLC(CEikApplication& aApp)
	{
	CNokiaspotifyDocument* self = new (ELeave) CNokiaspotifyDocument(aApp);

	CleanupStack::PushL(self);
	self->ConstructL();
	return self;
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyDocument::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CNokiaspotifyDocument::ConstructL()
	{
	// No implementation required
	}

// -----------------------------------------------------------------------------
// CNokiaspotifyDocument::CNokiaspotifyDocument()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CNokiaspotifyDocument::CNokiaspotifyDocument(CEikApplication& aApp) :
	CAknDocument(aApp)
	{
	// No implementation required
	}

// ---------------------------------------------------------------------------
// CNokiaspotifyDocument::~CNokiaspotifyDocument()
// Destructor.
// ---------------------------------------------------------------------------
//
CNokiaspotifyDocument::~CNokiaspotifyDocument()
	{
	// No implementation required
	}

// ---------------------------------------------------------------------------
// CNokiaspotifyDocument::CreateAppUiL()
// Constructs CreateAppUi.
// ---------------------------------------------------------------------------
//
CEikAppUi* CNokiaspotifyDocument::CreateAppUiL()
	{
	// Create the application user interface, and return a pointer to it;
	// the framework takes ownership of this object
	return new (ELeave) CNokiaspotifyAppUi;
	}

// End of File
