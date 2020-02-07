#version 150

layout(points) in;
layout(line_strip, max_vertices = 64) out;

uniform mat4 ciProjectionMatrix;

in vData{
	vec4	home;
}vertices[];

void main(){
	// drawing lines 
	for(int i = 0;i < gl_in.length();i++){
		gl_Position = ciProjectionMatrix * (gl_in[i].gl_Position );
		EmitVertex();
		gl_Position = ciProjectionMatrix * vertices[i].home ;
		EmitVertex();
	}
	EndPrimitive();

	// drawing rectangle
	for (int j = 0; j < gl_in.length (); ++j) {
		gl_Position = ciProjectionMatrix* (gl_in [j].gl_Position + vec4 (-05,  05, 0.0, 0.0) );
		EmitVertex   ();
		gl_Position = ciProjectionMatrix* (gl_in [j].gl_Position + vec4 ( 05,  05, 0.0, 0.0) );
		EmitVertex   ();
		gl_Position = ciProjectionMatrix* (gl_in [j].gl_Position + vec4 ( 05, -05, 0.0, 0.0) );
		EmitVertex   ();
		gl_Position = ciProjectionMatrix* (gl_in [j].gl_Position + vec4 (-05, -05, 0.0, 0.0) );
		EmitVertex   ();
		gl_Position = ciProjectionMatrix* (gl_in [j].gl_Position + vec4 (-05,  05, 0.0, 0.0) );
		EmitVertex   ();
		
	  }
	  EndPrimitive ();
}