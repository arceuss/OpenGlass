#include "pch.h"
#include "MsstyleInternals.hpp"
#include "CaptionTextHandler.hpp"
#include "Shared.hpp"
#include "GlassKernel.hpp"
#include "uDWMProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "dcompPrivates.hpp"
#include "CustomThemeAtlasLoader.hpp"
#include "CaptionTextRealizer.hpp"

using namespace OpenGlass;
namespace OpenGlass::CaptionTextHandler
{
	int WINAPI MyDrawTextW(
		HDC hdc,
		LPCWSTR lpchText,
		int cchText,
		LPRECT lprc,
		UINT format
	);
	HBITMAP WINAPI MyCreateBitmap(
		int nWidth,
		int nHeight,
		UINT nPlanes,
		UINT nBitCount,
		const void* lpBits
	);
	HRESULT MyIWICImagingFactory2_CreateBitmapFromHBITMAP(
		IWICImagingFactory2* This,
		HBITMAP hBitmap,
		HPALETTE hPalette,
		WICBitmapAlphaChannelOption options,
		IWICBitmap** ppIBitmap
	);
	HRESULT MyCText_ValidateResources(uDWM::CText* This);
	HRESULT MyCDrawImageInstruction_Create(uDWM::CBitmapSource* bitmapSource, LPCRECT lprc, PVOID* instruction);
	HRESULT MyCText_InitializeVisualTreeClone(uDWM::CText* This, uDWM::CText* clonedVisual, UINT cloneOption);
	HRESULT MyCText_CloneVisualTree(uDWM::CText* This, uDWM::CText** clonedVisual, bool unknown1, bool unknown2, bool unknown3);
	HRESULT MyCText_scalar_deleting_destructor(uDWM::CText* This, BYTE flag);
	HRESULT MyCChannel_MatrixTransformUpdate(dwmcore::CChannel* This, UINT handleId, MilMatrix3x2D* matrix);

	void MyID2D1DeviceContext_DrawTextLayout(
		ID2D1DeviceContext* This,
		D2D1_POINT_2F origin,
		IDWriteTextLayout* textLayout,
		ID2D1Brush* defaultFillBrush,
		D2D1_DRAW_TEXT_OPTIONS options
	);
	HRESULT MyICompositionGraphicsDevice_CreateDrawingSurface(
		abi::ICompositionGraphicsDevice* This,
		abi::Size sizePixels,
		abi::DirectXPixelFormat pixelFormat,
		abi::DirectXAlphaMode alphaMode,
		abi::ICompositionDrawingSurface** result
	);
	HRESULT MyICompositionSurfaceBrush2_put_Offset(
		abi::ICompositionSurfaceBrush2* This,
		abi::Vector2 value
	);
	HRESULT MyCDWriteText_ValidateVisual(uDWM::CDWriteText* This);
	HRESULT MyCDWriteText_UpdateOffset(uDWM::CDWriteText* This);
	HRESULT MyCDWriteText_SetSize(uDWM::CDWriteText* This, const SIZE* size);
	HRESULT MyCDWriteText_InitializeVisualTreeClone(uDWM::CDWriteText* This, uDWM::CDWriteText* clonedVisual, UINT cloneOption);
	HRESULT MyCDWriteText_scalar_deleting_destructor(uDWM::CDWriteText* This, BYTE flag);

	decltype(&MyDrawTextW) g_DrawTextW_Org{ nullptr };
	decltype(&MyCreateBitmap) g_CreateBitmap_Org{ nullptr };
	decltype(&MyIWICImagingFactory2_CreateBitmapFromHBITMAP) g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org{ nullptr };
	decltype(&MyCText_ValidateResources) g_CText_ValidateResources_Org{ nullptr };
	decltype(&MyCDrawImageInstruction_Create) g_CDrawImageInstruction_Create_Org{ nullptr };
	decltype(&MyCText_InitializeVisualTreeClone) g_CText_InitializeVisualTreeClone_Org{ nullptr };
	decltype(&MyCText_CloneVisualTree) g_CText_CloneVisualTree_Org{ nullptr };
	decltype(&MyCText_scalar_deleting_destructor) g_CText_scalar_deleting_destructor_Org{ nullptr };
	decltype(&MyCText_scalar_deleting_destructor)* g_CText_scalar_deleting_destructor_Org_Address{ nullptr };
	decltype(&MyCChannel_MatrixTransformUpdate) g_CChannel_MatrixTransformUpdate_Org{ nullptr };
	decltype(&MyIWICImagingFactory2_CreateBitmapFromHBITMAP)* g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address{ nullptr };

	decltype(&MyID2D1DeviceContext_DrawTextLayout) g_ID2D1DeviceContext_DrawTextLayout_Org{ nullptr };
	decltype(&MyID2D1DeviceContext_DrawTextLayout)* g_ID2D1DeviceContext_DrawTextLayout_Org_Address{ nullptr };
	decltype(&MyICompositionGraphicsDevice_CreateDrawingSurface) g_ICompositionGraphicsDevice_CreateDrawingSurface_Org{ nullptr };
	decltype(&MyICompositionGraphicsDevice_CreateDrawingSurface)* g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address{ nullptr };
	decltype(&MyICompositionSurfaceBrush2_put_Offset) g_ICompositionSurfaceBrush2_put_Offset_Org{ nullptr };
	decltype(&MyICompositionSurfaceBrush2_put_Offset)* g_ICompositionSurfaceBrush2_put_Offset_Org_Address{ nullptr };
	decltype(&MyCDWriteText_ValidateVisual) g_CDWriteText_ValidateVisual_Org{ nullptr };
	decltype(&MyCDWriteText_UpdateOffset) g_CDWriteText_UpdateOffset_Org{ nullptr };
	decltype(&MyCDWriteText_UpdateOffset)* g_CDWriteText_UpdateOffset_Org_Address{ nullptr };
	decltype(&MyCDWriteText_SetSize) g_CDWriteText_SetSize_Org{ nullptr };
	decltype(&MyCDWriteText_SetSize)* g_CDWriteText_SetSize_Org_Address{ nullptr };
	decltype(&MyCDWriteText_InitializeVisualTreeClone) g_CDWriteText_InitializeVisualTreeClone_Org{ nullptr };
	decltype(&MyCDWriteText_scalar_deleting_destructor) g_CDWriteText_scalar_deleting_destructor_Org{ nullptr };
	decltype(&MyCDWriteText_scalar_deleting_destructor)* g_CDWriteText_scalar_deleting_destructor_Org_Address{ nullptr };


	static union
	{
		uDWM::CText* g_textVisual;
		uDWM::CDWriteText* g_dwriteTextVisual;
	};
	uDWM::CTopLevelWindow* g_window{ nullptr };

	// coverage computed by RenderWin7CaptionText for the caption bitmap currently
	// being validated; bound to the bitmap's resource handle when the text visual's
	// CDrawImageInstruction is created and handed to the dwmcore-side realizer
	std::optional<CaptionTextRealizer::CoveragePayload> g_pendingCoverage{};

	bool g_isTrimmed{ false };
	// original sizes, no glow included
	static union
	{
		SIZE g_textSize;
		abi::Size g_textSizeF;
	};
	COLORREF g_captionActiveColor{};
	COLORREF g_captionInactiveColor{};
	COLORREF g_captionActiveColorMaximized{};
	COLORREF g_captionInactiveColorMaximized{};
	MARGINS g_contentMargins{}, g_sizingMargins{};
	struct CWindowState
	{
		bool active{};
		bool maximized{};
		LONG windowRectLeft{};
	};
	std::unordered_map<uDWM::CVisual*, CWindowState> g_textVisualStateMap{};
	winrt::com_ptr<ID2D1DCRenderTarget> g_textGlowRT{};
	winrt::com_ptr<ID2D1Bitmap1> g_textGlowD2DBitmap{};
	winrt::com_ptr<ID2D1Effect> g_textGlowEffect{};
	winrt::com_ptr<ID2D1Effect> g_textMorphologyEffect{};

	COLORREF g_textGlowColor{};

	int g_textGlowSize{};
	int g_textGlowIntensity{};
	int g_centerCaption{ 0 };
	// the display's ClearType extra-width (CGlyphRunMaker m_uExtraWidth base, Win7
	// hardcoded display settings default 1); PrecontrastLevel (+1 for thin fonts) is
	// added on top and the sum clamped to [0, 6]. Calibrated against Win7 captures.
	int g_captionTextContrast{ 1 };
	// which OS' caption text aliasing to reproduce: 0 = Windows 7 (6x glyph pipeline,
	// gamma index 9), 1 = Windows 8 (GDI ClearType remapped onto gamma index 2)
	int g_captionTextAliasing{ 0 };
	int CaptionCenterOffset(double visualWidth, double textWidth, double visualX, double parentWidth, double scale)
	{
		const int localOffset = std::max(
			static_cast<int>((visualWidth - textWidth) / 2.0),
			0
		);
		if (g_centerCaption <= 0 || g_isTrimmed)
			return 0;

		if (g_centerCaption == 1)
			return localOffset;

		// Win8 centers against the parent visual and uses a small scale-adjusted
		// guard band before falling back to the local visual width.
		int parentOffset = static_cast<int>((parentWidth - textWidth) / 2.0) - static_cast<int>(visualX);
		if (static_cast<double>(parentOffset + static_cast<int>(textWidth)) + scale * 5.0 > visualWidth)
			parentOffset = localOffset;

		return std::max(parentOffset, 0);
	}

	// builds the render font Win7's CGlyphGen::SetFont produces: measure at uniform 6x
	// scale, then keep the 1x height but adopt the 6x average character width so glyphs
	// rasterize at 6x horizontal resolution
	wil::unique_hfont CreateWin7ScaledFont(HDC referenceDC, HDC targetDC, LOGFONTW* captionFontInfo)
	{
		LOGFONTW captionFont{};
		if (!GetObjectW(GetCurrentObject(referenceDC, OBJ_FONT), sizeof(captionFont), &captionFont))
		{
			return {};
		}
		if (captionFontInfo)
		{
			*captionFontInfo = captionFont;
		}
		LOGFONTW measureFont{ captionFont };
		measureFont.lfHeight *= 6;
		measureFont.lfWidth *= 6;
		wil::unique_hfont scaledFont{ CreateFontIndirectW(&measureFont) };
		if (!scaledFont)
		{
			return {};
		}
		const HGDIOBJ previousFont{ SelectObject(targetDC, scaledFont.get()) };
		TEXTMETRICW textMetrics{};
		const bool measured{ GetTextMetricsW(targetDC, &textMetrics) != FALSE };
		SelectObject(targetDC, previousFont);
		if (!measured)
		{
			return {};
		}
		LOGFONTW renderFontInfo{ captionFont };
		renderFontInfo.lfWidth = textMetrics.tmAveCharWidth;
		return wil::unique_hfont{ CreateFontIndirectW(&renderFontInfo) };
	}

	// measures the text with the 6x render font and reports the extent in output pixels,
	// Win7-style ((right + 5) / 6, CGlyphBitmapHolder::MakeBitmap); 6x glyph advances
	// don't scale exactly 6x, so sizing the text bitmap from the 1x measurement would
	// spuriously ellipsize titles that actually fit
	LONG MeasureWin7ScaledTextWidth(HDC hdc, LPCWSTR lpchText, int cchText, UINT format)
	{
		wil::unique_hdc measureDC{ CreateCompatibleDC(nullptr) };
		if (!measureDC)
		{
			return 0;
		}
		const auto renderFont{ CreateWin7ScaledFont(hdc, measureDC.get(), nullptr) };
		if (!renderFont)
		{
			return 0;
		}
		const HGDIOBJ previousFont{ SelectObject(measureDC.get(), renderFont.get()) };
		RECT textRect{};
		g_DrawTextW_Org(measureDC.get(), lpchText, cchText, &textRect, (format & ~(DT_END_ELLIPSIS | DT_WORD_ELLIPSIS)) | DT_CALCRECT);
		SelectObject(measureDC.get(), previousFont);
		return (textRect.right + 5) / 6;
	}

