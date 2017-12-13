#pragma once

#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <numeric>

#include <immintrin.h>

#define WIN32_LEAN_AND_MEAN 1
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_4.h>

#include <DirectXMath.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <DirectXPackedVector.h>
using namespace DirectX;

#include "ittnotify.h"

#include "External/imgui/imgui.h"

#define USE_PIX
#pragma comment(lib, "WinPixEventRuntime.lib")
#include "External/WinPixEventRuntime/pix3.h"
