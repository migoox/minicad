#version 430 core

#define MAX_BSPLINE_DEGREE 3

layout(isolines, equal_spacing) in;

uniform mat4 u_pvMat;
uniform int u_bspline_degree = 3;

patch in float isolinesCount;
patch in int patch_ind;

// float knot(int i) {
//     // Using equidistant knots, each interval is of length 1
//     return float(patch_ind - 1) + float(i);
// }

vec3 find_bspline_point(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t) {
    // find bernstein basis functions, we can do so as the equidistant knots are in use
    float u = (1.0 - t);
    float b3 = t*t*t;
    float b2 = 3.0*t*t*u;
    float b1 = 3.0*t*u*u;
    float b0 = u*u*u;

    // convert to the de boor points to bernstein basis 
    vec3 pb0 = (p0 + 4*p1 + p2) / 6.0;
    vec3 pb1 = (4*p1 + 2*p2) / 6.0;
    vec3 pb2 = (2*p1 + 4*p2) / 6.0;
    vec3 pb3 = (p1 + 4*p2 + p3) / 6.0;

    return pb0*b0 + pb1*b1 + pb2*b2 + pb3*b3;

    // float bvals[MAX_BSPLINE_DEGREE + 1];
    // int n = u_bspline_degree;

    // t = knot(1) + t; 

    // bvals[n] = 1.0;
    // for (int d = n - 1; d >= 0; --d) {
    //     int i = d;
    //     bvals[i] = (knot(i + n) - t)/(knot(i + n) - knot(i))*bvals[i + 1];

    //     for (i = d + 1; i <= n - 1; ++i) {
    //         bvals[i] = (t - knot(i - 1))/(knot(i + n - 1) - knot(i - 1))*bvals[i] + 
    //                    (knot(i + n) - t)/(knot(i + n) - knot(i))*bvals[i + 1];
    //     }
        
    //     i = n;
    //     bvals[i] = (t - knot(i - 1))/(knot(i + n - 1) - knot(i - 1))*bvals[i];
    // }

    // return p0*bvals[0] + p1*bvals[1] + p2*bvals[2] + p3*bvals[3];
}

void main()
{
    float t = gl_TessCoord.x  / isolinesCount + gl_TessCoord.y;

    vec3 p0 = gl_in[0].gl_Position.xyz;
    vec3 p1 = gl_in[1].gl_Position.xyz;
    vec3 p2 = gl_in[2].gl_Position.xyz;
    vec3 p3 = gl_in[3].gl_Position.xyz;
    
    vec3 p = find_bspline_point(p0, p1, p2, p3, t);

    gl_Position = u_pvMat*vec4(p, 1.0);
}
