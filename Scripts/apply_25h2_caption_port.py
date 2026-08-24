from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HANDLER = ROOT / "OpenGlass/Architecture/Legacy/CaptionTextHandler.cpp"
CATALOG = ROOT / "Common/SettingsCatalog.hpp"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


handler = HANDLER.read_text(encoding="utf-8")

handler = replace_once(
    handler,
    "\tint g_textGlowSize{};\n\tint g_textGlowIntensity{};\n\tint g_centerCaption{ 0 };\n\tint CaptionCenterOffset",
    "\tint g_textGlowSize{};\n"
    "\tint g_textGlowIntensity{};\n"
    "\tint g_centerCaption{ 0 };\n"
    "\tint g_captionTextAliasing{ 0 };\n"
    "\tint g_captionTextContrast{ 1 };\n"
    "\n"
    "#include \"CaptionTextRasterizer.inl\"\n"
    "\n"
    "\tint CaptionSurfacePadding() noexcept\n"
    "\t{\n"
    "\t\treturn std::max(g_textGlowSize, 1);\n"
    "\t}\n"
    "\n"
    "\tint CaptionCenterOffset",
    "caption rasterizer include",
)

handler = replace_once(
    handler,
    "\tif (!g_textGlowSize)\n"
    "\t{\n"
    "\t\treturn g_ID2D1DeviceContext_DrawTextLayout_Org(\n"
    "\t\t\tThis,\n"
    "\t\t\torigin,\n"
    "\t\t\ttextLayout,\n"
    "\t\t\tdefaultFillBrush,\n"
    "\t\t\toptions\n"
    "\t\t);\n"
    "\t}\n",
    "\tif (!g_textGlowSize)\n"
    "\t{\n"
    "\t\torigin.x += static_cast<float>(CaptionSurfacePadding());\n"
    "\t\torigin.y += static_cast<float>(CaptionSurfacePadding());\n"
    "\t\tconst HRESULT aliasingResult = DrawAliasedCaptionText(\n"
    "\t\t\tThis,\n"
    "\t\t\torigin,\n"
    "\t\t\ttextLayout,\n"
    "\t\t\tsolidColorBrush.get()\n"
    "\t\t);\n"
    "\t\tif (aliasingResult == S_OK)\n"
    "\t\t{\n"
    "\t\t\treturn;\n"
    "\t\t}\n"
    "\t\tLOG_IF_FAILED(aliasingResult);\n"
    "\t\treturn g_ID2D1DeviceContext_DrawTextLayout_Org(\n"
    "\t\t\tThis,\n"
    "\t\t\torigin,\n"
    "\t\t\ttextLayout,\n"
    "\t\t\tdefaultFillBrush,\n"
    "\t\t\toptions\n"
    "\t\t);\n"
    "\t}\n",
    "zero-glow aliasing path",
)

handler = replace_once(
    handler,
    "\t\tThis->DrawImage(\n"
    "\t\t\tbitmap.get(),\n"
    "\t\t\t&origin,\n"
    "\t\t\tnullptr,\n"
    "\t\t\tD2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR\n"
    "\t\t);\n"
    "\t\treturn;\n"
    "\t}\n",
    "\t\tconst HRESULT aliasingResult = DrawAliasedCaptionText(\n"
    "\t\t\tThis,\n"
    "\t\t\torigin,\n"
    "\t\t\ttextLayout,\n"
    "\t\t\tsolidColorBrush.get()\n"
    "\t\t);\n"
    "\t\tif (aliasingResult == S_OK)\n"
    "\t\t{\n"
    "\t\t\treturn;\n"
    "\t\t}\n"
    "\t\tLOG_IF_FAILED(aliasingResult);\n"
    "\t\treturn g_ID2D1DeviceContext_DrawTextLayout_Org(\n"
    "\t\t\tThis,\n"
    "\t\t\torigin,\n"
    "\t\t\ttextLayout,\n"
    "\t\t\tdefaultFillBrush,\n"
    "\t\t\toptions\n"
    "\t\t);\n"
    "\t}\n",
    "mode-3 glow aliasing path",
)

