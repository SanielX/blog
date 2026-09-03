---
title: Abstracted Away - About me
---

# Projects
These games were made by me and my friends during our free time. It was real fun.
<br><br>
<div class="proj_list">
  <div class="proj">
   <a href="https://hostgame-studio.itch.io/slendytubbies-0">
    <div class="proj_name">Slendytubbies 0</div> 
    <div class="proj_image">{{<img src="/images/st0_project.png">}}</div>
   </a>
   <div class="proj_desc">Made with Unity.</div>
  </div>

  <div class="proj">
   <a href="https://ftremus.itch.io/ourlastdaystogether">
    <div class="proj_name">Our Last Days Together</div> 
    <div class="proj_image">{{<img src="/images/out_last_days_project.png">}}</div>
   </a>
   <div class="proj_desc">Made with Unity (Gamejam).<br>Runs on my own SRP.<br>We've placed 12th in Art category!</div>
  </div>

  <div class="proj">
   <a href="https://ftremus.itch.io/mine">
    <div class="proj_name">MINE!</div> 
    <div class="proj_image">{{<img src="/images/mine_project.gif">}}</div>
   </a>
   <div class="proj_desc">Made with Godot (Gamejam).<br>It's my favorite :)<br>Players didn't like it very much :(</div>
  </div>

</div>

<div class="proj big">
  <a href="https://github.com/SanielX/Hollow-TerrainSystem">
    <div class="proj_name big">Hollow;TerrainSystem</div>
    {{<img class="proj_image big" src="images/terrain_system.gif">}}
  </a>
  <div class="proj_desc big">
    <ul>
    <li>GPU driven editing & rendering</li>
    <li>Support for layers, similar to Photoshop.</li>
    <li>Adaptive Virtual Texturing, based on <a href="https://www.youtube.com/watch?v=SVPMhGteeuE">Far Cry 4</a>.</li>
    </ul>
  </div>
</div>

<div class="proj big">
  <a href="https://github.com/SanielX/Microscenes">
    <div class="proj_name big">Hollow;Microscenes</div>
    {{<img class="proj_image big" src="images/microscenes.gif">}}
  </a>
  <div class="proj_desc big">
    <ul>
    <li>Visual scripting plugin for Unity.</li>
    <li>Focused on being very user friendly.</li>
    <li>Provides a lot of feedback about graph execution.</li>
    </ul>
  </div>
</div>

## Other
I did not publish these projects but they are pretty interesting to me so I might as well share :)

<details>
<summary>
Raytracer (Win32 & C++)
</summary>
"Realtime" raytracer using C++, and Win32 API. Has code hot-reload by separating app code into its own DLL. Rendering is done on CPU (multithreaded), triangle meshes have AABB triangle tree to speed up lookups.
{{< figure src="images/raytracer.gif" class="expandable img_small" alt="" >}}
<hr>
</details>

<details>
<summary>
Custom NavMesh agent (Unity)
</summary>

So, as part of DOTS movement, Unity has added NavMeshQuery API. And since I hate built in navmesh agents, I've decided to make my own. Whole system is done in 3 parts: 
- **Navigation Server**: Issues pathfinding requests and completes them on background thread using jobs.
- **Navigation Agent System**: Computes agent speed, taking into account local avoidance.
- **Path post processor**: Makes AI paths nicer by converting them into bezier curves with a lot of post processing applied on top: making path more natural, removing redundant points, etc.

<div style="display: flex; flex-direction: row;">
{{< figure src="images/path_post_process_before.png" class="expandable img_small" alt="" >}}
{{< figure src="images/path_post_process_after.png" class="expandable img_small" alt="" >}}
</div>
<iframe width="560" height="315" src="https://www.youtube-nocookie.com/embed/fz9_-lDBbPU?si=fW0s1EbrF6t2RfYc&amp;controls=0" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>
<hr>
</details>


<details>
<summary>
EdRP: Custom Render Pipeline (Unity)
</summary>
It's just a custom render pipeline I've built. Features include: Physically based rendering, physical light units, volumetric fog, gamma adaptation, XeGTAO, Forward+ clustered lighting, decals, shadowmaps based on "Call Of Duty Cold War Black Ops".

{{< figure src="images/EDRP2.png" class="expandable img_small" alt="" >}}

I've spent a lot of time making volumetric fog look good. My favorite effect in all graphics, probably.

{{< figure src="images/volumetrics.gif" class="expandable img_small" alt="" >}}

<hr>
</details>
