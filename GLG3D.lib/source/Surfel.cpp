/** \file GLG3D.lib/source/Surfel.cpp
    \maintainer Morgan McGuire, http://graphics.cs.williams.edu

    Copyright 2013 Morgan McGuire
 */

#include "G3D/Random.h"
#include "G3D/CoordinateFrame.h"
#include "G3D/Any.h"
#include "GLG3D/Surfel.h"
#include "GLG3D/Material.h"
#include "GLG3D/Surface.h"

namespace G3D {

float Surfel::ExpressiveParameters::boost(const Color3& diffuseReflectivity) const {
    // Avoid computing the HSV transform in the common case
    if (unsaturatedMaterialBoost == saturatedMaterialBoost) {
        return unsaturatedMaterialBoost;
    }

    const float m = diffuseReflectivity.max();
    const float saturation = (m == 0.0f) ? 0.0f : ((m - diffuseReflectivity.min()) / m);

    return lerp(unsaturatedMaterialBoost, saturatedMaterialBoost, saturation);
}


Surfel::ExpressiveParameters::ExpressiveParameters(const Any& a) {
    *this = ExpressiveParameters();
    AnyTableReader r(a);

    r.get("unsaturatedMaterialBoost", unsaturatedMaterialBoost);
    r.get("saturatedMaterialBoost", saturatedMaterialBoost);
    r.verifyDone();
}


Any Surfel::ExpressiveParameters::toAny() const {
    Any a(Any::TABLE, "Surfel::ExpressiveParameters");

    a["unsaturatedMaterialBoost"] = unsaturatedMaterialBoost;
    a["saturatedMaterialBoost"] = saturatedMaterialBoost;

    return a;
}


Surfel::Surfel
(const Point3&     _pos,
 const Point3&     _prevPos,
 const Vector3&    geometricNormal,
 const Vector3&    shadingNormal,
 const Vector3&    shadingTangent1,
 const Vector3&    shadingTangent2,
 const float       etaPos,
 const Color3&     kappaPos,
 const float       etaNeg,
 const Color3&     kappaNeg,
 const Source&     source,
 const Material*   material,
 const Surface*    surface) :
    position(_pos),
    prevPosition(_prevPos),
    geometricNormal(geometricNormal),
    shadingNormal(shadingNormal),
    shadingTangent1(shadingTangent1),
    shadingTangent2(shadingTangent2),
    etaPos(etaPos), kappaPos(kappaPos),
    etaNeg(etaNeg), kappaNeg(kappaNeg),
    material(material), 
    surface(surface),
    source(source) {
}


void Surfel::transformToWorldSpace(const CFrame& xform) {
    position = xform.pointToWorldSpace(position);
    geometricNormal = xform.vectorToWorldSpace(geometricNormal);
    shadingNormal   = xform.vectorToWorldSpace(shadingNormal);
    shadingTangent1 = xform.vectorToWorldSpace(shadingTangent1);
    shadingTangent2 = xform.vectorToWorldSpace(shadingTangent2);
}


Color3 Surfel::finiteScatteringDensity
   (PathDirection pathDirection,
    const Vector3&    wFrom, 
    const Vector3&    wTo,
    const ExpressiveParameters& expressiveParameters) const {

    if (pathDirection == PathDirection::SOURCE_TO_EYE) {
        return finiteScatteringDensity(wFrom, wTo, expressiveParameters);
    } else {
        return finiteScatteringDensity(wTo, wFrom, expressiveParameters);
    }
}


void Surfel::sampleFiniteDirectionPDF
   (PathDirection      pathDirection,
    const Vector3&     w_o,
    Random&            rng,
    const ExpressiveParameters& expressiveParameters,
    Vector3&           w_i,
    float&             pdfValue) const {

    if (transmissive()) {
        Vector3::cosSphereRandom(shadingNormal, rng, w_i, pdfValue);
    } else {
        Vector3::cosHemiRandom(shadingNormal, rng, w_i, pdfValue);
    }
}


float Surfel::ignore = 0.0;
void Surfel::scatter
   (PathDirection    pathDirection,
    const Vector3&   w_o,
    bool             russianRoulette,
    Random&          rng,
    Color3&          weight,
    Vector3&         w_i,
    float&           probabilityHint,
    const ExpressiveParameters& expressiveParameters) const {

    // TODO: Russian roulette

    Surfel::ImpulseArray impulseArray;
    getImpulses(pathDirection, w_o, impulseArray, expressiveParameters);

    float impulseMagnitudeSum = 0.0f;
    float r = rng.uniform();
    for (int i = 0; i < impulseArray.size(); ++i) {
        const Surfel::Impulse& impulse = impulseArray[i];
        const float probabilityOfThisImpulse = impulse.magnitude.average();
        r -= probabilityOfThisImpulse;
        impulseMagnitudeSum += probabilityOfThisImpulse;
        if (r <= 0.0f) {
            w_i    = impulse.direction;
            weight = impulse.magnitude / probabilityOfThisImpulse;
            probabilityHint = probabilityOfThisImpulse;
            return;
        }
    }

    float pdfValue;
    sampleFiniteDirectionPDF(pathDirection, w_o, rng, expressiveParameters, w_i, pdfValue);

    // Took this branch with probability (1 - impulseMagnitudeSum)
    pdfValue *= 1.0f - impulseMagnitudeSum;
    if (pdfValue > 0.0f) {
        weight = finiteScatteringDensity(pathDirection, w_o, w_i, expressiveParameters) * max(w_i.dot(shadingNormal), 0.0f) / pdfValue;
    } else {
        weight = Color3::zero();
    }
    probabilityHint = float(weight.average() * 1e-3f);
}


#if 0 // old code with russianRoulette support

