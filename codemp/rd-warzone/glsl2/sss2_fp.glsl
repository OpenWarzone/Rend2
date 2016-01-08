uniform sampler2D				u_DiffuseMap;
uniform sampler2D				u_ScreenDepthMap;
uniform sampler2D				u_GlowMap;

uniform mat4					u_ModelViewProjectionMatrix;
uniform vec4					u_ViewInfo; // zmin, zmax, zmax / zmin
uniform vec4					u_LightOrigin;
uniform vec4					u_Local0;

varying vec2					var_ScreenTex;
varying vec2					var_Dimensions;
varying vec2					projAB;
varying vec3					viewRay;
varying vec3					light_p;

float linearize(float depth)
{
	//return -u_ViewInfo.y * u_ViewInfo.x / (depth * (u_ViewInfo.y - u_ViewInfo.x) - u_ViewInfo.y);
	//return depth / 4.0;
	return 1.0 / mix(u_ViewInfo.z, 1.0, depth);

	//return pow(depth, 255.0);
}

vec3 CalcPosition(void){
    float depth = texture2D(u_ScreenDepthMap, var_ScreenTex).r;
    //float linearDepth = projAB.y / (depth - projAB.x);
	float linearDepth = linearize(depth);
    vec3 ray = normalize(viewRay);
    ray = ray / ray.z;
    return linearDepth * ray;
}

void main(void){
	vec3 dglow = texture2D(u_GlowMap, var_ScreenTex).rgb;
	float dglowStrength = clamp(length(dglow.rgb) * 3.0, 0.0, 1.0);

	vec2 texel_size = vec2(1.0 / var_Dimensions);
	float depth = texture2D(u_ScreenDepthMap, var_ScreenTex).r;
	depth = linearize(depth);

	float invDepth = 1.0 - depth;

	//vec2 distFromCenter = vec2((0.5 - var_ScreenTex.x) * 2.0, (1.0 - var_ScreenTex.y) * invDepth);
	//vec2 offset = distFromCenter * 40.0;
	//vec2 pixOffset = (offset * texel_size) * invDepth;
	//vec2 pos = var_ScreenTex + pixOffset;

	//vec2 distFromCenter = vec2((0.5 - var_ScreenTex.x), 0.5);
	vec2 distFromCenter = vec2((0.5 - var_ScreenTex.x) * 2.0, (1.0 - var_ScreenTex.y) * 0.5);
	vec2 pixOffset = clamp((distFromCenter * invDepth) * texel_size * 80.0, vec2(0.0), texel_size * 80.0);
	vec2 pos = var_ScreenTex + pixOffset;

	float d2 = texture2D(u_ScreenDepthMap, pos).r;
	d2 = linearize(d2);

	vec4 diffuse = texture2D(u_DiffuseMap, var_ScreenTex);

	float depthDiff = clamp(depth - d2, 0.0, 1.0);

	if (depthDiff > u_Local0.r && depthDiff < u_Local0.g)
	{
		vec3 shadow = diffuse.rgb * 0.25;
		shadow += diffuse.rgb * (0.75 * (depthDiff / u_Local0.g)); // less darkness at higher distance for blending
		float invDglow = 1.0 - dglowStrength;
		diffuse.rgb = (diffuse.rgb * dglowStrength) + (shadow * invDglow);
	}
	else if (depthDiff < u_Local0.r)
	{
		vec3 shadow = diffuse.rgb * 0.25;
		shadow += diffuse.rgb * (0.75 * (1.0 - (depthDiff / u_Local0.r))); // less darkness at lower distance for blending
		float invDglow = 1.0 - dglowStrength;
		diffuse.rgb = (diffuse.rgb * dglowStrength) + (shadow * invDglow);
	}

	gl_FragColor = diffuse;
}