handler = replace_once(
    handler,
    "\treturn g_ID2D1DeviceContext_DrawTextLayout_Org(\n"
    "\t\tThis,\n"
    "\t\torigin,\n"
    "\t\ttextLayout,\n"
    "\t\tdefaultFillBrush,\n"
    "\t\toptions\n"
    "\t);\n"
    "}\n\nHRESULT CaptionTextHandler::MyICompositionGraphicsDevice_CreateDrawingSurface",
    "\tconst HRESULT aliasingResult = DrawAliasedCaptionText(\n"
    "\t\tThis,\n"
    "\t\torigin,\n"
    "\t\ttextLayout,\n"
    "\t\tsolidColorBrush.get()\n"
    "\t);\n"
    "\tif (aliasingResult == S_OK)\n"
    "\t{\n"
    "\t\treturn;\n"
    "\t}\n"
    "\tLOG_IF_FAILED(aliasingResult);\n"
    "\treturn g_ID2D1DeviceContext_DrawTextLayout_Org(\n"
    "\t\tThis,\n"
    "\t\torigin,\n"
    "\t\ttextLayout,\n"
    "\t\tdefaultFillBrush,\n"
    "\t\toptions\n"
    "\t);\n"
    "}\n\nHRESULT CaptionTextHandler::MyICompositionGraphicsDevice_CreateDrawingSurface",
    "final aliasing path",
)

handler = replace_once(
    handler,
    "\torigin.x += g_textGlowSize;\n"
    "\torigin.y += g_textGlowSize;",
    "\torigin.x += static_cast<float>(CaptionSurfacePadding());\n"
    "\torigin.y += static_cast<float>(CaptionSurfacePadding());",
    "caption surface origin padding",
)

handler = replace_once(
    handler,
    "\tif (g_dwriteTextVisual)\n"
    "\t{\n"
    "\t\tg_textSizeF = sizePixels;\n"
    "\t\tsizePixels.Width += g_textGlowSize * 2;\n"
    "\t\tsizePixels.Height += g_textGlowSize * 2;\n"
    "\t}",
    "\tif (g_dwriteTextVisual)\n"
    "\t{\n"
    "\t\tg_textSizeF = sizePixels;\n"
    "\t\tconst int padding = CaptionSurfacePadding();\n"
    "\t\tsizePixels.Width += padding * 2;\n"
    "\t\tsizePixels.Height += padding * 2;\n"
    "\t}",
    "caption surface size padding",
)

handler = replace_once(
    handler,
    "\tif (g_dwriteTextVisual)\n"
    "\t{\n"
    "\t\tvalue.Y -= g_textGlowSize;\n"
    "\t\tvalue.X -= g_textGlowSize;",
    "\tif (g_dwriteTextVisual)\n"
    "\t{\n"
    "\t\tconst int padding = CaptionSurfacePadding();\n"
    "\t\tvalue.Y -= padding;\n"
    "\t\tvalue.X -= padding;",
    "caption brush base padding",
)

handler = replace_once(
    handler,
    "\t\tif (auto& offset = const_cast<POINT&>(g_dwriteTextVisual->GetOffset()); g_dwriteTextVisual->IsRTLMirrored())\n"
    "\t\t{\n"
    "\t\t\tif (offset.x > g_textGlowSize)\n"
    "\t\t\t{\n"
    "\t\t\t\tvalue.X += g_textGlowSize;\n"
    "\t\t\t}\n"
    "\t\t\telse\n"
    "\t\t\t{\n"
    "\t\t\t\tvalue.X += g_textGlowSize + g_textGlowSize - offset.x;\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t\telse\n"
    "\t\t{\n"
    "\t\t\tvalue.X += offset.x - std::max(offset.x - g_textGlowSize, 0l);\n"
    "\t\t}",
    "\t\tif (auto& offset = const_cast<POINT&>(g_dwriteTextVisual->GetOffset()); g_dwriteTextVisual->IsRTLMirrored())\n"
    "\t\t{\n"
    "\t\t\tif (offset.x > padding)\n"
    "\t\t\t{\n"
    "\t\t\t\tvalue.X += padding;\n"
    "\t\t\t}\n"
    "\t\t\telse\n"
    "\t\t\t{\n"
    "\t\t\t\tvalue.X += padding + padding - offset.x;\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t\telse\n"
    "\t\t{\n"
    "\t\t\tvalue.X += offset.x - std::max(offset.x - padding, 0l);\n"
    "\t\t}",
    "caption brush visual padding",
)

