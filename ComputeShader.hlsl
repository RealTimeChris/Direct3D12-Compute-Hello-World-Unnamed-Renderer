// ComputeShader.hlsl - A "Hello World" Compute Shader.
// Mar 2020
// Chris M.
// https://github.com/RealTimeChris


struct InlineRootConstants{
	float GlobalTickInRadians;
	int2 PixelDimensions;
};


ConstantBuffer<InlineRootConstants> RootConstants : register(b0);
RWTexture2D<unorm float4> RenderTarget : register(u0);


// Converts the current pixel Id/coords to screen space coords.
// Where, (0.0f < x < 1.0f) and (0.0f < y < 1.0f)
float2 ConvertPixelToScreenSpaceCoords(int2 PixelCoords, int2 PixelDimensions) {
	float2 ScreenSpaceCoords;
	ScreenSpaceCoords.x = (float)PixelCoords.x / ((float)PixelDimensions.x - 1.0f);
	ScreenSpaceCoords.y = (float)PixelCoords.y / ((float)PixelDimensions.y - 1.0f);
	
	return ScreenSpaceCoords;
}


[numthreads(16, 12, 1)]
void ComputeMain(uint3 GlobalThreadId : SV_DispatchThreadID)
{	
	float2 CurrentPixelScreenSpaceCoords;
	CurrentPixelScreenSpaceCoords = ConvertPixelToScreenSpaceCoords(GlobalThreadId, RootConstants.PixelDimensions);
	
	float3 PixelColor;
	PixelColor.x = CurrentPixelScreenSpaceCoords.x * RootConstants.GlobalTickInRadians;
	PixelColor.y = CurrentPixelScreenSpaceCoords.y * RootConstants.GlobalTickInRadians;
	PixelColor.z = 0.2f * RootConstants.GlobalTickInRadians;
	
	RenderTarget[GlobalThreadId.xy].x = PixelColor.x;
	RenderTarget[GlobalThreadId.xy].y = PixelColor.y;
	RenderTarget[GlobalThreadId.xy].z = PixelColor.z;
}
