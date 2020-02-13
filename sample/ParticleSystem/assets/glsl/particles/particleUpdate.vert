/*

	particleUpdate.vert
		- Shuvashis Das | Red Paper Heart Studio

*/

#version 150 core


//uniforms 
uniform sampler2D			uTexOpticalFlow;
uniform vec2 				uCanvasSize;
uniform float				uForceMult;

in vec3						iPosition;
in vec3						iPPostion;
in vec3						iInitPos;
in float					iDamping;
in vec4						iColor;

out vec3					position;
out vec3					pposition;
out vec3					initPos;
out float					damping;
out vec4					color;

vec2						fluidVel;
const float					dt2 = 1.0 / (10.0 * 10.0);

// returns optical flow velocity based on parfticle position
vec2 lookupOpticalFlowVelocity(vec2 pos) {
	vec2 coord = (pos.xy / uCanvasSize);
	// flipping y coordinate
	coord.y = 1.0 - coord.y; 
	vec4 col = texture(uTexOpticalFlow, coord);
	if (col.w >0.95)  col.z=col.z*-1;
	return vec2(-1*(col.y-col.x),col.z);
}


void main()
{
	position		= iPosition;
	pposition		= iPPostion;
	damping			= iDamping;
	initPos			= iInitPos;
	color			= iColor;

	// adding optical flow vel
	fluidVel = lookupOpticalFlowVelocity( position.xy );
	position.xy += fluidVel * uForceMult;

	vec3 vel = (position - pposition) * damping;
	pposition = position;
	vec3 acc = (initPos - position);
	position += vel + acc * dt2;
}