	// Win7-accurate caption text rasterizer, replicating CGlyphGen/CGlyphBitmapHolder (6x
	// monochrome raster) plus dwmcore's CGlyphRunMaker coverage filter (ThickenBitmap +
	// FilterBox box downsample) and CGammaHandler gamma correction. The compositor only
	// honors a single alpha channel, so per-channel coverage is baked into the caption DIB
	// (exact against the glow already in the DIB, mean coverage as the alpha over glass).
	bool RenderWin7CaptionText(
		HDC hdc,
		LPCWSTR lpchText,
		int cchText,
		LPCRECT lprc,
		UINT format,
		COLORREF textColor,
		int* result
	)
	{
		BITMAP targetInfo{};
		if (
			!GetObjectW(GetCurrentObject(hdc, OBJ_BITMAP), sizeof(targetInfo), &targetInfo) ||
			!targetInfo.bmBits ||
			targetInfo.bmBitsPixel != 32
		)
		{
			return false;
		}
		const LONG textRectWidth{ wil::rect_width(*lprc) };
		const LONG textRectHeight{ wil::rect_height(*lprc) };
		if (textRectWidth <= 0 || textRectHeight <= 0)
		{
			return false;
		}

		wil::unique_hdc maskDC{ CreateCompatibleDC(nullptr) };
		if (!maskDC)
		{
			return false;
		}
		SetBkMode(maskDC.get(), TRANSPARENT);
		SetTextAlign(maskDC.get(), GetTextAlign(hdc));
		SetTextColor(maskDC.get(), RGB(255, 255, 255));

		LOGFONTW captionFont{};
		wil::unique_hfont renderFont{ CreateWin7ScaledFont(hdc, maskDC.get(), &captionFont) };
		if (!renderFont)
		{
			return false;
		}
		const HGDIOBJ previousFont{ SelectObject(maskDC.get(), renderFont.get()) };
		const auto fontCleanup{ wil::scope_exit([&] { SelectObject(maskDC.get(), previousFont); }) };

		// CGlyphBitmapHolder::MakeBitmap: 1bpp top-down DIB, width rounded up to 32 pixels
		const LONG scaledWidth{ textRectWidth * 6 };
		struct
		{
			BITMAPINFOHEADER header;
			RGBQUAD colors[2];
		} maskInfo{ { sizeof(BITMAPINFOHEADER), (scaledWidth + 31) & ~31, -textRectHeight, 1, 1, BI_RGB }, { {}, { 255, 255, 255 } } };
		void* maskBits{ nullptr };
		wil::unique_hbitmap maskBitmap{ CreateDIBSection(maskDC.get(), reinterpret_cast<BITMAPINFO*>(&maskInfo), DIB_RGB_COLORS, &maskBits, nullptr, 0) };
		if (!maskBitmap)
		{
			return false;
		}
		const HGDIOBJ previousBitmap{ SelectObject(maskDC.get(), maskBitmap.get()) };
		const auto bitmapCleanup{ wil::scope_exit([&] { SelectObject(maskDC.get(), previousBitmap); }) };

		// the caller measured the text with the 1x font, but 6x glyph advances don't scale
		// exactly 6x; re-measure at 6x and only keep the ellipsis flags when the text
		// genuinely doesn't fit, otherwise a few stray units would trim a fitting title
		RECT measureRect{ 0, 0, scaledWidth, textRectHeight };
		g_DrawTextW_Org(maskDC.get(), lpchText, cchText, &measureRect, (format & ~(DT_END_ELLIPSIS | DT_WORD_ELLIPSIS)) | DT_CALCRECT);
		UINT renderFormat{ format };
		if (measureRect.right <= scaledWidth)
		{
			renderFormat &= ~(DT_END_ELLIPSIS | DT_WORD_ELLIPSIS);
		}

		RECT scaledTextRect{ 0, 0, scaledWidth, textRectHeight };
		const int textHeight{ g_DrawTextW_Org(maskDC.get(), lpchText, cchText, &scaledTextRect, renderFormat) };
		if (!textHeight)
		{
			return false;
		}
		GdiFlush();

		// exact Win7 dwmcore glyph pipeline (CGlyphRunMaker::ThickenBitmap/FilterBox +
		// the ClearType pixel shader, recovered from the checked build). Horizontally
		// dilate the 6x mask rightward by clamp(extraWidth + PrecontrastLevel, 0, 6)
		// samples (ThickenBitmap), then box-downsample: each R/G/B subpixel is a
		// 6-sample popcount stepped 2 samples apart, coverage = trunc(popcount*255/6).
		// The shader's quadratic contrast curve (gamma index 9), with the text color's
		// luma folded into its coefficients, maps coverage per channel in sRGB space;
		// the per-channel over-blend below is then a plain lerp.
		const bool thinFont
		{
			_wcsicmp(captionFont.lfFaceName, L"Segoe UI") == 0 ||
			_wcsicmp(captionFont.lfFaceName, L"Meiryo") == 0
		};
		// PrecontrastLevel = 1 for the thin fonts Win7 special-cases
		const LONG dilation{ std::clamp<LONG>(g_captionTextContrast + (thinFont ? 1 : 0), 0, 6) };

		// FilterBox: 6-sample box per subpixel; on-screen registration calibrated
		// against Win7 captures puts the R window at [6x-2, 6x+4)
		constexpr LONG boxWidth{ 6 };

		// CD3DRenderState::SetConstantRegisters + pixel shader 2001 (sc_gammaRatios
		// entry 9): C1 = k1*luma + k2, C2 = k3*luma + k4, luma = (R + 2G + B)/4,
		// out = g + 4*g*(1-g)*(C1*g + C2) on sRGB values
		constexpr float k1{ 0.040675f }, k2{ -0.25425f }, k3{ 0.401275f }, k4{ -0.083675f };
		const float colorLuma{ (GetRValue(textColor) + 2.f * GetGValue(textColor) + GetBValue(textColor)) / (4.f * 255.f) };
		const float C1{ k1 * colorLuma + k2 };
		const float C2{ k3 * colorLuma + k4 };
		BYTE coverageTable[boxWidth + 1]{};
		for (int popcount = 0; popcount <= boxWidth; popcount++)
		{
			const float g{ static_cast<float>(popcount * 255 / boxWidth) / 255.f };
			coverageTable[popcount] = static_cast<BYTE>(std::clamp(g + 4.f * g * (1.f - g) * (C1 * g + C2), 0.f, 1.f) * 255.f + 0.5f);
		}

		// when the dwmcore-side realizer is operational, hand it the per-channel
		// coverage instead of baking the text into the single-alpha DIB; the
		// compositor then blends each channel against the live glass like Win7
		const bool payloadMode
		{
			g_CDrawImageInstruction_Create_Org != nullptr &&
			CaptionTextRealizer::IsAvailable()
		};
		CaptionTextRealizer::CoveragePayload payload{};
		if (payloadMode)
		{
			payload.bitmapWidth = targetInfo.bmWidth;
			payload.bitmapHeight = targetInfo.bmHeight;
			payload.x = lprc->left - 1;
			payload.y = lprc->top;
			payload.width = textRectWidth + 2;
			payload.height = textRectHeight;
			payload.textColor = textColor;
			payload.pixels.resize(static_cast<size_t>(payload.width) * payload.height * 4);
		}

		const LONG maskStride{ maskInfo.header.biWidth / 8 };
		const auto targetBits{ static_cast<BYTE*>(targetInfo.bmBits) };
		for (LONG y = 0; y < textRectHeight; y++)
		{
			const LONG targetY{ lprc->top + y };
			if (!payloadMode && (targetY < 0 || targetY >= targetInfo.bmHeight))
			{
				continue;
			}
			const BYTE* mask{ static_cast<const BYTE*>(maskBits) + y * maskStride };
			const auto sample = [&](LONG i) -> UINT
			{
				return (i < 0 || i >= scaledWidth) ? 0u : ((mask[i >> 3] >> (7 - (i & 7))) & 1u);
			};
			// ThickenBitmap: OR each sample with `dilation` left neighbors
			const auto dilatedSample = [&](LONG i) -> UINT
			{
				for (LONG k = 0; k <= dilation; k++)
				{
					if (sample(i - k))
					{
						return 1u;
					}
				}
				return 0u;
			};
			// FilterBox: popcount over the subpixel's box
			const auto boxPopcount = [&](LONG start) -> UINT
			{
				UINT total = 0;
				for (LONG k = 0; k < boxWidth; k++)
				{
					total += dilatedSample(start + k);
				}
				return total;
			};
			BYTE* targetRow{ !payloadMode ? targetBits + targetY * targetInfo.bmWidthBytes : nullptr };
			BYTE* payloadRow{ payloadMode ? payload.pixels.data() + static_cast<size_t>(y) * payload.width * 4 : nullptr };
			// the overlapping windows spill one pixel past the text rect on both sides
			// (Win7 pads the alpha box likewise)
			for (LONG x = -1; x <= textRectWidth; x++)
			{
				const LONG targetX{ lprc->left + x };
				if (!payloadMode && (targetX < 0 || targetX >= targetInfo.bmWidth))
				{
					continue;
				}
				// R/G/B subpixel coverage: 6-sample boxes stepped 2 samples apart
				const UINT alpha[3]
				{
					coverageTable[boxPopcount(x * 6 - 2)],
					coverageTable[boxPopcount(x * 6)],
					coverageTable[boxPopcount(x * 6 + 2)]
				};
				if (!alpha[0] && !alpha[1] && !alpha[2])
				{
					continue;
				}
				if (payloadMode)
				{
					// straight per-subpixel coverage for the realizer's blend; alpha is
					// the center (green) tap like the Win7 glyph shader outputs
					BYTE* pixel{ payloadRow + static_cast<size_t>(x + 1) * 4 };
					pixel[0] = static_cast<BYTE>(alpha[2]);
					pixel[1] = static_cast<BYTE>(alpha[1]);
					pixel[2] = static_cast<BYTE>(alpha[0]);
					pixel[3] = static_cast<BYTE>(alpha[1]);
					continue;
				}
				// per-channel premultiplied over-blend (BlendMode 2: src = textColor*cov,
				// dst *= 1 - cov, per channel); mean coverage is the single alpha the
				// compositor later composites over the glass
				BYTE* pixel{ targetRow + targetX * 4 };
				const UINT meanAlpha{ (alpha[0] + alpha[1] + alpha[2] + 1u) / 3u };
				pixel[0] = static_cast<BYTE>((GetBValue(textColor) * alpha[2] + pixel[0] * (255u - alpha[2]) + 127u) / 255u);
				pixel[1] = static_cast<BYTE>((GetGValue(textColor) * alpha[1] + pixel[1] * (255u - alpha[1]) + 127u) / 255u);
				pixel[2] = static_cast<BYTE>((GetRValue(textColor) * alpha[0] + pixel[2] * (255u - alpha[0]) + 127u) / 255u);
				pixel[3] = static_cast<BYTE>(meanAlpha + (pixel[3] * (255u - meanAlpha) + 127u) / 255u);
			}
		}
		if (payloadMode)
		{
			g_pendingCoverage.emplace(std::move(payload));
		}

		*result = textHeight;
		return true;
	}

