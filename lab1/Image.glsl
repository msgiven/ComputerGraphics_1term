const vec2 CASE_SIZE = vec2(0.05, 0.1);
const vec2 WHEEL_SIZE = vec2(0.07, 0.015);
const float DELTA_Y = 0.05;
const float LINE_SPACE = 0.1;

mat2 rotate(float angle){
    float c = cos(angle);
    float s = sin(angle);
    return mat2(c, -s, s, c);
}

vec2 GetCarCoord(){
    return vec2(texelFetch(iChannel0, ivec2(0),0).r, 
    texelFetch(iChannel0, ivec2(1,0),0).r);
}

float sdRect(vec2 p, vec2 size){
    vec2 d = abs(p)-size;
    return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

float sdCircle( vec2 p, float r )
{
    return length(p) - r;
}


float sdHexagon( in vec2 p, in
float r )
{
    const vec3 k = vec3(-0.866025404,0.5,0.577350269);
    p = abs(p);
    p -= 2.0*min(dot(k.xy,p),0.0)*k.xy;
    p -= vec2(clamp(p.x, -k.z*r, k.z*r), r);
    return length(p)*sign(p.y);
}

float SdCar(vec2 p, vec2 caseSize, vec2 wheelSize) {
    //p*= rotate(-iTime);
    float carCase = sdRect(p, caseSize);
    float wheelFront = sdRect(p + vec2(0.0, DELTA_Y), wheelSize);
    float wheelBack = sdRect(p + vec2(0.0, -DELTA_Y), wheelSize);
    
    return min(carCase, min(wheelFront, wheelBack));
}


void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    
    vec3 color = vec3(0.1); 
   

    for (int i = 0; i < 10; i++){
        float line = sdRect(uv - vec2(0.0, -0.45 + float(i) * LINE_SPACE), vec2(0.01, 0.02));
        if (line < 0.0){
            color = vec3(1.0, 1.0, 1.0);
        }
    }

    vec2 carPos = GetCarCoord();
    float dCar = SdCar((uv - carPos), CASE_SIZE, WHEEL_SIZE);
    if (dCar < 0.0) {
        color = vec3(0.4, 0.5, 0.6);
    }

    
    for (int i = 0; i < 5; i++) {
        vec4 data = texelFetch(iChannel1, ivec2(i,0), 0);
        vec2 figurePos = data.xy;
        float ac = data.w;

        if (ac == 1.0) {
            float dFigure;

            if (i == 1) {
                dFigure = sdCircle(uv - figurePos, 0.04);
            } else if (i == 2){
                dFigure = sdHexagon(uv - figurePos, 0.04);
            }else {
            
                dFigure = sdRect(uv - figurePos, vec2(0.04));
            }

            if (dFigure < 0.0) {
                color = vec3(1.0, 0.7, 0.2);
            }
        }
    }

    fragColor = vec4(color,1.0);
}
