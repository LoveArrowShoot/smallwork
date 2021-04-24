#include "d3dUtil.h"

using namespace::DirectX;
HRESULT CreateTexture2DArrayFromFile(
	ID3D11Device* d3dDevice,
	ID3D11DeviceContext* d3dDeviceContext,
	const std::vector<std::wstring>& fileNames,
	ID3D11Texture2D** textureArray,
	ID3D11ShaderResourceView** textureArrayView,
	bool generateMips)
{
	// 检查设备、文件名数组是否非空
	if (!d3dDevice || fileNames.empty())
		return E_INVALIDARG;

	HRESULT hr;
	UINT arraySize = (UINT)fileNames.size();

	// ******************
	// 读取第一个纹理
	//
	ID3D11Texture2D* pTexture;
	D3D11_TEXTURE2D_DESC texDesc;

	hr = CreateDDSTextureFromFileEx(d3dDevice,
		fileNames[0].c_str(), 0, D3D11_USAGE_STAGING, 0,
		D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ,
		0, false, reinterpret_cast<ID3D11Resource**>(&pTexture), nullptr);
	if (FAILED(hr))
	{
		hr = CreateWICTextureFromFileEx(d3dDevice,
			fileNames[0].c_str(), 0, D3D11_USAGE_STAGING, 0,
			D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ,
			0, false, reinterpret_cast<ID3D11Resource**>(&pTexture), nullptr);
	}

	if (FAILED(hr))
		return hr;

	// 读取创建好的纹理信息
	pTexture->GetDesc(&texDesc);

	// ******************
	// 创建纹理数组
	//
	D3D11_TEXTURE2D_DESC texArrayDesc;
	texArrayDesc.Width = texDesc.Width;
	texArrayDesc.Height = texDesc.Height;
	texArrayDesc.MipLevels = generateMips ? 0 : texDesc.MipLevels;
	texArrayDesc.ArraySize = arraySize;
	texArrayDesc.Format = texDesc.Format;
	texArrayDesc.SampleDesc.Count = 1;		// 不能使用多重采样
	texArrayDesc.SampleDesc.Quality = 0;
	texArrayDesc.Usage = D3D11_USAGE_DEFAULT;
	texArrayDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (generateMips ? D3D11_BIND_RENDER_TARGET : 0);
	texArrayDesc.CPUAccessFlags = 0;
	texArrayDesc.MiscFlags = (generateMips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0);

	ID3D11Texture2D* pTexArray = nullptr;
	hr = d3dDevice->CreateTexture2D(&texArrayDesc, nullptr, &pTexArray);
	if (FAILED(hr))
	{
		SAFE_RELEASE(pTexture);
		return hr;
	}

	// 获取实际创建的纹理数组信息
	pTexArray->GetDesc(&texArrayDesc);
	UINT updateMipLevels = generateMips ? 1 : texArrayDesc.MipLevels;

	// 写入到纹理数组第一个元素
	D3D11_MAPPED_SUBRESOURCE mappedTex2D;
	for (UINT i = 0; i < updateMipLevels; ++i)
	{
		d3dDeviceContext->Map(pTexture, i, D3D11_MAP_READ, 0, &mappedTex2D);
		d3dDeviceContext->UpdateSubresource(pTexArray, i, nullptr,
			mappedTex2D.pData, mappedTex2D.RowPitch, mappedTex2D.DepthPitch);
		d3dDeviceContext->Unmap(pTexture, i);
	}
	SAFE_RELEASE(pTexture);

	// ******************
	// 读取剩余的纹理并加载入纹理数组
	//
	D3D11_TEXTURE2D_DESC currTexDesc;
	for (UINT i = 1; i < texArrayDesc.ArraySize; ++i)
	{
		hr = CreateDDSTextureFromFileEx(d3dDevice,
			fileNames[0].c_str(), 0, D3D11_USAGE_STAGING, 0,
			D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ,
			0, false, reinterpret_cast<ID3D11Resource**>(&pTexture), nullptr);
		if (FAILED(hr))
		{
			hr = CreateWICTextureFromFileEx(d3dDevice,
				fileNames[0].c_str(), 0, D3D11_USAGE_STAGING, 0,
				D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ,
				0, WIC_LOADER_DEFAULT, reinterpret_cast<ID3D11Resource**>(&pTexture), nullptr);
		}

		if (FAILED(hr))
		{
			SAFE_RELEASE(pTexArray);
			return hr;
		}

		pTexture->GetDesc(&currTexDesc);
		// 需要检验所有纹理的mipLevels，宽度和高度，数据格式是否一致，
		// 若存在数据格式不一致的情况，请使用dxtex.exe(DirectX Texture Tool)
		// 将所有的图片转成一致的数据格式
		if (currTexDesc.MipLevels != texDesc.MipLevels || currTexDesc.Width != texDesc.Width ||
			currTexDesc.Height != texDesc.Height || currTexDesc.Format != texDesc.Format)
		{
			SAFE_RELEASE(pTexArray);
			SAFE_RELEASE(pTexture);
			return E_FAIL;
		}
		// 写入到纹理数组的对应元素
		for (UINT j = 0; j < updateMipLevels; ++j)
		{
			// 允许映射索引i纹理中，索引j的mipmap等级的2D纹理
			d3dDeviceContext->Map(pTexture, j, D3D11_MAP_READ, 0, &mappedTex2D);
			d3dDeviceContext->UpdateSubresource(pTexArray,
				D3D11CalcSubresource(j, i, texArrayDesc.MipLevels),	// i * mipLevel + j
				nullptr, mappedTex2D.pData, mappedTex2D.RowPitch, mappedTex2D.DepthPitch);
			// 停止映射
			d3dDeviceContext->Unmap(pTexture, j);
		}
		SAFE_RELEASE(pTexture);
	}
	// ******************
	// 必要时创建纹理数组的SRV
	//
	if (generateMips || textureArrayView)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = texArrayDesc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.MostDetailedMip = 0;
		viewDesc.Texture2DArray.MipLevels = -1;
		viewDesc.Texture2DArray.FirstArraySlice = 0;
		viewDesc.Texture2DArray.ArraySize = arraySize;

		ID3D11ShaderResourceView* pTexArraySRV;
		hr = d3dDevice->CreateShaderResourceView(pTexArray, &viewDesc, &pTexArraySRV);
		if (FAILED(hr))
		{
			SAFE_RELEASE(pTexArray);
			return hr;
		}

		// 生成mipmaps
		if (generateMips)
		{
			d3dDeviceContext->GenerateMips(pTexArraySRV);
		}

		if (textureArrayView)
			*textureArrayView = pTexArraySRV;
		else
			SAFE_RELEASE(pTexArraySRV);
	}

	if (textureArray)
		*textureArray = pTexArray;
	else
		SAFE_RELEASE(pTexArray);

	return S_OK;
}


