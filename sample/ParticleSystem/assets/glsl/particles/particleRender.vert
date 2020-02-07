#version 420
#extension GL_ARB_shader_storage_buffer_object : require


layout( location = 0 ) in int particleId;

struct Particle{
	vec3	pos;
	vec3	ppos;
	vec3	home;
	vec4	color;
	float	damping;
};

out vData{
	vec4	home;
}vertex;

layout( std140, binding = 0 ) buffer Part{
    Particle particles[];
};

uniform mat4 ciModelViewProjection;
uniform mat4 ciModelView;

void main(){
	gl_Position = ciModelView * vec4( particles[particleId].pos, 1 );
	vertex.home = ciModelView * vec4(particles[particleId].home, 1);
}