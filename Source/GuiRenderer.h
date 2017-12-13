#pragma once

#include "DirectX12.h"


class GuiRenderer
{
public:
	GuiRenderer(const DirectX12& dx12);
	~GuiRenderer();

	void Draw(ID3D12GraphicsCommandList* cmdList);

private:
	const DirectX12& m_Dx12;

	// vertex buffer
	ID3D12Resource* m_Vb[2] = {};
	void* m_VbCpuAddr[2];
	uint32_t m_VbSize[2] = {};
	D3D12_VERTEX_BUFFER_VIEW m_VbView[2];

	// index buffer
	ID3D12Resource* m_Ib[2] = {};
	void* m_IbCpuAddr[2];
	uint32_t m_IbSize[2] = {};
	D3D12_INDEX_BUFFER_VIEW m_IbView[2];

	ID3D12RootSignature* m_Rs = nullptr;
	ID3D12PipelineState* m_Pso = nullptr;
	ID3D12Resource* m_FontTex = nullptr;
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_FontTexDescriptor;

	void CompileShaders();
};
