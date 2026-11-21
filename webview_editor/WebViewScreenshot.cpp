#include "WebViewScreenshot.h"

#if JUCE_MAC
 #import <WebKit/WebKit.h>
#elif JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>

 #ifndef PW_RENDERFULLCONTENT
  #define PW_RENDERFULLCONTENT 0x00000002
 #endif
#endif

namespace
{
   #if JUCE_MAC
    juce::Image imageFrom (NSImage* snapshot)
    {
        NSData* tiff = [snapshot TIFFRepresentation];

        if (tiff == nil)
            return {};

        NSBitmapImageRep* pixels = [NSBitmapImageRep imageRepWithData: tiff];

        // A PNG from JUCE names no colour space, so it is read as sRGB
        NSBitmapImageRep* inSrgb = [pixels bitmapImageRepByConvertingToColorSpace: [NSColorSpace sRGBColorSpace]
                                                                  renderingIntent: NSColorRenderingIntentDefault];
        NSData* png = [inSrgb representationUsingType: NSBitmapImageFileTypePNG properties: @{}];

        return png == nil ? juce::Image() : juce::ImageFileFormat::loadFrom ([png bytes], [png length]);
    }

    // JUCE keeps its web view to itself: it is the view of one of the browser's children
    WKWebView* macWebView (juce::WebBrowserComponent& browser)
    {
        for (auto* child : browser.getChildren())
            if (auto* host = dynamic_cast<juce::NSViewComponent*> (child))
                if (id view = (id) host->getView(); [view isKindOfClass: [WKWebView class]])
                    return view;

        return nil;
    }

    void takeMacScreenshot (juce::WebBrowserComponent& browser, Screenshot::ImageCallback done)
    {
        auto* view = macWebView (browser);

        if (view == nil)
            return done ({});

        [view takeSnapshotWithConfiguration: nil
                          completionHandler: ^(NSImage* snapshot, NSError*) { done (imageFrom (snapshot)); }];
    }
   #elif JUCE_WINDOWS
    // WebView2 draws on the GPU, where only PrintWindow's full content reaches
    juce::Image takeWindowsScreenshot (juce::Component& browser)
    {
        auto* peer = browser.getPeer();

        if (peer == nullptr)
            return {};

        const auto window = static_cast<HWND> (peer->getNativeHandle());
        RECT client {};

        if (! GetClientRect (window, &client) || client.right <= 0 || client.bottom <= 0)
            return {};

        const int width = client.right, height = client.bottom;

        BITMAPINFO info {};
        info.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height; // the top row first
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        const auto windowContext = GetDC (window);
        const auto memoryContext = CreateCompatibleDC (windowContext);
        void* bits = nullptr;
        const auto bitmap = CreateDIBSection (memoryContext, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        juce::Image whole;

        if (bitmap != nullptr)
        {
            const auto previous = SelectObject (memoryContext, bitmap);

            if (PrintWindow (window, memoryContext, PW_CLIENTONLY | PW_RENDERFULLCONTENT))
            {
                GdiFlush();

                whole = juce::Image (juce::Image::RGB, width, height, false);
                juce::Image::BitmapData pixels (whole, juce::Image::BitmapData::writeOnly);
                const auto* blueGreenRed = static_cast<const juce::uint8*> (bits);

                for (int row = 0; row < height; ++row)
                    for (int column = 0; column < width; ++column, blueGreenRed += 4)
                        pixels.setPixelColour (column, row, juce::Colour (blueGreenRed[2], blueGreenRed[1], blueGreenRed[0]));
            }

            SelectObject (memoryContext, previous);
            DeleteObject (bitmap);
        }

        DeleteDC (memoryContext);
        ReleaseDC (window, windowContext);

        if (! whole.isValid())
            return {};

        // The Standalone's window holds more than the web view
        auto& windowComponent = peer->getComponent();
        const auto pixelsPerPoint = static_cast<double> (width) / windowComponent.getWidth();
        const auto browserArea = windowComponent.getLocalArea (&browser, browser.getLocalBounds()).toDouble() * pixelsPerPoint;

        return whole.getClippedImage (browserArea.toNearestInt()).createCopy();
    }
   #endif
}

void WebViewScreenshot::of (juce::WebBrowserComponent& browser, Screenshot::ImageCallback done)
{
   #if JUCE_MAC
    takeMacScreenshot (browser, std::move (done));
   #elif JUCE_WINDOWS
    done (takeWindowsScreenshot (browser));
   #else
    done ({});
   #endif
}
