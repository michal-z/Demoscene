#include "Pch.h"
#include "PlayWithNoise.h"
#include "DirectX12.h"
#include "Library.h"


PlayWithNoise::PlayWithNoise(const DirectX12& dx12) : m_Dx12(dx12), m_GuiRenderer(dx12)
{
	// PSO for culled object
	{
		std::vector<uint8_t> csoVs = Lib::LoadFile("Assets/Shaders/SimpleTransformVS.cso");
		std::vector<uint8_t> csoPs = Lib::LoadFile("Assets/Shaders/SimpleShadingPS.cso");

		D3D12_INPUT_ELEMENT_DESC inputElements[] =
		{
			{ "AV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "AV_Normal", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElements, (uint32_t)std::size(inputElements) };
		psoDesc.VS = { csoVs.data(), csoVs.size() };
		psoDesc.PS = { csoPs.data(), csoPs.size() };
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		psoDesc.SampleMask = 0xffffffff;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		psoDesc.SampleDesc.Count = 1;

		VHR(m_Dx12.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_ObjectPso)));
		VHR(m_Dx12.GetDevice()->CreateRootSignature(0, csoVs.data(), csoVs.size(), IID_PPV_ARGS(&m_ObjectRs)));
	}

	CreateBuffers();

	m_ObjectsAos.resize(k_NumObjects);
	m_ObjectsSoa.Alloc(k_NumObjects);

	for (uint32_t z = 0; z < k_NumObjectsPerSide; ++z)
		for (uint32_t y = 0; y < k_NumObjectsPerSide; ++y)
			for (uint32_t x = 0; x < k_NumObjectsPerSide; ++x)
			{
				const float scale = 3.0f;
				const float bias = -0.5f * (scale * k_NumObjectsPerSide);
				const uint32_t objIdx = x + y * k_NumObjectsPerSide + z * k_NumObjectsPerSide * k_NumObjectsPerSide;
	
				XMFLOAT3 pos = XMFLOAT3(x * scale + bias, y * scale + bias, z * scale + bias);
				XMMATRIX xform = XMMatrixTranslationFromVector(XMLoadFloat3(&pos));

				XMStoreFloat4x4(&m_ObjectsAos[objIdx].objectToWorldTransform, xform);	
				m_ObjectsAos[objIdx].color = PackedVector::XMCOLOR(Lib::Randomf(), Lib::Randomf(), Lib::Randomf(), 1.0f);
				m_ObjectsAos[objIdx].boundingSphere = BoundingSphere(pos, 0.5f);

				m_ObjectsSoa.objectToWorldTransforms[objIdx] = m_ObjectsAos[objIdx].objectToWorldTransform;
				m_ObjectsSoa.colors[objIdx] = m_ObjectsAos[objIdx].color;
				m_ObjectsSoa.boundingSpheres[objIdx] = m_ObjectsAos[objIdx].boundingSphere;
			}
}

PlayWithNoise::~PlayWithNoise()
{
	m_Dx12.Flush();
	SAFE_RELEASE(m_ObjectVb);
	SAFE_RELEASE(m_ObjectIb);
	SAFE_RELEASE(m_ObjectPso);
	SAFE_RELEASE(m_ObjectRs);
	_mm_free(m_CacheBuffer);
}