	// dwmcore!CGammaHandler::sc_gammaRatios. The table is byte-for-byte identical from
	// the Windows 8 builds (7762/8.0/8.1) through 19041; entry n is gamma level
	// 1000 + n*100, and CGammaHandler::CalculateGammaTable folds the two halves of the
	// polynomial with slightly different scale factors (below).
	struct CGammaRatios
	{
		float g1, g2, g3, g4;
	};
	constexpr CGammaRatios g_gammaRatios[]
	{
		{  0.000000f,  0.000000f,  0.000000f,  0.000000f },
		{  0.004150f, -0.020175f,  0.055675f, -0.018775f },
		{  0.008750f, -0.044000f,  0.108125f, -0.034250f },
		{  0.013575f, -0.070525f,  0.157550f, -0.046900f },
		{  0.018475f, -0.099075f,  0.204175f, -0.057175f },
		{  0.023325f, -0.129025f,  0.248150f, -0.065400f },
		{  0.028025f, -0.159875f,  0.289700f, -0.071925f },
		{  0.032500f, -0.191225f,  0.328975f, -0.077000f },
		{  0.036725f, -0.222775f,  0.366100f, -0.080850f },
		{  0.040675f, -0.254250f,  0.401275f, -0.083675f },
		{  0.044325f, -0.285500f,  0.434625f, -0.085650f },
		{  0.047700f, -0.316300f,  0.466250f, -0.086900f },
		{  0.050775f, -0.346600f,  0.496275f, -0.087525f }
	};
	// CGammaHandler::CalculateGammaTable multiplies the g1/g3 half by 4.0314341 and the
	// g2/g4 half by 4.0156865 before folding them into the shader's C1/C2 constants
	constexpr float g_gammaScale_g1g3{ 4.0314341f };
	constexpr float g_gammaScale_g2g4{ 4.0156865f };

	// The subpixel coverage filter is a 6 sample box per channel, so coverage is always
	// one of 7 values - the popcount 0..6 put through the gamma curve. Windows 8's caption
	// text uses gamma index 2 (level 1200), where Windows 7's DWM used its hardcoded
	// CGammaHandler::HardCodedGammaLevel = 1900 (index 9). Index 2's curve is far flatter,
	// which leaves partial-coverage subpixels much more opaque and is exactly what gives
	// Windows 8 its heavier, purple-fringed caption text.
	constexpr int g_captionTextGammaIndex{ 2 };
	constexpr int g_coverageLevelCount{ 7 };

	// The 7 coverage levels dwmcore's ClearType shader produces for this text colour.
	//
	// CGammaHandler::CalculateGammaTable precomputes a 256 entry table of byte pairs
	//     f1 = g + 4.0156865*g*(1-g)*(g2*g + g4)
	//     f2 =     4.0314341*g*(1-g)*(g1*g + g3)
	// and the shader combines them with the text colour's luma as f1 + luma*f2 (that is
	// CD3DRenderState::SetConstantRegisters' C1/C2 folded back together).
	//
	// Both halves are quantized to bytes *separately* when the table is built, and the
	// store is the classic magic-number trick - `v + 1.5*2^22` then `(int)(bits << 10) >> 11`
	// - which works out to floor(round(2*v)/2), not round(v). Getting that exactly right
	// matters: it is the difference between coverage 116 and 117 at half coverage, and so
	// between matching Windows 8's caption text byte for byte and being one unit off.
	void CalculateCoverageLevels(COLORREF textColor, BYTE(&levels)[g_coverageLevelCount])
	{
		const auto& ratios{ g_gammaRatios[g_captionTextGammaIndex] };
		const float luma{ (GetRValue(textColor) + 2.f * GetGValue(textColor) + GetBValue(textColor)) / (4.f * 255.f) };
		// CalculateGammaTable's fixed-point store
		const auto quantize = [](float value)
		{
			return static_cast<float>(std::clamp(static_cast<int>(std::floor(std::round(value * 2.f) / 2.f)), 0, 255));
		};
		for (int popcount = 0; popcount < g_coverageLevelCount; popcount++)
		{
			const float g{ static_cast<float>(popcount * 255 / (g_coverageLevelCount - 1)) / 255.f };
			const float shoulder{ g * (1.f - g) };
			const float f1{ quantize((g + g_gammaScale_g2g4 * shoulder * (ratios.g2 * g + ratios.g4)) * 255.f) };
			const float f2{ quantize((g_gammaScale_g1g3 * shoulder * (ratios.g1 * g + ratios.g3)) * 255.f) };
			levels[popcount] = static_cast<BYTE>(std::clamp(f1 + luma * f2, 0.f, 255.f));
		}
	}

	// A 32bpp top-down DIB we can render GDI text into and read back.
	struct CTextSurface
	{
		wil::unique_hdc dc{};
		wil::unique_hbitmap bitmap{};
		HGDIOBJ previousBitmap{ nullptr };
		BYTE* bits{ nullptr };
		LONG width{}, height{};

		~CTextSurface()
		{
			if (dc && previousBitmap)
			{
				SelectObject(dc.get(), previousBitmap);
			}
		}
		bool Create(LONG cx, LONG cy)
		{
			dc.reset(CreateCompatibleDC(nullptr));
			if (!dc || cx <= 0 || cy <= 0)
			{
				return false;
			}
			BITMAPINFO info{ { sizeof(BITMAPINFOHEADER), cx, -cy, 1, 32, BI_RGB } };
			void* pixels{ nullptr };
			bitmap.reset(CreateDIBSection(dc.get(), &info, DIB_RGB_COLORS, &pixels, nullptr, 0));
			if (!bitmap)
			{
				return false;
			}
			previousBitmap = SelectObject(dc.get(), bitmap.get());
			bits = static_cast<BYTE*>(pixels);
			width = cx;
			height = cy;
			return true;
		}
		void Fill(BYTE value) const
		{
			memset(bits, value, static_cast<size_t>(width) * height * 4);
		}
	};

	// Renders the text into `surface` over a flat `background`, with GDI's own ClearType
	// rasterizer. lfQuality is forced to CLEARTYPE_QUALITY so we get subpixel coverage
	// even though uDWM's DC was set up for the grayscale DrawThemeTextEx path.
	bool DrawClearTypeText(
		const CTextSurface& surface,
		HFONT font,
		UINT textAlign,
		COLORREF textColor,
		BYTE background,
		LPCWSTR lpchText,
		int cchText,
		LPCRECT textRect,
		UINT format,
		int* textHeight
	)
	{
		surface.Fill(background);
		const HGDIOBJ previousFont{ SelectObject(surface.dc.get(), font) };
		const auto fontCleanup{ wil::scope_exit([&] { SelectObject(surface.dc.get(), previousFont); }) };
		SetBkMode(surface.dc.get(), TRANSPARENT);
		SetTextAlign(surface.dc.get(), textAlign);
		SetTextColor(surface.dc.get(), textColor);

		// the text rect keeps its original size (so DT_CENTER/DT_*_ELLIPSIS behave the
		// same) but is placed one pixel in, leaving room for the fringe to spill
		RECT rect{ 1, 0, 1 + wil::rect_width(*textRect), wil::rect_height(*textRect) };
		const int height{ g_DrawTextW_Org(surface.dc.get(), lpchText, cchText, &rect, format) };
		GdiFlush();
		if (textHeight)
		{
			*textHeight = height;
		}
		return height != 0;
	}

	// GDI applies its own gamma to the coverage, so its 7 levels don't line up with
	// dwmcore's. Both are monotone functions of the same box popcount, so measuring GDI's
	// levels once lets us map level-for-level onto dwmcore's. The measurement needs a
	// string that exercises every popcount; a stem-heavy pangram-ish sweep does.
	constexpr LPCWSTR g_coverageCalibrationText{ L"Hamburgefonstiv WMil1|0OQ 123456789" };
	struct CCoverageMap
	{
		// GDI coverage byte -> dwmcore coverage byte
		BYTE remap[256]{};
	};
	// GDI's gamma depends on the text colour, so the remap does too. Cache one per colour
	// rather than one overall: a window alternating between its active and inactive caption
	// colours would otherwise recalibrate on every focus change.
	LOGFONTW g_coverageFont{};
	std::unordered_map<COLORREF, CCoverageMap> g_coverageMaps{};
	const CCoverageMap* g_coverageMap{ nullptr };

	// solves per-channel coverage out of the two flat-background renders:
	//   overBlack = T*a          overWhite = T*a + 255*(1 - a)
	//   => a = 1 - (overWhite - overBlack)/255
	inline BYTE SolveCoverage(BYTE overBlack, BYTE overWhite)
	{
		const int transparency{ overWhite - overBlack };
		return static_cast<BYTE>(255 - std::clamp(transparency, 0, 255));
	}

