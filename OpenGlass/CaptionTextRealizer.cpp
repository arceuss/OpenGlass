#include "pch.h"
#include "resource.h"
#include "HookHelper.hpp"
#include "uDwmProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "Util.hpp"
#include "CaptionTextRealizer.hpp"

using namespace OpenGlass;

namespace OpenGlass::CaptionTextRealizer
{
	// CRenderDataBuilder::DrawImage writes this record; data points at the type field
	struct CDrawImageCommand : dwmcore::CRenderCommand
	{
		UINT imageIndex;
		D2D1_RECT_F rect;
	};
	// mov dword ptr [rcx+4], 1CFh in CRenderDataBuilder::DrawImage (19041 codebase)
	constexpr int g_drawImageCommandType{ 463 };

	HRESULT STDMETHODCALLTYPE MyCRenderData_ProcessUpdate(
		dwmcore::CRenderData* This,
		dwmcore::CResourceTable* resourceTable,
		PVOID a3,
		PVOID a4,
		PVOID a5,
		PVOID a6
	);
	HRESULT MyCDrawingContext_DrawImage(
		dwmcore::IDrawingContext* This,
		dwmcore::CResource* image,
		const D2D1_RECT_F* lprc,
		dwmcore::CResource* rectResource
	);

	decltype(&MyCRenderData_ProcessUpdate) g_CRenderData_ProcessUpdate_Org{ nullptr };
	decltype(&MyCDrawingContext_DrawImage) g_CDrawingContext_DrawImage_Org{ nullptr };
	decltype(&MyCDrawingContext_DrawImage)* g_CDrawingContext_DrawImage_Org_Address{ nullptr };

	struct PayloadEntry
	{
		const void* owner{ nullptr };
		UINT generation{ 0 };
		CoveragePayload payload{};
	};
	wil::srwlock g_payloadLock{};
	std::unordered_map<UINT, PayloadEntry> g_payloads{};
	UINT g_payloadGeneration{ 0 };

	wil::srwlock g_tableLock{};
	std::unordered_map<dwmcore::CRenderData*, dwmcore::CResourceTable*> g_resourceTables{};

	// set when a caption text command is declined from the drawlist; consumed by the
	// immediately following IDrawingContext::DrawImage call on the same thread
	thread_local dwmcore::CResource* t_pendingImage{ nullptr };
	thread_local UINT t_pendingHandle{ 0 };

	// render-thread GPU objects
	struct TextureEntry
	{
		winrt::com_ptr<ID3D11Texture2D> texture{ nullptr };
		winrt::com_ptr<ID3D11ShaderResourceView> srv{ nullptr };
		UINT generation{ 0 };
	};
	struct QuadVertex
	{
		float x, y;
		float u, v;
	};
	struct ConstantBufferVS
	{
		D2D1_VECTOR_2F viewportSize;
		D2D1_VECTOR_2F padding;
	};
	ID3D11Device* g_deviceNoRef{ nullptr };
	winrt::com_ptr<ID3D11VertexShader> g_vertexShader{ nullptr };
	winrt::com_ptr<ID3D11PixelShader> g_pixelShader{ nullptr };
	winrt::com_ptr<ID3D11InputLayout> g_inputLayout{ nullptr };
	winrt::com_ptr<ID3D11Buffer> g_vertexBuffer{ nullptr };
	winrt::com_ptr<ID3D11Buffer> g_constantBufferVS{ nullptr };
	winrt::com_ptr<ID3D11BlendState> g_blendState{ nullptr };
	winrt::com_ptr<ID3D11SamplerState> g_samplerState{ nullptr };
	winrt::com_ptr<ID3D11RasterizerState> g_rasterizerState{ nullptr };
	std::unordered_map<UINT, TextureEntry> g_textures{};

	HRESULT EnsureDeviceResources(ID3D11Device* device);
	HRESULT EnsureTexture(ID3D11Device* device, UINT handle, const PayloadEntry& entry, TextureEntry** result);
	HRESULT DrawCaptionText(dwmcore::CDrawingContext* drawingContext, const D2D1_RECT_F& imageRect, UINT handle);
}

bool CaptionTextRealizer::IsAvailable()
{
	return g_CRenderData_ProcessUpdate_Org && dwmcore::CBitmapResource::vftable;
}

