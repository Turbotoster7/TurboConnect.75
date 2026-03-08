/*
 ============================================================================
 Name		: Nokiaspotify.pan
 Author	  : 
 Copyright   : Your copyright notice
 Description : This file contains panic codes.
 ============================================================================
 */

#ifndef __NOKIASPOTIFY_PAN__
#define __NOKIASPOTIFY_PAN__

/** Nokiaspotify application panic codes */
enum TNokiaspotifyPanics
	{
	ENokiaspotifyUi = 1
	// add further panics here
	};

inline void Panic(TNokiaspotifyPanics aReason)
	{
	_LIT(applicationName, "TurboMusic");
	User::Panic(applicationName, aReason);
	}

#endif // __NOKIASPOTIFY_PAN__