	// builds the GDI -> dwmcore coverage remap for a font/colour pair, leaving it in
	// g_coverageMap
	bool EnsureCoverageMap(HFONT font, const LOGFONTW& logFont, COLORREF textColor)
	{
		if (memcmp(&g_coverageFont, &logFont, sizeof(logFont)))
		{
			g_coverageMaps.clear();
			g_coverageFont = logFont;
		}
		else if (const auto it = g_coverageMaps.find(textColor); it != g_coverageMaps.end())
		{
			g_coverageMap = &it->second;
			return true;
		}
		g_coverageMap = nullptr;

		const int cchCalibration{ static_cast<int>(wcslen(g_coverageCalibrationText)) };
		wil::unique_hdc measureDC{ CreateCompatibleDC(nullptr) };
		if (!measureDC)
		{
			return false;
		}
		const HGDIOBJ previousFont{ SelectObject(measureDC.get(), font) };
		RECT measureRect{};
		g_DrawTextW_Org(measureDC.get(), g_coverageCalibrationText, cchCalibration, &measureRect, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
		SelectObject(measureDC.get(), previousFont);

		CTextSurface overBlack{}, overWhite{};
		const LONG width{ measureRect.right + 4 };
		const LONG height{ std::max(measureRect.bottom, 1l) + 2 };
		if (!overBlack.Create(width, height) || !overWhite.Create(width, height))
		{
			return false;
		}
		const RECT calibrationRect{ 0, 0, measureRect.right, height };
		constexpr UINT calibrationFormat{ DT_SINGLELINE | DT_NOPREFIX | DT_TOP | DT_LEFT };
		if (
			!DrawClearTypeText(overBlack, font, TA_LEFT | TA_TOP, textColor, 0x00, g_coverageCalibrationText, cchCalibration, &calibrationRect, calibrationFormat, nullptr) ||
			!DrawClearTypeText(overWhite, font, TA_LEFT | TA_TOP, textColor, 0xFF, g_coverageCalibrationText, cchCalibration, &calibrationRect, calibrationFormat, nullptr)
		)
		{
			return false;
		}

		// collect the distinct coverage values GDI produced
		std::bitset<256> seen{};
		for (size_t i = 0; i < static_cast<size_t>(width) * height * 4; i++)
		{
			if ((i % 4) == 3)
			{
				continue;
			}
			seen.set(SolveCoverage(overBlack.bits[i], overWhite.bits[i]));
		}
		std::vector<BYTE> gdiLevels{};
		for (int value = 0; value < 256; value++)
		{
			if (seen.test(value))
			{
				gdiLevels.push_back(static_cast<BYTE>(value));
			}
		}
		auto& map = g_coverageMaps[textColor];
		g_coverageMap = &map;
		// anything other than exactly the 7 box levels means GDI isn't running the plain
		// ClearType filter (grayscale antialiasing, a reduced ClearTypeLevel, ...); in that
		// case leave the coverage alone rather than mangling it
		if (gdiLevels.size() != g_coverageLevelCount)
		{
			for (int value = 0; value < 256; value++)
			{
				map.remap[value] = static_cast<BYTE>(value);
			}
			return true;
		}

		BYTE dwmLevels[g_coverageLevelCount]{};
		CalculateCoverageLevels(textColor, dwmLevels);
		// map every possible byte to the dwmcore level whose GDI counterpart is nearest
		for (int value = 0; value < 256; value++)
		{
			size_t nearest{ 0 };
			int nearestDistance{ 256 };
			for (size_t level = 0; level < gdiLevels.size(); level++)
			{
				const int distance{ std::abs(value - static_cast<int>(gdiLevels[level])) };
				if (distance < nearestDistance)
				{
					nearestDistance = distance;
					nearest = level;
				}
			}
			map.remap[value] = dwmLevels[nearest];
		}
		return true;
	}

	// Win8-accurate caption text rasterizer.
	//
	// dwmcore's glyph pipeline gets its 6x oversampled glyph coverage from GDI and then
	// applies its own filter and gamma, so GDI's plain ClearType output already carries
	// exactly the subpixel structure Windows 8 shows - same glyph positions, same subpixel
	// phases, same box popcounts. Reimplementing CGlyphGen/ThickenBitmap/FilterBox on top
	// of a stretched font instead drifts out of phase, because dwmcore places each glyph
	// from its 1x advance rather than from the 6x font's own advances.
	//
	// So: let GDI rasterize, recover the per-channel coverage it used, and remap its 7
	// levels onto dwmcore's (see EnsureCoverageMap). Coverage is recovered by rendering
	// the same text over black and over white, which cancels the text colour out.
	//
	// The compositor only honors a single alpha channel, so per-channel coverage goes to
	// the dwmcore-side realizer; without it there is nowhere to put subpixel text, so we
	// decline and let DrawThemeTextEx render as before.
	bool RenderWin8CaptionText(
		HDC hdc,
		LPCWSTR lpchText,
		int cchText,
		LPCRECT lprc,
		UINT format,
		COLORREF textColor,
		int* result
	)
	{
		// A single-alpha bitmap cannot express per-channel coverage, so without the
		// dwmcore-side realizer there is nowhere to put subpixel text. Decline and let the
		// caller fall back to DrawThemeTextEx's grayscale rendering rather than bake
		// something that would blend wrongly against the frame.
		if (!g_CDrawImageInstruction_Create_Org || !CaptionTextRealizer::IsAvailable())
		{
			return false;
		}

		BITMAP targetInfo{};
		if (
			!GetObjectW(GetCurrentObject(hdc, OBJ_BITMAP), sizeof(targetInfo), &targetInfo) ||
			!targetInfo.bmBits ||
			targetInfo.bmBitsPixel != 32
		)
		{
			return false;
		}
		const LONG textRectWidth{ wil::rect_width(*lprc) };
		const LONG textRectHeight{ wil::rect_height(*lprc) };
		if (textRectWidth <= 0 || textRectHeight <= 0)
		{
			return false;
		}

		LOGFONTW captionFont{};
		if (!GetObjectW(GetCurrentObject(hdc, OBJ_FONT), sizeof(captionFont), &captionFont))
		{
			return false;
		}
		// uDWM sets the DC up for the grayscale DrawThemeTextEx path; ask for ClearType
		captionFont.lfQuality = CLEARTYPE_QUALITY;
		wil::unique_hfont renderFont{ CreateFontIndirectW(&captionFont) };
		if (!renderFont)
		{
			return false;
		}

		if (!EnsureCoverageMap(renderFont.get(), captionFont, textColor) || !g_coverageMap)
		{
			return false;
		}
		const auto& coverageRemap = g_coverageMap->remap;

		// one pixel of slop on each side for the fringe to spill into
		const LONG coverageWidth{ textRectWidth + 2 };
		CTextSurface overBlack{}, overWhite{};
		if (!overBlack.Create(coverageWidth, textRectHeight) || !overWhite.Create(coverageWidth, textRectHeight))
		{
			return false;
		}
		const UINT textAlign{ GetTextAlign(hdc) };
		int textHeight{ 0 };
		if (
			!DrawClearTypeText(overBlack, renderFont.get(), textAlign, textColor, 0x00, lpchText, cchText, lprc, format, &textHeight) ||
			!DrawClearTypeText(overWhite, renderFont.get(), textAlign, textColor, 0xFF, lpchText, cchText, lprc, format, nullptr)
		)
		{
			return false;
		}

		CaptionTextRealizer::CoveragePayload payload{};
		payload.bitmapWidth = targetInfo.bmWidth;
		payload.bitmapHeight = targetInfo.bmHeight;
		payload.x = lprc->left - 1;
		payload.y = lprc->top;
		payload.width = coverageWidth;
		payload.height = textRectHeight;
		payload.textColor = textColor;
		payload.pixels.resize(static_cast<size_t>(payload.width) * payload.height * 4);

		for (LONG y = 0; y < textRectHeight; y++)
		{
			const BYTE* blackRow{ overBlack.bits + static_cast<size_t>(y) * coverageWidth * 4 };
			const BYTE* whiteRow{ overWhite.bits + static_cast<size_t>(y) * coverageWidth * 4 };
			BYTE* payloadRow{ payload.pixels.data() + static_cast<size_t>(y) * payload.width * 4 };
			for (LONG x = 0; x < coverageWidth; x++)
			{
				// B, G, R coverage in memory order, remapped onto dwmcore's gamma
				const BYTE alpha[3]
				{
					coverageRemap[SolveCoverage(blackRow[x * 4 + 0], whiteRow[x * 4 + 0])],
					coverageRemap[SolveCoverage(blackRow[x * 4 + 1], whiteRow[x * 4 + 1])],
					coverageRemap[SolveCoverage(blackRow[x * 4 + 2], whiteRow[x * 4 + 2])]
				};
				if (!alpha[0] && !alpha[1] && !alpha[2])
				{
					continue;
				}
				// straight per-subpixel coverage for the realizer's blend; alpha is
				// the center (green) tap like the ClearType glyph shader outputs
				BYTE* pixel{ payloadRow + static_cast<size_t>(x) * 4 };
				pixel[0] = alpha[0];
				pixel[1] = alpha[1];
				pixel[2] = alpha[2];
				pixel[3] = alpha[1];
			}
		}
		g_pendingCoverage.emplace(std::move(payload));

		*result = textHeight;
		return true;
	}

	// dispatches to the rasterizer the CaptionTextAliasing setting selects
	bool RenderCaptionText(
		HDC hdc,
		LPCWSTR lpchText,
		int cchText,
		LPCRECT lprc,
		UINT format,
		COLORREF textColor,
		int* result
	)
	{
		return g_captionTextAliasing == 1
			? RenderWin8CaptionText(hdc, lpchText, cchText, lprc, format, textColor, result)
			: RenderWin7CaptionText(hdc, lpchText, cchText, lprc, format, textColor, result);
	}

	void CalculateRealizedTextGlowParams(int textGlowMode);
}

int WINAPI CaptionTextHandler::MyDrawTextW(
	HDC hdc,
	LPCWSTR lpchText,
	int cchText,
	LPRECT lprc,
	UINT format
)
{
	int result{ 0 };
	auto drawTextCallback = [](HDC hdc, LPWSTR pszText, int cchText, LPRECT prc, UINT dwFlags, LPARAM lParam) static -> int
	{
		return *reinterpret_cast<int*>(lParam) = g_DrawTextW_Org(hdc, pszText, cchText, prc, dwFlags);
	};

	if ((format & DT_CALCRECT))
	{
		const LONG availableWidth{ wil::rect_width(*lprc) };
		result = g_DrawTextW_Org(hdc, lpchText, cchText, lprc, format);
		LONG naturalWidth{ wil::rect_width(*lprc) };

		// CText caches this CALCRECT result as the natural caption extent and uses it
		// to decide whether a later SetSize must recreate the bitmap. Always publish
		// the same 6x extent used by RenderWin7CaptionText, even while the title is
		// currently clipped; otherwise rerasterization stops at the narrower 1x GDI
		// extent and the last part of the title stays ellipsized. The Win8 rasterizer
		// draws with the ordinary 1x font, so its extent is the one GDI just returned.
		if (g_captionTextAliasing != 1)
		{
			if (const LONG scaledExtent{ MeasureWin7ScaledTextWidth(hdc, lpchText, cchText, format & ~DT_CALCRECT) };
				scaledExtent > 0)
			{
				naturalWidth = scaledExtent;
				lprc->right = lprc->left + scaledExtent;
			}
		}
		g_isTrimmed = naturalWidth > availableWidth;

		return result;
	}
	// clear the background, so the text can be shown transparent
	// with this hack, we don't need to hook FillRect any more
	BITMAP bmp{};
	if (GetObjectW(GetCurrentObject(hdc, OBJ_BITMAP), sizeof(bmp), &bmp) && bmp.bmBits)
	{
		memset(bmp.bmBits, 0, 4 * bmp.bmWidth * bmp.bmHeight);
	}

	OffsetRect(lprc, g_textGlowSize, g_textGlowSize);

	const auto& windowState = g_textVisualStateMap[g_dwriteTextVisual];
	const auto textColor = GetTextColor(hdc);
	const auto textColorOverride = windowState.active ? (windowState.maximized ? g_captionActiveColorMaximized : g_captionActiveColor) : (windowState.maximized ? g_captionInactiveColorMaximized : g_captionInactiveColor);
	DTTOPTS options
	{
		sizeof(DTTOPTS),
		DTT_TEXTCOLOR | DTT_COMPOSITED | DTT_CALLBACK | DTT_GLOWSIZE,
		textColorOverride != 0xFFFFFFFF ? textColorOverride : textColor,
		0,
		0,
		0,
		{},
		0,
		0,
		0,
		0,
		FALSE,
		0,
		drawTextCallback,
		(LPARAM)&result
	};

	auto glowDrawRect = *lprc;

	if (Shared::g_textGlowMode == 1 || Shared::g_textGlowMode == 2)
	{
		glowDrawRect.left -= g_contentMargins.cxLeftWidth;
		glowDrawRect.top -= g_contentMargins.cyTopHeight;
		glowDrawRect.right += g_contentMargins.cxRightWidth;
		glowDrawRect.bottom += g_contentMargins.cyBottomHeight;
	}
	else if (LOWORD(Shared::g_textGlowMode) == 3)
	{
		options.iGlowSize = g_textGlowSize;
		glowDrawRect.left -= g_textGlowSize;
		glowDrawRect.top -= g_textGlowSize;
		glowDrawRect.right += g_textGlowSize;
		glowDrawRect.bottom += g_textGlowSize;
	}

	const auto calcGlowClipRect = [&windowState](LPCRECT lprc, RECT& glowClipRect, bool mirrored)
	{
		LONG offset = 0;
		offset += windowState.windowRectLeft;
		offset -= g_textVisual->GetX();
		// Keep the glow clip rect aligned with the same centering offset as the text.
		offset -= CaptionCenterOffset(
			static_cast<DOUBLE>(g_textVisual->GetWidth()),
			static_cast<DOUBLE>(g_textSize.cx),
			static_cast<DOUBLE>(g_textVisual->GetX()),
			static_cast<DOUBLE>(g_textVisual->GetTransformParent()->GetWidth()),
			g_textVisual->GetScale().width
		);
		if (!mirrored)
		{
			glowClipRect.left = std::max(
				glowClipRect.left,
				lprc->left +
				offset
			);
		}
		else
		{
			glowClipRect.right = std::min(
				glowClipRect.right,
				lprc->right -
				offset
			);
		}
	};

	auto glowClipRect = glowDrawRect;
	calcGlowClipRect(lprc, glowClipRect, g_textVisual->IsRTLMirrored());

	SaveDC(hdc);
	const auto dcPaintScope = wil::scope_exit([hdc]
	{
		RestoreDC(hdc, -1);
	});
	IntersectClipRect(hdc, glowClipRect.left, glowClipRect.top, glowClipRect.right, glowClipRect.bottom);

	if (Shared::g_textGlowMode == 1 || Shared::g_textGlowMode == 2)
	{
		if (!g_textGlowRT)
		{
			winrt::com_ptr<ID2D1Factory> factory{};
			uDWM::CDesktopManager::GetInstance()->GetD2DDevice()->GetFactory(factory.put());
			D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
				D2D1_RENDER_TARGET_TYPE_SOFTWARE,
				D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
			);
			THROW_IF_FAILED(
				factory->CreateDCRenderTarget(
					&properties,
					g_textGlowRT.put()
				)
			);
		}
		winrt::com_ptr<ID2D1DeviceContext> context{};
		THROW_IF_FAILED(g_textGlowRT->QueryInterface(context.put()));
		if (!g_textGlowD2DBitmap)
		{
			THROW_IF_FAILED(
				context->CreateBitmap(
					D2D1::SizeU(
						Shared::g_textGlowBitmapInfo.bmiHeader.biWidth,
						-Shared::g_textGlowBitmapInfo.bmiHeader.biHeight
					),
					Shared::g_textGlowBitmapPixels,
					Shared::g_textGlowBitmapInfo.bmiHeader.biWidth * 4,
					D2D1::BitmapProperties1(
						D2D1_BITMAP_OPTIONS_NONE,
						D2D1::PixelFormat(
							DXGI_FORMAT_B8G8R8A8_UNORM,
							D2D1_ALPHA_MODE_PREMULTIPLIED
						)
					),
					g_textGlowD2DBitmap.put()
				)
			);
		}

		RECT targetRect
		{
			lprc->left - g_textGlowSize,
			lprc->top - g_textGlowSize,
			lprc->right + g_textGlowSize,
			lprc->bottom + g_textGlowSize
		};
		g_textGlowRT->BindDC(hdc, &targetRect);
		g_textGlowRT->BeginDraw();
		Util::DrawNineGridBitmap(
			context.get(),
			g_textGlowD2DBitmap.get(),
			D2D1::RectF(
				static_cast<float>(glowDrawRect.left),
				static_cast<float>(glowDrawRect.top),
				static_cast<float>(glowDrawRect.right),
				static_cast<float>(glowDrawRect.bottom)
			),
			g_sizingMargins,
			Shared::g_textGlowMode == 2 ? (windowState.active ? (windowState.maximized ? Shared::g_glowOpacityMaximized : Shared::g_glowOpacity) : (windowState.maximized ? Shared::g_glowOpacityInactiveMaximized : Shared::g_glowOpacityInactive)) : 1.f
		);
		LOG_IF_FAILED(g_textGlowRT->EndDraw());
		/*{
			FrameRect(
				hdc,
				&glowClipRect,
				GetStockBrush(WHITE_BRUSH)
			);
		}*/
	}

