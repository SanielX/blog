---
title: "Silly exponential fog trick"
type: publications
date: 2026-03-29
thumbnail: "/publications/fog/preview.jpg"

affiliations:

---
{{< numbering h2=false h3=false >}}
{{< figure src="../fog/preview.jpg" class="expandable" alt="" >}}

For my last project I worked on implementing fog post effect. As one does, I started with exponential fog, since it is simple and pretty. 
I have added height component to it as well. To learn how it works I recommend reading article by [Inigo Quilez](https://iquilezles.org/articles/fog/).

My problem was lack of control over the effect, since typical exponential fog only has a single uniform density parameter. 
So, I came up with a silly trick to add a "minimum distance" to it.

<br>
Let's first consider how one might compute exponential fog (for an opaque material):

```hlsl
float optical_depth = density * distance(camera_position, pixel_world_position);
float fog_amount    = exp(-optical_depth); // transmittance

// basically alpha blending fog on top of the material
float3 final_color = g_FogColor * (1 - fog_amount) + pixel_color.rgb * fog_amount;
```

The code is derived from Beer-Lambert's Law:

<div class="math_formula">`T(A->B) = e^-{:int_A^B beta e(x) dx:}`</div>

This formula computes amount of light that is *transmitted* through a medium (fog). 𝛽⁢𝑒⁡(𝑥) is a function that defines *density* of the medium for any given point.
For a constant density fog, it simplifies to a simple "distance times density".

<div class="math_formula">`T(A->B) = e^-{:int_A^B beta e(x) dx:} = e^-{:int_A^B a dx:} = e^-{:a int_A^B dx:} = e^-{:a x:}`</div>


For height fog this is more complicated. But the point is that we need to compute some integral. I will call its value *optical depth*. It is, in a way, length of the view ray weighted by medium's density at each point of the ray.

## The silly trick {#the_trick}
To add minimum distance to the exponential fog, we'll use smoothstep function to decrease fog's density around the camera.

<div class="math_formula">`beta e(x) = alpha * { ( s(x / M), x <= M), (1, x> M) :}`</div>
Where `M` is our minimum distance. `s(x)` is smoothstep function. It is equal to 0 at 0 and 1 at 1, so we scale `x` by minimum distance to stretch it as we want. Next, we need smoothstep's formula and its antiderivative. What's nice about it is that `beta e'(1) = 0` so it'll match our constant density (when `x >= M`), meaning there won't be a sharp visible seam on the border.

<div class="math_formula">`s(x) = 3(x/M)^2 - 2(x/M)^3`</div>
<div class="math_formula">`S(x) = int s(x) dx = -{:1/2:}x^4 + x^3`</div>
We need to take into account that `x` will be scaled by `1/M`, so the actual formula is this:
<div class="math_formula">`S(x) = -{:1/2:}(x/M)^4 + (x/M)^3 = {:1/M^3 :} x^3 -{: 1/{: 2 * M^4 :} :} x^4`</div>

<br>

Translating this into HLSL we get following:
```hlsl
float smoothstep_antiderivative(float x, float M)
{
  x = min(M, x);
  // 1/x is the same as (x^-1) so we can write pow(M, N-1) instead of 1/pow(M,N)
  return pow(M, 2) * pow(x,3) - 0.5 * pow(M, 3)*pow(x,4);
}

// ....
float d = distance(camera_position, pixel_world_position);
float optical_depth  = smoothstep_antiderivative(d, g_FogMinDistance);
      optical_depth += max(0, d-g_FogMinDistance);
```
In reality you can precompute a lot of this, since M is uniform. You can also precompute antiderivative for cases where d > M.

## Combining with height fog {#combine_height_fog}
Height fog computes its own optical depth and while we could derive formulas that take our little hack into account, here's a simple way to combine them:
```hlsl
float smooth_optical_depth = ComputeSmoothstep(distance);
float height_optical_depth = ComputeHeightFog(distance);
// We just want less fog near use so use smaller distance
float optical_depth = density * min(smooth_optical_depth, height_optical_depth);

float fog_amount = exp(-optical_depth); // transmittance
```

## A (maybe) simpler way {#custom_transmiattance}
Alternatively, you can just use smoothstep for your fog. In fact, you can replace exponent in the transmittance equation with anything you like.
<div class="math_formula">`T(A->B) = f({:int_A^B beta e(x) dx:})`</div>

For example:
```hlsl
float optical_depth = ComputeHeightFog(distance);
float fog_amount = smoothstep(g_FogMinDistance, g_FogMaxDistance, optical_depth);
```

This will work just fine but the problem is, height fog component is now affected by min and max distance. While keeping exponential fog allows you to control "force field" around the camera
separately from everything else.
To demonstrate. First image is exponential fog with M = 22, second uses smoothstep with the same parameter.

<div style="display: flex; flex-direction:row;">
{{< figure src="../fog/exp_fog.jpg" class="expandable" alt="" >}}
{{< figure src="../fog/smoothstep_fog.jpg" class="expandable" alt="" >}}
</div>

## Thanks for reading
This algorithm is implemented in my [Height Fog plugin](https://github.com/SanielX/Height-Fog) for Unity. You can just use that if you want.