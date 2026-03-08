/*
 ============================================================================
 Name		: NokiaspotifyApplication.h
 Author	  : 
 Copyright   : Your copyright notice
 Description : Declares main application class.
 ============================================================================
 */

#ifndef __NOKIASPOTIFYAPPLICATION_H__
#define __NOKIASPOTIFYAPPLICATION_H__

// INCLUDES
#include <aknapp.h>
#include "Nokiaspotify.hrh"

// UID for the application;
// this should correspond to the uid defined in the mmp file
const TUid KUidNokiaspotifyApp =
	{
	_UID3
	};

// CLASS DECLARATION

/**
 * CNokiaspotifyApplication application class.
 * Provides factory to create concrete document object.
 * An instance of CNokiaspotifyApplication is the application part of the
 * AVKON application framework for the Nokiaspotify example application.
 */
class CNokiaspotifyApplication : public CAknApplication
	{
public:
	// Functions from base classes

	/**
	 * From CApaApplication, AppDllUid.
	 * @return Application's UID (KUidNokiaspotifyApp).
	 */
	TUid AppDllUid() const;

protected:
	// Functions from base classes

	/**
	 * From CApaApplication, CreateDocumentL.
	 * Creates CNokiaspotifyDocument document object. The returned
	 * pointer in not owned by the CNokiaspotifyApplication object.
	 * @return A pointer to the created document object.
	 */
	CApaDocument* CreateDocumentL();
	};

#endif // __NOKIASPOTIFYAPPLICATION_H__
// End of File
