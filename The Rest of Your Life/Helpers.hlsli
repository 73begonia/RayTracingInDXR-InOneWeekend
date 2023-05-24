/*
 * ----------------------------------------
 * HELPER FUNCTIONS
 * ----------------------------------------
 */

static const float4 lightAmbientColor = float4(0.2, 0.2, 0.2, 1.0);
static const float3 lightPosition = float3(0.0, 1.5, 5.0);
static const float4 lightDiffuseColor = float4(0.2, 0.2, 0.2, 1.0);
static const float4 lightSpecularColor = float4(1, 1, 1, 1);
static const float4 primitiveAlbedo = float4(1.0, 0.0, 0.0, 1.0);
static const float diffuseCoef = 0.9;
static const float specularCoef = 0.7;
static const float specularPower = 50;

// Retrieve hit world position.
float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

// Diffuse lighting calculation.
float CalculateDiffuseCoefficient(in float3 hitPosition, in float3 incidentLightRay, in float3 normal)
{
    float fNDotL = saturate(dot(-incidentLightRay, normal));
    return fNDotL;
}

// Phong lighting specular component
float4 CalculateSpecularCoefficient(in float3 hitPosition, in float3 incidentLightRay, in float3 normal, in float specularPower)
{
    float3 reflectedLightRay = normalize(reflect(incidentLightRay, normal));
    return pow(saturate(dot(reflectedLightRay, normalize(-WorldRayDirection()))), specularPower);
}


// Phong lighting model = ambient + diffuse + specular components.
float4 CalculatePhongLighting(in float4 albedo, in float3 normal, in float diffuseCoef = 1.0, in float specularCoef = 1.0, in float specularPower = 50)
{
    float3 hitPosition = HitWorldPosition();
    float3 incidentLightRay = normalize(hitPosition - lightPosition);

    // Diffuse component.
    float Kd = CalculateDiffuseCoefficient(hitPosition, incidentLightRay, normal);
    float4 diffuseColor = diffuseCoef * Kd * lightDiffuseColor * albedo;

    // Specular component.
    float4 specularColor = float4(0, 0, 0, 0);
    float4 Ks = CalculateSpecularCoefficient(hitPosition, incidentLightRay, normal, specularPower);
    specularColor = specularCoef * Ks * lightSpecularColor;

    // Ambient component.
    // Fake AO: Darken faces with normal facing downwards/away from the sky a little bit.
    float4 ambientColorMin = lightAmbientColor - 0.15;
    float4 ambientColorMax = lightAmbientColor;
    float fNDotL = saturate(dot(-incidentLightRay, normal));
    float4 ambientColor = albedo * lerp(ambientColorMin, ambientColorMax, fNDotL);

    return ambientColor + diffuseColor + specularColor;
}