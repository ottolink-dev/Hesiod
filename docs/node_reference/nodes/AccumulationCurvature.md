
AccumulationCurvature Node
==========================


Computes the accumulation curvature of a heightmap, indicating how terrain shape influences water flow. Positive values suggest converging flow (e.g., channels or valleys), while negative values indicate diverging flow (e.g., ridges or hilltops).



![img](../../images/nodes/AccumulationCurvature_settings.png)


## Category


WIP/DEPRECATED
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|input|VirtualArray|Input heightmap used for accumulation curvature analysis.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|output|VirtualArray|Computed accumulation curvature of the input heightmap.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|clamp_max|Bool|No description|
|Gain|Float|Mid-centered gain transformation applied to the elevation values. This is a non-linear recurve operator centered around the mid elevation (typically 0.5). Increasing the gain pushes values toward the minimum and maximum elevations, creating flatter low/high regions with a steeper transition around the midpoint.|
|Gamma|Float|Standard gamma correction applied to the elevation values. This is a monotonic power-law remapping that shifts emphasis toward low or high elevations, making the overall shape sharper or bulkier without changing its ordering.|
|Invert Output|Bool|Inverts the output values after processing, flipping low and high values across the midrange.|
|Mix Factor|Float|Mixing factor for blending input and output values. A value of 0 uses only the input, 1 uses only the output, and intermediate values perform a linear interpolation.|
|Mix Method|Enumeration|Method used to combine input and output values. Options include linear interpolation (default), min, max, smooth min, smooth max, add, and subtract.|
|Remap Range|Value range|Linearly remaps the output values to a specified target range (default is [0, 1]).|
|Saturation Range|Value range|Modifies the amplitude of elevations by first clamping them to a given interval and then scaling them so that the restricted interval matches the original input range. This enhances contrast in elevation variations while maintaining overall structure.|
|Smoothing Radius|Float|Defines the radius for post-processing smoothing, determining the size of the neighborhood used to average local values and reduce high-frequency detail. A radius of 0 disables smoothing.|
|radius|Float|Defines the filter radius relative to the domain size, controlling the scale of curvature analysis.|
|vc_max|Float|No description|

## Example


!!! note "No example yet"
    No example available for this node.  
