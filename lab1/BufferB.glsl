float rand(vec2 co) {
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    ivec2 id = ivec2(fragCoord);   
    vec4 prev = texelFetch(iChannel1, id, 0);

    vec2 pos = prev.xy;   
    float timer = prev.z;   
    float ac = prev.w;  

    if (iFrame == 0) {
        
        pos = vec2(rand(vec2(id.x, 0.0))*12.0 - 0.6, 0.6);
        timer = 0.0;
        ac = 1.0;
    }

    if (ac == 1.0) {
       
        pos.y -= 0.01;

        if (pos.y <= -0.5) {
            ac = 0.0;
            timer = rand(vec2(id.x, iTime)) * 200.0 + 60.0;
        }
    } else {
        
        timer -= 1.0;
        if (timer <= 0.0) {
            ac = 1.0;
            pos.y = 0.5;  
            pos.x = rand(vec2(float(id.x) * 30.0, iTime)) * 0.9 - 0.45;
        }
    }

    fragColor = vec4(pos, timer, ac);
}

