#version 330
uniform mat4 matrix;
in vec4 position;
out vec2 texCoordf;
void main(){

	gl_Position= matrix*vec4(position.xy,0,1); //gl_ViewMatrix; ((position+vec4(-u_light.x,1024-u_light.y,0,0)) / vec4(1024,512,1,1));
	texCoordf=position.zw;
	
}