void CaptionTextRealizer::RegisterPayload(const void* owner, UINT resourceHandle, CoveragePayload&& payload)
{
	const auto lock = g_payloadLock.lock_exclusive();
	// Keep the previous bitmap alive until the replacement has actually reached the
	// render thread. Dropping it here leaves in-flight draw commands without text.
	g_payloads[resourceHandle] = PayloadEntry{ owner, ++g_payloadGeneration, std::move(payload) };
}

void CaptionTextRealizer::UnregisterOwner(const void* owner)
{
	const auto lock = g_payloadLock.lock_exclusive();
	std::erase_if(g_payloads, [owner](const auto& pair) { return pair.second.owner == owner; });
}

HRESULT STDMETHODCALLTYPE CaptionTextRealizer::MyCRenderData_ProcessUpdate(
	dwmcore::CRenderData* This,
	dwmcore::CResourceTable* resourceTable,
	PVOID a3,
	PVOID a4,
	PVOID a5,
	PVOID a6
)
{
	{
		const auto lock = g_tableLock.lock_exclusive();
		g_resourceTables[This] = resourceTable;
	}
	return g_CRenderData_ProcessUpdate_Org(
		This,
		resourceTable,
		a3,
		a4,
		a5,
		a6
	);
}

bool CaptionTextRealizer::OnTryDrawCommand(
	dwmcore::CRenderData* This,
	dwmcore::CDrawingContext* drawingContext,
	int commandType,
	DWM::span<dwmcore::CRenderCommand>* resources,
	bool* succeeded
)
{
	if (
		commandType != g_drawImageCommandType ||
		resources->length != sizeof(CDrawImageCommand)
	)
	{
		return false;
	}
	const auto command = static_cast<CDrawImageCommand*>(resources->data);
	const auto renderDataResources = This->GetResources();
	if (command->imageIndex >= renderDataResources->count)
	{
		return false;
	}
	const auto image = renderDataResources->data[command->imageIndex];
	if (HookHelper::get_vftable_from(image) != dwmcore::CBitmapResource::vftable)
	{
		return false;
	}

	dwmcore::CResourceTable* resourceTable{ nullptr };
	{
		const auto lock = g_tableLock.lock_shared();
		if (const auto it = g_resourceTables.find(This); it != g_resourceTables.end())
		{
			resourceTable = it->second;
		}
	}
	if (!resourceTable)
	{
		return false;
	}

	UINT matchedHandle{ 0 };
	{
		const auto lock = g_payloadLock.lock_shared();
		for (const auto& [handle, entry] : g_payloads)
		{
			if (resourceTable->GetResourceFromHandle(handle) == image)
			{
				matchedHandle = handle;
				break;
			}
		}
	}
	if (!matchedHandle)
	{
		return false;
	}

	if (!g_CDrawingContext_DrawImage_Org)
	{
		g_CDrawingContext_DrawImage_Org_Address = reinterpret_cast<decltype(g_CDrawingContext_DrawImage_Org_Address)>(&(HookHelper::get_vftable_from(drawingContext->GetInterface())[3]));
		HookHelper::PatchPointerT(
			g_CDrawingContext_DrawImage_Org_Address,
			MyCDrawingContext_DrawImage,
			&g_CDrawingContext_DrawImage_Org
		);
	}

	t_pendingImage = image;
	t_pendingHandle = matchedHandle;
	*succeeded = false;
	return true;
}

HRESULT CaptionTextRealizer::MyCDrawingContext_DrawImage(
	dwmcore::IDrawingContext* This,
	dwmcore::CResource* image,
	const D2D1_RECT_F* lprc,
	dwmcore::CResource* rectResource
)
{
	// original draw first: the caption DIB now carries only the glow
	const HRESULT hr{ g_CDrawingContext_DrawImage_Org(This, image, lprc, rectResource) };

	if (!t_pendingImage || t_pendingImage != image || !lprc)
	{
		return hr;
	}
	const UINT handle{ t_pendingHandle };
	t_pendingImage = nullptr;
	t_pendingHandle = 0;

	if (SUCCEEDED(hr))
	{
		LOG_IF_FAILED(DrawCaptionText(This->GetDrawingContext(), *lprc, handle));
	}
	return hr;
}

