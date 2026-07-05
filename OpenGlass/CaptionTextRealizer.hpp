#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "dwmcoreProjection.hpp"

// Composites Win7-accurate caption text with the real per-channel ClearType blend
// (BlendMode 2: src = textColor * coverage, dst *= 1 - coverage, per channel).
// The caption DIB a text visual uploads can only carry a single alpha channel, so
// uDWM-side rendering bakes the glow into the DIB and hands the per-subpixel
// coverage to this realizer; when dwmcore draws that bitmap we let the (glow-only)
// original draw run and then draw the glyph coverage over it with a custom D3D11
// blend state, against the fully composited glass behind it.
namespace OpenGlass::CaptionTextRealizer
{
	struct CoveragePayload
	{
		// dimensions of the caption DIB the payload belongs to
		LONG bitmapWidth{};
		LONG bitmapHeight{};
		// placement of the coverage block within that DIB
		LONG x{};
		LONG y{};
		LONG width{};
		LONG height{};
		COLORREF textColor{};
		// width*height BGRA pixels: B/G/R = per-subpixel gamma-corrected coverage,
		// A = green (center tap) coverage, matching the Win7 glyph shader output
		std::vector<BYTE> pixels{};
	};

	// uDWM side: true when the dwmcore hooks are operational and RenderWin7CaptionText
	// should produce a payload instead of baking text into the caption DIB
	bool IsAvailable();
	void RegisterPayload(const void* owner, UINT resourceHandle, CoveragePayload&& payload);
	void UnregisterOwner(const void* owner);

	// dwmcore side: called from GlassRenderer's CRenderData::TryDrawCommandAsDrawList
	// hook; declines the drawlist for caption text image commands so the immediate
	// IDrawingContext::DrawImage path (which we patch) executes them
	bool OnTryDrawCommand(
		dwmcore::CRenderData* This,
		dwmcore::CDrawingContext* drawingContext,
		int commandType,
		DWM::span<dwmcore::CRenderCommand>* resources,
		bool* succeeded
	);

	void DestroyDeviceResources();
	void Startup();
	void Shutdown();
}