handler = replace_once(
    handler,
    "HRESULT CaptionTextHandler::MyCDWriteText_UpdateOffset(uDWM::CDWriteText* This)\n"
    "{\n"
    "\tif (!g_textGlowSize)\n"
    "\t{\n"
    "\t\treturn g_CDWriteText_UpdateOffset_Org(This);\n"
    "\t}\n"
    "\n"
    "\t// SpriteVisual will crop what exceeds its bounding rectangle,",
    "HRESULT CaptionTextHandler::MyCDWriteText_UpdateOffset(uDWM::CDWriteText* This)\n"
    "{\n"
    "\tconst int padding = CaptionSurfacePadding();\n"
    "\n"
    "\t// SpriteVisual will crop what exceeds its bounding rectangle,",
    "caption visual offset padding prologue",
)

handler = replace_once(
    handler,
    "\tif (!This->IsRTLMirrored())\n"
    "\t{\n"
    "\t\toffset.x = std::max(offset.x - g_textGlowSize, 0l);\n"
    "\t}",
    "\tif (!This->IsRTLMirrored())\n"
    "\t{\n"
    "\t\toffset.x = std::max(offset.x - padding, 0l);\n"
    "\t}",
    "caption visual offset amount",
)

handler = replace_once(
    handler,
    "HRESULT CaptionTextHandler::MyCDWriteText_SetSize(uDWM::CDWriteText* This, const SIZE* size)\n"
    "{\n"
    "\tif (!g_textGlowSize)\n"
    "\t{\n"
    "\t\treturn g_CDWriteText_SetSize_Org(This, size);\n"
    "\t}\n"
    "\n"
    "\tconst auto hr = g_CDWriteText_SetSize_Org(This, size);",
    "HRESULT CaptionTextHandler::MyCDWriteText_SetSize(uDWM::CDWriteText* This, const SIZE* size)\n"
    "{\n"
    "\tconst int padding = CaptionSurfacePadding();\n"
    "\n"
    "\tconst auto hr = g_CDWriteText_SetSize_Org(This, size);",
    "caption visual size padding prologue",
)

handler = replace_once(
    handler,
    "\tif (This->IsRTLMirrored())\n"
    "\t{\n"
    "\t\tThis->GetVisualProxy()->SetSize(\n"
    "\t\t\tstatic_cast<double>(size->cx + offset.x - std::max(offset.x - g_textGlowSize, 0l)),\n"
    "\t\t\tstatic_cast<double>(size->cy)\n"
    "\t\t);\n"
    "\t}\n"
    "\telse\n"
    "\t{\n"
    "\t\tThis->GetVisualProxy()->SetSize(\n"
    "\t\t\tstatic_cast<double>(size->cx + offset.x - std::max(offset.x - g_textGlowSize, 0l) + g_textGlowSize),\n"
    "\t\t\tstatic_cast<double>(size->cy)\n"
    "\t\t);\n"
    "\t}",
    "\tif (This->IsRTLMirrored())\n"
    "\t{\n"
    "\t\tThis->GetVisualProxy()->SetSize(\n"
    "\t\t\tstatic_cast<double>(size->cx + offset.x - std::max(offset.x - padding, 0l)),\n"
    "\t\t\tstatic_cast<double>(size->cy)\n"
    "\t\t);\n"
    "\t}\n"
    "\telse\n"
    "\t{\n"
    "\t\tThis->GetVisualProxy()->SetSize(\n"
    "\t\t\tstatic_cast<double>(size->cx + offset.x - std::max(offset.x - padding, 0l) + padding),\n"
    "\t\t\tstatic_cast<double>(size->cy)\n"
    "\t\t);\n"
    "\t}",
    "caption visual size amount",
)

handler = replace_once(
    handler,
    "void CaptionTextHandler::DestroyDeviceResources()\n"
    "{\n"
    "\tg_textGlowRT = nullptr;",
    "void CaptionTextHandler::DestroyDeviceResources()\n"
    "{\n"
    "\tResetAliasedCaptionTextResources();\n"
    "\tg_textGlowRT = nullptr;",
    "aliasing cache cleanup",
)