HRESULT CaptionTextRealizer::EnsureDeviceResources(ID3D11Device* device)
{
	if (g_deviceNoRef != device)
	{
		DestroyDeviceResources();
	}
	if (g_vertexShader)
	{
		return S_OK;
	}

	std::span<const UCHAR> vsBytes{};
	RETURN_IF_FAILED(Util::GetResDataView(vsBytes, IDR_RCDATA_CAPTIONTEXT_VS));
	RETURN_IF_FAILED(device->CreateVertexShader(vsBytes.data(), vsBytes.size_bytes(), nullptr, g_vertexShader.put()));

	const D3D11_INPUT_ELEMENT_DESC layout[]
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	RETURN_IF_FAILED(device->CreateInputLayout(layout, ARRAYSIZE(layout), vsBytes.data(), vsBytes.size_bytes(), g_inputLayout.put()));

	std::span<const UCHAR> psBytes{};
	RETURN_IF_FAILED(Util::GetResDataView(psBytes, IDR_RCDATA_CAPTIONTEXT_PS));
	RETURN_IF_FAILED(device->CreatePixelShader(psBytes.data(), psBytes.size_bytes(), nullptr, g_pixelShader.put()));

	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = sizeof(QuadVertex) * 4;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	RETURN_IF_FAILED(device->CreateBuffer(&bufferDesc, nullptr, g_vertexBuffer.put()));

	bufferDesc = {};
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = sizeof(ConstantBufferVS);
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	RETURN_IF_FAILED(device->CreateBuffer(&bufferDesc, nullptr, g_constantBufferVS.put()));

	// the Win7 ClearType component-alpha blend (d3drenderstate BlendMode 2):
	// dst.ch = textColor.ch * coverage.ch + dst.ch * (1 - coverage.ch)
	D3D11_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_BLEND_FACTOR;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	RETURN_IF_FAILED(device->CreateBlendState(&blendDesc, g_blendState.put()));

	D3D11_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	RETURN_IF_FAILED(device->CreateSamplerState(&samplerDesc, g_samplerState.put()));

	D3D11_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.ScissorEnable = TRUE;
	rasterizerDesc.DepthClipEnable = TRUE;
	RETURN_IF_FAILED(device->CreateRasterizerState(&rasterizerDesc, g_rasterizerState.put()));

	g_deviceNoRef = device;
	return S_OK;
}

HRESULT CaptionTextRealizer::EnsureTexture(
	ID3D11Device* device,
	UINT handle,
	const PayloadEntry& entry,
	TextureEntry** result
)
{
	auto& texture = g_textures[handle];
	if (texture.texture && texture.generation == entry.generation)
	{
		*result = &texture;
		return S_OK;
	}

	texture = {};
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = entry.payload.width;
	textureDesc.Height = entry.payload.height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA initialData{ entry.payload.pixels.data(), static_cast<UINT>(entry.payload.width * 4), 0 };
	RETURN_IF_FAILED(device->CreateTexture2D(&textureDesc, &initialData, texture.texture.put()));
	RETURN_IF_FAILED(device->CreateShaderResourceView(texture.texture.get(), nullptr, texture.srv.put()));
	texture.generation = entry.generation;

	*result = &texture;
	return S_OK;
}

