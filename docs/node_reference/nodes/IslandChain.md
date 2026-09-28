
IslandChain Node
================


IslandChain scatters a series of islands along an input path, each an fbm-perturbed radial blob (the same construction as IslandLandMask), with a signed size falloff along the path. The output is a land mask suitable as input to the Island node.



![img](../../images/nodes/IslandChain_settings.png)


## Category


Primitive/Geological
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|path|Path|Input path along which the islands are scattered (at least two points, in normalized domain coordinates).|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|output|VirtualArray|Generated island-chain land mask.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|Displacement|Float|Amplitude of the fbm perturbation applied to each island boundary.|
|Islands|Integer|Number of islands spawned along the path.|
|Radius|Float|Base island radius in normalized domain units.|
|kw|Float|Base spatial frequencies in the X and Y directions. The frequencies are defined with respect to the entire domain: for example, kw = 2 produces two full oscillations across the domain width (and similarly for the Y direction).|
|Lacunarity|Float|The frequency scaling factor for successive noise octaves. Higher values increase the frequency of each successive octave.|
|Type|Enumeration|Noise type used for the island boundary perturbation.|
|Octaves|Integer|The number of octaves for fractal noise generation. More octaves add finer details to the terrain.|
|Persistence|Float|The amplitude scaling factor for subsequent noise octaves. Lower values reduce the contribution of higher octaves.|
|Gain|Float|Mid-centered gain transformation applied to the elevation values. This is a non-linear recurve operator centered around the mid elevation (typically 0.5). Increasing the gain pushes values toward the minimum and maximum elevations, creating flatter low/high regions with a steeper transition around the midpoint.|
|Gamma|Float|Standard gamma correction applied to the elevation values. This is a monotonic power-law remapping that shifts emphasis toward low or high elevations, making the overall shape sharper or bulkier without changing its ordering.|
|Invert Output|Bool|Inverts the output values after processing, flipping low and high values across the midrange.|
|Mix Factor|Float|Mixing factor for blending input and output values. A value of 0 uses only the input, 1 uses only the output, and intermediate values perform a linear interpolation.|
|Mix Method|Enumeration|Method used to combine input and output values. Options include linear interpolation (default), min, max, smooth min, smooth max, add, and subtract.|
|Remap Range|Value range|Linearly remaps the output values to a specified target range (default is [0, 1]).|
|Saturation Range|Value range|Modifies the amplitude of elevations by first clamping them to a given interval and then scaling them so that the restricted interval matches the original input range. This enhances contrast in elevation variations while maintaining overall structure.|
|Smoothing Radius|Float|Defines the radius for post-processing smoothing, determining the size of the neighborhood used to average local values and reduce high-frequency detail. A radius of 0 disables smoothing.|
|Scatter|Float|Random island displacement along and across the path.|
|Seed|Random seed number|Random seed number. The random seed is an offset to the randomized process. A different seed will produce a new result.|
|Size Falloff|Float|Signed size falloff along the path, in [-1, 1]: 0 keeps island sizes uniform, +1 shrinks islands toward the path end (largest island at the head), -1 shrinks them toward the start.|
|Size Jitter|Float|Per-island random radius variation ratio.|
|Weight|Float|Controls how much higher FBM octaves contribute to the noise based on local elevation. A higher weight suppresses high-frequency octaves at low elevations and increases their influence at higher elevations, producing terrain where fine details appear mainly near peaks while lower areas remain smoother.|

## Example


![img](../../images/nodes/IslandChain_hsd_example.png)

Corresponding Hesiod file: [IslandChain.hsd](../../examples/IslandChain.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).


