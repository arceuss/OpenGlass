// Windows 11 no longer routes caption text through uDWM!CText and the legacy
// dwmcore glyph handoff.  Keep the replacement caption-scoped by rasterizing only
// while CDWriteText::ValidateVisual is active, then draw the resulting bitmap into
// CDWriteText's own Direct2D composition surface.

struct CAliasedCaptionBitmap
{
	UINT width{};
	UINT height{};
	LONG originX{-1};
	LONG originY{};
	std::vector<BYTE> pixels{};
};

struct CAliasedTextSurface
{
	wil::unique_hdc dc{};
	wil::unique_hbitmap bitmap{};
	HGDIOBJ previousBitmap{};
	BYTE* bits{};
	LONG width{};
	LONG height{};

	~CAliasedTextSurface()
	{
		if (dc && previousBitmap)
		{
			SelectObject(dc.get(), previousBitmap);
		}
	}

	bool Create(LONG cx, LONG cy)
	{
		if (cx <= 0 || cy <= 0)
		{
			return false;
		}

		dc.reset(CreateCompatibleDC(nullptr));
		if (!dc)
		{
			return false;
		}

		BITMAPINFO bitmapInfo
		{
			{
				sizeof(BITMAPINFOHEADER),
				cx,
				-cy,
				1,
				32,
				BI_RGB
			}
		};
		void* pixels{};
		bitmap.reset(CreateDIBSection(dc.get(), &bitmapInfo, DIB_RGB_COLORS, &pixels, nullptr, 0));
		if (!bitmap || !pixels)
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

std::wstring ReadCurrentCaptionText()
{
	if (!g_window || !g_window->GetData())
	{
		return {};
	}

	const HWND hwnd = g_window->GetData()->GetHwnd();
	if (!hwnd)
	{
		return {};
	}

	const int length = GetWindowTextLengthW(hwnd);
	if (length <= 0)
	{
		return {};
	}

	std::wstring text(static_cast<size_t>(length) + 1, L'\0');
	const int copied = GetWindowTextW(hwnd, text.data(), static_cast<int>(text.size()));
	if (copied <= 0)
	{
		return {};
	}
	text.resize(static_cast<size_t>(copied));
	return text;
}

UINT GetCaptionDrawTextFlags(IDWriteTextLayout* textLayout)
{
	UINT flags = DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS;
	const auto textInterface = g_dwriteTextVisual ? g_dwriteTextVisual->GetTextInterface() : nullptr;
	const bool rtl =
		textLayout->GetReadingDirection() == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT ||
		(textInterface && textInterface->IsRTLReading());
	if (rtl)
	{
		flags |= DT_RTLREADING;
	}

	switch (textLayout->GetTextAlignment())
	{
	case DWRITE_TEXT_ALIGNMENT_CENTER:
		flags |= DT_CENTER;
		break;
	case DWRITE_TEXT_ALIGNMENT_TRAILING:
		flags |= rtl ? DT_LEFT : DT_RIGHT;
		break;
	case DWRITE_TEXT_ALIGNMENT_LEADING:
	case DWRITE_TEXT_ALIGNMENT_JUSTIFIED:
	default:
		flags |= rtl ? DT_RIGHT : DT_LEFT;
		break;
	}

	switch (textLayout->GetParagraphAlignment())
	{
	case DWRITE_PARAGRAPH_ALIGNMENT_CENTER:
		flags |= DT_VCENTER;
		break;
	case DWRITE_PARAGRAPH_ALIGNMENT_FAR:
		flags |= DT_BOTTOM;
		break;
	case DWRITE_PARAGRAPH_ALIGNMENT_NEAR:
	default:
		flags |= DT_TOP;
		break;
	}

	return flags;
}

bool GetCaptionLogFont(IDWriteTextLayout* textLayout, LOGFONTW& logFont)
{
	const UINT32 familyNameLength = textLayout->GetFontFamilyNameLength();
	if (!familyNameLength || familyNameLength >= LF_FACESIZE)
	{
		return false;
	}

	WCHAR familyName[LF_FACESIZE]{};
	if (FAILED(textLayout->GetFontFamilyName(familyName, ARRAYSIZE(familyName))))
	{
		return false;
	}

	logFont = {};
	logFont.lfHeight = -std::max(1l, std::lround(textLayout->GetFontSize()));
	logFont.lfWeight = static_cast<LONG>(textLayout->GetFontWeight());
	logFont.lfItalic = textLayout->GetFontStyle() == DWRITE_FONT_STYLE_NORMAL ? FALSE : TRUE;
	logFont.lfCharSet = DEFAULT_CHARSET;
	logFont.lfOutPrecision = OUT_TT_PRECIS;
	logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	logFont.lfQuality = CLEARTYPE_QUALITY;
	logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	wcscpy_s(logFont.lfFaceName, familyName);

	// Preserve the exact face/weight/stretch mapping that DirectWrite selected.
	// The layout's size is already in CDWriteText's pixel coordinate space, so keep
	// the height calculated above instead of applying monitor DPI a second time.
	winrt::com_ptr<IDWriteFactory> factory{};
	if (SUCCEEDED(
		DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(factory.put())
		)
	))
	{
		winrt::com_ptr<IDWriteGdiInterop> gdiInterop{};
		winrt::com_ptr<IDWriteFontCollection> collection{};
		winrt::com_ptr<IDWriteFontFamily> family{};
		winrt::com_ptr<IDWriteFont> font{};
		UINT32 familyIndex{};
		BOOL familyExists{};
		if (
			SUCCEEDED(factory->GetGdiInterop(gdiInterop.put())) &&
			SUCCEEDED(textLayout->GetFontCollection(collection.put())) &&
			collection &&
			SUCCEEDED(collection->FindFamilyName(familyName, &familyIndex, &familyExists)) &&
			familyExists &&
			SUCCEEDED(collection->GetFontFamily(familyIndex, family.put())) &&
			family &&
			SUCCEEDED(
				family->GetFirstMatchingFont(
					textLayout->GetFontWeight(),
					textLayout->GetFontStretch(),
					textLayout->GetFontStyle(),
					font.put()
				)
			) &&
			font
		)
		{
			LOGFONTW converted{};
			BOOL isSystemFont{};
			if (SUCCEEDED(gdiInterop->ConvertFontToLOGFONT(font.get(), &converted, &isSystemFont)))
			{
				const LONG pixelHeight = logFont.lfHeight;
				logFont = converted;
				logFont.lfHeight = pixelHeight;
				logFont.lfWidth = 0;
				logFont.lfQuality = CLEARTYPE_QUALITY;
			}
		}
	}
	return true;
}

wil::unique_hfont CreateWin7CaptionFont(HDC targetDC, const LOGFONTW& captionFont)
{
	LOGFONTW measureFont = captionFont;
	measureFont.lfHeight *= 6;
	measureFont.lfWidth *= 6;
	wil::unique_hfont scaledFont{ CreateFontIndirectW(&measureFont) };
	if (!scaledFont)
	{
		return {};
	}

	const HGDIOBJ previousFont = SelectObject(targetDC, scaledFont.get());
	TEXTMETRICW textMetrics{};
	const bool measured = GetTextMetricsW(targetDC, &textMetrics) != FALSE;
	SelectObject(targetDC, previousFont);
	if (!measured)
	{
		return {};
	}

	LOGFONTW renderFont = captionFont;
	renderFont.lfWidth = textMetrics.tmAveCharWidth;
	return wil::unique_hfont{ CreateFontIndirectW(&renderFont) };
}

void StoreCaptionCoveragePixel(
	BYTE* pixel,
	COLORREF textColor,
	UINT redCoverage,
	UINT greenCoverage,
	UINT blueCoverage,
	BYTE brushAlpha
)
{
	redCoverage = (redCoverage * brushAlpha + 127u) / 255u;
	greenCoverage = (greenCoverage * brushAlpha + 127u) / 255u;
	blueCoverage = (blueCoverage * brushAlpha + 127u) / 255u;
	const UINT meanCoverage = (redCoverage + greenCoverage + blueCoverage + 1u) / 3u;

	// This is the same single-alpha fallback used by the Windows 10 implementation
	// when its final-target component-alpha realizer is unavailable.  The per-channel
	// colour coverage stays intact; DComp necessarily attenuates the live backdrop by
	// the mean because a transparent BGRA surface has only one alpha channel.
	pixel[0] = static_cast<BYTE>((GetBValue(textColor) * blueCoverage + 127u) / 255u);
	pixel[1] = static_cast<BYTE>((GetGValue(textColor) * greenCoverage + 127u) / 255u);
	pixel[2] = static_cast<BYTE>((GetRValue(textColor) * redCoverage + 127u) / 255u);
	pixel[3] = static_cast<BYTE>(meanCoverage);
}

bool RenderWin7CaptionBitmap(
	std::wstring_view text,
	IDWriteTextLayout* textLayout,
	const LOGFONTW& captionFont,
	LONG textWidth,
	LONG textHeight,
	COLORREF textColor,
	BYTE brushAlpha,
	CAliasedCaptionBitmap& output
)
{
	if (textWidth <= 0 || textHeight <= 0)
	{
		return false;
	}

	wil::unique_hdc maskDC{ CreateCompatibleDC(nullptr) };
	if (!maskDC)
	{
		return false;
	}
	SetBkMode(maskDC.get(), TRANSPARENT);
	SetTextAlign(maskDC.get(), TA_LEFT | TA_TOP);
	SetTextColor(maskDC.get(), RGB(255, 255, 255));

	const auto renderFont = CreateWin7CaptionFont(maskDC.get(), captionFont);
	if (!renderFont)
	{
		return false;
	}
	const HGDIOBJ previousFont = SelectObject(maskDC.get(), renderFont.get());
	const auto fontCleanup = wil::scope_exit([&] { SelectObject(maskDC.get(), previousFont); });

	const LONG scaledWidth = textWidth * 6;
	struct CMaskBitmapInfo
	{
		BITMAPINFOHEADER header;
		RGBQUAD colors[2];
	};
	CMaskBitmapInfo maskInfo
	{
		{
			sizeof(BITMAPINFOHEADER),
			(scaledWidth + 31) & ~31,
			-textHeight,
			1,
			1,
			BI_RGB
		},
		{ {}, { 255, 255, 255 } }
	};
	void* maskBits{};
	wil::unique_hbitmap maskBitmap
	{
		CreateDIBSection(
			maskDC.get(),
			reinterpret_cast<BITMAPINFO*>(&maskInfo),
			DIB_RGB_COLORS,
			&maskBits,
			nullptr,
			0
		)
	};
	if (!maskBitmap || !maskBits)
	{
		return false;
	}
	memset(maskBits, 0, static_cast<size_t>(maskInfo.header.biWidth / 8) * textHeight);
	const HGDIOBJ previousBitmap = SelectObject(maskDC.get(), maskBitmap.get());
	const auto bitmapCleanup = wil::scope_exit([&] { SelectObject(maskDC.get(), previousBitmap); });

	UINT format = GetCaptionDrawTextFlags(textLayout);
	RECT measureRect{ 0, 0, scaledWidth, textHeight };
	DrawTextW(
		maskDC.get(),
		text.data(),
		static_cast<int>(text.size()),
		&measureRect,
		(format & ~(DT_END_ELLIPSIS | DT_WORD_ELLIPSIS)) | DT_CALCRECT
	);
	if (measureRect.right <= scaledWidth)
	{
		format &= ~(DT_END_ELLIPSIS | DT_WORD_ELLIPSIS);
	}

	RECT textRect{ 0, 0, scaledWidth, textHeight };
	if (!DrawTextW(maskDC.get(), text.data(), static_cast<int>(text.size()), &textRect, format))
	{
		return false;
	}
	GdiFlush();

	const bool thinFont =
		_wcsicmp(captionFont.lfFaceName, L"Segoe UI") == 0 ||
		_wcsicmp(captionFont.lfFaceName, L"Meiryo") == 0;
	const LONG dilation = std::clamp<LONG>(g_captionTextContrast + (thinFont ? 1 : 0), 0, 6);

	constexpr LONG boxWidth = 6;
	constexpr float k1 = 0.040675f;
	constexpr float k2 = -0.25425f;
	constexpr float k3 = 0.401275f;
	constexpr float k4 = -0.083675f;
	const float colorLuma =
		(GetRValue(textColor) + 2.f * GetGValue(textColor) + GetBValue(textColor)) /
		(4.f * 255.f);
	const float c1 = k1 * colorLuma + k2;
	const float c2 = k3 * colorLuma + k4;
	BYTE coverageTable[boxWidth + 1]{};
	for (int popcount = 0; popcount <= boxWidth; popcount++)
	{
		const float coverage = static_cast<float>(popcount * 255 / boxWidth) / 255.f;
		coverageTable[popcount] = static_cast<BYTE>(
			std::clamp(
				coverage + 4.f * coverage * (1.f - coverage) * (c1 * coverage + c2),
				0.f,
				1.f
			) * 255.f + 0.5f
		);
	}

	output = {};
	output.width = static_cast<UINT>(textWidth + 2);
	output.height = static_cast<UINT>(textHeight);
	output.originX = -1;
	output.pixels.resize(static_cast<size_t>(output.width) * output.height * 4);

	const LONG maskStride = maskInfo.header.biWidth / 8;
	for (LONG y = 0; y < textHeight; y++)
	{
		const BYTE* mask = static_cast<const BYTE*>(maskBits) + static_cast<size_t>(y) * maskStride;
		const auto sample = [&](LONG index) noexcept -> UINT
		{
			return index < 0 || index >= scaledWidth
				? 0u
				: ((mask[index >> 3] >> (7 - (index & 7))) & 1u);
		};
		const auto dilatedSample = [&](LONG index) noexcept -> UINT
		{
			for (LONG offset = 0; offset <= dilation; offset++)
			{
				if (sample(index - offset))
				{
					return 1u;
				}
			}
			return 0u;
		};
		const auto boxPopcount = [&](LONG start) noexcept -> UINT
		{
			UINT total{};
			for (LONG offset = 0; offset < boxWidth; offset++)
			{
				total += dilatedSample(start + offset);
			}
			return total;
		};

		BYTE* row = output.pixels.data() + static_cast<size_t>(y) * output.width * 4;
		for (LONG x = -1; x <= textWidth; x++)
		{
			const UINT redCoverage = coverageTable[boxPopcount(x * 6 - 2)];
			const UINT greenCoverage = coverageTable[boxPopcount(x * 6)];
			const UINT blueCoverage = coverageTable[boxPopcount(x * 6 + 2)];
			if (!redCoverage && !greenCoverage && !blueCoverage)
			{
				continue;
			}
			StoreCaptionCoveragePixel(
				row + static_cast<size_t>(x + 1) * 4,
				textColor,
				redCoverage,
				greenCoverage,
				blueCoverage,
				brushAlpha
			);
		}
	}

	return true;
}

struct CCaptionGammaRatios
{
	float g1;
	float g2;
	float g3;
	float g4;
};

constexpr CCaptionGammaRatios g_captionGammaRatios[]
{
	{ 0.000000f,  0.000000f, 0.000000f,  0.000000f },
	{ 0.004150f, -0.020175f, 0.055675f, -0.018775f },
	{ 0.008750f, -0.044000f, 0.108125f, -0.034250f },
	{ 0.013575f, -0.070525f, 0.157550f, -0.046900f },
	{ 0.018475f, -0.099075f, 0.204175f, -0.057175f },
	{ 0.023325f, -0.129025f, 0.248150f, -0.065400f },
	{ 0.028025f, -0.159875f, 0.289700f, -0.071925f },
	{ 0.032500f, -0.191225f, 0.328975f, -0.077000f },
	{ 0.036725f, -0.222775f, 0.366100f, -0.080850f },
	{ 0.040675f, -0.254250f, 0.401275f, -0.083675f },
	{ 0.044325f, -0.285500f, 0.434625f, -0.085650f },
	{ 0.047700f, -0.316300f, 0.466250f, -0.086900f },
	{ 0.050775f, -0.346600f, 0.496275f, -0.087525f }
};

constexpr float g_captionGammaScaleG1G3 = 4.0314341f;
constexpr float g_captionGammaScaleG2G4 = 4.0156865f;
constexpr int g_windows8CaptionGammaIndex = 2;
constexpr int g_captionCoverageLevelCount = 7;

void CalculateWindows8CoverageLevels(COLORREF textColor, BYTE(&levels)[g_captionCoverageLevelCount])
{
	const auto& ratios = g_captionGammaRatios[g_windows8CaptionGammaIndex];
	const float luma =
		(GetRValue(textColor) + 2.f * GetGValue(textColor) + GetBValue(textColor)) /
		(4.f * 255.f);
	const auto quantize = [](float value) noexcept
	{
		return static_cast<float>(
			std::clamp(static_cast<int>(std::floor(std::round(value * 2.f) / 2.f)), 0, 255)
		);
	};

	for (int popcount = 0; popcount < g_captionCoverageLevelCount; popcount++)
	{
		const float coverage =
			static_cast<float>(popcount * 255 / (g_captionCoverageLevelCount - 1)) / 255.f;
		const float shoulder = coverage * (1.f - coverage);
		const float first = quantize(
			(coverage + g_captionGammaScaleG2G4 * shoulder * (ratios.g2 * coverage + ratios.g4)) * 255.f
		);
		const float second = quantize(
			(g_captionGammaScaleG1G3 * shoulder * (ratios.g1 * coverage + ratios.g3)) * 255.f
		);
		levels[popcount] = static_cast<BYTE>(std::clamp(first + luma * second, 0.f, 255.f));
	}
}

bool DrawClearTypeCaptionSurface(
	const CAliasedTextSurface& surface,
	HFONT font,
	COLORREF textColor,
	BYTE background,
	std::wstring_view text,
	LONG textWidth,
	LONG textHeight,
	UINT format
)
{
	surface.Fill(background);
	const HGDIOBJ previousFont = SelectObject(surface.dc.get(), font);
	const auto fontCleanup = wil::scope_exit([&] { SelectObject(surface.dc.get(), previousFont); });
	SetBkMode(surface.dc.get(), TRANSPARENT);
	SetTextAlign(surface.dc.get(), TA_LEFT | TA_TOP);
	SetTextColor(surface.dc.get(), textColor);
	RECT rect{ 1, 0, 1 + textWidth, textHeight };
	const int result = DrawTextW(
		surface.dc.get(),
		text.data(),
		static_cast<int>(text.size()),
		&rect,
		format
	);
	GdiFlush();
	return result != 0;
}

BYTE SolveCaptionCoverage(BYTE overBlack, BYTE overWhite) noexcept
{
	return static_cast<BYTE>(255 - std::clamp(static_cast<int>(overWhite) - overBlack, 0, 255));
}

struct CCaptionCoverageMap
{
	BYTE remap[256]{};
};

LOGFONTW g_captionCoverageFont{};
std::unordered_map<COLORREF, CCaptionCoverageMap> g_captionCoverageMaps{};

const CCaptionCoverageMap* EnsureWindows8CoverageMap(
	HFONT font,
	const LOGFONTW& logFont,
	COLORREF textColor
)
{
	if (memcmp(&g_captionCoverageFont, &logFont, sizeof(logFont)) != 0)
	{
		g_captionCoverageMaps.clear();
		g_captionCoverageFont = logFont;
	}
	else if (const auto iterator = g_captionCoverageMaps.find(textColor); iterator != g_captionCoverageMaps.end())
	{
		return &iterator->second;
	}

	constexpr LPCWSTR calibrationText = L"Hamburgefonstiv WMil1|0OQ 123456789";
	const int calibrationLength = static_cast<int>(wcslen(calibrationText));
	wil::unique_hdc measureDC{ CreateCompatibleDC(nullptr) };
	if (!measureDC)
	{
		return nullptr;
	}
	const HGDIOBJ previousFont = SelectObject(measureDC.get(), font);
	RECT measureRect{};
	DrawTextW(
		measureDC.get(),
		calibrationText,
		calibrationLength,
		&measureRect,
		DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX
	);
	SelectObject(measureDC.get(), previousFont);

	const LONG width = measureRect.right + 4;
	const LONG height = std::max(measureRect.bottom, 1l) + 2;
	CAliasedTextSurface overBlack{};
	CAliasedTextSurface overWhite{};
	if (!overBlack.Create(width, height) || !overWhite.Create(width, height))
	{
		return nullptr;
	}
	constexpr UINT format = DT_SINGLELINE | DT_NOPREFIX | DT_TOP | DT_LEFT;
	const std::wstring_view calibrationView{ calibrationText, static_cast<size_t>(calibrationLength) };
	if (
		!DrawClearTypeCaptionSurface(overBlack, font, textColor, 0x00, calibrationView, measureRect.right, height, format) ||
		!DrawClearTypeCaptionSurface(overWhite, font, textColor, 0xff, calibrationView, measureRect.right, height, format)
	)
	{
		return nullptr;
	}

	std::bitset<256> seen{};
	for (size_t index = 0; index < static_cast<size_t>(width) * height * 4; index++)
	{
		if ((index & 3) != 3)
		{
			seen.set(SolveCaptionCoverage(overBlack.bits[index], overWhite.bits[index]));
		}
	}

	std::vector<BYTE> gdiLevels{};
	for (int value = 0; value < 256; value++)
	{
		if (seen.test(value))
		{
			gdiLevels.push_back(static_cast<BYTE>(value));
		}
	}

	auto& map = g_captionCoverageMaps[textColor];
	if (gdiLevels.size() != g_captionCoverageLevelCount)
	{
		for (int value = 0; value < 256; value++)
		{
			map.remap[value] = static_cast<BYTE>(value);
		}
		return &map;
	}

	BYTE dwmLevels[g_captionCoverageLevelCount]{};
	CalculateWindows8CoverageLevels(textColor, dwmLevels);
	for (int value = 0; value < 256; value++)
	{
		size_t nearest{};
		int nearestDistance = 256;
		for (size_t level = 0; level < gdiLevels.size(); level++)
		{
			const int distance = std::abs(value - static_cast<int>(gdiLevels[level]));
			if (distance < nearestDistance)
			{
				nearestDistance = distance;
				nearest = level;
			}
		}
		map.remap[value] = dwmLevels[nearest];
	}

	return &map;
}

bool RenderWindows8CaptionBitmap(
	std::wstring_view text,
	IDWriteTextLayout* textLayout,
	LOGFONTW captionFont,
	LONG textWidth,
	LONG textHeight,
	COLORREF textColor,
	BYTE brushAlpha,
	CAliasedCaptionBitmap& output
)
{
	captionFont.lfQuality = CLEARTYPE_QUALITY;
	wil::unique_hfont renderFont{ CreateFontIndirectW(&captionFont) };
	if (!renderFont)
	{
		return false;
	}
	const auto coverageMap = EnsureWindows8CoverageMap(renderFont.get(), captionFont, textColor);
	if (!coverageMap)
	{
		return false;
	}

	const LONG coverageWidth = textWidth + 2;
	CAliasedTextSurface overBlack{};
	CAliasedTextSurface overWhite{};
	if (!overBlack.Create(coverageWidth, textHeight) || !overWhite.Create(coverageWidth, textHeight))
	{
		return false;
	}
	const UINT format = GetCaptionDrawTextFlags(textLayout);
	if (
		!DrawClearTypeCaptionSurface(overBlack, renderFont.get(), textColor, 0x00, text, textWidth, textHeight, format) ||
		!DrawClearTypeCaptionSurface(overWhite, renderFont.get(), textColor, 0xff, text, textWidth, textHeight, format)
	)
	{
		return false;
	}

	output = {};
	output.width = static_cast<UINT>(coverageWidth);
	output.height = static_cast<UINT>(textHeight);
	output.originX = -1;
	output.pixels.resize(static_cast<size_t>(output.width) * output.height * 4);
	for (LONG y = 0; y < textHeight; y++)
	{
		const BYTE* blackRow = overBlack.bits + static_cast<size_t>(y) * coverageWidth * 4;
		const BYTE* whiteRow = overWhite.bits + static_cast<size_t>(y) * coverageWidth * 4;
		BYTE* outputRow = output.pixels.data() + static_cast<size_t>(y) * output.width * 4;
		for (LONG x = 0; x < coverageWidth; x++)
		{
			const UINT blueCoverage = coverageMap->remap[
				SolveCaptionCoverage(blackRow[x * 4 + 0], whiteRow[x * 4 + 0])
			];
			const UINT greenCoverage = coverageMap->remap[
				SolveCaptionCoverage(blackRow[x * 4 + 1], whiteRow[x * 4 + 1])
			];
			const UINT redCoverage = coverageMap->remap[
				SolveCaptionCoverage(blackRow[x * 4 + 2], whiteRow[x * 4 + 2])
			];
			if (!redCoverage && !greenCoverage && !blueCoverage)
			{
				continue;
			}
			StoreCaptionCoveragePixel(
				outputRow + static_cast<size_t>(x) * 4,
				textColor,
				redCoverage,
				greenCoverage,
				blueCoverage,
				brushAlpha
			);
		}
	}

	return true;
}

HRESULT DrawAliasedCaptionText(
	ID2D1DeviceContext* deviceContext,
	D2D1_POINT_2F origin,
	IDWriteTextLayout* textLayout,
	ID2D1SolidColorBrush* textBrush
)
{
	RETURN_HR_IF_NULL(E_INVALIDARG, deviceContext);
	RETURN_HR_IF_NULL(E_INVALIDARG, textLayout);
	RETURN_HR_IF_NULL(E_INVALIDARG, textBrush);
	if (!g_dwriteTextVisual || !g_window)
	{
		return S_FALSE;
	}

	const std::wstring text = ReadCurrentCaptionText();
	if (text.empty() || text.size() > static_cast<size_t>(INT_MAX))
	{
		return S_FALSE;
	}

	LOGFONTW captionFont{};
	if (!GetCaptionLogFont(textLayout, captionFont))
	{
		return S_FALSE;
	}

	DWRITE_TEXT_METRICS metrics{};
	RETURN_IF_FAILED(textLayout->GetMetrics(&metrics));
	const LONG textWidth = std::max(
		1l,
		static_cast<LONG>(std::ceil(
			g_textSizeF.Width > 0.f
				? g_textSizeF.Width
				: std::max(textLayout->GetMaxWidth(), metrics.layoutWidth)
		))
	);
	const LONG textHeight = std::max(
		1l,
		static_cast<LONG>(std::ceil(
			g_textSizeF.Height > 0.f
				? g_textSizeF.Height
				: std::max(textLayout->GetMaxHeight(), metrics.layoutHeight)
		))
	);
	if (textWidth > 32768 || textHeight > 4096)
	{
		return E_INVALIDARG;
	}

	const D2D1_COLOR_F brushColor = textBrush->GetColor();
	const auto toByte = [](float value) noexcept
	{
		return static_cast<BYTE>(std::clamp(std::lround(value * 255.f), 0l, 255l));
	};
	const COLORREF textColor = RGB(toByte(brushColor.r), toByte(brushColor.g), toByte(brushColor.b));
	const BYTE brushAlpha = toByte(brushColor.a);

	CAliasedCaptionBitmap captionBitmap{};
	const bool rendered = g_captionTextAliasing == 1
		? RenderWindows8CaptionBitmap(
			text,
			textLayout,
			captionFont,
			textWidth,
			textHeight,
			textColor,
			brushAlpha,
			captionBitmap
		)
		: RenderWin7CaptionBitmap(
			text,
			textLayout,
			captionFont,
			textWidth,
			textHeight,
			textColor,
			brushAlpha,
			captionBitmap
		);
	if (!rendered || captionBitmap.pixels.empty())
	{
		return S_FALSE;
	}

	float dpiX{};
	float dpiY{};
	deviceContext->GetDpi(&dpiX, &dpiY);
	winrt::com_ptr<ID2D1Bitmap1> bitmap{};
	RETURN_IF_FAILED(
		deviceContext->CreateBitmap(
			D2D1::SizeU(captionBitmap.width, captionBitmap.height),
			captionBitmap.pixels.data(),
			captionBitmap.width * 4,
			D2D1::BitmapProperties1(
				D2D1_BITMAP_OPTIONS_NONE,
				D2D1::PixelFormat(
					DXGI_FORMAT_B8G8R8A8_UNORM,
					D2D1_ALPHA_MODE_PREMULTIPLIED
				),
				dpiX,
				dpiY
			),
			bitmap.put()
		)
	);

	const D2D1_RECT_F destination
	{
		origin.x + static_cast<float>(captionBitmap.originX),
		origin.y + static_cast<float>(captionBitmap.originY),
		origin.x + static_cast<float>(captionBitmap.originX) + static_cast<float>(captionBitmap.width),
		origin.y + static_cast<float>(captionBitmap.originY) + static_cast<float>(captionBitmap.height)
	};
	deviceContext->DrawBitmap(
		bitmap.get(),
		&destination,
		1.f,
		D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
		nullptr
	);
	return S_OK;
}

void ResetAliasedCaptionTextResources()
{
	g_captionCoverageFont = {};
	g_captionCoverageMaps.clear();
}
