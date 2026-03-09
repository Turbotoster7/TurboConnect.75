/*
 ============================================================================
 Name		: NokiaspotifyAppView.h
 Author	  :
 Copyright   : Your copyright notice
 Description : Declares view class for application.
 ============================================================================
 */

#ifndef __NOKIASPOTIFYAPPVIEW_h__
#define __NOKIASPOTIFYAPPVIEW_h__

// INCLUDES
#include <coecntrl.h>
#include <e32base.h>
#include <e32std.h>

// CLASS DECLARATION
class CNokiaspotifyAppView : public CCoeControl
	{
public:
	// New methods

	/**
	 * NewL.
	 * Two-phased constructor.
	 * Create a CNokiaspotifyAppView object, which will draw itself to aRect.
	 * @param aRect The rectangle this view will be drawn to.
	 * @return a pointer to the created instance of CNokiaspotifyAppView.
	 */
	static CNokiaspotifyAppView* NewL(const TRect& aRect);

	/**
	 * NewLC.
	 * Two-phased constructor.
	 * Create a CNokiaspotifyAppView object, which will draw itself
	 * to aRect.
	 * @param aRect Rectangle this view will be drawn to.
	 * @return A pointer to the created instance of CNokiaspotifyAppView.
	 */
	static CNokiaspotifyAppView* NewLC(const TRect& aRect);

	/**
	 * ~CNokiaspotifyAppView
	 * Virtual Destructor.
	 */
	virtual ~CNokiaspotifyAppView();

public:
	// Functions from base classes

	/**
	 * From CCoeControl, Draw
	 * Draw this CNokiaspotifyAppView to the screen.
	 * @param aRect the rectangle of this view that needs updating
	 */
	void Draw(const TRect& aRect) const;

	/**
	 * From CoeControl, SizeChanged.
	 * Called by framework when the view size is changed.
	 */
	virtual void SizeChanged();

	/**
	 * From CoeControl, HandlePointerEventL.
	 * Called by framework when a pointer touch event occurs.
	 * Note: although this method is compatible with earlier SDKs,
	 * it will not be called in SDKs without Touch support.
	 * @param aPointerEvent the information about this event
	 */
	virtual void HandlePointerEventL(const TPointerEvent& aPointerEvent);

	virtual TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
	void BeginInlineSearchInputL(TBool aOnline);
	void ShowTrackListL(const RPointerArray<HBufC>& aTracks, const TDesC& aTitle);
	void ShowHomeScreen();
	void SetPlaybackPanel(const TDesC& aTitle, const TDesC& aDetail);
	void ClearPlaybackPanel();
	void SetNowPlayingState(const TDesC& aTitle, const TDesC& aDetail, const TDesC& aStatus, TBool aShuffleEnabled);
	void ShowNowPlayingScreen();
	TBool IsNowPlayingVisible() const;

private:
	// Constructors

	/**
	 * ConstructL
	 * 2nd phase constructor.
