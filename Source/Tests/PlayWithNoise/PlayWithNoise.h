#pragma once

#include "TestScene.h"
#include "GuiRenderer.h"


class DirectX12;
class PlayWithNoise : public TestScene
{
public:
	explicit PlayWithNoise(const DirectX12& dx12);
	~PlayWithNoise();

private:
	struct Object
	{
		XMFLOAT4X4 objectToWorldTransform;
		PackedVector::XMCOLOR color;
		BoundingSphere boundingSphere;
		XMFLOAT4X4 worldToObjectTransform;
		uint64_t stateMask;
		bool active;
		XMFLOAT3 velocity;
		char* name;
		XMFLOAT4X4 simulationStateTransform;
	};

	struct Objects
	{
		XMFLOAT4X4* objectToWorldTransforms = nullptr;
		PackedVector::XMCOLOR* colors = nullptr;
		BoundingSphere* boundingSpheres = nullptr;
		XMFLOAT4X4* worldToObjectTransforms = nullptr;
		uint64_t* stateMasks = nullptr;
		bool* actives = nullptr;
		XMFLOAT3* velocities = nullptr;
		char** names = nullptr;
		XMFLOAT4X4* simulationStateTransforms = nullptr;

		~Objects()
		{
			_mm_free(objectToWorldTransforms);
			_mm_free(colors);
			_mm_free(boundingSpheres);
			_mm_free(worldToObjectTransforms);
			_mm_free(stateMasks);
			_mm_free(actives);
			_mm_free(velocities);
			_mm_free(names);
			_mm_free(simulationStateTransforms);
		}
		void Alloc(uint32_t num)
		{
			objectToWorldTransforms = (XMFLOAT4X4*)_mm_malloc(k_NumObjects * sizeof(XMFLOAT4X4), 64);
			colors = (PackedVector::XMCOLOR*)_mm_malloc(k_NumObjects * sizeof(PackedVector::XMCOLOR), 64);
			boundingSpheres = (BoundingSphere*)_mm_malloc(k_NumObjects * sizeof(BoundingSphere), 64);
			worldToObjectTransforms = (XMFLOAT4X4*)_mm_malloc(k_NumObjects * sizeof(XMFLOAT4X4), 64);
			stateMasks = (uint64_t*)_mm_malloc(k_NumObjects * sizeof(uint64_t), 64);
			actives = (bool*)_mm_malloc(k_NumObjects * sizeof(bool), 64);
			velocities = (XMFLOAT3*)_mm_malloc(k_NumObjects * sizeof(XMFLOAT3), 64);
			names = (char**)_mm_malloc(k_NumObjects * sizeof(char*), 64);
			simulationStateTransforms = (XMFLOAT4X4*)_mm_malloc(k_NumObjects * sizeof(XMFLOAT4X4), 64);
		}
	};

	virtual void Update(double frameTime, float frameDeltaTime) override;
	virtual void Draw() override;


	const DirectX12& m_Dx12;

	GuiRenderer m_GuiRenderer;
	BoundingFrustum m_ViewFrustum;

	ID3D12PipelineState* m_ObjectPso = nullptr;
	ID3D12RootSignature* m_ObjectRs = nullptr;

	ID3D12Resource* m_ObjectVb = nullptr;
	ID3D12Resource* m_ObjectIb = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_ObjectVbView;
	D3D12_INDEX_BUFFER_VIEW m_ObjectIbView;
	uint32_t m_ObjectIndexCount;

	D3D12_GPU_VIRTUAL_ADDRESS m_InstancesDataGpuAddr;

	static const uint32_t k_NumObjectsPerSide = 47;
	static const uint32_t k_NumObjects = k_NumObjectsPerSide*k_NumObjectsPerSide*k_NumObjectsPerSide;
	std::vector<Object> m_ObjectsAos;
	Objects m_ObjectsSoa;
	uint32_t m_NumVisibleObjects;

	bool m_FrustumCullingEnabled = true;
	bool m_UseSoaDataLayout = false;

	float m_CullingTimes[20] = {};

	void CreateBuffers();

	float m_CameraPitch = 0.0f;
	float m_CameraYaw = 0.0f;
	XMFLOAT3 m_CameraPosition = XMFLOAT3(0.0f, 0.0f, -5.0f);

	static const uint32_t k_CacheBufferSize = 16 * 1024 * 1024;
	uint8_t* m_CacheBuffer = nullptr;
};
