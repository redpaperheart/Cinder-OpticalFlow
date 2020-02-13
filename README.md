# Cinder-OpticalFlow

This cinder block is based on the openFrameworks addon [ofxMIOFlowGLSL](https://github.com/WaltzBinaire/ofxMIOFlowGLSL/) 

The sample project uses the optical flow texture generated by the input camera device to displace particles laid out on a grid.
Lines are drawn to show the directions of the particles from their initial positions. 
![](docs/preview.gif)

### Notes
- A few chages where made to the shader files referenced from ofxMIOFlowGLSL to flip coordinate system and offset values.
- The optical flow texture is used as a sampler2D uniform in the compute shader and the velocity is read via the following function.

```cpp
// returns optical flow velocity based on parfticle position
vec2 lookupOpticalFlowVelocity(vec2 pos) {
	vec2 coord = (pos.xy / uCanvasSize);
	coord.y = 1.0 - coord.y; 
	vec4 col = texture2D(uTexOpticalFlow, coord);
	if (col.w >0.95)  col.z=col.z*-1;
	return vec2(-1*(col.y-col.x),col.z);
}
```

### To Do
- make MacOSx sample. 