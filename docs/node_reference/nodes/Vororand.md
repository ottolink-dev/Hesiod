
Vororand Node
=============


This function computes a Voronoi diagram or derived metric (such as F1, F2, or edge distances) on a grid of given shape. A set of random points is generated within an extended bounding box, based on the desired density and variability, to reduce edge artifacts. Optionally, per-pixel displacement can be applied through noise fields. Optionnaly, a user-defined set of points can be used instead of automatically generated random points.



![img](../../images/nodes/Vororand_settings.png)


## Category


Primitive/Coherent
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|cloud|Cloud|No description|
|dx|VirtualArray|Displacement with respect to the domain size (x-direction).|
|dy|VirtualArray|Displacement with respect to the domain size (y-direction).|
|envelope|VirtualArray|Heightmap used as a post-process amplitude multiplier for the generated noise.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|out|VirtualArray|No description|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|density|Float|No description|
|exp_sigma|Float|No description|
|k_smoothing|Float|No description|
|Gain|Float|Mid-centered gain transformation applied to the elevation values. This is a non-linear recurve operator centered around the mid elevation (typically 0.5). Increasing the gain pushes values toward the minimum and maximum elevations, creating flatter low/high regions with a steeper transition around the midpoint.|
|Gamma|Float|Standard gamma correction applied to the elevation values. This is a monotonic power-law remapping that shifts emphasis toward low or high elevations, making the overall shape sharper or bulkier without changing its ordering.|
|Invert Output|Bool|Inverts the output values after processing, flipping low and high values across the midrange.|
|Remap Range|Value range|Linearly remaps the output values to a specified target range (default is [0, 1]).|
|Saturation Range|Value range|Modifies the amplitude of elevations by first clamping them to a given interval and then scaling them so that the restricted interval matches the original input range. This enhances contrast in elevation variations while maintaining overall structure.|
|Smoothing Radius|Float|Defines the radius for post-processing smoothing, determining the size of the neighborhood used to average local values and reduce high-frequency detail. A radius of 0 disables smoothing.|
|return_type|Enumeration|No description|
|Seed|Random seed number|Random seed number. The random seed is an offset to the randomized process. A different seed will produce a new result.|
|sqrt_output|Bool|No description|
|variability|Float|No description|

## Example


![img](../../images/nodes/Vororand_hsd_example.png)

Corresponding Hesiod file: [Vororand.hsd](../../examples/Vororand.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).

  
