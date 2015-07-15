#version 100

varying lowp vec4 color;
varying mediump vec3 tpos;

void main() {
	mediump vec3 normal = normalize(cross(dFdx(tpos), dFdy(tpos)));
	gl_FragColor = color * vec4(normal, 1.0);
}
