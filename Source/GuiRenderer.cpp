#include "Pch.h"
#include "GuiRenderer.h"
#include "Library.h"


GuiRenderer::GuiRenderer(const DirectX12& dx12) : m_Dx12(dx12)
{
	ID3D12Device* device = m_Dx12.GetDevice();

	{	// font texture
		ImGuiIO& io = ImGui::GetIO();

		uint8_t* pixels;
		int32_t width, height;
		io.Fonts->AddFontFromFileTTF("Assets/Roboto-Medium.ttf", 18.0f);
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

		const auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, (UINT64)width, height);
		VHR(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
			IID_PPV_ARGS(&m_FontTex)));

		ID3D12Resource* intermediateBuf = nullptr;
		{
			uint64_t bufferSize;
			device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &bufferSize);

			const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
			VHR(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr, IID_PPV_ARGS(&intermediateBuf)));

			m_Dx12.AddIntermediateResource(intermediateBuf);
		}

		D3D12_SUBRESOURCE_DATA texData = { pixels, (LONG_PTR)(width * 4) };
		UpdateSubresources<1>(m_Dx12.GetCmdList(), m_FontTex, intermediateBuf, 0, 0, 1, &texData);

		m_Dx12.GetCmdList()->ResourceBarrier(
			1, &CD3DX12_RESOURCE_BARRIER::Transition(m_FontTex,
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));


		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		m_Dx12.AllocateCpuDescriptors(1, m_FontTexDescriptor);
		device->CreateShaderResourceView(m_FontTex, &srvDesc, m_FontTexDescriptor);
	}

	CompileShaders();
}

GuiRenderer::~GuiRenderer()
{
	SAFE_RELEASE(m_FontTex);
	SAFE_RELEASE(m_Pso);
	SAFE_RELEASE(m_Rs);
	for (uint32_t i = 0; i < 2; ++i)
	{
		SAFE_RELEASE(m_Vb[i]);
		SAFE_RELEASE(m_Ib[i]);
	}
}

