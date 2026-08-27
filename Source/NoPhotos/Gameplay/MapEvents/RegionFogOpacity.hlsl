// Paste this entire file into a Material Custom node (Output Type: CMOT Float1).
// Inputs and required material settings are listed in SoftRegionFog.md.
// Back faces alone integrate the volume; front faces must not blend a second time.
if (FaceSign > 0.0 || RegionFogOpacity <= 0.0)
{
    return 0.0;
}

float3 worldRay = WorldPos - CameraPos;
float proxyDistance = length(worldRay);
if (proxyDistance < 0.001 || ProxyDepth <= 0.001 || OpaqueDepth <= 0.0)
{
    return 0.0;
}
worldRay /= proxyDistance;

// Oriented box coordinates in world centimetres. Axes are unit vectors from C++.
float3 offset = CameraPos - RegionCenter;
float3 origin = float3(dot(offset, RegionAxisX), dot(offset, RegionAxisY), dot(offset, RegionAxisZ));
float3 ray = float3(dot(worldRay, RegionAxisX), dot(worldRay, RegionAxisY), dot(worldRay, RegionAxisZ));
float3 halfSize = max(RegionHalfExtent, 0.001);

// Slab intersection. Explicit parallel-axis handling avoids NaNs at box faces.
float entry = 0.0;
float exitDistance = 1.0e20;
[unroll]
for (int axis = 0; axis < 3; ++axis)
{
    if (abs(ray[axis]) < 0.000001)
    {
        if (abs(origin[axis]) >= halfSize[axis])
        {
            return 0.0;
        }
    }
    else
    {
        float a = (-halfSize[axis] - origin[axis]) / ray[axis];
        float b = ( halfSize[axis] - origin[axis]) / ray[axis];
        entry = max(entry, min(a, b));
        exitDistance = min(exitDistance, max(a, b));
    }
}

// Disable Depth Test is required for the proxy. Stop fog at the actual opaque
// scene instead, including when the camera or a wall is inside the box.
// SceneDepth/PixelDepth are view-axis depths; convert to perspective ray distance.
float sceneDistance = OpaqueDepth * (proxyDistance / ProxyDepth);
exitDistance = min(exitDistance, sceneDistance);
if (exitDistance <= entry)
{
    return 0.0;
}

float minHalfSize = min(halfSize.x, min(halfSize.y, halfSize.z));
float feather = clamp(RegionFogShape.x, 1.0, max(1.0, minHalfSize));
float cornerRadius = clamp(RegionFogShape.y, 0.0, minHalfSize);
float frequency = max(RegionFogShape.z, 0.0001);
float noiseStrength = saturate(RegionFogShape.w);
float3 innerSize = max(halfSize - cornerRadius, 0.0);

const int sampleCount = 32;
float stepLength = (exitDistance - entry) / sampleCount;
float integratedDensity = 0.0;
[unroll]
for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
{
    float t = entry + (sampleIndex + 0.5) * stepLength;
    float3 p = origin + ray * t;
    float3 q = abs(p) - innerSize;
    float roundedDistance = length(max(q, 0.0))
        + min(max(q.x, max(q.y, q.z)), 0.0) - cornerRadius;

    // Slowly drifting, texture-free density variation. Fade remains exactly zero
    // at the original box boundary so the cube silhouette cannot become a wall.
    float3 n = p * frequency + TimeSeconds * float3(0.08, 0.03, 0.04);
    float cloud = 0.5 + 0.5 * sin(dot(n, float3(1.3, 1.7, 0.9)))
        * sin(dot(n, float3(-1.1, 0.8, 1.5)));
    float3 edgeDistances = halfSize - abs(p);
    float boxEdge = min(edgeDistances.x, min(edgeDistances.y, edgeDistances.z));
    float softenedDistance = min(boxEdge,
        -roundedDistance + (cloud - 0.5) * feather * 0.6 * noiseStrength);
    float fade = saturate(softenedDistance / feather);
    fade = fade * fade * (3.0 - 2.0 * fade);
    float density = fade * lerp(1.0, lerp(0.35, 1.5, cloud), noiseStrength);
    integratedDensity += density * stepLength;
}

// RegionFogOpacity is now density per metre, not constant surface opacity.
return saturate(1.0 - exp(-saturate(RegionFogOpacity) * integratedDensity * 0.01));
