layout(std140) uniform UniformsHot {
    mat4 uProjection;
    mat4 uTransform;
    mat4 uInvTransform;
    mat3 uNormalMat;
};

layout(location = 0) in vec4    aPosition;
layout(location = 2) in vec4    aColor;
layout(location = 4) in vec3    aA0;
layout(location = 5) in vec3    aA1;
layout(location = 6) in vec3    aA2;
layout(location = 7) in vec3    aA3;
layout(location = 8) in float   aHalfHeightSq;
layout(location = 9) in float   aRadiusSq;

// in edge's local space
out vec4    vColor;
out mat4    vObjectToEye;
out mat4    vObjectToClip;
out vec4    vPosition;
out float   vHalfHeightSq;
out float   vRadiusSq;
out vec4    vEyePos;
out vec3    vLightDir;

mat4 InvertTR(mat4 m) {
    mat3 R = transpose(mat3(m));
    vec3 dt = R * -vec3(m[3]);
    return mat4(
        vec4(R[0], 0.0), 
        vec4(R[1], 0.0), 
        vec4(R[2], 0.0),
        vec4(dt, 1.0)
    );
}

void main() {
    vColor = aColor;
    mat4 objectToWorld = mat4(
        vec4(aA0, 0.0),
        vec4(aA1, 0.0),
        vec4(aA2, 0.0),
        vec4(aA3, 1.0)
    );
    mat4 worldToObject = InvertTR(objectToWorld);
    vObjectToEye = uTransform * objectToWorld;
    vObjectToClip = uProjection * vObjectToEye;
    mat4 eyeToObject = worldToObject * uInvTransform;
    vPosition = worldToObject * aPosition;
    vHalfHeightSq = aHalfHeightSq;
    vRadiusSq = aRadiusSq;
    vEyePos = eyeToObject * vec4(0.0, 0.0, 0.0, 1.0);
    vLightDir = (eyeToObject * vec4(0.0, 0.0, 1.0, 0.0)).xyz;
	gl_Position = uProjection * (uTransform * aPosition);
}
