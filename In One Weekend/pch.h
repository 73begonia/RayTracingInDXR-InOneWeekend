#pragma once

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include <assert.h>
#include <Windows.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <time.h>
#include <memory>
#include <wrl.h>

#include "d3dx12.h"

#include "basic_math.h"

using namespace std;
using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;