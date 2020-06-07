#version 410 core 
uniform sampler2DRect tex;
//layout(location = 0) out vec3 color;
uniform uint palette[16];
uniform uvec2 shift;
uniform uvec2 size;
uniform uint gif;
in vec2 texCoordf;
void main(){
	
	float light=.1;
	
	vec4 texcol=texture(tex, uvec2(texCoordf +shift)%size );
 	
	gl_FragColor=texcol*10;
	
	
	int colindex =int((texcol.r*256));
	if (colindex==0)
		gl_FragColor=vec4(0);
	else{
		uint col=palette[colindex];
		if(gif>0){
			gl_FragColor=vec4(texcol.rrr,1);
	    }else{
			gl_FragColor=vec4(((col>>0) &255)  /256.f  ,float((col>>8) & 255) /256.f,((col>>16) &255) /256.f,1  );
		}
	}
}


