const int KEY_LEFT = 37;
const int KEY_UP = 38;
const int KEY_RIGHT = 39;
const int KEY_DOWN = 40;

const float SPEED = 1.0;



void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    float outData = 0.0;
    
    switch(int(fragCoord.x)){
        case 0:
        	outData = texelFetch(iChannel0, ivec2(0), 0).r +
                (iTimeDelta * SPEED) * (texelFetch(iChannel1, ivec2(KEY_RIGHT, 0), 0).r -
                 texelFetch(iChannel1, ivec2(KEY_LEFT, 0), 0).r);
        	break;
        case 1:
            outData = texelFetch(iChannel0, ivec2(1,0), 0).r + (SPEED*iTimeDelta)*
            (texelFetch(iChannel1, ivec2(KEY_UP, 0), 0).r - texelFetch(iChannel1, ivec2(KEY_DOWN, 0), 0).r);
    }
    
    fragColor = vec4(outData, 0.0, 0.0, 1.0);
}