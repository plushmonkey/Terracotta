#version 330

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in uint inTexIndex;
layout (location = 3) in vec3 tint;
layout (location = 4) in uint ambientOcclusion;

out vec2 TexCoord;
flat out uint texIndex;
out vec3 varyingTint;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
	vec4 worldPos = model * vec4(position, 1.0f);
	vec4 viewPos = view * worldPos;

	gl_Position = projection * viewPos;

	TexCoord = texCoord;
	texIndex = inTexIndex;
	varyingTint = tint * (0.55 + float(ambientOcclusion) * 0.15);
}
