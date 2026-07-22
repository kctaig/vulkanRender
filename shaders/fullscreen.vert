#version 450

// Full-screen triangle using gl_VertexIndex (no vertex buffer needed).
// Call with vkCmdDraw(3, 1, 0, 0) and no bound vertex buffers.

layout(location = 0) out vec2 outTexCoord;

void main() {
    // Generate a full-screen triangle from vertex index:
    // 0 → (-1,-1), 1 → (3,-1), 2 → (-1,3)
    float u = float((gl_VertexIndex & 1) << 2);  // 0 or 4
    float v = float((gl_VertexIndex & 2) << 1);  // 0 or 4
    outTexCoord.x = u * 0.5;
    outTexCoord.y = v * 0.5;
    gl_Position  = vec4(u - 1.0, v - 1.0, 0.0, 1.0);
}