void PlayWithNoise::Update(double frameTime, float frameDeltaTime)
{
	static __itt_domain* vtuneDomain = __itt_domain_create("FrustumCulling.Update");
	static __itt_string_handle* vtuneCull = __itt_string_handle_create("Cull");
	static __itt_string_handle* vtuneTrashCache = __itt_string_handle_create("TrashCache");


	static double frameTime0 = frameTime;
	const float time = (float)(frameTime - frameTime0);

	ImGui::NewFrame();

	// camera movement
	{
		ImGuiIO& io = ImGui::GetIO();

		if (io.MouseDown[1])
		{
			m_CameraPitch += io.MouseDelta.y * 0.003f;
			m_CameraYaw += io.MouseDelta.x * 0.003f;
		}

		XMVECTOR forward = XMVector3Transform(g_XMIdentityR2, XMMatrixRotationRollPitchYaw(m_CameraPitch, m_CameraYaw, 0.0f));
		XMVECTOR right = XMVector3Normalize(XMVector3Cross(g_XMIdentityR1, forward));
		XMVECTOR position = XMLoadFloat3(&m_CameraPosition);

		if (io.KeysDown[VK_UP] || io.KeysDown['W'])
			position += forward * 10.0f * frameDeltaTime;
		else if (io.KeysDown[VK_DOWN] || io.KeysDown['S'])
			position -= forward * 10.0f * frameDeltaTime;

		if (io.KeysDown['D'])
			position += right * 10.0f * frameDeltaTime;
		if (io.KeysDown['A'])
			position -= right * 10.0f * frameDeltaTime;

		if (io.KeysDown[VK_RIGHT])
			m_CameraYaw += frameDeltaTime;
		else if (io.KeysDown[VK_LEFT])
			m_CameraYaw -= frameDeltaTime;

		XMStoreFloat3(&m_CameraPosition, position);
	}

	{
		XMVECTOR camPosition = XMLoadFloat3(&m_CameraPosition);

		XMVECTOR camOrientat = XMQuaternionRotationMatrix(XMMatrixRotationX(m_CameraPitch) * XMMatrixRotationY(m_CameraYaw));

		BoundingFrustum::CreateFromMatrix(m_ViewFrustum, XMMatrixPerspectiveFovLH(XM_PI / 3.1f, 1.777f, 0.1f, 1000.0f));
		m_ViewFrustum.Transform(m_ViewFrustum, 1.0f, camOrientat, camPosition);
	}


	uint8_t* cbCpuAddr = (uint8_t*)m_Dx12.AllocateUploadMemory(
		k_NumObjects * (sizeof(XMFLOAT4X4) + sizeof(PackedVector::XMCOLOR)), m_InstancesDataGpuAddr);


	m_NumVisibleObjects = 0;

	const double cullingBegin = Lib::GetTime();


	__itt_task_begin(vtuneDomain, __itt_null, __itt_null, vtuneCull);

	if (m_UseSoaDataLayout)
	{
		for (uint32_t objIdx = 0; objIdx < k_NumObjects; ++objIdx)
		{
			if (m_FrustumCullingEnabled && !m_ViewFrustum.Intersects(m_ObjectsSoa.boundingSpheres[objIdx]))
				continue;

			XMStoreFloat4x4((XMFLOAT4X4*)cbCpuAddr, XMMatrixTranspose(XMLoadFloat4x4(&m_ObjectsSoa.objectToWorldTransforms[objIdx])));
			*(PackedVector::XMCOLOR*)(cbCpuAddr + sizeof(XMFLOAT4X4)) = m_ObjectsSoa.colors[objIdx];

			cbCpuAddr += (sizeof(XMFLOAT4X4) + sizeof(PackedVector::XMCOLOR));

			m_NumVisibleObjects++;
		}
	}
	else
	{
		for (uint32_t objIdx = 0; objIdx < k_NumObjects; ++objIdx)
		{
			if (m_FrustumCullingEnabled && !m_ViewFrustum.Intersects(m_ObjectsAos[objIdx].boundingSphere))
				continue;

			XMStoreFloat4x4((XMFLOAT4X4*)cbCpuAddr, XMMatrixTranspose(XMLoadFloat4x4(&m_ObjectsAos[objIdx].objectToWorldTransform)));
			*(PackedVector::XMCOLOR*)(cbCpuAddr + sizeof(XMFLOAT4X4)) = m_ObjectsAos[objIdx].color;

			cbCpuAddr += (sizeof(XMFLOAT4X4) + sizeof(PackedVector::XMCOLOR));

			m_NumVisibleObjects++;
		}
	}

	if (m_FrustumCullingEnabled)
		m_CullingTimes[m_Dx12.GetFrameCount() % 20] = (float)((Lib::GetTime() - cullingBegin) * 1000000.0);

	__itt_task_end(vtuneDomain);


	// GUI
	{
		ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f));
		ImGui::SetNextWindowSize(ImVec2(400.0f, 120.0f));
		ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_ShowBorders);
		ImGui::Checkbox("Frustum Culling", &m_FrustumCullingEnabled);
		ImGui::Checkbox("Use SOA Data Layout", &m_UseSoaDataLayout);
		ImGui::Text("Visible Objects:  %d", m_NumVisibleObjects);
		ImGui::PlotLines("Culling Activity", m_CullingTimes, 20, 0, nullptr, 0.0f, FLT_MAX);
		ImGui::End();
	}

	{
		__itt_task_begin(vtuneDomain, __itt_null, __itt_null, vtuneTrashCache);

		volatile uint8_t* ptr = m_CacheBuffer;
		__m256i d = _mm256_set1_epi32((uint32_t)m_Dx12.GetFrameCount());

		for (uint32_t i = 0; i < k_CacheBufferSize / 64; ++i)
		{
			_mm256_store_si256((__m256i*)ptr, d);
			_mm256_store_si256((__m256i*)(ptr + 32), d);
			ptr += 64;
		}

		__itt_task_end(vtuneDomain);
	}

	ImGui::Render();
}


