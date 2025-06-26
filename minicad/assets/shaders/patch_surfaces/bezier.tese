#version 430 core

layout (isolines, equal_spacing) in;

uniform mat4 u_pvMat;
uniform bool u_horizontal;

patch in float isolinesCount;
patch in float subdivisions;
patch in int textureId;

patch in vec2 patchesDim;
patch in int patchId;

out vec3 texCoords;

vec3 find_bezier_degree_3(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t) {
    float u = (1.0 - t);
    float b3 = t*t*t;
    float b2 = 3.0*t*t*u;
    float b1 = 3.0*t*u*u;
    float b0 = u*u*u;
    return p0*b0 + p1*b1 + p2*b2 + p3*b3;
}

vec3 find_bezier_surface_degree_3(float u, float v) 
{ 
    vec3 pu[4]; 
    for (int i = 0; i < 4; ++i) { 
       vec3 p0 = gl_in[i*4].gl_Position.xyz;
       vec3 p1 = gl_in[i*4 + 1].gl_Position.xyz;
       vec3 p2 = gl_in[i*4 + 2].gl_Position.xyz;
       vec3 p3 = gl_in[i*4 + 3].gl_Position.xyz;

       pu[i] = find_bezier_degree_3(p0, p1, p2, p3, u); 
    } 
    return find_bezier_degree_3(pu[0], pu[1], pu[2], pu[3], v); 
}

vec2 find_global_uv(vec2 local_uv, int patch_index, ivec2 dim) {
    int coord_x = patch_index % dim.x;
    int coord_y = patch_index / dim.x;

    vec2 patch_size = 1.0 / vec2(dim);
    vec2 global_uv = vec2(coord_x, coord_y) * patch_size + local_uv * patch_size;

    return global_uv;
}

void main() {
    float u = gl_TessCoord.x;
    float v = (subdivisions + 1)/subdivisions*gl_TessCoord.y;

    if (!u_horizontal) {
        vec4 p = vec4(find_bezier_surface_degree_3(u, v), 1.0);
        vec2 tex = find_global_uv(vec2(u, v), patchId,  ivec2(patchesDim.x, patchesDim.y));
        texCoords = vec3(tex.x, tex.y, textureId);
        gl_Position = u_pvMat*p;
    } else {
        vec4 p = vec4(find_bezier_surface_degree_3(v, u), 1.0);
        vec2 tex = find_global_uv(vec2(v, u), patchId, ivec2(patchesDim.x, patchesDim.y));
        texCoords = vec3(tex.x, tex.y, textureId);
        gl_Position = u_pvMat*p;
    }
    
}

