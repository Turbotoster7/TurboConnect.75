/*
 ============================================================================
 Name		: Nokiaspotify.cpp
 Author	  : 
 Copyright   : Your copyright notice
 Description : Main application class
 ============================================================================
 */

// INCLUDE FILES
#include <eikstart.h>
#include "NokiaspotifyApplication.h"

LOCAL_C CApaApplication* NewApplication()
	{
	return new CNokiaspotifyApplication;
	}

GLDEF_C TInt E32Main()
	{
	return EikStart::RunApplication(NewApplication);
	}

