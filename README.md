# Unnamed-Renderer-Hello-World
First iteration of a work-in-progress hobby renderer, using Direct3D 12 Compute.

Currently, there is a single compute pipeline state with a single bound compute shader.

The compute shader converts the worker thread Ids into screen space values where; 0.0f <= x <= 1.0f and 0.0f <= y <= 1.0f, and these values are used to select the color channel values for each pixel.

The pixel values then have their intensity defined by the "GlobalTickInRadians" value that is passed from host to device as an inline root constant. This value is updated once per frame, where; GlobalTickInRadians = (CurrentFinalFrameIndex / FinalFrameCount) * 2.0f

Don't forget to grab the dxil.dll and dxccompiler.dll files to be placed with the project source before building/running. They can be found in "C:\Program Files (x86)\Windows Kits\10\Redist\D3D\x64".

## Link to Video:
[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/Qv-gqeewmew/0.jpg)](https://www.youtube.com/watch?v=Qv-gqeewmew)
