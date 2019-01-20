#version 330

in vec2 TexCoord;
in vec3 varyingPosition;
in vec3 varyingNormal;
flat in uint texIndex;
in vec3 varyingTint;

uniform sampler2D texture1;
uniform sampler2DArray texarray;
uniform vec3 tint;

out vec4 color;

void main() {
	vec3 diffuseSample = texture(texarray, vec3(TexCoord, texIndex)).xyz;
	
	diffuseSample = diffuseSample * varyingTint;
	
	color = vec4(diffuseSample, 1.0f);
}
