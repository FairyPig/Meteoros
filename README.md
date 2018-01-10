# Meteoros

![](/images/READMEImages/finalRender.PNG)

*Runs at < 3ms/Frame at a Full HD Resolution on a notebook GTX 1070. Update to readme about how it got to 3ms/frame coming soon*

## Overview

This project is a real-time cloudscape renderer in Vulkan that was made as the final project for the course, CIS 565: GPU Programming and Architecture, at the University of Pennsylvania. It is based on the cloud system 'NUBIS' that was implemented for the Decima Engine by Guerrilla Games. The clouds were originally made for the game 'Horizon Zero Dawn' and were described in the following SIGGRAPH 2015 and 2017 presentations: 

* 2015 [The Real-time Volumetric Cloudscapes of Horizon Zero Dawn](https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn)

* 2017 [Nubis: Authoring Realtime Volumetric Cloudscapes with the Decima Engine](https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine)

Contributors:
1. Aman Sachan - M.S.E. Computer Graphics and Game Technology, UPenn
2. Meghana Seshadri - M.S.E. Computer Graphics and Game Technology, UPenn

Skip Forward to:
1. [Instructions](#Instructions)
2. [Features](#Features)
	- [Current](#Current)
	- [Upcoming](#Upcoming)
3. [Pipeline Overview](#Pipeline)
4. [Implementation Overview](#Implementation)
	- [Rendering](#Rendering)
	- [Modelling](#Modeling)
	- [Remap](#remap)
	- [Lighting](#Lighting)
	- [Post-Processing](#Post)
5. [Performance Analysis and Optimizations](#Performance)
6. [Notes](#Notes)
7. [Resources](#Resources)
8. [Bloopers](#Bloopers)

## Instructions <a name="Instructions"></a>

If you wish to run or develop on top of this program, please refer to the [INSTRUCTION.md](https://github.com/Aman-Sachan-asach/Meteoros/blob/master/INSTRUCTION.md) file.

## Features <a name="Features"></a>

### Current <a name="Current"></a>
- Vulkan Framework that is easy to extend, heavily commented, and easy to read and understand
- Multiple Compute and Graphics pipelines working in sync
- Cloud Modelling, Lighting, and Rendering Models as defined by the papers
- HDR color space
- God-Rays and Tone Mapping Post processes
- Raymarching and Cloud rendering Optimizations
- Reprojection Optimization that is slightly buggy and currently only in the test branch 'AmanDev'

### Upcoming <a name="Upcoming"></a>
- Fully functional reprojection optimization
- More refined cloud shapes and lighting
- Anti-Aliasing - Temporal and Fast-Approximate (TXAA and FXAA)
- Offscreen Rendering Pipeline
- Cloud Shadow casting

## Pipeline Overview <a name="Pipeline"></a>

![](/images/READMEImages/pipelinelayout.png)

We have 3 distinct stages in our pipeline: compute stage, rasterization or the regular graphics pipeline stage, and a post-process stage.

#### Compute Stage:
This stage is responsible for the bulk of this project. It handles: 
- Reprojection Compute Shader: Reprojection calculations in a separate compute shader
- Cloud Compute Shader: Cloud raymarching, modeling, lighting, and rendering calculations and stores the result in a HDR color space, i.e. 32bit RGBA channels, texture. This shader also generates a "god-ray creation" texture, which is a grray-scale image used by the god-rays post process shder to create god rays.

#### Synchronization:
The synchronization is in place to ensure that the graphics pipeline doesn't use an image that is half complete and still being written to by the compute pipeline. This is necessary because we are following a compositing model in which we generate the clouds and then paint over them with the rasterized geometry in the world. After this the god rays shader also uses and adds on top of the same texture generated by the compute stage. 

The synchronization point is implemented as a Image Barrier which you can learn more about [here](https://vulkan.lunarg.com/doc/view/1.0.30.0/linux/vkspec.chunked/ch06s05.html#synchronization-memory-barriers).

We don't need a synchronization point between the graphics pipeline stage and the subsequent post-process stage because the commands for these stages are stored in the same Queue which stores the command buffer. All commands in the command buffer attached to a queue is executed in order after the previous command has completely finished executing thus if we store the commands in the command buffer in the correct order, we will not need additional synchronization points.

#### Graphics Pipeline Stage:

This stage is responsible for the rendering of 3D models, which is done via rasterization. The implementation closely follows [this vulkan tutorial](https://vulkan-tutorial.com/) except for the fact that it has been refactored into very readable classes instead of a single file.
This commands for this stage have been commented outis and thus are not being dispatched because they weren't adding anything to our scene.

#### Post Process Stage:

This stage is responsible for adding the god-rays, and tone mapping post-process effects.

## Implementation Overview <a name="Implementation"></a>

### Rendering <a name="Rendering"></a>

We render clouds to a texture using the ray marching technique, which is an image based volumetric rendering technique that is used to evalute volumes as opposed to surfaces. This means that the assumption that a objects properties can be defined at or by its surface are thrown out the window. Ray marching involves sampling a ray at various points along its length because the volume is defined at every point inside itself. 

Ray marching is a widely discussed subject and you can find many great resources to dive into it such as this presentation (https://cis700-procedural-graphics.github.io/files/implicit_surfaces_2_21_17.pdf) from a course on Procedural Graphics at UPenn and [iq's blog](http://www.iquilezles.org/www/articles/raymarchingdf/raymarchingdf.htm)

At every step of our ray march we determine how dense the atmosphere is and if it is dense enough to be quantified as a cloud we light that point. Our lighting model is described later in this readme, however it will make a lot more intuitive sense if one is familiar with volumetic lighting. You can learn more about volumetric lighting in the book [Physically Based Rendering from Theory to Implementation](http://www.pbrt.org/). That is a bit dense and so if you simply want a simple overview go [here](https://www.scratchapixel.com/lessons/advanced-rendering/volume-rendering-for-artists).

To render the sky as a skybox type dome we create 3 spheres, representing the earth, the inner layer of the atmosphere, and the outer layer of the atmosphere.

<img src="/images/READMEImages/layerLayout.png" width="642" height="362"> 

We don't want to render any thing beyond the horizon because we can't see anything beyond the horizon anyway.

<img src="/images/READMEImages/horizonLine.png" width="642" height="362"> 

Placing a camera atop this virtual earth, we can start our actual rendering process. Start raycasting from your camera, for every ray evaluate it at a fixed stepsize when it is inside the the 2 atmosphere layers we just created.

<img src="/images/READMEImages/raymarching.png" width="642" height="362"> 

When we evaluate a point along the ray and determine it has a non-zero density value we know we are inside a cloud.

<img src="/images/READMEImages/InOutOfCloud.png" width="642" height="362">

Now, to actually give this point in the cloud some coloration we can light it by shooting a ray towards our single light source, the sun, and use the resulting energy information to color that point.

<img src="/images/READMEImages/LightCalculationsNaive.png" width="642" height="362">

Cone Sampling is a more efficient way of determining the light energy that will be recieved by that point. Cone sampling involves taking some number of samples from inside the volume of a cone that is aligned with our light source. We take 6 samples using cone sampling and make sure to have the last one be placed relatively far. This far-away sample is a way of taking into account if the cloud and hence point we are trying to light is occluded by another cloud in the distance. Using these 6 samples from within the cone we get a density value which is used to attenuate the light energy reaching the point we are trying to color.

<img src="/images/READMEImages/ConeSampling.png" width="642" height="362"> 

### Modeling <a name="Modeling"></a>

Generating noise on the fly to determine our cloud shape is a very expensive process. This is why we use tiling 3D noise textures with precomputed density values to determine our cloud shape.

There are 3 textures that are used to define the shape of a cloud in this project: 

	3D cloudBaseShapeTexture
	4 channels…
	128^3 resolution…
	The first channel is the Perlin-Worley noise.
	The other 3 channels are Worley noise at increasing frequencies. 
	This 3d texture is used to define the base shape for our clouds.

![](/images/READMEImages/LowFrequencyNoiseChannels.png)

	3D cloudDetailsTexture
	3 channels…
	32^3 resolution…
	Uses Worley noise at increasing frequencies. 
	This texture is used to add detail to the base cloud shape defined by the first 3d noise.

![](/images/READMEImages/highFrequencyDetail.png)

	2D cloudMotionTexture
	3 channels…
	128^2 resolution…
	Uses curl noise. Which is non divergent and is used to fake fluid motion. 
	We use this noise to distort our cloud shapes and add a sense of turbulence.	

![](/images/READMEImages/curlNoise.png)

To define the shape of a cloud we first determine its base shape using the low frequency noise in the '3D cloudBaseShapeTexture', next we errode away the cloud at the edges using the '3D cloudDetailsTexture', and finally we give our cloud a sense of turbulence and fake the look of a moving cloud using the '2D cloudMotionTexture.'

![](/images/READMEImages/CloudErosion.png)

### Remap Function <a name="Remap"></a>

One function is used ubiquitously in modelling and lighting these clouds. The remap finction:

	float remap(in float value, in float original_min, in float original_max, in float new_min, in float new_max)
	{
		return new_min + ( ((value - original_min) / (original_max - original_min)) * (new_max - new_min) );
	}

This remap function simply takes a value, that lies inside one range and maps it to another range that you provide. It seems like a simple function and it is in concept but it is can be used in very clever maners to do all sorts of things.

For example, towards the end of the modelling section of this readme, we talked about erroding the edges of a cloud. How do you possibly determine that the point you're evaluatiing is at the edge of a cloud? Well we could use the remap function to do just that.

If we use the below graph as an example, where the red line represents the base density of the cloud and the green line represents the high frequency noise used to erode our cloud at the egdes. If we performed a remap operations on the base density using the high frequency noise as the new minimum value then we would not lose any density in the center of the cloud, which is exactly what we want.

![](/images/READMEImages/remapFunctionDemonstration.png)

### Lighting <a name="Lighting"></a>

The lighting model as described in the 2017 presentation is an attenuation based lighting model. This means that you start with full intensity, and then reduce it as combination of the following 3 probabilities: 

1. Directional Scattering
2. Absorption / Out-scattering 
3. In-scattering

![](/images/READMEImages/lightingProbs.PNG)

#### Directional Scattering

This retains baseline forward scattering and produces silver lining effects. It is calculated using Henyey-Greenstein equation.

The eccentricity value that generally works well for mid-day sunlight doesn't provide enough bright highlights around the sun during sunset. 

![](/images/READMEImages/hg01.PNG)

Change the eccentricity to have more forward scattering, hence bringing the highlights around the sun. Clouds 90 degrees away from the sun, however, become too dark.

![](/images/READMEImages/hg02.PNG)

To retain baseline forward scattering behavior and get the silver lining highlights, combine 2 HG functions, and factors to control the intensity of this effect as well as its spread away from the sun.

![](/images/READMEImages/hg03.PNG)

![](/images/READMEImages/hg04.PNG)

#### Absorption / Out-scattering

This is the transmittance produced as a result of the Beer-Lambert equation. 

Beer's Law only accounts for attenuation of light and not the emission of light that has in-scattered to the sample point, hence making clouds too dark. 

![](/images/beerslaw.png)

![](/images/READMEImages/beer01.PNG)

By combining 2 Beer-Lambert equations, the attenuation for the second one is reduced to push light further into the cloud.

![](/images/READMEImages/beer02.PNG)

#### In-scattering
This produces the dark edges and bases to the clouds. 

In-scattering is when a light ray that has scattered in a cloud is combined with others on its way to the eye, essentially brightening the region of the cloud you are looking at. In order for this to occur, an area must have a lot of rays scattering into it, which only occurs where there is cloud material. This means that the deeper in the cloud, the more scattering contributors there are, and the amount of in-scattering on the edges of the clouds is lower, which makes them appear dark. Also, since there are no strong scattering sources below clouds, the bottoms of them will have less occurences of in-scattering as well. 

Only attenuation and HG phase: 

![](/images/READMEImages/in01.PNG)

Sampling cloud at low level of density, and accounting for attenuation along in-scatter path. This appears dark because there is little to no in-scattering on the edges.

![](/images/READMEImages/in02.PNG)

Relax the effect over altitude and apply a bias to compensate. 

![](/images/READMEImages/in03.PNG)

Second component accounts for decrease in-scattering over height. 

![](/images/READMEImages/in04.PNG)

### Post Processing <a name="Post"></a>

#### GodRays

God Rays are the streaks of light that poke out from behind clouds. These streaks, which stream through gaps in clouds or between other objects, are columns of sunlit air separated by darker cloud-shadowed regions. Despite seeming to converge at a point, the rays are in fact near-parallel shafts of sunlight. Their apparent convergence is a perspective effect.

We faked the effect of god rays in screen space by radially blurring light from the location of the sun in screen space and using a grey scale godray mask that was generated in the cloudcompte shader to determine where the clouds lie and hence where streaks of light shouldn't appear.

If the sun was not present in the camera frustum that meant that its screen space location was outside the range [0,1] meaning we couldn't generate a mask for points outside screen space or even sample pixels for radial blurring. To overcome the pixel sampling issue we simply used a gradient to represent the energy value. The gradient moved from white to grey as we moved radially away from the sun. There isn't a good way to incorporate the mask back in so we assume that the gradient values outside the screen bounds are unoccluded.

There also isnt a great but also cheap way to do god-rays when the sun is behind the camerabecause we have no data whatsoever. We simply, blend out the god rays as the sun moves to points no longer in the same hemisphere as the camera lookAt vector.

| Only GodRays | GodRay Mask |
| ------------ |:-----------:|
| ![](/images/READMEImages/godRayMask.PNG)                    | ![](/images/READMEImages/onlyGodRays.png) |
| GodRays Composite on Cloud Density                          | Final Composite |
| ![](/images/READMEImages/godraycompositeOnCloudDensity.PNG) | ![](/images/READMEImages/godraysComposited.png) |

God Rays use a radial blurring technique which relies on sampling pixels from the god ray mask. The more samples you take the higher the performance toll. The FPS dropped by 3 FPS due to the god ray post-process at a 100 sample/pixel .
![](/images/READMEImages/godRayChart.PNG)

#### Tone Mapping

Tone Mapping is a technique used to map one color space into another to approximate the appearance of high dynamic range images because displays and monitors have more limited dynamic ranges. We implemented the uncharted 2 tone mapping technique that is really good and has become quite popular.

A great visual example of the different [types of tone mapping techniques](https://www.shadertoy.com/view/lslGzl).

Our project is working in the HDR color space and so requires tone mapping to avoid ridiculously blown out images. (a perfect example is in the [bloopers](#Bloopers))

| Without                                             | With                                             |
| --------------------------------------------------- |:------------------------------------------------:|
| ![](/images/READMEImages/lightingNotToneMapped.png) | ![](/images/READMEImages/lightingToneMapped.png) |

## Performance Analysis and Optimizations <a name="Performance"></a>

Performance analysis conducted on: Windows 10, i7-7700HQ @ 2.8GHz 32GB, GTX 1070(laptop GPU) 8074MB (Personal Machine: Customized MSI GT62VR 7RE)

### Early termination based on accumulated Density

If during our ray march we accumulate a density value of over 1 then we terminate the ray march early.

### Cheap Sampling

We determine we are inside a cloud by getting our base density for the cloud and ensuring its greater than 0. We will not do the high requency errosion and further cloud shaping nor will we do any expensive lighting calculations unless we have determined we are inside a cloud.

![](/images/READMEImages/cheapSamplingChart.PNG)

### Reprojection

Reprojection is a technique that uses pixels from the previous frame to color the pixels in the current frame. This is done by projecting the old frame onto the current frame. We calculate how much the camera has moved between 2 frames an

More technically this is implemented as follows:
1. We store our previous frame in a texture.
2. Store the old camera information
3. For a given pixel in the current frame, ray cast using the ray created by the current camera and the current pixels uv co-ordinates.
4. The ray cast will intersect with the sphere representing the inner layer of the atmosphere. The point of intersection gives ud a world space position.
5. This world space position is then converted all the way back into uv co-ordinates using the old camera's view and projection matrices.
6. This uv co-ordinate if in the range from [0,1] can be used to sample the pixel in the old frame that will be used to fill in the pixel in the current frame.

Using this reprojection technique we can get away with actuall only ray marching for 1/16th of the pixels in the current frame. The other 15 of the 16 pixels are filled in using reprojection.

This technique has been implemented in a test branch but is slightly buggy, but preliminary tests show it giving us a 5.97 times speed boost for a version with an inexpensive lighting model. This further implies that the way more complex lighting model in the master branch will gain even better performance boosts.

![](/images/READMEImages/reprojectionPerformance.PNG)

## Notes <a name="Notes"></a>
- We did not add checks (which is highly recommended when developing Vulkan code for other users) to make sure some features are supported by the GPU before using them, such as anisotropic filtering and the image formats that the GPU supports.

## Resources <a name="Resources"></a>

#### Texture Resources:
- [Low and High Frequency Noise Textures](https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine) were made using the 'Nubis Noise Generator' houdini tool that was released along with the 2015 paper. 
- [Curl Noise Textures](http://bitsquid.blogspot.com/2016/07/volumetric-clouds.html)
- Weather Map Texture by Dan Mccan

#### Libraries:
- [Image Loading Library](https://github.com/nothings/stb)
- [Obj Loading Library](https://github.com/syoyo/tinyobjloader)
- [Why to include stb in .cpp file](https://stackoverflow.com/questions/43348798/double-inclusion-and-headers-only-library-stbi-image)
- [Imgui](https://github.com/ocornut/imgui) for our partially wriiten gui
- [GLFW](http://www.glfw.org/) utilities for Windows
- [GLM](https://glm.g-truc.net/0.9.8/index.html) 

#### Vulkan
- [Vulkan Tutorial](https://vulkan-tutorial.com/)
- [RenderDoc](https://renderdoc.org/)
- [Setting Up Compute Shader that writes to a texture](https://github.com/SaschaWillems/Vulkan/tree/master/examples/raytracing)
- [3D Textures](https://github.com/SaschaWillems/Vulkan/tree/master/examples/texture3d)
- [Pipeline Caching](https://github.com/SaschaWillems/Vulkan/tree/master/examples/radialblur) was used for post-processing and so it made more sense to see how it is done for post processing
- [Radial Blur](https://github.com/SaschaWillems/Vulkan/tree/master/examples/radialblur)

#### Post-Processing:
- [Uncharted 2 Tone Mapping](http://filmicworlds.com/blog/filmic-tonemapping-operators/)
- [God Rays](https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch13.html)

#### Upcoming Feature Set:
- [Off-screen Rendering](https://github.com/SaschaWillems/Vulkan/tree/master/examples/offscreen)
- [Push Constants](https://github.com/SaschaWillems/Vulkan/tree/master/examples/pushconstants)

#### Other Resources
- FBM Procedural Noise Joe Klinger
- Preetham Sun/Sky model from Project Marshmallow 

## Bloopers <a name="Bloopers"></a>

* Tone Mapping Madness

![](/images/READMEImages/meg01.gif)

* Sobel's "edgy" clouds

![](/images/READMEImages/sobeltest.PNG)
