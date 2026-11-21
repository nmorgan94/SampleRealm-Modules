#pragma once

//==============================================================================
/** A picture of the page a web view is showing, at the display's scale: JUCE can't paint it. */
namespace WebViewScreenshot
{
    void of (juce::WebBrowserComponent&, Screenshot::ImageCallback done);
}
