Shader "ReactiveImageShader"
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

#define RING_COUNT 256
#define TOTAL_LENGTH 1.42
#define COLOR_CYCLES 4
#define DECAY 0.985

			float _LoudnessArray[RING_COUNT];
			float _PitchArray[RING_COUNT];
			uint _ArrayIndex;

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

			v2f vert(appdata v)
			{
				v2f o;
				o.vertex = UnityObjectToClipPos(v.vertex);
				o.uv = v.uv;
				return o;
			}

			float2x2 rotate2d(float _angle) {
				return float2x2(cos(_angle), -sin(_angle),
					sin(_angle), cos(_angle));
			}

			float3 HUEtoRGB(in float H)
			{
				float R = abs(H * 6 - 3) - 1;
				float G = 2 - abs(H * 6 - 2);
				float B = 2 - abs(H * 6 - 4);
				return saturate(float3(R, G, B));
			}

			float hash(float n)
			{
				return frac(sin(n)*43758.5453);
			}

			float noise(float3 x)
			{
				// The noise function returns a value in the range -1.0f -> 1.0f

				float3 p = floor(x);
				float3 f = frac(x);

				f = f * f*(3.0 - 2.0*f);
				float n = p.x + p.y*57.0 + 113.0*p.z;

				return lerp(lerp(lerp(hash(n + 0.0), hash(n + 1.0), f.x),
					lerp(hash(n + 57.0), hash(n + 58.0), f.x), f.y),
					lerp(lerp(hash(n + 113.0), hash(n + 114.0), f.x),
						lerp(hash(n + 170.0), hash(n + 171.0), f.x), f.y), f.z);
			}

			fixed4 frag(v2f i) : SV_Target
			{
				float width = TOTAL_LENGTH / RING_COUNT;
				float2 st = (i.vertex.xy * 2.0 - _ScreenParams.xy) / max(_ScreenParams.x, _ScreenParams.y);
				
				// st = mul(rotate2d(_Time[1] / 4.0 + sin(_Time[0])), st);
				//// st = lerp(st, st * st, abs(sin(_Time[0] / 2.0)));

				float3 c = float3(0.0, 0.0, 0.0);

				float d = sqrt(dot(st, st));

				// Wrap relative_ring_num and width around a linear -> function warp (add depth to the hole)

				float relative_ring_num = d / width;

				//relative_ring_num += noise(float3(st * (20.0 + sin(_Time[1] + sin(_Time[2])) * 10.0), _Time[2])) * (40.0 + sin(_Time[1]) * 30.0);

				uint ring_num = relative_ring_num;

				uint index = ((_ArrayIndex + RING_COUNT) - ring_num) % RING_COUNT;
				uint next_index = (index - 1 + RING_COUNT) % RING_COUNT;

				float3 inner_ring_color = _LoudnessArray[index] * HUEtoRGB(frac(_PitchArray[index] * COLOR_CYCLES + _Time[0]));
				float3 outer_ring_color = _LoudnessArray[next_index] * HUEtoRGB(frac(_PitchArray[next_index] * COLOR_CYCLES + _Time[0]));

				float inbetween = relative_ring_num - ring_num;

				c = lerp(inner_ring_color, outer_ring_color, inbetween) * pow(DECAY, relative_ring_num);

				return float4(c, 1.0);
			}
			ENDCG
		}
	}
}