HRESULT CreateShaderFromFile(
	const WCHAR* csoFileNameInOut,
	const WCHAR* hlslFileName,
	LPCSTR entryPoint,
	LPCSTR shaderModel,
	ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	// 寻找是否有已经编译好的顶点着色器
	if (csoFileNameInOut && D3DReadFileToBlob(csoFileNameInOut, ppBlobOut) == S_OK)
	{
		return hr;
	}
	else
	{
		DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
		// 设置 D3DCOMPILE_DEBUG 标志用于获取着色器调试信息。该标志可以提升调试体验，
		// 但仍然允许着色器进行优化操作
		dwShaderFlags |= D3DCOMPILE_DEBUG;

		// 在Debug环境下禁用优化以避免出现一些不合理的情况
		dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
		ID3DBlob* errorBlob = nullptr;
		hr = D3DCompileFromFile(hlslFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, shaderModel,
			dwShaderFlags, 0, ppBlobOut, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob != nullptr)
			{
				OutputDebugStringA(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
			}
			SAFE_RELEASE(errorBlob);
			return hr;
		}

		// 若指定了输出文件名，则将着色器二进制信息输出
		if (csoFileNameInOut)
		{
			return D3DWriteBlobToFile(*ppBlobOut, csoFileNameInOut, FALSE);
		}
	}

	return hr;
}