	// glow mode 3 generates its glow inside DrawThemeTextEx, so keep that call and
	// overdraw the accurate text on top of it; every other mode renders the text
	// directly (with DrawThemeTextEx as fallback should the rasterizer fail)
	const bool themeGeneratedGlow{ LOWORD(Shared::g_textGlowMode) == 3 && g_textGlowSize != 0 };
	bool captionTextRendered{ false };
	if (!themeGeneratedGlow)
	{
		captionTextRendered = RenderCaptionText(hdc, lpchText, cchText, lprc, format, options.crText, &result);
	}
	if (!captionTextRendered)
	{
		wil::unique_htheme hTheme{ OpenThemeData(nullptr, L"CompositedWindow::Window") };
		if (hTheme)
		{
			THROW_IF_FAILED(
				DrawThemeTextEx(
					hTheme.get(),
					hdc,
					0,
					0,
					lpchText,
					cchText,
					format,
					lprc,
					&options
				)
			);
			if (themeGeneratedGlow)
			{
				RenderCaptionText(hdc, lpchText, cchText, lprc, format, options.crText, &result);
			}
		}
		else
		{
			THROW_HR_IF_NULL(E_FAIL, hTheme);
			result = g_DrawTextW_Org(hdc, lpchText, cchText, lprc, format);
		}
	}

	// override that so we can use the correct param in CDrawImageInstruction::Create
	lprc->left -= g_textGlowSize;
	lprc->top -= g_textGlowSize;
	lprc->right += g_textGlowSize;
	lprc->bottom += g_textGlowSize;

	return result;
}
HBITMAP WINAPI CaptionTextHandler::MyCreateBitmap(
	int nWidth,
	int nHeight,
	[[maybe_unused]] UINT nPlanes,
	[[maybe_unused]] UINT nBitCount,
	[[maybe_unused]] const void* lpBits
)
{
	if (!g_textVisual)
	{
		return g_CreateBitmap_Org(nWidth, nHeight, nPlanes, nBitCount, lpBits);
	}

	g_textSize = { nWidth, nHeight };
	nWidth += g_textGlowSize * 2;
	nHeight += g_textGlowSize * 2;

	PVOID bits{ nullptr };
	BITMAPINFO bitmapInfo{ {sizeof(bitmapInfo.bmiHeader), nWidth, -nHeight, 1, 32, BI_RGB} };
	HBITMAP bitmap{ CreateDIBSection(nullptr, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0) };
	if (bits)
	{
		memset(bits, 0, static_cast<size_t>(nWidth) * nHeight * 4);
	}

	return bitmap;
}
HRESULT CaptionTextHandler::MyIWICImagingFactory2_CreateBitmapFromHBITMAP(
	IWICImagingFactory2* This,
	HBITMAP hBitmap,
	HPALETTE hPalette,
	[[maybe_unused]] WICBitmapAlphaChannelOption options,
	IWICBitmap** ppIBitmap
)
{
	return g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org(
		This,
		hBitmap,
		hPalette,
		WICBitmapAlphaChannelOption::WICBitmapUsePremultipliedAlpha,
		ppIBitmap
	);
}

HRESULT CaptionTextHandler::MyCText_ValidateResources(uDWM::CText* This)
{
	if (g_centerCaption)
	{
		// update alignment transform
		This->SetDirtyFlags(0x8000);
		// redraw the text to get the width and height
		This->SetDirtyFlags(0x1000);
	}
	if (!g_CText_scalar_deleting_destructor_Org)
	{
		g_CText_scalar_deleting_destructor_Org_Address = HookHelper::get_vftable_from<decltype(g_CText_scalar_deleting_destructor_Org)>(This);
		HookHelper::PatchPointerT(
			g_CText_scalar_deleting_destructor_Org_Address,
			MyCText_scalar_deleting_destructor,
			&g_CText_scalar_deleting_destructor_Org
		);
	}
	if (g_window = uDWM::TryGetWindowFromVisual(This); g_window && g_window->GetData())
	{
		auto& windowState = g_textVisualStateMap[This];

		windowState.active = g_window->TreatAsActiveWindow();
		windowState.maximized = g_window->TreatAsMaximized();

		RECT windowRect{};
		g_window->GetActualWindowRect(&windowRect, true, false, true);
		windowState.windowRectLeft = windowRect.left;
	}
	g_textVisual = This;
	const auto hr = g_CText_ValidateResources_Org(This);
	g_textVisual = nullptr;
	g_window = nullptr;

	return hr;
}
HRESULT CaptionTextHandler::MyCDrawImageInstruction_Create(uDWM::CBitmapSource* bitmapSource, LPCRECT lprc, PVOID* instruction)
{
	const auto hr = g_CDrawImageInstruction_Create_Org(bitmapSource, lprc, instruction);
	// the instruction created inside CText::ValidateResources right after our
	// rasterization pass is the caption text bitmap's; bind the pending coverage
	// to its resource handle for the dwmcore-side realizer
	if (g_pendingCoverage)
	{
		if (SUCCEEDED(hr) && g_textVisual && bitmapSource)
		{
			if (const UINT handle{ bitmapSource->GetResourceHandle() })
			{
				CaptionTextRealizer::RegisterPayload(g_textVisual, handle, std::move(*g_pendingCoverage));
			}
		}
		g_pendingCoverage.reset();
	}
	return hr;
}
HRESULT CaptionTextHandler::MyCText_InitializeVisualTreeClone(uDWM::CText* This, uDWM::CText* clonedVisual, UINT cloneOption)
{
	g_textVisualStateMap[clonedVisual] = g_textVisualStateMap[This];
	return g_CText_InitializeVisualTreeClone_Org(This, clonedVisual, cloneOption);
}
HRESULT CaptionTextHandler::MyCText_CloneVisualTree(uDWM::CText* This, uDWM::CText** clonedVisual, bool unknown1, bool unknown2, bool unknown3)
{
	const auto result = g_CText_CloneVisualTree_Org(This, clonedVisual, unknown1, unknown2, unknown3);
	if (clonedVisual && *clonedVisual)
	{
		g_textVisualStateMap[*clonedVisual] = g_textVisualStateMap[This];
	}
	return result;
}
HRESULT CaptionTextHandler::MyCText_scalar_deleting_destructor(uDWM::CText* This, BYTE flag)
{
	g_textVisualStateMap.erase(This);
	CaptionTextRealizer::UnregisterOwner(This);
	return g_CText_scalar_deleting_destructor_Org(This, flag);
}
HRESULT CaptionTextHandler::MyCChannel_MatrixTransformUpdate(dwmcore::CChannel* This, UINT handleId, MilMatrix3x2D* matrix)
{
	if (g_textVisual)
	{
		matrix->DX -= static_cast<DOUBLE>(g_textGlowSize);
		matrix->DY -= static_cast<DOUBLE>(g_textGlowSize);

		// Apply the same CenterCaption offset to the legacy text transform.
		const DOUBLE offset = static_cast<DOUBLE>(CaptionCenterOffset(
			static_cast<DOUBLE>(g_textVisual->GetWidth()),
			static_cast<DOUBLE>(g_textSize.cx),
			static_cast<DOUBLE>(g_textVisual->GetX()),
			static_cast<DOUBLE>(g_textVisual->GetTransformParent()->GetWidth()),
			g_textVisual->GetScale().width
		));
		if (offset > 0.0)
		{
			matrix->DX += g_textVisual->IsRTLMirrored() ? -offset : offset;
		}
	}

	return g_CChannel_MatrixTransformUpdate_Org(This, handleId, matrix);
}