HRESULT CaptionTextRealizer::DrawCaptionText(
	dwmcore::CDrawingContext* drawingContext,
	const D2D1_RECT_F& imageRect,
	UINT handle
)
{
	// snapshot the payload metrics, owner, generation, and text color under the lock
	LONG bitmapWidth{}, bitmapHeight{}, x{}, y{}, width{}, height{};
	COLORREF textColor{};
	const void* owner{};
	UINT generation{};
	{
		const auto lock = g_payloadLock.lock_shared();
		const auto it = g_payloads.find(handle);
		if (it == g_payloads.end())
		{
			return S_FALSE;
		}
		bitmapWidth = it->second.payload.bitmapWidth;
		bitmapHeight = it->second.payload.bitmapHeight;
		x = it->second.payload.x;
		y = it->second.payload.y;
		width = it->second.payload.width;
		height = it->second.payload.height;
		textColor = it->second.payload.textColor;
		owner = it->second.owner;
		generation = it->second.generation;
	}
	if (width <= 0 || height <= 0 || bitmapWidth <= 0 || bitmapHeight <= 0)
	{
		return S_FALSE;
	}

	const auto d3dDevice = drawingContext->GetD3DDevice();
	RETURN_HR_IF_NULL(E_FAIL, d3dDevice);
	const auto device = d3dDevice->GetDevice();
	const auto context = d3dDevice->GetImmediateContext();
	RETURN_HR_IF_NULL(E_FAIL, device);
	RETURN_HR_IF_NULL(E_FAIL, context);
	const auto deviceTarget = drawingContext->GetDeviceTarget();
	RETURN_HR_IF_NULL(E_FAIL, deviceTarget);
	const auto renderTargetView = deviceTarget->GetRenderTargetView();
	RETURN_HR_IF_NULL(E_FAIL, renderTargetView);

	RETURN_IF_FAILED(EnsureDeviceResources(device));
	TextureEntry* texture{ nullptr };
	{
		const auto lock = g_payloadLock.lock_shared();
		const auto it = g_payloads.find(handle);
		if (it == g_payloads.end())
		{
			return S_FALSE;
		}
		RETURN_IF_FAILED(EnsureTexture(device, handle, it->second, &texture));
	}

	// the coverage block in local (bitmap) space, scaled the way the DIB was
	// stretched into the destination rect (1:1 in practice)
	const float scaleX{ (imageRect.right - imageRect.left) / bitmapWidth };
	const float scaleY{ (imageRect.bottom - imageRect.top) / bitmapHeight };
	const D2D1_RECT_F localRect
	{
		imageRect.left + x * scaleX,
		imageRect.top + y * scaleY,
		imageRect.left + (x + width) * scaleX,
		imageRect.top + (y + height) * scaleY
	};
	const auto worldMatrix = drawingContext->GetWorldTransform()->GetD2DMatrix();
	const auto transformPoint = [&worldMatrix](float px, float py) -> D2D1_POINT_2F
	{
		return {
			px * worldMatrix.m11 + py * worldMatrix.m21 + worldMatrix.dx,
			px * worldMatrix.m12 + py * worldMatrix.m22 + worldMatrix.dy
		};
	};
	const D2D1_POINT_2F corners[4]
	{
		transformPoint(localRect.left, localRect.top),
		transformPoint(localRect.right, localRect.top),
		transformPoint(localRect.left, localRect.bottom),
		transformPoint(localRect.right, localRect.bottom)
	};

	D2D1_RECT_F clipBounds{};
	drawingContext->GetClipBoundsWorld(clipBounds);

	// order the D2D-batched glow draw before our raw quad
	RETURN_IF_FAILED(drawingContext->FlushD2D());

	winrt::com_ptr<ID3D11Resource> renderTargetResource{ nullptr };
	renderTargetView->GetResource(renderTargetResource.put());
	winrt::com_ptr<ID3D11Texture2D> renderTargetTexture{ renderTargetResource.try_as<ID3D11Texture2D>() };
	RETURN_HR_IF_NULL(E_FAIL, renderTargetTexture);
	D3D11_TEXTURE2D_DESC renderTargetDesc{};
	renderTargetTexture->GetDesc(&renderTargetDesc);

	Util::D3D11ContextStateGuard stateGuard{ context };

	D3D11_MAPPED_SUBRESOURCE mapped{};
	RETURN_IF_FAILED(context->Map(g_vertexBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
	{
		const auto vertices = static_cast<QuadVertex*>(mapped.pData);
		vertices[0] = { corners[0].x, corners[0].y, 0.f, 0.f };
		vertices[1] = { corners[1].x, corners[1].y, 1.f, 0.f };
		vertices[2] = { corners[2].x, corners[2].y, 0.f, 1.f };
		vertices[3] = { corners[3].x, corners[3].y, 1.f, 1.f };
	}
	context->Unmap(g_vertexBuffer.get(), 0);

	RETURN_IF_FAILED(context->Map(g_constantBufferVS.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
	{
		const auto constants = static_cast<ConstantBufferVS*>(mapped.pData);
		constants->viewportSize = { static_cast<float>(renderTargetDesc.Width), static_cast<float>(renderTargetDesc.Height) };
		constants->padding = {};
	}
	context->Unmap(g_constantBufferVS.get(), 0);

	ID3D11RenderTargetView* renderTargets[]{ renderTargetView };
	context->OMSetRenderTargets(1, renderTargets, nullptr);
	const float blendFactor[4]
	{
		GetRValue(textColor) / 255.f,
		GetGValue(textColor) / 255.f,
		GetBValue(textColor) / 255.f,
		1.f
	};
	context->OMSetBlendState(g_blendState.get(), blendFactor, 0xffffffff);
	context->OMSetDepthStencilState(nullptr, 0);

	const D3D11_VIEWPORT viewport{ 0.f, 0.f, static_cast<float>(renderTargetDesc.Width), static_cast<float>(renderTargetDesc.Height), 0.f, 1.f };
	context->RSSetViewports(1, &viewport);
	const D3D11_RECT scissor
	{
		static_cast<LONG>(std::floor(clipBounds.left)),
		static_cast<LONG>(std::floor(clipBounds.top)),
		static_cast<LONG>(std::ceil(clipBounds.right)),
		static_cast<LONG>(std::ceil(clipBounds.bottom))
	};
	context->RSSetScissorRects(1, &scissor);
	context->RSSetState(g_rasterizerState.get());

	const UINT stride{ sizeof(QuadVertex) };
	const UINT offset{ 0 };
	ID3D11Buffer* vertexBuffers[]{ g_vertexBuffer.get() };
	context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
	context->IASetInputLayout(g_inputLayout.get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	context->VSSetShader(g_vertexShader.get(), nullptr, 0);
	ID3D11Buffer* constantBuffers[]{ g_constantBufferVS.get() };
	context->VSSetConstantBuffers(0, 1, constantBuffers);

	context->PSSetShader(g_pixelShader.get(), nullptr, 0);
	ID3D11ShaderResourceView* shaderResourceViews[]{ texture->srv.get() };
	context->PSSetShaderResources(0, 1, shaderResourceViews);
	ID3D11SamplerState* samplers[]{ g_samplerState.get() };
	context->PSSetSamplers(0, 1, samplers);

	context->Draw(4, 0);

	// The replacement is now visible. Older bitmaps for this visual can no longer
	// be referenced by a future render update, so retire their CPU and GPU data.
	{
		const auto lock = g_payloadLock.lock_exclusive();
		for (auto it = g_payloads.begin(); it != g_payloads.end();)
		{
			if (it->second.owner == owner && it->second.generation < generation)
			{
				g_textures.erase(it->first);
				it = g_payloads.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	return S_OK;
}

void CaptionTextRealizer::DestroyDeviceResources()
{
	g_textures.clear();
	g_vertexShader = nullptr;
	g_pixelShader = nullptr;
	g_inputLayout = nullptr;
	g_vertexBuffer = nullptr;
	g_constantBufferVS = nullptr;
	g_blendState = nullptr;
	g_samplerState = nullptr;
	g_rasterizerState = nullptr;
	g_deviceNoRef = nullptr;
}

void CaptionTextRealizer::Startup()
{
	if (
		dwmcore::g_versionInfo.build < os::build_w10_2004 ||
		dwmcore::g_versionInfo.build >= os::build_server_2022
	)
	{
		return;
	}
	dwmcore::g_projectionArray.ApplyToVariable("CRenderData::ProcessUpdate", g_CRenderData_ProcessUpdate_Org);
	if (!g_CRenderData_ProcessUpdate_Org || !dwmcore::CBitmapResource::vftable)
	{
		g_CRenderData_ProcessUpdate_Org = nullptr;
		return;
	}
	HookHelper::PatchFunctions(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CRenderData_ProcessUpdate_Org, &MyCRenderData_ProcessUpdate }
		},
		true
	);
}

void CaptionTextRealizer::Shutdown()
{
	if (g_CRenderData_ProcessUpdate_Org)
	{
		HookHelper::PatchFunctions(
			std::initializer_list<HookHelper::DetourInfo>
			{
				{ &g_CRenderData_ProcessUpdate_Org, &MyCRenderData_ProcessUpdate }
			},
			false
		);
	}
	if (g_CDrawingContext_DrawImage_Org)
	{
		HookHelper::PatchPointerT(
			g_CDrawingContext_DrawImage_Org_Address,
			g_CDrawingContext_DrawImage_Org
		);
		g_CDrawingContext_DrawImage_Org_Address = nullptr;
		g_CDrawingContext_DrawImage_Org = nullptr;
	}
	{
		const auto lock = g_tableLock.lock_exclusive();
		g_resourceTables.clear();
	}
	const auto lock = g_payloadLock.lock_exclusive();
	g_payloads.clear();
}
