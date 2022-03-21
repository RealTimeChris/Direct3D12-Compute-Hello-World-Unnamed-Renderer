// Unnamed Renderer.cpp - Hobby renderer, no idea where it's going.
// Mar 2020
// Chris M.
// https://github.com/RealTimeChris

#include "WinRTStuff.hpp"
#include "DirectXStuff.hpp"

int __stdcall wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
	
	// General usage values.
	const unsigned __int64 PixelWidth{ 1920u }, PixelHeight{ 1080u }, PixelDepth{ 1u };
	const unsigned __int64 FinalFrameCount{ 120u };
	const DXGI_FORMAT FinalFrameFormat{ DXGI_FORMAT_R8G8B8A8_UNORM };
	HRESULT Result{ S_OK };





	// 32-bit Root Constants to be passed from Host to Device/Shader.
	struct InlineRootConstants{
		float GlobalTickInRadians;
		int PixelDimensions[2];
	};

	InlineRootConstants InlineRootConstants{};
	InlineRootConstants.PixelDimensions[0] = PixelWidth;
	InlineRootConstants.PixelDimensions[1] = PixelHeight;





	// Worker group dimension checking. (Don't forget to set these values in the Compute Shader.)
	UINT ThreadsPerGroupX{ 16u }, ThreadsPerGroupY{ 12u }, ThreadsPerGroupZ{ 1u };
	UINT GroupCountX{}, GroupCountY{}, GroupCountZ{};

	if ((PixelWidth % ThreadsPerGroupX != 0) || (PixelHeight % ThreadsPerGroupY != 0) || (PixelDepth % ThreadsPerGroupZ != 0))
	{
		MessageBoxW(NULL, L"Invalid worker group dimensions for the given output resolution.", L"Workload Mapping Error", NULL);

		exit(-1);
	}
	else
	{
		GroupCountX = PixelWidth / ThreadsPerGroupX;
		GroupCountY = PixelHeight / ThreadsPerGroupY;
		GroupCountZ = PixelDepth / ThreadsPerGroupZ;
	}





	// Unique Direct3D 12 interfaces.
	DirectXStuff::D3D12DebugController D3DDebugger{};
	DirectXStuff::Device Device{};
	DirectXStuff::CommandQueue CommandQueue{ Device.GetInterface(), D3D12_COMMAND_QUEUE_PRIORITY_HIGH };
	




	// A single Fence.
	DirectXStuff::Fence Fence{ Device.GetInterface(), L"MainFence" };
	




	// Final frame 2D textures, bound via unordered access, along with their resource barriers.
	DirectXStuff::UATexture2DConfig FinalFrameConfig{};
	FinalFrameConfig.TextureFormat = FinalFrameFormat;
	FinalFrameConfig.TextureWidth = PixelWidth;
	FinalFrameConfig.TextureHeight = PixelHeight;
	FinalFrameConfig.InitialResourceState = D3D12_RESOURCE_STATE_COPY_SOURCE;

	DirectXStuff::UATexture2D* FinalFrames[FinalFrameCount]{ nullptr };

	for (__int64 i{ 0 }; i < FinalFrameCount; i++) {
		FinalFrames[i] = new DirectXStuff::UATexture2D{ Device.GetInterface(), FinalFrameConfig, L"FinalFrameUATex2D" };
	}

	D3D12_RESOURCE_BARRIER FinalFrameCopySourceToUnorderedAccessBarriers[FinalFrameCount]{};

	for (__int64 i{ 0u }; i < FinalFrameCount; i++) {
		FinalFrameCopySourceToUnorderedAccessBarriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		FinalFrameCopySourceToUnorderedAccessBarriers[i].Transition.pResource = FinalFrames[i]->GetInterface();
		FinalFrameCopySourceToUnorderedAccessBarriers[i].Transition.Subresource = 0u;
		FinalFrameCopySourceToUnorderedAccessBarriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		FinalFrameCopySourceToUnorderedAccessBarriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	D3D12_RESOURCE_BARRIER FinalFrameUnorderedAccessToCopySourceBarriers[FinalFrameCount]{};

	for (__int64 i{ 0 }; i < FinalFrameCount; i++) {
		FinalFrameUnorderedAccessToCopySourceBarriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		FinalFrameUnorderedAccessToCopySourceBarriers[i].Transition.pResource = FinalFrames[i]->GetInterface();
		FinalFrameUnorderedAccessToCopySourceBarriers[i].Transition.Subresource = 0u;
		FinalFrameUnorderedAccessToCopySourceBarriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		FinalFrameUnorderedAccessToCopySourceBarriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	}
	




	// Descriptor heap for the Final Frame UAVs, along with their creation.
	DirectXStuff::DescriptorHeap FinalFramesDescriptorHeap{ Device.GetInterface(), FinalFrameCount, L"FinalFrameDescriptorHeap" };
	const unsigned __int64 DescriptorCount{ FinalFrameCount };
	const unsigned __int64 DescriptorHeapCount{ 1u };
	const unsigned __int64 DescriptorHandleIncrementSize{
		Device.GetInterface()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
	
	D3D12_UNORDERED_ACCESS_VIEW_DESC FinalFrameUAVDescription{};
	FinalFrameUAVDescription.Format = FinalFrameFormat;
	FinalFrameUAVDescription.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	FinalFrameUAVDescription.Texture2D.MipSlice = 0u;
	FinalFrameUAVDescription.Texture2D.PlaneSlice = 0u;

	D3D12_CPU_DESCRIPTOR_HANDLE FinalFrameUAVCPUDescriptorHandles[FinalFrameCount]{};
	D3D12_GPU_DESCRIPTOR_HANDLE FinalFrameUAVGPUDescriptorHandles[FinalFrameCount]{};

	for (__int64 i{ 0 }; i < FinalFrameCount; i++) {
		FinalFrameUAVCPUDescriptorHandles[i].ptr =
			FinalFramesDescriptorHeap.GetInterface()->GetCPUDescriptorHandleForHeapStart().ptr +
			(i * DescriptorHandleIncrementSize);
			
		FinalFrameUAVGPUDescriptorHandles[i].ptr =
			FinalFramesDescriptorHeap.GetInterface()->GetGPUDescriptorHandleForHeapStart().ptr +
			(i * DescriptorHandleIncrementSize);
		
		Device.GetInterface()->CreateUnorderedAccessView(FinalFrames[i]->GetInterface(), nullptr, &FinalFrameUAVDescription,
			FinalFrameUAVCPUDescriptorHandles[i]);
	}





	// Global Root Signature.
	DirectXStuff::Blob ComputeRootSignatureBlob{}, ComputeRootSignatureErrorBlob{};
	DirectXStuff::RootSignature ComputeRootSignature{ Device.GetInterface(), ComputeRootSignatureBlob.GetInterface(),
	ComputeRootSignatureErrorBlob.GetInterface() };





	// Shader construction.
	DirectXStuff::DXCLibrary DXCLibrary{};
	DirectXStuff::DXCCompiler DXCCompiler{};

	DirectXStuff::ShaderConfig ComputeShaderConfig{};
	ComputeShaderConfig.ShaderEntryPoint = L"ComputeMain";
	ComputeShaderConfig.ShaderFileName = L"ComputeShader.hlsl";

	DirectXStuff::Shader ComputeShader{ DXCLibrary.GetInterface(), DXCCompiler.GetInterface(), ComputeShaderConfig };





	// Pipeline State.
	DirectXStuff::PipelineState ComputePipelineState{ Device.GetInterface(), ComputeRootSignature.GetInterface(),
	ComputeShader.GetShaderByteCodeSize(), ComputeShader.GetShaderByteCode(), L"ComputePipelineState" };





	// Allocators and Graphics Command Lists for the Rendering work, along with the Rendering work.
	DirectXStuff::CommandAllocator* RenderingCommandAllocators[FinalFrameCount]{ nullptr };

	for (__int64 i{}; i < FinalFrameCount; i++) {
		RenderingCommandAllocators[i] = new DirectXStuff::CommandAllocator{ Device.GetInterface(), L"RenderingCommandAllocator" };
	}

	DirectXStuff::GraphicsCommandList* RenderCommandLists[FinalFrameCount]{ nullptr };

	for (__int64 i{ 0 }; i < FinalFrameCount; i++) {
		RenderCommandLists[i] = new DirectXStuff::GraphicsCommandList{ Device.GetInterface(),
			RenderingCommandAllocators[i]->GetInterface(), L"RenderCommandList" };
	}

	for (__int64 i{ 0 }; i < FinalFrameCount; i++) {
		RenderCommandLists[i]->GetInterface()->Reset(RenderingCommandAllocators[i]->GetInterface(),
			ComputePipelineState.GetInterface());

		RenderCommandLists[i]->GetInterface()->SetComputeRootSignature(ComputeRootSignature.GetInterface());

		auto pDescriptorHeap = FinalFramesDescriptorHeap.GetInterface();	// "lvalue"... What lol?
		RenderCommandLists[i]->GetInterface()->SetDescriptorHeaps(DescriptorHeapCount, &pDescriptorHeap);

		RenderCommandLists[i]->GetInterface()->SetComputeRootDescriptorTable(0u, FinalFrameUAVGPUDescriptorHandles[i]);

		InlineRootConstants.GlobalTickInRadians = (static_cast<float>(i) / static_cast<float>(FinalFrameCount)) * 2.0f;
		RenderCommandLists[i]->GetInterface()->SetComputeRoot32BitConstants(1u, sizeof(InlineRootConstants) / sizeof(float),
			&InlineRootConstants, 0u);

		RenderCommandLists[i]->GetInterface()->SetPipelineState(ComputePipelineState.GetInterface());

		RenderCommandLists[i]->GetInterface()->ResourceBarrier(1u, &FinalFrameCopySourceToUnorderedAccessBarriers[i]);

		RenderCommandLists[i]->GetInterface()->Dispatch(GroupCountX, GroupCountY, GroupCountZ);

		RenderCommandLists[i]->GetInterface()->ResourceBarrier(1u, &FinalFrameUnorderedAccessToCopySourceBarriers[i]);

		RenderCommandLists[i]->GetInterface()->Close();
	}

	



	// Prep for Presentation.
	WinRTStuff::WindowClass TheatreWindowClass{};
	WinRTStuff::RenderWindow TheatreWindow{ TheatreWindowClass.ReportClassName(), PixelWidth, PixelHeight, L"Unnamed Renderer" };
	DirectXStuff::Factory Factory{};
	const unsigned __int64 SwapChainBufferCount{ 2u };
	DirectXStuff::SwapChain SwapChain{ Factory.GetInterface(), CommandQueue.GetInterface(),
		TheatreWindow.ReportWindowHandle(), PixelWidth, PixelHeight, SwapChainBufferCount, FinalFrameFormat };

	const unsigned __int64 SwapChainSyncInterval{ 1u };
	unsigned __int64 CurrentBackBufferIndex{ 0u };

	ID3D12Resource* SwapChainBackBuffers[SwapChainBufferCount]{ nullptr };
	
	CurrentBackBufferIndex = SwapChain.GetInterface()->GetCurrentBackBufferIndex();

	Result = SwapChain.GetInterface()->GetBuffer(static_cast<UINT>(CurrentBackBufferIndex), __uuidof(ID3D12Resource),
		reinterpret_cast<void**>(&SwapChainBackBuffers[CurrentBackBufferIndex]));
	DirectXStuff::ResultCheck(Result, L"GetBuffer() failed.", L"SwapChain Prep Error");

	Result = SwapChainBackBuffers[CurrentBackBufferIndex]->SetName(L"SwapChainBackBuffer00");
	DirectXStuff::ResultCheck(Result, L"SetName() failed.", L"SwapChain Prep Error");

	Result = SwapChain.GetInterface()->Present(SwapChainSyncInterval, NULL);
	DirectXStuff::ResultCheck(Result, L"Present() failed.", L"SwapChain Prep Error");

	Fence.FlushCommandQueue(CommandQueue.GetInterface());

	CurrentBackBufferIndex = SwapChain.GetInterface()->GetCurrentBackBufferIndex();

	Result = SwapChain.GetInterface()->GetBuffer(static_cast<UINT>(CurrentBackBufferIndex), __uuidof(ID3D12Resource),
		reinterpret_cast<void**>(&SwapChainBackBuffers[CurrentBackBufferIndex]));
	DirectXStuff::ResultCheck(Result, L"GetBuffer() failed.", L"SwapChain Prep Error");

	Result = SwapChainBackBuffers[CurrentBackBufferIndex]->SetName(L"SwapChainBackBuffer01");
	DirectXStuff::ResultCheck(Result, L"SetName() failed.", L"SwapChain Prep Error");

	Result = SwapChain.GetInterface()->Present(SwapChainSyncInterval, NULL);
	DirectXStuff::ResultCheck(Result, L"Present() failed.", L"SwapChain Prep Error");

	Fence.FlushCommandQueue(CommandQueue.GetInterface());
	
	D3D12_RESOURCE_BARRIER SwapChainBufferPresentToCopyDestBarriers[SwapChainBufferCount]{};

	for (__int64 i{ 0 }; i < SwapChainBufferCount; i++) {
		SwapChainBufferPresentToCopyDestBarriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		SwapChainBufferPresentToCopyDestBarriers[i].Transition.pResource = SwapChainBackBuffers[i];
		SwapChainBufferPresentToCopyDestBarriers[i].Transition.Subresource = 0u;
		SwapChainBufferPresentToCopyDestBarriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		SwapChainBufferPresentToCopyDestBarriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	}

	D3D12_RESOURCE_BARRIER SwapChainBufferCopyDestToPresentBarriers[SwapChainBufferCount]{};

	for (__int64 i{ 0 }; i < SwapChainBufferCount; i++) {
		SwapChainBufferCopyDestToPresentBarriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		SwapChainBufferCopyDestToPresentBarriers[i].Transition.pResource = SwapChainBackBuffers[i];
		SwapChainBufferCopyDestToPresentBarriers[i].Transition.Subresource = 0u;
		SwapChainBufferCopyDestToPresentBarriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		SwapChainBufferCopyDestToPresentBarriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	}
	




	// Allocators and Graphics Command Lists for the Presenting work, along with recording the Presenting work.
	DirectXStuff::CommandAllocator* PresentCommandAllocators[FinalFrameCount]{ nullptr };

	for (__int64 i{ 0 }; i < FinalFrameCount; i++) {
		PresentCommandAllocators[i] = new DirectXStuff::CommandAllocator{ Device.GetInterface(), L"PresentCommandAllocator" };
	}

	DirectXStuff::GraphicsCommandList* PresentCommandLists[FinalFrameCount]{ nullptr };

	for (__int64 i{ 0 }; i < FinalFrameCount; i++) {
		PresentCommandLists[i] = new DirectXStuff::GraphicsCommandList{ Device.GetInterface(),
			PresentCommandAllocators[i]->GetInterface(), L"PresentCommandList" };
	}

	for (__int64 i{ 0 }; i < FinalFrameCount; i++) {
		// Map Even FinalFrames to SwapChain backbuffer 0, and Odd FinalFrames to backbuffer 1.
		if (i % 2 == 0) {
			PresentCommandLists[i]->GetInterface()->Reset(PresentCommandAllocators[i]->GetInterface(), nullptr);

			PresentCommandLists[i]->GetInterface()->ResourceBarrier(1u, &SwapChainBufferPresentToCopyDestBarriers[0]);

			PresentCommandLists[i]->GetInterface()->CopyResource(SwapChainBackBuffers[0], FinalFrames[i]->GetInterface());

			PresentCommandLists[i]->GetInterface()->ResourceBarrier(1u, &SwapChainBufferCopyDestToPresentBarriers[0]);

			PresentCommandLists[i]->GetInterface()->Close();
		}
		else {
			PresentCommandLists[i]->GetInterface()->Reset(PresentCommandAllocators[i]->GetInterface(), nullptr);

			PresentCommandLists[i]->GetInterface()->ResourceBarrier(1u, &SwapChainBufferPresentToCopyDestBarriers[1]);

			PresentCommandLists[i]->GetInterface()->CopyResource(SwapChainBackBuffers[1], FinalFrames[i]->GetInterface());

			PresentCommandLists[i]->GetInterface()->ResourceBarrier(1u, &SwapChainBufferCopyDestToPresentBarriers[1]);

			PresentCommandLists[i]->GetInterface()->Close();
		}
	}





	// Application event loop.
	const unsigned __int32 MessageFilterMin{ 0u }, MessageFilterMax{ 0u };
	MSG MessageStruct{};
	unsigned __int64 CurrentPresentIndex{ 0u };

	MessageBoxW(TheatreWindow.ReportWindowHandle(), L"Execute Rendering Work: Left Mouse Button", L"Control Message", NULL);

	while (true == true) {

		while (PeekMessageW(&MessageStruct, NULL, MessageFilterMin, MessageFilterMax, PM_REMOVE)) {
			TranslateMessage(&MessageStruct);
			DispatchMessageW(&MessageStruct);
		}
		
		if (IsWindowVisible(TheatreWindow.ReportWindowHandle()) == false) {
			break;
		}

		// Rendering Logic.
		if (MessageStruct.message == WM_LBUTTONUP) {
			for (__int64 i{ 0 }; i < FinalFrameCount; i++) {
				auto pRenderCommandList = RenderCommandLists[i]->GetListForSubmission();
				CommandQueue.GetInterface()->ExecuteCommandLists(1u, &pRenderCommandList);

				Fence.FlushCommandQueue(CommandQueue.GetInterface());
			}
		}

		// Present Logic.
		auto pPresentCommandList = PresentCommandLists[CurrentPresentIndex]->GetListForSubmission();
		CommandQueue.GetInterface()->ExecuteCommandLists(1u, &pPresentCommandList);

		SwapChain.GetInterface()->Present(1u, NULL);

		Fence.FlushCommandQueue(CommandQueue.GetInterface());

		CurrentPresentIndex++;

		if (CurrentPresentIndex == FinalFrameCount) {
			CurrentPresentIndex = 0u;
		}

	}
	




	// DXGI Debugger to report the debug info.
	DirectXStuff::DXGIDebugController DXGIDebugger{};
	DXGIDebugger.ReportDebugInfo();





	// Test message.
	if (DEBUG_ENABLED) {
		MessageBoxW(NULL, L"Success!", L"Debug Mode", NULL);
	}
	




	// Cleanup.
	for (__int64 i{ SwapChainBufferCount - 1 }; i >= 0; i--) {
		if (SwapChainBackBuffers[i] != nullptr) {
			SwapChainBackBuffers[i]->Release();
			SwapChainBackBuffers[i] = nullptr;
		}
	}
	
	for (__int64 i{ FinalFrameCount - 1 }; i >= 0; i--) {
		delete PresentCommandLists[i];
		PresentCommandLists[i] = nullptr;
	}
	
	for (__int64 i{ FinalFrameCount - 1 }; i >= 0; i--) {
		delete RenderCommandLists[i];
		RenderCommandLists[i] = nullptr;
	}

	for (__int64 i{ FinalFrameCount - 1 }; i >= 0; i--) {
		delete FinalFrames[i];
		FinalFrames[i] = nullptr;
	}

	for (__int64 i{ FinalFrameCount - 1 }; i >= 0; i--) {
		delete PresentCommandAllocators[i];
		PresentCommandAllocators[i] = nullptr;
	}

	for (__int64 i{ FinalFrameCount - 1 }; i >= 0; i--) {
		delete RenderingCommandAllocators[i];
		RenderingCommandAllocators[i] = nullptr;
	}





	// Exit.
	return 0;
}