void CaptionTextHandler::MyID2D1DeviceContext_DrawTextLayout(
	ID2D1DeviceContext* This,
	D2D1_POINT_2F origin,
	IDWriteTextLayout* textLayout,
	ID2D1Brush* defaultFillBrush,
	D2D1_DRAW_TEXT_OPTIONS options
)
{
	if (!g_dwriteTextVisual)
	{
		return g_ID2D1DeviceContext_DrawTextLayout_Org(
			This,
			origin,
			textLayout,
			defaultFillBrush,
			options
		);
	}

	winrt::com_ptr<ID2D1SolidColorBrush> solidColorBrush{};
	if (FAILED(defaultFillBrush->QueryInterface(solidColorBrush.put())))
	{
		return g_ID2D1DeviceContext_DrawTextLayout_Org(
			This,
			origin,
			textLayout,
			defaultFillBrush,
			options
		);
	}
	const auto color = solidColorBrush->GetColor();
	const auto cleanup = wil::scope_exit([&]
	{
		solidColorBrush->SetColor(color);
	});

	const auto& windowState = g_textVisualStateMap[g_dwriteTextVisual];
	const auto textColorOverride = windowState.active ? (windowState.maximized ? g_captionActiveColorMaximized : g_captionActiveColor) : (windowState.maximized ? g_captionInactiveColorMaximized : g_captionInactiveColor);
	if (textColorOverride != 0xFFFFFFFF)
	{
		solidColorBrush->SetColor(Color::FromAbgr(textColorOverride));
	}

	DWRITE_LINE_METRICS lineMetrics{};
	UINT32 lineCount{};
	THROW_IF_FAILED(
		textLayout->GetLineMetrics(
			&lineMetrics,
			1,
			&lineCount
		)
	);
	g_isTrimmed = lineMetrics.isTrimmed;

	if (!g_textGlowSize)
	{
		return g_ID2D1DeviceContext_DrawTextLayout_Org(
			This,
			origin,
			textLayout,
			defaultFillBrush,
			options
		);
	}

	origin.x += g_textGlowSize;
	origin.y += g_textGlowSize;

	DWRITE_TEXT_METRICS metrics{};
	THROW_IF_FAILED(
		textLayout->GetMetrics(
			&metrics
		)
	);

	if (!metrics.width || !metrics.height)
	{
		return g_ID2D1DeviceContext_DrawTextLayout_Org(
			This,
			origin,
			textLayout,
			defaultFillBrush,
			options
		);
	}

	if (LOWORD(Shared::g_textGlowMode) == 3 && g_textGlowIntensity)
	{
		winrt::com_ptr<ID2D1BitmapRenderTarget> bitmapRT{};
		THROW_IF_FAILED(
			This->CreateCompatibleRenderTarget(
				D2D1::SizeF(
					std::ceil(metrics.left + metrics.width) + static_cast<float>(g_textGlowSize * 2),
					std::ceil(metrics.top + metrics.height) + static_cast<float>(g_textGlowSize * 2)
				),
				bitmapRT.put()
			)
		);

		bitmapRT->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
		bitmapRT->BeginDraw();
		bitmapRT->Clear();
		bitmapRT->DrawTextLayout(
			D2D1::Point2F(),
			textLayout,
			defaultFillBrush,
			options
		);
		THROW_IF_FAILED(bitmapRT->EndDraw());

		winrt::com_ptr<ID2D1Bitmap> bitmap{};
		THROW_IF_FAILED(bitmapRT->GetBitmap(bitmap.put()));

		if (!g_textMorphologyEffect)
		{
			THROW_IF_FAILED(
				This->CreateEffect(
					CLSID_D2D1Morphology,
					g_textMorphologyEffect.put()
				)
			);
			THROW_IF_FAILED(
				g_textMorphologyEffect->SetValue(
					D2D1_MORPHOLOGY_PROP_MODE,
					D2D1_MORPHOLOGY_MODE_DILATE
				)
			);
			THROW_IF_FAILED(
				g_textMorphologyEffect->SetValue(
					D2D1_MORPHOLOGY_PROP_WIDTH,
					3 + g_textGlowSize / 12
				)
			);
			THROW_IF_FAILED(
				g_textMorphologyEffect->SetValue(
					D2D1_MORPHOLOGY_PROP_HEIGHT,
					3 + g_textGlowSize / 12
				)
			);
		}
		if (!g_textGlowEffect)
		{
			THROW_IF_FAILED(
				This->CreateEffect(
					CLSID_D2D1Shadow,
					g_textGlowEffect.put()
				)
			);
			THROW_IF_FAILED(
				g_textGlowEffect->SetValue(
					D2D1_SHADOW_PROP_OPTIMIZATION,
					D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED
				)
			);
			g_textGlowEffect->SetInputEffect(0, g_textMorphologyEffect.get());
		}
		THROW_IF_FAILED(
			g_textGlowEffect->SetValue(
				D2D1_SHADOW_PROP_COLOR,
				Color::FromAbgr(g_textGlowColor | (std::min(g_textGlowIntensity, 255) << 24), false)
			)
		);
		THROW_IF_FAILED(
			g_textGlowEffect->SetValue(
				D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION,
				std::max(
					0.f,
					(static_cast<float>(g_textGlowSize)) / 3.f + 0.5f
				)
			)
		);
		g_textMorphologyEffect->SetInput(0, bitmap.get());

		This->DrawImage(
			g_textGlowEffect.get(),
			&origin,
			nullptr,
			D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
			D2D1_COMPOSITE_MODE_SOURCE_COPY
		);
		This->DrawImage(
			bitmap.get(),
			&origin,
			nullptr,
			D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR
		);
		return;
	}
	if (Shared::g_textGlowMode == 1 || Shared::g_textGlowMode == 2)
	{
		if (!g_textGlowD2DBitmap)
		{
			THROW_IF_FAILED(
				This->CreateBitmap(
					D2D1::SizeU(
						Shared::g_textGlowBitmapInfo.bmiHeader.biWidth,
						-Shared::g_textGlowBitmapInfo.bmiHeader.biHeight
					),
					Shared::g_textGlowBitmapPixels,
					Shared::g_textGlowBitmapInfo.bmiHeader.biWidth * 4,
					D2D1::BitmapProperties1(
						D2D1_BITMAP_OPTIONS_NONE,
						D2D1::PixelFormat(
							DXGI_FORMAT_B8G8R8A8_UNORM,
							D2D1_ALPHA_MODE_PREMULTIPLIED
						)
					),
					g_textGlowD2DBitmap.put()
				)
			);
		}

		DWRITE_OVERHANG_METRICS overhangs{};
		THROW_IF_FAILED(
			textLayout->GetOverhangMetrics(
				&overhangs
			)
		);
		const D2D1_RECT_F textBoundingBox
		{
			origin.x + (std::floor(-overhangs.left) - 1.f),
			origin.y + std::floor(metrics.top) - 1.f,
			origin.x + (std::floor(-overhangs.left) - 1.f) + g_textSizeF.Width,
			origin.y + std::floor(metrics.top + metrics.height) + 1.f
		};
		D2D1_RECT_F glowRect
		{
			textBoundingBox.left - static_cast<float>(g_contentMargins.cxLeftWidth),
			textBoundingBox.top - static_cast<float>(g_contentMargins.cyTopHeight),
			textBoundingBox.right + static_cast<float>(g_contentMargins.cxRightWidth),
			textBoundingBox.bottom + static_cast<float>(g_contentMargins.cyBottomHeight)
		};

		const auto calcGlowClipRect = [&windowState](const D2D1_RECT_F& textRect, D2D1_RECT_F& glowClipRect, bool mirrored)
		{
			LONG offset = 0;
			offset += windowState.windowRectLeft;
			offset -= g_dwriteTextVisual->GetX();
			// DWrite glow clipping needs the same offset calculation as the text surface.
			offset -= CaptionCenterOffset(
				static_cast<double>(g_dwriteTextVisual->GetWidth()),
				static_cast<double>(g_textSizeF.Width),
				static_cast<double>(g_dwriteTextVisual->GetX()),
				static_cast<double>(g_dwriteTextVisual->GetTransformParent()->GetWidth()),
				g_dwriteTextVisual->GetScale().width
			);
			if (!mirrored)
			{
				glowClipRect.left = std::max(
					glowClipRect.left,
					textRect.left +
					offset
				);
			}
			else
			{
				glowClipRect.right = std::min(
					glowClipRect.right,
					textRect.right -
					offset
				);
			}
		};
		calcGlowClipRect(textBoundingBox, glowRect, g_dwriteTextVisual->IsRTLMirrored());

		THROW_IF_FAILED(
			Util::DrawNineGridBitmap(
				This,
				g_textGlowD2DBitmap.get(),
				glowRect,
				g_sizingMargins,
				Shared::g_textGlowMode == 2 ? (windowState.active ? (windowState.maximized ? Shared::g_glowOpacityMaximized : Shared::g_glowOpacity) : (windowState.maximized ? Shared::g_glowOpacityInactiveMaximized : Shared::g_glowOpacityInactive)) : 1.f
			)
		);
		/*{
			winrt::com_ptr<ID2D1SolidColorBrush> brush{};
			THROW_IF_FAILED(This->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), brush.put()));
			This->DrawRectangle(
				glowRect,
				brush.get(),
				1.f,
				nullptr
			);
		}*/
	}

	return g_ID2D1DeviceContext_DrawTextLayout_Org(
		This,
		origin,
		textLayout,
		defaultFillBrush,
		options
	);
}

HRESULT CaptionTextHandler::MyICompositionGraphicsDevice_CreateDrawingSurface(
	abi::ICompositionGraphicsDevice* This,
	abi::Size sizePixels,
	abi::DirectXPixelFormat pixelFormat,
	abi::DirectXAlphaMode alphaMode,
	abi::ICompositionDrawingSurface** result
)
{
	if (g_dwriteTextVisual)
	{
		g_textSizeF = sizePixels;
		sizePixels.Width += g_textGlowSize * 2;
		sizePixels.Height += g_textGlowSize * 2;
	}
	return g_ICompositionGraphicsDevice_CreateDrawingSurface_Org(
		This,
		sizePixels,
		pixelFormat,
		alphaMode,
		result
	);
}

HRESULT CaptionTextHandler::MyICompositionSurfaceBrush2_put_Offset(
	abi::ICompositionSurfaceBrush2* This,
	abi::Vector2 value
)
{
	if (g_dwriteTextVisual)
	{
		value.Y -= g_textGlowSize;
		value.X -= g_textGlowSize;

		// offset, glowSize
		// 40, 17
		// 11, 17
		// CDWriteVisual is rtl mirrored, but CSpriteVisual is not rtl mirrored
		if (auto& offset = const_cast<POINT&>(g_dwriteTextVisual->GetOffset()); g_dwriteTextVisual->IsRTLMirrored())
		{
			if (offset.x > g_textGlowSize)
			{
				value.X += g_textGlowSize;
			}
			else
			{
				value.X += g_textGlowSize + g_textGlowSize - offset.x;
			}
		}
		else
		{
			value.X += offset.x - std::max(offset.x - g_textGlowSize, 0l);
		}

		// Apply the same CenterCaption offset to the DWrite composition surface.
		const float offset = static_cast<float>(CaptionCenterOffset(
			static_cast<double>(g_dwriteTextVisual->GetWidth()),
			static_cast<double>(g_textSizeF.Width),
			static_cast<double>(g_dwriteTextVisual->GetX()),
			static_cast<double>(g_dwriteTextVisual->GetTransformParent()->GetWidth()),
			g_dwriteTextVisual->GetScale().width
		));
		if (offset > 0.0f)
		{
			value.X += g_dwriteTextVisual->IsRTLMirrored() ? -offset : offset;
		}
	}
	return g_ICompositionSurfaceBrush2_put_Offset_Org(
		This,
		value
	);
}

