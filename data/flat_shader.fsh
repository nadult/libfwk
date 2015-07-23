#version 100
#extension GL_OES_standard_derivatives : enable

varying lowp vec4 color;
varying mediump vec3 tpos;

void main() {
	mediump vec3 normal = normalize(cross(dFdx(tpos), dFdy(tpos)));
	mediump float normal_shade = abs(dot(normal, vec3(0, 0, 1))) * 0.5 + 0.5;
	gl_FragColor = color * normal_shade;
}
