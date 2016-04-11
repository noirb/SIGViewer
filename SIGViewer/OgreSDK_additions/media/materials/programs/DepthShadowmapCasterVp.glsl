uniform mat4 worldViewProj;
uniform vec4 texelOffsets;
uniform vec4 depthRange;
uniform float  depthOffset;
uniform float  coefficient;
varying vec2 depth;


void main()
{
	gl_Position = ftransform();
	gl_Position.xy += texelOffsets.zw * gl_Position.w;
	// linear depth storage
	// offset / scale range output
	
	
	depth.x = ( gl_Position.z - depthOffset) * 0.0039215686 * coefficient ;

	if( depth.x > 1.0)
	{
		depth.x = 1.0;
	}

	else if( depth.x < 0.0)
	{
		depth.x = 0.0;
	}
//#if LINEAR_RANGE
	//depth.x = (gl_Position.z - depthRange.x) * depthRange.w;
	//depth.x = 1;
//#else
	//depth.y = gl_Position.w;
//#endif
	depth.y = gl_Position.w;
}