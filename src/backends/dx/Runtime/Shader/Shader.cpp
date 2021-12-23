#pragma vengine_package vengine_directx
#include <Shader/Shader.h>
#include <d3dcompiler.h>
#include <Runtime/CommandBuffer.h>
#include <Resource/TopAccel.h>
#include <Resource/DefaultBuffer.h>
namespace toolhub::directx {
vstd::optional<Shader::Property> Shader::GetProperty(vstd::string_view str) {
	auto ite = properties.Find(str);
	if (!ite) return {};
	return ite.Value();
}

Shader::Shader(
    std::span<std::pair<vstd::string_view, Property>> prop,
	ID3D12Device* device) {
    properties.reserve(prop.size());
	for (auto&& i : prop) {
        properties.Emplace(i.first, i.second);
	}
	vstd::vector<CD3DX12_ROOT_PARAMETER, VEngine_AllocType::VEngine, 32> allParameter;
	vstd::vector<CD3DX12_DESCRIPTOR_RANGE, VEngine_AllocType::VEngine, 32> allRange;
    for (auto &&kv : prop) {
		auto&& var = kv.second;

		switch (var.type) {
			case ShaderVariableType::UAVDescriptorHeap:
			case ShaderVariableType::SRVDescriptorHeap: {
				allRange.emplace_back();
			} break;
		}
	}
	size_t offset = 0;
    for (auto &&kv : prop) {
		auto&& var = kv.second;

		switch (var.type) {
			case ShaderVariableType::SRVDescriptorHeap: {
				CD3DX12_DESCRIPTOR_RANGE& range = allRange[offset];
				offset++;
				range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, var.arrSize == 0 ? 1 : var.arrSize, var.registerIndex, var.spaceIndex);
				auto&& v = allParameter.emplace_back();
				memset(&v, 0, sizeof(CD3DX12_ROOT_PARAMETER));
				v.InitAsDescriptorTable(1, &range);
			} break;
			case ShaderVariableType::UAVDescriptorHeap: {
				CD3DX12_DESCRIPTOR_RANGE& range = allRange[offset];
				offset++;
				range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, var.arrSize == 0 ? 1 : var.arrSize, var.registerIndex, var.spaceIndex);
				allParameter.emplace_back().InitAsDescriptorTable(1, &range);
			} break;
			case ShaderVariableType::ConstantBuffer:
				allParameter.emplace_back().InitAsConstantBufferView(var.registerIndex, var.spaceIndex);
				break;
			case ShaderVariableType::StructuredBuffer:
				allParameter.emplace_back().InitAsShaderResourceView(var.registerIndex, var.spaceIndex);
				break;
			case ShaderVariableType::RWStructuredBuffer:
				allParameter.emplace_back().InitAsUnorderedAccessView(var.registerIndex, var.spaceIndex);
				break;
			default:
				break;
		}
		kv.second.rootSigPos = allParameter.size() - 1;
	}
	//
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		allParameter.size(), allParameter.data(),
		0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	ThrowIfFailed(D3D12SerializeRootSignature(
		&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));
	ThrowIfFailed(device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(rootSig.GetAddressOf())));
}
bool Shader::SetComputeResource(
	vstd::string_view propertyName,
	CommandBufferBuilder* cb,
	BufferView buffer) {
	auto cmdList = cb->CmdList();
	auto var = GetProperty(propertyName);
	if (!var) return false;
	switch (var->type) {
		case ShaderVariableType::ConstantBuffer: {
			cmdList->SetComputeRootConstantBufferView(
				var->rootSigPos,
				buffer.buffer->GetAddress() + buffer.offset);
		} break;
		case ShaderVariableType::StructuredBuffer: {
			cmdList->SetComputeRootShaderResourceView(
				var->rootSigPos,
				buffer.buffer->GetAddress() + buffer.offset);
		} break;
		case ShaderVariableType::RWStructuredBuffer: {
			cmdList->SetComputeRootUnorderedAccessView(
				var->rootSigPos,
				buffer.buffer->GetAddress() + buffer.offset);
		} break;
		default:
			return false;
	}
	return true;
}
bool Shader::SetComputeResource(
	vstd::string_view propertyName,
	CommandBufferBuilder* cb,
	DescriptorHeapView view) {
	auto cmdList = cb->CmdList();
	auto var = GetProperty(propertyName);
	if (!var) return false;
	switch (var->type) {
		case ShaderVariableType::UAVDescriptorHeap:
		case ShaderVariableType::SRVDescriptorHeap: {
			cb->SetDescHeap(view.heap);
			cmdList->SetComputeRootDescriptorTable(
				var->rootSigPos,
				view.heap->hGPU(view.index));
		} break;
		default:
			return false;
	}
	return true;
}
bool Shader::SetComputeResource(
	vstd::string_view propertyName, 
	CommandBufferBuilder* cmdList, 
	TopAccel const* bAccel) {
	return SetComputeResource(
		propertyName,
		cmdList,
		BufferView(bAccel->GetAccelBuffer()));
}
}// namespace toolhub::directx