void GuiRenderer::Draw(ID3D12GraphicsCommandList* cmdList)
{
	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData->TotalVtxCount == 0)
		return;

	ImGuiIO& io = ImGui::GetIO();
	const uint32_t frameIdx = m_Dx12.GetFrameIndex();

	const int32_t fbWidth = (int32_t)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
	const int32_t fbHeight = (int32_t)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
	drawData->ScaleClipRects(io.DisplayFramebufferScale);

	// create/resize vertex buffer
	if (m_VbSize[frameIdx] == 0 || m_VbSize[frameIdx] < drawData->TotalVtxCount * sizeof(ImDrawVert))
	{
		SAFE_RELEASE(m_Vb[frameIdx]);
		VHR(m_Dx12.GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(drawData->TotalVtxCount * sizeof(ImDrawVert)), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&m_Vb[frameIdx])));

		VHR(m_Vb[frameIdx]->Map(0, &CD3DX12_RANGE(0, 0), &m_VbCpuAddr[frameIdx]));

		m_VbSize[frameIdx] = drawData->TotalVtxCount * sizeof(ImDrawVert);

		m_VbView[frameIdx].BufferLocation = m_Vb[frameIdx]->GetGPUVirtualAddress();
		m_VbView[frameIdx].StrideInBytes = sizeof(ImDrawVert);
		m_VbView[frameIdx].SizeInBytes = drawData->TotalVtxCount * sizeof(ImDrawVert);
	}
	// create/resize index buffer
	if (m_IbSize[frameIdx] == 0 || m_IbSize[frameIdx] < drawData->TotalIdxCount * sizeof(ImDrawIdx))
	{
		SAFE_RELEASE(m_Ib[frameIdx]);
		VHR(m_Dx12.GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(drawData->TotalIdxCount * sizeof(ImDrawIdx)), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&m_Ib[frameIdx])));

		VHR(m_Ib[frameIdx]->Map(0, &CD3DX12_RANGE(0, 0), &m_IbCpuAddr[frameIdx]));

		m_IbSize[frameIdx] = drawData->TotalIdxCount * sizeof(ImDrawIdx);

		m_IbView[frameIdx].BufferLocation = m_Ib[frameIdx]->GetGPUVirtualAddress();
		m_IbView[frameIdx].Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
		m_IbView[frameIdx].SizeInBytes = drawData->TotalIdxCount * sizeof(ImDrawIdx);
	}
	// update vertex and index buffers
	{
		ImDrawVert* vtxDst = (ImDrawVert*)m_VbCpuAddr[frameIdx];
		ImDrawIdx* idxDst = (ImDrawIdx*)m_IbCpuAddr[frameIdx];

		for (uint32_t n = 0; n < (uint32_t)drawData->CmdListsCount; ++n)
		{
			ImDrawList* drawList = drawData->CmdLists[n];
			memcpy(vtxDst, &drawList->VtxBuffer[0], drawList->VtxBuffer.size() * sizeof(ImDrawVert));
			memcpy(idxDst, &drawList->IdxBuffer[0], drawList->IdxBuffer.size() * sizeof(ImDrawIdx));
			vtxDst += drawList->VtxBuffer.size();
			idxDst += drawList->IdxBuffer.size();
		}
	}

	D3D12_GPU_VIRTUAL_ADDRESS cbGpuAddr;
	void* cbCpuAddr = m_Dx12.AllocateUploadMemory(64, cbGpuAddr);

	// update constant buffer
	{
		const float L = 0.0f;
		const float R = (float)fbWidth;
		const float B = (float)fbHeight;
		const float T = 0.0f;
		const float mvp[4][4] =
		{
			{ 2.0f / (R - L),    0.0f,              0.0f,    0.0f },
			{ 0.0f,              2.0f / (T - B),    0.0f,    0.0f },
			{ 0.0f,              0.0f,              0.5f,    0.0f },
			{ (R + L) / (L - R), (T + B) / (B - T), 0.5f,    1.0f },
		};
		memcpy(cbCpuAddr, mvp, sizeof(mvp));
	}

	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)fbWidth, (float)fbHeight, 0.0f, 1.0f };
	cmdList->RSSetViewports(1, &viewport);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->SetPipelineState(m_Pso);
	cmdList->SetGraphicsRootSignature(m_Rs);

	{
		ID3D12DescriptorHeap* heap = m_Dx12.GetGpuDescriptorHeap();
		cmdList->SetDescriptorHeaps(1, &heap);

		CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;
		m_Dx12.AllocateGpuDescriptors(1, cpuHandle, gpuHandle);
		m_Dx12.GetDevice()->CopyDescriptorsSimple(1, cpuHandle, m_FontTexDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmdList->SetGraphicsRootDescriptorTable(1, gpuHandle);
	}

	cmdList->IASetVertexBuffers(0, 1, &m_VbView[frameIdx]);
	cmdList->IASetIndexBuffer(&m_IbView[frameIdx]);
	cmdList->SetGraphicsRootConstantBufferView(0, cbGpuAddr);

	int32_t vtxOffset = 0;
	uint32_t idxOffset = 0;
	for (uint32_t n = 0; n < (uint32_t)drawData->CmdListsCount; ++n)
	{
		ImDrawList* drawList = drawData->CmdLists[n];

		for (uint32_t cmd = 0; cmd < (uint32_t)drawList->CmdBuffer.size(); ++cmd)
		{
			ImDrawCmd* pcmd = &drawList->CmdBuffer[cmd];

			if (pcmd->UserCallback)
			{
				pcmd->UserCallback(drawList, pcmd);
			}
			else
			{
				D3D12_RECT r = { (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y, (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w };
				cmdList->RSSetScissorRects(1, &r);
				cmdList->DrawIndexedInstanced(pcmd->ElemCount, 1, idxOffset, vtxOffset, 0);
			}
			idxOffset += pcmd->ElemCount;
		}
		vtxOffset += drawList->VtxBuffer.size();
	}
}

void GuiRenderer::CompileShaders()
{
	D3D12_INPUT_ELEMENT_DESC inputElements[] =
	{
		{ "AV_Position", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "AV_Texcoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "AV_Color", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	std::vector<uint8_t> csoVs = Lib::LoadFile("Assets/Shaders/GuiVS.cso");
	std::vector<uint8_t> csoPs = Lib::LoadFile("Assets/Shaders/GuiPS.cso");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.InputLayout = { inputElements, (uint32_t)std::size(inputElements) };
	desc.VS = { csoVs.data(), csoVs.size() };
	desc.PS = { csoPs.data(), csoPs.size() };
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
	desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;

	VHR(m_Dx12.GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_Pso)));
	VHR(m_Dx12.GetDevice()->CreateRootSignature(0, csoVs.data(), csoVs.size(), IID_PPV_ARGS(&m_Rs)));
}
