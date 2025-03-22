#version 430 core

layout(location = 0) out vec4 fragColor;

uniform float u_gridCellSize = 0.025;
uniform vec4 u_gridColorThin = vec4(0.6, 0.67, 0.78, 0.5);
uniform vec4 u_gridColorThick = vec4(0.78, 0.82, 1.0, 0.9);

uniform float u_gridMinPixelsBetweenCells = 2.0;
uniform float u_gridSize = 100.0;

uniform vec3 u_camWorldPos;

in vec3 fragWorldPos;
in float camTilt;

float log10(float x)
{
    float f = log(x) / log(10.0);
    return f;
}

float satf(float x)
{
    float f = clamp(x, 0.0, 1.0);
    return f;
}

float max2(vec2 v) {
    return max(v.x, v.y);
}

vec2 satv(vec2 x)
{
    vec2 v = clamp(x, vec2(0.0), vec2(1.0));
    return v;
}

void main() {



   vec2 dvx = vec2(dFdx(fragWorldPos.x), dFdy(fragWorldPos.x));
   vec2 dvy = vec2(dFdx(fragWorldPos.z), dFdy(fragWorldPos.z));
   vec2 dudv = vec2(length(dvx), length(dvy));
   float lod = max(0.0, log10(length(dudv) * u_gridMinPixelsBetweenCells / u_gridCellSize) + 1.0);
   dudv *= 4.0;

   float gridCellSizeLod0 = u_gridCellSize * pow(10.0, floor(lod));
   float gridCellSizeLod1 = gridCellSizeLod0 * 10.0;
   float gridCellSizeLod2 = gridCellSizeLod1 * 10.0;

   vec2 mod_div_dudv = mod(fragWorldPos.xz, gridCellSizeLod0) / dudv;
   float lod0a = max2(vec2(1.0) - abs(satv(mod_div_dudv) * 2.0 - vec2(1.0)));

   mod_div_dudv = mod(fragWorldPos.xz, gridCellSizeLod1) / dudv;
   float lod1a = max2(vec2(1.0) - abs(satv(mod_div_dudv) * 2.0 - vec2(1.0)));

   mod_div_dudv = mod(fragWorldPos.xz, gridCellSizeLod2) / dudv;
   float lod2a = max2(vec2(1.0) - abs(satv(mod_div_dudv) * 2.0 - vec2(1.0)));

   float lod_fade = fract(lod);

   vec4 col;
   if (lod2a > 0.0) {
        col = u_gridColorThick;
        col.a *= lod2a;
   } else {
        if (lod1a > 0.0) {
            col = mix(u_gridColorThick, u_gridColorThin, lod_fade);
            col.a *= lod1a;
        } else {
            col = u_gridColorThin;
            col.a *= (lod0a * (1.0 - lod_fade));
        }
   }

   // Fade out the fragment basing on its distance from the camera
   float opacityFalloff = (1.0 - satf(length(fragWorldPos.xz + u_camWorldPos.xz) / u_gridSize));
   col.a *= opacityFalloff;

   // Fade out the fragment basing on it's vector to the camera
   float fragAngle = abs(normalize(fragWorldPos + u_camWorldPos).y);
   float angleFalloff = min(25*fragAngle*fragAngle,  1.0);
   col.a *= angleFalloff;

   // Fade out the fragments basing on the camera tilt
   col.a *= min(camTilt*camTilt*17 - 0.05, 1.0);

   fragColor = col;
}
