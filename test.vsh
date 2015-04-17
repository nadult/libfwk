uniform mat4 modelViewMatrix, projectionMatrix;

attribute vec3 vertex;

void main()
{	
	gl_Position = projectionMatrix * modelViewMatrix * vec4(vertex, 1.0);
} 

