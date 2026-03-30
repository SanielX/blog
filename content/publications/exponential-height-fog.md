---
title: "Exponential height fog in detail"
type: publications
date: 2026-03-29
thumbnail: "/publications/fog/preview.jpg"

affiliations:

---
{{< numbering h2=false h3=false >}}
{{< figure src="../fog/preview.jpg" class="expandable" alt="" >}}

This post is a result of me trying to figure out how exponential hight fog works and how to extend it to support features I need (such as min distance).

## Exponential fog equation {#fog_equation}
Here's how you compute exponential fog (for an opaque material):

```hlsl
float d = distance(camera_position, pixel_world_position);
float transmittance = exp(-d * density);

float3 final_color = g_FogColor + pixel_color.rgb * transmittance;
```

This code follows Beer-Lambert Law:
<div class="math_formula">
`T(A->B) = e^-(int_A^B beta e(x) dx)`
</div>

It states that, when light travels through a medium (fog) some of it gets absorbed by the medium.
The amount of light that is "transmitted" through the medium from point A to point B (`A->B`) is described by the formula above.
<div>
`beta e(x)` is a function that describes density of the medium at any given point.
</div>

Let's consider constant density:
<div class="math_formula">
`beta e(x) = C`,
where <br>C is some constant
</div>
We can simplify transmittance function to the form that we've seen in code:
<div class="math_formula">
`T(A->B) = e^-(int_A^B beta e(x) dx) = e^-(int_A^B C dx) = e^-(C * int_A^B dx) = e^-(C x)`
</div>

{{< figure src="../fog/Screenshot_1.jpg" class="expandable" alt="" >}}


## References
https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf
https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite