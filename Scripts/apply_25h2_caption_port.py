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