handler = replace_once(
    handler,
    "\tif (type & GlassEngine::UpdateType::Backdrop || type & GlassEngine::UpdateType::Theme)\n"
    "\t{\n"
    "\t\tg_centerCaption = std::clamp(static_cast<int>(GlassEngine::GetDwordFromRegistry(L\"CenterCaption\", FALSE)), 0, 2);",
    "\tif (type & GlassEngine::UpdateType::Backdrop || type & GlassEngine::UpdateType::Theme)\n"
    "\t{\n"
    "\t\tconst int previousAliasing = g_captionTextAliasing;\n"
    "\t\tconst int previousContrast = g_captionTextContrast;\n"
    "\t\tg_captionTextAliasing = std::clamp(\n"
    "\t\t\tstatic_cast<int>(GlassEngine::GetDwordFromRegistry(L\"CaptionTextAliasing\", 0)),\n"
    "\t\t\t0,\n"
    "\t\t\t1\n"
    "\t\t);\n"
    "\t\tg_captionTextContrast = std::clamp(\n"
    "\t\t\tstatic_cast<int>(GlassEngine::GetDwordFromRegistry(L\"CaptionTextContrast\", 1)),\n"
    "\t\t\t0,\n"
    "\t\t\t6\n"
    "\t\t);\n"
    "\t\tif (\n"
    "\t\t\tuDWM::g_versionInfo.build >= os::build_w11_22h2 &&\n"
    "\t\t\t(previousAliasing != g_captionTextAliasing || previousContrast != g_captionTextContrast)\n"
    "\t\t)\n"
    "\t\t{\n"
    "\t\t\tResetAliasedCaptionTextResources();\n"
    "\t\t\tfor (const auto& [visual, state] : g_textVisualStateMap)\n"
    "\t\t\t{\n"
    "\t\t\t\tif (visual && state)\n"
    "\t\t\t\t{\n"
    "\t\t\t\t\tvisual->SetDirtyFlags(0x2);\n"
    "\t\t\t\t}\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\n"
    "\t\tg_centerCaption = std::clamp(static_cast<int>(GlassEngine::GetDwordFromRegistry(L\"CenterCaption\", FALSE)), 0, 2);",
    "registry settings",
)

HANDLER.write_text(handler, encoding="utf-8", newline="\n")

catalog = CATALOG.read_text(encoding="utf-8")
catalog = replace_once(
    catalog,
    "\t\tCaptionButtons,\n\t\tCenterCaption,\n\t\tTextGlowMode,",
    "\t\tCaptionButtons,\n\t\tCenterCaption,\n\t\tCaptionTextAliasing,\n\t\tCaptionTextContrast,\n\t\tTextGlowMode,",
    "catalog ids",
)
catalog = replace_once(
    catalog,
    "\t\t{ Id::CaptionButtons, L\"CaptionButtons\", ValueType::Dword, Scope::Machine, AssetRole::None, UpdateImpact::Theme, false },\n"
    "\t\t{ Id::CenterCaption, L\"CenterCaption\", ValueType::Dword, Scope::Machine, AssetRole::None, UpdateImpact::Theme, false },\n"
    "\t\t{ Id::TextGlowMode, L\"TextGlowMode\", ValueType::Dword, Scope::Machine, AssetRole::None, UpdateImpact::Theme, false },",
    "\t\t{ Id::CaptionButtons, L\"CaptionButtons\", ValueType::Dword, Scope::Machine, AssetRole::None, UpdateImpact::Theme, false },\n"
    "\t\t{ Id::CenterCaption, L\"CenterCaption\", ValueType::Dword, Scope::Machine, AssetRole::None, UpdateImpact::Theme, false },\n"
    "\t\t{ Id::CaptionTextAliasing, L\"CaptionTextAliasing\", ValueType::Dword, Scope::Machine, AssetRole::None, UpdateImpact::Theme, false },\n"
    "\t\t{ Id::CaptionTextContrast, L\"CaptionTextContrast\", ValueType::Dword, Scope::Machine, AssetRole::None, UpdateImpact::Theme, false },\n"
    "\t\t{ Id::TextGlowMode, L\"TextGlowMode\", ValueType::Dword, Scope::Machine, AssetRole::None, UpdateImpact::Theme, false },",
    "catalog specs",
)
CATALOG.write_text(catalog, encoding="utf-8", newline="\n")
