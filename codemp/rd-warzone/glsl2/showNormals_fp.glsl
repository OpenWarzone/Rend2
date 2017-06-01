uniform sampler2D	u_DiffuseMap;
uniform sampler2D	u_NormalMap;

varying vec2		var_TexCoords;

void main(void)
{
	vec4 norm = texture(u_NormalMap, var_TexCoords);
	gl_FragColor = vec4(norm.rgb, 1.0);
}