HRESULT CaptionTextHandler::MyCDWriteText_ValidateVisual(uDWM::CDWriteText* This)
{
	// 0x2 redraw text
	// 0x8 offset changed
	// 0x10 rtl mirrored changed
	if ((This->GetDirtyFlags() & (0x8 | 0x10)))
	{
		This->SetDirtyFlags(0x2);
	}
	if ((This->GetDirtyFlags() & 0x2))
	{
		This->SetDirtyFlags(0x8);
	}
	if (!g_CDWriteText_scalar_deleting_destructor_Org)
	{
		g_CDWriteText_scalar_deleting_destructor_Org_Address = HookHelper::get_vftable_from<decltype(g_CDWriteText_scalar_deleting_destructor_Org)>(This);
		HookHelper::PatchPointerT(
			g_CDWriteText_scalar_deleting_destructor_Org_Address,
			MyCDWriteText_scalar_deleting_destructor,
			&g_CDWriteText_scalar_deleting_destructor_Org
		);
	}
	if (!g_CDWriteText_UpdateOffset_Org)
	{
		PVOID CVisual_UpdateOffset_Org{ nullptr };
		PVOID CSpriteVisual_SetSize_Org{ nullptr };
		uDWM::g_projectionArray.ApplyToVariable("CVisual::UpdateOffset", CVisual_UpdateOffset_Org);
		uDWM::g_projectionArray.ApplyToVariable("CSpriteVisual::SetSize", CSpriteVisual_SetSize_Org);

		for (auto& vf : std::span{ HookHelper::get_vftable_from(This), 32})
		{
			if (vf == CVisual_UpdateOffset_Org)
			{
				g_CDWriteText_UpdateOffset_Org_Address = reinterpret_cast<decltype(g_CDWriteText_UpdateOffset_Org_Address)>(&vf);
				HookHelper::PatchPointerT(
					g_CDWriteText_UpdateOffset_Org_Address,
					MyCDWriteText_UpdateOffset,
					&g_CDWriteText_UpdateOffset_Org
				);
			}
			if (vf == CSpriteVisual_SetSize_Org)
			{
				g_CDWriteText_SetSize_Org_Address = reinterpret_cast<decltype(g_CDWriteText_SetSize_Org_Address)>(&vf);
				HookHelper::PatchPointerT(
					g_CDWriteText_SetSize_Org_Address,
					MyCDWriteText_SetSize,
					&g_CDWriteText_SetSize_Org
				);
			}
		}
	}
	if (g_window = uDWM::TryGetWindowFromVisual(This); g_window && g_window->GetData())
	{
		auto& windowState = g_textVisualStateMap[This];

		windowState.active = g_window->TreatAsActiveWindow();
		windowState.maximized = g_window->TreatAsMaximized();

		RECT windowRect{};
		g_window->GetActualWindowRect(&windowRect, true, false, true);
		windowState.windowRectLeft = windowRect.left;
	}
	g_dwriteTextVisual = This;
	const auto hr = g_CDWriteText_ValidateVisual_Org(This);
	g_dwriteTextVisual = nullptr;
	g_window = nullptr;

	return hr;
}

HRESULT CaptionTextHandler::MyCDWriteText_UpdateOffset(uDWM::CDWriteText* This)
{
	if (!g_textGlowSize)
	{
		return g_CDWriteText_UpdateOffset_Org(This);
	}

	// SpriteVisual will crop what exceeds its bounding rectangle,
	// here we make it offset x minus the size of the glow,
	// and add it back later in the ICompositionSurfaceBrush2::put_Offset method
	//
	// This gives us enough space to render the glow.
	auto& offset = const_cast<POINT&>(This->GetOffset());
	const auto actualOffsetX = offset.x;
	if (!This->IsRTLMirrored())
	{
		offset.x = std::max(offset.x - g_textGlowSize, 0l);
	}
	const auto hr = g_CDWriteText_UpdateOffset_Org(This);
	offset.x = actualOffsetX;

	return hr;
}

HRESULT CaptionTextHandler::MyCDWriteText_SetSize(uDWM::CDWriteText* This, const SIZE* size)
{
	if (!g_textGlowSize)
	{
		return g_CDWriteText_SetSize_Org(This, size);
	}

	const auto hr = g_CDWriteText_SetSize_Org(This, size);
	// SpriteVisual will crop what exceeds its bounding rectangle,
	// expand it to ensure enough space to render the glow.
	auto& offset = const_cast<POINT&>(This->GetOffset());
	if (This->IsRTLMirrored())
	{
		This->GetVisualProxy()->SetSize(
			static_cast<double>(size->cx + offset.x - std::max(offset.x - g_textGlowSize, 0l)),
			static_cast<double>(size->cy)
		);
	}
	else
	{
		This->GetVisualProxy()->SetSize(
			static_cast<double>(size->cx + offset.x - std::max(offset.x - g_textGlowSize, 0l) + g_textGlowSize),
			static_cast<double>(size->cy)
		);
	}

	return hr;
}

HRESULT CaptionTextHandler::MyCDWriteText_InitializeVisualTreeClone(uDWM::CDWriteText* This, uDWM::CDWriteText* clonedVisual, UINT cloneOption)
{
	g_textVisualStateMap[clonedVisual] = g_textVisualStateMap[This];
	return g_CDWriteText_InitializeVisualTreeClone_Org(This, clonedVisual, cloneOption);
}

HRESULT CaptionTextHandler::MyCDWriteText_scalar_deleting_destructor(uDWM::CDWriteText* This, BYTE flag)
{
	g_textVisualStateMap.erase(This);
	return g_CDWriteText_scalar_deleting_destructor_Org(This, flag);
}

void CaptionTextHandler::CalculateRealizedTextGlowParams(int textGlowMode)
{
	if (textGlowMode == 0)
	{
		g_textGlowSize = 0;
	}
	else if (textGlowMode == 1 || textGlowMode == 2)
	{
		const auto themeHandle = CustomThemeAtlasLoader::GetThemeHandle();

		CustomThemeAtlasLoader::MyGetThemeMargins(
			themeHandle,
			nullptr,
			static_cast<int>(DWM_WINDOW_THEME_PART::TEXTGLOW),
			0,
			TMT_SIZINGMARGINS,
			nullptr,
			&g_sizingMargins
		);
		CustomThemeAtlasLoader::MyGetThemeMargins(
			themeHandle,
			nullptr,
			static_cast<int>(DWM_WINDOW_THEME_PART::TEXTGLOW),
			0,
			TMT_CONTENTMARGINS,
			nullptr,
			&g_contentMargins
		);
		g_textGlowSize = std::max(
			{
				g_contentMargins.cxLeftWidth,
				g_contentMargins.cxRightWidth,
				g_contentMargins.cyTopHeight,
				g_contentMargins.cyBottomHeight
			}
		);
	}
	else
	{
		wil::unique_htheme themeHandle{ OpenThemeData(nullptr, L"CompositedWindow::Window") };

		if (g_textGlowSize = HIWORD(textGlowMode); !g_textGlowSize)
		{
			CustomThemeAtlasLoader::MyGetThemeInt(themeHandle.get(), static_cast<int>(DWM_WINDOW_THEME_PART::COMMON), 0, TMT_TEXTGLOWSIZE, &g_textGlowSize);
		}
		CustomThemeAtlasLoader::MyGetThemeInt(themeHandle.get(), static_cast<int>(DWM_WINDOW_THEME_PART::COMMON), 0, TMT_GLOWINTENSITY, &g_textGlowIntensity);
		GetThemeColor(themeHandle.get(), static_cast<int>(DWM_WINDOW_THEME_PART::COMMON), 0, TMT_GLOWCOLOR, &g_textGlowColor);

		// debug
		//g_textGlowIntensity = 305;
		//g_textGlowColor = 0xFFFFFF;
	}
}

void CaptionTextHandler::DestroyDeviceResources()
{
	g_textGlowRT = nullptr;
	g_textGlowD2DBitmap = nullptr;

	if (uDWM::g_versionInfo.build < os::build_w11_22h2)
	{
		return;
	}
	g_textGlowEffect = nullptr;
	g_textMorphologyEffect = nullptr;
}

void CaptionTextHandler::Update(GlassEngine::UpdateType type)
{
	if (type & GlassEngine::UpdateType::Theme)
	{
		g_textGlowD2DBitmap = nullptr;
		CalculateRealizedTextGlowParams(Shared::g_textGlowMode);
	}
	if (type & GlassEngine::UpdateType::Backdrop || type & GlassEngine::UpdateType::Theme)
	{
		g_centerCaption = std::clamp(static_cast<int>(GlassEngine::GetDwordFromRegistry(L"CenterCaption", FALSE)), 0, 2);
		g_captionTextAliasing = std::clamp(static_cast<int>(GlassEngine::GetDwordFromRegistry(L"CaptionTextAliasing", FALSE)), 0, 1);
		g_captionActiveColor = GlassEngine::GetDwordFromRegistry(L"ColorizationColorCaption", 0xFFFFFFFD);
		g_captionInactiveColor = GlassEngine::GetDwordFromRegistry(L"ColorizationColorCaptionInactive", g_captionActiveColor);
		g_captionActiveColorMaximized = GlassEngine::GetDwordFromRegistry(L"ColorizationColorCaptionMaximized", g_captionActiveColor);
		g_captionInactiveColorMaximized = GlassEngine::GetDwordFromRegistry(L"ColorizationColorCaptionInactiveMaximized", g_captionInactiveColor);

		const auto themeHandle = CustomThemeAtlasLoader::GetThemeHandle();
		if (themeHandle)
		{
			if (g_captionActiveColor == 0xFFFFFFFE)
			{
				CustomThemeAtlasLoader::MyGetThemeColor(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 1, TMT_TEXTCOLOR, &g_captionActiveColor);
			}
			else if (g_captionActiveColor == 0xFFFFFFFD)
			{
				g_captionActiveColor = RGB(0, 0, 0);
			}
			if (g_captionInactiveColor == 0xFFFFFFFE)
			{
				CustomThemeAtlasLoader::MyGetThemeColor(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 2, TMT_TEXTCOLOR, &g_captionInactiveColor);
			}
			else if (g_captionInactiveColor == 0xFFFFFFFD)
			{
				g_captionInactiveColor = RGB(0, 0, 0);
			}
			if (g_captionActiveColorMaximized == 0xFFFFFFFE)
			{
				CustomThemeAtlasLoader::MyGetThemeColor(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 3, TMT_TEXTCOLOR, &g_captionActiveColorMaximized);
			}
			else if (g_captionActiveColorMaximized == 0xFFFFFFFD)
			{
				if (Shared::g_type == Shared::GlassType::Aero)
				{
					g_captionActiveColorMaximized = RGB(0, 0, 0);
				}
				else
				{
					g_captionActiveColorMaximized = RGB(255, 255, 255);
				}
			}
			if (g_captionInactiveColorMaximized == 0xFFFFFFFE)
			{
				CustomThemeAtlasLoader::MyGetThemeColor(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 4, TMT_TEXTCOLOR, &g_captionInactiveColorMaximized);
			}
			else if (g_captionInactiveColorMaximized == 0xFFFFFFFD)
			{
				if (Shared::g_type == Shared::GlassType::Aero)
				{
					g_captionInactiveColorMaximized = RGB(0, 0, 0);
				}
				else
				{
					g_captionInactiveColorMaximized = RGB(255, 255, 255);
				}
			}

			int value;

			value = 100;
			CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 1, TMT_OPACITY, &value);
			Shared::g_glowOpacity = value / 100.f;

			value = 100;
			CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 2, TMT_OPACITY, &value);
			Shared::g_glowOpacityInactive = value / 100.f;

			value = 100;
			CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 3, TMT_OPACITY, &value);
			Shared::g_glowOpacityMaximized = value / 100.f;

			value = 100;
			CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 4, TMT_OPACITY, &value);
			Shared::g_glowOpacityInactiveMaximized = value / 100.f;
		}
	}
}

