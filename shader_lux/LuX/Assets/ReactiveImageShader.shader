Shader "Hidden/ReactiveImageShader"
{
	Properties
	{
		
	}
	SubShader
	{
		// No culling or depth
		Cull Off ZWrite Off ZTest Always

		Pass
		{
			CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag
			
			#include "UnityCG.cginc"

			struct appdata
			{
				float4 vertex : POSITION;
				float2 uv : TEXCOORD0;
			};

			struct v2f
			{
				float2 uv : TEXCOORD0;
				float4 vertex : SV_POSITION;
			};

			v2f vert (appdata v)
			{
				v2f o;
				o.vertex = UnityObjectToClipPos(v.vertex);
				o.uv = v.uv;
				return o;
			}

			float random(in float2 st) {
				return frac(sin(dot(st.xy,
					float2(12.9898, 78.233)))*
					43758.5453123);
			}

			// Based on Morgan McGuire @morgan3d
			// https://www.shadertoy.com/view/4dS3Wd
			float noise(in float2 st) {
				float2 i = floor(st);
				float2 f = frac(st);

				// Four corners in 2D of a tile
				float a = random(i);
				float b = random(i + float2(1.0, 0.0));
				float c = random(i + float2(0.0, 1.0));
				float d = random(i + float2(1.0, 1.0));

				float2 u = f * f * (3.0 - 2.0 * f);

				return lerp(a, b, u.x) +
					(c - a)* u.y * (1.0 - u.x) +
					(d - b) * u.x * u.y;
			}

			#define OCTAVES 6
			float fbm(in float2 st) {
				// Initial values
				float value = 0.0;
				float amplitude = .5;
				float frequency = 0.;
				//
				// Loop of octaves
				for (int i = 0; i < OCTAVES; i++) {
					value += amplitude * noise(st);
					st *= 2.;
					amplitude *= .5;
				}
				return value;
			}

			fixed4 frag (v2f i) : SV_Target
			{
				float2 eye2 = float2(1.0, 1.0);

				float t = _Time[1];

				float3 c = float3(0.0, 0.0, 0.0);

				float l, z = t;
				for (int j = 0; j < 3; j++) {
					float2 uv = i.vertex.xy / _ScreenParams.xy;
					float2 p = uv;
					p -= 0.5;
					p.x *= _ScreenParams.x / _ScreenParams.y;
					z += 0.07;

					l = length(p);
					uv += p / l * (sin(z) + 1.0) * abs(sin(l * 9.0 - z * 2.0));
					c[j] = 0.01 / length(abs(frac(uv) - 0.5));
				}

				return float4(c / l, t);
			}
			ENDCG
		}
	}
}
