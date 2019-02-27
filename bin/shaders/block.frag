#version 330

in vec2 TexCoord;
flat in uint texIndex;
in vec3 varyingTint;

uniform sampler2DArray texarray;
uniform vec3 tint;

out vec4 color;

void main() {
	vec4 sample = texture(texarray, vec3(TexCoord, texIndex));
	
	if (sample.a <= 0.6) {
		discard;
	}
	
	vec3 diffuseSample = sample.xyz;
	
	diffuseSample = diffuseSample * varyingTint;
	
	color = vec4(diffuseSample, 1.0);
}