void CaptionTextHandler::Startup()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_CaptionTextHandler))
	{
		return;
	}

	if (uDWM::g_versionInfo.build < os::build_w11_22h2)
	{
		wil::unique_hmodule wincodecsMoudle{ LoadLibraryExW(L"WindowsCodecs.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR) };
		THROW_LAST_ERROR_IF_NULL(wincodecsMoudle);
		const auto WICCreateImagingFactory_Proxy_fn = reinterpret_cast<HRESULT(WINAPI*)(UINT, IWICImagingFactory2**)>(
			GetProcAddress(wincodecsMoudle.get(), "WICCreateImagingFactory_Proxy")
		);
		THROW_LAST_ERROR_IF_NULL(WICCreateImagingFactory_Proxy_fn);
		winrt::com_ptr<IWICImagingFactory2> wicFactory{ nullptr };
		THROW_IF_FAILED(WICCreateImagingFactory_Proxy_fn(WINCODEC_SDK_VERSION2, wicFactory.put()));
		g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address = reinterpret_cast<decltype(g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address)>(&HookHelper::get_vftable_from(wicFactory.get())[21]);
		HookHelper::PatchPointerT(
			g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address,
			MyIWICImagingFactory2_CreateBitmapFromHBITMAP,
			&g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org
		);
		HookHelper::PatchIAT(
			uDWM::g_moduleHandle,
			std::initializer_list<HookHelper::ImportDllDetourInfo>
			{
				{
					"user32.dll",
					std::initializer_list<HookHelper::ImportFunctionDetourInfo>
					{
						{ "DrawTextW", & g_DrawTextW_Org, &MyDrawTextW }
					}
				},
				{
					"gdi32.dll",
					std::initializer_list<HookHelper::ImportFunctionDetourInfo>
					{
						{ "CreateBitmap", &g_CreateBitmap_Org, &MyCreateBitmap }
					}
				}
			}
		);

		dwmcore::g_projectionArray.ApplyToVariable("CChannel::MatrixTransformUpdate", g_CChannel_MatrixTransformUpdate_Org);
		uDWM::g_projectionArray.ApplyToVariable("CText::ValidateResources", g_CText_ValidateResources_Org);
		uDWM::g_projectionArray.ApplyToVariable("CText::InitializeVisualTreeClone", g_CText_InitializeVisualTreeClone_Org);
		uDWM::g_projectionArray.ApplyToVariable("CText::CloneVisualTree", g_CText_CloneVisualTree_Org);
		uDWM::g_projectionArray.ApplyToVariable("CDrawImageInstruction::Create", g_CDrawImageInstruction_Create_Org);

		const auto build_before_w10_2004 = dwmcore::g_versionInfo.build < os::build_w10_2004;
		HookHelper::PatchFunctions(
			std::initializer_list<HookHelper::DetourInfo>
			{
				{ &g_CChannel_MatrixTransformUpdate_Org, &MyCChannel_MatrixTransformUpdate },
				{ &g_CText_ValidateResources_Org, &MyCText_ValidateResources },
				{ &g_CText_InitializeVisualTreeClone_Org, &MyCText_InitializeVisualTreeClone, !build_before_w10_2004 },
				{ &g_CText_CloneVisualTree_Org, &MyCText_CloneVisualTree, build_before_w10_2004 },
				{ &g_CDrawImageInstruction_Create_Org, &MyCDrawImageInstruction_Create, g_CDrawImageInstruction_Create_Org != nullptr }
			},
			true
		);
	}
	else
	{
		winrt::com_ptr<ID2D1DeviceContext> context{};
		THROW_IF_FAILED(
			uDWM::CDesktopManager::GetInstance()->GetD2DDevice()->CreateDeviceContext(
				D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
				context.put()
			)
		);

		g_ID2D1DeviceContext_DrawTextLayout_Org_Address = &HookHelper::get_vftable_from<decltype(g_ID2D1DeviceContext_DrawTextLayout_Org)>(context.get())[28];
		HookHelper::PatchPointerT(
			g_ID2D1DeviceContext_DrawTextLayout_Org_Address,
			MyID2D1DeviceContext_DrawTextLayout,
			&g_ID2D1DeviceContext_DrawTextLayout_Org
		);

		winrt::com_ptr<IDCompositionDesktopDevicePartner> dcompDevicePartner{ nullptr };
		THROW_IF_FAILED(uDWM::CDesktopManager::GetInstance()->GetInteropCompositorDCompDevicePartner()->QueryInterface(dcompDevicePartner.put()));
		winrt::com_ptr<abi::ICompositionGraphicsDevice> graphicsDevice{ nullptr };

		winrt::com_ptr<abi::ICompositor> compositor{};
		THROW_IF_FAILED(dcompDevicePartner->QueryInterface(compositor.put()));

		winrt::com_ptr<abi::ICompositorInterop> compositorInterop{};
		THROW_IF_FAILED(compositor->QueryInterface(compositorInterop.put()));
		THROW_IF_FAILED(
			compositorInterop->CreateGraphicsDevice(
				uDWM::CDesktopManager::GetInstance()->GetD2DDevice(),
				graphicsDevice.put()
			)
		);

		g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address = &HookHelper::get_vftable_from<decltype(g_ICompositionGraphicsDevice_CreateDrawingSurface_Org)>(graphicsDevice.get())[6];
		HookHelper::PatchPointerT(
			g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address,
			MyICompositionGraphicsDevice_CreateDrawingSurface,
			&g_ICompositionGraphicsDevice_CreateDrawingSurface_Org
		);

		winrt::com_ptr<abi::ICompositionSurfaceBrush> surfaceBrush{ nullptr };
		THROW_IF_FAILED(compositor->CreateSurfaceBrush(surfaceBrush.put()));

		winrt::com_ptr<abi::ICompositionSurfaceBrush2> surfaceBrush2{ nullptr };
		THROW_IF_FAILED(surfaceBrush->QueryInterface(surfaceBrush2.put()));
		g_ICompositionSurfaceBrush2_put_Offset_Org_Address = &HookHelper::get_vftable_from<decltype(g_ICompositionSurfaceBrush2_put_Offset_Org)>(surfaceBrush2.get())[11];
		HookHelper::PatchPointerT(
			g_ICompositionSurfaceBrush2_put_Offset_Org_Address,
			MyICompositionSurfaceBrush2_put_Offset,
			&g_ICompositionSurfaceBrush2_put_Offset_Org
		);

		uDWM::g_projectionArray.ApplyToVariable("CDWriteText::ValidateVisual", g_CDWriteText_ValidateVisual_Org);
		uDWM::g_projectionArray.ApplyToVariable("CDWriteText::InitializeVisualTreeClone", g_CDWriteText_InitializeVisualTreeClone_Org);

		HookHelper::PatchFunctions(
			std::initializer_list<HookHelper::DetourInfo>
			{
				{ &g_CDWriteText_ValidateVisual_Org, &MyCDWriteText_ValidateVisual },
				{ &g_CDWriteText_InitializeVisualTreeClone_Org, &MyCDWriteText_InitializeVisualTreeClone }
			},
			true
		);
	}
}
void CaptionTextHandler::Shutdown()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_CaptionTextHandler))
	{
		return;
	}

	if (uDWM::g_versionInfo.build < os::build_w11_22h2)
	{
		if (g_CText_scalar_deleting_destructor_Org)
		{
			HookHelper::PatchPointerT(
				g_CText_scalar_deleting_destructor_Org_Address,
				g_CText_scalar_deleting_destructor_Org
			);
		}

		const auto build_before_w10_2004 = dwmcore::g_versionInfo.build < os::build_w10_2004;
		HookHelper::PatchFunctions(
			std::initializer_list<HookHelper::DetourInfo>
			{
				{ &g_CChannel_MatrixTransformUpdate_Org, &MyCChannel_MatrixTransformUpdate },
				{ &g_CText_ValidateResources_Org, &MyCText_ValidateResources },
				{ &g_CText_InitializeVisualTreeClone_Org, &MyCText_InitializeVisualTreeClone, !build_before_w10_2004 },
				{ &g_CText_CloneVisualTree_Org, &MyCText_CloneVisualTree, build_before_w10_2004 },
				{ &g_CDrawImageInstruction_Create_Org, &MyCDrawImageInstruction_Create, g_CDrawImageInstruction_Create_Org != nullptr }
			},
			false
		);

		SwitchToThread();

		HookHelper::PatchPointerT(
			g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address,
			g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org
		);
		HookHelper::PatchIAT(
			uDWM::g_moduleHandle,
			std::initializer_list<HookHelper::ImportDllDetourInfo>
			{
				{
					"user32.dll",
					std::initializer_list<HookHelper::ImportFunctionDetourInfo>
					{
						{ "DrawTextW", & g_DrawTextW_Org, g_DrawTextW_Org }
					}
				},
				{
					"gdi32.dll",
					std::initializer_list<HookHelper::ImportFunctionDetourInfo>
					{
						{ "CreateBitmap", &g_CreateBitmap_Org, g_CreateBitmap_Org }
					}
				}
			}
		);

		g_textVisual = nullptr;
	}
	else
	{
		if (g_CDWriteText_scalar_deleting_destructor_Org)
		{
			HookHelper::PatchPointerT(
				g_CDWriteText_scalar_deleting_destructor_Org_Address,
				g_CDWriteText_scalar_deleting_destructor_Org
			);
		}
		if (g_CDWriteText_UpdateOffset_Org)
		{
			HookHelper::PatchPointerT(
				g_CDWriteText_UpdateOffset_Org_Address,
				g_CDWriteText_UpdateOffset_Org
			);
		}
		if (g_CDWriteText_SetSize_Org)
		{
			HookHelper::PatchPointerT(
				g_CDWriteText_SetSize_Org_Address,
				g_CDWriteText_SetSize_Org
			);
		}

		HookHelper::PatchFunctions(
			std::initializer_list<HookHelper::DetourInfo>
			{
				{ &g_CDWriteText_ValidateVisual_Org, &MyCDWriteText_ValidateVisual },
				{ &g_CDWriteText_InitializeVisualTreeClone_Org, &MyCDWriteText_InitializeVisualTreeClone }
			},
			false
		);

		SwitchToThread();

		HookHelper::PatchPointerT(
			g_ID2D1DeviceContext_DrawTextLayout_Org_Address,
			g_ID2D1DeviceContext_DrawTextLayout_Org
		);
		HookHelper::PatchPointerT(
			g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address,
			g_ICompositionGraphicsDevice_CreateDrawingSurface_Org
		);
		HookHelper::PatchPointerT(
			g_ICompositionSurfaceBrush2_put_Offset_Org_Address,
			g_ICompositionSurfaceBrush2_put_Offset_Org
		);

		g_dwriteTextVisual = nullptr;
	}

	DestroyDeviceResources();
	g_textSize = {};
	g_textVisualStateMap.clear();
}
