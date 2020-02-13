#version 420
#extension GL_ARB_shader_storage_buffer_object : require


//layout( location = 0 ) in int particleId;

in vec3					position;
in vec3					pposition;
in vec3					initPos;
in float				damping;
in vec4					color;

//struct Particle{
//	vec3	pos;
//	vec3	ppos;
//	vec3	initPos;
//	vec4	color;
//	float	damping;
//};

out vData{
	vec4	position;
	vec4	initPos;
	//vec4	color;
}vertex;

//layout( std140, binding = 0 ) buffer Part{
//    Particle particles[];
//};

uniform mat4 ciModelViewProjection;
uniform mat4 ciModelView;

void main(){
	//gl_Position = ciModelView * vec4( particles[particleId].pos, 1 );
	//vertex.initPos = ciModelView * vec4(particles[particleId].initPos, 1);
	//gl_Position =  ciModelView* vec4( position, 1 );
	vertex.initPos = ciModelView * vec4(initPos, 1);
	vertex.position = ciModelView * vec4(position, 1);
	//vertex.color = color;
}