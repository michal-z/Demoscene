#pragma once

#define VHR(hr) if (FAILED(hr)) { assert(0); }
#define SAFE_RELEASE(obj) if ((obj)) { (obj)->Release(); (obj) = nullptr; }

class DirectX12
{
public:
	~DirectX12();
	bool Initialize(HWND window);
	void Present() const;
	void Flush() const;

	ID3D12Device* GetDevice() const;
	ID3D12CommandAllocator* GetCmdAllocator() const;
	ID3D12CommandQueue* GetCmdQueue() const;
	ID3D12GraphicsCommandList* GetCmdList() const;
	ID3D12Resource* GetBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferHandle() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetDepthBufferHandle() const;
	const D3D12_VIEWPORT& GetViewport() const;
	const D3D12_RECT& GetScissorRect() const;
	uint32_t GetFrameIndex() const;
	uint64_t GetFrameCount() const;
	ID3D12DescriptorHeap* GetGpuDescriptorHeap() const;
	uint32_t GetDescriptorSize() const;
	uint32_t GetDescriptorSizeRtv() const;

	void AddIntermediateResource(ID3D12Resource* resource) const;
	void UploadAndReleaseAllIntermediateResources() const;

	void AllocateGpuDescriptors(uint32_t count, CD3DX12_CPU_DESCRIPTOR_HANDLE& o_CpuDesc, CD3DX12_GPU_DESCRIPTOR_HANDLE& o_GpuDesc) const;
	void AllocateCpuDescriptors(uint32_t count, CD3DX12_CPU_DESCRIPTOR_HANDLE& o_CpuDesc) const;

	void* AllocateUploadMemory(uint32_t size, D3D12_GPU_VIRTUAL_ADDRESS& o_GpuAddr) const;

private:
	mutable uint32_t m_BackBufferIndex = 0;
	mutable uint32_t m_FrameIndex = 0;

	uint32_t m_DescriptorSize;
	uint32_t m_DescriptorSizeRtv;
	D3D12_CPU_DESCRIPTOR_HANDLE m_SwapBuffersHeapStart;
	D3D12_CPU_DESCRIPTOR_HANDLE m_DepthBufferHeapStart;

	ID3D12Device* m_Device = nullptr;
	ID3D12CommandQueue* m_CmdQueue = nullptr;
	ID3D12CommandAllocator* m_CmdAlloc[2] = {};
	ID3D12GraphicsCommandList* m_CmdList = nullptr;

	IDXGIFactory4* m_Factory = nullptr;
	IDXGISwapChain3* m_SwapChain = nullptr;
	ID3D12DescriptorHeap* m_SwapBuffersHeap = nullptr;
	ID3D12DescriptorHeap* m_DepthBufferHeap = nullptr;
	ID3D12Resource* m_SwapBuffers[4] = {};
	ID3D12Resource*	m_DepthBuffer = nullptr;

	mutable uint64_t m_CpuCompletedFrames = 0;
	ID3D12Fence* m_FrameFence = nullptr;
	HANDLE m_FrameFenceEvent = nullptr;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;

	mutable std::vector<ID3D12Resource*> m_IntermediateResources;

	static const uint32_t k_MaxGpuDescriptorCount = 10000;
	ID3D12DescriptorHeap* m_GpuDescriptorHeap[2] = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_GpuDescriptorHeapCpuStart[2];
	CD3DX12_GPU_DESCRIPTOR_HANDLE m_GpuDescriptorHeapGpuStart[2];
	mutable uint32_t m_AllocatedGpuDescriptorCount;

	static const uint32_t k_MaxCpuDescriptorCount = 10000;
	ID3D12DescriptorHeap* m_CpuDescriptorHeap = nullptr;
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_CpuDescriptorHeapCpuStart;
	mutable uint32_t m_AllocatedCpuDescriptorCount;

	static const uint32_t k_UploadMemorySize = 8 * 1024 * 1024;
	ID3D12Resource* m_UploadMemory[2];
	uint8_t* m_UploadMemoryCpuAddr[2];
	D3D12_GPU_VIRTUAL_ADDRESS m_UploadMemoryGpuAddr[2];
	mutable uint32_t m_AllocatedUploadMemory;
};

inline ID3D12Device* DirectX12::GetDevice() const
{
	return m_Device;
}