void PlayWithNoise::Draw()
{
	const uint32_t frameIdx = m_Dx12.GetFrameIndex();


	ID3D12CommandAllocator* cmdAlloc = m_Dx12.GetCmdAllocator();
	cmdAlloc->Reset();

	ID3D12GraphicsCommandList* cmdList = m_Dx12.GetCmdList();

	cmdList->Reset(cmdAlloc, nullptr);
	cmdList->RSSetViewports(1, &m_Dx12.GetViewport());
	cmdList->RSSetScissorRects(1, &m_Dx12.GetScissorRect());

	cmdList->ResourceBarrier(
		1, &CD3DX12_RESOURCE_BARRIER::Transition(m_Dx12.GetBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	D3D12_CPU_DESCRIPTOR_HANDLE backBufferHandle = m_Dx12.GetBackBufferHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE depthBufferHandle = m_Dx12.GetDepthBufferHandle();

	cmdList->OMSetRenderTargets(1, &backBufferHandle, 0, &depthBufferHandle);

	cmdList->ClearRenderTargetView(backBufferHandle, XMVECTORF32{ 0.0f, 0.2f, 0.4f, 1.0f }, 0, nullptr);
	cmdList->ClearDepthStencilView(depthBufferHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	cmdList->IASetVertexBuffers(0, 1, &m_ObjectVbView);
	cmdList->IASetIndexBuffer(&m_ObjectIbView);
	cmdList->SetPipelineState(m_ObjectPso);

	cmdList->SetGraphicsRootSignature(m_ObjectRs);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	{
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddr;
		XMFLOAT4X4A* cpuAddr = (XMFLOAT4X4A*)m_Dx12.AllocateUploadMemory(64, gpuAddr);


		const XMMATRIX worldToProj =
			XMMatrixTranslation(-m_CameraPosition.x, -m_CameraPosition.y, -m_CameraPosition.z) *
			XMMatrixRotationY(-m_CameraYaw) *
			XMMatrixRotationX(-m_CameraPitch) *
			XMMatrixPerspectiveFovLH(XM_PI / 3.1f, 1.777f, 0.1f, 1000.0f);

		XMStoreFloat4x4A(cpuAddr, XMMatrixTranspose(worldToProj));

		cmdList->SetGraphicsRootConstantBufferView(1, gpuAddr);
	}

	cmdList->SetGraphicsRootShaderResourceView(0, m_InstancesDataGpuAddr);
	cmdList->DrawIndexedInstanced(m_ObjectIndexCount, m_NumVisibleObjects, 0, 0, 0);


	PIXBeginEvent(cmdList, PIX_COLOR(255, 255, 0), "DrawGui");

	m_GuiRenderer.Draw(cmdList);

	PIXEndEvent(cmdList);



	cmdList->ResourceBarrier(
		1, &CD3DX12_RESOURCE_BARRIER::Transition(m_Dx12.GetBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	VHR(cmdList->Close());



	m_Dx12.GetCmdQueue()->ExecuteCommandLists(1, (ID3D12CommandList**)&cmdList);
}

void PlayWithNoise::CreateBuffers()
{
	ID3D12Device* device = m_Dx12.GetDevice();

	m_CacheBuffer = (uint8_t*)_mm_malloc(k_CacheBufferSize, 64);

	{	// vertex buffer
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string errors;
		bool r = tinyobj::LoadObj(&attrib, &shapes, &materials, &errors, "Assets/Meshes/Sphere1.obj");
		assert(r);

		struct Vertex
		{
			XMFLOAT3 position;
			XMFLOAT3 normal;
		};
		std::vector<Vertex> vertices;
		std::vector<uint16_t> indices;
		vertices.reserve(shapes[0].mesh.indices.size());
		indices.reserve(shapes[0].mesh.indices.size());
		
		std::map<uint32_t, uint16_t> vertexIndexLookup;

		for (const tinyobj::index_t& idx : shapes[0].mesh.indices)
		{
			const uint16_t vi = (uint16_t)idx.vertex_index;
			const uint16_t ni = (uint16_t)idx.normal_index;
			const uint32_t key = vi | (uint32_t(ni) << 16);

			auto found = vertexIndexLookup.find(key);
			if (found == vertexIndexLookup.end())
			{
				const uint16_t index = (uint16_t)vertices.size();

				vertexIndexLookup.insert(std::make_pair(key, index));
				indices.push_back(index);

				vertices.push_back({
					XMFLOAT3(attrib.vertices[vi * 3], attrib.vertices[vi * 3 + 1], attrib.vertices[vi * 3 + 2]),
					XMFLOAT3(attrib.normals[ni * 3], attrib.normals[ni * 3 + 1], attrib.normals[ni * 3 + 2])
				});
			}
			else
			{
				indices.push_back(found->second);
			}
		}
		m_ObjectIndexCount = (uint32_t)indices.size();

		const auto vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vertices.size() * sizeof(vertices[0]));
		const auto ibDesc = CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0]));

		VHR(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &vbDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
			IID_PPV_ARGS(&m_ObjectVb)));

		VHR(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &ibDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
			IID_PPV_ARGS(&m_ObjectIb)));


		ID3D12Resource* intermediateBufs[2];
		VHR(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &vbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&intermediateBufs[0])));

		VHR(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &ibDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&intermediateBufs[1])));

		m_Dx12.AddIntermediateResource(intermediateBufs[0]);
		m_Dx12.AddIntermediateResource(intermediateBufs[1]);


		m_ObjectVbView.BufferLocation = m_ObjectVb->GetGPUVirtualAddress();
		m_ObjectVbView.StrideInBytes = sizeof(vertices[0]);
		m_ObjectVbView.SizeInBytes = (UINT)(vertices.size() * sizeof(vertices[0]));

		m_ObjectIbView.BufferLocation = m_ObjectIb->GetGPUVirtualAddress();
		m_ObjectIbView.Format = DXGI_FORMAT_R16_UINT;
		m_ObjectIbView.SizeInBytes = (UINT)(indices.size() * sizeof(indices[0]));

		D3D12_SUBRESOURCE_DATA vbData = { vertices.data(), (LONG_PTR)(vertices.size() * sizeof(vertices[0])) };
		D3D12_SUBRESOURCE_DATA ibData = { indices.data(), (LONG_PTR)(indices.size() * sizeof(indices[0])) };

		UpdateSubresources<1>(m_Dx12.GetCmdList(), m_ObjectVb, intermediateBufs[0], 0, 0, 1, &vbData);
		UpdateSubresources<1>(m_Dx12.GetCmdList(), m_ObjectIb, intermediateBufs[1], 0, 0, 1, &ibData);


		m_Dx12.GetCmdList()->ResourceBarrier(
			1, &CD3DX12_RESOURCE_BARRIER::Transition(m_ObjectVb,
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
		m_Dx12.GetCmdList()->ResourceBarrier(
			1, &CD3DX12_RESOURCE_BARRIER::Transition(m_ObjectIb,
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));
	}
}
