/*
 ============================================================================
 Name		: NokiaspotifyDocument.h
 Author	  : 
 Copyright   : Your copyright notice
 Description : Declares document class for application.
 ============================================================================
 */

#ifndef __NOKIASPOTIFYDOCUMENT_h__
#define __NOKIASPOTIFYDOCUMENT_h__

// INCLUDES
#include <akndoc.h>

// FORWARD DECLARATIONS
class CNokiaspotifyAppUi;
class CEikApplication;

// CLASS DECLARATION

/**
 * CNokiaspotifyDocument application class.
 * An instance of class CNokiaspotifyDocument is the Document part of the
 * AVKON application framework for the Nokiaspotify example application.
 */
class CNokiaspotifyDocument : public CAknDocument
	{
public:
	// Constructors and destructor

	/**
	 * NewL.
	 * Two-phased constructor.
	 * Construct a CNokiaspotifyDocument for the AVKON application aApp
	 * using two phase construction, and return a pointer
	 * to the created object.
	 * @param aApp Application creating this document.
	 * @return A pointer to the created instance of CNokiaspotifyDocument.
	 */
	static CNokiaspotifyDocument* NewL(CEikApplication& aApp);

	/**
	 * NewLC.
	 * Two-phased constructor.
	 * Construct a CNokiaspotifyDocument for the AVKON application aApp
	 * using two phase construction, and return a pointer
	 * to the created object.
	 * @param aApp Application creating this document.
	 * @return A pointer to the created instance of CNokiaspotifyDocument.
	 */
	static CNokiaspotifyDocument* NewLC(CEikApplication& aApp);

	/**
	 * ~CNokiaspotifyDocument
	 * Virtual Destructor.
	 */
	virtual ~CNokiaspotifyDocument();

public:
	// Functions from base classes

	/**
	 * CreateAppUiL
	 * From CEikDocument, CreateAppUiL.
	 * Create a CNokiaspotifyAppUi object and return a pointer to it.
	 * The object returned is owned by the Uikon framework.
	 * @return Pointer to created instance of AppUi.
	 */
	CEikAppUi* CreateAppUiL();

private:
	// Constructors

	/**
	 * ConstructL
	 * 2nd phase constructor.
	 */
	void ConstructL();

	/**
	 * CNokiaspotifyDocument.
	 * C++ default constructor.
	 * @param aApp Application creating this document.
	 */
	CNokiaspotifyDocument(CEikApplication& aApp);

	};

#endif // __NOKIASPOTIFYDOCUMENT_h__
// End of File