inline ID3D12CommandAllocator* DirectX12::GetCmdAllocator() const
{
	return m_CmdAlloc[m_FrameIndex];
}

inline ID3D12CommandQueue* DirectX12::GetCmdQueue() const
{
	return m_CmdQueue;
}

inline ID3D12GraphicsCommandList* DirectX12::GetCmdList() const
{
	return m_CmdList;
}

inline ID3D12Resource* DirectX12::GetBackBuffer() const
{
	return m_SwapBuffers[m_BackBufferIndex];
}

inline D3D12_CPU_DESCRIPTOR_HANDLE DirectX12::GetBackBufferHandle() const
{
	D3D12_CPU_DESCRIPTOR_HANDLE h = m_SwapBuffersHeapStart;
	h.ptr += m_BackBufferIndex * m_DescriptorSizeRtv;
	return h;
}

inline D3D12_CPU_DESCRIPTOR_HANDLE DirectX12::GetDepthBufferHandle() const
{
	return m_DepthBufferHeapStart;
}

inline const D3D12_VIEWPORT& DirectX12::GetViewport() const
{
	return m_Viewport;
}

inline const D3D12_RECT& DirectX12::GetScissorRect() const
{
	return m_ScissorRect;
}

inline uint32_t DirectX12::GetFrameIndex() const
{
	return m_FrameIndex;
}

inline uint64_t DirectX12::GetFrameCount() const
{
	return m_CpuCompletedFrames;
}

inline ID3D12DescriptorHeap* DirectX12::GetGpuDescriptorHeap() const
{
	return m_GpuDescriptorHeap[m_FrameIndex];
}

inline uint32_t DirectX12::GetDescriptorSize() const
{
	return m_DescriptorSize;
}

inline uint32_t DirectX12::GetDescriptorSizeRtv() const
{
	return m_DescriptorSizeRtv;
}

inline void DirectX12::AddIntermediateResource(ID3D12Resource* resource) const
{
	assert(resource);
	m_IntermediateResources.push_back(resource);
}

inline void DirectX12::AllocateGpuDescriptors(
	uint32_t count, CD3DX12_CPU_DESCRIPTOR_HANDLE& o_CpuDesc, CD3DX12_GPU_DESCRIPTOR_HANDLE& o_GpuDesc) const
{
	assert(count > 0);
	assert((m_AllocatedGpuDescriptorCount + count) < k_MaxGpuDescriptorCount);

	CD3DX12_CPU_DESCRIPTOR_HANDLE::InitOffsetted(
		o_CpuDesc, m_GpuDescriptorHeapCpuStart[m_FrameIndex], m_AllocatedGpuDescriptorCount * m_DescriptorSize);

	CD3DX12_GPU_DESCRIPTOR_HANDLE::InitOffsetted(
		o_GpuDesc, m_GpuDescriptorHeapGpuStart[m_FrameIndex], m_AllocatedGpuDescriptorCount * m_DescriptorSize);

	m_AllocatedGpuDescriptorCount += count;
}

inline void DirectX12::AllocateCpuDescriptors(uint32_t count, CD3DX12_CPU_DESCRIPTOR_HANDLE& o_CpuDesc) const
{
	assert(count > 0);
	assert((m_AllocatedCpuDescriptorCount + count) < k_MaxCpuDescriptorCount);

	CD3DX12_CPU_DESCRIPTOR_HANDLE::InitOffsetted(
		o_CpuDesc, m_CpuDescriptorHeapCpuStart, m_AllocatedCpuDescriptorCount * m_DescriptorSize);

	m_AllocatedCpuDescriptorCount += count;
}

inline void* DirectX12::AllocateUploadMemory(uint32_t size, D3D12_GPU_VIRTUAL_ADDRESS& o_GpuAddr) const
{
	assert(size > 0);
	if (size & 0xff) // always align to 256 bytes
		size = (size + 255) & ~0xff;
	assert((m_AllocatedUploadMemory + size) < k_UploadMemorySize);
	
	void* cpuAddr = m_UploadMemoryCpuAddr[m_FrameIndex] + m_AllocatedUploadMemory;
	o_GpuAddr = m_UploadMemoryGpuAddr[m_FrameIndex] + m_AllocatedUploadMemory;

	m_AllocatedUploadMemory += size;
	return cpuAddr;
}
