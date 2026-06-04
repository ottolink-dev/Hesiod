
SelectSoilFlow Node
===================


Computes a soil–flow selection map based on the terrain gradient and the flow-accumulation map. This node can be for instance chained with ColorizeGradient for texturing.



![img](../../images/nodes/SelectSoilFlow_settings.png)


## Category


Terrain Features/ForTexturing
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|input|VirtualArray|Input heightmap used to compute curvature and gradient contributions.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|output|VirtualArray|Output soil-flow mask combining flow and gradient effects.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|Clipping Ratio|Float|Flow accumulation clipping ratio. Smaller values will saturate the flow contrast.|
|Flow Distrib. Exponent|Float|Power-law (gamma) exponent applied to the flow values. Lower gamma flattens the distribution, higher gamma increases contrast and steepens the transition between low and high values.|
|Flow Weight|Float|Weight applied to the flow component in the final mix.|
|Gradient Weight|Float|Weight applied to the gradient component in the final mix.|
|Gain|Float|Mid-centered gain transformation applied to the elevation values. This is a non-linear recurve operator centered around the mid elevation (typically 0.5). Increasing the gain pushes values toward the minimum and maximum elevations, creating flatter low/high regions with a steeper transition around the midpoint.|
|Gamma|Float|Standard gamma correction applied to the elevation values. This is a monotonic power-law remapping that shifts emphasis toward low or high elevations, making the overall shape sharper or bulkier without changing its ordering.|
|Invert Output|Bool|Inverts the output values after processing, flipping low and high values across the midrange.|
|Mix Factor|Float|Mixing factor for blending input and output values. A value of 0 uses only the input, 1 uses only the output, and intermediate values perform a linear interpolation.|
|Mix Method|Enumeration|Method used to combine input and output values. Options include linear interpolation (default), min, max, smooth min, smooth max, add, and subtract.|
|Remap Range|Value range|Linearly remaps the output values to a specified target range (default is [0, 1]).|
|Saturation Range|Value range|Modifies the amplitude of elevations by first clamping them to a given interval and then scaling them so that the restricted interval matches the original input range. This enhances contrast in elevation variations while maintaining overall structure.|
|Smoothing Radius|Float|Defines the radius for post-processing smoothing, determining the size of the neighborhood used to average local values and reduce high-frequency detail. A radius of 0 disables smoothing.|
|Gradient Radius|Float|Radius used to compute local gradient magnitudes, defining the neighborhood size influencing slope estimation. Larger radii yield smooth gradients.|
|Ref. Talus|Float|Reference talus value for river extraction.|

## Example


![img](../../images/nodes/SelectSoilFlow_hsd_example.png)

Corresponding Hesiod file: [SelectSoilFlow.hsd](../../examples/SelectSoilFlow.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).