    // Net probability of scattering
    const Color3& prob3 = probabilityOfScattering(pathDirection, wi, rng, expressiveParameters);
    const float prob = prob3.average();

    probabilityHint = 1.0f;

    const float epsilon = 0.000001f;
    {
        // If the total probability is zero, we always want to absorb regardless
        // of the russianRoulette flag value
        const float thresholdProbability = 
            russianRoulette ? rng.uniform(0, 1) : 0.0f;
        
        if (prob <= thresholdProbability) {
            // There is no possible outgoing scattering direction for
            // this incoming direction.
            weight = Color3(0.0f);
            wo = Vector3::nan();
            return;
        }
    }

    ImpulseArray impulseArray;
    getImpulses(pathDirection, wi, impulseArray, expressiveParameters);

    // Importance sample against impulses vs. finite scattering
    float r = rng.uniform(0, prob);
    float totalIprob = 0.0f;

    for (int i = 0; i < impulseArray.size(); ++i) {
        // Impulses don't have projected area terms
        const Impulse& I     = impulseArray[i];
        const float    Iprob = I.magnitude.average();

        alwaysAssertM(Iprob > 0, "Zero-probability impulse");

        r -= Iprob;
        totalIprob += Iprob;
        if (r <= epsilon) {
            // Scatter along this impulse.  Divide the weight by
            // the average because we just importance sampled
            // among the impulses...but did so along a single
            // color channel.
              
            const float mag = I.magnitude.average();
            weight = I.magnitude / mag;
            if (russianRoulette) {
                weight /= prob;
            }

            wo = I.direction;
            probabilityHint = mag;
            return;
        }
    }

    alwaysAssertM(prob >= totalIprob,
                    "BSDF impulses accounted for more than "
                    "100% of scattering probability");

    // If we make it to here, the finite portion of the BSDF must
    // be non-zero because the BSDF itself is non-zero and
    // none of the impulses triggered sampling (note that r was
    // chosen on [0, prob], not [0, 1] above.
    
    sampleFinite(pathDirection, wi, rng, expressiveParameters, weight, wo);

    if (russianRoulette) {
        // For Russian roulette, if we successfully made it to this point without
        // rejecting the photon, normalize by the a priori
        // scattering probability, unless it is greater than 1.0 due to
        // non-physical boost (if it is greater than 1.0, there was zero
        // chance of absorption). 
        if (prob < 1.0f) {
            weight /= prob;
        }
    }

    probabilityHint = float(weight.average() * 1e-3);
}
#endif


Color3 Surfel::probabilityOfScattering
(PathDirection  pathDirection, 
 const Vector3& wi, 
 Random&        rng,
 const ExpressiveParameters& expressiveParameters) const {

    Color3 prob;

    // Sum the impulses (no cosine; principle of virtual images)
    ImpulseArray impulseArray;
    getImpulses(pathDirection, wi, impulseArray);
    for (int i = 0; i < impulseArray.size(); ++i) {
        prob += impulseArray[i].magnitude;
    }

    // This is uniform random sampling; would some kind of
    // striated or jittered sampling might produce a
    // lower-variance result.

    // Sample the finite portion.  Note the implicit cosine
    // weighting in the importance sampling of the sphere.
    const int N = 32;

    if (transmissive()) {
        // Check the entire sphere

        // Measure of each sample on a cosine-weighted sphere 
        // (the area of a cosine-weighted sphere is 2.0)
        const float dw = 2.0f * pif() / float(N);
        for (int i = 0; i < N; ++i) {
            const Vector3& wo = Vector3::cosSphereRandom(shadingNormal, rng);
            prob += finiteScatteringDensity(pathDirection, wi, wo, expressiveParameters) * dw;
        }
    } else {
        // Check only the positive hemisphere since the other hemisphere must be all zeros
        // Measure of each sample on a cosine-weighted hemisphere
        const float dw = 1.0f * pif() / float(N);
        for (int i = 0; i < N; ++i) {
            const Vector3& wo = Vector3::cosHemiRandom(shadingNormal, rng);
            prob += finiteScatteringDensity(pathDirection, wi, wo, expressiveParameters) * dw;
        }
    }

    return prob;
}


Color3 Surfel::reflectivity(Random& rng, const ExpressiveParameters& expressiveParameters) const {
    Color3 c;

    const int N = 10;
    for (int i = 0; i < N; ++i) {
        c += probabilityOfScattering(PathDirection::EYE_TO_SOURCE, Vector3::cosHemiRandom(shadingNormal, rng), rng, expressiveParameters) / float(N);
    }

    return c;
}


} // namespace G3D
