uniform sampler2D	u_DiffuseMap;
uniform sampler2D	u_NormalMap;
uniform sampler2D	u_OverlayMap; // Real normals. Alpha channel 1.0 means enabled...

uniform vec4		u_Settings0;
uniform vec2		u_Dimensions;

varying vec2		var_TexCoords;

//#define __NORMAL_METHOD_1__
#define __NORMAL_METHOD_2__

#ifdef __NORMAL_METHOD_1__
const vec3 LUMA_COEFFICIENT = vec3(0.2126, 0.7152, 0.0722);

float lumaAtCoord(vec2 coord) {
  vec3 pixel = texture(u_DiffuseMap, coord).rgb;
  float luma = dot(pixel, LUMA_COEFFICIENT);
  return luma;
}

vec4 normalVector(vec2 coord) {
  float lumaU0 = lumaAtCoord(coord + vec2(-1.0,  0.0) / u_Dimensions);
  float lumaU1 = lumaAtCoord(coord + vec2( 1.0,  0.0) / u_Dimensions);
  float lumaV0 = lumaAtCoord(coord + vec2( 0.0, -1.0) / u_Dimensions);
  float lumaV1 = lumaAtCoord(coord + vec2( 0.0,  1.0) / u_Dimensions);

  vec2 slope = vec2(lumaU0 - lumaU1, lumaV0 - lumaV1) * 0.5 + 0.5;

// Contrast...
#define normLower ( 128.0/*48.0*/ / 255.0 )
#define normUpper (255.0 / 192.0/*128.0*/ )
  slope = clamp((clamp(slope - normLower, 0.0, 1.0)) * normUpper, 0.0, 1.0);

  return vec4(slope, 1.0, length(slope.rg / 2.0));
}
#endif //__NORMAL_METHOD_1__

#ifdef __NORMAL_METHOD_2__
float getHeight(vec2 uv) {
  return length(texture(u_DiffuseMap, uv).rgb) / 3.0;
}

vec4 bumpFromDepth(vec2 uv, vec2 resolution, float scale) {
  vec2 step = 1. / resolution;
    
  float height = getHeight(uv);
    
  vec2 dxy = height - vec2(
      getHeight(uv + vec2(step.x, 0.)), 
      getHeight(uv + vec2(0., step.y))
  );

  vec3 N = vec3(dxy * scale / step, 1.);

// Contrast...
#define normLower ( 128.0/*48.0*/ / 255.0 )
#define normUpper (255.0 / 192.0/*128.0*/ )
  N = clamp((clamp(N - normLower, 0.0, 1.0)) * normUpper, 0.0, 1.0);

  return vec4(normalize(N) * 0.5 + 0.5, height);
}

vec4 normalVector(vec2 coord) {
	return bumpFromDepth(coord, u_Dimensions, 0.1 /*scale*/);
}
#endif //__NORMAL_METHOD_2__

void main(void)
{
	vec4 norm = texture(u_NormalMap, var_TexCoords);

	if (u_Settings0.r > 0.0)
	{
		vec4 normalDetail = textureLod(u_OverlayMap, var_TexCoords, 0.0);

		norm.rgb = norm.rgb * 2.0 - 1.0;

		if (normalDetail.a < 1.0)
		{// Don't have real normalmap, make normals for this pixel...
			normalDetail = normalVector(var_TexCoords);
		}

		normalDetail.rgb = normalDetail.rgb * 2.0 - 1.0;
		normalDetail.rgb *= 0.25;//u_Settings0.g;
		normalDetail.z = sqrt(clamp((0.25 - normalDetail.x * normalDetail.x) - normalDetail.y * normalDetail.y, 0.0, 1.0));
		norm.rgb = normalize(norm.rgb + normalDetail.rgb);
		norm.rgb = norm.rgb * 0.5 + 0.5;
	}

	gl_FragColor = vec4(norm.rgb, 1.0);
}